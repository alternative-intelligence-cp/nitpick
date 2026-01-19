/**
 * scanner.cpp
 * Dependency Scanner implementation
 *
 * Copyright (c) 2025 Aria Language Project
 */

#include "depgraph/scanner.hpp"
#include <fstream>
#include <sstream>
#include <cctype>

namespace aria {
namespace depgraph {

ScanResult DependencyScanner::scan_file(const fs::path& file) {
    ScanResult result;

    std::ifstream ifs(file);
    if (!ifs) {
        result.success = false;
        result.error = "Cannot open file: " + file.string();
        return result;
    }

    std::ostringstream buffer;
    buffer << ifs.rdbuf();
    std::string content = buffer.str();

    return scan_source(content);
}

ScanResult DependencyScanner::scan_source(std::string_view source) {
    ScanResult result;
    process_source(source, result.dependencies);
    return result;
}

void DependencyScanner::process_source(std::string_view source,
                                        std::vector<std::string>& dependencies) {
    size_t pos = 0;
    State state = State::CODE;
    int interp_depth = 0;

    while (pos < source.size()) {
        char c = source[pos];
        char next = (pos + 1 < source.size()) ? source[pos + 1] : '\0';

        switch (state) {
            case State::CODE:
                // Check for comment start
                if (c == '/' && next == '/') {
                    state = State::LINE_COMMENT;
                    pos += 2;
                    continue;
                }
                if (c == '/' && next == '*') {
                    state = State::BLOCK_COMMENT;
                    pos += 2;
                    continue;
                }
                // Check for string start
                if (c == '"') {
                    state = State::STRING;
                    pos++;
                    continue;
                }
                // Check for template string
                if (c == '`') {
                    state = State::TEMPLATE_STRING;
                    pos++;
                    continue;
                }
                // Check for 'use' keyword
                if (at_use_keyword(source, pos)) {
                    pos += 3;  // Skip "use"
                    skip_whitespace(source, pos);

                    std::string module = extract_module_path(source, pos);
                    if (!module.empty()) {
                        dependencies.push_back(module);
                    }
                    continue;
                }
                break;

            case State::LINE_COMMENT:
                if (c == '\n') {
                    state = State::CODE;
                }
                break;

            case State::BLOCK_COMMENT:
                if (c == '*' && next == '/') {
                    state = State::CODE;
                    pos++;  // Skip extra char
                }
                break;

            case State::STRING:
                if (c == '\\') {
                    pos++;  // Skip escape sequence
                } else if (c == '"') {
                    state = State::CODE;
                }
                break;

            case State::TEMPLATE_STRING:
                if (c == '&' && next == '{') {
                    state = State::INTERPOLATION;
                    interp_depth = 1;
                    pos++;
                } else if (c == '`') {
                    state = State::CODE;
                }
                break;

            case State::INTERPOLATION:
                if (c == '{') {
                    interp_depth++;
                } else if (c == '}') {
                    interp_depth--;
                    if (interp_depth == 0) {
                        state = State::TEMPLATE_STRING;
                    }
                }
                break;
        }

        pos++;
    }
}

bool DependencyScanner::at_use_keyword(std::string_view source, size_t pos) {
    // Must have "use" followed by whitespace
    if (pos + 3 >= source.size()) return false;

    // Check preceding character (must be start or non-identifier)
    if (pos > 0 && is_identifier_char(source[pos - 1])) {
        return false;
    }

    // Check "use"
    if (source.substr(pos, 3) != "use") {
        return false;
    }

    // Must be followed by whitespace or quote
    char after = source[pos + 3];
    return std::isspace(static_cast<unsigned char>(after)) || after == '"';
}

std::string DependencyScanner::extract_module_path(std::string_view source, size_t& pos) {
    skip_whitespace(source, pos);

    if (pos >= source.size()) {
        return "";
    }

    std::string result;

    // Check for quoted path: use "foo.aria"
    if (source[pos] == '"') {
        pos++;  // Skip opening quote
        while (pos < source.size() && source[pos] != '"' && source[pos] != '\n') {
            result += source[pos++];
        }
        if (pos < source.size() && source[pos] == '"') {
            pos++;  // Skip closing quote
        }
        return result;
    }

    // Logical path: use std.io
    while (pos < source.size()) {
        char c = source[pos];
        if (is_identifier_char(c) || c == '.') {
            result += c;
            pos++;
        } else {
            break;
        }
    }

    // Skip trailing semicolon if present
    skip_whitespace(source, pos);
    if (pos < source.size() && source[pos] == ';') {
        pos++;
    }

    return result;
}

void DependencyScanner::skip_whitespace(std::string_view source, size_t& pos) {
    while (pos < source.size() && std::isspace(static_cast<unsigned char>(source[pos]))) {
        pos++;
    }
}

bool DependencyScanner::is_identifier_start(char c) {
    return std::isalpha(static_cast<unsigned char>(c)) || c == '_';
}

bool DependencyScanner::is_identifier_char(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
}

bool DependencyScanner::is_valid_module_path(const std::string& path) {
    if (path.empty()) return false;

    // Check first character
    if (!is_identifier_start(path[0])) return false;

    for (size_t i = 1; i < path.size(); ++i) {
        char c = path[i];
        if (!is_identifier_char(c) && c != '.') {
            return false;
        }
        // Can't have consecutive dots
        if (c == '.' && i > 0 && path[i-1] == '.') {
            return false;
        }
    }

    // Can't end with dot
    if (path.back() == '.') return false;

    return true;
}

std::vector<fs::path> DependencyScanner::resolve_module(
    const std::string& module_path,
    const fs::path& source_dir,
    const std::vector<fs::path>& include_paths
) {
    std::vector<fs::path> candidates;

    // Relative path (starts with quote or has .aria extension)
    if (module_path.find(".aria") != std::string::npos) {
        fs::path relative = source_dir / module_path;
        candidates.push_back(relative);
        return candidates;
    }

    // Convert dots to directory separators
    std::string path_str = module_path;
    for (char& c : path_str) {
        if (c == '.') c = '/';
    }

    // Try each include path
    std::vector<fs::path> search_paths = include_paths;
    search_paths.insert(search_paths.begin(), source_dir);

    for (const fs::path& base : search_paths) {
        // Try direct file: std/io.aria
        fs::path direct = base / (path_str + ".aria");
        candidates.push_back(direct);

        // Try module directory: std/io/mod.aria
        fs::path mod = base / path_str / "mod.aria";
        candidates.push_back(mod);
    }

    return candidates;
}

} // namespace depgraph
} // namespace aria
