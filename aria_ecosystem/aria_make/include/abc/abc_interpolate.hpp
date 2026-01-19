/**
 * Aria Build Configuration (ABC) Interpolation Engine
 *
 * ARIA-019: ABC Config Parser Design and Implementation
 *
 * Resolves &{VAR} references in the AST using hierarchical scope resolution:
 * 1. Local Scope (target-level variables)
 * 2. Global Scope (project-level variables)
 * 3. Environment Scope (ENV.VAR for system environment)
 *
 * Features:
 * - Three-color DFS cycle detection (WHITE, GRAY, BLACK)
 * - Memoization for O(V+E) complexity
 * - Detailed error messages with resolution paths
 */

#ifndef ABC_INTERPOLATE_HPP
#define ABC_INTERPOLATE_HPP

#include "abc/abc_parser.hpp"
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <optional>

namespace abc {

/**
 * Variable resolution scope
 */
class Scope {
public:
    /**
     * Create an empty scope
     */
    Scope() = default;

    /**
     * Create scope from ObjectNode variables
     */
    explicit Scope(ObjectNode* variables);

    /**
     * Look up a variable
     *
     * @param name Variable name
     * @return Value if found, nullopt otherwise
     */
    std::optional<std::string> get(const std::string& name) const;

    /**
     * Set a variable
     */
    void set(const std::string& name, const std::string& value);

    /**
     * Check if variable exists
     */
    bool has(const std::string& name) const;

    /**
     * Get all variable names
     */
    std::vector<std::string> keys() const;

private:
    std::unordered_map<std::string, std::string> variables;
};

/**
 * Interpolation result
 */
struct InterpolationResult {
    bool success;
    std::string value;
    std::string error;

    static InterpolationResult ok(std::string val) {
        return {true, std::move(val), ""};
    }

    static InterpolationResult err(std::string msg) {
        return {false, "", std::move(msg)};
    }
};

/**
 * Interpolation Engine
 *
 * Resolves variable references in ABC configurations using
 * hierarchical scope lookup and cycle detection.
 */
class Interpolator {
public:
    /**
     * Create interpolator with global variables
     *
     * @param globalVars Project-level variables section
     */
    explicit Interpolator(ObjectNode* globalVars = nullptr);

    /**
     * Resolve a string that may contain &{VAR} references
     *
     * @param input String with potential interpolation
     * @param localScope Optional target-level variables
     * @return Resolved string or error
     */
    InterpolationResult resolve(const std::string& input,
                                const Scope* localScope = nullptr);

    /**
     * Resolve a CompositeStringNode
     */
    InterpolationResult resolveNode(CompositeStringNode* node,
                                    const Scope* localScope = nullptr);

    /**
     * Resolve a single variable reference
     *
     * @param name Variable name (may include ENV. prefix)
     * @param localScope Optional local scope
     * @return Resolved value or error
     */
    InterpolationResult resolveVariable(const std::string& name,
                                        const Scope* localScope = nullptr);

    /**
     * Set a global variable (for testing or dynamic config)
     */
    void setGlobal(const std::string& name, const std::string& value);

    /**
     * Get error messages
     */
    const std::vector<std::string>& getErrors() const { return errors; }

    /**
     * Check if there were errors
     */
    bool hasErrors() const { return !errors.empty(); }

    /**
     * Clear resolution cache (for re-evaluation)
     */
    void clearCache();

private:
    Scope globalScope;
    std::vector<std::string> errors;

    // Three-color marking for cycle detection
    enum class Color {
        WHITE,  // Not visited
        GRAY,   // Currently being resolved (in recursion stack)
        BLACK   // Fully resolved and cached
    };

    std::unordered_map<std::string, Color> colorMap;
    std::unordered_map<std::string, std::string> cache;
    std::vector<std::string> resolutionPath;  // For error reporting

    /**
     * Internal resolution with cycle detection
     */
    InterpolationResult resolveInternal(const std::string& name,
                                        const Scope* localScope);

    /**
     * Look up variable in environment
     */
    std::optional<std::string> getEnvironmentVariable(const std::string& name);

    /**
     * Record an error
     */
    void recordError(const std::string& message);
};

/**
 * Resolve all interpolations in an ABC document
 *
 * @param doc The parsed ABC document
 * @param arena Allocator for new string nodes
 * @return true if all interpolations succeeded
 */
bool resolveDocument(ABCDocument& doc, ArenaAllocator& arena);

/**
 * Resolve all interpolations in a target
 *
 * @param target Target object node
 * @param globalVars Global variables scope
 * @param arena Allocator
 * @return true if successful
 */
bool resolveTarget(ObjectNode* target, ObjectNode* globalVars,
                   ArenaAllocator& arena);

} // namespace abc

#endif // ABC_INTERPOLATE_HPP
