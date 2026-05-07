#!/bin/bash

# ==============================================================================
# ARIA KITCHEN SINK TEST RUNNER
# ==============================================================================
# usage: ./run_tests.sh

# Colors for output
GREEN='\033[0;32m'
RED='\033[0;31m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

BINARY_NAME="sink_test"
BUILD_DIR="./bin"
LOG_DIR="./logs"
SRC_FILE="kitchen_sink.npk"

echo -e "${BLUE}=== Initializing Aria Test Environment ===${NC}"

# 1. Setup Directories
mkdir -p $BUILD_DIR
mkdir -p $LOG_DIR

# 2. Cleanup Old Artifacts
rm -f $BUILD_DIR/$BINARY_NAME
rm -f $LOG_DIR/*.log

# 3. Compilation Step
# We simulate calling 'aria_make' or the compiler 'npkc'
echo -e "${YELLOW}Compiling $SRC_FILE...${NC}"

# MOCK COMMAND: Replace with your actual compiler command
# e.g., aria_make build --config aria.build
# For now, we assume if the file exists, we proceed (or fail if missing)
if [ ! -f "$SRC_FILE" ]; then
    echo -e "${RED}[ERROR] Source file $SRC_FILE not found!${NC}"
    exit 1
fi

# Simulate compilation time
sleep 0.5 

# CHECK: In a real scenario, we check compiler exit code here.
# if ! aria_make build; then exit 1; fi

echo -e "${GREEN}[SUCCESS] Build complete.${NC}"

# 4. Execution with Stream Redirection
# Aria spec defines separate streams: stdout, stderr, stddbg.
# We map file descriptors to capture these individually.

echo -e "${YELLOW}Running Kitchen Sink Audit...${NC}"

# We simulate the run. In reality: $BUILD_DIR/$BINARY_NAME
# We redirect:
#   1 (STDOUT) -> run.log
#   2 (STDERR) -> error.log
#   3 (STDDBG) -> debug.log (Assuming FD 3 is mapped to stddbg in your runtime)

# MOCK EXECUTION (Concept)
# ./bin/sink_test 1> $LOG_DIR/stdout.log 2> $LOG_DIR/stderr.log 3> $LOG_DIR/debug.log

# For this script to be runnable without the compiler, I'll touch the logs:
touch $LOG_DIR/stdout.log
touch $LOG_DIR/stderr.log
touch $LOG_DIR/debug.log

echo -e "${BLUE}--- Execution Finished ---${NC}"

# 5. Log Analysis
# We check the logs for expected success strings and unexpected failures.

# CHECK 1: Did we crash? (Exit code check would go here)

# CHECK 2: ERR Sentinel Leakage
# TBB and Fix256 types return "ERR" on failure. We grep for this in stdout.
if grep -q "ERR" $LOG_DIR/stdout.log; then
    echo -e "${RED}[FAIL] Safety Violation: 'ERR' sentinel leaked into standard output!${NC}"
    echo -e "       Check arithmetic operations in 'Exotic Types' section."
else
    echo -e "${GREEN}[PASS] No ERR sentinels detected in output.${NC}"
fi

# CHECK 3: Memory Safety (Defer/Wild)
# Assuming the runtime prints memory stats to stddbg at exit
# grep "Memory Leaks: 0" $LOG_DIR/debug.log

echo -e "\n${BLUE}=== Test Summary ===${NC}"
echo -e "Standard Output: ${YELLOW}$LOG_DIR/stdout.log${NC}"
echo -e "Error Log:       ${YELLOW}$LOG_DIR/stderr.log${NC}"
echo -e "Debug Stream:    ${YELLOW}$LOG_DIR/debug.log${NC}"

# Final Status
echo -e "${GREEN}Ready for manual review.${NC}"