/**
 * glob_engine.hpp
 * Host-side Globbing Subsystem for Aria Compiler & Build System
 *
 * Provides deterministic, recursive directory traversal with
 * ABC-compliant glob pattern matching.
 *
 * Features:
 * - Segment-based anchoring for performance (skips irrelevant subtrees)
 * - Glob-to-regex transpilation with full character class support
 * - Canonical sorting for reproducible builds
 * - FFI-safe error handling (no exceptions across boundary)
 * - Cross-platform path normalization
 *
 * Copyright (c) 2025 Aria Language Project
 */

#ifndef ARIA_GLOB_ENGINE_HPP
#define ARIA_GLOB_ENGINE_HPP

#include <string>
#include <string_view>
#include <vector>
#include <filesystem>
#include <regex>
#include <system_error>
#include <set>

namespace fs = std::filesystem;

namespace aria {
namespace glob {

// -----------------------------------------------------------------------------
// Error Codes (FFI-safe integers)
// -----------------------------------------------------------------------------
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

// Convert error to string for diagnostics
const char* glob_error_string(GlobError err);

// -----------------------------------------------------------------------------
// Segment Types for Pattern Parsing
// -----------------------------------------------------------------------------
enum class SegmentType {
    Literal,    // Exact match: "src", "main.aria"
    Wildcard,   // Contains *, ?, or [...]: "*.aria", "test_?"
    Recursive   // The ** globstar operator
};

// A parsed segment of the glob pattern
struct Segment {
    std::string text;
    SegmentType type;

    Segment(std::string t, SegmentType st) : text(std::move(t)), type(st) {}
};

// -----------------------------------------------------------------------------
// Configuration Options
// -----------------------------------------------------------------------------
struct GlobOptions {
    // Case-sensitive matching (default: true on Linux, false on Windows/macOS)
    bool case_sensitive =
#ifdef _WIN32
        false;
#elif __APPLE__
        false;
#else
        true;
#endif

    // Follow symbolic links during traversal (default: false for safety)
    bool follow_symlinks = false;

    // Maximum recursion depth (0 = unlimited, 64 = safe default)
    size_t max_depth = 64;

    // Return only regular files (default: true)
    bool files_only = true;

    // Return only directories (default: false)
    bool directories_only = false;

    // Include hidden files/directories (starting with '.')
    bool include_hidden = false;

    // Skip permission-denied errors instead of failing
    bool skip_permission_errors = true;
};

// -----------------------------------------------------------------------------
// GlobPattern - Parses and analyzes glob patterns
// -----------------------------------------------------------------------------
class GlobPattern {
public:
    explicit GlobPattern(const std::string& pattern);

    // Access parsed segments
    const std::vector<Segment>& segments() const noexcept { return segments_; }

    // Original normalized pattern
    const std::string& pattern() const noexcept { return pattern_; }

    // Check pattern properties
    bool is_absolute() const noexcept { return is_absolute_; }
    bool has_wildcards() const noexcept { return has_wildcards_; }
    bool has_recursive() const noexcept { return has_recursive_; }

    // Get the fixed anchor path (longest literal prefix before any wildcard)
    // Example: "src/core/**/*.aria" -> "src/core"
    fs::path anchor() const;

    // Index of first non-literal segment
    size_t anchor_depth() const noexcept { return anchor_depth_; }

    // Get the dynamic suffix (pattern after anchor)
    // Example: "src/core/**/*.aria" -> "**/*.aria"
    std::string dynamic_suffix() const;

    // Validate pattern syntax (returns error message or empty string)
    static std::string validate(const std::string& pattern);

private:
    void parse(const std::string& pattern);
    void add_segment(const std::string& token);
    static bool is_wildcard_segment(const std::string& token);
    static std::string normalize_separators(const std::string& input);

    std::string pattern_;
    std::vector<Segment> segments_;
    bool is_absolute_ = false;
    bool has_wildcards_ = false;
    bool has_recursive_ = false;
    size_t anchor_depth_ = 0;
};

// -----------------------------------------------------------------------------
// GlobEngine - Core globbing functionality
// -----------------------------------------------------------------------------
class GlobEngine {
public:
    explicit GlobEngine(GlobOptions opts = GlobOptions());

    // Expand a single glob pattern.
    // @param base_dir Root directory for relative patterns
    // @param pattern The glob pattern (e.g., "src/ ** / *.aria" without spaces)
    // @return Pair of (matched paths, error code)
    std::pair<std::vector<fs::path>, GlobError> match(
        const std::string& base_dir,
        const std::string& pattern
    );

    /**
     * Expand multiple patterns (logical OR).
     * Results are combined, sorted, and deduplicated.
     */
    std::pair<std::vector<fs::path>, GlobError> match_all(
        const std::string& base_dir,
        const std::vector<std::string>& patterns
    );

    /**
     * Test if a specific path matches a pattern.
     */
    static bool path_matches(
        const fs::path& path,
        const std::string& pattern,
        bool case_sensitive = true
    );

private:
    GlobOptions options_;
    std::regex compiled_regex_;
    std::set<fs::path> visited_symlinks_;  // Cycle detection
    std::vector<std::string> warnings_;

    // Glob-to-regex transpilation with full character class support
    std::string glob_to_regex(const std::string& glob);

    // Parse character class [...] and return regex equivalent
    std::string parse_char_class(const std::string& glob, size_t& pos);

    // Anchored traversal (starts from literal prefix)
    void scan_anchored(
        const fs::path& anchor_path,
        const std::string& suffix_pattern,
        std::vector<fs::path>& results,
        std::error_code& ec
    );

    // Full recursive scan (for patterns starting with **)
    void scan_recursive(
        const fs::path& current_dir,
        std::vector<fs::path>& results,
        std::error_code& ec,
        size_t current_depth
    );

    // Canonical sort for reproducible output
    void canonical_sort(std::vector<fs::path>& paths);

    // Check if entry should be skipped
    bool should_skip(const fs::directory_entry& entry);

    // Symlink cycle detection
    bool check_symlink_cycle(const fs::path& path);
};

} // namespace glob
} // namespace aria

#endif // ARIA_GLOB_ENGINE_HPP
