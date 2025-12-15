/**
 * src/runtime/debug/stacktrace.c
 *
 * Aria Runtime - Stack Trace Implementation
 * Version: 0.0.7
 *
 * Implements stack unwinding using backtrace() and libdl for symbol resolution.
 * This is a portable implementation that works on Linux, macOS, and BSD.
 * 
 * For production use, consider libunwind for better performance and control.
 */

#define _GNU_SOURCE  // For backtrace(), dladdr()
#include "stacktrace.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <execinfo.h>
#include <dlfcn.h>
#include <unistd.h>
#include <time.h>

#ifdef __cplusplus
#include <cxxabi.h>
#endif

// ANSI color codes for pretty printing
#define COLOR_RESET   "\033[0m"
#define COLOR_RED     "\033[31m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_BLUE    "\033[34m"
#define COLOR_MAGENTA "\033[35m"
#define COLOR_CYAN    "\033[36m"
#define COLOR_BOLD    "\033[1m"

// Original signal handlers (for restoration)
static struct sigaction old_sigsegv;
static struct sigaction old_sigabrt;
static struct sigaction old_sigfpe;
static struct sigaction old_sigill;
static struct sigaction old_sigbus;

// Global trace for signal handlers (signal-unsafe to allocate)
static aria_stacktrace_t g_crash_trace;

/**
 * Demangle C++ symbol names
 */
static void demangle_symbol(const char* mangled, char* output, size_t output_size) {
#ifdef __cplusplus
    int status = 0;
    char* demangled = abi::__cxa_demangle(mangled, NULL, NULL, &status);
    
    if (status == 0 && demangled != NULL) {
        snprintf(output, output_size, "%s", demangled);
        free(demangled);
    } else {
        snprintf(output, output_size, "%s", mangled);
    }
#else
    snprintf(output, output_size, "%s", mangled);
#endif
}

/**
 * Capture current stack trace
 */
int aria_capture_stacktrace(aria_stacktrace_t* trace, int skip_frames) {
    if (trace == NULL) {
        return -1;
    }
    
    memset(trace, 0, sizeof(aria_stacktrace_t));
    
    // Capture raw stack addresses
    void* buffer[ARIA_MAX_STACK_FRAMES + skip_frames];
    int captured = backtrace(buffer, ARIA_MAX_STACK_FRAMES + skip_frames);
    
    if (captured <= skip_frames) {
        return 0;
    }
    
    // Skip requested frames
    void** frames_to_process = buffer + skip_frames;
    int frames_count = captured - skip_frames;
    
    trace->frame_count = frames_count;
    
    // Get symbol strings from backtrace_symbols for better resolution
    char** symbols = backtrace_symbols(frames_to_process, frames_count);
    
    // Resolve symbols for each frame
    for (int i = 0; i < frames_count && i < ARIA_MAX_STACK_FRAMES; i++) {
        aria_stack_frame_t* frame = &trace->frames[i];
        frame->address = frames_to_process[i];
        
        // First try backtrace_symbols output (more reliable for local symbols)
        if (symbols && symbols[i]) {
            // Parse format: "./binary(function+offset) [address]"
            // or: "./binary [address]"
            char* func_start = strchr(symbols[i], '(');
            char* func_end = strchr(symbols[i], '+');
            
            if (func_start && func_end && func_end > func_start) {
                // Extract function name
                size_t name_len = func_end - func_start - 1;
                if (name_len > 0 && name_len < sizeof(frame->function_name)) {
                    strncpy(frame->function_name, func_start + 1, name_len);
                    frame->function_name[name_len] = '\0';
                    
                    // Demangle if needed
                    char demangled[256];
                    demangle_symbol(frame->function_name, demangled, sizeof(demangled));
                    strncpy(frame->function_name, demangled, sizeof(frame->function_name) - 1);
                    
                    // Parse offset
                    char* offset_end = strchr(func_end, ')');
                    if (offset_end) {
                        frame->offset = strtoul(func_end + 1, NULL, 0);
                    }
                }
            } else {
                // No function name in backtrace_symbols, try dladdr
                Dl_info info;
                if (dladdr(frame->address, &info) && info.dli_sname != NULL) {
                    demangle_symbol(info.dli_sname, frame->function_name, sizeof(frame->function_name));
                    frame->offset = (uint64_t)frame->address - (uint64_t)info.dli_saddr;
                } else {
                    snprintf(frame->function_name, sizeof(frame->function_name), "<unknown>");
                }
            }
            
            // Extract binary name
            char* space = strchr(symbols[i], ' ');
            char* paren = strchr(symbols[i], '(');
            char* end = (paren && paren < space) ? paren : space;
            if (end) {
                size_t path_len = end - symbols[i];
                if (path_len < sizeof(frame->source_file)) {
                    strncpy(frame->source_file, symbols[i], path_len);
                    frame->source_file[path_len] = '\0';
                }
            }
        } else {
            // Fallback to dladdr only
            Dl_info info;
            if (dladdr(frame->address, &info)) {
                if (info.dli_sname != NULL) {
                    demangle_symbol(info.dli_sname, frame->function_name, sizeof(frame->function_name));
                    frame->offset = (uint64_t)frame->address - (uint64_t)info.dli_saddr;
                } else {
                    snprintf(frame->function_name, sizeof(frame->function_name), "<unknown>");
                    frame->offset = 0;
                }
                
                if (info.dli_fname != NULL) {
                    snprintf(frame->source_file, sizeof(frame->source_file), "%s", info.dli_fname);
                }
            } else {
                snprintf(frame->function_name, sizeof(frame->function_name), "<unknown>");
                snprintf(frame->source_file, sizeof(frame->source_file), "<unknown>");
            }
        }
        
        // Note: Line numbers require DWARF parsing - that's Phase 2
        frame->line_number = 0;
        frame->column_number = 0;
    }
    
    // Free symbols array
    if (symbols) {
        free(symbols);
    }
    
    return frames_count;
}

/**
 * Print stack trace to stderr
 */
void aria_print_stacktrace(const aria_stacktrace_t* trace, int use_color) {
    if (trace == NULL || trace->frame_count == 0) {
        fprintf(stderr, "No stack trace available.\n");
        return;
    }
    
    const char* reset = use_color ? COLOR_RESET : "";
    const char* bold = use_color ? COLOR_BOLD : "";
    const char* red = use_color ? COLOR_RED : "";
    const char* yellow = use_color ? COLOR_YELLOW : "";
    const char* cyan = use_color ? COLOR_CYAN : "";
    const char* green = use_color ? COLOR_GREEN : "";
    
    fprintf(stderr, "\n%s%s=== Stack Trace ===%s\n", bold, red, reset);
    
    if (trace->signal_number != 0) {
        fprintf(stderr, "%sSignal: %s (%d)%s\n\n", 
                red, trace->signal_name, trace->signal_number, reset);
    }
    
    for (size_t i = 0; i < trace->frame_count; i++) {
        const aria_stack_frame_t* frame = &trace->frames[i];
        
        fprintf(stderr, "%s#%-2zu%s ", yellow, i, reset);
        fprintf(stderr, "%s0x%016lx%s in ", cyan, (unsigned long)frame->address, reset);
        fprintf(stderr, "%s%s%s", bold, frame->function_name, reset);
        
        if (frame->offset > 0) {
            fprintf(stderr, "+%s0x%lx%s", green, (unsigned long)frame->offset, reset);
        }
        
        if (frame->line_number > 0) {
            fprintf(stderr, "\n    at %s:%s%u%s", 
                    frame->source_file, green, frame->line_number, reset);
            if (frame->column_number > 0) {
                fprintf(stderr, ":%u", frame->column_number);
            }
        } else if (strcmp(frame->source_file, "<unknown>") != 0) {
            fprintf(stderr, "\n    from %s", frame->source_file);
        }
        
        fprintf(stderr, "\n");
    }
    
    fprintf(stderr, "\n");
}

/**
 * Save stack trace to file
 */
int aria_save_stacktrace(const aria_stacktrace_t* trace, const char* filename) {
    if (trace == NULL || filename == NULL) {
        return -1;
    }
    
    FILE* fp = fopen(filename, "w");
    if (fp == NULL) {
        return -1;
    }
    
    fprintf(fp, "=== Aria Runtime Crash Report ===\n");
    
    // Timestamp
    time_t now = time(NULL);
    fprintf(fp, "Time: %s", ctime(&now));
    
    if (trace->signal_number != 0) {
        fprintf(fp, "Signal: %s (%d)\n", trace->signal_name, trace->signal_number);
    }
    
    fprintf(fp, "\nStack Trace (%zu frames):\n\n", trace->frame_count);
    
    for (size_t i = 0; i < trace->frame_count; i++) {
        const aria_stack_frame_t* frame = &trace->frames[i];
        
        fprintf(fp, "#%-2zu 0x%016lx in %s", 
                i, (unsigned long)frame->address, frame->function_name);
        
        if (frame->offset > 0) {
            fprintf(fp, "+0x%lx", (unsigned long)frame->offset);
        }
        
        if (frame->line_number > 0) {
            fprintf(fp, "\n    at %s:%u", frame->source_file, frame->line_number);
            if (frame->column_number > 0) {
                fprintf(fp, ":%u", frame->column_number);
            }
        } else if (strcmp(frame->source_file, "<unknown>") != 0) {
            fprintf(fp, "\n    from %s", frame->source_file);
        }
        
        fprintf(fp, "\n");
    }
    
    fclose(fp);
    return 0;
}

/**
 * Get signal name
 */
const char* aria_signal_name(int signum) {
    switch (signum) {
        case SIGSEGV: return "SIGSEGV (Segmentation fault)";
        case SIGABRT: return "SIGABRT (Abort)";
        case SIGFPE:  return "SIGFPE (Floating point exception)";
        case SIGILL:  return "SIGILL (Illegal instruction)";
        case SIGBUS:  return "SIGBUS (Bus error)";
        default:      return "UNKNOWN";
    }
}

/**
 * Signal handler for crashes
 * NOTE: This must be signal-safe (no malloc, printf to stderr only)
 */
static void crash_signal_handler(int sig, siginfo_t* info, void* context) {
    // Populate trace info
    g_crash_trace.signal_number = sig;
    g_crash_trace.signal_name = aria_signal_name(sig);
    
    // Capture stack (skip 2 frames: this handler and signal trampoline)
    aria_capture_stacktrace(&g_crash_trace, 2);
    
    // Print to stderr
    aria_print_stacktrace(&g_crash_trace, isatty(STDERR_FILENO));
    
    // Try to save crash log
    char filename[256];
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    snprintf(filename, sizeof(filename), 
             "aria_crash_%04d%02d%02d_%02d%02d%02d.log",
             tm_info->tm_year + 1900, tm_info->tm_mon + 1, tm_info->tm_mday,
             tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec);
    
    if (aria_save_stacktrace(&g_crash_trace, filename) == 0) {
        fprintf(stderr, "\nCrash report saved to: %s\n", filename);
    }
    
    // Re-raise signal with default handler to generate core dump
    signal(sig, SIG_DFL);
    raise(sig);
}

/**
 * Install crash handlers
 */
void aria_install_crash_handlers(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = crash_signal_handler;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    
    sigaction(SIGSEGV, &sa, &old_sigsegv);
    sigaction(SIGABRT, &sa, &old_sigabrt);
    sigaction(SIGFPE, &sa, &old_sigfpe);
    sigaction(SIGILL, &sa, &old_sigill);
    sigaction(SIGBUS, &sa, &old_sigbus);
}

/**
 * Uninstall crash handlers
 */
void aria_uninstall_crash_handlers(void) {
    sigaction(SIGSEGV, &old_sigsegv, NULL);
    sigaction(SIGABRT, &old_sigabrt, NULL);
    sigaction(SIGFPE, &old_sigfpe, NULL);
    sigaction(SIGILL, &old_sigill, NULL);
    sigaction(SIGBUS, &old_sigbus, NULL);
}

/**
 * Check if debug symbols are available
 */
int aria_has_debug_symbols(void) {
    // Simple heuristic: capture a trace and see if we got symbols
    aria_stacktrace_t test_trace;
    if (aria_capture_stacktrace(&test_trace, 0) > 0) {
        // Check if any frame has non-unknown function name
        for (size_t i = 0; i < test_trace.frame_count; i++) {
            if (strcmp(test_trace.frames[i].function_name, "<unknown>") != 0) {
                return 1;
            }
        }
    }
    return 0;
}

/**
 * Auto-install crash handlers at program startup
 * This uses GCC/Clang constructor attribute
 */
__attribute__((constructor))
static void auto_install_handlers(void) {
    // Only auto-install in debug builds
#ifdef ARIA_DEBUG
    aria_install_crash_handlers();
#endif
}
