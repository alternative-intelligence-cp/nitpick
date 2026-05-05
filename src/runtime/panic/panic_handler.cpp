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
#include <setjmp.h>
#include "runtime/async/future.h"

namespace npk {
namespace panic {

// Hardware safety callback type
// Called before process termination to engage brakes, unlock actuators, etc.
// Must be async-signal-safe (no malloc, no locks beyond atomic ops)
typedef void (*HardwareSafetyCallback)(const char* panic_reason);

static std::atomic<HardwareSafetyCallback> g_hardware_safety_callback{nullptr};
static std::atomic<bool> g_panic_in_progress{false};

// Register hardware safety callback
// This should be called at application startup to register robot safety logic
extern "C" void npk_runtime_register_safety_callback(HardwareSafetyCallback callback) {
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

// Thread-local defer stack for panic-time cleanup
// Each entry is a void(*)(void) function pointer registered by codegen
typedef void (*DeferFn)(void);

struct DeferStack {
    static constexpr size_t MAX_DEFERS = 256;
    DeferFn entries[MAX_DEFERS];
    size_t count = 0;
};

static thread_local DeferStack tl_defer_stack;

// Register a defer cleanup function (called by compiler-generated code)
extern "C" void npk_defer_push(DeferFn fn) {
    if (tl_defer_stack.count < DeferStack::MAX_DEFERS && fn) {
        tl_defer_stack.entries[tl_defer_stack.count++] = fn;
    }
}

// Unregister the most recent defer (called on normal scope exit)
extern "C" void npk_defer_pop() {
    if (tl_defer_stack.count > 0) {
        tl_defer_stack.count--;
    }
}

// Execute defer blocks for current thread in LIFO order
// Used during panic to ensure cleanup functions run before shutdown
static void execute_defer_blocks() {
    // Execute in reverse (LIFO) order — last registered, first executed
    while (tl_defer_stack.count > 0) {
        tl_defer_stack.count--;
        DeferFn fn = tl_defer_stack.entries[tl_defer_stack.count];
        if (fn) {
            fn();
        }
    }
}

// Thread-local async task context for panic task isolation
// When running inside an async task, a panic kills only the faulting task
// (by marking its Future as ERROR and longjmp-ing back to the executor)
// instead of terminating the entire process.
struct AsyncTaskContext {
    bool active;                          // Are we in an async task?
    jmp_buf recovery_point;               // longjmp target for task-local panic
    npk::runtime::Future* task_future;   // The task's result Future
};

static thread_local AsyncTaskContext tl_async_ctx = {false, {}, nullptr};

// Enter async task context (called by executor/thread pool before running a task)
extern "C" void npk_async_task_enter(void* future_ptr) {
    tl_async_ctx.active = true;
    tl_async_ctx.task_future = static_cast<npk::runtime::Future*>(future_ptr);
}

// Get pointer to recovery jmp_buf (caller uses setjmp on the returned buffer)
extern "C" void* npk_async_task_get_jmpbuf() {
    return &tl_async_ctx.recovery_point;
}

// Leave async task context (called after task completes normally)
extern "C" void npk_async_task_leave() {
    tl_async_ctx.active = false;
    tl_async_ctx.task_future = nullptr;
}

// Attempt task-local panic isolation.
// If we're inside an async task: mark the task's Future as error,
// reset panic state, and longjmp back to the executor. Does NOT return.
// If we're NOT in an async task: returns normally (caller proceeds to _exit).
static void try_task_isolation() {
    if (tl_async_ctx.active && tl_async_ctx.task_future) {
        const char* msg = "[TASK ISOLATION] Panic contained to async task\n";
        write(STDERR_FILENO, msg, strlen(msg));

        tl_async_ctx.task_future->setError(true);
        tl_async_ctx.active = false;
        tl_async_ctx.task_future = nullptr;

        // Reset panic flag so other tasks can still panic independently
        g_panic_in_progress.store(false, std::memory_order_release);

        fsync(STDERR_FILENO);
        longjmp(tl_async_ctx.recovery_point, 1);
        // Does not return
    }
}

// Main panic handler
// Called when unwrap() fails, assertions trigger, or other fatal errors occur
extern "C" void npk_runtime_panic_unwrap(const char* reason) {
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
    
    // Task isolation: if in async task context, kill only this task
    try_task_isolation();
    
    // Not in async context — shut down entire process
    fsync(STDERR_FILENO);
    _exit(1);
}

// Generic panic handler for other panic types
extern "C" void npk_runtime_panic(const char* reason) {
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
    
    try_task_isolation();
    fsync(STDERR_FILENO);
    _exit(1);
}

// Assertion failure handler
extern "C" void npk_runtime_assert_failed(const char* expr, const char* file, int line) {
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
    
    try_task_isolation();
    fsync(STDERR_FILENO);
    _exit(1);
}

// Out-of-memory panic handler
// Called when heap allocation fails during string operations, etc.
extern "C" void npk_panic_oom(const char* message) {
    bool expected = false;
    if (!g_panic_in_progress.compare_exchange_strong(expected, true)) {
        const char* msg = "RECURSIVE PANIC DETECTED - ABORTING\n";
        write(STDERR_FILENO, msg, strlen(msg));
        _exit(99);
    }
    
    log_panic(message ? message : "Out of memory", "heap allocation");
    execute_defer_blocks();
    
    HardwareSafetyCallback safety_cb = g_hardware_safety_callback.load(std::memory_order_acquire);
    if (safety_cb) {
        safety_cb(message ? message : "OOM");
    }
    
    try_task_isolation();
    fsync(STDERR_FILENO);
    _exit(1);
}

// Overflow panic handler
// Called when a checked @cast detects value overflow during narrowing
extern "C" void npk_panic_overflow(const char* message) {
    bool expected = false;
    if (!g_panic_in_progress.compare_exchange_strong(expected, true)) {
        const char* msg = "RECURSIVE PANIC DETECTED - ABORTING\n";
        write(STDERR_FILENO, msg, strlen(msg));
        _exit(99);
    }
    
    log_panic(message ? message : "Integer overflow in cast", "checked cast");
    execute_defer_blocks();
    
    HardwareSafetyCallback safety_cb = g_hardware_safety_callback.load(std::memory_order_acquire);
    if (safety_cb) {
        safety_cb(message ? message : "overflow");
    }
    
    try_task_isolation();
    fsync(STDERR_FILENO);
    _exit(1);
}

} // namespace panic
} // namespace npk
