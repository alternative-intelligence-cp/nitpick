#!/bin/bash
# Phase 4.2: Adversarial Fuzzing Campaign Runner
#
# Usage:
#   ./run_campaign.sh [duration_hours]
#
# Examples:
#   ./run_campaign.sh          # 72 hours (default)
#   ./run_campaign.sh 24       # 24 hours
#   ./run_campaign.sh 0.1      # ~6 minutes (for testing)

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ARIA_ROOT="$SCRIPT_DIR/../.."
BUILD_DIR="$ARIA_ROOT/build"

# Check compiler exists
if [ ! -f "$BUILD_DIR/ariac" ]; then
    echo "ERROR: Compiler not found at $BUILD_DIR/ariac"
    echo "Please build first: cd $ARIA_ROOT && mkdir -p build && cd build && cmake .. && make"
    exit 1
fi

# Default duration
DURATION="${1:-72}"

echo "=============================================="
echo "PHASE 4.2: ADVERSARIAL FUZZING CAMPAIGN"
echo "=============================================="
echo "Duration: ${DURATION} hours"
echo "Start time: $(date)"
echo "Compiler: $BUILD_DIR/ariac"
echo "Corpus: $SCRIPT_DIR/corpus/"
echo "Crashes: $SCRIPT_DIR/crashes/"
echo "----------------------------------------------"

# Create log file
LOG_FILE="$SCRIPT_DIR/campaign_$(date +%Y%m%d_%H%M%S).log"
echo "Logging to: $LOG_FILE"
echo ""

# Run fuzzer
python3 "$SCRIPT_DIR/aria_fuzzer.py" \
    --duration "$DURATION" \
    --verbose \
    2>&1 | tee "$LOG_FILE"

echo ""
echo "Campaign complete at $(date)"
echo "Check $SCRIPT_DIR/crashes/ for any crashes found"
