/**
 * source_resolver.hpp
 * Source File Resolution for Aria Build Configuration
 *
 * Integrates with aglob (GlobEngine) to expand source patterns
 * into concrete file lists for build targets.
 *
 * Features:
 * - Glob pattern expansion (e.g. **.aria)
 * - Variable interpolation in patterns
 * - Exclusion pattern support (!pattern)
 * - Deterministic sorting for reproducible builds
 *
 * Copyright (c) 2025 Aria Language Project
 */

#ifndef ARIA_ABC_SOURCE_RESOLVER_HPP
#define ARIA_ABC_SOURCE_RESOLVER_HPP

#include "ast.hpp"
#include "interpolator.hpp"
#include <string>
#include <vector>
#include <filesystem>

// Forward declare aglob types
namespace aria {
namespace glob {
    class GlobEngine;
    struct GlobOptions;
    enum class GlobError;
}
}

namespace aria {
namespace abc {

namespace fs = std::filesystem;

// -----------------------------------------------------------------------------
// Source Resolution Error
// -----------------------------------------------------------------------------
struct SourceError {
    enum class Kind {
        GLOB_ERROR,
        PATH_NOT_FOUND,
        PERMISSION_DENIED,
        INVALID_PATTERN
    };

    Kind kind;
    std::string pattern;
    std::string message;

    std::string to_string() const;
};

// -----------------------------------------------------------------------------
// Source Resolution Result
// -----------------------------------------------------------------------------
struct SourceResult {
    std::vector<fs::path> files;
    std::vector<SourceError> errors;

    bool success() const { return errors.empty(); }
    bool has_errors() const { return !errors.empty(); }
    size_t count() const { return files.size(); }
};

// -----------------------------------------------------------------------------
// Source Resolver
// -----------------------------------------------------------------------------
class SourceResolver {
public:
    /**
     * Construct resolver with base directory.
     * All relative patterns are resolved from this directory.
     */
    explicit SourceResolver(const fs::path& base_dir);

    /**
     * Construct resolver with base directory and interpolator.
     * Variables in patterns will be resolved using the interpolator.
     */
    SourceResolver(const fs::path& base_dir, Interpolator* interp);

    /**
     * Resolve a single glob pattern.
     */
    SourceResult resolve(const std::string& pattern);

    /**
     * Resolve multiple patterns (from sources array).
     * Patterns starting with '!' are exclusions.
     * Results are combined, deduplicated, and sorted.
     */
    SourceResult resolve_all(const std::vector<std::string>& patterns);

    /**
     * Resolve sources from an ArrayNode.
     */
    SourceResult resolve_array(const ArrayNode& sources);

    /**
     * Set whether to include hidden files (default: false).
     */
    void set_include_hidden(bool include);

    /**
     * Set whether to follow symlinks (default: false).
     */
    void set_follow_symlinks(bool follow);

    /**
     * Set maximum recursion depth for ** patterns (default: 64).
     */
    void set_max_depth(size_t depth);

    /**
     * Get the base directory.
     */
    const fs::path& base_dir() const { return base_dir_; }

private:
    fs::path base_dir_;
    Interpolator* interpolator_ = nullptr;

    bool include_hidden_ = false;
    bool follow_symlinks_ = false;
    size_t max_depth_ = 64;
};

} // namespace abc
} // namespace aria

#endif // ARIA_ABC_SOURCE_RESOLVER_HPP
