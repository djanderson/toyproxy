#ifndef RESPONSE_H
#define RESPONSE_H

#include <stdio.h>              /* sscanf */
#include <stdlib.h>             /* size_t */

#include "hashmap.h"
#include "request.h"

#define RES_BUFLEN 8000


typedef struct response_header {
    bool complete;              /* parser read to end-of-header empty line */
    char *status_line;          /* value of Status-Line */
    hashmap_t fields;           /* a map of header key: value pairs */
} response_header_t;

typedef struct response {
    bool complete;              /* indicates response completely received */
    int thread_id;              /* id of thread handling response */
    char *raw;                  /* raw response header buffer or NULL */
    size_t raw_buffer_sz;       /* size of allocated raw buffer */
    size_t raw_len;             /* number of bytes in raw buffer */
    response_header_t header;   /* header struct */
    request_t *request;         /* request this response corresponds to */
    char *content;              /* ptr to start of content in raw or NULL */
    size_t content_offset;      /* used to reset content ptr if raw moved */
} response_t;

/* Basic response initialization. */
void response_init(response_t *res);
/* Initialize a response directly from webproxy to a given request. */
void response_init_from_request(const request_t *req, response_t *res,
                                int status, const char *ctype, size_t clen);
/* Read socket and build response. */
int response_read(response_t *res, int fd);
/* Free response memory. */
void response_destroy(response_t *res);
/* Return number of bytes not consumed if successful or -1 for error. */
int response_deserialize(response_t *res, char *buf, size_t buflen);
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

/* Return true if response code is 200, else false. */
static inline bool response_ok(const response_t *res)
{
    const char expected_code[] = "200";
    char ver[9] = { 0 };
    char actual_code[4] = { 0 };
    char desc[3] = { 0 };

    sscanf(res->header.status_line, "%8s %3s %2s", ver, actual_code, desc);

    return strncmp(expected_code, actual_code, 3) == 0;
}

/* Return value of Content-Length header field or 0. */
static inline size_t response_content_length(response_t *res)
{
    int len = 0;
    char *clen;

    hashmap_get(&res->header.fields, "Content-Length", &clen);

    if (clen)
        len = atoi(clen);

    free(clen);

    return len;
}

/* Return true if response Transfer-Encoding is "chunked", else false. */
static inline bool response_chunked(response_t *res)
{
    bool is_chunked;
    char *tenc;

    if (hashmap_get(&res->header.fields, "Transfer-Encoding", &tenc) == -1)
        return false;

    is_chunked = strcmp(tenc, "chunked") == 0;

    free(tenc);

    return is_chunked;
}

#endif  /* RESPONSE_H */
