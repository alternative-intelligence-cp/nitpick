#ifndef ARIA_Z3_VERIFIER_H
#define ARIA_Z3_VERIFIER_H

#include <z3.h>
#include <string>
#include <vector>
#include <map>

namespace aria {

class ASTNode;
class RulesDeclStmt;
class FuncDeclStmt;

namespace sema {
    class TypeSystem;
}

/// Result of attempting to verify a single constraint
enum class VerifyResult {
    PROVEN,     // Constraint proven to hold for all inputs
    DISPROVEN,  // Counterexample found — constraint can be violated
    UNKNOWN     // Solver could not determine (timeout/too complex)
};

/// A single verification outcome with details
struct VerifyOutcome {
    VerifyResult result;
    std::string rulesName;
    std::string conditionText;  // Human-readable condition
    std::string detail;         // Extra info (counterexample, etc.)
    int line = 0;
    int column = 0;
};

/// Summary of all verification results for a compilation unit
struct VerifySummary {
    int proven = 0;
    int disproven = 0;
    int unknown = 0;
    std::vector<VerifyOutcome> outcomes;
    
    int total() const { return proven + disproven + unknown; }
};

/**
 * Z3Verifier — SMT-based static constraint verification for Aria
 *
 * Uses Z3 to mathematically prove Rules/limit constraints at compile time.
 * For literal initializers: proves that the value satisfies all conditions.
 * For dynamic values: attempts proof using available range information.
 *
 * Core idea: to prove property P holds, assert NOT(P) and check satisfiability.
 * - If NOT(P) is UNSATISFIABLE → P is proven for all inputs
 * - If NOT(P) is SATISFIABLE → counterexample found (P can fail)
 * - If UNKNOWN → solver couldn't decide (keep runtime check)
 */
class Z3Verifier {
public:
    Z3Verifier();
    explicit Z3Verifier(int timeout_ms);
    ~Z3Verifier();

    // Non-copyable (Z3 context is not safely copyable)
    Z3Verifier(const Z3Verifier&) = delete;
    Z3Verifier& operator=(const Z3Verifier&) = delete;

    /// Set the type system for looking up type information
    void setTypeSystem(sema::TypeSystem* ts) { type_system = ts; }

    /// Set whether to produce detailed output
    void setVerbose(bool v) { verbose = v; }

    /// Register a Rules declaration for later verification
    void registerRules(const std::string& name, RulesDeclStmt* rules);

    /// Verify a limit<rulesName> applied to an integer literal value
    VerifyResult verifyLimitInt(const std::string& rulesName, int64_t value,
                                std::vector<VerifyOutcome>& outcomes,
                                int line = 0, int column = 0);

    /// Verify a limit<rulesName> applied to a float literal value
    VerifyResult verifyLimitFloat(const std::string& rulesName, double value,
                                  std::vector<VerifyOutcome>& outcomes,
                                  int line = 0, int column = 0);

    /// Verify that a Rules' conditions are satisfiable (not self-contradictory)
    VerifyResult verifyRulesConsistency(const std::string& rulesName,
                                        std::vector<VerifyOutcome>& outcomes);

    /// Attempt to prove a limit<rulesName> with a symbolic value constrained
    /// by known range [lo, hi]. Returns PROVEN if all conditions hold for all
    /// values in the range.
    VerifyResult verifyLimitRange(const std::string& rulesName,
                                  int64_t lo, int64_t hi,
                                  std::vector<VerifyOutcome>& outcomes,
                                  int line = 0, int column = 0);

    // ================================================================
    // Phase 2: Design-by-Contract — requires/ensures verification
    // ================================================================

    /// Verify that a function's requires/ensures clauses are internally
    /// consistent (no self-contradiction) and attempt to prove ensures
    /// from requires + function body structure.
    VerifyResult verifyFunctionContract(FuncDeclStmt* func,
                                        std::vector<VerifyOutcome>& outcomes);

    /// Verify a specific call site: given the caller's known constraints,
    /// prove the callee's requires clauses are satisfied by the arguments.
    /// paramConstraints maps parameter index → known range [lo, hi].
    VerifyResult verifyCallSiteRequires(FuncDeclStmt* callee,
                                         const std::vector<ASTNode*>& args,
                                         std::vector<VerifyOutcome>& outcomes,
                                         int line = 0, int column = 0);

    /// Cross-function contract propagation (v0.5.2): verify that a call site
    /// satisfies the callee's requires clauses, using the caller's own
    /// constraints (requires clauses + Rules on args + ensures-derived facts)
    /// as assumptions.
    /// ensuresFacts: variable name → FuncDeclStmt whose ensures apply to that var
    VerifyResult verifyCallSiteContract(
        FuncDeclStmt* callee,
        FuncDeclStmt* caller,
        const std::vector<ASTNode*>& args,
        const std::map<std::string, std::pair<RulesDeclStmt*, std::string>>& callerVarRules,
        const std::vector<std::pair<std::string, FuncDeclStmt*>>& ensuresFacts,
        std::vector<VerifyOutcome>& outcomes,
        int line = 0, int column = 0);

    /// Loop invariant verification (v0.5.2): verify that a loop invariant
    /// is consistent with the function's preconditions.
    /// Checks that the invariant is satisfiable given caller constraints.
    VerifyResult verifyLoopInvariant(
        FuncDeclStmt* enclosingFunc,
        const std::vector<ASTNode*>& invariants,
        std::vector<VerifyOutcome>& outcomes,
        int line = 0, int column = 0);

    // ================================================================
    // Phase 3: Arithmetic Overflow Verification
    // ================================================================

    /// Verify that an integer arithmetic operation cannot overflow given
    /// known ranges of operands. op is '+', '-', or '*'.
    /// Returns PROVEN if no overflow possible, DISPROVEN if overflow found.
    VerifyResult verifyNoOverflow(char op, int bitWidth, bool isSigned,
                                  int64_t lhsLo, int64_t lhsHi,
                                  int64_t rhsLo, int64_t rhsHi,
                                  std::vector<VerifyOutcome>& outcomes,
                                  int line = 0, int column = 0);

    // ================================================================
    // Phase 4: User Stack Type Homogeneity Verification (v0.4.3+)
    // ================================================================

    /// Verify that all pushes to a user stack in a scope use the same type.
    /// pushTypeTags contains the compile-time type tag for each apush() call.
    /// If PROVEN, runtime type checks can be eliminated (SMT-optimized fast path).
    VerifyResult verifyUStackHomogeneous(
        const std::vector<int64_t>& pushTypeTags,
        int64_t expectedPopTag,
        std::vector<VerifyOutcome>& outcomes,
        int line = 0, int column = 0);

    // Phase 5: User Hash Type Homogeneity Verification (v0.4.5+)
    // ================================================================

    /// Verify that all ahset() value insertions use the same type tag.
    /// If PROVEN, runtime type checks can be eliminated (SMT-optimized fast path).
    VerifyResult verifyUHashHomogeneous(
        const std::vector<int64_t>& setTypeTags,
        int64_t expectedTag,
        std::vector<VerifyOutcome>& outcomes,
        int line = 0, int column = 0);

    // Phase 6: Dead Branch Elimination (v0.5.0+)
    // ================================================================

    /// Prove whether a branch condition is always true or always false given
    /// Rules constraints on the variables involved. Used for dead branch
    /// elimination: if ALWAYS_TRUE, the else branch is dead; if ALWAYS_FALSE,
    /// the then branch is dead.
    ///
    /// @param condition   The if-condition AST expression
    /// @param varRules    Map of variable name → (Rules pointer, base type name)
    /// @param outcomes    Verification outcomes appended here
    /// @param line,column Source location for diagnostics
    /// @return PROVEN = always true, DISPROVEN = always false, UNKNOWN = can't decide
    VerifyResult proveDeadBranch(
        ASTNode* condition,
        const std::map<std::string, std::pair<RulesDeclStmt*, std::string>>& varRules,
        std::vector<VerifyOutcome>& outcomes,
        int line = 0, int column = 0);

    // Phase 7: Bounds Check Elimination (v0.5.0+)
    // ================================================================

    /// Prove that an array index expression is always within [0, arraySize)
    /// given Rules constraints on the variables involved. If PROVEN, the
    /// runtime bounds check can be skipped.
    ///
    /// @param indexExpr   The index expression AST node
    /// @param arraySize   Compile-time known array size (N for T[N])
    /// @param varRules    Map of variable name → (Rules pointer, base type name)
    /// @param outcomes    Verification outcomes appended here
    /// @param line,column Source location for diagnostics
    /// @return PROVEN = always in bounds, UNKNOWN = can't decide
    VerifyResult proveBoundsInRange(
        ASTNode* indexExpr,
        int64_t arraySize,
        const std::map<std::string, std::pair<RulesDeclStmt*, std::string>>& varRules,
        std::vector<VerifyOutcome>& outcomes,
        int line = 0, int column = 0);

    // Phase 8: Overflow Check Elimination (v0.5.0+)
    // ================================================================

    /// Prove that an integer arithmetic operation (+, -, *) cannot overflow
    /// given Rules constraints on the operand variables. If PROVEN, the
    /// runtime overflow check (generateSafeAdd/Sub/Mul) can be replaced
    /// with a plain add/sub/mul.
    ///
    /// @param op          '+', '-', or '*'
    /// @param lhsExpr     Left operand AST node
    /// @param rhsExpr     Right operand AST node
    /// @param varRules    Map of variable name → (Rules pointer, base type name)
    /// @param outcomes    Verification outcomes appended here
    /// @param line,column Source location for diagnostics
    /// @return PROVEN = no overflow possible, UNKNOWN = can't decide
    VerifyResult proveNoOverflowFromRules(
        char op,
        ASTNode* lhsExpr,
        ASTNode* rhsExpr,
        const std::map<std::string, std::pair<RulesDeclStmt*, std::string>>& varRules,
        std::vector<VerifyOutcome>& outcomes,
        int line = 0, int column = 0);

    // Phase 9: Null Check Elimination (v0.5.0+)
    // ================================================================

    /// Prove that an expression is never null/zero/None given Rules constraints
    /// on the variables involved. If PROVEN, the null check in ?? / ?| / ?
    /// operators can be skipped — codegen uses the value directly.
    ///
    /// For integers: proves the value is never 0 (NIL sentinel).
    /// For optionals: proves hasValue is always true.
    /// For pointers: proves pointer is never null.
    ///
    /// @param expr        The expression being null-checked
    /// @param varRules    Map of variable name → (Rules pointer, base type name)
    /// @param outcomes    Verification outcomes appended here
    /// @param line,column Source location for diagnostics
    /// @return PROVEN = never null, UNKNOWN = can't decide
    VerifyResult proveNonNullFromRules(
        ASTNode* expr,
        const std::map<std::string, std::pair<RulesDeclStmt*, std::string>>& varRules,
        std::vector<VerifyOutcome>& outcomes,
        int line = 0, int column = 0);

    // Phase 10: Rules<T> Propagation (v0.5.0+)
    // ================================================================

    /// Prove that one set of Rules constraints subsumes another:
    /// ∀x. callerRules(x) → calleeRules(x)
    /// If PROVEN, the callee's limit check can be skipped when the caller
    /// has already validated the argument under callerRules.
    ///
    /// @param callerRulesName  Rules already satisfied by the caller's argument
    /// @param calleeRulesName  Rules the callee wants to check
    /// @param typeName         Base type name (for sort resolution)
    /// @param outcomes         Verification outcomes appended here
    /// @param line,column      Source location for diagnostics
    /// @return PROVEN = caller constraints subsume callee, UNKNOWN = can't decide
    VerifyResult proveRulesSubsumption(
        const std::string& callerRulesName,
        const std::string& calleeRulesName,
        const std::string& typeName,
        std::vector<VerifyOutcome>& outcomes,
        int line = 0, int column = 0);

    /// Prove a user assertion (prove/assert_static statement).
    /// Translates the boolean expression to Z3 under the current scope's
    /// variable constraints (Rules<T> on local variables) and checks validity.
    /// Returns PROVEN if the assertion always holds, DISPROVEN with
    /// counterexample if a violation exists, or UNKNOWN if the solver
    /// can't determine.
    VerifyResult proveUserAssertion(
        ASTNode* condition,
        const std::map<std::string, std::pair<RulesDeclStmt*, std::string>>& varRules,
        std::vector<VerifyOutcome>& outcomes,
        int line = 0, int column = 0);

    // ================================================================
    // Phase 12: Rules<T> Transitivity — Call-site narrowing (v0.5.3)
    // ================================================================

    /// Verify Rules narrowing at a call site: for each callee parameter that
    /// has a Rules constraint (type name matches rules_table), check if the
    /// caller's argument constraint subsumes the callee's requirement.
    /// Reports DISPROVEN if narrowing is invalid (caller range doesn't cover
    /// callee range), PROVEN if valid, UNKNOWN if can't decide.
    VerifyResult verifyRulesNarrowing(
        FuncDeclStmt* callee,
        const std::vector<ASTNode*>& args,
        const std::map<std::string, std::pair<RulesDeclStmt*, std::string>>& callerVarRules,
        std::vector<VerifyOutcome>& outcomes,
        int line = 0, int column = 0);

    // ================================================================
    // Phase 13: Pattern Exhaustiveness via SMT (v0.5.3)
    // ================================================================

    /// Prove that a pick statement's cases exhaustively cover the selector's
    /// domain. If the selector variable has Rules constraints, uses them to
    /// bound the domain. Encodes each case pattern as a Z3 disjunct, then
    /// checks if NOT(any pattern matches) is UNSAT under the Rules constraints.
    /// Returns PROVEN if exhaustive, DISPROVEN with counterexample if there's
    /// an uncovered value.
    VerifyResult provePickExhaustiveness(
        ASTNode* selector,
        const std::vector<ASTNode*>& patterns,
        const std::map<std::string, std::pair<RulesDeclStmt*, std::string>>& varRules,
        std::vector<VerifyOutcome>& outcomes,
        int line = 0, int column = 0);

    // ================================================================
    // Phase 14: Rules + Null Interaction (v0.5.3)
    // ================================================================

    /// Prove that a variable's Rules constraints exclude NIL (null/zero).
    /// If the Rules conditions on a variable entail $ != 0 (the NIL
    /// representation), returns PROVEN. Useful for proving Optional values
    /// are never null when constrained by Rules.
    VerifyResult proveRulesExcludesNull(
        const std::string& rulesName,
        const std::string& typeName,
        std::vector<VerifyOutcome>& outcomes,
        int line = 0, int column = 0);

    // ================================================================
    // Phase 15: Automatic Rules Widening/Narrowing (v0.5.3)
    // ================================================================

    /// Infer the tightest integer bounds on a function's return value given
    /// its parameter Rules constraints. Uses Z3 optimization (minimize/maximize)
    /// to find the min and max possible return values.
    /// Returns PROVEN if bounds could be computed, with the inferred range
    /// in the outcome detail string. Stores inferred bounds in outMin/outMax.
    VerifyResult inferReturnBounds(
        FuncDeclStmt* func,
        const std::map<std::string, std::pair<RulesDeclStmt*, std::string>>& paramRules,
        int64_t& outMin, int64_t& outMax,
        std::vector<VerifyOutcome>& outcomes,
        int line = 0, int column = 0);

    // ================================================================
    // Phase 16: Data Race Detection (v0.5.4)
    // ================================================================

    /// Represents a variable access in a specific thread context.
    struct ThreadAccess {
        std::string varName;       // Variable being accessed
        bool isWrite;              // true = write, false = read
        std::string threadFunc;    // Function running in the thread
        bool isProtected;          // true if access is inside lock/unlock
        int line;
        int column;
    };

    /// Verify that shared variables accessed across thread boundaries are
    /// properly synchronized. Walks the spawning function and each spawned
    /// thread function to collect variable accesses, then uses Z3 to check
    /// if any conflicting accesses (read+write or write+write on the same
    /// variable) can occur without mutex protection.
    VerifyResult verifyDataRaceFreedom(
        FuncDeclStmt* spawner,
        const std::vector<FuncDeclStmt*>& threadFuncs,
        const std::map<std::string, FuncDeclStmt*>& allFuncs,
        std::vector<VerifyOutcome>& outcomes,
        int line = 0, int column = 0);

    // ================================================================
    // Phase 17: Deadlock Freedom (v0.5.4)
    // ================================================================

    /// Represents a lock acquisition in a function's call path.
    struct LockAcquisition {
        std::string lockVar;       // Variable name of the mutex/rwlock
        std::string funcName;      // Function where lock is acquired
        int line;
        int column;
    };

    /// Verify that lock acquisition order is consistent across all functions.
    /// Uses Z3 to assign integer ordinals to locks and prove that all
    /// acquisition sequences are strictly ascending (or compatible).
    /// Detects potential deadlocks from cyclic lock ordering.
    VerifyResult verifyDeadlockFreedom(
        const std::map<std::string, FuncDeclStmt*>& allFuncs,
        std::vector<VerifyOutcome>& outcomes,
        int line = 0, int column = 0);

    // ================================================================
    // Phase 18: Use-After-Free Proofs via SMT (v0.5.4)
    // ================================================================

    /// Strengthen the borrow checker's WildState analysis with Z3 for cases
    /// where control flow makes the state uncertain (MAY_FREED).
    /// Encodes the control flow conditions as Z3 formulas and proves whether
    /// a variable is ALWAYS freed or NEVER freed at a given program point.
    /// Returns PROVEN if variable cannot be used after free, DISPROVEN if
    /// a use-after-free path exists.
    VerifyResult proveNoUseAfterFree(
        FuncDeclStmt* func,
        const std::string& varName,
        const std::map<std::string, std::pair<RulesDeclStmt*, std::string>>& varRules,
        std::vector<VerifyOutcome>& outcomes,
        int line = 0, int column = 0);

    // ================================================================
    // Phase 19: Stack Overflow Prediction (v0.5.4)
    // ================================================================

    /// Prove that a recursive function's recursion depth is bounded.
    /// Identifies recursive calls and the "decreasing" argument, then uses
    /// Z3 to verify that: (1) the argument strictly decreases on each call,
    /// and (2) there is a base case that terminates recursion.
    /// If the function's parameter has Rules constraints, those bound the
    /// initial depth. Returns PROVEN with the max depth if bounded.
    VerifyResult proveRecursionBounded(
        FuncDeclStmt* func,
        const std::map<std::string, FuncDeclStmt*>& allFuncs,
        const std::map<std::string, std::pair<RulesDeclStmt*, std::string>>& paramRules,
        std::vector<VerifyOutcome>& outcomes,
        int line = 0, int column = 0);

    /// Get verification summary
    const VerifySummary& getSummary() const { return summary; }

    /// Reset state for a new compilation unit
    void reset();

private:
    Z3_context ctx;
    sema::TypeSystem* type_system = nullptr;
    bool verbose = false;

    // Rules table (mirrors the type checker's, set during AST walk)
    std::map<std::string, RulesDeclStmt*> rules_table;

    // Verification summary
    VerifySummary summary;

    // Translate an Aria Rules condition AST node to a Z3 expression
    // `dollar` is the Z3 variable representing $
    Z3_ast translateCondition(ASTNode* node, Z3_ast dollar, Z3_sort sort);

    // Translate a rule operand (sub-expression) to a Z3 expression
    Z3_ast translateOperand(ASTNode* node, Z3_ast dollar, Z3_sort sort);

    // Translate an expression to Z3 using a variable environment
    // (for contract verification where multiple named variables exist)
    Z3_ast translateExprWithEnv(ASTNode* node,
                                const std::map<std::string, Z3_ast>& env,
                                Z3_sort defaultSort);

    // Get the Z3 sort for an Aria integer type width
    Z3_sort getIntSort(int bitWidth);

    // Get the Z3 sort for an Aria float type
    Z3_sort getRealSort();

    // Resolve an Aria type name to a Z3 sort and bit width
    // Returns {sort, bitWidth, isFloat}
    struct TypeInfo {
        Z3_sort sort;
        int bitWidth;
        bool isFloat;
    };
    TypeInfo resolveTypeSort(const std::string& typeName);

    // Check satisfiability of current assertions
    Z3_lbool checkSat(Z3_solver solver);

    // Create a fresh solver with push/pop scope
    Z3_solver makeSolver();
    void deleteSolver(Z3_solver solver);
};

} // namespace aria

#endif // ARIA_Z3_VERIFIER_H
