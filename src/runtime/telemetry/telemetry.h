#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Initialize telemetry system
void aria_telemetry_init(void);

// Record compilation event
void aria_telemetry_record_compilation(void);

// Record FFI call
void aria_telemetry_record_ffi(const char* function_name);

// Record type usage
void aria_telemetry_record_type(const char* type_name);

// Record panic event
void aria_telemetry_record_panic(const char* reason);

// Flush telemetry data
void aria_telemetry_flush(void);

// Disable telemetry
void aria_telemetry_disable(void);

#ifdef __cplusplus
}
#endif
