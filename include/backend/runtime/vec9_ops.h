// Aria vec9 - 9-Dimensional Vector for Toroidal AGI Memory
// Optimized for AVX-512 with padding for full register utilization
// Generic over element type with TBB safety awareness

#ifndef ARIA_VEC9_H
#define ARIA_VEC9_H

#include <cstdint>
#include <cstddef>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// vec9 Structure (9 elements + 7 padding = 16 for AVX-512)
// ============================================================================
// Alignment: 64 bytes (cache line)
// Layout: [x1, x2, x3, x4, x5, x6, x7, x8, x9, pad0...pad6]

#pragma pack(push, 1)

// vec9 with TBB8 elements
typedef struct alignas(64) {
    int8_t data[16];  // 9 active + 7 padding
} vec9_tbb8;

// vec9 with TBB16 elements
typedef struct alignas(64) {
    int16_t data[16];  // 9 active + 7 padding
} vec9_tbb16;

// vec9 with TBB32 elements
typedef struct alignas(64) {
    int32_t data[16];  // 9 active + 7 padding
} vec9_tbb32;

// vec9 with TBB64 elements
typedef struct alignas(64) {
    int64_t data[16];  // 9 active + 7 padding (requires 2 cache lines)
} vec9_tbb64;

// vec9 with float32 elements
typedef struct alignas(64) {
    float data[16];  // 9 active + 7 padding
} vec9_flt32;

// vec9 with float64 elements
typedef struct alignas(64) {
    double data[16];  // 9 active + 7 padding (requires 2 cache lines)
} vec9_flt64;

#pragma pack(pop)

// ============================================================================
// Construction and Initialization
// ============================================================================

vec9_tbb32 npk_vec9_tbb32_zero();
vec9_tbb32 npk_vec9_tbb32_err();
vec9_tbb32 npk_vec9_tbb32_from_array(const int32_t* arr);  // Takes 9 elements

// ============================================================================
// Element Access
// ============================================================================
// Index: 0-8 (dimension 1-9)

int32_t npk_vec9_tbb32_get(const vec9_tbb32* v, int idx);
void npk_vec9_tbb32_set(vec9_tbb32* v, int idx, int32_t val);

// ============================================================================
// Vector Operations
// ============================================================================

// Addition (element-wise)
vec9_tbb32 npk_vec9_tbb32_add(const vec9_tbb32* a, const vec9_tbb32* b);

// Subtraction (element-wise)
vec9_tbb32 npk_vec9_tbb32_sub(const vec9_tbb32* a, const vec9_tbb32* b);

// Scalar multiplication
vec9_tbb32 npk_vec9_tbb32_scale(const vec9_tbb32* v, int32_t scalar);

// Dot product
int32_t npk_vec9_tbb32_dot(const vec9_tbb32* a, const vec9_tbb32* b);

// Magnitude squared (avoids sqrt for performance)
int64_t npk_vec9_tbb32_mag_sq(const vec9_tbb32* v);

// ============================================================================
// Toroidal Operations (for 9D torus wrapping)
// ============================================================================

// Wrap coordinates to toroidal bounds [0, dim) for each dimension
vec9_tbb32 npk_vec9_tbb32_wrap(const vec9_tbb32* v, const int32_t* dims);

// Toroidal distance (geodesic on 9D torus)
int64_t npk_vec9_tbb32_toroidal_dist_sq(const vec9_tbb32* a, const vec9_tbb32* b, const int32_t* dims);

// ============================================================================
// Safety Checks
// ============================================================================

int npk_vec9_tbb32_has_err(const vec9_tbb32* v);  // Returns 1 if any element is ERR
int npk_vec9_tbb32_is_zero(const vec9_tbb32* v);  // Returns 1 if all elements are 0

// ============================================================================
// String Conversion (for debugging)
// ============================================================================

int npk_vec9_tbb32_to_string(char* buffer, size_t size, const vec9_tbb32* v);

#ifdef __cplusplus
}
#endif

#endif // ARIA_VEC9_H
