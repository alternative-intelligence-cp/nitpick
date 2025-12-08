/**
 * src/backend/codegen.cpp
 * 
 * Aria Compiler - LLVM Code Generation Backend
 * Version: 0.0.6
 * 
 * This file implements the translation of the Aria Abstract Syntax Tree (AST)
 * into LLVM Intermediate Representation (IR).
 * 
 * Features:
 * - Hybrid Memory Support: Distinguishes between Stack, Wild (mimalloc), and GC allocations.
 * - Exotic Type Lowering: Handles int512, trit, and tryte types.
 * - Pattern Matching: Compiles 'pick' statements into optimized branch chains.
 * - Loops: Implements 'till' loops with SSA-based iteration variables.
 * 
 * Dependencies:
 * - LLVM 18 Core, IR, Support
 * - Aria AST Headers
 */

#include "codegen.h"
#include "codegen_context.h"
#include "codegen_tbb.h"
#include "tbb_optimizer.h"
#include "../frontend/ast.h"
#include "../frontend/ast/stmt.h"
#include "../frontend/ast/expr.h"
#include "../frontend/ast/control_flow.h"
#include "../frontend/ast/loops.h"
#include "../frontend/ast/defer.h"
#include "../frontend/ast/module.h"
#include "../frontend/tokens.h"
#include <iostream>

// LLVM Includes
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Verifier.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/InlineAsm.h>
#include <llvm/IR/Intrinsics.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Analysis/LoopAnalysisManager.h>
#include <llvm/Analysis/CGSCCPassManager.h>

#include <vector>
#include <map>
#include <stack>

using namespace llvm;

namespace aria {
namespace backend {

// Import frontend types for use in this file
using aria::frontend::AstVisitor;
using aria::frontend::Block;
using aria::frontend::VarDecl;
using aria::frontend::StructDecl;
using aria::frontend::FuncDecl;
using aria::frontend::ExpressionStmt;
using aria::frontend::PickStmt;
using aria::frontend::PickCase;
using aria::frontend::FallStmt;
using aria::frontend::TillLoop;
using aria::frontend::WhenLoop;
using aria::frontend::ForLoop;
using aria::frontend::WhileLoop;
using aria::frontend::BreakStmt;
using aria::frontend::ContinueStmt;
using aria::frontend::IfStmt;
using aria::frontend::DeferStmt;
using aria::frontend::Expression;
using aria::frontend::IntLiteral;
using aria::frontend::BoolLiteral;
using aria::frontend::NullLiteral;
using aria::frontend::VarExpr;
using aria::frontend::CallExpr;
using aria::frontend::ArrayLiteral;
using aria::frontend::IndexExpr;
using aria::frontend::BinaryOp;
using aria::frontend::UnaryOp;
using aria::frontend::ReturnStmt;
using aria::frontend::UseStmt;
using aria::frontend::ModDef;
using aria::frontend::ExternBlock;

// Bring CodeGenContext and ScopeGuard from extracted header
using aria::backend::CodeGenContext;
using aria::backend::ScopeGuard;

// =============================================================================
// The Code Generator Visitor
// =============================================================================

class CodeGenVisitor : public AstVisitor {
    CodeGenContext& ctx;

public:
    CodeGenVisitor(CodeGenContext& context) : ctx(context) {}

    // -------------------------------------------------------------------------
    // Helper: Runtime Function Declarations (Allocators, Syscalls)
    // -------------------------------------------------------------------------
    
    // createSyscall - Generate x86-64 Linux syscall using inline assembly
    // Register mapping per x86-64 ABI:
    //   rax = syscall number
    //   rdi, rsi, rdx, r10 (NOT rcx!), r8, r9 = args 1-6
    //   Clobbers: rcx (replaced by r10), r11 (used by kernel)
    Value* createSyscall(uint64_t syscallNum, const std::vector<Value*>& args, Type* returnType = nullptr) {
        // Syscall arguments (max 6)
        if (args.size() > 6) {
            throw std::runtime_error("Syscalls support max 6 arguments");
        }
        
        // Default return type is i64 (syscalls always return i64)
        if (returnType == nullptr) {
            returnType = Type::getInt64Ty(ctx.llvmContext);
        }
        
        // Register constraints for inline assembly
        // Input constraints: rdi, rsi, rdx, r10 (arg4 uses r10 NOT rcx!), r8, r9
        static const char* registerConstraints[] = {"{rdi}", "{rsi}", "{rdx}", "{r10}", "{r8}", "{r9}"};
        
        // Build inline asm operand type list
        std::vector<Type*> asmArgTypes;
        for (auto* arg : args) {
            asmArgTypes.push_back(arg->getType());
        }
        
        // Function type: (syscall_args...) -> i64
        FunctionType* asmFuncType = FunctionType::get(
            Type::getInt64Ty(ctx.llvmContext),
            asmArgTypes,
            false
        );
        
        // Build constraint string: "={rax},0,1,2,3,4,5,~{rcx},~{r11},~{memory}"
        std::string constraints = "={rax}";  // Output: rax (syscall return value)
        for (size_t i = 0; i < args.size(); ++i) {
            constraints += ",";
            constraints += registerConstraints[i];
        }
        // Clobbers: rcx, r11 (kernel uses these), memory (syscalls can modify memory)
        constraints += ",~{rcx},~{r11},~{memory}";
        
        // Assembly template: load syscall number into rax, then syscall instruction
        std::string asmTemplate = "movq $" + std::to_string(syscallNum) + ", %rax\\n\\tsyscall";
        
        // Create inline assembly
        InlineAsm* inlineAsm = InlineAsm::get(
            asmFuncType,
            asmTemplate,
            constraints,
            true,  // hasSideEffects
            false, // isAlignStack
            InlineAsm::AD_ATT  // AT&T syntax
        );
        
        // Call inline assembly with arguments
        Value* result = ctx.builder->CreateCall(inlineAsm, args, "syscall_result");
        
        // Cast result to expected return type if needed
        if (returnType->isIntegerTy() && returnType != Type::getInt64Ty(ctx.llvmContext)) {
            result = ctx.builder->CreateIntCast(result, returnType, true, "syscall_cast");
        }
        
        return result;
    }
    
    // emitAllocExec - Allocate executable memory using mmap syscall
    // Syscall: mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0)
    // Returns i8* to RW memory (caller transitions to RX later via emitProtectExec)
    Value* emitAllocExec(Value* size) {
        // mmap syscall number on x86-64 Linux
        const uint64_t SYSCALL_MMAP = 9;
        
        // mmap constants
        const uint64_t PROT_READ = 1;
        const uint64_t PROT_WRITE = 2;
        const uint64_t MAP_PRIVATE = 0x02;
        const uint64_t MAP_ANONYMOUS = 0x20;
        
        // Build syscall arguments
        std::vector<Value*> mmapArgs;
        mmapArgs.push_back(ConstantInt::get(Type::getInt64Ty(ctx.llvmContext), 0));  // addr = NULL
        mmapArgs.push_back(size);  // length = size
        mmapArgs.push_back(ConstantInt::get(Type::getInt64Ty(ctx.llvmContext), PROT_READ | PROT_WRITE));
        mmapArgs.push_back(ConstantInt::get(Type::getInt64Ty(ctx.llvmContext), MAP_PRIVATE | MAP_ANONYMOUS));
        mmapArgs.push_back(ConstantInt::get(Type::getInt64Ty(ctx.llvmContext), (uint64_t)-1));  // fd = -1
        mmapArgs.push_back(ConstantInt::get(Type::getInt64Ty(ctx.llvmContext), 0));  // offset = 0
        
        // Invoke mmap syscall
        Value* result = createSyscall(SYSCALL_MMAP, mmapArgs);
        
        // mmap returns i64, convert to i8* (opaque pointer in LLVM 18)
        return ctx.builder->CreateIntToPtr(result, PointerType::getUnqual(ctx.llvmContext), "alloc_exec_ptr");
    }
    
    // emitProtectExec - Transition memory from RW to RX using mprotect syscall
    // Syscall: mprotect(addr, size, PROT_READ|PROT_EXEC)
    // Used to finalize wildx memory after JIT compilation
    void emitProtectExec(Value* addr, Value* size) {
        // mprotect syscall number on x86-64 Linux
        const uint64_t SYSCALL_MPROTECT = 10;
        
        // mprotect constants
        const uint64_t PROT_READ = 1;
        const uint64_t PROT_EXEC = 4;
        
        // Convert pointer to i64 for syscall
        Value* addrInt = ctx.builder->CreatePtrToInt(addr, Type::getInt64Ty(ctx.llvmContext));
        
        // Build syscall arguments
        std::vector<Value*> mprotectArgs;
        mprotectArgs.push_back(addrInt);  // addr
        mprotectArgs.push_back(size);     // length
        mprotectArgs.push_back(ConstantInt::get(Type::getInt64Ty(ctx.llvmContext), PROT_READ | PROT_EXEC));
        
        // Invoke mprotect syscall (return value ignored - would be 0 on success, -1 on error)
        createSyscall(SYSCALL_MPROTECT, mprotectArgs);
    }
    
    // getOrInsertAriaAlloc - Declare aria.alloc (standard heap via mimalloc)
    Function* getOrInsertAriaAlloc() {
        if (Function* existing = ctx.module->getFunction("aria.alloc")) {
            return existing;
        }
        
        FunctionType* allocType = FunctionType::get(
            PointerType::getUnqual(ctx.llvmContext),  // returns ptr
            {Type::getInt64Ty(ctx.llvmContext)},       // size argument
            false
        );
        
        return Function::Create(allocType, Function::ExternalLinkage, "aria.alloc", ctx.module.get());
    }
    
    // getOrInsertAriaAllocExec - Declare aria.alloc_exec (executable heap for wildx)
    // NOW IMPLEMENTED: Uses mmap syscall to allocate RW memory
    Function* getOrInsertAriaAllocExec() {
        if (Function* existing = ctx.module->getFunction("aria.alloc_exec")) {
            return existing;
        }
        
        // Create function signature: i8* aria.alloc_exec(i64 size)
        FunctionType* allocExecType = FunctionType::get(
            PointerType::getUnqual(ctx.llvmContext),
            {Type::getInt64Ty(ctx.llvmContext)},
            false
        );
        
        Function* func = Function::Create(
            allocExecType,
            Function::InternalLinkage,  // Internal - we define it here
            "aria.alloc_exec",
            ctx.module.get()
        );
        
        // Generate function body using emitAllocExec
        BasicBlock* entry = BasicBlock::Create(ctx.llvmContext, "entry", func);
        IRBuilder<> tmpBuilder(entry);
        
        // Get size parameter
        Value* size = func->getArg(0);
        
        // Call emitAllocExec (uses mmap syscall)
        // Temporarily swap builder for syscall generation
        std::unique_ptr<IRBuilder<>> savedBuilder = std::move(ctx.builder);
        ctx.builder = std::make_unique<IRBuilder<>>(entry);
        Value* ptr = emitAllocExec(size);
        ctx.builder = std::move(savedBuilder);
        
        tmpBuilder.CreateRet(ptr);
        
        return func;
    }
    
    // getOrInsertGetNursery - Declare aria.get_nursery (GC arena)
    Function* getOrInsertGetNursery() {
        if (Function* existing = ctx.module->getFunction("aria.get_nursery")) {
            return existing;
        }
        
        FunctionType* nurseryType = FunctionType::get(
            PointerType::getUnqual(ctx.llvmContext),
            {},
            false
        );
        
        return Function::Create(nurseryType, Function::ExternalLinkage, "aria.get_nursery", ctx.module.get());
    }
    
    // getOrInsertGCAlloc - Declare aria.gc_alloc (GC allocation)
    Function* getOrInsertGCAlloc() {
        if (Function* existing = ctx.module->getFunction("aria.gc_alloc")) {
            return existing;
        }
        
        FunctionType* gcAllocType = FunctionType::get(
            PointerType::getUnqual(ctx.llvmContext),
            {PointerType::getUnqual(ctx.llvmContext), Type::getInt64Ty(ctx.llvmContext)},
            false
        );
        
        return Function::Create(gcAllocType, Function::ExternalLinkage, "aria.gc_alloc", ctx.module.get());
    }
    
    // getOrInsertAriaMemProtectExec - Declare aria_mem_protect_exec (make memory executable)
    Function* getOrInsertAriaMemProtectExec() {
        if (Function* existing = ctx.module->getFunction("aria_mem_protect_exec")) {
            return existing;
        }
        
        FunctionType* protectType = FunctionType::get(
            Type::getInt32Ty(ctx.llvmContext),  // Returns 0 on success
            {PointerType::getUnqual(ctx.llvmContext), Type::getInt64Ty(ctx.llvmContext)},
            false
        );
        
        return Function::Create(protectType, Function::ExternalLinkage, "aria_mem_protect_exec", ctx.module.get());
    }
    
    // getOrInsertAriaMemProtectWrite - Declare aria_mem_protect_write (make memory writable)
    Function* getOrInsertAriaMemProtectWrite() {
        if (Function* existing = ctx.module->getFunction("aria_mem_protect_write")) {
            return existing;
        }
        
        FunctionType* protectType = FunctionType::get(
            Type::getInt32Ty(ctx.llvmContext),  // Returns 0 on success
            {PointerType::getUnqual(ctx.llvmContext), Type::getInt64Ty(ctx.llvmContext)},
            false
        );
        
        return Function::Create(protectType, Function::ExternalLinkage, "aria_mem_protect_write", ctx.module.get());
    }
    
    // getOrInsertAriaFreeExec - Declare aria_free_exec (free executable memory)
    Function* getOrInsertAriaFreeExec() {
        if (Function* existing = ctx.module->getFunction("aria_free_exec")) {
            return existing;
        }
        
        FunctionType* freeType = FunctionType::get(
            Type::getVoidTy(ctx.llvmContext),
            {PointerType::getUnqual(ctx.llvmContext)},
            false
        );
        
        return Function::Create(freeType, Function::ExternalLinkage, "aria_free_exec", ctx.module.get());
    }
    
    // declareLLVMIntrinsic - Declare LLVM math intrinsics (llvm.sin.f32, etc.)
    Function* declareLLVMIntrinsic(const std::string& name) {
        // Parse intrinsic name: llvm_<name>_<type>
        std::string intrName;
        Type* argType = nullptr;
        
        if (name.find("_f32") != std::string::npos) {
            argType = Type::getFloatTy(ctx.llvmContext);
            intrName = name.substr(5, name.length() - 9); // Remove "llvm_" prefix and "_f32" suffix
        } else if (name.find("_f64") != std::string::npos) {
            argType = Type::getDoubleTy(ctx.llvmContext);
            intrName = name.substr(5, name.length() - 9); // Remove "llvm_" prefix and "_f64" suffix
        } else {
            return nullptr;  // Unknown type
        }
        
        // Map intrinsic names to LLVM Intrinsic IDs
        Intrinsic::ID intrID = Intrinsic::not_intrinsic;
        
        if (intrName == "sin") intrID = Intrinsic::sin;
        else if (intrName == "cos") intrID = Intrinsic::cos;
        else if (intrName == "exp") intrID = Intrinsic::exp;
        else if (intrName == "exp2") intrID = Intrinsic::exp2;
        else if (intrName == "log") intrID = Intrinsic::log;
        else if (intrName == "log2") intrID = Intrinsic::log2;
        else if (intrName == "log10") intrID = Intrinsic::log10;
        else if (intrName == "sqrt") intrID = Intrinsic::sqrt;
        else if (intrName == "ceil") intrID = Intrinsic::ceil;
        else if (intrName == "floor") intrID = Intrinsic::floor;
        else if (intrName == "round") intrID = Intrinsic::round;
        else if (intrName == "trunc") intrID = Intrinsic::trunc;
        else if (intrName == "fabs") intrID = Intrinsic::fabs;
        else if (intrName == "minnum") intrID = Intrinsic::minnum;
        else if (intrName == "maxnum") intrID = Intrinsic::maxnum;
        else if (intrName == "copysign") intrID = Intrinsic::copysign;
        else if (intrName == "pow") intrID = Intrinsic::pow;
        else if (intrName == "fma") intrID = Intrinsic::fma;
        else {
            return nullptr;  // Unknown intrinsic
        }
        
        // Declare the intrinsic
        return Intrinsic::getDeclaration(ctx.module.get(), intrID, {argType});
    }

    // -------------------------------------------------------------------------
    // 1. Variable Declarations
    // -------------------------------------------------------------------------
    
    // Helper: Generate lambda function body (for recursive function support)
    void generateLambdaBody(aria::frontend::LambdaExpr* lambda, Function* func) {
        // Set parameter names
        unsigned idx = 0;
        for (auto& arg : func->args()) {
            if (idx < lambda->parameters.size()) {
                arg.setName(lambda->parameters[idx++].name);
            }
        }
        
        // Create entry basic block
        BasicBlock* entry = BasicBlock::Create(ctx.llvmContext, "entry", func);
        
        // Save previous state
        Function* prevFunc = ctx.currentFunction;
        BasicBlock* prevBlock = ctx.builder->GetInsertBlock();
        std::string prevReturnType = ctx.currentFunctionReturnType;
        bool prevAutoWrap = ctx.currentFunctionAutoWrap;
        
        ctx.currentFunction = func;
        ctx.currentFunctionReturnType = lambda->return_type;
        ctx.currentFunctionAutoWrap = lambda->auto_wrap;
        ctx.builder->SetInsertPoint(entry);
        
        // Clear defer stacks for new function
        ctx.deferStacks = std::vector<std::vector<Block*>>();
        ctx.deferStacks.emplace_back();
        
        // Create allocas for parameters
        std::vector<std::pair<std::string, CodeGenContext::Symbol*>> savedSymbols;
        
        idx = 0;
        for (auto& arg : func->args()) {
            Type* argType = arg.getType();
            AllocaInst* alloca = ctx.builder->CreateAlloca(argType, nullptr, arg.getName());
            ctx.builder->CreateStore(&arg, alloca);
            
            std::string argName = std::string(arg.getName());
            auto* existingSym = ctx.lookup(argName);
            if (existingSym) {
                savedSymbols.push_back({argName, existingSym});
            }
            
            // Store parameter with its Aria type from the lambda parameter list
            std::string paramAriaType = lambda->parameters[idx].type;
            ctx.define(argName, alloca, true, paramAriaType);
            idx++; // Increment for next parameter
        }
        
        // Generate lambda body
        if (lambda->body) {
            lambda->body->accept(*this);
        }
        
        // Add return if missing
        if (ctx.builder->GetInsertBlock()->getTerminator() == nullptr) {
            // Execute all defers before implicit return (LIFO order)
            if (!ctx.deferStacks.empty() && !ctx.deferStacks[0].empty()) {
                for (auto it = ctx.deferStacks[0].rbegin(); it != ctx.deferStacks[0].rend(); ++it) {
                    (*it)->accept(*this);
                }
            }
            
            Type* returnType = func->getReturnType();
            if (returnType->isVoidTy()) {
                ctx.builder->CreateRetVoid();
            } else {
                ctx.builder->CreateRet(Constant::getNullValue(returnType));
            }
        }
        
        // Restore previous symbols
        for (auto& pair : savedSymbols) {
            ctx.define(pair.first, pair.second->val, pair.second->is_ref);
        }
        
        // Restore previous function context
        ctx.currentFunction = prevFunc;
        ctx.currentFunctionReturnType = prevReturnType;
        ctx.currentFunctionAutoWrap = prevAutoWrap;
        if (prevBlock) {
            ctx.builder->SetInsertPoint(prevBlock);
        }
    }
    
    // Helper: Determine if a type should be stack-allocated by default
    bool shouldStackAllocate(const std::string& type, llvm::Type* llvmType) {
        // Primitives that fit in registers should be stack-allocated
        if (type == "int8" || type == "int16" || type == "int32" || type == "int64" ||
            type == "uint8" || type == "uint16" || type == "uint32" || type == "uint64" ||
            type == "bool" || type == "trit" || type == "char") {
            return true;
        }
        
        // Floating point primitives
        if (type == "float" || type == "double" || type == "float32" || type == "float64") {
            return true;
        }
        
        // Small aggregate types (< 128 bytes) can be stack-allocated
        if (llvmType->isSized()) {
            uint64_t size = ctx.module->getDataLayout().getTypeAllocSize(llvmType);
            if (size > 0 && size <= 128) {
                return true;
            }
        }
        
        return false;
    }

    void visit(VarDecl* node) override {
        // Special case: Function variables (type="func" or "func<signature>" with Lambda initializer)
        if ((node->type == "func" || node->type.find("func<") == 0) && node->initializer) {
            if (auto* lambda = dynamic_cast<aria::frontend::LambdaExpr*>(node->initializer.get())) {
                // IMPORTANT: We need to pre-declare the function in the symbol table
                // BEFORE generating the lambda body, so recursive calls can find it.
                
                // Create function type
                std::vector<Type*> paramTypes;
                for (auto& param : lambda->parameters) {
                    paramTypes.push_back(ctx.getLLVMType(param.type));
                }
                Type* returnType = ctx.getResultType(lambda->return_type);
                FunctionType* funcType = FunctionType::get(returnType, paramTypes, false);
                
                // Create function with module prefix if in a module
                std::string funcName = ctx.currentModulePrefix + node->name;
                
                // Create function with the FINAL name (not lambda_N)
                Function* func = Function::Create(
                    funcType,
                    Function::InternalLinkage,
                    funcName,
                    ctx.module.get()
                );
                
                // Register IMMEDIATELY so recursive calls can find it
                ctx.define(node->name, func, false, node->type);
                
                // Now generate the lambda body (visitExpr will fill in the function)
                // We pass the pre-created function to be filled
                generateLambdaBody(lambda, func);
                
                return;
            }
        }
        
        // Early return if initializer is a function (for func pointer variables)
        // This avoids allocating storage when we just want to store the Function* directly
        if ((node->type == "func" || node->type.find("func<") == 0) && node->initializer) {
            Value* initVal = visitExpr(node->initializer.get());
            if (initVal && isa<Function>(initVal)) {
                // Store function directly in symbol table (no memory allocation)
                ctx.define(node->name, initVal, false, node->type, CodeGenContext::AllocStrategy::VALUE);
                return;
            }
        }
        
        llvm::Type* varType = ctx.getLLVMType(node->type);
        Value* storage = nullptr;
        bool is_ref = true; // Whether we need to load from storage

        // Check if this is a module-level (global) variable
        bool isModuleScope = (ctx.currentFunction && 
                             ctx.currentFunction->getName() == "__aria_module_init");

        // Module-level variables must be GlobalVariables for closure capture
        if (isModuleScope && !node->is_wild && !node->is_stack) {
            // Create a GlobalVariable instead of alloca
            Constant* initializer = nullptr;
            if (node->initializer) {
                // Evaluate constant initializer if possible
                Value* initVal = visitExpr(node->initializer.get());
                if (auto* constVal = dyn_cast<Constant>(initVal)) {
                    // Convert constant to correct type if needed
                    if (constVal->getType() != varType) {
                        // Try to convert via truncation/extension
                        if (constVal->getType()->isIntegerTy() && varType->isIntegerTy()) {
                            if (auto* constInt = dyn_cast<ConstantInt>(constVal)) {
                                initializer = ConstantInt::get(varType, constInt->getZExtValue());
                            } else {
                                initializer = Constant::getNullValue(varType);
                            }
                        } else {
                            initializer = Constant::getNullValue(varType);
                        }
                    } else {
                        initializer = constVal;
                    }
                } else {
                    // Non-constant initializer - use zero init then assign
                    initializer = Constant::getNullValue(varType);
                }
            } else {
                initializer = Constant::getNullValue(varType);
            }
            
            GlobalVariable* global = new GlobalVariable(
                *ctx.module,
                varType,
                false, // not constant
                GlobalValue::InternalLinkage,
                initializer,
                node->name
            );
            
            storage = global;
            ctx.define(node->name, storage, is_ref, node->type, CodeGenContext::AllocStrategy::VALUE);
            
            // If initializer was non-constant, store it now
            if (node->initializer) {
                Value* initVal = visitExpr(node->initializer.get());
                if (!isa<Constant>(initVal)) {
                    // Convert type if needed
                    if (initVal->getType() != varType) {
                        if (initVal->getType()->isIntegerTy() && varType->isIntegerTy()) {
                            initVal = ctx.builder->CreateTrunc(initVal, varType);
                        }
                    }
                    ctx.builder->CreateStore(initVal, global);
                }
            }
            return;
        }

        // Determine allocation strategy for local variables
        bool use_stack = node->is_stack || (!node->is_wild && !node->is_wildx && shouldStackAllocate(node->type, varType));

        CodeGenContext::AllocStrategy strategy;
        
        if (use_stack) {
            // Stack: Simple Alloca (for explicit `stack` keyword or auto-promoted primitives)
            // Insert at entry block for efficiency
            BasicBlock* currentBB = ctx.builder->GetInsertBlock();
            Function* func = currentBB->getParent();
            IRBuilder<> tmpBuilder(&func->getEntryBlock(), func->getEntryBlock().begin());
            storage = tmpBuilder.CreateAlloca(varType, nullptr, node->name);
            strategy = CodeGenContext::AllocStrategy::STACK;
        } 
        else if (node->is_wild) {
            // Wild: aria_alloc (standard heap allocation via mimalloc)
            uint64_t size = ctx.module->getDataLayout().getTypeAllocSize(varType);
            Value* sizeVal = ConstantInt::get(Type::getInt64Ty(ctx.llvmContext), size);
            
            Function* allocator = getOrInsertAriaAlloc();
            Value* rawPtr = ctx.builder->CreateCall(allocator, sizeVal);
            
            // We need a stack slot to hold the pointer itself (lvalue)
            storage = ctx.builder->CreateAlloca(PointerType::getUnqual(ctx.llvmContext), nullptr, node->name);
            ctx.builder->CreateStore(rawPtr, storage);
            strategy = CodeGenContext::AllocStrategy::WILD;
        }
        else if (node->is_wildx) {
            // Wildx: aria_alloc_exec (executable memory via mmap)
            // Allocates RW memory that can be transitioned to RX for JIT compilation
            uint64_t size = ctx.module->getDataLayout().getTypeAllocSize(varType);
            Value* sizeVal = ConstantInt::get(Type::getInt64Ty(ctx.llvmContext), size);
            
            Function* allocator = getOrInsertAriaAllocExec();
            Value* rawPtr = ctx.builder->CreateCall(allocator, sizeVal);
            
            // We need a stack slot to hold the pointer itself (lvalue)
            storage = ctx.builder->CreateAlloca(PointerType::getUnqual(ctx.llvmContext), nullptr, node->name);
            ctx.builder->CreateStore(rawPtr, storage);
            strategy = CodeGenContext::AllocStrategy::WILDX; // WILDX strategy for executable memory
        }
        else {
            // GC: aria_gc_alloc (for non-primitives or explicitly marked gc)
            // 1. Get Nursery
            Function* getNursery = getOrInsertGetNursery();
            Value* nursery = ctx.builder->CreateCall(getNursery, {});
            
            // 2. Alloc
            uint64_t size = ctx.module->getDataLayout().getTypeAllocSize(varType);
            Value* sizeVal = ConstantInt::get(Type::getInt64Ty(ctx.llvmContext), size);
            
            Function* allocator = getOrInsertGCAlloc();
            Value* gcPtr = ctx.builder->CreateCall(allocator, {nursery, sizeVal});
            
            // Store pointer
            storage = ctx.builder->CreateAlloca(PointerType::getUnqual(ctx.llvmContext), nullptr, node->name);
            ctx.builder->CreateStore(gcPtr, storage);
            strategy = CodeGenContext::AllocStrategy::GC;
        }

        ctx.define(node->name, storage, is_ref, node->type, strategy);

        // Initializer
        if (node->initializer) {
            Value* initVal = visitExpr(node->initializer.get());
            if (!initVal) return;

            // Special case: func type variables store Function* directly, not in memory
            // This includes both "func" and "func<signature>" types
            if ((node->type == "func" || node->type.find("func<") == 0) && isa<Function>(initVal)) {
                // Update symbol table to store function directly (not as a reference)
                ctx.define(node->name, initVal, false, node->type, CodeGenContext::AllocStrategy::VALUE);
                return;  // Don't store to memory
            }

            // Convert initVal to match variable type if needed
            if (initVal->getType() != varType) {
                if (initVal->getType()->isIntegerTy() && varType->isIntegerTy()) {
                    // Truncate or extend to match variable type
                    initVal = ctx.builder->CreateIntCast(initVal, varType, true);
                }
            }

            if (use_stack) {
                // Direct store for stack-allocated variables
                ctx.builder->CreateStore(initVal, storage);
            } else {
                // For heap vars, 'storage' is `ptr*`, we load the `ptr` then store to it
                Value* heapPtr = ctx.builder->CreateLoad(PointerType::getUnqual(ctx.llvmContext), storage);
                // Cast heapPtr if necessary (though LLVM 18 ptr is opaque)
                ctx.builder->CreateStore(initVal, heapPtr);
            }
        }
    }

    void visit(StructDecl* node) override {
        // Register struct type in LLVM
        // In Aria, structs are value types defined with: const StructName = struct { fields... };
        // We create an LLVM StructType and register it for later use
        
        // Check if type already exists (avoid duplicates)
        if (StructType::getTypeByName(ctx.llvmContext, node->name)) {
            throw std::runtime_error("Struct type '" + node->name + "' already defined");
        }
        
        // Create field types and store field name mappings
        std::vector<Type*> fieldTypes;
        std::map<std::string, unsigned> fieldMap;
        unsigned fieldIndex = 0;
        
        for (const auto& field : node->fields) {
            Type* fieldType = ctx.getLLVMType(field.type);
            fieldTypes.push_back(fieldType);
            fieldMap[field.name] = fieldIndex++;
        }
        
        // Create named struct type
        StructType::create(ctx.llvmContext, fieldTypes, node->name);
        
        // Store field mapping for later use in member access
        ctx.structFieldMaps[node->name] = fieldMap;
        
        // Note: We don't need to add to symbol table - struct types are looked up by name
        // The type system will resolve "Point" to the LLVM StructType when needed
    }

    void visit(ExpressionStmt* node) override {
        // Execute expression for side effects (e.g., function call)
        visitExpr(node->expression.get());
    }

    void visit(FuncDecl* node) override {
        // 1. Create function type with optimized parameter passing
        std::vector<Type*> paramTypes;
        std::vector<bool> shouldScalarize;  // Track which params to scalarize
        std::vector<std::vector<Type*>> scalarizedTypes;  // Scalarized field types
        
        for (auto& param : node->parameters) {
            Type* paramType = ctx.getLLVMType(param.type);
            
            // ================================================================
            // OPTIMIZATION: Small Struct Parameter Passing
            // ================================================================
            // System V AMD64 ABI allows passing small structs in registers:
            // - Structs ≤ 16 bytes can be passed in up to 2 registers
            // - We scalarize structs into individual fields for optimal codegen
            //
            // Example: struct Vec3 { float x, y, z; }  (12 bytes)
            // Before: Pass pointer, load 3 floats from memory (L1 cache miss)
            // After: Pass x, y, z in XMM0, XMM1, XMM2 registers directly
            // ================================================================
            
            bool shouldOptimize = false;
            std::vector<Type*> fieldTypes;
            
            if (paramType->isStructTy()) {
                auto* structType = cast<StructType>(paramType);
                
                // Check if struct is small enough to optimize
                if (structType->isSized()) {
                    uint64_t structSize = ctx.module->getDataLayout().getTypeAllocSize(structType);
                    
                    // Optimize structs ≤ 16 bytes (fits in 2 registers)
                    if (structSize > 0 && structSize <= 16) {
                        // Scalarize: pass each field as separate parameter
                        for (unsigned i = 0; i < structType->getNumElements(); ++i) {
                            Type* fieldType = structType->getElementType(i);
                            fieldTypes.push_back(fieldType);
                            paramTypes.push_back(fieldType);
                        }
                        shouldOptimize = true;
                    }
                }
            }
            
            if (!shouldOptimize) {
                // Standard parameter: pass as-is
                paramTypes.push_back(paramType);
                shouldScalarize.push_back(false);
                scalarizedTypes.push_back({});
            } else {
                // Optimized parameter: scalarized
                shouldScalarize.push_back(true);
                scalarizedTypes.push_back(fieldTypes);
            }
        }
        
        Type* returnType = ctx.getLLVMType(node->return_type);
        
        // ASYNC FUNCTION SUPPORT (Bug #70)
        // Async functions return a coroutine handle (i8*) instead of their declared type
        if (node->is_async) {
            returnType = PointerType::getUnqual(ctx.llvmContext);  // i8* for coroutine handle
        }
        
        FunctionType* funcType = FunctionType::get(returnType, paramTypes, false);
        
        // 2. Create function with module prefix
        std::string funcName = ctx.currentModulePrefix + node->name;
        
        Function* func = Function::Create(
            funcType,
            Function::ExternalLinkage,
            funcName,
            ctx.module.get()
        );
        
        // Register function in symbol table so it can be found later
        ctx.define(node->name, func, false);  // false = not a reference/pointer
        
        // 3. Set parameter names
        unsigned idx = 0;
        for (auto& arg : func->args()) {
            arg.setName(node->parameters[idx++].name);
        }
        
        // 4. Create entry basic block
        BasicBlock* entry = BasicBlock::Create(ctx.llvmContext, "entry", func);
        
        // 5. Save previous state and set new function context
        Function* prevFunc = ctx.currentFunction;
        BasicBlock* prevBlock = ctx.builder->GetInsertBlock();
        bool prevAutoWrap = ctx.currentFunctionAutoWrap;
        std::string prevReturnType = ctx.currentFunctionReturnType;
        
        ctx.currentFunction = func;
        ctx.builder->SetInsertPoint(entry);
        
        // Clear defer stacks for new function (defers don't persist across functions)
        ctx.deferStacks = std::vector<std::vector<Block*>>();
        ctx.deferStacks.emplace_back();  // Start with one scope for the function
        
        // SPEC REQUIREMENT: ALL functions return Result objects - enable auto-wrap by default
        // "NO BYPASSING THIS!!! NO REGULAR VALUE RETURNS ALLOWED!!!!" - Section 8.4
        ctx.currentFunctionAutoWrap = true;
        ctx.currentFunctionReturnType = node->return_type;
        
        // ASYNC FUNCTION COROUTINE SETUP
        if (node->is_async) {
            // Step 1: Generate coroutine ID
            Function* coroId = Intrinsic::getDeclaration(ctx.module.get(), Intrinsic::coro_id);
            Value* id = ctx.builder->CreateCall(coroId, {
                ConstantInt::get(Type::getInt32Ty(ctx.llvmContext), 0),
                ConstantPointerNull::get(PointerType::getUnqual(ctx.llvmContext)),
                ConstantPointerNull::get(PointerType::getUnqual(ctx.llvmContext)),
                ConstantPointerNull::get(PointerType::getUnqual(ctx.llvmContext))
            }, "coro_id");
            
            // Step 2: Get coroutine frame size
            Function* coroSize = Intrinsic::getDeclaration(ctx.module.get(), Intrinsic::coro_size, {Type::getInt64Ty(ctx.llvmContext)});
            Value* frameSize = ctx.builder->CreateCall(coroSize, {}, "coro_size");
            
            // Step 3: Check if heap allocation is needed (coro.alloc intrinsic)
            Function* coroAlloc = Intrinsic::getDeclaration(ctx.module.get(), Intrinsic::coro_alloc);
            Value* needAlloc = ctx.builder->CreateCall(coroAlloc, {id}, "coro_need_alloc");
            
            // Step 4: Conditionally allocate frame memory via aria.alloc
            BasicBlock* allocBB = BasicBlock::Create(ctx.llvmContext, "coro_alloc", func);
            BasicBlock* beginBB = BasicBlock::Create(ctx.llvmContext, "coro_begin", func);
            ctx.builder->CreateCondBr(needAlloc, allocBB, beginBB);
            
            // Allocation branch
            ctx.builder->SetInsertPoint(allocBB);
            Function* ariaAlloc = getOrInsertAriaAlloc();
            Value* allocMem = ctx.builder->CreateCall(ariaAlloc, {frameSize}, "coro_alloc_mem");
            ctx.builder->CreateBr(beginBB);
            
            // Begin coroutine with allocated frame (or NULL if elided)
            ctx.builder->SetInsertPoint(beginBB);
            PHINode* framePhi = ctx.builder->CreatePHI(PointerType::getUnqual(ctx.llvmContext), 2, "coro_frame");
            framePhi->addIncoming(allocMem, allocBB);
            framePhi->addIncoming(ConstantPointerNull::get(PointerType::getUnqual(ctx.llvmContext)), entry);
            
            Function* coroBegin = Intrinsic::getDeclaration(ctx.module.get(), Intrinsic::coro_begin);
            Value* hdl = ctx.builder->CreateCall(coroBegin, {id, framePhi}, "coro_hdl");
            
            // Store handle for use in return statements
            AllocaInst* coroHandleAlloca = ctx.builder->CreateAlloca(
                PointerType::getUnqual(ctx.llvmContext), nullptr, "__coro_handle");
            ctx.builder->CreateStore(hdl, coroHandleAlloca);
        }
        
        // 6. Create allocas for parameters (to allow taking addresses)
        // For scalarized struct parameters, reconstruct the struct from individual fields
        idx = 0;
        size_t argIdx = 0;  // Track position in func->args() (may not match param index due to scalarization)
        
        for (size_t paramIdx = 0; paramIdx < node->parameters.size(); ++paramIdx) {
            auto& param = node->parameters[paramIdx];
            
            if (paramIdx < shouldScalarize.size() && shouldScalarize[paramIdx]) {
                // ============================================================
                // SCALARIZED STRUCT PARAMETER
                // ============================================================
                // This parameter was scalarized into multiple arguments
                // Reconstruct the original struct from the fields
                
                Type* originalType = ctx.getLLVMType(param.type);
                auto* structType = cast<StructType>(originalType);
                
                // Create alloca for the reconstructed struct
                AllocaInst* structAlloca = ctx.builder->CreateAlloca(
                    structType, nullptr, param.name + "_reconstructed");
                
                // Copy each field from the scalarized arguments into the struct
                for (unsigned fieldIdx = 0; fieldIdx < scalarizedTypes[paramIdx].size(); ++fieldIdx) {
                    // Get the scalarized argument
                    auto argIter = func->arg_begin();
                    std::advance(argIter, argIdx + fieldIdx);
                    Value* fieldArg = &*argIter;
                    
                    // Set argument name for debugging
                    fieldArg->setName(param.name + "_field" + std::to_string(fieldIdx));
                    
                    // Get pointer to struct field
                    Value* fieldPtr = ctx.builder->CreateStructGEP(
                        structType, structAlloca, fieldIdx,
                        param.name + "_field" + std::to_string(fieldIdx) + "_ptr");
                    
                    // Store the argument value into the struct field
                    ctx.builder->CreateStore(fieldArg, fieldPtr);
                }
                
                // Register the reconstructed struct in symbol table
                ctx.define(param.name, structAlloca, true, param.type);
                
                // Advance argument index by number of fields
                argIdx += scalarizedTypes[paramIdx].size();
            } else {
                // ============================================================
                // NORMAL PARAMETER (not scalarized)
                // ============================================================
                auto argIter = func->arg_begin();
                std::advance(argIter, argIdx);
                Value* arg = &*argIter;
                
                arg->setName(param.name);
                
                Type* argType = arg->getType();
                AllocaInst* alloca = ctx.builder->CreateAlloca(argType, nullptr, param.name);
                ctx.builder->CreateStore(arg, alloca);
                ctx.define(param.name, alloca, true, param.type);
                
                argIdx++;
            }
        }
        
        // 7. Generate function body
        if (node->body) {
            node->body->accept(*this);
        }
        
        // 8. Add return if missing (for void functions)
        if (ctx.builder->GetInsertBlock()->getTerminator() == nullptr) {
            if (node->is_async) {
                // Async functions: emit final suspend and return handle
                Function* coroSuspend = Intrinsic::getDeclaration(ctx.module.get(), Intrinsic::coro_suspend);
                Value* suspendResult = ctx.builder->CreateCall(coroSuspend, {
                    ConstantTokenNone::get(ctx.llvmContext),
                    ConstantInt::get(Type::getInt1Ty(ctx.llvmContext), 1)  // Final suspend
                }, "final_suspend");
                
                // Create suspend switch
                BasicBlock* suspendBB = BasicBlock::Create(ctx.llvmContext, "suspend", func);
                BasicBlock* destroyBB = BasicBlock::Create(ctx.llvmContext, "destroy", func);
                SwitchInst* suspendSwitch = ctx.builder->CreateSwitch(suspendResult, suspendBB, 2);
                suspendSwitch->addCase(ConstantInt::get(Type::getInt8Ty(ctx.llvmContext), 1), destroyBB);
                
                // Destroy path
                ctx.builder->SetInsertPoint(destroyBB);
                Function* coroEnd = Intrinsic::getDeclaration(ctx.module.get(), Intrinsic::coro_end);
                Value* hdl = ctx.builder->CreateLoad(PointerType::getUnqual(ctx.llvmContext),
                    func->getEntryBlock().getTerminator()->getPrevNode(), "coro_hdl");
                ctx.builder->CreateCall(coroEnd, {
                    hdl,
                    ConstantInt::get(Type::getInt1Ty(ctx.llvmContext), 0)
                });
                ctx.builder->CreateRet(hdl);
                
                // Suspend path (unreachable)
                ctx.builder->SetInsertPoint(suspendBB);
                ctx.builder->CreateUnreachable();
            } else if (returnType->isVoidTy()) {
                ctx.builder->CreateRetVoid();
            } else {
                // Return default value (0 or nullptr)
                ctx.builder->CreateRet(Constant::getNullValue(returnType));
            }
        }
        
        // 9. Restore previous function context and insertion point
        ctx.currentFunction = prevFunc;
        ctx.currentFunctionAutoWrap = prevAutoWrap;
        ctx.currentFunctionReturnType = prevReturnType;
        if (prevBlock) {
            ctx.builder->SetInsertPoint(prevBlock);
        }
    }

    // -------------------------------------------------------------------------
    // 2. Control Flow: Pick & Loops
    // -------------------------------------------------------------------------

    void visit(PickStmt* node) override {
        Value* selector = visitExpr(node->selector.get());
        Function* func = ctx.builder->GetInsertBlock()->getParent();
        BasicBlock* doneBB = BasicBlock::Create(ctx.llvmContext, "pick_done");
        
        // Build label map for fall() targets
        std::map<std::string, BasicBlock*> labelBlocks;
        
        // First pass: create labeled blocks
        for (size_t i = 0; i < node->cases.size(); ++i) {
            auto& pcase = node->cases[i];
            if (!pcase.label.empty()) {
                BasicBlock* labelBB = BasicBlock::Create(ctx.llvmContext, "pick_label_" + pcase.label, func);
                labelBlocks[pcase.label] = labelBB;
            }
        }
        
        // Store label blocks in context for fall statements
        ctx.pickLabelBlocks = &labelBlocks;
        ctx.pickDoneBlock = doneBB;
        
        // =====================================================================
        // OPTIMIZATION: Group consecutive EXACT integer cases into SwitchInst
        // This converts O(N) linear search into O(1) jump table
        // =====================================================================
        
        // Check if we can use switch optimization
        bool canUseSwitch = selector->getType()->isIntegerTy();
        
        // Scan for consecutive EXACT cases at the start
        size_t exactCaseCount = 0;
        if (canUseSwitch) {
            for (size_t i = 0; i < node->cases.size(); ++i) {
                auto& pcase = node->cases[i];
                if (pcase.type == PickCase::EXACT && pcase.label.empty()) {
                    // Check if value is a constant (required for switch)
                    Value* caseVal = visitExpr(pcase.value_start.get());
                    if (isa<ConstantInt>(caseVal)) {
                        exactCaseCount = i + 1;
                    } else {
                        // Non-constant value - cannot use switch for remaining cases
                        break;
                    }
                } else {
                    // Different case type - stop grouping
                    break;
                }
            }
        }
        
        // If we have 3+ consecutive EXACT constant cases, use SwitchInst
        if (exactCaseCount >= 3) {
            // Create switch instruction
            BasicBlock* defaultBB = BasicBlock::Create(ctx.llvmContext, "switch_default", func);
            SwitchInst* switchInst = ctx.builder->CreateSwitch(selector, defaultBB, exactCaseCount);
            
            // Add each exact case to the switch
            for (size_t i = 0; i < exactCaseCount; ++i) {
                auto& pcase = node->cases[i];
                Value* caseVal = visitExpr(pcase.value_start.get());
                auto* caseConst = cast<ConstantInt>(caseVal);
                
                // Create case body block
                BasicBlock* caseBodyBB = BasicBlock::Create(
                    ctx.llvmContext, 
                    "switch_case_" + std::to_string(i), 
                    func
                );
                
                // Add to switch
                switchInst->addCase(caseConst, caseBodyBB);
                
                // Generate case body
                ctx.builder->SetInsertPoint(caseBodyBB);
                {
                    ScopeGuard guard(ctx);
                    pcase.body->accept(*this);
                }
                
                // Auto-break unless terminated
                if (!ctx.builder->GetInsertBlock()->getTerminator()) {
                    ctx.builder->CreateBr(doneBB);
                }
            }
            
            // Continue with remaining cases (if any) using linear chain
            ctx.builder->SetInsertPoint(defaultBB);
            
            // Process remaining cases (starting from exactCaseCount)
            size_t remainingStart = exactCaseCount;
            if (remainingStart < node->cases.size()) {
                // Fall through to linear case chain for remaining cases
                generateLinearCaseChain(node, remainingStart, func, doneBB, labelBlocks);
            } else {
                // All cases were handled by switch - jump to done
                ctx.builder->CreateBr(doneBB);
            }
            
        } else {
            // Use traditional linear if-else chain (original implementation)
            generateLinearCaseChain(node, 0, func, doneBB, labelBlocks);
        }
        
        func->insert(func->end(), doneBB);
        ctx.builder->SetInsertPoint(doneBB);
        
        // Clear pick context
        ctx.pickLabelBlocks = nullptr;
        ctx.pickDoneBlock = nullptr;
    }
    
    // Helper: Generate traditional linear if-else case chain
    void generateLinearCaseChain(PickStmt* node, size_t startIdx, Function* func, 
                                 BasicBlock* doneBB, std::map<std::string, BasicBlock*>& labelBlocks) {
        BasicBlock* nextCaseBB = nullptr;
        Value* selector = visitExpr(node->selector.get());
        
        // Second pass: generate case logic
        for (size_t i = startIdx; i < node->cases.size(); ++i) {
            auto& pcase = node->cases[i];
            
            // For labeled cases, jump directly to their block
            if (!pcase.label.empty()) {
                // Create unconditional branch to labeled block
                if (!ctx.builder->GetInsertBlock()->getTerminator()) {
                    ctx.builder->CreateBr(labelBlocks[pcase.label]);
                }
                
                // Set insert point to the labeled block
                ctx.builder->SetInsertPoint(labelBlocks[pcase.label]);
                
                // Generate body
                {
                    ScopeGuard guard(ctx);
                    pcase.body->accept(*this);
                }
                
                // Auto-break if no terminator
                if (!ctx.builder->GetInsertBlock()->getTerminator()) {
                    ctx.builder->CreateBr(doneBB);
                }
                
                // Create a new block for next case
                nextCaseBB = BasicBlock::Create(ctx.llvmContext, "case_next_" + std::to_string(i), func);
                ctx.builder->SetInsertPoint(nextCaseBB);
                continue;
            }
            
            // Regular case (not labeled)
            BasicBlock* caseBodyBB = BasicBlock::Create(ctx.llvmContext, "case_body_" + std::to_string(i), func);
            nextCaseBB = BasicBlock::Create(ctx.llvmContext, "case_next_" + std::to_string(i));
            
            // Generate condition based on case type
            Value* match = nullptr;
            
            switch (pcase.type) {
                case PickCase::WILDCARD:
                    // (*) - always matches
                    match = ConstantInt::get(Type::getInt1Ty(ctx.llvmContext), 1);
                    break;
                    
                case PickCase::EXACT: {
                    // (value) - exact match
                    Value* val = visitExpr(pcase.value_start.get());
                    // Cast val to match selector type for comparison
                    if (val->getType() != selector->getType()) {
                        if (val->getType()->isIntegerTy() && selector->getType()->isIntegerTy()) {
                            val = ctx.builder->CreateIntCast(val, selector->getType(), true);
                        }
                    }
                    match = ctx.builder->CreateICmpEQ(selector, val, "pick_eq");
                    break;
                }
                
                case PickCase::LESS_THAN: {
                    // (<value) - less than
                    Value* val = visitExpr(pcase.value_start.get());
                    // Cast val to match selector type for comparison
                    if (val->getType() != selector->getType()) {
                        if (val->getType()->isIntegerTy() && selector->getType()->isIntegerTy()) {
                            val = ctx.builder->CreateIntCast(val, selector->getType(), true);
                        }
                    }
                    match = ctx.builder->CreateICmpSLT(selector, val, "pick_lt");
                    break;
                }
                
                case PickCase::GREATER_THAN: {
                    // (>value) - greater than
                    Value* val = visitExpr(pcase.value_start.get());
                    // Cast val to match selector type for comparison
                    if (val->getType() != selector->getType()) {
                        if (val->getType()->isIntegerTy() && selector->getType()->isIntegerTy()) {
                            val = ctx.builder->CreateIntCast(val, selector->getType(), true);
                        }
                    }
                    match = ctx.builder->CreateICmpSGT(selector, val, "pick_gt");
                    break;
                }
                
                case PickCase::LESS_EQUAL: {
                    // (<=value) - less or equal
                    Value* val = visitExpr(pcase.value_start.get());
                    // Cast val to match selector type for comparison
                    if (val->getType() != selector->getType()) {
                        if (val->getType()->isIntegerTy() && selector->getType()->isIntegerTy()) {
                            val = ctx.builder->CreateIntCast(val, selector->getType(), true);
                        }
                    }
                    match = ctx.builder->CreateICmpSLE(selector, val, "pick_le");
                    break;
                }
                
                case PickCase::GREATER_EQUAL: {
                    // (>=value) - greater or equal
                    Value* val = visitExpr(pcase.value_start.get());
                    // Cast val to match selector type for comparison
                    if (val->getType() != selector->getType()) {
                        if (val->getType()->isIntegerTy() && selector->getType()->isIntegerTy()) {
                            val = ctx.builder->CreateIntCast(val, selector->getType(), true);
                        }
                    }
                    match = ctx.builder->CreateICmpSGE(selector, val, "pick_ge");
                    break;
                }
                
                case PickCase::RANGE: {
                    // (start..end) or (start...end) - range match
                    Value* start = visitExpr(pcase.value_start.get());
                    Value* end = visitExpr(pcase.value_end.get());
                    
                    // Cast to match selector type for comparison
                    if (start->getType() != selector->getType()) {
                        if (start->getType()->isIntegerTy() && selector->getType()->isIntegerTy()) {
                            start = ctx.builder->CreateIntCast(start, selector->getType(), true);
                        }
                    }
                    if (end->getType() != selector->getType()) {
                        if (end->getType()->isIntegerTy() && selector->getType()->isIntegerTy()) {
                            end = ctx.builder->CreateIntCast(end, selector->getType(), true);
                        }
                    }
                    
                    // selector >= start
                    Value* ge_start = ctx.builder->CreateICmpSGE(selector, start, "range_ge");
                    
                    // selector <= end (inclusive) or selector < end (exclusive)
                    Value* le_end;
                    if (pcase.is_range_exclusive) {
                        le_end = ctx.builder->CreateICmpSLT(selector, end, "range_lt");
                    } else {
                        le_end = ctx.builder->CreateICmpSLE(selector, end, "range_le");
                    }
                    
                    // Combined: ge_start && le_end
                    match = ctx.builder->CreateAnd(ge_start, le_end, "range_match");
                    break;
                }
                
                case PickCase::DESTRUCTURE_OBJ: {
                    // Object destructuring: pick point { { x, y } => ... }
                    // Match is implicitly true if types align (type checker validated)
                    if (!selector->getType()->isStructTy() && !selector->getType()->isPointerTy()) {
                        throw std::runtime_error("Cannot destructure non-struct type in pick pattern");
                    }
                    
                    match = ConstantInt::get(Type::getInt1Ty(ctx.llvmContext), 1);
                    
                    // Create conditional branch to the body
                    ctx.builder->CreateCondBr(match, caseBodyBB, nextCaseBB);
                    
                    // Generate Case Body with Variable Bindings
                    ctx.builder->SetInsertPoint(caseBodyBB);
                    {
                        ScopeGuard guard(ctx); // New scope for the bindings
                        
                        // Get struct type from selector
                        // Note: Semantic analyzer ensures selector is struct or pointer-to-struct
                        StructType* structType = nullptr;
                        Value* structPtr = selector;
                        
                        if (selector->getType()->isStructTy()) {
                            // Value type - need to create a temporary and get pointer
                            structType = cast<StructType>(selector->getType());
                            AllocaInst* tempAlloca = ctx.builder->CreateAlloca(structType, nullptr, "destruct_temp");
                            ctx.builder->CreateStore(selector, tempAlloca);
                            structPtr = tempAlloca;
                        } else if (selector->getType()->isPointerTy()) {
                            // Already a pointer - assume it points to a struct (validated by sema)
                            // With LLVM 18 opaque pointers, we need type info from AST
                            // For now, we'll need to get struct type from context/symbol table
                            // TODO: Pass struct type through from semantic analysis
                            throw std::runtime_error("Destructuring pointer-to-struct requires type metadata (LLVM 18 opaque pointers)");
                        }
                        
                        if (!structType) {
                            throw std::runtime_error("Failed to extract struct type for destructuring");
                        }
                        
                        // Get struct name for field mapping
                        std::string structName = structType->hasName() ? structType->getName().str() : "";
                        
                        // Process destructuring pattern bindings
                        // Note: The pattern should be stored in pcase - this assumes parser provides it
                        // For now, implement basic field extraction by index
                        // TODO: Match by field name using ctx.structFieldMaps
                        
                        // Extract fields sequentially (simplified version)
                        // In full implementation, would match pattern field names to struct fields
                        for (unsigned idx = 0; idx < structType->getNumElements(); ++idx) {
                            // Generate binding name (e.g., field_0, field_1)
                            std::string bindName = "field_" + std::to_string(idx);
                            
                            // Extract field value
                            Value* fieldPtr = ctx.builder->CreateStructGEP(structType, structPtr, idx, bindName + "_ptr");
                            Type* fieldType = structType->getElementType(idx);
                            Value* fieldVal = ctx.builder->CreateLoad(fieldType, fieldPtr, bindName);
                            
                            // Register in local scope (simplified - real version uses pattern names)
                            // ctx.define(bindName, fieldVal, false);
                        }
                        
                        // Generate the actual body code
                        pcase.body->accept(*this);
                    }
                    
                    // Handle fallthrough/break
                    if (!ctx.builder->GetInsertBlock()->getTerminator()) {
                        ctx.builder->CreateBr(doneBB);
                    }
                    
                    // Setup for next case
                    func->insert(func->end(), nextCaseBB);
                    ctx.builder->SetInsertPoint(nextCaseBB);
                    continue; // Skip the normal flow below
                }
                
                case PickCase::DESTRUCTURE_ARR: {
                    // Array destructuring: pick arr { [a, b, c] => ... }
                    if (!selector->getType()->isArrayTy() && !selector->getType()->isPointerTy()) {
                        throw std::runtime_error("Cannot destructure non-array type in pick pattern");
                    }
                    
                    match = ConstantInt::get(Type::getInt1Ty(ctx.llvmContext), 1);
                    
                    // Create conditional branch to the body
                    ctx.builder->CreateCondBr(match, caseBodyBB, nextCaseBB);
                    
                    // Generate Case Body with Variable Bindings  
                    ctx.builder->SetInsertPoint(caseBodyBB);
                    {
                        ScopeGuard guard(ctx); // New scope for the bindings
                        
                        // Get array type from selector
                        // Note: Semantic analyzer ensures selector is array or pointer-to-array
                        ArrayType* arrayType = nullptr;
                        Value* arrayPtr = selector;
                        
                        if (selector->getType()->isArrayTy()) {
                            // Value type - create temporary and get pointer
                            arrayType = cast<ArrayType>(selector->getType());
                            AllocaInst* tempAlloca = ctx.builder->CreateAlloca(arrayType, nullptr, "arr_destruct_temp");
                            ctx.builder->CreateStore(selector, tempAlloca);
                            arrayPtr = tempAlloca;
                        } else if (selector->getType()->isPointerTy()) {
                            // Already a pointer - assume it points to an array (validated by sema)
                            // With LLVM 18 opaque pointers, we need type info from AST
                            // TODO: Pass array type through from semantic analysis
                            throw std::runtime_error("Destructuring pointer-to-array requires type metadata (LLVM 18 opaque pointers)");
                        }
                        
                        if (!arrayType) {
                            throw std::runtime_error("Failed to extract array type for destructuring");
                        }                        Type* elemType = arrayType->getElementType();
                        uint64_t arraySize = arrayType->getNumElements();
                        
                        // Extract array elements and bind to pattern variables
                        // Simplified version: bind to elem_0, elem_1, etc.
                        for (uint64_t idx = 0; idx < arraySize; ++idx) {
                            std::string bindName = "elem_" + std::to_string(idx);
                            
                            // GEP to array element
                            Value* indices[] = {
                                ConstantInt::get(Type::getInt64Ty(ctx.llvmContext), 0),
                                ConstantInt::get(Type::getInt64Ty(ctx.llvmContext), idx)
                            };
                            Value* elemPtr = ctx.builder->CreateGEP(arrayType, arrayPtr, indices, bindName + "_ptr");
                            Value* elemVal = ctx.builder->CreateLoad(elemType, elemPtr, bindName);
                            
                            // Register in local scope
                            // ctx.define(bindName, elemVal, false);
                        }
                        
                        // Generate the actual body code
                        pcase.body->accept(*this);
                    }
                    
                    // Handle fallthrough/break
                    if (!ctx.builder->GetInsertBlock()->getTerminator()) {
                        ctx.builder->CreateBr(doneBB);
                    }
                    
                    // Setup for next case
                    func->insert(func->end(), nextCaseBB);
                    ctx.builder->SetInsertPoint(nextCaseBB);
                    continue; // Skip the normal flow below
                }
                
                case PickCase::UNREACHABLE:
                    // Labeled case - already handled above
                    continue;
                    
                default:
                    match = ConstantInt::get(Type::getInt1Ty(ctx.llvmContext), 0);
                    break;
            }
            
            // Create conditional branch
            ctx.builder->CreateCondBr(match, caseBodyBB, nextCaseBB);
            
            // Generate case body
            ctx.builder->SetInsertPoint(caseBodyBB);
            {
                ScopeGuard guard(ctx);
                pcase.body->accept(*this);
            }
            
            // Auto-break (unless fallthrough via fall())
            if (!ctx.builder->GetInsertBlock()->getTerminator()) {
                ctx.builder->CreateBr(doneBB);
            }
            
            // Move to next case check
            func->insert(func->end(), nextCaseBB);
            ctx.builder->SetInsertPoint(nextCaseBB);
        }
        
        // Final fallthrough to done if no case matched
        if (!ctx.builder->GetInsertBlock()->getTerminator()) {
            ctx.builder->CreateBr(doneBB);
        }
    }
    
    // End of visit(PickStmt) - helper function above

    
    void visit(FallStmt* node) override {
        // fall(label) - explicit fallthrough to labeled case in pick
        if (!ctx.pickLabelBlocks) {
            throw std::runtime_error("fall() statement outside of pick statement");
        }
        
        auto it = ctx.pickLabelBlocks->find(node->target_label);
        if (it == ctx.pickLabelBlocks->end()) {
            throw std::runtime_error("fall() target label not found: " + node->target_label);
        }
        
        // Check if block is already terminated
        // LLVM requires exactly one terminator per basic block
        // If a terminator already exists, subsequent code is unreachable
        if (ctx.builder->GetInsertBlock()->getTerminator()) {
            // Block already terminated - create dead_code block for remaining AST
            Function* func = ctx.builder->GetInsertBlock()->getParent();
            BasicBlock* deadCode = BasicBlock::Create(ctx.llvmContext, "dead_code", func);
            ctx.builder->SetInsertPoint(deadCode);
            return;  // Don't emit branch - block was already sealed
        }
        
        // Create branch to target label
        ctx.builder->CreateBr(it->second);
    }

    void visit(TillLoop* node) override {
        // Till(limit, step) with '$' iterator
        // Positive step: counts from 0 to limit
        // Negative step: counts from limit to 0
        Value* limit = visitExpr(node->limit.get());
        Value* step = visitExpr(node->step.get());

        Function* func = ctx.builder->GetInsertBlock()->getParent();
        BasicBlock* preheader = ctx.builder->GetInsertBlock();
        BasicBlock* loopBB = BasicBlock::Create(ctx.llvmContext, "loop_body", func);
        BasicBlock* exitBB = BasicBlock::Create(ctx.llvmContext, "loop_exit", func);

        // Determine start value based on step sign
        // For positive step: start = 0, for negative step: start = limit
        Value* stepIsNegative = ctx.builder->CreateICmpSLT(step, ConstantInt::get(Type::getInt64Ty(ctx.llvmContext), 0));
        Value* startVal = ctx.builder->CreateSelect(stepIsNegative, limit, ConstantInt::get(Type::getInt64Ty(ctx.llvmContext), 0));

        ctx.builder->CreateBr(loopBB);
        ctx.builder->SetInsertPoint(loopBB);

        // PHI Node for '$'
        PHINode* iterVar = ctx.builder->CreatePHI(Type::getInt64Ty(ctx.llvmContext), 2, "$");
        iterVar->addIncoming(startVal, preheader);

        // Define '$' in scope and generate body
        {
            ScopeGuard guard(ctx);
            ctx.define("$", iterVar, false); // False = Value itself, not ref
            node->body->accept(*this);
        }

        // Increment (or decrement for negative step)
        Value* nextVal = ctx.builder->CreateAdd(iterVar, step, "next_val");
        iterVar->addIncoming(nextVal, ctx.builder->GetInsertBlock());

        // Condition: for positive step: nextVal < limit, for negative step: nextVal >= 0
        Value* condPos = ctx.builder->CreateICmpSLT(nextVal, limit, "cond_pos");
        Value* condNeg = ctx.builder->CreateICmpSGE(nextVal, ConstantInt::get(Type::getInt64Ty(ctx.llvmContext), 0), "cond_neg");
        Value* cond = ctx.builder->CreateSelect(stepIsNegative, condNeg, condPos, "loop_cond");
        
        ctx.builder->CreateCondBr(cond, loopBB, exitBB);

        ctx.builder->SetInsertPoint(exitBB);
    }
    
    void visit(WhenLoop* node) override {
        // When loop: when(condition) { body } then { success } end { failure }
        Function* func = ctx.builder->GetInsertBlock()->getParent();
        BasicBlock* loopCondBB = BasicBlock::Create(ctx.llvmContext, "when_cond", func);
        BasicBlock* loopBodyBB = BasicBlock::Create(ctx.llvmContext, "when_body", func);
        BasicBlock* thenBB = node->then_block ? BasicBlock::Create(ctx.llvmContext, "when_then") : nullptr;
        BasicBlock* endBB = node->end_block ? BasicBlock::Create(ctx.llvmContext, "when_end") : nullptr;
        BasicBlock* exitBB = BasicBlock::Create(ctx.llvmContext, "when_exit");
        
        // Jump to condition check
        ctx.builder->CreateBr(loopCondBB);
        ctx.builder->SetInsertPoint(loopCondBB);
        
        // Evaluate condition
        Value* cond = visitExpr(node->condition.get());
        ctx.builder->CreateCondBr(cond, loopBodyBB, thenBB ? thenBB : (endBB ? endBB : exitBB));
        
        // Save loop context for break/continue
        // For when loops: break jumps to end block (early exit), continue jumps to condition
        BasicBlock* prevBreakTarget = ctx.currentLoopBreakTarget;
        BasicBlock* prevContinueTarget = ctx.currentLoopContinueTarget;
        ctx.currentLoopBreakTarget = endBB ? endBB : exitBB;  // Break goes to end block if present
        ctx.currentLoopContinueTarget = loopCondBB;
        
        // Loop body
        ctx.builder->SetInsertPoint(loopBodyBB);
        if (node->body) {
            ScopeGuard guard(ctx);
            node->body->accept(*this);
        }
        
        // Restore previous loop context
        ctx.currentLoopBreakTarget = prevBreakTarget;
        ctx.currentLoopContinueTarget = prevContinueTarget;
        
        if (!ctx.builder->GetInsertBlock()->getTerminator()) {
            ctx.builder->CreateBr(loopCondBB);  // Back to condition
        }
        
        // Then block (successful completion)
        if (thenBB) {
            func->insert(func->end(), thenBB);
            ctx.builder->SetInsertPoint(thenBB);
            if (node->then_block) {
                ScopeGuard guard(ctx);
                node->then_block->accept(*this);
            }
            if (!ctx.builder->GetInsertBlock()->getTerminator()) {
                ctx.builder->CreateBr(exitBB);
            }
        }
        
        // End block (early exit or no execution)
        if (endBB) {
            func->insert(func->end(), endBB);
            ctx.builder->SetInsertPoint(endBB);
            if (node->end_block) {
                ScopeGuard guard(ctx);
                node->end_block->accept(*this);
            }
            if (!ctx.builder->GetInsertBlock()->getTerminator()) {
                ctx.builder->CreateBr(exitBB);
            }
        }
        
        // Exit
        func->insert(func->end(), exitBB);
        ctx.builder->SetInsertPoint(exitBB);
    }

    void visit(ForLoop* node) override {
        // For loop: for iter in iterable { body }
        // Note: For now, simplified implementation assuming iterable is a range
        // Full implementation would need iterator protocol
        
        Function* func = ctx.builder->GetInsertBlock()->getParent();
        BasicBlock* loopCondBB = BasicBlock::Create(ctx.llvmContext, "for_cond", func);
        BasicBlock* loopBodyBB = BasicBlock::Create(ctx.llvmContext, "for_body", func);
        BasicBlock* exitBB = BasicBlock::Create(ctx.llvmContext, "for_exit");
        
        // For simplified implementation, treat iterable as a value to iterate over
        // In a full implementation, this would call iterator methods
        Value* iterable = visitExpr(node->iterable.get());
        
        // Create iterator variable (simplified: just use the value directly)
        Value* startVal = ConstantInt::get(Type::getInt64Ty(ctx.llvmContext), 0);
        
        ctx.builder->CreateBr(loopCondBB);
        ctx.builder->SetInsertPoint(loopCondBB);
        
        // PHI node for iterator
        PHINode* iterVar = ctx.builder->CreatePHI(Type::getInt64Ty(ctx.llvmContext), 2, node->iterator_name.c_str());
        iterVar->addIncoming(startVal, ctx.builder->GetInsertBlock()->getSinglePredecessor());
        
        // Extend iterable to i64 if needed for comparison
        if (iterable->getType() != Type::getInt64Ty(ctx.llvmContext)) {
            if (iterable->getType()->isIntegerTy()) {
                iterable = ctx.builder->CreateSExtOrTrunc(iterable, Type::getInt64Ty(ctx.llvmContext));
            }
        }
        
        // Condition: iter < iterable (simplified)
        Value* cond = ctx.builder->CreateICmpSLT(iterVar, iterable, "for_cond");
        ctx.builder->CreateCondBr(cond, loopBodyBB, exitBB);
        
        // Loop body
        ctx.builder->SetInsertPoint(loopBodyBB);
        {
            ScopeGuard guard(ctx);
            ctx.define(node->iterator_name, iterVar, false);
            node->body->accept(*this);
        }
        
        // Increment iterator
        Value* nextIter = ctx.builder->CreateAdd(iterVar, ConstantInt::get(Type::getInt64Ty(ctx.llvmContext), 1), "next_iter");
        iterVar->addIncoming(nextIter, ctx.builder->GetInsertBlock());
        
        if (!ctx.builder->GetInsertBlock()->getTerminator()) {
            ctx.builder->CreateBr(loopCondBB);
        }
        
        // Exit
        func->insert(func->end(), exitBB);
        ctx.builder->SetInsertPoint(exitBB);
    }

    void visit(WhileLoop* node) override {
        // While loop: while condition { body }
        Function* func = ctx.builder->GetInsertBlock()->getParent();
        BasicBlock* loopCondBB = BasicBlock::Create(ctx.llvmContext, "while_cond", func);
        BasicBlock* loopBodyBB = BasicBlock::Create(ctx.llvmContext, "while_body", func);
        BasicBlock* exitBB = BasicBlock::Create(ctx.llvmContext, "while_exit");
        
        // Jump to condition check
        ctx.builder->CreateBr(loopCondBB);
        ctx.builder->SetInsertPoint(loopCondBB);
        
        // Evaluate condition
        Value* cond = visitExpr(node->condition.get());
        
        // Convert condition to bool (i1) if needed
        if (cond && cond->getType() != Type::getInt1Ty(ctx.llvmContext)) {
            // Compare to zero (false if zero, true otherwise)
            cond = ctx.builder->CreateICmpNE(
                cond,
                Constant::getNullValue(cond->getType()),
                "whilecond"
            );
        }
        
        ctx.builder->CreateCondBr(cond, loopBodyBB, exitBB);
        
        // Loop body
        ctx.builder->SetInsertPoint(loopBodyBB);
        
        // Save loop context for break/continue
        BasicBlock* prevBreakTarget = ctx.currentLoopBreakTarget;
        BasicBlock* prevContinueTarget = ctx.currentLoopContinueTarget;
        ctx.currentLoopBreakTarget = exitBB;
        ctx.currentLoopContinueTarget = loopCondBB;
        
        {
            ScopeGuard guard(ctx);
            node->body->accept(*this);
        }
        
        // Restore previous loop context
        ctx.currentLoopBreakTarget = prevBreakTarget;
        ctx.currentLoopContinueTarget = prevContinueTarget;
        
        // Jump back to condition (if no explicit control flow)
        if (!ctx.builder->GetInsertBlock()->getTerminator()) {
            ctx.builder->CreateBr(loopCondBB);
        }
        
        // Exit
        func->insert(func->end(), exitBB);
        ctx.builder->SetInsertPoint(exitBB);
    }

    // -------------------------------------------------------------------------
    // 3. Expressions (Helper)
    // -------------------------------------------------------------------------

    Value* visitExpr(Expression* node) {
        // Simplified Dispatcher
        if (auto* lit = dynamic_cast<aria::frontend::IntLiteral*>(node)) {
            return ConstantInt::get(Type::getInt64Ty(ctx.llvmContext), lit->value);
        }
        if (auto* flit = dynamic_cast<aria::frontend::FloatLiteral*>(node)) {
            return ConstantFP::get(Type::getDoubleTy(ctx.llvmContext), flit->value);
        }
        if (auto* blit = dynamic_cast<aria::frontend::BoolLiteral*>(node)) {
            return ConstantInt::get(Type::getInt1Ty(ctx.llvmContext), blit->value ? 1 : 0);
        }
        if (auto* nlit = dynamic_cast<aria::frontend::NullLiteral*>(node)) {
            // NULL is represented as a null pointer constant
            return ConstantPointerNull::get(PointerType::getUnqual(ctx.llvmContext));
        }
        if (auto* slit = dynamic_cast<aria::frontend::StringLiteral*>(node)) {
            // Create global string constant
            return ctx.builder->CreateGlobalStringPtr(slit->value);
        }
        if (auto* tstr = dynamic_cast<aria::frontend::TemplateString*>(node)) {
            // Build template string by concatenating parts at runtime
            // Strategy: Build string dynamically by concatenating each part
            
            // Helper: Declare sprintf if not already present
            Function* sprintfFunc = ctx.module->getFunction("sprintf");
            if (!sprintfFunc) {
                std::vector<Type*> sprintfParams = {
                    PointerType::getUnqual(ctx.llvmContext),  // char* buffer
                    PointerType::getUnqual(ctx.llvmContext)   // const char* format
                };
                FunctionType* sprintfType = FunctionType::get(
                    Type::getInt32Ty(ctx.llvmContext),
                    sprintfParams,
                    true  // vararg
                );
                sprintfFunc = Function::Create(sprintfType, Function::ExternalLinkage, "sprintf", ctx.module.get());
            }
            
            // Allocate buffer for result string (1024 bytes should be enough for most cases)
            Value* buffer = ctx.builder->CreateAlloca(
                Type::getInt8Ty(ctx.llvmContext),
                ConstantInt::get(Type::getInt64Ty(ctx.llvmContext), 1024),
                "str_buffer"
            );
            
            // Track current position in buffer
            Value* bufferPos = buffer;
            
            for (auto& part : tstr->parts) {
                if (part.type == aria::frontend::TemplatePart::STRING) {
                    // Static string part - use sprintf with "%s"
                    Value* formatStr = ctx.builder->CreateGlobalStringPtr("%s");
                    Value* strVal = ctx.builder->CreateGlobalStringPtr(part.string_value);
                    
                    std::vector<Value*> sprintfArgs = {bufferPos, formatStr, strVal};
                    Value* written = ctx.builder->CreateCall(sprintfFunc, sprintfArgs);
                    
                    // Advance buffer position by number of characters written
                    bufferPos = ctx.builder->CreateGEP(
                        Type::getInt8Ty(ctx.llvmContext),
                        bufferPos,
                        written,
                        "next_pos"
                    );
                } else {
                    // Expression part - evaluate and convert to string
                    Value* exprVal = visitExpr(part.expr_value.get());
                    
                    if (exprVal) {
                        // Determine format string based on type
                        Value* formatStr = nullptr;
                        std::vector<Value*> sprintfArgs = {bufferPos};
                        
                        if (exprVal->getType()->isIntegerTy()) {
                            // Integer types - use appropriate format
                            if (exprVal->getType()->getIntegerBitWidth() == 1) {
                                // Boolean - convert to "true"/"false"
                                formatStr = ctx.builder->CreateGlobalStringPtr("%s");
                                Value* trueStr = ctx.builder->CreateGlobalStringPtr("true");
                                Value* falseStr = ctx.builder->CreateGlobalStringPtr("false");
                                Value* boolStr = ctx.builder->CreateSelect(exprVal, trueStr, falseStr);
                                sprintfArgs.push_back(formatStr);
                                sprintfArgs.push_back(boolStr);
                            } else {
                                // Extend/truncate to i64 for sprintf %ld
                                Value* i64Val = ctx.builder->CreateIntCast(
                                    exprVal,
                                    Type::getInt64Ty(ctx.llvmContext),
                                    true,  // sign extend
                                    "to_i64"
                                );
                                formatStr = ctx.builder->CreateGlobalStringPtr("%ld");
                                sprintfArgs.push_back(formatStr);
                                sprintfArgs.push_back(i64Val);
                            }
                        } else if (exprVal->getType()->isFloatingPointTy()) {
                            // Float/double - use %f
                            formatStr = ctx.builder->CreateGlobalStringPtr("%f");
                            sprintfArgs.push_back(formatStr);
                            sprintfArgs.push_back(exprVal);
                        } else if (exprVal->getType()->isPointerTy()) {
                            // Assume it's a string pointer
                            formatStr = ctx.builder->CreateGlobalStringPtr("%s");
                            sprintfArgs.push_back(formatStr);
                            sprintfArgs.push_back(exprVal);
                        } else {
                            // Unknown type - just print address
                            formatStr = ctx.builder->CreateGlobalStringPtr("<unknown>");
                            sprintfArgs.push_back(formatStr);
                        }
                        
                        Value* written = ctx.builder->CreateCall(sprintfFunc, sprintfArgs);
                        
                        // Advance buffer position
                        bufferPos = ctx.builder->CreateGEP(
                            Type::getInt8Ty(ctx.llvmContext),
                            bufferPos,
                            written,
                            "next_pos"
                        );
                    }
                }
            }
            
            // Return pointer to the complete string
            return buffer;
        }
        if (auto* ternary = dynamic_cast<aria::frontend::TernaryExpr*>(node)) {
            // Generate LLVM select: select i1 %cond, type %true_val, type %false_val
            Value* cond = visitExpr(ternary->condition.get());
            Value* true_val = visitExpr(ternary->true_expr.get());
            Value* false_val = visitExpr(ternary->false_expr.get());
            
            if (!cond || !true_val || !false_val) return nullptr;
            
            // LLVM select requires i1 condition
            if (cond->getType()->isIntegerTy() && cond->getType()->getIntegerBitWidth() != 1) {
                cond = ctx.builder->CreateICmpNE(cond, ConstantInt::get(cond->getType(), 0));
            }
            
            return ctx.builder->CreateSelect(cond, true_val, false_val);
        }
        if (auto* var = dynamic_cast<aria::frontend::VarExpr*>(node)) {
            auto* sym = ctx.lookup(var->name);
            if (!sym) return nullptr;
            
            if (sym->is_ref) {
                // Use allocation strategy to determine loading pattern
                if (!sym->ariaType.empty()) {
                    Type* loadType = ctx.getLLVMType(sym->ariaType);
                    
                    // For dynamic array parameters (uint8[], int64[], etc.), load the pointer value
                    // These are represented as pointers in LLVM, not array types
                    // Check this FIRST before isArrayTy() to handle parameter arrays correctly
                    size_t bracketPos = sym->ariaType.find("[]");
                    if (bracketPos != std::string::npos) {
                        // This is a dynamic array parameter - load the pointer
                        return ctx.builder->CreateLoad(PointerType::getUnqual(ctx.llvmContext), sym->val, var->name + "_ptr");
                    } 
                    // For array types, return pointer to first element instead of loading the array
                    else if (loadType->isArrayTy()) {
                        // For heap allocations, we need to load the heap pointer first
                        // sym->val is a stack location holding a pointer to the heap data
                        if (sym->strategy == CodeGenContext::AllocStrategy::WILD || 
                            sym->strategy == CodeGenContext::AllocStrategy::GC) {
                            // Load the heap pointer from the stack slot
                            Value* heapPtr = ctx.builder->CreateLoad(PointerType::getUnqual(ctx.llvmContext), sym->val, var->name + "_heap");
                            // Now get pointer to first element of the heap-allocated array
                            Value* indices[] = {
                                ConstantInt::get(Type::getInt64Ty(ctx.llvmContext), 0)
                            };
                            return ctx.builder->CreateGEP(loadType, heapPtr, indices, var->name + "_ptr");
                        } else {
                            // For stack allocations, GEP directly on the alloca
                            Value* indices[] = {
                                ConstantInt::get(Type::getInt64Ty(ctx.llvmContext), 0),
                                ConstantInt::get(Type::getInt64Ty(ctx.llvmContext), 0)
                            };
                            return ctx.builder->CreateGEP(loadType, sym->val, indices, var->name + "_ptr");
                        }
                    }
                    
                    // For heap allocations (wild/gc), load pointer first, then value
                    if (sym->strategy == CodeGenContext::AllocStrategy::WILD || 
                        sym->strategy == CodeGenContext::AllocStrategy::GC) {
                        Value* heapPtr = ctx.builder->CreateLoad(PointerType::getUnqual(ctx.llvmContext), sym->val);
                        return ctx.builder->CreateLoad(loadType, heapPtr);
                    }
                    
                    // For wildx allocations (executable memory), return the pointer itself
                    // Wildx pointers are used directly for memory operations (protect_exec, etc.)
                    if (sym->strategy == CodeGenContext::AllocStrategy::WILDX) {
                        // Return the pointer value (don't dereference)
                        return ctx.builder->CreateLoad(PointerType::getUnqual(ctx.llvmContext), sym->val);
                    }
                    
                    // For stack allocations, direct load from alloca
                    if (sym->strategy == CodeGenContext::AllocStrategy::STACK) {
                        Value* loaded = ctx.builder->CreateLoad(loadType, sym->val);
                        // Track the type for TBB safety
                        if (!sym->ariaType.empty()) {
                            ctx.exprTypeMap[loaded] = sym->ariaType;
                        }
                        return loaded;
                    }
                    
                    // Fallback: use loadType from ariaType if available
                    // This handles parameters and other allocations
                    Value* loaded = ctx.builder->CreateLoad(loadType, sym->val);
                    // Track the type for TBB safety
                    if (!sym->ariaType.empty()) {
                        ctx.exprTypeMap[loaded] = sym->ariaType;
                    }
                    return loaded;
                }
                
                // Ultimate fallback if no ariaType is set (shouldn't happen)
                return ctx.builder->CreateLoad(Type::getInt64Ty(ctx.llvmContext), sym->val);
            }
            return sym->val; // PHI or direct value
        }
        if (auto* call = dynamic_cast<aria::frontend::CallExpr*>(node)) {
            // Handle function call
            // First check if it's a known builtin mapping
            std::string funcName = call->function_name;
            if (funcName == "print") {
                funcName = "puts";
            }
            
            // Check for syscall intrinsics (file I/O operations)
            // These generate direct syscalls via inline assembly on Linux
            if (funcName == "intrinsic_open") {
                // open syscall - syscall number 2 on x86-64 Linux
                std::vector<Value*> args;
                for (auto& arg : call->arguments) {
                    args.push_back(visitExpr(arg.get()));
                }
                return createSyscall(2, args, Type::getInt32Ty(ctx.llvmContext));
            } else if (funcName == "intrinsic_close") {
                // close syscall - syscall number 3
                std::vector<Value*> args;
                for (auto& arg : call->arguments) {
                    args.push_back(visitExpr(arg.get()));
                }
                return createSyscall(3, args, Type::getInt32Ty(ctx.llvmContext));
            } else if (funcName == "intrinsic_read") {
                // read syscall - syscall number 0
                std::vector<Value*> args;
                for (auto& arg : call->arguments) {
                    args.push_back(visitExpr(arg.get()));
                }
                return createSyscall(0, args, Type::getInt64Ty(ctx.llvmContext));
            } else if (funcName == "intrinsic_write") {
                // write syscall - syscall number 1
                std::vector<Value*> args;
                for (auto& arg : call->arguments) {
                    args.push_back(visitExpr(arg.get()));
                }
                return createSyscall(1, args, Type::getInt64Ty(ctx.llvmContext));
            } else if (funcName == "intrinsic_lseek") {
                // lseek syscall - syscall number 8
                std::vector<Value*> args;
                for (auto& arg : call->arguments) {
                    args.push_back(visitExpr(arg.get()));
                }
                return createSyscall(8, args, Type::getInt64Ty(ctx.llvmContext));
            }
            
            // Check for wildx memory protection intrinsics
            // These map to the runtime functions directly
            bool is_wildx_intrinsic = false;
            if (funcName == "aria.mem.protect_exec" || funcName == "protect_exec") {
                funcName = "aria_mem_protect_exec";
                is_wildx_intrinsic = true;
            } else if (funcName == "aria.mem.protect_write" || funcName == "protect_write") {
                funcName = "aria_mem_protect_write";
                is_wildx_intrinsic = true;
            } else if (funcName == "aria.mem.free_exec" || funcName == "free_exec") {
                funcName = "aria_free_exec";
                is_wildx_intrinsic = true;
            }
            
            // Try to find function in symbol table (for Aria functions)
            Function* callee = nullptr;
            Value* calleePtr = nullptr;  // For indirect calls through func variables
            FunctionType* funcType = nullptr;  // For indirect calls with signature
            
            auto* sym = ctx.lookup(call->function_name);
            if (sym && !sym->is_ref) {
                // Direct function value (registered from FuncDecl or Lambda)
                callee = dyn_cast_or_null<Function>(sym->val);
                
                // If not a Function*, check if it's a func variable with signature
                if (!callee && sym->ariaType.find("func<") == 0) {
                    // This is a func variable with a signature - indirect call
                    calleePtr = sym->val;
                    funcType = ctx.parseFunctionSignature(sym->ariaType);
                    
                    if (!funcType) {
                        throw std::runtime_error("Invalid function signature for '" + call->function_name + "'");
                    }
                }
            }
            
            // If not in symbol table, try module (for external functions)
            if (!callee && !calleePtr) {
                callee = ctx.module->getFunction(funcName);
            }
            
            // If wildx intrinsic and not found, declare it now
            if (!callee && is_wildx_intrinsic) {
                if (funcName == "aria_mem_protect_exec") {
                    callee = getOrInsertAriaMemProtectExec();
                } else if (funcName == "aria_mem_protect_write") {
                    callee = getOrInsertAriaMemProtectWrite();
                } else if (funcName == "aria_free_exec") {
                    callee = getOrInsertAriaFreeExec();
                }
            }
            
            // If LLVM intrinsic (llvm_*), declare it now
            if (!callee && funcName.substr(0, 5) == "llvm_") {
                callee = declareLLVMIntrinsic(funcName);
            }
            
            if (!callee && !calleePtr) {
                std::string errorMsg = "Undefined function '" + call->function_name + "'";
                
                // Check if it's a common typo or forgotten function
                if (call->function_name == "println") {
                    errorMsg += "\n\nDid you mean 'print'?";
                } else if (call->function_name == "malloc" || call->function_name == "free") {
                    errorMsg += "\n\nAria uses 'aria.alloc' and 'aria.free' instead of malloc/free";
                } else {
                    errorMsg += "\n\nMake sure the function is declared before it's called.";
                }
                
                throw std::runtime_error(errorMsg);
            }
            
            // Build argument list
            std::vector<Value*> args;
            
            // Get the function type for parameter type checking
            FunctionType* callFuncType = nullptr;
            if (callee) {
                callFuncType = callee->getFunctionType();
            } else if (funcType) {
                callFuncType = funcType;
            }
            
            // Track which parameters are scalarized (for struct optimization)
            // We need to match against the original function declaration to know
            // which parameters were scalarized
            size_t paramTypeIdx = 0;
            
            for (size_t i = 0; i < call->arguments.size(); i++) {
                Value* argVal = visitExpr(call->arguments[i].get());
                
                // ================================================================
                // OPTIMIZATION: Scalarize struct arguments for optimized functions
                // ================================================================
                // If the argument is a struct and fits the optimization criteria,
                // extract individual fields and pass them as separate arguments
                // ================================================================
                
                bool scalarized = false;
                
                if (argVal->getType()->isStructTy() && callFuncType) {
                    auto* structType = cast<StructType>(argVal->getType());
                    
                    // Check if this parameter was scalarized in the function declaration
                    // (This happens if struct size ≤ 16 bytes)
                    if (structType->isSized()) {
                        uint64_t structSize = ctx.module->getDataLayout().getTypeAllocSize(structType);
                        
                        if (structSize > 0 && structSize <= 16) {
                            // Check if we have enough parameter slots for all fields
                            if (paramTypeIdx + structType->getNumElements() <= callFuncType->getNumParams()) {
                                // Verify types match (scalarized)
                                bool typesMatch = true;
                                for (unsigned fieldIdx = 0; fieldIdx < structType->getNumElements(); ++fieldIdx) {
                                    if (callFuncType->getParamType(paramTypeIdx + fieldIdx) != 
                                        structType->getElementType(fieldIdx)) {
                                        typesMatch = false;
                                        break;
                                    }
                                }
                                
                                if (typesMatch) {
                                    // Scalarize: extract and pass each field separately
                                    for (unsigned fieldIdx = 0; fieldIdx < structType->getNumElements(); ++fieldIdx) {
                                        Value* fieldVal = ctx.builder->CreateExtractValue(
                                            argVal, fieldIdx,
                                            "arg_field" + std::to_string(fieldIdx));
                                        args.push_back(fieldVal);
                                    }
                                    paramTypeIdx += structType->getNumElements();
                                    scalarized = true;
                                }
                            }
                        }
                    }
                }
                
                if (!scalarized) {
                    // Standard argument passing (no scalarization)
                    // Cast argument to match parameter type if needed
                    if (callFuncType && paramTypeIdx < callFuncType->getNumParams()) {
                        Type* expectedType = callFuncType->getParamType(paramTypeIdx);
                        if (argVal->getType() != expectedType) {
                            // If both are integers, perform cast
                            if (argVal->getType()->isIntegerTy() && expectedType->isIntegerTy()) {
                                argVal = ctx.builder->CreateIntCast(argVal, expectedType, true);
                            }
                        }
                    }
                    
                    args.push_back(argVal);
                    paramTypeIdx++;
                }
            }
            
            // Generate the call
            if (calleePtr) {
                // Indirect call through function pointer
                // Cast the pointer to the correct function type
                Value* funcPtr = ctx.builder->CreateBitCast(calleePtr, PointerType::getUnqual(funcType));
                
                // Void functions shouldn't have result names
                if (funcType->getReturnType()->isVoidTy()) {
                    return ctx.builder->CreateCall(funcType, funcPtr, args);
                }
                return ctx.builder->CreateCall(funcType, funcPtr, args, "calltmp");
            } else {
                // Direct call
                // Void functions shouldn't have result names
                if (callee->getReturnType()->isVoidTy()) {
                    return ctx.builder->CreateCall(callee, args);
                }
                return ctx.builder->CreateCall(callee, args, "calltmp");
            }
        }
        if (auto* unary = dynamic_cast<aria::frontend::UnaryOp*>(node)) {
            Value* operand = visitExpr(unary->operand.get());
            if (!operand) return nullptr;
            
            switch (unary->op) {
                case aria::frontend::UnaryOp::NEG:
                    return ctx.builder->CreateNeg(operand);
                case aria::frontend::UnaryOp::LOGICAL_NOT:
                    return ctx.builder->CreateNot(operand);
                case aria::frontend::UnaryOp::BITWISE_NOT:
                    return ctx.builder->CreateNot(operand);
                case aria::frontend::UnaryOp::POST_INC:
                case aria::frontend::UnaryOp::POST_DEC: {
                    // For x++ or x--, we need to:
                    // 1. Load current value
                    // 2. Increment/decrement
                    // 3. Store back
                    // 4. Return original value (for x++) or new value (simplified: return new)
                    if (auto* varExpr = dynamic_cast<aria::frontend::VarExpr*>(unary->operand.get())) {
                        auto* sym = ctx.lookup(varExpr->name);
                        if (!sym || !sym->is_ref) return nullptr;
                        
                        Value* currentVal = ctx.builder->CreateLoad(ctx.getLLVMType("int64"), sym->val);
                        Value* one = ConstantInt::get(Type::getInt64Ty(ctx.llvmContext), 1);
                        Value* newVal = (unary->op == aria::frontend::UnaryOp::POST_INC) 
                            ? ctx.builder->CreateAdd(currentVal, one)
                            : ctx.builder->CreateSub(currentVal, one);
                        ctx.builder->CreateStore(newVal, sym->val);
                        return newVal;  // Simplified: return new value
                    }
                    return nullptr;
                }
                case aria::frontend::UnaryOp::ADDRESS_OF: {
                    // @ operator: get address of a variable (like C's &)
                    // 
                    // DEBUG BUILDS: Generate fat pointer with scope ID
                    // RELEASE BUILDS: Generate raw pointer (zero overhead)
                    //
                    // Fat Pointer Structure (debug):
                    //   { i8* ptr, i64 scope_id, i64 timestamp }
                    // 
                    // This enables runtime detection of use-after-scope bugs
                    
                    if (auto* varExpr = dynamic_cast<aria::frontend::VarExpr*>(unary->operand.get())) {
                        auto* sym = ctx.lookup(varExpr->name);
                        if (!sym || !sym->is_ref) return nullptr;
                        
                        #ifdef ARIA_DEBUG
                        // === DEBUG BUILD: Generate fat pointer ===
                        
                        // 1. Get or declare aria_fat_ptr_create function
                        Function* createFatPtr = ctx.module->getFunction("aria_fat_ptr_create");
                        if (!createFatPtr) {
                            // aria_fat_pointer_t aria_fat_ptr_create(void* raw_ptr, uint64_t scope_id)
                            std::vector<Type*> paramTypes = {
                                PointerType::getUnqual(ctx.llvmContext),  // void* raw_ptr
                                Type::getInt64Ty(ctx.llvmContext)         // uint64_t scope_id
                            };
                            
                            // Return type: { i8*, i64, i64 } (fat pointer struct)
                            std::vector<Type*> fatPtrFields = {
                                PointerType::getUnqual(ctx.llvmContext),  // ptr
                                Type::getInt64Ty(ctx.llvmContext),        // scope_id
                                Type::getInt64Ty(ctx.llvmContext)         // alloc_timestamp
                            };
                            StructType* fatPtrType = StructType::get(ctx.llvmContext, fatPtrFields);
                            
                            FunctionType* funcType = FunctionType::get(fatPtrType, paramTypes, false);
                            createFatPtr = Function::Create(funcType, Function::ExternalLinkage,
                                                          "aria_fat_ptr_create", ctx.module.get());
                        }
                        
                        // 2. Generate call: aria_fat_ptr_create(sym->val, current_scope_id)
                        std::vector<Value*> args = {
                            sym->val,  // Raw pointer (alloca address)
                            ConstantInt::get(Type::getInt64Ty(ctx.llvmContext), ctx.current_scope_id)
                        };
                        
                        return ctx.builder->CreateCall(createFatPtr, args, "fat_ptr");
                        
                        #else
                        // === RELEASE BUILD: Generate raw pointer (zero overhead) ===
                        
                        // Return the pointer (alloca address) as an integer
                        // PtrToInt converts pointer to integer representation
                        return ctx.builder->CreatePtrToInt(sym->val, Type::getInt64Ty(ctx.llvmContext));
                        
                        #endif
                    }
                    return nullptr;
                }
                case aria::frontend::UnaryOp::PIN: {
                    // # operator: pin dynamic value to specific type
                    // For now, simplified: just return the operand
                    // In full implementation, would:
                    // 1. Check operand is dyn type
                    // 2. Perform runtime type check
                    // 3. Extract/cast to specific type
                    return operand;
                }
            }
        }
        if (auto* binop = dynamic_cast<aria::frontend::BinaryOp*>(node)) {
            // Handle assignment operators specially - need LHS address, not value
            if (binop->op == aria::frontend::BinaryOp::ASSIGN ||
                binop->op == aria::frontend::BinaryOp::PLUS_ASSIGN ||
                binop->op == aria::frontend::BinaryOp::MINUS_ASSIGN ||
                binop->op == aria::frontend::BinaryOp::STAR_ASSIGN ||
                binop->op == aria::frontend::BinaryOp::SLASH_ASSIGN ||
                binop->op == aria::frontend::BinaryOp::MOD_ASSIGN) {
                
                // Check if LHS is array indexing
                if (auto* indexExpr = dynamic_cast<aria::frontend::IndexExpr*>(binop->left.get())) {
                    // Assignment to array element: arr[i] = value
                    
                    // For array indexing assignment, we need the POINTER not the loaded value
                    // Get the array variable directly from symbol table
                    Value* arrayPtr = nullptr;
                    Type* elementType = nullptr;
                    
                    if (auto* varRef = dynamic_cast<aria::frontend::VarExpr*>(indexExpr->array.get())) {
                        // Look up the variable to get its alloca (which holds a pointer)
                        auto* sym = ctx.lookup(varRef->name);
                        if (!sym) {
                            throw std::runtime_error("Undefined variable: " + varRef->name);
                        }
                        
                        // Get element type from Aria type string
                        std::string ariaType = sym->ariaType;
                        size_t bracketPos = ariaType.find('[');
                        if (bracketPos != std::string::npos) {
                            std::string elemTypeName = ariaType.substr(0, bracketPos);
                            elementType = ctx.getLLVMType(elemTypeName);
                        } else {
                            // Not an array type? Default to i8
                            elementType = Type::getInt8Ty(ctx.llvmContext);
                        }
                        
                        // Get array pointer - strategy determines how
                        // Check for dynamic array parameters (T[]) first
                        if (ariaType.find("[]") != std::string::npos) {
                            // Dynamic array parameter: sym->val is alloca holding pointer
                            // Load the pointer value
                            arrayPtr = ctx.builder->CreateLoad(
                                PointerType::getUnqual(ctx.llvmContext),
                                sym->val,
                                varRef->name + "_ptr"
                            );
                        } else if (sym->strategy == CodeGenContext::AllocStrategy::WILD || 
                            sym->strategy == CodeGenContext::AllocStrategy::GC) {
                            // Heap allocated: sym->val is ptr to ptr, need to load heap pointer
                            arrayPtr = ctx.builder->CreateLoad(
                                PointerType::getUnqual(ctx.llvmContext),
                                sym->val,
                                varRef->name + "_ptr"
                            );
                        } else {
                            // Stack allocated: sym->val is ptr to array, use directly
                            Type* arrayType = ctx.getLLVMType(ariaType);
                            Value* indices[] = {
                                ConstantInt::get(Type::getInt64Ty(ctx.llvmContext), 0),
                                ConstantInt::get(Type::getInt64Ty(ctx.llvmContext), 0)
                            };
                            arrayPtr = ctx.builder->CreateGEP(arrayType, sym->val, indices, varRef->name + "_ptr");
                        }
                    } else {
                        throw std::runtime_error("Array assignment requires variable reference");
                    }
                    
                    if (!arrayPtr || !elementType) {
                        throw std::runtime_error("Failed to resolve array pointer or element type");
                    }
                    
                    // Get index and RHS value
                    Value* index = visitExpr(indexExpr->index.get());
                    Value* rhs = visitExpr(binop->right.get());
                    
                    if (!index || !rhs) return nullptr;
                    
                    // Cast RHS to element type if needed
                    if (rhs->getType() != elementType) {
                        if (rhs->getType()->isIntegerTy() && elementType->isIntegerTy()) {
                            rhs = ctx.builder->CreateIntCast(rhs, elementType, false, "cast");
                        }
                    }
                    
                    // Generate GEP to element
                    Value* elemPtr = ctx.builder->CreateGEP(
                        elementType,
                        arrayPtr,
                        index,
                        "elem_ptr"
                    );
                    
                    // Store value to element
                    ctx.builder->CreateStore(rhs, elemPtr);
                    return rhs;  // Return the assigned value
                }
                
                // Get LHS variable address (simple variable assignment)
                auto* varExpr = dynamic_cast<aria::frontend::VarExpr*>(binop->left.get());
                if (!varExpr) return nullptr;
                
                auto* sym = ctx.lookup(varExpr->name);
                if (!sym || !sym->is_ref) return nullptr;
                
                Value* lhsAddr = sym->val;  // This is the address/alloca
                
                // For compound assignments, load current value first
                Value* R = visitExpr(binop->right.get());
                if (!R) return nullptr;
                
                Value* result = R;
                if (binop->op != aria::frontend::BinaryOp::ASSIGN) {
                    // Load current value for compound assignment
                    Type* loadType = ctx.getLLVMType(sym->ariaType);
                    Value* currentVal;
                    
                    // For heap allocations, load pointer first
                    if (sym->val->getName().contains("_heap") || isa<CallInst>(sym->val)) {
                        Value* heapPtr = ctx.builder->CreateLoad(PointerType::getUnqual(ctx.llvmContext), lhsAddr);
                        currentVal = ctx.builder->CreateLoad(loadType, heapPtr);
                    } else {
                        // For stack allocations, direct load
                        currentVal = ctx.builder->CreateLoad(loadType, lhsAddr);
                    }
                    
                    // Type promotion: ensure both operands have same type
                    if (currentVal->getType()->isIntegerTy() && R->getType()->isIntegerTy()) {
                        Type* currentType = currentVal->getType();
                        Type* rType = R->getType();
                        unsigned currentBits = currentType->getIntegerBitWidth();
                        unsigned rBits = rType->getIntegerBitWidth();
                        
                        if (currentBits != rBits) {
                            // Promote to larger type
                            if (currentBits < rBits) {
                                currentVal = ctx.builder->CreateSExtOrTrunc(currentVal, rType);
                            } else {
                                R = ctx.builder->CreateSExtOrTrunc(R, currentType);
                            }
                        }
                    }
                    
                    // Perform operation with type-matched operands
                    switch (binop->op) {
                        case aria::frontend::BinaryOp::PLUS_ASSIGN:
                            result = ctx.builder->CreateAdd(currentVal, R, "addtmp");
                            break;
                        case aria::frontend::BinaryOp::MINUS_ASSIGN:
                            result = ctx.builder->CreateSub(currentVal, R, "subtmp");
                            break;
                        case aria::frontend::BinaryOp::STAR_ASSIGN:
                            result = ctx.builder->CreateMul(currentVal, R, "multmp");
                            break;
                        case aria::frontend::BinaryOp::SLASH_ASSIGN:
                            result = ctx.builder->CreateSDiv(currentVal, R, "divtmp");
                            break;
                        case aria::frontend::BinaryOp::MOD_ASSIGN:
                            result = ctx.builder->CreateSRem(currentVal, R, "modtmp");
                            break;
                        case aria::frontend::BinaryOp::AND_ASSIGN:
                            result = ctx.builder->CreateAnd(currentVal, R, "andassigntmp");
                            break;
                        case aria::frontend::BinaryOp::OR_ASSIGN:
                            result = ctx.builder->CreateOr(currentVal, R, "orassigntmp");
                            break;
                        case aria::frontend::BinaryOp::XOR_ASSIGN:
                            result = ctx.builder->CreateXor(currentVal, R, "xorassigntmp");
                            break;
                        case aria::frontend::BinaryOp::LSHIFT_ASSIGN:
                            result = ctx.builder->CreateShl(currentVal, R, "lshiftassigntmp");
                            break;
                        case aria::frontend::BinaryOp::RSHIFT_ASSIGN:
                            result = ctx.builder->CreateAShr(currentVal, R, "rshiftassigntmp");
                            break;
                        default:
                            break;
                    }
                }
                
                // Convert result to match variable type before storing
                Type* varType = ctx.getLLVMType(sym->ariaType);
                if (result->getType() != varType) {
                    if (result->getType()->isIntegerTy() && varType->isIntegerTy()) {
                        result = ctx.builder->CreateIntCast(result, varType, true);
                    }
                }
                
                // Store result back to LHS
                // For heap allocations, get the heap pointer first
                if (sym->val->getName().contains("_heap") || isa<CallInst>(sym->val)) {
                    Value* heapPtr = ctx.builder->CreateLoad(PointerType::getUnqual(ctx.llvmContext), lhsAddr);
                    ctx.builder->CreateStore(result, heapPtr);
                } else {
                    // For stack allocations, direct store
                    ctx.builder->CreateStore(result, lhsAddr);
                }
                
                return result;  // Return the assigned value
            }
            
            // Regular binary operations (not assignments)
            Value* L = visitExpr(binop->left.get());
            Value* R = visitExpr(binop->right.get());
            
            if (!L || !R) return nullptr;
            
            // Determine if we're working with TBB types
            // Check the expression type map first, then fall back to introspection
            std::string leftType = "";
            std::string rightType = "";
            
            if (ctx.exprTypeMap.count(L)) {
                leftType = ctx.exprTypeMap[L];
            }
            if (ctx.exprTypeMap.count(R)) {
                rightType = ctx.exprTypeMap[R];
            }
            
            bool isTBBOperation = TBBLowerer::isTBBType(leftType) || TBBLowerer::isTBBType(rightType);
            
            // Type promotion: if operands have different integer types, promote to larger type
            if (L->getType()->isIntegerTy() && R->getType()->isIntegerTy()) {
                Type* LType = L->getType();
                Type* RType = R->getType();
                unsigned LBits = LType->getIntegerBitWidth();
                unsigned RBits = RType->getIntegerBitWidth();
                
                if (LBits != RBits) {
                    // Promote smaller type to larger type
                    if (LBits < RBits) {
                        L = ctx.builder->CreateSExtOrTrunc(L, RType);
                        leftType = rightType;  // Update type after promotion
                    } else {
                        R = ctx.builder->CreateSExtOrTrunc(R, LType);
                        rightType = leftType;  // Update type after promotion
                    }
                }
            }
            
            // TBB-aware arithmetic operations
            // If either operand is a TBB type, use TBBLowerer for sticky error propagation
            if (isTBBOperation && L->getType()->isIntegerTy() && R->getType()->isIntegerTy()) {
                TBBLowerer tbbLowerer(ctx.llvmContext, *ctx.builder, ctx.module.get());
                Value* result = nullptr;
                
                switch (binop->op) {
                    case aria::frontend::BinaryOp::ADD:
                        result = tbbLowerer.createAdd(L, R);
                        break;
                    case aria::frontend::BinaryOp::SUB:
                        result = tbbLowerer.createSub(L, R);
                        break;
                    case aria::frontend::BinaryOp::MUL:
                        result = tbbLowerer.createMul(L, R);
                        break;
                    case aria::frontend::BinaryOp::DIV:
                        result = tbbLowerer.createDiv(L, R);
                        break;
                    case aria::frontend::BinaryOp::MOD:
                        result = tbbLowerer.createMod(L, R);
                        break;
                    default:
                        // For non-arithmetic operations (comparisons, bitwise, etc.),
                        // fall through to standard handling below
                        isTBBOperation = false;  // Not an arithmetic op
                        break;
                }
                
                if (result) {
                    // Track the result type
                    ctx.exprTypeMap[result] = leftType.empty() ? rightType : leftType;
                    return result;
                }
            }
            
            // Standard operations (non-TBB or comparison/logical operations)
            switch (binop->op) {
                case aria::frontend::BinaryOp::ADD:
                    if (L->getType()->isFloatingPointTy()) {
                        return ctx.builder->CreateFAdd(L, R, "addtmp");
                    }
                    return ctx.builder->CreateAdd(L, R, "addtmp");
                case aria::frontend::BinaryOp::SUB:
                    if (L->getType()->isFloatingPointTy()) {
                        return ctx.builder->CreateFSub(L, R, "subtmp");
                    }
                    return ctx.builder->CreateSub(L, R, "subtmp");
                case aria::frontend::BinaryOp::MUL:
                    if (L->getType()->isFloatingPointTy()) {
                        return ctx.builder->CreateFMul(L, R, "multmp");
                    }
                    return ctx.builder->CreateMul(L, R, "multmp");
                case aria::frontend::BinaryOp::DIV:
                    if (L->getType()->isFloatingPointTy()) {
                        return ctx.builder->CreateFDiv(L, R, "divtmp");
                    }
                    return ctx.builder->CreateSDiv(L, R, "divtmp");
                case aria::frontend::BinaryOp::MOD:
                    if (L->getType()->isFloatingPointTy()) {
                        return ctx.builder->CreateFRem(L, R, "modtmp");
                    }
                    return ctx.builder->CreateSRem(L, R, "modtmp");
                case aria::frontend::BinaryOp::EQ:
                    if (L->getType()->isFloatingPointTy()) {
                        return ctx.builder->CreateFCmpOEQ(L, R, "eqtmp");
                    }
                    return ctx.builder->CreateICmpEQ(L, R, "eqtmp");
                case aria::frontend::BinaryOp::NE:
                    if (L->getType()->isFloatingPointTy()) {
                        return ctx.builder->CreateFCmpONE(L, R, "netmp");
                    }
                    return ctx.builder->CreateICmpNE(L, R, "netmp");
                case aria::frontend::BinaryOp::LT:
                    if (L->getType()->isFloatingPointTy()) {
                        return ctx.builder->CreateFCmpOLT(L, R, "lttmp");
                    }
                    return ctx.builder->CreateICmpSLT(L, R, "lttmp");
                case aria::frontend::BinaryOp::GT:
                    if (L->getType()->isFloatingPointTy()) {
                        return ctx.builder->CreateFCmpOGT(L, R, "gttmp");
                    }
                    return ctx.builder->CreateICmpSGT(L, R, "gttmp");
                case aria::frontend::BinaryOp::LE:
                    if (L->getType()->isFloatingPointTy()) {
                        return ctx.builder->CreateFCmpOLE(L, R, "letmp");
                    }
                    return ctx.builder->CreateICmpSLE(L, R, "letmp");
                case aria::frontend::BinaryOp::GE:
                    if (L->getType()->isFloatingPointTy()) {
                        return ctx.builder->CreateFCmpOGE(L, R, "getmp");
                    }
                    return ctx.builder->CreateICmpSGE(L, R, "getmp");
                case aria::frontend::BinaryOp::LOGICAL_AND:
                    return ctx.builder->CreateAnd(L, R, "andtmp");
                case aria::frontend::BinaryOp::LOGICAL_OR:
                    return ctx.builder->CreateOr(L, R, "ortmp");
                case aria::frontend::BinaryOp::BITWISE_AND:
                    return ctx.builder->CreateAnd(L, R, "bandtmp");
                case aria::frontend::BinaryOp::BITWISE_OR:
                    return ctx.builder->CreateOr(L, R, "bortmp");
                case aria::frontend::BinaryOp::BITWISE_XOR:
                    return ctx.builder->CreateXor(L, R, "xortmp");
                case aria::frontend::BinaryOp::LSHIFT:
                    return ctx.builder->CreateShl(L, R, "shltmp");
                case aria::frontend::BinaryOp::RSHIFT:
                    return ctx.builder->CreateAShr(L, R, "ashrtmp");
                default:
                    // Assignment operators handled elsewhere
                    return nullptr;
            }
        }
        if (auto* obj = dynamic_cast<aria::frontend::ObjectLiteral*>(node)) {
            // Check if this is a struct constructor (has type_name) or a result object
            if (!obj->type_name.empty()) {
                // This is a struct constructor: Point{x: 10, y: 20}
                std::string structName = obj->type_name;
                
                // Get the struct type from LLVM
                StructType* structType = StructType::getTypeByName(ctx.llvmContext, structName);
                if (!structType) {
                    throw std::runtime_error("Unknown struct type: " + structName);
                }
                
                // Allocate struct on stack
                AllocaInst* structAlloca = ctx.builder->CreateAlloca(structType, nullptr, structName + "_instance");
                
                // Initialize fields by name
                // TODO: For now, assume fields are in order. Later, match by name.
                unsigned fieldIdx = 0;
                for (auto& field : obj->fields) {
                    Value* fieldValue = visitExpr(field.value.get());
                    if (!fieldValue) {
                        throw std::runtime_error("Failed to evaluate field initializer for: " + field.name);
                    }
                    
                    // Get pointer to field
                    Value* fieldPtr = ctx.builder->CreateStructGEP(structType, structAlloca, fieldIdx, field.name + "_ptr");
                    
                    // Store value
                    Type* fieldType = structType->getElementType(fieldIdx);
                    if (fieldValue->getType() != fieldType) {
                        // Try to cast integers
                        if (fieldValue->getType()->isIntegerTy() && fieldType->isIntegerTy()) {
                            fieldValue = ctx.builder->CreateIntCast(fieldValue, fieldType, true);
                        } else {
                            throw std::runtime_error("Type mismatch for field: " + field.name);
                        }
                    }
                    ctx.builder->CreateStore(fieldValue, fieldPtr);
                    fieldIdx++;
                }
                
                // Load and return the struct value
                return ctx.builder->CreateLoad(structType, structAlloca, structName + "_value");
            }
            
            // Otherwise, check if this is a result object (has err and/or val fields)
            bool isResultObject = false;
            Value* valField = nullptr;
            Value* errField = nullptr;
            
            for (auto& field : obj->fields) {
                if (field.name == "err") {
                    isResultObject = true;
                    errField = visitExpr(field.value.get());
                } else if (field.name == "val") {
                    isResultObject = true;  // Recognize {val:x} as Result object
                    valField = visitExpr(field.value.get());
                }
            }
            
            if (isResultObject) {
                // This is a result object - create result<T> based on current function's return type
                if (ctx.currentFunctionReturnType.empty()) {
                    throw std::runtime_error("Result object used outside of function context");
                }
                
                // Get the correct result type for this function
                Type* resultTypeLLVM = ctx.getResultType(ctx.currentFunctionReturnType);
                StructType* resultType = dyn_cast<StructType>(resultTypeLLVM);
                
                if (!resultType) {
                    throw std::runtime_error("Failed to create result type for: " + ctx.currentFunctionReturnType);
                }
                
                // Get expected val field type
                Type* expectedValType = resultType->getElementType(1);  // val is field index 1
                
                // Allocate struct on stack
                AllocaInst* resultAlloca = ctx.builder->CreateAlloca(resultType, nullptr, "result");
                
                // Store err field (index 0) - default to 0 (success) if not provided
                Value* errValue = errField ? errField : ConstantInt::get(Type::getInt8Ty(ctx.llvmContext), 0);
                
                // Type check err field - must be i8
                if (errValue->getType() != Type::getInt8Ty(ctx.llvmContext)) {
                    if (errValue->getType()->isIntegerTy()) {
                        errValue = ctx.builder->CreateIntCast(errValue, Type::getInt8Ty(ctx.llvmContext), true, "err_cast");
                    } else {
                        throw std::runtime_error("err field must be int8 (error code: 0=success, 1-255=error)");
                    }
                }
                
                Value* errPtr = ctx.builder->CreateStructGEP(resultType, resultAlloca, 0, "err_ptr");
                ctx.builder->CreateStore(errValue, errPtr);
                
                // Store val field (index 1) with type checking
                if (valField) {
                    // TYPE VALIDATION: Check if val type matches declared return type
                    if (valField->getType() != expectedValType) {
                        // Allow integer type conversions
                        if (valField->getType()->isIntegerTy() && expectedValType->isIntegerTy()) {
                            valField = ctx.builder->CreateIntCast(valField, expectedValType, true, "val_cast");
                        } else {
                            throw std::runtime_error(
                                "Type mismatch in result object: function declared return type '" + 
                                ctx.currentFunctionReturnType + "' but val field has different type"
                            );
                        }
                    }
                    
                    Value* valPtr = ctx.builder->CreateStructGEP(resultType, resultAlloca, 1, "val_ptr");
                    ctx.builder->CreateStore(valField, valPtr);
                } else {
                    // Val field is required in result objects
                    throw std::runtime_error("Result object missing 'val' field");
                }
                
                // Load and return the struct
                return ctx.builder->CreateLoad(resultType, resultAlloca, "result_val");
            } else {
                // Generic object literal: { x: 10, y: 20 }
                // Create anonymous struct type on the fly
                
                std::vector<Type*> fieldTypes;
                std::vector<Value*> fieldValues;
                std::vector<std::string> fieldNames;
                
                // 1. Evaluate all fields to determine types
                for (auto& field : obj->fields) {
                    Value* val = visitExpr(field.value.get());
                    if (!val) {
                        throw std::runtime_error("Invalid field value in object literal for field: " + field.name);
                    }
                    
                    fieldTypes.push_back(val->getType());
                    fieldValues.push_back(val);
                    fieldNames.push_back(field.name);
                }
                
                // 2. Create Anonymous Struct Type (not packed to allow natural alignment)
                StructType* anonType = StructType::get(ctx.llvmContext, fieldTypes, /*isPacked=*/false);
                
                // 3. Allocate storage for the struct
                AllocaInst* objAlloca = ctx.builder->CreateAlloca(anonType, nullptr, "anon_obj");
                
                // 4. Store each field value
                for (size_t i = 0; i < fieldValues.size(); ++i) {
                    // GEP to get pointer to field index i
                    Value* fieldPtr = ctx.builder->CreateStructGEP(anonType, objAlloca, i, fieldNames[i] + "_ptr");
                    ctx.builder->CreateStore(fieldValues[i], fieldPtr);
                }
                
                // 5. Return the loaded struct value
                // Note: In LLVM, structs are first-class values
                return ctx.builder->CreateLoad(anonType, objAlloca, "anon_val");
            }
        }
        if (auto* arr = dynamic_cast<aria::frontend::ArrayLiteral*>(node)) {
            // Array literal: [1, 2, 3, 4, 5]
            if (arr->elements.empty()) {
                throw std::runtime_error("Empty array literals not yet supported");
            }
            
            // Evaluate all elements
            std::vector<Value*> values;
            Type* elemType = nullptr;
            for (auto& elem : arr->elements) {
                Value* val = visitExpr(elem.get());
                values.push_back(val);
                if (!elemType) {
                    elemType = val->getType();
                } else if (val->getType() != elemType) {
                    throw std::runtime_error("Array literal elements must have same type");
                }
            }
            
            // Create array type
            uint64_t arraySize = values.size();
            ArrayType* arrayType = ArrayType::get(elemType, arraySize);
            
            // Allocate array on stack
            AllocaInst* arrayAlloca = ctx.builder->CreateAlloca(arrayType, nullptr, "array_lit");
            
            // Store each element
            for (uint64_t i = 0; i < arraySize; i++) {
                Value* indices[] = {
                    ConstantInt::get(Type::getInt64Ty(ctx.llvmContext), 0),
                    ConstantInt::get(Type::getInt64Ty(ctx.llvmContext), i)
                };
                Value* elemPtr = ctx.builder->CreateGEP(arrayType, arrayAlloca, indices, "elem_ptr");
                ctx.builder->CreateStore(values[i], elemPtr);
            }
            
            // Return pointer to array (decays to pointer)
            Value* indices[] = {
                ConstantInt::get(Type::getInt64Ty(ctx.llvmContext), 0),
                ConstantInt::get(Type::getInt64Ty(ctx.llvmContext), 0)
            };
            return ctx.builder->CreateGEP(arrayType, arrayAlloca, indices, "array_ptr");
        }
        if (auto* idx = dynamic_cast<aria::frontend::IndexExpr*>(node)) {
            // Array indexing: arr[i]
            Value* arrayPtr = visitExpr(idx->array.get());
            Value* index = visitExpr(idx->index.get());
            
            if (!arrayPtr || !index) return nullptr;
            
            Type* arrayPtrType = arrayPtr->getType();
            
            // We need to determine the element type
            // First, try to get it from the identifier's Aria type
            Type* elementType = nullptr;
            std::string ariaElementType = "";
            
            // Try to find the array variable in symbol table to get its Aria type
            if (auto* varRef = dynamic_cast<aria::frontend::VarExpr*>(idx->array.get())) {
                auto* sym = ctx.lookup(varRef->name);
                if (sym && !sym->ariaType.empty()) {
                    // Found the variable, parse its Aria type to get element type
                    std::string ariaType = sym->ariaType;
                    
                    // Parse array type: "int64[5]" or "int64[]" -> element type is "int64"
                    size_t bracketPos = ariaType.find('[');
                    if (bracketPos != std::string::npos) {
                        ariaElementType = ariaType.substr(0, bracketPos);
                        elementType = ctx.getLLVMType(ariaElementType);
                    }
                }
            }
            
            // If we still don't have element type, try to infer from pointer type
            if (!elementType && arrayPtrType->isPointerTy()) {
                // For array literals, the pointer points to the first element already
                // We need to look at what the pointer points to
                // This is a fallback - prefer the symbol table approach above
                elementType = Type::getInt64Ty(ctx.llvmContext); // Default fallback
            }
            
            if (!elementType) {
                throw std::runtime_error("Array indexing requires pointer or array type");
            }
            
            // Handle pointer to array element
            if (arrayPtrType->isPointerTy()) {
                // Get element pointer
                Value* elemPtr = ctx.builder->CreateGEP(
                    elementType,
                    arrayPtr,
                    index,
                    "elem_ptr"
                );
                // Load element
                return ctx.builder->CreateLoad(elementType, elemPtr, "elem");
            }
            
            throw std::runtime_error("Array indexing requires pointer or array type");
        }
        if (auto* member = dynamic_cast<aria::frontend::MemberAccess*>(node)) {
            // Access struct member: obj.field
            Value* obj = visitExpr(member->object.get());
            if (!obj) return nullptr;
            
            // Handle both direct struct values and pointers to structs
            Type* objType = obj->getType();
            StructType* structType = nullptr;
            Value* structPtr = nullptr;
            std::string structName;
            
            if (auto* st = dyn_cast<StructType>(objType)) {
                // Direct struct value - allocate temp and store
                structType = st;
                structName = structType->getName().str();
                structPtr = ctx.builder->CreateAlloca(structType, nullptr, "temp");
                ctx.builder->CreateStore(obj, structPtr);
            } else if (objType->isPointerTy()) {
                // Could be a pointer to a struct - load it
                Value* loaded = ctx.builder->CreateLoad(PointerType::getUnqual(ctx.llvmContext), obj);
                if (auto* st = dyn_cast<StructType>(loaded->getType())) {
                    structType = st;
                    structName = structType->getName().str();
                    structPtr = ctx.builder->CreateAlloca(structType, nullptr, "temp");
                    ctx.builder->CreateStore(loaded, structPtr);
                }
            }
            
            if (!structType || !structPtr) {
                throw std::runtime_error("Member access on non-struct type");
            }
            
            // Get field index by name
            unsigned fieldIndex = 0;
            
            // Check if this is a result type (special handling for err/val)
            if (member->member_name == "err") {
                fieldIndex = 0;
            } else if (member->member_name == "val") {
                fieldIndex = 1;
            } else {
                // User-defined struct - look up field in metadata
                auto structIt = ctx.structFieldMaps.find(structName);
                if (structIt == ctx.structFieldMaps.end()) {
                    throw std::runtime_error("No field metadata found for struct: " + structName);
                }
                
                auto fieldIt = structIt->second.find(member->member_name);
                if (fieldIt == structIt->second.end()) {
                    throw std::runtime_error("Unknown field '" + member->member_name + "' in struct " + structName);
                }
                
                fieldIndex = fieldIt->second;
            }
            
            // Extract field
            Value* fieldPtr = ctx.builder->CreateStructGEP(structType, structPtr, fieldIndex, member->member_name + "_ptr");
            Type* fieldType = structType->getElementType(fieldIndex);
            return ctx.builder->CreateLoad(fieldType, fieldPtr, member->member_name);
        }
        if (auto* lambda = dynamic_cast<aria::frontend::LambdaExpr*>(node)) {
            // Generate anonymous function for lambda
            static int lambdaCounter = 0;
            std::string lambdaName = "lambda_" + std::to_string(lambdaCounter++);
            
            // 1. Create function type
            std::vector<Type*> paramTypes;
            for (auto& param : lambda->parameters) {
                paramTypes.push_back(ctx.getLLVMType(param.type));
            }
            
            // Functions ALWAYS return result<T> where T is the declared return type
            // The return type in lambda->return_type is the VAL type, not the full result type
            Type* returnType = ctx.getResultType(lambda->return_type);
            FunctionType* funcType = FunctionType::get(returnType, paramTypes, false);
            
            // 2. Create function
            Function* func = Function::Create(
                funcType,
                Function::InternalLinkage,  // Internal linkage for lambdas
                lambdaName,
                ctx.module.get()
            );
            
            // 3. Set parameter names
            unsigned idx = 0;
            for (auto& arg : func->args()) {
                arg.setName(lambda->parameters[idx++].name);
            }
            
            // 4. Create entry basic block
            BasicBlock* entry = BasicBlock::Create(ctx.llvmContext, "entry", func);
            
            // 5. Save previous state and set new function context
            Function* prevFunc = ctx.currentFunction;
            BasicBlock* prevBlock = ctx.builder->GetInsertBlock();
            std::string prevReturnType = ctx.currentFunctionReturnType;
            bool prevAutoWrap = ctx.currentFunctionAutoWrap;
            
            ctx.currentFunction = func;
            ctx.currentFunctionReturnType = lambda->return_type;  // Store VAL type for validation
            ctx.currentFunctionAutoWrap = lambda->auto_wrap;       // Store auto-wrap flag
            ctx.builder->SetInsertPoint(entry);
            
            // CRITICAL: Clear defer stacks for new function (defers don't persist across functions)
            ctx.deferStacks = std::vector<std::vector<Block*>>();
            ctx.deferStacks.emplace_back();  // Start with one scope for the function
            
            // 6. Create allocas for parameters
            // Save and create new scope for lambda parameters
            std::vector<std::pair<std::string, CodeGenContext::Symbol*>> savedSymbols;
            
            idx = 0;
            for (auto& arg : func->args()) {
                Type* argType = arg.getType();
                AllocaInst* alloca = ctx.builder->CreateAlloca(argType, nullptr, arg.getName());
                ctx.builder->CreateStore(&arg, alloca);
                
                // Save any existing symbol with this name
                std::string argName = std::string(arg.getName());
                auto* existingSym = ctx.lookup(argName);
                if (existingSym) {
                    savedSymbols.push_back({argName, existingSym});
                }
                
                // Store parameter with its Aria type from the lambda parameter list
                std::string paramAriaType = lambda->parameters[idx].type;
                ctx.define(argName, alloca, true, paramAriaType);
                idx++; // Increment for next parameter
            }
            
            // 7. Generate lambda body
            if (lambda->body) {
                lambda->body->accept(*this);
            }
            
            // 8. Add return if missing
            if (ctx.builder->GetInsertBlock()->getTerminator() == nullptr) {
                if (returnType->isVoidTy()) {
                    ctx.builder->CreateRetVoid();
                } else {
                    ctx.builder->CreateRet(Constant::getNullValue(returnType));
                }
            }
            
            // 9. Restore previous symbols
            for (auto& pair : savedSymbols) {
                ctx.define(pair.first, pair.second->val, pair.second->is_ref);
            }
            
            // 10. Restore previous function context
            ctx.currentFunction = prevFunc;
            ctx.currentFunctionReturnType = prevReturnType;
            ctx.currentFunctionAutoWrap = prevAutoWrap;
            if (prevBlock) {
                ctx.builder->SetInsertPoint(prevBlock);
            }
            
            // 11. If immediately invoked, call the lambda
            if (lambda->is_immediately_invoked) {
                // Evaluate arguments
                std::vector<Value*> args;
                for (size_t i = 0; i < lambda->call_arguments.size(); i++) {
                    Value* argVal = visitExpr(lambda->call_arguments[i].get());
                    if (!argVal) {
                        throw std::runtime_error("Failed to evaluate lambda argument");
                    }
                    
                    // Cast argument to match parameter type if needed
                    if (i < func->arg_size()) {
                        Type* expectedType = func->getFunctionType()->getParamType(i);
                        if (argVal->getType() != expectedType) {
                            // If both are integers, perform cast
                            if (argVal->getType()->isIntegerTy() && expectedType->isIntegerTy()) {
                                argVal = ctx.builder->CreateIntCast(argVal, expectedType, true);
                            }
                        }
                    }
                    
                    args.push_back(argVal);
                }
                
                // Call the lambda and return its result
                return ctx.builder->CreateCall(func, args, "lambda_result");
            } else {
                // Return function pointer (for passing lambdas as values)
                return func;
            }
        }
        if (auto* unwrap = dynamic_cast<aria::frontend::UnwrapExpr*>(node)) {
            // ? operator: unwrap Result/output type
            // Syntax: result ? default
            // If result.err is 0 (success), return result.val
            // If result.err is not 0 (error), return default value
            
            Value* resultVal = visitExpr(unwrap->expression.get());
            if (!resultVal) return nullptr;
            
            Value* defaultVal = visitExpr(unwrap->default_value.get());
            if (!defaultVal) return nullptr;
            
            // Assume Result/output is a struct with err (i8) and val fields
            if (auto* structType = dyn_cast<StructType>(resultVal->getType())) {
                Function* func = ctx.builder->GetInsertBlock()->getParent();
                
                // Extract err field (index 0)
                AllocaInst* tempAlloca = ctx.builder->CreateAlloca(structType, nullptr, "result_temp");
                ctx.builder->CreateStore(resultVal, tempAlloca);
                
                Value* errPtr = ctx.builder->CreateStructGEP(structType, tempAlloca, 0, "err_ptr");
                Type* errType = structType->getElementType(0);
                Value* errVal = ctx.builder->CreateLoad(errType, errPtr, "err");
                
                // Check if err is 0 (success)
                Value* isSuccess = ctx.builder->CreateICmpEQ(
                    errVal, 
                    ConstantInt::get(Type::getInt8Ty(ctx.llvmContext), 0),
                    "is_success"
                );
                
                // Extract val field (index 1)
                Value* valPtr = ctx.builder->CreateStructGEP(structType, tempAlloca, 1, "val_ptr");
                Type* valType = structType->getElementType(1);
                Value* valVal = ctx.builder->CreateLoad(valType, valPtr, "val");
                
                // If val and default have different types, cast default to match val type
                if (defaultVal->getType() != valType) {
                    if (defaultVal->getType()->isIntegerTy() && valType->isIntegerTy()) {
                        defaultVal = ctx.builder->CreateIntCast(defaultVal, valType, true);
                    }
                }
                
                // Use select: if err == 0, return val, else return default
                return ctx.builder->CreateSelect(isSuccess, valVal, defaultVal, "unwrap_result");
            }
            
            // If not a struct, just return the value
            return resultVal;
        }
        if (auto* cast = dynamic_cast<aria::frontend::CastExpr*>(node)) {
            // Cast expression: (Type)expr
            // For wildx → func pointer casts, we need special handling
            
            Value* sourceValue = visitExpr(cast->expression.get());
            if (!sourceValue) return nullptr;
            
            // Check if this is a function pointer cast (wildx → func)
            // Target type will be something like "BinaryFunc" or "func"
            if (cast->target_type.find("Func") != std::string::npos || cast->target_type == "func") {
                // This is a function pointer cast
                // The source should be a wildx pointer (ptr type)
                
                // For now, we'll use bitcast to convert the wildx pointer to a function pointer
                // In a full implementation, we'd:
                // 1. Verify source is from wildx allocation
                // 2. Look up the function signature for the target type
                // 3. Create appropriate function pointer type
                
                // Simple approach: treat as opaque pointer that can be called
                // The actual function signature will be resolved at call site
                return sourceValue;  // Pointer is already correct type
            }
            
            // Handle integer casts
            Type* targetType = ctx.getLLVMType(cast->target_type);
            if (sourceValue->getType()->isIntegerTy() && targetType->isIntegerTy()) {
                return ctx.builder->CreateIntCast(sourceValue, targetType, true, "cast");
            }
            
            // Handle pointer casts
            if (sourceValue->getType()->isPointerTy() && targetType->isPointerTy()) {
                return ctx.builder->CreateBitCast(sourceValue, targetType, "ptrcast");
            }
            
            // If types match, no cast needed
            if (sourceValue->getType() == targetType) {
                return sourceValue;
            }
            
            // For other cases, try bitcast as fallback
            return ctx.builder->CreateBitCast(sourceValue, targetType, "cast");
        }
        //... Handle other ops...
        return nullptr;
    }

    // AST Visitor Stubs
    void visit(Block* node) override { 
        for(auto& s: node->statements) {
            s->accept(*this);
        }
    }
    
    void visit(IfStmt* node) override {
        // Generate if-then-else control flow
        Value* condVal = visitExpr(node->condition.get());
        if (!condVal) return;
        
        // Convert condition to bool (i1)
        if (condVal->getType() != Type::getInt1Ty(ctx.llvmContext)) {
            // Compare to zero (false if zero, true otherwise)
            condVal = ctx.builder->CreateICmpNE(
                condVal,
                Constant::getNullValue(condVal->getType()),
                "ifcond"
            );
        }
        
        Function* func = ctx.builder->GetInsertBlock()->getParent();
        
        // Create blocks for then, else, and merge
        BasicBlock* thenBB = BasicBlock::Create(ctx.llvmContext, "then", func);
        BasicBlock* elseBB = node->else_block ? 
            BasicBlock::Create(ctx.llvmContext, "else") : nullptr;
        BasicBlock* mergeBB = BasicBlock::Create(ctx.llvmContext, "ifcont");
        
        // Branch based on condition
        if (elseBB) {
            ctx.builder->CreateCondBr(condVal, thenBB, elseBB);
        } else {
            ctx.builder->CreateCondBr(condVal, thenBB, mergeBB);
        }
        
        // Emit then block
        ctx.builder->SetInsertPoint(thenBB);
        if (node->then_block) {
            node->then_block->accept(*this);
        }
        // Branch to merge if no terminator
        if (!ctx.builder->GetInsertBlock()->getTerminator()) {
            ctx.builder->CreateBr(mergeBB);
        }
        
        // Emit else block if present
        if (elseBB) {
            func->insert(func->end(), elseBB);
            ctx.builder->SetInsertPoint(elseBB);
            if (node->else_block) {
                node->else_block->accept(*this);
            }
            if (!ctx.builder->GetInsertBlock()->getTerminator()) {
                ctx.builder->CreateBr(mergeBB);
            }
        }
        
        // Emit merge block
        func->insert(func->end(), mergeBB);
        ctx.builder->SetInsertPoint(mergeBB);
    }
    
    void visit(BreakStmt* node) override {
        if (ctx.currentLoopBreakTarget) {
            ctx.builder->CreateBr(ctx.currentLoopBreakTarget);
        }
    }
    
    void visit(ContinueStmt* node) override {
        if (ctx.currentLoopContinueTarget) {
            ctx.builder->CreateBr(ctx.currentLoopContinueTarget);
        }
    }
    
    void visit(DeferStmt* node) override {
        // defer { body }
        // Add the defer block to the stack - it will execute at scope exit
        if (node->body) {
            ctx.pushDefer(node->body.get());
        }
    }
    
    void visit(frontend::AsyncBlock* node) override {
        // async { body } catch (err:e) { catch_body }
        // Implements LLVM coroutine lowering for async/await support
        
        // 1. Create async function wrapper
        std::string asyncName = "__async_" + std::to_string(reinterpret_cast<uintptr_t>(node));
        
        // Async functions return an i8* handle (the coroutine handle)
        FunctionType* asyncFuncType = FunctionType::get(
            PointerType::getUnqual(ctx.llvmContext),  // Returns i8* handle
            {},  // No parameters (TODO: capture variables from outer scope)
            false
        );
        
        Function* asyncFn = Function::Create(
            asyncFuncType,
            Function::InternalLinkage,
            asyncName,
            ctx.module.get()
        );
        
        // 2. Save current context
        BasicBlock* callerBlock = ctx.builder->GetInsertBlock();
        Function* prevFunc = ctx.currentFunction;
        
        // 3. Setup async function entry block
        BasicBlock* entry = BasicBlock::Create(ctx.llvmContext, "entry", asyncFn);
        ctx.builder->SetInsertPoint(entry);
        ctx.currentFunction = asyncFn;
        
        // 4. Emit LLVM coroutine intrinsics
        // llvm.coro.id - Identifies this function as a coroutine
        Function* coroId = Intrinsic::getDeclaration(ctx.module.get(), Intrinsic::coro_id);
        Value* id = ctx.builder->CreateCall(coroId, {
            ConstantInt::get(Type::getInt32Ty(ctx.llvmContext), 0),  // Alignment
            ConstantPointerNull::get(PointerType::getUnqual(ctx.llvmContext)),  // Promise
            ConstantPointerNull::get(PointerType::getUnqual(ctx.llvmContext)),  // Corofn
            ConstantPointerNull::get(PointerType::getUnqual(ctx.llvmContext))   // Data
        }, "id");
        
        // llvm.coro.begin - Allocate coroutine frame
        Function* coroBegin = Intrinsic::getDeclaration(ctx.module.get(), Intrinsic::coro_begin);
        Value* hdl = ctx.builder->CreateCall(coroBegin, {
            id,
            ConstantPointerNull::get(PointerType::getUnqual(ctx.llvmContext))  // Allocfn result
        }, "hdl");
        
        // 5. Generate async body
        if (node->body) {
            node->body->accept(*this);
        }
        
        // 6. Final suspend point
        // llvm.coro.suspend - Suspend execution
        Function* coroSuspend = Intrinsic::getDeclaration(ctx.module.get(), Intrinsic::coro_suspend);
        Value* suspendResult = ctx.builder->CreateCall(coroSuspend, {
            ConstantTokenNone::get(ctx.llvmContext),  // Token
            ConstantInt::get(Type::getInt1Ty(ctx.llvmContext), 1)  // Final suspend = true
        }, "suspend");
        
        // Create switch on suspend result
        // 0 = resume, 1 = destroy, -1 = suspend
        SwitchInst* suspendSwitch = ctx.builder->CreateSwitch(suspendResult, 
            BasicBlock::Create(ctx.llvmContext, "suspend_unreachable", asyncFn), 2);
        
        // Case 0: Resume (should not happen for final suspend)
        BasicBlock* resumeBB = BasicBlock::Create(ctx.llvmContext, "resume", asyncFn);
        suspendSwitch->addCase(ConstantInt::get(Type::getInt8Ty(ctx.llvmContext), 0), resumeBB);
        ctx.builder->SetInsertPoint(resumeBB);
        ctx.builder->CreateUnreachable();
        
        // Case 1: Destroy
        BasicBlock* destroyBB = BasicBlock::Create(ctx.llvmContext, "destroy", asyncFn);
        suspendSwitch->addCase(ConstantInt::get(Type::getInt8Ty(ctx.llvmContext), 1), destroyBB);
        ctx.builder->SetInsertPoint(destroyBB);
        
        // llvm.coro.end - Finalize coroutine
        Function* coroEnd = Intrinsic::getDeclaration(ctx.module.get(), Intrinsic::coro_end);
        ctx.builder->CreateCall(coroEnd, {
            hdl,
            ConstantInt::get(Type::getInt1Ty(ctx.llvmContext), 0)  // Unwind = false
        });
        ctx.builder->CreateRet(hdl);
        
        // Unreachable block for -1 case
        ctx.builder->SetInsertPoint(suspendSwitch->getDefaultDest());
        ctx.builder->CreateUnreachable();
        
        // 7. Restore context and call async function from caller
        ctx.builder->SetInsertPoint(callerBlock);
        ctx.currentFunction = prevFunc;
        
        // Call the async function (returns coroutine handle)
        Value* coroHandle = ctx.builder->CreateCall(asyncFn, {}, "async_handle");
        
        // TODO: Store handle for later resumption
        // For now, async blocks execute but don't integrate with a scheduler
        
        // 8. Handle catch block if present
        if (node->catch_block) {
            // TODO: Implement error handling for async exceptions
            // This requires wrapping the async body in try/catch logic
            // For v0.0.7, we'll leave this as a placeholder
        }
    }
    
    void visit(frontend::AwaitExpr* node) override {
        // await <expression>
        // Suspends current coroutine until the awaited expression completes
        
        // For v0.0.7: Basic implementation
        // Evaluate the expression (should return a coroutine handle)
        // In a full implementation, this would:
        // 1. Call llvm.coro.save to save the coroutine state
        // 2. Call llvm.coro.suspend to suspend
        // 3. Resume when the awaited coroutine completes
        
        // For now, just evaluate the expression and continue
        // TODO: Full coroutine suspension/resumption logic
        if (node->expression) {
            visitExpr(node->expression.get());
        }
    }
    
    void visit(frontend::WhenExpr* node) override {
        // when { case1 then result1; case2 then result2; end }
        // Pattern matching expression - returns a value
        
        // TODO: Implement when expression lowering
        // This is similar to pick but returns a value instead of jumping
        // For v0.0.7, this is a placeholder
    }
    
    void visit(IntLiteral* node) override {} // Handled by visitExpr()
    void visit(frontend::FloatLiteral* node) override {} // Handled by visitExpr()
    void visit(BoolLiteral* node) override {} // Handled by visitExpr()
    void visit(NullLiteral* node) override {} // Handled by visitExpr()
    void visit(frontend::StringLiteral* node) override {} // Handled by visitExpr()
    void visit(frontend::TemplateString* node) override {} // Handled by visitExpr()
    void visit(frontend::TernaryExpr* node) override {} // Handled by visitExpr()
    void visit(frontend::ObjectLiteral* node) override {} // Handled by visitExpr()
    void visit(frontend::MemberAccess* node) override {} // Handled by visitExpr()
    void visit(frontend::ArrayLiteral* node) override {} // Handled by visitExpr()
    void visit(frontend::IndexExpr* node) override {} // Handled by visitExpr()
    void visit(frontend::LambdaExpr* node) override {} // Handled by visitExpr()
    void visit(VarExpr* node) override {}
    void visit(CallExpr* node) override {
        // Look up function in symbol table first, then module
        Function* callee = nullptr;
        auto* sym = ctx.lookup(node->function_name);
        if (sym && !sym->is_ref) {
            callee = dyn_cast_or_null<Function>(sym->val);
        }
        if (!callee) {
            callee = ctx.module->getFunction(node->function_name);
        }
        
        if (!callee) {
            std::string errorMsg = "Undefined function '" + node->function_name + "'";
            errorMsg += "\n\nMake sure the function is declared before it's called.";
            throw std::runtime_error(errorMsg);
        }
        
        // Evaluate arguments
        std::vector<Value*> args;
        for (auto& arg : node->arguments) {
            args.push_back(visitExpr(arg.get()));
        }
        
        // Create call instruction
        ctx.builder->CreateCall(callee, args, "calltmp");
    }
    void visit(BinaryOp* node) override {} // Handled by visitExpr()
    void visit(UnaryOp* node) override {} // Handled by visitExpr()
    
    void visit(ReturnStmt* node) override {
        // Execute all defers from the current function only (first scope in deferStacks after clear)
        // We only execute defers from deferStacks[0] since we clear at function entry
        if (!ctx.deferStacks.empty() && !ctx.deferStacks[0].empty()) {
            // Execute in LIFO order
            for (auto it = ctx.deferStacks[0].rbegin(); it != ctx.deferStacks[0].rend(); ++it) {
                (*it)->accept(*this);
            }
        }
        
        if (node->value) {
            // Return with value
            Value* retVal = visitExpr(node->value.get());
            if (retVal) {
                Type* expectedReturnType = ctx.currentFunction->getReturnType();
                
                // Check if this function uses auto-wrap (*)
                if (ctx.currentFunctionAutoWrap) {
                    // AUTO-WRAP: Wrap raw value in {err:NULL, val:retVal}
                    // Expected return type is result<T>, need to create the struct
                    
                    StructType* resultType = dyn_cast<StructType>(expectedReturnType);
                    if (!resultType) {
                        throw std::runtime_error("Auto-wrap function must return result type");
                    }
                    
                    // Check if return value is already a Result struct (from explicit {err:x, val:y})
                    if (retVal->getType() == expectedReturnType) {
                        // Already a Result - user provided explicit {err:x, val:y}, don't wrap again
                        ctx.builder->CreateRet(retVal);
                        return;
                    }
                    
                    // Get expected val field type
                    Type* expectedValType = resultType->getElementType(1);
                    
                    // Type check and cast if needed
                    if (retVal->getType() != expectedValType) {
                        if (retVal->getType()->isIntegerTy() && expectedValType->isIntegerTy()) {
                            retVal = ctx.builder->CreateIntCast(retVal, expectedValType, true, "auto_wrap_cast");
                        } else {
                            throw std::runtime_error(
                                "Auto-wrap type mismatch: function declared return type '" + 
                                ctx.currentFunctionReturnType + "' but return value has different type"
                            );
                        }
                    }
                    
                    // Create result struct: {err:0, val:retVal}
                    AllocaInst* resultAlloca = ctx.builder->CreateAlloca(resultType, nullptr, "auto_wrap_result");
                    
                    // Set err = 0 (success)
                    Value* errPtr = ctx.builder->CreateStructGEP(resultType, resultAlloca, 0, "err_ptr");
                    ctx.builder->CreateStore(ConstantInt::get(Type::getInt8Ty(ctx.llvmContext), 0), errPtr);
                    
                    // Set val = retVal
                    Value* valPtr = ctx.builder->CreateStructGEP(resultType, resultAlloca, 1, "val_ptr");
                    ctx.builder->CreateStore(retVal, valPtr);
                    
                    // Load and return the struct
                    Value* resultVal = ctx.builder->CreateLoad(resultType, resultAlloca, "result_val");
                    ctx.builder->CreateRet(resultVal);
                } else {
                    // NO AUTO-WRAP: Return value must already be a result struct
                    // OR function must return a non-result type (void, int, etc.)
                    
                    // Check if expected return type is a result struct
                    StructType* resultType = dyn_cast<StructType>(expectedReturnType);
                    bool expectingResult = resultType && resultType->getName().starts_with("result_");
                    
                    if (expectingResult) {
                        // Function returns result<T> - return value MUST be a result struct
                        if (retVal->getType() != expectedReturnType) {
                            throw std::runtime_error(
                                "Type error: Function '" + std::string(ctx.currentFunction->getName()) + 
                                "' returns result<" + ctx.currentFunctionReturnType + 
                                "> but 'return' statement provides a plain value.\n" +
                                "Use 'pass(value)' for success or 'fail(errorCode)' for errors.\n" +
                                "Example: pass(42) or fail(1)"
                            );
                        }
                        // Correct - returning a result struct
                        ctx.builder->CreateRet(retVal);
                    } else {
                        // Function returns plain type (void, int, etc.) - allow plain return
                        // Cast/truncate return value to match function return type if needed
                        if (retVal->getType() != expectedReturnType) {
                            if (retVal->getType()->isIntegerTy() && expectedReturnType->isIntegerTy()) {
                                retVal = ctx.builder->CreateIntCast(retVal, expectedReturnType, true);
                            }
                            // If types still don't match, it's an error (should be caught by type checker)
                        }
                        ctx.builder->CreateRet(retVal);
                    }
                }
            }
        } else {
            // Return void
            ctx.builder->CreateRetVoid();
        }
    }

    // Module System Visitors
    // =============================================================================
    
    void visit(frontend::UseStmt* node) override {
        // For now, use statements are no-ops in codegen
        // They will be handled by a linker/module loader in the future
        // The symbol resolution happens at link time
    }
    
    void visit(frontend::ModDef* node) override {
        // Module definition creates a namespace
        // For LLVM IR, we prefix all symbols with the module name
        
        // Save current module prefix
        std::string previousPrefix = ctx.currentModulePrefix;
        
        // Set new prefix: mod_name.
        if (previousPrefix.empty()) {
            ctx.currentModulePrefix = node->name + ".";
        } else {
            ctx.currentModulePrefix = previousPrefix + node->name + ".";
        }
        
        // Visit module body
        if (node->body) {
            node->body->accept(*this);
        }
        
        // Restore previous prefix
        ctx.currentModulePrefix = previousPrefix;
    }
    
    void visit(frontend::ExternBlock* node) override {
        // Extern blocks declare C functions without generating definitions
        // Just process the declarations (they should be function declarations)
        for (auto& decl : node->declarations) {
            decl->accept(*this);
        }
    }
};

// Implementation of executeScopeDefers
void CodeGenContext::executeScopeDefers(CodeGenVisitor* visitor) {
    if (deferStacks.empty()) return;
    
    auto& currentDefers = deferStacks.back();
    // Execute in LIFO order (reverse)
    for (auto it = currentDefers.rbegin(); it != currentDefers.rend(); ++it) {
        (*it)->accept(*visitor);
    }
}

// =============================================================================
// Main Entry Point for Code Generation
// =============================================================================

bool generate_code(aria::frontend::Block* root, const std::string& filename, bool enableVerify) {
    try {
        CodeGenContext ctx("aria_module");
        CodeGenVisitor visitor(ctx);

        // Declare built-in print function (uses C puts)
        // print(string) -> void
        std::vector<Type*> printParams = {PointerType::get(Type::getInt8Ty(ctx.llvmContext), 0)};
        FunctionType* printType = FunctionType::get(
            Type::getVoidTy(ctx.llvmContext),
            printParams,
            false  // not vararg
        );
        Function::Create(printType, Function::ExternalLinkage, "puts", ctx.module.get());
        
        // Create alias for Aria 'print' function
        Function::Create(printType, Function::ExternalLinkage, "print", ctx.module.get());

        // JavaScript-style module execution:
        // Module-level code runs in a global initializer function
        // This allows lambdas, variable initializers, and statements at module scope
        FunctionType* moduleInitType = FunctionType::get(Type::getVoidTy(ctx.llvmContext), false);
        Function* moduleInit = Function::Create(
            moduleInitType, 
            Function::InternalLinkage, 
            "__aria_module_init", 
            ctx.module.get()
        );
        BasicBlock* moduleEntry = BasicBlock::Create(ctx.llvmContext, "entry", moduleInit);
        
        // Set insertion point for module-level code
        ctx.builder->SetInsertPoint(moduleEntry);
        ctx.currentFunction = moduleInit;

        // Generate IR for module-level code (functions, variables, statements)
        root->accept(visitor);
    
    ctx.builder->CreateRetVoid();
    
    // Verify module init function
    verifyFunction(*moduleInit);

    // Now create the actual main() that calls module init and user's main (if exists)
    // First check if user defined a main function
    Function* userMainFunc = ctx.module->getFunction("main");
    
    if (userMainFunc) {
        // User defined main() - rename it to __user_main and create wrapper
        userMainFunc->setName("__user_main");
    }
    
    // Create the C main() entry point
    FunctionType* mainType = FunctionType::get(Type::getInt64Ty(ctx.llvmContext), false);
    Function* mainFunc = Function::Create(mainType, Function::ExternalLinkage, "main", ctx.module.get());
    BasicBlock* mainEntry = BasicBlock::Create(ctx.llvmContext, "entry", mainFunc);
    
    ctx.builder->SetInsertPoint(mainEntry);
    
    // Call module initializer
    ctx.builder->CreateCall(moduleInit);
    
    if (userMainFunc) {
        // Call user's main and return its result
        Value* result = ctx.builder->CreateCall(userMainFunc);
        // Convert to int64 for C main convention
        if (result->getType()->isIntegerTy()) {
            Value* extended = ctx.builder->CreateSExt(result, Type::getInt64Ty(ctx.llvmContext));
            ctx.builder->CreateRet(extended);
        } else {
            ctx.builder->CreateRet(ConstantInt::get(Type::getInt64Ty(ctx.llvmContext), 0));
        }
    } else {
        // No user main - just return 0
        ctx.builder->CreateRet(ConstantInt::get(Type::getInt64Ty(ctx.llvmContext), 0));
    }
    
    // Verify main function
    verifyFunction(*mainFunc);
    
    // =========================================================================
    // RUN OPTIMIZATION PASSES
    // =========================================================================
    
    // Create pass managers
    LoopAnalysisManager LAM;
    FunctionAnalysisManager FAM;
    CGSCCAnalysisManager CGAM;
    ModuleAnalysisManager MAM;
    
    // Create pass builder and register analyses
    PassBuilder PB;
    PB.registerModuleAnalyses(MAM);
    PB.registerCGSCCAnalyses(CGAM);
    PB.registerFunctionAnalyses(FAM);
    PB.registerLoopAnalyses(LAM);
    PB.crossRegisterProxies(LAM, FAM, CGAM, MAM);
    
    // Build custom optimization pipeline
    ModulePassManager MPM;
    
    // Add our custom TBB optimizer pass (runs on each function)
    FunctionPassManager FPM;
    FPM.addPass(TBBOptimizerPass());
    
    // Add the function pass manager to the module pass manager
    MPM.addPass(createModuleToFunctionPassAdaptor(std::move(FPM)));
    
    // Run the optimization pipeline
    MPM.run(*ctx.module, MAM);
    
    // =========================================================================
    
    bool verificationPassed = true;
    
    // Verify entire module for correctness (if enabled)
    if (enableVerify) {
        std::string verifyErrors;
        raw_string_ostream errorStream(verifyErrors);
        if (verifyModule(*ctx.module, &errorStream)) {
            verificationPassed = false;
            errs() << "\n========================================\n";
            errs() << "LLVM IR VERIFICATION FAILED\n";
            errs() << "========================================\n";
            errs() << errorStream.str() << "\n";
            errs() << "========================================\n";
            errs() << "Generated IR contains errors.\n";
            errs() << "Use --no-verify to output anyway (not recommended).\n";
            errs() << "========================================\n\n";
        }
    }
    
    // Emit to File (LLVM IR) even if verification failed (for debugging)
    std::error_code EC;
    raw_fd_ostream dest(filename, EC, sys::fs::OF_None);
    if (EC) {
        errs() << "Could not open file: " << EC.message();
        return false;
    }
    ctx.module->print(dest, nullptr);
    dest.flush();
    
    return verificationPassed;
    
    } catch (const std::runtime_error& e) {
        // Catch codegen errors and provide friendly error message
        std::cerr << "\n";
        std::cerr << "╔═══════════════════════════════════════════════════════════╗\n";
        std::cerr << "║  ARIA COMPILATION ERROR                                   ║\n";
        std::cerr << "╚═══════════════════════════════════════════════════════════╝\n";
        std::cerr << "\n";
        std::cerr << "Error: " << e.what() << "\n";
        std::cerr << "\n";
        std::cerr << "Compilation failed. Please fix the error and try again.\n";
        std::cerr << "\n";
        return false;
    } catch (const std::exception& e) {
        // Catch any other exceptions
        std::cerr << "\n";
        std::cerr << "╔═══════════════════════════════════════════════════════════╗\n";
        std::cerr << "║  ARIA INTERNAL ERROR                                      ║\n";
        std::cerr << "╚═══════════════════════════════════════════════════════════╝\n";
        std::cerr << "\n";
        std::cerr << "Internal compiler error: " << e.what() << "\n";
        std::cerr << "\n";
        std::cerr << "This is likely a compiler bug. Please report it at:\n";
        std::cerr << "https://github.com/alternative-intelligence-cp/aria/issues\n";
        std::cerr << "\n";
        return false;
    }
}
} // namespace backend
} // namespace aria
