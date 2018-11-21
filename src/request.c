#include <arpa/inet.h>          /* inet_addr */
#include <assert.h>             /* assert */
#include <errno.h>              /* errno */
#include <netdb.h>              /* gethostbyname */
#include <string.h>             /* str* */
#include <sys/socket.h>         /* struct sockaddr */
#include <unistd.h>             /* read */

#include "printl.h"
#include "request.h"


int request_read(request_t *req)
{
    int nrecv, nrecvd, nunparsed;
    char reqbuf[REQ_BUFLEN] = "";
    int id = req->thread_id;

    nunparsed = 0;
    nrecv = REQ_BUFLEN;
    while ((nrecvd = read(req->client_fd, reqbuf + nunparsed, nrecv)) > 0) {
        reqbuf[nunparsed + nrecvd] = '\0';
        nunparsed = request_deserialize(req, reqbuf, nunparsed + nrecvd);
        if (nunparsed < 0) {
            return 400;         /* Bad Request Error */
        }
        nrecv = REQ_BUFLEN - nunparsed;
        if (req->complete)
            break;
    }

    if (nrecvd <= 0) {
        printl(LOG_DEBUG "[%d] Connection closed while reading request\n", id);
        if (nrecvd == 0 && nunparsed == REQ_BUFLEN) {
            return 431;         /* Request Header Fields Too Large Error */
        } else if (nrecvd == -1) {
            printl(LOG_WARN "[%d] request read - %s\n", id, strerror(errno));
            return 500;         /* Internal Server Error */
        }

        return 1;               /* just signal connection closed */
    }

    assert(req->complete);

    return 0;
}


int request_deserialize(request_t *req, char *buf, size_t buflen)
{
    bool last_line, last_line_partial;
    int nunparsed;
    char *line, *previous_line;
    char *orig_buf = buf;
    char *saveptr;
    const char delim[] = "\r\n";
    int id = req->thread_id;

    /* Copy request into raw buffer */
    req->raw = realloc(req->raw, req->raw_len + buflen + 1);
    memcpy(req->raw + req->raw_len, buf, buflen);
    req->raw[req->raw_len + buflen] = '\0';
    req->raw_buffer_sz = req->raw_len + buflen + 1;
    req->raw_len += buflen;

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
        req->raw_len -= nunparsed;
        strncpy(buf, previous_line, buflen);
        return nunparsed;
    } else {
        memset(buf, 0, buflen);
    }

    return 0;
}


int request_deserialize_line(request_t *req, const char *cline)
{
    int rval = 0;
    char *key, *value, *uri;
    char *line = strdup(cline);
    int id = req->thread_id;

    value = line;

    if (req->method == NULL) {
        /* If status line not initialized, assume this is it */
        printl(LOG_DEBUG "[%d] Got request: %s\n", id, cline);
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
    int id = req->thread_id;

    if (inet_aton(req->url->host, &ip_addr) == 1) {
        /* Host is already an ip address */
        req->url->ip = strdup(req->url->host);
        return 1;
    }

    if (hashmap_get(&hostname_cache, req->url->host, &ip) != -1) {
        /* Cache hit */
        msg = LOG_DEBUG "[%d] Host %s -> %s - cache hit\n";
        printl(msg, id, req->url->host, ip);
        req->url->ip = strdup(ip);
        return 1;
    }

    if ((hostinfo = gethostbyname(req->url->host)) == NULL) {
        msg = LOG_DEBUG "[%d] Couldn't resolve %s - %s\n";
        printl(msg, id, req->url->host, hstrerror(h_errno));
        return -1;
    }

    ip = inet_ntoa(*(struct in_addr *) hostinfo->h_addr);
    msg = LOG_DEBUG "[%d] Host lookup %s -> %s - cache miss\n";
    printl(msg, id, req->url->host, ip);
    hashmap_add(&hostname_cache, req->url->host, ip);
    req->url->ip = strdup(ip);

    return 0;
}


void request_init(request_t *req, int fd, const struct sockaddr_in *addr)
{
    char *ip =  malloc(INET_ADDRSTRLEN);

    memset(req, 0, sizeof(request_t));
    inet_ntop(AF_INET, &(addr->sin_addr), ip, INET_ADDRSTRLEN);
    req->client_fd = fd;
    memcpy(req->ip, ip, INET_ADDRSTRLEN);
    req->url = malloc(sizeof(url_t));
    memset(req->url, 0, sizeof(url_t));
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
        if (req->url->full)     /* verify url initialized */
            url_destroy(req->url);
        free(req->url);
    }
}
