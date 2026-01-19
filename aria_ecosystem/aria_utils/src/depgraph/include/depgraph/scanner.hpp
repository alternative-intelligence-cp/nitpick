/**
 * scanner.hpp
 * Dependency Scanner for Aria Source Files
 *
 * Lightweight scanner to extract 'use' statements from .aria files
 * without invoking the full compiler frontend.
 *
 * Features:
 * - Fast single-pass scanning
 * - Respects comments and strings (no false positives)
 * - Handles Aria module path syntax
 *
 * Copyright (c) 2025 Aria Language Project
 */

#ifndef ARIA_DEPGRAPH_SCANNER_HPP
#define ARIA_DEPGRAPH_SCANNER_HPP

#include <string>
#include <string_view>
#include <vector>
#include <filesystem>

namespace aria {
namespace depgraph {

namespace fs = std::filesystem;

// -----------------------------------------------------------------------------
// Scan Result
// -----------------------------------------------------------------------------
struct ScanResult {
    std::vector<std::string> dependencies;  // Module paths from 'use' statements
    std::vector<std::string> warnings;
    bool success = true;
    std::string error;
};

// -----------------------------------------------------------------------------
// Module Path
// -----------------------------------------------------------------------------
struct ModulePath {
    std::string logical;    // Logical path: "std.io"
    fs::path physical;      // Physical path: "std/io.aria" or "std/io/mod.aria"
    bool is_relative;       // use "foo.aria" vs use std.io
};

// -----------------------------------------------------------------------------
// Dependency Scanner
// -----------------------------------------------------------------------------
class DependencyScanner {
public:
    /**
     * Scan a single file for dependencies.
     */
    static ScanResult scan_file(const fs::path& file);

    /**
     * Scan source string directly.
     */
    static ScanResult scan_source(std::string_view source);

    /**
     * Resolve a logical module path to physical file path.
     *
     * Resolution rules (Aria v0.0.7):
     * - Relative: use "foo.aria" -> ./foo.aria
     * - Logical:  use std.io    -> std/io.aria or std/io/mod.aria
     * - Dots become directory separators
     */
    static std::vector<fs::path> resolve_module(
        const std::string& module_path,
        const fs::path& source_dir,
        const std::vector<fs::path>& include_paths
    );

    /**
     * Check if a string looks like a module path.
     */
    static bool is_valid_module_path(const std::string& path);

private:
    // Scanner state machine
    enum class State {
        CODE,
        LINE_COMMENT,
        BLOCK_COMMENT,
        STRING,
        TEMPLATE_STRING,
        INTERPOLATION
    };

    // Core scanning logic
    static void process_source(
        std::string_view source,
        std::vector<std::string>& dependencies
    );

    // Extract module path after 'use' keyword
    static std::string extract_module_path(
        std::string_view source,
        size_t& pos
    );

    // Check if we're at the start of 'use' keyword
    static bool at_use_keyword(std::string_view source, size_t pos);

    // Skip whitespace
    static void skip_whitespace(std::string_view source, size_t& pos);

    // Character classification
    static bool is_identifier_start(char c);
    static bool is_identifier_char(char c);
};

} // namespace depgraph
} // namespace aria

#endif // ARIA_DEPGRAPH_SCANNER_HPP
