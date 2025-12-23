#!/bin/bash
# Build and run standalone async runtime test

set -e

echo "=== Building Async Runtime Test ==="

cd /home/randy/._____RANDY_____/REPOS/aria

# Compile the runtime libraries
g++ -std=c++17 -fPIC -c -I include -I /usr/lib/llvm-20/include \
    src/runtime/async/executor.cpp -o /tmp/executor.o

g++ -std=c++17 -fPIC -c -I include -I /usr/lib/llvm-20/include \
    src/runtime/async/coroutine.cpp -o /tmp/coroutine.o

g++ -std=c++17 -fPIC -c -I include -I /usr/lib/llvm-20/include \
    src/runtime/async/runtime_api.cpp -o /tmp/runtime_api.o

# Compile the test helpers
g++ -std=c++17 -fPIC -c -I include tests/test_helpers.cpp -o /tmp/test_helpers.o

# Compile the test
g++ -std=c++17 -fPIC -c -I include -I /usr/lib/llvm-20/include \
    tests/runtime/test_async_runtime_api.cpp -o /tmp/test_async_api_test.o

# Link everything
g++ -std=c++17 \
    /tmp/executor.o \
    /tmp/coroutine.o \
    /tmp/runtime_api.o \
    /tmp/test_helpers.o \
    /tmp/test_async_api_test.o \
    -o /tmp/test_async_runtime

echo ""
echo "=== Running Async Runtime Tests ==="
echo ""

/tmp/test_async_runtime

echo ""
echo "=== Test Complete ==="
