/**
 * Aria Runtime - LLVM ORC JIT Implementation
 * 
 * Implementation of LLVM ORC JIT integration for high-optimization code generation.
 * Uses LLVM 20.1.2 ORC APIs for on-demand compilation with full optimization pipeline.
 */

#include "runtime/llvm_jit.h"

// LLVM Headers
#include <llvm/ExecutionEngine/Orc/LLJIT.h>
#include <llvm/ExecutionEngine/Orc/ThreadSafeModule.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Support/SourceMgr.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/Error.h>
#include <llvm/Transforms/InstCombine/InstCombine.h>
#include <llvm/Transforms/Scalar.h>
#include <llvm/Transforms/Scalar/GVN.h>
#include <llvm/Transforms/Utils.h>
#include <llvm/Passes/PassBuilder.h>

#include <cstdlib>
#include <cstring>
#include <memory>

using namespace llvm;
using namespace llvm::orc;

// ============================================================================
// Internal Structures
// ============================================================================

/**
 * LLVM JIT State (C++ implementation details)
 * 
 * Wraps LLVM LLJIT (Lazy Linking JIT) for simplified JIT management.
 * LLJIT provides automatic symbol resolution and lazy compilation.
 */
struct AriaLLVMJITImpl {
    std::unique_ptr<LLJIT> jit;
    std::unique_ptr<LLVMContext> context;
    int opt_level;
    bool enable_inlining;
    bool enable_vectorization;
    
    AriaLLVMJITImpl() : opt_level(2), enable_inlining(true), enable_vectorization(true) {}
};

/**
 * JIT Function Handle
 */
struct AriaJITFunctionImpl {
    void* function_ptr;
    std::string name;
    uint64_t address;
};

// ============================================================================
// LLVM Initialization
// ============================================================================

/**
 * Initialize LLVM native target (call once at startup)
 */
static void initialize_llvm_once() {
    static bool initialized = false;
    if (!initialized) {
        InitializeNativeTarget();
        InitializeNativeTargetAsmPrinter();
        InitializeNativeTargetAsmParser();
        initialized = true;
    }
}

// ============================================================================
// LLVM JIT Lifecycle
// ============================================================================

AriaLLVMJIT* aria_llvm_jit_create(int opt_level) {
    initialize_llvm_once();
    
    // Create LLJIT instance
    auto jit_or_err = LLJITBuilder().create();
    if (!jit_or_err) {
        // Error handling: LLJIT creation failed
        auto err = jit_or_err.takeError();
        logAllUnhandledErrors(std::move(err), errs(), "LLJIT creation failed: ");
        return nullptr;
    }
    
    // Create wrapper
    AriaLLVMJIT* wrapper = (AriaLLVMJIT*)malloc(sizeof(AriaLLVMJIT));
    if (!wrapper) return nullptr;
    
    AriaLLVMJITImpl* impl = new AriaLLVMJITImpl();
    impl->jit = std::move(*jit_or_err);
    impl->context = std::make_unique<LLVMContext>();
    impl->opt_level = (opt_level >= 0 && opt_level <= 3) ? opt_level : 2;
    
    // Store implementation pointer
    wrapper->execution_session = impl;
    wrapper->object_layer = nullptr;     // LLJIT manages internally
    wrapper->compile_layer = nullptr;    // LLJIT manages internally
    wrapper->main_jit_dylib = nullptr;   // LLJIT manages internally
    wrapper->context = impl->context.get();
    wrapper->data_layout = nullptr;
    wrapper->optimization_level = impl->opt_level;
    wrapper->enable_inlining = impl->enable_inlining;
    wrapper->enable_vectorization = impl->enable_vectorization;
    
    return wrapper;
}

void aria_llvm_jit_destroy(AriaLLVMJIT* jit) {
    if (!jit) return;
    
    AriaLLVMJITImpl* impl = (AriaLLVMJITImpl*)jit->execution_session;
    delete impl;
    free(jit);
}

// ============================================================================
// LLVM IR Compilation
// ============================================================================

/**
 * Apply optimization passes to module
 */
static void optimize_module(Module* module, int opt_level, bool inlining, bool vectorization) {
    // Create pass managers
    LoopAnalysisManager LAM;
    FunctionAnalysisManager FAM;
    CGSCCAnalysisManager CGAM;
    ModuleAnalysisManager MAM;
    
    PassBuilder PB;
    
    // Register analyses
    PB.registerModuleAnalyses(MAM);
    PB.registerCGSCCAnalyses(CGAM);
    PB.registerFunctionAnalyses(FAM);
    PB.registerLoopAnalyses(LAM);
    PB.crossRegisterProxies(LAM, FAM, CGAM, MAM);
    
    // Build optimization pipeline
    ModulePassManager MPM;
    
    if (opt_level == 0) {
        // O0: No optimizations
        MPM = PB.buildO0DefaultPipeline(OptimizationLevel::O0);
    } else if (opt_level == 1) {
        // O1: Basic optimizations
        MPM = PB.buildPerModuleDefaultPipeline(OptimizationLevel::O1);
    } else if (opt_level == 2) {
        // O2: Moderate optimizations (default)
        MPM = PB.buildPerModuleDefaultPipeline(OptimizationLevel::O2);
    } else {
        // O3: Aggressive optimizations
        MPM = PB.buildPerModuleDefaultPipeline(OptimizationLevel::O3);
    }
    
    // Run optimizations
    MPM.run(*module, MAM);
}

int aria_llvm_jit_add_module(AriaLLVMJIT* jit, Module* module) {
    if (!jit || !module) return -1;
    
    AriaLLVMJITImpl* impl = (AriaLLVMJITImpl*)jit->execution_session;
    
    // Optimize module before adding
    optimize_module(module, impl->opt_level, impl->enable_inlining, impl->enable_vectorization);
    
    // Wrap in ThreadSafeModule
    // NOTE: This creates a new context for the module. If the module was created with
    // a different context, there will be a context mismatch during cleanup.
    // Prefer using aria_llvm_jit_compile_ir which handles context management correctly.
    auto TSM = ThreadSafeModule(std::unique_ptr<Module>(module), 
                                  ThreadSafeContext(std::make_unique<LLVMContext>()));
    
    // Add module to JIT
    auto err = impl->jit->addIRModule(std::move(TSM));
    if (err) {
        logAllUnhandledErrors(std::move(err), errs(), "Failed to add module: ");
        return -1;
    }
    
    return 0;
}

int aria_llvm_jit_compile_ir(AriaLLVMJIT* jit, const char* ir_text, const char* module_name) {
    if (!jit || !ir_text || !module_name) return -1;
    
    AriaLLVMJITImpl* impl = (AriaLLVMJITImpl*)jit->execution_session;
    
    // Create a new context for this module
    // Each module needs its own context for proper lifetime management with ThreadSafeModule
    auto context = std::make_unique<LLVMContext>();
    
    // Parse LLVM IR
    SMDiagnostic err;
    std::unique_ptr<MemoryBuffer> buffer = MemoryBuffer::getMemBuffer(ir_text);
    std::unique_ptr<Module> module = parseIR(*buffer, err, *context);
    
    if (!module) {
        // Parse error
        err.print("aria_llvm_jit_compile_ir", errs());
        return -1;
    }
    
    module->setModuleIdentifier(module_name);
    
    // Optimize module
    optimize_module(module.get(), impl->opt_level, impl->enable_inlining, impl->enable_vectorization);
    
    // Wrap in ThreadSafeModule with its context
    auto TSM = ThreadSafeModule(std::move(module), ThreadSafeContext(std::move(context)));
    
    // Add module to JIT
    auto add_err = impl->jit->addIRModule(std::move(TSM));
    if (add_err) {
        logAllUnhandledErrors(std::move(add_err), errs(), "Failed to add module: ");
        return -1;
    }
    
    return 0;
}

// ============================================================================
// Function Lookup and Execution
// ============================================================================

AriaJITFunction* aria_llvm_jit_lookup(AriaLLVMJIT* jit, const char* function_name) {
    if (!jit || !function_name) return nullptr;
    
    AriaLLVMJITImpl* impl = (AriaLLVMJITImpl*)jit->execution_session;
    
    // Lookup function symbol
    auto sym_or_err = impl->jit->lookup(function_name);
    if (!sym_or_err) {
        auto err = sym_or_err.takeError();
        logAllUnhandledErrors(std::move(err), errs(), "Function lookup failed: ");
        return nullptr;
    }
    
    // Extract function pointer
    auto func_addr = sym_or_err->getValue();
    void* func_ptr = (void*)func_addr;
    
    // Create function handle
    AriaJITFunction* func = (AriaJITFunction*)malloc(sizeof(AriaJITFunction));
    if (!func) return nullptr;
    
    func->function_ptr = func_ptr;
    func->name = function_name;
    func->address = func_addr;
    
    return func;
}

void aria_jit_execute(AriaJITFunction* func) {
    if (!func || !func->function_ptr) return;
    
    typedef void (*FuncType)();
    FuncType fn = (FuncType)func->function_ptr;
    fn();
}

int64_t aria_jit_execute_i64(AriaJITFunction* func, int64_t arg) {
    if (!func || !func->function_ptr) return 0;
    
    typedef int64_t (*FuncType)(int64_t);
    FuncType fn = (FuncType)func->function_ptr;
    return fn(arg);
}

int64_t aria_jit_execute_i64_i64(AriaJITFunction* func, int64_t arg1, int64_t arg2) {
    if (!func || !func->function_ptr) return 0;
    
    typedef int64_t (*FuncType)(int64_t, int64_t);
    FuncType fn = (FuncType)func->function_ptr;
    return fn(arg1, arg2);
}

// ============================================================================
// JIT Configuration
// ============================================================================

void aria_llvm_jit_set_opt_level(AriaLLVMJIT* jit, int opt_level) {
    if (!jit) return;
    
    AriaLLVMJITImpl* impl = (AriaLLVMJITImpl*)jit->execution_session;
    impl->opt_level = (opt_level >= 0 && opt_level <= 3) ? opt_level : 2;
    jit->optimization_level = impl->opt_level;
}

void aria_llvm_jit_set_inlining(AriaLLVMJIT* jit, bool enable) {
    if (!jit) return;
    
    AriaLLVMJITImpl* impl = (AriaLLVMJITImpl*)jit->execution_session;
    impl->enable_inlining = enable;
    jit->enable_inlining = enable;
}

void aria_llvm_jit_set_vectorization(AriaLLVMJIT* jit, bool enable) {
    if (!jit) return;
    
    AriaLLVMJITImpl* impl = (AriaLLVMJITImpl*)jit->execution_session;
    impl->enable_vectorization = enable;
    jit->enable_vectorization = enable;
}

// ============================================================================
// Utilities
// ============================================================================

const char* aria_llvm_get_target_triple() {
    initialize_llvm_once();
    
    // Use JITTargetMachineBuilder to detect host target
    auto JTMB = orc::JITTargetMachineBuilder::detectHost();
    if (!JTMB) {
        // Fallback to empty string on error
        static std::string fallback = "";
        return fallback.c_str();
    }
    
    static std::string triple = JTMB->getTargetTriple().str();
    return triple.c_str();
}

const char* aria_llvm_get_data_layout() {
    initialize_llvm_once();
    
    // Use JITTargetMachineBuilder to get native data layout
    auto JTMB = orc::JITTargetMachineBuilder::detectHost();
    if (!JTMB) {
        // Fallback to x86-64 layout on error
        return "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128";
    }
    
    static std::string layout = JTMB->getDefaultDataLayoutForTarget()->getStringRepresentation();
    return layout.c_str();
}

void aria_llvm_jit_dump_module(AriaLLVMJIT* jit, const char* module_name) {
    if (!jit || !module_name) return;
    
    errs() << "Module dump not implemented (LLJIT doesn't expose module access)\n";
}
