#ifndef LINEBUF_H
#define LINEBUF_H

#include <stdio.h>

/* A reusable, growable line buffer. One allocation is reused for every
 * line in the file and only doubles when a line doesn't fit, so memory
 * stays flat no matter how large the input file is. */
typedef struct {
    char  *buf;
    size_t cap;
} LineBuf;

#define LINEBUF_EOF    (-1L)
#define LINEBUF_NOMEM  (-2L)

void linebuf_init(LineBuf *lb);

/* Reads one full line (any length). Returns its length,
 * LINEBUF_EOF at end of file, or LINEBUF_NOMEM if allocation fails. */
long linebuf_read(LineBuf *lb, FILE *fp);

void linebuf_free(LineBuf *lb);

#endif
