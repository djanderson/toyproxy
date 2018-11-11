#ifndef RESPONSE_H
#define RESPONSE_H

#include <stdlib.h>             /* size_t */

#include "request.h"

#define RES_BUFLEN 8000

typedef struct response {
    bool complete;              /* indicates response completely received */
    char *raw;                  /* raw response buffer */
    size_t raw_buffer_sz;       /* size of the raw buffer */
    char *body_ptr;             /* a pointer to the body in raw or NULL */
    char *status_line;          /* value of status line */
    char *server;               /* value of Server header or NULL */
    char *date;                 /* value of Date header or NULL */
    char *content_type;         /* content MIME type (e.g., "text/html") */
    char *content_length;       /* length of message body in bytes */
    char *connection;           /* value of Connection header or NULL */
} response_t;

void response_init(request_t *req, response_t *res, int status,
                   const char *ctype, size_t clen);
void response_destroy(response_t *res);
/* Return 0 if successful or -1 for error. */
int response_deserialize(response_t *res, char *buf, size_t buflen);
/* Serialize the response or 500 response if `buf' too small. */
size_t response_serialize(response_t *res, char *buf, size_t buflen);
/*
 * Copy at most buflen chars of string describing status into buf.
 *
 * String will always be null terminated.
 *
 * Returns pointer to destination buffer.
 *
 * E.g., 404 -> "Not Found"
 */
char *status_string(int status, char *buf, size_t buflen);


#endif  /* RESPONSE_H */
