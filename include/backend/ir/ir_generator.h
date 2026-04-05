#ifndef ARIA_IR_GENERATOR_H
#define ARIA_IR_GENERATOR_H

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/DIBuilder.h>
#include <llvm/IR/DebugInfoMetadata.h>
#include "backend/ir/tbb_codegen.h"
#include "backend/ir/ternary_codegen.h"
#include <map>
#include <set>
#include <string>
#include <memory>
#include <vector>

namespace aria {

// Forward declarations
class ASTNode;  // ASTNode is in aria namespace
class BlockStmt;  // For defer stack support
class RulesDeclStmt;  // v0.2.41: For rules_table
namespace sema {
    class Type;  // Forward declaration in correct namespace
    class TypeSystem;  // Forward declaration for custom type lookups
    struct Specialization;  // Forward declaration for generic specializations
}
using sema::Type;  // Make Type available in aria namespace

/**
 * IRGenerator - Generates LLVM IR from Aria AST
 * 
 * This is the main backend class that translates validated AST nodes
 * into LLVM Intermediate Representation for optimization and code generation.
 * 
 * Reference: Phase 4.1 - LLVM Infrastructure Setup
 */
class IRGenerator {
private:
    llvm::LLVMContext context;
    std::unique_ptr<llvm::Module> module;
    llvm::IRBuilder<> builder;
    
    // Type system for looking up struct types
    sema::TypeSystem* type_system;
    
    // Symbol table for LLVM values (maps variable names to LLVM Values)
    std::map<std::string, llvm::Value*> named_values;
    
    // Track Aria types for LLVM values (needed for member access with opaque pointers)
    std::map<llvm::Value*, Type*> value_types;

    // Track Aria type names by variable name (for UFCS method resolution)
    // Maps variable names to their Aria type name (e.g., "array", "string", "MyStruct")
    std::map<std::string, std::string> var_aria_types;
    
    // Type mapping cache (Aria types -> LLVM types)
    std::map<std::string, llvm::Type*> type_map;
    
    // Enum constants (maps enum.variant to integer value)
    std::map<std::string, int64_t> enum_constants;
    
    // v0.2.41: Rules table (maps rules name -> AST declaration)
    std::map<std::string, RulesDeclStmt*> rules_table;
    
    // v0.2.41: Tracks which variables have limit<> constraints (var name -> rules name)
    std::map<std::string, std::string> var_limit_rules;
    
    // TBB codegen for safe arithmetic with overflow detection
    TBBCodegen tbb_codegen;
    
    // Ternary codegen for balanced ternary/nonary arithmetic
    TernaryCodegen ternary_codegen;
    
    // Loop context stack for break/continue support
    struct LoopContext {
        llvm::BasicBlock* continueTarget;  // Where 'continue' jumps to
        llvm::BasicBlock* breakTarget;     // Where 'break' jumps to
        
        LoopContext(llvm::BasicBlock* cont, llvm::BasicBlock* brk)
            : continueTarget(cont), breakTarget(brk) {}
    };
    std::vector<LoopContext> loop_stack;

    // Pick context stack for fall() support (maps label -> body BasicBlock)
    std::vector<std::map<std::string, llvm::BasicBlock*>> pick_context_stack;
    
    // Defer statement support (RAII cleanup)
    std::vector<std::vector<BlockStmt*>> defer_stack;
    bool executing_defers = false;  // ARIA-023: Prevents defer_stack modification during iteration

    // ARIA-022: Recursion depth limit to prevent stack overflow
    static constexpr size_t MAX_CODEGEN_DEPTH = 256;
    size_t codegen_depth_ = 0;

    // ========================================================================
    // dyn Trait dispatch infrastructure (v0.2.36)
    // ========================================================================
    struct TraitMethodInfo {
        std::string name;       // Method name (e.g., "describe")
        std::string returnType; // Return type string (e.g., "int32")
        std::vector<std::string> paramTypes; // Parameter types including Self
    };

    struct TraitInfo {
        std::string traitName;
        std::vector<TraitMethodInfo> methods;
        llvm::StructType* vtableType = nullptr; // %TraitName_vtable_t
    };

    // Maps trait name -> trait info (populated during TRAIT_DECL codegen)
    std::map<std::string, TraitInfo> trait_info_map;

    // Maps trait name -> ordered method names (for vtable dispatch index lookup)
    std::map<std::string, std::vector<std::string>> trait_method_order;

    // Maps "TraitName:TypeName" -> vtable global constant
    std::map<std::string, llvm::GlobalVariable*> vtable_constants;

    // Maps funcName -> { paramIndex -> traitName } for dyn Trait parameters
    std::map<std::string, std::map<unsigned, std::string>> func_dyn_params;

    // The dyn fat pointer type: { ptr, ptr } (data + vtable)
    llvm::StructType* getDynFatPtrType();

    // Generate vtable thunk for a trait method's concrete implementation
    llvm::Function* generateVtableThunk(const std::string& traitName,
                                         const std::string& typeName,
                                         const TraitMethodInfo& method);

    // Debug info generation (Phase 7.4.1)
    std::unique_ptr<llvm::DIBuilder> di_builder;
    llvm::DICompileUnit* di_compile_unit;
    llvm::DIFile* di_file;
    std::vector<llvm::DIScope*> di_scope_stack;  // Stack of lexical scopes
    std::map<std::string, llvm::DIType*> di_type_map;  // Aria types -> DWARF types
    bool debug_enabled;
    
    // Current module name (for distinguishing module functions from main functions)
    std::string current_module_name;

    // Async function context tracking (Phase 4.6: Coroutines)
    bool current_func_is_async;                    // Is current function async?
    llvm::BasicBlock* current_coro_suspend_block;  // Where return statements branch to in async functions
    llvm::AllocaInst* current_async_promise;       // Promise alloca for storing Result<T> in coroutine frame
    llvm::Type* current_async_result_type;         // The LLVM Result<T> struct type for this async function

    // P1-4: Contract tracking (Design by Contract)
    class FuncDeclStmt* current_func_decl;  // Currently generating function (for contract checking)

    // v0.4.3: User stack pop/peek destination type context
    // Set by VarDecl handler before generating initializer, so the CALL dispatcher
    // can propagate it to the fresh ExprCodegen instance for apop()/apeek().
    llvm::Type* ustack_pop_dest_type = nullptr;

    // v0.4.3+: SMT-proven user stack optimization
    // Functions where Z3 proved all apush() calls are type-homogeneous.
    // When the current function is in this set, codegen uses unchecked fast variants.
    std::set<std::string> ustack_optimized_funcs;
    bool ustack_fast_mode = false;  // Per-function flag, set at FUNC_DECL codegen start

    // v0.4.5: User hash table get destination type context
    llvm::Type* uhash_get_dest_type = nullptr;

    // v0.4.5+: SMT-proven user hash table optimization
    std::set<std::string> uhash_optimized_funcs;
    bool uhash_fast_mode = false;
    int uhash_handle_counter = 0;  // Per-function counter for handle tracking allocas

    // v0.5.0: Result<T> elision — functions proven infallible (no fail, no sys, no calls to fallible)
    // These return raw T instead of Result{T, ptr, i8}, avoiding wrapping/unwrapping overhead.
    std::set<std::string> result_elide_funcs;
    bool result_elide_mode = false;  // Per-function flag, set at FUNC_DECL codegen start

    // v0.5.0: Dead branch elimination — IF nodes proven always-true or always-false
    // under Rules constraints. Codegen skips the dead branch entirely.
    std::set<ASTNode*> dead_branch_always_true;   // Condition always true → skip else
    std::set<ASTNode*> dead_branch_always_false;  // Condition always false → skip then

    // v0.5.0: Bounds check elimination — INDEX nodes proven always in-bounds
    // under Rules constraints. Codegen skips the runtime bounds check.
    std::set<ASTNode*> bounds_check_safe;  // Index always in [0, N)

    // v0.5.0: Overflow check elimination — BINARY_OP nodes proven overflow-free
    // under Rules constraints. Codegen uses plain add/sub/mul instead of safe variants.
    std::set<ASTNode*> overflow_check_safe;  // Arithmetic cannot overflow

    // v0.14.4: Division-by-zero check elimination — BINARY_OP nodes (/ and %) where
    // the divisor is proven non-zero via Rules/range inference. Codegen uses plain
    // sdiv/srem instead of the safe variant with zero-check select.
    std::set<ASTNode*> div_check_safe;  // Divisor is never zero

    // v0.5.0: Null check elimination — UNWRAP/NULL_COALESCE nodes proven non-null
    // under Rules constraints. Codegen skips the null/None branch entirely.
    std::set<ASTNode*> null_check_safe;  // Expression is never null/zero

    // v0.5.0: Loop invariant hoisting — VarDecl nodes inside loops proven
    // loop-invariant (initializer doesn't reference modified vars or loop counter).
    // Codegen emits them before the loop header and skips them in the body.
    std::map<ASTNode*, std::vector<ASTNode*>> loop_hoist_map;  // loop → hoistable VarDecls
    std::set<ASTNode*> loop_hoisted_set;  // All hoisted VarDecl nodes (skip in body)
    bool emitting_hoisted = false;  // Guard: true while emitting hoisted decls (don't skip)

    // v0.5.0: Rules<T> propagation — VarDecl nodes where the limit check can be
    // skipped because all callers already satisfy the constraint transitively.
    std::set<ASTNode*> limit_check_safe;  // VarDecl limit checks proven redundant

    // v0.5.0: Defaults/?| fallback elimination — DefaultsExpr nodes where the
    // fallback path is dead because the sub-expression is provably infallible.
    std::set<ASTNode*> defaults_safe;  // DefaultsExpr nodes with dead fallback

    // v0.5.1: assert_static nodes that Z3 proved at compile time (emit nothing)
    std::set<ASTNode*> assert_static_proven;

public:
    /// Register functions whose user stacks are provably type-homogeneous (from Z3 phase)
    void setUStackOptimizedFuncs(const std::set<std::string>& funcs) {
        ustack_optimized_funcs = funcs;
    }
    /// Register functions whose user hashes are provably type-homogeneous (from Z3 phase)
    void setUHashOptimizedFuncs(const std::set<std::string>& funcs) {
        uhash_optimized_funcs = funcs;
    }
    /// Register functions proven infallible for Result<T> elision (from static analysis phase)
    void setResultElideFuncs(const std::set<std::string>& funcs) {
        result_elide_funcs = funcs;
    }
    /// Register IF nodes with provably always-true conditions (dead else branch)
    void setDeadBranchTrue(const std::set<ASTNode*>& nodes) {
        dead_branch_always_true = nodes;
    }
    /// Register IF nodes with provably always-false conditions (dead then branch)
    void setDeadBranchFalse(const std::set<ASTNode*>& nodes) {
        dead_branch_always_false = nodes;
    }
    /// Register INDEX nodes with provably always-in-bounds index (skip bounds check)
    void setBoundsCheckSafe(const std::set<ASTNode*>& nodes) {
        bounds_check_safe = nodes;
    }
    /// Register BINARY_OP nodes with provably no overflow (use plain arithmetic)
    void setOverflowCheckSafe(const std::set<ASTNode*>& nodes) {
        overflow_check_safe = nodes;
    }
    /// Register BINARY_OP (/ %) nodes with provably non-zero divisor (skip zero check)
    void setDivCheckSafe(const std::set<ASTNode*>& nodes) {
        div_check_safe = nodes;
    }
    /// Register UNWRAP/NULL_COALESCE nodes with provably non-null expression (skip null check)
    void setNullCheckSafe(const std::set<ASTNode*>& nodes) {
        null_check_safe = nodes;
    }
    /// Register loop-invariant VarDecl nodes to hoist before their enclosing loop
    void setLoopHoistMap(const std::map<ASTNode*, std::vector<ASTNode*>>& m) {
        loop_hoist_map = m;
        loop_hoisted_set.clear();
        for (const auto& [loop, decls] : loop_hoist_map) {
            for (auto* d : decls) loop_hoisted_set.insert(d);
        }
    }
    /// Register VarDecl nodes whose limit checks are proven redundant (Rules propagation)
    void setLimitCheckSafe(const std::set<ASTNode*>& nodes) {
        limit_check_safe = nodes;
    }
    /// Register DefaultsExpr nodes whose fallback is proven dead (infallible sub-expression)
    void setDefaultsSafe(const std::set<ASTNode*>& nodes) {
        defaults_safe = nodes;
    }
    /// Register assert_static nodes that Z3 proved at compile time
    void setAssertStaticProven(const std::set<ASTNode*>& nodes) {
        assert_static_proven = nodes;
    }
private:

    /**
     * Map Aria type to LLVM type
     * Reference: research_012-017 for type specifications
     */
    llvm::Type* mapType(Type* aria_type);
    
    /**
     * Map type name string to LLVM Type
     * @param type_name String name like "int32", "flt64", "bool"
     * @return Corresponding LLVM type
     */
    llvm::Type* mapTypeFromName(const std::string& type_name);
    
    /**
     * Generate code for a statement node
     */
    llvm::Value* codegenStatement(ASTNode* stmt);
    
    /**
     * Generate code for an expression node
     */
    llvm::Value* codegenExpression(ASTNode* expr);
    
    /**
     * v0.2.41: Emit runtime limit<> checks for a value against rules
     * Binds $ to the value, evaluates each condition, branches to failsafe on violation
     */
    void emitLimitChecks(const std::string& rulesName, llvm::Value* value, llvm::Function* currentFunc);
    
    /**
     * Execute defer blocks in the current scope (LIFO order)
     */
    void executeScopeDefers();
    
    /**
     * Execute all defer blocks up to function level (LIFO order)
     * Called by return statements
     */
    void executeFunctionDefers();
    
    /**
     * Map Aria type to DWARF debug type
     * Creates typedef for TBB types to enable custom formatters
     */
    llvm::DIType* mapDebugType(Type* aria_type);
    
    /**
     * Push a new lexical scope onto the debug scope stack
     */
    void pushDebugScope(llvm::DIScope* scope);
    
    /**
     * Pop the current lexical scope from the stack
     */
    void popDebugScope();
    
    /**
     * Get the current debug scope (top of stack)
     */
    llvm::DIScope* getCurrentDebugScope();
    
    // ========================================================================
    // Optional Type Helper Methods (Session 23 - Phase 2)
    // ========================================================================
    
    /**
     * Create an optional value with a present value (Some)
     * @param value The value to wrap in the optional
     * @return LLVM struct { i1 true, T value }
     */
    llvm::Value* createOptionalSome(llvm::Value* value);
    
    /**
     * Create an optional value representing absence (None/NIL)
     * @param optionalType The full optional struct type { i1, T }
     * @return LLVM struct { i1 false, T undef }
     */
    llvm::Value* createOptionalNone(llvm::Type* optionalType);

    // ========================================================================
    // ARIA-024: Limb-Based Integral Model (LBIM) for Large Integers
    // Implements arithmetic on struct-wrapped limb arrays to bypass LLVM bugs
    // See: github.com/llvm/llvm-project/issues/68751, #56351
    // ========================================================================

    /**
     * Check if an LLVM type is a LBIM large integer struct (int128/256/512/1024)
     * @param type LLVM type to check
     * @return Number of limbs (2, 4, 8, or 16) if LBIM type, 0 otherwise
     */
    unsigned isLBIMType(llvm::Type* type);

    /**
     * Check if an LLVM type is fix256 (Q128.128 deterministic fixed-point)
     * @param type LLVM type to check
     * @return true if fix256, false otherwise
     */
    bool isFix256Type(llvm::Type* type);

    /**
     * Generate LBIM addition using ripple-carry algorithm
     * @param L Left operand (LBIM struct)
     * @param R Right operand (LBIM struct)
     * @param numLimbs Number of i64 limbs (2, 4, 8, or 16)
     * @return Result LBIM struct
     */
    llvm::Value* generateLBIMAdd(llvm::Value* L, llvm::Value* R, unsigned numLimbs);

    /**
     * Generate LBIM subtraction using borrow-chain algorithm
     * @param L Left operand (LBIM struct)
     * @param R Right operand (LBIM struct)
     * @param numLimbs Number of i64 limbs (2, 4, 8, or 16)
     * @return Result LBIM struct
     */
    llvm::Value* generateLBIMSub(llvm::Value* L, llvm::Value* R, unsigned numLimbs);

    /**
     * Generate LBIM comparison (signed less than)
     * Compares limbs from most significant to least significant
     * @param L Left operand (LBIM struct)
     * @param R Right operand (LBIM struct)
     * @param numLimbs Number of i64 limbs (2, 4, or 8)
     * @return i1 result (true if L < R)
     */
    llvm::Value* generateLBIMSLT(llvm::Value* L, llvm::Value* R, unsigned numLimbs);

    /**
     * Generate LBIM comparison (unsigned less than)
     * @param L Left operand (LBIM struct)
     * @param R Right operand (LBIM struct)
     * @param numLimbs Number of i64 limbs (2, 4, or 8)
     * @return i1 result (true if L < R unsigned)
     */
    llvm::Value* generateLBIMULT(llvm::Value* L, llvm::Value* R, unsigned numLimbs);

    /**
     * Generate LBIM equality comparison
     * @param L Left operand (LBIM struct)
     * @param R Right operand (LBIM struct)
     * @param numLimbs Number of i64 limbs (2, 4, or 8)
     * @return i1 result (true if L == R)
     */
    llvm::Value* generateLBIMEQ(llvm::Value* L, llvm::Value* R, unsigned numLimbs);

    /**
     * Generate LBIM multiplication using Schoolbook algorithm
     * Uses safe i128 intermediate operations to avoid IPSCCP crashes
     * @param L Left operand (LBIM struct)
     * @param R Right operand (LBIM struct)
     * @param numLimbs Number of i64 limbs (2, 4, or 8)
     * @return Result LBIM struct (lower N limbs of product)
     */
    llvm::Value* generateLBIMMul(llvm::Value* L, llvm::Value* R, unsigned numLimbs);

    /**
     * Promote int64 literal to LBIM struct (int128/256/512/1024/2048/4096)
     * Used when assigning literals to large integer variables
     * @param literal LLVM integer value to promote  
     * @param targetType Target LBIM struct type
     * @return Promoted LBIM struct value
     */
    llvm::Value* promoteToLBIMStruct(llvm::Value* literal, llvm::Type* targetType);

    /**
     * Generate LBIM signed division by calling runtime intrinsic
     * @param L Left operand (LBIM struct dividend)
     * @param R Right operand (LBIM struct divisor)
     * @param numLimbs Number of i64 limbs (2, 4, or 8)
     * @return Result LBIM struct (quotient)
     */
    llvm::Value* generateLBIMDiv(llvm::Value* L, llvm::Value* R, unsigned numLimbs);

    /**
     * Generate LBIM signed modulo by calling runtime intrinsic
     * @param L Left operand (LBIM struct dividend)
     * @param R Right operand (LBIM struct divisor)
     * @param numLimbs Number of i64 limbs (2, 4, or 8)
     * @return Result LBIM struct (remainder)
     */
    llvm::Value* generateLBIMMod(llvm::Value* L, llvm::Value* R, unsigned numLimbs);
    llvm::Value* generateLBIMAnd(llvm::Value* L, llvm::Value* R, unsigned numLimbs);
    llvm::Value* generateLBIMOr(llvm::Value* L, llvm::Value* R, unsigned numLimbs);
    llvm::Value* generateLBIMXor(llvm::Value* L, llvm::Value* R, unsigned numLimbs);
    llvm::Value* generateLBIMShl(llvm::Value* L, llvm::Value* R, unsigned numLimbs);
    llvm::Value* generateLBIMShr(llvm::Value* L, llvm::Value* R, unsigned numLimbs, bool isArithmetic);

    // ========================================================================
    // Unknown-Safe Arithmetic (Layer 1 Safety)
    // Division and modulo operations that return Unknown on divide-by-zero
    // instead of causing undefined behavior
    // ========================================================================

    /**
     * Generate safe integer division that returns Unknown sentinel on divide-by-zero
     * @param L Dividend (numerator)
     * @param R Divisor (denominator)
     * @param name Optional name for the result value
     * @return Division result, or Unknown sentinel (signed max) if R == 0
     */
    llvm::Value* generateSafeSDiv(llvm::Value* L, llvm::Value* R, const std::string& name = "divtmp");

    /**
     * Generate safe integer modulo that returns Unknown sentinel on divide-by-zero
     * @param L Dividend
     * @param R Divisor (modulus)
     * @param name Optional name for the result value
     * @return Modulo result, or Unknown sentinel (signed max) if R == 0
     */
    llvm::Value* generateSafeSRem(llvm::Value* L, llvm::Value* R, const std::string& name = "modtmp");

    /**
     * Generate safe integer addition that returns Unknown sentinel on overflow
     * @param L Left operand
     * @param R Right operand
     * @param name Optional name for the result value
     * @return Addition result, or Unknown sentinel (signed max) on overflow
     */
    llvm::Value* generateSafeAdd(llvm::Value* L, llvm::Value* R, const std::string& name = "addtmp");

    /**
     * Generate safe integer subtraction that returns Unknown sentinel on overflow
     * @param L Left operand (minuend)
     * @param R Right operand (subtrahend)
     * @param name Optional name for the result value
     * @return Subtraction result, or Unknown sentinel (signed max) on overflow
     */
    llvm::Value* generateSafeSub(llvm::Value* L, llvm::Value* R, const std::string& name = "subtmp");

    /**
     * Generate safe integer multiplication that returns Unknown sentinel on overflow
     * @param L Left operand
     * @param R Right operand
     * @param name Optional name for the result value
     * @return Multiplication result, or Unknown sentinel (signed max) on overflow
     */
    llvm::Value* generateSafeMul(llvm::Value* L, llvm::Value* R, const std::string& name = "multmp");

public:
    /**
     * Constructor
     * @param module_name Name of the LLVM module to generate
     * @param enable_debug Enable DWARF debug info emission (default: true)
     */
    explicit IRGenerator(const std::string& module_name, bool enable_debug = true);
    
    /**
     * Initialize debug info generation
     * Must be called before codegen if debug is enabled
     * @param filename Source filename (e.g., "main.aria")
     * @param directory Source directory (absolute path)
     */
    void initDebugInfo(const std::string& filename, const std::string& directory);
    
    /**
     * Finalize debug info generation
     * Must be called after all codegen is complete
     */
    void finalizeDebugInfo();
    
    /**
     * Set current source location for debug info
     * @param line Line number (1-based)
     * @param column Column number (1-based)
     */
    void setDebugLocation(unsigned line, unsigned column);
    
    /**
     * Clear debug location (for compiler-generated code)
     */
    void clearDebugLocation();
    
    /**
     * Process module declarations recursively, generating IR for all functions
     * @param declarations Vector of declarations (may include nested modules)
     * @param modulePrefix Qualified module name prefix (e.g., "math", "network.http")
     */
    void processModuleDeclarations(const std::vector<std::shared_ptr<ASTNode>>& declarations, 
                                    const std::string& modulePrefix = "");
    
    /**
     * Set the TypeSystem for looking up custom types (structs, unions, etc.)
     * Must be called before codegen if the code uses custom types
     * @param ts Pointer to TypeSystem (must remain valid for lifetime of IR generation)
     */
    void setTypeSystem(sema::TypeSystem* ts);
    
    /**
     * Set the current module name (for determining function linkage)
     * Module functions get external linkage, main file functions follow isPublic
     * @param module_name Name of current module being compiled (empty for main file)
     */
    void setCurrentModuleName(const std::string& module_name);
    
    /**
     * Generate LLVM IR for an AST node
     * @param node AST node to generate code for
     * @return LLVM Value representing the generated code
     */
    llvm::Value* codegen(ASTNode* node);
    
    /**
     * Generate LLVM IR for specialized generic functions
     * @param specializations Vector of function specializations from Monomorphizer
     * @return Number of functions generated
     */
    size_t codegenSpecializedFunctions(const std::vector<sema::Specialization*>& specializations);
    
    /**
     * Forward-declare specialized functions (signatures only, no bodies).
     * Call this BEFORE main module codegen so call sites can resolve.
     */
    void declareSpecializedFunctions(const std::vector<sema::Specialization*>& specializations);
    
    /**
     * Generate bodies for previously declared specialized functions.
     * Call this AFTER main module codegen so impl methods are available.
     */
    size_t codegenSpecializedBodies(const std::vector<sema::Specialization*>& specializations);
    
    /**
     * Get the generated LLVM module
     * @return Pointer to LLVM Module (ownership retained)
     */
    llvm::Module* getModule();
    
    /**
     * Take ownership of the generated LLVM module
     * @return unique_ptr to LLVM Module (ownership transferred)
     */
    std::unique_ptr<llvm::Module> takeModule();
    
    /**
     * Dump the generated IR to stdout (for debugging)
     */
    void dump();
    
    // ========================================================================
    // PHASE 4: Zero Implicit Conversion - Type Suffix Helpers
    // ========================================================================
    
    /**
     * Get LLVM type from explicit type suffix
     * @param suffix Type suffix string (e.g., "u32", "i64", "f32")
     * @return Corresponding LLVM type or nullptr if unknown
     */
    llvm::Type* getLLVMTypeFromSuffix(const std::string& suffix);
    
    /**
     * Check if type suffix represents a signed integer
     * @param suffix Type suffix string (e.g., "i32" -> true, "u32" -> false)
     * @return true if signed, false if unsigned or non-integer
     */
    bool isSuffixSigned(const std::string& suffix);
};

} // namespace aria

#endif // ARIA_IR_GENERATOR_H
