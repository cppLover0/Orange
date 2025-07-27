
#include <cstdint>

#include <etc/libc.hpp>

char *strtok(char *str, const char *delim) {
    static char *next = NULL;
    if (str) next = str;
    if (!next) return NULL;

    char *start = next;
    while (*start && strchr(delim, *start)) {
        start++;
    }
    if (!*start) {
        next = NULL;
        return NULL;
    }

    char *end = start;
    while (*end && !strchr(delim, *end)) {
        end++;
    }

    if (*end) {
        *end = '\0';
        next = end + 1;
    } else {
        next = NULL;
    }

    return start;
}
