#!/bin/bash
# AriaX Fuzzer Launcher with Screen
# Runs fuzzing in a persistent screen session

set -e

FUZZ_DIR="$HOME/._____RANDY_____/REPOS/aria/tests/fuzz"
SESSION_NAME="aria-fuzzer"
DURATION="${1:-72}"

echo "AriaX Fuzzer - Screen Launcher"
echo "==============================="
echo ""

# Check if session already exists
if screen -list | grep -q "$SESSION_NAME"; then
    echo "⚠️  Fuzzer session already running!"
    echo "   To view it: screen -r $SESSION_NAME"
    echo "   To kill it: screen -S $SESSION_NAME -X quit"
    exit 1
fi

# Start fuzzer in screen
echo "Starting fuzzer in screen session..."
echo "Duration: $DURATION"
echo ""

cd "$FUZZ_DIR"
screen -dmS "$SESSION_NAME" bash -c "./run_campaign.sh $DURATION; echo 'Fuzzer completed. Press ENTER to close.'; read"

echo "✅ Fuzzer started in background!"
echo ""
echo "Commands:"
echo "  View progress:  screen -r $SESSION_NAME"
echo "  Detach:         Ctrl+A then D"
echo "  Stop fuzzer:    screen -S $SESSION_NAME -X quit"
echo "  Check status:   screen -ls"
echo ""
echo "The fuzzer will survive:"
echo "  ✅ DE crashes/restarts"
echo "  ✅ Terminal closes"
echo "  ✅ Logout"
echo "  ❌ System reboot (will need restart)"
echo ""
