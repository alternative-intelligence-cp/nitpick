/**
 * tests/test_crash_handler.c
 *
 * Test crash handler - intentionally triggers a segfault
 */

#include "../src/runtime/debug/stacktrace.h"
#include <stdio.h>

void cause_crash() {
    // Dereference null pointer
    int* null_ptr = NULL;
    printf("About to crash...\n");
    *null_ptr = 42;  // SEGFAULT HERE
}

void intermediate_function() {
    cause_crash();
}

int main() {
    printf("=== Testing Crash Handler ===\n");
    printf("Installing crash handlers...\n");
    aria_install_crash_handlers();
    
    printf("Triggering intentional crash...\n\n");
    intermediate_function();
    
    // Should never reach here
    printf("ERROR: Did not crash!\n");
    return 1;
}
