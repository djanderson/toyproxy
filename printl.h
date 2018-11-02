#ifndef PRINTL_H
#define PRINTL_H

#define LOG_SOH "\001"         /* Start of lvl header */
#define LOG_SOH_ASCII '\001'   /* Start of lvl header character */
#define LOG_FATAL LOG_SOH "1"  /* The program is crashing */
#define LOG_ERR   LOG_SOH "2"  /* An error has occurred */
#define LOG_WARN  LOG_SOH "3"  /* An event requiring attention occurred */
#define LOG_INFO  LOG_SOH "4"  /* A notable event occurred */
#define LOG_DEBUG LOG_SOH "5"  /* An important internal event occurred  */
#define LOG_TRACE LOG_SOH "6"  /* TMI */


typedef enum { FATAL = 1, ERR, WARN, INFO, DEBUG, TRACE } printl_level;


/*
 * Logger with printk-like log level prefix, fmt string, and args.
 *
 * Log level defaults to INFO. If log level is provided and is FATAL, ERR, or
 * WARN, output is to stderr. Otherwise, output is to stdout.
 */
void printl(const char *msg, ...);


/* Set the log level. */
void printl_setlevel(printl_level lvl);


/* Get the log level */
printl_level printl_getlevel(void);


/* Enable color output. */
void printl_enable_color(void);


/* Disable color output. */
void printl_disable_color(void);


/* Return 1 if color output is enabled, else 0. */
int printl_color_enabled(void);


#endif  /* PRINTL_H */
