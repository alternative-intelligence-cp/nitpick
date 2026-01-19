/**
 * glob_ffi.cpp
 * Foreign Function Interface for GlobEngine
 *
 * Exposes C-compatible API for linking to:
 * - ariac compiler driver
 * - aria_make build system
 * - Future Aria std.fs.glob module
 *
 * Memory Management:
 * - All returned memory is owned by the caller
 * - Use aria_glob_free() to release GlobResult
 * - Thread-safe: each call creates its own engine instance
 */

#include "aglob/glob_engine.hpp"
#include <cstring>
#include <cstdlib>
#include <new>  // For nothrow

extern "C" {

// -----------------------------------------------------------------------------
// FFI Data Structures
// -----------------------------------------------------------------------------

/**
 * Result structure for glob operations.
 * Memory layout matches Aria's extern struct declarations.
 */
typedef struct {
    char** paths;       // Array of null-terminated path strings
    size_t count;       // Number of paths in array
    int error_code;     // 0 = OK, see GlobError enum
} AriaGlobResult;

/**
 * Options structure for glob operations.
 * Allows Aria code to configure engine behavior.
 */
typedef struct {
    int case_sensitive;      // 1 = case sensitive, 0 = case insensitive
    int follow_symlinks;     // 1 = follow, 0 = don't follow
    size_t max_depth;        // 0 = unlimited, >0 = limit
    int files_only;          // 1 = only files, 0 = include dirs
    int directories_only;    // 1 = only dirs, 0 = include files
    int include_hidden;      // 1 = include dotfiles, 0 = skip
    int skip_permission_errors; // 1 = skip, 0 = fail on permission denied
} AriaGlobOptions;

// -----------------------------------------------------------------------------
// FFI Functions
// -----------------------------------------------------------------------------

// Match a single glob pattern against a directory.
//
// @param base_dir Root directory for pattern matching (C-string, not null)
// @param pattern  Glob pattern to match (C-string, not null)
// @return AriaGlobResult with matched paths and status
//
// Example:
//   AriaGlobResult result = aria_glob_match("/project", "src/\*\*/*.aria");
//   for (size_t i = 0; i < result.count; i++) {
//       printf("%s\n", result.paths[i]);
//   }
//   aria_glob_free(&result);
AriaGlobResult aria_glob_match(const char* base_dir, const char* pattern) {
    // Safety checks - never crash on null input
    if (!base_dir || !pattern) {
        return {nullptr, 0, static_cast<int>(aria::glob::GlobError::UNKNOWN_ERROR)};
    }

    // Create engine with default options
    aria::glob::GlobEngine engine;

    // Perform the match
    auto [paths, error] = engine.match(base_dir, pattern);

    if (error != aria::glob::GlobError::OK) {
        return {nullptr, 0, static_cast<int>(error)};
    }

    // Marshal std::vector<fs::path> to char**
    size_t count = paths.size();

    if (count == 0) {
        return {nullptr, 0, 0};  // Valid result with no matches
    }

    // Allocate array of pointers
    char** out_paths = static_cast<char**>(malloc(sizeof(char*) * count));
    if (!out_paths) {
        return {nullptr, 0, static_cast<int>(aria::glob::GlobError::UNKNOWN_ERROR)};
    }

    // Allocate and copy each path string
    for (size_t i = 0; i < count; ++i) {
        std::string path_str = paths[i].string();
        out_paths[i] = strdup(path_str.c_str());
        if (!out_paths[i]) {
            // Cleanup on allocation failure
            for (size_t j = 0; j < i; ++j) {
                free(out_paths[j]);
            }
            free(out_paths);
            return {nullptr, 0, static_cast<int>(aria::glob::GlobError::UNKNOWN_ERROR)};
        }
    }

    return {out_paths, count, 0};
}

/**
 * Match a single glob pattern with custom options.
 *
 * @param base_dir Root directory for pattern matching
 * @param pattern  Glob pattern to match
 * @param options  Configuration options (can be null for defaults)
 * @return AriaGlobResult with matched paths and status
 */
AriaGlobResult aria_glob_match_with_options(
    const char* base_dir,
    const char* pattern,
    const AriaGlobOptions* options
) {
    if (!base_dir || !pattern) {
        return {nullptr, 0, static_cast<int>(aria::glob::GlobError::UNKNOWN_ERROR)};
    }

    // Build options struct
    aria::glob::GlobOptions opts;
    if (options) {
        opts.case_sensitive = options->case_sensitive != 0;
        opts.follow_symlinks = options->follow_symlinks != 0;
        opts.max_depth = options->max_depth;
        opts.files_only = options->files_only != 0;
        opts.directories_only = options->directories_only != 0;
        opts.include_hidden = options->include_hidden != 0;
        opts.skip_permission_errors = options->skip_permission_errors != 0;
    }

    aria::glob::GlobEngine engine(opts);
    auto [paths, error] = engine.match(base_dir, pattern);

    if (error != aria::glob::GlobError::OK) {
        return {nullptr, 0, static_cast<int>(error)};
    }

    size_t count = paths.size();
    if (count == 0) {
        return {nullptr, 0, 0};
    }

    char** out_paths = static_cast<char**>(malloc(sizeof(char*) * count));
    if (!out_paths) {
        return {nullptr, 0, static_cast<int>(aria::glob::GlobError::UNKNOWN_ERROR)};
    }

    for (size_t i = 0; i < count; ++i) {
        std::string path_str = paths[i].string();
        out_paths[i] = strdup(path_str.c_str());
        if (!out_paths[i]) {
            for (size_t j = 0; j < i; ++j) {
                free(out_paths[j]);
            }
            free(out_paths);
            return {nullptr, 0, static_cast<int>(aria::glob::GlobError::UNKNOWN_ERROR)};
        }
    }

    return {out_paths, count, 0};
}

/**
 * Match multiple glob patterns (logical OR).
 * Results are combined, sorted, and deduplicated.
 *
 * @param base_dir Root directory for pattern matching
 * @param patterns Array of glob pattern strings
 * @param pattern_count Number of patterns in array
 * @return AriaGlobResult with combined matched paths
 */
AriaGlobResult aria_glob_match_all(
    const char* base_dir,
    const char** patterns,
    size_t pattern_count
) {
    if (!base_dir || !patterns || pattern_count == 0) {
        return {nullptr, 0, static_cast<int>(aria::glob::GlobError::UNKNOWN_ERROR)};
    }

    // Convert C array to vector
    std::vector<std::string> pattern_vec;
    pattern_vec.reserve(pattern_count);
    for (size_t i = 0; i < pattern_count; ++i) {
        if (patterns[i]) {
            pattern_vec.emplace_back(patterns[i]);
        }
    }

    aria::glob::GlobEngine engine;
    auto [paths, error] = engine.match_all(base_dir, pattern_vec);

    if (error != aria::glob::GlobError::OK) {
        return {nullptr, 0, static_cast<int>(error)};
    }

    size_t count = paths.size();
    if (count == 0) {
        return {nullptr, 0, 0};
    }

    char** out_paths = static_cast<char**>(malloc(sizeof(char*) * count));
    if (!out_paths) {
        return {nullptr, 0, static_cast<int>(aria::glob::GlobError::UNKNOWN_ERROR)};
    }

    for (size_t i = 0; i < count; ++i) {
        std::string path_str = paths[i].string();
        out_paths[i] = strdup(path_str.c_str());
        if (!out_paths[i]) {
            for (size_t j = 0; j < i; ++j) {
                free(out_paths[j]);
            }
            free(out_paths);
            return {nullptr, 0, static_cast<int>(aria::glob::GlobError::UNKNOWN_ERROR)};
        }
    }

    return {out_paths, count, 0};
}

/**
 * Free memory allocated by aria_glob_match functions.
 * Must be called to prevent memory leaks.
 *
 * @param result Pointer to result struct to free
 */
void aria_glob_free(AriaGlobResult* result) {
    if (!result) return;

    if (result->paths) {
        for (size_t i = 0; i < result->count; ++i) {
            if (result->paths[i]) {
                free(result->paths[i]);
            }
        }
        free(result->paths);
    }

    // Reset to safe state
    result->paths = nullptr;
    result->count = 0;
    result->error_code = 0;
}

/**
 * Get human-readable error string for error code.
 *
 * @param error_code Error code from AriaGlobResult
 * @return Static string describing the error (do not free)
 */
const char* aria_glob_error_string(int error_code) {
    return aria::glob::glob_error_string(
        static_cast<aria::glob::GlobError>(error_code)
    );
}

/**
 * Validate a glob pattern syntax.
 *
 * @param pattern Pattern to validate
 * @return 1 if valid, 0 if invalid
 */
int aria_glob_validate_pattern(const char* pattern) {
    if (!pattern) return 0;

    std::string error = aria::glob::GlobPattern::validate(pattern);
    return error.empty() ? 1 : 0;
}

/**
 * Check if a specific path matches a pattern.
 * Useful for filtering without full directory traversal.
 *
 * @param path Full path to check
 * @param pattern Glob pattern to match against
 * @param case_sensitive 1 for case-sensitive, 0 for case-insensitive
 * @return 1 if matches, 0 if not
 */
int aria_glob_path_matches(
    const char* path,
    const char* pattern,
    int case_sensitive
) {
    if (!path || !pattern) return 0;

    return aria::glob::GlobEngine::path_matches(
        fs::path(path),
        pattern,
        case_sensitive != 0
    ) ? 1 : 0;
}

} // extern "C"
