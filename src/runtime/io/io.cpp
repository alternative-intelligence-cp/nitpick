/**
 * Aria Runtime - File I/O Implementation
 * 
 * Cross-platform file I/O with result-based error handling.
 */

#include "runtime/io.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#define PATH_SEPARATOR '\\'
#define stat _stat
#define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
#define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#else
#include <unistd.h>
#include <libgen.h>
#define PATH_SEPARATOR '/'
#endif

// ============================================================================
// Result Type Implementation
// ============================================================================

AriaResult* aria_result_ok(void* value, size_t size) {
    AriaResult* result = (AriaResult*)malloc(sizeof(AriaResult));
    if (!result) return NULL;
    
    result->err = NULL;
    result->val = value;
    result->val_size = size;
    return result;
}

AriaResult* aria_result_err(const char* error) {
    AriaResult* result = (AriaResult*)malloc(sizeof(AriaResult));
    if (!result) return NULL;
    
    result->err = strdup(error);
    result->val = NULL;
    result->val_size = 0;
    return result;
}

void aria_result_free(AriaResult* result) {
    if (!result) return;
    
    if (result->err) {
        free(result->err);
    }
    if (result->val) {
        free(result->val);
    }
    free(result);
}

// ============================================================================
// Stream Type Implementation
// ============================================================================

struct AriaStream {
    FILE* file;
    char* path;
    char* mode;
    bool is_eof;
};

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * Get error message from errno
 */
static char* get_error_message(const char* prefix, const char* path) {
    const char* err_msg = strerror(errno);
    size_t len = strlen(prefix) + strlen(path) + strlen(err_msg) + 10;
    char* msg = (char*)malloc(len);
    if (!msg) return strdup("Out of memory");
    
    snprintf(msg, len, "%s '%s': %s", prefix, path, err_msg);
    return msg;
}

// ============================================================================
// Simple File Operations
// ============================================================================

AriaResult* aria_read_file(const char* path) {
    if (!path) {
        return aria_result_err("Path is NULL");
    }
    
    FILE* file = fopen(path, "rb");
    if (!file) {
        char* msg = get_error_message("Failed to open file", path);
        AriaResult* r = aria_result_err(msg);
        free(msg);
        return r;
    }
    
    // Get file size
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    if (size < 0) {
        fclose(file);
        return aria_result_err("Failed to get file size");
    }
    
    // Allocate buffer (+ 1 for null terminator)
    char* buffer = (char*)malloc(size + 1);
    if (!buffer) {
        fclose(file);
        return aria_result_err("Out of memory");
    }
    
    // Read file
    size_t bytes_read = fread(buffer, 1, size, file);
    fclose(file);
    
    if (bytes_read != (size_t)size) {
        free(buffer);
        return aria_result_err("Failed to read entire file");
    }
    
    // Null-terminate for string usage
    buffer[size] = '\0';
    
    return aria_result_ok(buffer, size);
}

AriaResult* aria_write_file(const char* path, const char* content) {
    if (!path) {
        return aria_result_err("Path is NULL");
    }
    if (!content) {
        return aria_result_err("Content is NULL");
    }
    
    FILE* file = fopen(path, "wb");
    if (!file) {
        char* msg = get_error_message("Failed to open file for writing", path);
        AriaResult* r = aria_result_err(msg);
        free(msg);
        return r;
    }
    
    size_t len = strlen(content);
    size_t written = fwrite(content, 1, len, file);
    fclose(file);
    
    if (written != len) {
        return aria_result_err("Failed to write entire file");
    }
    
    return aria_result_ok(NULL, 0);
}

AriaResult* aria_read_binary(const char* path, size_t* size) {
    if (!path) {
        return aria_result_err("Path is NULL");
    }
    
    FILE* file = fopen(path, "rb");
    if (!file) {
        char* msg = get_error_message("Failed to open file", path);
        AriaResult* r = aria_result_err(msg);
        free(msg);
        return r;
    }
    
    // Get file size
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    if (file_size < 0) {
        fclose(file);
        return aria_result_err("Failed to get file size");
    }
    
    // Allocate buffer
    uint8_t* buffer = (uint8_t*)malloc(file_size);
    if (!buffer) {
        fclose(file);
        return aria_result_err("Out of memory");
    }
    
    // Read file
    size_t bytes_read = fread(buffer, 1, file_size, file);
    fclose(file);
    
    if (bytes_read != (size_t)file_size) {
        free(buffer);
        return aria_result_err("Failed to read entire file");
    }
    
    if (size) {
        *size = file_size;
    }
    
    return aria_result_ok(buffer, file_size);
}

AriaResult* aria_write_binary(const char* path, const void* data, size_t size) {
    if (!path) {
        return aria_result_err("Path is NULL");
    }
    if (!data) {
        return aria_result_err("Data is NULL");
    }
    
    FILE* file = fopen(path, "wb");
    if (!file) {
        char* msg = get_error_message("Failed to open file for writing", path);
        AriaResult* r = aria_result_err(msg);
        free(msg);
        return r;
    }
    
    size_t written = fwrite(data, 1, size, file);
    fclose(file);
    
    if (written != size) {
        return aria_result_err("Failed to write entire file");
    }
    
    return aria_result_ok(NULL, 0);
}

bool aria_file_exists(const char* path) {
    if (!path) return false;
    
    struct stat st;
    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

int64_t aria_file_size(const char* path) {
    if (!path) return -1;
    
    struct stat st;
    if (stat(path, &st) != 0) {
        return -1;
    }
    
    if (!S_ISREG(st.st_mode)) {
        return -1;
    }
    
    return st.st_size;
}

AriaResult* aria_delete_file(const char* path) {
    if (!path) {
        return aria_result_err("Path is NULL");
    }
    
    if (remove(path) != 0) {
        char* msg = get_error_message("Failed to delete file", path);
        AriaResult* r = aria_result_err(msg);
        free(msg);
        return r;
    }
    
    return aria_result_ok(NULL, 0);
}

// ============================================================================
// Stream Operations
// ============================================================================

AriaStream* aria_open_file(const char* path, const char* mode) {
    if (!path || !mode) return NULL;
    
    FILE* file = fopen(path, mode);
    if (!file) return NULL;
    
    AriaStream* stream = (AriaStream*)malloc(sizeof(AriaStream));
    if (!stream) {
        fclose(file);
        return NULL;
    }
    
    stream->file = file;
    stream->path = strdup(path);
    stream->mode = strdup(mode);
    stream->is_eof = false;
    
    return stream;
}

void aria_stream_close(AriaStream* stream) {
    if (!stream) return;
    
    if (stream->file) {
        fclose(stream->file);
    }
    if (stream->path) {
        free(stream->path);
    }
    if (stream->mode) {
        free(stream->mode);
    }
    free(stream);
}

char* aria_stream_read_line(AriaStream* stream) {
    if (!stream || !stream->file || stream->is_eof) return NULL;
    
    char* line = NULL;
    size_t capacity = 0;
    size_t length = 0;
    
    int c;
    while ((c = fgetc(stream->file)) != EOF) {
        // Resize buffer if needed
        if (length + 1 >= capacity) {
            size_t new_capacity = capacity == 0 ? 128 : capacity * 2;
            char* new_line = (char*)realloc(line, new_capacity);
            if (!new_line) {
                free(line);
                return NULL;
            }
            line = new_line;
            capacity = new_capacity;
        }
        
        // Add character
        line[length++] = (char)c;
        
        // Stop at newline
        if (c == '\n') break;
    }
    
    // Check for EOF
    if (c == EOF && length == 0) {
        stream->is_eof = true;
        return NULL;
    }
    
    // Null-terminate
    if (length + 1 >= capacity) {
        char* new_line = (char*)realloc(line, length + 1);
        if (!new_line) {
            free(line);
            return NULL;
        }
        line = new_line;
    }
    line[length] = '\0';
    
    return line;
}

int64_t aria_stream_write(AriaStream* stream, const char* str) {
    if (!stream || !stream->file || !str) return -1;
    
    size_t len = strlen(str);
    size_t written = fwrite(str, 1, len, stream->file);
    
    if (written != len) return -1;
    return written;
}

int64_t aria_stream_write_bytes(AriaStream* stream, const void* data, size_t size) {
    if (!stream || !stream->file || !data) return -1;
    
    size_t written = fwrite(data, 1, size, stream->file);
    if (written != size) return -1;
    return written;
}

int64_t aria_stream_read_bytes(AriaStream* stream, void* buffer, size_t size) {
    if (!stream || !stream->file || !buffer) return -1;
    
    size_t bytes_read = fread(buffer, 1, size, stream->file);
    if (bytes_read == 0 && ferror(stream->file)) {
        return -1;
    }
    
    if (bytes_read == 0 && feof(stream->file)) {
        stream->is_eof = true;
    }
    
    return bytes_read;
}

bool aria_stream_eof(AriaStream* stream) {
    if (!stream || !stream->file) return true;
    return stream->is_eof || feof(stream->file);
}

int aria_stream_flush(AriaStream* stream) {
    if (!stream || !stream->file) return -1;
    return fflush(stream->file);
}

int aria_stream_seek(AriaStream* stream, int64_t offset, int whence) {
    if (!stream || !stream->file) return -1;
    
    stream->is_eof = false;
    return fseek(stream->file, offset, whence);
}

int64_t aria_stream_tell(AriaStream* stream) {
    if (!stream || !stream->file) return -1;
    return ftell(stream->file);
}

// ============================================================================
// Structured File Parsing (Simplified Implementations)
// ============================================================================

// JSON parsing - simplified implementation
// For production, would use a proper JSON library like cJSON

void aria_json_free(AriaJsonValue* value) {
    if (!value) return;
    
    switch (value->type) {
        case ARIA_JSON_STRING:
            if (value->data.string_val) {
                free(value->data.string_val);
            }
            break;
        case ARIA_JSON_ARRAY:
            for (size_t i = 0; i < value->data.array_val.count; i++) {
                aria_json_free(value->data.array_val.items[i]);
            }
            free(value->data.array_val.items);
            break;
        case ARIA_JSON_OBJECT:
            for (size_t i = 0; i < value->data.object_val.count; i++) {
                free(value->data.object_val.keys[i]);
                aria_json_free(value->data.object_val.values[i]);
            }
            free(value->data.object_val.keys);
            free(value->data.object_val.values);
            break;
        default:
            break;
    }
    
    free(value);
}

AriaResult* aria_read_json(const char* path) {
    // Read file first
    AriaResult* file_result = aria_read_file(path);
    if (file_result->err != NULL) {
        return file_result;
    }
    
    // Parse JSON (stub - would use real parser)
    AriaResult* json_result = aria_parse_json((const char*)file_result->val);
    aria_result_free(file_result);
    
    return json_result;
}

AriaResult* aria_parse_json(const char* json_str) {
    // Stub implementation - returns empty object
    // In production, would use a proper JSON parser
    (void)json_str;
    
    AriaJsonValue* value = (AriaJsonValue*)malloc(sizeof(AriaJsonValue));
    if (!value) {
        return aria_result_err("Out of memory");
    }
    
    value->type = ARIA_JSON_OBJECT;
    value->data.object_val.keys = NULL;
    value->data.object_val.values = NULL;
    value->data.object_val.count = 0;
    
    return aria_result_ok(value, sizeof(AriaJsonValue));
}

AriaJsonValue* aria_json_get(AriaJsonValue* obj, const char* key) {
    if (!obj || obj->type != ARIA_JSON_OBJECT || !key) {
        return NULL;
    }
    
    for (size_t i = 0; i < obj->data.object_val.count; i++) {
        if (strcmp(obj->data.object_val.keys[i], key) == 0) {
            return obj->data.object_val.values[i];
        }
    }
    
    return NULL;
}

const char* aria_json_as_string(AriaJsonValue* value, const char* default_val) {
    if (!value || value->type != ARIA_JSON_STRING) {
        return default_val;
    }
    return value->data.string_val;
}

double aria_json_as_number(AriaJsonValue* value, double default_val) {
    if (!value || value->type != ARIA_JSON_NUMBER) {
        return default_val;
    }
    return value->data.number_val;
}

bool aria_json_as_bool(AriaJsonValue* value, bool default_val) {
    if (!value || value->type != ARIA_JSON_BOOL) {
        return default_val;
    }
    return value->data.bool_val;
}

// CSV parsing - basic implementation

void aria_csv_free(AriaCsvData* csv) {
    if (!csv) return;
    
    for (size_t i = 0; i < csv->row_count; i++) {
        for (size_t j = 0; j < csv->rows[i].field_count; j++) {
            free(csv->rows[i].fields[j]);
        }
        free(csv->rows[i].fields);
    }
    free(csv->rows);
    free(csv);
}

AriaResult* aria_read_csv(const char* path) {
    // Read file first
    AriaResult* file_result = aria_read_file(path);
    if (file_result->err != NULL) {
        return file_result;
    }
    
    // Parse CSV
    AriaResult* csv_result = aria_parse_csv((const char*)file_result->val);
    aria_result_free(file_result);
    
    return csv_result;
}

AriaResult* aria_parse_csv(const char* csv_str) {
    if (!csv_str) {
        return aria_result_err("CSV string is NULL");
    }
    
    AriaCsvData* csv = (AriaCsvData*)malloc(sizeof(AriaCsvData));
    if (!csv) {
        return aria_result_err("Out of memory");
    }
    
    csv->rows = NULL;
    csv->row_count = 0;
    
    // Simple CSV parser (basic implementation)
    const char* ptr = csv_str;
    size_t row_capacity = 0;
    
    while (*ptr) {
        // Allocate row if needed
        if (csv->row_count >= row_capacity) {
            size_t new_capacity = row_capacity == 0 ? 16 : row_capacity * 2;
            AriaCsvRow* new_rows = (AriaCsvRow*)realloc(csv->rows, new_capacity * sizeof(AriaCsvRow));
            if (!new_rows) {
                aria_csv_free(csv);
                return aria_result_err("Out of memory");
            }
            csv->rows = new_rows;
            row_capacity = new_capacity;
        }
        
        // Parse row
        AriaCsvRow* row = &csv->rows[csv->row_count];
        row->fields = NULL;
        row->field_count = 0;
        size_t field_capacity = 0;
        
        while (*ptr && *ptr != '\n') {
            // Allocate field if needed
            if (row->field_count >= field_capacity) {
                size_t new_capacity = field_capacity == 0 ? 8 : field_capacity * 2;
                char** new_fields = (char**)realloc(row->fields, new_capacity * sizeof(char*));
                if (!new_fields) {
                    aria_csv_free(csv);
                    return aria_result_err("Out of memory");
                }
                row->fields = new_fields;
                field_capacity = new_capacity;
            }
            
            // Parse field
            const char* field_start = ptr;
            while (*ptr && *ptr != ',' && *ptr != '\n') {
                ptr++;
            }
            
            size_t field_len = ptr - field_start;
            char* field = (char*)malloc(field_len + 1);
            if (!field) {
                aria_csv_free(csv);
                return aria_result_err("Out of memory");
            }
            memcpy(field, field_start, field_len);
            field[field_len] = '\0';
            
            row->fields[row->field_count++] = field;
            
            if (*ptr == ',') ptr++;
        }
        
        csv->row_count++;
        
        if (*ptr == '\n') ptr++;
    }
    
    return aria_result_ok(csv, sizeof(AriaCsvData));
}

// ============================================================================
// Path Operations
// ============================================================================

char* aria_path_absolute(const char* path) {
    if (!path) return NULL;
    
#ifdef _WIN32
    char abs_path[MAX_PATH];
    if (_fullpath(abs_path, path, MAX_PATH) == NULL) {
        return NULL;
    }
    return strdup(abs_path);
#else
    char* abs_path = realpath(path, NULL);
    return abs_path;
#endif
}

char* aria_path_dirname(const char* path) {
    if (!path) return NULL;
    
    char* path_copy = strdup(path);
    if (!path_copy) return NULL;
    
#ifdef _WIN32
    // Windows path handling
    char* last_sep = strrchr(path_copy, '\\');
    if (!last_sep) {
        last_sep = strrchr(path_copy, '/');
    }
    if (last_sep) {
        *last_sep = '\0';
        char* result = strdup(path_copy);
        free(path_copy);
        return result;
    } else {
        free(path_copy);
        return strdup(".");
    }
#else
    char* dir = dirname(path_copy);
    char* result = strdup(dir);
    free(path_copy);
    return result;
#endif
}

char* aria_path_basename(const char* path) {
    if (!path) return NULL;
    
    char* path_copy = strdup(path);
    if (!path_copy) return NULL;
    
#ifdef _WIN32
    // Windows path handling
    char* last_sep = strrchr(path_copy, '\\');
    if (!last_sep) {
        last_sep = strrchr(path_copy, '/');
    }
    if (last_sep) {
        char* result = strdup(last_sep + 1);
        free(path_copy);
        return result;
    } else {
        return path_copy;
    }
#else
    char* base = basename(path_copy);
    char* result = strdup(base);
    free(path_copy);
    return result;
#endif
}

char* aria_path_join(const char* dir, const char* name) {
    if (!dir || !name) return NULL;
    
    size_t dir_len = strlen(dir);
    size_t name_len = strlen(name);
    size_t total_len = dir_len + name_len + 2; // +2 for separator and null terminator
    
    char* result = (char*)malloc(total_len);
    if (!result) return NULL;
    
    snprintf(result, total_len, "%s%c%s", dir, PATH_SEPARATOR, name);
    return result;
}

bool aria_path_is_absolute(const char* path) {
    if (!path || path[0] == '\0') return false;
    
#ifdef _WIN32
    // Windows: Check for drive letter (C:\) or UNC path (\\)
    if ((path[0] >= 'A' && path[0] <= 'Z') || (path[0] >= 'a' && path[0] <= 'z')) {
        if (path[1] == ':' && (path[2] == '\\' || path[2] == '/')) {
            return true;
        }
    }
    if (path[0] == '\\' && path[1] == '\\') {
        return true;
    }
    return false;
#else
    // Unix: Check for leading /
    return path[0] == '/';
#endif
}
