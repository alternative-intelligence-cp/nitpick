#ifndef ARIA_SEMA_EXHAUSTIVENESS_H
#define ARIA_SEMA_EXHAUSTIVENESS_H

#include <set>
#include <vector>
#include <string>
#include "frontend/sema/type.h"
#include "frontend/ast/stmt.h"  // For PickStmt and PickCase

namespace aria {
namespace sema {

/**
 * Represents a range of values in a domain
 * Used for coverage analysis of pick statements
 */
struct ValueRange {
    int64_t min;
    int64_t max;
    
    ValueRange(int64_t min_val, int64_t max_val) 
        : min(min_val), max(max_val) {}
    
    bool contains(int64_t value) const {
        return value >= min && value <= max;
    }
    
    bool overlaps(const ValueRange& other) const {
        return !(max < other.min || min > other.max);
    }
    
    // Merge with another range (union)
    ValueRange merge(const ValueRange& other) const {
        return ValueRange(
            std::min(min, other.min),
            std::max(max, other.max)
        );
    }
};

/**
 * Represents the complete domain of possible values for a type
 */
class TypeDomain {
public:
    enum class Kind {
        INTEGER_RANGE,  // Finite integer range (TBB, int8, etc.)
        BOOLEAN,        // true, false
        ENUM,           // Finite set of enum variants
        INFINITE,       // Infinite domain (int64, string, etc.)
        UNKNOWN         // Domain cannot be determined
    };
    
private:
    Kind kind;
    std::vector<ValueRange> ranges;     // For INTEGER_RANGE
    std::set<std::string> symbols;      // For ENUM (variant names)
    bool hasERR;                         // For TBB types
    
public:
    TypeDomain(Kind k = Kind::UNKNOWN) 
        : kind(k), hasERR(false) {}
    
    Kind getKind() const { return kind; }
    
    // Factory methods
    static TypeDomain forBoolean() {
        TypeDomain domain(Kind::BOOLEAN);
        domain.symbols.insert("true");
        domain.symbols.insert("false");
        return domain;
    }
    
    static TypeDomain forTBB8() {
        TypeDomain domain(Kind::INTEGER_RANGE);
        domain.ranges.push_back(ValueRange(-127, 127));
        domain.hasERR = true;  // ERR is -128 (min i8)
        return domain;
    }
    
    static TypeDomain forTBB16() {
        TypeDomain domain(Kind::INTEGER_RANGE);
        domain.ranges.push_back(ValueRange(-32767, 32767));
        domain.hasERR = true;  // ERR is -32768 (min i16)
        return domain;
    }
    
    static TypeDomain forTBB32() {
        TypeDomain domain(Kind::INTEGER_RANGE);
        domain.ranges.push_back(ValueRange(-2147483647, 2147483647));
        domain.hasERR = true;  // ERR is -2147483648 (min i32)
        return domain;
    }
    
    static TypeDomain forTBB64() {
        TypeDomain domain(Kind::INTEGER_RANGE);
        domain.ranges.push_back(ValueRange(-9223372036854775807LL, 9223372036854775807LL));
        domain.hasERR = true;  // ERR is min i64
        return domain;
    }
    
    static TypeDomain forEnum(const std::set<std::string>& variants) {
        TypeDomain domain(Kind::ENUM);
        domain.symbols = variants;
        return domain;
    }
    
    static TypeDomain infinite() {
        return TypeDomain(Kind::INFINITE);
    }
    
    const std::vector<ValueRange>& getRanges() const { return ranges; }
    const std::set<std::string>& getSymbols() const { return symbols; }
    bool requiresERR() const { return hasERR; }
    
    // Coverage tracking
    bool isEmpty() const {
        if (kind == Kind::BOOLEAN) return symbols.empty();
        if (kind == Kind::ENUM) return symbols.empty();
        if (kind == Kind::INTEGER_RANGE) return ranges.empty();
        return false;
    }
    
    size_t size() const {
        if (kind == Kind::BOOLEAN) return 2;
        if (kind == Kind::ENUM) return symbols.size();
        if (kind == Kind::INTEGER_RANGE) {
            size_t total = 0;
            for (const auto& range : ranges) {
                total += (range.max - range.min + 1);
            }
            if (hasERR) total += 1;  // ERR counts as one value
            return total;
        }
        return 0;  // INFINITE or UNKNOWN
    }
};

/**
 * Tracks which values have been covered by pick cases
 */
class CoverageSet {
private:
    std::vector<ValueRange> covered_ranges;
    std::set<std::string> covered_symbols;
    bool hasERRCase;
    bool hasDefaultCase;
    
public:
    CoverageSet() : hasERRCase(false), hasDefaultCase(false) {}
    
    void addRange(int64_t min, int64_t max) {
        covered_ranges.push_back(ValueRange(min, max));
    }
    
    void addSymbol(const std::string& symbol) {
        covered_symbols.insert(symbol);
    }
    
    void addERR() {
        hasERRCase = true;
    }
    
    void addDefault() {
        hasDefaultCase = true;
    }
    
    bool coversERR() const { return hasERRCase || hasDefaultCase; }
    bool hasDefault() const { return hasDefaultCase; }
    
    // Check if domain is fully covered
    bool isExhaustive(const TypeDomain& domain) const;
    
    // Get missing values/symbols
    std::vector<ValueRange> getMissingRanges(const TypeDomain& domain) const;
    std::set<std::string> getMissingSymbols(const TypeDomain& domain) const;
};

/**
 * Analyzes pick statements for exhaustiveness
 */
class ExhaustivenessAnalyzer {
public:
    struct Analysis {
        bool isExhaustive;
        bool missingERR;
        std::vector<ValueRange> missingRanges;
        std::set<std::string> missingSymbols;
        std::string errorMessage;
    };
    
    /**
     * Analyze a pick statement for exhaustiveness
     * 
     * @param pickStmt The pick statement to analyze
     * @param selectorType The type of the selector expression
     * @return Analysis result with missing cases
     */
    static Analysis analyze(PickStmt* pickStmt, Type* selectorType);
    
private:
    // Determine the domain of a type
    static TypeDomain getDomain(Type* type);
    
    // Extract coverage from pick cases
    static CoverageSet extractCoverage(PickStmt* pickStmt, const TypeDomain& domain);
    
    // Parse a case pattern into coverage
    static void analyzePattern(ASTNode* pattern, CoverageSet& coverage, const TypeDomain& domain);
    
    // Generate helpful error message
    static std::string generateErrorMessage(const Analysis& analysis, const TypeDomain& domain);
};

} // namespace sema
} // namespace aria

#endif // ARIA_SEMA_EXHAUSTIVENESS_H
