#!/usr/bin/env bash
# build_ttt_ffi.sh — builds ttt_ffi (Aria + C FFI + Python curses display)
#
# Usage:
#   cd examples/
#   bash build_ttt_ffi.sh
#   ./ttt_ffi
#
# Requires:
#   - ariac   (built at ../build/ariac)
#   - gcc
#   - python3 with curses (built-in on Linux)

set -e
cd "$(dirname "$0")"
ARIAC=${ARIAC:-"../build/ariac"}

echo "── Compiling C FFI shim ──────────────────────────────────"
gcc -c -o ttt_display.o ttt_display.c
echo "   ttt_display.o  OK"

echo ""
echo "── Compiling + linking Aria FFI program ──────────────────"
"$ARIAC" ttt_ffi.aria -Wl,ttt_display.o -lstdc++ -o ttt_ffi
echo "   ttt_ffi        OK"

echo ""
echo "Build complete.  Run:  ./ttt_ffi"
echo ""
echo "The binary:"
echo "  1. Plays the game and prints moves to the terminal"
echo "  2. Writes /tmp/aria_ttt.dat (move history)"
echo "  3. Launches ttt_display.py (Python curses animated replay)"
echo "     Press any key in the curses window to exit."
