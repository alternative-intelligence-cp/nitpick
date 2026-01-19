#!/bin/bash
# Test command execution in ariash

cd "$(dirname "$0")/build"

echo "Testing command execution in ariash..."
echo ""

# Test 1: Simple echo
echo "=== Test 1: echo command ==="
echo 'echo "Hello from ariash!"' | timeout 2 ./ariash 2>&1 | tail -20

echo ""
echo "=== Test 2: ls command ==="
echo 'ls -la' | timeout 2 ./ariash 2>&1 | tail -20

echo ""
echo "=== Test 3: pwd command ==="
echo 'pwd' | timeout 2 ./ariash 2>&1 | tail -20

echo ""
echo "=== Test 4: date command ==="
echo 'date' | timeout 2 ./ariash 2>&1 | tail -20

echo ""
echo "All tests complete!"
