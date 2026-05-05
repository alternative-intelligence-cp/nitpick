// type_checker_internal.h — Shared helpers for type_checker split files (v0.8.2)
// These were static functions in the monolithic type_checker.cpp. They are now
// inline in this header so that all translation units can use them.
#pragma once

#include "frontend/sema/type.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace npk {
namespace sema {
namespace tc_helpers {

// Levenshtein distance for typo detection (find similar names)
inline size_t levenshteinDistance(const std::string& a, const std::string& b) {
    const size_t m = a.size();
    const size_t n = b.size();

    if (m == 0) return n;
    if (n == 0) return m;

    std::vector<std::vector<size_t>> dp(m + 1, std::vector<size_t>(n + 1));

    for (size_t i = 0; i <= m; ++i) dp[i][0] = i;
    for (size_t j = 0; j <= n; ++j) dp[0][j] = j;

    for (size_t i = 1; i <= m; ++i) {
        for (size_t j = 1; j <= n; ++j) {
            size_t cost = (a[i-1] == b[j-1]) ? 0 : 1;
            dp[i][j] = std::min({
                dp[i-1][j] + 1,
                dp[i][j-1] + 1,
                dp[i-1][j-1] + cost
            });
        }
    }

    return dp[m][n];
}

// Method registry for common types
inline const std::unordered_map<std::string, std::vector<std::string>>& getTypeMethodRegistry() {
    static const std::unordered_map<std::string, std::vector<std::string>> registry = {
        {"string", {
            "from_char", "from_cstr", "from_int",
            "length", "is_empty", "equals",
            "contains", "starts_with", "ends_with", "substring",
            "concat", "trim", "to_upper", "to_lower",
            "to_int", "to_hex", "pad_right", "format_float"
        }},
        {"array", {"new", "length", "push", "get", "set", "pop"}},
        {"map", {"new", "length", "insert", "get", "has", "remove"}},
        {"File", {
            "open", "read_line", "read_bytes", "write", "write_bytes",
            "close", "eof", "flush", "seek", "tell",
            "read", "write_all", "delete", "exists", "size"
        }},
        {"tbb8", {"add", "sub", "mul", "div", "mod", "neg", "abs", "is_err", "to_int"}},
        {"tbb16", {"add", "sub", "mul", "div", "mod", "neg", "abs", "is_err", "to_int"}},
        {"tbb32", {"add", "sub", "mul", "div", "mod", "neg", "abs", "is_err", "to_int"}},
        {"tbb64", {"add", "sub", "mul", "div", "mod", "neg", "abs", "is_err", "to_int"}},
    };
    return registry;
}

// Find similar method name for "Did you mean X?" suggestions
inline std::string findSimilarMethod(const std::string& typeName, const std::string& methodName) {
    const auto& registry = getTypeMethodRegistry();
    auto it = registry.find(typeName);
    if (it == registry.end()) return "";

    const std::vector<std::string>& methods = it->second;
    std::string bestMatch;
    size_t bestDistance = SIZE_MAX;
    size_t maxDistance = std::max(size_t(3), methodName.length() * 2 / 5);

    for (const std::string& method : methods) {
        size_t dist = levenshteinDistance(methodName, method);
        if (dist < bestDistance && dist <= maxDistance) {
            bestDistance = dist;
            bestMatch = method;
        }
    }
    return bestMatch;
}

// Get formatted list of available methods for a type
inline std::string getAvailableMethods(const std::string& typeName) {
    const auto& registry = getTypeMethodRegistry();
    auto it = registry.find(typeName);
    if (it == registry.end()) return "";

    const std::vector<std::string>& methods = it->second;
    if (methods.empty()) return "";

    std::string result;
    for (size_t i = 0; i < methods.size(); ++i) {
        if (i > 0) result += ", ";
        result += methods[i];
    }
    return result;
}

// Find similar type name for "Did you mean?" suggestions on unknown types
inline std::string findSimilarType(const std::string& typeName) {
    static const std::vector<std::string> knownTypes = {
        "int1", "int2", "int4", "int8", "int16", "int32", "int64", "int128", "int256", "int512",
        "uint1", "uint2", "uint4", "uint8", "uint16", "uint32", "uint64", "uint128", "uint256", "uint512",
        "tbb8", "tbb16", "tbb32", "tbb64",
        "flt32", "flt64", "flt128", "flt256",
        "bool", "string", "void", "NIL", "obj", "dyn", "any",
        "array", "map", "Result", "option",
        "vec2", "vec3", "vec9", "dvec2", "dvec3", "dvec9",
        "fvec2", "fvec3", "fvec9", "ivec2", "ivec3", "ivec9"
    };

    std::string bestMatch;
    size_t bestDistance = SIZE_MAX;
    size_t maxDistance = std::max(size_t(3), typeName.length() * 2 / 5);

    std::string lowerInput = typeName;
    std::transform(lowerInput.begin(), lowerInput.end(), lowerInput.begin(), ::tolower);
    for (const std::string& known : knownTypes) {
        std::string lowerKnown = known;
        std::transform(lowerKnown.begin(), lowerKnown.end(), lowerKnown.begin(), ::tolower);
        if (lowerInput == lowerKnown && typeName != known) {
            return known;
        }
    }

    for (const std::string& known : knownTypes) {
        size_t dist = levenshteinDistance(typeName, known);
        if (dist < bestDistance && dist <= maxDistance) {
            bestDistance = dist;
            bestMatch = known;
        }
    }
    return bestMatch;
}

// Helper to check if a string is a type keyword (for UFCS disambiguation)
inline bool isTypeKeyword(const std::string& name) {
    static const std::unordered_set<std::string> typeKeywords = {
        "int1", "int2", "int4", "int8", "int16", "int32", "int64", "int128", "int256", "int512",
        "uint1", "uint2", "uint4", "uint8", "uint16", "uint32", "uint64", "uint128", "uint256", "uint512",
        "tbb8", "tbb16", "tbb32", "tbb64",
        "flt32", "flt64", "flt128", "flt256",
        "bool", "string", "void", "NIL", "obj", "dyn", "any",
        "array", "map", "Result", "option"
    };
    return typeKeywords.count(name) > 0;
}

// Helper to check if a type is compatible with atomic<T>
inline bool isAtomicCompatible(Type* t) {
    if (!t || t->getKind() != TypeKind::PRIMITIVE) return false;
    std::string name = t->toString();
    static const std::unordered_set<std::string> compatibleTypes = {
        "int8", "int16", "int32", "int64",
        "uint8", "uint16", "uint32", "uint64",
        "bool",
        "tbb8", "tbb16", "tbb32", "tbb64"
    };
    return compatibleTypes.count(name) > 0;
}

// Convert type suffix (u32, i64, f32, etc.) to type system name (uint32, int64, flt32, etc.)
inline std::string typeSuffixToSystemName(const std::string& suffix) {
    if (suffix[0] == 'u' && suffix.length() >= 2) return "uint" + suffix.substr(1);
    if (suffix[0] == 'i' && suffix.length() >= 2) return "int" + suffix.substr(1);
    if (suffix[0] == 'f' && suffix.length() >= 2) return "flt" + suffix.substr(1);
    if (suffix.substr(0, 3) == "tbb") return suffix;
    if (suffix.substr(0, 3) == "fix") return suffix;
    if (suffix == "trit" || suffix == "tryte" || suffix == "nit" || suffix == "nyte") return suffix;
    return suffix;
}

// Check if integer value fits in target type without loss
inline bool integerFitsInType(int64_t value, const std::string& type_name) {
    if (type_name == "int8")    return value >= -128 && value <= 127;
    if (type_name == "int16")   return value >= -32768 && value <= 32767;
    if (type_name == "int32")   return value >= -2147483648LL && value <= 2147483647LL;
    if (type_name == "int64")   return true;
    if (type_name == "int128" || type_name == "int256" || 
        type_name == "int512" || type_name == "int1024" ||
        type_name == "int2048" || type_name == "int4096") return true;
    if (type_name == "uint8")   return value >= 0 && value <= 255;
    if (type_name == "uint16")  return value >= 0 && value <= 65535;
    if (type_name == "uint32")  return value >= 0 && value <= 4294967295LL;
    if (type_name == "uint64")  return value >= 0;
    if (type_name == "uint128" || type_name == "uint256" || 
        type_name == "uint512" || type_name == "uint1024" ||
        type_name == "uint2048" || type_name == "uint4096") return value >= 0;
    if (type_name == "tbb8")    return value >= -128 && value <= 127;
    if (type_name == "tbb16")   return value >= -32768 && value <= 32767;
    if (type_name == "tbb32")   return value >= -2147483648LL && value <= 2147483647LL;
    if (type_name == "tbb64")   return true;
    return false;
}

} // namespace tc_helpers
} // namespace sema
} // namespace npk
