/**
 * Aria Runtime - File I/O Library
 * 
 * Complete file I/O operations with result-based error handling.
 * Supports both simple file operations and streaming I/O.
 * 
 * Features:
 * - Simple file operations (readFile, writeFile)
 * - Stream operations (openFile, readLine, write, close)
 * - Structured file parsing (readJSON, readCSV)
 * - Result type integration for error handling
 * - Cross-platform (POSIX and Windows)
 * 
 * Usage:
 *   // Simple file read
 *   AriaResult* result = aria_read_file("config.txt");
 *   if (result->err == NULL) {
 *       printf("Content: %s\n", (char*)result->val);
 *       aria_result_free(result);
 *   }
 *   
 *   // Stream operations
 *   AriaStream* stream = aria_open_file("data.txt", "r");
 *   if (stream) {
 *       char* line = aria_stream_read_line(stream);
 *       while (line) {
 *           process(line);
 *           free(line);
 *           line = aria_stream_read_line(stream);
 *       }
 *       aria_stream_close(stream);
 *   }
 */

#ifndef ARIA_IO_H
#define ARIA_IO_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Result Type (for error handling)
// ============================================================================

/**
 * Result Type
 * 
 * All I/O operations return result<T> with {err, val} pattern.
 * - err: NULL on success, error message on failure
 * - val: Return value on success, NULL/undefined on failure
 */
typedef struct AriaResult {
    char* err;          // Error message (NULL on success)
    void* val;          // Value (NULL on error)
    size_t val_size;    // Size of value (for memory management)
} AriaResult;

/**
 * Create successful result
 * 
 * @param value Value pointer (takes ownership)
 * @param size Size of value
 * @return Result with err=NULL, val=value
 */
AriaResult* aria_result_ok(void* value, size_t size);

/**
 * Create error result
 * 
 * @param error Error message (copied internally)
 * @return Result with err=message, val=NULL
 */
AriaResult* aria_result_err(const char* error);

/**
 * Free result
 * 
 * Frees both error message and value.
 * 
 * @param result Result to free
 */
void aria_result_free(AriaResult* result);

// ============================================================================
// Stream Type
// ============================================================================

/**
 * Stream Handle
 * 
 * Opaque handle for file stream operations.
 * Supports both text and binary modes.
 */
typedef struct AriaStream AriaStream;

// ============================================================================
// Simple File Operations
// ============================================================================

/**
 * Read entire file into string
 * 
 * @param path File path
 * @return Result with string content or error
 * 
 * Example:
 *   AriaResult* r = aria_read_file("config.txt");
 *   if (r->err == NULL) {
 *       printf("%s\n", (char*)r->val);
 *   }
 *   aria_result_free(r);
 */
AriaResult* aria_read_file(const char* path);

/**
 * Write string to file
 * 
 * @param path File path
 * @param content String content to write
 * @return Result with NULL val on success, error on failure
 * 
 * Example:
 *   AriaResult* r = aria_write_file("output.txt", "Hello world");
 *   if (r->err != NULL) {
 *       fprintf(stderr, "Error: %s\n", r->err);
 *   }
 *   aria_result_free(r);
 */
AriaResult* aria_write_file(const char* path, const char* content);

/**
 * Read binary file into buffer
 * 
 * @param path File path
 * @param size Output parameter for buffer size
 * @return Result with buffer or error
 * 
 * Example:
 *   size_t size;
 *   AriaResult* r = aria_read_binary(path, &size);
 *   if (r->err == NULL) {
 *       uint8_t* data = (uint8_t*)r->val;
 *       process(data, size);
 *   }
 *   aria_result_free(r);
 */
AriaResult* aria_read_binary(const char* path, size_t* size);

/**
 * Write binary buffer to file
 * 
 * @param path File path
 * @param data Buffer to write
 * @param size Size of buffer
 * @return Result with NULL val on success, error on failure
 */
AriaResult* aria_write_binary(const char* path, const void* data, size_t size);

/**
 * Check if file exists
 * 
 * @param path File path
 * @return true if file exists, false otherwise
 */
bool aria_file_exists(const char* path);

/**
 * Get file size
 * 
 * @param path File path
 * @return File size in bytes, or -1 on error
 */
int64_t aria_file_size(const char* path);

/**
 * Delete file
 * 
 * @param path File path
 * @return Result with NULL val on success, error on failure
 */
AriaResult* aria_delete_file(const char* path);

// ============================================================================
// Stream Operations
// ============================================================================

/**
 * Open file stream
 * 
 * @param path File path
 * @param mode Mode string: "r" (read), "w" (write), "a" (append), 
 *             "rb" (read binary), "wb" (write binary), "ab" (append binary)
 * @return Stream handle or NULL on error
 * 
 * Example:
 *   AriaStream* stream = aria_open_file("data.txt", "r");
 *   if (stream) {
 *       // Use stream
 *       aria_stream_close(stream);
 *   }
 */
AriaStream* aria_open_file(const char* path, const char* mode);

/**
 * Close file stream
 * 
 * @param stream Stream handle
 */
void aria_stream_close(AriaStream* stream);

/**
 * Read line from stream
 * 
 * @param stream Stream handle
 * @return Line string (caller must free), or NULL on EOF/error
 * 
 * Example:
 *   char* line = aria_stream_read_line(stream);
 *   while (line) {
 *       printf("%s\n", line);
 *       free(line);
 *       line = aria_stream_read_line(stream);
 *   }
 */
char* aria_stream_read_line(AriaStream* stream);

/**
 * Write string to stream
 * 
 * @param stream Stream handle
 * @param str String to write
 * @return Number of bytes written, or -1 on error
 */
int64_t aria_stream_write(AriaStream* stream, const char* str);

/**
 * Write bytes to stream
 * 
 * @param stream Stream handle
 * @param data Buffer to write
 * @param size Size of buffer
 * @return Number of bytes written, or -1 on error
 */
int64_t aria_stream_write_bytes(AriaStream* stream, const void* data, size_t size);

/**
 * Read bytes from stream
 * 
 * @param stream Stream handle
 * @param buffer Buffer to read into
 * @param size Maximum bytes to read
 * @return Number of bytes read, or -1 on error
 */
int64_t aria_stream_read_bytes(AriaStream* stream, void* buffer, size_t size);

/**
 * Check if stream is at EOF
 * 
 * @param stream Stream handle
 * @return true if at EOF, false otherwise
 */
bool aria_stream_eof(AriaStream* stream);

/**
 * Flush stream buffer
 * 
 * @param stream Stream handle
 * @return 0 on success, -1 on error
 */
int aria_stream_flush(AriaStream* stream);

/**
 * Seek to position in stream
 * 
 * @param stream Stream handle
 * @param offset Byte offset
 * @param whence SEEK_SET (0), SEEK_CUR (1), or SEEK_END (2)
 * @return 0 on success, -1 on error
 */
int aria_stream_seek(AriaStream* stream, int64_t offset, int whence);

/**
 * Get current position in stream
 * 
 * @param stream Stream handle
 * @return Current byte position, or -1 on error
 */
int64_t aria_stream_tell(AriaStream* stream);

// ============================================================================
// Structured File Parsing
// ============================================================================

/**
 * JSON Value Type
 */
typedef enum {
    ARIA_JSON_NULL,
    ARIA_JSON_BOOL,
    ARIA_JSON_NUMBER,
    ARIA_JSON_STRING,
    ARIA_JSON_ARRAY,
    ARIA_JSON_OBJECT
} AriaJsonType;

/**
 * JSON Value
 * 
 * Simplified JSON representation for basic parsing.
 */
typedef struct AriaJsonValue {
    AriaJsonType type;
    union {
        bool bool_val;
        double number_val;
        char* string_val;
        struct {
            struct AriaJsonValue** items;
            size_t count;
        } array_val;
        struct {
            char** keys;
            struct AriaJsonValue** values;
            size_t count;
        } object_val;
    } data;
} AriaJsonValue;

/**
 * Read and parse JSON file
 * 
 * @param path File path
 * @return Result with AriaJsonValue* or error
 * 
 * Example:
 *   AriaResult* r = aria_read_json("config.json");
 *   if (r->err == NULL) {
 *       AriaJsonValue* json = (AriaJsonValue*)r->val;
 *       // Use json
 *       aria_json_free(json);
 *   }
 *   aria_result_free(r);
 */
AriaResult* aria_read_json(const char* path);

/**
 * Parse JSON string
 * 
 * @param json_str JSON string
 * @return Result with AriaJsonValue* or error
 */
AriaResult* aria_parse_json(const char* json_str);

/**
 * Free JSON value
 * 
 * @param value JSON value to free
 */
void aria_json_free(AriaJsonValue* value);

/**
 * Get value from JSON object by key
 * 
 * @param obj JSON object
 * @param key Key name
 * @return JSON value or NULL if not found
 */
AriaJsonValue* aria_json_get(AriaJsonValue* obj, const char* key);

/**
 * Get string from JSON value
 * 
 * @param value JSON value
 * @param default_val Default value if not a string
 * @return String value or default
 */
const char* aria_json_as_string(AriaJsonValue* value, const char* default_val);

/**
 * Get number from JSON value
 * 
 * @param value JSON value
 * @param default_val Default value if not a number
 * @return Number value or default
 */
double aria_json_as_number(AriaJsonValue* value, double default_val);

/**
 * Get boolean from JSON value
 * 
 * @param value JSON value
 * @param default_val Default value if not a boolean
 * @return Boolean value or default
 */
bool aria_json_as_bool(AriaJsonValue* value, bool default_val);

/**
 * CSV Row
 */
typedef struct AriaCsvRow {
    char** fields;      // Array of field strings
    size_t field_count; // Number of fields
} AriaCsvRow;

/**
 * CSV Data
 */
typedef struct AriaCsvData {
    AriaCsvRow* rows;   // Array of rows
    size_t row_count;   // Number of rows
} AriaCsvData;

/**
 * Read and parse CSV file
 * 
 * @param path File path
 * @return Result with AriaCsvData* or error
 * 
 * Example:
 *   AriaResult* r = aria_read_csv("data.csv");
 *   if (r->err == NULL) {
 *       AriaCsvData* csv = (AriaCsvData*)r->val;
 *       for (size_t i = 0; i < csv->row_count; i++) {
 *           for (size_t j = 0; j < csv->rows[i].field_count; j++) {
 *               printf("%s ", csv->rows[i].fields[j]);
 *           }
 *           printf("\n");
 *       }
 *       aria_csv_free(csv);
 *   }
 *   aria_result_free(r);
 */
AriaResult* aria_read_csv(const char* path);

/**
 * Parse CSV string
 * 
 * @param csv_str CSV string
 * @return Result with AriaCsvData* or error
 */
AriaResult* aria_parse_csv(const char* csv_str);

/**
 * Free CSV data
 * 
 * @param csv CSV data to free
 */
void aria_csv_free(AriaCsvData* csv);

// ============================================================================
// Path Operations
// ============================================================================

/**
 * Get absolute path
 * 
 * @param path Relative or absolute path
 * @return Absolute path (caller must free), or NULL on error
 */
char* aria_path_absolute(const char* path);

/**
 * Get directory name from path
 * 
 * @param path File path
 * @return Directory name (caller must free), or NULL on error
 */
char* aria_path_dirname(const char* path);

/**
 * Get base name from path
 * 
 * @param path File path
 * @return Base name (caller must free), or NULL on error
 */
char* aria_path_basename(const char* path);

/**
 * Join path components
 * 
 * @param dir Directory path
 * @param name File name
 * @return Joined path (caller must free), or NULL on error
 */
char* aria_path_join(const char* dir, const char* name);

/**
 * Check if path is absolute
 * 
 * @param path File path
 * @return true if absolute, false otherwise
 */
bool aria_path_is_absolute(const char* path);

#ifdef __cplusplus
}
#endif

#endif // ARIA_IO_H
