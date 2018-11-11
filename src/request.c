#include <arpa/inet.h>          /* inet_addr */
#include <netdb.h>              /* gethostbyname */
#include <string.h>             /* str* */
#include <sys/socket.h>         /* struct sockaddr */

#include "printl.h"
#include "request.h"


int request_deserialize(request_t *req, char *buf, size_t buflen)
{
    bool last_line, last_line_partial;
    int nunparsed;
    char *line, *previous_line;
    char *tmpbuf[REQ_BUFLEN + 1];
    char *orig_buf = buf;
    char *saveptr;
    const char delim[] = "\r\n";

    last_line_partial = (buflen > 2 && strcmp(buf + buflen - 2, "\r\n") != 0);
    req->complete = (buflen > 4 && strcmp(buf + buflen - 4, "\r\n\r\n") == 0);

    line = strtok_r(buf, delim, &saveptr);
    while (line != NULL) {
        previous_line = line;
        line = strtok_r(NULL, delim, &saveptr);
        last_line = line == NULL;
        if (!last_line || !last_line_partial)
            if (request_deserialize_line(req, previous_line))
                return -1;
    }

    buf = orig_buf;

    if (last_line_partial) {
        nunparsed = strlen(previous_line);
        strcpy((char *)tmpbuf, previous_line);
        strcpy(buf, (char *)tmpbuf);
        return nunparsed;
    }

    return 0;
}


int request_deserialize_line(request_t *req, const char *cline)
{
    printl(LOG_DEBUG "Got request line: %s\n", cline);

    int rval = 0;
    char *key, *value, *uri;
    char *line = strdup(cline); /* need to keep this ptr for free */
    value = line;

    if (req->method == NULL) {
        /* If status line not initialized, assume this is it */
        req->method = strdup(strsep(&value, " "));
        uri = strdup(strsep(&value, " "));
        rval = url_init(req->url, uri);
        free(uri);
        req->http_version = strdup(strsep(&value, " "));
        if (rval || strsep(&value, " ") != NULL)
            rval = -1;
    } else {
        /* Otherwise, parse key: value pairs */
        key = strsep(&value, " ");
        if (!strcasecmp(key, "connection:"))
            req->connection = strdup(value);
        else if (!strcasecmp(key, "content-length:"))
            req->content_length = strdup(value);
    }

    free(line);
    return rval;
}


int request_lookup_host(request_t *req)
{
    char *ip, *msg;
    struct hostent *hostinfo;
    struct in_addr ip_addr;

    if (inet_aton(req->url->host, &ip_addr) == 1) {
        req->url->ip = strdup(req->url->host);
        return 1;               /* host is already an ip address */
    }

    if (hashmap_get(&hostname_cache, req->url->host, &ip) != -1) {
        /* Cache hit */
        printl(LOG_DEBUG "Host %s -> %s - cache hit\n", req->url->host, ip);
        req->url->ip = strdup(ip);
        return 1;
    }

    if ((hostinfo = gethostbyname(req->url->host)) == NULL) {
        msg = LOG_DEBUG "Couldn't resolve %s - %s\n";
        printl(msg, req->url->host, hstrerror(h_errno));
        return -1;
    }

    ip = inet_ntoa(*(struct in_addr *) hostinfo->h_addr);
    printl(LOG_DEBUG "Host %s -> %s - cache miss\n", req->url->host, ip);
    hashmap_add(&hostname_cache, req->url->host, ip);
    req->url->ip = strdup(ip);

    return 0;
}


void request_init(request_t *req, int fd, const struct sockaddr_in *addr)
{
    char *ip =  malloc(INET_ADDRSTRLEN);
    inet_ntop(AF_INET, &(addr->sin_addr), ip, INET_ADDRSTRLEN);
    req->client_fd = fd;
    memcpy(req->ip, ip, INET_ADDRSTRLEN);
    req->complete = 0;
    req->method = NULL;
    req->url = malloc(sizeof(url_t));
    req->http_version = NULL;
    req->connection = NULL;
    req->content_length = NULL;
}


void request_destroy(request_t *req)
{
    if (req->method)
        free(req->method);
    if (req->http_version)
        free(req->http_version);
    if (req->connection)
        free(req->connection);
    if (req->content_length)
        free(req->content_length);
    if (req->url) {
        url_destroy(req->url);
        free(req->url);
    }
}
