#ifndef RESPONSE_H
#define RESPONSE_H

#include <stdlib.h>             /* size_t */

#include "hashmap.h"
#include "request.h"

#define RES_BUFLEN 8000

/* XXX: consider - consolidate into http.h and call http_header_t */
typedef struct response_header {
    bool complete;              /* parser read to end-of-header empty line */
    char *raw;                  /* raw response header buffer or NULL */
    size_t raw_buffer_sz;       /* size of the raw buffer */
    size_t raw_sz;              /* number of bytes of content in raw buffer */
    char *status_line;          /* value of Status-Line */
    hashmap_t fields;           /* a map of header key: value pairs */
} response_header_t;

typedef struct response {
    bool complete;              /* indicates response completely received */
    response_header_t header;   /* header struct */
    char *content;              /* content or NULL */
} response_t;

void response_init_from_request(request_t *req, response_t *res, int status,
                                const char *ctype, size_t clen);
void response_destroy(response_t *res);
/* Return number of bytes not consumed if successful or -1 for error. */
int response_deserialize(response_t *res, char *buf, size_t buflen);
/* Return -1 for parse error or 0 for success. */
int response_deserialize_line(response_t *res, const char *line);
/*
 * Serialize a response into a character buffer.
 *
 * If successful, return value is 0 and `buf' points toward a heap-allocated,
 * null-terminated buffer of length `buflen' that the user is responsible for
 * freeing.
 *
 * If memory allocation fails, return value is -1, and `buf' points towards a
 * stack-allocated null-terminated buffer holding a valid 500 error response of
 * length `buflen` that does not need to be freed.
 */
int response_serialize(response_t *res, char **buf, size_t *buflen);
/*
 * Copy at most buflen chars of string describing status into buf.
 *
 * String will always be null terminated.
 *
 * Returns pointer to destination buffer.
 *
 * E.g., 404 -> "404 Not Found"
 */
char *status_string(int status, char *buf, size_t buflen);


#endif  /* RESPONSE_H */
