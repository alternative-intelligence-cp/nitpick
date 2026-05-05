// C++ test for fix256 GPU division
// Tests that divisions compile and give sensible results

#include "runtime/fix256.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>

// Use the GPU version
extern "C" {
    npk_fix256_t npk_fix256_div_gpu(npk_fix256_t a, npk_fix256_t b);
}

// Helper to create fix256 from int64
npk_fix256_t make_fix256_int(int64_t val) {
    npk_fix256_t result;
    memset(&result, 0, sizeof(result));
    
    if (val >= 0) {
        // Positive: high 128 bits (integer part)
        result.limbs[2] = (uint64_t)val;
        result.limbs[3] = 0;
    } else {
        // Negative: two's complement
        result.limbs[2] = (uint64_t)val;
        result.limbs[3] = ~0ULL;  // Sign extend
    }
    
    return result;
}

// Helper to extract int64 from fix256
int64_t fix256_get_int(npk_fix256_t val) {
    return (int64_t)val.limbs[2];
}

// Helper to get float representation
double fix256_get_float(npk_fix256_t val) {
    // Integer part in limbs[2:3], fractional in limbs[0:1]
    double integer_part = (double)(int64_t)val.limbs[2];
    double frac_part = (double)val.limbs[0] / (double)(1ULL << 63) / 2.0;
    if (val.limbs[1] != 0) {
        frac_part += (double)val.limbs[1] / (double)(1ULL << 63) / 2.0 * (double)(1ULL << 63) * 2.0;
    }
    return integer_part + frac_part;
}

int main() {
    printf("=== fix256 GPU Division Tests ===\n\n");
    
    // Test 1: 10 / 2 = 5
    npk_fix256_t a1 = make_fix256_int(10);
    npk_fix256_t b1 = make_fix256_int(2);
    npk_fix256_t result1 = npk_fix256_div_gpu(a1, b1);
    printf("Test 1: 10 / 2 = %ld (expected 5)\n", fix256_get_int(result1));
    
    // Test 2: 7 / 2 = 3.5
    npk_fix256_t a2 = make_fix256_int(7);
    npk_fix256_t b2 = make_fix256_int(2);
    npk_fix256_t result2 = npk_fix256_div_gpu(a2, b2);
    printf("Test 2: 7 / 2 = %f (expected 3.5)\n", fix256_get_float(result2));
    
    // Test 3: 100 / 3 = 33.333...
    npk_fix256_t a3 = make_fix256_int(100);
    npk_fix256_t b3 = make_fix256_int(3);
    npk_fix256_t result3 = npk_fix256_div_gpu(a3, b3);
    printf("Test 3: 100 / 3 = %f (expected ~33.333)\n", fix256_get_float(result3));
    
    // Test 4: 42 / 1 = 42
    npk_fix256_t a4 = make_fix256_int(42);
    npk_fix256_t b4 = make_fix256_int(1);
    npk_fix256_t result4 = npk_fix256_div_gpu(a4, b4);
    printf("Test 4: 42 / 1 = %ld (expected 42)\n", fix256_get_int(result4));
    
    // Test 5: 123 / 123 = 1
    npk_fix256_t a5 = make_fix256_int(123);
    npk_fix256_t result5 = npk_fix256_div_gpu(a5, a5);
    printf("Test 5: 123 / 123 = %ld (expected 1)\n", fix256_get_int(result5));
    
    // Test 6: -10 / 2 = -5
    npk_fix256_t a6 = make_fix256_int(-10);
    npk_fix256_t b6 = make_fix256_int(2);
    npk_fix256_t result6 = npk_fix256_div_gpu(a6, b6);
    printf("Test 6: -10 / 2 = %ld (expected -5)\n", fix256_get_int(result6));
    
    // Test 7: -10 / -2 = 5
    npk_fix256_t a7 = make_fix256_int(-10);
    npk_fix256_t b7 = make_fix256_int(-2);
    npk_fix256_t result7 = npk_fix256_div_gpu(a7, b7);
    printf("Test 7: -10 / -2 = %ld (expected 5)\n", fix256_get_int(result7));
    
    printf("\n=== Tests Complete ===\n");
    
    return 0;
}

