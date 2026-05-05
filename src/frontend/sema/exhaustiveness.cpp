#include "frontend/sema/exhaustiveness.h"
#include "frontend/ast/stmt.h"
#include "frontend/ast/expr.h"
#include <algorithm>
#include <sstream>

namespace npk {
namespace sema {

// ============================================================================
// CoverageSet Implementation
// ============================================================================

bool CoverageSet::isExhaustive(const TypeDomain& domain) const {
    // Default case covers everything
    if (hasDefaultCase) {
        return true;
    }
    
    switch (domain.getKind()) {
        case TypeDomain::Kind::BOOLEAN:
            return covered_symbols.size() == 2 &&
                   covered_symbols.count("true") > 0 &&
                   covered_symbols.count("false") > 0;
        
        case TypeDomain::Kind::ENUM:
            // All symbols must be covered
            for (const auto& symbol : domain.getSymbols()) {
                if (covered_symbols.find(symbol) == covered_symbols.end()) {
                    return false;
                }
            }
            return true;
        
        case TypeDomain::Kind::INTEGER_RANGE: {
            // Check if all ranges are covered
            std::vector<ValueRange> missing = getMissingRanges(domain);
            bool rangesCovered = missing.empty();
            
            // For TBB types, ERR must also be covered
            if (domain.requiresERR()) {
                return rangesCovered && hasERRCase;
            }
            return rangesCovered;
        }
        
        case TypeDomain::Kind::INFINITE:
            // Infinite domains require default case
            return hasDefaultCase;
        
        case TypeDomain::Kind::UNKNOWN:
            // Cannot determine exhaustiveness
            return false;
    }
    
    return false;
}

std::vector<ValueRange> CoverageSet::getMissingRanges(const TypeDomain& domain) const {
    if (domain.getKind() != TypeDomain::Kind::INTEGER_RANGE) {
        return {};
    }
    
    std::vector<ValueRange> missing;
    
    for (const auto& domainRange : domain.getRanges()) {
        int64_t current = domainRange.min;
        
        // Sort covered ranges by min value
        auto sorted_covered = covered_ranges;
        std::sort(sorted_covered.begin(), sorted_covered.end(),
                 [](const ValueRange& a, const ValueRange& b) {
                     return a.min < b.min;
                 });
        
        // Find gaps in coverage
        for (const auto& covered : sorted_covered) {
            if (covered.max < domainRange.min || covered.min > domainRange.max) {
                continue;  // Outside domain range
            }
            
            int64_t coverStart = std::max(covered.min, domainRange.min);
            
            if (current < coverStart) {
                // Gap found
                missing.push_back(ValueRange(current, coverStart - 1));
            }
            
            // Guard against overflow when covered.max == INT64_MAX
            if (covered.max < INT64_MAX) {
                current = std::max(current, covered.max + 1);
            } else {
                current = INT64_MAX;
            }
        }
        
        // Check if anything remains uncovered at the end
        if (current <= domainRange.max) {
            missing.push_back(ValueRange(current, domainRange.max));
        }
    }
    
    return missing;
}

std::set<std::string> CoverageSet::getMissingSymbols(const TypeDomain& domain) const {
    std::set<std::string> missing;
    
    if (domain.getKind() == TypeDomain::Kind::BOOLEAN) {
        if (covered_symbols.find("true") == covered_symbols.end()) {
            missing.insert("true");
        }
        if (covered_symbols.find("false") == covered_symbols.end()) {
            missing.insert("false");
        }
    } else if (domain.getKind() == TypeDomain::Kind::ENUM) {
        for (const auto& symbol : domain.getSymbols()) {
            if (covered_symbols.find(symbol) == covered_symbols.end()) {
                missing.insert(symbol);
            }
        }
    }
    
    return missing;
}

// ============================================================================
// ExhaustivenessAnalyzer Implementation
// ============================================================================

ExhaustivenessAnalyzer::Analysis ExhaustivenessAnalyzer::analyze(
    PickStmt* pickStmt, Type* selectorType) {
    
    Analysis result;
    result.isExhaustive = false;
    result.missingERR = false;
    result.missingUnknown = false;
    
    // Determine the domain of the selector type
    TypeDomain domain = getDomain(selectorType);
    
    // Cannot check exhaustiveness for unknown or infinite domains
    if (domain.getKind() == TypeDomain::Kind::UNKNOWN) {
        result.isExhaustive = true;  // Assume exhaustive if we can't check
        return result;
    }
    
    if (domain.getKind() == TypeDomain::Kind::INFINITE) {
        // Infinite domains must have a default case
        for (const auto& caseNode : pickStmt->cases) {
            PickCase* pickCase = static_cast<PickCase*>(caseNode.get());
            
            // Check for unreachable (!)
            if (pickCase->is_unreachable) {
                result.isExhaustive = true;
                return result;
            }
            
            // Check for wildcard (*) - represented as string literal "*"
            if (pickCase->pattern && pickCase->pattern->type == ASTNode::NodeType::LITERAL) {
                LiteralExpr* lit = static_cast<LiteralExpr*>(pickCase->pattern.get());
                if (std::holds_alternative<std::string>(lit->value)) {
                    std::string strVal = std::get<std::string>(lit->value);
                    if (strVal == "*") {
                        result.isExhaustive = true;
                        return result;
                    }
                }
            }
        }
        result.errorMessage = "Non-exhaustive pick statement. Infinite domain requires default case (*).";
        return result;
    }
    
    // Extract coverage from cases
    CoverageSet coverage = extractCoverage(pickStmt, domain);
    
    // Check if exhaustive
    result.isExhaustive = coverage.isExhaustive(domain);
    
    if (!result.isExhaustive) {
        // Find what's missing
        result.missingRanges = coverage.getMissingRanges(domain);
        result.missingSymbols = coverage.getMissingSymbols(domain);
        
        // Check for missing ERR in TBB types
        if (domain.requiresERR() && !coverage.coversERR()) {
            result.missingERR = true;
        }
        
        // Check for missing unknown (types that can be indeterminate)
        if (domain.needsUnknown() && !coverage.coversUnknown()) {
            result.missingUnknown = true;
        }
        
        result.errorMessage = generateErrorMessage(result, domain);
    }
    
    return result;
}

TypeDomain ExhaustivenessAnalyzer::getDomain(Type* type) {
    if (!type) {
        return TypeDomain::infinite();
    }
    
    // Primitive types
    if (type->getKind() == TypeKind::PRIMITIVE) {
        PrimitiveType* primType = static_cast<PrimitiveType*>(type);
        const std::string& name = primType->getName();
        
        // Boolean
        if (name == "bool") {
            return TypeDomain::forBoolean();
        }
        
        // Standard integers with finite domains (small types)
        if (name == "int8") return TypeDomain::forInt8();
        if (name == "uint8") return TypeDomain::forUInt8();
        if (name == "int16") return TypeDomain::forInt16();
        if (name == "uint16") return TypeDomain::forUInt16();
        
        // TBB types
        if (name == "tbb8") return TypeDomain::forTBB8();
        if (name == "tbb16") return TypeDomain::forTBB16();
        if (name == "tbb32") return TypeDomain::forTBB32();
        if (name == "tbb64") return TypeDomain::forTBB64();
        
        // Fixed-size integers have finite domains but are large
        // Treat as infinite for practical purposes
        return TypeDomain::infinite();
    }
    
    // Enum types have finite, enumerable domains (v0.2.39)
    if (type->getKind() == TypeKind::ENUM) {
        EnumType* enumType = static_cast<EnumType*>(type);
        std::set<std::string> variantNames;
        for (const auto& [name, value] : enumType->getVariants()) {
            variantNames.insert(name);
        }
        return TypeDomain::forEnum(variantNames);
    }
    
    // All other types are infinite
    return TypeDomain::infinite();
}

CoverageSet ExhaustivenessAnalyzer::extractCoverage(
    PickStmt* pickStmt, const TypeDomain& domain) {
    
    CoverageSet coverage;
    
    for (const auto& caseNode : pickStmt->cases) {
        PickCase* pickCase = static_cast<PickCase*>(caseNode.get());
        
        // Check for unreachable case (!)
        if (pickCase->is_unreachable) {
            continue;  // Unreachable doesn't contribute to coverage
        }
        
        analyzePattern(pickCase->pattern.get(), coverage, domain);
    }
    
    return coverage;
}

// Helper: Extract integer value from pattern (handles negative numbers)
static bool extractIntValue(ASTNode* node, int64_t& outValue) {
    if (!node) return false;
    
    // Direct literal: 5, 127, etc.
    if (node->type == ASTNode::NodeType::LITERAL) {
        LiteralExpr* lit = static_cast<LiteralExpr*>(node);
        if (std::holds_alternative<int64_t>(lit->value)) {
            outValue = std::get<int64_t>(lit->value);
            return true;
        }
        return false;
    }
    
    // Negative literal: -5, -127, etc.
    if (node->type == ASTNode::NodeType::UNARY_OP) {
        UnaryExpr* unary = static_cast<UnaryExpr*>(node);
        if (unary->op.lexeme == "-" &&
            unary->operand->type == ASTNode::NodeType::LITERAL) {
            
            LiteralExpr* lit = static_cast<LiteralExpr*>(unary->operand.get());
            if (std::holds_alternative<int64_t>(lit->value)) {
                outValue = -std::get<int64_t>(lit->value);
                return true;
            }
        }
        return false;
    }
    
    return false;
}

void ExhaustivenessAnalyzer::analyzePattern(
    ASTNode* pattern, CoverageSet& coverage, const TypeDomain& domain) {
    
    if (!pattern) return;
    
    switch (pattern->type) {
        case ASTNode::NodeType::IDENTIFIER: {
            // Could be: true, false, enum variant, ERR, or unknown
            IdentifierExpr* ident = static_cast<IdentifierExpr*>(pattern);
            
            
            if (ident->name == "ERR") {
                coverage.addERR();
            } else if (ident->name == "unknown") {
                coverage.addUnknown();
            } else {
                coverage.addSymbol(ident->name);
            }
            break;
        }
        
        case ASTNode::NodeType::LITERAL: {
            // Single value literal or wildcard
            LiteralExpr* lit = static_cast<LiteralExpr*>(pattern);
            
            // Check if it's ERR or unknown sentinel (parsed as string literal)
            if (std::holds_alternative<std::string>(lit->value)) {
                std::string strVal = std::get<std::string>(lit->value);
                
                if (strVal == "ERR") {
                    coverage.addERR();
                } else if (strVal == "unknown") {
                    coverage.addUnknown();
                } else if (strVal == "*") {
                    // Wildcard (*) covers everything
                    coverage.addDefault();
                } else {
                    // Enum variant or other symbolic pattern
                    coverage.addSymbol(strVal);
                }
            } else if (std::holds_alternative<int64_t>(lit->value)) {
                int64_t value = std::get<int64_t>(lit->value);
                coverage.addRange(value, value);
            } else if (std::holds_alternative<bool>(lit->value)) {
                bool boolValue = std::get<bool>(lit->value);
                coverage.addSymbol(boolValue ? "true" : "false");
            }
            break;
        }
        
        case ASTNode::NodeType::RANGE: {
            // Range pattern: 0..10, -5..5, -127..-1, etc.
            RangeExpr* rangeExpr = static_cast<RangeExpr*>(pattern);
            
            // Extract min and max from range (handles negative numbers)
            int64_t min, max;
            if (extractIntValue(rangeExpr->start.get(), min) &&
                extractIntValue(rangeExpr->end.get(), max)) {
                coverage.addRange(min, max);
            }
            break;
        }
        
        case ASTNode::NodeType::BINARY_OP: {
            // Range pattern: 0..10, -5..5, -127..-1, etc.
            // NOTE: This might be legacy - RANGE NodeType is preferred
            BinaryExpr* binExpr = static_cast<BinaryExpr*>(pattern);
            
            
            if (binExpr->op.lexeme == "..") {
                // Extract min and max from range (handles negative numbers)
                int64_t min, max;
                if (extractIntValue(binExpr->left.get(), min) &&
                    extractIntValue(binExpr->right.get(), max)) {
                    coverage.addRange(min, max);
                } else {
                }
            }
            break;
        }
        
        case ASTNode::NodeType::UNARY_OP: {
            // Negative literal: -5, -127, etc. (single value, not a range)
            int64_t value;
            if (extractIntValue(pattern, value)) {
                coverage.addRange(value, value);
            }
            break;
        }
        
        case ASTNode::NodeType::MEMBER_ACCESS: {
            // v0.2.39: Enum variant pattern: Color.RED, Direction.NORTH, etc.
            MemberAccessExpr* member = static_cast<MemberAccessExpr*>(pattern);
            if (domain.getKind() == TypeDomain::Kind::ENUM) {
                // For enum patterns, add the variant name as a covered symbol
                coverage.addSymbol(member->member);
            } else {
                // For non-enum domains, try to resolve as integer constant
                // (enum used in integer pick context)
                if (member->object->type == ASTNode::NodeType::IDENTIFIER) {
                    IdentifierExpr* ident = static_cast<IdentifierExpr*>(member->object.get());
                    std::string fullName = ident->name + "." + member->member;
                    // Add as symbol since we can't resolve the value here
                    coverage.addSymbol(fullName);
                }
            }
            break;
        }
        
        default:
            // Unknown pattern type - conservatively assume it doesn't cover anything
            break;
    }
}

std::string ExhaustivenessAnalyzer::generateErrorMessage(
    const Analysis& analysis, const TypeDomain& domain) {
    
    std::ostringstream msg;
    msg << "Non-exhaustive pick statement. Missing cases: ";
    
    bool needsSeparator = false;
    
    // Report missing ERR first (HIGHEST priority - safety critical)
    if (analysis.missingERR) {
        msg << "ERR";
        needsSeparator = true;
    }
    
    // Report missing unknown second (requires explicit handling)
    if (analysis.missingUnknown) {
        if (needsSeparator) msg << ", ";
        msg << "unknown";
        needsSeparator = true;
    }
    
    // Report missing symbols
    if (!analysis.missingSymbols.empty()) {
        if (needsSeparator) msg << ", ";
        
        bool first = true;
        for (const auto& symbol : analysis.missingSymbols) {
            if (!first) msg << ", ";
            msg << symbol;
            first = false;
        }
        needsSeparator = true;
    }
    
    // Report missing ranges (sample a few values)
    if (!analysis.missingRanges.empty()) {
        if (needsSeparator) msg << ", ";
        
        // Show first few ranges
        int count = 0;
        for (const auto& range : analysis.missingRanges) {
            if (count > 0) msg << ", ";
            
            if (range.min == range.max) {
                msg << range.min;
            } else if (range.max - range.min < 10) {
                // Small range: show all values
                msg << range.min << ".." << range.max;
            } else {
                // Large range: show range notation
                msg << range.min << ".." << range.max;
            }
            
            count++;
            if (count >= 3) {
                msg << ", ...";
                break;
            }
        }
    }
    
    // Special message for TBB types
    if (domain.requiresERR() && analysis.missingERR) {
        msg << " (TBB type requires ERR handling)";
    }
    
    return msg.str();
}

} // namespace sema
} // namespace npk
