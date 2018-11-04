#include <assert.h>             /* assert */
#include <stdio.h>              /* sprintf */
#include <stdlib.h>             /* size_t */
#include <string.h>             /* memset, strlen */
#include <time.h>               /* gmtime */

#include "printl.h"
#include "request.h"
#include "response.h"


const char response_date_fmt[] = "%a, %d %b %Y %H:%M:%S %Z";
const char response_server[] = "shws";
const char response_version_1_0[] = "HTTP/1.0";
const char response_version_1_1[] = "HTTP/1.1";
const char response_success_200[] = "200 Success";
const char response_client_error_400[] = "400 Bad Request";
const char response_client_error_404[] = "404 Not Found";
const char response_client_error_405[] = "405 Method Not Allowed";
const char response_client_error_431[] = "431 Request Header Fields Too Large";
const char response_server_error_500[] = "500 Internal Server Error";
const char error_500[] = "HTTP/1.1 500 Internal Server Error\r\n\r\n";


size_t response_serialize(response_t *res, char *buf, size_t buflen)
{
    size_t nbytes = 0;

    memset(buf, 0, buflen);

    /* Precalculate size of response header (line + 2 for \r\n) */
    nbytes += strlen(res->status_line) + 2;
    nbytes += strlen(res->server) + 2;
    nbytes += strlen(res->date) + 2;
    if (res->content_type)
        nbytes += strlen(res->content_type) + 2;
    if (res->content_length)
        nbytes += strlen(res->content_length) + 2;
    nbytes += strlen(res->connection) + 2;
    nbytes += 2;                /* for end of buffer \r\n */

    /* If response won't fit in buf, return 500 */
    if (nbytes > buflen) {
        printl(LOG_ERR "Error serializing response header, need %d bytes, but "
               "given %d\n", nbytes, buflen);
        nbytes = strlen(error_500);
        assert(nbytes <= buflen);
        strcpy(buf, error_500);
        return nbytes;
    }

    /* Build response buffer */
    strcat(buf, res->status_line);
    strcat(buf, "\r\n");
    strcat(buf, res->server);
    strcat(buf, "\r\n");
    strcat(buf, res->date);
    strcat(buf, "\r\n");
    if (res->content_type) {
        strcat(buf, res->content_type);
        strcat(buf, "\r\n");
    }
    if (res->content_length) {
        strcat(buf, res->content_length);
        strcat(buf, "\r\n");
    }
    strcat(buf, res->connection);
    strcat(buf, "\r\n");
    strcat(buf, "\r\n");
    assert(strlen(buf) == nbytes);

    return nbytes;
}


void response_init(request_t *req, response_t *res, int status,
                   const char *ctype, size_t clen)
{
    const int field_len = 100;

    char status_str[field_len];
    status_string(status, status_str, field_len);
    res->status_line = malloc(field_len);
    sprintf(res->status_line, "%s %s", req->version, status_str);

    res->server = malloc(field_len);
    sprintf(res->server, "Server: %s", response_server);

    res->date = malloc(field_len);
    time_t now = time(0);
    struct tm gmt = *gmtime(&now);
    char date[field_len];
    strftime(date, field_len, response_date_fmt, &gmt);
    sprintf(res->date, "Date: %s", date);

    if (ctype) {
        res->content_type = malloc(field_len);
        sprintf(res->content_type, "Content-Type: %s", ctype);
    } else {
        res->content_type = NULL;
    }

    if (clen) {
        res->content_length = malloc(field_len);
        sprintf(res->content_length, "Content-Length: %lu", clen);
    } else {
        res->content_length = NULL;
    }

    res->connection = malloc(field_len);
    if (req->connection) {
        sprintf(res->connection, "Connection: %s", req->connection);
    } else {
        char *conn = request_version_is_1_1(req) ? "keep-alive" : "close";
        sprintf(res->connection, "Connection: %s", conn);
    }
}


void response_destroy(response_t *res)
{
    if (res->status_line)
        free(res->status_line);
    if (res->server)
        free(res->server);
    if (res->date)
       free(res->date);
    if (res->content_type)
        free(res->content_type);
    if (res->content_length)
        free(res->content_length);
    if (res->connection)
        free(res->connection);
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
