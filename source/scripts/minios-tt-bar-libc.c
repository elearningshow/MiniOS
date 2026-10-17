/* Old names for the C library calls the player bar refers to.
   The laptop's library is older than the one on this build machine.
   These functions are the ones the bar already used. */
#include <stdarg.h>
#include <stddef.h>

extern int vsscanf(const char *s, const char *fmt, va_list ap);
extern int vfscanf(void *stream, const char *fmt, va_list ap);
extern long strtol(const char *nptr, char **endptr, int base);
extern unsigned long strtoul(const char *nptr, char **endptr, int base);
extern size_t strlen(const char *s);
extern void *memcpy(void *dst, const void *src, size_t n);

int __isoc23_sscanf(const char *s, const char *fmt, ...)
{
    va_list ap;
    int r;

    va_start(ap, fmt);
    r = vsscanf(s, fmt, ap);
    va_end(ap);
    return r;
}

int __isoc23_fscanf(void *stream, const char *fmt, ...)
{
    va_list ap;
    int r;

    va_start(ap, fmt);
    r = vfscanf(stream, fmt, ap);
    va_end(ap);
    return r;
}

long __isoc23_strtol(const char *nptr, char **endptr, int base)
{
    return strtol(nptr, endptr, base);
}

unsigned long __isoc23_strtoul(const char *nptr, char **endptr, int base)
{
    return strtoul(nptr, endptr, base);
}

size_t strlcpy(char *dst, const char *src, size_t n)
{
    size_t len = strlen(src);
    size_t c;

    if (n > 0) {
        c = len >= n ? n - 1 : len;
        memcpy(dst, src, c);
        dst[c] = 0;
    }
    return len;
}

size_t strlcat(char *dst, const char *src, size_t n)
{
    size_t dlen = strlen(dst);
    size_t slen = strlen(src);
    size_t c;

    if (dlen < n) {
        c = n - dlen - 1;
        if (c > slen)
            c = slen;
        memcpy(dst + dlen, src, c);
        dst[dlen + c] = 0;
    }
    return dlen + slen;
}
