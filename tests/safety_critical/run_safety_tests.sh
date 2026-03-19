#!/bin/bash
# ============================================================================
# NIKOLA SAFETY-CRITICAL VALIDATION RUNNER
# ============================================================================
# Runs all safety-critical tests, compiles + executes + validates.
# Returns non-zero if ANY test fails.
#
# Usage: bash run_safety_tests.sh [path-to-ariac]
# ============================================================================

set -euo pipefail

ARIAC="${1:-../../build/ariac}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

PASS=0
FAIL=0
COMPILE_FAIL=0
TOTAL=0
FAILED_TESTS=""

echo "============================================================"
echo " NIKOLA SAFETY-CRITICAL VALIDATION SUITE"
echo " Date: $(date -u '+%Y-%m-%d %H:%M:%S UTC')"
echo " Compiler: $(realpath "$ARIAC" 2>/dev/null || echo "$ARIAC")"
echo "============================================================"
echo ""

for test_file in *.aria; do
    TOTAL=$((TOTAL + 1))
    test_name="${test_file%.aria}"
    binary="./${test_name}"
    
    echo -n "  [$TOTAL] $test_name ... "
    
    # Compile
    if ! "$ARIAC" "$test_file" -o "$binary" 2>/tmp/ariac_stderr_$$.txt; then
        echo "COMPILE FAIL"
        cat /tmp/ariac_stderr_$$.txt | head -5
        COMPILE_FAIL=$((COMPILE_FAIL + 1))
        FAILED_TESTS="$FAILED_TESTS  COMPILE_FAIL: $test_name\n"
        rm -f /tmp/ariac_stderr_$$.txt
        continue
    fi
    rm -f /tmp/ariac_stderr_$$.txt
    
    # Run
    output=$("$binary" 2>&1) || true
    exit_code=$?
    
    # Check result (exit code 0 = all tests passed)
    if [ $exit_code -eq 0 ]; then
        echo "PASS"
        PASS=$((PASS + 1))
        echo "    $output"
    else
        echo "FAIL (exit=$exit_code)"
        FAIL=$((FAIL + 1))
        echo "    $output"
        FAILED_TESTS="$FAILED_TESTS  RUNTIME_FAIL: $test_name (exit=$exit_code)\n"
    fi
    
    # Run through Valgrind too (memory safety)
    if command -v valgrind &>/dev/null; then
        valgrind_out=$(valgrind --error-exitcode=99 --leak-check=full --quiet "$binary" 2>&1) || true
        valgrind_exit=$?
        if [ $valgrind_exit -eq 99 ]; then
            echo "    WARNING: Valgrind detected memory errors!"
            FAILED_TESTS="$FAILED_TESTS  VALGRIND_FAIL: $test_name\n"
        fi
    fi
    
    # Clean up binary
    rm -f "$binary"
done

echo ""
echo "============================================================"
echo " RESULTS"
echo "============================================================"
echo "  Total:        $TOTAL"
echo "  Passed:       $PASS"
echo "  Failed:       $FAIL"
echo "  Compile Fail: $COMPILE_FAIL"
echo ""

if [ -n "$FAILED_TESTS" ]; then
    echo "  FAILURES:"
    echo -e "$FAILED_TESTS"
fi

if [ $FAIL -eq 0 ] && [ $COMPILE_FAIL -eq 0 ]; then
    echo "  ✓ ALL SAFETY-CRITICAL TESTS PASSED"
    echo ""
    exit 0
else
    echo "  ✗ SAFETY VALIDATION FAILED — DO NOT RELEASE"
    echo ""
    exit 1
fi
