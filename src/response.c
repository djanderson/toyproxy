#include <assert.h>             /* assert */
#include <errno.h>              /* errno */
#include <stdio.h>              /* sprintf */
#include <stdlib.h>             /* size_t */
#include <string.h>             /* memset, str* */
#include <time.h>               /* gmtime */

#include "printl.h"
#include "request.h"
#include "response.h"


const char response_date_fmt[] = "%a, %d %b %Y %H:%M:%S %Z";
const char response_server[] = "webproxy";
const char response_version_1_0[] = "HTTP/1.0";
const char response_version_1_1[] = "HTTP/1.1";
const char response_success_200[] = "200 Success";
const char response_client_error_400[] = "400 Bad Request";
const char response_client_error_404[] = "404 Not Found";
const char response_client_error_405[] = "405 Method Not Allowed";
const char response_client_error_431[] = "431 Request Header Fields Too Large";
const char response_server_error_500[] = "500 Internal Server Error";
const char error_500[] = "HTTP/1.1 500 Internal Server Error\r\n\r\n";


int response_serialize(response_t *res, char **buf, size_t *buflen)
{
    int rval;
    char *status, *server, *date, *connection, *content_type, *content_length;
    int clen = 0;
    size_t nbytes = 0;

    status = res->header.status_line;
    hashmap_get(&res->header.fields, "Date", &date);
    hashmap_get(&res->header.fields, "Server", &server);
    hashmap_get(&res->header.fields, "Connection", &connection);
    hashmap_get(&res->header.fields, "Content-Type", &content_type);
    hashmap_get(&res->header.fields, "Content-Length", &content_length);

    /* Precalculate size of response header (line + 2 for \r\n) */
    nbytes += strlen(status) + 2;
    nbytes += strlen(server) + 2;
    nbytes += strlen(date) + 2;
    nbytes += strlen(connection) + 2;
    if (content_type)
        nbytes += strlen(content_type) + 2;
    if (content_length) {
        nbytes += strlen(content_length) + 2;
        clen = atoi(content_length);
    }
    nbytes += 2;                /* end of header \r\n */
    nbytes += clen;             /* response body */

    /* Try to allocate enough memory to serialize this response */
    *buf = malloc(nbytes);
    if (*buf) {
        /* Build response buffer */
        sprintf("%s\r\n%s\r\n%s\r\n%s\r\n", status, server, date, connection);
        if (content_type) {
            strcat(*buf, content_type);
            strcat(*buf, "\r\n");
        }
        if (content_length) {
            strcat(*buf, content_length);
            strcat(*buf, "\r\n");
        }
        strcat(*buf, "\r\n");       /* end of header */
        assert(strlen(*buf) == nbytes - clen);
        memcpy(*buf + (nbytes - clen), res->content, clen);
        rval = nbytes;
    } else {
        /* Out of memory */
        printl(LOG_ERR "Error serializing response - %s\n", strerror(errno));
        *buf = (char *) error_500;
        *buflen = strlen(error_500);
        rval = -1;
    }

    /* Clean up */
    free(date);
    free(server);
    free(connection);
    free(content_type);
    free(content_length);

    return nbytes;
}


int response_deserialize(response_t* res, char* buf, size_t buflen)
{
    int nremaining = 0, content_length;
    bool header_complete = false;
    char *line, *key, *value;
    char *tmpbuf = malloc(buflen);
    memcpy(tmpbuf, buf, buflen);
    char * const saveptr = tmpbuf;
    const char delim[] = "\n";

    return 0;
}


void response_init_from_request(request_t *req, response_t *res, int status,
                                const char *ctype, size_t clen)
{
    const int field_len = 100;
    char field[field_len];

    status_string(status, field, field_len);
    res->header.status_line = malloc(strlen(field) + 10);
    sprintf(res->header.status_line, "%s %s", req->http_version, field);

    hashmap_add(&res->header.fields, "Server", response_server);

    time_t now = time(0);
    struct tm gmt = *gmtime(&now);
    strftime(field, field_len, response_date_fmt, &gmt);
    hashmap_add(&res->header.fields, "Date", field);

    if (ctype)
        hashmap_add(&res->header.fields, "Content-Type", ctype);

    if (clen) {
        sprintf(field, "%lu", clen);
        hashmap_add(&res->header.fields, "Content-Length", field);
    }

    if (req->connection)
        strcpy(field, req->connection);
    else
        strcpy(field, request_version_is_1_1(req) ? "keep-alive" : "close");

    hashmap_add(&res->header.fields, "Connection", field);
}


void response_destroy(response_t *res)
{
    if (res->header.status_line)
        free(res->header.status_line);
    if (res->header.raw)
        free(res->header.raw);

    hashmap_destroy(&res->header.fields);
}


char *status_string(int status, char *buf, size_t buflen)
{
    const char *status_str;

    switch (status) {
    case 200:
        status_str = response_success_200;
        break;
    case 400:
        status_str = response_client_error_400;
        break;
    case 404:
        status_str = response_client_error_404;
        break;
    case 405:
        status_str = response_client_error_405;
        break;
    case 431:
        status_str = response_client_error_431;
        break;
    default:
        status_str = response_server_error_500;
    }

    strncpy(buf, status_str, buflen);
    buf[buflen] = '\0';

    return buf;
}
