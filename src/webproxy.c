#include <arpa/inet.h>          /* inet_ntoa */
#include <assert.h>             /* assert */
#include <errno.h>              /* errno */
#include <getopt.h>             /* getopt_long, struct option, no_argument */
#include <netdb.h>              /* gethostbyname */
#include <netinet/tcp.h>        /* TCP_NODELAY */
#include <pthread.h>            /* pthread_* */
#include <signal.h>             /* sigset_t, sigaction */
#include <stdatomic.h>          /* atomic_ */
#include <stdlib.h>             /* size_t, strtoul */
#include <string.h>             /* memset */
#include <stdio.h>              /* printf, fprintf */
#include <sys/socket.h>         /* setsockopt */
#include <sys/stat.h>           /* stat, struct st */
#include <unistd.h>             /* close, read, write */

#include "hashmap.h"
#include "printl.h"
#include "request.h"
#include "response.h"

#define CACHE_ROOT ".cache"
#define BLACKLIST_FILE "blacklist.txt"
#define DIR_PERMS 0700
#define MAX_BACKLOG 100         /* Max connections before ECONNREFUSED error */
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
atomic_int global_thread_count = 0;
__thread int thread_id;

hashmap_t file_cache;

/* If a requested URL or IP is in the blacklist, return 403 Forbidden. */
char **blacklist;

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
/* Send an HTTP error response (no body). */
int send_error(request_t *req, int status);
/* Save a response's content in at `path`. */
void save_cache_file(response_t *res, char *path);
/* Send an HTTP response including the file at `path'. */
int send_cache_file(request_t *req, char *path);
/* Handle cache timeout. */
void *cache_gc(void *cache_vptr);
/* Return heap-allocated string that the user must free. */
char *url_to_cache_path(const url_t *url);
/* Return true if a and b have the same IP and port. */
bool addrs_equal(struct sockaddr_in *a, struct sockaddr_in *b);
/* Load blacklist.txt into blacklist character array. */
int blacklist_init();
/* Free blacklist memory. */
void blacklist_destroy();
/* Return true if the requested URL or IP is on the blacklist. */
bool blacklist_has_entry(request_t *req);


int main(int argc, char *argv[])
{
    int rval, ssock, port, cache_timeout;
    pthread_t cache_gc_thread;
    sigset_t set;
    struct stat st;
    struct sockaddr_in addr;

    thread_id = global_thread_count++;

    printl_setlevel(INFO);

    parse_options(argc, argv, &port, &cache_timeout);

    signal(SIGINT, signal_handler);
    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    pthread_sigmask(SIG_BLOCK, &set, NULL);

    hashmap_init(&hostname_cache, 100);
    hashmap_init(&file_cache, 100);
    file_cache.timeout = cache_timeout;
    file_cache.unlinker = unlink;       /* unlink cached files on timeout */

    if (stat(CACHE_ROOT, &st) == -1) {
        mkdir(CACHE_ROOT, DIR_PERMS);
    }

    /* Spawn cache timeout handler */
    if (pthread_create(&cache_gc_thread, NULL, cache_gc,
                       (void *)&file_cache) < 0) {
        printl(LOG_ERR "pthread_create - %s\n", strerror(errno));
        hashmap_destroy(&hostname_cache);
        hashmap_destroy(&file_cache);
        return errno;
    }

    pthread_sigmask(SIG_UNBLOCK, &set, NULL);

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if ((rval = initialize_listener(&addr, &ssock) < 0)) {
        hashmap_destroy(&hostname_cache);
        hashmap_destroy(&file_cache);
        return rval;
    }

    if (blacklist_init() == -1)
        printl(LOG_ERR "Failed to load blacklist from %s\n", BLACKLIST_FILE);

    /* Serve until terminated */
    printl(LOG_INFO "Webproxy started on port %d\n", port);
    rval = proxy(ssock);

    /* Wait for worker threads to exit */
    printl(LOG_INFO "Exiting...\n");
    pthread_join(cache_gc_thread, NULL);

    close(ssock);
    hashmap_destroy(&hostname_cache);
    hashmap_destroy(&file_cache);
    blacklist_destroy();

    return rval;
}


int proxy(int ssock)
{
    int rval, ready;
    int *fd;
    pthread_t thread;
    struct sockaddr_in client_addr;
    socklen_t addr_sz = sizeof(struct sockaddr_in);
    fd_set readfds_master, readfds;
    char *msg;
    int id = thread_id;

    FD_ZERO(&readfds_master);
    FD_SET(ssock, &readfds_master);

    /* Watch for data on listener socket and spawn connection handler */
    while (!exit_requested) {
        readfds = readfds_master;

        ready = pselect(ssock + 1, &readfds, NULL, NULL, NULL, NULL);
        if (ready < 0 && errno != EINTR) {
            printl(LOG_ERR "[%d] pselect - %s\n", id, strerror(errno));
            break;
        } else if (exit_requested) {
            printl(LOG_DEBUG "[%d] Caught SIGINT\n", id);
        } else if (ready) {
            fd = malloc(sizeof(int *));
            *fd = accept(ssock, (struct sockaddr *)&client_addr, &addr_sz);
            if (*fd < 0) {
                printl(LOG_ERR "[%d] accept - %s\n", id, strerror(errno));
                break;
            }

            msg = LOG_DEBUG "[%d] Connection accepted on socket %d\n";
            printl(msg, id, *fd);

            /* Spawn connection handler */
            rval = pthread_create(&thread, NULL, handle_connection, fd);
            if (rval < 0) {
                msg = LOG_ERR "[%d] pthread_create - %s\n";
                printl(msg, id, strerror(errno));
                return errno;
            }
            pthread_detach(thread);
        } else {
            msg = LOG_WARN "[%d] pselect returned 0 on listener socket\n";
            printl(msg, id);
        }
    }

    return 0;
}


void *handle_connection(void *cfd_vptr)
{
    int cfd = *(int *)cfd_vptr; /* client socket fd */
    int sfd = -1;               /* server socket fd */
    int rval, timer, ready;
    char *path, *msg;
    char cache_dir[REQ_BUFLEN] = "";
    fd_set readfds_master, readfds;
    bool keepalive;
    request_t req = {0};
    response_t res = {0};
    struct stat st = {0};
    struct sockaddr_in client_addr;
    struct sockaddr_in server_addr;
    struct sockaddr_in current_server_addr = {0}; /* open sock addr */
    const struct timespec one_second = { .tv_sec = 1, .tv_nsec = 0 };
    socklen_t addr_sz = sizeof(struct sockaddr_in);
    int id = thread_id = global_thread_count++;

    free(cfd_vptr);

    if ((getpeername(cfd, (struct sockaddr *)&client_addr, &addr_sz)) < 0) {
        printl(LOG_WARN "[%d] getpeername failed - %s\n", id, strerror(errno));
        pthread_exit(NULL);
    }

    FD_ZERO(&readfds_master);
    FD_SET(cfd, &readfds_master);

    printl(LOG_DEBUG "[%d] Handling connection on socket %d\n", id, cfd);

    /* If keep-alive requested, watch fd for KEEPALIVE_TIMEOUT_S seconds */
    do {
        timer = 1;
        request_destroy(&req);  /* safe to call on uninit'd req struct */
        request_init(&req, cfd, &client_addr);
        req.thread_id = id;

        if ((rval = request_read(&req) != 0)) {
            if (rval >= 100 && rval <= 599)
                send_error(&req, rval);

            break;
        }

        keepalive = request_conn_is_keepalive(&req);

        rval = request_lookup_host(&req);
        if (rval == -1) {
            send_error(&req, 404);
            break;
        }

        if (blacklist_has_entry(&req)) {
            msg = LOG_WARN "[%d] Requested URL %s or IP %s is blacklisted\n";
            printl(msg, id, req.url->host, req.url->ip);
            send_error(&req, 403);
            break;
        }

        printl("%s %s %s\n", req.ip, req.method, req.url->full);

        /* Only GET required to implement at this time */
        if (!request_method_is_get(&req)) {
            send_error(&req, 405);
            break;
        }

        if (hashmap_get(&file_cache, req.url->full, (char **) &path) != -1) {
            printl(LOG_DEBUG "[%d] Cache hit: %s\n", id, path);
            rval = send_cache_file(&req, path);
            free(path);
            if (rval < 0) {
                send_error(&req, 404);
                break;
            }
            continue;
        }

        server_addr.sin_addr.s_addr = inet_addr(req.url->ip);
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(req.url->port);

        /* Open socket to req.url->host:req.url->port if not already open */
        if (!addrs_equal(&current_server_addr, &server_addr)) {
            if (sfd > -1) {
                /* close the open socket */
                printl(LOG_DEBUG "[%d] Closing socket %d\n", id, sfd);
                close(sfd);
            }

            sfd = socket(AF_INET, SOCK_STREAM, 0);
            req.server_fd = sfd;
            if (sfd == -1) {
                printl(LOG_ERR "[%d] socket - %s", id, strerror(errno));
                break;
            }
            msg = LOG_DEBUG "[%d] Socket %d opened for %s\n";
            printl(msg, id, sfd, req.url->host);

            /* Connect to remote server */
            rval = connect(sfd, (struct sockaddr *)&server_addr , addr_sz);
            if (rval < 0) {
                printl(LOG_ERR "[%d] connect - %s", id, strerror(errno));
                break;
            }
            msg = LOG_DEBUG "[%d] Socket %d connected to %s\n";
            printl(msg, id, sfd, req.url->host);

            memcpy(&current_server_addr, &server_addr, addr_sz);
        }

        /* Send full request to above */
        msg = LOG_DEBUG "[%d] Forwarding request to %s on socket %d\n";
        printl(msg, id, req.url->host, sfd);
        write(sfd, req.raw, req.raw_len);

        response_init(&res);
        res.thread_id = id;

        /* Read response from remote */
        msg = LOG_DEBUG "[%d] Waiting for response from %s on socket %d\n";
        printl(msg, id, req.url->host, sfd);
        if ((rval = response_read(&res, sfd) != 0)) {
            if (rval >= 100 && rval <= 599)
                send_error(&req, rval); /* send error back to requester */

            response_destroy(&res);
            break;
        }

        /* Write response to requester */
        msg = LOG_DEBUG "[%d] Forwarding response from %s to %s on socket %d\n";
        printl(msg, id, req.url->host, req.ip, cfd);
        write(cfd, res.raw, res.raw_len);

        /* If response is 200, cache file */
        if (response_ok(&res)) {
            /* Ensure a cache directory exists for this host */
            sprintf(cache_dir, "%s/%s", CACHE_ROOT, req.url->host);
            if (stat(cache_dir, &st) == -1) {
                mkdir(cache_dir, DIR_PERMS);
            }

            path = url_to_cache_path(req.url);
            save_cache_file(&res, path);
            hashmap_add(&file_cache, req.url->full, path);
            printl(LOG_DEBUG "[%d] Cache entry created: %s\n", id, path);
            free(path);
        }

        response_destroy(&res);

        while (keepalive) {
            readfds = readfds_master;
            ready = pselect(cfd + 1, &readfds, NULL, NULL, &one_second, NULL);
            if (exit_requested) {
                keepalive = false;
            } else if (ready == -1) {
                printl(LOG_WARN "[%d] pselect - %s\n", id, strerror(errno));
                keepalive = false;
            } else if (ready) {
                msg = LOG_DEBUG "[%d] Reusing keep-alive socket %d\n";
                printl(msg, id, cfd);
                break;
            } else {
                if (++timer > KEEPALIVE_TIMEOUT_S) {
                    printl(LOG_DEBUG "[%d] Keep-alive timeout\n", id);
                    keepalive = false;
                }
            }
        }

    } while (keepalive);

    printl(LOG_DEBUG "[%d] Closing socket %d\n", id, cfd);
    close(cfd);

    if (sfd > 0) {
        printl(LOG_DEBUG "[%d] Closing socket %d\n", id, sfd);
        close(sfd);
    }

    request_destroy(&req);
    pthread_exit(NULL);
}


/* Return total bytes sent or -1. */
int send_cache_file(request_t *req, char *path)
{
    response_t res;
    char *msg, *resbuf;
    size_t resbuflen, clen;
    FILE *file;
    struct stat st;
    char *fileext;
    char filebuf[RES_BUFLEN] = "";
    const char *ctype;
    int ntotal = 0, nsend, nsent;
    int id = thread_id;

    if ((file = fopen(path, "r")) == NULL) {
        msg = LOG_DEBUG "[%d] Failed to open %s - %s\n";
        printl(msg, id, path, strerror(errno));
        return -1;
    }

    /* Set Content-Length */
    stat(path, &st);
    clen = st.st_size;

    /* Set Content-Type */
    fileext = strrchr(req->url->path, '.');
    if (fileext && !strcmp(fileext, ".png"))
        ctype = "image/png";
    else if (fileext && !strcmp(fileext, ".txt"))
        ctype = "text/plain";
    else if (fileext && !strcmp(fileext, ".gif"))
        ctype = "image/gif";
    else if (fileext && !strcmp(fileext, ".jpg"))
        ctype = "image/jpg";
    else if (fileext && !strcmp(fileext, ".css"))
        ctype = "text/css";
    else if (fileext && !strcmp(fileext, ".js"))
        ctype = "application/javascript";
    else
        ctype = "text/html";

    /* Send header */
    response_init_from_request(req, &res, 200, ctype, clen);
    response_serialize(&res, &resbuf, &resbuflen);
    ntotal += write(req->client_fd, resbuf, resbuflen);
    free(resbuf);

    printl("-> %s 200 %s %s (%lu)\n", req->ip, path, ctype, clen);

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
    char *msg, *resbuf;
    size_t resbuflen;
    int nsent;
    int id = thread_id;

    response_init_from_request(req, &res, status, NULL, 0);
    response_serialize(&res, &resbuf, &resbuflen);

    printl("-> %s %s\n", req->ip, res.header.status_line);

    if ((nsent = write(req->client_fd, resbuf, resbuflen)) < 0) {
        msg = LOG_WARN "[%d] Socket write failed - %s\n";
        printl(msg, id, strerror(errno));
    }

    free(resbuf);
    response_destroy(&res);
    return nsent;
}


int initialize_listener(struct sockaddr_in *saddr, int *fd)
{
    const int on = 1;
    socklen_t addr_sz = sizeof(struct sockaddr_in);
    int id = thread_id;

    /* Setup listener socket */
    *fd = socket(AF_INET, SOCK_STREAM|SOCK_CLOEXEC, IPPROTO_TCP);
    if (*fd == -1) {
        printl(LOG_ERR "[%d] socket - %s\n", id, strerror(errno));
        return errno;
    }

    if ((setsockopt(*fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on))) < 0) {
        printl(LOG_ERR "[%d] setsockopt - %s\n", id, strerror(errno));
        return errno;
    }
    printl(LOG_DEBUG "[%d] Created listener socket %d\n", id, *fd);

    if (bind(*fd, (struct sockaddr *)saddr, addr_sz) < 0) {
        printl(LOG_ERR "[%d] bind - %s\n", id, strerror(errno));
        return errno;
    }
    printl(LOG_DEBUG "[%d] Bind succeeded\n", id);

    if (listen(*fd, MAX_BACKLOG) < 0) {
        printl(LOG_ERR "[%d] listen - %s\n", id, strerror(errno));
        return errno;
    };

    return 0;
}


void parse_options(int argc, char *argv[], int *port, int *cache_timeout)
{
    int c, id = thread_id;
    char *msg, *portstr, *timeoutstr;

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

    msg = LOG_DEBUG "[%d] Cache timeout set to %d seconds\n";
    printl(msg, id, *cache_timeout);
}


void *cache_gc(void *cache_vptr)
{
    hashmap_t *cache = (hashmap_t *) cache_vptr;
    unsigned int clk = 0;
    int id = thread_id = global_thread_count++;

    printl(LOG_DEBUG "[%d] Cache GC running\n", id);

    while (!exit_requested) {
        usleep(100000);         /* check exit_requested 10 times a second */
        if (!(clk++ % 10))
            hashmap_gc(cache);  /* run gc only once a second */
    }

    printl(LOG_DEBUG "[%d] Cache GC exiting\n", id);

    pthread_exit(NULL);
}


char *url_to_cache_path(const url_t *url)
{
    char *p = strdup(url->path);
    char *cache_path = malloc(strlen(CACHE_ROOT) + strlen(url->host) +
                              strlen(url->path) + 3);

    /* Replace illegal path characters */
    for (int i = 0; p[i] != '\0'; i++)
        if (p[i] == '/')
            p[i] = '_';

    sprintf(cache_path, "%s/%s/%s", CACHE_ROOT, url->host, p);
    free(p);

    return cache_path;
}


void save_cache_file(response_t *res, char *path)
{
    size_t content_length, chunk_length;
    char *msg, *chunk_end;
    const char *chunk_start;
    FILE *file;
    int id = thread_id;

    if ((file = fopen(path, "w")) == NULL) {
        msg = LOG_WARN "[%d] Failed to open %s - %s\n";
        printl(msg, id, path, strerror(errno));
        return;
    }

    if (response_chunked(res)) {
        /* Response specified Transfer-Encoding: chunked */
        chunk_start = res->content;
        while ((chunk_length = strtoul(chunk_start, &chunk_end, 16))) {
            if (chunk_length == 0)
                break;

            chunk_start = chunk_end + 2; /* step over length and \r\n */

            if (fwrite(chunk_start, 1, chunk_length, file) != chunk_length) {
                msg = LOG_WARN "[%d] Failed to write to %s - %s";
                printl(msg, id, path, strerror(errno));
            }
        }
    } else {
        /* Response specified Content-Length: xxx */
        content_length = response_content_length(res);

        if (fwrite(res->content, 1, content_length, file) != content_length) {
            msg = LOG_WARN "[%d] Failed to write to %s - %s";
            printl(msg, id, path, strerror(errno));
        }
    }

    fclose(file);
}


bool addrs_equal(struct sockaddr_in *a, struct sockaddr_in *b)
{
    return (a->sin_addr.s_addr == b->sin_addr.s_addr &&
            a->sin_port == b->sin_port);
}


int blacklist_init()
{
    int nread;
    int list_len = 0;
    int blacklist_buffer_sz = 10;
    size_t len = 0;
    FILE *file;
    char *msg, *host = NULL;
    int id = thread_id;

    printl(LOG_DEBUG "[%d] Loading blacklist\n", id);

    if ((file = fopen(BLACKLIST_FILE, "r")) == NULL) {
        msg = LOG_ERR "[%d] fopen %s - %s\n";
        printl(msg, id, BLACKLIST_FILE, strerror(errno));
        return -1;
    }

    blacklist = calloc(blacklist_buffer_sz, sizeof(char *));

    while ((nread = getline(&host, &len, file)) != -1) {
        if (host[nread - 1] == '\n')
            host[nread - 1] = '\0';

        if (strlen(host) == 0 || host[0] == '#')
            continue;

        printl(LOG_DEBUG "[%d] Adding `%s' to blacklist\n", id, host);
        blacklist[list_len] = strdup(host);

        if (++list_len == blacklist_buffer_sz) {
            printl(LOG_DEBUG "[%d] Resizing blacklist array %d -> %d\n", id,
                   blacklist_buffer_sz, blacklist_buffer_sz + 10);
            blacklist_buffer_sz += 10;
            blacklist = realloc(blacklist, blacklist_buffer_sz);
        }
    }

    /* Null terminate blacklist */
    blacklist[list_len] = NULL;

    free(host);
    fclose(file);

    return 0;
}


void blacklist_destroy()
{
    int id = thread_id;

    printl(LOG_DEBUG "[%d] Freeing blacklist\n", id);

    for (int i = 0; blacklist[i] != NULL; i++)
        free(blacklist[i]);

    free(blacklist);
}


bool blacklist_has_entry(request_t *req)
{
    char *host;

    for (int i = 0; blacklist[i] != NULL; i++) {
        host = blacklist[i];
        if (!strcmp(req->url->host, host) || !strcmp(req->url->ip, host))
            return true;
    }

    return false;
}
