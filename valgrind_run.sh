#!/bin/sh
# valgrind_run.sh - memory / fd-leak / data-race checks for mycp.
#   1. unit-test binary under memcheck
#   2. the real ./mycp binary, many option combinations, under memcheck
#   3. threaded copy under helgrind (data races) and drd
# Exit status is non-zero if valgrind reports any error.
set -u
cd "$(dirname "$0")/.." || exit 1

BIN=./mycp
TESTBIN=build/test/unit_tests
OUT=reports/valgrind.txt
VG="valgrind --error-exitcode=99 --leak-check=full --show-leak-kinds=all \
--errors-for-leak-kinds=all --track-origins=yes -q"
mkdir -p reports
: > "$OUT"
fail=0
T=$(mktemp -d /tmp/mycp_vg.XXXXXX)
trap 'rm -rf "$T"' EXIT

run() {  # run <label> <valgrind-cmd...>
    label=$1; shift
    "$@" >"$T/stdout" 2>"$T/stderr"
    rc=$?
    if [ $rc -eq 99 ]; then
        echo "FAIL  $label" | tee -a "$OUT"; cat "$T/stderr" >> "$OUT"; fail=1
    else
        echo "ok    $label (exit $rc)" | tee -a "$OUT"
    fi
}

# --- test data ---------------------------------------------------------
head -c 300000 /dev/urandom > "$T/big"
head -c 10      /dev/urandom > "$T/small"
: > "$T/empty"
mkdir -p "$T/tree/a/b" "$T/tree/empty"
for i in 1 2 3 4 5 6 7 8 9 10 11 12; do head -c $((i * 1000)) /dev/urandom > "$T/tree/f$i"; done
head -c 5000 /dev/urandom > "$T/tree/a/x"; head -c 7000 /dev/urandom > "$T/tree/a/b/y"
ln -s f1 "$T/tree/link"
head -c 100000 "$T/big" > "$T/partial"

# --- 1. unit tests -----------------------------------------------------
if [ -x "$TESTBIN" ]; then
    run "unit tests (memcheck)" $VG $TESTBIN
fi

# --- 2. CLI scenarios under memcheck ----------------------------------
V="$VG --track-fds=yes"
run "plain copy"                 $V $BIN "$T/big" "$T/o1"
run "empty file"                 $V $BIN "$T/empty" "$T/o2"
run "tiny buffer -b 1"           $V $BIN -b 1 "$T/small" "$T/o3"
run "big buffer -b 1M"           $V $BIN -b 1M "$T/big" "$T/o4"
run "info+progress+preserve+verify" $V $BIN -i -p -m -v "$T/big" "$T/o5"
run "resume -c"                  $V $BIN -c "$T/big" "$T/partial"
run "copy into directory"        $V $BIN "$T/big" "$T/tree/empty"
run "log file -l"                $V $BIN -l "$T/log" "$T/big" "$T/o6"
run "benchmark -B"               $V $BIN -B "$T/big" "$T/o7"
run "recursive -r"               $V $BIN -r "$T/tree" "$T/r1"
run "recursive -r -m -i -p"      $V $BIN -r -m -i -p "$T/tree" "$T/r2"
run "recursive -r -j 4 -v"       $V $BIN -r -j 4 -v "$T/tree" "$T/r3"
run "dest inside source"         $V $BIN -r "$T/tree" "$T/tree/out"
run "error: missing source"      $V $BIN "$T/nope" "$T/o8"
run "error: dir without -r"      $V $BIN "$T/tree" "$T/o9"
run "error: same file"           $V $BIN "$T/big" "$T/big"
run "error: bad -b"              $V $BIN -b zzz "$T/big" "$T/o10"
run "error: bad -j"              $V $BIN -j 0 "$T/big" "$T/o11"
run "error: bad log path"        $V $BIN -l /no/such/dir/log "$T/big" "$T/o12"
run "help -h"                    $V $BIN -h
run "usage (no args)"            $V $BIN

# --- 3. thread checks --------------------------------------------------
run "helgrind  -r -j 4"  valgrind -q --tool=helgrind --suppressions=tools/helgrind.supp --error-exitcode=99 $BIN -r -j 4 "$T/tree" "$T/h1"
run "helgrind  -r -j 8 -v" valgrind -q --tool=helgrind --suppressions=tools/helgrind.supp --error-exitcode=99 $BIN -r -j 8 -v "$T/tree" "$T/h2"
run "drd       -r -j 4"  valgrind -q --tool=drd --error-exitcode=99 $BIN -r -j 4 "$T/tree" "$T/d1"

echo
if [ $fail -eq 0 ]; then echo "valgrind: ALL CLEAN" | tee -a "$OUT"; else echo "valgrind: ERRORS FOUND (see $OUT)" | tee -a "$OUT"; fi
exit $fail
