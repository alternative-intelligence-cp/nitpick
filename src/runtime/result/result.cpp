/**
 * Aria Standard Library - Result Type Implementation
 * 
 * Provides runtime support for Aria's result<T> error handling.
 */

#include "runtime/result.h"
#include "runtime/gc.h"
#include "runtime/stdlib.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// ============================================================================
// Result Construction Functions
// ============================================================================

AriaResultPtr npk_result_ok_ptr(void* value) {
    AriaResultPtr result;
    result.value = value;
    result.error = NULL;
    result.is_error = false;
    return result;
}

AriaResultPtr npk_result_err_ptr(AriaError* error) {
    AriaResultPtr result;
    result.value = NULL;
    result.error = error;
    result.is_error = true;
    return result;
}

AriaResultI64 npk_result_ok_i64(int64_t value) {
    AriaResultI64 result;
    result.value = value;
    result.error = NULL;
    result.is_error = false;
    return result;
}

AriaResultI64 npk_result_err_i64(AriaError* error) {
    AriaResultI64 result;
    result.value = 0;
    result.error = error;
    result.is_error = true;
    return result;
}

AriaResultF64 npk_result_ok_f64(double value) {
    AriaResultF64 result;
    result.value = value;
    result.error = NULL;
    result.is_error = false;
    return result;
}

AriaResultF64 npk_result_err_f64(AriaError* error) {
    AriaResultF64 result;
    result.value = 0.0;
    result.error = error;
    result.is_error = true;
    return result;
}

AriaResultBool npk_result_ok_bool(bool value) {
    AriaResultBool result;
    result.value = value;
    result.error = NULL;
    result.is_error = false;
    return result;
}

AriaResultBool npk_result_err_bool(AriaError* error) {
    AriaResultBool result;
    result.value = false;
    result.error = error;
    result.is_error = true;
    return result;
}

AriaResultVoid npk_result_ok_void(void) {
    AriaResultVoid result;
    result.error = NULL;
    result.is_error = false;
    return result;
}

AriaResultVoid npk_result_err_void(AriaError* error) {
    AriaResultVoid result;
    result.error = error;
    result.is_error = true;
    return result;
}

// ============================================================================
// Error Construction Functions
// ============================================================================

AriaError* npk_error_new(int32_t code, const char* message, const char* file, int32_t line) {
    // Allocate error on GC heap (type_id 0 for generic)
    AriaError* error = (AriaError*)npk_gc_alloc(sizeof(AriaError), 0);
    if (!error) {
        return NULL;  // OOM - caller must handle
    }
    
    error->code = code;
    error->line = line;
    
    // Copy message to GC heap
    if (message) {
        size_t len = strlen(message);
        char* msg_copy = (char*)npk_gc_alloc(len + 1, 0);
        if (msg_copy) {
            memcpy(msg_copy, message, len);
            msg_copy[len] = '\0';
            error->message = msg_copy;
        } else {
            error->message = "Out of memory";
        }
    } else {
        error->message = "Unknown error";
    }
    
    // Copy file path to GC heap
    if (file) {
        size_t len = strlen(file);
        char* file_copy = (char*)npk_gc_alloc(len + 1, 0);
        if (file_copy) {
            memcpy(file_copy, file, len);
            file_copy[len] = '\0';
            error->file = file_copy;
        } else {
            error->file = NULL;
        }
    } else {
        error->file = NULL;
    }
    
    return error;
}

AriaError* npk_error_msg(const char* message) {
    return npk_error_new(ARIA_ERR_UNKNOWN, message, NULL, 0);
}

// ============================================================================
// Result Query Functions
// ============================================================================

bool npk_result_is_ok_ptr(AriaResultPtr result) {
    return !result.is_error;
}

bool npk_result_is_ok_i64(AriaResultI64 result) {
    return !result.is_error;
}

bool npk_result_is_ok_f64(AriaResultF64 result) {
    return !result.is_error;
}

bool npk_result_is_ok_bool(AriaResultBool result) {
    return !result.is_error;
}

bool npk_result_is_ok_void(AriaResultVoid result) {
    return !result.is_error;
}

bool npk_result_is_err_ptr(AriaResultPtr result) {
    return result.is_error;
}

bool npk_result_is_err_i64(AriaResultI64 result) {
    return result.is_error;
}

bool npk_result_is_err_f64(AriaResultF64 result) {
    return result.is_error;
}

bool npk_result_is_err_bool(AriaResultBool result) {
    return result.is_error;
}

bool npk_result_is_err_void(AriaResultVoid result) {
    return result.is_error;
}

AriaError* npk_result_get_error_ptr(AriaResultPtr result) {
    return result.is_error ? (AriaError*)result.error : NULL;
}

AriaError* npk_result_get_error_i64(AriaResultI64 result) {
    return result.is_error ? (AriaError*)result.error : NULL;
}

AriaError* npk_result_get_error_f64(AriaResultF64 result) {
    return result.is_error ? (AriaError*)result.error : NULL;
}

AriaError* npk_result_get_error_bool(AriaResultBool result) {
    return result.is_error ? (AriaError*)result.error : NULL;
}

AriaError* npk_result_get_error_void(AriaResultVoid result) {
    return result.is_error ? (AriaError*)result.error : NULL;
}

// ============================================================================
// Unwrap Functions
// ============================================================================

void* npk_result_unwrap_ptr(AriaResultPtr result) {
    if (result.is_error) {
        AriaError* err = (AriaError*)result.error;
        fprintf(stderr, "PANIC: Attempted to unwrap error result\n");
        if (err && err->message) {
            fprintf(stderr, "Error: %s\n", err->message);
            if (err->file) {
                fprintf(stderr, "  at %s:%d\n", err->file, err->line);
            }
        }
        abort();
    }
    return result.value;
}

int64_t npk_result_unwrap_i64(AriaResultI64 result) {
    if (result.is_error) {
        AriaError* err = (AriaError*)result.error;
        fprintf(stderr, "PANIC: Attempted to unwrap error result\n");
        if (err && err->message) {
            fprintf(stderr, "Error: %s\n", err->message);
            if (err->file) {
                fprintf(stderr, "  at %s:%d\n", err->file, err->line);
            }
        }
        abort();
    }
    return result.value;
}

double npk_result_unwrap_f64(AriaResultF64 result) {
    if (result.is_error) {
        AriaError* err = (AriaError*)result.error;
        fprintf(stderr, "PANIC: Attempted to unwrap error result\n");
        if (err && err->message) {
            fprintf(stderr, "Error: %s\n", err->message);
            if (err->file) {
                fprintf(stderr, "  at %s:%d\n", err->file, err->line);
            }
        }
        abort();
    }
    return result.value;
}

bool npk_result_unwrap_bool(AriaResultBool result) {
    if (result.is_error) {
        AriaError* err = (AriaError*)result.error;
        fprintf(stderr, "PANIC: Attempted to unwrap error result\n");
        if (err && err->message) {
            fprintf(stderr, "Error: %s\n", err->message);
            if (err->file) {
                fprintf(stderr, "  at %s:%d\n", err->file, err->line);
            }
        }
        abort();
    }
    return result.value;
}

// ============================================================================
// Unwrap-or Functions
// ============================================================================

void* npk_result_unwrap_or_ptr(AriaResultPtr result, void* default_value) {
    return result.is_error ? default_value : result.value;
}

int64_t npk_result_unwrap_or_i64(AriaResultI64 result, int64_t default_value) {
    return result.is_error ? default_value : result.value;
}

double npk_result_unwrap_or_f64(AriaResultF64 result, double default_value) {
    return result.is_error ? default_value : result.value;
}

bool npk_result_unwrap_or_bool(AriaResultBool result, bool default_value) {
    return result.is_error ? default_value : result.value;
}

// ============================================================================
// Allocation Result Functions (Phase 4.2)
// ============================================================================

AriaAllocResult npk_alloc_result_ok(void* ptr, size_t size, size_t align) {
    AriaAllocResult result;
    result.value = ptr;
    result.error_code = ARIA_ALLOC_OK;
    result.requested_size = size;
    result.requested_align = align;
    return result;
}

AriaAllocResult npk_alloc_result_err(AriaAllocError error, size_t size, size_t align) {
    AriaAllocResult result;
    result.value = NULL;
    result.error_code = error;
    result.requested_size = size;
    result.requested_align = align;
    return result;
}

bool npk_alloc_result_is_ok(AriaAllocResult result) {
    return result.error_code == ARIA_ALLOC_OK;
}

bool npk_alloc_result_is_err(AriaAllocResult result) {
    return result.error_code != ARIA_ALLOC_OK;
}

const char* npk_alloc_error_message(AriaAllocError error) {
    switch (error) {
        case ARIA_ALLOC_OK:
            return "Success";
        case ARIA_ALLOC_ERR_OUT_OF_MEMORY:
            return "Out of memory";
        case ARIA_ALLOC_ERR_INVALID_SIZE:
            return "Invalid size (must be > 0)";
        case ARIA_ALLOC_ERR_INVALID_ALIGNMENT:
            return "Invalid alignment (must be power of 2)";
        case ARIA_ALLOC_ERR_SIZE_OVERFLOW:
            return "Size overflow (size * count too large)";
        case ARIA_ALLOC_ERR_UNSUPPORTED:
            return "Operation not supported";
        default:
            return "Unknown error";
    }
}
