#include <stdio.h>              /* *printf */
#include <stdlib.h>             /* malloc, atoi */
#include <string.h>             /* str* */

#include "url.h"


const char err_invalid_scheme[] = "Invalid scheme `%s' - use http";
const char err_invalid_port[] = "Invalid port `%s'";
const char err_invalid_path[] = "Invalid path includes `/../'";


int url_init(url_t *url, char *url_str)
{
    size_t strsize;
    unsigned short portno = 80;
    char *c, *scheme, *host, *port, *path;

    memset(url, 0, sizeof(url_t));
    url->full = strdup(url_str);

    /* Parse scheme */
    if ((c = strstr(url_str, "://"))) {
        scheme = url_str;
        *c = '\0';
        url_str = c + 3;
        url->scheme = strdup(scheme);
    } else {
        url->scheme = strdup("http");
    }

    if (strcmp(url->scheme, "http") != 0) {
        strsize = strlen(err_invalid_scheme) + strlen(url->scheme) + 1;
        url->error = malloc(strsize);
        sprintf(url->error, err_invalid_scheme, url->scheme);
        return -1;
    }

    /* Parse host and port */
    host = strsep(&url_str, ":");
    if (url_str == NULL) {
        /* No port found */
        url_str = host;               /* reset url_str */
        host = strsep(&url_str, "/"); /* split host/path */
        url->host = strdup(host);
    } else {
        /* Port found */
        url->host = strdup(host);
        port = strsep(&url_str, "/"); /* split port/path */
        portno = atoi(port);
        if (!portno) {
            strsize = strlen(err_invalid_port) + strlen(port) + 1;
            url->error = malloc(strsize);
            sprintf(url->error, err_invalid_port, port);
            return -1;
        }
    }

    url->port = portno;

    /* Parse path segement */
    if (url_str && url_str[0]) {
        path = malloc(strlen(url_str) + 2);
        sprintf(path, "/%s", url_str);
        url->path = path;
    } else {
        url->path = strdup("/");
    }

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
