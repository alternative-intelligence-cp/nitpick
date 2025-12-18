/**
 * Phase 6.2 Standard Library - Collections Implementation
 * 
 * Array utilities and functional programming operations.
 */

#include "runtime/collections.h"
#include "runtime/gc.h"
#include "runtime/stdlib.h"
#include <cstring>
#include <cstdlib>

// Default initial capacity for arrays
#define ARIA_ARRAY_DEFAULT_CAPACITY 16

// Growth factor for array expansion (1.5x)
#define ARIA_ARRAY_GROWTH_FACTOR 3
#define ARIA_ARRAY_GROWTH_DIVISOR 2

// ═══════════════════════════════════════════════════════════════════════
// Array Creation and Destruction
// ═══════════════════════════════════════════════════════════════════════

AriaResultPtr aria_array_new(size_t element_size, size_t initial_capacity, int type_id) {
    if (element_size == 0) {
        AriaError* error = aria_error_new(
            ARIA_ERR_INVALID_ARG,
            "Element size cannot be zero",
            __FILE__, __LINE__
        );
        return aria_result_err_ptr(error);
    }
    
    // Use default capacity if not specified
    size_t capacity = initial_capacity > 0 ? initial_capacity : ARIA_ARRAY_DEFAULT_CAPACITY;
    
    // Allocate array structure on GC heap
    AriaArray* array = (AriaArray*)aria_gc_alloc(sizeof(AriaArray), 0);
    if (!array) {
        AriaError* error = aria_error_new(
            ARIA_ERR_OUT_OF_MEMORY,
            "Failed to allocate array structure",
            __FILE__, __LINE__
        );
        return aria_result_err_ptr(error);
    }
    
    // Allocate data array on GC heap
    array->data = aria_gc_alloc(element_size * capacity, type_id);
    if (!array->data) {
        AriaError* error = aria_error_new(
            ARIA_ERR_OUT_OF_MEMORY,
            "Failed to allocate array data",
            __FILE__, __LINE__
        );
        return aria_result_err_ptr(error);
    }
    
    array->length = 0;
    array->capacity = capacity;
    array->element_size = element_size;
    array->type_id = type_id;
    
    return aria_result_ok_ptr(array);
}

void aria_array_free(AriaArray* array) {
    // No-op for GC-managed memory
    // In future, could mark for eager collection
    (void)array;
}

// ═══════════════════════════════════════════════════════════════════════
// Array Basic Operations
// ═══════════════════════════════════════════════════════════════════════

size_t aria_array_length(const AriaArray* array) {
    if (!array) return 0;
    return array->length;
}

void* aria_array_get_unchecked(const AriaArray* array, size_t index) {
    if (!array || !array->data) return nullptr;
    return (char*)array->data + (index * array->element_size);
}

AriaResultPtr aria_array_get(const AriaArray* array, size_t index) {
    if (!array) {
        AriaError* error = aria_error_new(
            ARIA_ERR_NULL_PTR,
            "Array is NULL",
            __FILE__, __LINE__
        );
        return aria_result_err_ptr(error);
    }
    
    if (index >= array->length) {
        AriaError* error = aria_error_new(
            ARIA_ERR_INDEX_OUT_OF_BOUNDS,
            "Array index out of bounds",
            __FILE__, __LINE__
        );
        return aria_result_err_ptr(error);
    }
    
    void* element = aria_array_get_unchecked(array, index);
    return aria_result_ok_ptr(element);
}

void aria_array_set_unchecked(AriaArray* array, size_t index, const void* value) {
    if (!array || !array->data || !value) return;
    void* dest = (char*)array->data + (index * array->element_size);
    std::memcpy(dest, value, array->element_size);
}

AriaResultVoid aria_array_set(AriaArray* array, size_t index, const void* value) {
    if (!array) {
        AriaError* error = aria_error_new(
            ARIA_ERR_NULL_PTR,
            "Array is NULL",
            __FILE__, __LINE__
        );
        return aria_result_err_void(error);
    }
    
    if (!value) {
        AriaError* error = aria_error_new(
            ARIA_ERR_NULL_PTR,
            "Value is NULL",
            __FILE__, __LINE__
        );
        return aria_result_err_void(error);
    }
    
    if (index >= array->length) {
        AriaError* error = aria_error_new(
            ARIA_ERR_INDEX_OUT_OF_BOUNDS,
            "Array index out of bounds",
            __FILE__, __LINE__
        );
        return aria_result_err_void(error);
    }
    
    aria_array_set_unchecked(array, index, value);
    return aria_result_ok_void();
}

AriaResultVoid aria_array_push(AriaArray* array, const void* value) {
    if (!array) {
        AriaError* error = aria_error_new(
            ARIA_ERR_NULL_PTR,
            "Array is NULL",
            __FILE__, __LINE__
        );
        return aria_result_err_void(error);
    }
    
    if (!value) {
        AriaError* error = aria_error_new(
            ARIA_ERR_NULL_PTR,
            "Value is NULL",
            __FILE__, __LINE__
        );
        return aria_result_err_void(error);
    }
    
    // Grow array if needed
    if (array->length >= array->capacity) {
        size_t new_capacity = (array->capacity * ARIA_ARRAY_GROWTH_FACTOR) / ARIA_ARRAY_GROWTH_DIVISOR;
        if (new_capacity <= array->capacity) new_capacity = array->capacity + 1;
        
        void* new_data = aria_gc_alloc(array->element_size * new_capacity, array->type_id);
        if (!new_data) {
            AriaError* error = aria_error_new(
                ARIA_ERR_OUT_OF_MEMORY,
                "Failed to grow array",
                __FILE__, __LINE__
            );
            return aria_result_err_void(error);
        }
        
        // Copy existing elements
        std::memcpy(new_data, array->data, array->element_size * array->length);
        array->data = new_data;
        array->capacity = new_capacity;
    }
    
    // Copy element to end
    aria_array_set_unchecked(array, array->length, value);
    array->length++;
    
    return aria_result_ok_void();
}

AriaResultVoid aria_array_pop(AriaArray* array, void* out_value) {
    if (!array) {
        AriaError* error = aria_error_new(
            ARIA_ERR_NULL_PTR,
            "Array is NULL",
            __FILE__, __LINE__
        );
        return aria_result_err_void(error);
    }
    
    if (array->length == 0) {
        AriaError* error = aria_error_new(
            ARIA_ERR_INDEX_OUT_OF_BOUNDS,
            "Cannot pop from empty array",
            __FILE__, __LINE__
        );
        return aria_result_err_void(error);
    }
    
    // Copy element if output pointer provided
    if (out_value) {
        void* element = aria_array_get_unchecked(array, array->length - 1);
        std::memcpy(out_value, element, array->element_size);
    }
    
    array->length--;
    return aria_result_ok_void();
}

AriaResultPtr aria_array_slice(const AriaArray* array, size_t start, size_t end) {
    if (!array) {
        AriaError* error = aria_error_new(
            ARIA_ERR_NULL_PTR,
            "Array is NULL",
            __FILE__, __LINE__
        );
        return aria_result_err_ptr(error);
    }
    
    if (start > end || end > array->length) {
        AriaError* error = aria_error_new(
            ARIA_ERR_INDEX_OUT_OF_BOUNDS,
            "Invalid slice range",
            __FILE__, __LINE__
        );
        return aria_result_err_ptr(error);
    }
    
    size_t slice_length = end - start;
    
    // Create new array for slice
    AriaResultPtr result = aria_array_new(array->element_size, slice_length, array->type_id);
    if (result.is_error) return result;
    
    AriaArray* slice = (AriaArray*)result.value;
    
    // Copy elements
    for (size_t i = 0; i < slice_length; i++) {
        void* src = aria_array_get_unchecked(array, start + i);
        aria_array_set_unchecked(slice, i, src);
    }
    slice->length = slice_length;
    
    return aria_result_ok_ptr(slice);
}

// ═══════════════════════════════════════════════════════════════════════
// Array Functional Operations
// ═══════════════════════════════════════════════════════════════════════

AriaResultPtr aria_array_filter(const AriaArray* array, AriaPredicateFn predicate, void* context) {
    if (!array) {
        AriaError* error = aria_error_new(
            ARIA_ERR_NULL_PTR,
            "Array is NULL",
            __FILE__, __LINE__
        );
        return aria_result_err_ptr(error);
    }
    
    if (!predicate) {
        AriaError* error = aria_error_new(
            ARIA_ERR_NULL_PTR,
            "Predicate function is NULL",
            __FILE__, __LINE__
        );
        return aria_result_err_ptr(error);
    }
    
    // Create result array with same initial capacity
    AriaResultPtr result = aria_array_new(array->element_size, array->capacity, array->type_id);
    if (result.is_error) return result;
    
    AriaArray* filtered = (AriaArray*)result.value;
    
    // Filter elements
    for (size_t i = 0; i < array->length; i++) {
        void* element = aria_array_get_unchecked(array, i);
        if (predicate(element, i, context)) {
            AriaResultVoid push_result = aria_array_push(filtered, element);
            if (push_result.is_error) {
                return aria_result_err_ptr((AriaError*)push_result.error);
            }
        }
    }
    
    return aria_result_ok_ptr(filtered);
}

AriaResultPtr aria_array_transform(const AriaArray* array, AriaMapperFn mapper,
                                   size_t output_element_size, int output_type_id, void* context) {
    if (!array) {
        AriaError* error = aria_error_new(
            ARIA_ERR_NULL_PTR,
            "Array is NULL",
            __FILE__, __LINE__
        );
        return aria_result_err_ptr(error);
    }
    
    if (!mapper) {
        AriaError* error = aria_error_new(
            ARIA_ERR_NULL_PTR,
            "Mapper function is NULL",
            __FILE__, __LINE__
        );
        return aria_result_err_ptr(error);
    }
    
    // Create result array with same length
    AriaResultPtr result = aria_array_new(output_element_size, array->length, output_type_id);
    if (result.is_error) return result;
    
    AriaArray* transformed = (AriaArray*)result.value;
    transformed->length = array->length;
    
    // Transform elements
    for (size_t i = 0; i < array->length; i++) {
        void* in_element = aria_array_get_unchecked(array, i);
        void* out_element = aria_array_get_unchecked(transformed, i);
        mapper(in_element, i, out_element, context);
    }
    
    return aria_result_ok_ptr(transformed);
}

AriaResultPtr aria_array_reduce(const AriaArray* array, AriaReducerFn reducer,
                                const void* initial, size_t accumulator_size, void* context) {
    if (!array) {
        AriaError* error = aria_error_new(
            ARIA_ERR_NULL_PTR,
            "Array is NULL",
            __FILE__, __LINE__
        );
        return aria_result_err_ptr(error);
    }
    
    if (!reducer) {
        AriaError* error = aria_error_new(
            ARIA_ERR_NULL_PTR,
            "Reducer function is NULL",
            __FILE__, __LINE__
        );
        return aria_result_err_ptr(error);
    }
    
    if (!initial) {
        AriaError* error = aria_error_new(
            ARIA_ERR_NULL_PTR,
            "Initial value is NULL",
            __FILE__, __LINE__
        );
        return aria_result_err_ptr(error);
    }
    
    // Allocate accumulator on GC heap
    void* accumulator = aria_gc_alloc(accumulator_size, 0);
    if (!accumulator) {
        AriaError* error = aria_error_new(
            ARIA_ERR_OUT_OF_MEMORY,
            "Failed to allocate accumulator",
            __FILE__, __LINE__
        );
        return aria_result_err_ptr(error);
    }
    
    // Initialize accumulator
    std::memcpy(accumulator, initial, accumulator_size);
    
    // Reduce elements
    for (size_t i = 0; i < array->length; i++) {
        void* element = aria_array_get_unchecked(array, i);
        reducer(accumulator, element, i, context);
    }
    
    return aria_result_ok_ptr(accumulator);
}

// Global context for qsort (not thread-safe, but simple)
static struct {
    AriaComparatorFn comparator;
    void* context;
} g_sort_context;

static int sort_wrapper(const void* a, const void* b) {
    return g_sort_context.comparator(a, b, g_sort_context.context);
}

AriaResultVoid aria_array_sort(AriaArray* array, AriaComparatorFn comparator, void* context) {
    if (!array) {
        AriaError* error = aria_error_new(
            ARIA_ERR_NULL_PTR,
            "Array is NULL",
            __FILE__, __LINE__
        );
        return aria_result_err_void(error);
    }
    
    if (!comparator) {
        AriaError* error = aria_error_new(
            ARIA_ERR_NULL_PTR,
            "Comparator function is NULL",
            __FILE__, __LINE__
        );
        return aria_result_err_void(error);
    }
    
    if (array->length <= 1) {
        return aria_result_ok_void();
    }
    
    // Use qsort with global context (not thread-safe, but works)
    g_sort_context.comparator = comparator;
    g_sort_context.context = context;
    
    std::qsort(array->data, array->length, array->element_size, sort_wrapper);
    
    return aria_result_ok_void();
}

AriaResultVoid aria_array_reverse(AriaArray* array) {
    if (!array) {
        AriaError* error = aria_error_new(
            ARIA_ERR_NULL_PTR,
            "Array is NULL",
            __FILE__, __LINE__
        );
        return aria_result_err_void(error);
    }
    
    if (array->length <= 1) {
        return aria_result_ok_void();
    }
    
    // Allocate temporary buffer for swapping
    void* temp = std::malloc(array->element_size);
    if (!temp) {
        AriaError* error = aria_error_new(
            ARIA_ERR_OUT_OF_MEMORY,
            "Failed to allocate temp buffer for reverse",
            __FILE__, __LINE__
        );
        return aria_result_err_void(error);
    }
    
    // Swap elements from both ends
    size_t left = 0;
    size_t right = array->length - 1;
    
    while (left < right) {
        void* left_elem = aria_array_get_unchecked(array, left);
        void* right_elem = aria_array_get_unchecked(array, right);
        
        // Swap
        std::memcpy(temp, left_elem, array->element_size);
        std::memcpy(left_elem, right_elem, array->element_size);
        std::memcpy(right_elem, temp, array->element_size);
        
        left++;
        right--;
    }
    
    std::free(temp);
    return aria_result_ok_void();
}

AriaResultPtr aria_array_unique(const AriaArray* array, AriaComparatorFn comparator, void* context) {
    if (!array) {
        AriaError* error = aria_error_new(
            ARIA_ERR_NULL_PTR,
            "Array is NULL",
            __FILE__, __LINE__
        );
        return aria_result_err_ptr(error);
    }
    
    // Create result array
    AriaResultPtr result = aria_array_new(array->element_size, array->capacity, array->type_id);
    if (result.is_error) return result;
    
    AriaArray* unique = (AriaArray*)result.value;
    
    // Add elements if not already present
    for (size_t i = 0; i < array->length; i++) {
        void* element = aria_array_get_unchecked(array, i);
        
        // Check if element already exists in unique array
        bool found = false;
        for (size_t j = 0; j < unique->length; j++) {
            void* existing = aria_array_get_unchecked(unique, j);
            
            int cmp;
            if (comparator) {
                cmp = comparator(element, existing, context);
            } else {
                cmp = std::memcmp(element, existing, array->element_size);
            }
            
            if (cmp == 0) {
                found = true;
                break;
            }
        }
        
        if (!found) {
            AriaResultVoid push_result = aria_array_push(unique, element);
            if (push_result.is_error) {
                return aria_result_err_ptr((AriaError*)push_result.error);
            }
        }
    }
    
    return aria_result_ok_ptr(unique);
}
