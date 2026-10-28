#!/bin/sh
# Test suite for mbc. Run with:  make test     (or: sh tests/run_tests.sh)
#
# For every examples/NAME.mc that has a NAME.out file it checks that
#   1. the bytecode VM prints exactly NAME.out
#   2. the VM with constant folding (-O) prints exactly NAME.out
#   3. the tree-walking interpreter prints exactly NAME.out
#      (so two independent back ends agree with the hand-written answer)
#   4. compile to a .mbc file and exec it prints exactly NAME.out
# For every examples/errors/NAME.mc it checks that mbc fails with a non-zero
# exit status and that the message contains the text in NAME.err.

cd "$(dirname "$0")/.." || exit 1
BIN=./mbc
[ -x "$BIN" ] || { echo "build first: make"; exit 1; }

TMP=$(mktemp -d 2>/dev/null || echo /tmp/mbc_tests_$$)
mkdir -p "$TMP"
trap 'rm -rf "$TMP"' EXIT

pass=0
fail=0

ok()  { pass=$((pass + 1)); printf '  ok    %s\n' "$1"; }
bad() { fail=$((fail + 1)); printf '  FAIL  %s\n' "$1"; }

check_output() { # label, command...
    label=$1; shift
    expected=$(cat "$EXPECT")
    actual=$("$@" 2>&1)
    if [ "$actual" = "$expected" ]; then ok "$label"; else bad "$label"; fi
}

echo "== programs =="
for src in examples/*.mc; do
    base=${src%.mc}
    [ -f "$base.out" ] || continue
    EXPECT="$base.out"
    name=$(basename "$base")
    check_output "$name  (vm)"            $BIN run "$src"
    check_output "$name  (vm -O)"         $BIN run "$src" -O
    check_output "$name  (tree-walking)"  $BIN interp "$src"
    $BIN compile "$src" -o "$TMP/$name.mbc" >/dev/null
    check_output "$name  (compile + exec)" $BIN exec "$TMP/$name.mbc"
done

echo "== error handling =="
for src in examples/errors/*.mc; do
    base=${src%.mc}
    name=$(basename "$base")
    want=$(cat "$base.err")
    out=$($BIN run "$src" 2>&1)
    status=$?
    if [ "$status" -ne 0 ] && printf '%s' "$out" | grep -F -q "$want"; then ok "$name"; else bad "$name"; fi
done

echo "== tools =="
if $BIN tokens examples/hello.mc | grep -q "KEYWORD"; then ok "tokens"; else bad "tokens"; fi
if $BIN ast examples/hello.mc | grep -q "Function main"; then ok "ast"; else bad "ast"; fi
if $BIN dis examples/hello.mc | grep -q "PRINT"; then ok "dis"; else bad "dis"; fi
if $BIN run examples/expr.mc --trace | grep -q "output: 14"; then ok "trace"; else bad "trace"; fi
if [ "$($BIN dis examples/expr.mc -O | grep -c MUL)" = "0" ]; then ok "constant folding removes MUL"; else bad "constant folding removes MUL"; fi
if [ "$($BIN dis examples/deadcode.mc -O | grep -c "PUSH.*\(111\|333\|444\|999\)")" = "0" ]; then ok "dead-code removal drops unreachable code"; else bad "dead-code removal drops unreachable code"; fi
if $BIN run examples/deadcode.mc -O --stats 2>&1 | grep -q "dead statements removed: 6"; then ok "dead-code removal count"; else bad "dead-code removal count"; fi
if [ "$($BIN ast examples/breaks.mc | grep -c "Break\|Continue")" = "3" ]; then ok "break and continue in the syntax tree"; else bad "break and continue in the syntax tree"; fi

echo
echo "passed: $pass   failed: $fail"
[ "$fail" -eq 0 ]
