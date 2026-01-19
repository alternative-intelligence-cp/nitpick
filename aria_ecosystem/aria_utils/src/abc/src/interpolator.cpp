/**
 * interpolator.cpp
 * Variable Interpolation Engine implementation
 *
 * Copyright (c) 2025 Aria Language Project
 */

#include "abc/interpolator.hpp"
#include <sstream>
#include <cstdlib>

namespace aria {
namespace abc {

// -----------------------------------------------------------------------------
// InterpolationError
// -----------------------------------------------------------------------------
std::string InterpolationError::to_string() const {
    std::ostringstream ss;

    switch (kind) {
        case Kind::UNDEFINED_VARIABLE:
            ss << "Undefined variable: &{" << variable << "}";
            break;

        case Kind::CIRCULAR_REFERENCE:
            ss << "Circular variable reference detected: ";
            for (size_t i = 0; i < cycle_path.size(); ++i) {
                if (i > 0) ss << " -> ";
                ss << cycle_path[i];
            }
            break;

        case Kind::UNDEFINED_ENV_VAR:
            ss << "Undefined environment variable: " << variable;
            break;

        case Kind::SYNTAX_ERROR:
            ss << "Interpolation syntax error: " << message;
            break;
    }

    return ss.str();
}

// -----------------------------------------------------------------------------
// VariableScope
// -----------------------------------------------------------------------------
void VariableScope::set(const std::string& name, const std::string& value) {
    variables_[name] = value;
}

std::optional<std::string> VariableScope::get(const std::string& name) const {
    auto it = variables_.find(name);
    if (it != variables_.end()) {
        return it->second;
    }
    return std::nullopt;
}

bool VariableScope::has(const std::string& name) const {
    return variables_.find(name) != variables_.end();
}

void VariableScope::clear() {
    variables_.clear();
}

// -----------------------------------------------------------------------------
// Interpolator
// -----------------------------------------------------------------------------
Interpolator::Interpolator() = default;

void Interpolator::set_global_variables(const ObjectNode& variables) {
    for (const auto& member : variables.members) {
        if (member.value && member.value->is_string()) {
            global_scope_.set(member.key, member.value->as_string());
        }
    }
}

void Interpolator::set_variable(const std::string& name, const std::string& value) {
    global_scope_.set(name, value);
    cache_.erase(name);  // Invalidate cache
}

std::optional<std::string> Interpolator::get_variable(const std::string& name) {
    return lookup(name);
}

void Interpolator::push_scope() {
    local_scopes_.emplace_back();
}

void Interpolator::pop_scope() {
    if (!local_scopes_.empty()) {
        local_scopes_.pop_back();
    }
}

void Interpolator::set_local(const std::string& name, const std::string& value) {
    if (local_scopes_.empty()) {
        push_scope();
    }
    local_scopes_.back().set(name, value);
}

InterpolationResult Interpolator::interpolate(const std::string& input) {
    InterpolationResult result;
    errors_.clear();

    if (!has_interpolation(input)) {
        result.value = input;
        return result;
    }

    std::unordered_set<std::string> visited;
    std::vector<std::string> path;

    result.value = resolve(input, visited, path);
    result.errors = errors_;

    return result;
}

std::vector<InterpolationError> Interpolator::interpolate_ast(BuildFileNode& ast) {
    errors_.clear();

    // First pass: collect all global variables as raw strings
    if (ast.variables) {
        set_global_variables(*ast.variables);
    }

    // Second pass: resolve all variables in the variables section itself
    if (ast.variables) {
        interpolate_object(*ast.variables);

        // Update global scope with resolved values
        for (const auto& member : ast.variables->members) {
            if (member.value && member.value->is_string()) {
                global_scope_.set(member.key, member.value->as_string());
            }
        }
    }

    // Third pass: interpolate project section
    if (ast.project) {
        interpolate_object(*ast.project);
    }

    // Fourth pass: interpolate targets
    if (ast.targets) {
        interpolate_array(*ast.targets);
    }

    return errors_;
}

bool Interpolator::has_interpolation(const std::string& s) {
    return s.find("&{") != std::string::npos;
}

void Interpolator::clear_cache() {
    cache_.clear();
}

std::string Interpolator::resolve(
    const std::string& input,
    std::unordered_set<std::string>& visited,
    std::vector<std::string>& path
) {
    std::string result;
    result.reserve(input.size());

    size_t i = 0;
    while (i < input.size()) {
        // Check for &{ pattern
        if (i + 1 < input.size() && input[i] == '&' && input[i + 1] == '{') {
            std::string var_name;
            size_t end = parse_pattern(input, i, var_name);

            if (end == i) {
                // Syntax error - append as-is
                result += input[i++];
                continue;
            }

            // Check for environment variable
            std::optional<std::string> value;
            if (var_name.substr(0, 4) == "ENV.") {
                std::string env_name = var_name.substr(4);
                value = get_env(env_name);
                if (!value) {
                    InterpolationError err;
                    err.kind = InterpolationError::Kind::UNDEFINED_ENV_VAR;
                    err.variable = env_name;
                    errors_.push_back(std::move(err));
                    // Keep original pattern on error
                    result += input.substr(i, end - i);
                    i = end;
                    continue;
                }
            } else {
                // Check for circular reference
                if (visited.count(var_name)) {
                    InterpolationError err;
                    err.kind = InterpolationError::Kind::CIRCULAR_REFERENCE;
                    err.variable = var_name;
                    err.cycle_path = path;
                    err.cycle_path.push_back(var_name);
                    errors_.push_back(std::move(err));
                    // Keep original pattern on error
                    result += input.substr(i, end - i);
                    i = end;
                    continue;
                }

                // Look up variable
                value = lookup(var_name);
                if (!value) {
                    InterpolationError err;
                    err.kind = InterpolationError::Kind::UNDEFINED_VARIABLE;
                    err.variable = var_name;
                    errors_.push_back(std::move(err));
                    // Keep original pattern on error
                    result += input.substr(i, end - i);
                    i = end;
                    continue;
                }

                // Recursively resolve if value contains patterns
                if (has_interpolation(*value)) {
                    visited.insert(var_name);
                    path.push_back(var_name);
                    value = resolve(*value, visited, path);
                    path.pop_back();
                    visited.erase(var_name);
                }
            }

            result += *value;
            i = end;
        } else {
            result += input[i++];
        }
    }

    return result;
}

std::optional<std::string> Interpolator::lookup(const std::string& name) {
    // Check cache first
    auto cache_it = cache_.find(name);
    if (cache_it != cache_.end()) {
        return cache_it->second;
    }

    // Search local scopes (most recent first)
    for (auto it = local_scopes_.rbegin(); it != local_scopes_.rend(); ++it) {
        if (auto val = it->get(name)) {
            return val;
        }
    }

    // Search global scope
    if (auto val = global_scope_.get(name)) {
        return val;
    }

    return std::nullopt;
}

std::optional<std::string> Interpolator::get_env(const std::string& name) {
    const char* value = std::getenv(name.c_str());
    if (value) {
        return std::string(value);
    }
    return std::nullopt;
}

size_t Interpolator::parse_pattern(const std::string& input, size_t start, std::string& var_name) {
    // Must start with &{
    if (start + 1 >= input.size() || input[start] != '&' || input[start + 1] != '{') {
        return start;
    }

    size_t i = start + 2;  // Skip &{

    // Find closing }
    size_t brace_count = 1;
    while (i < input.size() && brace_count > 0) {
        if (input[i] == '{') {
            brace_count++;
        } else if (input[i] == '}') {
            brace_count--;
        }
        if (brace_count > 0) {
            var_name += input[i];
        }
        i++;
    }

    if (brace_count != 0) {
        // Unclosed pattern
        InterpolationError err;
        err.kind = InterpolationError::Kind::SYNTAX_ERROR;
        err.message = "Unclosed variable pattern starting at position " + std::to_string(start);
        errors_.push_back(std::move(err));
        var_name.clear();
        return start;
    }

    return i;
}

void Interpolator::interpolate_node(ValueNode& node) {
    if (node.is_string()) {
        std::string& str = node.as_string();
        if (has_interpolation(str)) {
            auto result = interpolate(str);
            str = std::move(result.value);
        }
    } else if (node.is_object()) {
        interpolate_object(node.as_object());
    } else if (node.is_array()) {
        interpolate_array(node.as_array());
    }
}

void Interpolator::interpolate_object(ObjectNode& obj) {
    for (auto& member : obj.members) {
        if (member.value) {
            interpolate_node(*member.value);
        }
    }
}

void Interpolator::interpolate_array(ArrayNode& arr) {
    for (auto& elem : arr.elements) {
        if (elem) {
            interpolate_node(*elem);
        }
    }
}

} // namespace abc
} // namespace aria
