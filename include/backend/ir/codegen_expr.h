#ifndef ARIA_CODEGEN_EXPR_H
#define ARIA_CODEGEN_EXPR_H

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/Module.h>
#include "backend/ir/tbb_codegen.h"
#include "backend/ir/ternary_codegen.h"
#include <map>
#include <set>
#include <string>
#include <unordered_map>

// Forward declarations
namespace npk {
    class ASTNode;
    class LiteralExpr;
    class IdentifierExpr;
    class BinaryExpr;
    class UnaryExpr;
    class CallExpr;
    class TernaryExpr;
    class IndexExpr;
    class MemberAccessExpr;
    class ObjectLiteralExpr;
    class LambdaExpr;
    class AwaitExpr;
    class TemplateLiteralExpr;
    class VectorConstructorExpr;
    class CastExpr;
    
    namespace sema {
        class Type;
    }
    
    namespace frontend {
        enum class TokenType;  // Forward declaration for frontend::TokenType
    }
}

namespace npk {
namespace backend {

/**
 * ExprCodegen - Expression code generation
 * 
 * Generates LLVM IR for Aria expressions including literals, identifiers,
 * operators, and function calls.
 * 
 * Phase 4.2: Expression Code Generation
 */
class ExprCodegen {
private:
    llvm::LLVMContext& context;
    llvm::IRBuilder<>& builder;
    llvm::Module* module;
    
    // Symbol table (maps variable names to their LLVM values)
    std::map<std::string, llvm::Value*>& named_values;

    // Track Aria type names by variable name (for UFCS method resolution)
    std::map<std::string, std::string>& var_aria_types;
    
    // Type system for struct field lookup
    sema::TypeSystem* type_system;

    // TBB codegen for safe arithmetic with overflow detection
    TBBCodegen tbb_codegen;
    
    // Ternary codegen for balanced ternary/nonary arithmetic (trit/nit/tryte/nyte)
    TernaryCodegen ternary_codegen;
    
    // ARIA-026: Expression recursion depth guard (Gemini Safety Audit Fix #4)
    size_t expr_depth_ = 0;
    static constexpr size_t MAX_EXPR_DEPTH = 500;
    
    // v0.4.3: Defaults fallback block stack for ?|/defaults scoped expression fallback.
    // When non-empty, Result ERR checks in arg auto-unwrap branch to the top
    // fallback block instead of returning ERR to the caller.
    std::vector<llvm::BasicBlock*> defaults_fallback_stack_;
    
    // ARIA-026: String interning pool (Gemini Safety Audit Fix #5)
    // Prevents OOM crashes from duplicate string literals in Teacher system
    std::map<std::string, llvm::GlobalVariable*> string_pool_;

    // v0.2.36: dyn Trait parameter info for call-site coercion
    // Maps funcName -> { paramIndex -> traitName }
    const std::map<std::string, std::map<unsigned, std::string>>* func_dyn_params_ = nullptr;
    
    // v0.2.36: Trait method ordering for vtable dispatch
    // Maps traitName -> ordered list of method names
    const std::map<std::string, std::vector<std::string>>* trait_method_order_ = nullptr;
    
    // Helper: Get LLVM type from Aria type
    llvm::Type* getLLVMType(sema::Type* type);
    
    // Helper: Get LLVM type from Aria type name string
    llvm::Type* getLLVMTypeFromString(const std::string& typeName);
    
    // Helper: Get size of Aria type in bytes
    size_t getTypeSize(sema::Type* type);
    
    // Helper: Check if type is TBB type
    bool isTBBType(sema::Type* type);

    // Helper: Resolve the Aria type name of a struct field from a MEMBER_ACCESS expression
    std::string getMemberAccessFieldTypeName(ASTNode* expr);

    // Helper: Get TBB type name from an expression (returns empty string if not TBB)
    std::string getExprTBBTypeName(ASTNode* expr);

    // Helper: Get ERR sentinel constant for TBB type
    llvm::Value* getTBBSentinel(llvm::Type* type);
    llvm::Value* getUnknownSentinel(llvm::Type* type);

    // ARIA-018: Sentinel-Preserving TBB Widening
    // Generates branchless widening that preserves error sentinels across bit widths
    llvm::Value* generateTBBWiden(llvm::Value* srcVal, llvm::Type* dstType);

    // Helper: Generate intrinsic-based TBB binary operation (optimized)
    llvm::Value* generateTBBBinaryOp(const std::string& tbbType,
                                      frontend::TokenType op,
                                      llvm::Value* left,
                                      llvm::Value* right);

    // Helper: Get exotic type name from an expression (returns empty string if not exotic)
    std::string getExprExoticTypeName(ASTNode* expr);

    // Helper: Get numeric type name from an expression (frac*, tfp*, vec9)
    std::string getExprNumericTypeName(ASTNode* expr);

    // Helper: Get LBIM type name from an expression (int128/256/512/1024, uint*, fix256)
    std::string getExprLBIMTypeName(ASTNode* expr);

    // Helper: Generate exotic type binary operation (tryte/nyte runtime calls)
    llvm::Value* generateExoticBinaryOp(const std::string& exoticType,
                                         frontend::TokenType op,
                                         llvm::Value* left,
                                         llvm::Value* right);

    // Helper: Generate numeric type binary operation (frac*, tfp* runtime calls)
    llvm::Value* generateNumericBinaryOp(const std::string& numericType,
                                          frontend::TokenType op,
                                          llvm::Value* left,
                                          llvm::Value* right);

    // Helper: Generate LBIM type binary operation (int128/256/512/1024 runtime calls)
    llvm::Value* generateLBIMBinaryOp(const std::string& lbimType,
                                       frontend::TokenType op,
                                       llvm::Value* left,
                                       llvm::Value* right);

public:
    /**
     * Constructor
     * @param ctx LLVM context
     * @param bldr IR builder
     * @param mod LLVM module
     * @param values Symbol table for named values
     * @param types Aria type names by variable name (for UFCS resolution)
     * @param ts Type system for struct field lookup
     */
    ExprCodegen(llvm::LLVMContext& ctx, llvm::IRBuilder<>& bldr,
                llvm::Module* mod, std::map<std::string, llvm::Value*>& values,
                std::map<std::string, std::string>& types,
                sema::TypeSystem* ts = nullptr);

    /**
     * Set dyn Trait parameter info for call-site coercion (v0.2.36)
     */
    void setDynParamInfo(const std::map<std::string, std::map<unsigned, std::string>>* info) {
        func_dyn_params_ = info;
    }
    void setTraitMethodOrder(const std::map<std::string, std::vector<std::string>>* order) {
        trait_method_order_ = order;
    }

    // v0.4.3: User stack pop/peek destination type context
    // Set before codegenning an initializer that may be apop()/apeek().
    // The apop/apeek codegen reads this to determine the expected type tag and
    // convert the raw i64 to the correct LLVM type before returning.
    llvm::Type* ustack_pop_dest_type = nullptr;

    // v0.4.3+: SMT-proven user stack fast mode
    // When true, Z3 has proven all pushes in this function are type-homogeneous.
    // Codegen uses npk_ustack_*_fast() functions (no tags, no bounds checks).
    bool ustack_fast_mode = false;

    // v0.4.5: User hash table get destination type context
    // Set before codegenning an initializer that may be ahget().
    // The ahget codegen reads this to determine the expected type tag and
    // convert the raw i64 to the correct LLVM type before returning.
    llvm::Type* uhash_get_dest_type = nullptr;

    // v0.4.5+: SMT-proven user hash fast mode
    bool uhash_fast_mode = false;

    // v0.4.5: Counter for uhash handle tracking (auto-cleanup)
    // Points to ir_generator's counter; incremented each time ahash() is called.
    int* uhash_handle_counter_ptr = nullptr;

    // v0.5.0: Pointer to the set of DefaultsExpr nodes whose fallback is proven dead.
    // Set from IRGenerator::defaults_safe when creating ExprCodegen for CALL delegation.
    const std::set<ASTNode*>* defaults_safe_ptr = nullptr;

    // v0.31.0.1 (Phase 1 / D-2): set true when the current module contains at
    // least one `async func:`. When true, the `exit(code)` lowering in
    // main/failsafe is preceded by `npk_executor_run(NULL)` so any queued
    // async tasks are drained before the process terminates. Propagated from
    // IRGenerator::module_has_async_ at every ExprCodegen instantiation that
    // may reach `exit`.
    bool module_has_async = false;

    /**
     * Promote int64 literal to LBIM struct type (int128/256/512/1024/2048/4096)
     * Used when assigning literals to large integer variables
     * @param literal LLVM integer value to promote
     * @param targetType Target LBIM struct type
     * @return Promoted LBIM struct value
     */
    llvm::Value* promoteLiteralToLBIM(llvm::Value* literal, llvm::Type* targetType);
    
    /**
     * Generate code for a literal expression
     * Handles: int, float, string, bool, null
     * @param expr Literal expression node
     * @return LLVM constant value
     */
    llvm::Value* codegenLiteral(LiteralExpr* expr);
    
    /**
     * Generate code for an identifier (variable reference)
     * @param expr Identifier expression node
     * @return LLVM load instruction
     */
    llvm::Value* codegenIdentifier(IdentifierExpr* expr);
    
    /**
     * Generate code for a binary operation
     * Handles: arithmetic, comparison, logical, bitwise operators
     * @param expr Binary expression node
     * @return LLVM value of the operation result
     */
    llvm::Value* codegenBinary(BinaryExpr* expr);
    
    /**
     * Generate code for a unary operation
     * Handles: neg, not, address, deref
     * @param expr Unary expression node
     * @return LLVM value of the operation result
     */
    llvm::Value* codegenUnary(UnaryExpr* expr);
    
    /**
     * Generate code for a function call
     * @param expr Call expression node
     * @return LLVM call instruction result
     */
    llvm::Value* codegenCall(CallExpr* expr);
    
    /**
     * Generate code for a ternary expression (is ? : operator)
     * @param expr Ternary expression node
     * @return LLVM value selected by condition
     */
    llvm::Value* codegenTernary(TernaryExpr* expr);
    
    /**
     * Generate code for template literals with interpolation
     * Handles: `text &{expr} more text`
     * @param expr Template literal expression node
     * @return LLVM pointer to concatenated string
     */
    llvm::Value* codegenTemplateLiteral(TemplateLiteralExpr* expr);
    
    /**
     * Generate code for an array index operation
     * @param expr Index expression node
     * @return LLVM value at indexed location
     */
    llvm::Value* codegenIndex(IndexExpr* expr);
    
    /**
     * Generate code for member access
     * @param expr Member access expression node
     * @return LLVM value of the member
     */
    llvm::Value* codegenMemberAccess(MemberAccessExpr* expr);
    
    // GPU/PTX Backend - Phase 4: GPU Intrinsics
    /**
     * Generate code for GPU intrinsics (gpu.thread_id(), gpu.sync_threads(), etc.)
     * @param intrinsic_name Name of the intrinsic (thread_id, block_id, sync_threads, etc.)
     * @param call_expr Call expression containing arguments
     * @return LLVM value from NVVM intrinsic call
     */
    llvm::Value* codegenGPUIntrinsic(const std::string& intrinsic_name,
                                      CallExpr* call_expr);
    
    /**
     * Generate code for lambda expressions (closures)
     * Creates fat pointer: { method_ptr, env_ptr }
     * @param expr Lambda expression node
     * @return LLVM value of the fat pointer struct
     * Reference: research_016 (Functional Types)
     */
    llvm::Value* codegenLambda(LambdaExpr* expr);
    
    /**
     * Generate code for await expression (async/await)
     * Creates suspension point in coroutine with state machine
     * @param expr Await expression node
     * @return LLVM value after resumption (Future result)
     * Reference: research_029 (Async/Await System)
     */
    llvm::Value* codegenAwait(ASTNode* expr);
    
    /**
     * Generate code for cast expressions (@cast and @cast_unchecked)
     * Handles: safe widening, checked narrowing, unchecked truncation
     * @param expr Cast expression node
     * @return LLVM value with target type
     * Reference: Zero Implicit Conversion (explicit casting)
     */
    llvm::Value* codegenCast(CastExpr* expr);
    
    /**
     * Generate code for object literal expressions
     * Handles: {val1, val2, ...} syntax for struct-like initialization
     * Used for vec9, frac types, and other exotic composite types
     * @param expr Object literal expression node
     * @return LLVM value (struct or array)
     */
    llvm::Value* codegenObjectLiteral(ObjectLiteralExpr* expr);
    
    // ========================================================================
    // Phase 2: Optional Types & Special Operators
    // ========================================================================
    
    /**
     * Get LLVM struct type for optional<T>: { i1 hasValue, T value }
     * @param wrappedType The LLVM type being wrapped
     * @return LLVM struct type for optional
     */
    llvm::StructType* getOptionalType(llvm::Type* wrappedType);
    
    /**
     * Create an optional value with hasValue=true
     * @param value The value to wrap
     * @param wrappedType The type of the wrapped value
     * @return LLVM optional struct with value set
     */
    llvm::Value* createOptionalSome(llvm::Value* value, llvm::Type* wrappedType);
    
    /**
     * Create an optional value with hasValue=false (NIL)
     * @param wrappedType The type that would be wrapped
     * @return LLVM optional struct representing NIL
     */
    llvm::Value* createOptionalNone(llvm::Type* wrappedType);
    
    /**
     * Check if optional hasValue (is not NIL)
     * @param optional The optional struct value
     * @return LLVM i1 value (true if has value)
     */
    llvm::Value* isOptionalSome(llvm::Value* optional);
    
    /**
     * Extract value from optional (unwrap)
     * Note: Caller must check hasValue first!
     * @param optional The optional struct value
     * @return The wrapped value
     */
    llvm::Value* unwrapOptional(llvm::Value* optional);
    
    // ========================================================================
    // Phase 3.3: Mathematical Types - Vectors
    // ========================================================================
    
    /**
     * Generate code for vector constructor
     * Handles: vec2(x, y), vec3(x, y, z), vec9(c0, ..., c8)
     * @param expr Vector constructor expression node
     * @return LLVM vector value (FixedVectorType for vec2/vec3, struct for vec9)
     */
    llvm::Value* codegenVectorConstructor(VectorConstructorExpr* expr);
    
    /**
     * Generate code for any expression (dispatcher)
     * @param node Expression node
     * @param codegen ExprCodegen instance
     * @return LLVM value of the expression
     */
    static llvm::Value* codegenExpressionNode(npk::ASTNode* node, ExprCodegen* codegen);
};

} // namespace backend
} // namespace npk

#endif // ARIA_CODEGEN_EXPR_H
