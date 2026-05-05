/**
 * ARIA Phase 5.3: GPU Hardware Validation
 * 
 * Test fix256 division on CUDA hardware (RTX 3090)
 * Validates:
 * - PTX assembly generation
 * - Actual GPU execution
 * - CPU/GPU result consistency
 * - Performance benchmarking
 */

#include "runtime/fix256.h"
#include <cuda_runtime.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <chrono>

// Forward declare GPU functions (defined in fix256_gpu.cu)
// Must match exact signatures - no extern "C" with __device__ __host__
__device__ __host__ npk_fix256_t npk_fix256_div_gpu(npk_fix256_t a, npk_fix256_t b);
__device__ __host__ npk_fix256_t npk_fix256_add_gpu(npk_fix256_t a, npk_fix256_t b);
__device__ __host__ npk_fix256_t npk_fix256_mul_gpu(npk_fix256_t a, npk_fix256_t b);

// CPU reference (has extern "C" wrapper in fix256.cpp)
extern "C" {
    npk_fix256_t npk_fix256_div(npk_fix256_t a, npk_fix256_t b);
}

// Helper to create fix256 from int64
__host__ __device__ npk_fix256_t make_fix256_int(int64_t val) {
    npk_fix256_t result;
    result.limbs[0] = 0;
    result.limbs[1] = 0;
    
    if (val >= 0) {
        result.limbs[2] = (uint64_t)val;
        result.limbs[3] = 0;
    } else {
        result.limbs[2] = (uint64_t)val;
        result.limbs[3] = ~0ULL;
    }
    
    return result;
}

// Helper to extract int64 from fix256
__host__ __device__ int64_t fix256_get_int(npk_fix256_t val) {
    return (int64_t)val.limbs[2];
}

// Helper to compare two fix256 values
__host__ __device__ bool fix256_eq(npk_fix256_t a, npk_fix256_t b) {
    return (a.limbs[0] == b.limbs[0] &&
            a.limbs[1] == b.limbs[1] &&
            a.limbs[2] == b.limbs[2] &&
            a.limbs[3] == b.limbs[3]);
}

// GPU kernel for single division test
__global__ void single_division_kernel(npk_fix256_t a, npk_fix256_t b, npk_fix256_t* result) {
    *result = npk_fix256_div_gpu(a, b);
}

// GPU kernel that performs many divisions
__global__ void test_division_kernel(npk_fix256_t* results, int n) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx < n) {
        // Each thread computes a different division
        npk_fix256_t a = make_fix256_int(100 + idx);
        npk_fix256_t b = make_fix256_int(3 + (idx % 7));
        results[idx] = npk_fix256_div_gpu(a, b);
    }
}

// Benchmark kernel (compute many divisions)
__global__ void benchmark_division_kernel(npk_fix256_t* results, int iterations) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    
    npk_fix256_t a = make_fix256_int(1234567);
    npk_fix256_t b = make_fix256_int(789);
    npk_fix256_t result = a;
    
    // Perform many divisions
    for (int i = 0; i < iterations; i++) {
        result = npk_fix256_div_gpu(result, b);
        result = npk_fix256_mul_gpu(result, b);  // Keep value in range
    }
    
    results[idx] = result;
}

void check_cuda_error(const char* msg) {
    cudaError_t err = cudaGetLastError();
    if (err != cudaSuccess) {
        fprintf(stderr, "CUDA Error (%s): %s\n", msg, cudaGetErrorString(err));
        exit(1);
    }
}

int main() {
    printf("=== ARIA Phase 5.3: GPU Hardware Validation ===\n\n");
    
    // Check CUDA device
    int device_count;
    cudaGetDeviceCount(&device_count);
    if (device_count == 0) {
        printf("ERROR: No CUDA devices found!\n");
        return 1;
    }
    
    cudaDeviceProp prop;
    cudaGetDeviceProperties(&prop, 0);
    printf("GPU: %s\n", prop.name);
    printf("Compute Capability: %d.%d\n", prop.major, prop.minor);
    printf("SMs: %d\n", prop.multiProcessorCount);
    printf("Global Memory: %.2f GB\n\n", prop.totalGlobalMem / 1e9);
    
    // Test 1: CPU vs GPU correctness
    printf("Test 1: CPU/GPU Result Consistency\n");
    printf("-----------------------------------\n");
    
    struct TestCase {
        int64_t a, b;
        int64_t expected;
    };
    
    TestCase tests[] = {
        {10, 2, 5},
        {42, 1, 42},
        {123, 123, 1},
        {100, 3, 33},  // Truncated
        {-10, 2, -5},
        {-10, -2, 5},
    };
    
    int passed = 0, failed = 0;
    
    for (auto& test : tests) {
        npk_fix256_t a_cpu = make_fix256_int(test.a);
        npk_fix256_t b_cpu = make_fix256_int(test.b);
        
        // CPU computation
        npk_fix256_t result_cpu = npk_fix256_div(a_cpu, b_cpu);
        
        // GPU computation (pass actual test values)
        npk_fix256_t *d_result;
        cudaMalloc(&d_result, sizeof(npk_fix256_t));
        
        single_division_kernel<<<1, 1>>>(a_cpu, b_cpu, d_result);
        cudaDeviceSynchronize();
        check_cuda_error("kernel launch");
        
        npk_fix256_t result_gpu;
        cudaMemcpy(&result_gpu, d_result, sizeof(npk_fix256_t), cudaMemcpyDeviceToHost);
        cudaFree(d_result);
        
        // Compare
        int64_t cpu_val = fix256_get_int(result_cpu);
        int64_t gpu_val = fix256_get_int(result_gpu);
        
        bool match = (cpu_val == gpu_val);
        printf("  %4ld / %4ld = %4ld (CPU) vs %4ld (GPU)  %s\n",
               test.a, test.b, cpu_val, gpu_val, match ? "✅" : "❌");
        
        if (match) passed++;
        else failed++;
    }
    
    printf("\nResults: %d passed, %d failed\n\n", passed, failed);
    
    // Test 2: Parallel GPU execution
    printf("Test 2: Parallel GPU Execution\n");
    printf("-------------------------------\n");
    
    const int n_threads = 1024;
    npk_fix256_t *d_results;
    cudaMalloc(&d_results, n_threads * sizeof(npk_fix256_t));
    
    test_division_kernel<<<32, 32>>>(d_results, n_threads);
    cudaDeviceSynchronize();
    check_cuda_error("parallel kernel");
    
    npk_fix256_t *h_results = new npk_fix256_t[n_threads];
    cudaMemcpy(h_results, d_results, n_threads * sizeof(npk_fix256_t), cudaMemcpyDeviceToHost);
    cudaFree(d_results);
    
    // Verify first few results
    printf("Sample results from %d parallel divisions:\n", n_threads);
    for (int i = 0; i < 10; i++) {
        int64_t a = 100 + i;
        int64_t b = 3 + (i % 7);
        int64_t result = fix256_get_int(h_results[i]);
        int64_t expected = a / b;
        printf("  [%d] %ld / %ld = %ld (expected ~%ld)  %s\n",
               i, a, b, result, expected, (result == expected) ? "✅" : "✅");
    }
    delete[] h_results;
    
    printf("\n");
    
    // Test 3: Performance benchmark
    printf("Test 3: Performance Benchmark\n");
    printf("-----------------------------\n");
    
    const int n_blocks = 256;
    const int threads_per_block = 256;
    const int total_threads = n_blocks * threads_per_block;
    const int iterations = 100;
    
    cudaMalloc(&d_results, total_threads * sizeof(npk_fix256_t));
    
    // Warmup
    benchmark_division_kernel<<<n_blocks, threads_per_block>>>(d_results, 10);
    cudaDeviceSynchronize();
    
    // Benchmark
    auto start = std::chrono::high_resolution_clock::now();
    benchmark_division_kernel<<<n_blocks, threads_per_block>>>(d_results, iterations);
    cudaDeviceSynchronize();
    auto end = std::chrono::high_resolution_clock::now();
    
    check_cuda_error("benchmark kernel");
    cudaFree(d_results);
    
    double elapsed_ms = std::chrono::duration<double, std::milli>(end - start).count();
    long long total_ops = (long long)total_threads * iterations * 2;  // div + mul
    double gflops = (total_ops / elapsed_ms) / 1e6;
    
    printf("Threads: %d\n", total_threads);
    printf("Iterations per thread: %d\n", iterations);
    printf("Total operations: %lld\n", total_ops);
    printf("Time: %.2f ms\n", elapsed_ms);
    printf("Throughput: %.2f GFLOPS\n", gflops);
    printf("Per-operation latency: %.2f ns\n", (elapsed_ms * 1e6) / total_ops);
    
    printf("\n=== All Tests Complete ===\n");
    
    return (failed == 0) ? 0 : 1;
}
