#include <assert.h>             /* assert */
#include <errno.h>              /* errno */
#include <stdio.h>              /* sprintf */
#include <stdlib.h>             /* size_t */
#include <string.h>             /* memset, str* */
#include <time.h>               /* gmtime */
#include <unistd.h>             /* read */

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


int response_read(response_t *res, int fd)
{
    int nrecv, nrecvd, nunparsed;
    char resbuf[RES_BUFLEN] = "";

    nunparsed = 0;
    nrecv = RES_BUFLEN;
    while ((nrecvd = read(fd, resbuf + nunparsed, nrecv)) > 0) {
        resbuf[nunparsed + nrecvd] = '\0';
        nunparsed = response_deserialize(res, resbuf, nunparsed + nrecvd);
        if (nunparsed < 0) {
            return 400;         /* Bad Response Error */
        }
        nrecv = RES_BUFLEN - nunparsed;
        if (res->complete) {
            printl(LOG_DEBUG "Response complete\n");
            break;
        }
    }

    if (nrecvd <= 0) {
        printl(LOG_DEBUG "Connection closed\n");
        if (nrecvd == 0 && nunparsed == RES_BUFLEN) {
            return 431;         /* Response Header Fields Too Large Error */
        } else if (nrecvd == -1) {
            printl(LOG_WARN "read - %s\n", strerror(errno));
            return 500;         /* Internal Server Error */
        }

        return 1;               /* just signal connection closed */
    }

    assert(res->complete);

    return 0;
}


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
    const char *bufcur = buf;     /* work on a const str until the end */
    char *line, *key, *value, *clen, *header_end, *line_end;
    size_t nunparsed, line_len, line_buffer_sz;
    size_t header_len, expected_content_len, actual_content_len;

    /* Copy response into raw buffer */
    res->raw = realloc(res->raw, res->raw_len + buflen + 1);
    memcpy(res->raw + res->raw_len, buf, buflen);
    res->raw[res->raw_len + buflen] = '\0';
    res->raw_buffer_sz = res->raw_len + buflen + 1;
    res->raw_len += buflen;

    nunparsed = 0;

    /* Parse header */
    if (!res->header.complete) {
        line_buffer_sz = 100;
        line = malloc(line_buffer_sz);
        header_end = strstr(bufcur, "\r\n\r\n");
        do {
            line_end = strstr(bufcur, "\r\n");
            if (line_end == NULL)
                break;

            line_len = line_end - bufcur;
            if (line_buffer_sz < line_len + 1) {
                line_buffer_sz = line_len + 1;
                line = realloc(line, line_buffer_sz);
            }

            memcpy(line, bufcur, line_len);
            line[line_len] = '\0';

            if (res->header.status_line == NULL) {
                /* header status line */
                res->header.status_line = strdup(line);
            } else {
                /* header field line */
                value = line;
                key = strtok_r(line, ":", &value);
                if (value[0] == ' ')
                    value++;    /* step past the space after `:` */
                hashmap_add(&res->header.fields, key, value);
            }

            bufcur = line_end + 2;
        } while (line_end != header_end);

        free(line);

        nunparsed = buf + buflen - bufcur;

        if (line_end == NULL) {
            /* Remaining buffer is a partial header line */
            nunparsed = buf + buflen - bufcur;
            res->raw_len -= nunparsed;
        } else if (line_end == header_end) {
            /* Header complete - if buffer remaining, it's content */
            res->header.complete = true;
            bufcur += 2;        /* step over final \r\n */
            nunparsed = 0;      /* parse all the content */
            res->content_offset = res->raw_len - (buf + buflen - bufcur);
        } else {
            /* Buffer ended at complete header line but header not complete */
            if (nunparsed)
                return -1;      /* if anything left, response is malformed */
        }
    }

    /* Reset input buffer to hold only unparsed content */
    memcpy(buf, bufcur, nunparsed);
    memset(buf + nunparsed, 0, buflen - nunparsed);

    /* Check if all content received */
    if (res->header.complete) {
        expected_content_len = 0;
        hashmap_get(&res->header.fields, "Content-Length", &clen);
        if (clen != NULL) {
            expected_content_len = atoi(clen);
            free(clen);
        }

        res->content = res->raw + res->content_offset;
        header_len = res->content - res->raw;
        actual_content_len = res->raw_len - header_len;
        if (actual_content_len == expected_content_len)
            res->complete = true;
    }

    return nunparsed;
}


void response_init(response_t *res)
{
    memset(res, 0, sizeof(response_t));
    hashmap_init(&res->header.fields, 10);
}


void response_init_from_request(request_t *req, response_t *res, int status,
                                const char *ctype, size_t clen)
{
    const int field_len = 100;
    char field[field_len];

    response_init(res);
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
    if (res->raw)
        free(res->raw);

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
