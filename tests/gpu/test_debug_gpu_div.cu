#include <stdio.h>
#include <stdint.h>
#include <cuda_runtime.h>

// Minimal aria_fix256_t definition
typedef struct {
    uint64_t limbs[4];
} aria_fix256_t;

// Forward declare GPU function
__device__ __host__ aria_fix256_t aria_fix256_div_gpu(aria_fix256_t a, aria_fix256_t b);

__device__ __host__ aria_fix256_t make_int(int64_t val) {
    aria_fix256_t result;
    result.limbs[0] = 0;
    result.limbs[1] = 0;
    result.limbs[2] = (uint64_t)val;
    result.limbs[3] = (val < 0) ? ~0ULL : 0;
    return result;
}

__global__ void debug_division_kernel(int64_t a_val, int64_t b_val, aria_fix256_t* result) {
    aria_fix256_t a = make_int(a_val);
    aria_fix256_t b = make_int(b_val);
    
    printf("[GPU] Input A: [%016lx, %016lx, %016lx, %016lx]\n", 
           a.limbs[0], a.limbs[1], a.limbs[2], a.limbs[3]);
    printf("[GPU] Input B: [%016lx, %016lx, %016lx, %016lx]\n", 
           b.limbs[0], b.limbs[1], b.limbs[2], b.limbs[3]);
    
    aria_fix256_t res = aria_fix256_div_gpu(a, b);
    
    printf("[GPU] Result: [%016lx, %016lx, %016lx, %016lx]\n", 
           res.limbs[0], res.limbs[1], res.limbs[2], res.limbs[3]);
    printf("[GPU] Result as int: %ld\n", (int64_t)res.limbs[2]);
    
    *result = res;
}

int main() {
    printf("=== GPU Division Debug Test ===\n\n");
    
    aria_fix256_t* d_result;
    cudaMalloc(&d_result, sizeof(aria_fix256_t));
    
    // Test 10 / 2
    printf("Testing 10 / 2:\n");
    debug_division_kernel<<<1, 1>>>(10, 2, d_result);
    cudaDeviceSynchronize();
    
    aria_fix256_t result;
    cudaMemcpy(&result, d_result, sizeof(aria_fix256_t), cudaMemcpyDeviceToHost);
    
    printf("[Host] Result: [%016lx, %016lx, %016lx, %016lx]\n", 
           result.limbs[0], result.limbs[1], result.limbs[2], result.limbs[3]);
    printf("[Host] Result as int: %ld\n\n", (int64_t)result.limbs[2]);
    
    // Test 123 / 123
    printf("Testing 123 / 123:\n");
    debug_division_kernel<<<1, 1>>>(123, 123, d_result);
    cudaDeviceSynchronize();
    
    cudaMemcpy(&result, d_result, sizeof(aria_fix256_t), cudaMemcpyDeviceToHost);
    
    printf("[Host] Result: [%016lx, %016lx, %016lx, %016lx]\n", 
           result.limbs[0], result.limbs[1], result.limbs[2], result.limbs[3]);
    printf("[Host] Result as int: %ld\n", (int64_t)result.limbs[2]);
    
    cudaFree(d_result);
    return 0;
}
