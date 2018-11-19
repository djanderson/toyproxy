#include <arpa/inet.h>          /* inet_ntoa */
#include <assert.h>             /* assert */
#include <errno.h>              /* errno */
#include <getopt.h>             /* getopt_long, struct option, no_argument */
#include <netdb.h>              /* gethostbyname */
#include <netinet/tcp.h>        /* TCP_NODELAY */
#include <pthread.h>            /* pthread_* */
#include <signal.h>             /* sigset_t, sigaction */
#include <stdatomic.h>          /* atomic_ */
#include <stdlib.h>             /* size_t */
#include <string.h>             /* memset */
#include <stdio.h>              /* printf, fprintf */
#include <sys/socket.h>         /* setsockopt */
#include <sys/stat.h>           /* stat, struct st */
#include <unistd.h>             /* close, read, write */

#include <openssl/md5.h>        /* MD5_* */

#include "queue.h"
#include "hashmap.h"
#include "printl.h"
#include "request.h"
#include "response.h"

#define PROXY_ROOT ".cache"
#define MAX_THREADS 15          /* Max request handler threads */
#define MAX_BACKLOG 100         /* Max connections before ECONNREFUSED error */
#define MAX_REQUESTS 100        /* Request queue size */
#define KEEPALIVE_TIMEOUT_S 10
#define DEFAULT_CACHE_TIMEOUT_S 60


/* Command line options */
const char usage[] = "USAGE: %s [-h] port [cache timeout (secs)]\n";
const char shortopts[] = "hd";
const struct option longopts[] = {
    {"help", no_argument, 0, 'h'},
    {"debug", no_argument, 0, 'd'}
};

atomic_bool exit_requested = false;

queue_t requestq;
hashmap_t file_cache;

static void signal_handler(int __attribute__((__unused__)) sig)
{
    exit_requested = true;
}


/* Parse command line options. */
void parse_options(int argc, char *argv[], int *port, int *cache_timeout);
/* Setup the listener socket. */
int initialize_listener(struct sockaddr_in *saddr, int *fd);
/* Watch for incoming socket connections and spawn connection handler. */
int proxy(int ssock);
/* Handle a single connection until connection close or keep-alive timeout. */
void *handle_connection(void *fd_vptr);
/* Consume requests from the queue. */
void *handle_request(void *queue_vptr);
/* Send an HTTP error response (no body). */
int send_error(request_t *req, int status);
/* Send an HTTP response including the file at `path'. */
int send_file(request_t *req, char *path);
/* Handle cache timeout. */
void *cache_gc(void *cache_vptr);
/* `path' must be allocated with at least size strlen(url->path) + 3. */
void url_to_cache_path(const url_t *url, char *path);


int main(int argc, char *argv[])
{
    int rval, ssock, port, cache_timeout;
    pthread_t cache_gc_thread;
    pthread_t threads[MAX_THREADS];
    sigset_t set;
    struct sockaddr_in addr;
    int nthreads = sysconf(_SC_NPROCESSORS_CONF);

    if (nthreads > MAX_THREADS)
        nthreads = MAX_THREADS;

    printl_setlevel(INFO);

    parse_options(argc, argv, &port, &cache_timeout);

    signal(SIGINT, signal_handler);
    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    pthread_sigmask(SIG_BLOCK, &set, NULL);

    queue_init(&requestq, MAX_REQUESTS);
    hashmap_init(&hostname_cache, 100);
    hashmap_init(&file_cache, 100);
    file_cache.timeout = cache_timeout;

    /* Spawn cache timeout handler */
    if (pthread_create(&cache_gc_thread, NULL, cache_gc,
                       (void *)&file_cache) < 0) {
        printl(LOG_ERR "pthread_create - %s\n", strerror(errno));
        queue_destroy(&requestq);
        hashmap_destroy(&hostname_cache);
        return errno;
    }

    /* Spawn request handler thread pool */
    printl(LOG_DEBUG "Initializing %d worker threads\n", nthreads);
    for (int i = 0; i < nthreads; i++) {
        if (pthread_create(&threads[i], NULL,  handle_request, NULL) < 0) {
            printl(LOG_ERR "pthread_create - %s\n", strerror(errno));
            queue_destroy(&requestq);
            hashmap_destroy(&hostname_cache);
            return errno;
        }
    }

    pthread_sigmask(SIG_UNBLOCK, &set, NULL);

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if ((rval = initialize_listener(&addr, &ssock) < 0)) {
        queue_destroy(&requestq);
        hashmap_destroy(&hostname_cache);
        return rval;
    }

    /* Serve until terminated */
    printl(LOG_INFO "Webproxy started on port %d\n", port);
    rval = proxy(ssock);

    /* Unblock waiting threads */
    for (int i = 0; i < nthreads; i++)
        queue_signal_available(&requestq);

    /* Wait for worker threads to exit */
    printl(LOG_INFO "Exiting...\n");
    pthread_join(cache_gc_thread, NULL);
    for (int i = 0; i < nthreads; i++)
        pthread_join(threads[i], NULL);

    queue_destroy(&requestq);
    hashmap_destroy(&hostname_cache);
    close(ssock);

    return rval;
}


int proxy(int ssock)
{
    int rval, fd, ready, on = 1;
    struct sockaddr_in caddr;
    socklen_t addr_sz = sizeof(struct sockaddr_in);
    fd_set readfds_master, readfds;

    FD_ZERO(&readfds_master);
    FD_SET(ssock, &readfds_master);

    /* Watch for data on listener socket and spawn connection handler */
    while (!exit_requested) {
        readfds = readfds_master;

        ready = pselect(ssock + 1, &readfds, NULL, NULL, NULL, NULL);
        if (ready < 0 && errno != EINTR) {
            printl(LOG_ERR "pselect - %s\n", strerror(errno));
            break;
        } else if (exit_requested) {
            printl(LOG_DEBUG "Caught SIGINT\n");
        } else if (ready) {
            fd = accept(ssock, (struct sockaddr *)&caddr, &addr_sz);
            if (fd < 0) {
                printl(LOG_ERR "accept - %s\n", strerror(errno));
                break;
            }

            printl(LOG_DEBUG "Connection accepted on socket %d\n", fd);

            rval = setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on));
            if (rval < 0) {
                printl(LOG_WARN "setsockopt - %s\n", strerror(errno));
                break;
            }

            /* Spawn connection handler */
            pthread_t thread;
            if (pthread_create(&thread, NULL, handle_connection, (void *)&fd) < 0) {
                printl(LOG_ERR "pthread_create - %s\n", strerror(errno));
                return errno;
            }
            pthread_detach(thread);
        } else {
            printl(LOG_WARN "pselect returned 0 on listener socket\n");
        }
    }

    return 0;
}


int build_request(request_t *req)
{
    int nrecv, nrecvd, nunparsed;
    char reqbuf[REQ_BUFLEN] = "";

    nunparsed = 0;
    nrecv = REQ_BUFLEN;
    while ((nrecvd = read(req->client_fd, reqbuf + nunparsed, nrecv)) > 0) {
        reqbuf[nunparsed + nrecvd] = '\0';
        nunparsed = request_deserialize(req, reqbuf, nunparsed + nrecvd);
        if (nunparsed < 0) {
            send_error(req, 400);
            return 1;
        }
        nrecv = REQ_BUFLEN - nunparsed;
        if (req->complete) {
            printl(LOG_DEBUG "Request complete\n");
            break;
        }
    }

    if (nrecvd <= 0) {
        printl(LOG_DEBUG "Connection closed\n");
        if (nrecvd == 0 && nunparsed == REQ_BUFLEN) {
            send_error(req, 431);
        } else if (nrecvd == -1) {
            printl(LOG_WARN "read - %s\n", strerror(errno));
            send_error(req, 500);
        }

        return 1;
    }

    assert(req->complete);

    return 0;
}


void *handle_connection(void *fd_vptr)
{
    int fd = *(int *)fd_vptr;
    request_t req = {0};
    struct sockaddr_in peer_addr;
    socklen_t addr_len = sizeof(peer_addr);
    const struct timespec one_second = { .tv_sec = 1, .tv_nsec = 0 };
    fd_set readfds_master, readfds;
    int rval, timer, ready;
    bool keepalive;
    char requested_file_path[REQ_BUFLEN] = "";

    if ((getpeername(fd, (struct sockaddr *)&peer_addr, &addr_len)) < 0) {
        printl(LOG_WARN "getpeername failed - %s\n", strerror(errno));
        pthread_exit(NULL);
    }

    /* If keep-alive requested, watch fd for KEEPALIVE_TIMEOUT_S seconds */
    FD_ZERO(&readfds_master);
    FD_SET(fd, &readfds_master);

    do {
        timer = 1;
        request_destroy(&req);  /* safe to call on uninit'd req struct */
        request_init(&req, fd, &peer_addr);

        if (build_request(&req) != 0)
            break;

        keepalive = request_conn_is_keepalive(&req);

        rval = request_lookup_host(&req);
        if (rval == -1) {
            send_error(&req, 404);
            break;
        }

        printl(LOG_INFO "(%d) %s %s %s\n", fd, req.ip, req.method,
               req.url->full);

        strcpy(requested_file_path, PROXY_ROOT);

        char *path = malloc(strlen(req.url->path + 3));
        if (request_method_is_get(&req)) {
            if (hashmap_get(&file_cache, req.url->path, (char **) &path) != -1) {
                if (send_file(&req, path) < 0) {
                    send_error(&req, 404);
                    break;
                }
            }

            /* Open socket to req.url->host:req.url->port */
            int sock;
            struct sockaddr_in server;

            /* FIXME printf etc */
            sock = socket(AF_INET , SOCK_STREAM , 0);
            if (sock == -1) {
                printf("Could not create socket");
            }
            puts("Socket created");

            server.sin_addr.s_addr = inet_addr(req.url->host);
            server.sin_family = AF_INET;
            server.sin_port = htons(req.url->port);

            /* Connect to remote server */
            /* FIXME: perror */
            if (connect(sock , (struct sockaddr *)&server , sizeof(server)) < 0) {
                perror("connect failed. Error");
            }

            /* Send FULL request to above */
            /* write(req.raw, ); */

            /* TODO - read response from remote */
            /* TODO - write response to requester */
            /* TODO - if response is 200 - cache file */

        } else {
            send_error(&req, 405);
            break;
        }

        while (keepalive) {
            readfds = readfds_master;
            ready = pselect(fd + 1, &readfds, NULL, NULL, &one_second, NULL);
            if (exit_requested) {
                keepalive = false;
            } else if (ready == -1) {
                printl(LOG_WARN "pselect - %s\n", strerror(errno));
                keepalive = false;
            } else if (ready) {
                printl(LOG_DEBUG "Reusing keep-alive socket %d\n", fd);
                break;
            } else {
                if (++timer > KEEPALIVE_TIMEOUT_S) {
                    printl(LOG_DEBUG "Closing keep-alive socket %d\n", fd);
                    keepalive = false;
                }
            }
        }

    } while (keepalive);

    close(fd);
    request_destroy(&req);
    pthread_exit(NULL);
}


void *handle_request(void *param)
{
    (void)param;
    request_t req = {0};
    const unsigned int thread_id = pthread_self();

    while (!exit_requested) {
        printl(LOG_DEBUG "Thread %u waiting for request\n", thread_id);
        queue_wait_if_empty(&requestq);
        if (exit_requested) {
            printl(LOG_DEBUG "Request handler %u terminated\n", thread_id);
            break;
        }

        req = queue_get(&requestq);
    }

    printl(LOG_DEBUG "Worker thread %u exiting\n", thread_id);
    pthread_exit(NULL);
}



/* Return total bytes sent or -1. */
int send_file(request_t *req, char *path)
{
    response_t res;
    char *resbuf;
    size_t resbuflen, clen;
    FILE *file;
    char *fileext;
    char filebuf[RES_BUFLEN] = "";
    const char *ctype;
    int ntotal = 0, nsend, nsent;

    if ((file = fopen(path, "rb")) == NULL) {
        printl(LOG_DEBUG "Failed to open %s - %s\n", path, strerror(errno));
        return -1;
    }

    /* Set Content-Length */
    struct stat st;
    stat(path, &st);
    clen = st.st_size;

    /* Set Content-Type */
    fileext = strrchr(path, '.');
    if (fileext && !strcmp(fileext, ".html"))
        ctype = "text/html";
    else if (fileext && !strcmp(fileext, ".txt"))
        ctype = "text/plain";
    else if (fileext && !strcmp(fileext, ".png"))
        ctype = "image/png";
    else if (fileext && !strcmp(fileext, ".gif"))
        ctype = "image/gif";
    else if (fileext && !strcmp(fileext, ".jpg"))
        ctype = "image/jpg";
    else if (fileext && !strcmp(fileext, ".css"))
        ctype = "text/css";
    else if (fileext && !strcmp(fileext, ".js"))
        ctype = "application/javascript";
    else
        ctype = "application/octet-stream";

    /* Send header */
    response_init_from_request(req, &res, 200, ctype, clen);
    response_serialize(&res, &resbuf, &resbuflen);
    ntotal += write(req->client_fd, resbuf, resbuflen);

    const char *msg = LOG_INFO "(%d) -> %s 200 %s %s (%lu)\n";
    /* drop .cache */
    printl(msg, req->client_fd, req->ip, path + 6, ctype, clen);

    /* Send body */
    while ((nsend = fread(filebuf, 1, RES_BUFLEN, file))) {
        nsent = write(req->client_fd, filebuf, nsend);
        ntotal+= nsent;
    }

    response_destroy(&res);
    fclose(file);

    return ntotal;
}


int send_error(request_t *req, int status)
{
    response_t res;
    char *resbuf;
    size_t resbuflen;
    int nsent;

    printl(LOG_INFO "(%d) -> %s %d\n", req->client_fd, req->ip, status);

    response_init_from_request(req, &res, status, NULL, 0);
    response_serialize(&res, &resbuf, &resbuflen);
    if ((nsent = write(req->client_fd, resbuf, resbuflen)) < 0)
        printl(LOG_WARN "Socket write failed - %s\n", strerror(errno));

    response_destroy(&res);
    return nsent;
}


int initialize_listener(struct sockaddr_in *saddr, int *fd)
{
    const int on = 1;
    socklen_t addr_sz = sizeof(struct sockaddr_in);

    /* Setup listener socket */
    *fd = socket(AF_INET, SOCK_STREAM|SOCK_CLOEXEC, IPPROTO_TCP);
    if (*fd == -1) {
        printl(LOG_ERR "socket - %s\n", strerror(errno));
        return errno;
    }

    if ((setsockopt(*fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on))) < 0) {
        printl(LOG_ERR "setsockopt - %s\n", strerror(errno));
        return errno;
    }
    printl(LOG_DEBUG "Created listener socket %d\n", *fd);

    if (bind(*fd, (struct sockaddr *)saddr, addr_sz) < 0) {
        printl(LOG_ERR "bind - %s\n", strerror(errno));
        return errno;
    }
    printl(LOG_DEBUG "Bind succeeded\n");

    if (listen(*fd, MAX_BACKLOG) < 0) {
        printl(LOG_ERR "listen - %s\n", strerror(errno));
        return errno;
    };

    return 0;
}


void parse_options(int argc, char *argv[], int *port, int *cache_timeout)
{
    int c;
    char *portstr, *timeoutstr;

    /* Parse cmdline options */
    while ((c = getopt_long(argc, argv, shortopts, longopts, 0)) != -1) {
        switch (c) {
        case 'h':
            printf(usage, argv[0]);
            exit(0);
            break;
        case 'd':
            printl_setlevel(DEBUG);
            break;
        case '?':
            /* handled by getopt */
            break;
        default:
            fprintf(stderr, usage, argv[0]);
            exit(EXIT_FAILURE);
        }
    }

    int n_posargs = argc - optind;
    if (!n_posargs || n_posargs > 2) {
        printf(usage, argv[0]);
        exit(EXIT_FAILURE);
    }

    /* Check port */
    portstr = argv[optind++];
    *port = atoi(portstr);
    if (*port < 1) {
        printl(LOG_FATAL "Invalid port `%s'\n", portstr);
        printf(usage, argv[0]);
        exit(EXIT_FAILURE);
    }

    if (n_posargs == 1) {
        *cache_timeout = DEFAULT_CACHE_TIMEOUT_S;
    } else {
        /* Check timeout */
        timeoutstr = argv[optind++];
        *cache_timeout = atoi(timeoutstr);
        if (*cache_timeout < 1) {
            printl(LOG_FATAL "Invalid cache timeout `%s'\n", timeoutstr);
            printf(usage, argv[0]);
            exit(EXIT_FAILURE);
        }
    }

    printl(LOG_DEBUG "Cache timeout set to %d seconds\n", *cache_timeout);
}


void *cache_gc(void *cache_vptr)
{
    hashmap_t *cache = (hashmap_t *) cache_vptr;
    unsigned int clk = 0;

    printl(LOG_DEBUG "Cache GC running\n");

    while (!exit_requested) {
        usleep(100000);         /* check exit_requested 10 times a second */
        if (!(clk++ % 10))
            hashmap_gc(cache);  /* run gc only once a second */
    }

    printl(LOG_DEBUG "Cache GC exiting\n");

    pthread_exit(NULL);
}


void url_to_cache_path(const url_t *url, char *path)
{
    char *p = strdup(url->path);

    /* Replace illegal path characters */
    for (int i = 0; p[i] != '\0'; i++)
        if (p[i] == '/')
            p[i] = '_';

    sprintf(path, "%s/%s/%s", PROXY_ROOT, url->host, p);
    free(p);
}
