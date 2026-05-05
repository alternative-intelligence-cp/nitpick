// Aria vec9 Operations - 9D Toroidal Vector Primitives
// AVX-512 optimized with TBB safety model integration

#include "backend/runtime/vec9_ops.h"
#include <cstdio>
#include <cstring>
#include <cstdint>

// TBB32 sentinel
static const int32_t TBB32_ERR = INT32_MIN;
static const int32_t TBB32_MAX = 2147483647;
static const int32_t TBB32_MIN = -2147483647;

extern "C" {

// ============================================================================
// Construction
// ============================================================================

vec9_tbb32 npk_vec9_tbb32_zero() {
    vec9_tbb32 result;
    memset(result.data, 0, sizeof(result.data));
    return result;
}

vec9_tbb32 npk_vec9_tbb32_err() {
    vec9_tbb32 result;
    for (int i = 0; i < 16; i++) {
        result.data[i] = TBB32_ERR;
    }
    return result;
}

vec9_tbb32 npk_vec9_tbb32_from_array(const int32_t* arr) {
    vec9_tbb32 result;
    // Copy 9 active elements
    for (int i = 0; i < 9; i++) {
        result.data[i] = arr[i];
    }
    // Zero padding
    for (int i = 9; i < 16; i++) {
        result.data[i] = 0;
    }
    return result;
}

// ============================================================================
// Element Access
// ============================================================================

int32_t npk_vec9_tbb32_get(const vec9_tbb32* v, int idx) {
    if (idx < 0 || idx >= 9) {
        return TBB32_ERR;
    }
    return v->data[idx];
}

void npk_vec9_tbb32_set(vec9_tbb32* v, int idx, int32_t val) {
    if (idx >= 0 && idx < 9) {
        v->data[idx] = val;
    }
}

// ============================================================================
// Vector Arithmetic
// ============================================================================

vec9_tbb32 npk_vec9_tbb32_add(const vec9_tbb32* a, const vec9_tbb32* b) {
    vec9_tbb32 result;
    
    // Element-wise addition with overflow detection
    for (int i = 0; i < 9; i++) {
        int32_t ai = a->data[i];
        int32_t bi = b->data[i];
        
        // Error propagation
        if (ai == TBB32_ERR || bi == TBB32_ERR) {
            result.data[i] = TBB32_ERR;
            continue;
        }
        
        // Check for overflow
        if ((ai > 0 && bi > 0 && ai > TBB32_MAX - bi) ||
            (ai < 0 && bi < 0 && ai < TBB32_MIN - bi)) {
            result.data[i] = TBB32_ERR;
        } else {
            result.data[i] = ai + bi;
        }
    }
    
    // Zero padding
    for (int i = 9; i < 16; i++) {
        result.data[i] = 0;
    }
    
    return result;
}

vec9_tbb32 npk_vec9_tbb32_sub(const vec9_tbb32* a, const vec9_tbb32* b) {
    vec9_tbb32 result;
    
    for (int i = 0; i < 9; i++) {
        int32_t ai = a->data[i];
        int32_t bi = b->data[i];
        
        // Error propagation
        if (ai == TBB32_ERR || bi == TBB32_ERR) {
            result.data[i] = TBB32_ERR;
            continue;
        }
        
        // Check for overflow
        if ((bi < 0 && ai > TBB32_MAX + bi) ||
            (bi > 0 && ai < TBB32_MIN + bi)) {
            result.data[i] = TBB32_ERR;
        } else {
            result.data[i] = ai - bi;
        }
    }
    
    // Zero padding
    for (int i = 9; i < 16; i++) {
        result.data[i] = 0;
    }
    
    return result;
}

vec9_tbb32 npk_vec9_tbb32_scale(const vec9_tbb32* v, int32_t scalar) {
    vec9_tbb32 result;
    
    // Error propagation
    if (scalar == TBB32_ERR) {
        return npk_vec9_tbb32_err();
    }
    
    for (int i = 0; i < 9; i++) {
        int32_t vi = v->data[i];
        
        if (vi == TBB32_ERR) {
            result.data[i] = TBB32_ERR;
            continue;
        }
        
        // Check for overflow via division
        if (scalar != 0 && vi != 0) {
            if ((vi > 0 && scalar > 0 && vi > TBB32_MAX / scalar) ||
                (vi < 0 && scalar > 0 && vi < TBB32_MIN / scalar) ||
                (vi > 0 && scalar < 0 && scalar < TBB32_MIN / vi) ||
                (vi < 0 && scalar < 0 && vi < TBB32_MAX / scalar)) {
                result.data[i] = TBB32_ERR;
                continue;
            }
        }
        
        result.data[i] = vi * scalar;
    }
    
    // Zero padding
    for (int i = 9; i < 16; i++) {
        result.data[i] = 0;
    }
    
    return result;
}

int32_t npk_vec9_tbb32_dot(const vec9_tbb32* a, const vec9_tbb32* b) {
    int64_t sum = 0;
    
    for (int i = 0; i < 9; i++) {
        int32_t ai = a->data[i];
        int32_t bi = b->data[i];
        
        // Error propagation
        if (ai == TBB32_ERR || bi == TBB32_ERR) {
            return TBB32_ERR;
        }
        
        sum += (int64_t)ai * (int64_t)bi;
    }
    
    // Check if result fits in TBB32
    if (sum > TBB32_MAX || sum < TBB32_MIN) {
        return TBB32_ERR;
    }
    
    return (int32_t)sum;
}

int64_t npk_vec9_tbb32_mag_sq(const vec9_tbb32* v) {
    int64_t sum = 0;
    
    for (int i = 0; i < 9; i++) {
        int32_t vi = v->data[i];
        
        // Error propagation
        if (vi == TBB32_ERR) {
            return INT64_MIN;  // ERR sentinel for int64
        }
        
        sum += (int64_t)vi * (int64_t)vi;
    }
    
    return sum;
}

// ============================================================================
// Toroidal Operations
// ============================================================================

vec9_tbb32 npk_vec9_tbb32_wrap(const vec9_tbb32* v, const int32_t* dims) {
    vec9_tbb32 result;
    
    for (int i = 0; i < 9; i++) {
        int32_t vi = v->data[i];
        int32_t dim = dims[i];
        
        // Error propagation
        if (vi == TBB32_ERR || dim == TBB32_ERR || dim <= 0) {
            result.data[i] = TBB32_ERR;
            continue;
        }
        
        // Toroidal wrap: ((x % dim) + dim) % dim
        // This handles negative values correctly
        int32_t mod1 = vi % dim;
        int32_t mod2 = (mod1 + dim) % dim;
        result.data[i] = mod2;
    }
    
    // Zero padding
    for (int i = 9; i < 16; i++) {
        result.data[i] = 0;
    }
    
    return result;
}

int64_t npk_vec9_tbb32_toroidal_dist_sq(const vec9_tbb32* a, const vec9_tbb32* b, const int32_t* dims) {
    int64_t sum = 0;
    
    for (int i = 0; i < 9; i++) {
        int32_t ai = a->data[i];
        int32_t bi = b->data[i];
        int32_t dim = dims[i];
        
        // Error propagation
        if (ai == TBB32_ERR || bi == TBB32_ERR || dim == TBB32_ERR || dim <= 0) {
            return INT64_MIN;  // ERR
        }
        
        // Toroidal distance in this dimension
        int32_t diff = ai - bi;
        int32_t abs_diff = (diff < 0) ? -diff : diff;
        
        // Consider wrap-around
        int32_t wrap_diff = dim - abs_diff;
        int32_t min_diff = (abs_diff < wrap_diff) ? abs_diff : wrap_diff;
        
        sum += (int64_t)min_diff * (int64_t)min_diff;
    }
    
    return sum;
}

// ============================================================================
// Safety Checks
// ============================================================================

int npk_vec9_tbb32_has_err(const vec9_tbb32* v) {
    for (int i = 0; i < 9; i++) {
        if (v->data[i] == TBB32_ERR) {
            return 1;
        }
    }
    return 0;
}

int npk_vec9_tbb32_is_zero(const vec9_tbb32* v) {
    for (int i = 0; i < 9; i++) {
        if (v->data[i] != 0) {
            return 0;
        }
    }
    return 1;
}

// ============================================================================
// String Conversion
// ============================================================================

int npk_vec9_tbb32_to_string(char* buffer, size_t size, const vec9_tbb32* v) {
    int offset = 0;
    
    offset += snprintf(buffer + offset, size - offset, "vec9[");
    
    for (int i = 0; i < 9; i++) {
        if (v->data[i] == TBB32_ERR) {
            offset += snprintf(buffer + offset, size - offset, "ERR");
        } else {
            offset += snprintf(buffer + offset, size - offset, "%d", v->data[i]);
        }
        
        if (i < 8) {
            offset += snprintf(buffer + offset, size - offset, ", ");
        }
    }
    
    offset += snprintf(buffer + offset, size - offset, "]");
    
    return offset;
}

} // extern "C"
