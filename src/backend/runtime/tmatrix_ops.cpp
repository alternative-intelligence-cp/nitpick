#include "backend/runtime/tmatrix_ops.h"
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <cmath>

// Sentinel values for TBB32
static const int32_t TBB32_ERR = INT32_MIN;
static const int32_t TBB32_MAX_SAFE = INT32_MAX - 1;
static const int32_t TBB32_MIN_SAFE = INT32_MIN + 1;

// ============================================================================
// Helper Functions
// ============================================================================

static inline bool is_err_tbb32(int32_t x) {
    return x == TBB32_ERR;
}

static inline bool has_overflow_add_tbb32(int32_t a, int32_t b) {
    if (is_err_tbb32(a) || is_err_tbb32(b)) return true;
    
    if (b > 0 && a > TBB32_MAX_SAFE - b) return true;
    if (b < 0 && a < TBB32_MIN_SAFE - b) return true;
    
    return false;
}

static inline bool has_overflow_mul_tbb32(int32_t a, int32_t b) {
    if (is_err_tbb32(a) || is_err_tbb32(b)) return true;
    if (a == 0 || b == 0) return false;
    
    int64_t result = (int64_t)a * (int64_t)b;
    return result > TBB32_MAX_SAFE || result < TBB32_MIN_SAFE;
}

static inline int32_t tbb32_add(int32_t a, int32_t b) {
    return has_overflow_add_tbb32(a, b) ? TBB32_ERR : a + b;
}

static inline int32_t tbb32_mul(int32_t a, int32_t b) {
    return has_overflow_mul_tbb32(a, b) ? TBB32_ERR : a * b;
}

// ============================================================================
// Construction/Destruction
// ============================================================================

tmatrix_tbb32 aria_tmatrix_tbb32_create(int32_t rows, int32_t cols) {
    tmatrix_tbb32 m;
    
    // Check for ERR in dimensions
    if (is_err_tbb32(rows) || is_err_tbb32(cols)) {
        m.rows = TBB32_ERR;
        m.cols = TBB32_ERR;
        m.stride_row = TBB32_ERR;
        m.stride_col = TBB32_ERR;
        m.data = nullptr;
        m.owns_data = 0;
        return m;
    }
    
    // Check for invalid dimensions
    if (rows <= 0 || cols <= 0) {
        m.rows = TBB32_ERR;
        m.cols = TBB32_ERR;
        m.stride_row = TBB32_ERR;
        m.stride_col = TBB32_ERR;
        m.data = nullptr;
        m.owns_data = 0;
        return m;
    }
    
    // Check for allocation size overflow
    int64_t total_elements = (int64_t)rows * (int64_t)cols;
    int64_t byte_size = total_elements * sizeof(int32_t);
    
    if (total_elements > (int64_t)TBB32_MAX_SAFE || byte_size > (int64_t)TBB32_MAX_SAFE) {
        m.rows = TBB32_ERR;
        m.cols = TBB32_ERR;
        m.stride_row = TBB32_ERR;
        m.stride_col = TBB32_ERR;
        m.data = nullptr;
        m.owns_data = 0;
        return m;
    }
    
    // Allocate memory
    m.data = std::malloc(byte_size);
    if (!m.data) {
        m.rows = TBB32_ERR;
        m.cols = TBB32_ERR;
        m.stride_row = TBB32_ERR;
        m.stride_col = TBB32_ERR;
        m.owns_data = 0;
        return m;
    }
    
    m.rows = rows;
    m.cols = cols;
    m.stride_row = cols * sizeof(int32_t);
    m.stride_col = sizeof(int32_t);
    m.owns_data = 1;
    
    return m;
}

tmatrix_tbb32 aria_tmatrix_tbb32_create_err(void) {
    tmatrix_tbb32 m;
    m.rows = TBB32_ERR;
    m.cols = TBB32_ERR;
    m.stride_row = TBB32_ERR;
    m.stride_col = TBB32_ERR;
    m.data = nullptr;
    m.owns_data = 0;
    return m;
}

void aria_tmatrix_tbb32_destroy(tmatrix_tbb32* m) {
    if (m && m->data && m->owns_data) {
        std::free(m->data);
        m->data = nullptr;
    }
}

// ============================================================================
// Initialization
// ============================================================================

tmatrix_tbb32 aria_tmatrix_tbb32_zero(int32_t rows, int32_t cols) {
    tmatrix_tbb32 m = aria_tmatrix_tbb32_create(rows, cols);
    if (!aria_tmatrix_tbb32_has_err(&m)) {
        std::memset(m.data, 0, rows * cols * sizeof(int32_t));
    }
    return m;
}

tmatrix_tbb32 aria_tmatrix_tbb32_identity(int32_t n) {
    tmatrix_tbb32 m = aria_tmatrix_tbb32_zero(n, n);
    if (!aria_tmatrix_tbb32_has_err(&m)) {
        int32_t* data = (int32_t*)m.data;
        for (int32_t i = 0; i < n; i++) {
            data[i * n + i] = 1;
        }
    }
    return m;
}

tmatrix_tbb32 aria_tmatrix_tbb32_from_array(int32_t rows, int32_t cols, const int32_t* data) {
    tmatrix_tbb32 m = aria_tmatrix_tbb32_create(rows, cols);
    if (!aria_tmatrix_tbb32_has_err(&m)) {
        std::memcpy(m.data, data, rows * cols * sizeof(int32_t));
    }
    return m;
}

// ============================================================================
// Element Access
// ============================================================================

int32_t aria_tmatrix_tbb32_get(const tmatrix_tbb32* m, int32_t row, int32_t col) {
    // Pre-flight ERR check
    if (aria_tmatrix_tbb32_has_err(m)) {
        return TBB32_ERR;
    }
    
    // Bounds check
    if (row < 0 || row >= m->rows || col < 0 || col >= m->cols) {
        return TBB32_ERR;
    }
    
    // Calculate address using stride
    int32_t* data = (int32_t*)m->data;
    return data[row * m->cols + col];
}

void aria_tmatrix_tbb32_set(tmatrix_tbb32* m, int32_t row, int32_t col, int32_t value) {
    // Pre-flight ERR check
    if (aria_tmatrix_tbb32_has_err(m)) {
        return;
    }
    
    // Bounds check
    if (row < 0 || row >= m->rows || col < 0 || col >= m->cols) {
        // Taint the entire matrix on out-of-bounds write attempt
        m->rows = TBB32_ERR;
        m->cols = TBB32_ERR;
        return;
    }
    
    int32_t* data = (int32_t*)m->data;
    data[row * m->cols + col] = value;
}

// ============================================================================
// Safety Checks
// ============================================================================

bool aria_tmatrix_tbb32_has_err(const tmatrix_tbb32* m) {
    if (!m) return true;
    
    // Check dimensions for ERR
    if (is_err_tbb32(m->rows) || is_err_tbb32(m->cols)) {
        return true;
    }
    
    // Check for null data with non-zero dimensions
    if (!m->data && (m->rows > 0 || m->cols > 0)) {
        return true;
    }
    
    // Scan data for ERR sentinel (full matrix scan - expensive!)
    // NOTE: This is O(n*m) - consider caching a "tainted" flag in production
    if (m->data) {
        int32_t* data = (int32_t*)m->data;
        int32_t total = m->rows * m->cols;
        for (int32_t i = 0; i < total; i++) {
            if (is_err_tbb32(data[i])) {
                return true;
            }
        }
    }
    
    return false;
}

bool aria_tmatrix_tbb32_is_zero(const tmatrix_tbb32* m) {
    if (aria_tmatrix_tbb32_has_err(m)) {
        return false;
    }
    
    int32_t* data = (int32_t*)m->data;
    int32_t total = m->rows * m->cols;
    for (int32_t i = 0; i < total; i++) {
        if (data[i] != 0) {
            return false;
        }
    }
    
    return true;
}

// ============================================================================
// Arithmetic Operations
// ============================================================================

tmatrix_tbb32 aria_tmatrix_tbb32_add(const tmatrix_tbb32* a, const tmatrix_tbb32* b) {
    // Pre-flight ERR check
    if (aria_tmatrix_tbb32_has_err(a) || aria_tmatrix_tbb32_has_err(b)) {
        return aria_tmatrix_tbb32_create_err();
    }
    
    // Dimension check
    if (a->rows != b->rows || a->cols != b->cols) {
        return aria_tmatrix_tbb32_create_err();
    }
    
    tmatrix_tbb32 result = aria_tmatrix_tbb32_create(a->rows, a->cols);
    if (aria_tmatrix_tbb32_has_err(&result)) {
        return result;
    }
    
    int32_t* a_data = (int32_t*)a->data;
    int32_t* b_data = (int32_t*)b->data;
    int32_t* r_data = (int32_t*)result.data;
    
    int32_t total = a->rows * a->cols;
    for (int32_t i = 0; i < total; i++) {
        r_data[i] = tbb32_add(a_data[i], b_data[i]);
    }
    
    return result;
}

tmatrix_tbb32 aria_tmatrix_tbb32_sub(const tmatrix_tbb32* a, const tmatrix_tbb32* b) {
    // Pre-flight ERR check
    if (aria_tmatrix_tbb32_has_err(a) || aria_tmatrix_tbb32_has_err(b)) {
        return aria_tmatrix_tbb32_create_err();
    }
    
    // Dimension check
    if (a->rows != b->rows || a->cols != b->cols) {
        return aria_tmatrix_tbb32_create_err();
    }
    
    tmatrix_tbb32 result = aria_tmatrix_tbb32_create(a->rows, a->cols);
    if (aria_tmatrix_tbb32_has_err(&result)) {
        return result;
    }
    
    int32_t* a_data = (int32_t*)a->data;
    int32_t* b_data = (int32_t*)b->data;
    int32_t* r_data = (int32_t*)result.data;
    
    int32_t total = a->rows * a->cols;
    for (int32_t i = 0; i < total; i++) {
        // Subtract is add with negation
        int32_t neg_b = (b_data[i] == TBB32_MIN_SAFE) ? TBB32_ERR : -b_data[i];
        r_data[i] = tbb32_add(a_data[i], neg_b);
    }
    
    return result;
}

tmatrix_tbb32 aria_tmatrix_tbb32_scale(const tmatrix_tbb32* m, int32_t scalar) {
    // Pre-flight ERR check
    if (aria_tmatrix_tbb32_has_err(m) || is_err_tbb32(scalar)) {
        return aria_tmatrix_tbb32_create_err();
    }
    
    tmatrix_tbb32 result = aria_tmatrix_tbb32_create(m->rows, m->cols);
    if (aria_tmatrix_tbb32_has_err(&result)) {
        return result;
    }
    
    int32_t* m_data = (int32_t*)m->data;
    int32_t* r_data = (int32_t*)result.data;
    
    int32_t total = m->rows * m->cols;
    for (int32_t i = 0; i < total; i++) {
        r_data[i] = tbb32_mul(m_data[i], scalar);
    }
    
    return result;
}

// ============================================================================
// Matrix Multiply (Sentinel-Aware GEMM)
// ============================================================================

tmatrix_tbb32 aria_tmatrix_tbb32_mul(const tmatrix_tbb32* a, const tmatrix_tbb32* b) {
    // Pre-flight ERR check
    if (aria_tmatrix_tbb32_has_err(a) || aria_tmatrix_tbb32_has_err(b)) {
        return aria_tmatrix_tbb32_create_err();
    }
    
    // Dimension check: (m x n) * (n x p) = (m x p)
    if (a->cols != b->rows) {
        return aria_tmatrix_tbb32_create_err();
    }
    
    int32_t m = a->rows;
    int32_t n = a->cols;  // = b->rows
    int32_t p = b->cols;
    
    tmatrix_tbb32 result = aria_tmatrix_tbb32_zero(m, p);
    if (aria_tmatrix_tbb32_has_err(&result)) {
        return result;
    }
    
    int32_t* a_data = (int32_t*)a->data;
    int32_t* b_data = (int32_t*)b->data;
    int32_t* c_data = (int32_t*)result.data;
    
    // Cache-friendly tiled GEMM: C[i,j] += A[i,k] * B[k,j]
    // Uses ikj loop order with blocking for L1/L2 cache locality.
    // TBB sentinel checking is preserved for safety.
    static constexpr int32_t TILE = 32;

    for (int32_t ii = 0; ii < m; ii += TILE) {
        int32_t i_end = (ii + TILE < m) ? ii + TILE : m;
        for (int32_t kk = 0; kk < n; kk += TILE) {
            int32_t k_end = (kk + TILE < n) ? kk + TILE : n;
            for (int32_t jj = 0; jj < p; jj += TILE) {
                int32_t j_end = (jj + TILE < p) ? jj + TILE : p;

                // Micro-tile: i-k-j order
                for (int32_t i = ii; i < i_end; i++) {
                    for (int32_t k = kk; k < k_end; k++) {
                        int32_t a_elem = a_data[i * n + k];
                        if (is_err_tbb32(a_elem)) {
                            // Taint entire row slice
                            for (int32_t j = jj; j < j_end; j++)
                                c_data[i * p + j] = TBB32_ERR;
                            continue;
                        }
                        for (int32_t j = jj; j < j_end; j++) {
                            if (is_err_tbb32(c_data[i * p + j])) continue;
                            int32_t b_elem = b_data[k * p + j];
                            if (is_err_tbb32(b_elem)) {
                                c_data[i * p + j] = TBB32_ERR;
                                continue;
                            }
                            int32_t prod = tbb32_mul(a_elem, b_elem);
                            if (is_err_tbb32(prod)) {
                                c_data[i * p + j] = TBB32_ERR;
                                continue;
                            }
                            int32_t sum = tbb32_add(c_data[i * p + j], prod);
                            c_data[i * p + j] = is_err_tbb32(sum) ? TBB32_ERR : sum;
                        }
                    }
                }
            }
        }
    }
    
    return result;
}

// ============================================================================
// Matrix Operations
// ============================================================================

tmatrix_tbb32 aria_tmatrix_tbb32_transpose(const tmatrix_tbb32* m) {
    if (aria_tmatrix_tbb32_has_err(m)) {
        return aria_tmatrix_tbb32_create_err();
    }
    
    tmatrix_tbb32 result = aria_tmatrix_tbb32_create(m->cols, m->rows);
    if (aria_tmatrix_tbb32_has_err(&result)) {
        return result;
    }
    
    int32_t* m_data = (int32_t*)m->data;
    int32_t* r_data = (int32_t*)result.data;
    
    for (int32_t i = 0; i < m->rows; i++) {
        for (int32_t j = 0; j < m->cols; j++) {
            r_data[j * m->rows + i] = m_data[i * m->cols + j];
        }
    }
    
    return result;
}

tmatrix_tbb32 aria_tmatrix_tbb32_slice(const tmatrix_tbb32* m, 
                                       int32_t row_start, int32_t row_end,
                                       int32_t col_start, int32_t col_end) {
    if (aria_tmatrix_tbb32_has_err(m)) {
        return aria_tmatrix_tbb32_create_err();
    }
    
    // Bounds check
    if (row_start < 0 || row_end > m->rows || row_start >= row_end ||
        col_start < 0 || col_end > m->cols || col_start >= col_end) {
        return aria_tmatrix_tbb32_create_err();
    }
    
    int32_t new_rows = row_end - row_start;
    int32_t new_cols = col_end - col_start;
    
    tmatrix_tbb32 result = aria_tmatrix_tbb32_create(new_rows, new_cols);
    if (aria_tmatrix_tbb32_has_err(&result)) {
        return result;
    }
    
    int32_t* m_data = (int32_t*)m->data;
    int32_t* r_data = (int32_t*)result.data;
    
    for (int32_t i = 0; i < new_rows; i++) {
        for (int32_t j = 0; j < new_cols; j++) {
            int32_t src_row = row_start + i;
            int32_t src_col = col_start + j;
            r_data[i * new_cols + j] = m_data[src_row * m->cols + src_col];
        }
    }
    
    return result;
}

// ============================================================================
// Utility
// ============================================================================

const char* aria_tmatrix_tbb32_to_string(const tmatrix_tbb32* m) {
    static char buffer[4096];
    
    if (aria_tmatrix_tbb32_has_err(m)) {
        snprintf(buffer, sizeof(buffer), "tmatrix[ERR]");
        return buffer;
    }
    
    int offset = snprintf(buffer, sizeof(buffer), "tmatrix[%d x %d]:\n", m->rows, m->cols);
    
    int32_t* data = (int32_t*)m->data;
    for (int32_t i = 0; i < m->rows && offset < (int)sizeof(buffer) - 100; i++) {
        offset += snprintf(buffer + offset, sizeof(buffer) - offset, "  [");
        for (int32_t j = 0; j < m->cols && offset < (int)sizeof(buffer) - 100; j++) {
            int32_t val = data[i * m->cols + j];
            if (is_err_tbb32(val)) {
                offset += snprintf(buffer + offset, sizeof(buffer) - offset, "ERR");
            } else {
                offset += snprintf(buffer + offset, sizeof(buffer) - offset, "%d", val);
            }
            if (j < m->cols - 1) {
                offset += snprintf(buffer + offset, sizeof(buffer) - offset, ", ");
            }
        }
        offset += snprintf(buffer + offset, sizeof(buffer) - offset, "]\n");
    }
    
    return buffer;
}

bool aria_tmatrix_tbb32_equal(const tmatrix_tbb32* a, const tmatrix_tbb32* b) {
    // If both are ERR, they're equal
    bool a_err = aria_tmatrix_tbb32_has_err(a);
    bool b_err = aria_tmatrix_tbb32_has_err(b);
    
    if (a_err && b_err) return true;
    if (a_err || b_err) return false;
    
    // Dimension check
    if (a->rows != b->rows || a->cols != b->cols) {
        return false;
    }
    
    // Element-wise comparison
    int32_t* a_data = (int32_t*)a->data;
    int32_t* b_data = (int32_t*)b->data;
    
    int32_t total = a->rows * a->cols;
    for (int32_t i = 0; i < total; i++) {
        if (a_data[i] != b_data[i]) {
            return false;
        }
    }
    
    return true;
}
