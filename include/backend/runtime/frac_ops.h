// Aria Fraction Operations Header
// Mixed-number exact rational arithmetic with TBB error propagation

#ifndef ARIA_FRAC_OPS_H
#define ARIA_FRAC_OPS_H

#include <cstdint>
#include <cstddef>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Fraction Structure (matches Aria language struct layout)
// ============================================================================

#pragma pack(push, 1)

struct Frac8 {
    int8_t whole;
    int8_t num;
    int8_t denom;
};

struct Frac16 {
    int16_t whole;
    int16_t num;
    int16_t denom;
};

struct Frac32 {
    int32_t whole;
    int32_t num;
    int32_t denom;
};

struct Frac64 {
    int64_t whole;
    int64_t num;
    int64_t denom;
};

#pragma pack(pop)

// ============================================================================
// Construction from parts
// ============================================================================

Frac8  frac8_from_parts(int8_t whole, int8_t num, int8_t denom);
Frac16 frac16_from_parts(int16_t whole, int16_t num, int16_t denom);
Frac32 frac32_from_parts(int32_t whole, int32_t num, int32_t denom);
Frac64 frac64_from_parts(int64_t whole, int64_t num, int64_t denom);

// ============================================================================
// GCD Operations
// ============================================================================

int8_t aria_gcd8(int8_t a, int8_t b);
int16_t aria_gcd16(int16_t a, int16_t b);
int32_t aria_gcd32(int32_t a, int32_t b);
int64_t aria_gcd64(int64_t a, int64_t b);

// ============================================================================
// Arithmetic Operations
// ============================================================================

// Addition
void aria_frac8_add(Frac8* result, const Frac8* a, const Frac8* b);
void aria_frac16_add(Frac16* result, const Frac16* a, const Frac16* b);
void aria_frac32_add(Frac32* result, const Frac32* a, const Frac32* b);
void aria_frac64_add(Frac64* result, const Frac64* a, const Frac64* b);

// Subtraction
void aria_frac8_sub(Frac8* result, const Frac8* a, const Frac8* b);
void aria_frac16_sub(Frac16* result, const Frac16* a, const Frac16* b);
void aria_frac32_sub(Frac32* result, const Frac32* a, const Frac32* b);
void aria_frac64_sub(Frac64* result, const Frac64* a, const Frac64* b);

// Multiplication
void aria_frac8_mul(Frac8* result, const Frac8* a, const Frac8* b);
void aria_frac16_mul(Frac16* result, const Frac16* a, const Frac16* b);
void aria_frac32_mul(Frac32* result, const Frac32* a, const Frac32* b);
void aria_frac64_mul(Frac64* result, const Frac64* a, const Frac64* b);

// Division
void aria_frac8_div(Frac8* result, const Frac8* a, const Frac8* b);
void aria_frac16_div(Frac16* result, const Frac16* a, const Frac16* b);
void aria_frac32_div(Frac32* result, const Frac32* a, const Frac32* b);
void aria_frac64_div(Frac64* result, const Frac64* a, const Frac64* b);

// Negation
void aria_frac8_neg(Frac8* result, const Frac8* a);
void aria_frac16_neg(Frac16* result, const Frac16* a);
void aria_frac32_neg(Frac32* result, const Frac32* a);
void aria_frac64_neg(Frac64* result, const Frac64* a);

// ============================================================================
// Comparison Operations  
// ============================================================================
// Returns: -1 (a < b), 0 (a == b), +1 (a > b)

int32_t aria_frac8_cmp(const Frac8* a, const Frac8* b);
int32_t aria_frac16_cmp(const Frac16* a, const Frac16* b);
int32_t aria_frac32_cmp(const Frac32* a, const Frac32* b);
int32_t aria_frac64_cmp(const Frac64* a, const Frac64* b);

// ============================================================================
// Conversion Operations
// ============================================================================

// To integer (rounds toward zero)
int8_t aria_frac8_to_int(const Frac8* f);
int16_t aria_frac16_to_int(const Frac16* f);
int32_t aria_frac32_to_int(const Frac32* f);
int64_t aria_frac64_to_int(const Frac64* f);

// To float
float aria_frac8_to_float(const Frac8* f);
float aria_frac16_to_float(const Frac16* f);
float aria_frac32_to_float(const Frac32* f);
double aria_frac64_to_float(const Frac64* f);

// ============================================================================
// String Conversion
// ============================================================================
// Returns number of characters written (excluding null terminator)

int aria_frac8_to_string(char* buffer, size_t size, const Frac8* f);
int aria_frac16_to_string(char* buffer, size_t size, const Frac16* f);
int aria_frac32_to_string(char* buffer, size_t size, const Frac32* f);
int aria_frac64_to_string(char* buffer, size_t size, const Frac64* f);

#ifdef __cplusplus
}
#endif

#endif // ARIA_FRAC_OPS_H
