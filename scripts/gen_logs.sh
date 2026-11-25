#!/usr/bin/env bash
# Generates a fake telecom log for testing the analyzer.
# All numbers are made up; no real customer data is used.
#
# Usage: ./scripts/gen_logs.sh [num_lines] [seed] > logs.txt
#
# The output deliberately includes things the analyzer should catch:
#   - about 3% malformed lines
#   - CELL007 with a high dropped/failed rate
#   - one caller (4165550199) sending far more events than anyone else
#   - a few calls that run longer than 4 hours

set -euo pipefail

LINES="${1:-1000}"
SEED="${2:-42}"

if ! [[ "$LINES" =~ ^[0-9]+$ ]] || ! [[ "$SEED" =~ ^[0-9]+$ ]]; then
    echo "usage: $0 [num_lines] [seed]" >&2
    exit 1
fi

awk -v n="$LINES" -v seed="$SEED" '
function rint(lo, hi) { return lo + int(rand() * (hi - lo + 1)) }
# portable timestamp (macOS awk has no strftime); uses 28-day months for simplicity
function stamp(s,   d) {
    d = int(s / 86400)
    return sprintf("2026-%02d-%02d %02d:%02d:%02d", 3 + int(d / 28) % 10, 1 + d % 28,
                   int(s / 3600) % 24, int(s / 60) % 60, s % 60)
}
function phone()      { split("416 647 905 519", ac, " "); return sprintf("%s555%04d", ac[rint(1, 4)], rint(0, 499)) }
BEGIN {
    srand(seed)
    split("CALL CALL CALL SMS SMS DATA", events, " ")
    t = 0            # seconds since 2026-03-01 00:00:00
    print "# timestamp|event|cell_id|caller|callee|duration_sec|status"

    for (i = 1; i <= n; i++) {
        t += rint(1, 40)
        ts   = stamp(t)
        ev   = events[rint(1, 6)]
        cell = sprintf("CELL%03d", rint(1, 8))
        from = (rand() < 0.06) ? "4165550199" : phone()
        to   = phone()

        if (ev == "CALL")      dur = (rand() < 0.003) ? rint(14401, 30000) : rint(5, 900)
        else if (ev == "DATA") dur = rint(1, 3600)
        else                   dur = 0

        bad = (cell == "CELL007") ? 0.30 : 0.04
        r = rand()
        st = (r < bad / 2) ? "DROPPED" : (r < bad) ? "FAILED" : "OK"

        # inject malformed lines
        if (rand() < 0.03) {
            k = rint(1, 6)
            if (k == 1) ts   = "2026-13-01 25:00:00"
            if (k == 2) ev   = "VIDEO"
            if (k == 3) from = "41655"
            if (k == 4) dur  = "abc"
            if (k == 5) { print ts "|" ev "|" cell "|" from; continue }
            if (k == 6) st   = "UNKNOWN"
        }
        print ts "|" ev "|" cell "|" from "|" to "|" dur "|" st
    }
}'
