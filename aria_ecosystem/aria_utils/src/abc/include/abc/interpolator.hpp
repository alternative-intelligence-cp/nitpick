/**
 * interpolator.hpp
 * Variable Interpolation Engine for Aria Build Configuration
 *
 * Resolves &{variable} patterns in string values with:
 * - Scoped resolution (local -> global -> environment)
 * - Cycle detection to prevent infinite recursion
 * - Nested variable support (&{a}/&{b})
 * - Environment variable access via &{ENV.VAR}
 * - Caching for resolved values (performance)
 *
 * Copyright (c) 2025 Aria Language Project
 */

#ifndef ARIA_ABC_INTERPOLATOR_HPP
#define ARIA_ABC_INTERPOLATOR_HPP

#include "ast.hpp"
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <optional>
#include <functional>

namespace aria {
namespace abc {

// -----------------------------------------------------------------------------
// Interpolation Error
// -----------------------------------------------------------------------------
struct InterpolationError {
    enum class Kind {
        UNDEFINED_VARIABLE,
        CIRCULAR_REFERENCE,
        UNDEFINED_ENV_VAR,
        SYNTAX_ERROR
    };

    Kind kind;
    std::string variable;
    std::string message;
    std::vector<std::string> cycle_path;  // For circular reference errors

    std::string to_string() const;
};

// -----------------------------------------------------------------------------
// Interpolation Result
// -----------------------------------------------------------------------------
struct InterpolationResult {
    std::string value;
    std::vector<InterpolationError> errors;

    bool success() const { return errors.empty(); }
    bool has_errors() const { return !errors.empty(); }

    // Implicit conversion to string (for convenience when errors are ignored)
    operator const std::string&() const { return value; }
};

// -----------------------------------------------------------------------------
// Variable Scope
// -----------------------------------------------------------------------------
class VariableScope {
public:
    // Set a variable in this scope
    void set(const std::string& name, const std::string& value);

    // Get a variable from this scope only
    std::optional<std::string> get(const std::string& name) const;

    // Check if variable exists in this scope
    bool has(const std::string& name) const;

    // Get all variables
    const std::unordered_map<std::string, std::string>& variables() const {
        return variables_;
    }

    // Clear all variables
    void clear();

private:
    std::unordered_map<std::string, std::string> variables_;
};

// -----------------------------------------------------------------------------
// Interpolator
// -----------------------------------------------------------------------------
class Interpolator {
public:
    /**
     * Construct interpolator with optional global variables.
     */
    Interpolator();

    // No copying (contains cached state)
    Interpolator(const Interpolator&) = delete;
    Interpolator& operator=(const Interpolator&) = delete;

    /**
     * Set global variables from parsed ObjectNode.
     * Call this after parsing the 'variables' section.
     */
    void set_global_variables(const ObjectNode& variables);

    /**
     * Set a single global variable.
     */
    void set_variable(const std::string& name, const std::string& value);

    /**
     * Get a resolved variable value.
     */
    std::optional<std::string> get_variable(const std::string& name);

    /**
     * Push a local scope (for target-specific variables).
     */
    void push_scope();

    /**
     * Pop the most recent local scope.
     */
    void pop_scope();

    /**
     * Set a variable in the current (top) scope.
     */
    void set_local(const std::string& name, const std::string& value);

    /**
     * Interpolate all &{var} patterns in a string.
     * Returns resolved string and any errors.
     */
    InterpolationResult interpolate(const std::string& input);

    /**
     * Interpolate in-place, modifying all strings in the AST.
     * Returns list of all errors encountered.
     */
    std::vector<InterpolationError> interpolate_ast(BuildFileNode& ast);

    /**
     * Check if a string contains interpolation patterns.
     */
    static bool has_interpolation(const std::string& s);

    /**
     * Clear all cached resolved values.
     */
    void clear_cache();

    /**
     * Get all errors from the last operation.
     */
    const std::vector<InterpolationError>& errors() const { return errors_; }

private:
    // Core resolution with cycle detection
    std::string resolve(
        const std::string& input,
        std::unordered_set<std::string>& visited,
        std::vector<std::string>& path
    );

    // Look up a variable across all scopes
    std::optional<std::string> lookup(const std::string& name);

    // Get environment variable
    std::optional<std::string> get_env(const std::string& name);

    // Parse &{var} pattern at position, returns end position
    size_t parse_pattern(const std::string& input, size_t start, std::string& var_name);

    // Interpolate all strings in a node recursively
    void interpolate_node(ValueNode& node);
    void interpolate_object(ObjectNode& obj);
    void interpolate_array(ArrayNode& arr);

private:
    VariableScope global_scope_;
    std::vector<VariableScope> local_scopes_;

    // Cache of fully resolved values
    std::unordered_map<std::string, std::string> cache_;

    // Accumulated errors
    std::vector<InterpolationError> errors_;
};

} // namespace abc
} // namespace aria

#endif // ARIA_ABC_INTERPOLATOR_HPP
