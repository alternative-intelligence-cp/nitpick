/**
 * glob_engine.cpp
 * Implementation of the Aria Globbing Subsystem
 *
 * Hybrid approach combining:
 * - Segment-based anchoring for performance
 * - Regex transpilation for pattern matching
 * - Full character class support [a-z], [!0-9], etc.
 * - Canonical sorting for reproducible builds
 */

#include "aglob/glob_engine.hpp"
#include <algorithm>
#include <stdexcept>

namespace aria {
namespace glob {

// -----------------------------------------------------------------------------
// Error String Conversion
// -----------------------------------------------------------------------------
const char* glob_error_string(GlobError err) {
    switch (err) {
        case GlobError::OK: return "OK";
        case GlobError::INVALID_BASE_DIR: return "Invalid base directory";
        case GlobError::PATTERN_SYNTAX_ERROR: return "Pattern syntax error";
        case GlobError::ACCESS_DENIED: return "Access denied";
        case GlobError::FILESYSTEM_ERROR: return "Filesystem error";
        case GlobError::SYMLINK_CYCLE: return "Symlink cycle detected";
        case GlobError::MAX_DEPTH_EXCEEDED: return "Maximum depth exceeded";
        case GlobError::UNKNOWN_ERROR: return "Unknown error";
        default: return "Unknown error";
    }
}

// =============================================================================
// GlobPattern Implementation
// =============================================================================

GlobPattern::GlobPattern(const std::string& pattern) {
    parse(pattern);
}

void GlobPattern::parse(const std::string& pattern) {
    pattern_ = normalize_separators(pattern);

    if (pattern_.empty()) {
        return;
    }

    // Check if absolute path
    fs::path p(pattern_);
    is_absolute_ = p.is_absolute();

    // Split on '/' and parse each segment
    size_t start = 0;
    size_t end = 0;

    // Handle leading slash for absolute paths
    if (!pattern_.empty() && pattern_[0] == '/') {
        start = 1;
    }

    // Parse segments
    while ((end = pattern_.find('/', start)) != std::string::npos) {
        if (end > start) {
            add_segment(pattern_.substr(start, end - start));
        }
        start = end + 1;
    }

    // Add final segment
    if (start < pattern_.size()) {
        add_segment(pattern_.substr(start));
    }

    // Calculate anchor depth
    anchor_depth_ = 0;
    for (const auto& seg : segments_) {
        if (seg.type != SegmentType::Literal) {
            break;
        }
        anchor_depth_++;
    }
}

void GlobPattern::add_segment(const std::string& token) {
    if (token.empty()) return;

    if (token == "**") {
        segments_.emplace_back(token, SegmentType::Recursive);
        has_wildcards_ = true;
        has_recursive_ = true;
    } else if (is_wildcard_segment(token)) {
        segments_.emplace_back(token, SegmentType::Wildcard);
        has_wildcards_ = true;
    } else {
        segments_.emplace_back(token, SegmentType::Literal);
    }
}

bool GlobPattern::is_wildcard_segment(const std::string& token) {
    bool in_escape = false;
    bool in_bracket = false;
    bool found_bracket = false;

    for (char c : token) {
        if (in_escape) {
            in_escape = false;
            continue;
        }
        if (c == '\\') {
            in_escape = true;
            continue;
        }
        if (in_bracket) {
            if (c == ']') {
                in_bracket = false;
                found_bracket = true;  // Found a complete [...] pattern
            }
            continue;
        }
        switch (c) {
            case '*':
            case '?':
                return true;
            case '[':
                in_bracket = true;
                break;
        }
    }
    return found_bracket;  // Return true if we found a character class
}

std::string GlobPattern::normalize_separators(const std::string& input) {
    std::string result;
    result.reserve(input.size());

    for (size_t i = 0; i < input.size(); ++i) {
        char c = input[i];
#ifdef _WIN32
        // On Windows, convert backslash to forward slash
        if (c == '\\') {
            result += '/';
            continue;
        }
#endif
        result += c;
    }

    // Remove trailing slash
    while (!result.empty() && result.back() == '/') {
        result.pop_back();
    }

    return result;
}

fs::path GlobPattern::anchor() const {
    fs::path result;

    for (size_t i = 0; i < anchor_depth_ && i < segments_.size(); ++i) {
        result /= segments_[i].text;
    }

    return result;
}

std::string GlobPattern::dynamic_suffix() const {
    if (anchor_depth_ >= segments_.size()) {
        return "";
    }

    std::string result;
    for (size_t i = anchor_depth_; i < segments_.size(); ++i) {
        if (!result.empty()) result += '/';
        result += segments_[i].text;
    }
    return result;
}

std::string GlobPattern::validate(const std::string& pattern) {
    bool in_bracket = false;
    bool in_escape = false;
    int bracket_start = -1;

    for (size_t i = 0; i < pattern.size(); ++i) {
        char c = pattern[i];

        if (in_escape) {
            in_escape = false;
            continue;
        }
        if (c == '\\') {
            in_escape = true;
            continue;
        }
        if (in_bracket) {
            if (c == ']' && i > static_cast<size_t>(bracket_start) + 1) {
                in_bracket = false;
            }
            continue;
        }
        if (c == '[') {
            in_bracket = true;
            bracket_start = static_cast<int>(i);
            // Skip negation
            if (i + 1 < pattern.size() && (pattern[i + 1] == '!' || pattern[i + 1] == '^')) {
                ++i;
            }
        }
    }

    if (in_bracket) {
        return "Unclosed character class '[' at position " + std::to_string(bracket_start);
    }
    if (in_escape) {
        return "Trailing escape character '\\'";
    }
    return "";
}

// =============================================================================
// GlobEngine Implementation
// =============================================================================

GlobEngine::GlobEngine(GlobOptions opts) : options_(std::move(opts)) {}

std::string GlobEngine::glob_to_regex(const std::string& glob) {
    std::string regex_str;
    regex_str.reserve(glob.size() * 2);

    // Anchor to start
    regex_str.push_back('^');

    for (size_t i = 0; i < glob.size(); ++i) {
        char c = glob[i];

        switch (c) {
            case '*':
                // Check for ** (recursive globstar)
                if (i + 1 < glob.size() && glob[i + 1] == '*') {
                    i++;  // Skip second *
                    // Check if followed by /
                    if (i + 1 < glob.size() && glob[i + 1] == '/') {
                        i++;  // Skip the /
                        // **/ means "any path prefix (including empty)"
                        // This matches: "", "dir/", "a/b/c/"
                        regex_str.append("(.*/)?");
                    } else {
                        // ** at end or before non-slash
                        regex_str.append(".*");
                    }
                } else {
                    // * matches anything EXCEPT separators
#ifdef _WIN32
                    regex_str.append("[^\\\\/]*");
#else
                    regex_str.append("[^/]*");
#endif
                }
                break;

            case '?':
                // ? matches single char except separator
#ifdef _WIN32
                regex_str.append("[^\\\\/]");
#else
                regex_str.append("[^/]");
#endif
                break;

            case '[':
                // Character class - parse it fully
                regex_str.append(parse_char_class(glob, i));
                break;

            case '\\':
                // Escape sequence
                if (i + 1 < glob.size()) {
                    regex_str.push_back('\\');
                    regex_str.push_back(glob[++i]);
                } else {
                    regex_str.append("\\\\");
                }
                break;

            // Escape regex metacharacters
            case '.':
            case '+':
            case '(':
            case ')':
            case '{':
            case '}':
            case '^':
            case '$':
            case '|':
                regex_str.push_back('\\');
                regex_str.push_back(c);
                break;

            default:
                regex_str.push_back(c);
        }
    }

    // Anchor to end
    regex_str.push_back('$');
    return regex_str;
}

std::string GlobEngine::parse_char_class(const std::string& glob, size_t& pos) {
    // pos points to '[', we'll advance it past ']'
    std::string result;
    result.push_back('[');
    pos++;  // Skip '['

    // Check for negation
    if (pos < glob.size() && (glob[pos] == '!' || glob[pos] == '^')) {
        result.push_back('^');  // Regex uses ^ for negation
        pos++;
    }

    // Handle ] as first character (literal])
    if (pos < glob.size() && glob[pos] == ']') {
        result.push_back('\\');
        result.push_back(']');
        pos++;
    }

    // Parse until closing ]
    while (pos < glob.size() && glob[pos] != ']') {
        char c = glob[pos];

        if (c == '\\' && pos + 1 < glob.size()) {
            // Escape sequence
            result.push_back('\\');
            result.push_back(glob[++pos]);
        } else if (c == '-' && !result.empty() && result.back() != '[' && result.back() != '^') {
            // Range: check if valid
            if (pos + 1 < glob.size() && glob[pos + 1] != ']') {
                result.push_back('-');
            } else {
                // Literal dash at end
                result.push_back('\\');
                result.push_back('-');
            }
        } else {
            // Regular character - escape if needed for regex
            if (c == '\\' || c == '^' || c == '-') {
                result.push_back('\\');
            }
            result.push_back(c);
        }
        pos++;
    }

    result.push_back(']');
    // pos now points to ']' or past end
    return result;
}

void GlobEngine::canonical_sort(std::vector<fs::path>& paths) {
    // Sort using generic_string() for cross-platform consistency
    // This normalizes separators to '/' for comparison
    std::sort(paths.begin(), paths.end(),
        [](const fs::path& a, const fs::path& b) {
            return a.generic_string() < b.generic_string();
        }
    );
}

bool GlobEngine::should_skip(const fs::directory_entry& entry) {
    // Skip hidden files if not requested
    if (!options_.include_hidden) {
        std::string filename = entry.path().filename().string();
        if (!filename.empty() && filename[0] == '.') {
            return true;
        }
    }
    return false;
}

bool GlobEngine::check_symlink_cycle(const fs::path& path) {
    if (!options_.follow_symlinks) {
        return false;  // Not following symlinks, no cycle possible
    }

    std::error_code ec;
    if (!fs::is_symlink(path, ec)) {
        return false;
    }

    fs::path canonical = fs::canonical(path, ec);
    if (ec) {
        return true;  // Can't resolve, assume cycle
    }

    if (visited_symlinks_.count(canonical)) {
        return true;  // Cycle detected
    }

    visited_symlinks_.insert(canonical);
    return false;
}

std::pair<std::vector<fs::path>, GlobError> GlobEngine::match(
    const std::string& base_dir,
    const std::string& pattern
) {
    // Clear state from previous runs
    visited_symlinks_.clear();
    warnings_.clear();

    fs::path root(base_dir);
    std::error_code ec;

    // Validate base directory
    if (!fs::exists(root, ec) || !fs::is_directory(root, ec)) {
        return {{}, GlobError::INVALID_BASE_DIR};
    }

    // Validate pattern syntax
    std::string validation_error = GlobPattern::validate(pattern);
    if (!validation_error.empty()) {
        return {{}, GlobError::PATTERN_SYNTAX_ERROR};
    }

    // Parse the pattern
    GlobPattern parsed(pattern);

    // Determine search root using anchoring optimization
    fs::path search_root = root;
    std::string match_pattern = pattern;

    if (parsed.anchor_depth() > 0) {
        // We have a literal prefix - start from there
        fs::path anchor = root / parsed.anchor();
        if (fs::exists(anchor, ec) && fs::is_directory(anchor, ec)) {
            search_root = anchor;
            match_pattern = parsed.dynamic_suffix();
        }
        // If anchor doesn't exist, no matches possible
        if (!fs::exists(anchor, ec)) {
            return {{}, GlobError::OK};  // Empty result, not an error
        }
    }

    // Compile the regex for the pattern
    try {
        auto flags = std::regex::ECMAScript | std::regex::optimize;
        if (!options_.case_sensitive) {
            flags |= std::regex::icase;
        }
        std::string regex_str = glob_to_regex(match_pattern);
        compiled_regex_ = std::regex(regex_str, flags);
    } catch (const std::regex_error&) {
        return {{}, GlobError::PATTERN_SYNTAX_ERROR};
    }

    // Perform the scan
    std::vector<fs::path> results;

    if (parsed.has_recursive()) {
        // Pattern contains **, need full recursive scan
        scan_recursive(search_root, results, ec, 0);
    } else {
        // Non-recursive pattern, can use anchored scan
        scan_anchored(search_root, match_pattern, results, ec);
    }

    if (ec && !options_.skip_permission_errors) {
        return {{}, GlobError::FILESYSTEM_ERROR};
    }

    // Canonical sort for reproducibility
    canonical_sort(results);

    return {results, GlobError::OK};
}

void GlobEngine::scan_anchored(
    const fs::path& anchor_path,
    const std::string& suffix_pattern,
    std::vector<fs::path>& results,
    std::error_code& ec
) {
    // For non-recursive patterns, we can use a simple directory iterator
    // and only go one level deep per segment

    auto opts = options_.follow_symlinks
        ? fs::directory_options::follow_directory_symlink
        : fs::directory_options::none;

    if (options_.skip_permission_errors) {
        opts |= fs::directory_options::skip_permission_denied;
    }

    for (auto it = fs::directory_iterator(anchor_path, opts, ec);
         it != fs::directory_iterator();
         it.increment(ec)) {

        if (ec) {
            if (options_.skip_permission_errors) {
                ec.clear();
                continue;
            }
            return;
        }

        const auto& entry = *it;

        if (should_skip(entry)) {
            continue;
        }

        // Get path relative to anchor
        fs::path rel_path = fs::relative(entry.path(), anchor_path, ec);
        if (ec) continue;

        std::string path_str = rel_path.generic_string();

        // Match against pattern
        if (std::regex_match(path_str, compiled_regex_)) {
            // Check file type constraints
            bool is_file = entry.is_regular_file(ec);
            bool is_dir = entry.is_directory(ec);

            if (options_.files_only && !is_file) continue;
            if (options_.directories_only && !is_dir) continue;

            results.push_back(entry.path());
        }
    }
}

void GlobEngine::scan_recursive(
    const fs::path& current_dir,
    std::vector<fs::path>& results,
    std::error_code& ec,
    size_t current_depth
) {
    if (options_.max_depth > 0 && current_depth > options_.max_depth) {
        return;
    }

    auto opts = options_.follow_symlinks
        ? fs::directory_options::follow_directory_symlink
        : fs::directory_options::none;

    if (options_.skip_permission_errors) {
        opts |= fs::directory_options::skip_permission_denied;
    }

    for (auto it = fs::recursive_directory_iterator(current_dir, opts, ec);
         it != fs::recursive_directory_iterator();
         /* manual increment */) {

        if (ec) {
            if (options_.skip_permission_errors) {
                it.disable_recursion_pending();
                ec.clear();
                it.increment(ec);
                continue;
            }
            return;
        }

        const auto& entry = *it;

        // Check for symlink cycles
        if (entry.is_symlink(ec) && check_symlink_cycle(entry.path())) {
            it.disable_recursion_pending();
            it.increment(ec);
            continue;
        }

        if (should_skip(entry)) {
            if (entry.is_directory(ec)) {
                it.disable_recursion_pending();
            }
            it.increment(ec);
            continue;
        }

        // Calculate path relative to search root for matching
        fs::path rel_path = fs::relative(entry.path(), current_dir, ec);
        if (ec) {
            it.increment(ec);
            continue;
        }

        std::string path_str = rel_path.generic_string();

        // Match against compiled regex
        if (std::regex_match(path_str, compiled_regex_)) {
            bool is_file = entry.is_regular_file(ec);
            bool is_dir = entry.is_directory(ec);

            if (options_.files_only && !is_file) {
                it.increment(ec);
                continue;
            }
            if (options_.directories_only && !is_dir) {
                it.increment(ec);
                continue;
            }

            results.push_back(entry.path());
        }

        it.increment(ec);
    }
}

std::pair<std::vector<fs::path>, GlobError> GlobEngine::match_all(
    const std::string& base_dir,
    const std::vector<std::string>& patterns
) {
    std::vector<fs::path> combined;
    std::set<fs::path> seen;  // For deduplication

    for (const auto& pattern : patterns) {
        auto [paths, err] = match(base_dir, pattern);
        if (err != GlobError::OK) {
            return {{}, err};
        }

        for (auto& p : paths) {
            if (seen.insert(p).second) {
                combined.push_back(std::move(p));
            }
        }
    }

    canonical_sort(combined);
    return {combined, GlobError::OK};
}

bool GlobEngine::path_matches(
    const fs::path& path,
    const std::string& pattern,
    bool case_sensitive
) {
    GlobOptions opts;
    opts.case_sensitive = case_sensitive;
    GlobEngine engine(opts);

    std::string regex_str = engine.glob_to_regex(pattern);

    try {
        auto flags = std::regex::ECMAScript;
        if (!case_sensitive) {
            flags |= std::regex::icase;
        }
        std::regex re(regex_str, flags);
        return std::regex_match(path.generic_string(), re);
    } catch (...) {
        return false;
    }
}

} // namespace glob
} // namespace aria
