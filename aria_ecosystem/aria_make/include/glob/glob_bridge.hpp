/**
 * Glob Bridge for aria_make
 *
 * Integrates the aglob engine from aria_utils for pattern-based
 * source file discovery in build configurations.
 *
 * Features:
 * - Full glob pattern support: *, **, ?, [...]
 * - Canonical sorting for reproducible builds
 * - Integration with ABC source patterns
 *
 * Copyright (c) 2025 Aria Language Project
 */

#ifndef ARIA_MAKE_GLOB_BRIDGE_HPP
#define ARIA_MAKE_GLOB_BRIDGE_HPP

#include <string>
#include <vector>
#include <filesystem>

namespace aria::make::glob {

namespace fs = std::filesystem;

/**
 * Error codes from glob operations
 */
enum class GlobError {
    OK = 0,
    INVALID_BASE_DIR = 1,
    PATTERN_SYNTAX_ERROR = 2,
    ACCESS_DENIED = 3,
    FILESYSTEM_ERROR = 4,
    SYMLINK_CYCLE = 5,
    MAX_DEPTH_EXCEEDED = 6,
    UNKNOWN_ERROR = 99
};

/**
 * Result of a glob operation
 */
struct GlobResult {
    std::vector<std::string> paths;
    GlobError error = GlobError::OK;
    std::string error_message;

    bool ok() const { return error == GlobError::OK; }
};

/**
 * Glob options
 */
struct GlobOptions {
    bool case_sensitive = true;
    bool follow_symlinks = false;
    size_t max_depth = 64;
    bool files_only = true;
    bool include_hidden = false;
};

/**
 * Expand a glob pattern to matching files.
 *
 * @param base_dir Root directory for pattern matching
 * @param pattern Glob pattern - supports wildcards and recursive matching
 * @param options Optional configuration
 * @return GlobResult with matched paths or error
 *
 * Pattern syntax:
 * - *     matches any sequence (except path separator)
 * - **    matches any sequence including path separators (recursive)
 * - ?     matches any single character
 * - [abc] matches any character in set
 * - [!abc] matches any character not in set
 * - [a-z] matches any character in range
 */
GlobResult expand_pattern(
    const fs::path& base_dir,
    const std::string& pattern,
    const GlobOptions& options = GlobOptions{}
);

/**
 * Expand multiple patterns (combined results, deduplicated).
 */
GlobResult expand_patterns(
    const fs::path& base_dir,
    const std::vector<std::string>& patterns,
    const GlobOptions& options = GlobOptions{}
);

/**
 * Check if a path matches a glob pattern.
 *
 * @param path Path to test
 * @param pattern Glob pattern
 * @param case_sensitive Whether to match case-sensitively
 * @return true if path matches pattern
 */
bool path_matches(
    const fs::path& path,
    const std::string& pattern,
    bool case_sensitive = true
);

/**
 * Validate a glob pattern syntax.
 *
 * @param pattern Pattern to validate
 * @return true if pattern is syntactically valid
 */
bool validate_pattern(const std::string& pattern);

/**
 * Get human-readable error string.
 */
const char* error_string(GlobError error);

} // namespace aria::make::glob

#endif // ARIA_MAKE_GLOB_BRIDGE_HPP
