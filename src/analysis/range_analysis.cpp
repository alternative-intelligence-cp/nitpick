// ============================================================================
// RangeAnalyzer — Intraprocedural range inference for Aria (v0.14.1)
//
// Walks a function's AST body and infers [lo, hi] integer ranges per variable.
// Produces synthetic RulesDeclStmt objects that plug into the existing Z3
// elimination passes (dead branch, bounds check, overflow, null check).
//
// Supported:
//   - Literal assignments: int32:x = 42 → [42, 42]
//   - Arithmetic: y = x + 5 → [x.lo+5, x.hi+5]
//   - Conditional narrowing: if (x > 10) → [11, x.hi] in then-branch
//   - Loop induction: loop(0, 100, 1) → $ ∈ [0, 99]
//   - Variable copies: y = x → y inherits x's range
//
// Conservative:
//   - Function calls → full type range
//   - Complex expressions → full type range
//   - Pointer operations → skipped
// ============================================================================

#include "analysis/range_analysis.h"
#include "frontend/ast/stmt.h"
#include "frontend/ast/expr.h"
#include "frontend/token.h"
#include <algorithm>
#include <climits>

namespace aria {

// ============================================================================
// Type-width helpers
// ============================================================================

InferredRange RangeAnalyzer::fullRange(const std::string& typeName) {
    if (typeName == "int8")   return {INT8_MIN, INT8_MAX, typeName};
    if (typeName == "int16")  return {INT16_MIN, INT16_MAX, typeName};
    if (typeName == "int32")  return {INT32_MIN, INT32_MAX, typeName};
    if (typeName == "int64")  return {INT64_MIN, INT64_MAX, typeName};
    if (typeName == "uint8")  return {0, UINT8_MAX, typeName};
    if (typeName == "uint16") return {0, UINT16_MAX, typeName};
    if (typeName == "uint32") return {0, static_cast<int64_t>(UINT32_MAX), typeName};
    if (typeName == "uint64") return {0, INT64_MAX, typeName}; // capped to int64 max
    if (typeName == "tbb8")   return {-121, 121, typeName};
    if (typeName == "tbb16")  return {-29524, 29524, typeName};
    if (typeName == "tbb32")  return {INT32_MIN, INT32_MAX, typeName};
    if (typeName == "tbb64")  return {INT64_MIN, INT64_MAX, typeName};
    // Default: 32-bit signed
    return {INT32_MIN, INT32_MAX, typeName.empty() ? "int32" : typeName};
}

bool RangeAnalyzer::addOverflows(int64_t a, int64_t b) {
    if (b > 0 && a > INT64_MAX - b) return true;
    if (b < 0 && a < INT64_MIN - b) return true;
    return false;
}

bool RangeAnalyzer::subOverflows(int64_t a, int64_t b) {
    if (b < 0 && a > INT64_MAX + b) return true;
    if (b > 0 && a < INT64_MIN + b) return true;
    return false;
}

bool RangeAnalyzer::mulOverflows(int64_t a, int64_t b) {
    if (a == 0 || b == 0) return false;
    if (a > 0 && b > 0 && a > INT64_MAX / b) return true;
    if (a < 0 && b < 0 && a < INT64_MAX / b) return true;
    if (a > 0 && b < 0 && b < INT64_MIN / a) return true;
    if (a < 0 && b > 0 && a < INT64_MIN / b) return true;
    return false;
}

// ============================================================================
// Constant evaluation
// ============================================================================

std::pair<int64_t, bool> RangeAnalyzer::evalConstant(ASTNode* node, const RangeMap& ranges) {
    if (!node) return {0, false};

    if (node->type == ASTNode::NodeType::LITERAL) {
        auto* lit = static_cast<LiteralExpr*>(node);
        if (std::holds_alternative<int64_t>(lit->value)) {
            return {std::get<int64_t>(lit->value), true};
        }
        if (std::holds_alternative<bool>(lit->value)) {
            return {std::get<bool>(lit->value) ? 1 : 0, true};
        }
        return {0, false};
    }

    if (node->type == ASTNode::NodeType::IDENTIFIER) {
        auto* ident = static_cast<IdentifierExpr*>(node);
        auto it = ranges.find(ident->name);
        if (it != ranges.end() && it->second.valid && it->second.lo == it->second.hi) {
            return {it->second.lo, true};
        }
        return {0, false};
    }

    if (node->type == ASTNode::NodeType::UNARY_OP) {
        auto* unary = static_cast<UnaryExpr*>(node);
        if (unary->op.type == frontend::TokenType::TOKEN_MINUS) {
            auto [val, ok] = evalConstant(unary->operand.get(), ranges);
            if (ok && val != INT64_MIN) return {-val, true};
        }
        return {0, false};
    }

    if (node->type == ASTNode::NodeType::BINARY_OP) {
        auto* bin = static_cast<BinaryExpr*>(node);
        auto [lv, lok] = evalConstant(bin->left.get(), ranges);
        auto [rv, rok] = evalConstant(bin->right.get(), ranges);
        if (lok && rok) {
            switch (bin->op.type) {
                case frontend::TokenType::TOKEN_PLUS:
                    if (!addOverflows(lv, rv)) return {lv + rv, true};
                    break;
                case frontend::TokenType::TOKEN_MINUS:
                    if (!subOverflows(lv, rv)) return {lv - rv, true};
                    break;
                case frontend::TokenType::TOKEN_STAR:
                    if (!mulOverflows(lv, rv)) return {lv * rv, true};
                    break;
                case frontend::TokenType::TOKEN_SLASH:
                    if (rv != 0) return {lv / rv, true};
                    break;
                case frontend::TokenType::TOKEN_PERCENT:
                    if (rv != 0) return {lv % rv, true};
                    break;
                default:
                    break;
            }
        }
    }

    return {0, false};
}

// ============================================================================
// Expression range evaluation
// ============================================================================

InferredRange RangeAnalyzer::evalExprRange(ASTNode* node, const RangeMap& ranges,
                                            const std::string& defaultType) {
    if (!node) return fullRange(defaultType);

    // Literal → exact range
    if (node->type == ASTNode::NodeType::LITERAL) {
        auto [val, ok] = evalConstant(node, ranges);
        if (ok) return {val, val, defaultType};
        return fullRange(defaultType);
    }

    // Identifier → look up known range
    if (node->type == ASTNode::NodeType::IDENTIFIER) {
        auto* ident = static_cast<IdentifierExpr*>(node);
        auto it = ranges.find(ident->name);
        if (it != ranges.end() && it->second.valid) {
            return it->second;
        }
        return fullRange(defaultType);
    }

    // Unary minus: -x → [-x.hi, -x.lo]
    if (node->type == ASTNode::NodeType::UNARY_OP) {
        auto* unary = static_cast<UnaryExpr*>(node);
        if (unary->op.type == frontend::TokenType::TOKEN_MINUS) {
            auto inner = evalExprRange(unary->operand.get(), ranges, defaultType);
            if (inner.valid && inner.lo != INT64_MIN && inner.hi != INT64_MIN) {
                return {-inner.hi, -inner.lo, defaultType};
            }
        }
        return fullRange(defaultType);
    }

    // Binary operations: compute range from operand ranges
    if (node->type == ASTNode::NodeType::BINARY_OP) {
        auto* bin = static_cast<BinaryExpr*>(node);
        auto lhs = evalExprRange(bin->left.get(), ranges, defaultType);
        auto rhs = evalExprRange(bin->right.get(), ranges, defaultType);

        if (!lhs.valid || !rhs.valid) return fullRange(defaultType);

        switch (bin->op.type) {
            case frontend::TokenType::TOKEN_PLUS: {
                if (!addOverflows(lhs.lo, rhs.lo) && !addOverflows(lhs.hi, rhs.hi)) {
                    return {lhs.lo + rhs.lo, lhs.hi + rhs.hi, defaultType};
                }
                return fullRange(defaultType);
            }
            case frontend::TokenType::TOKEN_MINUS: {
                if (!subOverflows(lhs.lo, rhs.hi) && !subOverflows(lhs.hi, rhs.lo)) {
                    return {lhs.lo - rhs.hi, lhs.hi - rhs.lo, defaultType};
                }
                return fullRange(defaultType);
            }
            case frontend::TokenType::TOKEN_STAR: {
                // For multiplication, need to check all 4 corner combinations
                int64_t a = lhs.lo, b = lhs.hi, c = rhs.lo, d = rhs.hi;
                if (mulOverflows(a, c) || mulOverflows(a, d) ||
                    mulOverflows(b, c) || mulOverflows(b, d)) {
                    return fullRange(defaultType);
                }
                int64_t ac = a * c, ad = a * d, bc = b * c, bd = b * d;
                int64_t lo = std::min({ac, ad, bc, bd});
                int64_t hi = std::max({ac, ad, bc, bd});
                return {lo, hi, defaultType};
            }
            case frontend::TokenType::TOKEN_SLASH: {
                // Division: conservative — if divisor range includes 0, give up
                if (rhs.lo <= 0 && rhs.hi >= 0) return fullRange(defaultType);
                // All-positive or all-negative divisor
                int64_t a = lhs.lo, b = lhs.hi, c = rhs.lo, d = rhs.hi;
                int64_t ac = a / c, ad = a / d, bc = b / c, bd = b / d;
                int64_t lo = std::min({ac, ad, bc, bd});
                int64_t hi = std::max({ac, ad, bc, bd});
                return {lo, hi, defaultType};
            }
            case frontend::TokenType::TOKEN_PERCENT: {
                // Modulo: result is in [0, |divisor|-1] for positive, more complex otherwise
                if (rhs.lo > 0) {
                    return {0, rhs.hi - 1, defaultType};
                }
                if (rhs.hi < 0) {
                    return {rhs.lo + 1, 0, defaultType};
                }
                return fullRange(defaultType);
            }
            default:
                break;
        }
    }

    return fullRange(defaultType);
}

// ============================================================================
// Conditional narrowing
// ============================================================================

void RangeAnalyzer::narrowFromCondition(ASTNode* condition, RangeMap& ranges, bool condIsTrue) {
    if (!condition) return;

    // Handle logical NOT: narrow with inverted truth
    if (condition->type == ASTNode::NodeType::UNARY_OP) {
        auto* unary = static_cast<UnaryExpr*>(condition);
        if (unary->op.type == frontend::TokenType::TOKEN_BANG) {
            narrowFromCondition(unary->operand.get(), ranges, !condIsTrue);
            return;
        }
    }

    // Handle logical AND: both sides are true
    if (condition->type == ASTNode::NodeType::BINARY_OP) {
        auto* bin = static_cast<BinaryExpr*>(condition);

        if (bin->op.type == frontend::TokenType::TOKEN_AND_AND && condIsTrue) {
            narrowFromCondition(bin->left.get(), ranges, true);
            narrowFromCondition(bin->right.get(), ranges, true);
            return;
        }

        // Handle logical OR: both sides are false (when !condIsTrue via De Morgan)
        if (bin->op.type == frontend::TokenType::TOKEN_OR_OR && !condIsTrue) {
            narrowFromCondition(bin->left.get(), ranges, false);
            narrowFromCondition(bin->right.get(), ranges, false);
            return;
        }

        // Handle comparison: var <op> constant  or  constant <op> var
        ASTNode* varSide = nullptr;
        ASTNode* constSide = nullptr;
        bool varOnLeft = false;

        if (bin->left->type == ASTNode::NodeType::IDENTIFIER) {
            auto [val, ok] = evalConstant(bin->right.get(), ranges);
            if (ok) {
                varSide = bin->left.get();
                constSide = bin->right.get();
                varOnLeft = true;
            }
        }
        if (!varSide && bin->right->type == ASTNode::NodeType::IDENTIFIER) {
            auto [val, ok] = evalConstant(bin->left.get(), ranges);
            if (ok) {
                varSide = bin->right.get();
                constSide = bin->left.get();
                varOnLeft = false;
            }
        }

        if (!varSide) return;

        auto* ident = static_cast<IdentifierExpr*>(varSide);
        auto [constVal, _] = evalConstant(constSide, ranges);

        auto it = ranges.find(ident->name);
        if (it == ranges.end() || !it->second.valid) return;

        int64_t lo = it->second.lo;
        int64_t hi = it->second.hi;
        std::string type = it->second.typeName;

        // Normalize: we want "var <op> const" form
        // If var is on right, flip the operator
        auto opType = bin->op.type;
        if (!varOnLeft) {
            switch (opType) {
                case frontend::TokenType::TOKEN_LESS:          opType = frontend::TokenType::TOKEN_GREATER; break;
                case frontend::TokenType::TOKEN_LESS_EQUAL:    opType = frontend::TokenType::TOKEN_GREATER_EQUAL; break;
                case frontend::TokenType::TOKEN_GREATER:       opType = frontend::TokenType::TOKEN_LESS; break;
                case frontend::TokenType::TOKEN_GREATER_EQUAL: opType = frontend::TokenType::TOKEN_LESS_EQUAL; break;
                default: break;
            }
        }

        // If condition is false, invert the operator
        if (!condIsTrue) {
            switch (opType) {
                case frontend::TokenType::TOKEN_LESS:          opType = frontend::TokenType::TOKEN_GREATER_EQUAL; break;
                case frontend::TokenType::TOKEN_LESS_EQUAL:    opType = frontend::TokenType::TOKEN_GREATER; break;
                case frontend::TokenType::TOKEN_GREATER:       opType = frontend::TokenType::TOKEN_LESS_EQUAL; break;
                case frontend::TokenType::TOKEN_GREATER_EQUAL: opType = frontend::TokenType::TOKEN_LESS; break;
                case frontend::TokenType::TOKEN_EQUAL_EQUAL:   opType = frontend::TokenType::TOKEN_BANG_EQUAL; break;
                case frontend::TokenType::TOKEN_BANG_EQUAL:    opType = frontend::TokenType::TOKEN_EQUAL_EQUAL; break;
                default: break;
            }
        }

        // Apply narrowing
        switch (opType) {
            case frontend::TokenType::TOKEN_LESS:
                // var < const → hi = min(hi, const-1)
                if (constVal - 1 < hi && constVal > INT64_MIN)
                    hi = constVal - 1;
                break;
            case frontend::TokenType::TOKEN_LESS_EQUAL:
                // var <= const → hi = min(hi, const)
                if (constVal < hi)
                    hi = constVal;
                break;
            case frontend::TokenType::TOKEN_GREATER:
                // var > const → lo = max(lo, const+1)
                if (constVal + 1 > lo && constVal < INT64_MAX)
                    lo = constVal + 1;
                break;
            case frontend::TokenType::TOKEN_GREATER_EQUAL:
                // var >= const → lo = max(lo, const)
                if (constVal > lo)
                    lo = constVal;
                break;
            case frontend::TokenType::TOKEN_EQUAL_EQUAL:
                // var == const → exact
                lo = constVal;
                hi = constVal;
                break;
            case frontend::TokenType::TOKEN_BANG_EQUAL:
                // var != const → if at boundary, shrink by 1
                if (lo == constVal && lo < INT64_MAX) lo = constVal + 1;
                else if (hi == constVal && hi > INT64_MIN) hi = constVal - 1;
                break;
            default:
                break;
        }

        if (lo <= hi) {
            ranges[ident->name] = {lo, hi, type};
        }
    }
}

// ============================================================================
// Sub-expression range recording
// ============================================================================

void RangeAnalyzer::recordSubExprs(ASTNode* node, const RangeMap& ranges) {
    if (!node) return;
    nodeRanges[node] = ranges;

    using NT = ASTNode::NodeType;

    switch (node->type) {
        case NT::BINARY_OP: {
            auto* bin = static_cast<BinaryExpr*>(node);
            recordSubExprs(bin->left.get(), ranges);
            recordSubExprs(bin->right.get(), ranges);
            break;
        }
        case NT::UNARY_OP: {
            auto* unary = static_cast<UnaryExpr*>(node);
            recordSubExprs(unary->operand.get(), ranges);
            break;
        }
        case NT::INDEX: {
            auto* idx = static_cast<IndexExpr*>(node);
            recordSubExprs(idx->array.get(), ranges);
            recordSubExprs(idx->index.get(), ranges);
            break;
        }
        case NT::CALL: {
            auto* call = static_cast<CallExpr*>(node);
            if (call->callee) recordSubExprs(call->callee.get(), ranges);
            for (auto& arg : call->arguments) recordSubExprs(arg.get(), ranges);
            break;
        }
        case NT::UNWRAP: {
            auto* unwrap = static_cast<UnwrapExpr*>(node);
            recordSubExprs(unwrap->result.get(), ranges);
            recordSubExprs(unwrap->defaultValue.get(), ranges);
            break;
        }
        case NT::ASSIGNMENT: {
            auto* assign = static_cast<AssignmentExpr*>(node);
            recordSubExprs(assign->target.get(), ranges);
            recordSubExprs(assign->value.get(), ranges);
            break;
        }
        case NT::TERNARY: {
            auto* tern = static_cast<TernaryExpr*>(node);
            recordSubExprs(tern->condition.get(), ranges);
            recordSubExprs(tern->trueValue.get(), ranges);
            recordSubExprs(tern->falseValue.get(), ranges);
            break;
        }
        case NT::MEMBER_ACCESS:
        case NT::POINTER_MEMBER: {
            auto* mem = static_cast<MemberAccessExpr*>(node);
            recordSubExprs(mem->object.get(), ranges);
            break;
        }
        case NT::CAST: {
            auto* cast = static_cast<CastExpr*>(node);
            recordSubExprs(cast->expression.get(), ranges);
            break;
        }
        default:
            // Leaf nodes (LITERAL, IDENTIFIER) or unhandled — already recorded above
            break;
    }
}

// ============================================================================
// Statement walker
// ============================================================================

void RangeAnalyzer::walkStmt(ASTNode* node, RangeMap& ranges) {
    if (!node) return;

    using NT = ASTNode::NodeType;

    switch (node->type) {
    case NT::BLOCK: {
        nodeRanges[node] = ranges;
        auto* block = static_cast<BlockStmt*>(node);
        for (auto& stmt : block->statements) {
            walkStmt(stmt.get(), ranges);
        }
        break;
    }

    case NT::VAR_DECL: {
        nodeRanges[node] = ranges;
        auto* var = static_cast<VarDeclStmt*>(node);
        std::string typeName = var->typeName;
        if (typeName.empty() && var->typeNode) {
            typeName = var->typeNode->toString();
        }

        // Record ranges at initializer sub-expressions BEFORE updating ranges
        if (var->initializer) {
            recordSubExprs(var->initializer.get(), ranges);
        }

        if (var->initializer) {
            auto range = evalExprRange(var->initializer.get(), ranges, typeName);
            if (range.valid) {
                range.typeName = typeName.empty() ? range.typeName : typeName;
                ranges[var->varName] = range;
            } else {
                ranges[var->varName] = fullRange(typeName);
            }
        } else {
            ranges[var->varName] = fullRange(typeName);
        }
        break;
    }

    case NT::EXPRESSION_STMT: {
        nodeRanges[node] = ranges;
        auto* exprStmt = static_cast<ExpressionStmt*>(node);
        // Record ranges at the expression tree
        if (exprStmt->expression) {
            recordSubExprs(exprStmt->expression.get(), ranges);
        }
        if (exprStmt->expression &&
            exprStmt->expression->type == NT::ASSIGNMENT) {
            auto* assign = static_cast<AssignmentExpr*>(exprStmt->expression.get());
            if (assign->target && assign->target->type == NT::IDENTIFIER) {
                auto* ident = static_cast<IdentifierExpr*>(assign->target.get());
                auto it = ranges.find(ident->name);
                std::string typeName = (it != ranges.end()) ? it->second.typeName : "int32";

                if (assign->op.type == frontend::TokenType::TOKEN_EQUAL) {
                    auto range = evalExprRange(assign->value.get(), ranges, typeName);
                    if (range.valid) {
                        ranges[ident->name] = range;
                    }
                } else if (assign->op.type == frontend::TokenType::TOKEN_PLUS_EQUAL) {
                    if (it != ranges.end() && it->second.valid) {
                        auto rhs = evalExprRange(assign->value.get(), ranges, typeName);
                        if (rhs.valid && !addOverflows(it->second.lo, rhs.lo) &&
                            !addOverflows(it->second.hi, rhs.hi)) {
                            ranges[ident->name] = {it->second.lo + rhs.lo,
                                                    it->second.hi + rhs.hi, typeName};
                        } else {
                            ranges[ident->name] = fullRange(typeName);
                        }
                    }
                } else if (assign->op.type == frontend::TokenType::TOKEN_MINUS_EQUAL) {
                    if (it != ranges.end() && it->second.valid) {
                        auto rhs = evalExprRange(assign->value.get(), ranges, typeName);
                        if (rhs.valid && !subOverflows(it->second.lo, rhs.hi) &&
                            !subOverflows(it->second.hi, rhs.lo)) {
                            ranges[ident->name] = {it->second.lo - rhs.hi,
                                                    it->second.hi - rhs.lo, typeName};
                        } else {
                            ranges[ident->name] = fullRange(typeName);
                        }
                    }
                } else if (assign->op.type == frontend::TokenType::TOKEN_STAR_EQUAL) {
                    // Conservative: widen to full range
                    if (it != ranges.end()) {
                        ranges[ident->name] = fullRange(typeName);
                    }
                }
            }
        }
        break;
    }

    case NT::IF: {
        nodeRanges[node] = ranges;
        auto* ifStmt = static_cast<IfStmt*>(node);

        // Record ranges at condition sub-expressions
        recordSubExprs(ifStmt->condition.get(), ranges);

        // Walk then-branch with narrowed ranges
        if (ifStmt->thenBranch) {
            auto thenRanges = ranges;
            narrowFromCondition(ifStmt->condition.get(), thenRanges, true);
            walkStmt(ifStmt->thenBranch.get(), thenRanges);

            // Merge then-branch ranges back (widen: take union)
            for (auto& [name, range] : thenRanges) {
                auto it = ranges.find(name);
                if (it != ranges.end() && it->second.valid && range.valid) {
                    ranges[name] = {std::min(it->second.lo, range.lo),
                                    std::max(it->second.hi, range.hi),
                                    range.typeName};
                }
            }
        }

        // Walk else-branch with inversely narrowed ranges
        if (ifStmt->elseBranch) {
            auto elseRanges = ranges;
            narrowFromCondition(ifStmt->condition.get(), elseRanges, false);
            walkStmt(ifStmt->elseBranch.get(), elseRanges);

            // Merge else-branch ranges back (widen: take union)
            for (auto& [name, range] : elseRanges) {
                auto it = ranges.find(name);
                if (it != ranges.end() && it->second.valid && range.valid) {
                    ranges[name] = {std::min(it->second.lo, range.lo),
                                    std::max(it->second.hi, range.hi),
                                    range.typeName};
                }
            }
        }
        break;
    }

    case NT::LOOP: {
        nodeRanges[node] = ranges;
        // loop(start, limit, step) { body }
        // The induction variable $ ranges from start to limit-step
        auto* loop = static_cast<LoopStmt*>(node);

        // Record ranges at loop control expressions
        recordSubExprs(loop->start.get(), ranges);
        recordSubExprs(loop->limit.get(), ranges);
        if (loop->step) recordSubExprs(loop->step.get(), ranges);

        auto [startVal, startOk] = evalConstant(loop->start.get(), ranges);
        auto [limitVal, limitOk] = evalConstant(loop->limit.get(), ranges);

        if (startOk && limitOk) {
            // $ ∈ [min(start, limit-1), max(start, limit-1)]
            int64_t lo = std::min(startVal, limitVal);
            int64_t hi = std::max(startVal, limitVal);
            // The loop variable doesn't reach 'limit' itself (exclusive)
            if (hi == limitVal && hi > lo) hi--;
            ranges["$"] = {lo, hi, "int64"};
        }

        // Walk body with loop variable range set
        if (loop->body) {
            walkStmt(loop->body.get(), ranges);
        }
        break;
    }

    case NT::TILL: {
        nodeRanges[node] = ranges;
        // till(limit, step) { body } — $ counts from 0 to limit-1
        auto* till = static_cast<TillStmt*>(node);

        recordSubExprs(till->limit.get(), ranges);
        if (till->step) recordSubExprs(till->step.get(), ranges);

        auto [limitVal, limitOk] = evalConstant(till->limit.get(), ranges);

        if (limitOk && limitVal > 0) {
            ranges["$"] = {0, limitVal - 1, "int64"};
        }

        if (till->body) {
            walkStmt(till->body.get(), ranges);
        }
        break;
    }

    case NT::FOR: {
        nodeRanges[node] = ranges;
        auto* forStmt = static_cast<ForStmt*>(node);

        // Walk initializer
        if (forStmt->initializer) {
            walkStmt(forStmt->initializer.get(), ranges);
        }

        // Record ranges at condition and update expressions
        if (forStmt->condition) recordSubExprs(forStmt->condition.get(), ranges);
        if (forStmt->update) recordSubExprs(forStmt->update.get(), ranges);

        // If we can determine the induction variable's range from init + condition:
        // Pattern: for (int32:i = start; i < limit; i += step)
        if (forStmt->initializer && forStmt->condition &&
            forStmt->initializer->type == NT::VAR_DECL) {
            auto* initVar = static_cast<VarDeclStmt*>(forStmt->initializer.get());
            auto [startVal, startOk] = evalConstant(initVar->initializer.get(), ranges);

            if (startOk && forStmt->condition->type == NT::BINARY_OP) {
                auto* cond = static_cast<BinaryExpr*>(forStmt->condition.get());
                // Check if condition is: i < limit or i <= limit
                if (cond->left->type == NT::IDENTIFIER) {
                    auto* condVar = static_cast<IdentifierExpr*>(cond->left.get());
                    if (condVar->name == initVar->varName) {
                        auto [limitVal, limitOk] = evalConstant(cond->right.get(), ranges);
                        if (limitOk) {
                            if (cond->op.type == frontend::TokenType::TOKEN_LESS) {
                                ranges[initVar->varName] = {startVal, limitVal - 1,
                                    initVar->typeName.empty() ? "int32" : initVar->typeName};
                            } else if (cond->op.type == frontend::TokenType::TOKEN_LESS_EQUAL) {
                                ranges[initVar->varName] = {startVal, limitVal,
                                    initVar->typeName.empty() ? "int32" : initVar->typeName};
                            } else if (cond->op.type == frontend::TokenType::TOKEN_GREATER) {
                                ranges[initVar->varName] = {limitVal + 1, startVal,
                                    initVar->typeName.empty() ? "int32" : initVar->typeName};
                            } else if (cond->op.type == frontend::TokenType::TOKEN_GREATER_EQUAL) {
                                ranges[initVar->varName] = {limitVal, startVal,
                                    initVar->typeName.empty() ? "int32" : initVar->typeName};
                            }
                        }
                    }
                }
            }
        }

        // Walk body
        if (forStmt->body) {
            walkStmt(forStmt->body.get(), ranges);
        }
        break;
    }

    case NT::WHILE: {
        nodeRanges[node] = ranges;
        auto* whileStmt = static_cast<WhileStmt*>(node);

        // Record ranges at condition
        recordSubExprs(whileStmt->condition.get(), ranges);

        // Narrow inside body based on loop condition
        if (whileStmt->body) {
            auto bodyRanges = ranges;
            narrowFromCondition(whileStmt->condition.get(), bodyRanges, true);
            walkStmt(whileStmt->body.get(), bodyRanges);
        }
        break;
    }

    case NT::RETURN:
    case NT::PASS: {
        nodeRanges[node] = ranges;
        // Record ranges at the return value expression
        if (node->type == NT::RETURN) {
            auto* ret = static_cast<ReturnStmt*>(node);
            if (ret->value) recordSubExprs(ret->value.get(), ranges);
        } else {
            auto* pass = static_cast<PassStmt*>(node);
            if (pass->value) recordSubExprs(pass->value.get(), ranges);
        }
        break;
    }

    default:
        // Other statement types — record snapshot but no range updates
        nodeRanges[node] = ranges;
        break;
    }
}

// ============================================================================
// Public API
// ============================================================================

RangeMap RangeAnalyzer::analyzeFunction(FuncDeclStmt* func) {
    RangeMap ranges;
    if (!func) return ranges;

    // Initialize parameter ranges from their types
    for (auto& param : func->parameters) {
        auto* p = static_cast<ParameterNode*>(param.get());
        if (!p) continue;
        std::string typeName = p->typeNode ? p->typeNode->toString() : "int32";
        ranges[p->paramName] = fullRange(typeName);
    }

    // Walk the function body
    if (func->body) {
        walkStmt(func->body.get(), ranges);
    }

    return ranges;
}

VarRulesMap RangeAnalyzer::toVarRules(const RangeMap& ranges) {
    VarRulesMap result;

    for (auto& [varName, range] : ranges) {
        if (!range.valid) continue;
        // Skip if range is full-width (no useful constraint)
        auto full = fullRange(range.typeName);
        if (range.lo <= full.lo && range.hi >= full.hi) continue;

        // Create synthetic RulesDeclStmt with $ >= lo AND $ <= hi
        std::vector<ASTNodePtr> conditions;

        // $ >= lo
        {
            auto dollar = std::make_shared<IdentifierExpr>("$");
            auto loLit = std::make_shared<LiteralExpr>(range.lo);
            frontend::Token geOp(frontend::TokenType::TOKEN_GREATER_EQUAL, ">=", 0, 0);
            conditions.push_back(std::make_shared<BinaryExpr>(
                std::move(dollar), geOp, std::move(loLit)));
        }

        // $ <= hi
        {
            auto dollar = std::make_shared<IdentifierExpr>("$");
            auto hiLit = std::make_shared<LiteralExpr>(range.hi);
            frontend::Token leOp(frontend::TokenType::TOKEN_LESS_EQUAL, "<=", 0, 0);
            conditions.push_back(std::make_shared<BinaryExpr>(
                std::move(dollar), leOp, std::move(hiLit)));
        }

        std::string syntheticName = "_inferred_" + varName;
        auto syntheticRule = std::make_unique<RulesDeclStmt>(
            syntheticName,
            std::vector<std::string>{},  // no type params
            std::move(conditions),
            std::vector<std::string>{}   // no cascaded rules
        );

        result[varName] = {syntheticRule.get(), range.typeName};
        syntheticRules.push_back(std::move(syntheticRule));
    }

    return result;
}

// ============================================================================
// Per-node range lookup
// ============================================================================

const RangeMap* RangeAnalyzer::getRangesAt(ASTNode* node) const {
    if (!node) return nullptr;
    auto it = nodeRanges.find(node);
    return (it != nodeRanges.end()) ? &it->second : nullptr;
}

// ============================================================================
// Merge inferred ranges into VarRulesMap for elimination passes
// ============================================================================

void RangeAnalyzer::mergeInferred(VarRulesMap& target, const RangeMap& ranges) {
    for (const auto& [varName, range] : ranges) {
        // Explicit Rules take priority — skip if already in target
        if (target.find(varName) != target.end()) continue;
        if (!range.valid) continue;

        // Skip if range is full-width (no useful constraint)
        auto full = fullRange(range.typeName);
        if (range.lo <= full.lo && range.hi >= full.hi) continue;

        // Create synthetic RulesDeclStmt with $ >= lo AND $ <= hi
        std::vector<ASTNodePtr> conditions;

        // $ >= lo
        {
            auto dollar = std::make_shared<IdentifierExpr>("$");
            auto loLit = std::make_shared<LiteralExpr>(range.lo);
            frontend::Token geOp(frontend::TokenType::TOKEN_GREATER_EQUAL, ">=", 0, 0);
            conditions.push_back(std::make_shared<BinaryExpr>(
                std::move(dollar), geOp, std::move(loLit)));
        }

        // $ <= hi
        {
            auto dollar = std::make_shared<IdentifierExpr>("$");
            auto hiLit = std::make_shared<LiteralExpr>(range.hi);
            frontend::Token leOp(frontend::TokenType::TOKEN_LESS_EQUAL, "<=", 0, 0);
            conditions.push_back(std::make_shared<BinaryExpr>(
                std::move(dollar), leOp, std::move(hiLit)));
        }

        std::string syntheticName = "_inferred_" + varName;
        auto syntheticRule = std::make_unique<RulesDeclStmt>(
            syntheticName,
            std::vector<std::string>{},  // no type params
            std::move(conditions),
            std::vector<std::string>{}   // no cascaded rules
        );

        target[varName] = {syntheticRule.get(), range.typeName};
        syntheticRules.push_back(std::move(syntheticRule));
    }
}

} // namespace aria
