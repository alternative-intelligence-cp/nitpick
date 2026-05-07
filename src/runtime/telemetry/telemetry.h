#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// Initialize telemetry system
void npk_telemetry_init(void);

// Record compilation event
void npk_telemetry_record_compilation(void);

// Record FFI call
void npk_telemetry_record_ffi(const char* function_name);

// Record type usage
void npk_telemetry_record_type(const char* type_name);

// Record panic event
void npk_telemetry_record_panic(const char* reason);

// Flush telemetry data
void npk_telemetry_flush(void);

// Disable telemetry
void npk_telemetry_disable(void);

#ifdef __cplusplus
}
#endif
