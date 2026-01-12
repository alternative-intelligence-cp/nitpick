#!/bin/bash
# Quick check on fuzzing campaign status

cd "$(dirname "$0")"

echo "======================================"
echo "FUZZING CAMPAIGN STATUS"
echo "======================================"
echo "Time: $(date)"
echo ""

# Check if running
if pgrep -f "aria_fuzzer.py.*72" > /dev/null; then
    echo "Status: ✅ RUNNING"
    PID=$(pgrep -f "aria_fuzzer.py.*72")
    echo "PID: $PID"
    
    # CPU and memory usage
    ps -p $PID -o %cpu,%mem,etime,cmd --no-headers
    echo ""
else
    echo "Status: ❌ NOT RUNNING"
    echo ""
fi

# Latest log file
LATEST_LOG=$(ls -t campaign_72hr*.log 2>/dev/null | head -1)
if [ -n "$LATEST_LOG" ]; then
    echo "Latest log: $LATEST_LOG"
    echo "Log size: $(wc -l < "$LATEST_LOG") lines"
    echo ""
    echo "Last 15 lines:"
    echo "--------------------------------------"
    tail -15 "$LATEST_LOG"
fi

# Check for new crashes
NEW_CRASHES=$(find crashes/ -name "*.aria" -mmin -60 2>/dev/null | wc -l)
if [ $NEW_CRASHES -gt 0 ]; then
    echo ""
    echo "⚠️  NEW CRASHES FOUND: $NEW_CRASHES in last hour"
    find crashes/ -name "*.aria" -mmin -60 2>/dev/null
fi

echo ""
echo "======================================"
