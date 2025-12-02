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
#include "../frontend/ast/stmt.h"
#include "../frontend/ast/expr.h"
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

// Import frontend types for use in this file
using aria::frontend::AstVisitor;
using aria::frontend::Block;
using aria::frontend::VarDecl;
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
using aria::frontend::VarExpr;
using aria::frontend::CallExpr;
using aria::frontend::BinaryOp;
using aria::frontend::UnaryOp;
using aria::frontend::ReturnStmt;

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
    
    // Pick statement context (for fall() statements)
    std::map<std::string, BasicBlock*>* pickLabelBlocks = nullptr;
    BasicBlock* pickDoneBlock = nullptr;

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
        if (ariaType == "int8" || ariaType == "byte" || ariaType == "trit") 
            return Type::getInt8Ty(llvmContext);
        if (ariaType == "int16" || ariaType == "tryte") 
            return Type::getInt16Ty(llvmContext);
        if (ariaType == "int32") return Type::getInt32Ty(llvmContext);
        if (ariaType == "int64") return Type::getInt64Ty(llvmContext);
        if (ariaType == "int128") return Type::getInt128Ty(llvmContext);
        
        // Exotic Type: int512
        // Lowered to standard LLVM i512. LLVM backend handles splitting for x86.
        if (ariaType == "int512") return Type::getIntNTy(llvmContext, 512);
        
        if (ariaType == "float" || ariaType == "flt32") 
            return Type::getFloatTy(llvmContext);
        if (ariaType == "double" || ariaType == "flt64") 
            return Type::getDoubleTy(llvmContext);
        
        if (ariaType == "void") return Type::getVoidTy(llvmContext);
        
        // Pointers (opaque in LLVM 18)
        // We return ptr for strings, arrays, objects
        return PointerType::getUnqual(llvmContext);
    }
};

// =============================================================================
// RAII Scope Guard for Symbol Table Management
// =============================================================================

// Ensures popScope() is called even if exceptions occur or early returns happen
// This prevents scope leaks in the symbol table
class ScopeGuard {
    CodeGenContext& ctx;
public:
    ScopeGuard(CodeGenContext& c) : ctx(c) { ctx.pushScope(); }
    ~ScopeGuard() { ctx.popScope(); }
    // Prevent copying to avoid double-pop
    ScopeGuard(const ScopeGuard&) = delete;
    ScopeGuard& operator=(const ScopeGuard&) = delete;
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

    void visit(ExpressionStmt* node) override {
        // Execute expression for side effects (e.g., function call)
        visitExpr(node->expression.get());
    }

    void visit(FuncDecl* node) override {
        // 1. Create function type
        std::vector<Type*> paramTypes;
        for (auto& param : node->parameters) {
            paramTypes.push_back(ctx.getLLVMType(param.type));
        }
        
        Type* returnType = ctx.getLLVMType(node->return_type);
        FunctionType* funcType = FunctionType::get(returnType, paramTypes, false);
        
        // 2. Create function
        Function* func = Function::Create(
            funcType,
            Function::ExternalLinkage,
            node->name,
            ctx.module.get()
        );
        
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
        ctx.currentFunction = func;
        ctx.builder->SetInsertPoint(entry);
        
        // 6. Create allocas for parameters (to allow taking addresses)
        idx = 0;
        for (auto& arg : func->args()) {
            Type* argType = arg.getType();
            AllocaInst* alloca = ctx.builder->CreateAlloca(argType, nullptr, arg.getName());
            ctx.builder->CreateStore(&arg, alloca);
            ctx.define(std::string(arg.getName()), alloca, true);
        }
        
        // 7. Generate function body
        if (node->body) {
            node->body->accept(*this);
        }
        
        // 8. Add return if missing (for void functions)
        if (ctx.builder->GetInsertBlock()->getTerminator() == nullptr) {
            if (returnType->isVoidTy()) {
                ctx.builder->CreateRetVoid();
            } else {
                // Return default value (0 or nullptr)
                ctx.builder->CreateRet(Constant::getNullValue(returnType));
            }
        }
        
        // 9. Restore previous function context and insertion point
        ctx.currentFunction = prevFunc;
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
        
        BasicBlock* nextCaseBB = nullptr;
        
        // Second pass: generate case logic
        for (size_t i = 0; i < node->cases.size(); ++i) {
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
                    match = ctx.builder->CreateICmpEQ(selector, val, "pick_eq");
                    break;
                }
                
                case PickCase::LESS_THAN: {
                    // (<value) - less than
                    Value* val = visitExpr(pcase.value_start.get());
                    match = ctx.builder->CreateICmpSLT(selector, val, "pick_lt");
                    break;
                }
                
                case PickCase::GREATER_THAN: {
                    // (>value) - greater than
                    Value* val = visitExpr(pcase.value_start.get());
                    match = ctx.builder->CreateICmpSGT(selector, val, "pick_gt");
                    break;
                }
                
                case PickCase::LESS_EQUAL: {
                    // (<=value) - less or equal
                    Value* val = visitExpr(pcase.value_start.get());
                    match = ctx.builder->CreateICmpSLE(selector, val, "pick_le");
                    break;
                }
                
                case PickCase::GREATER_EQUAL: {
                    // (>=value) - greater or equal
                    Value* val = visitExpr(pcase.value_start.get());
                    match = ctx.builder->CreateICmpSGE(selector, val, "pick_ge");
                    break;
                }
                
                case PickCase::RANGE: {
                    // (start..end) or (start...end) - range match
                    Value* start = visitExpr(pcase.value_start.get());
                    Value* end = visitExpr(pcase.value_end.get());
                    
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
        
        func->insert(func->end(), doneBB);
        ctx.builder->SetInsertPoint(doneBB);
        
        // Clear pick context
        ctx.pickLabelBlocks = nullptr;
        ctx.pickDoneBlock = nullptr;
    }
    
    void visit(FallStmt* node) override {
        // fall(label) - explicit fallthrough to labeled case in pick
        if (!ctx.pickLabelBlocks) {
            throw std::runtime_error("fall() statement outside of pick statement");
        }
        
        auto it = ctx.pickLabelBlocks->find(node->target_label);
        if (it == ctx.pickLabelBlocks->end()) {
            throw std::runtime_error("fall() target label not found: " + node->target_label);
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
        
        // Loop body
        ctx.builder->SetInsertPoint(loopBodyBB);
        if (node->body) {
            ScopeGuard guard(ctx);
            node->body->accept(*this);
        }
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
        ctx.builder->CreateCondBr(cond, loopBodyBB, exitBB);
        
        // Loop body
        ctx.builder->SetInsertPoint(loopBodyBB);
        {
            ScopeGuard guard(ctx);
            node->body->accept(*this);
        }
        
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
        if (auto* blit = dynamic_cast<aria::frontend::BoolLiteral*>(node)) {
            return ConstantInt::get(Type::getInt1Ty(ctx.llvmContext), blit->value ? 1 : 0);
        }
        if (auto* slit = dynamic_cast<aria::frontend::StringLiteral*>(node)) {
            // Create global string constant
            return ctx.builder->CreateGlobalStringPtr(slit->value);
        }
        if (auto* tstr = dynamic_cast<aria::frontend::TemplateString*>(node)) {
            // Build template string by concatenating parts
            // For now, simplified: just concatenate string representations
            // TODO: Proper string concatenation with LLVM runtime
            std::string result;
            for (auto& part : tstr->parts) {
                if (part.type == aria::frontend::TemplatePart::STRING) {
                    result += part.string_value;
                } else {
                    // For now, just append placeholder
                    // TODO: Convert expression value to string
                    result += "<expr>";
                }
            }
            return ctx.builder->CreateGlobalStringPtr(result);
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
                // Load from stack slot - get the type from the alloca
                Type* allocaType = sym->val->getType();
                if (auto* ptrType = dyn_cast<PointerType>(allocaType)) {
                    // For heap allocations, we need to load the pointer first, then load from it
                    if (sym->val->getName().contains("_heap") || isa<CallInst>(sym->val)) {
                        Value* heapPtr = ctx.builder->CreateLoad(PointerType::getUnqual(ctx.llvmContext), sym->val);
                        return ctx.builder->CreateLoad(Type::getInt64Ty(ctx.llvmContext), heapPtr);
                    }
                    // For stack allocations (parameters), get the allocated type
                    if (auto* allocaInst = dyn_cast<AllocaInst>(sym->val)) {
                        Type* elementType = allocaInst->getAllocatedType();
                        return ctx.builder->CreateLoad(elementType, sym->val);
                    }
                }
                // Fallback to int64
                return ctx.builder->CreateLoad(Type::getInt64Ty(ctx.llvmContext), sym->val);
            }
            return sym->val; // PHI or direct value
        }
        if (auto* call = dynamic_cast<aria::frontend::CallExpr*>(node)) {
            // Handle function call
            // Map 'print' to 'puts' for libc compatibility
            std::string funcName = call->function_name;
            if (funcName == "print") {
                funcName = "puts";
            }
            
            Function* callee = ctx.module->getFunction(funcName);
            if (!callee) {
                throw std::runtime_error("Unknown function: " + call->function_name);
            }
            
            std::vector<Value*> args;
            for (auto& arg : call->arguments) {
                args.push_back(visitExpr(arg.get()));
            }
            
            // Void functions shouldn't have result names
            if (callee->getReturnType()->isVoidTy()) {
                return ctx.builder->CreateCall(callee, args);
            }
            return ctx.builder->CreateCall(callee, args, "calltmp");
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
            }
        }
        if (auto* binop = dynamic_cast<aria::frontend::BinaryOp*>(node)) {
            Value* L = visitExpr(binop->left.get());
            Value* R = visitExpr(binop->right.get());
            
            if (!L || !R) return nullptr;
            
            switch (binop->op) {
                case aria::frontend::BinaryOp::ADD:
                    return ctx.builder->CreateAdd(L, R, "addtmp");
                case aria::frontend::BinaryOp::SUB:
                    return ctx.builder->CreateSub(L, R, "subtmp");
                case aria::frontend::BinaryOp::MUL:
                    return ctx.builder->CreateMul(L, R, "multmp");
                case aria::frontend::BinaryOp::DIV:
                    return ctx.builder->CreateSDiv(L, R, "divtmp");
                case aria::frontend::BinaryOp::MOD:
                    return ctx.builder->CreateSRem(L, R, "modtmp");
                case aria::frontend::BinaryOp::EQ:
                    return ctx.builder->CreateICmpEQ(L, R, "eqtmp");
                case aria::frontend::BinaryOp::NE:
                    return ctx.builder->CreateICmpNE(L, R, "netmp");
                case aria::frontend::BinaryOp::LT:
                    return ctx.builder->CreateICmpSLT(L, R, "lttmp");
                case aria::frontend::BinaryOp::GT:
                    return ctx.builder->CreateICmpSGT(L, R, "gttmp");
                case aria::frontend::BinaryOp::LE:
                    return ctx.builder->CreateICmpSLE(L, R, "letmp");
                case aria::frontend::BinaryOp::GE:
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
            // Create Result struct: { i8* err, <type> val }
            // For now, we'll create a simple struct with two fields
            std::vector<Type*> fieldTypes;
            fieldTypes.push_back(PointerType::getUnqual(ctx.llvmContext)); // err field (pointer)
            
            // Determine val field type from the actual value
            Value* valField = nullptr;
            Value* errField = nullptr;
            
            for (auto& field : obj->fields) {
                if (field.name == "err") {
                    errField = visitExpr(field.value.get());
                } else if (field.name == "val") {
                    valField = visitExpr(field.value.get());
                    if (valField) {
                        fieldTypes.push_back(valField->getType());
                    }
                }
            }
            
            // Create struct type and allocate on stack
            StructType* resultType = StructType::create(ctx.llvmContext, fieldTypes, "Result");
            AllocaInst* resultAlloca = ctx.builder->CreateAlloca(resultType, nullptr, "result");
            
            // Store err field (index 0)
            if (errField) {
                Value* errPtr = ctx.builder->CreateStructGEP(resultType, resultAlloca, 0, "err_ptr");
                ctx.builder->CreateStore(errField, errPtr);
            }
            
            // Store val field (index 1)
            if (valField) {
                Value* valPtr = ctx.builder->CreateStructGEP(resultType, resultAlloca, 1, "val_ptr");
                ctx.builder->CreateStore(valField, valPtr);
            }
            
            // Load and return the struct
            return ctx.builder->CreateLoad(resultType, resultAlloca, "result_val");
        }
        if (auto* member = dynamic_cast<aria::frontend::MemberAccess*>(node)) {
            // Access struct member: obj.field
            Value* obj = visitExpr(member->object.get());
            if (!obj) return nullptr;
            
            // For Result type, we have two fields: err (index 0) and val (index 1)
            Type* objType = obj->getType();
            if (auto* structType = dyn_cast<StructType>(objType)) {
                // Allocate temporary to hold the struct
                AllocaInst* tempAlloca = ctx.builder->CreateAlloca(structType, nullptr, "temp");
                ctx.builder->CreateStore(obj, tempAlloca);
                
                // Get field index
                unsigned fieldIndex = 0;
                if (member->member_name == "val") {
                    fieldIndex = 1;
                } else if (member->member_name == "err") {
                    fieldIndex = 0;
                }
                
                // Extract field
                Value* fieldPtr = ctx.builder->CreateStructGEP(structType, tempAlloca, fieldIndex, member->member_name + "_ptr");
                Type* fieldType = structType->getElementType(fieldIndex);
                return ctx.builder->CreateLoad(fieldType, fieldPtr, member->member_name);
            }
            
            return nullptr;
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
            
            Type* returnType = ctx.getLLVMType(lambda->return_type);
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
            ctx.currentFunction = func;
            ctx.builder->SetInsertPoint(entry);
            
            // 6. Create allocas for parameters
            idx = 0;
            for (auto& arg : func->args()) {
                Type* argType = arg.getType();
                AllocaInst* alloca = ctx.builder->CreateAlloca(argType, nullptr, arg.getName());
                ctx.builder->CreateStore(&arg, alloca);
                ctx.define(std::string(arg.getName()), alloca, true);
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
            
            // 9. Restore previous function context
            ctx.currentFunction = prevFunc;
            if (prevBlock) {
                ctx.builder->SetInsertPoint(prevBlock);
            }
            
            // 10. If immediately invoked, call the lambda
            if (lambda->is_immediately_invoked) {
                // Evaluate arguments
                std::vector<Value*> args;
                for (auto& arg : lambda->call_arguments) {
                    args.push_back(visitExpr(arg.get()));
                }
                
                // Call the lambda and return its result
                return ctx.builder->CreateCall(func, args, "lambda_result");
            } else {
                // Return function pointer (for passing lambdas as values)
                return func;
            }
        }
        //... Handle other ops...
        return nullptr;
    }

    // AST Visitor Stubs
    void visit(Block* node) override { for(auto& s: node->statements) s->accept(*this); }
    void visit(IfStmt* node) override {} // Omitted for brevity
    void visit(DeferStmt* node) override {}
    void visit(IntLiteral* node) override {} // Handled by visitExpr()
    void visit(BoolLiteral* node) override {} // Handled by visitExpr()
    void visit(frontend::StringLiteral* node) override {} // Handled by visitExpr()
    void visit(frontend::TemplateString* node) override {} // Handled by visitExpr()
    void visit(frontend::TernaryExpr* node) override {} // Handled by visitExpr()
    void visit(frontend::ObjectLiteral* node) override {} // Handled by visitExpr()
    void visit(frontend::MemberAccess* node) override {} // Handled by visitExpr()
    void visit(frontend::LambdaExpr* node) override {} // Handled by visitExpr()
    void visit(VarExpr* node) override {}
    void visit(CallExpr* node) override {
        // Look up function in module
        Function* callee = ctx.module->getFunction(node->function_name);
        
        if (!callee) {
            throw std::runtime_error("Unknown function: " + node->function_name);
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
        if (node->value) {
            // Return with value
            Value* retVal = visitExpr(node->value.get());
            if (retVal) {
                ctx.builder->CreateRet(retVal);
            }
        } else {
            // Return void
            ctx.builder->CreateRetVoid();
        }
    }

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

void generate_code(aria::frontend::Block* root, const std::string& filename) {
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

    // Create 'main' function wrapper
    FunctionType* ft = FunctionType::get(Type::getVoidTy(ctx.llvmContext), false);
    Function* mainFunc = Function::Create(ft, Function::ExternalLinkage, "main", ctx.module.get());
    BasicBlock* entry = BasicBlock::Create(ctx.llvmContext, "entry", mainFunc);

    // IMPORTANT: Always call SetInsertPoint before using the builder!
    // Without this, CreateAlloca and other builder calls will fail.
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
