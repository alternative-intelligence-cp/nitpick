#ifndef ARIA_TTENSOR_OPS_H
#define ARIA_TTENSOR_OPS_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Twisted Tensor - 9D Toroidal Tensor for Nikola AGI
//
// Purpose: Safety-critical 9D toroidal memory system for neurodivergent child AGI
// Topology: 9-dimensional torus with wrap-around connectivity
// Safety Model: "Infected Hypercube" prevention via circuit breaker pattern
//
// Key Features:
// - 9D toroidal indexing (coordinates wrap mod dimensions)
// - Wave interference memory (9 emitters in toroidal space)
// - Mamba SSM integration (selective state space models on torus)
// - ERR propagation with exponential explosion detection
// - Circuit breaker: auto-shutdown if ERR density > threshold
//
// Threat Model:
// - Single ERR has 19,682 neighbors in 9D (catastrophic spread risk)
// - "Infected Hypercube": ERR propagates exponentially without containment
// - Neurodivergent child safety: erratic behavior = life-threatening
// - Circuit breaker triggers emergency AGI shutdown
//
// Mathematical Properties:
// - Toroidal distance: geodesic (shortest path considering wrap-around)
// - Neighbor count: 3^9 - 1 = 19,682 (9D hypercube neighbors)
// - Wave interference: superposition of 9 source waves
// - Mamba scan: selective state propagation along toroidal axes

// ============================================================================
// Core Structure
// ============================================================================

typedef struct {
    int32_t rank;           // Always 9 for 9D tensor (ERR = tainted)
    int32_t dims[9];        // Size along each dimension (ERR in any = tainted)
    int32_t strides[9];     // Byte stride for each dimension
    void* data;             // Wild memory for tensor data
    int32_t owns_data;      // 1 if owns data, 0 for views
    int32_t err_count;      // Number of ERR sentinels in tensor
    int32_t total_elements; // Total elements (product of dims)
} ttensor_tbb32;

// ============================================================================
// Construction/Destruction
// ============================================================================

// Create 9D tensor with specified dimensions
ttensor_tbb32 npk_ttensor_tbb32_create(const int32_t dims[9]);
ttensor_tbb32 npk_ttensor_tbb32_create_err(void);
void npk_ttensor_tbb32_destroy(ttensor_tbb32* t);

// ============================================================================
// Initialization
// ============================================================================

ttensor_tbb32 npk_ttensor_tbb32_zero(const int32_t dims[9]);
ttensor_tbb32 npk_ttensor_tbb32_from_array(const int32_t dims[9], const int32_t* data);

// ============================================================================
// Toroidal Indexing
// ============================================================================

// Convert 9D coordinates to flat index with toroidal wrapping
// Returns -1 on ERR
int64_t npk_ttensor_tbb32_toroidal_index(const ttensor_tbb32* t, const int32_t coords[9]);

// Wrap coordinates to valid range [0, dim) for each dimension
void npk_ttensor_tbb32_wrap_coords(const ttensor_tbb32* t, const int32_t in[9], int32_t out[9]);

// ============================================================================
// Element Access
// ============================================================================

// Get element at 9D coordinates (with toroidal wrapping)
int32_t npk_ttensor_tbb32_get(const ttensor_tbb32* t, const int32_t coords[9]);

// Set element at 9D coordinates (with toroidal wrapping)
void npk_ttensor_tbb32_set(ttensor_tbb32* t, const int32_t coords[9], int32_t value);

// ============================================================================
// Safety Checks
// ============================================================================

bool npk_ttensor_tbb32_has_err(const ttensor_tbb32* t);
bool npk_ttensor_tbb32_is_zero(const ttensor_tbb32* t);

// Circuit Breaker: Check if ERR density exceeds threshold
// Returns true if density > threshold (triggers emergency shutdown)
// Threshold: fraction of total elements (e.g., 0.01 = 1%)
bool npk_ttensor_tbb32_circuit_breaker(const ttensor_tbb32* t, float threshold);

// ============================================================================
// Toroidal Distance
// ============================================================================

// Compute toroidal distance between two points in 9D space
// Uses geodesic (shortest path with wrap-around)
int64_t npk_ttensor_tbb32_toroidal_distance_sq(const ttensor_tbb32* t, 
                                                 const int32_t a[9], 
                                                 const int32_t b[9]);

// ============================================================================
// Wave Interference Operations
// ============================================================================

// Compute wave value at point from 9 emitters in toroidal space
// wave_value[point] = sum_i amplitude[i] * decay(toroidal_distance(point, emitter[i]))
ttensor_tbb32 npk_ttensor_tbb32_wave_interference(const ttensor_tbb32* base,
                                                    const int32_t emitters[9][9],
                                                    const int32_t amplitudes[9],
                                                    float decay_factor);

// ============================================================================
// Mamba SSM Integration
// ============================================================================

// Selective State Space scan along toroidal axis
// Propagates state with gating (selective memory)
// axis: 0-8 (which dimension to scan along)
// state: input state tensor (updated in-place)
// gate: gating values (0 = forget, 1 = remember)
ttensor_tbb32 npk_ttensor_tbb32_mamba_scan(const ttensor_tbb32* input,
                                            ttensor_tbb32* state,
                                            const ttensor_tbb32* gate,
                                            int32_t axis);

// ============================================================================
// Arithmetic Operations
// ============================================================================

ttensor_tbb32 npk_ttensor_tbb32_add(const ttensor_tbb32* a, const ttensor_tbb32* b);
ttensor_tbb32 npk_ttensor_tbb32_sub(const ttensor_tbb32* a, const ttensor_tbb32* b);
ttensor_tbb32 npk_ttensor_tbb32_mul_elementwise(const ttensor_tbb32* a, const ttensor_tbb32* b);
ttensor_tbb32 npk_ttensor_tbb32_scale(const ttensor_tbb32* t, int32_t scalar);

// ============================================================================
// 9D Convolution (for wave processing)
// ============================================================================

// Convolve tensor with 9D kernel (toroidal boundary conditions)
// kernel_size: size of kernel along each dimension (must be odd)
ttensor_tbb32 npk_ttensor_tbb32_convolve(const ttensor_tbb32* input,
                                          const ttensor_tbb32* kernel);

// ============================================================================
// Utility
// ============================================================================

const char* npk_ttensor_tbb32_to_string(const ttensor_tbb32* t);
bool npk_ttensor_tbb32_equal(const ttensor_tbb32* a, const ttensor_tbb32* b);

// Print tensor dimensions and statistics
void npk_ttensor_tbb32_print_stats(const ttensor_tbb32* t);

#ifdef __cplusplus
}
#endif

#endif // ARIA_TTENSOR_OPS_H
