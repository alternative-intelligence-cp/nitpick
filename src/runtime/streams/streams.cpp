/**
 * Aria Runtime - Modern Streams Implementation
 * 
 * Implements Aria's 6-channel I/O system with text/binary separation.
 */

#include "runtime/streams.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>

// ============================================================================
// Stream Structures
// ============================================================================

struct AriaTextStream {
    FILE* file;
    AriaStreamMode mode;
    char* buffer;
    size_t buffer_size;
    size_t buffer_used;
    bool owns_file;      // Whether to fclose() on cleanup
    bool is_eof;
};

struct AriaBinaryStream {
    FILE* file;
    char* buffer;
    size_t buffer_size;
    size_t buffer_used;
    bool owns_file;
    bool is_eof;
};

struct AriaDebugSession {
    char* session_name;
    AriaLogLevel min_level;
    bool timestamps_enabled;
    AriaTextStream* output;  // Usually stddbg
};

// ============================================================================
// Global Stream Instances
// ============================================================================

static AriaTextStream* g_stdin = NULL;
static AriaTextStream* g_stdout = NULL;
static AriaTextStream* g_stderr = NULL;
static AriaTextStream* g_stddbg = NULL;
static AriaBinaryStream* g_stddati = NULL;
static AriaBinaryStream* g_stddato = NULL;
static bool g_streams_initialized = false;

// ============================================================================
// Internal Helpers
// ============================================================================

/**
 * Default buffer size for text streams
 */
#define TEXT_BUFFER_SIZE 4096

/**
 * Default buffer size for binary streams
 */
#define BINARY_BUFFER_SIZE 8192

/**
 * Get log level name
 */
static const char* get_log_level_name(AriaLogLevel level) {
    switch (level) {
        case ARIA_LOG_DEBUG: return "DEBUG";
        case ARIA_LOG_INFO:  return "INFO";
        case ARIA_LOG_WARN:  return "WARN";
        case ARIA_LOG_ERROR: return "ERROR";
        case ARIA_LOG_FATAL: return "FATAL";
        default: return "UNKNOWN";
    }
}

/**
 * Get current timestamp string
 */
static char* get_timestamp(void) {
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    char* buffer = (char*)malloc(32);
    if (!buffer) return NULL;
    
    strftime(buffer, 32, "%Y-%m-%d %H:%M:%S", tm_info);
    return buffer;
}

// ============================================================================
// Text Stream Implementation
// ============================================================================

AriaTextStream* aria_text_stream_create(void* file, AriaStreamMode mode) {
    if (!file) return NULL;
    
    AriaTextStream* stream = (AriaTextStream*)malloc(sizeof(AriaTextStream));
    if (!stream) return NULL;
    
    stream->file = (FILE*)file;
    stream->mode = mode;
    stream->buffer = NULL;
    stream->buffer_size = 0;
    stream->buffer_used = 0;
    stream->owns_file = false;
    stream->is_eof = false;
    
    // Allocate buffer for buffered modes
    if (mode != ARIA_STREAM_UNBUFFERED) {
        stream->buffer = (char*)malloc(TEXT_BUFFER_SIZE);
        if (!stream->buffer) {
            free(stream);
            return NULL;
        }
        stream->buffer_size = TEXT_BUFFER_SIZE;
    }
    
    return stream;
}

int64_t aria_text_stream_write(AriaTextStream* stream, const char* str) {
    if (!stream || !str) return -1;
    
    size_t len = strlen(str);
    
    // Unbuffered: write directly
    if (stream->mode == ARIA_STREAM_UNBUFFERED) {
        size_t written = fwrite(str, 1, len, stream->file);
        fflush(stream->file);
        return (int64_t)written;
    }
    
    // Buffered modes
    size_t written = 0;
    while (written < len) {
        size_t remaining = len - written;
        size_t buffer_space = stream->buffer_size - stream->buffer_used;
        
        // If buffer is full, flush it
        if (buffer_space == 0) {
            if (aria_text_stream_flush(stream) < 0) {
                return -1;
            }
            buffer_space = stream->buffer_size;
        }
        
        // Copy what we can to buffer
        size_t to_copy = remaining < buffer_space ? remaining : buffer_space;
        memcpy(stream->buffer + stream->buffer_used, str + written, to_copy);
        stream->buffer_used += to_copy;
        written += to_copy;
        
        // Line-buffered: flush on newline
        if (stream->mode == ARIA_STREAM_LINE_BUFFERED) {
            if (memchr(str + written - to_copy, '\n', to_copy)) {
                if (aria_text_stream_flush(stream) < 0) {
                    return -1;
                }
            }
        }
    }
    
    return (int64_t)written;
}

int64_t aria_text_stream_printf(AriaTextStream* stream, const char* format, ...) {
    if (!stream || !format) return -1;
    
    va_list args;
    va_start(args, format);
    
    // Calculate required size
    va_list args_copy;
    va_copy(args_copy, args);
    int size = vsnprintf(NULL, 0, format, args_copy);
    va_end(args_copy);
    
    if (size < 0) {
        va_end(args);
        return -1;
    }
    
    // Allocate buffer
    char* buffer = (char*)malloc(size + 1);
    if (!buffer) {
        va_end(args);
        return -1;
    }
    
    // Format string
    vsnprintf(buffer, size + 1, format, args);
    va_end(args);
    
    // Write to stream
    int64_t result = aria_text_stream_write(stream, buffer);
    free(buffer);
    
    return result;
}

char* aria_text_stream_read_line(AriaTextStream* stream) {
    if (!stream) return NULL;
    
    size_t capacity = 128;
    size_t length = 0;
    char* line = (char*)malloc(capacity);
    if (!line) return NULL;
    
    while (1) {
        int c = fgetc(stream->file);
        
        if (c == EOF) {
            stream->is_eof = true;
            if (length == 0) {
                free(line);
                return NULL;
            }
            break;
        }
        
        // Grow buffer if needed
        if (length + 2 > capacity) {
            capacity *= 2;
            char* new_line = (char*)realloc(line, capacity);
            if (!new_line) {
                free(line);
                return NULL;
            }
            line = new_line;
        }
        
        line[length++] = (char)c;
        
        if (c == '\n') {
            break;
        }
    }
    
    line[length] = '\0';
    return line;
}

char* aria_text_stream_read_all(AriaTextStream* stream) {
    if (!stream) return NULL;
    
    // Get file size
    long current = ftell(stream->file);
    fseek(stream->file, 0, SEEK_END);
    long size = ftell(stream->file);
    fseek(stream->file, current, SEEK_SET);
    
    long remaining = size - current;
    if (remaining < 0) remaining = 0;
    
    // Allocate buffer
    char* buffer = (char*)malloc(remaining + 1);
    if (!buffer) return NULL;
    
    // Read all data
    size_t bytes_read = fread(buffer, 1, remaining, stream->file);
    buffer[bytes_read] = '\0';
    
    if (feof(stream->file)) {
        stream->is_eof = true;
    }
    
    return buffer;
}

int aria_text_stream_flush(AriaTextStream* stream) {
    if (!stream) return -1;
    
    // Write buffered data
    if (stream->buffer_used > 0) {
        size_t written = fwrite(stream->buffer, 1, stream->buffer_used, stream->file);
        if (written != stream->buffer_used) {
            return -1;
        }
        stream->buffer_used = 0;
    }
    
    // Flush underlying FILE*
    return fflush(stream->file);
}

bool aria_text_stream_eof(AriaTextStream* stream) {
    if (!stream) return true;
    return stream->is_eof || feof(stream->file);
}

void aria_text_stream_set_mode(AriaTextStream* stream, AriaStreamMode mode) {
    if (!stream) return;
    
    // Flush before changing mode
    aria_text_stream_flush(stream);
    
    stream->mode = mode;
    
    // Allocate or free buffer as needed
    if (mode == ARIA_STREAM_UNBUFFERED) {
        if (stream->buffer) {
            free(stream->buffer);
            stream->buffer = NULL;
            stream->buffer_size = 0;
            stream->buffer_used = 0;
        }
    } else if (!stream->buffer) {
        stream->buffer = (char*)malloc(TEXT_BUFFER_SIZE);
        stream->buffer_size = TEXT_BUFFER_SIZE;
        stream->buffer_used = 0;
    }
}

void aria_text_stream_close(AriaTextStream* stream) {
    if (!stream) return;
    
    aria_text_stream_flush(stream);
    
    if (stream->owns_file && stream->file) {
        fclose(stream->file);
    }
    
    if (stream->buffer) {
        free(stream->buffer);
    }
    
    free(stream);
}

// ============================================================================
// Binary Stream Implementation
// ============================================================================

AriaBinaryStream* aria_binary_stream_create(void* file, size_t buffer_size) {
    if (!file) return NULL;
    
    AriaBinaryStream* stream = (AriaBinaryStream*)malloc(sizeof(AriaBinaryStream));
    if (!stream) return NULL;
    
    stream->file = (FILE*)file;
    stream->buffer = NULL;
    stream->buffer_size = buffer_size;
    stream->buffer_used = 0;
    stream->owns_file = false;
    stream->is_eof = false;
    
    // Allocate buffer if requested
    if (buffer_size > 0) {
        stream->buffer = (char*)malloc(buffer_size);
        if (!stream->buffer) {
            free(stream);
            return NULL;
        }
    }
    
    return stream;
}

int64_t aria_binary_stream_write(AriaBinaryStream* stream, const void* data, size_t size) {
    if (!stream || !data) return -1;
    
    // Unbuffered: write directly
    if (stream->buffer_size == 0) {
        size_t written = fwrite(data, 1, size, stream->file);
        return (int64_t)written;
    }
    
    // Buffered write
    size_t written = 0;
    const char* bytes = (const char*)data;
    
    while (written < size) {
        size_t remaining = size - written;
        size_t buffer_space = stream->buffer_size - stream->buffer_used;
        
        // If buffer is full, flush it
        if (buffer_space == 0) {
            if (aria_binary_stream_flush(stream) < 0) {
                return -1;
            }
            buffer_space = stream->buffer_size;
        }
        
        // Copy what we can to buffer
        size_t to_copy = remaining < buffer_space ? remaining : buffer_space;
        memcpy(stream->buffer + stream->buffer_used, bytes + written, to_copy);
        stream->buffer_used += to_copy;
        written += to_copy;
    }
    
    return (int64_t)written;
}

int64_t aria_binary_stream_read(AriaBinaryStream* stream, void* buffer, size_t size) {
    if (!stream || !buffer) return -1;
    
    size_t bytes_read = fread(buffer, 1, size, stream->file);
    
    if (feof(stream->file)) {
        stream->is_eof = true;
    }
    
    return (int64_t)bytes_read;
}

void* aria_binary_stream_read_all(AriaBinaryStream* stream, size_t* size_out) {
    if (!stream || !size_out) return NULL;
    
    // Get file size
    long current = ftell(stream->file);
    fseek(stream->file, 0, SEEK_END);
    long size = ftell(stream->file);
    fseek(stream->file, current, SEEK_SET);
    
    long remaining = size - current;
    if (remaining < 0) remaining = 0;
    
    // Allocate buffer
    void* buffer = malloc(remaining);
    if (!buffer) {
        *size_out = 0;
        return NULL;
    }
    
    // Read all data
    size_t bytes_read = fread(buffer, 1, remaining, stream->file);
    *size_out = bytes_read;
    
    if (feof(stream->file)) {
        stream->is_eof = true;
    }
    
    return buffer;
}

int aria_binary_stream_flush(AriaBinaryStream* stream) {
    if (!stream) return -1;
    
    // Write buffered data
    if (stream->buffer_used > 0) {
        size_t written = fwrite(stream->buffer, 1, stream->buffer_used, stream->file);
        if (written != stream->buffer_used) {
            return -1;
        }
        stream->buffer_used = 0;
    }
    
    // Flush underlying FILE*
    return fflush(stream->file);
}

bool aria_binary_stream_eof(AriaBinaryStream* stream) {
    if (!stream) return true;
    return stream->is_eof || feof(stream->file);
}

void aria_binary_stream_close(AriaBinaryStream* stream) {
    if (!stream) return;
    
    aria_binary_stream_flush(stream);
    
    if (stream->owns_file && stream->file) {
        fclose(stream->file);
    }
    
    if (stream->buffer) {
        free(stream->buffer);
    }
    
    free(stream);
}

// ============================================================================
// Debug Session Implementation
// ============================================================================

AriaDebugSession* aria_debug_session_create(const char* session_name) {
    if (!session_name) return NULL;
    
    AriaDebugSession* session = (AriaDebugSession*)malloc(sizeof(AriaDebugSession));
    if (!session) return NULL;
    
    session->session_name = strdup(session_name);
    if (!session->session_name) {
        free(session);
        return NULL;
    }
    
    session->min_level = ARIA_LOG_DEBUG;
    session->timestamps_enabled = true;
    session->output = aria_get_stddbg();
    
    return session;
}

void aria_debug_session_log(AriaDebugSession* session, AriaLogLevel level, const char* message) {
    if (!session || !message) return;
    
    // Filter by level
    if (level < session->min_level) return;
    
    // Build log line
    char* timestamp = NULL;
    if (session->timestamps_enabled) {
        timestamp = get_timestamp();
    }
    
    const char* level_name = get_log_level_name(level);
    
    if (timestamp) {
        aria_text_stream_printf(session->output, "[%s] [%s] [%s] %s\n",
            timestamp, level_name, session->session_name, message);
        free(timestamp);
    } else {
        aria_text_stream_printf(session->output, "[%s] [%s] %s\n",
            level_name, session->session_name, message);
    }
}

void aria_debug_session_logf(AriaDebugSession* session, AriaLogLevel level, const char* format, ...) {
    if (!session || !format) return;
    
    // Filter by level
    if (level < session->min_level) return;
    
    // Format message
    va_list args;
    va_start(args, format);
    
    va_list args_copy;
    va_copy(args_copy, args);
    int size = vsnprintf(NULL, 0, format, args_copy);
    va_end(args_copy);
    
    if (size < 0) {
        va_end(args);
        return;
    }
    
    char* message = (char*)malloc(size + 1);
    if (!message) {
        va_end(args);
        return;
    }
    
    vsnprintf(message, size + 1, format, args);
    va_end(args);
    
    // Log the message
    aria_debug_session_log(session, level, message);
    free(message);
}

void aria_debug_session_set_min_level(AriaDebugSession* session, AriaLogLevel min_level) {
    if (!session) return;
    session->min_level = min_level;
}

void aria_debug_session_set_timestamps(AriaDebugSession* session, bool enabled) {
    if (!session) return;
    session->timestamps_enabled = enabled;
}

void aria_debug_session_close(AriaDebugSession* session) {
    if (!session) return;
    
    if (session->session_name) {
        free(session->session_name);
    }
    
    free(session);
}

// ============================================================================
// Global Stream Initialization
// ============================================================================

void aria_streams_init(void) {
    if (g_streams_initialized) return;
    
    // Initialize text streams
    g_stdin = aria_text_stream_create(stdin, ARIA_STREAM_LINE_BUFFERED);
    g_stdout = aria_text_stream_create(stdout, ARIA_STREAM_LINE_BUFFERED);
    g_stderr = aria_text_stream_create(stderr, ARIA_STREAM_UNBUFFERED);
    g_stddbg = aria_text_stream_create(stderr, ARIA_STREAM_UNBUFFERED); // Debug goes to stderr by default
    
    // Initialize binary streams
    g_stddati = aria_binary_stream_create(stdin, BINARY_BUFFER_SIZE);
    g_stddato = aria_binary_stream_create(stdout, BINARY_BUFFER_SIZE);
    
    g_streams_initialized = true;
}

void aria_streams_cleanup(void) {
    if (!g_streams_initialized) return;
    
    // Flush and close text streams (but don't fclose the underlying FILE*)
    if (g_stdin) aria_text_stream_close(g_stdin);
    if (g_stdout) aria_text_stream_close(g_stdout);
    if (g_stderr) aria_text_stream_close(g_stderr);
    if (g_stddbg) aria_text_stream_close(g_stddbg);
    
    // Flush and close binary streams
    if (g_stddati) aria_binary_stream_close(g_stddati);
    if (g_stddato) aria_binary_stream_close(g_stddato);
    
    g_stdin = NULL;
    g_stdout = NULL;
    g_stderr = NULL;
    g_stddbg = NULL;
    g_stddati = NULL;
    g_stddato = NULL;
    
    g_streams_initialized = false;
}

// ============================================================================
// Global Stream Accessors
// ============================================================================

AriaTextStream* aria_get_stdin(void) {
    if (!g_streams_initialized) aria_streams_init();
    return g_stdin;
}

AriaTextStream* aria_get_stdout(void) {
    if (!g_streams_initialized) aria_streams_init();
    return g_stdout;
}

AriaTextStream* aria_get_stderr(void) {
    if (!g_streams_initialized) aria_streams_init();
    return g_stderr;
}

AriaTextStream* aria_get_stddbg(void) {
    if (!g_streams_initialized) aria_streams_init();
    return g_stddbg;
}

AriaBinaryStream* aria_get_stddati(void) {
    if (!g_streams_initialized) aria_streams_init();
    return g_stddati;
}

AriaBinaryStream* aria_get_stddato(void) {
    if (!g_streams_initialized) aria_streams_init();
    return g_stddato;
}

// ============================================================================
// Convenience Functions - stdout
// ============================================================================

int64_t aria_stdout_write(const char* str) {
    return aria_text_stream_write(aria_get_stdout(), str);
}

int64_t aria_stdout_printf(const char* format, ...) {
    va_list args;
    va_start(args, format);
    
    va_list args_copy;
    va_copy(args_copy, args);
    int size = vsnprintf(NULL, 0, format, args_copy);
    va_end(args_copy);
    
    if (size < 0) {
        va_end(args);
        return -1;
    }
    
    char* buffer = (char*)malloc(size + 1);
    if (!buffer) {
        va_end(args);
        return -1;
    }
    
    vsnprintf(buffer, size + 1, format, args);
    va_end(args);
    
    int64_t result = aria_stdout_write(buffer);
    free(buffer);
    
    return result;
}

int aria_stdout_flush(void) {
    return aria_text_stream_flush(aria_get_stdout());
}

// ============================================================================
// Convenience Functions - stderr
// ============================================================================

int64_t aria_stderr_write(const char* str) {
    return aria_text_stream_write(aria_get_stderr(), str);
}

int64_t aria_stderr_printf(const char* format, ...) {
    va_list args;
    va_start(args, format);
    
    va_list args_copy;
    va_copy(args_copy, args);
    int size = vsnprintf(NULL, 0, format, args_copy);
    va_end(args_copy);
    
    if (size < 0) {
        va_end(args);
        return -1;
    }
    
    char* buffer = (char*)malloc(size + 1);
    if (!buffer) {
        va_end(args);
        return -1;
    }
    
    vsnprintf(buffer, size + 1, format, args);
    va_end(args);
    
    int64_t result = aria_stderr_write(buffer);
    free(buffer);
    
    return result;
}

int aria_stderr_flush(void) {
    return aria_text_stream_flush(aria_get_stderr());
}

// ============================================================================
// Convenience Functions - stddbg
// ============================================================================

int64_t aria_stddbg_write(const char* str) {
    return aria_text_stream_write(aria_get_stddbg(), str);
}

int64_t aria_stddbg_printf(const char* format, ...) {
    va_list args;
    va_start(args, format);
    
    va_list args_copy;
    va_copy(args_copy, args);
    int size = vsnprintf(NULL, 0, format, args_copy);
    va_end(args_copy);
    
    if (size < 0) {
        va_end(args);
        return -1;
    }
    
    char* buffer = (char*)malloc(size + 1);
    if (!buffer) {
        va_end(args);
        return -1;
    }
    
    vsnprintf(buffer, size + 1, format, args);
    va_end(args);
    
    int64_t result = aria_stddbg_write(buffer);
    free(buffer);
    
    return result;
}

int aria_stddbg_flush(void) {
    return aria_text_stream_flush(aria_get_stddbg());
}

// ============================================================================
// Convenience Functions - stdin
// ============================================================================

char* aria_stdin_read_line(void) {
    return aria_text_stream_read_line(aria_get_stdin());
}

char* aria_stdin_read_all(void) {
    return aria_text_stream_read_all(aria_get_stdin());
}

bool aria_stdin_eof(void) {
    return aria_text_stream_eof(aria_get_stdin());
}

// ============================================================================
// Convenience Functions - stddati
// ============================================================================

int64_t aria_stddati_read(void* buffer, size_t size) {
    return aria_binary_stream_read(aria_get_stddati(), buffer, size);
}

void* aria_stddati_read_all(size_t* size_out) {
    return aria_binary_stream_read_all(aria_get_stddati(), size_out);
}

bool aria_stddati_eof(void) {
    return aria_binary_stream_eof(aria_get_stddati());
}

// ============================================================================
// Convenience Functions - stddato
// ============================================================================

int64_t aria_stddato_write(const void* data, size_t size) {
    return aria_binary_stream_write(aria_get_stddato(), data, size);
}

int aria_stddato_flush(void) {
    return aria_binary_stream_flush(aria_get_stddato());
}

// ============================================================================
// Convenience Debug Logging
// ============================================================================

void aria_log_debug(const char* message) {
    aria_stddbg_printf("[DEBUG] %s\n", message);
}

void aria_log_debugf(const char* format, ...) {
    va_list args;
    va_start(args, format);
    
    aria_stddbg_write("[DEBUG] ");
    
    va_list args_copy;
    va_copy(args_copy, args);
    int size = vsnprintf(NULL, 0, format, args_copy);
    va_end(args_copy);
    
    if (size >= 0) {
        char* buffer = (char*)malloc(size + 1);
        if (buffer) {
            vsnprintf(buffer, size + 1, format, args);
            aria_stddbg_write(buffer);
            free(buffer);
        }
    }
    
    aria_stddbg_write("\n");
    va_end(args);
}

void aria_log_info(const char* message) {
    aria_stddbg_printf("[INFO] %s\n", message);
}

void aria_log_infof(const char* format, ...) {
    va_list args;
    va_start(args, format);
    
    aria_stddbg_write("[INFO] ");
    
    va_list args_copy;
    va_copy(args_copy, args);
    int size = vsnprintf(NULL, 0, format, args_copy);
    va_end(args_copy);
    
    if (size >= 0) {
        char* buffer = (char*)malloc(size + 1);
        if (buffer) {
            vsnprintf(buffer, size + 1, format, args);
            aria_stddbg_write(buffer);
            free(buffer);
        }
    }
    
    aria_stddbg_write("\n");
    va_end(args);
}

void aria_log_warn(const char* message) {
    aria_stddbg_printf("[WARN] %s\n", message);
}

void aria_log_warnf(const char* format, ...) {
    va_list args;
    va_start(args, format);
    
    aria_stddbg_write("[WARN] ");
    
    va_list args_copy;
    va_copy(args_copy, args);
    int size = vsnprintf(NULL, 0, format, args_copy);
    va_end(args_copy);
    
    if (size >= 0) {
        char* buffer = (char*)malloc(size + 1);
        if (buffer) {
            vsnprintf(buffer, size + 1, format, args);
            aria_stddbg_write(buffer);
            free(buffer);
        }
    }
    
    aria_stddbg_write("\n");
    va_end(args);
}

void aria_log_error(const char* message) {
    aria_stddbg_printf("[ERROR] %s\n", message);
}

void aria_log_errorf(const char* format, ...) {
    va_list args;
    va_start(args, format);
    
    aria_stddbg_write("[ERROR] ");
    
    va_list args_copy;
    va_copy(args_copy, args);
    int size = vsnprintf(NULL, 0, format, args_copy);
    va_end(args_copy);
    
    if (size >= 0) {
        char* buffer = (char*)malloc(size + 1);
        if (buffer) {
            vsnprintf(buffer, size + 1, format, args);
            aria_stddbg_write(buffer);
            free(buffer);
        }
    }
    
    aria_stddbg_write("\n");
    va_end(args);
}

void aria_log_fatal(const char* message) {
    aria_stddbg_printf("[FATAL] %s\n", message);
}

void aria_log_fatalf(const char* format, ...) {
    va_list args;
    va_start(args, format);
    
    aria_stddbg_write("[FATAL] ");
    
    va_list args_copy;
    va_copy(args_copy, args);
    int size = vsnprintf(NULL, 0, format, args_copy);
    va_end(args_copy);
    
    if (size >= 0) {
        char* buffer = (char*)malloc(size + 1);
        if (buffer) {
            vsnprintf(buffer, size + 1, format, args);
            aria_stddbg_write(buffer);
            free(buffer);
        }
    }
    
    aria_stddbg_write("\n");
    va_end(args);
}
