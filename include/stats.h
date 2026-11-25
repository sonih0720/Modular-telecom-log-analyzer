#ifndef STATS_H
#define STATS_H

#include <stdbool.h>
#include <stddef.h>

#include "record.h"

#define CALLER_BUCKETS     1024
#define LONG_CALL_SECONDS  (4L * 60 * 60)   /* calls over 4 hours are flagged */

typedef struct {
    char               cell_id[CELL_LEN];
    unsigned long      total;
    unsigned long      by_status[ST_COUNT];
    unsigned long long call_seconds;
} CellStats;

/* Node in a chained hash table that counts events per caller. */
typedef struct CallerNode {
    char               number[PHONE_LEN];
    unsigned long      count;
    struct CallerNode *next;
} CallerNode;

typedef struct {
    unsigned long lines, valid, malformed, blank;
    unsigned long by_event[EV_COUNT];
    unsigned long by_status[ST_COUNT];

    CellStats *cells;          /* dynamic array, doubles when full */
    size_t     cell_count, cell_cap;

    CallerNode *callers[CALLER_BUCKETS];
    size_t      caller_count;

    unsigned long long_calls;
    LogRecord     longest_call;
    bool          has_longest;
} Stats;

void stats_init(Stats *s);

/* Adds one valid record. Returns 0, or -1 if memory runs out. */
int stats_add(Stats *s, const LogRecord *r);

/* Returns a newly allocated array of every caller, sorted by count
 * (highest first). The caller must free() the array, not the nodes. */
CallerNode **stats_sorted_callers(const Stats *s);

void stats_free(Stats *s);

#endif
