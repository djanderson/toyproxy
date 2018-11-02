#ifndef REQUEST_H
#define REQUEST_H

#include <netinet/in.h>         /* struct sockaddr_in */
#include <stdbool.h>            /* bool */
#include <stdlib.h>             /* size_t */
#include <string.h>             /* strcmp, strcasecmp */

#define REQ_BUFLEN 1000


struct request {
    int fd;
    bool complete;
    char *ip;
    char *method;
    char *uri;
    char *version;
    char *connection;
    char *content_length;
};


void request_init(struct request *req, int fd, const struct sockaddr_in *addr);
void request_destroy(struct request *req);
/* Return the number of bytes not consumed from buf or -1 for error. */
size_t request_deserialize(struct request *req, char *buf, size_t buflen);
/* Return 1 for parse error or 0 for success. */
int request_deserialize_line(struct request *req, const char *line);


static inline bool request_method_is_get(struct request *req)
{
    return !strcmp(req->method, "GET");
}


static inline bool request_method_is_post(struct request *req)
{
    return !strcmp(req->method, "POST");
}


static inline bool request_uri_is_root(struct request *req)
{
    return !strcmp(req->uri, "/");
}


static inline bool request_version_is_1_1(struct request *req)
{
    return !strcasecmp(req->version, "HTTP/1.1");
}


static inline bool request_conn_is_keepalive(struct request *req)
{
    return ((req->connection == NULL && request_version_is_1_1(req)) ||
            (req->connection && !strcasecmp(req->connection, "keep-alive")));
}


#endif  /* REQUEST_H */
