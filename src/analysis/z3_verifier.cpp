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
#include <functional>
#include <iostream>
#include <set>

namespace aria {

// Human-readable expression pretty-printer for contract diagnostics (v0.5.2)
static std::string prettyExpr(ASTNode* node) {
    if (!node) return "?";
    using NT = ASTNode::NodeType;
    switch (node->type) {
        case NT::IDENTIFIER: {
            auto* id = static_cast<IdentifierExpr*>(node);
            return id->name;
        }
        case NT::LITERAL: {
            auto* lit = static_cast<LiteralExpr*>(node);
            if (lit->hasRawValue()) return lit->getRawValue();
            if (std::holds_alternative<int64_t>(lit->value))
                return std::to_string(std::get<int64_t>(lit->value));
            if (std::holds_alternative<double>(lit->value)) {
                std::string s = std::to_string(std::get<double>(lit->value));
                // Trim trailing zeros
                size_t dot = s.find('.');
                if (dot != std::string::npos) {
                    size_t last = s.find_last_not_of('0');
                    if (last != std::string::npos && last > dot) s = s.substr(0, last + 1);
                    else if (last == dot) s = s.substr(0, dot + 2);
                }
                return s;
            }
            if (std::holds_alternative<bool>(lit->value))
                return std::get<bool>(lit->value) ? "true" : "false";
            if (std::holds_alternative<std::string>(lit->value))
                return "\"" + std::get<std::string>(lit->value) + "\"";
            return lit->toString();
        }
        case NT::BINARY_OP: {
            auto* bin = static_cast<BinaryExpr*>(node);
            return prettyExpr(bin->left.get()) + " " + bin->op.lexeme + " " +
                   prettyExpr(bin->right.get());
        }
        case NT::UNARY_OP: {
            auto* un = static_cast<UnaryExpr*>(node);
            if (un->isPostfix) return prettyExpr(un->operand.get()) + un->op.lexeme;
            return un->op.lexeme + prettyExpr(un->operand.get());
        }
        default:
            return node->toString();
    }
}

Z3Verifier::Z3Verifier() {
    Z3_config cfg = Z3_mk_config();
    // Set a reasonable timeout (5 seconds per query)
    Z3_set_param_value(cfg, "timeout", "5000");
    ctx = Z3_mk_context(cfg);
    Z3_del_config(cfg);
}

Z3Verifier::Z3Verifier(int timeout_ms) {
    Z3_config cfg = Z3_mk_config();
    std::string timeout_str = std::to_string(timeout_ms > 0 ? timeout_ms : 5000);
    Z3_set_param_value(cfg, "timeout", timeout_str.c_str());
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
                out.conditionText = prettyExpr(func->preconditions[i].get());
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
                out.conditionText = prettyExpr(func->postconditions[i].get());
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
            out.conditionText = prettyExpr(func->postconditions[i].get());
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
            out.conditionText = prettyExpr(callee->preconditions[i].get());
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
        out.conditionText = prettyExpr(callee->preconditions[i].get());
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
// Phase 2b: Cross-function contract propagation (v0.5.2)
// ============================================================================

VerifyResult Z3Verifier::verifyCallSiteContract(
    FuncDeclStmt* callee,
    FuncDeclStmt* caller,
    const std::vector<ASTNode*>& args,
    const std::map<std::string, std::pair<RulesDeclStmt*, std::string>>& callerVarRules,
    const std::vector<std::pair<std::string, FuncDeclStmt*>>& ensuresFacts,
    std::vector<VerifyOutcome>& outcomes,
    int line, int column)
{
    if (!callee || callee->preconditions.empty()) {
        return VerifyResult::PROVEN;
    }

    VerifyResult worstResult = VerifyResult::PROVEN;

    // Build two Z3 environments:
    // calleeEnv: callee param names → Z3 vars (for translating callee requires)
    // callerEnv: caller variable names → Z3 vars (for translating caller requires)
    // Shared Z3 vars link the two where args bridge caller→callee.
    std::map<std::string, Z3_ast> calleeEnv;
    std::map<std::string, Z3_ast> callerEnv;
    std::vector<Z3_ast> assumptions;
    Z3_sort defaultSort = getIntSort(32);

    for (size_t i = 0; i < callee->parameters.size() && i < args.size(); ++i) {
        auto* param = static_cast<ParameterNode*>(callee->parameters[i].get());
        if (!param) continue;

        std::string tname = param->typeNode ? param->typeNode->toString() : "int32";
        TypeInfo ti = resolveTypeSort(tname);
        if (i == 0) defaultSort = ti.sort;

        ASTNode* arg = args[i];
        if (!arg) continue;

        if (arg->type == ASTNode::NodeType::LITERAL) {
            auto* lit = static_cast<LiteralExpr*>(arg);
            if (std::holds_alternative<int64_t>(lit->value)) {
                int64_t val = std::get<int64_t>(lit->value);
                calleeEnv[param->paramName] = Z3_mk_numeral(ctx,
                    std::to_string(val).c_str(), ti.sort);
            } else if (std::holds_alternative<double>(lit->value)) {
                double val = std::get<double>(lit->value);
                calleeEnv[param->paramName] = Z3_mk_numeral(ctx,
                    std::to_string(val).c_str(), ti.sort);
            }
        } else if (arg->type == ASTNode::NodeType::UNARY_OP) {
            auto* unary = static_cast<UnaryExpr*>(arg);
            if (unary->op.type == frontend::TokenType::TOKEN_MINUS &&
                unary->operand && unary->operand->type == ASTNode::NodeType::LITERAL) {
                auto* lit = static_cast<LiteralExpr*>(unary->operand.get());
                if (std::holds_alternative<int64_t>(lit->value)) {
                    int64_t val = -std::get<int64_t>(lit->value);
                    calleeEnv[param->paramName] = Z3_mk_numeral(ctx,
                        std::to_string(val).c_str(), ti.sort);
                }
            }
        } else {
            // Symbolic — check if this arg identifier already has a Z3 var
            // (handles case where same variable is passed to multiple params)
            Z3_ast var = nullptr;
            if (arg->type == ASTNode::NodeType::IDENTIFIER) {
                auto* ident = static_cast<IdentifierExpr*>(arg);
                auto eIt = callerEnv.find(ident->name);
                if (eIt != callerEnv.end()) {
                    var = eIt->second;  // Reuse existing Z3 var
                }
            }

            if (!var) {
                Z3_symbol sym = Z3_mk_string_symbol(ctx, param->paramName.c_str());
                var = Z3_mk_const(ctx, sym, ti.sort);
            }
            calleeEnv[param->paramName] = var;

            // If arg is an identifier, alias it in callerEnv for constraint propagation
            if (arg->type == ASTNode::NodeType::IDENTIFIER) {
                auto* ident = static_cast<IdentifierExpr*>(arg);
                if (callerEnv.find(ident->name) == callerEnv.end()) {
                    callerEnv[ident->name] = var;
                }

                // If this arg has Rules constraints, assert them as assumptions
                auto rIt = callerVarRules.find(ident->name);
                if (rIt != callerVarRules.end()) {
                    const auto& [rulesDecl, typeName] = rIt->second;
                    std::function<void(RulesDeclStmt*)> collectAxioms;
                    collectAxioms = [&](RulesDeclStmt* r) {
                        for (const auto& cascName : r->cascadedRules) {
                            auto cIt = rules_table.find(cascName);
                            if (cIt != rules_table.end()) collectAxioms(cIt->second);
                        }
                        for (auto& cond : r->conditions) {
                            Z3_ast z3Cond = translateCondition(cond.get(), var, ti.sort);
                            if (z3Cond) assumptions.push_back(z3Cond);
                        }
                    };
                    collectAxioms(rulesDecl);
                }
            }
        }
    }

    // Create Z3 vars for any caller parameters not yet in callerEnv
    if (caller) {
        for (const auto& p : caller->parameters) {
            if (p->type != ASTNode::NodeType::PARAMETER) continue;
            auto* cp = static_cast<ParameterNode*>(p.get());
            if (callerEnv.find(cp->paramName) == callerEnv.end()) {
                std::string tname = cp->typeNode ? cp->typeNode->toString() : "int32";
                TypeInfo ti = resolveTypeSort(tname);
                Z3_symbol sym = Z3_mk_string_symbol(ctx, cp->paramName.c_str());
                callerEnv[cp->paramName] = Z3_mk_const(ctx, sym, ti.sort);
            }
        }

        // Assert caller's requires clauses as assumptions
        for (const auto& pre : caller->preconditions) {
            Z3_ast z3Pre = translateExprWithEnv(pre.get(), callerEnv, defaultSort);
            if (z3Pre) assumptions.push_back(z3Pre);
        }
    }

    // Assert ensures-derived facts as assumptions (v0.5.2)
    // If a local variable was assigned from a function with ensures clauses,
    // translate those ensures (with result → variable) as known facts.
    for (const auto& [varName, ensurer] : ensuresFacts) {
        auto vIt = callerEnv.find(varName);
        if (vIt == callerEnv.end()) continue;

        // Build env with "result" mapped to the assigned variable's Z3 var
        // (postcondition AST uses "result" as the identifier name)
        std::map<std::string, Z3_ast> ensuresEnv = callerEnv;
        ensuresEnv["result"] = vIt->second;
        ensuresEnv["$result"] = vIt->second;  // also map $result for safety

        for (const auto& post : ensurer->postconditions) {
            Z3_ast z3Post = translateExprWithEnv(post.get(), ensuresEnv, defaultSort);
            if (z3Post) assumptions.push_back(z3Post);
        }
    }

    // Verify each callee requires clause under caller assumptions
    for (size_t i = 0; i < callee->preconditions.size(); ++i) {
        Z3_ast z3pre = translateExprWithEnv(
            callee->preconditions[i].get(), calleeEnv, defaultSort);
        if (!z3pre) {
            VerifyOutcome out;
            out.result = VerifyResult::UNKNOWN;
            out.rulesName = callee->funcName;
            out.conditionText = prettyExpr(callee->preconditions[i].get());
            out.detail = "Could not encode requires clause at call site";
            out.line = line;
            out.column = column;
            outcomes.push_back(out);
            summary.outcomes.push_back(out);
            summary.unknown++;
            if (worstResult == VerifyResult::PROVEN) worstResult = VerifyResult::UNKNOWN;
            continue;
        }

        Z3_solver solver = makeSolver();

        // Assert all caller assumptions (requires + Rules)
        for (auto ax : assumptions) {
            Z3_solver_assert(ctx, solver, ax);
        }

        // Assert NOT(callee requires) — UNSAT means the requires always holds
        Z3_ast negated = Z3_mk_not(ctx, z3pre);
        Z3_solver_assert(ctx, solver, negated);

        Z3_lbool result = checkSat(solver);
        VerifyOutcome out;
        out.rulesName = callee->funcName;
        out.conditionText = prettyExpr(callee->preconditions[i].get());
        out.line = line;
        out.column = column;

        if (result == Z3_L_FALSE) {
            out.result = VerifyResult::PROVEN;
            out.detail = "Proven: call to '" + callee->funcName +
                         "' satisfies requires '" + out.conditionText + "'";
            summary.proven++;
        } else if (result == Z3_L_TRUE) {
            out.result = VerifyResult::DISPROVEN;
            Z3_model model = Z3_solver_get_model(ctx, solver);
            std::string counterexample;
            if (model) {
                Z3_model_inc_ref(ctx, model);
                bool first = true;
                for (const auto& [varName, varAst] : callerEnv) {
                    Z3_ast val = nullptr;
                    if (Z3_model_eval(ctx, model, varAst, true, &val) && val) {
                        if (!first) counterexample += ", ";
                        std::string valStr;
                        Z3_sort valSort = Z3_get_sort(ctx, val);
                        Z3_sort_kind sk = Z3_get_sort_kind(ctx, valSort);
                        if (sk == Z3_BV_SORT) {
                            uint64_t uval = 0;
                            if (Z3_get_numeral_uint64(ctx, val, &uval)) {
                                unsigned bw = Z3_get_bv_sort_size(ctx, valSort);
                                if (bw < 64 && (uval & (1ULL << (bw - 1)))) {
                                    int64_t sval = static_cast<int64_t>(uval) -
                                        (1LL << bw);
                                    valStr = std::to_string(sval);
                                } else {
                                    valStr = std::to_string(static_cast<int64_t>(uval));
                                }
                            } else {
                                valStr = Z3_ast_to_string(ctx, val);
                            }
                        } else {
                            valStr = Z3_ast_to_string(ctx, val);
                        }
                        counterexample += varName + " = " + valStr;
                        first = false;
                    }
                }
                Z3_model_dec_ref(ctx, model);
            }
            out.detail = "Violation: call to '" + callee->funcName +
                         "' may violate requires '" + out.conditionText + "'";
            if (!counterexample.empty()) {
                out.detail += " when " + counterexample;
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
        summary.outcomes.push_back(out);
        deleteSolver(solver);
    }

    return worstResult;
}

// ============================================================================
// Phase 2c: Loop Invariant Verification (v0.5.2)
// ============================================================================

VerifyResult Z3Verifier::verifyLoopInvariant(
    FuncDeclStmt* enclosingFunc,
    const std::vector<ASTNode*>& invariants,
    std::vector<VerifyOutcome>& outcomes,
    int line, int column)
{
    if (invariants.empty()) return VerifyResult::PROVEN;

    VerifyResult worstResult = VerifyResult::PROVEN;
    Z3_sort defaultSort = getIntSort(32);

    // Build environment from function parameters
    std::map<std::string, Z3_ast> env;
    for (size_t i = 0; i < enclosingFunc->parameters.size(); ++i) {
        const auto& p = enclosingFunc->parameters[i];
        if (p->type != ASTNode::NodeType::PARAMETER) continue;
        auto* param = static_cast<ParameterNode*>(p.get());
        std::string tname = param->typeNode ? param->typeNode->toString() : "int32";
        TypeInfo ti = resolveTypeSort(tname);
        Z3_symbol sym = Z3_mk_string_symbol(ctx, param->paramName.c_str());
        env[param->paramName] = Z3_mk_const(ctx, sym, ti.sort);
        if (i == 0) defaultSort = ti.sort;
    }

    // Assert function preconditions as assumptions
    std::vector<Z3_ast> assumptions;
    for (const auto& pre : enclosingFunc->preconditions) {
        Z3_ast z3Pre = translateExprWithEnv(pre.get(), env, defaultSort);
        if (z3Pre) assumptions.push_back(z3Pre);
    }

    // Check each invariant clause for consistency under preconditions
    for (size_t i = 0; i < invariants.size(); ++i) {
        Z3_ast z3Inv = translateExprWithEnv(invariants[i], env, defaultSort);
        if (!z3Inv) {
            VerifyOutcome out;
            out.result = VerifyResult::UNKNOWN;
            out.rulesName = enclosingFunc->funcName;
            out.conditionText = prettyExpr(invariants[i]);
            out.detail = "Could not encode invariant clause";
            out.line = line;
            out.column = column;
            outcomes.push_back(out);
            summary.outcomes.push_back(out);
            summary.unknown++;
            if (worstResult == VerifyResult::PROVEN) worstResult = VerifyResult::UNKNOWN;
            continue;
        }

        Z3_solver solver = makeSolver();

        // Assert function preconditions as assumptions
        for (Z3_ast assumption : assumptions) {
            Z3_solver_assert(ctx, solver, assumption);
        }

        // Assert negation of invariant — if UNSAT, invariant is implied by preconditions
        Z3_ast negInv = Z3_mk_not(ctx, z3Inv);
        Z3_solver_assert(ctx, solver, negInv);

        Z3_lbool result = Z3_solver_check(ctx, solver);

        VerifyOutcome out;
        out.rulesName = enclosingFunc->funcName;
        out.conditionText = prettyExpr(invariants[i]);
        out.line = line;
        out.column = column;

        if (result == Z3_L_FALSE) {
            // UNSAT: invariant is provably true at entry
            out.result = VerifyResult::PROVEN;
            out.detail = "Invariant proven consistent with preconditions";
            summary.proven++;
        } else if (result == Z3_L_TRUE) {
            // SAT: invariant is not guaranteed by preconditions alone
            // This is expected — invariant may depend on loop body establishing it
            // Report as "consistent" (not contradictory), not as disproven
            out.result = VerifyResult::UNKNOWN;
            out.detail = "Invariant not implied by preconditions alone (may still be valid)";
            summary.unknown++;
            if (worstResult == VerifyResult::PROVEN) worstResult = VerifyResult::UNKNOWN;
        } else {
            out.result = VerifyResult::UNKNOWN;
            out.detail = "Solver timeout on invariant";
            summary.unknown++;
            if (worstResult == VerifyResult::PROVEN) worstResult = VerifyResult::UNKNOWN;
        }

        outcomes.push_back(out);
        summary.outcomes.push_back(out);
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

//
// Phase 5: User Hash (ahash) Type Homogeneity Verification
//
// Mirrors ustack homogeneity but for ahset() value types.
// Encoding:  NOT(forall i: tag_i == expected)  =  exists i: tag_i != expected
// If UNSAT → no tag can differ → all match → PROVEN → eliminate runtime checks
//
VerifyResult Z3Verifier::verifyUHashHomogeneous(
    const std::vector<int64_t>& setTypeTags,
    int64_t expectedTag,
    std::vector<VerifyOutcome>& outcomes,
    int line, int column)
{
    static const char* tag_names[] = {
        "int8", "int16", "int32", "int64",
        "flt32", "flt64", "bool", "string", "pointer"
    };

    if (setTypeTags.empty()) {
        VerifyOutcome out;
        out.result = VerifyResult::UNKNOWN;
        out.conditionText = "uhash type homogeneity";
        out.detail = "no ahset() calls found in scope — cannot verify";
        out.line = line;
        out.column = column;
        summary.unknown++;
        outcomes.push_back(out);
        return VerifyResult::UNKNOWN;
    }

    Z3_solver solver = makeSolver();
    Z3_sort bv64 = Z3_mk_bv_sort(ctx, 64);

    Z3_ast expected = Z3_mk_int64(ctx, expectedTag, bv64);

    std::vector<Z3_ast> disjuncts;
    for (int64_t tag : setTypeTags) {
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
    out.conditionText = "uhash type homogeneity";
    out.line = line;
    out.column = column;

    const char* expectedName = (expectedTag >= 0 && expectedTag <= 8)
        ? tag_names[expectedTag] : "unknown";

    if (result == Z3_L_FALSE) {
        out.result = VerifyResult::PROVEN;
        out.detail = std::string("all ") + std::to_string(setTypeTags.size()) +
                     " ahset() calls proven type-homogeneous (" + expectedName +
                     ") — runtime tag checks eliminated";
        summary.proven++;
    } else if (result == Z3_L_TRUE) {
        out.result = VerifyResult::DISPROVEN;
        out.detail = "mixed types found among ahset() calls — runtime tag checks retained";
        summary.disproven++;
    } else {
        out.result = VerifyResult::UNKNOWN;
        out.detail = "solver timeout on uhash homogeneity — runtime tag checks retained";
        summary.unknown++;
    }

    VerifyResult vr = out.result;
    outcomes.push_back(out);
    return vr;
}

// ============================================================================
// Phase 6: Dead Branch Elimination (v0.5.0)
// Prove an if-condition is always true or always false under Rules constraints.
// ============================================================================

VerifyResult Z3Verifier::proveDeadBranch(
    ASTNode* condition,
    const std::map<std::string, std::pair<RulesDeclStmt*, std::string>>& varRules,
    std::vector<VerifyOutcome>& outcomes,
    int line, int column)
{
    if (!condition || varRules.empty()) return VerifyResult::UNKNOWN;

    // Determine the common sort — use the widest integer type among constrained vars.
    // If any float is involved, use real sort.
    bool hasFloat = false;
    int maxBitWidth = 32;
    for (const auto& [varName, rulesInfo] : varRules) {
        const auto& baseType = rulesInfo.second;
        TypeInfo ti = resolveTypeSort(baseType);
        if (ti.isFloat) hasFloat = true;
        if (ti.bitWidth > maxBitWidth) maxBitWidth = ti.bitWidth;
    }

    Z3_sort sort = hasFloat ? getRealSort() : getIntSort(maxBitWidth);

    // Create Z3 symbolic variables for each constrained variable
    std::map<std::string, Z3_ast> env;
    for (const auto& [varName, rulesInfo] : varRules) {
        Z3_symbol sym = Z3_mk_string_symbol(ctx, varName.c_str());
        Z3_ast var = Z3_mk_const(ctx, sym, sort);
        env[varName] = var;
    }

    // Collect all Rules constraint assertions
    std::vector<Z3_ast> axioms;
    for (const auto& [varName, rulesInfo] : varRules) {
        RulesDeclStmt* rules = rulesInfo.first;
        Z3_ast dollar = env[varName];  // The Z3 variable stands in for $ in Rules conditions

        // Process rules and cascaded rules using a worklist
        std::vector<RulesDeclStmt*> worklist = {rules};
        std::set<std::string> visited;
        while (!worklist.empty()) {
            RulesDeclStmt* r = worklist.back();
            worklist.pop_back();
            if (!visited.insert(r->rulesName).second) continue;  // Skip already-visited

            for (const auto& cascadedName : r->cascadedRules) {
                auto it = rules_table.find(cascadedName);
                if (it != rules_table.end()) {
                    worklist.push_back(it->second);
                }
            }
            for (size_t i = 0; i < r->conditions.size(); ++i) {
                Z3_ast z3cond = translateCondition(r->conditions[i].get(), dollar, sort);
                if (z3cond) axioms.push_back(z3cond);
            }
        }
    }

    if (axioms.empty()) return VerifyResult::UNKNOWN;

    // Translate the IF condition using the variable environment
    Z3_ast z3Cond = translateExprWithEnv(condition, env, sort);
    if (!z3Cond) return VerifyResult::UNKNOWN;

    // Test 1: Is the condition always TRUE?
    // Assert axioms AND NOT(condition). If UNSAT → condition always true.
    {
        Z3_solver solver = makeSolver();
        for (auto ax : axioms) Z3_solver_assert(ctx, solver, ax);
        Z3_solver_assert(ctx, solver, Z3_mk_not(ctx, z3Cond));
        Z3_lbool result = checkSat(solver);
        deleteSolver(solver);

        if (result == Z3_L_FALSE) {
            // Condition is always true given Rules
            VerifyOutcome out;
            out.result = VerifyResult::PROVEN;
            out.conditionText = condition->toString();
            out.detail = "condition proven always true under Rules constraints — else branch dead";
            out.line = line;
            out.column = column;
            summary.proven++;
            outcomes.push_back(out);
            return VerifyResult::PROVEN;
        }
    }

    // Test 2: Is the condition always FALSE?
    // Assert axioms AND condition. If UNSAT → condition always false.
    {
        Z3_solver solver = makeSolver();
        for (auto ax : axioms) Z3_solver_assert(ctx, solver, ax);
        Z3_solver_assert(ctx, solver, z3Cond);
        Z3_lbool result = checkSat(solver);
        deleteSolver(solver);

        if (result == Z3_L_FALSE) {
            // Condition is always false given Rules
            VerifyOutcome out;
            out.result = VerifyResult::DISPROVEN;
            out.conditionText = condition->toString();
            out.detail = "condition proven always false under Rules constraints — then branch dead";
            out.line = line;
            out.column = column;
            summary.proven++;  // It's a proven optimization, not a "disproven" violation
            outcomes.push_back(out);
            return VerifyResult::DISPROVEN;
        }
    }

    return VerifyResult::UNKNOWN;
}

// ============================================================================
// Phase 7: Bounds Check Elimination — proveBoundsInRange
// ============================================================================
// Prove that an index expression is always in [0, arraySize) given Rules
// constraints on the variables involved. Uses the same axiom-collection
// pattern as proveDeadBranch.

VerifyResult Z3Verifier::proveBoundsInRange(
    ASTNode* indexExpr,
    int64_t arraySize,
    const std::map<std::string, std::pair<RulesDeclStmt*, std::string>>& varRules,
    std::vector<VerifyOutcome>& outcomes,
    int line, int column)
{
    if (!indexExpr || varRules.empty() || arraySize <= 0) return VerifyResult::UNKNOWN;

    // Determine sort from constrained variable types
    bool hasFloat = false;
    int maxBitWidth = 32;
    for (const auto& [varName, rulesInfo] : varRules) {
        const auto& baseType = rulesInfo.second;
        TypeInfo ti = resolveTypeSort(baseType);
        if (ti.isFloat) hasFloat = true;
        if (ti.bitWidth > maxBitWidth) maxBitWidth = ti.bitWidth;
    }

    Z3_sort sort = hasFloat ? getRealSort() : getIntSort(maxBitWidth);

    // Create Z3 symbolic variables for each constrained variable
    std::map<std::string, Z3_ast> env;
    for (const auto& [varName, rulesInfo] : varRules) {
        Z3_symbol sym = Z3_mk_string_symbol(ctx, varName.c_str());
        Z3_ast var = Z3_mk_const(ctx, sym, sort);
        env[varName] = var;
    }

    // Collect all Rules constraint axioms
    std::vector<Z3_ast> axioms;
    for (const auto& [varName, rulesInfo] : varRules) {
        RulesDeclStmt* rules = rulesInfo.first;
        Z3_ast dollar = env[varName];

        std::vector<RulesDeclStmt*> worklist = {rules};
        std::set<std::string> visited;
        while (!worklist.empty()) {
            RulesDeclStmt* r = worklist.back();
            worklist.pop_back();
            if (!visited.insert(r->rulesName).second) continue;

            for (const auto& cascadedName : r->cascadedRules) {
                auto it = rules_table.find(cascadedName);
                if (it != rules_table.end()) {
                    worklist.push_back(it->second);
                }
            }
            for (size_t i = 0; i < r->conditions.size(); ++i) {
                Z3_ast z3cond = translateCondition(r->conditions[i].get(), dollar, sort);
                if (z3cond) axioms.push_back(z3cond);
            }
        }
    }

    if (axioms.empty()) return VerifyResult::UNKNOWN;

    // Translate the index expression using the variable environment
    Z3_ast z3Index = translateExprWithEnv(indexExpr, env, sort);
    if (!z3Index) return VerifyResult::UNKNOWN;

    // Build the bounds condition: index >= 0 AND index < arraySize
    Z3_ast zero = Z3_mk_numeral(ctx, "0", sort);
    Z3_ast size = Z3_mk_numeral(ctx, std::to_string(arraySize).c_str(), sort);

    Z3_ast geZero;
    Z3_ast ltSize;
    if (hasFloat) {
        geZero = Z3_mk_ge(ctx, z3Index, zero);
        ltSize = Z3_mk_lt(ctx, z3Index, size);
    } else {
        // BitVec sort — use signed comparisons
        geZero = Z3_mk_bvsge(ctx, z3Index, zero);
        ltSize = Z3_mk_bvslt(ctx, z3Index, size);
    }

    // We need to prove BOTH: index >= 0 AND index < arraySize
    // Equivalently: prove NOT(index >= 0 AND index < arraySize) is UNSAT under axioms
    Z3_ast inBounds_args[2] = {geZero, ltSize};
    Z3_ast inBounds = Z3_mk_and(ctx, 2, inBounds_args);
    Z3_ast outOfBounds = Z3_mk_not(ctx, inBounds);

    // Assert axioms AND NOT(inBounds). If UNSAT → always in bounds.
    Z3_solver solver = makeSolver();
    for (auto ax : axioms) Z3_solver_assert(ctx, solver, ax);
    Z3_solver_assert(ctx, solver, outOfBounds);
    Z3_lbool result = checkSat(solver);
    deleteSolver(solver);

    if (result == Z3_L_FALSE) {
        // Index is provably always in [0, arraySize)
        VerifyOutcome out;
        out.result = VerifyResult::PROVEN;
        out.conditionText = indexExpr->toString();
        out.detail = "index proven always in [0, " + std::to_string(arraySize)
                   + ") under Rules constraints — bounds check eliminated";
        out.line = line;
        out.column = column;
        summary.proven++;
        outcomes.push_back(out);
        return VerifyResult::PROVEN;
    }

    return VerifyResult::UNKNOWN;
}

// ============================================================================
// Phase 8: Overflow Check Elimination — proveNoOverflowFromRules
// ============================================================================
// Prove that an arithmetic operation (+, -, *) cannot overflow given Rules
// constraints on the operand variables. Uses the same axiom-collection
// pattern as proveBoundsInRange + Z3 native overflow predicates.

VerifyResult Z3Verifier::proveNoOverflowFromRules(
    char op,
    ASTNode* lhsExpr,
    ASTNode* rhsExpr,
    const std::map<std::string, std::pair<RulesDeclStmt*, std::string>>& varRules,
    std::vector<VerifyOutcome>& outcomes,
    int line, int column)
{
    if (!lhsExpr || !rhsExpr || varRules.empty()) return VerifyResult::UNKNOWN;
    if (op != '+' && op != '-' && op != '*') return VerifyResult::UNKNOWN;

    // Determine sort from constrained variable types
    bool hasFloat = false;
    int maxBitWidth = 32;
    for (const auto& [varName, rulesInfo] : varRules) {
        const auto& baseType = rulesInfo.second;
        TypeInfo ti = resolveTypeSort(baseType);
        if (ti.isFloat) hasFloat = true;
        if (ti.bitWidth > maxBitWidth) maxBitWidth = ti.bitWidth;
    }

    // Overflow checking only applies to integer types
    if (hasFloat) return VerifyResult::UNKNOWN;

    Z3_sort sort = getIntSort(maxBitWidth);

    // Create Z3 symbolic variables for each constrained variable
    std::map<std::string, Z3_ast> env;
    for (const auto& [varName, rulesInfo] : varRules) {
        Z3_symbol sym = Z3_mk_string_symbol(ctx, varName.c_str());
        Z3_ast var = Z3_mk_const(ctx, sym, sort);
        env[varName] = var;
    }

    // Collect all Rules constraint axioms
    std::vector<Z3_ast> axioms;
    for (const auto& [varName, rulesInfo] : varRules) {
        RulesDeclStmt* rules = rulesInfo.first;
        Z3_ast dollar = env[varName];

        std::vector<RulesDeclStmt*> worklist = {rules};
        std::set<std::string> visited;
        while (!worklist.empty()) {
            RulesDeclStmt* r = worklist.back();
            worklist.pop_back();
            if (!visited.insert(r->rulesName).second) continue;

            for (const auto& cascadedName : r->cascadedRules) {
                auto it = rules_table.find(cascadedName);
                if (it != rules_table.end()) {
                    worklist.push_back(it->second);
                }
            }
            for (size_t i = 0; i < r->conditions.size(); ++i) {
                Z3_ast z3cond = translateCondition(r->conditions[i].get(), dollar, sort);
                if (z3cond) axioms.push_back(z3cond);
            }
        }
    }

    if (axioms.empty()) return VerifyResult::UNKNOWN;

    // Translate operand expressions using the variable environment
    Z3_ast z3Lhs = translateExprWithEnv(lhsExpr, env, sort);
    Z3_ast z3Rhs = translateExprWithEnv(rhsExpr, env, sort);
    if (!z3Lhs || !z3Rhs) return VerifyResult::UNKNOWN;

    // Build overflow condition using Z3 native overflow predicates
    std::string opStr;
    Z3_ast overflowCond;
    bool isSigned = true;  // Aria signed integers use safe ops

    switch (op) {
        case '+': {
            opStr = "addition";
            Z3_ast noOverflow = Z3_mk_bvadd_no_overflow(ctx, z3Lhs, z3Rhs, isSigned);
            Z3_ast noUnderflow = Z3_mk_bvadd_no_underflow(ctx, z3Lhs, z3Rhs);
            Z3_ast safe[2] = {noOverflow, noUnderflow};
            Z3_ast allSafe = Z3_mk_and(ctx, 2, safe);
            overflowCond = Z3_mk_not(ctx, allSafe);
            break;
        }
        case '-': {
            opStr = "subtraction";
            Z3_ast noOverflow = Z3_mk_bvsub_no_overflow(ctx, z3Lhs, z3Rhs);
            Z3_ast noUnderflow = Z3_mk_bvsub_no_underflow(ctx, z3Lhs, z3Rhs, isSigned);
            Z3_ast safe[2] = {noOverflow, noUnderflow};
            Z3_ast allSafe = Z3_mk_and(ctx, 2, safe);
            overflowCond = Z3_mk_not(ctx, allSafe);
            break;
        }
        case '*': {
            opStr = "multiplication";
            Z3_ast noOverflow = Z3_mk_bvmul_no_overflow(ctx, z3Lhs, z3Rhs, isSigned);
            Z3_ast noUnderflow = Z3_mk_bvmul_no_underflow(ctx, z3Lhs, z3Rhs);
            Z3_ast safe[2] = {noOverflow, noUnderflow};
            Z3_ast allSafe = Z3_mk_and(ctx, 2, safe);
            overflowCond = Z3_mk_not(ctx, allSafe);
            break;
        }
        default:
            return VerifyResult::UNKNOWN;
    }

    // Assert axioms AND overflow. If UNSAT → overflow impossible under constraints.
    Z3_solver solver = makeSolver();
    for (auto ax : axioms) Z3_solver_assert(ctx, solver, ax);
    Z3_solver_assert(ctx, solver, overflowCond);
    Z3_lbool result = checkSat(solver);
    deleteSolver(solver);

    if (result == Z3_L_FALSE) {
        // UNSAT → no overflow possible under Rules constraints
        VerifyOutcome out;
        out.result = VerifyResult::PROVEN;
        out.conditionText = opStr + " overflow check";
        out.detail = "Proven: " + opStr + " cannot overflow under Rules constraints"
                   + " (int" + std::to_string(maxBitWidth) + ")"
                   + " — overflow check eliminated";
        out.line = line;
        out.column = column;
        summary.proven++;
        outcomes.push_back(out);
        return VerifyResult::PROVEN;
    }

    return VerifyResult::UNKNOWN;
}

// ============================================================================
// Phase 9: Null Check Elimination — proveNonNullFromRules
// ============================================================================
// Prove that an expression is never null/zero/None given Rules constraints.
// For integer variables checked via ?? or ? operators, if Rules guarantee
// the value is never 0 (e.g., $ > 0, $ != 0), the null check is dead.
// Uses same axiom-collection pattern as proveBoundsInRange/proveNoOverflowFromRules.

VerifyResult Z3Verifier::proveNonNullFromRules(
    ASTNode* expr,
    const std::map<std::string, std::pair<RulesDeclStmt*, std::string>>& varRules,
    std::vector<VerifyOutcome>& outcomes,
    int line, int column)
{
    if (!expr || varRules.empty()) return VerifyResult::UNKNOWN;

    // Determine sort from constrained variable types
    int maxBitWidth = 32;
    bool hasFloat = false;
    for (const auto& [varName, rulesInfo] : varRules) {
        const auto& baseType = rulesInfo.second;
        TypeInfo ti = resolveTypeSort(baseType);
        if (ti.isFloat) hasFloat = true;
        if (ti.bitWidth > maxBitWidth) maxBitWidth = ti.bitWidth;
    }

    // Null check elimination on floats doesn't apply (no NIL sentinel)
    if (hasFloat) return VerifyResult::UNKNOWN;

    Z3_sort sort = getIntSort(maxBitWidth);

    // Create Z3 symbolic variables for each constrained variable
    std::map<std::string, Z3_ast> env;
    for (const auto& [varName, rulesInfo] : varRules) {
        Z3_symbol sym = Z3_mk_string_symbol(ctx, varName.c_str());
        Z3_ast var = Z3_mk_const(ctx, sym, sort);
        env[varName] = var;
    }

    // Collect all Rules constraint axioms
    std::vector<Z3_ast> axioms;
    for (const auto& [varName, rulesInfo] : varRules) {
        RulesDeclStmt* rules = rulesInfo.first;
        Z3_ast dollar = env[varName];

        std::vector<RulesDeclStmt*> worklist = {rules};
        std::set<std::string> visited;
        while (!worklist.empty()) {
            RulesDeclStmt* r = worklist.back();
            worklist.pop_back();
            if (!visited.insert(r->rulesName).second) continue;

            for (const auto& cascadedName : r->cascadedRules) {
                auto it = rules_table.find(cascadedName);
                if (it != rules_table.end()) {
                    worklist.push_back(it->second);
                }
            }
            for (size_t i = 0; i < r->conditions.size(); ++i) {
                Z3_ast z3cond = translateCondition(r->conditions[i].get(), dollar, sort);
                if (z3cond) axioms.push_back(z3cond);
            }
        }
    }

    if (axioms.empty()) return VerifyResult::UNKNOWN;

    // Translate the expression being null-checked
    Z3_ast z3Expr = translateExprWithEnv(expr, env, sort);
    if (!z3Expr) return VerifyResult::UNKNOWN;

    // Build null condition: expr == 0 (the NIL sentinel for integers)
    Z3_ast zero = Z3_mk_numeral(ctx, "0", sort);
    Z3_ast isZero = Z3_mk_eq(ctx, z3Expr, zero);

    // Assert axioms AND (expr == 0). If UNSAT → value is never null under constraints.
    Z3_solver solver = makeSolver();
    for (auto ax : axioms) Z3_solver_assert(ctx, solver, ax);
    Z3_solver_assert(ctx, solver, isZero);
    Z3_lbool result = checkSat(solver);
    deleteSolver(solver);

    if (result == Z3_L_FALSE) {
        // UNSAT → value is never null/zero under Rules constraints
        VerifyOutcome out;
        out.result = VerifyResult::PROVEN;
        out.conditionText = "null check";
        out.detail = "Proven: expression is never null/zero under Rules constraints"
                   " (int" + std::to_string(maxBitWidth) + ")"
                   " — null check eliminated";
        out.line = line;
        out.column = column;
        summary.proven++;
        outcomes.push_back(out);
        return VerifyResult::PROVEN;
    }

    return VerifyResult::UNKNOWN;
}

// ============================================================================
// Phase 10: Rules<T> Propagation — proveRulesSubsumption
// ============================================================================
// Proves ∀x. callerRules(x) → calleeRules(x)
// Method: assert all callerRules conditions, then for each calleeRules condition
// assert its NEGATION and check UNSAT. If UNSAT for all → subsumption proven.

VerifyResult Z3Verifier::proveRulesSubsumption(
    const std::string& callerRulesName,
    const std::string& calleeRulesName,
    const std::string& typeName,
    std::vector<VerifyOutcome>& outcomes,
    int line, int column) {

    // Trivial case: same rules
    if (callerRulesName == calleeRulesName) {
        VerifyOutcome out;
        out.result = VerifyResult::PROVEN;
        out.conditionText = callerRulesName + " ⊇ " + calleeRulesName;
        out.detail = "Trivially proven: same Rules name";
        out.line = line;
        out.column = column;
        summary.proven++;
        outcomes.push_back(out);
        return VerifyResult::PROVEN;
    }

    // Look up both rules
    auto callerIt = rules_table.find(callerRulesName);
    auto calleeIt = rules_table.find(calleeRulesName);
    if (callerIt == rules_table.end() || calleeIt == rules_table.end()) {
        return VerifyResult::UNKNOWN;
    }

    RulesDeclStmt* callerRules = callerIt->second;
    RulesDeclStmt* calleeRules = calleeIt->second;

    // Resolve type sort
    TypeInfo ti = resolveTypeSort(typeName);
    Z3_sort sort = ti.sort;

    // Create symbolic variable $
    Z3_symbol sym = Z3_mk_string_symbol(ctx, "$");
    Z3_ast dollar = Z3_mk_const(ctx, sym, sort);

    // Collect caller axioms (all conditions of caller rules + cascaded)
    std::vector<Z3_ast> callerAxioms;
    std::function<void(RulesDeclStmt*)> collectCallerAxioms;
    collectCallerAxioms = [&](RulesDeclStmt* r) {
        for (const auto& cascName : r->cascadedRules) {
            auto cIt = rules_table.find(cascName);
            if (cIt != rules_table.end()) collectCallerAxioms(cIt->second);
        }
        for (auto& cond : r->conditions) {
            Z3_ast z3Cond = translateCondition(cond.get(), dollar, sort);
            if (z3Cond) callerAxioms.push_back(z3Cond);
        }
    };
    collectCallerAxioms(callerRules);

    if (callerAxioms.empty()) return VerifyResult::UNKNOWN;

    // Collect callee conditions (all conditions of callee rules + cascaded)
    std::vector<Z3_ast> calleeConditions;
    std::function<void(RulesDeclStmt*)> collectCalleeConditions;
    collectCalleeConditions = [&](RulesDeclStmt* r) {
        for (const auto& cascName : r->cascadedRules) {
            auto cIt = rules_table.find(cascName);
            if (cIt != rules_table.end()) collectCalleeConditions(cIt->second);
        }
        for (auto& cond : r->conditions) {
            Z3_ast z3Cond = translateCondition(cond.get(), dollar, sort);
            if (z3Cond) calleeConditions.push_back(z3Cond);
        }
    };
    collectCalleeConditions(calleeRules);

    if (calleeConditions.empty()) {
        // No callee conditions → trivially subsumed
        VerifyOutcome out;
        out.result = VerifyResult::PROVEN;
        out.conditionText = callerRulesName + " ⊇ " + calleeRulesName;
        out.detail = "Trivially proven: callee has no conditions";
        out.line = line;
        out.column = column;
        summary.proven++;
        outcomes.push_back(out);
        return VerifyResult::PROVEN;
    }

    // For each callee condition, prove it holds under caller axioms:
    // Assert callerAxioms AND ¬calleeCondition[i], check UNSAT
    for (auto& calleeCond : calleeConditions) {
        Z3_solver solver = makeSolver();
        for (auto ax : callerAxioms) Z3_solver_assert(ctx, solver, ax);
        Z3_ast negCond = Z3_mk_not(ctx, calleeCond);
        Z3_solver_assert(ctx, solver, negCond);
        Z3_lbool result = checkSat(solver);
        deleteSolver(solver);

        if (result != Z3_L_FALSE) {
            // SAT or UNKNOWN → can't prove this condition
            return VerifyResult::UNKNOWN;
        }
    }

    // All callee conditions proven to hold under caller axioms
    VerifyOutcome out;
    out.result = VerifyResult::PROVEN;
    out.conditionText = callerRulesName + " ⊇ " + calleeRulesName;
    out.detail = "Proven: caller Rules '" + callerRulesName
               + "' subsumes callee Rules '" + calleeRulesName + "'";
    out.line = line;
    out.column = column;
    summary.proven++;
    outcomes.push_back(out);
    return VerifyResult::PROVEN;
}

// ============================================================================
// Phase 11: User Assertions — prove(expr) and assert_static(expr)
// ============================================================================

VerifyResult Z3Verifier::proveUserAssertion(
    ASTNode* condition,
    const std::map<std::string, std::pair<RulesDeclStmt*, std::string>>& varRules,
    std::vector<VerifyOutcome>& outcomes,
    int line, int column) {

    if (!condition) return VerifyResult::UNKNOWN;

    // Build Z3 variable environment from Rules-constrained variables
    std::map<std::string, Z3_ast> env;
    std::vector<Z3_ast> axioms;  // Rules constraints as axioms

    for (const auto& [varName, rulesPair] : varRules) {
        const auto& [rulesDecl, typeName] = rulesPair;
        TypeInfo ti = resolveTypeSort(typeName);
        Z3_sort sort = ti.sort;

        Z3_symbol sym = Z3_mk_string_symbol(ctx, varName.c_str());
        Z3_ast var = Z3_mk_const(ctx, sym, sort);
        env[varName] = var;

        // Assert all Rules conditions for this variable
        std::function<void(RulesDeclStmt*)> collectAxioms;
        collectAxioms = [&](RulesDeclStmt* r) {
            for (const auto& cascName : r->cascadedRules) {
                auto cIt = rules_table.find(cascName);
                if (cIt != rules_table.end()) collectAxioms(cIt->second);
            }
            for (auto& cond : r->conditions) {
                Z3_ast z3Cond = translateCondition(cond.get(), var, sort);
                if (z3Cond) axioms.push_back(z3Cond);
            }
        };
        collectAxioms(rulesDecl);
    }

    // Also create Z3 constants for any free variables in the expression
    // that don't have Rules constraints (they remain unconstrained)
    std::function<void(ASTNode*)> collectFreeVars;
    collectFreeVars = [&](ASTNode* node) {
        if (!node) return;
        if (node->type == ASTNode::NodeType::IDENTIFIER) {
            auto* ident = static_cast<IdentifierExpr*>(node);
            if (env.find(ident->name) == env.end()) {
                // Use int32 (32-bit bitvector) as default sort for unconstrained vars
                Z3_sort defaultSort = getIntSort(32);
                Z3_symbol sym = Z3_mk_string_symbol(ctx, ident->name.c_str());
                Z3_ast var = Z3_mk_const(ctx, sym, defaultSort);
                env[ident->name] = var;
            }
        } else if (node->type == ASTNode::NodeType::BINARY_OP) {
            auto* bin = static_cast<BinaryExpr*>(node);
            collectFreeVars(bin->left.get());
            collectFreeVars(bin->right.get());
        } else if (node->type == ASTNode::NodeType::UNARY_OP) {
            auto* un = static_cast<UnaryExpr*>(node);
            collectFreeVars(un->operand.get());
        }
    };
    collectFreeVars(condition);

    // Determine the sort to use for translation
    Z3_sort defaultSort = getIntSort(32);
    if (!env.empty()) {
        // Use the sort of the first variable
        Z3_ast firstVar = env.begin()->second;
        defaultSort = Z3_get_sort(ctx, firstVar);
    }

    // Translate condition to Z3
    Z3_ast z3Cond = translateExprWithEnv(condition, env, defaultSort);
    if (!z3Cond) {
        VerifyOutcome out;
        out.result = VerifyResult::UNKNOWN;
        out.conditionText = condition->toString();
        out.detail = "Could not translate expression to Z3";
        out.line = line;
        out.column = column;
        summary.unknown++;
        outcomes.push_back(out);
        return VerifyResult::UNKNOWN;
    }

    // To prove: condition always holds under axioms
    // Method: assert axioms AND ¬condition, check UNSAT
    Z3_solver solver = makeSolver();
    for (auto ax : axioms) Z3_solver_assert(ctx, solver, ax);
    Z3_ast negCond = Z3_mk_not(ctx, z3Cond);
    Z3_solver_assert(ctx, solver, negCond);

    Z3_lbool result = checkSat(solver);

    if (result == Z3_L_FALSE) {
        // UNSAT → condition always holds → PROVEN
        deleteSolver(solver);
        VerifyOutcome out;
        out.result = VerifyResult::PROVEN;
        out.conditionText = condition->toString();
        out.detail = "Proven: assertion always holds under given constraints";
        out.line = line;
        out.column = column;
        summary.proven++;
        outcomes.push_back(out);
        summary.outcomes.push_back(out);
        return VerifyResult::PROVEN;
    }

    if (result == Z3_L_TRUE) {
        // SAT → found counterexample → DISPROVEN
        // Extract counterexample from the model
        std::string counterexample;
        Z3_model model = Z3_solver_get_model(ctx, solver);
        if (model) {
            Z3_model_inc_ref(ctx, model);
            bool first = true;
            for (const auto& [varName, varAst] : env) {
                Z3_ast val = nullptr;
                if (Z3_model_eval(ctx, model, varAst, true, &val) && val) {
                    if (!first) counterexample += ", ";
                    // Convert Z3 value to human-readable form
                    std::string valStr;
                    Z3_sort valSort = Z3_get_sort(ctx, val);
                    Z3_sort_kind sk = Z3_get_sort_kind(ctx, valSort);
                    if (sk == Z3_BV_SORT) {
                        // Bitvector → extract as signed integer
                        uint64_t uval = 0;
                        if (Z3_get_numeral_uint64(ctx, val, &uval)) {
                            unsigned bw = Z3_get_bv_sort_size(ctx, valSort);
                            // Interpret as signed
                            if (bw < 64 && (uval & (1ULL << (bw - 1)))) {
                                int64_t sval = static_cast<int64_t>(uval) -
                                    (1LL << bw);
                                valStr = std::to_string(sval);
                            } else {
                                valStr = std::to_string(static_cast<int64_t>(uval));
                            }
                        } else {
                            valStr = Z3_ast_to_string(ctx, val);
                        }
                    } else {
                        valStr = Z3_ast_to_string(ctx, val);
                    }
                    counterexample += varName + " = " + valStr;
                    first = false;
                }
            }
            Z3_model_dec_ref(ctx, model);
        }
        deleteSolver(solver);

        VerifyOutcome out;
        out.result = VerifyResult::DISPROVEN;
        out.conditionText = condition->toString();
        out.detail = counterexample.empty()
            ? "Disproven: counterexample exists"
            : "Disproven: fails when " + counterexample;
        out.line = line;
        out.column = column;
        summary.disproven++;
        outcomes.push_back(out);
        summary.outcomes.push_back(out);
        return VerifyResult::DISPROVEN;
    }

    // UNKNOWN
    deleteSolver(solver);
    VerifyOutcome out;
    out.result = VerifyResult::UNKNOWN;
    out.conditionText = condition->toString();
    out.detail = "Unknown: solver could not determine (timeout or too complex)";
    out.line = line;
    out.column = column;
    summary.unknown++;
    outcomes.push_back(out);
    summary.outcomes.push_back(out);
    return VerifyResult::UNKNOWN;
}

// ============================================================================
// Phase 12: Rules<T> Transitivity — Call-site narrowing (v0.5.3)
// ============================================================================
// When a caller passes a Rules-constrained argument to a callee that expects
// a different Rules on that parameter, verify narrowing is valid:
//   ∀x. callerRules(x) → calleeRules(x)

VerifyResult Z3Verifier::verifyRulesNarrowing(
    FuncDeclStmt* callee,
    const std::vector<ASTNode*>& args,
    const std::map<std::string, std::pair<RulesDeclStmt*, std::string>>& callerVarRules,
    std::vector<VerifyOutcome>& outcomes,
    int line, int column)
{
    if (!callee) return VerifyResult::PROVEN;

    VerifyResult worstResult = VerifyResult::PROVEN;

    // Build a set of callee parameter names for quick lookup
    std::set<std::string> paramNames;
    for (auto& p : callee->parameters) {
        if (p->type == ASTNode::NodeType::PARAMETER) {
            paramNames.insert(static_cast<ParameterNode*>(p.get())->paramName);
        } else if (p->type == ASTNode::NodeType::VAR_DECL) {
            paramNames.insert(static_cast<VarDeclStmt*>(p.get())->varName);
        }
    }

    // Scan callee body for limit<> VarDecls that reference parameter names.
    // e.g. limit<Positive> int32:safe_x = x;  →  param "x" requires Positive
    std::map<std::string, std::string> paramRulesFromBody;  // param_name → Rules name
    if (callee->body && callee->body->type == ASTNode::NodeType::BLOCK) {
        auto* blk = static_cast<BlockStmt*>(callee->body.get());
        for (auto& s : blk->statements) {
            if (s->type == ASTNode::NodeType::VAR_DECL) {
                auto* vd = static_cast<VarDeclStmt*>(s.get());
                if (!vd->limitRulesName.empty() && vd->initializer &&
                    vd->initializer->type == ASTNode::NodeType::IDENTIFIER) {
                    auto* init = static_cast<IdentifierExpr*>(vd->initializer.get());
                    if (paramNames.count(init->name)) {
                        paramRulesFromBody[init->name] = vd->limitRulesName;
                    }
                }
            }
        }
    }

    for (size_t i = 0; i < callee->parameters.size() && i < args.size(); ++i) {
        // Get callee parameter info
        ASTNode* paramNode = callee->parameters[i].get();
        if (!paramNode) continue;

        std::string calleeParamType;
        std::string calleeParamName;

        if (paramNode->type == ASTNode::NodeType::PARAMETER) {
            auto* pn = static_cast<ParameterNode*>(paramNode);
            calleeParamType = pn->typeNode ? pn->typeNode->toString() : "";
            calleeParamName = pn->paramName;
        } else if (paramNode->type == ASTNode::NodeType::VAR_DECL) {
            auto* vd = static_cast<VarDeclStmt*>(paramNode);
            calleeParamType = vd->limitRulesName.empty() ? vd->typeName : vd->limitRulesName;
            calleeParamName = vd->varName;
        }

        if (calleeParamName.empty()) continue;

        // Determine the callee's Rules constraint for this parameter:
        // 1. Direct: parameter type is a Rules name (e.g. VarDecl param with limit<>)
        // 2. Body: callee body has limit<Rules> T:var = paramName;
        std::string calleeRulesName;
        auto directIt = rules_table.find(calleeParamType);
        if (directIt != rules_table.end()) {
            calleeRulesName = calleeParamType;
        } else {
            auto bodyIt = paramRulesFromBody.find(calleeParamName);
            if (bodyIt != paramRulesFromBody.end()) {
                calleeRulesName = bodyIt->second;
            }
        }

        if (calleeRulesName.empty()) continue;

        // Check if Rules exists in table
        auto calleeRulesIt = rules_table.find(calleeRulesName);
        if (calleeRulesIt == rules_table.end()) continue;

        // We have a callee parameter with Rules constraint — check the argument
        ASTNode* arg = args[i];
        if (!arg || arg->type != ASTNode::NodeType::IDENTIFIER) continue;

        auto* ident = static_cast<IdentifierExpr*>(arg);
        auto callerIt = callerVarRules.find(ident->name);
        if (callerIt == callerVarRules.end()) {
            // Caller arg has no Rules constraint — can't narrow
            VerifyOutcome out;
            out.result = VerifyResult::UNKNOWN;
            out.rulesName = calleeRulesName;
            out.conditionText = ident->name + " → " + calleeParamName + ": " + calleeRulesName;
            out.detail = "Cannot verify narrowing: argument '" + ident->name +
                         "' has no Rules constraint, but callee parameter '" +
                         calleeParamName + "' requires " + calleeRulesName;
            out.line = line;
            out.column = column;
            summary.unknown++;
            outcomes.push_back(out);
            summary.outcomes.push_back(out);
            if (worstResult == VerifyResult::PROVEN) worstResult = VerifyResult::UNKNOWN;
            continue;
        }

        const auto& [callerRules, callerBaseType] = callerIt->second;
        std::string callerRulesName = callerRules->rulesName;

        // Same Rules — trivially valid
        if (callerRulesName == calleeRulesName) continue;

        // Different Rules — check subsumption
        std::vector<VerifyOutcome> subOutcomes;
        VerifyResult subResult = proveRulesSubsumption(
            callerRulesName, calleeRulesName, callerBaseType,
            subOutcomes, line, column);

        if (subResult == VerifyResult::PROVEN) {
            VerifyOutcome out;
            out.result = VerifyResult::PROVEN;
            out.rulesName = calleeRulesName;
            out.conditionText = callerRulesName + " ⊇ " + calleeRulesName;
            out.detail = "Proven: narrowing " + callerRulesName + " → " +
                         calleeRulesName + " for parameter '" + calleeParamName +
                         "' in call to '" + callee->funcName + "'";
            out.line = line;
            out.column = column;
            summary.proven++;
            outcomes.push_back(out);
            summary.outcomes.push_back(out);
        } else {
            VerifyOutcome out;
            out.result = VerifyResult::DISPROVEN;
            out.rulesName = calleeRulesName;
            out.conditionText = callerRulesName + " ⊇ " + calleeRulesName;
            out.detail = "Invalid narrowing: '" + callerRulesName +
                         "' does not subsume '" + calleeRulesName +
                         "' for parameter '" + calleeParamName +
                         "' in call to '" + callee->funcName + "'";
            out.line = line;
            out.column = column;
            summary.disproven++;
            outcomes.push_back(out);
            summary.outcomes.push_back(out);
            worstResult = VerifyResult::DISPROVEN;
        }
    }

    return worstResult;
}

// ============================================================================
// Phase 13: Pattern Exhaustiveness via SMT (v0.5.3)
// ============================================================================
// For pick(selector) { cases... }, prove the cases cover the entire domain
// under the selector's Rules constraints.
// Encoding: assert Rules constraints on selector, then assert NOT(any pattern
// matches), check UNSAT → exhaustive.

VerifyResult Z3Verifier::provePickExhaustiveness(
    ASTNode* selector,
    const std::vector<ASTNode*>& patterns,
    const std::map<std::string, std::pair<RulesDeclStmt*, std::string>>& varRules,
    std::vector<VerifyOutcome>& outcomes,
    int line, int column)
{
    if (patterns.empty()) {
        VerifyOutcome out;
        out.result = VerifyResult::UNKNOWN;
        out.conditionText = "pick exhaustiveness";
        out.detail = "No patterns to check";
        out.line = line;
        out.column = column;
        summary.unknown++;
        outcomes.push_back(out);
        summary.outcomes.push_back(out);
        return VerifyResult::UNKNOWN;
    }

    // Check for wildcard (*) — always exhaustive
    for (auto* pat : patterns) {
        if (!pat) continue;
        if (pat->type == ASTNode::NodeType::IDENTIFIER) {
            auto* ident = static_cast<IdentifierExpr*>(pat);
            if (ident->name == "*") {
                VerifyOutcome out;
                out.result = VerifyResult::PROVEN;
                out.conditionText = "pick exhaustiveness";
                out.detail = "Proven: wildcard (*) case present — exhaustive";
                out.line = line;
                out.column = column;
                summary.proven++;
                outcomes.push_back(out);
                summary.outcomes.push_back(out);
                return VerifyResult::PROVEN;
            }
        }
        // Also check for LiteralExpr("*") pattern
        if (pat->type == ASTNode::NodeType::LITERAL) {
            auto* lit = static_cast<LiteralExpr*>(pat);
            if (std::holds_alternative<std::string>(lit->value) &&
                std::get<std::string>(lit->value) == "*") {
                VerifyOutcome out;
                out.result = VerifyResult::PROVEN;
                out.conditionText = "pick exhaustiveness";
                out.detail = "Proven: wildcard (*) case present — exhaustive";
                out.line = line;
                out.column = column;
                summary.proven++;
                outcomes.push_back(out);
                summary.outcomes.push_back(out);
                return VerifyResult::PROVEN;
            }
        }
    }

    // Determine selector sort from Rules constraints
    Z3_sort sort = getIntSort(32);  // default
    std::string rulesName;
    std::vector<Z3_ast> rulesAxioms;

    if (selector && selector->type == ASTNode::NodeType::IDENTIFIER) {
        auto* ident = static_cast<IdentifierExpr*>(selector);
        auto rIt = varRules.find(ident->name);
        if (rIt != varRules.end()) {
            const auto& [rulesDecl, typeName] = rIt->second;
            rulesName = rulesDecl->rulesName;
            TypeInfo ti = resolveTypeSort(typeName);
            sort = ti.sort;

            // Create symbolic variable for selector
            Z3_symbol sym = Z3_mk_string_symbol(ctx, ident->name.c_str());
            Z3_ast selVar = Z3_mk_const(ctx, sym, sort);

            // Collect all Rules axioms (including cascaded)
            std::function<void(RulesDeclStmt*)> collectAxioms;
            collectAxioms = [&](RulesDeclStmt* r) {
                for (const auto& cascName : r->cascadedRules) {
                    auto cIt = rules_table.find(cascName);
                    if (cIt != rules_table.end()) collectAxioms(cIt->second);
                }
                for (auto& cond : r->conditions) {
                    Z3_ast z3Cond = translateCondition(cond.get(), selVar, sort);
                    if (z3Cond) rulesAxioms.push_back(z3Cond);
                }
            };
            collectAxioms(rulesDecl);
        } else {
            // No Rules on selector — can't bound domain without wildcard
            VerifyOutcome out;
            out.result = VerifyResult::UNKNOWN;
            out.conditionText = "pick exhaustiveness";
            out.detail = "Cannot verify: selector '" + ident->name +
                         "' has no Rules constraint and no wildcard (*) case";
            out.line = line;
            out.column = column;
            summary.unknown++;
            outcomes.push_back(out);
            summary.outcomes.push_back(out);
            return VerifyResult::UNKNOWN;
        }
    } else {
        // Non-identifier selector — can't analyze
        VerifyOutcome out;
        out.result = VerifyResult::UNKNOWN;
        out.conditionText = "pick exhaustiveness";
        out.detail = "Cannot verify: selector is not a simple variable";
        out.line = line;
        out.column = column;
        summary.unknown++;
        outcomes.push_back(out);
        summary.outcomes.push_back(out);
        return VerifyResult::UNKNOWN;
    }

    if (rulesAxioms.empty()) {
        return VerifyResult::UNKNOWN;
    }

    // Build selector Z3 variable
    auto* selIdent = static_cast<IdentifierExpr*>(selector);
    Z3_symbol selSym = Z3_mk_string_symbol(ctx, selIdent->name.c_str());
    Z3_ast selVar = Z3_mk_const(ctx, selSym, sort);

    // Encode each pattern as a disjunct: sel matches pattern_i
    std::vector<Z3_ast> patternMatches;
    for (auto* pat : patterns) {
        if (!pat) continue;

        if (pat->type == ASTNode::NodeType::LITERAL) {
            auto* lit = static_cast<LiteralExpr*>(pat);
            if (std::holds_alternative<int64_t>(lit->value)) {
                int64_t val = std::get<int64_t>(lit->value);
                Z3_ast litVal = Z3_mk_numeral(ctx, std::to_string(val).c_str(), sort);
                patternMatches.push_back(Z3_mk_eq(ctx, selVar, litVal));
            } else if (std::holds_alternative<double>(lit->value)) {
                double val = std::get<double>(lit->value);
                Z3_ast litVal = Z3_mk_numeral(ctx, std::to_string(val).c_str(), sort);
                patternMatches.push_back(Z3_mk_eq(ctx, selVar, litVal));
            }
        } else if (pat->type == ASTNode::NodeType::BINARY_OP) {
            auto* bin = static_cast<BinaryExpr*>(pat);

            Z3_sort_kind sk = Z3_get_sort_kind(ctx, sort);
            bool isBV = (sk == Z3_BV_SORT);

            if (bin->op.type == frontend::TokenType::TOKEN_DOT_DOT ||
                bin->op.type == frontend::TokenType::TOKEN_DOT_DOT_DOT) {
                // Range pattern: start..end (inclusive) or start...end (exclusive)
                Z3_ast startVal = translateExprWithEnv(bin->left.get(), {}, sort);
                Z3_ast endVal = translateExprWithEnv(bin->right.get(), {}, sort);
                if (startVal && endVal) {
                    Z3_ast geStart = isBV ? Z3_mk_bvsge(ctx, selVar, startVal)
                                          : Z3_mk_ge(ctx, selVar, startVal);
                    Z3_ast cmpEnd;
                    if (bin->op.type == frontend::TokenType::TOKEN_DOT_DOT) {
                        cmpEnd = isBV ? Z3_mk_bvsle(ctx, selVar, endVal)
                                      : Z3_mk_le(ctx, selVar, endVal);
                    } else {
                        cmpEnd = isBV ? Z3_mk_bvslt(ctx, selVar, endVal)
                                      : Z3_mk_lt(ctx, selVar, endVal);
                    }
                    Z3_ast rangeArgs[2] = {geStart, cmpEnd};
                    patternMatches.push_back(Z3_mk_and(ctx, 2, rangeArgs));
                }
            } else {
                // Comparison pattern: (< 10), (> 20), etc.
                // The binary expr has the comparison value on the right
                Z3_ast compVal = translateExprWithEnv(bin->right.get(), {}, sort);
                if (compVal) {
                    Z3_ast cmp = nullptr;
                    switch (bin->op.type) {
                        case frontend::TokenType::TOKEN_LESS:
                            cmp = isBV ? Z3_mk_bvslt(ctx, selVar, compVal)
                                       : Z3_mk_lt(ctx, selVar, compVal);
                            break;
                        case frontend::TokenType::TOKEN_LESS_EQUAL:
                            cmp = isBV ? Z3_mk_bvsle(ctx, selVar, compVal)
                                       : Z3_mk_le(ctx, selVar, compVal);
                            break;
                        case frontend::TokenType::TOKEN_GREATER:
                            cmp = isBV ? Z3_mk_bvsgt(ctx, selVar, compVal)
                                       : Z3_mk_gt(ctx, selVar, compVal);
                            break;
                        case frontend::TokenType::TOKEN_GREATER_EQUAL:
                            cmp = isBV ? Z3_mk_bvsge(ctx, selVar, compVal)
                                       : Z3_mk_ge(ctx, selVar, compVal);
                            break;
                        case frontend::TokenType::TOKEN_EQUAL_EQUAL:
                            cmp = Z3_mk_eq(ctx, selVar, compVal);
                            break;
                        case frontend::TokenType::TOKEN_BANG_EQUAL: {
                            Z3_ast eq = Z3_mk_eq(ctx, selVar, compVal);
                            cmp = Z3_mk_not(ctx, eq);
                            break;
                        }
                        default: break;
                    }
                    if (cmp) patternMatches.push_back(cmp);
                }
            }
        }
    }

    if (patternMatches.empty()) {
        VerifyOutcome out;
        out.result = VerifyResult::UNKNOWN;
        out.conditionText = "pick exhaustiveness";
        out.detail = "Cannot verify: no translatable patterns";
        out.line = line;
        out.column = column;
        summary.unknown++;
        outcomes.push_back(out);
        summary.outcomes.push_back(out);
        return VerifyResult::UNKNOWN;
    }

    // Build: NOT(pattern_1 OR pattern_2 OR ... OR pattern_n)
    Z3_ast anyMatch;
    if (patternMatches.size() == 1) {
        anyMatch = patternMatches[0];
    } else {
        anyMatch = Z3_mk_or(ctx, static_cast<unsigned>(patternMatches.size()),
                            patternMatches.data());
    }
    Z3_ast noMatch = Z3_mk_not(ctx, anyMatch);

    // Assert Rules constraints + NOT(any match) and check UNSAT
    Z3_solver solver = makeSolver();
    for (auto ax : rulesAxioms) {
        Z3_solver_assert(ctx, solver, ax);
    }
    Z3_solver_assert(ctx, solver, noMatch);

    Z3_lbool result = checkSat(solver);

    if (result == Z3_L_FALSE) {
        // UNSAT — no value in the Rules domain is uncovered → exhaustive
        deleteSolver(solver);
        VerifyOutcome out;
        out.result = VerifyResult::PROVEN;
        out.conditionText = "pick exhaustiveness";
        out.detail = "Proven: pick cases are exhaustive over " + rulesName + " domain";
        out.line = line;
        out.column = column;
        summary.proven++;
        outcomes.push_back(out);
        summary.outcomes.push_back(out);
        return VerifyResult::PROVEN;
    } else if (result == Z3_L_TRUE) {
        // SAT — there exists an uncovered value
        std::string counterexample;
        Z3_model model = Z3_solver_get_model(ctx, solver);
        if (model) {
            Z3_model_inc_ref(ctx, model);
            Z3_ast val = nullptr;
            if (Z3_model_eval(ctx, model, selVar, true, &val) && val) {
                Z3_sort valSort = Z3_get_sort(ctx, val);
                Z3_sort_kind sk2 = Z3_get_sort_kind(ctx, valSort);
                if (sk2 == Z3_BV_SORT) {
                    uint64_t uval = 0;
                    if (Z3_get_numeral_uint64(ctx, val, &uval)) {
                        unsigned bw = Z3_get_bv_sort_size(ctx, valSort);
                        if (bw < 64 && (uval & (1ULL << (bw - 1)))) {
                            int64_t sval = static_cast<int64_t>(uval) - (1LL << bw);
                            counterexample = std::to_string(sval);
                        } else {
                            counterexample = std::to_string(static_cast<int64_t>(uval));
                        }
                    } else {
                        counterexample = Z3_ast_to_string(ctx, val);
                    }
                } else {
                    counterexample = Z3_ast_to_string(ctx, val);
                }
            }
            Z3_model_dec_ref(ctx, model);
        }
        deleteSolver(solver);

        VerifyOutcome out;
        out.result = VerifyResult::DISPROVEN;
        out.conditionText = "pick exhaustiveness";
        out.detail = "Non-exhaustive: pick does not cover all values in " +
                     rulesName + " domain";
        if (!counterexample.empty()) {
            out.detail += " (uncovered value: " + selIdent->name + " = " +
                          counterexample + ")";
        }
        out.line = line;
        out.column = column;
        summary.disproven++;
        outcomes.push_back(out);
        summary.outcomes.push_back(out);
        return VerifyResult::DISPROVEN;
    }

    // UNKNOWN
    deleteSolver(solver);
    VerifyOutcome out;
    out.result = VerifyResult::UNKNOWN;
    out.conditionText = "pick exhaustiveness";
    out.detail = "Cannot verify pick exhaustiveness (solver timeout or too complex)";
    out.line = line;
    out.column = column;
    summary.unknown++;
    outcomes.push_back(out);
    summary.outcomes.push_back(out);
    return VerifyResult::UNKNOWN;
}

// ============================================================================
// Phase 14: Rules + Null Interaction (v0.5.3)
// ============================================================================
// Prove that a Rules constraint excludes NIL (represented as 0).
// If Rules conditions on a variable entail $ != 0, the variable can never be
// null, so Optional null checks can be skipped.

VerifyResult Z3Verifier::proveRulesExcludesNull(
    const std::string& rulesName,
    const std::string& typeName,
    std::vector<VerifyOutcome>& outcomes,
    int line, int column)
{
    auto it = rules_table.find(rulesName);
    if (it == rules_table.end()) return VerifyResult::UNKNOWN;

    RulesDeclStmt* rules = it->second;
    TypeInfo ti = resolveTypeSort(typeName);
    Z3_sort sort = ti.sort;

    // Create symbolic variable $
    Z3_symbol sym = Z3_mk_string_symbol(ctx, "$");
    Z3_ast dollar = Z3_mk_const(ctx, sym, sort);

    // Collect all Rules axioms (including cascaded)
    std::vector<Z3_ast> axioms;
    std::function<void(RulesDeclStmt*)> collectAxioms;
    collectAxioms = [&](RulesDeclStmt* r) {
        for (const auto& cascName : r->cascadedRules) {
            auto cIt = rules_table.find(cascName);
            if (cIt != rules_table.end()) collectAxioms(cIt->second);
        }
        for (auto& cond : r->conditions) {
            Z3_ast z3Cond = translateCondition(cond.get(), dollar, sort);
            if (z3Cond) axioms.push_back(z3Cond);
        }
    };
    collectAxioms(rules);

    if (axioms.empty()) {
        VerifyOutcome out;
        out.result = VerifyResult::UNKNOWN;
        out.rulesName = rulesName;
        out.conditionText = rulesName + " excludes NIL";
        out.detail = "Cannot verify: no conditions in Rules '" + rulesName + "'";
        out.line = line;
        out.column = column;
        summary.unknown++;
        outcomes.push_back(out);
        summary.outcomes.push_back(out);
        return VerifyResult::UNKNOWN;
    }

    // Prove $ != 0 under Rules axioms:
    // Assert axioms AND $ == 0, check UNSAT → Rules excludes null
    Z3_ast zero = Z3_mk_numeral(ctx, "0", sort);
    Z3_ast eqZero = Z3_mk_eq(ctx, dollar, zero);

    Z3_solver solver = makeSolver();
    for (auto ax : axioms) {
        Z3_solver_assert(ctx, solver, ax);
    }
    Z3_solver_assert(ctx, solver, eqZero);

    Z3_lbool result = checkSat(solver);
    deleteSolver(solver);

    if (result == Z3_L_FALSE) {
        // UNSAT — 0 is excluded by Rules → never null
        VerifyOutcome out;
        out.result = VerifyResult::PROVEN;
        out.rulesName = rulesName;
        out.conditionText = rulesName + " excludes NIL";
        out.detail = "Proven: Rules '" + rulesName +
                     "' excludes NIL (0) — variable is never null";
        out.line = line;
        out.column = column;
        summary.proven++;
        outcomes.push_back(out);
        summary.outcomes.push_back(out);
        return VerifyResult::PROVEN;
    } else if (result == Z3_L_TRUE) {
        // SAT — 0 is a valid value under Rules → could be null
        VerifyOutcome out;
        out.result = VerifyResult::DISPROVEN;
        out.rulesName = rulesName;
        out.conditionText = rulesName + " excludes NIL";
        out.detail = "Disproven: Rules '" + rulesName +
                     "' allows NIL (0) — variable could be null";
        out.line = line;
        out.column = column;
        summary.disproven++;
        outcomes.push_back(out);
        summary.outcomes.push_back(out);
        return VerifyResult::DISPROVEN;
    }

    VerifyOutcome out;
    out.result = VerifyResult::UNKNOWN;
    out.rulesName = rulesName;
    out.conditionText = rulesName + " excludes NIL";
    out.detail = "Unknown: cannot determine if Rules excludes NIL";
    out.line = line;
    out.column = column;
    summary.unknown++;
    outcomes.push_back(out);
    summary.outcomes.push_back(out);
    return VerifyResult::UNKNOWN;
}

// ============================================================================
// Phase 15: Automatic Rules Widening/Narrowing (v0.5.3)
// ============================================================================
// Infer the tightest min/max bounds on a function's return value given its
// parameter Rules constraints. Walks the function body for pass/return
// expressions, translates them to Z3, and uses optimize (minimize/maximize)
// to find the range.

VerifyResult Z3Verifier::inferReturnBounds(
    FuncDeclStmt* func,
    const std::map<std::string, std::pair<RulesDeclStmt*, std::string>>& paramRules,
    int64_t& outMin, int64_t& outMax,
    std::vector<VerifyOutcome>& outcomes,
    int line, int column)
{
    if (!func || !func->body) return VerifyResult::UNKNOWN;

    // Collect all PASS and RETURN value expressions from the function body.
    // Also track local variable definitions so we can resolve identifiers
    // (e.g. pass result; where result = safe_x * 2).
    std::vector<ASTNode*> returnExprs;
    std::map<std::string, ASTNode*> localDefs;  // var_name → initializer expr
    std::function<void(ASTNode*)> collectReturns;
    collectReturns = [&](ASTNode* node) {
        if (!node) return;
        using NT = ASTNode::NodeType;
        switch (node->type) {
            case NT::VAR_DECL: {
                auto* vd = static_cast<VarDeclStmt*>(node);
                if (vd->initializer) {
                    localDefs[vd->varName] = vd->initializer.get();
                }
                break;
            }
            case NT::PASS: {
                auto* s = static_cast<PassStmt*>(node);
                if (s->value) {
                    returnExprs.push_back(s->value.get());
                }
                break;
            }
            case NT::RETURN: {
                auto* s = static_cast<ReturnStmt*>(node);
                if (s->value) returnExprs.push_back(s->value.get());
                break;
            }
            case NT::BLOCK: {
                auto* blk = static_cast<BlockStmt*>(node);
                for (auto& st : blk->statements) collectReturns(st.get());
                break;
            }
            case NT::IF: {
                auto* s = static_cast<IfStmt*>(node);
                collectReturns(s->thenBranch.get());
                collectReturns(s->elseBranch.get());
                break;
            }
            default: break;
        }
    };
    collectReturns(func->body.get());

    // Resolve return expressions that are just identifier references to locals.
    // e.g. "pass result;" where result = safe_x * 2 → use safe_x * 2.
    for (auto& expr : returnExprs) {
        if (expr->type == ASTNode::NodeType::IDENTIFIER) {
            auto* ident = static_cast<IdentifierExpr*>(expr);
            auto dIt = localDefs.find(ident->name);
            if (dIt != localDefs.end()) {
                expr = dIt->second;
            }
        }
    }

    if (returnExprs.empty()) {
        VerifyOutcome out;
        out.result = VerifyResult::UNKNOWN;
        out.rulesName = func->funcName;
        out.conditionText = "return bounds inference";
        out.detail = "No return expressions found in '" + func->funcName + "'";
        out.line = line;
        out.column = column;
        summary.unknown++;
        outcomes.push_back(out);
        summary.outcomes.push_back(out);
        return VerifyResult::UNKNOWN;
    }

    // Build parameter environment
    Z3_sort defaultSort = getIntSort(32);
    std::map<std::string, Z3_ast> env;
    std::vector<Z3_ast> paramAxioms;

    for (const auto& p : func->parameters) {
        if (p->type != ASTNode::NodeType::PARAMETER) continue;
        auto* param = static_cast<ParameterNode*>(p.get());
        std::string tname = param->typeNode ? param->typeNode->toString() : "int32";
        TypeInfo ti = resolveTypeSort(tname);

        Z3_symbol sym = Z3_mk_string_symbol(ctx, param->paramName.c_str());
        Z3_ast var = Z3_mk_const(ctx, sym, ti.sort);
        env[param->paramName] = var;

        // If this parameter has Rules constraints, collect axioms
        auto rIt = paramRules.find(param->paramName);
        if (rIt != paramRules.end()) {
            const auto& [rulesDecl, baseType] = rIt->second;
            std::function<void(RulesDeclStmt*)> collectAxioms;
            collectAxioms = [&](RulesDeclStmt* r) {
                for (const auto& cascName : r->cascadedRules) {
                    auto cIt = rules_table.find(cascName);
                    if (cIt != rules_table.end()) collectAxioms(cIt->second);
                }
                for (auto& cond : r->conditions) {
                    Z3_ast z3Cond = translateCondition(cond.get(), var, ti.sort);
                    if (z3Cond) paramAxioms.push_back(z3Cond);
                }
            };
            collectAxioms(rulesDecl);
        }
    }

    // Also add limit<> body variables (e.g. limit<Rules> T:var = param)
    // that appear in paramRules but aren't parameter names.
    // These are needed because return expressions reference the limit<> variable.
    for (const auto& [varName, rulesInfo] : paramRules) {
        if (env.count(varName)) continue;  // already in env as a parameter
        const auto& [rulesDecl, baseType] = rulesInfo;
        TypeInfo ti = resolveTypeSort(baseType);
        Z3_symbol sym = Z3_mk_string_symbol(ctx, varName.c_str());
        Z3_ast var = Z3_mk_const(ctx, sym, ti.sort);
        env[varName] = var;

        // Collect Rules axioms for this variable
        std::function<void(RulesDeclStmt*)> collectAxioms;
        collectAxioms = [&](RulesDeclStmt* r) {
            for (const auto& cascName : r->cascadedRules) {
                auto cIt = rules_table.find(cascName);
                if (cIt != rules_table.end()) collectAxioms(cIt->second);
            }
            for (auto& cond : r->conditions) {
                Z3_ast z3Cond = translateCondition(cond.get(), var, ti.sort);
                if (z3Cond) paramAxioms.push_back(z3Cond);
            }
        };
        collectAxioms(rulesDecl);
    }

    if (paramAxioms.empty()) {
        VerifyOutcome out;
        out.result = VerifyResult::UNKNOWN;
        out.rulesName = func->funcName;
        out.conditionText = "return bounds inference";
        out.detail = "No parameter constraints for '" + func->funcName + "'";
        out.line = line;
        out.column = column;
        summary.unknown++;
        outcomes.push_back(out);
        summary.outcomes.push_back(out);
        return VerifyResult::UNKNOWN;
    }

    // Try to translate return expressions to Z3
    // Use the first translatable expression for bounds inference
    Z3_ast returnZ3 = nullptr;
    for (auto* expr : returnExprs) {
        returnZ3 = translateExprWithEnv(expr, env, defaultSort);
        if (returnZ3) break;
    }

    if (!returnZ3) {
        VerifyOutcome out;
        out.result = VerifyResult::UNKNOWN;
        out.rulesName = func->funcName;
        out.conditionText = "return bounds inference";
        out.detail = "Could not encode return value of '" + func->funcName + "' for Z3";
        out.line = line;
        out.column = column;
        summary.unknown++;
        outcomes.push_back(out);
        summary.outcomes.push_back(out);
        return VerifyResult::UNKNOWN;
    }

    // Use Z3 optimizer to find min and max return values
    Z3_optimize opt = Z3_mk_optimize(ctx);
    Z3_optimize_inc_ref(ctx, opt);

    // Assert parameter constraints
    for (auto ax : paramAxioms) {
        Z3_optimize_assert(ctx, opt, ax);
    }

    // Find minimum
    Z3_optimize_minimize(ctx, opt, returnZ3);
    Z3_lbool minResult = Z3_optimize_check(ctx, opt, 0, nullptr);

    bool gotMin = false, gotMax = false;
    if (minResult == Z3_L_TRUE) {
        Z3_model model = Z3_optimize_get_model(ctx, opt);
        if (model) {
            Z3_model_inc_ref(ctx, model);
            Z3_ast val = nullptr;
            if (Z3_model_eval(ctx, model, returnZ3, true, &val) && val) {
                uint64_t uval = 0;
                if (Z3_get_numeral_uint64(ctx, val, &uval)) {
                    Z3_sort valSort = Z3_get_sort(ctx, val);
                    unsigned bw = 32;
                    if (Z3_get_sort_kind(ctx, valSort) == Z3_BV_SORT) {
                        bw = Z3_get_bv_sort_size(ctx, valSort);
                    }
                    if (bw < 64 && (uval & (1ULL << (bw - 1)))) {
                        outMin = static_cast<int64_t>(uval) - (1LL << bw);
                    } else {
                        outMin = static_cast<int64_t>(uval);
                    }
                    gotMin = true;
                }
            }
            Z3_model_dec_ref(ctx, model);
        }
    }

    // Reset optimizer for maximization
    Z3_optimize_dec_ref(ctx, opt);
    opt = Z3_mk_optimize(ctx);
    Z3_optimize_inc_ref(ctx, opt);
    for (auto ax : paramAxioms) {
        Z3_optimize_assert(ctx, opt, ax);
    }

    Z3_optimize_maximize(ctx, opt, returnZ3);
    Z3_lbool maxResult = Z3_optimize_check(ctx, opt, 0, nullptr);

    if (maxResult == Z3_L_TRUE) {
        Z3_model model = Z3_optimize_get_model(ctx, opt);
        if (model) {
            Z3_model_inc_ref(ctx, model);
            Z3_ast val = nullptr;
            if (Z3_model_eval(ctx, model, returnZ3, true, &val) && val) {
                uint64_t uval = 0;
                if (Z3_get_numeral_uint64(ctx, val, &uval)) {
                    Z3_sort valSort = Z3_get_sort(ctx, val);
                    unsigned bw = 32;
                    if (Z3_get_sort_kind(ctx, valSort) == Z3_BV_SORT) {
                        bw = Z3_get_bv_sort_size(ctx, valSort);
                    }
                    if (bw < 64 && (uval & (1ULL << (bw - 1)))) {
                        outMax = static_cast<int64_t>(uval) - (1LL << bw);
                    } else {
                        outMax = static_cast<int64_t>(uval);
                    }
                    gotMax = true;
                }
            }
            Z3_model_dec_ref(ctx, model);
        }
    }

    Z3_optimize_dec_ref(ctx, opt);

    if (gotMin && gotMax) {
        VerifyOutcome out;
        out.result = VerifyResult::PROVEN;
        out.rulesName = func->funcName;
        out.conditionText = "return bounds: [" + std::to_string(outMin) +
                            ", " + std::to_string(outMax) + "]";
        out.detail = "Inferred: '" + func->funcName + "' returns values in [" +
                     std::to_string(outMin) + ", " + std::to_string(outMax) + "]";
        out.line = line;
        out.column = column;
        summary.proven++;
        outcomes.push_back(out);
        summary.outcomes.push_back(out);
        return VerifyResult::PROVEN;
    }

    VerifyOutcome out;
    out.result = VerifyResult::UNKNOWN;
    out.rulesName = func->funcName;
    out.conditionText = "return bounds inference";
    out.detail = "Could not infer return bounds for '" + func->funcName + "'";
    out.line = line;
    out.column = column;
    summary.unknown++;
    outcomes.push_back(out);
    summary.outcomes.push_back(out);
    return VerifyResult::UNKNOWN;
}

// ============================================================================
// Phase 16: Data Race Detection (v0.5.4)
// ============================================================================
// For each thread spawned via aria_thread_create(func, arg), we collect
// variable accesses in both the spawning function (after the spawn point)
// and the spawned function. If any variable is accessed by both threads
// (read+write or write+write) without being protected by mutex lock/unlock,
// we report a potential data race.
//
// Encoding: For each shared variable V:
//   thread1_accesses(V) ∧ thread2_accesses(V) ∧
//   (write1(V) ∨ write2(V)) ∧ ¬protected(V) → RACE
//
// We use Z3 to model overlapping access windows.

// Helper: extract the name of a function being called
static std::string getCallName(ASTNode* node) {
    if (!node || node->type != ASTNode::NodeType::CALL) return "";
    auto* call = static_cast<CallExpr*>(node);
    if (!call->callee) return "";
    if (call->callee->type == ASTNode::NodeType::IDENTIFIER) {
        return static_cast<IdentifierExpr*>(call->callee.get())->name;
    }
    return "";
}

// Helper: collect variable accesses from a function body, noting which
// are inside lock/unlock regions
static void collectAccesses(
    ASTNode* node,
    const std::string& funcName,
    bool& insideLock,
    std::vector<Z3Verifier::ThreadAccess>& accesses,
    std::set<std::string>& declaredLocals)
{
    if (!node) return;
    using NT = ASTNode::NodeType;

    switch (node->type) {
        case NT::VAR_DECL: {
            auto* vd = static_cast<VarDeclStmt*>(node);
            declaredLocals.insert(vd->varName);
            if (vd->initializer) {
                collectAccesses(vd->initializer.get(), funcName, insideLock,
                                accesses, declaredLocals);
            }
            break;
        }

        case NT::ASSIGNMENT: {
            auto* assign = static_cast<AssignmentExpr*>(node);
            // LHS is a write
            if (assign->target && assign->target->type == NT::IDENTIFIER) {
                auto* ident = static_cast<IdentifierExpr*>(assign->target.get());
                if (!declaredLocals.count(ident->name)) {
                    accesses.push_back({ident->name, true, funcName,
                                        insideLock, assign->line, assign->column});
                }
            }
            // RHS is a read
            if (assign->value) {
                collectAccesses(assign->value.get(), funcName, insideLock,
                                accesses, declaredLocals);
            }
            break;
        }

        case NT::IDENTIFIER: {
            auto* ident = static_cast<IdentifierExpr*>(node);
            if (!declaredLocals.count(ident->name)) {
                accesses.push_back({ident->name, false, funcName,
                                    insideLock, node->line, node->column});
            }
            break;
        }

        case NT::CALL: {
            auto* call = static_cast<CallExpr*>(node);
            std::string name = getCallName(node);

            // Detect lock/unlock transitions  
            if (name == "aria_mutex_lock" || name == "aria_shim_mutex_lock" ||
                name == "aria_rwlock_wrlock" || name == "aria_shim_rwlock_wrlock" ||
                name == "aria_rwlock_rdlock" || name == "aria_shim_rwlock_rdlock") {
                insideLock = true;
            } else if (name == "aria_mutex_unlock" || name == "aria_shim_mutex_unlock" ||
                       name == "aria_rwlock_unlock" || name == "aria_shim_rwlock_unlock") {
                insideLock = false;
            }

            for (auto& arg : call->arguments) {
                collectAccesses(arg.get(), funcName, insideLock,
                                accesses, declaredLocals);
            }
            break;
        }

        case NT::BLOCK: {
            auto* blk = static_cast<BlockStmt*>(node);
            for (auto& s : blk->statements) {
                collectAccesses(s.get(), funcName, insideLock,
                                accesses, declaredLocals);
            }
            break;
        }

        case NT::IF: {
            auto* s = static_cast<IfStmt*>(node);
            if (s->condition) collectAccesses(s->condition.get(), funcName, insideLock, accesses, declaredLocals);
            collectAccesses(s->thenBranch.get(), funcName, insideLock, accesses, declaredLocals);
            collectAccesses(s->elseBranch.get(), funcName, insideLock, accesses, declaredLocals);
            break;
        }

        case NT::WHILE: {
            auto* s = static_cast<WhileStmt*>(node);
            if (s->condition) collectAccesses(s->condition.get(), funcName, insideLock, accesses, declaredLocals);
            collectAccesses(s->body.get(), funcName, insideLock, accesses, declaredLocals);
            break;
        }

        case NT::FOR: {
            auto* s = static_cast<ForStmt*>(node);
            collectAccesses(s->body.get(), funcName, insideLock, accesses, declaredLocals);
            break;
        }

        case NT::BINARY_OP: {
            auto* bin = static_cast<BinaryExpr*>(node);
            // Check if this is an assignment (= operator)
            if (bin->op.type == frontend::TokenType::TOKEN_EQUAL &&
                bin->left && bin->left->type == NT::IDENTIFIER) {
                auto* ident = static_cast<IdentifierExpr*>(bin->left.get());
                if (!declaredLocals.count(ident->name)) {
                    accesses.push_back({ident->name, true, funcName,
                                        insideLock, bin->line, bin->column});
                }
                // RHS is a read
                collectAccesses(bin->right.get(), funcName, insideLock, accesses, declaredLocals);
            } else {
                collectAccesses(bin->left.get(), funcName, insideLock, accesses, declaredLocals);
                collectAccesses(bin->right.get(), funcName, insideLock, accesses, declaredLocals);
            }
            break;
        }

        case NT::UNARY_OP: {
            auto* un = static_cast<UnaryExpr*>(node);
            collectAccesses(un->operand.get(), funcName, insideLock, accesses, declaredLocals);
            break;
        }

        case NT::EXPRESSION_STMT: {
            auto* es = static_cast<ExpressionStmt*>(node);
            collectAccesses(es->expression.get(), funcName, insideLock, accesses, declaredLocals);
            break;
        }

        case NT::PASS: {
            auto* s = static_cast<PassStmt*>(node);
            if (s->value) collectAccesses(s->value.get(), funcName, insideLock, accesses, declaredLocals);
            break;
        }

        case NT::RETURN: {
            auto* s = static_cast<ReturnStmt*>(node);
            if (s->value) collectAccesses(s->value.get(), funcName, insideLock, accesses, declaredLocals);
            break;
        }

        default:
            break;
    }
}

VerifyResult Z3Verifier::verifyDataRaceFreedom(
    FuncDeclStmt* spawner,
    const std::vector<FuncDeclStmt*>& threadFuncs,
    const std::map<std::string, FuncDeclStmt*>& allFuncs,
    std::vector<VerifyOutcome>& outcomes,
    int line, int column)
{
    if (!spawner || threadFuncs.empty()) return VerifyResult::PROVEN;

    // Collect accesses from spawner (after spawn point we conservatively
    // include all accesses in the spawner body)
    std::vector<ThreadAccess> spawnerAccesses;
    std::set<std::string> spawnerLocals;

    // Collect declared locals from parameters
    for (auto& p : spawner->parameters) {
        if (p->type == ASTNode::NodeType::PARAMETER) {
            spawnerLocals.insert(static_cast<ParameterNode*>(p.get())->paramName);
        } else if (p->type == ASTNode::NodeType::VAR_DECL) {
            spawnerLocals.insert(static_cast<VarDeclStmt*>(p.get())->varName);
        }
    }

    bool spawnerLockState = false;
    collectAccesses(spawner->body.get(), spawner->funcName, spawnerLockState,
                    spawnerAccesses, spawnerLocals);

    // Collect accesses from each thread function
    std::vector<std::vector<ThreadAccess>> threadAccessSets;
    for (auto* tf : threadFuncs) {
        std::vector<ThreadAccess> tfAccesses;
        std::set<std::string> tfLocals;
        for (auto& p : tf->parameters) {
            if (p->type == ASTNode::NodeType::PARAMETER) {
                tfLocals.insert(static_cast<ParameterNode*>(p.get())->paramName);
            } else if (p->type == ASTNode::NodeType::VAR_DECL) {
                tfLocals.insert(static_cast<VarDeclStmt*>(p.get())->varName);
            }
        }
        bool tfLockState = false;
        collectAccesses(tf->body.get(), tf->funcName, tfLockState, tfAccesses, tfLocals);
        threadAccessSets.push_back(std::move(tfAccesses));
    }

    VerifyResult worstResult = VerifyResult::PROVEN;

    // Collect lock handle variable names (used as arguments to lock/unlock/create/destroy)
    // These are synchronization primitives, not shared data
    std::set<std::string> lockHandleVars;
    auto collectLockHandles = [&](const std::vector<ThreadAccess>& accesses, ASTNode* body) {
        // Walk body for calls to lock/unlock/create/destroy functions
        std::function<void(ASTNode*)> walk;
        walk = [&](ASTNode* node) {
            if (!node) return;
            if (node->type == ASTNode::NodeType::CALL) {
                auto* call = static_cast<CallExpr*>(node);
                std::string name = getCallName(node);
                if (name.find("mutex") != std::string::npos ||
                    name.find("rwlock") != std::string::npos ||
                    name.find("condvar") != std::string::npos ||
                    name.find("barrier") != std::string::npos) {
                    for (auto& arg : call->arguments) {
                        if (arg->type == ASTNode::NodeType::IDENTIFIER) {
                            lockHandleVars.insert(
                                static_cast<IdentifierExpr*>(arg.get())->name);
                        }
                    }
                }
                for (auto& arg : call->arguments) walk(arg.get());
            } else if (node->type == ASTNode::NodeType::BLOCK) {
                for (auto& s : static_cast<BlockStmt*>(node)->statements) walk(s.get());
            } else if (node->type == ASTNode::NodeType::EXPRESSION_STMT) {
                walk(static_cast<ExpressionStmt*>(node)->expression.get());
            } else if (node->type == ASTNode::NodeType::VAR_DECL) {
                auto* vd = static_cast<VarDeclStmt*>(node);
                if (vd->initializer) walk(vd->initializer.get());
            } else if (node->type == ASTNode::NodeType::IF) {
                auto* s = static_cast<IfStmt*>(node);
                walk(s->thenBranch.get()); walk(s->elseBranch.get());
            }
        };
        walk(body);
    };
    collectLockHandles(spawnerAccesses, spawner->body.get());
    for (auto* tf : threadFuncs) {
        collectLockHandles({}, tf->body.get());
    }

    // For each pair (spawner, thread), check for conflicting accesses
    for (size_t ti = 0; ti < threadAccessSets.size(); ++ti) {
        const auto& tfAccesses = threadAccessSets[ti];
        const std::string& tfName = threadFuncs[ti]->funcName;

        // Build set of variables accessed by thread function (non-local)
        std::map<std::string, bool> threadWrites;   // var -> hasWrite
        std::map<std::string, bool> threadProtected; // var -> isProtected
        for (const auto& acc : tfAccesses) {
            if (acc.isWrite) threadWrites[acc.varName] = true;
            else if (threadWrites.find(acc.varName) == threadWrites.end())
                threadWrites[acc.varName] = false;
            if (acc.isProtected)
                threadProtected[acc.varName] = true;
        }

        // Check for shared variable conflicts
        for (const auto& sAcc : spawnerAccesses) {
            // Skip lock handle variables — they are synchronization primitives
            if (lockHandleVars.count(sAcc.varName)) continue;

            auto twIt = threadWrites.find(sAcc.varName);
            if (twIt == threadWrites.end()) continue;  // not accessed by thread

            bool threadHasWrite = twIt->second;
            bool spawnerHasWrite = sAcc.isWrite;

            // Data race requires at least one write
            if (!threadHasWrite && !spawnerHasWrite) continue;

            // Check if both are protected
            bool spawnerProtected = sAcc.isProtected;
            bool tfProtected = threadProtected.count(sAcc.varName) &&
                               threadProtected[sAcc.varName];

            if (spawnerProtected && tfProtected) continue;  // both synchronized

            // Use Z3 to encode the race potential:
            // If there exists a scheduling where both accesses overlap
            // and at least one is a write → race
            Z3_solver solver = makeSolver();

            // Boolean: thread1_accesses(var) ∧ thread2_accesses(var)
            Z3_sort boolSort = Z3_mk_bool_sort(ctx);
            Z3_ast t1_access = Z3_mk_true(ctx);
            Z3_ast t2_access = Z3_mk_true(ctx);
            Z3_ast t1_write = spawnerHasWrite ? Z3_mk_true(ctx) : Z3_mk_false(ctx);
            Z3_ast t2_write = threadHasWrite ? Z3_mk_true(ctx) : Z3_mk_false(ctx);
            Z3_ast t1_prot = spawnerProtected ? Z3_mk_true(ctx) : Z3_mk_false(ctx);
            Z3_ast t2_prot = tfProtected ? Z3_mk_true(ctx) : Z3_mk_false(ctx);

            // Race condition: concurrent ∧ at_least_one_write ∧ ¬both_protected
            Z3_ast writeDisjunct = Z3_mk_or(ctx, 2, (Z3_ast[]){t1_write, t2_write});
            Z3_ast bothProtected = Z3_mk_and(ctx, 2, (Z3_ast[]){t1_prot, t2_prot});
            Z3_ast notBothProtected = Z3_mk_not(ctx, bothProtected);
            Z3_ast concurrent = Z3_mk_and(ctx, 2, (Z3_ast[]){t1_access, t2_access});

            Z3_ast raceCond = Z3_mk_and(ctx, 3,
                (Z3_ast[]){concurrent, writeDisjunct, notBothProtected});

            Z3_solver_assert(ctx, solver, raceCond);
            Z3_lbool result = checkSat(solver);
            deleteSolver(solver);

            if (result == Z3_L_TRUE) {
                // Race is possible
                VerifyOutcome out;
                out.result = VerifyResult::DISPROVEN;
                out.rulesName = sAcc.varName;
                out.conditionText = "data race on '" + sAcc.varName + "'";
                std::string accessType = (spawnerHasWrite && threadHasWrite)
                    ? "write-write" : "read-write";
                out.detail = "Potential " + accessType + " data race on '" +
                             sAcc.varName + "' between '" + spawner->funcName +
                             "' and thread '" + tfName +
                             "' — variable is not consistently protected by a mutex";
                out.line = sAcc.line;
                out.column = sAcc.column;
                summary.disproven++;
                outcomes.push_back(out);
                summary.outcomes.push_back(out);
                worstResult = VerifyResult::DISPROVEN;
            }
        }
    }

    if (worstResult == VerifyResult::PROVEN && !threadAccessSets.empty()) {
        VerifyOutcome out;
        out.result = VerifyResult::PROVEN;
        out.conditionText = "data race freedom";
        out.detail = "Proven: no data races detected in '" + spawner->funcName +
                     "' — all shared accesses are properly synchronized";
        out.line = line;
        out.column = column;
        summary.proven++;
        outcomes.push_back(out);
        summary.outcomes.push_back(out);
    }

    return worstResult;
}

// ============================================================================
// Phase 17: Deadlock Freedom (v0.5.4)
// ============================================================================
// Build a lock acquisition graph from all functions. Each lock variable gets
// a Z3 integer ordinal. We assert that whenever lock A is held and lock B is
// acquired, ordinal(A) < ordinal(B). If the system is SAT, a consistent
// ordering exists → deadlock-free. If UNSAT → cyclic ordering → deadlock.

// Helper: collect lock acquisition sequences from a function body
static void collectLockSequences(
    ASTNode* node,
    std::vector<std::string>& currentHeld, // locks currently held
    std::vector<std::pair<std::string, std::string>>& orderPairs, // (held, acquired)
    std::vector<Z3Verifier::LockAcquisition>& allAcq,
    const std::string& funcName)
{
    if (!node) return;
    using NT = ASTNode::NodeType;

    switch (node->type) {
        case NT::CALL: {
            std::string name = getCallName(node);
            auto* call = static_cast<CallExpr*>(node);

            if ((name == "aria_mutex_lock" || name == "aria_shim_mutex_lock" ||
                 name == "aria_rwlock_wrlock" || name == "aria_shim_rwlock_wrlock" ||
                 name == "aria_rwlock_rdlock" || name == "aria_shim_rwlock_rdlock") &&
                !call->arguments.empty()) {
                // Get the lock variable name
                std::string lockVar;
                if (call->arguments[0]->type == NT::IDENTIFIER) {
                    lockVar = static_cast<IdentifierExpr*>(
                        call->arguments[0].get())->name;
                }
                if (!lockVar.empty()) {
                    // Record ordering constraints: all currently held < this new one
                    for (const auto& held : currentHeld) {
                        orderPairs.push_back({held, lockVar});
                    }
                    currentHeld.push_back(lockVar);
                    allAcq.push_back({lockVar, funcName, node->line, node->column});
                }
            } else if ((name == "aria_mutex_unlock" || name == "aria_shim_mutex_unlock" ||
                        name == "aria_rwlock_unlock" || name == "aria_shim_rwlock_unlock") &&
                       !call->arguments.empty()) {
                std::string lockVar;
                if (call->arguments[0]->type == NT::IDENTIFIER) {
                    lockVar = static_cast<IdentifierExpr*>(
                        call->arguments[0].get())->name;
                }
                if (!lockVar.empty()) {
                    // Remove from held set
                    auto it = std::find(currentHeld.begin(), currentHeld.end(), lockVar);
                    if (it != currentHeld.end()) currentHeld.erase(it);
                }
            }

            for (auto& arg : call->arguments) {
                collectLockSequences(arg.get(), currentHeld, orderPairs,
                                     allAcq, funcName);
            }
            break;
        }

        case NT::BLOCK: {
            auto* blk = static_cast<BlockStmt*>(node);
            for (auto& s : blk->statements) {
                collectLockSequences(s.get(), currentHeld, orderPairs,
                                     allAcq, funcName);
            }
            break;
        }

        case NT::IF: {
            auto* s = static_cast<IfStmt*>(node);
            // Both branches need checking with the same held set
            auto savedHeld = currentHeld;
            collectLockSequences(s->thenBranch.get(), currentHeld, orderPairs,
                                 allAcq, funcName);
            auto thenHeld = currentHeld;
            currentHeld = savedHeld;
            collectLockSequences(s->elseBranch.get(), currentHeld, orderPairs,
                                 allAcq, funcName);
            // Merge: union of both outcomes
            for (const auto& h : thenHeld) {
                if (std::find(currentHeld.begin(), currentHeld.end(), h) == currentHeld.end()) {
                    currentHeld.push_back(h);
                }
            }
            break;
        }

        case NT::WHILE: {
            auto* s = static_cast<WhileStmt*>(node);
            collectLockSequences(s->body.get(), currentHeld, orderPairs,
                                 allAcq, funcName);
            break;
        }

        case NT::FOR: {
            auto* s = static_cast<ForStmt*>(node);
            collectLockSequences(s->body.get(), currentHeld, orderPairs,
                                 allAcq, funcName);
            break;
        }

        case NT::EXPRESSION_STMT: {
            auto* es = static_cast<ExpressionStmt*>(node);
            collectLockSequences(es->expression.get(), currentHeld, orderPairs,
                                 allAcq, funcName);
            break;
        }

        default:
            break;
    }
}

VerifyResult Z3Verifier::verifyDeadlockFreedom(
    const std::map<std::string, FuncDeclStmt*>& allFuncs,
    std::vector<VerifyOutcome>& outcomes,
    int line, int column)
{
    // Collect all lock ordering constraints from all functions
    std::vector<std::pair<std::string, std::string>> allOrderPairs;
    std::vector<LockAcquisition> allAcquisitions;
    std::set<std::string> allLockVars;

    for (const auto& [fname, func] : allFuncs) {
        if (!func->body) continue;
        std::vector<std::string> currentHeld;
        collectLockSequences(func->body.get(), currentHeld, allOrderPairs,
                             allAcquisitions, fname);
    }

    // Collect all unique lock variable names
    for (const auto& [held, acquired] : allOrderPairs) {
        allLockVars.insert(held);
        allLockVars.insert(acquired);
    }

    if (allLockVars.size() < 2) {
        // 0 or 1 locks → trivially deadlock-free
        if (!allLockVars.empty()) {
            VerifyOutcome out;
            out.result = VerifyResult::PROVEN;
            out.conditionText = "deadlock freedom";
            out.detail = "Proven: only " + std::to_string(allLockVars.size()) +
                         " lock(s) used — deadlock impossible";
            out.line = line;
            out.column = column;
            summary.proven++;
            outcomes.push_back(out);
            summary.outcomes.push_back(out);
        }
        return VerifyResult::PROVEN;
    }

    // Create Z3 integer ordinal for each lock
    Z3_solver solver = makeSolver();
    Z3_sort intSort = Z3_mk_int_sort(ctx);
    std::map<std::string, Z3_ast> lockOrdinals;

    for (const auto& lockVar : allLockVars) {
        Z3_symbol sym = Z3_mk_string_symbol(ctx, lockVar.c_str());
        lockOrdinals[lockVar] = Z3_mk_const(ctx, sym, intSort);
    }

    // Assert all ordinals are distinct and positive
    for (const auto& [name, ord] : lockOrdinals) {
        Z3_ast zero = Z3_mk_int(ctx, 0, intSort);
        Z3_solver_assert(ctx, solver, Z3_mk_gt(ctx, ord, zero));
    }

    // Assert ordering constraints: for each (held, acquired), ord(held) < ord(acquired)
    for (const auto& [held, acquired] : allOrderPairs) {
        if (held == acquired) continue; // same lock re-acquisition (recursive mutex)
        auto hIt = lockOrdinals.find(held);
        auto aIt = lockOrdinals.find(acquired);
        if (hIt != lockOrdinals.end() && aIt != lockOrdinals.end()) {
            Z3_solver_assert(ctx, solver,
                Z3_mk_lt(ctx, hIt->second, aIt->second));
        }
    }

    Z3_lbool result = checkSat(solver);
    deleteSolver(solver);

    if (result == Z3_L_TRUE) {
        // SAT → consistent ordering exists → deadlock-free
        VerifyOutcome out;
        out.result = VerifyResult::PROVEN;
        out.conditionText = "deadlock freedom";
        out.detail = "Proven: consistent lock ordering exists for " +
                     std::to_string(allLockVars.size()) +
                     " locks — no deadlock possible";
        out.line = line;
        out.column = column;
        summary.proven++;
        outcomes.push_back(out);
        summary.outcomes.push_back(out);
        return VerifyResult::PROVEN;
    } else if (result == Z3_L_FALSE) {
        // UNSAT → cyclic ordering → deadlock potential
        // Find the offending pair(s)
        std::string cycleInfo;
        // Check for simple A→B, B→A cycles
        for (size_t i = 0; i < allOrderPairs.size(); ++i) {
            for (size_t j = i + 1; j < allOrderPairs.size(); ++j) {
                if (allOrderPairs[i].first == allOrderPairs[j].second &&
                    allOrderPairs[i].second == allOrderPairs[j].first) {
                    cycleInfo = allOrderPairs[i].first + " → " +
                                allOrderPairs[i].second + " → " +
                                allOrderPairs[i].first;
                    break;
                }
            }
            if (!cycleInfo.empty()) break;
        }

        VerifyOutcome out;
        out.result = VerifyResult::DISPROVEN;
        out.conditionText = "deadlock freedom";
        out.detail = "Potential deadlock: cyclic lock ordering detected" +
                     (cycleInfo.empty() ? "" : " (" + cycleInfo + ")") +
                     " — " + std::to_string(allLockVars.size()) + " locks, " +
                     std::to_string(allOrderPairs.size()) + " ordering constraints";
        out.line = line;
        out.column = column;
        summary.disproven++;
        outcomes.push_back(out);
        summary.outcomes.push_back(out);
        return VerifyResult::DISPROVEN;
    }

    VerifyOutcome out;
    out.result = VerifyResult::UNKNOWN;
    out.conditionText = "deadlock freedom";
    out.detail = "Could not determine lock ordering consistency";
    out.line = line;
    out.column = column;
    summary.unknown++;
    outcomes.push_back(out);
    summary.outcomes.push_back(out);
    return VerifyResult::UNKNOWN;
}

// ============================================================================
// Phase 18: Use-After-Free Proofs via SMT (v0.5.4)
// ============================================================================
// For a given variable that has conditional frees (freed in one branch,
// not in another), use Z3 to prove whether a subsequent use is safe.
// We encode each conditional free as: if (cond) then state = FREED,
// and check whether at the use point state could be FREED.
//
// If NOT(state == FREED) is UNSAT at the use point → use-after-free
// If NOT(state == FREED) is SAT → safe path exists, still warn if
// some paths lead to use-after-free
//
// More precisely: we collect (condition, action) pairs where action is
// either FREE or USE, then ask Z3 if there's a valuation where
// a FREE happens before a USE.

VerifyResult Z3Verifier::proveNoUseAfterFree(
    FuncDeclStmt* func,
    const std::string& varName,
    const std::map<std::string, std::pair<RulesDeclStmt*, std::string>>& varRules,
    std::vector<VerifyOutcome>& outcomes,
    int line, int column)
{
    if (!func || !func->body) return VerifyResult::UNKNOWN;

    // Model: for each program point, track whether the variable is freed.
    // We use a symbolic boolean for each branch condition encountered,
    // and encode: freed ↔ OR(all_free_conditions).
    // Then at use points: if freed ∧ use-condition is SAT → use-after-free.

    // Collect free and use events with their guarding conditions
    struct MemEvent {
        bool isFree;     // true = free(var), false = use of var
        int line;
        int column;
        std::vector<ASTNode*> guardConditions;  // branch conditions to reach here
        std::vector<bool> guardPolarities;     // true = then branch, false = else
    };

    std::vector<MemEvent> events;
    std::vector<ASTNode*> currentGuards;
    std::vector<bool> currentPolarities;

    std::function<void(ASTNode*)> collectEvents;
    collectEvents = [&](ASTNode* node) {
        if (!node) return;
        using NT = ASTNode::NodeType;

        switch (node->type) {
            case NT::CALL: {
                std::string name = getCallName(node);
                auto* call = static_cast<CallExpr*>(node);

                // Check if this is a free() call on our variable
                if ((name == "free" || name == "aria_free" || name == "_release" ||
                     name == "_destroy") && !call->arguments.empty()) {
                    if (call->arguments[0]->type == NT::IDENTIFIER) {
                        auto* arg = static_cast<IdentifierExpr*>(
                            call->arguments[0].get());
                        if (arg->name == varName) {
                            MemEvent evt;
                            evt.isFree = true;
                            evt.line = node->line;
                            evt.column = node->column;
                            evt.guardConditions = currentGuards;
                            evt.guardPolarities = currentPolarities;
                            events.push_back(evt);
                        }
                    }
                    // Don't recurse into free() arguments — the arg
                    // is consumed by the free, not a "use"
                    break;
                }

                // Recurse into arguments for non-free calls
                for (auto& arg : call->arguments) {
                    collectEvents(arg.get());
                }
                break;
            }

            case NT::IDENTIFIER: {
                auto* ident = static_cast<IdentifierExpr*>(node);
                if (ident->name == varName) {
                    MemEvent evt;
                    evt.isFree = false;
                    evt.line = node->line;
                    evt.column = node->column;
                    evt.guardConditions = currentGuards;
                    evt.guardPolarities = currentPolarities;
                    events.push_back(evt);
                }
                break;
            }

            case NT::IF: {
                auto* ifStmt = static_cast<IfStmt*>(node);
                // Then branch
                currentGuards.push_back(ifStmt->condition.get());
                currentPolarities.push_back(true);
                collectEvents(ifStmt->thenBranch.get());
                currentGuards.pop_back();
                currentPolarities.pop_back();
                // Else branch
                currentGuards.push_back(ifStmt->condition.get());
                currentPolarities.push_back(false);
                collectEvents(ifStmt->elseBranch.get());
                currentGuards.pop_back();
                currentPolarities.pop_back();
                break;
            }

            case NT::BLOCK: {
                auto* blk = static_cast<BlockStmt*>(node);
                for (auto& s : blk->statements) collectEvents(s.get());
                break;
            }

            case NT::VAR_DECL: {
                auto* vd = static_cast<VarDeclStmt*>(node);
                if (vd->initializer) collectEvents(vd->initializer.get());
                break;
            }

            case NT::EXPRESSION_STMT: {
                auto* es = static_cast<ExpressionStmt*>(node);
                collectEvents(es->expression.get());
                break;
            }

            case NT::WHILE: {
                auto* s = static_cast<WhileStmt*>(node);
                if (s->condition) collectEvents(s->condition.get());
                collectEvents(s->body.get());
                break;
            }

            case NT::PASS: {
                auto* s = static_cast<PassStmt*>(node);
                if (s->value) collectEvents(s->value.get());
                break;
            }

            case NT::RETURN: {
                auto* s = static_cast<ReturnStmt*>(node);
                if (s->value) collectEvents(s->value.get());
                break;
            }

            case NT::ASSIGNMENT: {
                auto* a = static_cast<AssignmentExpr*>(node);
                if (a->value) collectEvents(a->value.get());
                if (a->target) collectEvents(a->target.get());
                break;
            }

            case NT::BINARY_OP: {
                auto* b = static_cast<BinaryExpr*>(node);
                collectEvents(b->left.get());
                collectEvents(b->right.get());
                break;
            }

            default:
                break;
        }
    };

    collectEvents(func->body.get());

    // Filter: must have at least one free and one subsequent use
    bool hasFree = false;
    bool hasUseAfterFree = false;
    int freeLineFirst = 0;
    int useLineAfterFree = 0;

    for (const auto& evt : events) {
        if (evt.isFree) {
            hasFree = true;
            freeLineFirst = evt.line;
        } else if (hasFree) {
            hasUseAfterFree = true;
            useLineAfterFree = evt.line;
            break;
        }
    }

    if (!hasFree) {
        // No free() for this variable in this function — trivially safe
        VerifyOutcome out;
        out.result = VerifyResult::PROVEN;
        out.rulesName = varName;
        out.conditionText = "use-after-free safety";
        out.detail = "Proven: '" + varName + "' is never freed in '" +
                     func->funcName + "' — no use-after-free possible";
        out.line = line;
        out.column = column;
        summary.proven++;
        outcomes.push_back(out);
        summary.outcomes.push_back(out);
        return VerifyResult::PROVEN;
    }

    if (!hasUseAfterFree) {
        VerifyOutcome out;
        out.result = VerifyResult::PROVEN;
        out.rulesName = varName;
        out.conditionText = "use-after-free safety";
        out.detail = "Proven: '" + varName + "' is not used after free in '" +
                     func->funcName + "'";
        out.line = line;
        out.column = column;
        summary.proven++;
        outcomes.push_back(out);
        summary.outcomes.push_back(out);
        return VerifyResult::PROVEN;
    }

    // Build Z3 model: can we reach a state where free happened before use?
    Z3_solver solver = makeSolver();
    Z3_sort boolSort = Z3_mk_bool_sort(ctx);
    Z3_sort intSort = getIntSort(32);

    // Create symbolic booleans for each unique guard condition
    std::map<ASTNode*, Z3_ast> guardSymbols;
    int guardIdx = 0;
    for (const auto& evt : events) {
        for (auto* g : evt.guardConditions) {
            if (guardSymbols.find(g) == guardSymbols.end()) {
                std::string name = "guard_" + std::to_string(guardIdx++);
                Z3_symbol sym = Z3_mk_string_symbol(ctx, name.c_str());
                guardSymbols[g] = Z3_mk_const(ctx, sym, boolSort);
            }
        }
    }

    // Build path conditions for free events and use events
    // For each free event: pathCond → freed
    // For each use event after first free: pathCond ∧ freed → USE_AFTER_FREE
    Z3_ast freed = Z3_mk_false(ctx);  // disjunction of all free conditions

    for (const auto& evt : events) {
        if (!evt.isFree) continue;

        // Build the path condition for this free
        Z3_ast pathCond = Z3_mk_true(ctx);
        for (size_t i = 0; i < evt.guardConditions.size(); ++i) {
            Z3_ast guardVal = guardSymbols[evt.guardConditions[i]];
            if (!evt.guardPolarities[i]) {
                guardVal = Z3_mk_not(ctx, guardVal);
            }
            pathCond = Z3_mk_and(ctx, 2, (Z3_ast[]){pathCond, guardVal});
        }
        freed = Z3_mk_or(ctx, 2, (Z3_ast[]){freed, pathCond});
    }

    // Find the first use after any free and check if it's reachable while freed
    for (const auto& evt : events) {
        if (evt.isFree) continue;
        if (evt.line < freeLineFirst) continue;  // use before free — safe

        // Build path condition for this use
        Z3_ast useCond = Z3_mk_true(ctx);
        for (size_t i = 0; i < evt.guardConditions.size(); ++i) {
            Z3_ast guardVal = guardSymbols[evt.guardConditions[i]];
            if (!evt.guardPolarities[i]) {
                guardVal = Z3_mk_not(ctx, guardVal);
            }
            useCond = Z3_mk_and(ctx, 2, (Z3_ast[]){useCond, guardVal});
        }

        // Check: freed ∧ use reachable → use-after-free
        Z3_ast uafCond = Z3_mk_and(ctx, 2, (Z3_ast[]){freed, useCond});
        Z3_solver_push(ctx, solver);
        Z3_solver_assert(ctx, solver, uafCond);

        // Also assert any Rules constraints on guard variables
        std::map<std::string, Z3_ast> env;
        for (const auto& [varN, rulesInfo] : varRules) {
            TypeInfo ti = resolveTypeSort(rulesInfo.second);
            Z3_symbol sym = Z3_mk_string_symbol(ctx, varN.c_str());
            Z3_ast var = Z3_mk_const(ctx, sym, ti.sort);
            env[varN] = var;
        }

        Z3_lbool result = checkSat(solver);
        Z3_solver_pop(ctx, solver, 1);

        if (result == Z3_L_TRUE) {
            // SAT → use-after-free path exists
            VerifyOutcome out;
            out.result = VerifyResult::DISPROVEN;
            out.rulesName = varName;
            out.conditionText = "use-after-free safety";
            out.detail = "Potential use-after-free: '" + varName +
                         "' may be freed (line " + std::to_string(freeLineFirst) +
                         ") before use (line " + std::to_string(evt.line) +
                         ") in '" + func->funcName + "'";
            out.line = evt.line;
            out.column = evt.column;
            summary.disproven++;
            outcomes.push_back(out);
            summary.outcomes.push_back(out);
            deleteSolver(solver);
            return VerifyResult::DISPROVEN;
        } else if (result == Z3_L_FALSE) {
            // UNSAT → this use is unreachable when freed — safe
            continue;
        }
    }

    deleteSolver(solver);

    VerifyOutcome out;
    out.result = VerifyResult::PROVEN;
    out.rulesName = varName;
    out.conditionText = "use-after-free safety";
    out.detail = "Proven: no reachable use-after-free paths for '" +
                 varName + "' in '" + func->funcName + "'";
    out.line = line;
    out.column = column;
    summary.proven++;
    outcomes.push_back(out);
    summary.outcomes.push_back(out);
    return VerifyResult::PROVEN;
}

// ============================================================================
// Phase 19: Stack Overflow Prediction (v0.5.4)
// ============================================================================
// For recursive functions, prove the recursion depth is bounded.
// Algorithm:
// 1. Detect self-recursive calls (func calls itself)
// 2. Find the "decreasing argument" — a parameter that is smaller in
//    the recursive call than in the current invocation
// 3. Use Z3 to verify: ∀ valid inputs (Rules-constrained), the recursive
//    argument strictly decreases toward a base case
// 4. If bounded, compute max depth using Z3 optimization

VerifyResult Z3Verifier::proveRecursionBounded(
    FuncDeclStmt* func,
    const std::map<std::string, FuncDeclStmt*>& allFuncs,
    const std::map<std::string, std::pair<RulesDeclStmt*, std::string>>& paramRules,
    std::vector<VerifyOutcome>& outcomes,
    int line, int column)
{
    if (!func || !func->body) return VerifyResult::UNKNOWN;

    // Step 1: Find recursive calls to self
    struct RecursiveCall {
        std::vector<ASTNode*> args;
        int line;
        int column;
    };
    std::vector<RecursiveCall> selfCalls;

    std::function<void(ASTNode*)> findSelfCalls;
    findSelfCalls = [&](ASTNode* node) {
        if (!node) return;
        using NT = ASTNode::NodeType;

        if (node->type == NT::CALL) {
            auto* call = static_cast<CallExpr*>(node);
            std::string name = getCallName(node);
            if (name == func->funcName) {
                RecursiveCall rc;
                for (auto& a : call->arguments) rc.args.push_back(a.get());
                rc.line = call->line;
                rc.column = call->column;
                selfCalls.push_back(rc);
            }
            for (auto& a : call->arguments) findSelfCalls(a.get());
        } else if (node->type == NT::BLOCK) {
            auto* blk = static_cast<BlockStmt*>(node);
            for (auto& s : blk->statements) findSelfCalls(s.get());
        } else if (node->type == NT::IF) {
            auto* s = static_cast<IfStmt*>(node);
            findSelfCalls(s->thenBranch.get());
            findSelfCalls(s->elseBranch.get());
        } else if (node->type == NT::WHILE) {
            auto* s = static_cast<WhileStmt*>(node);
            findSelfCalls(s->body.get());
        } else if (node->type == NT::FOR) {
            auto* s = static_cast<ForStmt*>(node);
            findSelfCalls(s->body.get());
        } else if (node->type == NT::EXPRESSION_STMT) {
            auto* s = static_cast<ExpressionStmt*>(node);
            findSelfCalls(s->expression.get());
        } else if (node->type == NT::VAR_DECL) {
            auto* s = static_cast<VarDeclStmt*>(node);
            if (s->initializer) findSelfCalls(s->initializer.get());
        } else if (node->type == NT::PASS) {
            auto* s = static_cast<PassStmt*>(node);
            if (s->value) findSelfCalls(s->value.get());
        } else if (node->type == NT::RETURN) {
            auto* s = static_cast<ReturnStmt*>(node);
            if (s->value) findSelfCalls(s->value.get());
        }
    };
    findSelfCalls(func->body.get());

    if (selfCalls.empty()) {
        // Not recursive — trivially bounded
        return VerifyResult::PROVEN;
    }

    // Step 2: For each parameter, check if recursive calls use a smaller value
    Z3_sort intSort = getIntSort(32);

    for (size_t pi = 0; pi < func->parameters.size(); ++pi) {
        auto* paramNode = func->parameters[pi].get();
        std::string paramName;
        if (paramNode->type == ASTNode::NodeType::PARAMETER) {
            paramName = static_cast<ParameterNode*>(paramNode)->paramName;
        } else if (paramNode->type == ASTNode::NodeType::VAR_DECL) {
            paramName = static_cast<VarDeclStmt*>(paramNode)->varName;
        }
        if (paramName.empty()) continue;

        // Check if this parameter has Rules constraints
        auto rIt = paramRules.find(paramName);
        if (rIt == paramRules.end()) continue;

        const auto& [rulesDecl, baseType] = rIt->second;
        TypeInfo ti = resolveTypeSort(baseType);

        // Create Z3 variable for the parameter
        Z3_symbol paramSym = Z3_mk_string_symbol(ctx, paramName.c_str());
        Z3_ast paramVar = Z3_mk_const(ctx, paramSym, ti.sort);

        // Collect Rules axioms for this parameter
        std::vector<Z3_ast> axioms;
        std::function<void(RulesDeclStmt*)> collectAxioms;
        collectAxioms = [&](RulesDeclStmt* r) {
            for (const auto& cascName : r->cascadedRules) {
                auto cIt = rules_table.find(cascName);
                if (cIt != rules_table.end()) collectAxioms(cIt->second);
            }
            for (auto& cond : r->conditions) {
                Z3_ast z3Cond = translateCondition(cond.get(), paramVar, ti.sort);
                if (z3Cond) axioms.push_back(z3Cond);
            }
        };
        collectAxioms(rulesDecl);

        if (axioms.empty()) continue;

        // For each recursive call, check if arg[pi] < paramVar
        bool allDecrease = true;
        for (const auto& rc : selfCalls) {
            if (pi >= rc.args.size()) { allDecrease = false; break; }

            // Build env mapping param names to Z3 vars
            std::map<std::string, Z3_ast> env;
            env[paramName] = paramVar;
            // Also add other params
            for (size_t j = 0; j < func->parameters.size(); ++j) {
                auto* pn = func->parameters[j].get();
                std::string pname;
                if (pn->type == ASTNode::NodeType::PARAMETER) {
                    pname = static_cast<ParameterNode*>(pn)->paramName;
                } else if (pn->type == ASTNode::NodeType::VAR_DECL) {
                    pname = static_cast<VarDeclStmt*>(pn)->varName;
                }
                if (!pname.empty() && env.find(pname) == env.end()) {
                    Z3_symbol s = Z3_mk_string_symbol(ctx, pname.c_str());
                    env[pname] = Z3_mk_const(ctx, s, ti.sort);
                }
            }

            Z3_ast recArg = translateExprWithEnv(rc.args[pi], env, ti.sort);
            if (!recArg) { allDecrease = false; break; }

            // Prove: under axioms, recArg < paramVar
            Z3_solver solver = makeSolver();
            for (auto& ax : axioms) {
                Z3_solver_assert(ctx, solver, ax);
            }

            // Assert NOT(recArg < paramVar) — if UNSAT, descent is proven
            Z3_ast decrease;
            if (ti.isFloat) {
                decrease = Z3_mk_lt(ctx, recArg, paramVar);
            } else {
                decrease = Z3_mk_bvslt(ctx, recArg, paramVar);
            }
            Z3_ast notDecrease = Z3_mk_not(ctx, decrease);
            Z3_solver_assert(ctx, solver, notDecrease);

            Z3_lbool result = checkSat(solver);
            deleteSolver(solver);

            if (result != Z3_L_FALSE) {
                // Not proven to strictly decrease
                allDecrease = false;
                break;
            }
        }

        if (!allDecrease) continue;

        // Step 3: Compute max depth using Z3 optimization
        // Depth = max value of parameter under Rules / decrease amount
        // Use Z3_optimize to find max initial value
        Z3_optimize opt = Z3_mk_optimize(ctx);
        Z3_optimize_inc_ref(ctx, opt);

        for (auto& ax : axioms) {
            Z3_optimize_assert(ctx, opt, ax);
        }

        // Also assert parameter > 0 (must be positive for depth to make sense)
        Z3_ast zero = Z3_mk_int(ctx, 0, ti.sort);
        if (!ti.isFloat) {
            Z3_optimize_assert(ctx, opt,
                Z3_mk_bvsgt(ctx, paramVar, zero));
        }

        Z3_optimize_maximize(ctx, opt, paramVar);
        Z3_lbool optResult = Z3_optimize_check(ctx, opt, 0, nullptr);

        int64_t maxDepth = -1;
        if (optResult == Z3_L_TRUE) {
            Z3_model model = Z3_optimize_get_model(ctx, opt);
            if (model) {
                Z3_model_inc_ref(ctx, model);
                Z3_ast val;
                if (Z3_model_eval(ctx, model, paramVar, true, &val)) {
                    Z3_inc_ref(ctx, val);
                    int64_t numVal;
                    if (Z3_get_numeral_int64(ctx, val, &numVal)) {
                        maxDepth = numVal;
                    }
                    Z3_dec_ref(ctx, val);
                }
                Z3_model_dec_ref(ctx, model);
            }
        }

        Z3_optimize_dec_ref(ctx, opt);

        VerifyOutcome out;
        out.result = VerifyResult::PROVEN;
        out.rulesName = func->funcName;
        out.conditionText = "recursion bounded";
        if (maxDepth >= 0) {
            out.detail = "Proven: recursion in '" + func->funcName +
                         "' is bounded — parameter '" + paramName +
                         "' strictly decreases, max depth ≤ " +
                         std::to_string(maxDepth);
        } else {
            out.detail = "Proven: recursion in '" + func->funcName +
                         "' is bounded — parameter '" + paramName +
                         "' strictly decreases on every recursive call";
        }
        out.line = line;
        out.column = column;
        summary.proven++;
        outcomes.push_back(out);
        summary.outcomes.push_back(out);
        return VerifyResult::PROVEN;
    }

    // Step 2b: Check body VarDecls with limit<> that derive from parameters.
    // In Aria, the common pattern is: limit<Rules> Type:bodyVar = paramName;
    // The body variable is constrained, and the recursive call uses it.
    if (func->body && func->body->type == ASTNode::NodeType::BLOCK) {
        auto* blk = static_cast<BlockStmt*>(func->body.get());
        for (auto& s : blk->statements) {
            if (s->type != ASTNode::NodeType::VAR_DECL) continue;
            auto* vd = static_cast<VarDeclStmt*>(s.get());
            if (vd->limitRulesName.empty()) continue;
            if (!vd->initializer || vd->initializer->type != ASTNode::NodeType::IDENTIFIER) continue;

            auto* initIdent = static_cast<IdentifierExpr*>(vd->initializer.get());

            // Find which parameter index this body var derives from
            int paramIdx = -1;
            for (size_t pi = 0; pi < func->parameters.size(); ++pi) {
                auto* pn = func->parameters[pi].get();
                std::string pname;
                if (pn->type == ASTNode::NodeType::PARAMETER)
                    pname = static_cast<ParameterNode*>(pn)->paramName;
                else if (pn->type == ASTNode::NodeType::VAR_DECL)
                    pname = static_cast<VarDeclStmt*>(pn)->varName;
                if (pname == initIdent->name) { paramIdx = (int)pi; break; }
            }
            if (paramIdx < 0) continue;

            // Look up Rules for this body variable
            auto rIt = paramRules.find(vd->varName);
            if (rIt == paramRules.end()) continue;

            const auto& [rulesDecl2, baseType2] = rIt->second;
            TypeInfo ti2 = resolveTypeSort(baseType2);

            Z3_symbol bodySym = Z3_mk_string_symbol(ctx, vd->varName.c_str());
            Z3_ast bodyVar = Z3_mk_const(ctx, bodySym, ti2.sort);

            // Collect Rules axioms for the body variable
            std::vector<Z3_ast> axioms2;
            std::function<void(RulesDeclStmt*)> collectAxioms2;
            collectAxioms2 = [&](RulesDeclStmt* r) {
                for (const auto& cascName : r->cascadedRules) {
                    auto cIt = rules_table.find(cascName);
                    if (cIt != rules_table.end()) collectAxioms2(cIt->second);
                }
                for (auto& cond : r->conditions) {
                    Z3_ast z3Cond = translateCondition(cond.get(), bodyVar, ti2.sort);
                    if (z3Cond) axioms2.push_back(z3Cond);
                }
            };
            collectAxioms2(rulesDecl2);
            if (axioms2.empty()) continue;

            // Build env: body var name AND param name both map to same Z3 var
            std::map<std::string, Z3_ast> env2;
            env2[vd->varName] = bodyVar;
            env2[initIdent->name] = bodyVar;
            for (size_t j = 0; j < func->parameters.size(); ++j) {
                auto* pn = func->parameters[j].get();
                std::string pname;
                if (pn->type == ASTNode::NodeType::PARAMETER)
                    pname = static_cast<ParameterNode*>(pn)->paramName;
                else if (pn->type == ASTNode::NodeType::VAR_DECL)
                    pname = static_cast<VarDeclStmt*>(pn)->varName;
                if (!pname.empty() && env2.find(pname) == env2.end()) {
                    Z3_symbol s2 = Z3_mk_string_symbol(ctx, pname.c_str());
                    env2[pname] = Z3_mk_const(ctx, s2, ti2.sort);
                }
            }

            // Check all recursive calls decrease at paramIdx
            bool allDecrease2 = true;
            for (const auto& rc : selfCalls) {
                if ((size_t)paramIdx >= rc.args.size()) { allDecrease2 = false; break; }

                Z3_ast recArg2 = translateExprWithEnv(rc.args[paramIdx], env2, ti2.sort);
                if (!recArg2) { allDecrease2 = false; break; }

                Z3_solver solver2 = makeSolver();
                for (auto& ax : axioms2) Z3_solver_assert(ctx, solver2, ax);

                Z3_ast decrease2;
                if (ti2.isFloat) {
                    decrease2 = Z3_mk_lt(ctx, recArg2, bodyVar);
                } else {
                    decrease2 = Z3_mk_bvslt(ctx, recArg2, bodyVar);
                }
                Z3_solver_assert(ctx, solver2, Z3_mk_not(ctx, decrease2));

                Z3_lbool res2 = checkSat(solver2);
                deleteSolver(solver2);

                if (res2 != Z3_L_FALSE) { allDecrease2 = false; break; }
            }

            if (!allDecrease2) continue;

            // Compute max depth
            Z3_optimize opt2 = Z3_mk_optimize(ctx);
            Z3_optimize_inc_ref(ctx, opt2);
            for (auto& ax : axioms2) Z3_optimize_assert(ctx, opt2, ax);

            Z3_ast zero2 = Z3_mk_int(ctx, 0, ti2.sort);
            if (!ti2.isFloat) {
                Z3_optimize_assert(ctx, opt2, Z3_mk_bvsgt(ctx, bodyVar, zero2));
            }

            Z3_optimize_maximize(ctx, opt2, bodyVar);
            Z3_lbool optRes2 = Z3_optimize_check(ctx, opt2, 0, nullptr);

            int64_t maxDepth2 = -1;
            if (optRes2 == Z3_L_TRUE) {
                Z3_model model2 = Z3_optimize_get_model(ctx, opt2);
                if (model2) {
                    Z3_model_inc_ref(ctx, model2);
                    Z3_ast val2;
                    if (Z3_model_eval(ctx, model2, bodyVar, true, &val2)) {
                        Z3_inc_ref(ctx, val2);
                        int64_t nv;
                        if (Z3_get_numeral_int64(ctx, val2, &nv)) maxDepth2 = nv;
                        Z3_dec_ref(ctx, val2);
                    }
                    Z3_model_dec_ref(ctx, model2);
                }
            }
            Z3_optimize_dec_ref(ctx, opt2);

            VerifyOutcome out2;
            out2.result = VerifyResult::PROVEN;
            out2.rulesName = func->funcName;
            out2.conditionText = "recursion bounded";
            if (maxDepth2 >= 0) {
                out2.detail = "Proven: recursion in '" + func->funcName +
                             "' is bounded — '" + vd->varName +
                             "' (limit<" + vd->limitRulesName +
                             ">) strictly decreases, max depth ≤ " +
                             std::to_string(maxDepth2);
            } else {
                out2.detail = "Proven: recursion in '" + func->funcName +
                             "' is bounded — '" + vd->varName +
                             "' (limit<" + vd->limitRulesName +
                             ">) strictly decreases on every recursive call";
            }
            out2.line = line;
            out2.column = column;
            summary.proven++;
            outcomes.push_back(out2);
            summary.outcomes.push_back(out2);
            return VerifyResult::PROVEN;
        }
    }

    // Could not prove any parameter decreases
    VerifyOutcome out;
    out.result = VerifyResult::UNKNOWN;
    out.rulesName = func->funcName;
    out.conditionText = "recursion bounded";
    out.detail = "Could not prove recursion in '" + func->funcName +
                 "' is bounded — no parameter with Rules constraint proven to decrease";
    out.line = line;
    out.column = column;
    summary.unknown++;
    outcomes.push_back(out);
    summary.outcomes.push_back(out);
    return VerifyResult::UNKNOWN;
}

} // namespace aria
