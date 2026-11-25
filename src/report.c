#include "report.h"

#include <stdlib.h>
#include <string.h>

#define MIN_CELL_EVENTS      20    /* ignore tiny cells when flagging   */
#define HEAVY_CALLER_FACTOR  5.0   /* x the average count = suspicious  */
#define MIN_HEAVY_EVENTS     20

static double pct(unsigned long part, unsigned long whole)
{
    return whole ? 100.0 * (double)part / (double)whole : 0.0;
}

static int by_cell_id(const void *a, const void *b)
{
    return strcmp(((const CellStats *)a)->cell_id, ((const CellStats *)b)->cell_id);
}

static void rule(FILE *out, const char *title)
{
    fprintf(out, "\n--- %s ", title);
    for (size_t i = strlen(title); i < 52; i++)
        fputc('-', out);
    fputc('\n', out);
}

void write_report(FILE *out, Stats *s, const ReportOptions *opt)
{
    fprintf(out, "==========================================================\n");
    fprintf(out, " TELECOM LOG ANALYSIS REPORT\n");
    fprintf(out, " Source: %s\n", opt->source);
    fprintf(out, "==========================================================\n");

    rule(out, "Overview");
    fprintf(out, "Lines read      : %lu\n", s->lines);
    fprintf(out, "Valid records   : %lu (%.1f%%)\n", s->valid, pct(s->valid, s->lines));
    fprintf(out, "Malformed lines : %lu (%.1f%%)\n", s->malformed, pct(s->malformed, s->lines));
    fprintf(out, "Blank lines     : %lu\n", s->blank);
    fprintf(out, "Events          : CALL %lu | SMS %lu | DATA %lu\n",
            s->by_event[EV_CALL], s->by_event[EV_SMS], s->by_event[EV_DATA]);
    fprintf(out, "Status          : OK %lu | DROPPED %lu | FAILED %lu\n",
            s->by_status[ST_OK], s->by_status[ST_DROPPED], s->by_status[ST_FAILED]);

    /* ---- per cell ---- */
    qsort(s->cells, s->cell_count, sizeof *s->cells, by_cell_id);
    rule(out, "Per-cell summary");
    fprintf(out, "%-8s %8s %8s %8s %12s\n", "CELL", "EVENTS", "DROP%", "FAIL%", "CALL MIN");
    for (size_t i = 0; i < s->cell_count; i++) {
        const CellStats *c = &s->cells[i];
        fprintf(out, "%-8s %8lu %7.1f%% %7.1f%% %12.1f\n", c->cell_id, c->total,
                pct(c->by_status[ST_DROPPED], c->total),
                pct(c->by_status[ST_FAILED], c->total),
                (double)c->call_seconds / 60.0);
    }

    /* ---- anomalies ---- */
    rule(out, "Anomalies");
    int flagged = 0;

    for (size_t i = 0; i < s->cell_count; i++) {
        const CellStats *c = &s->cells[i];
        double bad = pct(c->by_status[ST_DROPPED] + c->by_status[ST_FAILED], c->total);
        if (c->total >= MIN_CELL_EVENTS && bad > opt->fail_threshold) {
            fprintf(out, "[!] %s: %.1f%% dropped/failed (threshold %.1f%%)\n",
                    c->cell_id, bad, opt->fail_threshold);
            flagged++;
        }
    }

    if (s->long_calls > 0) {
        fprintf(out, "[!] %lu call(s) longer than %ld hours. Longest: %.1f h on %s "
                     "(%s -> %s, %s)\n",
                s->long_calls, LONG_CALL_SECONDS / 3600,
                (double)s->longest_call.duration / 3600.0, s->longest_call.timestamp,
                s->longest_call.caller, s->longest_call.callee, s->longest_call.cell_id);
        flagged++;
    }

    CallerNode **callers = stats_sorted_callers(s);
    if (callers) {
        double avg = (double)s->valid / (double)s->caller_count;
        for (size_t i = 0; i < s->caller_count; i++) {
            double c = (double)callers[i]->count;
            if (c < MIN_HEAVY_EVENTS || c < avg * HEAVY_CALLER_FACTOR)
                break;  /* sorted, so nobody after this qualifies */
            fprintf(out, "[!] Caller %s: %lu events (%.0fx the average of %.1f) - "
                         "possible spam or abuse\n",
                    callers[i]->number, callers[i]->count, c / avg, avg);
            flagged++;
        }
    }

    if (s->malformed > 0 && pct(s->malformed, s->lines) > 5.0) {
        fprintf(out, "[!] %.1f%% of lines are malformed - check the log source\n",
                pct(s->malformed, s->lines));
        flagged++;
    }
    if (!flagged)
        fprintf(out, "No anomalies detected.\n");

    /* ---- top callers ---- */
    if (callers) {
        rule(out, "Top callers");
        size_t n = opt->top_n < s->caller_count ? opt->top_n : s->caller_count;
        for (size_t i = 0; i < n; i++)
            fprintf(out, "%2zu. %s  %lu events\n", i + 1, callers[i]->number, callers[i]->count);
        free(callers);
    }

    rule(out, "Memory");
    fprintf(out, "Peak line buffer: %zu bytes | cells tracked: %zu | unique callers: %zu\n",
            opt->peak_line_buf, s->cell_count, s->caller_count);
}
