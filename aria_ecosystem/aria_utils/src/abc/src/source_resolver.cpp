/**
 * source_resolver.cpp
 * Source File Resolution implementation
 *
 * Integrates with aglob for glob pattern expansion.
 *
 * Copyright (c) 2025 Aria Language Project
 */

#include "abc/source_resolver.hpp"
#include "aglob/glob_engine.hpp"
#include <algorithm>
#include <set>
#include <sstream>

namespace aria {
namespace abc {

// -----------------------------------------------------------------------------
// SourceError
// -----------------------------------------------------------------------------
std::string SourceError::to_string() const {
    std::ostringstream ss;
    ss << "Source resolution error";
    if (!pattern.empty()) {
        ss << " for pattern '" << pattern << "'";
    }
    ss << ": " << message;
    return ss.str();
}

// -----------------------------------------------------------------------------
// SourceResolver
// -----------------------------------------------------------------------------
SourceResolver::SourceResolver(const fs::path& base_dir)
    : base_dir_(base_dir) {}

SourceResolver::SourceResolver(const fs::path& base_dir, Interpolator* interp)
    : base_dir_(base_dir), interpolator_(interp) {}

SourceResult SourceResolver::resolve(const std::string& pattern) {
    SourceResult result;

    // Interpolate variables if interpolator is available
    std::string resolved_pattern = pattern;
    if (interpolator_ && Interpolator::has_interpolation(pattern)) {
        auto interp_result = interpolator_->interpolate(pattern);
        resolved_pattern = interp_result.value;
        // Note: interpolation errors are reported through the interpolator
    }

    // Check if this is a literal file path (no wildcards)
    bool has_wildcards = (resolved_pattern.find('*') != std::string::npos ||
                          resolved_pattern.find('?') != std::string::npos ||
                          resolved_pattern.find('[') != std::string::npos);

    if (!has_wildcards) {
        // Literal path - check if it exists
        fs::path file_path = resolved_pattern;
        if (!file_path.is_absolute()) {
            file_path = base_dir_ / file_path;
        }

        if (fs::exists(file_path)) {
            result.files.push_back(fs::canonical(file_path));
        } else {
            SourceError err;
            err.kind = SourceError::Kind::PATH_NOT_FOUND;
            err.pattern = pattern;
            err.message = "File not found: " + file_path.string();
            result.errors.push_back(std::move(err));
        }
        return result;
    }

    // Use GlobEngine for pattern matching
    glob::GlobOptions opts;
    opts.include_hidden = include_hidden_;
    opts.follow_symlinks = follow_symlinks_;
    opts.max_depth = max_depth_;
    opts.files_only = true;

    glob::GlobEngine engine(opts);

    auto [paths, error] = engine.match(base_dir_.string(), resolved_pattern);

    if (error != glob::GlobError::OK) {
        SourceError err;
        err.kind = SourceError::Kind::GLOB_ERROR;
        err.pattern = pattern;
        err.message = glob::glob_error_string(error);
        result.errors.push_back(std::move(err));
    }

    result.files = std::move(paths);
    return result;
}

SourceResult SourceResolver::resolve_all(const std::vector<std::string>& patterns) {
    SourceResult result;
    std::set<fs::path> included;
    std::set<fs::path> excluded;

    // First pass: collect all exclusion patterns
    for (const auto& pattern : patterns) {
        if (!pattern.empty() && pattern[0] == '!') {
            std::string exclusion = pattern.substr(1);
            auto exc_result = resolve(exclusion);

            for (const auto& path : exc_result.files) {
                excluded.insert(path);
            }

            // Propagate errors
            for (auto& err : exc_result.errors) {
                result.errors.push_back(std::move(err));
            }
        }
    }

    // Second pass: collect all inclusion patterns
    for (const auto& pattern : patterns) {
        if (pattern.empty() || pattern[0] == '!') {
            continue;  // Skip exclusions and empty patterns
        }

        auto inc_result = resolve(pattern);

        for (const auto& path : inc_result.files) {
            // Only include if not excluded
            if (excluded.find(path) == excluded.end()) {
                included.insert(path);
            }
        }

        // Propagate errors
        for (auto& err : inc_result.errors) {
            result.errors.push_back(std::move(err));
        }
    }

    // Convert set to sorted vector (set is already sorted)
    result.files.assign(included.begin(), included.end());

    return result;
}

SourceResult SourceResolver::resolve_array(const ArrayNode& sources) {
    std::vector<std::string> patterns;
    patterns.reserve(sources.size());

    for (const auto& elem : sources.elements) {
        if (elem && elem->is_string()) {
            patterns.push_back(elem->as_string());
        }
    }

    return resolve_all(patterns);
}

void SourceResolver::set_include_hidden(bool include) {
    include_hidden_ = include;
}

void SourceResolver::set_follow_symlinks(bool follow) {
    follow_symlinks_ = follow;
}

void SourceResolver::set_max_depth(size_t depth) {
    max_depth_ = depth;
}

} // namespace abc
} // namespace aria
