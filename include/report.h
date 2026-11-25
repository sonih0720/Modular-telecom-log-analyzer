#ifndef REPORT_H
#define REPORT_H

#include <stdio.h>

#include "stats.h"

typedef struct {
    const char *source;          /* file name shown in the header           */
    double      fail_threshold;  /* % of DROPPED+FAILED that flags a cell   */
    size_t      top_n;           /* how many top callers to list            */
    size_t      peak_line_buf;   /* bytes, for the memory summary           */
} ReportOptions;

/* Writes the full summary report. May reorder s->cells (sorts by id). */
void write_report(FILE *out, Stats *s, const ReportOptions *opt);

#endif
