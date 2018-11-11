#include <stdio.h>              /* *printf */
#include <stdlib.h>             /* malloc, atoi */
#include <string.h>             /* str* */

#include "url.h"


const char err_invalid_scheme[] = "Invalid scheme `%s' - use http";
const char err_invalid_port[] = "Invalid port `%s'";
const char err_invalid_path[] = "Invalid path includes `/../'";


int url_init(url_t *url, const char *url_str)
{
    size_t strsize;
    unsigned short portno = 80;
    char *c, *scheme, *host, *port, *path, *url_ptr;
    /* Keep original ptr for free */
    char * const url_cptr = url_ptr = strdup(url_str);

    memset(url, 0, sizeof(url_t));
    url->full = strdup(url_str);


    /* Parse scheme */
    if ((c = strstr(url_ptr, "://"))) {
        scheme = url_ptr;
        *c = '\0';
        url_ptr = c + 3;
        url->scheme = strdup(scheme);
    } else {
        url->scheme = strdup("http");
    }

    if (strcmp(url->scheme, "http") != 0) {
        strsize = strlen(err_invalid_scheme) + strlen(url->scheme) + 1;
        url->error = malloc(strsize);
        sprintf(url->error, err_invalid_scheme, url->scheme);
        free(url_cptr);
        return -1;
    }

    /* Parse host and port */
    host = strsep(&url_ptr, ":");
    if (url_ptr == NULL) {
        /* No port found */
        url_ptr = host;               /* reset url_ptr */
        host = strsep(&url_ptr, "/"); /* split host/path */
        url->host = strdup(host);
    } else {
        /* Port found */
        url->host = strdup(host);
        port = strsep(&url_ptr, "/"); /* split port/path */
        portno = atoi(port);
        if (!portno) {
            strsize = strlen(err_invalid_port) + strlen(port) + 1;
            url->error = malloc(strsize);
            sprintf(url->error, err_invalid_port, port);
            free(url_cptr);
            return -1;
        }
    }

    url->port = portno;

    /* Parse path segement */
    if (url_ptr && url_ptr[0]) {
        path = malloc(strlen(url_ptr) + 2);
        sprintf(path, "/%s", url_ptr);
        url->path = path;
    } else {
        url->path = strdup("/");
    }

    free(url_cptr);

    /* Don't allow client to read above server root */
    if (strstr(url->path, "/../") != NULL) {
        url->error = strdup(err_invalid_path);
        return -1;
    }

    return 0;
}


void url_destroy(url_t *url)
{
    if (url->full)
        free(url->full);
    if (url->scheme)
        free(url->scheme);
    if (url->host)
        free(url->host);
    if (url->ip)
        free(url->ip);
    if (url->path)
        free(url->path);
    if (url->error)
        free(url->error);
}
