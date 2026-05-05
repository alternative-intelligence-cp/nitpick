#ifndef ARIA_RANGE_ANALYSIS_H
#define ARIA_RANGE_ANALYSIS_H

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace npk {

class ASTNode;
class FuncDeclStmt;
class RulesDeclStmt;

// ============================================================================
// v0.14.1: Intraprocedural Range Inference
//
// Walks a function body and infers [lo, hi] integer ranges for local variables
// based on: literal assignments, arithmetic, conditional narrowing, and loop
// induction variables. Produces a VarRulesMap compatible with the existing
// Z3 elimination passes (dead branch, bounds check, overflow, null check).
//
// Flow-sensitive: records per-node range snapshots so elimination walkers
// can look up the exact ranges valid at each program point.
// ============================================================================

/// Inferred value range for a single variable
struct InferredRange {
    int64_t lo;
    int64_t hi;
    std::string typeName;  // e.g., "int32", "int64"
    bool valid = true;     // false if range is unknown/full-width

    InferredRange() : lo(0), hi(0), valid(false) {}
    InferredRange(int64_t l, int64_t h, const std::string& t)
        : lo(l), hi(h), typeName(t), valid(true) {}
};

/// Range map: variable name → inferred range
using RangeMap = std::map<std::string, InferredRange>;

/// VarRulesMap type matching the existing elimination pass interface
using VarRulesMap = std::map<std::string, std::pair<RulesDeclStmt*, std::string>>;

class RangeAnalyzer {
public:
    RangeAnalyzer() = default;
    ~RangeAnalyzer() = default;

    /// Analyze a function and infer variable ranges.
    /// Records per-node range snapshots for flow-sensitive lookups.
    /// Returns the final RangeMap at function exit.
    RangeMap analyzeFunction(FuncDeclStmt* func);

    /// Convert inferred ranges to synthetic RulesDeclStmt objects that can
    /// be merged into existing VarRulesMap for elimination passes.
    /// The returned VarRulesMap contains only the inferred entries.
    /// Caller should merge with explicit Rules (explicit takes priority).
    VarRulesMap toVarRules(const RangeMap& ranges);

    /// Get the inferred ranges snapshot at a specific AST node.
    /// Returns nullptr if the node was not visited during analysis.
    const RangeMap* getRangesAt(ASTNode* node) const;

    /// Merge inferred ranges into a VarRulesMap for Z3 elimination passes.
    /// For each variable in 'ranges' that is NOT already in 'target' (explicit
    /// Rules take priority), creates a synthetic RulesDeclStmt and adds it.
    /// Skips full-width ranges (no useful constraint).
    void mergeInferred(VarRulesMap& target, const RangeMap& ranges);

    /// Get the full-width range for a type name (public for external use)
    static InferredRange fullRange(const std::string& typeName);

    /// Get the synthetic RulesDeclStmt storage (lifetime managed here)
    const std::vector<std::unique_ptr<RulesDeclStmt>>& getSyntheticRules() const {
        return syntheticRules;
    }

private:
    /// Storage for synthetic RulesDeclStmt objects (lifetime management)
    std::vector<std::unique_ptr<RulesDeclStmt>> syntheticRules;

    /// Per-node range snapshots (filled during analyzeFunction)
    std::unordered_map<ASTNode*, RangeMap> nodeRanges;

    /// Walk a statement/block and update ranges (also records per-node snapshots)
    void walkStmt(ASTNode* node, RangeMap& ranges);

    /// Record ranges at all sub-expression nodes in an expression tree
    void recordSubExprs(ASTNode* node, const RangeMap& ranges);

    /// Evaluate a constant expression, returning (value, valid)
    std::pair<int64_t, bool> evalConstant(ASTNode* node, const RangeMap& ranges);

    /// Compute the range of an expression given current ranges
    InferredRange evalExprRange(ASTNode* node, const RangeMap& ranges,
                                const std::string& defaultType);

    /// Narrow ranges based on a condition being true
    void narrowFromCondition(ASTNode* condition, RangeMap& ranges, bool condIsTrue);

    /// Check if a range addition/subtraction/multiplication overflows
    static bool addOverflows(int64_t a, int64_t b);
    static bool subOverflows(int64_t a, int64_t b);
    static bool mulOverflows(int64_t a, int64_t b);
};

} // namespace npk

#endif // ARIA_RANGE_ANALYSIS_H
