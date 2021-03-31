#ifndef UTIL_H
#define UTIL_H

struct ll {
    char *val;
    struct ll *next;
};

int errprintf(char const *msg, ...);

#endif /* UTIL_H */
