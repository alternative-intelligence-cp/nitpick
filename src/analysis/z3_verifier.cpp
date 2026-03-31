// ============================================================================
// Z3Verifier — SMT-based static constraint verification for Aria
// v0.2.45: Phase 1 — Rules/limit verification with Z3
// v0.3.4:  Phase 2 — requires/ensures contract verification
//          Phase 3 — Arithmetic overflow verification
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

// ============================================================================
// Helper: resolve Aria type name to Z3 sort
// ============================================================================

Z3Verifier::TypeInfo Z3Verifier::resolveTypeSort(const std::string& typeName) {
    TypeInfo info{};
    info.isFloat = false;
    info.bitWidth = 32;

    if (typeName == "int8" || typeName == "uint8") {
        info.bitWidth = 8;
    } else if (typeName == "int16" || typeName == "uint16") {
        info.bitWidth = 16;
    } else if (typeName == "int32" || typeName == "uint32") {
        info.bitWidth = 32;
    } else if (typeName == "int64" || typeName == "uint64") {
        info.bitWidth = 64;
    } else if (typeName == "flt32" || typeName == "flt64") {
        info.isFloat = true;
        info.bitWidth = (typeName == "flt64") ? 64 : 32;
    }

    info.sort = info.isFloat ? getRealSort() : getIntSort(info.bitWidth);
    return info;
}

// ============================================================================
// Translate expressions using a named variable environment
// (for contract verification: requires/ensures use parameter names, not $)
// ============================================================================

Z3_ast Z3Verifier::translateExprWithEnv(
    ASTNode* node,
    const std::map<std::string, Z3_ast>& env,
    Z3_sort defaultSort)
{
    if (!node) return nullptr;

    // Identifier → look up in environment
    if (node->type == ASTNode::NodeType::IDENTIFIER) {
        auto* ident = static_cast<IdentifierExpr*>(node);
        auto it = env.find(ident->name);
        if (it != env.end()) {
            return it->second;
        }
        // NIL → 0
        if (ident->name == "NIL") {
            return Z3_mk_numeral(ctx, "0", defaultSort);
        }
        // $result → look up special name
        if (ident->name == "$result") {
            auto it2 = env.find("$result");
            if (it2 != env.end()) return it2->second;
        }
        return nullptr; // unknown identifier
    }

    // Literal
    if (node->type == ASTNode::NodeType::LITERAL) {
        auto* literal = static_cast<LiteralExpr*>(node);
        if (std::holds_alternative<int64_t>(literal->value)) {
            int64_t val = std::get<int64_t>(literal->value);
            return Z3_mk_numeral(ctx, std::to_string(val).c_str(), defaultSort);
        }
        if (std::holds_alternative<double>(literal->value)) {
            double val = std::get<double>(literal->value);
            return Z3_mk_numeral(ctx, std::to_string(val).c_str(), defaultSort);
        }
        // Boolean literal
        if (std::holds_alternative<bool>(literal->value)) {
            bool val = std::get<bool>(literal->value);
            return val ? Z3_mk_true(ctx) : Z3_mk_false(ctx);
        }
    }

    // Unary minus
    if (node->type == ASTNode::NodeType::UNARY_OP) {
        auto* unary = static_cast<UnaryExpr*>(node);
        if (unary->op.type == frontend::TokenType::TOKEN_MINUS) {
            Z3_ast operand = translateExprWithEnv(unary->operand.get(), env, defaultSort);
            if (!operand) return nullptr;
            Z3_sort_kind sk = Z3_get_sort_kind(ctx, defaultSort);
            if (sk == Z3_BV_SORT) {
                return Z3_mk_bvneg(ctx, operand);
            }
            Z3_ast neg_one = Z3_mk_numeral(ctx, "-1", defaultSort);
            Z3_ast args[2] = {neg_one, operand};
            return Z3_mk_mul(ctx, 2, args);
        }
        // Logical NOT
        if (unary->op.type == frontend::TokenType::TOKEN_BANG) {
            Z3_ast operand = translateExprWithEnv(unary->operand.get(), env, defaultSort);
            if (!operand) return nullptr;
            return Z3_mk_not(ctx, operand);
        }
    }

    // Binary expression (arithmetic or comparison)
    if (node->type == ASTNode::NodeType::BINARY_OP) {
        auto* binary = static_cast<BinaryExpr*>(node);
        Z3_ast left = translateExprWithEnv(binary->left.get(), env, defaultSort);
        Z3_ast right = translateExprWithEnv(binary->right.get(), env, defaultSort);
        if (!left || !right) return nullptr;

        Z3_sort_kind sk = Z3_get_sort_kind(ctx, defaultSort);

        // Logical operators
        if (binary->op.type == frontend::TokenType::TOKEN_AND_AND) {
            Z3_ast args[2] = {left, right};
            return Z3_mk_and(ctx, 2, args);
        }
        if (binary->op.type == frontend::TokenType::TOKEN_OR_OR) {
            Z3_ast args[2] = {left, right};
            return Z3_mk_or(ctx, 2, args);
        }

        if (sk == Z3_BV_SORT) {
            // Bitvector arithmetic
            switch (binary->op.type) {
                case frontend::TokenType::TOKEN_PLUS:    return Z3_mk_bvadd(ctx, left, right);
                case frontend::TokenType::TOKEN_MINUS:   return Z3_mk_bvsub(ctx, left, right);
                case frontend::TokenType::TOKEN_STAR:    return Z3_mk_bvmul(ctx, left, right);
                case frontend::TokenType::TOKEN_SLASH:   return Z3_mk_bvsdiv(ctx, left, right);
                case frontend::TokenType::TOKEN_PERCENT: return Z3_mk_bvsrem(ctx, left, right);
                // Bitvector comparisons (signed)
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
            // Real/integer arithmetic
            Z3_ast args[2] = {left, right};
            switch (binary->op.type) {
                case frontend::TokenType::TOKEN_PLUS:    return Z3_mk_add(ctx, 2, args);
                case frontend::TokenType::TOKEN_MINUS:   return Z3_mk_sub(ctx, 2, args);
                case frontend::TokenType::TOKEN_STAR:    return Z3_mk_mul(ctx, 2, args);
                case frontend::TokenType::TOKEN_SLASH:   return Z3_mk_div(ctx, left, right);
                case frontend::TokenType::TOKEN_PERCENT: {
                    Z3_ast div = Z3_mk_div(ctx, left, right);
                    Z3_ast mul_args[2] = {right, div};
                    Z3_ast prod = Z3_mk_mul(ctx, 2, mul_args);
                    Z3_ast sub_args[2] = {left, prod};
                    return Z3_mk_sub(ctx, 2, sub_args);
                }
                // Real comparisons
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

    return nullptr; // Unsupported expression
}

// ============================================================================
// Phase 2: Design-by-Contract — requires/ensures Verification
// ============================================================================

VerifyResult Z3Verifier::verifyFunctionContract(
    FuncDeclStmt* func,
    std::vector<VerifyOutcome>& outcomes)
{
    if (!func) return VerifyResult::UNKNOWN;
    if (func->preconditions.empty() && func->postconditions.empty()) {
        return VerifyResult::PROVEN; // No contracts = trivially satisfied
    }

    VerifyResult worstResult = VerifyResult::PROVEN;

    // Build variable environment: parameter names → symbolic Z3 variables
    std::map<std::string, Z3_ast> env;
    Z3_sort defaultSort = getIntSort(32); // fallback sort

    for (size_t i = 0; i < func->parameters.size(); ++i) {
        auto* param = static_cast<ParameterNode*>(func->parameters[i].get());
        if (!param) continue;

        // Resolve parameter type to Z3 sort
        std::string tname = param->typeNode ? param->typeNode->toString() : "int32";
        TypeInfo ti = resolveTypeSort(tname);

        Z3_symbol sym = Z3_mk_string_symbol(ctx, param->paramName.c_str());
        Z3_ast var = Z3_mk_const(ctx, sym, ti.sort);
        env[param->paramName] = var;

        // Use first parameter's sort as the default
        if (i == 0) defaultSort = ti.sort;
    }

    // 1. Verify requires clauses are consistent (not self-contradictory)
    if (!func->preconditions.empty()) {
        Z3_solver solver = makeSolver();
        bool allTranslated = true;

        for (size_t i = 0; i < func->preconditions.size(); ++i) {
            Z3_ast z3pre = translateExprWithEnv(func->preconditions[i].get(), env, defaultSort);
            if (!z3pre) {
                allTranslated = false;
                VerifyOutcome out;
                out.result = VerifyResult::UNKNOWN;
                out.rulesName = func->funcName;
                out.conditionText = func->preconditions[i]->toString();
                out.detail = "Could not encode requires clause for SMT verification";
                out.line = func->line;
                out.column = func->column;
                outcomes.push_back(out);
                summary.unknown++;
                if (worstResult == VerifyResult::PROVEN) worstResult = VerifyResult::UNKNOWN;
                continue;
            }
            Z3_solver_assert(ctx, solver, z3pre);
        }

        if (allTranslated) {
            Z3_lbool result = checkSat(solver);
            VerifyOutcome out;
            out.rulesName = func->funcName;
            out.conditionText = "(all requires clauses)";
            out.line = func->line;
            out.column = func->column;

            if (result == Z3_L_FALSE) {
                out.result = VerifyResult::DISPROVEN;
                out.detail = "Contract error: requires clauses on '" + func->funcName +
                             "' are contradictory — no arguments can satisfy them";
                summary.disproven++;
                outcomes.push_back(out);
                deleteSolver(solver);
                return VerifyResult::DISPROVEN;
            } else if (result == Z3_L_TRUE) {
                out.result = VerifyResult::PROVEN;
                out.detail = "Requires clauses on '" + func->funcName + "' are consistent";
                summary.proven++;
                outcomes.push_back(out);
            } else {
                out.result = VerifyResult::UNKNOWN;
                out.detail = "Could not determine consistency of requires clauses";
                summary.unknown++;
                if (worstResult == VerifyResult::PROVEN) worstResult = VerifyResult::UNKNOWN;
                outcomes.push_back(out);
            }
        }

        deleteSolver(solver);
    }

    // 2. If function has ensures clauses, attempt to prove them
    //    under the assumption that requires hold.
    //    For now: check that ensures are satisfiable given requires
    //    (full body analysis would require symbolic execution — future work)
    if (!func->postconditions.empty()) {
        // Add $result as a symbolic variable
        Z3_symbol resSym = Z3_mk_string_symbol(ctx, "$result");
        Z3_ast resultVar = Z3_mk_const(ctx, resSym, defaultSort);
        env["$result"] = resultVar;

        for (size_t i = 0; i < func->postconditions.size(); ++i) {
            Z3_solver solver = makeSolver();

            // Assert all requires as assumptions
            for (auto& pre : func->preconditions) {
                Z3_ast z3pre = translateExprWithEnv(pre.get(), env, defaultSort);
                if (z3pre) Z3_solver_assert(ctx, solver, z3pre);
            }

            Z3_ast z3post = translateExprWithEnv(func->postconditions[i].get(), env, defaultSort);
            if (!z3post) {
                VerifyOutcome out;
                out.result = VerifyResult::UNKNOWN;
                out.rulesName = func->funcName;
                out.conditionText = func->postconditions[i]->toString();
                out.detail = "Could not encode ensures clause for SMT verification";
                out.line = func->line;
                out.column = func->column;
                outcomes.push_back(out);
                summary.unknown++;
                if (worstResult == VerifyResult::PROVEN) worstResult = VerifyResult::UNKNOWN;
                deleteSolver(solver);
                continue;
            }

            // Check if ensures is satisfiable given requires (sanity check)
            // Assert NOT(ensures) — if UNSAT, ensures is always true given requires
            Z3_ast negPost = Z3_mk_not(ctx, z3post);
            Z3_solver_assert(ctx, solver, negPost);

            Z3_lbool result = checkSat(solver);
            VerifyOutcome out;
            out.rulesName = func->funcName;
            out.conditionText = func->postconditions[i]->toString();
            out.line = func->line;
            out.column = func->column;

            if (result == Z3_L_FALSE) {
                // NOT(ensures) is UNSAT given requires → ensures always holds
                out.result = VerifyResult::PROVEN;
                out.detail = "Proven: ensures clause '" + out.conditionText +
                             "' holds for all valid inputs to '" + func->funcName + "'";
                summary.proven++;
            } else if (result == Z3_L_TRUE) {
                // NOT(ensures) is SAT → ensures doesn't always hold
                // This is expected for most ensures (they constrain $result)
                // Report as "checked" not "disproven" since we lack body analysis
                out.result = VerifyResult::UNKNOWN;
                out.detail = "Ensures clause '" + out.conditionText +
                             "' on '" + func->funcName +
                             "' cannot be statically proven without body analysis (kept as runtime check)";
                summary.unknown++;
                if (worstResult == VerifyResult::PROVEN) worstResult = VerifyResult::UNKNOWN;
            } else {
                out.result = VerifyResult::UNKNOWN;
                out.detail = "Solver returned unknown for ensures clause";
                summary.unknown++;
                if (worstResult == VerifyResult::PROVEN) worstResult = VerifyResult::UNKNOWN;
            }

            outcomes.push_back(out);
            deleteSolver(solver);
        }
    }

    return worstResult;
}

VerifyResult Z3Verifier::verifyCallSiteRequires(
    FuncDeclStmt* callee,
    const std::vector<ASTNode*>& args,
    std::vector<VerifyOutcome>& outcomes,
    int line, int column)
{
    if (!callee || callee->preconditions.empty()) {
        return VerifyResult::PROVEN; // No requires = trivially satisfied
    }

    VerifyResult worstResult = VerifyResult::PROVEN;

    // Build environment mapping parameter names to argument Z3 expressions
    std::map<std::string, Z3_ast> env;
    Z3_sort defaultSort = getIntSort(32);

    for (size_t i = 0; i < callee->parameters.size() && i < args.size(); ++i) {
        auto* param = static_cast<ParameterNode*>(callee->parameters[i].get());
        if (!param) continue;

        std::string tname = param->typeNode ? param->typeNode->toString() : "int32";
        TypeInfo ti = resolveTypeSort(tname);
        if (i == 0) defaultSort = ti.sort;

        // Try to extract a concrete value from the argument
        ASTNode* arg = args[i];
        if (!arg) continue;

        if (arg->type == ASTNode::NodeType::LITERAL) {
            auto* lit = static_cast<LiteralExpr*>(arg);
            if (std::holds_alternative<int64_t>(lit->value)) {
                int64_t val = std::get<int64_t>(lit->value);
                env[param->paramName] = Z3_mk_numeral(ctx, std::to_string(val).c_str(), ti.sort);
            } else if (std::holds_alternative<double>(lit->value)) {
                double val = std::get<double>(lit->value);
                env[param->paramName] = Z3_mk_numeral(ctx, std::to_string(val).c_str(), ti.sort);
            }
        } else if (arg->type == ASTNode::NodeType::UNARY_OP) {
            // Negative literal
            auto* unary = static_cast<UnaryExpr*>(arg);
            if (unary->op.type == frontend::TokenType::TOKEN_MINUS &&
                unary->operand && unary->operand->type == ASTNode::NodeType::LITERAL) {
                auto* lit = static_cast<LiteralExpr*>(unary->operand.get());
                if (std::holds_alternative<int64_t>(lit->value)) {
                    int64_t val = -std::get<int64_t>(lit->value);
                    env[param->paramName] = Z3_mk_numeral(ctx, std::to_string(val).c_str(), ti.sort);
                }
            }
        } else {
            // Non-literal argument: create symbolic variable
            Z3_symbol sym = Z3_mk_string_symbol(ctx, ("arg_" + std::to_string(i)).c_str());
            env[param->paramName] = Z3_mk_const(ctx, sym, ti.sort);
        }
    }

    // Verify each requires clause
    for (size_t i = 0; i < callee->preconditions.size(); ++i) {
        Z3_ast z3pre = translateExprWithEnv(callee->preconditions[i].get(), env, defaultSort);
        if (!z3pre) {
            VerifyOutcome out;
            out.result = VerifyResult::UNKNOWN;
            out.rulesName = callee->funcName;
            out.conditionText = callee->preconditions[i]->toString();
            out.detail = "Could not encode requires clause at call site";
            out.line = line;
            out.column = column;
            outcomes.push_back(out);
            summary.unknown++;
            if (worstResult == VerifyResult::PROVEN) worstResult = VerifyResult::UNKNOWN;
            continue;
        }

        // Assert NOT(requires) — if UNSAT, the call satisfies the precondition
        Z3_solver solver = makeSolver();
        Z3_ast negated = Z3_mk_not(ctx, z3pre);
        Z3_solver_assert(ctx, solver, negated);

        Z3_lbool result = checkSat(solver);
        VerifyOutcome out;
        out.rulesName = callee->funcName;
        out.conditionText = callee->preconditions[i]->toString();
        out.line = line;
        out.column = column;

        if (result == Z3_L_FALSE) {
            out.result = VerifyResult::PROVEN;
            out.detail = "Proven: call to '" + callee->funcName +
                         "' satisfies requires clause '" + out.conditionText + "'";
            summary.proven++;
        } else if (result == Z3_L_TRUE) {
            out.result = VerifyResult::DISPROVEN;
            // Get counterexample
            Z3_model model = Z3_solver_get_model(ctx, solver);
            std::string counterexample;
            if (model) {
                Z3_model_inc_ref(ctx, model);
                counterexample = Z3_model_to_string(ctx, model);
                Z3_model_dec_ref(ctx, model);
            }
            out.detail = "Violation: call to '" + callee->funcName +
                         "' may violate requires clause '" + out.conditionText + "'";
            if (!counterexample.empty()) {
                out.detail += "\n  Counterexample: " + counterexample;
            }
            summary.disproven++;
            worstResult = VerifyResult::DISPROVEN;
        } else {
            out.result = VerifyResult::UNKNOWN;
            out.detail = "Could not determine if call satisfies requires clause";
            summary.unknown++;
            if (worstResult == VerifyResult::PROVEN) worstResult = VerifyResult::UNKNOWN;
        }

        outcomes.push_back(out);
        deleteSolver(solver);
    }

    return worstResult;
}

// ============================================================================
// Phase 3: Arithmetic Overflow Verification
// ============================================================================

VerifyResult Z3Verifier::verifyNoOverflow(
    char op, int bitWidth, bool isSigned,
    int64_t lhsLo, int64_t lhsHi,
    int64_t rhsLo, int64_t rhsHi,
    std::vector<VerifyOutcome>& outcomes,
    int line, int column)
{
    Z3_sort bv_sort = getIntSort(bitWidth);

    // Create symbolic operands
    Z3_symbol lhsSym = Z3_mk_string_symbol(ctx, "lhs");
    Z3_symbol rhsSym = Z3_mk_string_symbol(ctx, "rhs");
    Z3_ast lhs = Z3_mk_const(ctx, lhsSym, bv_sort);
    Z3_ast rhs = Z3_mk_const(ctx, rhsSym, bv_sort);

    // Range constraints
    Z3_ast lhsLoVal = Z3_mk_numeral(ctx, std::to_string(lhsLo).c_str(), bv_sort);
    Z3_ast lhsHiVal = Z3_mk_numeral(ctx, std::to_string(lhsHi).c_str(), bv_sort);
    Z3_ast rhsLoVal = Z3_mk_numeral(ctx, std::to_string(rhsLo).c_str(), bv_sort);
    Z3_ast rhsHiVal = Z3_mk_numeral(ctx, std::to_string(rhsHi).c_str(), bv_sort);

    Z3_ast lhsRange1 = Z3_mk_bvsle(ctx, lhsLoVal, lhs);
    Z3_ast lhsRange2 = Z3_mk_bvsle(ctx, lhs, lhsHiVal);
    Z3_ast rhsRange1 = Z3_mk_bvsle(ctx, rhsLoVal, rhs);
    Z3_ast rhsRange2 = Z3_mk_bvsle(ctx, rhs, rhsHiVal);

    // Build the overflow condition using Z3's native overflow detection
    Z3_ast overflowCond = nullptr;
    std::string opStr;

    switch (op) {
        case '+': {
            opStr = "addition";
            // Z3_mk_bvadd_no_overflow returns true when NO overflow occurs
            // We want to find if overflow IS possible, so negate it
            Z3_ast noOverflow = Z3_mk_bvadd_no_overflow(ctx, lhs, rhs, isSigned);
            Z3_ast noUnderflow = isSigned ?
                Z3_mk_bvadd_no_underflow(ctx, lhs, rhs) : Z3_mk_true(ctx);
            Z3_ast safe[2] = {noOverflow, noUnderflow};
            Z3_ast allSafe = Z3_mk_and(ctx, 2, safe);
            overflowCond = Z3_mk_not(ctx, allSafe);
            break;
        }
        case '-': {
            opStr = "subtraction";
            Z3_ast noOverflow = isSigned ?
                Z3_mk_bvsub_no_overflow(ctx, lhs, rhs) : Z3_mk_true(ctx);
            Z3_ast noUnderflow = Z3_mk_bvsub_no_underflow(ctx, lhs, rhs, isSigned);
            Z3_ast safe[2] = {noOverflow, noUnderflow};
            Z3_ast allSafe = Z3_mk_and(ctx, 2, safe);
            overflowCond = Z3_mk_not(ctx, allSafe);
            break;
        }
        case '*': {
            opStr = "multiplication";
            Z3_ast noOverflow = Z3_mk_bvmul_no_overflow(ctx, lhs, rhs, isSigned);
            Z3_ast noUnderflow = isSigned ?
                Z3_mk_bvmul_no_underflow(ctx, lhs, rhs) : Z3_mk_true(ctx);
            Z3_ast safe[2] = {noOverflow, noUnderflow};
            Z3_ast allSafe = Z3_mk_and(ctx, 2, safe);
            overflowCond = Z3_mk_not(ctx, allSafe);
            break;
        }
        default: {
            VerifyOutcome out;
            out.result = VerifyResult::UNKNOWN;
            out.conditionText = std::string(1, op) + " (unsupported op for overflow check)";
            out.detail = "Overflow verification only supports +, -, *";
            out.line = line;
            out.column = column;
            outcomes.push_back(out);
            summary.unknown++;
            return VerifyResult::UNKNOWN;
        }
    }

    // Assert: operands in range AND overflow occurs
    // If UNSAT → overflow impossible within these ranges
    Z3_solver solver = makeSolver();
    Z3_solver_assert(ctx, solver, lhsRange1);
    Z3_solver_assert(ctx, solver, lhsRange2);
    Z3_solver_assert(ctx, solver, rhsRange1);
    Z3_solver_assert(ctx, solver, rhsRange2);
    Z3_solver_assert(ctx, solver, overflowCond);

    Z3_lbool result = checkSat(solver);

    VerifyOutcome out;
    out.conditionText = opStr + " overflow check (int" + std::to_string(bitWidth) + ")";
    out.line = line;
    out.column = column;

    if (result == Z3_L_FALSE) {
        // UNSAT → no overflow possible
        out.result = VerifyResult::PROVEN;
        out.detail = "Proven: " + opStr + " cannot overflow for operands in [" +
                     std::to_string(lhsLo) + "," + std::to_string(lhsHi) + "] " +
                     std::string(1, op) + " [" +
                     std::to_string(rhsLo) + "," + std::to_string(rhsHi) + "]";
        summary.proven++;
    } else if (result == Z3_L_TRUE) {
        // SAT → overflow is possible
        Z3_model model = Z3_solver_get_model(ctx, solver);
        std::string counterexample;
        if (model) {
            Z3_model_inc_ref(ctx, model);
            Z3_ast lhsVal, rhsVal;
            if (Z3_model_eval(ctx, model, lhs, true, &lhsVal) &&
                Z3_model_eval(ctx, model, rhs, true, &rhsVal)) {
                counterexample = "lhs=" + std::string(Z3_ast_to_string(ctx, lhsVal)) +
                                 ", rhs=" + std::string(Z3_ast_to_string(ctx, rhsVal));
            }
            Z3_model_dec_ref(ctx, model);
        }
        out.result = VerifyResult::DISPROVEN;
        out.detail = "Overflow possible: " + opStr + " on int" + std::to_string(bitWidth);
        if (!counterexample.empty()) {
            out.detail += " (counterexample: " + counterexample + ")";
        }
        summary.disproven++;
    } else {
        out.result = VerifyResult::UNKNOWN;
        out.detail = "Solver returned unknown for overflow check";
        summary.unknown++;
    }

    VerifyResult vr = out.result;
    outcomes.push_back(out);
    deleteSolver(solver);
    return vr;
}

// ============================================================================
// Phase 4: User Stack Type Homogeneity Verification (v0.4.3+)
// ============================================================================
//
// Given a set of compile-time type tags from all apush() calls in a scope,
// prove that they are ALL identical to the expected pop tag.
//
// Encoding:  NOT(forall i: tag_i == expected)  =  exists i: tag_i != expected
// If UNSAT → no tag can differ → all match → PROVEN → eliminate runtime checks
//
VerifyResult Z3Verifier::verifyUStackHomogeneous(
    const std::vector<int64_t>& pushTypeTags,
    int64_t expectedPopTag,
    std::vector<VerifyOutcome>& outcomes,
    int line, int column)
{
    static const char* tag_names[] = {
        "int8", "int16", "int32", "int64",
        "flt32", "flt64", "bool", "string", "pointer"
    };

    if (pushTypeTags.empty()) {
        VerifyOutcome out;
        out.result = VerifyResult::UNKNOWN;
        out.conditionText = "ustack type homogeneity";
        out.detail = "no apush() calls found in scope — cannot verify";
        out.line = line;
        out.column = column;
        summary.unknown++;
        outcomes.push_back(out);
        return VerifyResult::UNKNOWN;
    }

    Z3_solver solver = makeSolver();
    Z3_sort bv64 = Z3_mk_bv_sort(ctx, 64);

    // Create the expected tag as a Z3 bitvector constant
    Z3_ast expected = Z3_mk_int64(ctx, expectedPopTag, bv64);

    // Build disjunction: (tag_0 != expected) OR (tag_1 != expected) OR ...
    // This is the negation of "all tags equal expected"
    std::vector<Z3_ast> disjuncts;
    for (int64_t tag : pushTypeTags) {
        Z3_ast tagAst = Z3_mk_int64(ctx, tag, bv64);
        Z3_ast neq = Z3_mk_not(ctx, Z3_mk_eq(ctx, tagAst, expected));
        disjuncts.push_back(neq);
    }

    Z3_ast anyDiffers;
    if (disjuncts.size() == 1) {
        anyDiffers = disjuncts[0];
    } else {
        anyDiffers = Z3_mk_or(ctx, static_cast<unsigned>(disjuncts.size()),
                               disjuncts.data());
    }

    Z3_solver_assert(ctx, solver, anyDiffers);
    Z3_lbool result = checkSat(solver);
    deleteSolver(solver);

    VerifyOutcome out;
    out.conditionText = "ustack type homogeneity";
    out.line = line;
    out.column = column;

    const char* expectedName = (expectedPopTag >= 0 && expectedPopTag <= 8)
        ? tag_names[expectedPopTag] : "unknown";

    if (result == Z3_L_FALSE) {
        // UNSAT → NOT(all equal) has no solution → all ARE equal → PROVEN
        out.result = VerifyResult::PROVEN;
        out.detail = std::string("all ") + std::to_string(pushTypeTags.size()) +
                     " apush() calls proven type-homogeneous (" + expectedName +
                     ") — runtime tag checks eliminated";
        summary.proven++;
    } else if (result == Z3_L_TRUE) {
        // SAT → at least one tag differs → cannot optimize
        out.result = VerifyResult::DISPROVEN;
        out.detail = "mixed types found among apush() calls — runtime tag checks retained";
        summary.disproven++;
    } else {
        out.result = VerifyResult::UNKNOWN;
        out.detail = "solver timeout on ustack homogeneity — runtime tag checks retained";
        summary.unknown++;
    }

    VerifyResult vr = out.result;
    outcomes.push_back(out);
    return vr;
}

} // namespace aria
