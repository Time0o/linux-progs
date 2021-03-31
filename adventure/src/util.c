#define _GNU_SOURCE

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "util.h"


/* program name */
extern char *progname;


/* print formatted error message to stderr */
int errprintf(char const *msg, ...) {
    va_list va;
    char *buf;

    va_start(va, msg);

    if (vasprintf(&buf, msg, va) == -1) {
        va_end(va);
        return -1;
    }

    fprintf(stderr, "%s: error: %s\n", progname, buf);
    free(buf);

    va_end(va);
    return 0;
}
