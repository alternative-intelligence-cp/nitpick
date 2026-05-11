#!/bin/bash
# Regression tests for v0.23.5: statement macros (MACRO-006) & code-generating macros (MACRO-007)
# Usage: ./run_bug_tests_0235.sh

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
NPKC="${SCRIPT_DIR}/../../build/npkc"
TMPDIR="/tmp/aria_macro_tests_0235"
mkdir -p "$TMPDIR"

echo "================================================================================="
echo "v0.23.5 Regression Tests: Statement & Code-Generating Macros (MACRO-006, 007)"
echo "================================================================================="

# ============================================================================
# MACRO-006 Tests: Statement-Position Macro Invocations
# ============================================================================

echo ""
echo "--- MACRO-006: Statement-Position Macros ---"

# Test bug133: Statement-position macro with print-like invocation
echo ""
echo "[bug133] Statement-position macro invoking println"
cat > "$TMPDIR/bug133.aria" << 'EOF'
macro:print_msg = () {
    println("hello from macro");
};

func:main = int32() {
    print_msg!();  // statement-position macro invocation
    exit(0);
};

func:failsafe = int32(tbb32:err) {
    exit(1);
};
EOF

"$NPKC" "$TMPDIR/bug133.aria" -o "$TMPDIR/bug133" 2>&1
"$TMPDIR/bug133" > /tmp/bug133_out.txt 2>&1
if grep -q "hello from macro" /tmp/bug133_out.txt; then
    echo "✓ PASS: bug133"
else
    echo "✗ FAIL: bug133 - expected output not found"
    cat /tmp/bug133_out.txt
    exit 1
fi

# Test bug134: Macro at statement level with multiple statements in body
echo ""
echo "[bug134] Macro with multiple statements in body"
cat > "$TMPDIR/bug134.aria" << 'EOF'
macro:multi_print = () {
    println("line 1");
    println("line 2");
};

func:main = int32() {
    multi_print!();
    exit(0);
};

func:failsafe = int32(tbb32:err) {
    exit(1);
};
EOF

"$NPKC" "$TMPDIR/bug134.aria" -o "$TMPDIR/bug134" 2>&1
"$TMPDIR/bug134" > /tmp/bug134_out.txt 2>&1
if grep -q "line 1" /tmp/bug134_out.txt && grep -q "line 2" /tmp/bug134_out.txt; then
    echo "✓ PASS: bug134"
else
    echo "✗ FAIL: bug134"
    cat /tmp/bug134_out.txt
    exit 1
fi

# Test bug135: Nested macro calls at statement level
echo ""
echo "[bug135] Nested macro calls at statement level"
cat > "$TMPDIR/bug135.aria" << 'EOF'
macro:inner = () {
    println("inner macro");
};

macro:outer = () {
    inner!();
};

func:main = int32() {
    outer!();
    exit(0);
};

func:failsafe = int32(tbb32:err) {
    exit(1);
};
EOF

"$NPKC" "$TMPDIR/bug135.aria" -o "$TMPDIR/bug135" 2>&1
"$TMPDIR/bug135" > /tmp/bug135_out.txt 2>&1
if grep -q "inner macro" /tmp/bug135_out.txt; then
    echo "✓ PASS: bug135"
else
    echo "✗ FAIL: bug135"
    cat /tmp/bug135_out.txt
    exit 1
fi

# Test bug136: Macro invocation mixed with other statements
echo ""
echo "[bug136] Macro mixed with other statements"
cat > "$TMPDIR/bug136.aria" << 'EOF'
macro:say_hello = () {
    println("hello");
};

func:main = int32() {
    int32:x = 5;
    say_hello!();
    int32:y = 10;
    println("done");
    exit(0);
};

func:failsafe = int32(tbb32:err) {
    exit(1);
};
EOF

"$NPKC" "$TMPDIR/bug136.aria" -o "$TMPDIR/bug136" 2>&1
"$TMPDIR/bug136" > /tmp/bug136_out.txt 2>&1
if grep -q "hello" /tmp/bug136_out.txt && grep -q "done" /tmp/bug136_out.txt; then
    echo "✓ PASS: bug136"
else
    echo "✗ FAIL: bug136"
    cat /tmp/bug136_out.txt
    exit 1
fi

# ============================================================================
# MACRO-007 Tests: Code-Generating Macros (TBD - placeholder for now)
# ============================================================================

echo ""
echo "--- MACRO-007: Code-Generating Macros ---"
echo "[placeholder] Code-generating macro tests deferred to later implementation"

echo ""
echo "================================================================================="
echo "All v0.23.5 regression tests passed!"
echo "================================================================================="
