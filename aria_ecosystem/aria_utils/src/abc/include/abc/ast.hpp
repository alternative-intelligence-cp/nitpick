/**
 * ast.hpp
 * Abstract Syntax Tree for Aria Build Configuration (ABC) Parser
 *
 * Defines the AST node hierarchy for build.aria files:
 * - ValueNode: Base for all value types (string, object, array, bool, int)
 * - ObjectNode: Key-value maps preserving insertion order
 * - ArrayNode: Ordered lists of values
 * - BuildFileNode: Root node with project, variables, targets sections
 *
 * Uses std::variant for type-safe value representation and
 * preserves source locations for error reporting.
 *
 * Copyright (c) 2025 Aria Language Project
 */

#ifndef ARIA_ABC_AST_HPP
#define ARIA_ABC_AST_HPP

#include "token.hpp"
#include <string>
#include <string_view>
#include <vector>
#include <memory>
#include <variant>
#include <optional>

namespace aria {
namespace abc {

// Forward declarations
struct ObjectNode;
struct ArrayNode;

// -----------------------------------------------------------------------------
// Value Type - Type-safe union of all possible values
// -----------------------------------------------------------------------------
using Value = std::variant<
    std::string,                        // String literal
    int64_t,                            // Integer literal
    bool,                               // Boolean literal
    std::unique_ptr<ObjectNode>,        // Nested object
    std::unique_ptr<ArrayNode>          // Array
>;

// -----------------------------------------------------------------------------
// AST Node Base
// -----------------------------------------------------------------------------
struct ASTNode {
    SourceLocation loc;

    explicit ASTNode(SourceLocation l = {}) : loc(l) {}
    virtual ~ASTNode() = default;
};

// -----------------------------------------------------------------------------
// Value Node - Wraps a Value with source location
// -----------------------------------------------------------------------------
struct ValueNode : public ASTNode {
    Value value;

    ValueNode() = default;
    ValueNode(Value v, SourceLocation l = {})
        : ASTNode(l), value(std::move(v)) {}

    // Type checking helpers
    bool is_string() const { return std::holds_alternative<std::string>(value); }
    bool is_integer() const { return std::holds_alternative<int64_t>(value); }
    bool is_bool() const { return std::holds_alternative<bool>(value); }
    bool is_object() const { return std::holds_alternative<std::unique_ptr<ObjectNode>>(value); }
    bool is_array() const { return std::holds_alternative<std::unique_ptr<ArrayNode>>(value); }

    // Type-safe accessors (throw if wrong type)
    const std::string& as_string() const { return std::get<std::string>(value); }
    std::string& as_string() { return std::get<std::string>(value); }
    int64_t as_integer() const { return std::get<int64_t>(value); }
    bool as_bool() const { return std::get<bool>(value); }
    const ObjectNode& as_object() const { return *std::get<std::unique_ptr<ObjectNode>>(value); }
    ObjectNode& as_object() { return *std::get<std::unique_ptr<ObjectNode>>(value); }
    const ArrayNode& as_array() const { return *std::get<std::unique_ptr<ArrayNode>>(value); }
    ArrayNode& as_array() { return *std::get<std::unique_ptr<ArrayNode>>(value); }

    // Optional accessors (return nullptr/nullopt if wrong type)
    const std::string* try_string() const {
        return std::holds_alternative<std::string>(value) ? &std::get<std::string>(value) : nullptr;
    }
    const ObjectNode* try_object() const {
        return is_object() ? std::get<std::unique_ptr<ObjectNode>>(value).get() : nullptr;
    }
    const ArrayNode* try_array() const {
        return is_array() ? std::get<std::unique_ptr<ArrayNode>>(value).get() : nullptr;
    }
};

// -----------------------------------------------------------------------------
// Object Member - Key-value pair preserving insertion order
// -----------------------------------------------------------------------------
struct ObjectMember {
    std::string key;
    SourceLocation key_loc;
    std::unique_ptr<ValueNode> value;

    ObjectMember(std::string k, std::unique_ptr<ValueNode> v, SourceLocation kl = {})
        : key(std::move(k)), key_loc(kl), value(std::move(v)) {}
};

// -----------------------------------------------------------------------------
// Object Node - Ordered key-value map
// -----------------------------------------------------------------------------
struct ObjectNode : public ASTNode {
    // Vector to preserve insertion order (important for determinism)
    std::vector<ObjectMember> members;

    ObjectNode() = default;
    explicit ObjectNode(SourceLocation l) : ASTNode(l) {}

    // Add a member
    void add(std::string key, std::unique_ptr<ValueNode> value, SourceLocation key_loc = {}) {
        members.emplace_back(std::move(key), std::move(value), key_loc);
    }

    // Find a member by key (returns nullptr if not found)
    const ValueNode* get(const std::string& key) const {
        for (const auto& m : members) {
            if (m.key == key) return m.value.get();
        }
        return nullptr;
    }

    ValueNode* get(const std::string& key) {
        for (auto& m : members) {
            if (m.key == key) return m.value.get();
        }
        return nullptr;
    }

    // Check if key exists
    bool has(const std::string& key) const {
        return get(key) != nullptr;
    }

    // Get string value or default
    std::string get_string(const std::string& key, const std::string& default_val = "") const {
        const ValueNode* v = get(key);
        if (v && v->is_string()) return v->as_string();
        return default_val;
    }

    // Get integer value or default
    int64_t get_integer(const std::string& key, int64_t default_val = 0) const {
        const ValueNode* v = get(key);
        if (v && v->is_integer()) return v->as_integer();
        return default_val;
    }

    // Get bool value or default
    bool get_bool(const std::string& key, bool default_val = false) const {
        const ValueNode* v = get(key);
        if (v && v->is_bool()) return v->as_bool();
        return default_val;
    }

    // Get object value
    const ObjectNode* get_object(const std::string& key) const {
        const ValueNode* v = get(key);
        return v ? v->try_object() : nullptr;
    }

    // Get array value
    const ArrayNode* get_array(const std::string& key) const {
        const ValueNode* v = get(key);
        return v ? v->try_array() : nullptr;
    }

    // Number of members
    size_t size() const { return members.size(); }
    bool empty() const { return members.empty(); }
};

// -----------------------------------------------------------------------------
// Array Node - Ordered list of values
// -----------------------------------------------------------------------------
struct ArrayNode : public ASTNode {
    std::vector<std::unique_ptr<ValueNode>> elements;

    ArrayNode() = default;
    explicit ArrayNode(SourceLocation l) : ASTNode(l) {}

    // Add an element
    void add(std::unique_ptr<ValueNode> value) {
        elements.push_back(std::move(value));
    }

    // Get element at index (no bounds check)
    const ValueNode& at(size_t i) const { return *elements.at(i); }
    ValueNode& at(size_t i) { return *elements.at(i); }

    // Get element (unsafe)
    const ValueNode& operator[](size_t i) const { return *elements[i]; }
    ValueNode& operator[](size_t i) { return *elements[i]; }

    // Number of elements
    size_t size() const { return elements.size(); }
    bool empty() const { return elements.empty(); }

    // Collect all string elements into a vector
    std::vector<std::string> to_string_vector() const {
        std::vector<std::string> result;
        for (const auto& elem : elements) {
            if (elem->is_string()) {
                result.push_back(elem->as_string());
            }
        }
        return result;
    }
};

// -----------------------------------------------------------------------------
// Build File Node - Root of the AST
// -----------------------------------------------------------------------------
struct BuildFileNode : public ASTNode {
    std::unique_ptr<ObjectNode> project;    // project: { ... }
    std::unique_ptr<ObjectNode> variables;  // variables: { ... }
    std::unique_ptr<ArrayNode> targets;     // targets: [ ... ]

    BuildFileNode() = default;

    // Convenience accessors for project metadata
    std::string project_name() const {
        return project ? project->get_string("name") : "";
    }

    std::string project_version() const {
        return project ? project->get_string("version") : "";
    }

    // Check if file has required sections
    bool has_project() const { return project != nullptr; }
    bool has_variables() const { return variables != nullptr; }
    bool has_targets() const { return targets != nullptr; }
};

// -----------------------------------------------------------------------------
// Helper: Create value nodes
// -----------------------------------------------------------------------------
inline std::unique_ptr<ValueNode> make_string_value(std::string s, SourceLocation loc = {}) {
    return std::make_unique<ValueNode>(std::move(s), loc);
}

inline std::unique_ptr<ValueNode> make_int_value(int64_t i, SourceLocation loc = {}) {
    return std::make_unique<ValueNode>(i, loc);
}

inline std::unique_ptr<ValueNode> make_bool_value(bool b, SourceLocation loc = {}) {
    return std::make_unique<ValueNode>(b, loc);
}

inline std::unique_ptr<ValueNode> make_object_value(std::unique_ptr<ObjectNode> obj, SourceLocation loc = {}) {
    return std::make_unique<ValueNode>(std::move(obj), loc);
}

inline std::unique_ptr<ValueNode> make_array_value(std::unique_ptr<ArrayNode> arr, SourceLocation loc = {}) {
    return std::make_unique<ValueNode>(std::move(arr), loc);
}

} // namespace abc
} // namespace aria

#endif // ARIA_ABC_AST_HPP
