#!/bin/bash
# Build and test cross-language bindings example
set -e

ARIAC="../../build/ariac"
echo "=== Building Aria shared library ==="
$ARIAC mathlib.aria --shared -o libmathlib.so
echo "Built libmathlib.so"
nm -D libmathlib.so | grep " T "
echo

echo "=== Testing C bindings ==="
gcc use_from_c.c -L. -lmathlib -Wl,-rpath,. -o use_from_c
./use_from_c
echo

echo "=== Testing Python bindings ==="
python3 use_from_python.py
echo

echo "All binding tests passed!"
