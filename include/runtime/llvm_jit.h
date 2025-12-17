/**
 * Aria Runtime - LLVM ORC JIT Integration
 * 
 * Provides high-optimization JIT compilation using LLVM's ORC (On-Request Compilation) engine.
 * This complements the lightweight Runtime Assembler (ARA) for scenarios requiring advanced
 * optimizations like loop unrolling, vectorization, and inlining.
 * 
 * Use Cases:
 * - Heavy optimization: Complex functions benefiting from LLVM optimization passes
 * - Cross-architecture: LLVM handles target-specific codegen automatically
 * - Advanced features: SIMD, loop optimization, function inlining
 * 
 * Comparison with ARA:
 * - ARA: Lightweight, fast compilation, direct x86-64 emission, ~1-10µs
 * - LLVM JIT: Full optimization, architecture-independent, ~100-1000µs
 * 
 * Architecture:
 * - ExecutionSession: Manages JIT execution state
 * - JITDylib: Symbol lookup and resolution
 * - ObjectLinkingLayer: Links compiled objects
 * - IRCompileLayer: Compiles LLVM IR to machine code
 * - DataLayout: Target-specific data layout
 */

#pragma once

#include <memory>
#include <string>
#include <cstdint>

// Forward declarations for LLVM types (avoid header pollution)
namespace llvm {
    class LLVMContext;
    class Module;
    class Function;
    namespace orc {
        class ExecutionSession;
        class RTDyldObjectLinkingLayer;
        class IRCompileLayer;
        class JITDylib;
        class ThreadSafeContext;
    }
}

/**
 * LLVM ORC JIT Compiler
 * 
 * Manages LLVM JIT compilation sessions with full optimization pipeline.
 * Thread-safe design allows concurrent compilation of independent modules.
 */
struct AriaLLVMJIT {
    // Opaque LLVM state (hidden from public API)
    void* execution_session;     // ExecutionSession*
    void* object_layer;          // RTDyldObjectLinkingLayer*
    void* compile_layer;         // IRCompileLayer*
    void* main_jit_dylib;        // JITDylib*
    void* context;               // ThreadSafeContext*
    void* data_layout;           // DataLayout*
    
    // JIT configuration
    int optimization_level;      // 0-3 (O0, O1, O2, O3)
    bool enable_inlining;
    bool enable_vectorization;
};

/**
 * Compiled function handle
 * 
 * Represents a JIT-compiled function with typed function pointer.
 * Lifetime managed by AriaLLVMJIT; invalidated when JIT is destroyed.
 */
struct AriaJITFunction {
    void* function_ptr;          // Typed function pointer
    const char* name;            // Function name (for debugging)
    uint64_t address;            // Function address (for introspection)
};

// ============================================================================
// LLVM JIT Lifecycle
// ============================================================================

/**
 * Create LLVM JIT compiler with specified optimization level
 * 
 * @param opt_level Optimization level (0=O0, 1=O1, 2=O2, 3=O3)
 * @return JIT compiler instance (must be destroyed with aria_llvm_jit_destroy)
 */
AriaLLVMJIT* aria_llvm_jit_create(int opt_level);

/**
 * Destroy LLVM JIT compiler and free all resources
 * 
 * Invalidates all AriaJITFunction handles obtained from this JIT instance.
 * 
 * @param jit JIT compiler to destroy
 */
void aria_llvm_jit_destroy(AriaLLVMJIT* jit);

// ============================================================================
// LLVM IR Compilation
// ============================================================================

/**
 * Compile LLVM IR module and add to JIT
 * 
 * Takes ownership of the LLVM Module (caller must not modify after this call).
 * Module is compiled with the optimization level specified at JIT creation.
 * 
 * @param jit JIT compiler
 * @param module LLVM Module to compile (ownership transferred)
 * @return 0 on success, -1 on error
 */
int aria_llvm_jit_add_module(AriaLLVMJIT* jit, llvm::Module* module);

/**
 * Compile LLVM IR from string representation
 * 
 * Parses LLVM IR text, creates module, and compiles to machine code.
 * Convenience wrapper around aria_llvm_jit_add_module for textual IR.
 * 
 * @param jit JIT compiler
 * @param ir_text LLVM IR as null-terminated string
 * @param module_name Name for the compiled module
 * @return 0 on success, -1 on error
 */
int aria_llvm_jit_compile_ir(AriaLLVMJIT* jit, const char* ir_text, const char* module_name);

// ============================================================================
// Function Lookup and Execution
// ============================================================================

/**
 * Lookup compiled function by name
 * 
 * Returns function handle for execution. Handle is valid until JIT is destroyed.
 * Function must have been compiled via aria_llvm_jit_add_module or aria_llvm_jit_compile_ir.
 * 
 * @param jit JIT compiler
 * @param function_name Name of function to lookup
 * @return Function handle, or NULL if not found
 */
AriaJITFunction* aria_llvm_jit_lookup(AriaLLVMJIT* jit, const char* function_name);

/**
 * Execute JIT-compiled function with no arguments
 * 
 * Casts function pointer to void(*)() and invokes.
 * 
 * @param func Function handle from aria_llvm_jit_lookup
 */
void aria_jit_execute(AriaJITFunction* func);

/**
 * Execute JIT-compiled function with one int64_t argument
 * 
 * Casts function pointer to int64_t(*)(int64_t) and invokes.
 * 
 * @param func Function handle
 * @param arg Argument value
 * @return Function return value
 */
int64_t aria_jit_execute_i64(AriaJITFunction* func, int64_t arg);

/**
 * Execute JIT-compiled function with two int64_t arguments
 * 
 * Casts function pointer to int64_t(*)(int64_t, int64_t) and invokes.
 * 
 * @param func Function handle
 * @param arg1 First argument
 * @param arg2 Second argument
 * @return Function return value
 */
int64_t aria_jit_execute_i64_i64(AriaJITFunction* func, int64_t arg1, int64_t arg2);

// ============================================================================
// JIT Configuration
// ============================================================================

/**
 * Set optimization level for subsequent compilations
 * 
 * Does not affect already-compiled modules.
 * 
 * @param jit JIT compiler
 * @param opt_level Optimization level (0-3)
 */
void aria_llvm_jit_set_opt_level(AriaLLVMJIT* jit, int opt_level);

/**
 * Enable/disable function inlining
 * 
 * @param jit JIT compiler
 * @param enable true to enable inlining, false to disable
 */
void aria_llvm_jit_set_inlining(AriaLLVMJIT* jit, bool enable);

/**
 * Enable/disable auto-vectorization
 * 
 * @param jit JIT compiler
 * @param enable true to enable vectorization, false to disable
 */
void aria_llvm_jit_set_vectorization(AriaLLVMJIT* jit, bool enable);

// ============================================================================
// Utilities
// ============================================================================

/**
 * Get LLVM target triple for current architecture
 * 
 * @return Target triple string (e.g., "x86_64-pc-linux-gnu")
 */
const char* aria_llvm_get_target_triple();

/**
 * Get LLVM data layout string for current architecture
 * 
 * @return Data layout string
 */
const char* aria_llvm_get_data_layout();

/**
 * Dump compiled module IR to stderr (for debugging)
 * 
 * @param jit JIT compiler
 * @param module_name Name of module to dump
 */
void aria_llvm_jit_dump_module(AriaLLVMJIT* jit, const char* module_name);
