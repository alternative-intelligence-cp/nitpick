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
 *   AriaResult* result = npk_read_file("config.txt");
 *   if (result->err == NULL) {
 *       printf("Content: %s\n", (char*)result->val);
 *       npk_result_free(result);
 *   }
 *   
 *   // Stream operations
 *   AriaStream* stream = npk_open_file("data.txt", "r");
 *   if (stream) {
 *       char* line = npk_stream_read_line(stream);
 *       while (line) {
 *           process(line);
 *           free(line);
 *           line = npk_stream_read_line(stream);
 *       }
 *       npk_stream_close(stream);
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
AriaResult* npk_result_ok(void* value, size_t size);

/**
 * Create error result
 * 
 * @param error Error message (copied internally)
 * @return Result with err=message, val=NULL
 */
AriaResult* npk_result_err(const char* error);

/**
 * Free result
 * 
 * Frees both error message and value.
 * 
 * @param result Result to free
 */
void npk_result_free(AriaResult* result);

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
 *   AriaResult* r = npk_read_file("config.txt");
 *   if (r->err == NULL) {
 *       printf("%s\n", (char*)r->val);
 *   }
 *   npk_result_free(r);
 */
AriaResult* npk_read_file(const char* path);

/**
 * Write string to file
 * 
 * @param path File path
 * @param content String content to write
 * @return Result with NULL val on success, error on failure
 * 
 * Example:
 *   AriaResult* r = npk_write_file("output.txt", "Hello world");
 *   if (r->err != NULL) {
 *       fprintf(stderr, "Error: %s\n", r->err);
 *   }
 *   npk_result_free(r);
 */
AriaResult* npk_write_file(const char* path, const char* content);

/**
 * Read binary file into buffer
 * 
 * @param path File path
 * @param size Output parameter for buffer size
 * @return Result with buffer or error
 * 
 * Example:
 *   size_t size;
 *   AriaResult* r = npk_read_binary(path, &size);
 *   if (r->err == NULL) {
 *       uint8_t* data = (uint8_t*)r->val;
 *       process(data, size);
 *   }
 *   npk_result_free(r);
 */
AriaResult* npk_read_binary(const char* path, size_t* size);

/**
 * Write binary buffer to file
 * 
 * @param path File path
 * @param data Buffer to write
 * @param size Size of buffer
 * @return Result with NULL val on success, error on failure
 */
AriaResult* npk_write_binary(const char* path, const void* data, size_t size);

/**
 * Check if file exists
 * 
 * @param path File path
 * @return true if file exists, false otherwise
 */
bool npk_file_exists(const char* path);

/**
 * Get file size
 * 
 * @param path File path
 * @return File size in bytes, or -1 on error
 */
int64_t npk_file_size(const char* path);

/**
 * Delete file
 * 
 * @param path File path
 * @return Result with NULL val on success, error on failure
 */
AriaResult* npk_delete_file(const char* path);

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
 *   AriaStream* stream = npk_open_file("data.txt", "r");
 *   if (stream) {
 *       // Use stream
 *       npk_stream_close(stream);
 *   }
 */
AriaStream* npk_open_file(const char* path, const char* mode);

/**
 * Close file stream
 * 
 * @param stream Stream handle
 */
void npk_stream_close(AriaStream* stream);

/**
 * Read line from stream
 * 
 * @param stream Stream handle
 * @return Line string (caller must free), or NULL on EOF/error
 * 
 * Example:
 *   char* line = npk_stream_read_line(stream);
 *   while (line) {
 *       printf("%s\n", line);
 *       free(line);
 *       line = npk_stream_read_line(stream);
 *   }
 */
char* npk_stream_read_line(AriaStream* stream);

/**
 * Write string to stream
 * 
 * @param stream Stream handle
 * @param str String to write
 * @return Number of bytes written, or -1 on error
 */
int64_t npk_stream_write(AriaStream* stream, const char* str);

/**
 * Write bytes to stream
 * 
 * @param stream Stream handle
 * @param data Buffer to write
 * @param size Size of buffer
 * @return Number of bytes written, or -1 on error
 */
int64_t npk_stream_write_bytes(AriaStream* stream, const void* data, size_t size);

/**
 * Read bytes from stream
 * 
 * @param stream Stream handle
 * @param buffer Buffer to read into
 * @param size Maximum bytes to read
 * @return Number of bytes read, or -1 on error
 */
int64_t npk_stream_read_bytes(AriaStream* stream, void* buffer, size_t size);

/**
 * Check if stream is at EOF
 * 
 * @param stream Stream handle
 * @return true if at EOF, false otherwise
 */
bool npk_stream_eof(AriaStream* stream);

/**
 * Flush stream buffer
 * 
 * @param stream Stream handle
 * @return 0 on success, -1 on error
 */
int npk_stream_flush(AriaStream* stream);

/**
 * Seek to position in stream
 * 
 * @param stream Stream handle
 * @param offset Byte offset
 * @param whence SEEK_SET (0), SEEK_CUR (1), or SEEK_END (2)
 * @return 0 on success, -1 on error
 */
int npk_stream_seek(AriaStream* stream, int64_t offset, int whence);

/**
 * Get current position in stream
 * 
 * @param stream Stream handle
 * @return Current byte position, or -1 on error
 */
int64_t npk_stream_tell(AriaStream* stream);

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
 *   AriaResult* r = npk_read_json("config.json");
 *   if (r->err == NULL) {
 *       AriaJsonValue* json = (AriaJsonValue*)r->val;
 *       // Use json
 *       npk_json_free(json);
 *   }
 *   npk_result_free(r);
 */
AriaResult* npk_read_json(const char* path);

/**
 * Parse JSON string
 * 
 * @param json_str JSON string
 * @return Result with AriaJsonValue* or error
 */
AriaResult* npk_parse_json(const char* json_str);

// npk_json_free is internal to the runtime (static in io.cpp)
// User packages should define their own cleanup functions

/**
 * Get value from JSON object by key
 * 
 * @param obj JSON object
 * @param key Key name
 * @return JSON value or NULL if not found
 */
AriaJsonValue* npk_json_get(AriaJsonValue* obj, const char* key);

/**
 * Get string from JSON value
 * 
 * @param value JSON value
 * @param default_val Default value if not a string
 * @return String value or default
 */
const char* npk_json_as_string(AriaJsonValue* value, const char* default_val);

/**
 * Get number from JSON value
 * 
 * @param value JSON value
 * @param default_val Default value if not a number
 * @return Number value or default
 */
double npk_json_as_number(AriaJsonValue* value, double default_val);

/**
 * Get boolean from JSON value
 * 
 * @param value JSON value
 * @param default_val Default value if not a boolean
 * @return Boolean value or default
 */
bool npk_json_as_bool(AriaJsonValue* value, bool default_val);

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
 *   AriaResult* r = npk_read_csv("data.csv");
 *   if (r->err == NULL) {
 *       AriaCsvData* csv = (AriaCsvData*)r->val;
 *       for (size_t i = 0; i < csv->row_count; i++) {
 *           for (size_t j = 0; j < csv->rows[i].field_count; j++) {
 *               printf("%s ", csv->rows[i].fields[j]);
 *           }
 *           printf("\n");
 *       }
 *       npk_csv_free(csv);
 *   }
 *   npk_result_free(r);
 */
AriaResult* npk_read_csv(const char* path);

/**
 * Parse CSV string
 * 
 * @param csv_str CSV string
 * @return Result with AriaCsvData* or error
 */
AriaResult* npk_parse_csv(const char* csv_str);

// npk_csv_free is internal to the runtime (static in io.cpp)
// User packages should define their own cleanup functions

// ============================================================================
// Path Operations
// ============================================================================

/**
 * Get absolute path
 * 
 * @param path Relative or absolute path
 * @return Absolute path (caller must free), or NULL on error
 */
char* npk_path_absolute(const char* path);

/**
 * Get directory name from path
 * 
 * @param path File path
 * @return Directory name (caller must free), or NULL on error
 */
char* npk_path_dirname(const char* path);

/**
 * Get base name from path
 * 
 * @param path File path
 * @return Base name (caller must free), or NULL on error
 */
char* npk_path_basename(const char* path);

/**
 * Join path components
 * 
 * @param dir Directory path
 * @param name File name
 * @return Joined path (caller must free), or NULL on error
 */
char* npk_path_join(const char* dir, const char* name);

/**
 * Check if path is absolute
 * 
 * @param path File path
 * @return true if absolute, false otherwise
 */
bool npk_path_is_absolute(const char* path);

// ============================================================================
// Result-Based File I/O Functions for Aria Builtins (Phase 4.2)
// ============================================================================

#include "runtime/result.h"

/**
 * Read file and return result<string>
 * 
 * @param path File path (C string)
 * @return AriaResultPtr with AriaString* on success, error on failure
 */
AriaResultPtr npk_read_file_result(const char* path);

/**
 * Write file and return result<void>
 * 
 * @param path File path (C string)
 * @param content Content to write (AriaString*)
 * @return AriaResultVoid with success/error status
 */
AriaResultVoid npk_write_file_result(const char* path, const void* content);

/**
 * Delete file and return result<void>
 * 
 * @param path File path (C string)
 * @return AriaResultVoid with success/error status
 */
AriaResultVoid npk_delete_file_result(const char* path);

/**
 * Check if file exists and return result<bool>
 * 
 * @param path File path (C string)
 * @return AriaResultBool with exists status or error
 */
AriaResultBool npk_file_exists_result(const char* path);

/**
 * Get file size and return result<int64>
 * 
 * @param path File path (C string)
 * @return AriaResultI64 with file size in bytes or error
 */
AriaResultI64 npk_file_size_result(const char* path);

/**
 * Convert path to absolute and return result<string>
 * 
 * @param path File path (C string)
 * @return AriaResultPtr with AriaString* on success, error on failure
 */
AriaResultPtr npk_path_absolute_result(const char* path);

/**
 * Get directory portion of path and return result<string>
 * 
 * @param path File path (C string)
 * @return AriaResultPtr with AriaString* on success, error on failure
 */
AriaResultPtr npk_path_dirname_result(const char* path);

/**
 * Get filename portion of path and return result<string>
 * 
 * @param path File path (C string)
 * @return AriaResultPtr with AriaString* on success, error on failure
 */
AriaResultPtr npk_path_basename_result(const char* path);

/**
 * Join path components and return result<string>
 * 
 * @param dir Directory path (C string)
 * @param name File/directory name (C string)
 * @return AriaResultPtr with AriaString* on success, error on failure
 */
AriaResultPtr npk_path_join_result(const char* dir, const char* name);

/**
 * Check if path is absolute and return result<bool>
 * 
 * @param path File path (C string)
 * @return AriaResultBool with is_absolute status or error
 */
AriaResultBool npk_path_is_absolute_result(const char* path);

// ============================================================================
// Simplified Wrappers for Aria Builtins (DEPRECATED - will be removed)
// ============================================================================

/**
 * Simplified readFile for Aria - returns AriaString directly
 * Returns empty string on error (check length == 0)
 * 
 * @param path File path (C string)
 * @return Pointer to AriaString with file contents (or empty on error)
 */
struct AriaString;  // Forward declaration
struct AriaString* npk_read_file_simple(const char* path);

/**
 * Simplified writeFile for Aria - returns int64
 * 
 * @param path File path (C string)
 * @param content Content to write (C string)
 * @return 0 on success, -1 on error
 */
int64_t npk_write_file_simple(const char* path, const char* content);

/**
 * Simplified deleteFile for Aria - returns int64
 * 
 * @param path File path (C string)
 * @return 0 on success, -1 on error
 */
int64_t npk_delete_file_simple(const char* path);

/**
 * Path operation wrappers that return AriaString*
 */
AriaString* npk_path_absolute_string(const char* path);
AriaString* npk_path_dirname_string(const char* path);
AriaString* npk_path_basename_string(const char* path);
AriaString* npk_path_join_string(const char* dir, const char* name);

/**
 * Simplified stream read_line for Aria - returns AriaString*
 * Reads a line from the stream, converting the raw char* to AriaString.
 * Returns empty AriaString on EOF/error (never NULL).
 *
 * @param stream Stream handle
 * @return AriaString with line content (empty on EOF)
 */
AriaString* npk_stream_read_line_string(AriaStream* stream);

// ============================================================================
// UFCS-Compatible File Aliases (for File.open(), stream.read(), etc.)
// ============================================================================

/**
 * Static method: File.open(path, mode) -> AriaStream*
 * Opens a file stream for reading or writing.
 */
AriaStream* File_open(const char* path, const char* mode);

/**
 * Instance method: stream.read_line() -> char*
 * Reads a line from the stream (caller must free).
 */
char* File_read_line(AriaStream* stream);

/**
 * Instance method: stream.read_bytes(buffer, size) -> int64
 * Reads bytes from the stream into a buffer.
 */
int64_t File_read_bytes(AriaStream* stream, void* buffer, size_t size);

/**
 * Instance method: stream.write(str) -> int64
 * Writes a string to the stream.
 */
int64_t File_write(AriaStream* stream, const char* str);

/**
 * Instance method: stream.write_bytes(data, size) -> int64
 * Writes bytes to the stream.
 */
int64_t File_write_bytes(AriaStream* stream, const void* data, size_t size);

/**
 * Instance method: stream.close() -> void
 * Closes the file stream.
 */
void File_close(AriaStream* stream);

/**
 * Instance method: stream.eof() -> bool
 * Returns true if at end of file.
 */
bool File_eof(AriaStream* stream);

/**
 * Instance method: stream.flush() -> int
 * Flushes the stream buffer.
 */
int File_flush(AriaStream* stream);

/**
 * Instance method: stream.seek(offset, whence) -> int
 * Seeks to a position in the stream.
 */
int File_seek(AriaStream* stream, int64_t offset, int whence);

/**
 * Instance method: stream.tell() -> int64
 * Returns the current position in the stream.
 */
int64_t File_tell(AriaStream* stream);

/**
 * Static method: File.read(path) -> AriaString*
 * Reads entire file contents into a string.
 */
AriaString* File_read(const char* path);

/**
 * Static method: File.write_all(path, content) -> int64
 * Writes a string to a file (creates/overwrites).
 */
int64_t File_write_all(const char* path, const char* content);

/**
 * Static method: File.delete(path) -> int64
 * Deletes a file.
 */
int64_t File_delete(const char* path);

/**
 * Static method: File.exists(path) -> bool
 * Returns true if file exists.
 */
bool File_exists(const char* path);

/**
 * Static method: File.size(path) -> int64
 * Returns file size in bytes.
 */
int64_t File_size(const char* path);

#ifdef __cplusplus
}
#endif

#endif // ARIA_IO_H
