// Aria Runtime Panic Handler
// Provides controlled shutdown on fatal errors instead of instant abort()
// Critical for life-safety systems where defer blocks must execute
//
// SIL-4 Requirements:
// - Execute defer blocks (NOTE: requires defer infrastructure - stubbed for now)
// - Call hardware safety callback (engage brakes, unlock actuators)
// - Task isolation (kill task not process for async)
// - Full context logging before shutdown

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <atomic>

namespace aria {
namespace panic {

// Hardware safety callback type
// Called before process termination to engage brakes, unlock actuators, etc.
// Must be async-signal-safe (no malloc, no locks beyond atomic ops)
typedef void (*HardwareSafetyCallback)(const char* panic_reason);

static std::atomic<HardwareSafetyCallback> g_hardware_safety_callback{nullptr};
static std::atomic<bool> g_panic_in_progress{false};

// Register hardware safety callback
// This should be called at application startup to register robot safety logic
extern "C" void aria_runtime_register_safety_callback(HardwareSafetyCallback callback) {
    g_hardware_safety_callback.store(callback, std::memory_order_release);
}

// Log panic with timestamp and context
static void log_panic(const char* reason, const char* location) {
    // Get timestamp
    time_t now = time(nullptr);
    char time_buf[64];
    strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", localtime(&now));
    
    // Write to stderr (async-signal-safe)
    const char* header = "\n=== ARIA RUNTIME PANIC ===\n";
    const char* time_prefix = "Time: ";
    const char* reason_prefix = "Reason: ";
    const char* location_prefix = "Location: ";
    const char* footer = "==========================\n\n";
    
    write(STDERR_FILENO, header, strlen(header));
    write(STDERR_FILENO, time_prefix, strlen(time_prefix));
    write(STDERR_FILENO, time_buf, strlen(time_buf));
    write(STDERR_FILENO, "\n", 1);
    
    if (reason) {
        write(STDERR_FILENO, reason_prefix, strlen(reason_prefix));
        write(STDERR_FILENO, reason, strlen(reason));
        write(STDERR_FILENO, "\n", 1);
    }
    
    if (location) {
        write(STDERR_FILENO, location_prefix, strlen(location_prefix));
        write(STDERR_FILENO, location, strlen(location));
        write(STDERR_FILENO, "\n", 1);
    }
    
    write(STDERR_FILENO, footer, strlen(footer));
}

// Execute defer blocks for current thread
// NOTE: This is a stub - requires defer infrastructure to be implemented
// The defer mechanism needs to maintain a thread-local stack of cleanup functions
static void execute_defer_blocks() {
    // TODO: Walk thread-local defer stack and execute all registered cleanup functions
    // For now, this is a placeholder to document the requirement
    //
    // Defer infrastructure should:
    // 1. Maintain per-thread stack of cleanup functions
    // 2. Execute in LIFO order (last registered, first executed)
    // 3. Handle nested panics (if defer itself panics, abort immediately)
    // 4. Use async-signal-safe primitives only
    
    // Placeholder: Log that defer execution would happen here
    const char* msg = "[DEFER STUB] Would execute defer blocks here\n";
    write(STDERR_FILENO, msg, strlen(msg));
}

// Main panic handler
// Called when unwrap() fails, assertions trigger, or other fatal errors occur
extern "C" void aria_runtime_panic_unwrap(const char* reason) {
    // Prevent recursive panics
    bool expected = false;
    if (!g_panic_in_progress.compare_exchange_strong(expected, true)) {
        // Already panicking - abort immediately to prevent infinite recursion
        const char* msg = "RECURSIVE PANIC DETECTED - ABORTING\n";
        write(STDERR_FILENO, msg, strlen(msg));
        _exit(99);  // Use _exit to skip atexit handlers
    }
    
    // Log the panic with full context
    log_panic(reason, "unwrap()");
    
    // Execute defer blocks in current thread
    // This allows unlock_actuators(), file closes, etc. to run
    execute_defer_blocks();
    
    // Call hardware safety callback if registered
    // This should engage brakes, power down motors, unlock grippers, etc.
    HardwareSafetyCallback safety_cb = g_hardware_safety_callback.load(std::memory_order_acquire);
    if (safety_cb) {
        const char* msg = "[SAFETY] Calling hardware safety callback\n";
        write(STDERR_FILENO, msg, strlen(msg));
        safety_cb(reason);
    } else {
        const char* msg = "[WARNING] No hardware safety callback registered\n";
        write(STDERR_FILENO, msg, strlen(msg));
    }
    
    // TODO: For async tasks, implement task isolation
    // - Determine if we're in an async task context
    // - If yes: kill only the faulting task, not entire process
    // - If no: proceed with process shutdown
    //
    // For now, we always shut down the entire process
    
    // Flush stderr before exit
    fsync(STDERR_FILENO);
    
    // Exit with non-zero status
    // Use _exit instead of exit() to skip atexit handlers that might panic
    _exit(1);
}

// Generic panic handler for other panic types
extern "C" void aria_runtime_panic(const char* reason) {
    // Same as unwrap panic, but with different location
    bool expected = false;
    if (!g_panic_in_progress.compare_exchange_strong(expected, true)) {
        const char* msg = "RECURSIVE PANIC DETECTED - ABORTING\n";
        write(STDERR_FILENO, msg, strlen(msg));
        _exit(99);
    }
    
    log_panic(reason, "generic panic");
    execute_defer_blocks();
    
    HardwareSafetyCallback safety_cb = g_hardware_safety_callback.load(std::memory_order_acquire);
    if (safety_cb) {
        safety_cb(reason);
    }
    
    fsync(STDERR_FILENO);
    _exit(1);
}

// Assertion failure handler
extern "C" void aria_runtime_assert_failed(const char* expr, const char* file, int line) {
    char location[512];
    snprintf(location, sizeof(location), "%s:%d", file, line);
    
    char reason[512];
    snprintf(reason, sizeof(reason), "Assertion failed: %s", expr);
    
    bool expected = false;
    if (!g_panic_in_progress.compare_exchange_strong(expected, true)) {
        const char* msg = "RECURSIVE PANIC DETECTED - ABORTING\n";
        write(STDERR_FILENO, msg, strlen(msg));
        _exit(99);
    }
    
    log_panic(reason, location);
    execute_defer_blocks();
    
    HardwareSafetyCallback safety_cb = g_hardware_safety_callback.load(std::memory_order_acquire);
    if (safety_cb) {
        safety_cb(reason);
    }
    
    fsync(STDERR_FILENO);
    _exit(1);
}

} // namespace panic
} // namespace aria
