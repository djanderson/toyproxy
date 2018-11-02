#ifndef RESPONSE_H
#define RESPONSE_H

#include <stdlib.h>             /* size_t */

#include "request.h"

#define RES_BUFLEN 8000

struct response {
    char *status_line;
    char *server;
    char *date;
    char *content_type;
    char *content_length;
    char *connection;
};

void response_init(struct request *req, struct response *res, int status,
                   const char *ctype, size_t clen);
void response_destroy(struct response *res);
/* Serialize the response or 500 response if `buf' too small. */
size_t response_serialize(struct response *res, char *buf, size_t buflen);
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
