# Telecom Log Analyzer

A command-line tool written in C that reads large telecom log files, checks every record, catches malformed lines, detects unusual activity and prints a clear summary report.

It streams the file one line at a time, so memory use stays flat whether the log has 100 lines or 10 million.

## Features

- **Validates every field** of each record: timestamp format and ranges, event type, cell ID, phone numbers, duration and status. Each rejected line gets a specific reason.
- **Detects anomalies:**
  - cell towers with a high dropped/failed rate (the threshold is configurable)
  - calls longer than 4 hours
  - callers sending far more events than average, a possible sign of spam or abuse
  - log sources with too many malformed lines
- **Per-cell summary:** event counts, drop and fail rates, and total call minutes for each tower.
- **Memory-efficient design:**
  - one reusable line buffer that doubles only when a line doesn't fit
  - a dynamic array for cell stats
  - a chained hash table (djb2) for counting callers
- **Reads from a file or stdin**, so it works in Bash pipelines.
- **Tested:** 21 automated tests, and the debug build runs clean under AddressSanitizer and UndefinedBehaviorSanitizer.

## Build

You need `gcc` and `make` (on Linux, macOS, or WSL on Windows).

```bash
make            # builds ./analyzer
make test       # runs the test suite
make debug      # builds with -g and AddressSanitizer for GDB debugging
```

## Usage

```bash
./analyzer data/sample_logs.txt                 # analyze a file
./analyzer -t 5 -n 10 data/sample_logs.txt      # 5% threshold, top 10 callers
./analyzer -v data/sample_logs.txt              # show why each bad line was rejected
./analyzer -o report.txt data/sample_logs.txt   # save the report to a file

# generate a 100,000-line test log and pipe it straight in
./scripts/gen_logs.sh 100000 | ./analyzer -
```

| Option    | Meaning                                                          |
|-----------|------------------------------------------------------------------|
| `-t PCT`  | Flag cells whose dropped+failed rate is above PCT (default 10)   |
| `-n N`    | Number of top callers to list (default 5)                        |
| `-o FILE` | Write the report to a file                                       |
| `-v`      | Print each malformed line number and reason to stderr            |

## Log format

One record per line, with fields separated by `|`:

```
timestamp|event|cell_id|caller|callee|duration_sec|status
2026-03-01 00:00:02|DATA|CELL006|6475550015|4165550349|2655|OK
```

| Field         | Rule                                         |
|---------------|----------------------------------------------|
| timestamp     | `YYYY-MM-DD HH:MM:SS`                        |
| event         | `CALL`, `SMS` or `DATA`                      |
| cell_id       | `CELL` + 3 digits                            |
| caller/callee | 10 digits                                    |
| duration_sec  | 0 – 86400 (must be 0 for SMS)                |
| status        | `OK`, `DROPPED` or `FAILED`                  |

Blank lines and lines starting with `#` are skipped.

## Example output

```
==========================================================
 TELECOM LOG ANALYSIS REPORT
 Source: data/sample_logs.txt
==========================================================

--- Overview --------------------------------------------
Lines read      : 1001
Valid records   : 966 (96.5%)
Malformed lines : 34 (3.4%)
Blank lines     : 1
Events          : CALL 475 | SMS 347 | DATA 144
Status          : OK 898 | DROPPED 36 | FAILED 32

--- Per-cell summary ------------------------------------
CELL       EVENTS    DROP%    FAIL%     CALL MIN
CELL001       119     1.7%     1.7%        425.1
CELL002       105     1.0%     1.9%        810.3
...
CELL007       132    11.4%    15.2%        430.4

--- Anomalies -------------------------------------------
[!] CELL007: 26.5% dropped/failed (threshold 10.0%)
[!] 1 call(s) longer than 4 hours. Longest: 7.1 h on 2026-03-01 04:31:30 (5195550351 -> 6475550274, CELL002)
[!] Caller 4165550199: 58 events (43x the average of 1.3) - possible spam or abuse
```

With `-v`:

```
line 7: bad timestamp '2026-13-01 25:00:00'
line 45: unknown status 'UNKNOWN'
line 134: duration 'abc' is not a number
```

## Project structure

```
include/        header files (the public interface of each module)
src/
  main.c        command-line options, read loop, error handling
  linebuf.c     growable line buffer for reading lines of any length
  record.c      splits and validates one log line into a LogRecord struct
  stats.c       aggregates records: cell array + caller hash table
  report.c      formats the summary and anomaly report
scripts/
  gen_logs.sh   generates fake logs with planted anomalies for testing
tests/
  fixture.log   hand-written log with known answers
  run_tests.sh  automated tests
data/
  sample_logs.txt   1,000-line sample (fake data)
```

## Notes

All data in this repo is randomly generated. No real customer or network data is included.

## Tech

C11 · Linux · Bash · Make · GDB · AddressSanitizer
# Modular-Telecom-Log-Analyzer
