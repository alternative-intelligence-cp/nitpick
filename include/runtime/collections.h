/**
 * Phase 6.2 Standard Library - Collections
 * 
 * Array utilities and functional programming operations for Aria runtime.
 * 
 * Design:
 * - Generic array wrapper with dynamic capacity
 * - GC-integrated memory management
 * - Type-safe operations with result types
 * - Functional programming support (filter, map, reduce)
 */

#ifndef ARIA_RUNTIME_COLLECTIONS_H
#define ARIA_RUNTIME_COLLECTIONS_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "runtime/result.h"

#ifdef __cplusplus
extern "C" {
#endif

// ═══════════════════════════════════════════════════════════════════════
// Array Structure
// ═══════════════════════════════════════════════════════════════════════

/**
 * Dynamic array structure.
 * 
 * Memory layout:
 * - data: GC-allocated array of elements
 * - length: Current number of elements
 * - capacity: Allocated capacity
 * - element_size: Size of each element in bytes
 * - type_id: Type ID for GC tracking (0 for generic)
 */
typedef struct {
    void* data;          // Pointer to element array
    size_t length;       // Current number of elements
    size_t capacity;     // Allocated capacity
    size_t element_size; // Size of each element
    int type_id;         // Type ID for GC (0=generic)
} AriaArray;

// ═══════════════════════════════════════════════════════════════════════
// Array Creation and Destruction
// ═══════════════════════════════════════════════════════════════════════

/**
 * Create a new array with specified element size and initial capacity.
 * 
 * @param element_size Size of each element in bytes
 * @param initial_capacity Initial capacity (0 for default)
 * @param type_id Type ID for GC tracking (0 for generic)
 * @return Result containing pointer to AriaArray or error
 */
AriaResultPtr aria_array_new(size_t element_size, size_t initial_capacity, int type_id);

/**
 * Free an array (only needed for Wild allocator, no-op for GC).
 * 
 * @param array Array to free
 */
void aria_array_free(AriaArray* array);

// ═══════════════════════════════════════════════════════════════════════
// Array Basic Operations
// ═══════════════════════════════════════════════════════════════════════

/**
 * Get the length of an array.
 * 
 * @param array Array to query
 * @return Number of elements in array
 */
size_t aria_array_length(const AriaArray* array);

/**
 * Get pointer to element at index (no bounds checking).
 * 
 * @param array Array to access
 * @param index Element index
 * @return Pointer to element or NULL if array is NULL
 */
void* aria_array_get_unchecked(const AriaArray* array, size_t index);

/**
 * Get pointer to element at index with bounds checking.
 * 
 * @param array Array to access
 * @param index Element index
 * @return Result containing pointer to element or error
 */
AriaResultPtr aria_array_get(const AriaArray* array, size_t index);

/**
 * Set element at index (no bounds checking).
 * 
 * @param array Array to modify
 * @param index Element index
 * @param value Pointer to value to copy
 */
void aria_array_set_unchecked(AriaArray* array, size_t index, const void* value);

/**
 * Set element at index with bounds checking.
 * 
 * @param array Array to modify
 * @param index Element index
 * @param value Pointer to value to copy
 * @return Result indicating success or error
 */
AriaResultVoid aria_array_set(AriaArray* array, size_t index, const void* value);

/**
 * Push element to end of array (grows if needed).
 * 
 * @param array Array to modify
 * @param value Pointer to value to copy
 * @return Result indicating success or error
 */
AriaResultVoid aria_array_push(AriaArray* array, const void* value);

/**
 * Pop element from end of array.
 * 
 * @param array Array to modify
 * @param out_value Pointer to store popped value (optional, can be NULL)
 * @return Result indicating success or error
 */
AriaResultVoid aria_array_pop(AriaArray* array, void* out_value);

/**
 * Create a slice (view) of an array.
 * 
 * @param array Source array
 * @param start Start index (inclusive)
 * @param end End index (exclusive)
 * @return Result containing new array with copied elements or error
 */
AriaResultPtr aria_array_slice(const AriaArray* array, size_t start, size_t end);

// ═══════════════════════════════════════════════════════════════════════
// Array Functional Operations
// ═══════════════════════════════════════════════════════════════════════

/**
 * Predicate function type for filter.
 * 
 * @param element Pointer to element
 * @param index Element index
 * @param context User-provided context
 * @return true if element should be included
 */
typedef bool (*AriaPredicateFn)(const void* element, size_t index, void* context);

/**
 * Mapper function type for transform/map.
 * 
 * @param element Pointer to input element
 * @param index Element index
 * @param out_element Pointer to output element (to write result)
 * @param context User-provided context
 */
typedef void (*AriaMapperFn)(const void* element, size_t index, void* out_element, void* context);

/**
 * Reducer function type for reduce.
 * 
 * @param accumulator Pointer to accumulator value
 * @param element Pointer to current element
 * @param index Element index
 * @param context User-provided context
 */
typedef void (*AriaReducerFn)(void* accumulator, const void* element, size_t index, void* context);

/**
 * Comparator function type for sort.
 * 
 * @param a Pointer to first element
 * @param b Pointer to second element
 * @param context User-provided context
 * @return Negative if a < b, 0 if a == b, positive if a > b
 */
typedef int (*AriaComparatorFn)(const void* a, const void* b, void* context);

/**
 * Filter array elements using predicate function.
 * 
 * @param array Source array
 * @param predicate Predicate function
 * @param context User-provided context (optional)
 * @return Result containing new filtered array or error
 */
AriaResultPtr aria_array_filter(const AriaArray* array, AriaPredicateFn predicate, void* context);

/**
 * Transform/map array elements using mapper function.
 * 
 * @param array Source array
 * @param mapper Mapper function
 * @param output_element_size Size of output elements
 * @param output_type_id Type ID for output elements
 * @param context User-provided context (optional)
 * @return Result containing new transformed array or error
 */
AriaResultPtr aria_array_transform(const AriaArray* array, AriaMapperFn mapper, 
                                   size_t output_element_size, int output_type_id, void* context);

/**
 * Reduce array to single value using reducer function.
 * 
 * @param array Source array
 * @param reducer Reducer function
 * @param initial Pointer to initial accumulator value
 * @param accumulator_size Size of accumulator
 * @param context User-provided context (optional)
 * @return Result containing pointer to final accumulator or error
 */
AriaResultPtr aria_array_reduce(const AriaArray* array, AriaReducerFn reducer,
                                const void* initial, size_t accumulator_size, void* context);

/**
 * Sort array in-place using comparator function.
 * 
 * @param array Array to sort
 * @param comparator Comparator function
 * @param context User-provided context (optional)
 * @return Result indicating success or error
 */
AriaResultVoid aria_array_sort(AriaArray* array, AriaComparatorFn comparator, void* context);

/**
 * Reverse array elements in-place.
 * 
 * @param array Array to reverse
 * @return Result indicating success or error
 */
AriaResultVoid aria_array_reverse(AriaArray* array);

/**
 * Remove duplicate elements from array (preserves order, keeps first occurrence).
 * 
 * @param array Array to deduplicate
 * @param comparator Comparator function for equality (NULL for memcmp)
 * @param context User-provided context (optional)
 * @return Result containing new deduplicated array or error
 */
AriaResultPtr aria_array_unique(const AriaArray* array, AriaComparatorFn comparator, void* context);

#ifdef __cplusplus
}
#endif

#endif // ARIA_RUNTIME_COLLECTIONS_H
