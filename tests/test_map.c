#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "runtime/map.h"

int main() {
    printf("Testing Map Module\n");
    printf("==================\n\n");
    
    // Create a map for int64 -> string pointers
    AriaMap* map = aria_map_new_simple(sizeof(int64_t), sizeof(char*));
    printf("✓ Created map\n");
    
    // Insert some key-value pairs
    int64_t key1 = 42;
    const char* val1 = "Hello";
    aria_map_insert_simple(map, &key1, &val1);
    printf("✓ Inserted (42 -> \"Hello\")\n");
    
    int64_t key2 = 100;
    const char* val2 = "World";
    aria_map_insert_simple(map, &key2, &val2);
    printf("✓ Inserted (100 -> \"World\")\n");
    
    int64_t key3 = 7;
    const char* val3 = "Test";
    aria_map_insert_simple(map, &key3, &val3);
    printf("✓ Inserted (7 -> \"Test\")\n");
    
    // Check length
    printf("\nMap length: %zu\n", aria_map_length(map));
    
    // Test has
    printf("\nTesting 'has':\n");
    printf("  has(42): %s\n", aria_map_has(map, &key1) ? "true" : "false");
    printf("  has(100): %s\n", aria_map_has(map, &key2) ? "true" : "false");
    int64_t missing = 999;
    printf("  has(999): %s\n", aria_map_has(map, &missing) ? "true" : "false");
    
    // Test get
    printf("\nTesting 'get':\n");
    const char** result1 = (const char**)aria_map_get_simple(map, &key1);
    printf("  get(42): \"%s\"\n", *result1);
    
    const char** result2 = (const char**)aria_map_get_simple(map, &key2);
    printf("  get(100): \"%s\"\n", *result2);
    
    const char** result3 = (const char**)aria_map_get_simple(map, &key3);
    printf("  get(7): \"%s\"\n", *result3);
    
    // Test update (insert existing key)
    printf("\nUpdating existing key:\n");
    const char* new_val1 = "Updated!";
    aria_map_insert_simple(map, &key1, &new_val1);
    const char** updated = (const char**)aria_map_get_simple(map, &key1);
    printf("  get(42) after update: \"%s\"\n", *updated);
    printf("  length after update: %zu (should still be 3)\n", aria_map_length(map));
    
    // Test remove
    printf("\nRemoving key 100:\n");
    aria_map_remove(map, &key2);
    printf("  has(100) after remove: %s\n", aria_map_has(map, &key2) ? "true" : "false");
    printf("  length after remove: %zu\n", aria_map_length(map));
    
    // Test stress - insert many items to trigger resize
    printf("\nStress test (inserting 100 items to test resize):\n");
    for (int64_t i = 1000; i < 1100; i++) {
        const char* val = "bulk";
        aria_map_insert_simple(map, &i, &val);
    }
    printf("  length after bulk insert: %zu\n", aria_map_length(map));
    
    // Verify some bulk items exist
    int64_t test_key = 1050;
    printf("  has(1050): %s\n", aria_map_has(map, &test_key) ? "true" : "false");
    
    // Clean up
    aria_map_free(map);
    printf("\n✓ All tests passed!\n");
    
    return 0;
}
