# GPU/PTX Backend - Phase 4 Complete: GPU Intrinsics

**Status:** ✅ COMPLETE  
**Date:** February 12, 2026  
**Time Investment:** ~4 hours (Phases 1-4 total: ~10 hours)

---

## Summary

Successfully implemented complete GPU intrinsic system for CUDA/PTX backend. All 13 GPU intrinsics now map correctly to LLVM NVVM intrinsics and generate valid PTX code.

---

## Implementation Details

### 1. GPU Intrinsic API Design

**Namespace:** `gpu`  
**Pattern:** `gpu.intrinsic_name()` (zero arguments for all)

**Thread Indexing (3 intrinsics):**
```aria
int32:tid_x = gpu.thread_id();      // X dimension (most common)
int32:tid_y = gpu.thread_id_y();    // Y dimension
int32:tid_z = gpu.thread_id_z();    // Z dimension
```

**Block Indexing (3 intrinsics):**
```aria
int32:bid_x = gpu.block_id();       // X dimension
int32:bid_y = gpu.block_id_y();     // Y dimension
int32:bid_z = gpu.block_id_z();     // Z dimension
```

**Block Dimensions (3 intrinsics):**
```aria
int32:bdim_x = gpu.block_dim();     // Threads per block (X)
int32:bdim_y = gpu.block_dim_y();   // Threads per block (Y)
int32:bdim_z = gpu.block_dim_z();   // Threads per block (Z)
```

**Grid Dimensions (3 intrinsics):**
```aria
int32:gdim_x = gpu.grid_dim();      // Blocks in grid (X)
int32:gdim_y = gpu.grid_dim_y();    // Blocks in grid (Y)
int32:gdim_z = gpu.grid_dim_z();    // Blocks in grid (Z)
```

**Synchronization (1 intrinsic):**
```aria
gpu.sync_threads();                 // Barrier - wait for all threads in block
```

### 2. LLVM NVVM Intrinsic Mapping

Each Aria intrinsic maps to its corresponding LLVM NVVM intrinsic:

**Thread Indexing:**
- `gpu.thread_id()` → `llvm.nvvm.read.ptx.sreg.tid.x()`
- `gpu.thread_id_y()` → `llvm.nvvm.read.ptx.sreg.tid.y()`
- `gpu.thread_id_z()` → `llvm.nvvm.read.ptx.sreg.tid.z()`

**Block Indexing:**
- `gpu.block_id()` → `llvm.nvvm.read.ptx.sreg.ctaid.x()`
- `gpu.block_id_y()` → `llvm.nvvm.read.ptx.sreg.ctaid.y()`
- `gpu.block_id_z()` → `llvm.nvvm.read.ptx.sreg.ctaid.z()`

**Block Dimensions:**
- `gpu.block_dim()` → `llvm.nvvm.read.ptx.sreg.ntid.x()`
- `gpu.block_dim_y()` → `llvm.nvvm.read.ptx.sreg.ntid.y()`
- `gpu.block_dim_z()` → `llvm.nvvm.read.ptx.sreg.ntid.z()`

**Grid Dimensions:**
- `gpu.grid_dim()` → `llvm.nvvm.read.ptx.sreg.nctaid.x()`
- `gpu.grid_dim_y()` → `llvm.nvvm.read.ptx.sreg.nctaid.y()`
- `gpu.grid_dim_z()` → `llvm.nvvm.read.ptx.sreg.nctaid.z()`

**Synchronization:**
- `gpu.sync_threads()` → `llvm.nvvm.barrier0()`

### 3. PTX Code Generation

**Example Source (test_gpu_vector_add.aria):**
```aria
func:gpu_vector_add = void(int32:n) {
    int32:tid = gpu.thread_id();
    int32:bid = gpu.block_id();
    int32:bdim = gpu.block_dim();
    int32:idx = bid * bdim + tid;
    gpu.sync_threads();
};
```

**Generated LLVM IR:**
```llvm
%tid.x = call i32 @llvm.nvvm.read.ptx.sreg.tid.x()
%bid.x = call i32 @llvm.nvvm.read.ptx.sreg.ctaid.x()
%bdim.x = call i32 @llvm.nvvm.read.ptx.sreg.ntid.x()
call void @llvm.nvvm.barrier0()

declare noundef i32 @llvm.nvvm.read.ptx.sreg.tid.x() #0
declare noundef i32 @llvm.nvvm.read.ptx.sreg.ctaid.x() #0
declare noundef i32 @llvm.nvvm.read.ptx.sreg.ntid.x() #0
declare void @llvm.nvvm.barrier0() #1
```

**Generated PTX (with -O0):**
```ptx
.visible .entry gpu_vector_add(
    .param .u32 gpu_vector_add_param_0
)
{
    mov.u32 %r1, %tid.x;        // Thread ID (X)
    mov.u32 %r2, %ctaid.x;      // Block ID (X)
    mov.u32 %r3, %ntid.x;       // Block dimension (X)
    bar.sync 0;                  // Barrier synchronization
    ret;
}
```

---

## Files Modified

### Backend (IR Code Generation)

**include/backend/ir/codegen_expr.h**
- Added `codegenGPUIntrinsic()` method declaration
- Lines added: ~10

**src/backend/ir/codegen_expr.cpp**
- Added GPU intrinsic interception in `codegenCall()` (line ~2910)
- Implemented `codegenGPUIntrinsic()` function (170 lines, line ~8950)
- Manually declares NVVM intrinsics using `Function::Create()` pattern
- Uses lambda helper `getOrDeclareNVVMIntrinsic()` for clean declaration
- Total lines added: ~180

### Frontend (Semantic Analysis)

**src/frontend/sema/type_checker.cpp**
- Added `gpu` namespace special case in `inferIdentifier()` (returns UnknownType placeholder)
- Added GPU intrinsic type checking in `inferMemberAccessExpr()` (returns int32)
- Added GPU call handling in `inferCallExpr()` (returns int32)
- Total lines added: ~30

---

## Testing

### Test Files Created

**test_gpu_intrinsics.aria** - Comprehensive test of all 13 intrinsics
**test_gpu_vector_add.aria** - Realistic vector addition kernel pattern

### Verification

✅ **Compilation:** All test files compile without errors  
✅ **Semantic Analysis:** `gpu` namespace recognized, intrinsics type-checked  
✅ **IR Generation:** All NVVM intrinsics generated correctly  
✅ **PTX Emission:** Valid PTX code with proper barrier and thread ID access

**LLVM IR Verification:**
```bash
grep "llvm.nvvm" test_vector_add.ll
# Output:
#   %tid.x = call i32 @llvm.nvvm.read.ptx.sreg.tid.x()
#   %bid.x = call i32 @llvm.nvvm.read.ptx.sreg.ctaid.x()
#   %bdim.x = call i32 @llvm.nvvm.read.ptx.sreg.ntid.x()
#   call void @llvm.nvvm.barrier0()
```

---

## Technical Challenges & Solutions

### Challenge 1: NVVM Intrinsics Not in llvm::Intrinsic Enum

**Problem:** Standard `llvm::Intrinsic::getDeclaration()` doesn't work for NVVM intrinsics  
**Root Cause:** NVVM intrinsics are target-specific, not in core LLVM intrinsic enum  
**Solution:** Manually declare intrinsics using `llvm::Function::Create()` pattern from `ternary_codegen.cpp`

**Implementation:**
```cpp
auto getOrDeclareNVVMIntrinsic = [this](const std::string& name, 
                                        llvm::Type* returnType) -> llvm::Function* {
    if (llvm::Function* existing = module->getFunction(name)) {
        return existing;
    }
    llvm::FunctionType* funcType = llvm::FunctionType::get(returnType, {}, false);
    return llvm::Function::Create(funcType, llvm::Function::ExternalLinkage, name, module);
};
```

### Challenge 2: `gpu` Namespace Not Defined in Symbol Table

**Problem:** Parser creates `IdentifierExpr("gpu")`, semantic analysis rejects as undefined  
**Solution:** Added special case for `gpu` in `inferIdentifier()` to return placeholder type

**Why Not Use Symbol Table:** `gpu` is a compile-time intrinsic namespace, not a runtime value - doesn't need symbol table entry

### Challenge 3: Member Access vs. Function Call Type Checking

**Problem:** `gpu.thread_id()` parsed as CALL(MEMBER_ACCESS), type checker saw "call non-function type int32"  
**Solution:** Added GPU intrinsic check in BOTH `inferMemberAccessExpr()` AND `inferCallExpr()`

---

## Design Decisions

### 1. Zero-Argument Intrinsics

**Decision:** All intrinsics take no arguments  
**Rationale:**
- GPU hardware registers are read-only
- No configuration needed
- Simpler API surface
- Matches CUDA/PTX model exactly

### 2. int32 Return Type

**Decision:** All intrinsics return `int32` (including dimensions/IDs)  
**Rationale:**
- GPU thread/block IDs fit in 32 bits
- Matches CUDA convention (blockIdx.x is uint32_t)
- Avoids unnecessary 64-bit operations on GPU

### 3. `gpu` Namespace Pattern

**Decision:** Use `gpu.intrinsic()` instead of global functions  
**Rationale:**
- Clear separation from regular code
- Prevents name collisions
- Makes GPU code visually distinct
- Extensible for future GPU APIs (gpu.atomic_add(), etc.)

### 4. Manual Intrinsic Declaration

**Decision:** Use `Function::Create()` instead of `Intrinsic::getDeclaration()`  
**Rationale:**
- NVVM intrinsics not in LLVM core enum
- Explicit declaration makes generated IR clearer
- Matches pattern used in existing ternary codegen
- More portable across LLVM versions

---

## Example: Complete GPU Kernel

```aria
// Vector addition kernel demonstrating all Phase 4 features
func:gpu_vector_add = void(float32*:a, float32*:b, float32*:c, int32:n) {
    // Calculate global thread index
    int32:tid = gpu.thread_id();      // Local thread ID in block
    int32:bid = gpu.block_id();       // Block ID in grid
    int32:bdim = gpu.block_dim();     // Threads per block
    int32:gdim = gpu.grid_dim();      // Blocks in grid
    
    int32:idx = bid * bdim + tid;
    
    // Grid-stride loop for large arrays
    till (idx < n) {
        float32:val_a = a[idx];       // Phase 6: Memory ops
        float32:val_b = b[idx];
        c[idx] = val_a + val_b;
        
        idx = idx + (gdim * bdim);     // Stride by grid size
    }
    
    // Synchronize before shared memory ops (Phase 6)
    gpu.sync_threads();
};
```

---

## Performance Characteristics

**Zero Runtime Overhead:**
- All intrinsics are direct PTX special register reads
- `bar.sync 0` is single hardware instruction
- No function call overhead

**GPU Register Pressure:**
- Each intrinsic read uses 1 register
- Compiler optimizes away unused reads
- Cached in registers if used multiple times

**Compilation Time:**
- Negligible impact on compile time
- Intrinsic declarations cached in IR module
- No complex transformations needed

---

## Next Steps (Phase 5: fix256 on GPU)

Now that GPU intrinsics work, Phase 5 will implement deterministic fixed-point arithmetic:

**Goal:** Bit-exact fix256 arithmetic across all GPU architectures  
**Approach:** 4x int64 limbs (256-bit total) using software arithmetic  
**Why:** Floating-point is non-deterministic across hardware (different rounding modes, FMA units, etc.)

**Key Requirements:**
1. Addition/subtraction with carry propagation
2. Multiplication via Karatsuba algorithm
3. Division with exact quotient/remainder
4. Range checking for physics constraints
5. Identical results on RTX 3090, A100, H100, etc.

**Estimated Time:** 8-12 hours

---

## Cumulative Progress

**Completed Phases:**
- ✅ Phase 1: NVPTX Initialization (2 hrs)
- ✅ Phase 2: PTX Emission (2 hrs)
- ✅ Phase 3: Kernel Metadata (2 hrs)
- ✅ Phase 4: GPU Intrinsics (4 hrs)

**Total Time:** 10 hours  
**Remaining:** 22-34 hours (Phases 5-7)

**Overall Goal:** <1ms physics timestep on RTX 3090 (vs 50ms on CPU)  
**Status:** On track - core infrastructure complete, deterministic math next

