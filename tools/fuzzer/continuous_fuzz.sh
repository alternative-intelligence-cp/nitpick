#!/bin/bash
# Continuous fuzzing - runs until stopped
# For long-term safety validation

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
ARIA_ROOT="$SCRIPT_DIR/../.."
cd "$ARIA_ROOT"

LOG_DIR="./fuzz_logs"
mkdir -p "$LOG_DIR"

ITERATION=0
START_TIME=$(date +%s)

echo "======================================================================"
echo "           CONTINUOUS FUZZING - CHILD SAFETY VALIDATION"
echo "======================================================================"
echo ""
echo "This fuzzer will run indefinitely, testing:"
echo "  • Random valid programs (grammar fuzzing)"
echo "  • Corrupted inputs (mutation fuzzing)"
echo "  • Edge cases (boundary fuzzing)"
echo ""
echo "Press Ctrl+C to stop"
echo ""
echo "Logs: $LOG_DIR/"
echo ""
echo "======================================================================"
echo ""

while true; do
    ITERATION=$((ITERATION + 1))
    CURRENT_TIME=$(date +%s)
    ELAPSED=$((CURRENT_TIME - START_TIME))
    HOURS=$((ELAPSED / 3600))
    MINUTES=$(((ELAPSED % 3600) / 60))
    
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    echo "Iteration: $ITERATION | Runtime: ${HOURS}h ${MINUTES}m | $(date)"
    echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    
    # Grammar fuzzing - 1000 random programs
    echo "🎲 Grammar fuzzing (1000 programs)..."
    python3 tools/fuzzer/grammar_fuzzer.py \
        --iterations 1000 \
        --ariac ./build/ariac \
        --save-failures \
        > "$LOG_DIR/grammar_${ITERATION}.log" 2>&1
    
    if [ $? -ne 0 ]; then
        echo "❌ GRAMMAR FUZZING FAILED - See $LOG_DIR/grammar_${ITERATION}.log"
        echo "CRITICAL: Parser cannot handle valid syntax - STOPPING"
        exit 1
    fi
    echo "   ✅ Passed"
    
    # Mutation fuzzing - 500 mutations per test
    echo "🧬 Mutation fuzzing (500 mutations/test)..."
    python3 tools/fuzzer/mutation_fuzzer.py \
        --test-dir ./tests \
        --iterations 500 \
        --ariac ./build/ariac \
        --save-crashes \
        > "$LOG_DIR/mutation_${ITERATION}.log" 2>&1
    
    if [ $? -ne 0 ]; then
        echo "❌ MUTATION FUZZING FAILED - See $LOG_DIR/mutation_${ITERATION}.log"
        echo "CRITICAL: Parser crashed on corrupted input - CHILD SAFETY AT RISK"
        exit 1
    fi
    echo "   ✅ Passed"
    
    # Boundary testing
    echo "📊 Boundary testing..."
    python3 tools/fuzzer/boundary_fuzzer.py \
        --ariac ./build/ariac \
        > "$LOG_DIR/boundary_${ITERATION}.log" 2>&1
    
    if [ $? -ne 0 ]; then
        echo "❌ BOUNDARY TESTING FAILED - See $LOG_DIR/boundary_${ITERATION}.log"
        echo "CRITICAL: Edge case handling broken - OVERFLOW/UNDERFLOW RISK"
        exit 1
    fi
    echo "   ✅ Passed"
    
    echo ""
    echo "✅ Iteration $ITERATION complete - Parser stable"
    echo ""
    
    # Brief pause before next iteration
    sleep 2
done
