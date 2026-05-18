#include "unp.h"
#include <stdarg.h>

static void err_doit(int, int, const char *, va_list);

/* Fatal error related to a system call (e.g., socket, bind) */
void err_sys(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    err_doit(1, errno, fmt, ap);
    va_end(ap);
    exit(1);
}

/* Fatal error unrelated to a system call */
void err_quit(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    err_doit(0, 0, fmt, ap);
    va_end(ap);
    exit(1);
}

/* Core error printing function */
static void err_doit(int errnoflag, int error, const char *fmt, va_list ap)
{
    char buf[MAXLINE + 1];
    vsnprintf(buf, MAXLINE, fmt, ap);
    if (errnoflag) {
        snprintf(buf + strlen(buf), MAXLINE - strlen(buf), ": %s", strerror(error));
    }
    strcat(buf, "\n");
    fflush(stdout);
    fputs(buf, stderr);
    fflush(stderr);
}
