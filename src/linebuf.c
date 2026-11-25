#include "linebuf.h"

#include <stdlib.h>
#include <string.h>

#define INITIAL_CAP 128

void linebuf_init(LineBuf *lb)
{
    lb->buf = NULL;
    lb->cap = 0;
}

static int grow(LineBuf *lb)
{
    size_t new_cap = lb->cap ? lb->cap * 2 : INITIAL_CAP;
    char *nb = realloc(lb->buf, new_cap);
    if (!nb)
        return -1;
    lb->buf = nb;
    lb->cap = new_cap;
    return 0;
}

long linebuf_read(LineBuf *lb, FILE *fp)
{
    if (!lb->buf && grow(lb) != 0)
        return LINEBUF_NOMEM;

    size_t len = 0;
    lb->buf[0] = '\0';

    while (fgets(lb->buf + len, (int)(lb->cap - len), fp)) {
        len += strlen(lb->buf + len);

        if (lb->buf[len - 1] == '\n')      /* got a whole line */
            return (long)len;
        if (len < lb->cap - 1)             /* last line, no newline at EOF */
            return (long)len;

        if (grow(lb) != 0)                 /* buffer was full: double it */
            return LINEBUF_NOMEM;
    }
    return len > 0 ? (long)len : LINEBUF_EOF;
}

void linebuf_free(LineBuf *lb)
{
    free(lb->buf);
    lb->buf = NULL;
    lb->cap = 0;
}
