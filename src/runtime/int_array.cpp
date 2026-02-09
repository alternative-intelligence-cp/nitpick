// Runtime support for int32 array operations
// Provides helper functions for IntVec Type

#include <cstdint>

extern "C" {
    // Get element from int32 array at index
    int32_t int_array_get(void* buffer, int32_t index) {
        int32_t* array = static_cast<int32_t*>(buffer);
        return array[index];
    }
    
    // Set element in int32 array at index
    void int_array_set(void* buffer, int32_t index, int32_t value) {
        int32_t* array = static_cast<int32_t*>(buffer);
        array[index] = value;
    }
}
