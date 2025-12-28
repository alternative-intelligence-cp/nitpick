/**
 * telemetry.h
 * Aria Telemetry System
 *
 * Provides structured logging infrastructure for the stddbg (FD 3) stream.
 * Implements the Aria six-stream topology's Observability Plane.
 *
 * ARIA-007: TelemetryDaemon - Ecosystem Integration
 *
 * Features:
 * - NDJSON (Newline Delimited JSON) telemetry emission
 * - Async buffering to prevent blocking application code
 * - TBB-safe metric aggregation
 * - Work-stealing executor integration
 *
 * Copyright (c) 2025 Aria Language Project
 */

#ifndef ARIA_RUNTIME_TELEMETRY_H
#define ARIA_RUNTIME_TELEMETRY_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Telemetry Event Types
// ============================================================================

typedef enum {
    ARIA_TELEM_DEBUG = 0,       // Generic debug message
    ARIA_TELEM_INFO = 1,        // Informational message
    ARIA_TELEM_WARN = 2,        // Warning
    ARIA_TELEM_ERROR = 3,       // Error
    ARIA_TELEM_METRIC = 4,      // Metric/counter update
    ARIA_TELEM_SPAN_START = 5,  // Trace span start
    ARIA_TELEM_SPAN_END = 6,    // Trace span end
    ARIA_TELEM_EVENT = 7        // Generic event
} AriaTelemetryLevel;

// ============================================================================
// Telemetry Writer (stdout-like API for stddbg)
// ============================================================================

/**
 * Telemetry writer handle
 * Manages buffered output to stddbg (FD 3)
 */
typedef struct AriaTelemetryWriter AriaTelemetryWriter;

/**
 * Get the global telemetry writer
 * Creates one if it doesn't exist (lazy initialization)
 *
 * @return Global writer instance
 */
AriaTelemetryWriter* aria_telemetry_get_writer(void);

/**
 * Create a new telemetry writer
 *
 * @param fd           File descriptor to write to (usually 3 for stddbg)
 * @param buffer_size  Internal buffer size (0 for default 8KB)
 * @param async        Use async buffering (recommended)
 * @return Writer instance or NULL on failure
 */
AriaTelemetryWriter* aria_telemetry_writer_create(int fd, size_t buffer_size, bool async);

/**
 * Destroy a telemetry writer
 * Flushes any pending output before destruction
 */
void aria_telemetry_writer_destroy(AriaTelemetryWriter* writer);

/**
 * Flush buffered telemetry to the output
 */
void aria_telemetry_flush(AriaTelemetryWriter* writer);

// ============================================================================
// Low-Level Emission API
// ============================================================================

/**
 * Emit a raw JSON line to stddbg
 * The line should NOT include the trailing newline
 *
 * @param writer  Telemetry writer (NULL for global)
 * @param json    JSON string (without newline)
 * @param len     Length of JSON string (0 for strlen)
 * @return 0 on success, negative on error
 */
int aria_telemetry_emit_raw(AriaTelemetryWriter* writer, const char* json, size_t len);

/**
 * Emit a formatted JSON event
 *
 * @param writer   Telemetry writer (NULL for global)
 * @param source   Source component name (e.g., "ariac", "atar")
 * @param event    Event type name (e.g., "file_processed", "compile_start")
 * @param payload  JSON payload string (e.g., "{\"file\": \"foo.aria\"}")
 * @return 0 on success, negative on error
 */
int aria_telemetry_emit(AriaTelemetryWriter* writer,
                         const char* source,
                         const char* event,
                         const char* payload);

/**
 * Emit a log message with level
 *
 * @param writer  Telemetry writer (NULL for global)
 * @param level   Log level (ARIA_TELEM_DEBUG, etc.)
 * @param source  Source component name
 * @param message Log message
 * @return 0 on success, negative on error
 */
int aria_telemetry_log(AriaTelemetryWriter* writer,
                        AriaTelemetryLevel level,
                        const char* source,
                        const char* message);

// ============================================================================
// Metric Emission API
// ============================================================================

/**
 * Emit a counter metric
 *
 * @param writer  Telemetry writer (NULL for global)
 * @param source  Source component name
 * @param name    Metric name
 * @param value   Counter value (tbb64)
 * @return 0 on success, negative on error
 */
int aria_telemetry_counter(AriaTelemetryWriter* writer,
                            const char* source,
                            const char* name,
                            int64_t value);

/**
 * Emit a gauge metric
 *
 * @param writer  Telemetry writer (NULL for global)
 * @param source  Source component name
 * @param name    Metric name
 * @param value   Gauge value (tbb64)
 * @return 0 on success, negative on error
 */
int aria_telemetry_gauge(AriaTelemetryWriter* writer,
                          const char* source,
                          const char* name,
                          int64_t value);

/**
 * Emit a histogram sample
 *
 * @param writer  Telemetry writer (NULL for global)
 * @param source  Source component name
 * @param name    Metric name
 * @param value   Sample value (tbb64)
 * @return 0 on success, negative on error
 */
int aria_telemetry_histogram(AriaTelemetryWriter* writer,
                              const char* source,
                              const char* name,
                              int64_t value);

// ============================================================================
// Trace/Span API
// ============================================================================

/**
 * Trace span handle
 */
typedef struct AriaSpan AriaSpan;

/**
 * Start a new trace span
 *
 * @param writer      Telemetry writer (NULL for global)
 * @param source      Source component name
 * @param operation   Operation name
 * @return Span handle (must be ended with aria_span_end)
 */
AriaSpan* aria_span_start(AriaTelemetryWriter* writer,
                           const char* source,
                           const char* operation);

/**
 * Add an attribute to a span
 *
 * @param span   Span handle
 * @param key    Attribute key
 * @param value  Attribute value (as string)
 */
void aria_span_set_attribute(AriaSpan* span, const char* key, const char* value);

/**
 * Add an integer attribute to a span
 */
void aria_span_set_attribute_int(AriaSpan* span, const char* key, int64_t value);

/**
 * Mark span as error
 */
void aria_span_set_error(AriaSpan* span, const char* message);

/**
 * End a span and emit the telemetry
 * The span is destroyed after this call
 */
void aria_span_end(AriaSpan* span);

// ============================================================================
// Session Stats (atar-compatible)
// ============================================================================

/**
 * Session statistics structure (matches atar SessionStats schema)
 */
typedef struct {
    int64_t start_time_ns;       // High-precision start time
    int64_t files_processed;     // Counter: Total files handled
    int64_t files_skipped;       // Counter: Files filtered out
    int64_t bytes_read;          // Counter: Raw I/O input
    int64_t bytes_written;       // Counter: Raw I/O output
    int64_t errors_count;        // Counter: Non-fatal errors
    int64_t current_throughput;  // Gauge: Bytes/sec (Instantaneous)
    int64_t average_throughput;  // Gauge: Bytes/sec (Session avg)
    int64_t compression_ratio;   // Gauge: Percentage (0-100)
} AriaSessionStats;

/**
 * Emit session statistics
 *
 * @param writer  Telemetry writer (NULL for global)
 * @param source  Source component name (e.g., "atar")
 * @param stats   Session statistics
 * @return 0 on success, negative on error
 */
int aria_telemetry_session_stats(AriaTelemetryWriter* writer,
                                  const char* source,
                                  const AriaSessionStats* stats);

// ============================================================================
// Telemetry Consumer (Daemon side)
// ============================================================================

/**
 * Telemetry consumer handle
 * For implementing ariad or similar daemon
 */
typedef struct AriaTelemetryConsumer AriaTelemetryConsumer;

/**
 * Callback for received telemetry events
 *
 * @param source   Source component
 * @param event    Event type
 * @param payload  JSON payload (null-terminated)
 * @param userdata User-provided context
 */
typedef void (*AriaTelemetryCallback)(const char* source,
                                       const char* event,
                                       const char* payload,
                                       void* userdata);

/**
 * Create a telemetry consumer
 *
 * @param fd       File descriptor to read from (stddbg pipe read end)
 * @param callback Callback for received events
 * @param userdata User context passed to callback
 * @return Consumer instance or NULL on failure
 */
AriaTelemetryConsumer* aria_telemetry_consumer_create(int fd,
                                                       AriaTelemetryCallback callback,
                                                       void* userdata);

/**
 * Start consuming telemetry (spawns background thread)
 */
int aria_telemetry_consumer_start(AriaTelemetryConsumer* consumer);

/**
 * Stop consuming telemetry
 */
void aria_telemetry_consumer_stop(AriaTelemetryConsumer* consumer);

/**
 * Destroy consumer
 */
void aria_telemetry_consumer_destroy(AriaTelemetryConsumer* consumer);

/**
 * Get count of events processed
 */
int64_t aria_telemetry_consumer_event_count(AriaTelemetryConsumer* consumer);

/**
 * Get count of parse errors
 */
int64_t aria_telemetry_consumer_error_count(AriaTelemetryConsumer* consumer);

// ============================================================================
// Utility Functions
// ============================================================================

/**
 * Get current timestamp in nanoseconds
 */
int64_t aria_telemetry_timestamp_ns(void);

/**
 * Escape a string for JSON output
 *
 * @param input  Input string
 * @param output Output buffer
 * @param outlen Output buffer size
 * @return Length of escaped string (may exceed outlen if truncated)
 */
size_t aria_json_escape(const char* input, char* output, size_t outlen);

/**
 * Convert log level to string
 */
const char* aria_telemetry_level_string(AriaTelemetryLevel level);

// ============================================================================
// TBB Safety
// ============================================================================

#define TBB64_ERR ((int64_t)0x8000000000000000LL)

/**
 * Check if value is a TBB error sentinel
 */
static inline bool aria_telem_is_tbb_err(int64_t value) {
    return value == TBB64_ERR;
}

#ifdef __cplusplus
}
#endif

#endif // ARIA_RUNTIME_TELEMETRY_H
