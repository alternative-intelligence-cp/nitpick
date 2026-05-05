#include "runtime/streams.h"
#include <stdio.h>
#include <unistd.h>

int main() {
    // Initialize six-stream I/O
    npk_streams_init();
    
    // Test 1: Write to each stream
    printf("=== Six-Stream I/O Test ===\n");
    
    npk_stdout_write("1. This is stdout (FD 1)\n");
    npk_stderr_write("2. This is stderr (FD 2)\n");
    npk_stddbg_write("3. This is stddbg (FD 3)\n");
    
    // Test 2: Verify FD numbers
    printf("\n=== FD Verification ===\n");
    
    // We can check which FDs are open
    printf("FD 0 (stdin):  %s\n", isatty(0) ? "TTY" : "Redirected");
    printf("FD 1 (stdout): %s\n", isatty(1) ? "TTY" : "Redirected");
    printf("FD 2 (stderr): %s\n", isatty(2) ? "TTY" : "Redirected");
    printf("FD 3 (stddbg): %s\n", isatty(3) ? "TTY" : "Redirected");
    printf("FD 4 (stddati): %s\n", isatty(4) ? "TTY" : "Redirected");
    printf("FD 5 (stddato): %s\n", isatty(5) ? "TTY" : "Redirected");
    
    // Test 3: Write debug messages
    printf("\n=== Debug Output Test ===\n");
    npk_stddbg_printf("[DEBUG] Integer: %d\n", 42);
    npk_stddbg_printf("[DEBUG] String: %s\n", "Hello from FD 3!");
    npk_stddbg_printf("[DEBUG] Float: %.2f\n", 3.14159);
    
    // Test 4: Binary streams (write test data)
    printf("\n=== Binary Stream Test ===\n");
    const char test_data[] = "BINARY_DATA_TEST";
    int64_t written = npk_stddato_write(test_data, sizeof(test_data));
    printf("Wrote %ld bytes to stddato (FD 5)\n", written);
    
    // Cleanup
    npk_streams_cleanup();
    
    printf("\n=== Test Complete ===\n");
    return 0;
}
