#!/usr/bin/env bash
# Runs the analyzer on known inputs and checks the results.
set -uo pipefail

cd "$(dirname "$0")/.."
BIN=./analyzer
pass=0
fail=0

check() {   # check "description" "text to find" "output"
    if grep -qF -- "$2" <<< "$3"; then
        echo "  PASS  $1"; pass=$((pass + 1))
    else
        echo "  FAIL  $1  (expected: $2)"; fail=$((fail + 1))
    fi
}

echo "== fixture.log =="
out=$($BIN tests/fixture.log 2>&1)
check "counts every line"             "Lines read      : 16"        "$out"
check "finds the 4 valid records"     "Valid records   : 4 "        "$out"
check "rejects 10 malformed lines"    "Malformed lines : 10 "       "$out"
check "skips comment + blank line"    "Blank lines     : 2"         "$out"
check "flags the 5.6 hour call"       "1 call(s) longer than 4 hours" "$out"

echo "== verbose reasons =="
err=$($BIN -v tests/fixture.log 2>&1 >/dev/null)
check "bad month/hour"        "bad timestamp"                     "$err"
check "unknown event"         "unknown event type 'VIDEO'"        "$err"
check "short cell id"         "bad cell id 'CELL01'"              "$err"
check "short phone number"    "bad caller number '41655'"         "$err"
check "non-numeric duration"  "duration 'abc' is not a number"    "$err"
check "SMS with a duration"   "SMS record has non-zero duration"  "$err"
check "unknown status"        "unknown status 'MAYBE'"            "$err"
check "missing fields"        "expected 7 fields, got 4"          "$err"
check "extra field"           "expected 7 fields, got more than 7" "$err"
check "negative duration"     "duration -5 out of range"          "$err"

echo "== generated log (stdin) =="
out=$(bash scripts/gen_logs.sh 5000 7 | $BIN - 2>&1)
check "reads from stdin"          "Source: (stdin)"            "$out"
check "flags the bad cell"        "[!] CELL007"                "$out"
check "flags the heavy caller"    "[!] Caller 4165550199"      "$out"

echo "== very long line =="
long=$(printf 'x%.0s' $(seq 1 5000))
out=$(printf '%s\n2026-03-01 08:00:00|CALL|CELL001|4165550001|4165550002|60|OK\n' "$long" | $BIN - 2>&1)
check "survives a 5000-char line" "Valid records   : 1 "      "$out"

echo "== errors =="
$BIN /no/such/file >/dev/null 2>&1; code=$?
check "missing file exits 2" "2" "$code"
$BIN -t 500 tests/fixture.log >/dev/null 2>&1; code=$?
check "bad -t exits 1" "1" "$code"

echo
echo "$pass passed, $fail failed"
[ "$fail" -eq 0 ]
