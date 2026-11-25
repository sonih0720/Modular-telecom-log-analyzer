#include "record.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NUM_FIELDS 7

static const char *EVENT_NAMES[EV_COUNT]  = { "CALL", "SMS", "DATA" };
static const char *STATUS_NAMES[ST_COUNT] = { "OK", "DROPPED", "FAILED" };

const char *event_name(EventType e)  { return (e < EV_COUNT) ? EVENT_NAMES[e] : "?"; }
const char *status_name(Status s)    { return (s < ST_COUNT) ? STATUS_NAMES[s] : "?"; }

/* Splits on '|' in place. Unlike strtok, empty fields are kept,
 * so "a||b" gives three fields. Returns the field count, or max + 1
 * if there are too many. */
static int split_fields(char *line, char *fields[], int max)
{
    int n = 0;
    char *p = line;
    for (;;) {
        if (n == max)
            return max + 1;
        fields[n++] = p;
        char *bar = strchr(p, '|');
        if (!bar)
            return n;
        *bar = '\0';
        p = bar + 1;
    }
}

static bool all_digits(const char *s, size_t len)
{
    for (size_t i = 0; i < len; i++)
        if (!isdigit((unsigned char)s[i]))
            return false;
    return true;
}

static int two_digits(const char *s) { return (s[0] - '0') * 10 + (s[1] - '0'); }

/* Expects exactly "YYYY-MM-DD HH:MM:SS" with sane ranges. */
static bool valid_timestamp(const char *s)
{
    if (strlen(s) != 19)
        return false;
    for (int i = 0; i < 19; i++) {
        char c = s[i];
        if (i == 4 || i == 7)        { if (c != '-') return false; }
        else if (i == 10)            { if (c != ' ') return false; }
        else if (i == 13 || i == 16) { if (c != ':') return false; }
        else if (!isdigit((unsigned char)c)) return false;
    }
    int month = two_digits(s + 5), day = two_digits(s + 8);
    int hour  = two_digits(s + 11), min = two_digits(s + 14), sec = two_digits(s + 17);
    return month >= 1 && month <= 12 && day >= 1 && day <= 31 &&
           hour < 24 && min < 60 && sec < 60;
}

static bool valid_phone(const char *s) { return strlen(s) == 10 && all_digits(s, 10); }

static bool valid_cell(const char *s)
{
    return strlen(s) == 7 && strncmp(s, "CELL", 4) == 0 && all_digits(s + 4, 3);
}

static int lookup(const char *s, const char *names[], int count)
{
    for (int i = 0; i < count; i++)
        if (strcmp(s, names[i]) == 0)
            return i;
    return -1;
}

#define FAIL(...) do { snprintf(err, errlen, __VA_ARGS__); return false; } while (0)

bool parse_record(char *line, LogRecord *out, char *err, size_t errlen)
{
    /* strip trailing newline / carriage return / spaces */
    size_t len = strlen(line);
    while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r' || line[len - 1] == ' '))
        line[--len] = '\0';

    char *f[NUM_FIELDS];
    int n = split_fields(line, f, NUM_FIELDS);
    if (n != NUM_FIELDS)
        FAIL("expected %d fields, got %s%d", NUM_FIELDS, n > NUM_FIELDS ? "more than " : "",
             n > NUM_FIELDS ? NUM_FIELDS : n);

    if (!valid_timestamp(f[0]))
        FAIL("bad timestamp '%s'", f[0]);

    int ev = lookup(f[1], EVENT_NAMES, EV_COUNT);
    if (ev < 0)
        FAIL("unknown event type '%s'", f[1]);

    if (!valid_cell(f[2]))
        FAIL("bad cell id '%s'", f[2]);
    if (!valid_phone(f[3]))
        FAIL("bad caller number '%s'", f[3]);
    if (!valid_phone(f[4]))
        FAIL("bad callee number '%s'", f[4]);

    errno = 0;
    char *end;
    long dur = strtol(f[5], &end, 10);
    if (f[5][0] == '\0' || *end != '\0' || errno == ERANGE)
        FAIL("duration '%s' is not a number", f[5]);
    if (dur < 0 || dur > MAX_DURATION)
        FAIL("duration %ld out of range", dur);
    if (ev == EV_SMS && dur != 0)
        FAIL("SMS record has non-zero duration %ld", dur);

    int st = lookup(f[6], STATUS_NAMES, ST_COUNT);
    if (st < 0)
        FAIL("unknown status '%s'", f[6]);

    /* every field is validated, so these copies always fit */
    memcpy(out->timestamp, f[0], TS_LEN);
    memcpy(out->cell_id,   f[2], CELL_LEN);
    memcpy(out->caller,    f[3], PHONE_LEN);
    memcpy(out->callee,    f[4], PHONE_LEN);
    out->event    = (EventType)ev;
    out->duration = dur;
    out->status   = (Status)st;
    return true;
}
