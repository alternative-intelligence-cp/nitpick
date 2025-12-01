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
#include "../frontend/ast.h"
#include "../frontend/ast/control_flow.h"
#include "../frontend/ast/loops.h"
#include "../frontend/ast/defer.h"
#include "../frontend/tokens.h"

// LLVM Includes
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Verifier.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Function.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/MC/TargetRegistry.h>

#include <vector>
#include <map>
#include <stack>

using namespace llvm;

namespace aria {
namespace backend {

// =============================================================================
// Code Generation Context
// =============================================================================

class CodeGenContext {
public:
    LLVMContext llvmContext;
    std::unique_ptr<Module> module;
    std::unique_ptr<IRBuilder<>> builder;
    
    // Symbol Table: Maps variable names to LLVM Allocas or Values
    struct Symbol {
        Value* val;
        bool is_ref; // Is this a pointer to the value (alloca) or the value itself?
    };
    std::vector<std::map<std::string, Symbol>> scopeStack;

    // Current compilation state
    Function* currentFunction = nullptr;
    BasicBlock* returnBlock = nullptr;
    Value* returnValue = nullptr; // Pointer to return value storage

    CodeGenContext(std::string moduleName) {
        module = std::make_unique<Module>(moduleName, llvmContext);
        builder = std::make_unique<IRBuilder<>>(llvmContext);
        pushScope(); // Global scope
    }

    void pushScope() { scopeStack.emplace_back(); }
    void popScope() { scopeStack.pop_back(); }

    void define(const std::string& name, Value* val, bool is_ref = true) {
        scopeStack.back()[name] = {val, is_ref};
    }

    Symbol* lookup(const std::string& name) {
        for (auto it = scopeStack.rbegin(); it!= scopeStack.rend(); ++it) {
            auto found = it->find(name);
            if (found!= it->end()) return &found->second;
        }
        return nullptr;
    }

    // Helper: Map Aria Types to LLVM Types
    llvm::Type* getLLVMType(const std::string& ariaType) {
        if (ariaType == "int1") return Type::getInt1Ty(llvmContext);
        if (ariaType == "int8" |

| ariaType == "byte" |
| ariaType == "trit") return Type::getInt8Ty(llvmContext);
        if (ariaType == "int16" |

| ariaType == "tryte") return Type::getInt16Ty(llvmContext);
        if (ariaType == "int32") return Type::getInt32Ty(llvmContext);
        if (ariaType == "int64") return Type::getInt64Ty(llvmContext);
        if (ariaType == "int128") return Type::getInt128Ty(llvmContext);
        
        // Exotic Type: int512
        // Lowered to standard LLVM i512. LLVM backend handles splitting for x86.
        if (ariaType == "int512") return Type::getIntNTy(llvmContext, 512);
        
        if (ariaType == "float" |

| ariaType == "flt32") return Type::getFloatTy(llvmContext);
        if (ariaType == "double" |

| ariaType == "flt64") return Type::getDoubleTy(llvmContext);
        
        if (ariaType == "void") return Type::getVoidTy(llvmContext);
        
        // Pointers (opaque in LLVM 18)
        // We return ptr for strings, arrays, objects
        return PointerType::getUnqual(llvmContext);
    }
};

// =============================================================================
// The Code Generator Visitor
// =============================================================================

class CodeGenVisitor : public AstVisitor {
    CodeGenContext& ctx;

public:
    CodeGenVisitor(CodeGenContext& context) : ctx(context) {}

    // -------------------------------------------------------------------------
    // 1. Variable Declarations
    // -------------------------------------------------------------------------

    void visit(VarDecl* node) override {
        llvm::Type* varType = ctx.getLLVMType(node->type);
        Value* storage = nullptr;

        if (node->is_stack) {
            // Stack: Simple Alloca
            // Insert at entry block for efficiency
            BasicBlock* currentBB = ctx.builder->GetInsertBlock();
            Function* func = currentBB->getParent();
            IRBuilder<> tmpBuilder(&func->getEntryBlock(), func->getEntryBlock().begin());
            storage = tmpBuilder.CreateAlloca(varType, nullptr, node->name);
        } 
        else if (node->is_wild) {
            // Wild: aria_alloc
            uint64_t size = ctx.module->getDataLayout().getTypeAllocSize(varType);
            Value* sizeVal = ConstantInt::get(Type::getInt64Ty(ctx.llvmContext), size);
            
            Function* allocator = getOrInsertAriaAlloc();
            Value* rawPtr = ctx.builder->CreateCall(allocator, sizeVal);
            
            // We need a stack slot to hold the pointer itself (lvalue)
            storage = ctx.builder->CreateAlloca(PointerType::getUnqual(ctx.llvmContext), nullptr, node->name);
            ctx.builder->CreateStore(rawPtr, storage);
        }
        else {
            // GC: aria_gc_alloc
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
        }

        ctx.define(node->name, storage, true);

        // Initializer
        if (node->initializer) {
            Value* initVal = visitExpr(node->initializer.get());
            if (!initVal) return;

            if (node->is_stack) {
                ctx.builder->CreateStore(initVal, storage);
            } else {
                // For heap vars, 'storage' is `ptr*`, we load the `ptr` then store to it
                Value* heapPtr = ctx.builder->CreateLoad(PointerType::getUnqual(ctx.llvmContext), storage);
                // Cast heapPtr if necessary (though LLVM 18 ptr is opaque)
                ctx.builder->CreateStore(initVal, heapPtr);
            }
        }
    }

    // -------------------------------------------------------------------------
    // 2. Control Flow: Pick & Loops
    // -------------------------------------------------------------------------

    void visit(PickStmt* node) override {
        Value* selector = visitExpr(node->selector.get());
        Function* func = ctx.builder->GetInsertBlock()->getParent();
        BasicBlock* doneBB = BasicBlock::Create(ctx.llvmContext, "pick_done");
        
        // Aria's 'pick' can be complex (ranges, wildcards). 
        // We implement it as a chain of if-else blocks (Lowering to branches).
        // Optimizers will convert this to a jump table if possible.

        BasicBlock* nextCaseBB = nullptr;

        for (size_t i = 0; i < node->cases.size(); ++i) {
            auto& pcase = node->cases[i];
            
            BasicBlock* caseBodyBB = BasicBlock::Create(ctx.llvmContext, "case_body_" + std::to_string(i), func);
            
            // Logic Check
            Value* match = nullptr;
            if (pcase.type == PickCase::WILDCARD) {
                match = ConstantInt::get(Type::getInt1Ty(ctx.llvmContext), 1);
            } else {
                // Range or Exact
                // Note: Simplified. Full impl needs range checking (<9) handling
                Value* val = visitExpr(pcase.value_start.get());
                match = ctx.builder->CreateICmpEQ(selector, val, "pick_eq");
            }

            nextCaseBB = BasicBlock::Create(ctx.llvmContext, "case_next_" + std::to_string(i));
            
            ctx.builder->CreateCondBr(match, caseBodyBB, nextCaseBB);
            
            // Body Generation
            ctx.builder->SetInsertPoint(caseBodyBB);
            ctx.pushScope();
            pcase.body->accept(*this);
            ctx.popScope();

            // Auto-break (unless fallthrough logic is added here via labels)
            if (!ctx.builder->GetInsertBlock()->getTerminator()) {
                ctx.builder->CreateBr(doneBB);
            }

            // Move to next
            func->insert(func->end(), nextCaseBB);
            ctx.builder->SetInsertPoint(nextCaseBB);
        }

        // Final fallthrough to done
        ctx.builder->CreateBr(doneBB);
        func->insert(func->end(), doneBB);
        ctx.builder->SetInsertPoint(doneBB);
    }

    void visit(TillLoop* node) override {
        // Till(limit, step) with '$' iterator
        Value* limit = visitExpr(node->limit.get());
        Value* step = visitExpr(node->step.get());

        Function* func = ctx.builder->GetInsertBlock()->getParent();
        BasicBlock* preheader = ctx.builder->GetInsertBlock();
        BasicBlock* loopBB = BasicBlock::Create(ctx.llvmContext, "loop_body", func);
        BasicBlock* exitBB = BasicBlock::Create(ctx.llvmContext, "loop_exit", func);

        ctx.builder->CreateBr(loopBB);
        ctx.builder->SetInsertPoint(loopBB);

        // PHI Node for '$'
        // It starts at 0 (implicit start for till)
        PHINode* iterVar = ctx.builder->CreatePHI(Type::getInt64Ty(ctx.llvmContext), 2, "$");
        iterVar->addIncoming(ConstantInt::get(Type::getInt64Ty(ctx.llvmContext), 0), preheader);

        // Define '$' in scope
        ctx.pushScope();
        ctx.define("$", iterVar, false); // False = Value itself, not ref

        // Body
        node->body->accept(*this);
        ctx.popScope();

        // Increment
        Value* nextVal = ctx.builder->CreateAdd(iterVar, step, "next_val");
        iterVar->addIncoming(nextVal, ctx.builder->GetInsertBlock());

        // Condition
        Value* cond = ctx.builder->CreateICmpSLT(nextVal, limit, "loop_cond");
        ctx.builder->CreateCondBr(cond, loopBB, exitBB);

        ctx.builder->SetInsertPoint(exitBB);
    }

    // -------------------------------------------------------------------------
    // 3. Expressions (Helper)
    // -------------------------------------------------------------------------

    Value* visitExpr(Expression* node) {
        // Simplified Dispatcher
        if (auto* lit = dynamic_cast<IntLiteral*>(node)) {
            return ConstantInt::get(Type::getInt64Ty(ctx.llvmContext), lit->value);
        }
        if (auto* var = dynamic_cast<VarExpr*>(node)) {
            auto* sym = ctx.lookup(var->name);
            if (!sym) return nullptr;
            if (sym->is_ref) {
                // Load from stack slot
                return ctx.builder->CreateLoad(ctx.getLLVMType("int64"), sym->val); // Type inference simplified
            }
            return sym->val; // PHI or direct value
        }
        //... Handle other ops...
        return nullptr;
    }

    // AST Visitor Stubs
    void visit(Block* node) override { for(auto& s: node->statements) s->accept(*this); }
    void visit(IfStmt* node) override {} // Omitted for brevity
    void visit(DeferStmt* node) override {} 
    void visit(VarExpr* node) override {}
    void visit(CallExpr* node) override {}

private:
    // Runtime Linkage
    Function* getOrInsertAriaAlloc() {
        return Function::Create(
            FunctionType::get(PointerType::getUnqual(ctx.llvmContext), {Type::getInt64Ty(ctx.llvmContext)}, false),
            Function::ExternalLinkage, "aria_alloc", ctx.module.get());
    }
    
    Function* getOrInsertGCAlloc() {
        std::vector<Type*> args = {PointerType::getUnqual(ctx.llvmContext), Type::getInt64Ty(ctx.llvmContext)};
        return Function::Create(
            FunctionType::get(PointerType::getUnqual(ctx.llvmContext), args, false),
            Function::ExternalLinkage, "aria_gc_alloc", ctx.module.get());
    }
    
    Function* getOrInsertGetNursery() {
        return Function::Create(
            FunctionType::get(PointerType::getUnqual(ctx.llvmContext), {}, false),
            Function::ExternalLinkage, "get_current_thread_nursery", ctx.module.get());
    }
};

// =============================================================================
// Main Entry Point for Code Generation
// =============================================================================

void generate_code(Block* root, const std::string& filename) {
    CodeGenContext ctx("aria_module");
    CodeGenVisitor visitor(ctx);
    
    // Create 'main' function wrapper
    FunctionType* ft = FunctionType::get(Type::getVoidTy(ctx.llvmContext), false);
    Function* mainFunc = Function::Create(ft, Function::ExternalLinkage, "main", ctx.module.get());
    BasicBlock* entry = BasicBlock::Create(ctx.llvmContext, "entry", mainFunc);
    ctx.builder->SetInsertPoint(entry);
    ctx.currentFunction = mainFunc;

    // Generate IR
    root->accept(visitor);
    
    ctx.builder->CreateRetVoid();

    // Verify
    verifyFunction(*mainFunc);
    
    // Emit to File (LLVM IR)
    std::error_code EC;
    raw_fd_ostream dest(filename, EC, sys::fs::OF_None);
    if (EC) {
        errs() << "Could not open file: " << EC.message();
        return;
    }
    ctx.module->print(dest, nullptr);
    dest.flush();
}

} // namespace backend
} // namespace aria
