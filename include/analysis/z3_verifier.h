#ifndef ARIA_Z3_VERIFIER_H
#define ARIA_Z3_VERIFIER_H

#include <z3.h>
#include <string>
#include <vector>
#include <map>

namespace aria {

class ASTNode;
class RulesDeclStmt;

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

    // Get the Z3 sort for an Aria integer type width
    Z3_sort getIntSort(int bitWidth);

    // Get the Z3 sort for an Aria float type
    Z3_sort getRealSort();

    // Check satisfiability of current assertions
    Z3_lbool checkSat(Z3_solver solver);

    // Create a fresh solver with push/pop scope
    Z3_solver makeSolver();
    void deleteSolver(Z3_solver solver);
};

} // namespace aria

#endif // ARIA_Z3_VERIFIER_H
