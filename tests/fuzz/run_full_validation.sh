#!/bin/bash
# Master Fuzzing Campaign for Aria v0.1.0
# Runs all validation tests before release

set -e

cd "$(dirname "$0")/../.."
ARIA_ROOT="$(pwd)"
cd "$ARIA_ROOT"

echo "========================================"
echo "Aria v0.1.0 Fuzzing Campaign"
echo "========================================"
echo "Date: $(date)"
echo "Compiler: $(./build/ariac --version 2>&1 | head -1 || echo 'unknown')"
echo "Hardware: $(nproc) CPU cores, $(free -h | awk '/^Mem:/ {print $2}') RAM"
echo ""

# Check compiler is built
if [ ! -f "./build/ariac" ]; then
    echo "ERROR: Compiler not built. Run ./build.sh first."
    exit 1
fi

echo "========================================"
echo "PHASE 1: Ecosystem Audit (TBB Widening)"
echo "========================================"
echo ""

cd "$ARIA_ROOT/.."
echo "Searching for implicit TBB widening in ecosystem..."
TBB_USAGE=$(grep -r "tbb8.*=\|tbb16.*=\|tbb32.*=\|tbb64.*=" \
    aria_make/src aria_shell/src aria_utils/src \
    --include="*.aria" 2>/dev/null | grep -v "Binary" || true)

if [ -z "$TBB_USAGE" ]; then
    echo "✅ No TBB usage found in ecosystem source code"
    echo "   (Only design docs contain TBB examples - this is OK)"
else
    echo "⚠️  Found TBB usage:"
    echo "$TBB_USAGE"
    echo ""
    echo "MANUAL ACTION REQUIRED:"
    echo "Check if any of the above need tbb_widen<T>() conversion"
    read -p "Continue anyway? (y/N) " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        exit 1
    fi
fi

cd "$ARIA_ROOT"
echo ""

echo "========================================"
echo "PHASE 2: LBIM Multiplication (1M tests)"
echo "========================================"
echo ""

echo "Starting 1,000,000 multiplication fuzz tests..."
echo "This will use all available CPU cores (~2 min on 48-thread system)"
echo ""

if ! python3 tests/fuzz/test_lbim_multiplication.py; then
    echo ""
    echo "❌ LBIM multiplication fuzz test FAILED"
    echo "   See errors above. Fix before continuing."
    exit 1
fi

echo ""
echo "✅ LBIM multiplication validated (1M tests passed)"
echo ""

echo "========================================"
echo "PHASE 3: Quick Compiler Fuzz (1 hour)"
echo "========================================"
echo ""

echo "Running 1-hour compiler fuzzing campaign..."
echo "This is a quick validation before the full 72-hour run."
echo ""

cd tests/fuzz
./run_campaign.sh standard 1 || {
    echo ""
    echo "❌ Quick fuzz campaign found crashes"
    echo "   Review crashes/ directory"
    echo "   Fix issues before running 72-hour campaign"
    exit 1
}

cd "$ARIA_ROOT"
echo ""
echo "✅ 1-hour quick fuzz complete (no crashes)"
echo ""

echo "========================================"
echo "READY FOR 72-HOUR FINAL FUZZING"
echo "========================================"
echo ""
echo "Quick validation complete. You can now run:"
echo ""
echo "  cd tests/fuzz"
echo "  ./run_campaign.sh standard 72"
echo ""
echo "This will run for 72 hours (3 days) as required by Phase 4.2."
echo "Recommended: Run in tmux/screen session"
echo ""
read -p "Start 72-hour campaign now? (y/N) " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    echo ""
    echo "Starting 72-hour fuzzing campaign..."
    echo "Monitor with: tail -f tests/fuzz/campaign_stats.json"
    echo ""
    cd tests/fuzz
    ./run_campaign.sh standard 72
    
    echo ""
    echo "========================================"
    echo "72-HOUR CAMPAIGN COMPLETE"
    echo "========================================"
    echo ""
    
    CRASH_COUNT=$(find crashes -name "*.aria" 2>/dev/null | wc -l)
    if [ "$CRASH_COUNT" -gt 0 ]; then
        echo "❌ Found $CRASH_COUNT crashes"
        echo "   These must be fixed before v0.1.0 release"
        exit 1
    else
        echo "✅ No crashes found!"
        echo ""
        echo "All fuzzing requirements for v0.1.0 complete:"
        echo "  ✅ Ecosystem audit (no implicit TBB widening)"
        echo "  ✅ 1M multiplication tests (carry propagation validated)"
        echo "  ✅ 72-hour compiler fuzzing (no crashes)"
        echo ""
        echo "Next phase: Documentation (Phase 5)"
    fi
else
    echo ""
    echo "72-hour campaign deferred."
    echo "Run manually when ready:"
    echo "  cd $ARIA_ROOT/tests/fuzz && ./run_campaign.sh standard 72"
fi

echo ""
echo "========================================"
echo "Fuzzing campaign script complete"
echo "========================================"
