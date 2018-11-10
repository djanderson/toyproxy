#ifndef REQUEST_H
#define REQUEST_H

#include <netinet/in.h>         /* struct sockaddr_in */
#include <stdbool.h>            /* bool */
#include <stdlib.h>             /* size_t */
#include <string.h>             /* strcmp, strcasecmp */

#include "hashmap.h"
#include "url.h"

#define REQ_BUFLEN 1000


typedef struct request {
    int fd;
    bool complete;
    char *ip;
    char *method;
    url_t *url;
    char *version;
    char *connection;
    char *content_length;
} request_t;

hashmap_t hostname_cache;

void request_init(request_t *req, int fd, const struct sockaddr_in *addr);
void request_destroy(request_t *req);
/* Return the number of bytes not consumed from buf or -1 for error. */
size_t request_deserialize(request_t *req, char *buf, size_t buflen);
/* Return -1 for parse error or 0 for success. */
int request_deserialize_line(request_t *req, const char *line);
/*
 * Return -1 for invalid host, 0 for cache miss, and 1 for cache hit.
 *
 * Also return 1 if req->url->host is already a valid IP.
 */
int request_lookup_host(request_t *req);


static inline bool request_method_is_get(request_t *req)
{
    return !strcmp(req->method, "GET");
}


static inline bool request_method_is_post(request_t *req)
{
    return !strcmp(req->method, "POST");
}


static inline bool request_path_is_dir(request_t *req)
{
    char *c = req->url->path;

    while (++req->url->path)
        c = req->url->path;

    return *c == '/';
}


static inline bool request_version_is_1_1(request_t *req)
{
    return !strcasecmp(req->version, "HTTP/1.1");
}


static inline bool request_conn_is_keepalive(request_t *req)
{
    return ((req->connection == NULL && request_version_is_1_1(req)) ||
            (req->connection && !strcasecmp(req->connection, "keep-alive")));
}


#endif  /* REQUEST_H */
