#include <arpa/inet.h>          /* inet_addr */
#include <string.h>             /* str* */
#include <sys/socket.h>         /* struct sockaddr */

#include "printl.h"
#include "request.h"


size_t request_deserialize(struct request *req, char *buf, size_t buflen)
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


int request_deserialize_line(struct request *req, const char *cline)
{
    printl(LOG_DEBUG "Got request line: %s\n", cline);

    int rval = 0;
    char *key, *value;
    char *line = strdup(cline); /* need to keep this ptr for free */
    value = line;

    if (req->method == NULL) {
        /* If status line not initialized, assume this is it */
        req->method = strdup(strsep(&value, " "));
        req->uri = strdup(strsep(&value, " "));
        req->version = strdup(strsep(&value, " "));
        if (strsep(&value, " ") != NULL)
            rval = 1;
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


void request_init(struct request *req, int fd, const struct sockaddr_in *addr)
{
    char *ip =  malloc(INET_ADDRSTRLEN);
    inet_ntop(AF_INET, &(addr->sin_addr), ip, INET_ADDRSTRLEN);
    req->fd = fd;
    req->ip = ip;
    req->complete = 0;
    req->method = NULL;
    req->uri = NULL;
    req->version = NULL;
    req->connection = NULL;
    req->content_length = NULL;
}


void request_destroy(struct request *req)
{
    if (req->ip)
        free(req->ip);
    if (req->method)
        free(req->method);
    if (req->uri)
        free(req->uri);
    if (req->version)
        free(req->version);
    if (req->connection)
        free(req->connection);
    if (req->content_length)
        free(req->content_length);
}
