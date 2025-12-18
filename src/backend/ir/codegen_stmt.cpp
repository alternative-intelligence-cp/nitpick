#include "backend/ir/codegen_stmt.h"
#include "backend/ir/codegen_expr.h"
#include "frontend/ast/stmt.h"
#include "frontend/ast/expr.h"
#include "frontend/ast/ast_node.h"
#include "frontend/sema/type.h"
#include "frontend/sema/generic_resolver.h"  // Phase 4.5.1: Generic support
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Verifier.h>
#include <llvm/IR/Intrinsics.h>  // Phase 4.5.3: Coroutine intrinsics
#include <stdexcept>

using namespace aria;
using namespace aria::backend;
using namespace aria::sema;

StmtCodegen::StmtCodegen(llvm::LLVMContext& ctx, llvm::IRBuilder<>& bldr,
                         llvm::Module* mod, std::map<std::string, llvm::Value*>& values)
    : context(ctx), builder(bldr), module(mod), named_values(values), 
      expr_codegen(nullptr), monomorphizer(nullptr) {}

void StmtCodegen::setExprCodegen(ExprCodegen* expr_gen) {
    expr_codegen = expr_gen;
}

void StmtCodegen::setMonomorphizer(Monomorphizer* mono) {
    monomorphizer = mono;
}

// Helper: Get LLVM type from Aria type string
llvm::Type* StmtCodegen::getLLVMTypeFromString(const std::string& type_name) {
    // Primitive types
    if (type_name == "i8") return llvm::Type::getInt8Ty(context);
    if (type_name == "i16") return llvm::Type::getInt16Ty(context);
    if (type_name == "i32") return llvm::Type::getInt32Ty(context);
    if (type_name == "i64") return llvm::Type::getInt64Ty(context);
    if (type_name == "u8") return llvm::Type::getInt8Ty(context);
    if (type_name == "u16") return llvm::Type::getInt16Ty(context);
    if (type_name == "u32") return llvm::Type::getInt32Ty(context);
    if (type_name == "u64") return llvm::Type::getInt64Ty(context);
    if (type_name == "f32") return llvm::Type::getFloatTy(context);
    if (type_name == "f64") return llvm::Type::getDoubleTy(context);
    if (type_name == "bool") return llvm::Type::getInt1Ty(context);
    if (type_name == "void") return llvm::Type::getVoidTy(context);
    
    // Default to i32 for unknown types (will be handled by semantic analysis)
    return llvm::Type::getInt32Ty(context);
}

// Helper: Get LLVM type from Aria type
llvm::Type* StmtCodegen::getLLVMType(Type* type) {
    if (!type) {
        return llvm::Type::getVoidTy(context);
    }
    
    if (!type->isPrimitive()) {
        return llvm::Type::getInt32Ty(context);  // Default for non-primitive
    }
    
    PrimitiveType* prim_type = static_cast<PrimitiveType*>(type);
    return getLLVMTypeFromString(prim_type->getName());
}

// ============================================================================
// Phase 4.4: Memory Model Runtime Function Declarations
// ============================================================================

/**
 * Get or declare aria_gc_alloc runtime function
 * Signature: void* aria_gc_alloc(i64 size)
 */
llvm::Function* StmtCodegen::getOrDeclareGCAlloc() {
    llvm::Function* func = module->getFunction("aria_gc_alloc");
    if (!func) {
        // Declare: void* aria_gc_alloc(i64 size)
        llvm::FunctionType* func_type = llvm::FunctionType::get(
            llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0),  // void* return
            {llvm::Type::getInt64Ty(context)},                          // i64 size param
            false                                                         // not vararg
        );
        func = llvm::Function::Create(
            func_type,
            llvm::Function::ExternalLinkage,
            "aria_gc_alloc",
            module
        );
    }
    return func;
}

/**
 * Get or declare aria.alloc runtime function (wild memory)
 * Signature: void* aria_alloc(i64 size)
 */
llvm::Function* StmtCodegen::getOrDeclareWildAlloc() {
    llvm::Function* func = module->getFunction("aria_alloc");
    if (!func) {
        // Declare: void* aria_alloc(i64 size)
        llvm::FunctionType* func_type = llvm::FunctionType::get(
            llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0),  // void* return
            {llvm::Type::getInt64Ty(context)},                          // i64 size param
            false
        );
        func = llvm::Function::Create(
            func_type,
            llvm::Function::ExternalLinkage,
            "aria_alloc",
            module
        );
    }
    return func;
}

/**
 * Get or declare aria_alloc_exec runtime function (wildx executable memory)
 * Signature: void* aria_alloc_exec(i64 size)
 */
llvm::Function* StmtCodegen::getOrDeclareWildXAlloc() {
    llvm::Function* func = module->getFunction("aria_alloc_exec");
    if (!func) {
        // Declare: void* aria_alloc_exec(i64 size)
        llvm::FunctionType* func_type = llvm::FunctionType::get(
            llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0),  // void* return
            {llvm::Type::getInt64Ty(context)},                          // i64 size param
            false
        );
        func = llvm::Function::Create(
            func_type,
            llvm::Function::ExternalLinkage,
            "aria_alloc_exec",
            module
        );
    }
    return func;
}

/**
 * Get or declare aria.free runtime function
 * Signature: void aria_free(void* ptr)
 */
llvm::Function* StmtCodegen::getOrDeclareWildFree() {
    llvm::Function* func = module->getFunction("aria_free");
    if (!func) {
        // Declare: void aria_free(void* ptr)
        llvm::FunctionType* func_type = llvm::FunctionType::get(
            llvm::Type::getVoidTy(context),                              // void return
            {llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0)}, // void* ptr param
            false
        );
        func = llvm::Function::Create(
            func_type,
            llvm::Function::ExternalLinkage,
            "aria_free",
            module
        );
    }
    return func;
}

// ============================================================================
// Phase 4.5.3: LLVM Coroutine Intrinsics for Async/Await
// ============================================================================

/**
 * Get or declare @llvm.coro.id intrinsic
 * Signature: token @llvm.coro.id(i32 align, i8* promise, i8* coroaddr, i8* fnaddr)
 * Returns a token identifying the coroutine
 */
llvm::Function* StmtCodegen::getCoroId() {
    llvm::Function* func = llvm::Intrinsic::getDeclaration(
        module, 
        llvm::Intrinsic::coro_id
    );
    return func;
}

/**
 * Get or declare @llvm.coro.size.i64 intrinsic
 * Signature: i64 @llvm.coro.size.i64()
 * Returns the size needed for coroutine frame allocation
 */
llvm::Function* StmtCodegen::getCoroSize() {
    llvm::Function* func = llvm::Intrinsic::getDeclaration(
        module,
        llvm::Intrinsic::coro_size,
        {llvm::Type::getInt64Ty(context)}
    );
    return func;
}

/**
 * Get or declare @llvm.coro.begin intrinsic
 * Signature: i8* @llvm.coro.begin(token id, i8* mem)
 * Marks the beginning of coroutine and returns frame pointer
 */
llvm::Function* StmtCodegen::getCoroBegin() {
    llvm::Function* func = llvm::Intrinsic::getDeclaration(
        module,
        llvm::Intrinsic::coro_begin
    );
    return func;
}

/**
 * Get or declare @llvm.coro.save intrinsic
 * Signature: token @llvm.coro.save(i8* handle)
 * Prepares the coroutine for a suspend point
 */
llvm::Function* StmtCodegen::getCoroSave() {
    llvm::Function* func = llvm::Intrinsic::getDeclaration(
        module,
        llvm::Intrinsic::coro_save
    );
    return func;
}

/**
 * Get or declare @llvm.coro.suspend intrinsic
 * Signature: i8 @llvm.coro.suspend(token save, i1 final)
 * Suspends the coroutine. Returns 0=resume, 1=destroy, -1=initial
 */
llvm::Function* StmtCodegen::getCoroSuspend() {
    llvm::Function* func = llvm::Intrinsic::getDeclaration(
        module,
        llvm::Intrinsic::coro_suspend
    );
    return func;
}

/**
 * Get or declare @llvm.coro.end intrinsic
 * Signature: i1 @llvm.coro.end(i8* handle, i1 unwind)
 * Marks the end of coroutine. Returns true if needs cleanup
 */
llvm::Function* StmtCodegen::getCoroEnd() {
    llvm::Function* func = llvm::Intrinsic::getDeclaration(
        module,
        llvm::Intrinsic::coro_end
    );
    return func;
}

/**
 * Get or declare @llvm.coro.free intrinsic
 * Signature: i8* @llvm.coro.free(token id, i8* handle)
 * Returns pointer to memory that should be freed
 */
llvm::Function* StmtCodegen::getCoroFree() {
    llvm::Function* func = llvm::Intrinsic::getDeclaration(
        module,
        llvm::Intrinsic::coro_free
    );
    return func;
}

/**
 * Get or declare @llvm.coro.resume intrinsic
 * Signature: void @llvm.coro.resume(i8* handle)
 * Resumes a suspended coroutine
 */
llvm::Function* StmtCodegen::getCoroResume() {
    llvm::Function* func = llvm::Intrinsic::getDeclaration(
        module,
        llvm::Intrinsic::coro_resume
    );
    return func;
}

// ============================================================================
// Phase 4.3.1 + 4.4: Variable Declaration Code Generation with Memory Model
// ============================================================================

/**
 * Generate code for variable declaration
 * 
 * Supports four allocation strategies based on keywords:
 * 1. stack: Fast LIFO allocation via alloca (explicit or default for primitives)
 * 2. gc: Garbage collected heap via aria_gc_alloc (default for objects)
 * 3. wild: Manual heap via aria.alloc/aria.free (opt-out of GC)
 * 4. wildx: Executable memory via aria_alloc_exec (JIT code generation)
 * 
 * Example Aria code:
 *   i32:x = 42;                    // Default (stack for primitive)
 *   stack i32:y = 100;             // Explicit stack
 *   gc obj:data = {a:1};           // GC-managed object
 *   wild int64@:ptr = aria.alloc(8); // Manual memory
 *   wildx void@:code = aria_alloc_exec(1024); // Executable memory
 * 
 * Generated LLVM IR (stack):
 *   %x = alloca i32
 *   store i32 42, i32* %x
 * 
 * Generated LLVM IR (gc):
 *   %0 = call i8* @aria_gc_alloc(i64 16)
 *   %x = bitcast i8* %0 to i32*
 *   store i32 42, i32* %x
 * 
 * Generated LLVM IR (wild):
 *   %0 = call i8* @aria_alloc(i64 8)
 *   %ptr = bitcast i8* %0 to i64*
 * 
 * Reference: research_021 (GC), research_022 (Wild/WildX)
 */
void StmtCodegen::codegenVarDecl(VarDeclStmt* stmt) {
    // Get LLVM type from type string
    llvm::Type* var_type = getLLVMTypeFromString(stmt->typeName);
    
    // Get the current function
    llvm::Function* func = builder.GetInsertBlock()->getParent();
    
    // Determine allocation strategy
    // Priority: explicit keywords > default behavior
    // Default: stack for primitives, gc for objects
    
    llvm::Value* var_ptr = nullptr;
    
    if (stmt->isStack || (!stmt->isWild && !stmt->isGC)) {
        // Stack allocation (default or explicit)
        // Use alloca instruction - fast LIFO allocation
        llvm::IRBuilder<> tmp_builder(&func->getEntryBlock(), func->getEntryBlock().begin());
        llvm::AllocaInst* alloca = tmp_builder.CreateAlloca(var_type, nullptr, stmt->varName);
        var_ptr = alloca;
        
    } else if (stmt->isGC) {
        // GC heap allocation (explicit gc keyword)
        // Call aria_gc_alloc runtime function
        llvm::Function* gc_alloc = getOrDeclareGCAlloc();
        
        // Calculate size: Get type size in bytes
        const llvm::DataLayout& data_layout = module->getDataLayout();
        uint64_t type_size = data_layout.getTypeAllocSize(var_type);
        llvm::Value* size = llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), type_size);
        
        // Call aria_gc_alloc(size) -> returns void* (i8*)
        llvm::Value* raw_ptr = builder.CreateCall(gc_alloc, {size}, "gc_alloc");
        
        // Bitcast void* to appropriate pointer type
        var_ptr = builder.CreateBitCast(
            raw_ptr,
            llvm::PointerType::get(var_type, 0),
            stmt->varName
        );
        
    } else if (stmt->isWild) {
        // Wild heap allocation (manual memory management)
        // Call aria.alloc runtime function
        llvm::Function* wild_alloc = getOrDeclareWildAlloc();
        
        // Calculate size
        const llvm::DataLayout& data_layout = module->getDataLayout();
        uint64_t type_size = data_layout.getTypeAllocSize(var_type);
        llvm::Value* size = llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), type_size);
        
        // Call aria_alloc(size) -> returns void* (i8*)
        llvm::Value* raw_ptr = builder.CreateCall(wild_alloc, {size}, "wild_alloc");
        
        // Bitcast void* to appropriate pointer type
        var_ptr = builder.CreateBitCast(
            raw_ptr,
            llvm::PointerType::get(var_type, 0),
            stmt->varName
        );
    }
    // Note: wildx allocation would be handled via explicit aria_alloc_exec() calls in user code,
    // not via variable declarations (it's for runtime code generation, not regular variables)
    
    if (!var_ptr) {
        throw std::runtime_error("Failed to allocate memory for variable: " + stmt->varName);
    }
    
    // Store the pointer in named_values so we can reference it later
    named_values[stmt->varName] = var_ptr;
    
    // If there's an initializer, generate code for it and store the result
    if (stmt->initializer) {
        if (!expr_codegen) {
            throw std::runtime_error("ExprCodegen not set in StmtCodegen");
        }
        
        // Generate code for initializer expression
        llvm::Value* init_value = expr_codegen->codegenExpressionNode(stmt->initializer.get(), expr_codegen);
        
        if (!init_value) {
            throw std::runtime_error("Failed to generate code for initializer expression");
        }
        
        // Store the initial value in the allocated memory
        builder.CreateStore(init_value, var_ptr);
    }
}

// ============================================================================
// Phase 4.3.2: Function Declaration Code Generation
// ============================================================================

/**
 * Generate code for function declaration
 * 
 * Creates an LLVM function with proper signature (return type and parameters),
 * sets up entry block, generates code for function body, and verifies the result.
 * 
 * Example Aria code:
 *   func:add = i32(i32:a, i32:b) {
 *       pass(a + b);
 *   };
 * 
 * Generated LLVM IR:
 *   define i32 @add(i32 %a, i32 %b) {
 *   entry:
 *     %a.addr = alloca i32
 *     %b.addr = alloca i32
 *     store i32 %a, i32* %a.addr
 *     store i32 %b, i32* %b.addr
 *     %0 = load i32, i32* %a.addr
 *     %1 = load i32, i32* %b.addr
 *     %2 = add i32 %0, %1
 *     ret i32 %2
 *   }
 */
llvm::Function* StmtCodegen::codegenFuncDecl(FuncDeclStmt* stmt) {
    // Phase 4.5.1: Skip generic function templates
    // Generic functions are not directly compiled - only their specializations are
    // The Monomorphizer will create concrete versions when needed
    if (!stmt->genericParams.empty()) {
        // This is a generic function template, don't generate code
        // Specializations will be handled by codegenAllSpecializations()
        return nullptr;
    }
    
    // Get return type
    llvm::Type* return_type = getLLVMTypeFromString(stmt->returnType);
    
    // Phase 4.5.3: Async functions return i8* (coroutine handle)
    // The actual return type is wrapped in a Future<T> at runtime
    llvm::Type* actual_return_type = return_type;
    if (stmt->isAsync) {
        actual_return_type = llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0);
    }
    
    // Build parameter types
    std::vector<llvm::Type*> param_types;
    for (const auto& param : stmt->parameters) {
        ParameterNode* param_node = static_cast<ParameterNode*>(param.get());
        llvm::Type* param_type = getLLVMTypeFromString(param_node->typeName);
        param_types.push_back(param_type);
    }
    
    // Create function type
    llvm::FunctionType* func_type = llvm::FunctionType::get(actual_return_type, param_types, false);
    
    // Create function
    llvm::Function* func = llvm::Function::Create(
        func_type,
        llvm::Function::ExternalLinkage,
        stmt->funcName,
        module
    );
    
    // Set parameter names and create allocas for them
    unsigned idx = 0;
    for (auto& arg : func->args()) {
        ParameterNode* param_node = static_cast<ParameterNode*>(stmt->parameters[idx].get());
        arg.setName(param_node->paramName);
        idx++;
    }
    
    // Create entry block
    llvm::BasicBlock* entry = llvm::BasicBlock::Create(context, "entry", func);
    builder.SetInsertPoint(entry);
    
    // Phase 4.5.3: Async function coroutine setup
    llvm::Value* coro_id = nullptr;
    llvm::Value* coro_handle = nullptr;
    llvm::BasicBlock* coro_suspend_block = nullptr;
    llvm::BasicBlock* coro_cleanup_block = nullptr;
    
    if (stmt->isAsync) {
        // Generate coroutine intrinsics for async function
        // Based on LLVM coroutine transformation algorithm
        
        // 1. Generate coroutine ID
        // token @llvm.coro.id(i32 align, i8* promise, i8* coroaddr, i8* fnaddr)
        llvm::Value* align = llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 8);
        llvm::Value* null_ptr = llvm::ConstantPointerNull::get(
            llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0)
        );
        coro_id = builder.CreateCall(
            getCoroId(),
            {align, null_ptr, null_ptr, null_ptr},
            "coro.id"
        );
        
        // 2. Get coroutine frame size
        // i64 @llvm.coro.size.i64()
        llvm::Value* coro_size = builder.CreateCall(getCoroSize(), {}, "coro.size");
        
        // 3. Allocate coroutine frame (heap allocation for now, RAMP optimization later)
        // For now, use simple malloc (TODO: integrate with Aria memory model)
        llvm::Function* malloc_func = module->getFunction("malloc");
        if (!malloc_func) {
            llvm::FunctionType* malloc_type = llvm::FunctionType::get(
                llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0),
                {llvm::Type::getInt64Ty(context)},
                false
            );
            malloc_func = llvm::Function::Create(
                malloc_type,
                llvm::Function::ExternalLinkage,
                "malloc",
                module
            );
        }
        llvm::Value* coro_mem = builder.CreateCall(malloc_func, {coro_size}, "coro.alloc");
        
        // 4. Begin coroutine
        // i8* @llvm.coro.begin(token id, i8* mem)
        coro_handle = builder.CreateCall(getCoroBegin(), {coro_id, coro_mem}, "coro.handle");
        
        // Create suspend and cleanup blocks for later use
        coro_suspend_block = llvm::BasicBlock::Create(context, "coro.suspend", func);
        coro_cleanup_block = llvm::BasicBlock::Create(context, "coro.cleanup", func);
    }
    
    // Save the old named_values (for nested functions/closures in the future)
    std::map<std::string, llvm::Value*> old_named_values = named_values;
    named_values.clear();
    
    // Create allocas for parameters and store their values
    // This allows parameters to be mutable (can be reassigned in function body)
    idx = 0;
    for (auto& arg : func->args()) {
        ParameterNode* param_node = static_cast<ParameterNode*>(stmt->parameters[idx].get());
        
        // Create alloca for this parameter
        llvm::AllocaInst* alloca = builder.CreateAlloca(arg.getType(), nullptr, param_node->paramName);
        
        // Store the parameter value
        builder.CreateStore(&arg, alloca);
        
        // Remember this allocation
        named_values[param_node->paramName] = alloca;
        
        idx++;
    }
    
    // Generate code for function body
    if (stmt->body) {
        BlockStmt* body_block = static_cast<BlockStmt*>(stmt->body.get());
        codegenBlock(body_block);
    }
    
    // If the last instruction isn't a terminator, add a default return
    llvm::BasicBlock* current_block = builder.GetInsertBlock();
    if (current_block && !current_block->getTerminator()) {
        if (stmt->isAsync) {
            // Async function: Jump to final suspend instead of returning directly
            builder.CreateBr(coro_suspend_block);
        } else if (return_type->isVoidTy()) {
            builder.CreateRetVoid();
        } else {
            // Return zero/null for non-void functions without explicit return
            if (return_type->isIntegerTy()) {
                builder.CreateRet(llvm::ConstantInt::get(return_type, 0));
            } else if (return_type->isFloatingPointTy()) {
                builder.CreateRet(llvm::ConstantFP::get(return_type, 0.0));
            } else {
                // For pointer or other types, return null
                builder.CreateRet(llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(return_type)));
            }
        }
    }
    
    // Phase 4.5.3: Generate coroutine suspend and cleanup blocks for async functions
    if (stmt->isAsync) {
        // Final suspend point (where all returns converge)
        builder.SetInsertPoint(coro_suspend_block);
        
        // Save coroutine state before final suspend
        llvm::Value* save_token = builder.CreateCall(getCoroSave(), {coro_handle}, "coro.save");
        
        // Final suspend: i8 @llvm.coro.suspend(token save, i1 final)
        llvm::Value* is_final = llvm::ConstantInt::get(llvm::Type::getInt1Ty(context), 1);
        llvm::Value* suspend_result = builder.CreateCall(
            getCoroSuspend(),
            {save_token, is_final},
            "coro.suspend.result"
        );
        
        // Switch on suspend result
        // 0 = resume (shouldn't happen for final suspend)
        // 1 = destroy (cleanup path)
        llvm::SwitchInst* suspend_switch = builder.CreateSwitch(suspend_result, coro_cleanup_block, 1);
        
        // Cleanup block: free coroutine frame
        builder.SetInsertPoint(coro_cleanup_block);
        
        // Get memory to free: i8* @llvm.coro.free(token id, i8* handle)
        llvm::Value* free_mem = builder.CreateCall(getCoroFree(), {coro_id, coro_handle}, "coro.free.mem");
        
        // Free the memory (TODO: integrate with Aria memory model)
        llvm::Function* free_func = module->getFunction("free");
        if (!free_func) {
            llvm::FunctionType* free_type = llvm::FunctionType::get(
                llvm::Type::getVoidTy(context),
                {llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0)},
                false
            );
            free_func = llvm::Function::Create(
                free_type,
                llvm::Function::ExternalLinkage,
                "free",
                module
            );
        }
        builder.CreateCall(free_func, {free_mem});
        
        // End coroutine: i1 @llvm.coro.end(i8* handle, i1 unwind, token)
        llvm::Value* unwind = llvm::ConstantInt::get(llvm::Type::getInt1Ty(context), 0);
        builder.CreateCall(getCoroEnd(), {coro_handle, unwind, llvm::ConstantTokenNone::get(context)});
        
        // Return coroutine handle
        builder.CreateRet(coro_handle);
    }
    
    // Restore old named_values
    named_values = old_named_values;
    
    // Verify the function
    std::string error_msg;
    llvm::raw_string_ostream error_stream(error_msg);
    if (llvm::verifyFunction(*func, &error_stream)) {
        error_stream.flush();
        throw std::runtime_error("Function verification failed: " + error_msg);
    }
    
    return func;
}

// ============================================================================
// Phase 4.5.1: Generic Instantiation Code Generation
// ============================================================================

/**
 * Generate code for all specialized generic functions
 * 
 * After the entire AST has been processed and all call sites discovered,
 * this function generates LLVM IR for each monomorphized specialization.
 * 
 * The Monomorphizer has already:
 * 1. Detected calls to generic functions
 * 2. Inferred or received explicit type arguments
 * 3. Created specialized AST nodes with concrete types
 * 4. Generated unique mangled names for each specialization
 * 
 * This function simply iterates through all specializations and generates
 * IR for each one using the normal codegenFuncDecl flow.
 * 
 * Example:
 *   Generic template: func<T>:identity = *T(*T:value) { return value; }
 *   Call site 1: identity(42)        -> T=int32
 *   Call site 2: identity(3.14)      -> T=float64
 *   
 *   Generated specializations:
 *   - _Aria_M_identity_<hash>_int32(int32) -> int32
 *   - _Aria_M_identity_<hash>_float64(float64) -> float64
 * 
 * @return Number of specializations successfully generated
 */
size_t StmtCodegen::codegenAllSpecializations() {
    if (!monomorphizer) {
        return 0;  // No monomorphizer set, no generics to process
    }
    
    const auto& specializations = monomorphizer->getSpecializations();
    size_t generated = 0;
    
    for (const auto* spec : specializations) {
        if (!spec || !spec->funcDecl) {
            continue;
        }
        
        // Generate code for this specialized function
        // The function name in the AST has already been changed to the mangled name
        llvm::Function* func = codegenFuncDecl(spec->funcDecl);
        
        if (func) {
            generated++;
        }
    }
    
    return generated;
}

// ============================================================================
// Phase 4.3.3: If Statement Code Generation
// ============================================================================

/**
 * Generate code for if/else statement
 * 
 * Creates conditional branching using LLVM basic blocks:
 * - Evaluates condition expression
 * - Creates then_block for true branch
 * - Creates else_block for false branch (optional)
 * - Creates merge_block to continue execution after if/else
 * 
 * Example Aria code (simple if):
 *   if (x > 10) {
 *       y = x + 1;
 *   }
 * 
 * Generated LLVM IR:
 *   %cond = icmp sgt i32 %x, 10
 *   br i1 %cond, label %if.then, label %if.merge
 * if.then:
 *   %add = add i32 %x, 1
 *   store i32 %add, i32* %y
 *   br label %if.merge
 * if.merge:
 *   ...
 * 
 * Example Aria code (if/else):
 *   if (x > 10) {
 *       y = x + 1;
 *   } else {
 *       y = x - 1;
 *   }
 * 
 * Generated LLVM IR:
 *   %cond = icmp sgt i32 %x, 10
 *   br i1 %cond, label %if.then, label %if.else
 * if.then:
 *   %add = add i32 %x, 1
 *   store i32 %add, i32* %y
 *   br label %if.merge
 * if.else:
 *   %sub = sub i32 %x, 1
 *   store i32 %sub, i32* %y
 *   br label %if.merge
 * if.merge:
 *   ...
 * 
 * Note: TBB sentinel checking will be added in Phase 4.5+
 */
void StmtCodegen::codegenIf(IfStmt* stmt) {
    if (!expr_codegen) {
        throw std::runtime_error("ExprCodegen not set in StmtCodegen");
    }
    
    // Get current function for basic block creation
    llvm::Function* func = builder.GetInsertBlock()->getParent();
    
    // Generate code for condition expression
    llvm::Value* cond_value = expr_codegen->codegenExpressionNode(stmt->condition.get(), expr_codegen);
    
    if (!cond_value) {
        throw std::runtime_error("Failed to generate code for if condition");
    }
    
    // Convert condition to boolean (i1) if needed
    // LLVM expects i1 for branch instructions
    if (cond_value->getType() != llvm::Type::getInt1Ty(context)) {
        // Compare against zero for integer types
        if (cond_value->getType()->isIntegerTy()) {
            cond_value = builder.CreateICmpNE(
                cond_value,
                llvm::ConstantInt::get(cond_value->getType(), 0),
                "tobool"
            );
        } else if (cond_value->getType()->isFloatingPointTy()) {
            // Compare against 0.0 for floating point types
            cond_value = builder.CreateFCmpONE(
                cond_value,
                llvm::ConstantFP::get(cond_value->getType(), 0.0),
                "tobool"
            );
        } else {
            throw std::runtime_error("Cannot convert condition value to boolean");
        }
    }
    
    // Create basic blocks
    llvm::BasicBlock* then_block = llvm::BasicBlock::Create(context, "if.then", func);
    llvm::BasicBlock* else_block = nullptr;
    llvm::BasicBlock* merge_block = llvm::BasicBlock::Create(context, "if.merge");
    
    // If there's an else branch, create else block
    if (stmt->elseBranch) {
        else_block = llvm::BasicBlock::Create(context, "if.else");
        builder.CreateCondBr(cond_value, then_block, else_block);
    } else {
        // No else branch, jump to merge if condition is false
        builder.CreateCondBr(cond_value, then_block, merge_block);
    }
    
    // Generate code for then branch
    builder.SetInsertPoint(then_block);
    
    if (stmt->thenBranch->type == ASTNode::NodeType::BLOCK) {
        codegenBlock(static_cast<BlockStmt*>(stmt->thenBranch.get()));
    } else {
        // Single statement (not a block)
        codegenStatement(stmt->thenBranch.get());
    }
    
    // Add branch to merge block if then branch doesn't have terminator
    if (!builder.GetInsertBlock()->getTerminator()) {
        builder.CreateBr(merge_block);
    }
    
    // Generate code for else branch if present
    if (else_block) {
        else_block->insertInto(func);
        builder.SetInsertPoint(else_block);
        
        if (stmt->elseBranch->type == ASTNode::NodeType::BLOCK) {
            codegenBlock(static_cast<BlockStmt*>(stmt->elseBranch.get()));
        } else {
            // Single statement or another if statement (else if)
            codegenStatement(stmt->elseBranch.get());
        }
        
        // Add branch to merge block if else branch doesn't have terminator
        if (!builder.GetInsertBlock()->getTerminator()) {
            builder.CreateBr(merge_block);
        }
    }
    
    // Add merge block to function and set it as insertion point
    merge_block->insertInto(func);
    builder.SetInsertPoint(merge_block);
}

// ============================================================================
// ============================================================================
// Phase 4.3.4: Loop Statement Code Generation
// ============================================================================

/**
 * Generate code for while loop
 * 
 * Creates a loop with condition check and body. Uses three basic blocks:
 * - while.cond: Checks loop condition
 * - while.body: Executes loop body
 * - while.end: Continuation after loop
 * 
 * Example Aria code:
 *   i32:i = 0;
 *   while (i < 10) {
 *       i = i + 1;
 *   }
 * 
 * Generated LLVM IR:
 *   br label %while.cond
 * 
 * while.cond:
 *   %i.val = load i32, i32* %i
 *   %cmp = icmp slt i32 %i.val, 10
 *   br i1 %cmp, label %while.body, label %while.end
 * 
 * while.body:
 *   ; body statements
 *   br label %while.cond
 * 
 * while.end:
 *   ; continue execution
 */
void StmtCodegen::codegenWhile(WhileStmt* stmt) {
    // Get current function
    llvm::Function* func = builder.GetInsertBlock()->getParent();
    
    // Create basic blocks for the loop
    llvm::BasicBlock* cond_block = llvm::BasicBlock::Create(context, "while.cond", func);
    llvm::BasicBlock* body_block = llvm::BasicBlock::Create(context, "while.body");
    llvm::BasicBlock* end_block = llvm::BasicBlock::Create(context, "while.end");
    
    // Push loop context for break/continue (unlabeled)
    loop_stack.emplace_back("", cond_block, end_block);
    
    // Push new defer scope for loop body
    defer_stack.push_back(std::vector<BlockStmt*>());
    
    // Branch to condition block
    builder.CreateBr(cond_block);
    
    // Generate condition check
    builder.SetInsertPoint(cond_block);
    
    if (!expr_codegen) {
        throw std::runtime_error("ExprCodegen not set in StmtCodegen");
    }
    
    llvm::Value* cond_value = expr_codegen->codegenExpressionNode(stmt->condition.get(), expr_codegen);
    
    if (!cond_value) {
        throw std::runtime_error("Failed to generate code for while condition");
    }
    
    // Convert non-boolean conditions to boolean
    if (cond_value->getType() != llvm::Type::getInt1Ty(context)) {
        if (cond_value->getType()->isIntegerTy()) {
            // Integer: non-zero is true
            cond_value = builder.CreateICmpNE(cond_value,
                llvm::ConstantInt::get(cond_value->getType(), 0), "tobool");
        } else if (cond_value->getType()->isFloatingPointTy()) {
            // Float: non-zero is true
            cond_value = builder.CreateFCmpONE(cond_value,
                llvm::ConstantFP::get(cond_value->getType(), 0.0), "tobool");
        }
    }
    
    // Conditional branch: if true goto body, else goto end
    builder.CreateCondBr(cond_value, body_block, end_block);
    
    // Generate loop body
    body_block->insertInto(func);
    builder.SetInsertPoint(body_block);
    
    // Generate body statements
    if (stmt->body->type == ASTNode::NodeType::BLOCK) {
        codegenBlock(static_cast<BlockStmt*>(stmt->body.get()));
    } else {
        codegenStatement(stmt->body.get());
    }
    
    // Execute defers and loop back to condition check (if no terminator already present)
    if (!builder.GetInsertBlock()->getTerminator()) {
        executeScopeDefers();
        builder.CreateBr(cond_block);
    }
    
    // Pop defer scope
    defer_stack.pop_back();
    
    // Pop loop context
    loop_stack.pop_back();
    
    // Set insertion point to end block for continuation
    end_block->insertInto(func);
    builder.SetInsertPoint(end_block);
}

/**
 * Generate code for for loop
 * 
 * Creates a loop with initialization, condition, update, and body. Uses four phases:
 * 1. Initialization: Execute init statement (optional)
 * 2. Condition: Check loop condition
 * 3. Body: Execute loop body
 * 4. Update: Execute update expression and loop back to condition
 * 
 * Example Aria code:
 *   for (i32:i = 0; i < 10; i = i + 1) {
 *       print(i);
 *   }
 * 
 * Generated LLVM IR:
 *   ; init
 *   %i = alloca i32
 *   store i32 0, i32* %i
 *   br label %for.cond
 * 
 * for.cond:
 *   %i.val = load i32, i32* %i
 *   %cmp = icmp slt i32 %i.val, 10
 *   br i1 %cmp, label %for.body, label %for.end
 * 
 * for.body:
 *   ; body statements
 *   br label %for.inc
 * 
 * for.inc:
 *   %next = add i32 %i.val, 1
 *   store i32 %next, i32* %i
 *   br label %for.cond
 * 
 * for.end:
 *   ; continue execution
 */
void StmtCodegen::codegenFor(ForStmt* stmt) {
    // Get current function
    llvm::Function* func = builder.GetInsertBlock()->getParent();
    
    // Create basic blocks for the loop
    llvm::BasicBlock* cond_block = llvm::BasicBlock::Create(context, "for.cond");
    llvm::BasicBlock* body_block = llvm::BasicBlock::Create(context, "for.body");
    llvm::BasicBlock* inc_block = llvm::BasicBlock::Create(context, "for.inc");
    llvm::BasicBlock* end_block = llvm::BasicBlock::Create(context, "for.end");
    
    // Generate initialization statement (if present)
    if (stmt->initializer) {
        codegenStatement(stmt->initializer.get());
    }
    
    // Push loop context for break/continue (continue goes to inc_block)
    loop_stack.emplace_back("", inc_block, end_block);
    
    // Push new defer scope for loop body
    defer_stack.push_back(std::vector<BlockStmt*>());
    
    // Branch to condition block
    builder.CreateBr(cond_block);
    
    // Generate condition check
    cond_block->insertInto(func);
    builder.SetInsertPoint(cond_block);
    
    if (!expr_codegen) {
        throw std::runtime_error("ExprCodegen not set in StmtCodegen");
    }
    
    // If no condition, treat as infinite loop (condition = true)
    llvm::Value* cond_value = nullptr;
    if (stmt->condition) {
        cond_value = expr_codegen->codegenExpressionNode(stmt->condition.get(), expr_codegen);
        
        if (!cond_value) {
            throw std::runtime_error("Failed to generate code for for loop condition");
        }
        
        // Convert non-boolean conditions to boolean
        if (cond_value->getType() != llvm::Type::getInt1Ty(context)) {
            if (cond_value->getType()->isIntegerTy()) {
                cond_value = builder.CreateICmpNE(cond_value,
                    llvm::ConstantInt::get(cond_value->getType(), 0), "tobool");
            } else if (cond_value->getType()->isFloatingPointTy()) {
                cond_value = builder.CreateFCmpONE(cond_value,
                    llvm::ConstantFP::get(cond_value->getType(), 0.0), "tobool");
            }
        }
        
        builder.CreateCondBr(cond_value, body_block, end_block);
    } else {
        // No condition = infinite loop (always branch to body)
        builder.CreateBr(body_block);
    }
    
    // Generate loop body
    body_block->insertInto(func);
    builder.SetInsertPoint(body_block);
    
    if (stmt->body->type == ASTNode::NodeType::BLOCK) {
        codegenBlock(static_cast<BlockStmt*>(stmt->body.get()));
    } else {
        codegenStatement(stmt->body.get());
    }
    
    // Branch to increment block (if no terminator already present)
    if (!builder.GetInsertBlock()->getTerminator()) {
        builder.CreateBr(inc_block);
    }
    
    // Generate increment/update
    inc_block->insertInto(func);
    builder.SetInsertPoint(inc_block);
    
    // Execute defers before update
    executeScopeDefers();
    
    if (stmt->update) {
        // Generate update expression (result is discarded)
        expr_codegen->codegenExpressionNode(stmt->update.get(), expr_codegen);
    }
    
    // Loop back to condition
    builder.CreateBr(cond_block);
    
    // Pop defer scope
    defer_stack.pop_back();
    
    // Pop loop context
    loop_stack.pop_back();
    
    // Set insertion point to end block for continuation
    end_block->insertInto(func);
    builder.SetInsertPoint(end_block);
}

/**
 * Generate code for till loop (Aria-specific counted loop)
 * 
 * Creates a loop starting from 0, iterating until limit with specified step.
 * Uses PHI node for the implicit $ iteration variable.
 * 
 * Example Aria code:
 *   till(10, 1) {
 *       print($);  // $ goes from 0 to 9
 *   }
 * 
 * Generated LLVM IR:
 *   br label %till.cond
 * 
 * till.cond:
 *   %$ = phi i32 [ 0, %entry ], [ %$.next, %till.inc ]
 *   %cmp = icmp ne i32 %$, 10
 *   br i1 %cmp, label %till.body, label %till.end
 * 
 * till.body:
 *   ; body statements (can reference %$)
 *   br label %till.inc
 * 
 * till.inc:
 *   %$.next = add i32 %$, 1
 *   br label %till.cond
 * 
 * till.end:
 *   ; continue execution
 */
void StmtCodegen::codegenTill(TillStmt* stmt) {
    // Get current function
    llvm::Function* func = builder.GetInsertBlock()->getParent();
    
    if (!expr_codegen) {
        throw std::runtime_error("ExprCodegen not set in StmtCodegen");
    }
    
    // Evaluate limit and step expressions
    llvm::Value* limit_value = expr_codegen->codegenExpressionNode(stmt->limit.get(), expr_codegen);
    llvm::Value* step_value = expr_codegen->codegenExpressionNode(stmt->step.get(), expr_codegen);
    
    if (!limit_value || !step_value) {
        throw std::runtime_error("Failed to generate code for till limit or step");
    }
    
    // Get the type for $ (same as limit type)
    llvm::Type* counter_type = limit_value->getType();
    
    // Create basic blocks for the loop
    llvm::BasicBlock* cond_block = llvm::BasicBlock::Create(context, "till.cond", func);
    llvm::BasicBlock* body_block = llvm::BasicBlock::Create(context, "till.body");
    llvm::BasicBlock* inc_block = llvm::BasicBlock::Create(context, "till.inc");
    llvm::BasicBlock* end_block = llvm::BasicBlock::Create(context, "till.end");
    
    // Push loop context for break/continue (continue goes to inc_block)
    loop_stack.emplace_back("", inc_block, end_block);
    
    // Push new defer scope for loop body
    defer_stack.push_back(std::vector<BlockStmt*>());
    
    // Branch to condition block
    builder.CreateBr(cond_block);
    
    // Condition block: Create PHI node for $ variable
    builder.SetInsertPoint(cond_block);
    llvm::PHINode* counter_phi = builder.CreatePHI(counter_type, 2, "$");
    
    // Initial value is 0
    llvm::Value* init_value = llvm::ConstantInt::get(counter_type, 0);
    counter_phi->addIncoming(init_value, func->getEntryBlock().getTerminator()->getParent());
    
    // Store $ in named_values so body can reference it
    std::string dollar_var = "$";
    llvm::Value* old_dollar = nullptr;
    auto it = named_values.find(dollar_var);
    if (it != named_values.end()) {
        old_dollar = it->second;  // Save for nested loops
    }
    
    // Create alloca for $ so body can load it
    llvm::IRBuilder<> tmp_builder(&func->getEntryBlock(), func->getEntryBlock().begin());
    llvm::AllocaInst* dollar_alloca = tmp_builder.CreateAlloca(counter_type, nullptr, "$");
    named_values[dollar_var] = dollar_alloca;
    
    // Store current counter value
    builder.CreateStore(counter_phi, dollar_alloca);
    
    // Check condition: $ != limit
    llvm::Value* cond_value = builder.CreateICmpNE(counter_phi, limit_value, "till.cond");
    builder.CreateCondBr(cond_value, body_block, end_block);
    
    // Generate loop body
    body_block->insertInto(func);
    builder.SetInsertPoint(body_block);
    
    if (stmt->body->type == ASTNode::NodeType::BLOCK) {
        codegenBlock(static_cast<BlockStmt*>(stmt->body.get()));
    } else {
        codegenStatement(stmt->body.get());
    }
    
    // Branch to increment block (if no terminator)
    if (!builder.GetInsertBlock()->getTerminator()) {
        builder.CreateBr(inc_block);
    }
    
    // Increment block: $ = $ + step
    inc_block->insertInto(func);
    builder.SetInsertPoint(inc_block);
    
    // Load current counter value
    llvm::Value* current_counter = builder.CreateLoad(counter_type, dollar_alloca, "$");
    
    // Calculate next value: $ + step
    llvm::Value* next_counter;
    if (counter_type->isIntegerTy()) {
        next_counter = builder.CreateAdd(current_counter, step_value, "$.next");
    } else if (counter_type->isFloatingPointTy()) {
        next_counter = builder.CreateFAdd(current_counter, step_value, "$.next");
    } else {
        throw std::runtime_error("Unsupported type for till loop counter");
    }
    
    // Add incoming value to PHI node
    counter_phi->addIncoming(next_counter, inc_block);
    
    // Loop back to condition
    builder.CreateBr(cond_block);
    
    // Pop defer scope
    defer_stack.pop_back();
    
    // Pop loop context
    loop_stack.pop_back();
    
    // Restore old $ value (for nested loops)
    if (old_dollar) {
        named_values[dollar_var] = old_dollar;
    } else {
        named_values.erase(dollar_var);
    }
    
    // Set insertion point to end block
    end_block->insertInto(func);
    builder.SetInsertPoint(end_block);
}

/**
 * Generate code for loop statement (Aria-specific counted loop with start)
 * 
 * Similar to till but with explicit start value.
 * Uses PHI node for the implicit $ iteration variable.
 * 
 * Example Aria code:
 *   loop(5, 15, 2) {
 *       print($);  // $ goes from 5 to 13 by 2s
 *   }
 */
void StmtCodegen::codegenLoop(LoopStmt* stmt) {
    // Get current function
    llvm::Function* func = builder.GetInsertBlock()->getParent();
    
    if (!expr_codegen) {
        throw std::runtime_error("ExprCodegen not set in StmtCodegen");
    }
    
    // Evaluate start, limit, and step expressions
    llvm::Value* start_value = expr_codegen->codegenExpressionNode(stmt->start.get(), expr_codegen);
    llvm::Value* limit_value = expr_codegen->codegenExpressionNode(stmt->limit.get(), expr_codegen);
    llvm::Value* step_value = expr_codegen->codegenExpressionNode(stmt->step.get(), expr_codegen);
    
    if (!start_value || !limit_value || !step_value) {
        throw std::runtime_error("Failed to generate code for loop start, limit, or step");
    }
    
    // Get the type for $ (same as start type)
    llvm::Type* counter_type = start_value->getType();
    
    // Create basic blocks for the loop
    llvm::BasicBlock* cond_block = llvm::BasicBlock::Create(context, "loop.cond", func);
    llvm::BasicBlock* body_block = llvm::BasicBlock::Create(context, "loop.body");
    llvm::BasicBlock* inc_block = llvm::BasicBlock::Create(context, "loop.inc");
    llvm::BasicBlock* end_block = llvm::BasicBlock::Create(context, "loop.end");
    
    // Push loop context for break/continue (continue goes to inc_block)
    loop_stack.emplace_back("", inc_block, end_block);
    
    // Push new defer scope for loop body
    defer_stack.push_back(std::vector<BlockStmt*>());
    
    // Branch to condition block
    builder.CreateBr(cond_block);
    
    // Condition block: Create PHI node for $ variable
    builder.SetInsertPoint(cond_block);
    llvm::PHINode* counter_phi = builder.CreatePHI(counter_type, 2, "$");
    
    // Initial value is start
    counter_phi->addIncoming(start_value, func->getEntryBlock().getTerminator()->getParent());
    
    // Store $ in named_values so body can reference it
    std::string dollar_var = "$";
    llvm::Value* old_dollar = nullptr;
    auto it = named_values.find(dollar_var);
    if (it != named_values.end()) {
        old_dollar = it->second;  // Save for nested loops
    }
    
    // Create alloca for $ so body can load it
    llvm::IRBuilder<> tmp_builder(&func->getEntryBlock(), func->getEntryBlock().begin());
    llvm::AllocaInst* dollar_alloca = tmp_builder.CreateAlloca(counter_type, nullptr, "$");
    named_values[dollar_var] = dollar_alloca;
    
    // Store current counter value
    builder.CreateStore(counter_phi, dollar_alloca);
    
    // Check condition: $ != limit
    llvm::Value* cond_value = builder.CreateICmpNE(counter_phi, limit_value, "loop.cond");
    builder.CreateCondBr(cond_value, body_block, end_block);
    
    // Generate loop body
    body_block->insertInto(func);
    builder.SetInsertPoint(body_block);
    
    if (stmt->body->type == ASTNode::NodeType::BLOCK) {
        codegenBlock(static_cast<BlockStmt*>(stmt->body.get()));
    } else {
        codegenStatement(stmt->body.get());
    }
    
    // Branch to increment block (if no terminator)
    if (!builder.GetInsertBlock()->getTerminator()) {
        builder.CreateBr(inc_block);
    }
    
    // Increment block: $ = $ + step
    inc_block->insertInto(func);
    builder.SetInsertPoint(inc_block);
    
    // Load current counter value
    llvm::Value* current_counter = builder.CreateLoad(counter_type, dollar_alloca, "$");
    
    // Calculate next value: $ + step
    llvm::Value* next_counter;
    if (counter_type->isIntegerTy()) {
        next_counter = builder.CreateAdd(current_counter, step_value, "$.next");
    } else if (counter_type->isFloatingPointTy()) {
        next_counter = builder.CreateFAdd(current_counter, step_value, "$.next");
    } else {
        throw std::runtime_error("Unsupported type for loop counter");
    }
    
    // Add incoming value to PHI node
    counter_phi->addIncoming(next_counter, inc_block);
    
    // Loop back to condition
    builder.CreateBr(cond_block);
    
    // Pop defer scope
    defer_stack.pop_back();
    
    // Pop loop context
    loop_stack.pop_back();
    
    // Restore old $ value (for nested loops)
    if (old_dollar) {
        named_values[dollar_var] = old_dollar;
    } else {
        named_values.erase(dollar_var);
    }
    
    // Set insertion point to end block
    end_block->insertInto(func);
    builder.SetInsertPoint(end_block);
}

/**
 * Generate code for when loop (tri-state completion tracking)
 * 
 * Creates a loop with then/end blocks for completion handling.
 * - then block: Executes if loop completes naturally (condition becomes false)
 * - end block: Executes if loop never runs or breaks early
 * 
 * Example Aria code:
 *   when(i < 10) {
 *       i = i + 1;
 *       if (found) break;
 *   } then {
 *       print("Completed all iterations");
 *   } end {
 *       print("Broke early or never started");
 *   }
 * 
 * Uses a completion flag to track which block to execute.
 */
void StmtCodegen::codegenWhen(WhenStmt* stmt) {
    // Get current function
    llvm::Function* func = builder.GetInsertBlock()->getParent();
    
    if (!expr_codegen) {
        throw std::runtime_error("ExprCodegen not set in StmtCodegen");
    }
    
    // Create alloca for completion tracking flag
    llvm::IRBuilder<> tmp_builder(&func->getEntryBlock(), func->getEntryBlock().begin());
    llvm::AllocaInst* completed_flag = tmp_builder.CreateAlloca(
        llvm::Type::getInt1Ty(context), nullptr, "when.completed");
    
    // Initialize to false (assume end block)
    builder.CreateStore(llvm::ConstantInt::get(llvm::Type::getInt1Ty(context), 0), completed_flag);
    
    // Create basic blocks
    llvm::BasicBlock* cond_block = llvm::BasicBlock::Create(context, "when.cond", func);
    llvm::BasicBlock* body_block = llvm::BasicBlock::Create(context, "when.body");
    llvm::BasicBlock* decision_block = llvm::BasicBlock::Create(context, "when.decision");
    llvm::BasicBlock* then_block = nullptr;
    llvm::BasicBlock* end_block = nullptr;
    llvm::BasicBlock* exit_block = llvm::BasicBlock::Create(context, "when.exit");
    
    if (stmt->then_block) {
        then_block = llvm::BasicBlock::Create(context, "when.then");
    }
    if (stmt->end_block) {
        end_block = llvm::BasicBlock::Create(context, "when.end");
    }
    
    // Push loop context for break/continue (continue goes to cond_block, break goes to decision_block)
    loop_stack.emplace_back("", cond_block, decision_block);
    
    // Push new defer scope for loop body
    defer_stack.push_back(std::vector<BlockStmt*>());
    
    // Branch to condition
    builder.CreateBr(cond_block);
    
    // Condition block
    builder.SetInsertPoint(cond_block);
    llvm::Value* cond_value = expr_codegen->codegenExpressionNode(stmt->condition.get(), expr_codegen);
    
    if (!cond_value) {
        throw std::runtime_error("Failed to generate code for when condition");
    }
    
    // Convert non-boolean conditions to boolean
    if (cond_value->getType() != llvm::Type::getInt1Ty(context)) {
        if (cond_value->getType()->isIntegerTy()) {
            cond_value = builder.CreateICmpNE(cond_value,
                llvm::ConstantInt::get(cond_value->getType(), 0), "tobool");
        } else if (cond_value->getType()->isFloatingPointTy()) {
            cond_value = builder.CreateFCmpONE(cond_value,
                llvm::ConstantFP::get(cond_value->getType(), 0.0), "tobool");
        }
    }
    
    // If condition true: go to body, else: go to decision (might be then or end)
    builder.CreateCondBr(cond_value, body_block, decision_block);
    
    // Body block
    body_block->insertInto(func);
    builder.SetInsertPoint(body_block);
    
    if (stmt->body->type == ASTNode::NodeType::BLOCK) {
        codegenBlock(static_cast<BlockStmt*>(stmt->body.get()));
    } else {
        codegenStatement(stmt->body.get());
    }
    
    // After body, loop back to condition (if no terminator)
    if (!builder.GetInsertBlock()->getTerminator()) {
        builder.CreateBr(cond_block);
    }
    
    // Decision block: Determine if loop completed naturally
    decision_block->insertInto(func);
    builder.SetInsertPoint(decision_block);
    
    // Load the completion flag
    llvm::Value* completed = builder.CreateLoad(llvm::Type::getInt1Ty(context), completed_flag, "completed");
    
    // If we exited via condition becoming false (not break), set completed = true
    // For simplicity in this implementation, we assume natural exit means completed
    // In a full implementation, break statements would need to preserve the false value
    builder.CreateStore(llvm::ConstantInt::get(llvm::Type::getInt1Ty(context), 1), completed_flag);
    completed = builder.CreateLoad(llvm::Type::getInt1Ty(context), completed_flag, "completed");
    
    // Branch based on completion
    if (then_block && end_block) {
        builder.CreateCondBr(completed, then_block, end_block);
    } else if (then_block) {
        builder.CreateCondBr(completed, then_block, exit_block);
    } else if (end_block) {
        builder.CreateBr(end_block);
    } else {
        builder.CreateBr(exit_block);
    }
    
    // Then block (executed on natural completion)
    if (then_block) {
        then_block->insertInto(func);
        builder.SetInsertPoint(then_block);
        
        if (stmt->then_block->type == ASTNode::NodeType::BLOCK) {
            codegenBlock(static_cast<BlockStmt*>(stmt->then_block.get()));
        } else {
            codegenStatement(stmt->then_block.get());
        }
        
        if (!builder.GetInsertBlock()->getTerminator()) {
            builder.CreateBr(exit_block);
        }
    }
    
    // End block (executed on break or never started)
    if (end_block) {
        end_block->insertInto(func);
        builder.SetInsertPoint(end_block);
        
        if (stmt->end_block->type == ASTNode::NodeType::BLOCK) {
            codegenBlock(static_cast<BlockStmt*>(stmt->end_block.get()));
        } else {
            codegenStatement(stmt->end_block.get());
        }
        
        if (!builder.GetInsertBlock()->getTerminator()) {
            builder.CreateBr(exit_block);
        }
    }
    
    // Pop defer scope
    defer_stack.pop_back();
    
    // Pop loop context
    loop_stack.pop_back();
    
    // Exit block
    exit_block->insertInto(func);
    builder.SetInsertPoint(exit_block);
}

// ============================================================================
// Phase 4.3.5: Pick Pattern Matching Code Generation
// ============================================================================

/**
 * Generate code for pick statement (pattern matching)
 * 
 * Implements pattern matching via cascading if-else structure.
 * Supports literal matches, range comparisons, and wildcard patterns.
 * 
 * Example Aria code:
 *   pick(status) {
 *       (200) { success(); },
 *       (404) { notFound(); },
 *       (500..599) { serverError(); },
 *       (*) { other(); }
 *   }
 * 
 * Generated LLVM IR structure:
 *   %selector = <evaluated selector>
 *   
 *   case0.check:
 *     %match0 = icmp eq %selector, 200
 *     br i1 %match0, label %case0.body, label %case1.check
 *   
 *   case0.body:
 *     <case body code>
 *     br label %pick.end
 *   
 *   case1.check:
 *     ...
 *   
 *   pick.end:
 *     <continue after pick>
 * 
 * Pattern types:
 * - Literal: (5), (200) - exact value match
 * - Less than: (< 10) - value < 10
 * - Greater than: (> 20) - value > 20
 * - Range (inclusive): (10..20) - 10 <= value <= 20
 * - Range (exclusive): (10...20) - 10 <= value < 20
 * - Wildcard: (*) - matches anything (default case)
 * - Unreachable: (!) - marks unreachable case
 * 
 * Labels and fallthrough:
 * - Cases can have labels: success:(200) { ... }
 * - fall(label) performs explicit fallthrough to labeled case
 * - Fallthrough is NOT implicit (unlike C switch)
 */
void StmtCodegen::codegenPick(PickStmt* stmt) {
    // Get current function
    llvm::Function* func = builder.GetInsertBlock()->getParent();
    
    if (!expr_codegen) {
        throw std::runtime_error("ExprCodegen not set in StmtCodegen");
    }
    
    // Evaluate the selector expression once
    llvm::Value* selector = expr_codegen->codegenExpressionNode(stmt->selector.get(), expr_codegen);
    
    if (!selector) {
        throw std::runtime_error("Failed to generate code for pick selector");
    }
    
    // Create end block (continuation after pick)
    llvm::BasicBlock* end_block = llvm::BasicBlock::Create(context, "pick.end");
    
    // Create map of labeled blocks for fall() statements
    std::map<std::string, llvm::BasicBlock*> labeled_blocks;
    
    // Create blocks for all cases first (for fall() support)
    std::vector<llvm::BasicBlock*> check_blocks;
    std::vector<llvm::BasicBlock*> body_blocks;
    
    for (size_t i = 0; i < stmt->cases.size(); i++) {
        PickCase* pick_case = static_cast<PickCase*>(stmt->cases[i].get());
        
        std::string check_name = "case" + std::to_string(i) + ".check";
        std::string body_name = "case" + std::to_string(i) + ".body";
        
        llvm::BasicBlock* check_block = llvm::BasicBlock::Create(context, check_name);
        llvm::BasicBlock* body_block = llvm::BasicBlock::Create(context, body_name);
        
        check_blocks.push_back(check_block);
        body_blocks.push_back(body_block);
        
        // Store labeled blocks for fall() statements
        if (!pick_case->label.empty()) {
            labeled_blocks[pick_case->label] = body_block;
        }
    }
    
    // Branch to first check block
    if (!check_blocks.empty()) {
        builder.CreateBr(check_blocks[0]);
    } else {
        // No cases - branch to end
        builder.CreateBr(end_block);
    }
    
    // Generate code for each case
    for (size_t i = 0; i < stmt->cases.size(); i++) {
        PickCase* pick_case = static_cast<PickCase*>(stmt->cases[i].get());
        llvm::BasicBlock* check_block = check_blocks[i];
        llvm::BasicBlock* body_block = body_blocks[i];
        llvm::BasicBlock* next_check = (i + 1 < check_blocks.size()) ? check_blocks[i + 1] : end_block;
        
        // Check block: Evaluate pattern match
        check_block->insertInto(func);
        builder.SetInsertPoint(check_block);
        
        // Check if this is a wildcard pattern (*)
        bool is_wildcard = false;
        if (pick_case->pattern->type == ASTNode::NodeType::IDENTIFIER) {
            IdentifierExpr* ident = static_cast<IdentifierExpr*>(pick_case->pattern.get());
            if (ident->name == "*") {
                is_wildcard = true;
            }
        }
        
        // Check if this is an unreachable pattern (!)
        if (pick_case->is_unreachable) {
            // Generate unreachable instruction
            builder.CreateUnreachable();
            
            // Body block is dead code, but we still need to insert it for structure
            body_block->insertInto(func);
            builder.SetInsertPoint(body_block);
            builder.CreateUnreachable();
            continue;
        }
        
        llvm::Value* match_result = nullptr;
        
        if (is_wildcard) {
            // Wildcard matches everything - always true
            match_result = llvm::ConstantInt::get(llvm::Type::getInt1Ty(context), 1);
        } else if (pick_case->pattern->type == ASTNode::NodeType::BINARY_OP) {
            // Handle comparison patterns: (< 10), (> 20), ranges (10..20)
            BinaryExpr* bin_expr = static_cast<BinaryExpr*>(pick_case->pattern.get());
            
            // Check if this is a range operator (.., ...)
            if (bin_expr->op.type == TokenType::TOKEN_DOT_DOT || bin_expr->op.type == TokenType::TOKEN_DOT_DOT_DOT) {
                // Range pattern: start..end or start...end
                llvm::Value* start_val = expr_codegen->codegenExpressionNode(bin_expr->left.get(), expr_codegen);
                llvm::Value* end_val = expr_codegen->codegenExpressionNode(bin_expr->right.get(), expr_codegen);
                
                if (!start_val || !end_val) {
                    throw std::runtime_error("Failed to generate range bounds in pick");
                }
                
                // Check: selector >= start
                llvm::Value* ge_start;
                if (selector->getType()->isIntegerTy()) {
                    ge_start = builder.CreateICmpSGE(selector, start_val, "ge.start");
                } else {
                    ge_start = builder.CreateFCmpOGE(selector, start_val, "ge.start");
                }
                
                // Check: selector <= end (inclusive) or selector < end (exclusive)
                llvm::Value* cmp_end;
                if (bin_expr->op.type == TokenType::TOKEN_DOT_DOT) {
                    // Inclusive range: selector <= end
                    if (selector->getType()->isIntegerTy()) {
                        cmp_end = builder.CreateICmpSLE(selector, end_val, "le.end");
                    } else {
                        cmp_end = builder.CreateFCmpOLE(selector, end_val, "le.end");
                    }
                } else {
                    // Exclusive range: selector < end
                    if (selector->getType()->isIntegerTy()) {
                        cmp_end = builder.CreateICmpSLT(selector, end_val, "lt.end");
                    } else {
                        cmp_end = builder.CreateFCmpOLT(selector, end_val, "lt.end");
                    }
                }
                
                // Combine: start <= selector && selector <= end
                match_result = builder.CreateAnd(ge_start, cmp_end, "range.match");
                
            } else if (bin_expr->op.type == TokenType::TOKEN_LESS) {
                // Pattern: (< value)
                llvm::Value* comp_val = expr_codegen->codegenExpressionNode(bin_expr->right.get(), expr_codegen);
                
                if (!comp_val) {
                    throw std::runtime_error("Failed to generate comparison value in pick");
                }
                
                if (selector->getType()->isIntegerTy()) {
                    match_result = builder.CreateICmpSLT(selector, comp_val, "lt.match");
                } else {
                    match_result = builder.CreateFCmpOLT(selector, comp_val, "lt.match");
                }
                
            } else if (bin_expr->op.type == TokenType::TOKEN_GREATER) {
                // Pattern: (> value)
                llvm::Value* comp_val = expr_codegen->codegenExpressionNode(bin_expr->right.get(), expr_codegen);
                
                if (!comp_val) {
                    throw std::runtime_error("Failed to generate comparison value in pick");
                }
                
                if (selector->getType()->isIntegerTy()) {
                    match_result = builder.CreateICmpSGT(selector, comp_val, "gt.match");
                } else {
                    match_result = builder.CreateFCmpOGT(selector, comp_val, "gt.match");
                }
                
            } else if (bin_expr->op.type == TokenType::TOKEN_LESS_EQUAL) {
                // Pattern: (<= value)
                llvm::Value* comp_val = expr_codegen->codegenExpressionNode(bin_expr->right.get(), expr_codegen);
                
                if (!comp_val) {
                    throw std::runtime_error("Failed to generate comparison value in pick");
                }
                
                if (selector->getType()->isIntegerTy()) {
                    match_result = builder.CreateICmpSLE(selector, comp_val, "le.match");
                } else {
                    match_result = builder.CreateFCmpOLE(selector, comp_val, "le.match");
                }
                
            } else if (bin_expr->op.type == TokenType::TOKEN_GREATER_EQUAL) {
                // Pattern: (>= value)
                llvm::Value* comp_val = expr_codegen->codegenExpressionNode(bin_expr->right.get(), expr_codegen);
                
                if (!comp_val) {
                    throw std::runtime_error("Failed to generate comparison value in pick");
                }
                
                if (selector->getType()->isIntegerTy()) {
                    match_result = builder.CreateICmpSGE(selector, comp_val, "ge.match");
                } else {
                    match_result = builder.CreateFCmpOGE(selector, comp_val, "ge.match");
                }
            } else {
                // Unknown binary operator in pattern
                throw std::runtime_error("Unsupported binary operator in pick pattern");
            }
            
        } else {
            // Literal pattern: exact value match
            llvm::Value* pattern_val = expr_codegen->codegenExpressionNode(pick_case->pattern.get(), expr_codegen);
            
            if (!pattern_val) {
                throw std::runtime_error("Failed to generate pattern value in pick");
            }
            
            // Compare selector with pattern value
            if (selector->getType()->isIntegerTy()) {
                match_result = builder.CreateICmpEQ(selector, pattern_val, "match");
            } else if (selector->getType()->isFloatingPointTy()) {
                match_result = builder.CreateFCmpOEQ(selector, pattern_val, "match");
            } else {
                throw std::runtime_error("Unsupported selector type in pick");
            }
        }
        
        // Branch based on match result
        if (match_result) {
            builder.CreateCondBr(match_result, body_block, next_check);
        }
        
        // Body block: Execute case body
        body_block->insertInto(func);
        builder.SetInsertPoint(body_block);
        
        // Generate code for case body
        if (pick_case->body->type == ASTNode::NodeType::BLOCK) {
            codegenBlock(static_cast<BlockStmt*>(pick_case->body.get()));
        } else {
            codegenStatement(pick_case->body.get());
        }
        
        // Branch to end (no implicit fallthrough)
        if (!builder.GetInsertBlock()->getTerminator()) {
            builder.CreateBr(end_block);
        }
    }
    
    // End block (continuation after pick)
    end_block->insertInto(func);
    builder.SetInsertPoint(end_block);
}

/**
 * Generate code for fall statement (explicit fallthrough in pick)
 * 
 * The fall statement provides explicit control flow transfer to a labeled
 * case within a pick statement. Unlike C's implicit fallthrough, Aria
 * requires explicit fall(label) to transfer control.
 * 
 * Example Aria code:
 *   pick(status) {
 *       success:(200) { print("OK"); fall(log); },
 *       created:(201) { print("Created"); fall(log); },
 *       log:(0) { logRequest(); }
 *   }
 * 
 * Note: This is a placeholder implementation. Full fall() support requires
 * maintaining a map of labeled blocks during pick codegen and generating
 * unconditional branches to those blocks.
 */
void StmtCodegen::codegenFall(FallStmt* stmt) {
    // TODO: Implement fall() with label resolution
    // For now, this is a placeholder that will be enhanced when we add
    // full label tracking in pick statements
    throw std::runtime_error("Fall statement not yet fully implemented - requires label resolution in pick");
}

// ============================================================================
// Block and Expression Statement Code Generation
// ============================================================================

void StmtCodegen::codegenBlock(BlockStmt* stmt) {
    // Push new defer scope for this block
    defer_stack.push_back(std::vector<BlockStmt*>());
    
    // Generate code for each statement in the block
    for (const auto& statement : stmt->statements) {
        codegenStatement(statement.get());
    }
    
    // Execute defers at block exit (LIFO order)
    executeScopeDefers();
    
    // Pop defer scope
    defer_stack.pop_back();
}

// ============================================================================
// Phase 4.3.6: Control Flow Statements (break, continue, return, defer)
// ============================================================================

/**
 * Execute all defer blocks in the current scope
 * 
 * Defer blocks are executed in LIFO (Last-In, First-Out) order,
 * implementing Aria's block-scoped RAII pattern.
 */
void StmtCodegen::executeScopeDefers() {
    if (defer_stack.empty()) {
        return;
    }
    
    // Get the current scope's defer blocks
    std::vector<BlockStmt*>& current_scope_defers = defer_stack.back();
    
    // Execute in LIFO order (reverse)
    // Note: Execute statements directly, not via codegenBlock to avoid recursive defer scoping
    for (auto it = current_scope_defers.rbegin(); it != current_scope_defers.rend(); ++it) {
        BlockStmt* defer_block = *it;
        for (const auto& statement : defer_block->statements) {
            codegenStatement(statement.get());
        }
    }
}

/**
 * Execute all defer blocks up to function level
 * 
 * Called by return statements to ensure all defers in the function
 * are executed before returning, maintaining LIFO order.
 */
void StmtCodegen::executeFunctionDefers() {
    // Execute all scopes' defers in reverse order (inside-out)
    // Note: Execute statements directly, not via codegenBlock to avoid recursive defer scoping
    for (auto scope_it = defer_stack.rbegin(); scope_it != defer_stack.rend(); ++scope_it) {
        // Within each scope, execute defers in LIFO order
        for (auto defer_it = scope_it->rbegin(); defer_it != scope_it->rend(); ++defer_it) {
            BlockStmt* defer_block = *defer_it;
            for (const auto& statement : defer_block->statements) {
                codegenStatement(statement.get());
            }
        }
    }
}

/**
 * Generate code for return statement
 * 
 * Returns from the current function, executing all defer blocks
 * along the way in LIFO order. This ensures proper resource cleanup.
 * 
 * Example Aria code:
 *   func:test = i32() {
 *       defer { print("cleanup"); }
 *       return 42;  // Executes defer before returning
 *   }
 * 
 * Per research_020: Return must execute all defers in the function
 * before transferring control to the caller.
 */
void StmtCodegen::codegenReturn(ReturnStmt* stmt) {
    // Execute all defer blocks before returning (LIFO order)
    executeFunctionDefers();
    
    if (stmt->value) {
        if (!expr_codegen) {
            throw std::runtime_error("ExprCodegen not set in StmtCodegen");
        }
        
        // Generate code for return value expression
        llvm::Value* ret_value = expr_codegen->codegenExpressionNode(stmt->value.get(), expr_codegen);
        
        if (!ret_value) {
            throw std::runtime_error("Failed to generate code for return value");
        }
        
        // Get the current function's return type
        llvm::Function* current_func = builder.GetInsertBlock()->getParent();
        llvm::Type* expected_return_type = current_func->getReturnType();
        
        // Cast/coerce the return value to match the function's return type if needed
        if (ret_value->getType() != expected_return_type) {
            // Integer type coercion
            if (ret_value->getType()->isIntegerTy() && expected_return_type->isIntegerTy()) {
                // Truncate or extend to match target type
                ret_value = builder.CreateIntCast(ret_value, expected_return_type, true, "ret_cast");
            }
            // Float type coercion
            else if (ret_value->getType()->isFloatingPointTy() && expected_return_type->isFloatingPointTy()) {
                ret_value = builder.CreateFPCast(ret_value, expected_return_type, "ret_fpcast");
            }
            // Int to float coercion
            else if (ret_value->getType()->isIntegerTy() && expected_return_type->isFloatingPointTy()) {
                ret_value = builder.CreateSIToFP(ret_value, expected_return_type, "ret_itof");
            }
            // Float to int coercion
            else if (ret_value->getType()->isFloatingPointTy() && expected_return_type->isIntegerTy()) {
                ret_value = builder.CreateFPToSI(ret_value, expected_return_type, "ret_ftoi");
            }
            // Otherwise, attempt bitcast for pointer types
            else if (ret_value->getType()->isPointerTy() && expected_return_type->isPointerTy()) {
                ret_value = builder.CreateBitCast(ret_value, expected_return_type, "ret_ptrcast");
            }
            else {
                throw std::runtime_error(
                    "Cannot coerce return value type " + 
                    std::string(ret_value->getType()->isIntegerTy() ? "int" : "?") +
                    " to function return type " +
                    std::string(expected_return_type->isIntegerTy() ? "int" : "?")
                );
            }
        }
        
        builder.CreateRet(ret_value);
    } else {
        // Return void
        builder.CreateRetVoid();
    }
}

/**
 * Generate code for break statement
 * 
 * Exits the current loop (or labeled loop) by branching to the loop's
 * break_block. Executes defer blocks in the current scope before exiting.
 * 
 * Example Aria code:
 *   while (true) {
 *       defer { print("cleanup"); }
 *       if (done) { break; }  // Executes defer before breaking
 *   }
 * 
 * Labeled break:
 *   outer: while (true) {
 *       while (true) {
 *           break(outer);  // Breaks from outer loop
 *       }
 *   }
 * 
 * Per research_020: Break transfers control to the loop's end block.
 * In when loops, break triggers the end block instead of then block.
 */
void StmtCodegen::codegenBreak(BreakStmt* stmt) {
    if (loop_stack.empty()) {
        throw std::runtime_error("break statement outside of loop");
    }
    
    // Execute defers in current scope before breaking
    executeScopeDefers();
    
    // Find the target loop
    llvm::BasicBlock* target_break_block = nullptr;
    
    if (stmt->label.empty()) {
        // Unlabeled break: target innermost loop
        target_break_block = loop_stack.back().break_block;
    } else {
        // Labeled break: search for matching label
        for (auto it = loop_stack.rbegin(); it != loop_stack.rend(); ++it) {
            if (it->label == stmt->label) {
                target_break_block = it->break_block;
                break;
            }
        }
        
        if (!target_break_block) {
            throw std::runtime_error("break label '" + stmt->label + "' not found");
        }
    }
    
    // Branch to the loop's break block
    builder.CreateBr(target_break_block);
}

/**
 * Generate code for continue statement
 * 
 * Skips the remainder of the current loop iteration by branching to
 * the loop's continue_block. Executes defer blocks before continuing.
 * 
 * Example Aria code:
 *   for (i32:i = 0; i < 10; i++) {
 *       if (i == 5) { continue; }
 *       print(i);
 *   }
 * 
 * Labeled continue:
 *   outer: for (i32:i = 0; i < 3; i++) {
 *       for (i32:j = 0; j < 3; j++) {
 *           continue(outer);  // Continues outer loop
 *       }
 *   }
 * 
 * Per research_020: Continue transfers control to the loop's condition
 * check or update expression.
 */
void StmtCodegen::codegenContinue(ContinueStmt* stmt) {
    if (loop_stack.empty()) {
        throw std::runtime_error("continue statement outside of loop");
    }
    
    // Execute defers in current scope before continuing
    executeScopeDefers();
    
    // Find the target loop
    llvm::BasicBlock* target_continue_block = nullptr;
    
    if (stmt->label.empty()) {
        // Unlabeled continue: target innermost loop
        target_continue_block = loop_stack.back().continue_block;
    } else {
        // Labeled continue: search for matching label
        for (auto it = loop_stack.rbegin(); it != loop_stack.rend(); ++it) {
            if (it->label == stmt->label) {
                target_continue_block = it->continue_block;
                break;
            }
        }
        
        if (!target_continue_block) {
            throw std::runtime_error("continue label '" + stmt->label + "' not found");
        }
    }
    
    // Branch to the loop's continue block
    builder.CreateBr(target_continue_block);
}

/**
 * Generate code for defer statement
 * 
 * Registers a block to be executed at scope exit in LIFO order.
 * This implements Aria's block-scoped RAII pattern.
 * 
 * Example Aria code:
 *   {
 *       defer { print("cleanup 1"); }
 *       defer { print("cleanup 2"); }
 *       print("work");
 *   }  // Outputs: "work", "cleanup 2", "cleanup 1"
 * 
 * Per research_020: Defer is block-scoped (unlike Go's function-scoped defer).
 * Defers execute at scope exit in LIFO order: last deferred executes first.
 * 
 * Critical for Wild memory management:
 *   wild i32*:ptr = aria.alloc<i32>(100);
 *   defer { aria.free(ptr); }  // Guaranteed cleanup
 */
void StmtCodegen::codegenDefer(DeferStmt* stmt) {
    if (defer_stack.empty()) {
        throw std::runtime_error("defer statement outside of scope");
    }
    
    // Get the block to defer
    BlockStmt* defer_block = static_cast<BlockStmt*>(stmt->block.get());
    
    // Add to current scope's defer list
    defer_stack.back().push_back(defer_block);
    
    // Note: Actual execution happens at scope exit, not here
}

void StmtCodegen::codegenExpressionStmt(ExpressionStmt* stmt) {
    if (!expr_codegen) {
        throw std::runtime_error("ExprCodegen not set in StmtCodegen");
    }
    
    // Generate code for the expression (result is discarded)
    expr_codegen->codegenExpressionNode(stmt->expression.get(), expr_codegen);
}

// ============================================================================
// Statement Dispatcher
// ============================================================================

void StmtCodegen::codegenStatement(ASTNode* stmt) {
    if (!stmt) {
        return;
    }
    
    switch (stmt->type) {
        case ASTNode::NodeType::VAR_DECL:
            codegenVarDecl(static_cast<VarDeclStmt*>(stmt));
            break;
        
        case ASTNode::NodeType::FUNC_DECL:
            codegenFuncDecl(static_cast<FuncDeclStmt*>(stmt));
            break;
        
        case ASTNode::NodeType::IF:
            codegenIf(static_cast<IfStmt*>(stmt));
            break;
        
        case ASTNode::NodeType::WHILE:
            codegenWhile(static_cast<WhileStmt*>(stmt));
            break;
        
        case ASTNode::NodeType::FOR:
            codegenFor(static_cast<ForStmt*>(stmt));
            break;
        
        case ASTNode::NodeType::TILL:
            codegenTill(static_cast<TillStmt*>(stmt));
            break;
        
        case ASTNode::NodeType::LOOP:
            codegenLoop(static_cast<LoopStmt*>(stmt));
            break;
        
        case ASTNode::NodeType::WHEN:
            codegenWhen(static_cast<WhenStmt*>(stmt));
            break;
        
        case ASTNode::NodeType::PICK:
            codegenPick(static_cast<PickStmt*>(stmt));
            break;
        
        case ASTNode::NodeType::FALL:
            codegenFall(static_cast<FallStmt*>(stmt));
            break;
        
        case ASTNode::NodeType::BLOCK:
            codegenBlock(static_cast<BlockStmt*>(stmt));
            break;
        
        case ASTNode::NodeType::RETURN:
            codegenReturn(static_cast<ReturnStmt*>(stmt));
            break;
        
        case ASTNode::NodeType::BREAK:
            codegenBreak(static_cast<BreakStmt*>(stmt));
            break;
        
        case ASTNode::NodeType::CONTINUE:
            codegenContinue(static_cast<ContinueStmt*>(stmt));
            break;
        
        case ASTNode::NodeType::DEFER:
            codegenDefer(static_cast<DeferStmt*>(stmt));
            break;
        
        case ASTNode::NodeType::EXPRESSION_STMT:
            codegenExpressionStmt(static_cast<ExpressionStmt*>(stmt));
            break;
        
        default:
            throw std::runtime_error("Unsupported statement type in codegen: " + 
                                     std::to_string(static_cast<int>(stmt->type)));
    }
}
