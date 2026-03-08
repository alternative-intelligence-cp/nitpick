#!/usr/bin/env bash
# Build script for alife — Conway's Game of Life (Aria + C FFI)
set -e
cd "$(dirname "$0")"
echo "Building libalife.so..."
gcc -shared -fPIC -o libalife.so libalife.c
echo "Compiling alife.aria..."
../build/ariac alife.aria -L. -lalife -Wl,-rpath,'$ORIGIN' -o alife
echo "Done.  Run:  ./alife"
