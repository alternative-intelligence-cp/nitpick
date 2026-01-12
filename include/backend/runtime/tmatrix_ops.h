#ifndef ARIA_TMATRIX_OPS_H
#define ARIA_TMATRIX_OPS_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Twisted Matrix - safe matrix operations with wild memory and sentinel propagation
// 
// Layout: Row-major by default, but supports arbitrary striding for views/slices
// Memory: "Wild" memory managed separately (not embedded in struct)
// Safety: Pre-flight sentinel checks prevent "Infected Matrix" propagation
//
// ERR Propagation Rules:
// - Any matrix with ERR in dimensions is fully tainted
// - Operations pre-check all inputs for ERR before touching data
// - Single ERR in matrix data taints entire result (sticky)
// - GEMM kernel performs sentinel-aware multiply-accumulate
//
// Memory Model:
// - Matrices own their data (allocation/deallocation)
// - Views/slices reference parent data (stride arithmetic)
// - Reference counting to prevent use-after-free

// ============================================================================
// tbb32-based Matrix (for indexing and dimensions)
// ============================================================================

typedef struct {
    int32_t rows;       // Number of rows (ERR = tainted matrix)
    int32_t cols;       // Number of columns (ERR = tainted matrix)
    int32_t stride_row; // Bytes between consecutive rows (for slicing)
    int32_t stride_col; // Bytes between consecutive columns (usually sizeof(T))
    void* data;         // Pointer to matrix data (wild memory)
    int32_t owns_data;  // 1 if this matrix owns the data, 0 for views
} tmatrix_tbb32;

typedef struct {
    int64_t rows;
    int64_t cols;
    int64_t stride_row;
    int64_t stride_col;
    void* data;
    int32_t owns_data;
} tmatrix_tbb64;

typedef struct {
    float rows;
    float cols;
    float stride_row;
    float stride_col;
    void* data;
    int32_t owns_data;
} tmatrix_flt32;

typedef struct {
    double rows;
    double cols;
    double stride_row;
    double stride_col;
    void* data;
    int32_t owns_data;
} tmatrix_flt64;

// ============================================================================
// tmatrix_tbb32 - Primary Implementation
// ============================================================================

// Construction/Destruction
tmatrix_tbb32 aria_tmatrix_tbb32_create(int32_t rows, int32_t cols);
tmatrix_tbb32 aria_tmatrix_tbb32_create_err(void);
void aria_tmatrix_tbb32_destroy(tmatrix_tbb32* m);

// Initialization
tmatrix_tbb32 aria_tmatrix_tbb32_zero(int32_t rows, int32_t cols);
tmatrix_tbb32 aria_tmatrix_tbb32_identity(int32_t n);
tmatrix_tbb32 aria_tmatrix_tbb32_from_array(int32_t rows, int32_t cols, const int32_t* data);

// Element Access
int32_t aria_tmatrix_tbb32_get(const tmatrix_tbb32* m, int32_t row, int32_t col);
void aria_tmatrix_tbb32_set(tmatrix_tbb32* m, int32_t row, int32_t col, int32_t value);

// Safety Checks
bool aria_tmatrix_tbb32_has_err(const tmatrix_tbb32* m);
bool aria_tmatrix_tbb32_is_zero(const tmatrix_tbb32* m);

// Arithmetic Operations
tmatrix_tbb32 aria_tmatrix_tbb32_add(const tmatrix_tbb32* a, const tmatrix_tbb32* b);
tmatrix_tbb32 aria_tmatrix_tbb32_sub(const tmatrix_tbb32* a, const tmatrix_tbb32* b);
tmatrix_tbb32 aria_tmatrix_tbb32_scale(const tmatrix_tbb32* m, int32_t scalar);

// Matrix Multiply (Sentinel-Aware GEMM)
// C = A * B
// Performs pre-flight ERR check on dimensions and matrix data
// If any sentinel detected, returns ERR matrix immediately
tmatrix_tbb32 aria_tmatrix_tbb32_mul(const tmatrix_tbb32* a, const tmatrix_tbb32* b);

// Matrix Operations
tmatrix_tbb32 aria_tmatrix_tbb32_transpose(const tmatrix_tbb32* m);
tmatrix_tbb32 aria_tmatrix_tbb32_slice(const tmatrix_tbb32* m, 
                                       int32_t row_start, int32_t row_end,
                                       int32_t col_start, int32_t col_end);

// Utility
const char* aria_tmatrix_tbb32_to_string(const tmatrix_tbb32* m);
bool aria_tmatrix_tbb32_equal(const tmatrix_tbb32* a, const tmatrix_tbb32* b);

#ifdef __cplusplus
}
#endif

#endif // ARIA_TMATRIX_OPS_H
