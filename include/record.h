#ifndef RECORD_H
#define RECORD_H

#include <stdbool.h>
#include <stddef.h>

#define TS_LEN    20   /* "YYYY-MM-DD HH:MM:SS" + '\0' */
#define CELL_LEN  8    /* "CELL###" + '\0'              */
#define PHONE_LEN 11   /* 10 digits + '\0'              */

#define MAX_DURATION 86400L  /* one day, in seconds */

typedef enum { EV_CALL, EV_SMS, EV_DATA, EV_COUNT } EventType;
typedef enum { ST_OK, ST_DROPPED, ST_FAILED, ST_COUNT } Status;

/* One parsed line of a telecom log:
 * timestamp|event|cell_id|caller|callee|duration_sec|status */
typedef struct {
    char      timestamp[TS_LEN];
    EventType event;
    char      cell_id[CELL_LEN];
    char      caller[PHONE_LEN];
    char      callee[PHONE_LEN];
    long      duration;
    Status    status;
} LogRecord;

/* Parses `line` (modified in place) into `out`.
 * Returns true on success. On failure, writes a reason into `err`. */
bool parse_record(char *line, LogRecord *out, char *err, size_t errlen);

const char *event_name(EventType e);
const char *status_name(Status s);

#endif
