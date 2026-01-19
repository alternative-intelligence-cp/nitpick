/**
 * Aria Build Configuration (ABC) Interpolation Engine Implementation
 *
 * ARIA-019: ABC Config Parser Design and Implementation
 */

#include "abc/abc_interpolate.hpp"
#include <cstdlib>
#include <sstream>

namespace abc {

// =============================================================================
// Scope
// =============================================================================

Scope::Scope(ObjectNode* varsNode) {
    if (!varsNode) return;

    for (const auto& pair : varsNode->members) {
        if (pair.value->getKind() == ASTNode::Kind::LiteralString) {
            auto* str = static_cast<LiteralStringNode*>(pair.value);
            variables[pair.key] = str->value;
        }
        // Note: CompositeStrings need to be resolved first
    }
}

std::optional<std::string> Scope::get(const std::string& name) const {
    auto it = variables.find(name);
    if (it != variables.end()) {
        return it->second;
    }
    return std::nullopt;
}

void Scope::set(const std::string& name, const std::string& value) {
    variables[name] = value;
}

bool Scope::has(const std::string& name) const {
    return variables.find(name) != variables.end();
}

std::vector<std::string> Scope::keys() const {
    std::vector<std::string> result;
    result.reserve(variables.size());
    for (const auto& pair : variables) {
        result.push_back(pair.first);
    }
    return result;
}

// =============================================================================
// Interpolator
// =============================================================================

Interpolator::Interpolator(ObjectNode* globalVars)
    : globalScope(globalVars) {}

void Interpolator::setGlobal(const std::string& name, const std::string& value) {
    globalScope.set(name, value);
}

void Interpolator::clearCache() {
    colorMap.clear();
    cache.clear();
}

void Interpolator::recordError(const std::string& message) {
    errors.push_back(message);
}

std::optional<std::string> Interpolator::getEnvironmentVariable(const std::string& name) {
    const char* value = std::getenv(name.c_str());
    if (value) {
        return std::string(value);
    }
    return std::nullopt;
}

InterpolationResult Interpolator::resolve(const std::string& input,
                                          const Scope* localScope) {
    // Find all &{...} patterns and resolve them
    std::string result;
    size_t pos = 0;

    while (pos < input.size()) {
        size_t interpStart = input.find("&{", pos);

        if (interpStart == std::string::npos) {
            // No more interpolations, append rest
            result += input.substr(pos);
            break;
        }

        // Append literal before interpolation
        result += input.substr(pos, interpStart - pos);

        // Find closing brace
        size_t interpEnd = input.find('}', interpStart);
        if (interpEnd == std::string::npos) {
            return InterpolationResult::err("Unterminated variable reference");
        }

        // Extract variable name
        std::string varName = input.substr(interpStart + 2, interpEnd - interpStart - 2);

        // Resolve variable
        auto varResult = resolveVariable(varName, localScope);
        if (!varResult.success) {
            return varResult;
        }

        result += varResult.value;
        pos = interpEnd + 1;
    }

    return InterpolationResult::ok(result);
}

InterpolationResult Interpolator::resolveNode(CompositeStringNode* node,
                                              const Scope* localScope) {
    std::string result;

    for (const auto& segment : node->segments) {
        if (!segment.isVariable) {
            result += segment.value;
        } else {
            auto varResult = resolveVariable(segment.value, localScope);
            if (!varResult.success) {
                return varResult;
            }
            result += varResult.value;
        }
    }

    return InterpolationResult::ok(result);
}

InterpolationResult Interpolator::resolveVariable(const std::string& name,
                                                  const Scope* localScope) {
    // Track resolution path for error messages
    resolutionPath.push_back(name);

    auto result = resolveInternal(name, localScope);

    resolutionPath.pop_back();
    return result;
}

InterpolationResult Interpolator::resolveInternal(const std::string& name,
                                                  const Scope* localScope) {
    // Check for cycle (GRAY = in progress)
    auto colorIt = colorMap.find(name);
    if (colorIt != colorMap.end()) {
        if (colorIt->second == Color::GRAY) {
            // Cycle detected!
            std::ostringstream oss;
            oss << "Circular dependency detected: ";
            for (size_t i = 0; i < resolutionPath.size(); ++i) {
                if (i > 0) oss << " -> ";
                oss << resolutionPath[i];
            }
            return InterpolationResult::err(oss.str());
        }

        if (colorIt->second == Color::BLACK) {
            // Already resolved, return cached value
            return InterpolationResult::ok(cache[name]);
        }
    }

    // Mark as being resolved (GRAY)
    colorMap[name] = Color::GRAY;

    std::string resolved;

    // Check for ENV. prefix (environment variables)
    if (name.substr(0, 4) == "ENV.") {
        std::string envName = name.substr(4);
        auto envValue = getEnvironmentVariable(envName);
        if (!envValue) {
            return InterpolationResult::err(
                "Environment variable not found: " + envName);
        }
        resolved = *envValue;
    }
    // Check local scope first
    else if (localScope && localScope->has(name)) {
        auto localValue = localScope->get(name);
        if (localValue) {
            // Local value might contain interpolations
            auto result = resolve(*localValue, localScope);
            if (!result.success) {
                return result;
            }
            resolved = result.value;
        }
    }
    // Check global scope
    else if (globalScope.has(name)) {
        auto globalValue = globalScope.get(name);
        if (globalValue) {
            // Global value might contain interpolations
            auto result = resolve(*globalValue, localScope);
            if (!result.success) {
                return result;
            }
            resolved = result.value;
        }
    }
    else {
        return InterpolationResult::err("Undefined variable: " + name);
    }

    // Mark as fully resolved (BLACK)
    colorMap[name] = Color::BLACK;
    cache[name] = resolved;

    return InterpolationResult::ok(resolved);
}

// =============================================================================
// Document Resolution
// =============================================================================

/**
 * Resolve a single AST node, returning resolved string or empty on error
 */
static std::string resolveASTNode(ASTNode* node, Interpolator& interp,
                                  const Scope* localScope) {
    if (!node) return "";

    switch (node->getKind()) {
        case ASTNode::Kind::LiteralString: {
            auto* lit = static_cast<LiteralStringNode*>(node);
            // Even literal strings might contain interpolations
            auto result = interp.resolve(lit->value, localScope);
            return result.success ? result.value : "";
        }

        case ASTNode::Kind::CompositeString: {
            auto* comp = static_cast<CompositeStringNode*>(node);
            auto result = interp.resolveNode(comp, localScope);
            return result.success ? result.value : "";
        }

        default:
            return "";
    }
}

bool resolveDocument(ABCDocument& doc, ArenaAllocator& arena) {
    Interpolator interp(doc.variables);

    // First, resolve global variables (they might reference each other)
    if (doc.variables) {
        for (auto& pair : doc.variables->members) {
            std::string resolved = resolveASTNode(pair.value, interp, nullptr);
            if (!resolved.empty()) {
                // Replace with resolved literal
                auto* newNode = arena.create<LiteralStringNode>(resolved);
                newNode->line = pair.value->line;
                newNode->column = pair.value->column;
                pair.value = newNode;

                // Update scope
                interp.setGlobal(pair.key, resolved);
            }
        }
    }

    // Resolve targets
    if (doc.targets) {
        for (ASTNode* elem : doc.targets->elements) {
            if (elem->getKind() != ASTNode::Kind::Object) continue;

            ObjectNode* target = static_cast<ObjectNode*>(elem);

            // Build local scope from target's variables
            Scope localScope(target->getObject("variables"));

            // Resolve all string values in target
            for (auto& pair : target->members) {
                if (pair.key == "variables") continue;  // Already in scope

                std::string resolved = resolveASTNode(pair.value, interp, &localScope);
                if (!resolved.empty() && pair.value->getKind() != ASTNode::Kind::Object &&
                    pair.value->getKind() != ASTNode::Kind::Array) {
                    auto* newNode = arena.create<LiteralStringNode>(resolved);
                    newNode->line = pair.value->line;
                    newNode->column = pair.value->column;
                    pair.value = newNode;
                }

                // Handle arrays of strings
                if (pair.value->getKind() == ASTNode::Kind::Array) {
                    ArrayNode* arr = static_cast<ArrayNode*>(pair.value);
                    for (size_t i = 0; i < arr->elements.size(); ++i) {
                        std::string elemResolved = resolveASTNode(arr->elements[i],
                                                                  interp, &localScope);
                        if (!elemResolved.empty()) {
                            auto* newNode = arena.create<LiteralStringNode>(elemResolved);
                            newNode->line = arr->elements[i]->line;
                            newNode->column = arr->elements[i]->column;
                            arr->elements[i] = newNode;
                        }
                    }
                }
            }
        }
    }

    return !interp.hasErrors();
}

bool resolveTarget(ObjectNode* target, ObjectNode* globalVars,
                   ArenaAllocator& arena) {
    Interpolator interp(globalVars);
    Scope localScope(target->getObject("variables"));

    for (auto& pair : target->members) {
        if (pair.key == "variables") continue;

        std::string resolved = resolveASTNode(pair.value, interp, &localScope);
        if (!resolved.empty() && pair.value->getKind() != ASTNode::Kind::Object &&
            pair.value->getKind() != ASTNode::Kind::Array) {
            auto* newNode = arena.create<LiteralStringNode>(resolved);
            newNode->line = pair.value->line;
            newNode->column = pair.value->column;
            pair.value = newNode;
        }
    }

    return !interp.hasErrors();
}

} // namespace abc
