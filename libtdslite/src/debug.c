#include "tdslite/debug.h"

#include <errno.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdarg.h>

static
unsigned char toprint(unsigned char c)
{
    if (c < ' ' || c >= 0x7f)
        return '.';

    return c;
}

static
void dump_mem(const void *ptr, int size)
{
    const unsigned char *p = (const unsigned char *)ptr;
    int i;

    for (i = 0; i < size; )
    {
        char buf[0x81];
        char *writep = buf;

        int cell = 0;
        writep += sprintf(writep, "\t%04x: ", i);
        for (cell = 0; cell < 16; cell++)
        {
            if (i + cell < size)
                writep += sprintf(writep, "%02x", p[cell]);
            else
                writep += sprintf(writep, "  ");

            if ((cell % 4)==3)
            {
                *writep++ = ' '; *writep = 0;
            }
        }

        *writep++ = ' '; *writep = 0;

        for (cell = 0; cell < 16; cell++)
        {
            if (i + cell < size)
                writep += sprintf(writep, "%c", toprint(p[cell]));
            else
                writep += sprintf(writep, " ");
        }
        fprintf(stderr, "%s\n", buf);
        i+= 16;
        p+= 16;
    }
}

void tp_debug_f(const char *file, const int line, const char *func, const char *fmt,...)
{
    int saved_errno = errno;

    va_list ap;
    va_start(ap, fmt);

    struct timeval tv;
    gettimeofday(&tv, 0);
    fprintf(stderr, "[%d.%06d] %s:%d (%s) ", (int)tv.tv_sec, (int)tv.tv_usec,
            file, line, func);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    fflush(stderr);

    errno = saved_errno;
}

void tp_debug_dump_f(const char *file, const int line, const char *func, const void *ptr, int size, const char *fmt,...)
{
    int saved_errno = errno;

    va_list ap;
    va_start(ap, fmt);

    struct timeval tv;
    gettimeofday(&tv, 0);
    fprintf(stderr, "[%d.%06d] %s:%d (%s) ", (int)tv.tv_sec, (int)tv.tv_usec,
            file, line, func);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    fflush(stderr);
    dump_mem(ptr,size);
    fflush(stderr);

    errno = saved_errno;
}
