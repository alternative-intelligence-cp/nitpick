#include "backend/runtime/ttensor_ops.h"
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

static inline int32_t wrap_coord(int32_t coord, int32_t dim) {
    if (dim <= 0) return TBB32_ERR;
    
    // Modulo with proper handling of negatives
    int32_t mod1 = coord % dim;
    int32_t mod2 = (mod1 + dim) % dim;
    return mod2;
}

// ============================================================================
// Construction/Destruction
// ============================================================================

ttensor_tbb32 npk_ttensor_tbb32_create(const int32_t dims[9]) {
    ttensor_tbb32 t;
    t.rank = 9;
    
    // Check for ERR in dimensions
    for (int i = 0; i < 9; i++) {
        if (is_err_tbb32(dims[i])) {
            t.rank = TBB32_ERR;
            t.data = nullptr;
            t.owns_data = 0;
            t.err_count = TBB32_ERR;
            t.total_elements = TBB32_ERR;
            return t;
        }
        t.dims[i] = dims[i];
    }
    
    // Check for invalid dimensions
    for (int i = 0; i < 9; i++) {
        if (dims[i] <= 0) {
            t.rank = TBB32_ERR;
            t.data = nullptr;
            t.owns_data = 0;
            t.err_count = TBB32_ERR;
            t.total_elements = TBB32_ERR;
            return t;
        }
    }
    
    // Compute total elements (with overflow check)
    int64_t total = 1;
    for (int i = 0; i < 9; i++) {
        total *= dims[i];
        if (total > (int64_t)TBB32_MAX_SAFE) {
            t.rank = TBB32_ERR;
            t.data = nullptr;
            t.owns_data = 0;
            t.err_count = TBB32_ERR;
            t.total_elements = TBB32_ERR;
            return t;
        }
    }
    
    t.total_elements = (int32_t)total;
    
    // Compute strides (row-major order)
    t.strides[8] = sizeof(int32_t);  // Innermost dimension
    for (int i = 7; i >= 0; i--) {
        int64_t stride = (int64_t)t.strides[i + 1] * (int64_t)dims[i + 1];
        if (stride > (int64_t)TBB32_MAX_SAFE) {
            t.rank = TBB32_ERR;
            t.data = nullptr;
            t.owns_data = 0;
            t.err_count = TBB32_ERR;
            t.total_elements = TBB32_ERR;
            return t;
        }
        t.strides[i] = (int32_t)stride;
    }
    
    // Allocate memory
    size_t byte_size = (size_t)total * sizeof(int32_t);
    t.data = std::malloc(byte_size);
    if (!t.data) {
        t.rank = TBB32_ERR;
        t.owns_data = 0;
        t.err_count = TBB32_ERR;
        t.total_elements = TBB32_ERR;
        return t;
    }
    
    t.owns_data = 1;
    t.err_count = 0;
    
    return t;
}

ttensor_tbb32 npk_ttensor_tbb32_create_err(void) {
    ttensor_tbb32 t;
    t.rank = TBB32_ERR;
    for (int i = 0; i < 9; i++) {
        t.dims[i] = TBB32_ERR;
        t.strides[i] = TBB32_ERR;
    }
    t.data = nullptr;
    t.owns_data = 0;
    t.err_count = TBB32_ERR;
    t.total_elements = TBB32_ERR;
    return t;
}

void npk_ttensor_tbb32_destroy(ttensor_tbb32* t) {
    if (t && t->data && t->owns_data) {
        std::free(t->data);
        t->data = nullptr;
    }
}

// ============================================================================
// Initialization
// ============================================================================

ttensor_tbb32 npk_ttensor_tbb32_zero(const int32_t dims[9]) {
    ttensor_tbb32 t = npk_ttensor_tbb32_create(dims);
    if (!npk_ttensor_tbb32_has_err(&t)) {
        std::memset(t.data, 0, t.total_elements * sizeof(int32_t));
    }
    return t;
}

ttensor_tbb32 npk_ttensor_tbb32_from_array(const int32_t dims[9], const int32_t* data) {
    ttensor_tbb32 t = npk_ttensor_tbb32_create(dims);
    if (!npk_ttensor_tbb32_has_err(&t)) {
        std::memcpy(t.data, data, t.total_elements * sizeof(int32_t));
        
        // Count ERR sentinels
        int32_t* t_data = (int32_t*)t.data;
        t.err_count = 0;
        for (int32_t i = 0; i < t.total_elements; i++) {
            if (is_err_tbb32(t_data[i])) {
                t.err_count++;
            }
        }
    }
    return t;
}

// ============================================================================
// Toroidal Indexing
// ============================================================================

int64_t npk_ttensor_tbb32_toroidal_index(const ttensor_tbb32* t, const int32_t coords[9]) {
    if (npk_ttensor_tbb32_has_err(t)) {
        return -1;
    }
    
    // Wrap coordinates
    int32_t wrapped[9];
    for (int i = 0; i < 9; i++) {
        wrapped[i] = wrap_coord(coords[i], t->dims[i]);
        if (is_err_tbb32(wrapped[i])) {
            return -1;
        }
    }
    
    // Compute flat index
    int64_t index = 0;
    for (int i = 0; i < 9; i++) {
        index += (int64_t)wrapped[i] * (t->strides[i] / sizeof(int32_t));
    }
    
    return index;
}

void npk_ttensor_tbb32_wrap_coords(const ttensor_tbb32* t, const int32_t in[9], int32_t out[9]) {
    for (int i = 0; i < 9; i++) {
        out[i] = wrap_coord(in[i], t->dims[i]);
    }
}

// ============================================================================
// Element Access
// ============================================================================

int32_t npk_ttensor_tbb32_get(const ttensor_tbb32* t, const int32_t coords[9]) {
    int64_t index = npk_ttensor_tbb32_toroidal_index(t, coords);
    if (index < 0) {
        return TBB32_ERR;
    }
    
    int32_t* data = (int32_t*)t->data;
    return data[index];
}

void npk_ttensor_tbb32_set(ttensor_tbb32* t, const int32_t coords[9], int32_t value) {
    int64_t index = npk_ttensor_tbb32_toroidal_index(t, coords);
    if (index < 0) {
        return;
    }
    
    int32_t* data = (int32_t*)t->data;
    
    // Update ERR count
    if (is_err_tbb32(data[index]) && !is_err_tbb32(value)) {
        t->err_count--;
    } else if (!is_err_tbb32(data[index]) && is_err_tbb32(value)) {
        t->err_count++;
    }
    
    data[index] = value;
}

// ============================================================================
// Safety Checks
// ============================================================================

bool npk_ttensor_tbb32_has_err(const ttensor_tbb32* t) {
    if (!t) return true;
    
    if (is_err_tbb32(t->rank)) return true;
    
    for (int i = 0; i < 9; i++) {
        if (is_err_tbb32(t->dims[i])) return true;
    }
    
    if (!t->data && t->total_elements > 0) return true;
    
    // Check if any ERR in data
    return t->err_count > 0;
}

bool npk_ttensor_tbb32_is_zero(const ttensor_tbb32* t) {
    if (npk_ttensor_tbb32_has_err(t)) {
        return false;
    }
    
    int32_t* data = (int32_t*)t->data;
    for (int32_t i = 0; i < t->total_elements; i++) {
        if (data[i] != 0) {
            return false;
        }
    }
    
    return true;
}

bool npk_ttensor_tbb32_circuit_breaker(const ttensor_tbb32* t, float threshold) {
    if (npk_ttensor_tbb32_has_err(t)) {
        return true;  // Trigger shutdown on ERR tensor
    }
    
    if (t->total_elements == 0) {
        return false;
    }
    
    float err_density = (float)t->err_count / (float)t->total_elements;
    return err_density > threshold;
}

// ============================================================================
// Toroidal Distance
// ============================================================================

int64_t npk_ttensor_tbb32_toroidal_distance_sq(const ttensor_tbb32* t, 
                                                 const int32_t a[9], 
                                                 const int32_t b[9]) {
    if (npk_ttensor_tbb32_has_err(t)) {
        return -1;
    }
    
    int64_t dist_sq = 0;
    
    for (int i = 0; i < 9; i++) {
        int32_t dim = t->dims[i];
        
        // Wrap coordinates
        int32_t a_wrapped = wrap_coord(a[i], dim);
        int32_t b_wrapped = wrap_coord(b[i], dim);
        
        if (is_err_tbb32(a_wrapped) || is_err_tbb32(b_wrapped)) {
            return -1;
        }
        
        // Compute direct distance
        int32_t direct = abs(b_wrapped - a_wrapped);
        
        // Compute wrap-around distance
        int32_t wraparound = dim - direct;
        
        // Take minimum (geodesic)
        int32_t min_dist = (direct < wraparound) ? direct : wraparound;
        
        dist_sq += (int64_t)min_dist * (int64_t)min_dist;
    }
    
    return dist_sq;
}

// ============================================================================
// Wave Interference Operations
// ============================================================================

ttensor_tbb32 npk_ttensor_tbb32_wave_interference(const ttensor_tbb32* base,
                                                    const int32_t emitters[9][9],
                                                    const int32_t amplitudes[9],
                                                    float decay_factor) {
    if (npk_ttensor_tbb32_has_err(base)) {
        return npk_ttensor_tbb32_create_err();
    }
    
    ttensor_tbb32 result = npk_ttensor_tbb32_zero(base->dims);
    if (npk_ttensor_tbb32_has_err(&result)) {
        return result;
    }
    
    // For each point in the tensor
    int32_t coords[9];
    
    // Iterate through all coordinates (nested loops for 9D)
    // NOTE: This is extremely slow for large tensors - for production,
    // vectorize or use GPU acceleration
    
    for (coords[0] = 0; coords[0] < base->dims[0]; coords[0]++) {
    for (coords[1] = 0; coords[1] < base->dims[1]; coords[1]++) {
    for (coords[2] = 0; coords[2] < base->dims[2]; coords[2]++) {
    for (coords[3] = 0; coords[3] < base->dims[3]; coords[3]++) {
    for (coords[4] = 0; coords[4] < base->dims[4]; coords[4]++) {
    for (coords[5] = 0; coords[5] < base->dims[5]; coords[5]++) {
    for (coords[6] = 0; coords[6] < base->dims[6]; coords[6]++) {
    for (coords[7] = 0; coords[7] < base->dims[7]; coords[7]++) {
    for (coords[8] = 0; coords[8] < base->dims[8]; coords[8]++) {
        
        float sum = 0.0f;
        
        // Sum contributions from all 9 emitters
        for (int e = 0; e < 9; e++) {
            int64_t dist_sq = npk_ttensor_tbb32_toroidal_distance_sq(base, coords, emitters[e]);
            if (dist_sq < 0) continue;  // Skip on error
            
            float dist = sqrtf((float)dist_sq);
            float decay = expf(-decay_factor * dist);
            sum += (float)amplitudes[e] * decay;
        }
        
        // Convert to int32_t (with overflow check)
        int32_t value = (sum > TBB32_MAX_SAFE) ? TBB32_ERR : 
                       (sum < TBB32_MIN_SAFE) ? TBB32_ERR : 
                       (int32_t)sum;
        
        npk_ttensor_tbb32_set(&result, coords, value);
    }}}}}}}}}
    
    return result;
}

// ============================================================================
// Mamba SSM Integration
// ============================================================================

ttensor_tbb32 npk_ttensor_tbb32_mamba_scan(const ttensor_tbb32* input,
                                            ttensor_tbb32* state,
                                            const ttensor_tbb32* gate,
                                            int32_t axis) {
    if (npk_ttensor_tbb32_has_err(input) || 
        npk_ttensor_tbb32_has_err(state) || 
        npk_ttensor_tbb32_has_err(gate)) {
        return npk_ttensor_tbb32_create_err();
    }
    
    if (axis < 0 || axis >= 9) {
        return npk_ttensor_tbb32_create_err();
    }
    
    // Check dimension compatibility
    for (int i = 0; i < 9; i++) {
        if (input->dims[i] != state->dims[i] || input->dims[i] != gate->dims[i]) {
            return npk_ttensor_tbb32_create_err();
        }
    }
    
    // Create output tensor
    ttensor_tbb32 output = npk_ttensor_tbb32_create(input->dims);
    if (npk_ttensor_tbb32_has_err(&output)) {
        return output;
    }
    
    // Scan along specified axis with gating
    // state[t] = gate[t] * state[t-1] + input[t]
    
    int32_t coords[9] = {0};
    
    // NOTE: Proper implementation requires iteration along scan axis
    // This is a simplified version - production code needs optimized scan
    
    for (coords[0] = 0; coords[0] < input->dims[0]; coords[0]++) {
    for (coords[1] = 0; coords[1] < input->dims[1]; coords[1]++) {
    for (coords[2] = 0; coords[2] < input->dims[2]; coords[2]++) {
    for (coords[3] = 0; coords[3] < input->dims[3]; coords[3]++) {
    for (coords[4] = 0; coords[4] < input->dims[4]; coords[4]++) {
    for (coords[5] = 0; coords[5] < input->dims[5]; coords[5]++) {
    for (coords[6] = 0; coords[6] < input->dims[6]; coords[6]++) {
    for (coords[7] = 0; coords[7] < input->dims[7]; coords[7]++) {
    for (coords[8] = 0; coords[8] < input->dims[8]; coords[8]++) {
        
        int32_t input_val = npk_ttensor_tbb32_get(input, coords);
        int32_t gate_val = npk_ttensor_tbb32_get(gate, coords);
        int32_t state_val = npk_ttensor_tbb32_get(state, coords);
        
        // state[t] = gate[t] * state[t-1] + input[t]
        int32_t gated_state = tbb32_mul(gate_val, state_val);
        int32_t new_state = tbb32_add(gated_state, input_val);
        
        npk_ttensor_tbb32_set(&output, coords, new_state);
        npk_ttensor_tbb32_set(state, coords, new_state);  // Update state
    }}}}}}}}}
    
    return output;
}

// ============================================================================
// Arithmetic Operations
// ============================================================================

ttensor_tbb32 npk_ttensor_tbb32_add(const ttensor_tbb32* a, const ttensor_tbb32* b) {
    if (npk_ttensor_tbb32_has_err(a) || npk_ttensor_tbb32_has_err(b)) {
        return npk_ttensor_tbb32_create_err();
    }
    
    // Check dimensions
    for (int i = 0; i < 9; i++) {
        if (a->dims[i] != b->dims[i]) {
            return npk_ttensor_tbb32_create_err();
        }
    }
    
    ttensor_tbb32 result = npk_ttensor_tbb32_create(a->dims);
    if (npk_ttensor_tbb32_has_err(&result)) {
        return result;
    }
    
    int32_t* a_data = (int32_t*)a->data;
    int32_t* b_data = (int32_t*)b->data;
    int32_t* r_data = (int32_t*)result.data;
    
    for (int32_t i = 0; i < a->total_elements; i++) {
        r_data[i] = tbb32_add(a_data[i], b_data[i]);
        if (is_err_tbb32(r_data[i])) {
            result.err_count++;
        }
    }
    
    return result;
}

ttensor_tbb32 npk_ttensor_tbb32_sub(const ttensor_tbb32* a, const ttensor_tbb32* b) {
    if (npk_ttensor_tbb32_has_err(a) || npk_ttensor_tbb32_has_err(b)) {
        return npk_ttensor_tbb32_create_err();
    }
    
    for (int i = 0; i < 9; i++) {
        if (a->dims[i] != b->dims[i]) {
            return npk_ttensor_tbb32_create_err();
        }
    }
    
    ttensor_tbb32 result = npk_ttensor_tbb32_create(a->dims);
    if (npk_ttensor_tbb32_has_err(&result)) {
        return result;
    }
    
    int32_t* a_data = (int32_t*)a->data;
    int32_t* b_data = (int32_t*)b->data;
    int32_t* r_data = (int32_t*)result.data;
    
    for (int32_t i = 0; i < a->total_elements; i++) {
        int32_t neg_b = (b_data[i] == TBB32_MIN_SAFE) ? TBB32_ERR : -b_data[i];
        r_data[i] = tbb32_add(a_data[i], neg_b);
        if (is_err_tbb32(r_data[i])) {
            result.err_count++;
        }
    }
    
    return result;
}

ttensor_tbb32 npk_ttensor_tbb32_mul_elementwise(const ttensor_tbb32* a, const ttensor_tbb32* b) {
    if (npk_ttensor_tbb32_has_err(a) || npk_ttensor_tbb32_has_err(b)) {
        return npk_ttensor_tbb32_create_err();
    }
    
    for (int i = 0; i < 9; i++) {
        if (a->dims[i] != b->dims[i]) {
            return npk_ttensor_tbb32_create_err();
        }
    }
    
    ttensor_tbb32 result = npk_ttensor_tbb32_create(a->dims);
    if (npk_ttensor_tbb32_has_err(&result)) {
        return result;
    }
    
    int32_t* a_data = (int32_t*)a->data;
    int32_t* b_data = (int32_t*)b->data;
    int32_t* r_data = (int32_t*)result.data;
    
    for (int32_t i = 0; i < a->total_elements; i++) {
        r_data[i] = tbb32_mul(a_data[i], b_data[i]);
        if (is_err_tbb32(r_data[i])) {
            result.err_count++;
        }
    }
    
    return result;
}

ttensor_tbb32 npk_ttensor_tbb32_scale(const ttensor_tbb32* t, int32_t scalar) {
    if (npk_ttensor_tbb32_has_err(t) || is_err_tbb32(scalar)) {
        return npk_ttensor_tbb32_create_err();
    }
    
    ttensor_tbb32 result = npk_ttensor_tbb32_create(t->dims);
    if (npk_ttensor_tbb32_has_err(&result)) {
        return result;
    }
    
    int32_t* t_data = (int32_t*)t->data;
    int32_t* r_data = (int32_t*)result.data;
    
    for (int32_t i = 0; i < t->total_elements; i++) {
        r_data[i] = tbb32_mul(t_data[i], scalar);
        if (is_err_tbb32(r_data[i])) {
            result.err_count++;
        }
    }
    
    return result;
}

// ============================================================================
// 9D Convolution
// ============================================================================

ttensor_tbb32 npk_ttensor_tbb32_convolve(const ttensor_tbb32* input,
                                          const ttensor_tbb32* kernel) {
    if (npk_ttensor_tbb32_has_err(input) || npk_ttensor_tbb32_has_err(kernel)) {
        return npk_ttensor_tbb32_create_err();
    }
    
    // Check kernel sizes are odd
    for (int i = 0; i < 9; i++) {
        if (kernel->dims[i] % 2 == 0) {
            return npk_ttensor_tbb32_create_err();
        }
    }
    
    ttensor_tbb32 result = npk_ttensor_tbb32_zero(input->dims);
    if (npk_ttensor_tbb32_has_err(&result)) {
        return result;
    }
    
    // Compute kernel offsets
    int32_t kernel_half[9];
    for (int i = 0; i < 9; i++) {
        kernel_half[i] = kernel->dims[i] / 2;
    }
    
    // For each output position
    int32_t out_coords[9];
    int32_t in_coords[9];
    int32_t k_coords[9];
    
    // NOTE: 9-nested loops - extremely slow, needs optimization for production
    for (out_coords[0] = 0; out_coords[0] < input->dims[0]; out_coords[0]++) {
    for (out_coords[1] = 0; out_coords[1] < input->dims[1]; out_coords[1]++) {
    for (out_coords[2] = 0; out_coords[2] < input->dims[2]; out_coords[2]++) {
    for (out_coords[3] = 0; out_coords[3] < input->dims[3]; out_coords[3]++) {
    for (out_coords[4] = 0; out_coords[4] < input->dims[4]; out_coords[4]++) {
    for (out_coords[5] = 0; out_coords[5] < input->dims[5]; out_coords[5]++) {
    for (out_coords[6] = 0; out_coords[6] < input->dims[6]; out_coords[6]++) {
    for (out_coords[7] = 0; out_coords[7] < input->dims[7]; out_coords[7]++) {
    for (out_coords[8] = 0; out_coords[8] < input->dims[8]; out_coords[8]++) {
        
        int32_t sum = 0;
        bool tainted = false;
        
        // Convolve with kernel
        for (k_coords[0] = 0; k_coords[0] < kernel->dims[0]; k_coords[0]++) {
        for (k_coords[1] = 0; k_coords[1] < kernel->dims[1]; k_coords[1]++) {
        for (k_coords[2] = 0; k_coords[2] < kernel->dims[2]; k_coords[2]++) {
        for (k_coords[3] = 0; k_coords[3] < kernel->dims[3]; k_coords[3]++) {
        for (k_coords[4] = 0; k_coords[4] < kernel->dims[4]; k_coords[4]++) {
        for (k_coords[5] = 0; k_coords[5] < kernel->dims[5]; k_coords[5]++) {
        for (k_coords[6] = 0; k_coords[6] < kernel->dims[6]; k_coords[6]++) {
        for (k_coords[7] = 0; k_coords[7] < kernel->dims[7]; k_coords[7]++) {
        for (k_coords[8] = 0; k_coords[8] < kernel->dims[8]; k_coords[8]++) {
            
            // Compute input coordinates (with toroidal wrapping)
            for (int i = 0; i < 9; i++) {
                in_coords[i] = out_coords[i] + k_coords[i] - kernel_half[i];
            }
            
            int32_t input_val = npk_ttensor_tbb32_get(input, in_coords);
            int32_t kernel_val = npk_ttensor_tbb32_get(kernel, k_coords);
            
            if (is_err_tbb32(input_val) || is_err_tbb32(kernel_val)) {
                tainted = true;
                break;
            }
            
            int32_t prod = tbb32_mul(input_val, kernel_val);
            if (is_err_tbb32(prod)) {
                tainted = true;
                break;
            }
            
            sum = tbb32_add(sum, prod);
            if (is_err_tbb32(sum)) {
                tainted = true;
                break;
            }
        }}}}}}}}}
        
        if (tainted) break;
        
        npk_ttensor_tbb32_set(&result, out_coords, tainted ? TBB32_ERR : sum);
    }}}}}}}}}
    
    return result;
}

// ============================================================================
// Utility
// ============================================================================

const char* npk_ttensor_tbb32_to_string(const ttensor_tbb32* t) {
    static char buffer[1024];
    
    if (npk_ttensor_tbb32_has_err(t)) {
        snprintf(buffer, sizeof(buffer), "ttensor[ERR]");
        return buffer;
    }
    
    snprintf(buffer, sizeof(buffer), 
             "ttensor9D[%d,%d,%d,%d,%d,%d,%d,%d,%d] (%d elements, %d ERRs)",
             t->dims[0], t->dims[1], t->dims[2], t->dims[3], t->dims[4],
             t->dims[5], t->dims[6], t->dims[7], t->dims[8],
             t->total_elements, t->err_count);
    
    return buffer;
}

bool npk_ttensor_tbb32_equal(const ttensor_tbb32* a, const ttensor_tbb32* b) {
    bool a_err = npk_ttensor_tbb32_has_err(a);
    bool b_err = npk_ttensor_tbb32_has_err(b);
    
    if (a_err && b_err) return true;
    if (a_err || b_err) return false;
    
    for (int i = 0; i < 9; i++) {
        if (a->dims[i] != b->dims[i]) {
            return false;
        }
    }
    
    int32_t* a_data = (int32_t*)a->data;
    int32_t* b_data = (int32_t*)b->data;
    
    for (int32_t i = 0; i < a->total_elements; i++) {
        if (a_data[i] != b_data[i]) {
            return false;
        }
    }
    
    return true;
}

void npk_ttensor_tbb32_print_stats(const ttensor_tbb32* t) {
    if (npk_ttensor_tbb32_has_err(t)) {
        printf("ttensor[ERR]\n");
        return;
    }
    
    printf("9D Toroidal Tensor Statistics:\n");
    printf("  Dimensions: [%d, %d, %d, %d, %d, %d, %d, %d, %d]\n",
           t->dims[0], t->dims[1], t->dims[2], t->dims[3], t->dims[4],
           t->dims[5], t->dims[6], t->dims[7], t->dims[8]);
    printf("  Total elements: %d\n", t->total_elements);
    printf("  ERR count: %d\n", t->err_count);
    printf("  ERR density: %.2f%%\n", 
           100.0f * (float)t->err_count / (float)t->total_elements);
    printf("  Memory: %.2f MB\n", 
           (float)(t->total_elements * sizeof(int32_t)) / (1024.0f * 1024.0f));
}
