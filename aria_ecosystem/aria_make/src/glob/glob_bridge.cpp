/**
 * Glob Bridge Implementation
 *
 * Wraps the aglob C FFI from aria_utils for use in aria_make.
 *
 * Copyright (c) 2025 Aria Language Project
 */

#include "glob/glob_bridge.hpp"
#include <cstdlib>
#include <algorithm>
#include <set>

// Forward declarations of aglob C FFI (from aria_utils/aglob)
extern "C" {

typedef struct {
    char** paths;
    size_t count;
    int error_code;
} AriaGlobResult;

typedef struct {
    int case_sensitive;
    int follow_symlinks;
    size_t max_depth;
    int files_only;
    int directories_only;
    int include_hidden;
    int skip_permission_errors;
} AriaGlobOptions;

AriaGlobResult aria_glob_match(const char* base_dir, const char* pattern);
AriaGlobResult aria_glob_match_with_options(
    const char* base_dir,
    const char* pattern,
    const AriaGlobOptions* options
);
AriaGlobResult aria_glob_match_all(
    const char* base_dir,
    const char** patterns,
    size_t pattern_count
);
void aria_glob_free(AriaGlobResult* result);
const char* aria_glob_error_string(int error_code);
int aria_glob_validate_pattern(const char* pattern);
int aria_glob_path_matches(const char* path, const char* pattern, int case_sensitive);

} // extern "C"

namespace aria::make::glob {

GlobResult expand_pattern(
    const fs::path& base_dir,
    const std::string& pattern,
    const GlobOptions& options
) {
    GlobResult result;

    // Build C options struct
    AriaGlobOptions c_opts;
    c_opts.case_sensitive = options.case_sensitive ? 1 : 0;
    c_opts.follow_symlinks = options.follow_symlinks ? 1 : 0;
    c_opts.max_depth = options.max_depth;
    c_opts.files_only = options.files_only ? 1 : 0;
    c_opts.directories_only = 0;
    c_opts.include_hidden = options.include_hidden ? 1 : 0;
    c_opts.skip_permission_errors = 1;

    // Call the aglob engine
    AriaGlobResult c_result = aria_glob_match_with_options(
        base_dir.string().c_str(),
        pattern.c_str(),
        &c_opts
    );

    // Convert error code
    result.error = static_cast<GlobError>(c_result.error_code);

    if (c_result.error_code != 0) {
        result.error_message = aria_glob_error_string(c_result.error_code);
        aria_glob_free(&c_result);
        return result;
    }

    // Copy paths to C++ vector
    result.paths.reserve(c_result.count);
    for (size_t i = 0; i < c_result.count; ++i) {
        result.paths.emplace_back(c_result.paths[i]);
    }

    // Free the C result
    aria_glob_free(&c_result);

    return result;
}

GlobResult expand_patterns(
    const fs::path& base_dir,
    const std::vector<std::string>& patterns,
    const GlobOptions& options
) {
    GlobResult result;

    if (patterns.empty()) {
        return result;
    }

    // For now, expand each pattern and merge results
    // (The aglob aria_glob_match_all doesn't take options)
    std::set<std::string> seen;

    for (const auto& pattern : patterns) {
        GlobResult partial = expand_pattern(base_dir, pattern, options);

        if (!partial.ok()) {
            result.error = partial.error;
            result.error_message = partial.error_message;
            return result;
        }

        for (auto& path : partial.paths) {
            if (seen.insert(path).second) {
                result.paths.push_back(std::move(path));
            }
        }
    }

    // Canonical sort for reproducibility
    std::sort(result.paths.begin(), result.paths.end());

    return result;
}

bool path_matches(
    const fs::path& path,
    const std::string& pattern,
    bool case_sensitive
) {
    return aria_glob_path_matches(
        path.string().c_str(),
        pattern.c_str(),
        case_sensitive ? 1 : 0
    ) != 0;
}

bool validate_pattern(const std::string& pattern) {
    return aria_glob_validate_pattern(pattern.c_str()) != 0;
}

const char* error_string(GlobError error) {
    return aria_glob_error_string(static_cast<int>(error));
}

} // namespace aria::make::glob
