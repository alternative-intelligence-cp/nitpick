/*
 * Collection API Tests (Array and Map)
 * 
 * Tests the collection constructors and operations:
 * - Arrays: dynamic resizable arrays
 * - Maps: hash tables for key-value storage
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "../include/runtime/collections.h"
#include "../include/runtime/map.h"

// Test counters
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST_ASSERT(condition, test_name) do { \
    if (condition) { \
        tests_passed++; \
        printf("✓ %s\n", test_name); \
    } else { \
        tests_failed++; \
        printf("✗ %s\n", test_name); \
    } \
} while(0)

// ============================================================================
// Array Tests
// ============================================================================

void test_array_basic() {
    printf("\n=== Array Basic Tests ===\n");
    
    // Create array for int32 elements
    AriaResultPtr result = npk_array_new(sizeof(int32_t), 0);
    TEST_ASSERT(!result.is_error, "array_new: creation succeeds");
    
    AriaArray* arr = (AriaArray*)result.value;
    TEST_ASSERT(arr != NULL, "array_new: returns valid pointer");
    
    // Check initial state
    size_t len = npk_array_length(arr);
    TEST_ASSERT(len == 0, "array_length: new array is empty");
    
    size_t cap = npk_array_capacity(arr);
    TEST_ASSERT(cap >= 0, "array_capacity: has valid capacity");
    
    // Clean up
    npk_array_free(arr);
}

void test_array_push_pop() {
    printf("\n=== Array Push/Pop Tests ===\n");
    
    AriaResultPtr result = npk_array_new(sizeof(int32_t), 0);
    AriaArray* arr = (AriaArray*)result.value;
    
    // Push elements
    int32_t val1 = 42;
    AriaResultVoid r1 = npk_array_push(arr, &val1);
    TEST_ASSERT(!r1.is_error, "array_push: first element succeeds");
    TEST_ASSERT(npk_array_length(arr) == 1, "array_length: 1 after first push");
    
    int32_t val2 = 99;
    AriaResultVoid r2 = npk_array_push(arr, &val2);
    TEST_ASSERT(!r2.is_error, "array_push: second element succeeds");
    TEST_ASSERT(npk_array_length(arr) == 2, "array_length: 2 after second push");
    
    int32_t val3 = -17;
    npk_array_push(arr, &val3);
    TEST_ASSERT(npk_array_length(arr) == 3, "array_length: 3 after third push");
    
    // Get elements
    AriaResultPtr g1 = npk_array_get(arr, 0);
    TEST_ASSERT(!g1.is_error, "array_get: index 0 succeeds");
    TEST_ASSERT(*(int32_t*)g1.value == 42, "array_get: index 0 has correct value");
    
    AriaResultPtr g2 = npk_array_get(arr, 1);
    TEST_ASSERT(*(int32_t*)g2.value == 99, "array_get: index 1 has correct value");
    
    AriaResultPtr g3 = npk_array_get(arr, 2);
    TEST_ASSERT(*(int32_t*)g3.value == -17, "array_get: index 2 has correct value");
    
    // Pop element
    AriaResultPtr p1 = npk_array_pop(arr);
    TEST_ASSERT(!p1.is_error, "array_pop: succeeds");
    TEST_ASSERT(*(int32_t*)p1.value == -17, "array_pop: returns last value");
    TEST_ASSERT(npk_array_length(arr) == 2, "array_length: 2 after pop");
    
    // Clean up
    npk_array_free(arr);
}

void test_array_set() {
    printf("\n=== Array Set Tests ===\n");
    
    AriaResultPtr result = npk_array_new(sizeof(int32_t), 0);
    AriaArray* arr = (AriaArray*)result.value;
    
    // Push some elements
    int32_t vals[] = {10, 20, 30};
    for (int i = 0; i < 3; i++) {
        npk_array_push(arr, &vals[i]);
    }
    
    // Modify middle element
    int32_t new_val = 999;
    AriaResultVoid set_result = npk_array_set(arr, 1, &new_val);
    TEST_ASSERT(!set_result.is_error, "array_set: succeeds");
    
    // Verify modification
    AriaResultPtr get_result = npk_array_get(arr, 1);
    TEST_ASSERT(*(int32_t*)get_result.value == 999, "array_set: value updated correctly");
    
    // Verify other elements unchanged
    AriaResultPtr g0 = npk_array_get(arr, 0);
    TEST_ASSERT(*(int32_t*)g0.value == 10, "array_set: index 0 unchanged");
    
    AriaResultPtr g2 = npk_array_get(arr, 2);
    TEST_ASSERT(*(int32_t*)g2.value == 30, "array_set: index 2 unchanged");
    
    // Clean up
    npk_array_free(arr);
}

void test_array_bounds() {
    printf("\n=== Array Bounds Tests ===\n");
    
    AriaResultPtr result = npk_array_new(sizeof(int32_t), 0);
    AriaArray* arr = (AriaArray*)result.value;
    
    int32_t val = 42;
    npk_array_push(arr, &val);
    
    // Out of bounds access
    AriaResultPtr get_oob = npk_array_get(arr, 100);
    TEST_ASSERT(get_oob.is_error, "array_get: out of bounds returns error");
    
    // Out of bounds set
    AriaResultVoid set_oob = npk_array_set(arr, 100, &val);
    TEST_ASSERT(set_oob.is_error, "array_set: out of bounds returns error");
    
    // Pop from empty
    npk_array_pop(arr); // Remove the one element
    AriaResultPtr pop_empty = npk_array_pop(arr);
    TEST_ASSERT(pop_empty.is_error, "array_pop: empty array returns error");
    
    // Clean up
    npk_array_free(arr);
}

// ============================================================================
// Map Tests
// ============================================================================

void test_map_basic() {
    printf("\n=== Map Basic Tests ===\n");
    
    // Create map for int32 keys and int32 values
    AriaResultPtr result = npk_map_new(sizeof(int32_t), sizeof(int32_t), 0);
    TEST_ASSERT(!result.is_error, "map_new: creation succeeds");
    
    AriaMap* map = (AriaMap*)result.value;
    TEST_ASSERT(map != NULL, "map_new: returns valid pointer");
    
    // Check initial state
    size_t len = npk_map_length(map);
    TEST_ASSERT(len == 0, "map_length: new map is empty");
    
    // Clean up
    npk_map_free(map);
}

void test_map_insert_get() {
    printf("\n=== Map Insert/Get Tests ===\n");
    
    AriaMap* map = npk_map_new_simple(sizeof(int32_t), sizeof(int32_t));
    
    // Insert key-value pairs
    int32_t key1 = 10;
    int32_t val1 = 100;
    npk_map_insert_simple(map, &key1, &val1);
    TEST_ASSERT(npk_map_length(map) == 1, "map_insert: length 1 after first insert");
    
    int32_t key2 = 20;
    int32_t val2 = 200;
    npk_map_insert_simple(map, &key2, &val2);
    TEST_ASSERT(npk_map_length(map) == 2, "map_insert: length 2 after second insert");
    
    // Get values
    int32_t* get1 = (int32_t*)npk_map_get_simple(map, &key1);
    TEST_ASSERT(get1 != NULL, "map_get: key 10 found");
    TEST_ASSERT(*get1 == 100, "map_get: key 10 has value 100");
    
    int32_t* get2 = (int32_t*)npk_map_get_simple(map, &key2);
    TEST_ASSERT(get2 != NULL, "map_get: key 20 found");
    TEST_ASSERT(*get2 == 200, "map_get: key 20 has value 200");
    
    // Get non-existent key
    int32_t key3 = 999;
    int32_t* get3 = (int32_t*)npk_map_get_simple(map, &key3);
    TEST_ASSERT(get3 == NULL, "map_get: non-existent key returns NULL");
    
    // Clean up
    npk_map_free(map);
}

void test_map_has() {
    printf("\n=== Map Has Tests ===\n");
    
    AriaMap* map = npk_map_new_simple(sizeof(int32_t), sizeof(int32_t));
    
    int32_t key1 = 42;
    int32_t val1 = 999;
    npk_map_insert_simple(map, &key1, &val1);
    
    // Check existence
    bool has1 = npk_map_has(map, &key1);
    TEST_ASSERT(has1, "map_has: inserted key exists");
    
    int32_t key2 = 100;
    bool has2 = npk_map_has(map, &key2);
    TEST_ASSERT(!has2, "map_has: non-existent key doesn't exist");
    
    // Clean up
    npk_map_free(map);
}

void test_map_update() {
    printf("\n=== Map Update Tests ===\n");
    
    AriaMap* map = npk_map_new_simple(sizeof(int32_t), sizeof(int32_t));
    
    // Insert initial value
    int32_t key = 5;
    int32_t val1 = 50;
    npk_map_insert_simple(map, &key, &val1);
    
    int32_t* get1 = (int32_t*)npk_map_get_simple(map, &key);
    TEST_ASSERT(*get1 == 50, "map_update: initial value is 50");
    
    // Update with new value
    int32_t val2 = 500;
    npk_map_insert_simple(map, &key, &val2);
    TEST_ASSERT(npk_map_length(map) == 1, "map_update: length still 1 after update");
    
    int32_t* get2 = (int32_t*)npk_map_get_simple(map, &key);
    TEST_ASSERT(*get2 == 500, "map_update: value updated to 500");
    
    // Clean up
    npk_map_free(map);
}

void test_map_remove() {
    printf("\n=== Map Remove Tests ===\n");
    
    AriaMap* map = npk_map_new_simple(sizeof(int32_t), sizeof(int32_t));
    
    // Insert multiple pairs
    int32_t keys[] = {1, 2, 3};
    int32_t vals[] = {10, 20, 30};
    for (int i = 0; i < 3; i++) {
        npk_map_insert_simple(map, &keys[i], &vals[i]);
    }
    TEST_ASSERT(npk_map_length(map) == 3, "map_remove: 3 pairs inserted");
    
    // Remove middle element
    AriaResultVoid remove_result = npk_map_remove(map, &keys[1]);
    TEST_ASSERT(!remove_result.is_error, "map_remove: removal succeeds");
    TEST_ASSERT(npk_map_length(map) == 2, "map_remove: length 2 after removal");
    
    // Verify removal
    bool has = npk_map_has(map, &keys[1]);
    TEST_ASSERT(!has, "map_remove: removed key doesn't exist");
    
    // Verify others still exist
    bool has0 = npk_map_has(map, &keys[0]);
    TEST_ASSERT(has0, "map_remove: key 1 still exists");
    
    bool has2 = npk_map_has(map, &keys[2]);
    TEST_ASSERT(has2, "map_remove: key 3 still exists");
    
    // Clean up
    npk_map_free(map);
}

void test_map_clear() {
    printf("\n=== Map Clear Tests ===\n");
    
    AriaMap* map = npk_map_new_simple(sizeof(int32_t), sizeof(int32_t));
    
    // Insert elements
    for (int i = 0; i < 10; i++) {
        int32_t key = i;
        int32_t val = i * 10;
        npk_map_insert_simple(map, &key, &val);
    }
    TEST_ASSERT(npk_map_length(map) == 10, "map_clear: 10 pairs inserted");
    
    // Clear map
    npk_map_clear(map);
    TEST_ASSERT(npk_map_length(map) == 0, "map_clear: length 0 after clear");
    
    // Verify empty
    int32_t key = 5;
    bool has = npk_map_has(map, &key);
    TEST_ASSERT(!has, "map_clear: no keys exist after clear");
    
    // Clean up
    npk_map_free(map);
}

void test_map_string_keys() {
    printf("\n=== Map String Key Tests ===\n");
    
    // Map with string keys (char*) and int values
    AriaMap* map = npk_map_new_simple(sizeof(char*), sizeof(int32_t));
    
    // Note: For production use, you'd want to copy the strings
    // Here we're using string literals which is fine for testing
    const char* key1 = "hello";
    int32_t val1 = 42;
    npk_map_insert_simple(map, &key1, &val1);
    
    const char* key2 = "world";
    int32_t val2 = 99;
    npk_map_insert_simple(map, &key2, &val2);
    
    TEST_ASSERT(npk_map_length(map) == 2, "map_string: 2 string keys inserted");
    
    // Note: This test is basic - proper string key handling would require
    // custom hash/compare functions which the map API supports
    
    // Clean up
    npk_map_free(map);
}

// ============================================================================
// Main Test Runner
// ============================================================================

int main() {
    printf("\n");
    printf("╔════════════════════════════════════════════════════════╗\n");
    printf("║  Collection API Tests (Array & Map)                   ║\n");
    printf("╚════════════════════════════════════════════════════════╝\n");
    
    // Run array test suites
    test_array_basic();
    test_array_push_pop();
    test_array_set();
    test_array_bounds();
    
    // Run map test suites
    test_map_basic();
    test_map_insert_get();
    test_map_has();
    test_map_update();
    test_map_remove();
    test_map_clear();
    test_map_string_keys();
    
    // Print summary
    printf("\n");
    printf("╔════════════════════════════════════════════════════════╗\n");
    printf("║  Test Results                                          ║\n");
    printf("╠════════════════════════════════════════════════════════╣\n");
    printf("║  Passed: %-4d                                          ║\n", tests_passed);
    printf("║  Failed: %-4d                                          ║\n", tests_failed);
    printf("╚════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    return (tests_failed == 0) ? 0 : 1;
}
