#ifndef RESPONSE_H
#define RESPONSE_H

#include <stdlib.h>             /* size_t */

#include "request.h"

#define RES_BUFLEN 8000

typedef struct response {
    char *status_line;
    char *server;
    char *date;
    char *content_type;
    char *content_length;
    char *connection;
} response_t;

void response_init(request_t *req, response_t *res, int status,
                   const char *ctype, size_t clen);
void response_destroy(response_t *res);
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
