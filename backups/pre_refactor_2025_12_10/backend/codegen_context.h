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
#include <set>
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
    
    // Fat pointer support (debug builds) - WP 004.3
    uint64_t current_scope_id = 0;  // Current scope ID for fat pointer generation
    std::stack<uint64_t> scope_id_stack;  // Stack of scope IDs for proper nesting
    StructType* fatPointerTy = nullptr;  // Cached fat pointer type (32-byte struct)
    bool enableSafety = false;  // Runtime flag for safety mode (set from ARIA_ENABLE_SAFETY)
    
    // Generic function monomorphization support
    std::map<std::string, std::string> typeSubstitution;  // Map generic type params to concrete types (T -> int8)
    std::string currentMangledName = "";  // Current mangled name for specialized function
    
    // Module system support
    std::set<std::string> loadedModules;  // Track which modules have been loaded (prevent circular imports)
    std::vector<std::string> moduleSearchPaths;  // Directories to search for .aria modules
    std::string currentSourceFile = "";  // Current file being compiled (for relative imports)

    CodeGenContext(std::string moduleName) {
        module = std::make_unique<Module>(moduleName, llvmContext);
        builder = std::make_unique<IRBuilder<>>(llvmContext);
        pushScope(); // Global scope
        
        // Check if safety mode is enabled at compile time
        #ifdef ARIA_SAFETY_ENABLED
        enableSafety = true;
        #else
        enableSafety = false;
        #endif
    }
    
    /**
     * Get the Fat Pointer struct type for safety mode (WP 004.3)
     * Layout: { i8* ptr, i8* base, i64 size, i64 alloc_id }
     * Matches struct aria_fat_pointer in fat_pointer.h
     */
    StructType* getFatPointerType() {
        if (fatPointerTy) return fatPointerTy;
        
        Type* voidPtr = PointerType::getUnqual(llvmContext);
        Type* sizeTy = Type::getInt64Ty(llvmContext);
        Type* idTy = Type::getInt64Ty(llvmContext);
        
        // { ptr, base, size, alloc_id }
        std::vector<Type*> members = { voidPtr, voidPtr, sizeTy, idTy };
        fatPointerTy = StructType::create(llvmContext, members, "struct.aria_fat_pointer");
        return fatPointerTy;
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
        // Check for generic type parameter substitution (T -> int8, etc.)
        std::string actualType = ariaType;
        if (typeSubstitution.count(ariaType) > 0) {
            actualType = typeSubstitution[ariaType];
        }
        
        // Check for array types: int8[256] or int8[]
        size_t bracketPos = actualType.find('[');
        if (bracketPos != std::string::npos) {
            std::string elemType = actualType.substr(0, bracketPos);
            std::string sizeStr = actualType.substr(bracketPos + 1);
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
        if (actualType == "int1" || actualType == "uint1") return Type::getInt1Ty(llvmContext);
        if (actualType == "int2" || actualType == "uint2") return Type::getIntNTy(llvmContext, 2);
        if (actualType == "int4" || actualType == "uint4" || actualType == "nit") return Type::getIntNTy(llvmContext, 4);
        if (actualType == "int8" || actualType == "uint8" || actualType == "byte" || actualType == "trit") 
            return Type::getInt8Ty(llvmContext);
        if (actualType == "int16" || actualType == "uint16" || actualType == "tryte" || actualType == "nyte") 
            return Type::getInt16Ty(llvmContext);
        if (actualType == "int32" || actualType == "uint32") return Type::getInt32Ty(llvmContext);
        if (actualType == "int64" || actualType == "uint64") return Type::getInt64Ty(llvmContext);
        if (actualType == "int128" || actualType == "uint128") return Type::getInt128Ty(llvmContext);
        if (actualType == "int256" || actualType == "uint256") return Type::getIntNTy(llvmContext, 256);
        if (actualType == "int512" || actualType == "uint512") return Type::getIntNTy(llvmContext, 512);
        
        // Twisted Balanced Binary (TBB) types - symmetric range with error sentinel
        // tbb8: [-127, +127] with -128 (0x80) as ERR
        // tbb16: [-32767, +32767] with -32768 (0x8000) as ERR
        // tbb32: [-2147483647, +2147483647] with -2147483648 (0x80000000) as ERR
        // tbb64: [-9223372036854775807, +9223372036854775807] with min as ERR
        // NOTE: Storage representation is identical to standard int types (two's complement)
        // Semantic difference is in arithmetic operations and range validation
        if (actualType == "tbb8") return Type::getInt8Ty(llvmContext);
        if (actualType == "tbb16") return Type::getInt16Ty(llvmContext);
        if (actualType == "tbb32") return Type::getInt32Ty(llvmContext);
        if (actualType == "tbb64") return Type::getInt64Ty(llvmContext);
        
        // Float types (all bit widths)
        if (actualType == "float" || actualType == "flt32") 
            return Type::getFloatTy(llvmContext);
        if (actualType == "double" || actualType == "flt64") 
            return Type::getDoubleTy(llvmContext);
        if (actualType == "flt128") return Type::getFP128Ty(llvmContext);
        if (actualType == "flt256") return Type::getFP128Ty(llvmContext);  // LLVM max is fp128, use for now
        if (actualType == "flt512") return Type::getFP128Ty(llvmContext);  // LLVM max is fp128, use for now
        
        // SIMD Vector types - map to LLVM fixed vector types for hardware acceleration
        // vec2: 2-element float vector -> <2 x float>  (SSE, NEON compatible)
        // vec3: 3-element float vector -> <4 x float>  (padded to 4 for alignment)
        // vec4: 4-element float vector -> <4 x float>  (SSE, NEON, AVX compatible)
        // These enable automatic vectorization and SIMD instruction generation
        if (actualType == "vec2") return FixedVectorType::get(Type::getFloatTy(llvmContext), 2);
        if (actualType == "vec3") return FixedVectorType::get(Type::getFloatTy(llvmContext), 4);  // Padded to 4
        if (actualType == "vec4") return FixedVectorType::get(Type::getFloatTy(llvmContext), 4);
        
        // Double-precision vector types for high-precision scientific computing
        if (actualType == "dvec2") return FixedVectorType::get(Type::getDoubleTy(llvmContext), 2);
        if (actualType == "dvec3") return FixedVectorType::get(Type::getDoubleTy(llvmContext), 4);  // Padded to 4
        if (actualType == "dvec4") return FixedVectorType::get(Type::getDoubleTy(llvmContext), 4);
        
        // Integer vector types for data processing and bit manipulation
        if (actualType == "ivec2") return FixedVectorType::get(Type::getInt32Ty(llvmContext), 2);
        if (actualType == "ivec3") return FixedVectorType::get(Type::getInt32Ty(llvmContext), 4);  // Padded to 4
        if (actualType == "ivec4") return FixedVectorType::get(Type::getInt32Ty(llvmContext), 4);
        
        // Unsigned integer vector types
        if (actualType == "uvec2") return FixedVectorType::get(Type::getInt32Ty(llvmContext), 2);
        if (actualType == "uvec3") return FixedVectorType::get(Type::getInt32Ty(llvmContext), 4);  // Padded to 4
        if (actualType == "uvec4") return FixedVectorType::get(Type::getInt32Ty(llvmContext), 4);
        
        // Boolean vector types
        if (actualType == "bvec2") return FixedVectorType::get(Type::getInt1Ty(llvmContext), 2);
        if (actualType == "bvec3") return FixedVectorType::get(Type::getInt1Ty(llvmContext), 4);  // Padded to 4
        if (actualType == "bvec4") return FixedVectorType::get(Type::getInt1Ty(llvmContext), 4);
        
        // Matrix types (stored as vectors for SIMD efficiency)
        // Square matrices
        if (actualType == "mat2") return FixedVectorType::get(Type::getFloatTy(llvmContext), 4);   // 2x2 = 4 floats
        if (actualType == "mat3") return FixedVectorType::get(Type::getFloatTy(llvmContext), 9);   // 3x3 = 9 floats
        if (actualType == "mat4") return FixedVectorType::get(Type::getFloatTy(llvmContext), 16);  // 4x4 = 16 floats
        
        // Non-square matrices
        if (actualType == "mat2x3") return FixedVectorType::get(Type::getFloatTy(llvmContext), 6);
        if (actualType == "mat2x4") return FixedVectorType::get(Type::getFloatTy(llvmContext), 8);
        if (actualType == "mat3x2") return FixedVectorType::get(Type::getFloatTy(llvmContext), 6);
        if (actualType == "mat3x4") return FixedVectorType::get(Type::getFloatTy(llvmContext), 12);
        if (actualType == "mat4x2") return FixedVectorType::get(Type::getFloatTy(llvmContext), 8);
        if (actualType == "mat4x3") return FixedVectorType::get(Type::getFloatTy(llvmContext), 12);
        
        // Double-precision matrices
        if (actualType == "dmat2") return FixedVectorType::get(Type::getDoubleTy(llvmContext), 4);
        if (actualType == "dmat3") return FixedVectorType::get(Type::getDoubleTy(llvmContext), 9);
        if (actualType == "dmat4") return FixedVectorType::get(Type::getDoubleTy(llvmContext), 16);
        
        if (actualType == "dmat2x3") return FixedVectorType::get(Type::getDoubleTy(llvmContext), 6);
        if (actualType == "dmat2x4") return FixedVectorType::get(Type::getDoubleTy(llvmContext), 8);
        if (actualType == "dmat3x2") return FixedVectorType::get(Type::getDoubleTy(llvmContext), 6);
        if (actualType == "dmat3x4") return FixedVectorType::get(Type::getDoubleTy(llvmContext), 12);
        if (actualType == "dmat4x2") return FixedVectorType::get(Type::getDoubleTy(llvmContext), 8);
        if (actualType == "dmat4x3") return FixedVectorType::get(Type::getDoubleTy(llvmContext), 12);
        
        if (actualType == "void") return Type::getVoidTy(llvmContext);
        
        // Dynamic type (GC-allocated catch-all)
        if (actualType == "dyn") return PointerType::getUnqual(llvmContext);
        
        // Result type: struct with err (ptr) and val (T) fields
        // Generic result type without val type specified - use default i64
        if (actualType == "result" || actualType == "Result") {
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
    // Special case: result<void> is just i8 (error code only, no value)
    Type* getResultType(const std::string& valTypeName) {
        // Special case: void results are just the error code
        if (valTypeName == "void") {
            return Type::getInt8Ty(llvmContext);  // Just the error byte
        }
        
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
