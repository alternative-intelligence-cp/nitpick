/**
 * src/runtime/debug/stacktrace.h
 *
 * Aria Runtime - Stack Trace Utilities
 * Version: 0.0.7
 *
 * Provides stack unwinding and crash handling functionality for debugging.
 * Captures and prints stack traces when runtime errors occur.
 */

#ifndef ARIA_RUNTIME_DEBUG_STACKTRACE_H
#define ARIA_RUNTIME_DEBUG_STACKTRACE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>

/**
 * Maximum number of stack frames to capture
 */
#define ARIA_MAX_STACK_FRAMES 128

/**
 * Stack frame information
 */
typedef struct aria_stack_frame {
    void* address;              // Instruction pointer
    char function_name[256];    // Demangled function name
    char source_file[512];      // Source file path
    uint32_t line_number;       // Line number in source
    uint32_t column_number;     // Column number in source
    uint64_t offset;            // Offset from function start
} aria_stack_frame_t;

/**
 * Stack trace structure
 */
typedef struct aria_stacktrace {
    aria_stack_frame_t frames[ARIA_MAX_STACK_FRAMES];
    size_t frame_count;
    int signal_number;          // Signal that triggered capture (0 if manual)
    const char* signal_name;    // Human-readable signal name
} aria_stacktrace_t;

/**
 * Capture current stack trace
 * 
 * @param trace Output structure to fill with stack information
 * @param skip_frames Number of top frames to skip (e.g., capture function itself)
 * @return Number of frames captured, or -1 on error
 */
int aria_capture_stacktrace(aria_stacktrace_t* trace, int skip_frames);

/**
 * Print stack trace to stderr
 * 
 * @param trace Stack trace to print
 * @param use_color Whether to use ANSI color codes
 */
void aria_print_stacktrace(const aria_stacktrace_t* trace, int use_color);

/**
 * Print stack trace to file
 * 
 * @param trace Stack trace to print
 * @param filename Output file path
 * @return 0 on success, -1 on error
 */
int aria_save_stacktrace(const aria_stacktrace_t* trace, const char* filename);

/**
 * Install crash signal handlers
 * Automatically captures and prints stack traces on:
 * - SIGSEGV (segmentation fault)
 * - SIGABRT (abort)
 * - SIGFPE (floating point exception)
 * - SIGILL (illegal instruction)
 * - SIGBUS (bus error)
 */
void aria_install_crash_handlers(void);

/**
 * Uninstall crash signal handlers
 */
void aria_uninstall_crash_handlers(void);

/**
 * Get human-readable name for signal number
 * 
 * @param signum Signal number
 * @return Signal name string (e.g., "SIGSEGV")
 */
const char* aria_signal_name(int signum);

/**
 * Check if debug symbols are available
 * 
 * @return 1 if debug symbols found, 0 otherwise
 */
int aria_has_debug_symbols(void);

#ifdef __cplusplus
}
#endif

#endif // ARIA_RUNTIME_DEBUG_STACKTRACE_H
