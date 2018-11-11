#include <stdarg.h>             /* va_end, va_list, va_start */
#include <stdio.h>              /* fprintf, stdout, stderr, vfprintf */
#include <stdlib.h>             /* atoi */
#include <string.h>             /* strnlen */

#include "printl.h"


#define BOLD_RED "\033[1;31m"    /* FATAL, ERROR */
#define BOLD_YELLOW "\033[1;33m" /* WARN */
#define BOLD_BLUE "\033[1;34m"   /* INFO */
#define BOLD_WHITE "\033[1;37m"  /* DEBUG */
#define NORMAL_WHITE "\033[37m"  /* TRACE */
#define RESET "\033[0m"
#define MAX_PREFIX 30            /* Max characters to prepend to fmt string */


printl_level default_printl_level = INFO;
printl_level current_printl_level = INFO;
int use_color = 1;


/* Return the log level specified in the string `s', or 0 if none found. */
static inline printl_level printl_get_level(const char *s)
{
    if (s[0] == LOG_SOH_ASCII && s[1]) {
        int lvl = atoi(&s[1]);
        switch (lvl) {
        case FATAL ... TRACE:
            return lvl;
        }
    }

    return 0;
}


void printl(const char *msg, ...)
{
    va_list args;
    va_start(args, msg);

    printl_level msglvl = printl_get_level(msg);
    const char *fmt = msglvl ? msg + 2 : msg;
    if (!msglvl)
        msglvl = default_printl_level;

    if (msglvl > current_printl_level)
        return;

    char *buf = malloc(strlen(fmt) + MAX_PREFIX);
    char *prefix, *color;
    switch (msglvl) {
    case FATAL:
        prefix = "%s[FATAL]%s: ";
        color = BOLD_RED;
        break;
    case ERR:
        prefix = "%s[ERROR]%s: ";
        color = BOLD_RED;
        break;
    case WARN:
        prefix = "%s[WARNING]%s: ";
        color = BOLD_YELLOW;
        break;
    case INFO:
        prefix = "%s[INFO]%s: ";
        color = BOLD_BLUE;
        break;
    case DEBUG:
        prefix = "%s[DEBUG]%s: ";
        color = BOLD_WHITE;
        break;
    case TRACE:
        prefix = "%s[TRACE]%s: ";
        color = NORMAL_WHITE;
        break;
    }

    char formatted_prefix[50];
    if (use_color)
        sprintf(formatted_prefix, prefix, color, RESET);
    else
        sprintf(formatted_prefix, prefix, "", "");

    strcpy(buf, formatted_prefix);
    strcat(buf, fmt);

    if (msglvl <= WARN)
        vfprintf(stderr, buf, args);
    else
        vfprintf(stdout, buf, args);

    free(buf);
    va_end(args);
}


void printl_setlevel(printl_level lvl)
{
    current_printl_level = lvl;
}


printl_level printl_getlevel()
{
    return current_printl_level;
}


void printl_enable_color()
{
    use_color = 1;
}


void printl_disable_color()
{
    use_color = 0;
}


int printl_color_enabled()
{
    return use_color;
}
