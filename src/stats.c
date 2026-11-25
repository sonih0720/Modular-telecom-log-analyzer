#include "stats.h"

#include <stdlib.h>
#include <string.h>

void stats_init(Stats *s)
{
    memset(s, 0, sizeof *s);
}

/* djb2 string hash */
static size_t hash(const char *str)
{
    size_t h = 5381;
    for (const unsigned char *p = (const unsigned char *)str; *p; p++)
        h = h * 33 + *p;
    return h % CALLER_BUCKETS;
}

static CellStats *find_or_add_cell(Stats *s, const char *cell_id)
{
    for (size_t i = 0; i < s->cell_count; i++)
        if (strcmp(s->cells[i].cell_id, cell_id) == 0)
            return &s->cells[i];

    if (s->cell_count == s->cell_cap) {
        size_t new_cap = s->cell_cap ? s->cell_cap * 2 : 8;
        CellStats *nc = realloc(s->cells, new_cap * sizeof *nc);
        if (!nc)
            return NULL;
        s->cells = nc;
        s->cell_cap = new_cap;
    }
    CellStats *c = &s->cells[s->cell_count++];
    memset(c, 0, sizeof *c);
    strcpy(c->cell_id, cell_id);
    return c;
}

static int count_caller(Stats *s, const char *number)
{
    size_t b = hash(number);
    for (CallerNode *n = s->callers[b]; n; n = n->next) {
        if (strcmp(n->number, number) == 0) {
            n->count++;
            return 0;
        }
    }
    CallerNode *n = malloc(sizeof *n);
    if (!n)
        return -1;
    strcpy(n->number, number);
    n->count = 1;
    n->next = s->callers[b];
    s->callers[b] = n;
    s->caller_count++;
    return 0;
}

int stats_add(Stats *s, const LogRecord *r)
{
    s->valid++;
    s->by_event[r->event]++;
    s->by_status[r->status]++;

    CellStats *c = find_or_add_cell(s, r->cell_id);
    if (!c)
        return -1;
    c->total++;
    c->by_status[r->status]++;

    if (r->event == EV_CALL) {
        c->call_seconds += (unsigned long long)r->duration;
        if (r->duration > LONG_CALL_SECONDS)
            s->long_calls++;
        if (!s->has_longest || r->duration > s->longest_call.duration) {
            s->longest_call = *r;
            s->has_longest = true;
        }
    }
    return count_caller(s, r->caller);
}

static int by_count_desc(const void *a, const void *b)
{
    const CallerNode *x = *(CallerNode *const *)a;
    const CallerNode *y = *(CallerNode *const *)b;
    if (x->count != y->count)
        return (x->count < y->count) ? 1 : -1;
    return strcmp(x->number, y->number);
}

CallerNode **stats_sorted_callers(const Stats *s)
{
    if (s->caller_count == 0)
        return NULL;
    CallerNode **arr = malloc(s->caller_count * sizeof *arr);
    if (!arr)
        return NULL;
    size_t k = 0;
    for (size_t b = 0; b < CALLER_BUCKETS; b++)
        for (CallerNode *n = s->callers[b]; n; n = n->next)
            arr[k++] = n;
    qsort(arr, k, sizeof *arr, by_count_desc);
    return arr;
}

void stats_free(Stats *s)
{
    free(s->cells);
    for (size_t b = 0; b < CALLER_BUCKETS; b++) {
        CallerNode *n = s->callers[b];
        while (n) {
            CallerNode *next = n->next;
            free(n);
            n = next;
        }
    }
    memset(s, 0, sizeof *s);
}
