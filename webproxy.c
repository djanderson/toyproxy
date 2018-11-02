#include <arpa/inet.h>          /* inet_ntoa */
#include <assert.h>             /* assert */
#include <errno.h>              /* errno */
#include <getopt.h>             /* getopt_long, struct option, no_argument */
#include <netdb.h>              /* gethostbyname */
#include <netinet/tcp.h>        /* TCP_NODELAY */
#include <pthread.h>            /* pthread_* */
#include <signal.h>             /* sigset_t, sigaction */
#include <stdlib.h>             /* size_t */
#include <string.h>             /* memset */
#include <stdio.h>              /* printf, fprintf */
#include <sys/socket.h>         /* setsockopt */
#include <sys/stat.h>           /* stat, struct st */
#include <unistd.h>             /* close, read, write */

#include "connectionq.h"
#include "hash_map.h"
#include "printl.h"
#include "request.h"
#include "response.h"

#define PROXY_ROOT "www"
#define MAX_THREADS 15
#define MAX_BACKLOG 100         /* Max connections before ECONNREFUSED error */
#define TIMEOUT 10              /* Keep-alive timeout in seconds */


/* Command line options */
const char usage[] = "USAGE: %s [-h] <port>\n";
const char shortopts[] = "hd";
const struct option longopts[] = {
    {"help", no_argument, 0, 'h'},
    {"debug", no_argument, 0, 'd'}
};

static volatile bool exit_requested = false;


static void signal_handler(int __attribute__((__unused__)) sig)
{
    exit_requested = true;
}

/* Parse command line options. */
void parse_options(int argc, char *argv[], int *port);
/* Setup the listener socket. */
int initialize_listener(struct sockaddr_in *saddr, int *fd);
/* Watch for incoming socket connections and place on queue. */
int serve(struct connectionq *q, int ssock);
/* Consume connections from the queue. */
void *worker(void *queue);
/* Handle a single connection until connection close or keep-alive timeout. */
void handle_connection(int fd, struct sockaddr_in *peer_addr);
/* Send an HTTP error response (no body). */
int send_error(struct request *req, int status);
/* Send an HTTP response including the file at `path'. */
int send_file(struct request *req, char *path);


void test_hash_map()
{
    hash_map_t map;
    char *ip, *hostname;
    char *hostnames[] = {
        "google.com",
        "youtube.com",
        "facebook.com",
        "wikipedia.com",
        "reddit.com",
        "yahoo.com",
        "amazon.com",
        "twitter.com",
        "instagram.com",
        "microsoft.com",
        "bing.com",
        "office.com",
        "ebay.com",
        "msn.com",
        "spotify.com",
    };
    struct hostent *host;

    hash_map_init(&map, 10);

    printl(LOG_DEBUG "\nAdding hostnames to cache\n");
    for (int i = 0; i < 15; i++) {
        hostname = hostnames[i];
        host = gethostbyname(hostname);
        if (host == NULL) {
            herror("gethostbyname");
            continue;
        }

        ip = inet_ntoa(*(struct in_addr *) host->h_addr);
        int idx1 = hash_map_add(&map, hostname, ip);
        printl(LOG_DEBUG "map[%s]@%d = %s\n", hostname, idx1, ip);
    }

    printl(LOG_DEBUG "\nReading hostnames from cache\n");
    for (int i = 0; i < 15; i++) {
        hostname = hostnames[i];
        int idx2 = hash_map_get(&map, hostname, &ip);
        printl(LOG_DEBUG "map[%s]@%d = %s\n", hostname, idx2, ip);
    }

    printl(LOG_DEBUG "\nRemoving hostnames from cache\n");
    for (int i = 0; i < 15; i++) {
        hostname = hostnames[i];
        int idx3 = hash_map_del(&map, hostname);
        printl(LOG_DEBUG "del map[%s]@%d\n", hostname, idx3);
    }

    hash_map_destroy(&map);
}

int main(int argc, char *argv[])
{
    int rval, ssock, port;
    pthread_t threads[MAX_THREADS];
    struct sockaddr_in addr;
    struct connectionq q;
    int nthreads = sysconf(_SC_NPROCESSORS_CONF);

    if (nthreads > MAX_THREADS)
        nthreads = MAX_THREADS;

    printl_setlevel(DEBUG);

    test_hash_map();

    parse_options(argc, argv, &port);

    signal(SIGINT, signal_handler);

    /* Initialize connection queue */
    connectionq_init(&q, nthreads);

    /* Spawn worker thread pool */
    printl(LOG_DEBUG "Initializing %d worker threads\n", nthreads);
    for (int i = 0; i < nthreads; i++) {
        if (pthread_create(&threads[i], NULL,  worker, (void *)&q) < 0) {
            printl(LOG_ERR "pthread_create - %s\n", strerror(errno));
            connectionq_destroy(&q);
            return errno;
        }
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if ((rval = initialize_listener(&addr, &ssock) < 0)) {
        connectionq_destroy(&q);
        return rval;
    }

    /* Serve until terminated */
    printl(LOG_INFO "Webproxy started on port %d\n", port);
    rval = serve(&q, ssock);

    /* Unblock waiting threads */
    for (int i = 0; i < nthreads; i++)
        connectionq_signal_available(&q);

    /* Wait for worker threads to exit */
    printl(LOG_INFO "Waiting for workers to exit...\n");
    for (int i = 0; i < nthreads; i++)
        pthread_join(threads[i], NULL);

    connectionq_destroy(&q);
    close(ssock);

    return rval;
}


int serve(struct connectionq *q, int ssock)
{
    int rval, fd, ready, on = 1;
    struct sockaddr_in caddr;
    socklen_t addr_sz = sizeof(struct sockaddr_in);
    fd_set readfds_master, readfds;

    FD_ZERO(&readfds_master);
    FD_SET(ssock, &readfds_master);

    /* Watch for data on listener socket and add to connection queue */
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

            connectionq_put(q, fd);
        } else {
            printl(LOG_WARN "pselect returned 0 on listener socket\n");
        }
    }

    return 0;
}


void *worker(void *queue)
{
    struct connectionq *q = (struct connectionq *)queue;
    int fd;
    struct sockaddr_in peer_addr;
    socklen_t addr_len = sizeof(peer_addr);
    const unsigned int thread_id = pthread_self();

    while (!exit_requested) {
        printl(LOG_DEBUG "Thread %u waiting for connections\n", thread_id);
        connectionq_wait_if_empty(q);
        if (exit_requested) {
            printl(LOG_DEBUG "Thread %u terminated\n", thread_id);
            break;
        }

        fd = connectionq_get(q);
        if ((getpeername(fd, (struct sockaddr *)&peer_addr, &addr_len)) < 0) {
            printl(LOG_WARN "getpeername failed - %s\n", strerror(errno));
            break;
        }

        printl(LOG_DEBUG "Thread %u handling socket %d\n", thread_id, fd);
        handle_connection(fd, &peer_addr);
        close(fd);
    }

    printl(LOG_DEBUG "Exiting worker thread %u\n", thread_id);
    pthread_exit(NULL);
}


/*
 * Pulled into separate function for readability but inlined into thread
 * worker, otherwise not much gain from thread pool.
 */
inline void handle_connection(int fd, struct sockaddr_in *peer_addr)
{
    struct request req = {0};
    const struct timespec one_second = { .tv_sec = 1, .tv_nsec = 0 };
    int timer;
    fd_set readfds_master, readfds;
    int nrecv, nrecvd, nunparsed, ready;
    bool keepalive;
    char reqbuf[REQ_BUFLEN] = "";
    char requested_file_path[REQ_BUFLEN] = "";

    /* If keep-alive requested, watch fd for TIMEOUT seconds */
    FD_ZERO(&readfds_master);
    FD_SET(fd, &readfds_master);

    do {
        timer = 1;
        request_destroy(&req);  /* safe to call on uninit'd req struct */
        request_init(&req, fd, peer_addr);

        nunparsed = 0;
        nrecv = REQ_BUFLEN;
        while ((nrecvd = read(fd, reqbuf + nunparsed, nrecv)) > 0) {
            reqbuf[nunparsed + nrecvd] = '\0';
            nunparsed = request_deserialize(&req, reqbuf, nunparsed + nrecvd);
            if (nunparsed < 0) {
                send_error(&req, 400);
                continue;
            }
            nrecv = REQ_BUFLEN - nunparsed;
            if (req.complete)
                break;
        }

        if (nrecvd <= 0) {
            printl(LOG_DEBUG "Connection closed\n");
            if (nrecvd == 0 && nunparsed == REQ_BUFLEN)
                send_error(&req, 431);
            else if (nrecvd == -1)
                printl(LOG_WARN "read - %s\n", strerror(errno));

            break;
        }

        assert(req.complete);
        keepalive = request_conn_is_keepalive(&req);
        printl(LOG_INFO "(%d) %s %s %s\n", fd, req.ip, req.method,
               req.uri);

        strcpy(requested_file_path, PROXY_ROOT);

        if (request_method_is_get(&req)) {
            if (send_file(&req, requested_file_path) < 0)
                send_error(&req, 404);
        } else {
            send_error(&req, 405);
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
                if (++timer > TIMEOUT) {
                    printl(LOG_DEBUG "Closing keep-alive socket %d\n", fd);
                    keepalive = false;
                }
            }
        }

    } while (keepalive);

    request_destroy(&req);
}


/* Return total bytes sent or -1. */
int send_file(struct request *req, char *path)
{
    struct response res;
    char buf[RES_BUFLEN] = "";
    size_t buflen, clen;
    FILE *file;
    char *fileext;
    const char *ctype;
    int ntotal = 0, nsend, nsent;

    /* Don't allow client to read above server root */
    if (strstr(path, "/../") != NULL) {
        printl(LOG_DEBUG "Invalid uri path includes `/../'\n");
        return -1;
    }

    if (request_uri_is_root(req))
        strcat(path, "/index.html");
    else
        strcat(path, req->uri);

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
    response_init(req, &res, 200, ctype, clen);
    buflen = response_serialize(&res, buf, RES_BUFLEN);
    ntotal += write(req->fd, buf, buflen);

    const char *msg = LOG_INFO "(%d) -> %s 200 %s %s (%lu)\n";
    printl(msg, req->fd, req->ip, path + 3, ctype, clen); /* drop www */

    /* Send body */
    while ((nsend = fread(buf, 1, RES_BUFLEN, file))) {
        nsent = write(req->fd, buf, nsend);
        ntotal+= nsent;
    }

    response_destroy(&res);
    fclose(file);

    return ntotal;
}


int send_error(struct request *req, int status)
{
    struct response res;
    char buf[RES_BUFLEN] = "";
    int nsent, buflen;

    printl(LOG_INFO "(%d) -> %s %d\n", req->fd, req->ip, status);

    response_init(req, &res, status, NULL, 0);
    buflen = response_serialize(&res, buf, RES_BUFLEN);
    if ((nsent = write(req->fd, buf, buflen)) < 0)
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


void parse_options(int argc, char *argv[], int *port)
{
    int c;
    char *portstr;

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
    if (n_posargs != 1) {
        printf(usage, argv[0]);
        exit(EXIT_FAILURE);
    }

    /* Check port */
    portstr = argv[optind++];
    *port = atoi(portstr);
    if (!*port) {
        printl(LOG_FATAL "Invalid port `%s'\n", *portstr);
        printf(usage, argv[0]);
        exit(EXIT_FAILURE);
    }
}