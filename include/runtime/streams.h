/**
 * Aria Runtime - Modern Streams Library
 * 
 * Implements Aria's 6-channel I/O system:
 *   - stdin:   Text input stream
 *   - stdout:  Text output stream  
 *   - stderr:  Error output stream
 *   - stddbg:  Debug output stream
 *   - stddati: Binary data input stream
 *   - stddato: Binary data output stream
 * 
 * This separation prevents mixing text and binary data, provides dedicated
 * debug channels, and enables structured logging.
 */

#ifndef ARIA_RUNTIME_STREAMS_H
#define ARIA_RUNTIME_STREAMS_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Forward Declarations
// ============================================================================

typedef struct AriaTextStream AriaTextStream;
typedef struct AriaBinaryStream AriaBinaryStream;
typedef struct AriaDebugSession AriaDebugSession;

// ============================================================================
// Stream Configuration
// ============================================================================

/**
 * Text stream modes for buffering control
 */
typedef enum {
    ARIA_STREAM_LINE_BUFFERED,    // Buffer until newline (default for stdout)
    ARIA_STREAM_FULLY_BUFFERED,   // Buffer until flush or buffer full
    ARIA_STREAM_UNBUFFERED        // No buffering (default for stderr)
} AriaStreamMode;

/**
 * Debug log levels
 */
typedef enum {
    ARIA_LOG_DEBUG,    // Detailed debug information
    ARIA_LOG_INFO,     // Informational messages
    ARIA_LOG_WARN,     // Warning messages
    ARIA_LOG_ERROR,    // Error messages
    ARIA_LOG_FATAL     // Fatal error messages
} AriaLogLevel;

// ============================================================================
// Global Stream Handles
// ============================================================================

/**
 * Global text stream handles (initialized on first use)
 * 
 * Usage:
 *   aria_stdout_write("Hello, World!");
 *   aria_stderr_write("Error occurred");
 *   aria_stddbg_write("Debug: x = 42");
 *   char* line = aria_stdin_read_line();
 */

// Get global stream instances
AriaTextStream* aria_get_stdin(void);
AriaTextStream* aria_get_stdout(void);
AriaTextStream* aria_get_stderr(void);
AriaTextStream* aria_get_stddbg(void);
AriaBinaryStream* aria_get_stddati(void);
AriaBinaryStream* aria_get_stddato(void);

// ============================================================================
// Text Stream API
// ============================================================================

/**
 * Create a text stream wrapping a FILE*
 * 
 * @param file The FILE* to wrap (stdin, stdout, stderr, or opened file)
 * @param mode Buffering mode
 * @return New text stream, or NULL on error
 */
AriaTextStream* aria_text_stream_create(void* file, AriaStreamMode mode);

/**
 * Write a string to a text stream
 * 
 * @param stream The stream to write to
 * @param str The null-terminated string to write
 * @return Number of bytes written, or -1 on error
 */
int64_t aria_text_stream_write(AriaTextStream* stream, const char* str);

/**
 * Write formatted output to a text stream (printf-style)
 * 
 * @param stream The stream to write to
 * @param format Format string
 * @param ... Format arguments
 * @return Number of bytes written, or -1 on error
 */
int64_t aria_text_stream_printf(AriaTextStream* stream, const char* format, ...);

/**
 * Read a line from a text stream (up to and including newline)
 * 
 * @param stream The stream to read from
 * @return Allocated string containing the line (caller must free), or NULL on EOF/error
 */
char* aria_text_stream_read_line(AriaTextStream* stream);

/**
 * Read all remaining text from a stream
 * 
 * @param stream The stream to read from
 * @return Allocated string with all content (caller must free), or NULL on error
 */
char* aria_text_stream_read_all(AriaTextStream* stream);

/**
 * Flush buffered output to the underlying stream
 * 
 * @param stream The stream to flush
 * @return 0 on success, -1 on error
 */
int aria_text_stream_flush(AriaTextStream* stream);

/**
 * Check if stream is at end-of-file
 * 
 * @param stream The stream to check
 * @return true if at EOF, false otherwise
 */
bool aria_text_stream_eof(AriaTextStream* stream);

/**
 * Set buffering mode for a text stream
 * 
 * @param stream The stream to configure
 * @param mode New buffering mode
 */
void aria_text_stream_set_mode(AriaTextStream* stream, AriaStreamMode mode);

/**
 * Close and free a text stream
 * 
 * @param stream The stream to close
 */
void aria_text_stream_close(AriaTextStream* stream);

// ============================================================================
// Convenience Functions for Global Streams
// ============================================================================

// stdout operations
int64_t aria_stdout_write(const char* str);
int64_t aria_stdout_printf(const char* format, ...);
int aria_stdout_flush(void);

// stderr operations
int64_t aria_stderr_write(const char* str);
int64_t aria_stderr_printf(const char* format, ...);
int aria_stderr_flush(void);

// stddbg operations
int64_t aria_stddbg_write(const char* str);
int64_t aria_stddbg_printf(const char* format, ...);
int aria_stddbg_flush(void);

// stdin operations
char* aria_stdin_read_line(void);
char* aria_stdin_read_all(void);
bool aria_stdin_eof(void);

// ============================================================================
// Binary Stream API
// ============================================================================

/**
 * Create a binary stream wrapping a FILE*
 * 
 * @param file The FILE* to wrap (opened in binary mode)
 * @param buffer_size Size of internal buffer (0 for unbuffered)
 * @return New binary stream, or NULL on error
 */
AriaBinaryStream* aria_binary_stream_create(void* file, size_t buffer_size);

/**
 * Write binary data to a stream
 * 
 * @param stream The stream to write to
 * @param data Pointer to data buffer
 * @param size Number of bytes to write
 * @return Number of bytes written, or -1 on error
 */
int64_t aria_binary_stream_write(AriaBinaryStream* stream, const void* data, size_t size);

/**
 * Read binary data from a stream
 * 
 * @param stream The stream to read from
 * @param buffer Buffer to read into
 * @param size Number of bytes to read
 * @return Number of bytes actually read, or -1 on error
 */
int64_t aria_binary_stream_read(AriaBinaryStream* stream, void* buffer, size_t size);

/**
 * Read all remaining binary data from a stream
 * 
 * @param stream The stream to read from
 * @param size_out Pointer to store the size of returned data
 * @return Allocated buffer with data (caller must free), or NULL on error
 */
void* aria_binary_stream_read_all(AriaBinaryStream* stream, size_t* size_out);

/**
 * Flush buffered binary data to the underlying stream
 * 
 * @param stream The stream to flush
 * @return 0 on success, -1 on error
 */
int aria_binary_stream_flush(AriaBinaryStream* stream);

/**
 * Check if binary stream is at end-of-file
 * 
 * @param stream The stream to check
 * @return true if at EOF, false otherwise
 */
bool aria_binary_stream_eof(AriaBinaryStream* stream);

/**
 * Close and free a binary stream
 * 
 * @param stream The stream to close
 */
void aria_binary_stream_close(AriaBinaryStream* stream);

// ============================================================================
// Convenience Functions for Global Binary Streams
// ============================================================================

// stddati (binary input) operations
int64_t aria_stddati_read(void* buffer, size_t size);
void* aria_stddati_read_all(size_t* size_out);
bool aria_stddati_eof(void);

// stddato (binary output) operations
int64_t aria_stddato_write(const void* data, size_t size);
int aria_stddato_flush(void);

// ============================================================================
// Debug Session API (Structured Logging)
// ============================================================================

/**
 * Create a debug session with a name/context
 * 
 * Debug sessions provide structured logging with automatic context tracking.
 * Each session can log messages at different levels (debug, info, warn, error, fatal).
 * 
 * @param session_name Name/identifier for this logging session
 * @return New debug session, or NULL on error
 */
AriaDebugSession* aria_debug_session_create(const char* session_name);

/**
 * Log a message to a debug session
 * 
 * @param session The debug session
 * @param level Log level (debug, info, warn, error, fatal)
 * @param message The log message
 */
void aria_debug_session_log(AriaDebugSession* session, AriaLogLevel level, const char* message);

/**
 * Log a formatted message to a debug session (printf-style)
 * 
 * @param session The debug session
 * @param level Log level
 * @param format Format string
 * @param ... Format arguments
 */
void aria_debug_session_logf(AriaDebugSession* session, AriaLogLevel level, const char* format, ...);

/**
 * Set minimum log level for a session
 * 
 * Messages below this level will be filtered out.
 * 
 * @param session The debug session
 * @param min_level Minimum level to log
 */
void aria_debug_session_set_min_level(AriaDebugSession* session, AriaLogLevel min_level);

/**
 * Enable/disable timestamp prefixes for log messages
 * 
 * @param session The debug session
 * @param enabled true to enable timestamps, false to disable
 */
void aria_debug_session_set_timestamps(AriaDebugSession* session, bool enabled);

/**
 * Close and free a debug session
 * 
 * @param session The session to close
 */
void aria_debug_session_close(AriaDebugSession* session);

// ============================================================================
// Convenience Debug Functions (using default stddbg)
// ============================================================================

/**
 * Log to stddbg at debug level
 */
void aria_log_debug(const char* message);
void aria_log_debugf(const char* format, ...);

/**
 * Log to stddbg at info level
 */
void aria_log_info(const char* message);
void aria_log_infof(const char* format, ...);

/**
 * Log to stddbg at warn level
 */
void aria_log_warn(const char* message);
void aria_log_warnf(const char* format, ...);

/**
 * Log to stddbg at error level
 */
void aria_log_error(const char* message);
void aria_log_errorf(const char* format, ...);

/**
 * Log to stddbg at fatal level
 */
void aria_log_fatal(const char* message);
void aria_log_fatalf(const char* format, ...);

// ============================================================================
// Stream Initialization and Cleanup
// ============================================================================

/**
 * Initialize the global stream system
 * 
 * This is called automatically on first use of any global stream,
 * but can be called explicitly for initialization control.
 */
void aria_streams_init(void);

/**
 * Cleanup the global stream system
 * 
 * Flushes and closes all global streams. Should be called before
 * program exit to ensure all buffered data is written.
 */
void aria_streams_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif // ARIA_RUNTIME_STREAMS_H
