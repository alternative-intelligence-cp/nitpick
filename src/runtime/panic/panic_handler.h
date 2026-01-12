// Aria Runtime Panic Handler Header
// C interface for panic handling from Aria code

#ifndef ARIA_RUNTIME_PANIC_HANDLER_H
#define ARIA_RUNTIME_PANIC_HANDLER_H

#ifdef __cplusplus
extern "C" {
#endif

// Hardware safety callback type
// Called before process termination to engage brakes, unlock actuators, etc.
// Must be async-signal-safe (no malloc, no locks beyond atomic ops)
typedef void (*HardwareSafetyCallback)(const char* panic_reason);

// Register hardware safety callback
// Should be called at application startup to register robot safety logic
void aria_runtime_register_safety_callback(HardwareSafetyCallback callback);

// Panic handlers called from Aria code
void aria_runtime_panic_unwrap(const char* reason);
void aria_runtime_panic(const char* reason);
void aria_runtime_assert_failed(const char* expr, const char* file, int line);

#ifdef __cplusplus
}
#endif

#endif // ARIA_RUNTIME_PANIC_HANDLER_H
