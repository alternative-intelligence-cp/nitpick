# GPU/PTX Backend Design
**Date:** February 12, 2026  
**Status:** Design Phase  
**Target:** <1ms physics timestep for Nikola AGI

---

## 1. Executive Summary

### Mission
Enable Aria to generate NVIDIA PTX (Parallel Thread Execution) code for GPU acceleration, targeting Nikola's real-time wave mechanics substrate with <1ms physics timesteps across diverse hardware (datacenter GPUs → smartwatch).

### Key Requirements
1. **Deterministic Arithmetic:** fix256 operations must be bit-exact across all GPU hardware
2. **Performance:** <1ms physics timestep (vs current CPU: ~10-50ms)
3. **Portability:** Same PTX code runs on datacenter → mobile GPUs
4. **Safety:** Layered safety system (Result<T> + unknown + failsafe) on GPU
5. **Integration:** Seamless CPU↔GPU data transfer

---

## 2. Current Architecture Analysis

### Current Compilation Pipeline
```
Aria Source (.aria)
    ↓ Lexer
Tokens
    ↓ Parser
AST
    ↓ Semantic Analysis
Typed AST
    ↓ IR Generator
LLVM IR
    ↓ Target Machine (x86_64/ARM64)
Assembly (.s) / Executable
```

### Current Backend Structure
- **Location:** `src/backend/ir/`
- **Key Files:**
  - `ir_generator.cpp` (7560 lines) - Main IR generation
  - `codegen_expr.cpp` - Expression codegen
  - `codegen_stmt.cpp` - Statement codegen
  - `tbb_codegen.cpp` - TBB arithmetic with overflow detection
  - `ternary_codegen.cpp` - Balanced ternary/nonary arithmetic

### Current Target Support
- **Native Target:** x86_64 (Linux), ARM64 (macOS/Linux)
- **Initialization:** `InitializeNativeTarget()`
- **Triple:** `llvm::sys::getDefaultTargetTriple()`
- **Machine:** `TargetMachine` with PIC relocation

---

## 3. GPU/PTX Backend Architecture

### 3.1 Compilation Modes

**Mode 1: CPU-Only (Current)**
```
ariac physics.aria -o physics          # Native x86_64 executable
```

**Mode 2: GPU-Only (New)**
```
ariac physics.aria --emit-ptx -o physics.ptx   # PTX module
```

**Mode 3: Heterogeneous (Future)**
```
ariac physics.aria --target=gpu+cpu -o physics # Fat binary with both
```

### 3.2 PTX Code Generation Flow
```
Aria Source (.aria)
    ↓ Parser & Semantic Analysis
Typed AST
    ↓ IR Generator (GPU Mode)
LLVM IR (with NVPTX attributes)
    ↓ NVPTX Target Machine
PTX Assembly (.ptx)
    ↓ CUDA Driver (runtime)
GPU Executable (cubin)
```

### 3.3 New Compiler Options

Add to `CompilerOptions` struct:
```cpp
struct CompilerOptions {
    // ... existing fields ...
    
    // GPU/PTX backend
    bool emit_ptx = false;              // --emit-ptx
    std::string target_arch;            // --target=gpu, --target=cpu, --target=gpu+cpu
    std::string gpu_arch = "sm_50";     // --gpu-arch=sm_75 (CUDA compute capability)
    int gpu_opt_level = 3;              // --gpu-opt (default O3 for GPU)
    bool enable_gpu_debug = false;      // --gpu-debug (line info in PTX)
};
```

Command-line flags:
```bash
--emit-ptx                     # Generate PTX assembly
--target=gpu                   # Target GPU only
--target=cpu                   # Target CPU only (default)
--target=gpu+cpu               # Heterogeneous (future)
--gpu-arch=sm_XX               # CUDA compute capability (sm_50, sm_75, sm_86, sm_89, sm_90)
--gpu-opt=<0-3>                # GPU optimization level (default: 3)
--gpu-debug                    # Embed debug info in PTX
```

---

## 4. Implementation Plan

### Phase 1: NVPTX Target Initialization ✅
**Timeline:** 2-3 hours  
**Files Modified:** `src/main.cpp`

```cpp
// Add to main.cpp
void initialize_gpu_targets() {
    // Initialize NVPTX backend
    LLVMInitializeNVPTXTarget();
    LLVMInitializeNVPTXTargetInfo();
    LLVMInitializeNVPTXTargetMC();
    LLVMInitializeNVPTXAsmPrinter();
}
```

**Tasks:**
- [x] Add NVPTX include headers
- [ ] Add GPU target initialization function
- [ ] Call initialization in main() before compilation
- [ ] Test: Verify NVPTX target is registered

### Phase 2: PTX Emission Function 🔄
**Timeline:** 4-6 hours  
**Files Modified:** `src/main.cpp`

```cpp
/**
 * Emit PTX assembly for GPU execution
 * 
 * @param module LLVM module to compile
 * @param output_file Output PTX file path
 * @param gpu_arch CUDA compute capability (e.g., "sm_75")
 * @return true on success
 */
bool emit_ptx(
    llvm::Module* module,
    const std::string& output_file,
    const std::string& gpu_arch = "sm_50"
) {
    // 1. Set target triple to NVPTX64
    module->setTargetTriple("nvptx64-nvidia-cuda");
    
    // 2. Lookup NVPTX target
    std::string error;
    auto target = llvm::TargetRegistry::lookupTarget("nvptx64", error);
    if (!target) {
        std::cerr << "Error: NVPTX target not found: " << error << "\n";
        return false;
    }
    
    // 3. Configure target machine
    std::string cpu = gpu_arch;  // e.g., "sm_75"
    std::string features = "+ptx70";  // PTX ISA version 7.0+
    llvm::TargetOptions opts;
    
    auto target_machine = target->createTargetMachine(
        "nvptx64-nvidia-cuda",
        cpu,
        features,
        llvm::OptimizationLevel::O3,  // GPU code benefits from optimization
        llvm::Reloc::PIC_,
        std::nullopt
    );
    
    // 4. Set data layout
    module->setDataLayout(target_machine->createDataLayout());
    
    // 5. Emit PTX assembly
    std::error_code ec;
    llvm::raw_fd_ostream out(output_file, ec, llvm::sys::fs::OF_None);
    if (ec) {
        std::cerr << "Error: Could not open PTX output file: " << ec.message() << "\n";
        return false;
    }
    
    llvm::legacy::PassManager pass;
    auto file_type = llvm::CodeGenFileType::AssemblyFile;  // PTX is assembly
    
    if (target_machine->addPassesToEmitFile(pass, out, nullptr, file_type)) {
        std::cerr << "Error: NVPTX target can't emit PTX file\n";
        return false;
    }
    
    pass.run(*module);
    return true;
}
```

**Tasks:**
- [ ] Implement emit_ptx() function
- [ ] Add PTX emission to main() pipeline
- [ ] Handle --emit-ptx flag
- [ ] Test: Compile simple Aria function to PTX

### Phase 3: GPU Kernel Attributes 🔄
**Timeline:** 4-6 hours  
**Files Modified:** `src/backend/ir/ir_generator.cpp`

PTX requires kernel functions to be marked with special attributes:

```cpp
// In IRGenerator::genFuncDecl()
if (is_gpu_kernel(func_decl)) {
    // Mark as CUDA kernel
    llvm::Metadata* md_args[] = {
        llvm::ValueAsMetadata::get(llvm_func),
        llvm::MDString::get(context, "kernel"),
        llvm::ConstantAsMetadata::get(llvm::ConstantInt::get(
            llvm::Type::getInt32Ty(context), 1
        ))
    };
    
    llvm::MDNode* kernel_md = llvm::MDNode::get(context, md_args);
    module->getOrInsertNamedMetadata("nvvm.annotations")->addOperand(kernel_md);
}
```

**Aria Syntax for GPU Kernels:**
```aria
// GPU kernel function (public, can be launched from CPU)
#[gpu_kernel]
func:physics_step = void(float64@:positions, int32:num_particles) {
    int32:tid = gpu.thread_id();
    if (tid < num_particles) {
        // Update particle position...
    }
}

// GPU device function (internal, called from kernels)
#[gpu_device]
func:compute_force = float64(float64:pos1, float64:pos2) {
    // Calculate force between particles...
}
```

**Tasks:**
- [ ] Add attribute parsing (#[gpu_kernel], #[gpu_device])
- [ ] Add NVVM annotations to kernel functions
- [ ] Implement gpu.thread_id() intrinsic
- [ ] Test: Generate kernel with proper attributes

### Phase 4: GPU Intrinsics 🔄
**Timeline:** 6-8 hours  
**Files Modified:** `src/stdlib/cuda/`, `src/backend/ir/ir_generator.cpp`

**GPU Intrinsics Needed:**
```aria
// Thread indexing
int32:tid = gpu.thread_id();        // threadIdx.x
int32:bid = gpu.block_id();         // blockIdx.x
int32:threads = gpu.block_size();   // blockDim.x
int32:gid = bid * threads + tid;    // Global thread ID

// Synchronization
gpu.sync_threads();                 // __syncthreads()

// Shared memory
#[gpu_shared]
float64[256]:shared_data;           // __shared__ float64 data[256];

// Atomic operations
gpu.atomic_add(@old_val, 1.0);     // atomicAdd
gpu.atomic_cas(@ptr, old, new);    // atomicCAS
```

**LLVM IR Generation:**
```cpp
// gpu.thread_id() → llvm.nvvm.read.ptx.sreg.tid.x
llvm::Value* IRGenerator::genGPUThreadID() {
    llvm::Function* intrinsic = llvm::Intrinsic::getDeclaration(
        module.get(),
        llvm::Intrinsic::nvvm_read_ptx_sreg_tid_x
    );
    return builder.CreateCall(intrinsic, {}, "tid");
}

// gpu.sync_threads() → llvm.nvvm.barrier0
void IRGenerator::genGPUSyncThreads() {
    llvm::Function* intrinsic = llvm::Intrinsic::getDeclaration(
        module.get(),
        llvm::Intrinsic::nvvm_barrier0
    );
    builder.CreateCall(intrinsic);
}
```

**Tasks:**
- [ ] Implement GPU intrinsic library (stdlib/cuda/)
- [ ] Add NVVM intrinsic mappings in IR generator
- [ ] Support shared memory allocation
- [ ] Support atomic operations
- [ ] Test: Kernel with thread indexing

### Phase 5: fix256 Deterministic Arithmetic on GPU 🔄
**Timeline:** 8-12 hours (CRITICAL for Nikola)  
**Files Modified:** `src/backend/runtime/fix256_gpu.cu`

**Challenge:** GPU floating-point is non-deterministic across hardware.  
**Solution:** Software fix256 implementation in PTX.

**Approach 1: Pure PTX Implementation**
- Implement fix256 add/sub/mul/div in PTX assembly
- Guarantee bit-exact results across all GPUs
- ~10-20x slower than native float64, but deterministic

**Approach 2: Integer SIMD (Preferred)**
```cuda
// fix256 = 256-bit fixed-point (128.128 format)
// Represented as 4x int64 (256 bits total)
struct fix256 {
    int64_t limb[4];  // [0] = bits 0-63, [1] = 64-127, [2] = 128-191, [3] = 192-255
};

__device__ fix256 fix256_add(fix256 a, fix256 b) {
    fix256 result;
    uint64_t carry = 0;
    
    // Add limbs with carry propagation
    for (int i = 0; i < 4; i++) {
        __uint128_t sum = (__uint128_t)a.limb[i] + b.limb[i] + carry;
        result.limb[i] = (int64_t)sum;
        carry = sum >> 64;
    }
    
    return result;
}
```

**PTX Generation:**
```ptx
.func (.param .b64 result) fix256_add(
    .param .b64 a_ptr,
    .param .b64 b_ptr
) {
    .reg .b64 %a_limb, %b_limb, %sum, %carry;
    .reg .pred %carry_pred;
    
    // Load limbs from memory
    ld.param.b64 %a_ptr, [a_ptr];
    ld.param.b64 %b_ptr, [b_ptr];
    
    // Add limb 0 with carry
    ld.u64 %a_limb, [%a_ptr];
    ld.u64 %b_limb, [%b_ptr + 8];
    add.cc.u64 %sum, %a_limb, %b_limb;
    
    // Store result and handle carry...
}
```

**Tasks:**
- [ ] Design fix256 representation for GPU
- [ ] Implement fix256 arithmetic in CUDA C
- [ ] Generate PTX code for fix256 ops
- [ ] Verify bit-exact determinism across GPU architectures
- [ ] Benchmark performance (target: <10x slowdown vs native float64)

### Phase 6: Memory Transfer & Kernel Launch 🔄
**Timeline:** 6-8 hours  
**Files Modified:** `src/runtime/cuda/kernel_launcher.cpp`

**Runtime API for GPU Execution:**
```aria
// Allocate GPU memory
float64@:gpu_data = gpu.alloc(1024 * sizeof(float64));

// Copy CPU → GPU
float64[1024]:cpu_data = init_data();
gpu.copy_to_device(gpu_data, @cpu_data[0], 1024 * sizeof(float64));

// Launch kernel
gpu.launch(
    physics_step,               // Kernel function
    blocks: 32,                 // Grid size (32 blocks)
    threads: 256,               // Block size (256 threads/block)
    args: [gpu_data, 1024]      // Kernel arguments
);

// Synchronize (wait for kernel completion)
gpu.sync();

// Copy GPU → CPU
gpu.copy_from_device(@cpu_data[0], gpu_data, 1024 * sizeof(float64));

// Free GPU memory
gpu.free(gpu_data);
```

**CUDA Runtime Wrapper:**
```cpp
extern "C" {
    // Allocate device memory
    void* aria_gpu_alloc(size_t bytes);
    
    // Free device memory
    void aria_gpu_free(void* ptr);
    
    // Copy CPU → GPU
    void aria_gpu_copy_to_device(void* dst, const void* src, size_t bytes);
    
    // Copy GPU → CPU
    void aria_gpu_copy_from_device(void* dst, const void* src, size_t bytes);
    
    // Launch kernel
    void aria_gpu_launch(
        const void* kernel_func,
        unsigned int grid_x,
        unsigned int grid_y,
        unsigned int grid_z,
        unsigned int block_x,
        unsigned int block_y,
        unsigned int block_z,
        void** args,
        size_t shared_mem_bytes
    );
    
    // Synchronize device
    void aria_gpu_sync();
}
```

**Tasks:**
- [ ] Implement CUDA runtime wrapper
- [ ] Add GPU memory management intrinsics
- [ ] Add kernel launch intrinsics
- [ ] Test: Launch simple kernel, verify results

### Phase 7: Error Handling on GPU 🔄
**Timeline:** 4-6 hours (Layered Safety System)  
**Challenge:** How to handle Result<T>, unknown, failsafe() on GPU?

**Approach: Error Codes + Host-Side Failsafe**
```aria
#[gpu_kernel]
func:physics_step = void(
    float64@:positions,
    int32:num_particles,
    int32@:error_code  // Output: error code per thread
) {
    int32:tid = gpu.thread_id();
    if (tid >= num_particles) {
        pass;  // Early exit, no error
    }
    
    // Try to compute force
    Result<float64>:force_result = compute_force(positions[tid]);
    
    if (force_result is fail) {
        error_code[tid] = -1;  // Mark error
        pass;
    }
    
    float64:force = force_result?;  // Unwrap (guaranteed safe)
    positions[tid] = positions[tid] + force * dt;
    error_code[tid] = 0;  // Success
}
```

**Host-Side Error Check:**
```aria
int32[1024]:error_codes = gpu_alloc(1024 * sizeof(int32));

gpu.launch(physics_step, blocks: 4, threads: 256, args: [positions, 1024, error_codes]);
gpu.sync();

// Check for errors
int32[1024]:cpu_errors;
gpu.copy_from_device(@cpu_errors[0], error_codes, 1024 * sizeof(int32));

for (int32:i = 0; i < 1024; i = i + 1) {
    if (cpu_errors[i] != 0) {
        !!! 42;  // GPU kernel failed, invoke failsafe
    }
}
```

**Tasks:**
- [ ] Design GPU error handling strategy
- [ ] Implement Result<T> on GPU (error code propagation)
- [ ] Add host-side error checking
- [ ] Test: Kernel with error handling

---

## 5. Testing Strategy

### Test 1: Simple Kernel ✅
```aria
#[gpu_kernel]
func:vector_add = void(float64@:a, float64@:b, float64@:c, int32:n) {
    int32:tid = gpu.thread_id() + gpu.block_id() * gpu.block_size();
    if (tid < n) {
        c[tid] = a[tid] + b[tid];
    }
}
```
**Expected PTX:**
```ptx
.visible .entry vector_add(
    .param .u64 a,
    .param .u64 b,
    .param .u64 c,
    .param .u32 n
) {
    .reg .pred %p<2>;
    .reg .b32 %r<4>;
    .reg .f64 %fd<3>;
    .reg .b64 %rd<7>;

    // tid = threadIdx.x + blockIdx.x * blockDim.x
    mov.u32 %r1, %tid.x;
    mov.u32 %r2, %ctaid.x;
    mov.u32 %r3, %ntid.x;
    mad.lo.s32 %r0, %r2, %r3, %r1;

    // if (tid < n)
    ld.param.u32 %r4, [n];
    setp.ge.s32 %p0, %r0, %r4;
    @%p0 bra RETURN;

    // c[tid] = a[tid] + b[tid]
    cvt.s64.s32 %rd0, %r0;
    shl.b64 %rd1, %rd0, 3;  // tid * 8 (sizeof float64)
    
    ld.param.u64 %rd2, [a];
    add.s64 %rd3, %rd2, %rd1;
    ld.global.f64 %fd0, [%rd3];
    
    ld.param.u64 %rd4, [b];
    add.s64 %rd5, %rd4, %rd1;
    ld.global.f64 %fd1, [%rd5];
    
    add.f64 %fd2, %fd0, %fd1;
    
    ld.param.u64 %rd6, [c];
    add.s64 %rd7, %rd6, %rd1;
    st.global.f64 [%rd7], %fd2;

RETURN:
    ret;
}
```

### Test 2: Shared Memory Reduction ✅
```aria
#[gpu_kernel]
func:sum_reduce = void(float64@:input, float64@:output, int32:n) {
    #[gpu_shared]
    float64[256]:sdata;
    
    int32:tid = gpu.thread_id();
    int32:gid = gpu.block_id() * gpu.block_size() + tid;
    
    // Load data into shared memory
    if (gid < n) {
        sdata[tid] = input[gid];
    } else {
        sdata[tid] = 0.0;
    }
    
    gpu.sync_threads();
    
    // Parallel reduction in shared memory
    int32:stride = gpu.block_size() / 2;
    while (stride > 0) {
        if (tid < stride) {
            sdata[tid] = sdata[tid] + sdata[tid + stride];
        }
        gpu.sync_threads();
        stride = stride / 2;
    }
    
    // Thread 0 writes result
    if (tid == 0) {
        output[gpu.block_id()] = sdata[0];
    }
}
```

### Test 3: fix256 Physics Kernel 🎯 (Nikola Target)
```aria
#[gpu_kernel]
func:wave_propagation = void(
    fix256@:wave,          // Wave amplitude field
    fix256@:laplacian,     // Laplacian of wave
    fix256:dt,             // Timestep (fixed-point)
    int32:grid_size
) {
    int32:tid = gpu.thread_id() + gpu.block_id() * gpu.block_size();
    
    if (tid < grid_size) {
        // Wave equation: ∂²ψ/∂t² = c² ∇²ψ
        // Update: ψ(t+dt) = ψ(t) + dt * ∂ψ/∂t
        fix256:c_squared = fix256_from_int(340 * 340);  // Speed of sound squared
        fix256:acceleration = fix256_mul(c_squared, laplacian[tid]);
        
        wave[tid] = fix256_add(wave[tid], fix256_mul(dt, acceleration));
    }
}
```

---

## 6. Performance Targets (Nikola AGI)

### Current CPU Performance (Baseline)
```
Grid Size: 1024 × 1024 × 1024 (1B cells)
Timestep: ~50ms (20 FPS)
Hardware: AMD Ryzen 9 5950X (16 cores)
```

### Target GPU Performance
```
Grid Size: 1024 × 1024 × 1024 (1B cells)
Timestep: <1ms (1000 FPS)
Hardware: NVIDIA RTX 4090 (16,384 CUDA cores)
Speedup: ~50x minimum
```

### Portability Targets
| Hardware | CUDA Cores | Memory | Expected Timestep |
|----------|-----------|--------|-------------------|
| RTX 4090 (Datacenter) | 16,384 | 24 GB | <1ms |
| RTX 4070 (Desktop) | 5,888 | 12 GB | ~2ms |
| RTX 3060 (Laptop) | 3,584 | 6 GB | ~4ms |
| Tegra Xavier (Embedded) | 512 | 8 GB | ~20ms |
| Snapdragon (Mobile) | 256 | 4 GB | ~40ms* |

*Hypothetical - Snapdragon uses Adreno (not CUDA), would need OpenCL backend

---

## 7. Success Criteria

1. ✅ **PTX Generation:** Compile simple Aria function to valid PTX
2. ✅ **Kernel Launch:** Launch GPU kernel from Aria runtime
3. ✅ **Thread Indexing:** Correctly compute thread IDs and grid dimensions
4. ✅ **Shared Memory:** Allocate and use shared memory in kernels
5. ✅ **fix256 Determinism:** Bit-exact results across GPU architectures
6. ✅ **Performance:** <1ms timestep for Nikola physics (1B cells)
7. ✅ **Error Handling:** Result<T> works on GPU with host-side failsafe
8. ✅ **Memory Safety:** No GPU memory leaks or corruption

---

## 8. Future Enhancements (Post-MVP)

### 8.1 Multi-GPU Support
```aria
// Distribute work across 4 GPUs
gpu.set_device(0);
gpu.launch(physics_step_gpu0, ...);

gpu.set_device(1);
gpu.launch(physics_step_gpu1, ...);

gpu.sync_all_devices();
```

### 8.2 Unified Memory
```aria
// Automatic CPU↔GPU transfers
#[unified_memory]
float64[1M]:data = init_data();

gpu.launch(process, args: [@data[0], 1M]);  // Data transfers automatically
```

### 8.3 Dynamic Parallelism
```aria
#[gpu_kernel]
func:parent_kernel = void() {
    // Launch child kernel from GPU
    gpu.launch_child(child_kernel, blocks: 16, threads: 128);
}
```

### 8.4 Tensor Cores
```aria
// Use Tensor Cores for matrix multiplication
#[gpu_kernel]
#[use_tensor_cores]
func:matmul = void(float16@:A, float16@:B, float32@:C) {
    // Automatically uses Tensor Core instructions
}
```

### 8.5 OpenCL Backend
```aria
// Compile same code to OpenCL for AMD GPUs, Intel GPUs, FPGAs
ariac physics.aria --target=opencl -o physics.cl
```

---

## 9. Timeline & Milestones

| Phase | Timeline | Deliverable |
|-------|----------|-------------|
| **1. NVPTX Init** | 2-3 hours | NVPTX target registered |
| **2. PTX Emission** | 4-6 hours | Simple function → PTX |
| **3. Kernel Attributes** | 4-6 hours | #[gpu_kernel] support |
| **4. GPU Intrinsics** | 6-8 hours | Thread ID, sync, atomics |
| **5. fix256 on GPU** | 8-12 hours | Deterministic arithmetic |
| **6. Memory Transfer** | 6-8 hours | Kernel launch runtime |
| **7. Error Handling** | 4-6 hours | Result<T> on GPU |
| **TEST & BENCHMARK** | 4-6 hours | Nikola physics kernel |
| **TOTAL** | **38-55 hours** | Production-ready GPU backend |

**Target Completion:** 5-7 days (1 week sprint)

---

## 10. Dependencies & Prerequisites

### LLVM Requirements
- LLVM 18+ with NVPTX backend enabled
- Check: `llvm-config --targets-built` should include `NVPTX`

### CUDA Requirements
- CUDA Toolkit 12.0+ (for runtime API)
- NVIDIA Driver 525.60.13+ (supports CUDA 12.0)
- Test GPU with compute capability ≥5.0 (Maxwell or newer)

### Build System
```bash
# CMakeLists.txt additions
find_package(CUDAToolkit REQUIRED)
target_link_libraries(ariac PRIVATE CUDA::cudart CUDA::cuda_driver)
```

---

## 11. Risk Assessment

| Risk | Impact | Mitigation |
|------|--------|------------|
| LLVM NVPTX bugs | High | Test thoroughly, file LLVM bugs, workaround codegen |
| fix256 performance | Medium | Profile, optimize, consider half-precision (fix128) |
| GPU hardware unavailable | High | Use CUDA emulator (cuobjdump), cloud GPU instances |
| Memory transfer overhead | Medium | Use unified memory, async transfers |
| Error handling complexity | Low | Start simple (error codes), iterate based on real usage |

---

## 12. References

1. **LLVM NVPTX Backend:** https://llvm.org/docs/NVPTXUsage.html
2. **PTX ISA Reference:** https://docs.nvidia.com/cuda/parallel-thread-execution/
3. **CUDA C Programming Guide:** https://docs.nvidia.com/cuda/cuda-c-programming-guide/
4. **Nikola AGI Architecture:** /home/randy/Workspace/REPOS/nikola/docs/ARCHITECTURE.md
5. **Aria Specs (Layered Safety):** /home/randy/Workspace/REPOS/aria/.internal/aria_specs.txt

---

*"Deterministic physics at <1ms timestep. Three layers between consciousness and chaos."*
