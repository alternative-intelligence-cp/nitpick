#!/bin/bash
# Quick status check for background tasks
# Safe to run anytime - doesn't modify anything

echo "=========================================="
echo "Background Task Status - $(date '+%Y-%m-%d %H:%M:%S')"
echo "=========================================="

echo ""
echo "🧪 FUZZER STATUS:"
if ps aux | grep "aria_fuzzer_v2.py" | grep -v grep > /dev/null; then
    echo "  ✅ Running (PID: $(pgrep -f aria_fuzzer_v2.py))"
    cd /home/randy/Workspace/REPOS/aria/tests/fuzz/v2
    echo "  $(tail -7 fuzzer_24h.log | grep 'Duration:' | tail -1)"
    echo "  $(tail -7 fuzzer_24h.log | grep 'Total programs:' | tail -1)"
    echo "  $(tail -7 fuzzer_24h.log | grep 'Crashes:' | tail -1)"
    echo ""
    echo "  ⚠️  DO NOT rebuild ariac until fuzzer completes!"
else
    echo "  ❌ Not running"
fi

echo ""
echo "📚 SEMANTIC ARCHIVE STATUS:"
if ps aux | grep "node index.js watch" | grep -v grep > /dev/null; then
    echo "  ✅ Running (PID: $(pgrep -f 'node index.js watch'))"
    echo "  Last 3 lines:"
    tail -3 /tmp/semantic-archive.log | sed 's/^/    /'
else
    echo "  ❌ Not running"
fi

echo ""
echo "=========================================="
