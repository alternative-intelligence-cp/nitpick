/**
 * tests/test_stacktrace.c
 *
 * Test program for stack trace functionality
 */

#include "../src/runtime/debug/stacktrace.h"
#include <stdio.h>
#include <stdlib.h>

// Function that captures a stack trace
void capture_and_print() {
    aria_stacktrace_t trace;
    int frames = aria_capture_stacktrace(&trace, 0);
    
    printf("Captured %d stack frames:\n", frames);
    aria_print_stacktrace(&trace, 1);  // Use colors
}

// Nested function calls to create a stack
void level3() {
    capture_and_print();
}

void level2() {
    level3();
}

void level1() {
    level2();
}

int main() {
    printf("=== Testing Manual Stack Trace Capture ===\n\n");
    
    // Check if debug symbols are available
    if (aria_has_debug_symbols()) {
        printf("Debug symbols: Available\n");
    } else {
        printf("Debug symbols: Not available (compile with -g for better traces)\n");
    }
    
    printf("\nCalling nested functions...\n\n");
    level1();
    
    printf("\n=== Test Complete ===\n");
    return 0;
}
