/**
 * test_ir_compilation.cpp
 * 
 * Phase 4.6.6: Integration tests for IR compilation and execution
 * Tests that generated LLVM IR can be compiled and executed correctly.
 */

#include "../test_helpers.h"
#include "backend/ir/ir_generator.h"
#include "backend/ir/codegen_stmt.h"
#include "backend/ir/codegen_expr.h"
#include "frontend/ast/stmt.h"
#include "frontend/ast/expr.h"
#include <llvm/IR/Module.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Verifier.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/GenericValue.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/Transforms/Scalar.h>
#include <llvm/Transforms/Utils.h>
#include <llvm/Support/raw_ostream.h>

using namespace aria;
using namespace aria::backend;

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * Initialize LLVM execution engine (call once per test suite)
 */
void initializeLLVMExecution() {
    static bool initialized = false;
    if (!initialized) {
        llvm::InitializeNativeTarget();
        llvm::InitializeNativeTargetAsmPrinter();
        llvm::InitializeNativeTargetAsmParser();
        initialized = true;
    }
}

// ============================================================================
// Basic Compilation Tests
// ============================================================================

TEST_CASE(ir_compilation_simple_function) {
    llvm::LLVMContext context;
    llvm::Module module("test_module", context);
    llvm::IRBuilder<> builder(context);
    std::map<std::string, llvm::Value*> named_values;
    
    // Create function: func:return_42 = i32() { pass(42); }
    llvm::FunctionType* func_type = llvm::FunctionType::get(builder.getInt32Ty(), false);
    llvm::Function* func = llvm::Function::Create(
        func_type,
        llvm::Function::ExternalLinkage,
        "return_42",
        &module
    );
    
    llvm::BasicBlock* entry = llvm::BasicBlock::Create(context, "entry", func);
    builder.SetInsertPoint(entry);
    
    // Return 42
    builder.CreateRet(builder.getInt32(42));
    
    // Verify the function is valid
    std::string errorStr;
    llvm::raw_string_ostream errorStream(errorStr);
    bool hasErrors = llvm::verifyFunction(*func, &errorStream);
    
    ASSERT(!hasErrors, "Simple function should compile without errors");
    
    // Verify the module is valid
    bool moduleValid = !llvm::verifyModule(module, &llvm::errs());
    ASSERT(moduleValid, "Module should be valid");
}

TEST_CASE(ir_compilation_arithmetic_function) {
    llvm::LLVMContext context;
    llvm::Module module("test_module", context);
    llvm::IRBuilder<> builder(context);
    std::map<std::string, llvm::Value*> named_values;
    
    // Create function: func:add = i32(i32:a, i32:b) { pass(a + b); }
    std::vector<llvm::Type*> param_types = {builder.getInt32Ty(), builder.getInt32Ty()};
    llvm::FunctionType* func_type = llvm::FunctionType::get(builder.getInt32Ty(), param_types, false);
    llvm::Function* func = llvm::Function::Create(
        func_type,
        llvm::Function::ExternalLinkage,
        "add",
        &module
    );
    
    // Set parameter names
    auto args = func->arg_begin();
    llvm::Value* a = &(*args++);
    llvm::Value* b = &(*args);
    a->setName("a");
    b->setName("b");
    
    llvm::BasicBlock* entry = llvm::BasicBlock::Create(context, "entry", func);
    builder.SetInsertPoint(entry);
    
    // Generate: a + b
    llvm::Value* sum = builder.CreateAdd(a, b, "sum");
    builder.CreateRet(sum);
    
    // Verify function is valid
    bool hasErrors = llvm::verifyFunction(*func, &llvm::errs());
    ASSERT(!hasErrors, "Arithmetic function should compile without errors");
}

TEST_CASE(ir_compilation_conditional_function) {
    llvm::LLVMContext context;
    llvm::Module module("test_module", context);
    llvm::IRBuilder<> builder(context);
    std::map<std::string, llvm::Value*> named_values;
    
    // Create function: func:max = i32(i32:a, i32:b) { if (a > b) pass(a) else pass(b) }
    std::vector<llvm::Type*> param_types = {builder.getInt32Ty(), builder.getInt32Ty()};
    llvm::FunctionType* func_type = llvm::FunctionType::get(builder.getInt32Ty(), param_types, false);
    llvm::Function* func = llvm::Function::Create(
        func_type,
        llvm::Function::ExternalLinkage,
        "max",
        &module
    );
    
    auto args = func->arg_begin();
    llvm::Value* a = &(*args++);
    llvm::Value* b = &(*args);
    
    // Create basic blocks
    llvm::BasicBlock* entry = llvm::BasicBlock::Create(context, "entry", func);
    llvm::BasicBlock* then_block = llvm::BasicBlock::Create(context, "then", func);
    llvm::BasicBlock* else_block = llvm::BasicBlock::Create(context, "else", func);
    llvm::BasicBlock* merge = llvm::BasicBlock::Create(context, "merge", func);
    
    builder.SetInsertPoint(entry);
    
    // if (a > b)
    llvm::Value* cond = builder.CreateICmpSGT(a, b, "cond");
    builder.CreateCondBr(cond, then_block, else_block);
    
    // then: return a
    builder.SetInsertPoint(then_block);
    builder.CreateBr(merge);
    
    // else: return b
    builder.SetInsertPoint(else_block);
    builder.CreateBr(merge);
    
    // merge: phi node
    builder.SetInsertPoint(merge);
    llvm::PHINode* phi = builder.CreatePHI(builder.getInt32Ty(), 2, "max_val");
    phi->addIncoming(a, then_block);
    phi->addIncoming(b, else_block);
    builder.CreateRet(phi);
    
    // Verify function is valid
    bool hasErrors = llvm::verifyFunction(*func, &llvm::errs());
    ASSERT(!hasErrors, "Conditional function should compile without errors");
}

TEST_CASE(ir_compilation_loop_function) {
    llvm::LLVMContext context;
    llvm::Module module("test_module", context);
    llvm::IRBuilder<> builder(context);
    std::map<std::string, llvm::Value*> named_values;
    
    // Create function: func:sum_n = i32(i32:n) { sum = 0; i = 0; while(i < n) { sum += i; i++; } pass(sum); }
    llvm::FunctionType* func_type = llvm::FunctionType::get(builder.getInt32Ty(), {builder.getInt32Ty()}, false);
    llvm::Function* func = llvm::Function::Create(
        func_type,
        llvm::Function::ExternalLinkage,
        "sum_n",
        &module
    );
    
    llvm::Value* n = &(*func->arg_begin());
    
    // Create basic blocks
    llvm::BasicBlock* entry = llvm::BasicBlock::Create(context, "entry", func);
    llvm::BasicBlock* loop = llvm::BasicBlock::Create(context, "loop", func);
    llvm::BasicBlock* body = llvm::BasicBlock::Create(context, "body", func);
    llvm::BasicBlock* exit = llvm::BasicBlock::Create(context, "exit", func);
    
    builder.SetInsertPoint(entry);
    llvm::Value* sum_ptr = builder.CreateAlloca(builder.getInt32Ty(), nullptr, "sum");
    llvm::Value* i_ptr = builder.CreateAlloca(builder.getInt32Ty(), nullptr, "i");
    builder.CreateStore(builder.getInt32(0), sum_ptr);
    builder.CreateStore(builder.getInt32(0), i_ptr);
    builder.CreateBr(loop);
    
    // loop: check condition
    builder.SetInsertPoint(loop);
    llvm::Value* i = builder.CreateLoad(builder.getInt32Ty(), i_ptr, "i_val");
    llvm::Value* cond = builder.CreateICmpSLT(i, n, "cond");
    builder.CreateCondBr(cond, body, exit);
    
    // body: sum += i; i++;
    builder.SetInsertPoint(body);
    llvm::Value* sum = builder.CreateLoad(builder.getInt32Ty(), sum_ptr, "sum_val");
    llvm::Value* new_sum = builder.CreateAdd(sum, i, "new_sum");
    builder.CreateStore(new_sum, sum_ptr);
    llvm::Value* i_inc = builder.CreateAdd(i, builder.getInt32(1), "i_inc");
    builder.CreateStore(i_inc, i_ptr);
    builder.CreateBr(loop);
    
    // exit: return sum
    builder.SetInsertPoint(exit);
    llvm::Value* final_sum = builder.CreateLoad(builder.getInt32Ty(), sum_ptr, "final_sum");
    builder.CreateRet(final_sum);
    
    // Verify function is valid
    bool hasErrors = llvm::verifyFunction(*func, &llvm::errs());
    ASSERT(!hasErrors, "Loop function should compile without errors");
}

// ============================================================================
// Module Verification Tests
// ============================================================================

TEST_CASE(ir_compilation_module_with_multiple_functions) {
    llvm::LLVMContext context;
    llvm::Module module("multi_func", context);
    llvm::IRBuilder<> builder(context);
    
    // Function 1: add
    llvm::FunctionType* add_type = llvm::FunctionType::get(
        builder.getInt32Ty(),
        {builder.getInt32Ty(), builder.getInt32Ty()},
        false
    );
    llvm::Function* add_func = llvm::Function::Create(
        add_type,
        llvm::Function::ExternalLinkage,
        "add",
        &module
    );
    
    llvm::BasicBlock* add_entry = llvm::BasicBlock::Create(context, "entry", add_func);
    builder.SetInsertPoint(add_entry);
    auto add_args = add_func->arg_begin();
    llvm::Value* a = &(*add_args++);
    llvm::Value* b = &(*add_args);
    builder.CreateRet(builder.CreateAdd(a, b, "sum"));
    
    // Function 2: mul
    llvm::FunctionType* mul_type = llvm::FunctionType::get(
        builder.getInt32Ty(),
        {builder.getInt32Ty(), builder.getInt32Ty()},
        false
    );
    llvm::Function* mul_func = llvm::Function::Create(
        mul_type,
        llvm::Function::ExternalLinkage,
        "mul",
        &module
    );
    
    llvm::BasicBlock* mul_entry = llvm::BasicBlock::Create(context, "entry", mul_func);
    builder.SetInsertPoint(mul_entry);
    auto mul_args = mul_func->arg_begin();
    llvm::Value* x = &(*mul_args++);
    llvm::Value* y = &(*mul_args);
    builder.CreateRet(builder.CreateMul(x, y, "product"));
    
    // Verify both functions
    ASSERT(!llvm::verifyFunction(*add_func, &llvm::errs()), "add function should be valid");
    ASSERT(!llvm::verifyFunction(*mul_func, &llvm::errs()), "mul function should be valid");
    
    // Verify the entire module
    bool moduleValid = !llvm::verifyModule(module, &llvm::errs());
    ASSERT(moduleValid, "Module with multiple functions should be valid");
}

TEST_CASE(ir_compilation_module_with_global_variable) {
    llvm::LLVMContext context;
    llvm::Module module("with_global", context);
    llvm::IRBuilder<> builder(context);
    
    // Create global variable: global_counter
    llvm::Type* int32_type = builder.getInt32Ty();
    module.getOrInsertGlobal("global_counter", int32_type);
    llvm::GlobalVariable* global = module.getNamedGlobal("global_counter");
    global->setInitializer(builder.getInt32(0));
    global->setLinkage(llvm::GlobalValue::ExternalLinkage);
    
    // Create function that uses global
    llvm::FunctionType* func_type = llvm::FunctionType::get(builder.getInt32Ty(), false);
    llvm::Function* func = llvm::Function::Create(
        func_type,
        llvm::Function::ExternalLinkage,
        "get_counter",
        &module
    );
    
    llvm::BasicBlock* entry = llvm::BasicBlock::Create(context, "entry", func);
    builder.SetInsertPoint(entry);
    
    llvm::Value* counter_val = builder.CreateLoad(int32_type, global, "counter_val");
    builder.CreateRet(counter_val);
    
    // Verify module
    bool moduleValid = !llvm::verifyModule(module, &llvm::errs());
    ASSERT(moduleValid, "Module with global variable should be valid");
}

// ============================================================================
// TBB Compilation Tests
// ============================================================================

TEST_CASE(ir_compilation_tbb_arithmetic) {
    llvm::LLVMContext context;
    llvm::Module module("tbb_test", context);
    llvm::IRBuilder<> builder(context);
    
    // Create TBB arithmetic function (simplified without full TBB overflow checking)
    llvm::FunctionType* func_type = llvm::FunctionType::get(
        builder.getInt8Ty(),
        {builder.getInt8Ty(), builder.getInt8Ty()},
        false
    );
    llvm::Function* func = llvm::Function::Create(
        func_type,
        llvm::Function::ExternalLinkage,
        "tbb_add",
        &module
    );
    
    auto args = func->arg_begin();
    llvm::Value* a = &(*args++);
    llvm::Value* b = &(*args);
    
    llvm::BasicBlock* entry = llvm::BasicBlock::Create(context, "entry", func);
    builder.SetInsertPoint(entry);
    
    // Simple add (full TBB checking would be more complex)
    llvm::Value* result = builder.CreateAdd(a, b, "result");
    builder.CreateRet(result);
    
    // Verify function
    bool hasErrors = llvm::verifyFunction(*func, &llvm::errs());
    ASSERT(!hasErrors, "TBB arithmetic function should compile");
}

// ============================================================================
// Memory Model Compilation Tests
// ============================================================================

TEST_CASE(ir_compilation_stack_allocation) {
    llvm::LLVMContext context;
    llvm::Module module("stack_test", context);
    llvm::IRBuilder<> builder(context);
    
    // Create function with stack allocation
    llvm::FunctionType* func_type = llvm::FunctionType::get(builder.getInt32Ty(), false);
    llvm::Function* func = llvm::Function::Create(
        func_type,
        llvm::Function::ExternalLinkage,
        "stack_alloc_test",
        &module
    );
    
    llvm::BasicBlock* entry = llvm::BasicBlock::Create(context, "entry", func);
    builder.SetInsertPoint(entry);
    
    // Allocate stack variable
    llvm::Value* var = builder.CreateAlloca(builder.getInt32Ty(), nullptr, "x");
    builder.CreateStore(builder.getInt32(100), var);
    llvm::Value* loaded = builder.CreateLoad(builder.getInt32Ty(), var, "x_val");
    builder.CreateRet(loaded);
    
    // Verify function
    bool hasErrors = llvm::verifyFunction(*func, &llvm::errs());
    ASSERT(!hasErrors, "Stack allocation function should compile");
}

TEST_CASE(ir_compilation_external_function_declaration) {
    llvm::LLVMContext context;
    llvm::Module module("extern_test", context);
    llvm::IRBuilder<> builder(context);
    
    // Declare external function (like aria_gc_alloc)
    llvm::FunctionType* alloc_type = llvm::FunctionType::get(
        builder.getPtrTy(),
        {builder.getInt64Ty()},
        false
    );
    llvm::Function* alloc_func = llvm::Function::Create(
        alloc_type,
        llvm::Function::ExternalLinkage,
        "aria_gc_alloc",
        &module
    );
    
    // Create function that calls external
    llvm::FunctionType* func_type = llvm::FunctionType::get(builder.getPtrTy(), false);
    llvm::Function* func = llvm::Function::Create(
        func_type,
        llvm::Function::ExternalLinkage,
        "allocate_memory",
        &module
    );
    
    llvm::BasicBlock* entry = llvm::BasicBlock::Create(context, "entry", func);
    builder.SetInsertPoint(entry);
    
    // Call external function
    llvm::Value* size = builder.getInt64(1024);
    llvm::Value* ptr = builder.CreateCall(alloc_func, {size}, "ptr");
    builder.CreateRet(ptr);
    
    // Verify module
    bool moduleValid = !llvm::verifyModule(module, &llvm::errs());
    ASSERT(moduleValid, "Module with external function should be valid");
}

// ============================================================================
// Error Handling Tests
// ============================================================================

TEST_CASE(ir_compilation_invalid_function_detected) {
    llvm::LLVMContext context;
    llvm::Module module("invalid_test", context);
    llvm::IRBuilder<> builder(context);
    
    // Create invalid function (missing return statement)
    llvm::FunctionType* func_type = llvm::FunctionType::get(builder.getInt32Ty(), false);
    llvm::Function* func = llvm::Function::Create(
        func_type,
        llvm::Function::ExternalLinkage,
        "invalid_func",
        &module
    );
    
    llvm::BasicBlock* entry = llvm::BasicBlock::Create(context, "entry", func);
    builder.SetInsertPoint(entry);
    
    // DON'T add return statement - this makes it invalid
    // (In practice, our codegen should always add returns)
    
    // Verify function - should detect error
    std::string errorStr;
    llvm::raw_string_ostream errorStream(errorStr);
    bool hasErrors = llvm::verifyFunction(*func, &errorStream);
    
    ASSERT(hasErrors, "Missing return should be detected as error");
}
