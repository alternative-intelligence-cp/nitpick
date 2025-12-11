/**
 * tests/ternary/test_ternary_ops.cpp
 * 
 * Unit tests for balanced ternary operations
 * Compile with: g++ -std=c++17 -I../../src/backend test_ternary_ops.cpp ../../src/backend/ternary_ops.cpp -o test_ternary_ops
 */

#include "../../src/backend/ternary_ops.h"
#include <iostream>
#include <cassert>
#include <cstdint>

using namespace aria::backend;

void test_packing() {
    std::cout << "Testing packing/unpacking...\n";
    
    // Test simple case: [0, 0, 0, 0, 0]
    trit trits1[5] = {0, 0, 0, 0, 0};
    uint8_t packed1 = TernaryOps::packTrybble(trits1);
    assert(packed1 == 121);  // Bias = 121
    std::cout << "  [0,0,0,0,0] -> " << (int)packed1 << " ✓\n";
    
    // Test all positive: [1, 1, 1, 1, 1]
    trit trits2[5] = {1, 1, 1, 1, 1};  // 1+3+9+27+81 = 121
    uint8_t packed2 = TernaryOps::packTrybble(trits2);
    assert(packed2 == 242);  // 121 + 121
    std::cout << "  [1,1,1,1,1] -> " << (int)packed2 << " ✓\n";
    
    // Test all negative: [-1, -1, -1, -1, -1]
    trit trits3[5] = {-1, -1, -1, -1, -1};  // -121
    uint8_t packed3 = TernaryOps::packTrybble(trits3);
    assert(packed3 == 0);  // 121 - 121
    std::cout << "  [-1,-1,-1,-1,-1] -> " << (int)packed3 << " ✓\n";
}

void test_unpacking() {
    std::cout << "\nTesting unpacking with LUT...\n";
    
    TernaryOps::initialize();
    
    // Unpack bias value (should be all zeros)
    trit result1[5];
    bool success1 = TernaryOps::unpackTrybble(121, result1);
    assert(success1);
    assert(result1[0] == 0 && result1[1] == 0 && result1[2] == 0 && 
           result1[3] == 0 && result1[4] == 0);
    std::cout << "  121 -> [0,0,0,0,0] ✓\n";
    
    // Unpack 242 (should be all ones)
    trit result2[5];
    bool success2 = TernaryOps::unpackTrybble(242, result2);
    assert(success2);
    assert(result2[0] == 1 && result2[1] == 1 && result2[2] == 1 && 
           result2[3] == 1 && result2[4] == 1);
    std::cout << "  242 -> [1,1,1,1,1] ✓\n";
}

void test_round_trip() {
    std::cout << "\nTesting round-trip (pack -> unpack)...\n";
    
    TernaryOps::initialize();
    
    // Test various combinations
    trit original[10] = {1, 0, -1, 1, 0, -1, 1, 0, -1, 0};
    uint16_t packed = TernaryOps::packTryte(original);
    
    trit unpacked[10];
    bool success = TernaryOps::unpackTryte(packed, unpacked);
    assert(success);
    
    for (int i = 0; i < 10; i++) {
        assert(original[i] == unpacked[i]);
    }
    std::cout << "  Round-trip successful ✓\n";
}

void test_binary_conversion() {
    std::cout << "\nTesting binary ↔ ternary conversion...\n";
    
    TernaryOps::initialize();
    
    // Test positive number
    int32_t val1 = 100;
    uint16_t tryte1 = TernaryOps::binaryToTryte(val1);
    int32_t back1 = TernaryOps::tryteToBinary(tryte1);
    assert(back1 == val1);
    std::cout << "  100 -> tryte -> 100 ✓\n";
    
    // Test negative number
    int32_t val2 = -500;
    uint16_t tryte2 = TernaryOps::binaryToTryte(val2);
    int32_t back2 = TernaryOps::tryteToBinary(tryte2);
    assert(back2 == val2);
    std::cout << "  -500 -> tryte -> -500 ✓\n";
    
    // Test zero
    int32_t val3 = 0;
    uint16_t tryte3 = TernaryOps::binaryToTryte(val3);
    int32_t back3 = TernaryOps::tryteToBinary(tryte3);
    assert(back3 == val3);
    std::cout << "  0 -> tryte -> 0 ✓\n";
    
    // Test overflow
    int32_t val4 = 50000;  // > TRYTE_MAX (29,524)
    uint16_t tryte4 = TernaryOps::binaryToTryte(val4);
    assert(tryte4 == TRYTE_ERR);
    std::cout << "  50000 (overflow) -> ERR ✓\n";
}

void test_addition() {
    std::cout << "\nTesting addition...\n";
    
    TernaryOps::initialize();
    
    // Basic addition
    uint16_t a1 = TernaryOps::binaryToTryte(100);
    uint16_t b1 = TernaryOps::binaryToTryte(50);
    uint16_t sum1 = TernaryOps::addTrytes(a1, b1);
    int32_t result1 = TernaryOps::tryteToBinary(sum1);
    assert(result1 == 150);
    std::cout << "  100 + 50 = 150 ✓\n";
    
    // Negative addition
    uint16_t a2 = TernaryOps::binaryToTryte(-200);
    uint16_t b2 = TernaryOps::binaryToTryte(50);
    uint16_t sum2 = TernaryOps::addTrytes(a2, b2);
    int32_t result2 = TernaryOps::tryteToBinary(sum2);
    assert(result2 == -150);
    std::cout << "  -200 + 50 = -150 ✓\n";
    
    // Overflow
    uint16_t a3 = TernaryOps::binaryToTryte(29000);
    uint16_t b3 = TernaryOps::binaryToTryte(1000);
    uint16_t sum3 = TernaryOps::addTrytes(a3, b3);
    assert(sum3 == TRYTE_ERR);
    std::cout << "  29000 + 1000 = ERR (overflow) ✓\n";
}

void test_subtraction() {
    std::cout << "\nTesting subtraction...\n";
    
    TernaryOps::initialize();
    
    uint16_t a = TernaryOps::binaryToTryte(200);
    uint16_t b = TernaryOps::binaryToTryte(75);
    uint16_t diff = TernaryOps::subtractTrytes(a, b);
    int32_t result = TernaryOps::tryteToBinary(diff);
    assert(result == 125);
    std::cout << "  200 - 75 = 125 ✓\n";
}

void test_negation() {
    std::cout << "\nTesting negation...\n";
    
    TernaryOps::initialize();
    
    uint16_t a = TernaryOps::binaryToTryte(42);
    uint16_t neg = TernaryOps::negateTryte(a);
    int32_t result = TernaryOps::tryteToBinary(neg);
    assert(result == -42);
    std::cout << "  -(42) = -42 ✓\n";
    
    // Double negation
    uint16_t neg2 = TernaryOps::negateTryte(neg);
    int32_t result2 = TernaryOps::tryteToBinary(neg2);
    assert(result2 == 42);
    std::cout << "  -(-42) = 42 ✓\n";
}

void test_multiplication() {
    std::cout << "\nTesting multiplication...\n";
    
    TernaryOps::initialize();
    
    uint16_t a = TernaryOps::binaryToTryte(12);
    uint16_t b = TernaryOps::binaryToTryte(10);
    uint16_t product = TernaryOps::multiplyTrytes(a, b);
    int32_t result = TernaryOps::tryteToBinary(product);
    assert(result == 120);
    std::cout << "  12 * 10 = 120 ✓\n";
}

void test_division() {
    std::cout << "\nTesting division...\n";
    
    TernaryOps::initialize();
    
    // Basic division
    uint16_t a = TernaryOps::binaryToTryte(100);
    uint16_t b = TernaryOps::binaryToTryte(5);
    uint16_t quotient = TernaryOps::divideTrytes(a, b);
    int32_t result = TernaryOps::tryteToBinary(quotient);
    assert(result == 20);
    std::cout << "  100 / 5 = 20 ✓\n";
    
    // Division by zero
    uint16_t c = TernaryOps::binaryToTryte(100);
    uint16_t d = TernaryOps::binaryToTryte(0);
    uint16_t quotient2 = TernaryOps::divideTrytes(c, d);
    assert(quotient2 == TRYTE_ERR);
    std::cout << "  100 / 0 = ERR ✓\n";
}

void test_sticky_error() {
    std::cout << "\nTesting sticky error propagation...\n";
    
    TernaryOps::initialize();
    
    uint16_t err = TRYTE_ERR;
    uint16_t x = TernaryOps::binaryToTryte(100);
    
    // ERR + x = ERR
    uint16_t result1 = TernaryOps::addTrytes(err, x);
    assert(result1 == TRYTE_ERR);
    std::cout << "  ERR + 100 = ERR ✓\n";
    
    // x + ERR = ERR
    uint16_t result2 = TernaryOps::addTrytes(x, err);
    assert(result2 == TRYTE_ERR);
    std::cout << "  100 + ERR = ERR ✓\n";
    
    // ERR * x = ERR
    uint16_t result3 = TernaryOps::multiplyTrytes(err, x);
    assert(result3 == TRYTE_ERR);
    std::cout << "  ERR * 100 = ERR ✓\n";
}

int main() {
    std::cout << "=== Balanced Ternary Operations Test Suite ===\n\n";
    
    test_packing();
    test_unpacking();
    test_round_trip();
    test_binary_conversion();
    test_addition();
    test_subtraction();
    test_negation();
    test_multiplication();
    test_division();
    test_sticky_error();
    
    std::cout << "\n=== All tests passed! ===\n";
    return 0;
}
