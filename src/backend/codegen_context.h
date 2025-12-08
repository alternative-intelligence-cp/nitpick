/**
 * src/backend/codegen_context.h
 * 
 * Aria Compiler - Code Generation Context
 * Version: 0.0.7
 * 
 * This header contains the CodeGenContext class and supporting utilities
 * for LLVM code generation. Extracted from monolithic codegen.cpp as part
 * of refactoring initiative.
 * 
 * Dependencies:
 * - LLVM 18 Core, IR, Support
 * - Aria AST Headers
 */

#ifndef ARIA_BACKEND_CODEGEN_CONTEXT_H
#define ARIA_BACKEND_CODEGEN_CONTEXT_H

#include "../frontend/ast.h"
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Function.h>

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <stack>

using namespace llvm;

namespace aria {
namespace backend {

// Forward declarations
class CodeGenVisitor;

// =============================================================================
// Code Generation Context
// =============================================================================

/**
 * CodeGenContext: Central state for LLVM IR generation
 * 
 * Manages:
 * - LLVM context, module, and IR builder
 * - Symbol table with scoping
 * - Type mappings (Aria → LLVM)
 * - Compilation state (current function, return handling, etc.)
 * - Control flow context (loops, pick statements)
 * - Module system prefix
 * - Fat pointer scope tracking (debug builds)
 */
class CodeGenContext {
public:
    LLVMContext llvmContext;
    std::unique_ptr<Module> module;
    std::unique_ptr<IRBuilder<>> builder;
    
    // Symbol Table: Maps variable names to LLVM Allocas or Values
    enum class AllocStrategy { STACK, WILD, WILDX, GC, VALUE };
    struct Symbol {
        Value* val;
        bool is_ref; // Is this a pointer to the value (alloca) or the value itself?
        std::string ariaType; // Store the Aria type for proper loading
        AllocStrategy strategy; // How was this allocated?
    };
    std::vector<std::map<std::string, Symbol>> scopeStack;
    
    // Expression type tracking: Maps LLVM Value* to its Aria type
    // This is critical for TBB safety - we need to know if a value is TBB to apply sticky error propagation
    std::map<Value*, std::string> exprTypeMap;
    
    // Struct metadata: Maps struct name to field name->index mapping
    std::map<std::string, std::map<std::string, unsigned>> structFieldMaps;

    // Current compilation state
    Function* currentFunction = nullptr;
    BasicBlock* returnBlock = nullptr;
    Value* returnValue = nullptr; // Pointer to return value storage
    
    // Function return type tracking (for result type validation)
    std::string currentFunctionReturnType = "";  // The VAL type (e.g., "int8")
    bool currentFunctionAutoWrap = false;         // Whether function uses * auto-wrap
    
    // Pick statement context (for fall() statements)
    std::map<std::string, BasicBlock*>* pickLabelBlocks = nullptr;
    BasicBlock* pickDoneBlock = nullptr;
    
    // Loop context (for break/continue)
    BasicBlock* currentLoopBreakTarget = nullptr;
    BasicBlock* currentLoopContinueTarget = nullptr;
    
    // Defer statement stack (LIFO execution on scope exit)
    std::vector<std::vector<frontend::Block*>> deferStacks;  // Stack of defer blocks per scope
    
    // Module system
    std::string currentModulePrefix = "";  // Current module namespace prefix (e.g., "math.")
    
    // Fat pointer support (debug builds)
    uint64_t current_scope_id = 0;  // Current scope ID for fat pointer generation
    std::stack<uint64_t> scope_id_stack;  // Stack of scope IDs for proper nesting

    CodeGenContext(std::string moduleName) {
        module = std::make_unique<Module>(moduleName, llvmContext);
        builder = std::make_unique<IRBuilder<>>(llvmContext);
        pushScope(); // Global scope
    }

    void pushScope() { 
        scopeStack.emplace_back(); 
        deferStacks.emplace_back();  // New defer stack for this scope
        
        // For fat pointer support in debug builds
        // Generate call to aria_scope_enter() and track scope ID
        #ifdef ARIA_DEBUG
        // In real implementation, we'd emit IR call to aria_scope_enter()
        // For now, just increment a counter (placeholder)
        static uint64_t scope_counter = 1;
        current_scope_id = scope_counter++;
        scope_id_stack.push(current_scope_id);
        #endif
    }
    
    void popScope() { 
        scopeStack.pop_back(); 
        if (!deferStacks.empty()) {
            deferStacks.pop_back();  // Remove defer stack for this scope
        }
        
        // For fat pointer support in debug builds
        // Generate call to aria_scope_exit(scope_id)
        #ifdef ARIA_DEBUG
        if (!scope_id_stack.empty()) {
            scope_id_stack.pop();
            current_scope_id = scope_id_stack.empty() ? 0 : scope_id_stack.top();
        }
        #endif
    }
    
    // Add a defer block to the current scope
    void pushDefer(frontend::Block* deferBlock) {
        if (!deferStacks.empty()) {
            deferStacks.back().push_back(deferBlock);
        }
    }
    
    // Execute all defers for the current scope in LIFO order
    void executeScopeDefers(CodeGenVisitor* visitor);  // Forward declare

    void define(const std::string& name, Value* val, bool is_ref = true, const std::string& ariaType = "", AllocStrategy strategy = AllocStrategy::VALUE) {
        scopeStack.back()[name] = {val, is_ref, ariaType, strategy};
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
        // Check for array types: int8[256] or int8[]
        size_t bracketPos = ariaType.find('[');
        if (bracketPos != std::string::npos) {
            std::string elemType = ariaType.substr(0, bracketPos);
            std::string sizeStr = ariaType.substr(bracketPos + 1);
            sizeStr = sizeStr.substr(0, sizeStr.find(']'));
            
            Type* elementType = getLLVMType(elemType);
            
            if (sizeStr.empty()) {
                // Dynamic array: int8[] - represented as pointer
                return PointerType::getUnqual(llvmContext);
            } else {
                // Fixed-size array: int8[256] - represented as [256 x i8]
                uint64_t arraySize = std::stoull(sizeStr);
                return ArrayType::get(elementType, arraySize);
            }
        }
        
        // Integer types (all bit widths, signed and unsigned)
        // Note: uint1, uint2, uint4 alias to int1, int2, int4 (not in spec, but user-friendly)
        if (ariaType == "int1" || ariaType == "uint1") return Type::getInt1Ty(llvmContext);
        if (ariaType == "int2" || ariaType == "uint2") return Type::getIntNTy(llvmContext, 2);
        if (ariaType == "int4" || ariaType == "uint4" || ariaType == "nit") return Type::getIntNTy(llvmContext, 4);
        if (ariaType == "int8" || ariaType == "uint8" || ariaType == "byte" || ariaType == "trit") 
            return Type::getInt8Ty(llvmContext);
        if (ariaType == "int16" || ariaType == "uint16" || ariaType == "tryte" || ariaType == "nyte") 
            return Type::getInt16Ty(llvmContext);
        if (ariaType == "int32" || ariaType == "uint32") return Type::getInt32Ty(llvmContext);
        if (ariaType == "int64" || ariaType == "uint64") return Type::getInt64Ty(llvmContext);
        if (ariaType == "int128" || ariaType == "uint128") return Type::getInt128Ty(llvmContext);
        if (ariaType == "int256" || ariaType == "uint256") return Type::getIntNTy(llvmContext, 256);
        if (ariaType == "int512" || ariaType == "uint512") return Type::getIntNTy(llvmContext, 512);
        
        // Twisted Balanced Binary (TBB) types - symmetric range with error sentinel
        // tbb8: [-127, +127] with -128 (0x80) as ERR
        // tbb16: [-32767, +32767] with -32768 (0x8000) as ERR
        // tbb32: [-2147483647, +2147483647] with -2147483648 (0x80000000) as ERR
        // tbb64: [-9223372036854775807, +9223372036854775807] with min as ERR
        // NOTE: Storage representation is identical to standard int types (two's complement)
        // Semantic difference is in arithmetic operations and range validation
        if (ariaType == "tbb8") return Type::getInt8Ty(llvmContext);
        if (ariaType == "tbb16") return Type::getInt16Ty(llvmContext);
        if (ariaType == "tbb32") return Type::getInt32Ty(llvmContext);
        if (ariaType == "tbb64") return Type::getInt64Ty(llvmContext);
        
        // Float types (all bit widths)
        if (ariaType == "float" || ariaType == "flt32") 
            return Type::getFloatTy(llvmContext);
        if (ariaType == "double" || ariaType == "flt64") 
            return Type::getDoubleTy(llvmContext);
        if (ariaType == "flt128") return Type::getFP128Ty(llvmContext);
        if (ariaType == "flt256") return Type::getFP128Ty(llvmContext);  // LLVM max is fp128, use for now
        if (ariaType == "flt512") return Type::getFP128Ty(llvmContext);  // LLVM max is fp128, use for now
        
        if (ariaType == "void") return Type::getVoidTy(llvmContext);
        
        // Dynamic type (GC-allocated catch-all)
        if (ariaType == "dyn") return PointerType::getUnqual(llvmContext);
        
        // Result type: struct with err (ptr) and val (T) fields
        // Generic result type without val type specified - use default i64
        if (ariaType == "result" || ariaType == "Result") {
            return getResultType("int64");
        }
        
        // Pointers (opaque in LLVM 18)
        // We return ptr for strings, arrays, objects
        return PointerType::getUnqual(llvmContext);
    }
    
    // Get or create parametric result type: result<valType>
    // Creates a struct { i8 err, T val } where T is the val type
    // err: uint8 semantics - 0 = success, 1-255 = error codes (C-style)
    // Note: LLVM uses i8 for both signed/unsigned - semantics determined by operations
    // Each unique val type gets its own struct: result_int8, result_int32, etc.
    Type* getResultType(const std::string& valTypeName) {
        // Generate unique name for this result variant
        std::string structName = "result_" + valTypeName;
        
        // Try to get existing type first (avoid duplicates)
        if (auto* existing = StructType::getTypeByName(llvmContext, structName)) {
            return existing;
        }
        
        // Get the LLVM type for the val field
        Type* valType = getLLVMType(valTypeName);
        
        // Create new named struct: { i8 err, T val }
        std::vector<Type*> fields;
        fields.push_back(Type::getInt8Ty(llvmContext));  // err field (always i8, 0=success)
        fields.push_back(valType);                        // val field (type-specific)
        
        return StructType::create(llvmContext, fields, structName);
    }
    
    // Parse function signature from type string
    // Format: "func<returnType(param1Type,param2Type,...)>"
    // Returns FunctionType, or nullptr if not a function signature
    FunctionType* parseFunctionSignature(const std::string& typeStr) {
        // Check if it's a function signature
        if (typeStr.find("func<") != 0) {
            return nullptr;  // Not a function signature
        }
        
        // Find the return type (between < and ()
        size_t ltPos = typeStr.find('<');
        size_t parenPos = typeStr.find('(');
        if (ltPos == std::string::npos || parenPos == std::string::npos) {
            return nullptr;
        }
        
        std::string returnTypeStr = typeStr.substr(ltPos + 1, parenPos - ltPos - 1);
        Type* returnType = getLLVMType(returnTypeStr);
        
        // Parse parameter types (between ( and ))
        size_t endParenPos = typeStr.find(')');
        if (endParenPos == std::string::npos) {
            return nullptr;
        }
        
        std::string paramsStr = typeStr.substr(parenPos + 1, endParenPos - parenPos - 1);
        std::vector<Type*> paramTypes;
        
        if (!paramsStr.empty()) {
            // Split by comma
            size_t start = 0;
            while (start < paramsStr.length()) {
                size_t comma = paramsStr.find(',', start);
                if (comma == std::string::npos) {
                    comma = paramsStr.length();
                }
                
                std::string paramTypeStr = paramsStr.substr(start, comma - start);
                paramTypes.push_back(getLLVMType(paramTypeStr));
                
                start = comma + 1;
            }
        }
        
        return FunctionType::get(returnType, paramTypes, false);
    }
};

// =============================================================================
// RAII Scope Guard for Symbol Table Management
// =============================================================================

/**
 * ScopeGuard: RAII wrapper for scope management
 * 
 * Ensures popScope() is called even if exceptions occur or early returns happen.
 * This prevents scope leaks in the symbol table.
 * 
 * Usage:
 *   {
 *       ScopeGuard guard(ctx);
 *       // ... code that uses the scope ...
 *   } // popScope() automatically called here
 */
class ScopeGuard {
    CodeGenContext& ctx;
public:
    ScopeGuard(CodeGenContext& c) : ctx(c) { ctx.pushScope(); }
    ~ScopeGuard() { ctx.popScope(); }
    // Prevent copying to avoid double-pop
    ScopeGuard(const ScopeGuard&) = delete;
    ScopeGuard& operator=(const ScopeGuard&) = delete;
};

} // namespace backend
} // namespace aria

#endif // ARIA_BACKEND_CODEGEN_CONTEXT_H
