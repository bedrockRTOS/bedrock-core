/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include <stddef.h>

int memcmp(const void *a, const void *b, size_t n)
{
    const unsigned char *p = a;
    const unsigned char *q = b;

    for (; n > 0; n--, p++, q++) {
        if (*p != *q) {
            return *p < *q ? -1 : 1;
        }
    }
    return 0;
}

char *strchr(const char *s, int c)
{
    for (; *s != '\0'; s++) {
        if (*s == (char)c) {
            return (char *)s;
        }
    }
    return (char)c == '\0' ? (char *)s : NULL;
}

char *strcpy(char *dst, const char *src)
{
    char *d = dst;

    while ((*d++ = *src++) != '\0') { }
    return dst;
}

size_t strspn(const char *s, const char *accept)
{
    size_t n = 0;

    for (; s[n] != '\0'; n++) {
        const char *a = accept;
        while (*a != '\0' && *a != s[n]) {
            a++;
        }
        if (*a == '\0') {
            break;
        }
    }
    return n;
}

size_t strcspn(const char *s, const char *reject)
{
    size_t n = 0;

    for (; s[n] != '\0'; n++) {
        for (const char *r = reject; *r != '\0'; r++) {
            if (*r == s[n]) {
                return n;
            }
        }
    }
    return n;
}
