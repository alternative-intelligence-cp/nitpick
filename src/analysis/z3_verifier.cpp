// ============================================================================
// Z3Verifier — SMT-based static constraint verification for Aria
// v0.2.45: Phase 1 — Rules/limit verification with Z3
// ============================================================================
//
// How SMT verification works:
//
// To prove that property P always holds, we ask Z3: "Is NOT(P) satisfiable?"
//   - If Z3 says UNSAT (no solution exists) → P is proven for ALL inputs
//   - If Z3 says SAT (solution exists) → the solution is a counterexample
//   - If Z3 says UNKNOWN → solver timed out or formula too complex
//
// For Aria's Rules/limit system:
//   Rules:r_pos = { $ > 0 };
//   limit<r_pos> int32:x = 42;
//
// We encode:  dollar = 42, NOT(dollar > 0)  →  42 > 0 is false?  →  UNSAT  →  PROVEN
//
// For range proofs (symbolic values with known constraints):
//   We assert: lo <= dollar <= hi, then check NOT(condition)
//   If UNSAT → condition holds for all values in [lo, hi]
//
// ============================================================================

#include "analysis/z3_verifier.h"
#include "frontend/ast/stmt.h"
#include "frontend/ast/expr.h"
#include "frontend/token.h"
#include <iostream>

namespace aria {

Z3Verifier::Z3Verifier() {
    Z3_config cfg = Z3_mk_config();
    // Set a reasonable timeout (5 seconds per query)
    Z3_set_param_value(cfg, "timeout", "5000");
    ctx = Z3_mk_context(cfg);
    Z3_del_config(cfg);
}

Z3Verifier::~Z3Verifier() {
    if (ctx) {
        Z3_del_context(ctx);
        ctx = nullptr;
    }
}

void Z3Verifier::reset() {
    rules_table.clear();
    summary = VerifySummary{};
}

void Z3Verifier::registerRules(const std::string& name, RulesDeclStmt* rules) {
    rules_table[name] = rules;
}

Z3_solver Z3Verifier::makeSolver() {
    Z3_solver s = Z3_mk_solver(ctx);
    Z3_solver_inc_ref(ctx, s);
    return s;
}

void Z3Verifier::deleteSolver(Z3_solver s) {
    Z3_solver_dec_ref(ctx, s);
}

Z3_sort Z3Verifier::getIntSort(int bitWidth) {
    return Z3_mk_bv_sort(ctx, bitWidth);
}

Z3_sort Z3Verifier::getRealSort() {
    return Z3_mk_real_sort(ctx);
}

Z3_lbool Z3Verifier::checkSat(Z3_solver solver) {
    return Z3_solver_check(ctx, solver);
}

// ============================================================================
// Translate Aria AST condition → Z3 expression
// ============================================================================

Z3_ast Z3Verifier::translateOperand(ASTNode* node, Z3_ast dollar, Z3_sort sort) {
    if (!node) return nullptr;

    // $ placeholder → return the dollar variable
    if (node->type == ASTNode::NodeType::IDENTIFIER) {
        auto* ident = static_cast<IdentifierExpr*>(node);
        if (ident->name == "$") {
            return dollar;
        }
        // NIL → 0
        if (ident->name == "NIL") {
            Z3_sort_kind sk = Z3_get_sort_kind(ctx, sort);
            if (sk == Z3_BV_SORT) {
                unsigned bw = Z3_get_bv_sort_size(ctx, sort);
                return Z3_mk_numeral(ctx, "0", Z3_mk_bv_sort(ctx, bw));
            }
            return Z3_mk_numeral(ctx, "0", sort);
        }
    }

    // Integer/float literal
    if (node->type == ASTNode::NodeType::LITERAL) {
        auto* literal = static_cast<LiteralExpr*>(node);
        if (std::holds_alternative<int64_t>(literal->value)) {
            int64_t val = std::get<int64_t>(literal->value);
            return Z3_mk_numeral(ctx, std::to_string(val).c_str(), sort);
        }
        if (std::holds_alternative<double>(literal->value)) {
            double val = std::get<double>(literal->value);
            // For reals, use decimal string
            return Z3_mk_numeral(ctx, std::to_string(val).c_str(), sort);
        }
    }

    // Unary minus
    if (node->type == ASTNode::NodeType::UNARY_OP) {
        auto* unary = static_cast<UnaryExpr*>(node);
        if (unary->op.type == frontend::TokenType::TOKEN_MINUS) {
            Z3_ast operand = translateOperand(unary->operand.get(), dollar, sort);
            if (!operand) return nullptr;
            Z3_sort_kind sk = Z3_get_sort_kind(ctx, sort);
            if (sk == Z3_BV_SORT) {
                return Z3_mk_bvneg(ctx, operand);
            }
            // Real sort: multiply by -1
            Z3_ast neg_one = Z3_mk_numeral(ctx, "-1", sort);
            Z3_ast args[2] = {neg_one, operand};
            return Z3_mk_mul(ctx, 2, args);
        }
    }

    // Binary arithmetic sub-expression (e.g., $ % 2, $ * 3, $ + 1)
    if (node->type == ASTNode::NodeType::BINARY_OP) {
        auto* binary = static_cast<BinaryExpr*>(node);
        Z3_ast left = translateOperand(binary->left.get(), dollar, sort);
        Z3_ast right = translateOperand(binary->right.get(), dollar, sort);
        if (!left || !right) return nullptr;

        Z3_sort_kind sk = Z3_get_sort_kind(ctx, sort);

        if (sk == Z3_BV_SORT) {
            // Bitvector arithmetic (signed)
            switch (binary->op.type) {
                case frontend::TokenType::TOKEN_PLUS:    return Z3_mk_bvadd(ctx, left, right);
                case frontend::TokenType::TOKEN_MINUS:   return Z3_mk_bvsub(ctx, left, right);
                case frontend::TokenType::TOKEN_STAR:    return Z3_mk_bvmul(ctx, left, right);
                case frontend::TokenType::TOKEN_SLASH:   return Z3_mk_bvsdiv(ctx, left, right);
                case frontend::TokenType::TOKEN_PERCENT: return Z3_mk_bvsrem(ctx, left, right);
                default: break;
            }
        } else {
            // Real/integer arithmetic
            Z3_ast args[2] = {left, right};
            switch (binary->op.type) {
                case frontend::TokenType::TOKEN_PLUS:    return Z3_mk_add(ctx, 2, args);
                case frontend::TokenType::TOKEN_MINUS:   return Z3_mk_sub(ctx, 2, args);
                case frontend::TokenType::TOKEN_STAR:    return Z3_mk_mul(ctx, 2, args);
                case frontend::TokenType::TOKEN_SLASH:   return Z3_mk_div(ctx, left, right);
                case frontend::TokenType::TOKEN_PERCENT: {
                    // Modulo for reals: a % b = a - b * (a / b)
                    // Use integer division semantics
                    Z3_ast div = Z3_mk_div(ctx, left, right);
                    Z3_ast mul_args[2] = {right, div};
                    Z3_ast prod = Z3_mk_mul(ctx, 2, mul_args);
                    Z3_ast sub_args[2] = {left, prod};
                    return Z3_mk_sub(ctx, 2, sub_args);
                }
                default: break;
            }
        }
    }

    return nullptr; // Unsupported operand
}

Z3_ast Z3Verifier::translateCondition(ASTNode* node, Z3_ast dollar, Z3_sort sort) {
    if (!node) return nullptr;

    // Binary comparison: the top-level condition
    if (node->type == ASTNode::NodeType::BINARY_OP) {
        auto* binary = static_cast<BinaryExpr*>(node);
        Z3_ast left = translateOperand(binary->left.get(), dollar, sort);
        Z3_ast right = translateOperand(binary->right.get(), dollar, sort);
        if (!left || !right) return nullptr;

        Z3_sort_kind sk = Z3_get_sort_kind(ctx, sort);

        if (sk == Z3_BV_SORT) {
            // Bitvector comparisons (signed)
            switch (binary->op.type) {
                case frontend::TokenType::TOKEN_LESS:          return Z3_mk_bvslt(ctx, left, right);
                case frontend::TokenType::TOKEN_LESS_EQUAL:    return Z3_mk_bvsle(ctx, left, right);
                case frontend::TokenType::TOKEN_GREATER:       return Z3_mk_bvsgt(ctx, left, right);
                case frontend::TokenType::TOKEN_GREATER_EQUAL: return Z3_mk_bvsge(ctx, left, right);
                case frontend::TokenType::TOKEN_EQUAL_EQUAL:   return Z3_mk_eq(ctx, left, right);
                case frontend::TokenType::TOKEN_BANG_EQUAL: {
                    Z3_ast eq = Z3_mk_eq(ctx, left, right);
                    return Z3_mk_not(ctx, eq);
                }
                default: break;
            }
        } else {
            // Real/integer comparisons
            switch (binary->op.type) {
                case frontend::TokenType::TOKEN_LESS:          return Z3_mk_lt(ctx, left, right);
                case frontend::TokenType::TOKEN_LESS_EQUAL:    return Z3_mk_le(ctx, left, right);
                case frontend::TokenType::TOKEN_GREATER:       return Z3_mk_gt(ctx, left, right);
                case frontend::TokenType::TOKEN_GREATER_EQUAL: return Z3_mk_ge(ctx, left, right);
                case frontend::TokenType::TOKEN_EQUAL_EQUAL:   return Z3_mk_eq(ctx, left, right);
                case frontend::TokenType::TOKEN_BANG_EQUAL: {
                    Z3_ast eq = Z3_mk_eq(ctx, left, right);
                    return Z3_mk_not(ctx, eq);
                }
                default: break;
            }
        }
    }

    return nullptr; // Unsupported condition shape
}

// ============================================================================
// Verification methods
// ============================================================================

VerifyResult Z3Verifier::verifyLimitInt(
    const std::string& rulesName, int64_t value,
    std::vector<VerifyOutcome>& outcomes,
    int line, int column)
{
    auto it = rules_table.find(rulesName);
    if (it == rules_table.end()) return VerifyResult::UNKNOWN;

    RulesDeclStmt* rules = it->second;
    VerifyResult worstResult = VerifyResult::PROVEN;

    // Process cascaded rules first
    for (const auto& cascadedName : rules->cascadedRules) {
        VerifyResult r = verifyLimitInt(cascadedName, value, outcomes, line, column);
        if (r == VerifyResult::DISPROVEN) return VerifyResult::DISPROVEN;
        if (r == VerifyResult::UNKNOWN) worstResult = VerifyResult::UNKNOWN;
    }

    // Determine bit width from type params (default 32)
    int bitWidth = 32;
    if (!rules->typeParams.empty()) {
        const std::string& tp = rules->typeParams[0];
        if (tp == "int8" || tp == "uint8") bitWidth = 8;
        else if (tp == "int16" || tp == "uint16") bitWidth = 16;
        else if (tp == "int32" || tp == "uint32") bitWidth = 32;
        else if (tp == "int64" || tp == "uint64") bitWidth = 64;
    }

    Z3_sort bv_sort = getIntSort(bitWidth);
    
    // Create $ as a concrete value
    Z3_ast dollar = Z3_mk_numeral(ctx, std::to_string(value).c_str(), bv_sort);

    // Verify each condition
    for (size_t i = 0; i < rules->conditions.size(); ++i) {
        Z3_ast z3cond = translateCondition(rules->conditions[i].get(), dollar, bv_sort);
        if (!z3cond) {
            // Can't translate this condition — mark as unknown
            VerifyOutcome out;
            out.result = VerifyResult::UNKNOWN;
            out.rulesName = rulesName;
            out.conditionText = rules->conditions[i]->toString();
            out.detail = "Could not encode condition for SMT verification";
            out.line = line;
            out.column = column;
            outcomes.push_back(out);
            summary.unknown++;
            if (worstResult == VerifyResult::PROVEN) worstResult = VerifyResult::UNKNOWN;
            continue;
        }

        // To prove the condition holds: assert NOT(condition) and check
        Z3_solver solver = makeSolver();
        Z3_ast negated = Z3_mk_not(ctx, z3cond);
        Z3_solver_assert(ctx, solver, negated);

        Z3_lbool result = checkSat(solver);
        
        VerifyOutcome out;
        out.rulesName = rulesName;
        out.conditionText = rules->conditions[i]->toString();
        out.line = line;
        out.column = column;

        if (result == Z3_L_FALSE) {
            // NOT(condition) is UNSATISFIABLE → condition always holds
            out.result = VerifyResult::PROVEN;
            out.detail = "Proven: value " + std::to_string(value) + " satisfies condition";
            summary.proven++;
        } else if (result == Z3_L_TRUE) {
            // NOT(condition) is SATISFIABLE → condition can fail
            out.result = VerifyResult::DISPROVEN;
            out.detail = "Violation: value " + std::to_string(value) + " fails condition";
            summary.disproven++;
            worstResult = VerifyResult::DISPROVEN;
        } else {
            out.result = VerifyResult::UNKNOWN;
            out.detail = "Solver returned unknown";
            summary.unknown++;
            if (worstResult == VerifyResult::PROVEN) worstResult = VerifyResult::UNKNOWN;
        }

        outcomes.push_back(out);
        deleteSolver(solver);
    }

    return worstResult;
}

VerifyResult Z3Verifier::verifyLimitFloat(
    const std::string& rulesName, double value,
    std::vector<VerifyOutcome>& outcomes,
    int line, int column)
{
    auto it = rules_table.find(rulesName);
    if (it == rules_table.end()) return VerifyResult::UNKNOWN;

    RulesDeclStmt* rules = it->second;
    VerifyResult worstResult = VerifyResult::PROVEN;

    // Process cascaded rules
    for (const auto& cascadedName : rules->cascadedRules) {
        VerifyResult r = verifyLimitFloat(cascadedName, value, outcomes, line, column);
        if (r == VerifyResult::DISPROVEN) return VerifyResult::DISPROVEN;
        if (r == VerifyResult::UNKNOWN) worstResult = VerifyResult::UNKNOWN;
    }

    Z3_sort real_sort = getRealSort();
    Z3_ast dollar = Z3_mk_numeral(ctx, std::to_string(value).c_str(), real_sort);

    for (size_t i = 0; i < rules->conditions.size(); ++i) {
        Z3_ast z3cond = translateCondition(rules->conditions[i].get(), dollar, real_sort);
        if (!z3cond) {
            VerifyOutcome out;
            out.result = VerifyResult::UNKNOWN;
            out.rulesName = rulesName;
            out.conditionText = rules->conditions[i]->toString();
            out.detail = "Could not encode float condition for SMT verification";
            out.line = line;
            out.column = column;
            outcomes.push_back(out);
            summary.unknown++;
            if (worstResult == VerifyResult::PROVEN) worstResult = VerifyResult::UNKNOWN;
            continue;
        }

        Z3_solver solver = makeSolver();
        Z3_ast negated = Z3_mk_not(ctx, z3cond);
        Z3_solver_assert(ctx, solver, negated);

        Z3_lbool result = checkSat(solver);

        VerifyOutcome out;
        out.rulesName = rulesName;
        out.conditionText = rules->conditions[i]->toString();
        out.line = line;
        out.column = column;

        if (result == Z3_L_FALSE) {
            out.result = VerifyResult::PROVEN;
            out.detail = "Proven: value " + std::to_string(value) + " satisfies condition";
            summary.proven++;
        } else if (result == Z3_L_TRUE) {
            out.result = VerifyResult::DISPROVEN;
            out.detail = "Violation: value " + std::to_string(value) + " fails condition";
            summary.disproven++;
            worstResult = VerifyResult::DISPROVEN;
        } else {
            out.result = VerifyResult::UNKNOWN;
            out.detail = "Solver returned unknown";
            summary.unknown++;
            if (worstResult == VerifyResult::PROVEN) worstResult = VerifyResult::UNKNOWN;
        }

        outcomes.push_back(out);
        deleteSolver(solver);
    }

    return worstResult;
}

VerifyResult Z3Verifier::verifyRulesConsistency(
    const std::string& rulesName,
    std::vector<VerifyOutcome>& outcomes)
{
    auto it = rules_table.find(rulesName);
    if (it == rules_table.end()) return VerifyResult::UNKNOWN;

    RulesDeclStmt* rules = it->second;

    // Determine sort
    int bitWidth = 32;
    bool useReals = false;
    if (!rules->typeParams.empty()) {
        const std::string& tp = rules->typeParams[0];
        if (tp == "flt32" || tp == "flt64") useReals = true;
        else if (tp == "int8" || tp == "uint8") bitWidth = 8;
        else if (tp == "int16" || tp == "uint16") bitWidth = 16;
        else if (tp == "int64" || tp == "uint64") bitWidth = 64;
    }

    Z3_sort sort = useReals ? getRealSort() : getIntSort(bitWidth);

    // Create symbolic $ variable
    Z3_symbol sym = Z3_mk_string_symbol(ctx, "dollar");
    Z3_ast dollar = Z3_mk_const(ctx, sym, sort);

    Z3_solver solver = makeSolver();

    // Assert ALL conditions simultaneously, including cascaded
    std::vector<RulesDeclStmt*> all_rules;
    std::vector<std::string> queue = {rulesName};
    while (!queue.empty()) {
        std::string name = queue.back();
        queue.pop_back();
        auto rit = rules_table.find(name);
        if (rit == rules_table.end()) continue;
        all_rules.push_back(rit->second);
        for (const auto& cascaded : rit->second->cascadedRules) {
            queue.push_back(cascaded);
        }
    }

    for (auto* r : all_rules) {
        for (size_t i = 0; i < r->conditions.size(); ++i) {
            Z3_ast z3cond = translateCondition(r->conditions[i].get(), dollar, sort);
            if (z3cond) {
                Z3_solver_assert(ctx, solver, z3cond);
            }
        }
    }

    // Check if the combined constraints are satisfiable
    Z3_lbool result = checkSat(solver);

    VerifyOutcome out;
    out.rulesName = rulesName;
    out.conditionText = "(all conditions combined)";

    if (result == Z3_L_FALSE) {
        // UNSAT — the constraints are self-contradictory!
        out.result = VerifyResult::DISPROVEN;
        out.detail = "Rules '" + rulesName + "' constraints are contradictory — no value can satisfy all conditions";
        summary.disproven++;
        outcomes.push_back(out);
        deleteSolver(solver);
        return VerifyResult::DISPROVEN;
    } else if (result == Z3_L_TRUE) {
        out.result = VerifyResult::PROVEN;
        out.detail = "Rules '" + rulesName + "' constraints are consistent (satisfiable)";
        summary.proven++;
        outcomes.push_back(out);
        deleteSolver(solver);
        return VerifyResult::PROVEN;
    } else {
        out.result = VerifyResult::UNKNOWN;
        out.detail = "Could not determine consistency";
        summary.unknown++;
        outcomes.push_back(out);
        deleteSolver(solver);
        return VerifyResult::UNKNOWN;
    }
}

VerifyResult Z3Verifier::verifyLimitRange(
    const std::string& rulesName,
    int64_t lo, int64_t hi,
    std::vector<VerifyOutcome>& outcomes,
    int line, int column)
{
    auto it = rules_table.find(rulesName);
    if (it == rules_table.end()) return VerifyResult::UNKNOWN;

    RulesDeclStmt* rules = it->second;
    VerifyResult worstResult = VerifyResult::PROVEN;

    // Process cascaded rules
    for (const auto& cascadedName : rules->cascadedRules) {
        VerifyResult r = verifyLimitRange(cascadedName, lo, hi, outcomes, line, column);
        if (r == VerifyResult::DISPROVEN) return VerifyResult::DISPROVEN;
        if (r == VerifyResult::UNKNOWN) worstResult = VerifyResult::UNKNOWN;
    }

    int bitWidth = 32;
    if (!rules->typeParams.empty()) {
        const std::string& tp = rules->typeParams[0];
        if (tp == "int8" || tp == "uint8") bitWidth = 8;
        else if (tp == "int16" || tp == "uint16") bitWidth = 16;
        else if (tp == "int64" || tp == "uint64") bitWidth = 64;
    }

    Z3_sort bv_sort = getIntSort(bitWidth);

    // Create symbolic $ variable
    Z3_symbol sym = Z3_mk_string_symbol(ctx, "dollar");
    Z3_ast dollar = Z3_mk_const(ctx, sym, bv_sort);

    // Range constraints: lo <= $ <= hi
    Z3_ast lo_val = Z3_mk_numeral(ctx, std::to_string(lo).c_str(), bv_sort);
    Z3_ast hi_val = Z3_mk_numeral(ctx, std::to_string(hi).c_str(), bv_sort);
    Z3_ast range_lo = Z3_mk_bvsle(ctx, lo_val, dollar);
    Z3_ast range_hi = Z3_mk_bvsle(ctx, dollar, hi_val);

    for (size_t i = 0; i < rules->conditions.size(); ++i) {
        Z3_ast z3cond = translateCondition(rules->conditions[i].get(), dollar, bv_sort);
        if (!z3cond) {
            VerifyOutcome out;
            out.result = VerifyResult::UNKNOWN;
            out.rulesName = rulesName;
            out.conditionText = rules->conditions[i]->toString();
            out.detail = "Could not encode condition for range proof";
            out.line = line;
            out.column = column;
            outcomes.push_back(out);
            summary.unknown++;
            if (worstResult == VerifyResult::PROVEN) worstResult = VerifyResult::UNKNOWN;
            continue;
        }

        // Assert: $ in [lo, hi] AND NOT(condition)
        // If UNSAT → condition holds for all values in range
        Z3_solver solver = makeSolver();
        Z3_solver_assert(ctx, solver, range_lo);
        Z3_solver_assert(ctx, solver, range_hi);
        Z3_ast negated = Z3_mk_not(ctx, z3cond);
        Z3_solver_assert(ctx, solver, negated);

        Z3_lbool result = checkSat(solver);

        VerifyOutcome out;
        out.rulesName = rulesName;
        out.conditionText = rules->conditions[i]->toString();
        out.line = line;
        out.column = column;

        if (result == Z3_L_FALSE) {
            out.result = VerifyResult::PROVEN;
            out.detail = "Proven: condition holds for all values in [" +
                         std::to_string(lo) + ", " + std::to_string(hi) + "]";
            summary.proven++;
        } else if (result == Z3_L_TRUE) {
            // Get counterexample
            Z3_model model = Z3_solver_get_model(ctx, solver);
            std::string counterexample;
            if (model) {
                Z3_model_inc_ref(ctx, model);
                Z3_ast val;
                if (Z3_model_eval(ctx, model, dollar, true, &val)) {
                    counterexample = Z3_ast_to_string(ctx, val);
                }
                Z3_model_dec_ref(ctx, model);
            }
            out.result = VerifyResult::DISPROVEN;
            out.detail = "Counterexample found in [" + std::to_string(lo) + ", " +
                         std::to_string(hi) + "]";
            if (!counterexample.empty()) {
                out.detail += ": $ = " + counterexample;
            }
            summary.disproven++;
            worstResult = VerifyResult::DISPROVEN;
        } else {
            out.result = VerifyResult::UNKNOWN;
            out.detail = "Solver returned unknown for range proof";
            summary.unknown++;
            if (worstResult == VerifyResult::PROVEN) worstResult = VerifyResult::UNKNOWN;
        }

        outcomes.push_back(out);
        deleteSolver(solver);
    }

    return worstResult;
}

} // namespace aria
