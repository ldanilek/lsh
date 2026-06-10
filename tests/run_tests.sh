#!/bin/bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
LSH="$ROOT/lsh"
PASS=0
FAIL=0

if [[ ! -x "$LSH" ]]; then
    echo "lsh binary not found; run 'make' first"
    exit 1
fi

# Detach from controlling tty so pipeline children never block on SIGTTIN.
run_lsh() {
    "$LSH" "$@" < /dev/null
}

assert_eq() {
    local desc="$1" expected="$2"
    shift 2
    local actual
    actual="$(run_lsh "$@")" || true
    if [[ "$actual" == "$expected" ]]; then
        echo "PASS: $desc"
        PASS=$((PASS + 1))
    else
        echo "FAIL: $desc"
        echo "  expected: [$expected]"
        echo "  actual:   [$actual]"
        FAIL=$((FAIL + 1))
    fi
}

assert_status() {
    local desc="$1" expected="$2"
    shift 2
    set +e
    run_lsh "$@"
    local actual=$?
    set -e
    if [[ "$actual" -eq "$expected" ]]; then
        echo "PASS: $desc"
        PASS=$((PASS + 1))
    else
        echo "FAIL: $desc (expected status $expected, got $actual)"
        FAIL=$((FAIL + 1))
    fi
}

assert_file_eq() {
    local desc="$1" expected="$2" file="$3"
    local actual
    actual="$(cat "$file" | tr -d '\r')"
    if [[ "$actual" == "$expected" ]]; then
        echo "PASS: $desc"
        PASS=$((PASS + 1))
    else
        echo "FAIL: $desc"
        echo "  expected: [$expected]"
        echo "  actual:   [$(cat "$file")]"
        FAIL=$((FAIL + 1))
    fi
}

TMPDIR="${TMPDIR:-/tmp}/lsh-test-$$"
mkdir -p "$TMPDIR"
trap 'rm -rf "$TMPDIR"' EXIT

# Basic commands
assert_eq "echo hello" "hello" -c "echo hello"
assert_eq "true produces 0" "0" -c "true; echo \$?"
assert_eq "false produces 1" "1" -c "false; echo \$?"
assert_status "exit code" 42 -c "exit 42"

# Variables
assert_eq "variable expansion" "bar" -c 'FOO=bar; echo $FOO'
assert_eq "export persists" "baz" -c 'export X=baz; echo $X'

# Pipes
assert_eq "simple pipe" "hello" -c "echo hello | cat"
WC_OUT=$(run_lsh -c 'echo one two three | wc -w' | tr -dc '0-9')
if [[ "$WC_OUT" == "3" ]]; then
    echo "PASS: pipe wc"
    PASS=$((PASS + 1))
else
    echo "FAIL: pipe wc (expected 3, got [$WC_OUT])"
    FAIL=$((FAIL + 1))
fi

# Redirects
OUT="$TMPDIR/out.txt"
run_lsh -c "echo redirected > $OUT"
assert_file_eq "output redirect" "redirected" "$OUT"

run_lsh -c "ls /lsh_nonexistent_path_xyz 2> $TMPDIR/err.txt" 2>/dev/null || true
if grep -q . "$TMPDIR/err.txt"; then
    echo "PASS: stderr redirect"
    PASS=$((PASS + 1))
else
    echo "FAIL: stderr redirect (file empty)"
    FAIL=$((FAIL + 1))
fi

# Append
run_lsh -c "echo line1 >> $TMPDIR/append.txt"
run_lsh -c "echo line2 >> $TMPDIR/append.txt"
assert_file_eq "append redirect" $'line1\nline2' "$TMPDIR/append.txt"

# Input redirect
echo "input data" > "$TMPDIR/in.txt"
assert_eq "input redirect" "input data" -c "cat < $TMPDIR/in.txt"

# Logical operators
assert_eq "and success" "yes" -c "true && echo yes"
assert_eq "and fail" "" -c "false && echo no"
assert_eq "or fallback" "ok" -c "false || echo ok"
assert_eq "semicolon" $'a\nb' -c "echo a; echo b"

# Builtins
assert_eq "pwd" "$(pwd)" -c "pwd"
CD_OUT=$(run_lsh -c "cd /tmp && pwd")
if [[ "$CD_OUT" == *"tmp"* ]]; then
    echo "PASS: cd"
    PASS=$((PASS + 1))
else
    echo "FAIL: cd (got [$CD_OUT])"
    FAIL=$((FAIL + 1))
fi
assert_eq "type builtin" "echo is a shell builtin" -c "type echo"

# Glob
touch "$TMPDIR/testglob_a" "$TMPDIR/testglob_b"
GLOB_OUT=$(run_lsh -c "echo $TMPDIR/testglob_*")
if echo "$GLOB_OUT" | grep -q testglob_a && echo "$GLOB_OUT" | grep -q testglob_b; then
    echo "PASS: glob expansion"
    PASS=$((PASS + 1))
else
    echo "FAIL: glob expansion got [$GLOB_OUT]"
    FAIL=$((FAIL + 1))
fi

# Script execution
cat > "$TMPDIR/script.lsh" <<'SCRIPT'
echo script_ok
FOO=from_script
echo $FOO
SCRIPT
assert_eq "script file" $'script_ok\nfrom_script' "$TMPDIR/script.lsh"

# Source
echo 'echo sourced' > "$TMPDIR/source.lsh"
assert_eq "source builtin" "sourced" -c "source $TMPDIR/source.lsh"

# Comments
assert_eq "comments" "hi" -c $'# comment\necho hi'

# Quotes
assert_eq "single quotes" 'a b' -c "echo 'a b'"
assert_eq "double quotes" "hello world" -c 'echo "hello world"'

# Background (use builtin only — no external sleep dependency)
run_lsh -c "true &" 2>/dev/null
echo "PASS: background command"
PASS=$((PASS + 1))

# Combined redirect and pipe
WC_LINES=$(run_lsh -c $'printf "a\\nb\\nc" | wc -l' | tr -dc '0-9')
if [[ "$WC_LINES" == "2" ]]; then
    echo "PASS: pipe with redirect"
    PASS=$((PASS + 1))
else
    echo "FAIL: pipe with redirect (expected 2, got [$WC_LINES])"
    FAIL=$((FAIL + 1))
fi

echo ""
echo "Results: $PASS passed, $FAIL failed"
if [[ "$FAIL" -gt 0 ]]; then
    exit 1
fi
