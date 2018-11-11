#ifndef URLPARSE_H
#define URLPARSE_H

typedef struct url {
    char *full;                 /* the original input string */
    char *scheme;               /* e.g., http */
    char *host;                 /* e.g., google.com, 192.168.1.10 */
    char *ip;                   /* used to store DNS query result */
    unsigned short port;        /* e.g., 8000 */
    char *path;                 /* e.g., /images/cute_kitten.jpg */
    char *error;                /* a string for describing parse errors */
} url_t;


/*
 * Parse a url string.
 *
 * Return -1 and set uri.error to reason if parse fails, else 0.
 *
 * You must call `url_destroy` on the url struct even if the parse fails.
 */
int url_init(url_t *url, const char *url_str);
/* Free memory. */
void url_destroy(url_t *url);

#endif  /* URLPARSE_H */
