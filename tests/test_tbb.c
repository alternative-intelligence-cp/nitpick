/*
 * TBB (Twisted Balanced Binary) Type System Tests
 * 
 * Tests the error-propagating signed integer types with symmetric ranges.
 * TBB types use sentinel values for error propagation:
 *   tbb8:  ERR = -128, range = [-127, +127]
 *   tbb16: ERR = -32768, range = [-32767, +32767]
 *   tbb32: ERR = INT32_MIN, range = [INT32_MIN+1, INT32_MAX]
 *   tbb64: ERR = INT64_MIN, range = [INT64_MIN+1, INT64_MAX]
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "../include/runtime/tbb.h"

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
// TBB8 Tests
// ============================================================================

void test_tbb8_basic() {
    printf("\n=== TBB8 Basic Tests ===\n");
    
    // Test from_int with valid range
    int8_t t1 = tbb8_from_int(42);
    TEST_ASSERT(t1 == 42, "tbb8_from_int: 42 -> 42");
    TEST_ASSERT(!tbb8_is_err(t1), "tbb8_is_err: 42 is not ERR");
    
    // Test from_int with negative value
    int8_t t2 = tbb8_from_int(-100);
    TEST_ASSERT(t2 == -100, "tbb8_from_int: -100 -> -100");
    
    // Test from_int with max valid value
    int8_t t3 = tbb8_from_int(127);
    TEST_ASSERT(t3 == 127, "tbb8_from_int: max valid (127)");
    
    // Test from_int with min valid value
    int8_t t4 = tbb8_from_int(-127);
    TEST_ASSERT(t4 == -127, "tbb8_from_int: min valid (-127)");
    
    // Test from_int with overflow (positive)
    int8_t t5 = tbb8_from_int(128);
    TEST_ASSERT(tbb8_is_err(t5), "tbb8_from_int: overflow 128 -> ERR");
    
    // Test from_int with overflow (negative) 
    int8_t t6 = tbb8_from_int(-128);
    TEST_ASSERT(tbb8_is_err(t6), "tbb8_from_int: -128 (ERR sentinel) -> ERR");
    
    // Test to_int round-trip
    int8_t t7 = tbb8_from_int(77);
    int64_t back = tbb8_to_int(t7);
    TEST_ASSERT(back == 77, "tbb8_to_int: round-trip 77");
}

void test_tbb8_arithmetic() {
    printf("\n=== TBB8 Arithmetic Tests ===\n");
    
    // Test addition
    int8_t a = tbb8_from_int(50);
    int8_t b = tbb8_from_int(30);
    int8_t sum = tbb8_add(a, b);
    TEST_ASSERT(sum == 80, "tbb8_add: 50 + 30 = 80");
    
    // Test addition overflow
    int8_t c = tbb8_from_int(100);
    int8_t d = tbb8_from_int(100);
    int8_t overflow_sum = tbb8_add(c, d);
    TEST_ASSERT(tbb8_is_err(overflow_sum), "tbb8_add: overflow 100+100 -> ERR");
    
    // Test subtraction
    int8_t e = tbb8_from_int(50);
    int8_t f = tbb8_from_int(30);
    int8_t diff = tbb8_sub(e, f);
    TEST_ASSERT(diff == 20, "tbb8_sub: 50 - 30 = 20");
    
    // Test subtraction underflow
    int8_t g = tbb8_from_int(-100);
    int8_t h = tbb8_from_int(100);
    int8_t underflow = tbb8_sub(g, h);
    TEST_ASSERT(tbb8_is_err(underflow), "tbb8_sub: underflow -100-100 -> ERR");
    
    // Test multiplication
    int8_t i = tbb8_from_int(10);
    int8_t j = tbb8_from_int(5);
    int8_t prod = tbb8_mul(i, j);
    TEST_ASSERT(prod == 50, "tbb8_mul: 10 * 5 = 50");
    
    // Test multiplication overflow
    int8_t k = tbb8_from_int(20);
    int8_t l = tbb8_from_int(20);
    int8_t overflow_prod = tbb8_mul(k, l);
    TEST_ASSERT(tbb8_is_err(overflow_prod), "tbb8_mul: overflow 20*20 -> ERR");
    
    // Test division
    int8_t m = tbb8_from_int(100);
    int8_t n = tbb8_from_int(5);
    int8_t quot = tbb8_div(m, n);
    TEST_ASSERT(quot == 20, "tbb8_div: 100 / 5 = 20");
    
    // Test division by zero
    int8_t o = tbb8_from_int(100);
    int8_t p = tbb8_from_int(0);
    int8_t div_zero = tbb8_div(o, p);
    TEST_ASSERT(tbb8_is_err(div_zero), "tbb8_div: division by zero -> ERR");
}

void test_tbb8_unary() {
    printf("\n=== TBB8 Unary Operations ===\n");
    
    // Test negation
    int8_t a = tbb8_from_int(42);
    int8_t neg_a = tbb8_neg(a);
    TEST_ASSERT(neg_a == -42, "tbb8_neg: -(42) = -42");
    
    // Test negation of negative
    int8_t b = tbb8_from_int(-42);
    int8_t neg_b = tbb8_neg(b);
    TEST_ASSERT(neg_b == 42, "tbb8_neg: -(-42) = 42");
    
    // Test negation of max (symmetric range allows this!)
    int8_t c = tbb8_from_int(127);
    int8_t neg_c = tbb8_neg(c);
    TEST_ASSERT(neg_c == -127, "tbb8_neg: -(127) = -127 (symmetric!)");
    
    // Test absolute value
    int8_t d = tbb8_from_int(-42);
    int8_t abs_d = tbb8_abs(d);
    TEST_ASSERT(abs_d == 42, "tbb8_abs: abs(-42) = 42");
    
    // Test abs of positive
    int8_t e = tbb8_from_int(42);
    int8_t abs_e = tbb8_abs(e);
    TEST_ASSERT(abs_e == 42, "tbb8_abs: abs(42) = 42");
    
    // Test abs of min (symmetric range allows this!)
    int8_t f = tbb8_from_int(-127);
    int8_t abs_f = tbb8_abs(f);
    TEST_ASSERT(abs_f == 127, "tbb8_abs: abs(-127) = 127 (symmetric!)");
}

void test_tbb8_error_propagation() {
    printf("\n=== TBB8 Error Propagation (Sticky Errors) ===\n");
    
    // Test ERR + valid = ERR
    int8_t err = tbb8_from_int(200); // overflow -> ERR
    int8_t valid = tbb8_from_int(10);
    int8_t result = tbb8_add(err, valid);
    TEST_ASSERT(tbb8_is_err(result), "tbb8_add: ERR + 10 = ERR (sticky)");
    
    // Test valid + ERR = ERR
    int8_t result2 = tbb8_add(valid, err);
    TEST_ASSERT(tbb8_is_err(result2), "tbb8_add: 10 + ERR = ERR (sticky)");
    
    // Test ERR * valid = ERR
    int8_t result3 = tbb8_mul(err, valid);
    TEST_ASSERT(tbb8_is_err(result3), "tbb8_mul: ERR * 10 = ERR (sticky)");
    
    // Test neg(ERR) = ERR
    int8_t result4 = tbb8_neg(err);
    TEST_ASSERT(tbb8_is_err(result4), "tbb8_neg: neg(ERR) = ERR (sticky)");
    
    // Test abs(ERR) = ERR
    int8_t result5 = tbb8_abs(err);
    TEST_ASSERT(tbb8_is_err(result5), "tbb8_abs: abs(ERR) = ERR (sticky)");
}

// ============================================================================
// TBB16 Tests
// ============================================================================

void test_tbb16_basic() {
    printf("\n=== TBB16 Basic Tests ===\n");
    
    int16_t t1 = tbb16_from_int(1000);
    TEST_ASSERT(t1 == 1000, "tbb16_from_int: 1000 -> 1000");
    TEST_ASSERT(!tbb16_is_err(t1), "tbb16_is_err: 1000 is not ERR");
    
    int16_t t2 = tbb16_from_int(32767);
    TEST_ASSERT(t2 == 32767, "tbb16_from_int: max valid (32767)");
    
    int16_t t3 = tbb16_from_int(-32767);
    TEST_ASSERT(t3 == -32767, "tbb16_from_int: min valid (-32767)");
    
    int16_t t4 = tbb16_from_int(40000);
    TEST_ASSERT(tbb16_is_err(t4), "tbb16_from_int: overflow 40000 -> ERR");
}

void test_tbb16_arithmetic() {
    printf("\n=== TBB16 Arithmetic Tests ===\n");
    
    int16_t a = tbb16_from_int(10000);
    int16_t b = tbb16_from_int(5000);
    int16_t sum = tbb16_add(a, b);
    TEST_ASSERT(sum == 15000, "tbb16_add: 10000 + 5000 = 15000");
    
    int16_t c = tbb16_from_int(30000);
    int16_t d = tbb16_from_int(30000);
    int16_t overflow = tbb16_add(c, d);
    TEST_ASSERT(tbb16_is_err(overflow), "tbb16_add: overflow 30000+30000 -> ERR");
    
    int16_t e = tbb16_from_int(100);
    int16_t f = tbb16_from_int(50);
    int16_t prod = tbb16_mul(e, f);
    TEST_ASSERT(prod == 5000, "tbb16_mul: 100 * 50 = 5000");
}

// ============================================================================
// TBB32 Tests
// ============================================================================

void test_tbb32_basic() {
    printf("\n=== TBB32 Basic Tests ===\n");
    
    int32_t t1 = tbb32_from_int(100000);
    TEST_ASSERT(t1 == 100000, "tbb32_from_int: 100000 -> 100000");
    
    int32_t t2 = tbb32_from_int(2147483647);
    TEST_ASSERT(t2 == 2147483647, "tbb32_from_int: max valid (INT32_MAX)");
    
    int32_t t3 = tbb32_from_int(-2147483647);
    TEST_ASSERT(t3 == -2147483647, "tbb32_from_int: min valid (INT32_MIN+1)");
}

void test_tbb32_arithmetic() {
    printf("\n=== TBB32 Arithmetic Tests ===\n");
    
    int32_t a = tbb32_from_int(1000000);
    int32_t b = tbb32_from_int(500000);
    int32_t sum = tbb32_add(a, b);
    TEST_ASSERT(sum == 1500000, "tbb32_add: 1000000 + 500000 = 1500000");
    
    int32_t c = tbb32_from_int(2000000000);
    int32_t d = tbb32_from_int(2000000000);
    int32_t overflow = tbb32_add(c, d);
    TEST_ASSERT(tbb32_is_err(overflow), "tbb32_add: overflow 2B+2B -> ERR");
}

// ============================================================================
// TBB64 Tests
// ============================================================================

void test_tbb64_basic() {
    printf("\n=== TBB64 Basic Tests ===\n");
    
    int64_t t1 = tbb64_from_int(1000000000LL);
    TEST_ASSERT(t1 == 1000000000LL, "tbb64_from_int: 1 billion");
    
    int64_t t2 = tbb64_from_int(9223372036854775807LL); // INT64_MAX
    TEST_ASSERT(t2 == 9223372036854775807LL, "tbb64_from_int: max valid (INT64_MAX)");
}

void test_tbb64_arithmetic() {
    printf("\n=== TBB64 Arithmetic Tests ===\n");
    
    int64_t a = tbb64_from_int(1000000000LL);
    int64_t b = tbb64_from_int(500000000LL);
    int64_t sum = tbb64_add(a, b);
    TEST_ASSERT(sum == 1500000000LL, "tbb64_add: 1B + 500M = 1.5B");
    
    int64_t c = tbb64_from_int(100);
    int64_t d = tbb64_from_int(50);
    int64_t prod = tbb64_mul(c, d);
    TEST_ASSERT(prod == 5000, "tbb64_mul: 100 * 50 = 5000");
}

// ============================================================================
// Main Test Runner
// ============================================================================

int main() {
    printf("\n");
    printf("╔════════════════════════════════════════════════════════╗\n");
    printf("║  TBB (Twisted Balanced Binary) Type System Tests      ║\n");
    printf("╚════════════════════════════════════════════════════════╝\n");
    
    // Run all test suites
    test_tbb8_basic();
    test_tbb8_arithmetic();
    test_tbb8_unary();
    test_tbb8_error_propagation();
    
    test_tbb16_basic();
    test_tbb16_arithmetic();
    
    test_tbb32_basic();
    test_tbb32_arithmetic();
    
    test_tbb64_basic();
    test_tbb64_arithmetic();
    
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
