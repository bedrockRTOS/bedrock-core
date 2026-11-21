/*
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>

#include "bedrock/bedrock.h"
#include "br_console.h"

typedef struct {
    bool    uart;
    char   *buf;
    size_t  size;
    size_t  len;
} out_t;

static void out_char(out_t *out, char c)
{
    if (out->uart) {
        br_uart_putc(c);
    } else if (out->buf != NULL && out->len + 1 < out->size) {
        out->buf[out->len] = c;
    }
    out->len++;
}

static void out_pad(out_t *out, char c, int count)
{
    while (count-- > 0) {
        out_char(out, c);
    }
}

static size_t str_length(const char *s)
{
    size_t n = 0;
    while (s[n] != '\0') {
        n++;
    }
    return n;
}

static void out_field(out_t *out, const char *sign, size_t sign_len,
                      const char *body, size_t body_len,
                      int width, bool left, bool zero)
{
    int pad = width - (int)(sign_len + body_len);

    if (!left && !zero) {
        out_pad(out, ' ', pad);
    }
    for (size_t i = 0; i < sign_len; i++) {
        out_char(out, sign[i]);
    }
    if (!left && zero) {
        out_pad(out, '0', pad);
    }
    for (size_t i = 0; i < body_len; i++) {
        out_char(out, body[i]);
    }
    if (left) {
        out_pad(out, ' ', pad);
    }
}

static size_t format_number(char *tmp, uint64_t value, unsigned base, bool upper)
{
    const char *digits = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    size_t n = 0;

    do {
        tmp[n++] = digits[value % base];
        value /= base;
    } while (value != 0);

    for (size_t i = 0; i < n / 2; i++) {
        char t = tmp[i];
        tmp[i] = tmp[n - 1 - i];
        tmp[n - 1 - i] = t;
    }
    return n;
}

static void vformat(out_t *out, const char *fmt, va_list ap)
{
    while (*fmt != '\0') {
        if (*fmt != '%') {
            out_char(out, *fmt++);
            continue;
        }
        fmt++;

        bool left = false;
        bool zero = false;
        for (;; fmt++) {
            if (*fmt == '-') {
                left = true;
            } else if (*fmt == '0') {
                zero = true;
            } else {
                break;
            }
        }

        int width = 0;
        while (*fmt >= '0' && *fmt <= '9') {
            width = width * 10 + (*fmt++ - '0');
        }

        int longs = 0;
        bool is_size = false;
        while (*fmt == 'l') {
            longs++;
            fmt++;
        }
        if (*fmt == 'z') {
            is_size = true;
            fmt++;
        }

        char spec = *fmt;
        if (spec == '\0') {
            break;
        }
        fmt++;

        char tmp[24];

        switch (spec) {
        case '%':
            out_char(out, '%');
            break;

        case 'c':
            tmp[0] = (char)va_arg(ap, int);
            out_field(out, "", 0, tmp, 1, width, left, false);
            break;

        case 's': {
            const char *s = va_arg(ap, const char *);
            if (s == NULL) {
                s = "(null)";
            }
            out_field(out, "", 0, s, str_length(s), width, left, false);
            break;
        }

        case 'd':
        case 'i': {
            int64_t v;
            if (longs >= 2) {
                v = va_arg(ap, long long);
            } else if (longs == 1) {
                v = va_arg(ap, long);
            } else if (is_size) {
                v = (int64_t)va_arg(ap, size_t);
            } else {
                v = va_arg(ap, int);
            }
            uint64_t mag = (v < 0) ? (uint64_t)0 - (uint64_t)v : (uint64_t)v;
            size_t n = format_number(tmp, mag, 10, false);
            out_field(out, "-", (v < 0) ? 1 : 0, tmp, n, width, left, zero && !left);
            break;
        }

        case 'u':
        case 'x':
        case 'X':
        case 'p': {
            uint64_t v;
            if (spec == 'p') {
                v = (uint64_t)(uintptr_t)va_arg(ap, void *);
            } else if (longs >= 2) {
                v = va_arg(ap, unsigned long long);
            } else if (longs == 1) {
                v = va_arg(ap, unsigned long);
            } else if (is_size) {
                v = va_arg(ap, size_t);
            } else {
                v = va_arg(ap, unsigned int);
            }
            unsigned base = (spec == 'u') ? 10 : 16;
            size_t n = format_number(tmp, v, base, spec == 'X');
            if (spec == 'p') {
                out_field(out, "0x", 2, tmp, n, width, left, zero && !left);
            } else {
                out_field(out, "", 0, tmp, n, width, left, zero && !left);
            }
            break;
        }

        default:
            out_char(out, '%');
            out_char(out, spec);
            break;
        }
    }
}

int br_printf(const char *fmt, ...)
{
    out_t out = { .uart = true, .buf = NULL, .size = 0, .len = 0 };
    va_list ap;

    va_start(ap, fmt);
    vformat(&out, fmt, ap);
    va_end(ap);

    return (int)out.len;
}

int br_snprintf(char *buf, size_t size, const char *fmt, ...)
{
    out_t out = { .uart = false, .buf = buf, .size = size, .len = 0 };
    va_list ap;

    va_start(ap, fmt);
    vformat(&out, fmt, ap);
    va_end(ap);

    if (size > 0) {
        buf[(out.len < size) ? out.len : size - 1] = '\0';
    }
    return (int)out.len;
}
