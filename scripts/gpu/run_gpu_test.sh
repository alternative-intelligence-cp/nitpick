#!/bin/bash
# Build and run GPU hardware validation test

set -e

echo "=== Building GPU Test with NVCC ==="

# Check if nvcc is available
if ! command -v nvcc &> /dev/null; then
    echo "ERROR: nvcc not found. Please install CUDA Toolkit."
    echo "Install with: sudo apt install nvidia-cuda-toolkit"
    exit 1
fi

# Show CUDA version
nvcc --version

echo ""
echo "Compiling GPU test..."

# Compile CUDA test - need relocatable device code for cross-file device calls
nvcc -std=c++17 -O3 \
    -I./include \
    -o build/test_fix256_gpu_cuda \
    test_fix256_gpu_cuda.cu \
    src/runtime/math/fix256_gpu.cu \
    src/runtime/math/fix256.cpp \
    -arch=sm_86 \
    -rdc=true \
    --expt-relaxed-constexpr \
    -Xcompiler -fPIC

if [ $? -ne 0 ]; then
    echo ""
    echo "ERROR: Compilation failed. Trying with verbose output..."
    nvcc -std=c++17 -O3 \
        -I./include \
        -o build/test_fix256_gpu_cuda \
        test_fix256_gpu_cuda.cu \
        src/runtime/math/fix256_gpu.cu \
        src/runtime/math/fix256.cpp \
        -arch=sm_86 \
        -rdc=true \
        --expt-relaxed-constexpr \
        -Xcompiler -fPIC \
        -v
    exit 1
fi

echo ""
echo "=== Running GPU Test ==="
echo ""

# Run on GPU
./build/test_fix256_gpu_cuda

echo ""
echo "=== Test Complete ==="
