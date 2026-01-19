/**
 * build_orchestrator.cpp
 * Implementation of the Central Build Orchestrator
 *
 * Copyright (c) 2025 Aria Language Project
 */

#include "core/build_orchestrator.hpp"
#include "core/compiler_interface.hpp"
#include "core/c_compiler_interface.hpp"
#include "glob/glob_bridge.hpp"

// ABC Parser (from aria_utils)
// Note: In production, this would be linked from aria_utils
// For now, we include minimal parsing support inline

#include <fstream>
#include <sstream>
#include <algorithm>
#include <thread>
#include <future>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <regex>
#include <array>
#include <variant>
#include <iostream>

namespace aria::make {

// =============================================================================
// Minimal ABC AST Support (inline for self-contained build)
// =============================================================================

namespace abc {

struct SourceLocation {
    size_t line = 1;
    size_t column = 1;
    std::string file;
};

struct ObjectNode;
struct ArrayNode;

using Value = std::variant<
    std::string,
    int64_t,
    bool,
    std::unique_ptr<ObjectNode>,
    std::unique_ptr<ArrayNode>
>;

struct ValueNode {
    Value value;
    SourceLocation loc;

    bool is_string() const { return std::holds_alternative<std::string>(value); }
    bool is_array() const { return std::holds_alternative<std::unique_ptr<ArrayNode>>(value); }
    bool is_object() const { return std::holds_alternative<std::unique_ptr<ObjectNode>>(value); }

    const std::string& as_string() const { return std::get<std::string>(value); }
    const ArrayNode& as_array() const { return *std::get<std::unique_ptr<ArrayNode>>(value); }
    const ObjectNode& as_object() const { return *std::get<std::unique_ptr<ObjectNode>>(value); }
};

struct ObjectMember {
    std::string key;
    std::unique_ptr<ValueNode> value;
};

struct ObjectNode {
    std::vector<ObjectMember> members;

    const ValueNode* get(const std::string& key) const {
        for (const auto& m : members) {
            if (m.key == key) return m.value.get();
        }
        return nullptr;
    }

    std::string get_string(const std::string& key, const std::string& def = "") const {
        const ValueNode* v = get(key);
        if (v && v->is_string()) return v->as_string();
        return def;
    }

    const ArrayNode* get_array(const std::string& key) const {
        const ValueNode* v = get(key);
        if (v && v->is_array()) return &v->as_array();
        return nullptr;
    }
};

struct ArrayNode {
    std::vector<std::unique_ptr<ValueNode>> elements;

    std::vector<std::string> to_string_vector() const {
        std::vector<std::string> result;
        for (const auto& e : elements) {
            if (e->is_string()) result.push_back(e->as_string());
        }
        return result;
    }
};

struct BuildFileNode {
    std::unique_ptr<ObjectNode> project;
    std::unique_ptr<ObjectNode> variables;
    std::unique_ptr<ArrayNode> targets;

    std::string project_name() const {
        return project ? project->get_string("name") : "";
    }
};

} // namespace abc

// =============================================================================
// Thread Pool for Parallel Builds
// =============================================================================

class ThreadPool {
public:
    explicit ThreadPool(size_t num_threads) : stop_(false), active_(0) {
        for (size_t i = 0; i < num_threads; ++i) {
            workers_.emplace_back([this] {
                while (true) {
                    std::function<void()> task;
                    {
                        std::unique_lock<std::mutex> lock(mutex_);
                        cv_.wait(lock, [this] { return stop_ || !tasks_.empty(); });
                        if (stop_ && tasks_.empty()) return;
                        task = std::move(tasks_.front());
                        tasks_.pop();
                        ++active_;
                    }
                    task();
                    {
                        std::unique_lock<std::mutex> lock(mutex_);
                        --active_;
                        if (tasks_.empty() && active_ == 0) {
                            done_cv_.notify_all();
                        }
                    }
                }
            });
        }
    }

    ~ThreadPool() {
        {
            std::unique_lock<std::mutex> lock(mutex_);
            stop_ = true;
        }
        cv_.notify_all();
        for (auto& w : workers_) {
            if (w.joinable()) w.join();
        }
    }

    template<typename F>
    void enqueue(F&& f) {
        {
            std::unique_lock<std::mutex> lock(mutex_);
            tasks_.emplace(std::forward<F>(f));
        }
        cv_.notify_one();
    }

    void wait_all() {
        std::unique_lock<std::mutex> lock(mutex_);
        done_cv_.wait(lock, [this] { return tasks_.empty() && active_ == 0; });
    }

    size_t queue_size() const {
        std::unique_lock<std::mutex> lock(mutex_);
        return tasks_.size();
    }

private:
    std::vector<std::thread> workers_;
    std::queue<std::function<void()>> tasks_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::condition_variable done_cv_;
    bool stop_;
    size_t active_;
};

// =============================================================================
// Build Orchestrator Implementation
// =============================================================================

BuildOrchestrator::BuildOrchestrator(BuildConfig config)
    : config_(std::move(config))
    , state_(config_.state_dir)
{
    // Set default thread count
    if (config_.num_threads == 0) {
        config_.num_threads = std::thread::hardware_concurrency();
        if (config_.num_threads == 0) config_.num_threads = 4;
    }
}

BuildOrchestrator::~BuildOrchestrator() = default;

BuildResult BuildOrchestrator::build() {
    start_time_ = std::chrono::steady_clock::now();
    result_ = BuildResult{};
    cancelled_ = false;

    // Stage 1: Parse build.abc
    report_progress(BuildPhase::PARSING, 0, 1, "", "Parsing build configuration...");
    if (!parse_build_file()) {
        result_.success = false;
        return result_;
    }

    // Stage 2: Extract targets
    if (!extract_targets()) {
        result_.success = false;
        return result_;
    }

    // Stage 3: Expand source patterns
    if (!expand_sources()) {
        result_.success = false;
        return result_;
    }

    // Stage 4: Load previous state
    report_progress(BuildPhase::LOADING_STATE, 0, 1, "", "Loading build state...");
    state_.load();

    // Stage 5: Build dependency graph
    report_progress(BuildPhase::ANALYZING, 0, 1, "", "Analyzing dependencies...");
    if (!scan_dependencies()) {
        result_.success = false;
        return result_;
    }

    if (!build_dependency_graph()) {
        result_.success = false;
        return result_;
    }

    // Stage 6: Detect cycles
    if (!detect_cycles()) {
        result_.success = false;
        result_.has_cycle = true;
        return result_;
    }

    // Stage 7: Mark dirty targets
    report_progress(BuildPhase::CHECKING_DIRTY, 0, 1, "", "Checking for changes...");
    if (!mark_dirty_targets()) {
        result_.success = false;
        return result_;
    }

    // Stage 8: Execute builds
    report_progress(BuildPhase::COMPILING, 0, dirty_targets_.size(), "", "Building...");
    if (!execute_builds()) {
        result_.success = false;
        return result_;
    }

    // Stage 9: Save state
    report_progress(BuildPhase::SAVING_STATE, 0, 1, "", "Saving build state...");
    save_state();

    // Calculate total time
    auto end_time = std::chrono::steady_clock::now();
    result_.total_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time_);

    result_.success = (result_.failed_targets == 0);
    report_progress(BuildPhase::COMPLETE, 0, 0, "", "Build complete");

    return result_;
}

bool BuildOrchestrator::clean() {
    std::error_code ec;

    // Remove build output directory
    if (fs::exists(config_.output_dir)) {
        fs::remove_all(config_.output_dir, ec);
        if (ec) {
            add_error("Failed to remove output directory: " + ec.message());
            return false;
        }
    }

    // Clear state
    state_.clear();

    // Remove state file
    fs::path state_file = config_.state_dir / "state.json";
    if (fs::exists(state_file)) {
        fs::remove(state_file, ec);
    }

    return true;
}

BuildResult BuildOrchestrator::rebuild() {
    clean();
    config_.force_rebuild = true;
    return build();
}

BuildResult BuildOrchestrator::check() {
    config_.dry_run = true;
    return build();
}

std::vector<BuildTarget> BuildOrchestrator::list_targets() const {
    return targets_;
}

std::string BuildOrchestrator::dependency_graph_dot() const {
    std::ostringstream oss;
    oss << "digraph dependencies {\n";
    oss << "  rankdir=LR;\n";
    oss << "  node [shape=box];\n";

    for (const auto& [target, deps] : dependencies_) {
        for (const auto& dep : deps) {
            oss << "  \"" << target << "\" -> \"" << dep << "\";\n";
        }
    }

    oss << "}\n";
    return oss.str();
}

void BuildOrchestrator::cancel() {
    cancelled_ = true;
}

// =============================================================================
// Build Pipeline Stages
// =============================================================================

bool BuildOrchestrator::parse_build_file() {
    fs::path build_path = config_.project_root / config_.build_file;

    if (!fs::exists(build_path)) {
        add_error("Build file not found: " + build_path.string());
        return false;
    }

    // Read file content
    std::ifstream file(build_path);
    if (!file) {
        add_error("Cannot open build file: " + build_path.string());
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();

    // For now, use a simple INI-like parser for build.abc
    // Format:
    //   [project]
    //   name = "project_name"
    //   version = "0.1.0"
    //
    //   [target.main]
    //   type = "binary"
    //   sources = ["src/*.aria"]
    //   deps = []

    build_ast_ = std::make_unique<abc::BuildFileNode>();
    build_ast_->project = std::make_unique<abc::ObjectNode>();
    build_ast_->targets = std::make_unique<abc::ArrayNode>();

    std::string current_section;
    std::unique_ptr<abc::ObjectNode> current_target;
    std::string current_target_name;

    std::istringstream stream(content);
    std::string line;
    size_t line_num = 0;

    while (std::getline(stream, line)) {
        line_num++;

        // Trim whitespace
        auto start = line.find_first_not_of(" \t");
        if (start == std::string::npos) continue;
        line = line.substr(start);

        // Skip comments
        if (line[0] == '#' || line[0] == ';') continue;

        // Check for section header
        if (line[0] == '[') {
            // Save previous target if any
            if (current_target) {
                auto target_value = std::make_unique<abc::ValueNode>();
                target_value->value = std::move(current_target);
                build_ast_->targets->elements.push_back(std::move(target_value));
            }

            auto end = line.find(']');
            if (end == std::string::npos) {
                add_error("Invalid section header at line " + std::to_string(line_num));
                continue;
            }

            current_section = line.substr(1, end - 1);

            // Check for target section
            if (current_section.substr(0, 7) == "target.") {
                current_target_name = current_section.substr(7);
                current_target = std::make_unique<abc::ObjectNode>();

                // Add name to target object
                auto name_val = std::make_unique<abc::ValueNode>();
                name_val->value = current_target_name;
                current_target->members.push_back({"name", std::move(name_val)});
            } else {
                current_target.reset();
                current_target_name.clear();
            }
            continue;
        }

        // Parse key = value
        auto eq_pos = line.find('=');
        if (eq_pos == std::string::npos) continue;

        std::string key = line.substr(0, eq_pos);
        std::string value = line.substr(eq_pos + 1);

        // Trim key and value
        auto key_end = key.find_last_not_of(" \t");
        if (key_end != std::string::npos) key = key.substr(0, key_end + 1);

        auto val_start = value.find_first_not_of(" \t");
        if (val_start != std::string::npos) value = value.substr(val_start);

        // Remove quotes from string values
        if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
            value = value.substr(1, value.size() - 2);
        }

        // Create value node
        auto val_node = std::make_unique<abc::ValueNode>();

        // Check for array value
        if (value.front() == '[' && value.back() == ']') {
            auto arr = std::make_unique<abc::ArrayNode>();
            std::string arr_content = value.substr(1, value.size() - 2);

            // Simple array parsing (comma-separated quoted strings)
            std::regex item_regex("\"([^\"]*)\"");
            std::sregex_iterator it(arr_content.begin(), arr_content.end(), item_regex);
            std::sregex_iterator end;

            while (it != end) {
                auto elem = std::make_unique<abc::ValueNode>();
                elem->value = (*it)[1].str();
                arr->elements.push_back(std::move(elem));
                ++it;
            }

            val_node->value = std::move(arr);
        } else {
            val_node->value = value;
        }

        // Add to appropriate section
        if (current_section == "project" && build_ast_->project) {
            build_ast_->project->members.push_back({key, std::move(val_node)});
        } else if (current_target) {
            current_target->members.push_back({key, std::move(val_node)});
        }
    }

    // Save last target
    if (current_target) {
        auto target_value = std::make_unique<abc::ValueNode>();
        target_value->value = std::move(current_target);
        build_ast_->targets->elements.push_back(std::move(target_value));
    }

    return true;
}

bool BuildOrchestrator::extract_targets() {
    if (!build_ast_ || !build_ast_->targets) {
        add_error("No targets defined in build file");
        return false;
    }

    for (const auto& elem : build_ast_->targets->elements) {
        if (!elem->is_object()) continue;

        const auto& obj = elem->as_object();
        BuildTarget target;

        target.name = obj.get_string("name");
        target.type = obj.get_string("type", "binary");

        // Get sources
        if (const auto* sources = obj.get_array("sources")) {
            target.sources = sources->to_string_vector();
        }

        // Get dependencies
        if (const auto* deps = obj.get_array("deps")) {
            target.dependencies = deps->to_string_vector();
        }

        // Get flags
        if (const auto* flags = obj.get_array("flags")) {
            target.flags = flags->to_string_vector();
        }
        
        // Get link libraries (for FFI)
        if (const auto* link_libs = obj.get_array("link_libraries")) {
            target.link_libraries = link_libs->to_string_vector();
        }
        
        // Get library search paths
        if (const auto* link_paths = obj.get_array("link_paths")) {
            target.link_paths = link_paths->to_string_vector();
        }
        
        // Get compiler (for C/C++ targets)
        target.compiler = obj.get_string("compiler", "");
        
        // Get explicit output (for C libraries)
        target.output = obj.get_string("output", "");

        // Compute output path
        if (!target.output.empty()) {
            // Explicit output specified
            target.output_path = config_.output_dir / target.output;
        } else if (target.type == "binary") {
            target.output_path = config_.output_dir / target.name;
        } else if (target.type == "library" || target.type == "c_library") {
            target.output_path = config_.output_dir / ("lib" + target.name + ".a");
        } else {
            target.output_path = config_.output_dir / (target.name + ".o");
        }

        targets_.push_back(std::move(target));
    }

    result_.total_targets = targets_.size();

    if (targets_.empty()) {
        add_error("No valid targets found in build file");
        return false;
    }

    return true;
}

bool BuildOrchestrator::expand_sources() {
    for (auto& target : targets_) {
        std::vector<std::string> expanded;

        for (const auto& pattern : target.sources) {
            // Check if it's a glob pattern (contains *, **, ?, or [...])
            bool is_glob = pattern.find('*') != std::string::npos ||
                           pattern.find('?') != std::string::npos ||
                           pattern.find('[') != std::string::npos;

            if (is_glob) {
                // Use the aglob engine for full pattern support
                glob::GlobOptions opts;
                opts.files_only = true;
                opts.include_hidden = false;

                glob::GlobResult result = glob::expand_pattern(
                    config_.project_root,
                    pattern,
                    opts
                );

                if (!result.ok()) {
                    add_error("Glob expansion failed for '" + pattern + "': " +
                              result.error_message);
                    return false;
                }

                // Add matched files
                for (auto& path : result.paths) {
                    expanded.push_back(std::move(path));
                }

                if (config_.verbose && result.paths.empty()) {
                    std::cerr << "[WARN] Pattern '" << pattern
                              << "' matched no files\n";
                }
            } else {
                // Direct file path
                fs::path full_path = config_.project_root / pattern;
                if (fs::exists(full_path)) {
                    expanded.push_back(full_path.string());
                } else if (config_.verbose) {
                    std::cerr << "[WARN] Source file not found: "
                              << full_path << "\n";
                }
            }
        }

        // Sort for reproducibility (aglob does this, but merge needs it too)
        std::sort(expanded.begin(), expanded.end());

        target.sources = std::move(expanded);
    }

    return true;
}

// =============================================================================
// Dependency Extraction via Compiler API (ARIA-011)
// =============================================================================
// Uses `ariac --emit-deps` to get accurate module dependencies instead of regex.
// This ensures the build system uses the same parsing logic as the compiler.

std::vector<std::string> BuildOrchestrator::extract_dependencies_from_compiler(
    const std::string& source_file) {

    std::vector<std::string> modules;

    // Build command: ariac <file> --emit-deps
    std::ostringstream cmd;
    cmd << config_.compiler << " " << source_file << " --emit-deps 2>&1";

    // Execute command
    std::array<char, 4096> buffer;
    std::string result;

    FILE* pipe = popen(cmd.str().c_str(), "r");
    if (!pipe) {
        if (config_.verbose) {
            std::cout << "[WARN] Failed to run --emit-deps for: " << source_file << "\n";
        }
        return modules;
    }

    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        result += buffer.data();
    }

    int status = pclose(pipe);
    if (status != 0) {
        // Compiler failed - fall back to regex parsing
        if (config_.verbose) {
            std::cout << "[WARN] --emit-deps failed for " << source_file
                      << ", using fallback\n";
        }
        return extract_dependencies_fallback(source_file);
    }

    // Parse JSON output: {"source": "...", "imports": [...], "error": null}
    // Simple JSON parsing for our specific format
    size_t imports_pos = result.find("\"imports\"");
    if (imports_pos == std::string::npos) {
        return modules;
    }

    // Find the array start
    size_t array_start = result.find('[', imports_pos);
    size_t array_end = result.find(']', array_start);
    if (array_start == std::string::npos || array_end == std::string::npos) {
        return modules;
    }

    // Extract module names from the imports array
    // Format: [{"module": "std.io", "path": "..."}, ...]
    std::string imports_str = result.substr(array_start, array_end - array_start + 1);

    size_t pos = 0;
    while ((pos = imports_str.find("\"module\"", pos)) != std::string::npos) {
        // Find the module name value
        size_t colon = imports_str.find(':', pos);
        if (colon == std::string::npos) break;

        size_t quote_start = imports_str.find('"', colon + 1);
        if (quote_start == std::string::npos) break;

        size_t quote_end = imports_str.find('"', quote_start + 1);
        if (quote_end == std::string::npos) break;

        std::string module_name = imports_str.substr(quote_start + 1,
                                                      quote_end - quote_start - 1);

        // Extract first component (e.g., "std" from "std.io")
        size_t dot = module_name.find('.');
        if (dot != std::string::npos) {
            module_name = module_name.substr(0, dot);
        }

        if (!module_name.empty() &&
            std::find(modules.begin(), modules.end(), module_name) == modules.end()) {
            modules.push_back(module_name);
        }

        pos = quote_end + 1;
    }

    return modules;
}

std::vector<std::string> BuildOrchestrator::extract_dependencies_fallback(
    const std::string& source_file) {

    // Fallback: regex-based extraction for when compiler isn't available
    std::vector<std::string> modules;
    std::regex use_regex(R"(use\s+([a-zA-Z_][a-zA-Z0-9_]*(?:\.[a-zA-Z_][a-zA-Z0-9_]*)*))");

    std::ifstream file(source_file);
    if (!file) return modules;

    std::string line;
    while (std::getline(file, line)) {
        std::smatch match;
        if (std::regex_search(line, match, use_regex)) {
            std::string module_name = match[1].str();

            // Extract first component
            size_t dot = module_name.find('.');
            if (dot != std::string::npos) {
                module_name = module_name.substr(0, dot);
            }

            if (std::find(modules.begin(), modules.end(), module_name) == modules.end()) {
                modules.push_back(module_name);
            }
        }
    }

    return modules;
}

bool BuildOrchestrator::scan_dependencies() {
    // Extract dependencies using compiler's --emit-deps API (ARIA-011)
    // This uses the same parser as the compiler for accurate dependency detection

    for (const auto& target : targets_) {
        std::vector<std::string> deps = target.dependencies;

        for (const auto& source : target.sources) {
            // Use compiler API to extract dependencies
            std::vector<std::string> source_deps = extract_dependencies_from_compiler(source);

            for (const auto& dep_name : source_deps) {
                // Check if this matches another target
                for (const auto& t : targets_) {
                    if (t.name == dep_name &&
                        std::find(deps.begin(), deps.end(), dep_name) == deps.end()) {
                        deps.push_back(dep_name);
                    }
                }
            }
        }

        dependencies_[target.name] = deps;

        // Build reverse dependency map
        for (const auto& dep : deps) {
            dependents_[dep].push_back(target.name);
        }
    }

    return true;
}

bool BuildOrchestrator::build_dependency_graph() {
    // Topological sort using Kahn's algorithm
    std::unordered_map<std::string, int> in_degree;

    // Initialize in-degrees
    for (const auto& target : targets_) {
        in_degree[target.name] = 0;
    }

    for (const auto& [target, deps] : dependencies_) {
        in_degree[target] = deps.size();
    }

    // Find all nodes with in-degree 0
    std::queue<std::string> ready;
    for (const auto& [name, degree] : in_degree) {
        if (degree == 0) {
            ready.push(name);
        }
    }

    // Process nodes
    while (!ready.empty()) {
        std::string current = ready.front();
        ready.pop();
        build_order_.push_back(current);

        // Decrease in-degree of dependents
        if (dependents_.count(current)) {
            for (const auto& dependent : dependents_[current]) {
                in_degree[dependent]--;
                if (in_degree[dependent] == 0) {
                    ready.push(dependent);
                }
            }
        }
    }

    return true;
}

bool BuildOrchestrator::detect_cycles() {
    // Check if all targets are in build order (if not, there's a cycle)
    if (build_order_.size() != targets_.size()) {
        // Find the cycle
        std::unordered_set<std::string> processed(build_order_.begin(), build_order_.end());

        for (const auto& target : targets_) {
            if (processed.find(target.name) == processed.end()) {
                result_.cycle_path.push_back(target.name);

                // Trace the cycle
                std::string current = target.name;
                std::unordered_set<std::string> visited;

                while (visited.find(current) == visited.end()) {
                    visited.insert(current);
                    if (dependencies_.count(current) && !dependencies_[current].empty()) {
                        for (const auto& dep : dependencies_[current]) {
                            if (processed.find(dep) == processed.end()) {
                                result_.cycle_path.push_back(dep);
                                current = dep;
                                break;
                            }
                        }
                    } else {
                        break;
                    }
                }

                break;
            }
        }

        std::string cycle_str;
        for (size_t i = 0; i < result_.cycle_path.size(); ++i) {
            if (i > 0) cycle_str += " -> ";
            cycle_str += result_.cycle_path[i];
        }
        add_error("Dependency cycle detected: " + cycle_str);
        return false;
    }

    return true;
}

bool BuildOrchestrator::mark_dirty_targets() {
    dirty_targets_.clear();

    // Set toolchain info
    state_.set_toolchain(ToolchainInfo(config_.compiler));

    for (const auto& target : targets_) {
        if (config_.force_rebuild) {
            dirty_targets_.insert(target.name);
            continue;
        }

        // Collect all flags for this target
        std::vector<std::string> all_flags = config_.global_flags;
        all_flags.insert(all_flags.end(), target.flags.begin(), target.flags.end());

        // Check if target is dirty
        DirtyReason reason = state_.check_dirty(
            target.name,
            target.output_path,
            target.sources,
            all_flags
        );

        if (reason != DirtyReason::CLEAN) {
            dirty_targets_.insert(target.name);

            // Mark dependents as dirty too
            std::queue<std::string> to_mark;
            for (const auto& dep : dependents_[target.name]) {
                to_mark.push(dep);
            }

            while (!to_mark.empty()) {
                std::string name = to_mark.front();
                to_mark.pop();

                if (dirty_targets_.find(name) == dirty_targets_.end()) {
                    dirty_targets_.insert(name);
                    if (dependents_.count(name)) {
                        for (const auto& d : dependents_[name]) {
                            to_mark.push(d);
                        }
                    }
                }
            }
        }
    }

    result_.skipped_targets = targets_.size() - dirty_targets_.size();
    return true;
}

bool BuildOrchestrator::execute_builds() {
    if (dirty_targets_.empty()) {
        if (config_.verbose) {
            report_progress(BuildPhase::COMPLETE, 0, 0, "", "Nothing to build - all targets up to date");
        }
        return true;
    }

    // Create output directory
    std::error_code ec;
    fs::create_directories(config_.output_dir, ec);

    // For single-threaded or dry-run, use simple sequential build
    if (config_.num_threads == 1 || config_.dry_run) {
        return execute_builds_sequential();
    }

    // Parallel build with dependency tracking
    return execute_builds_parallel();
}

bool BuildOrchestrator::execute_builds_sequential() {
    size_t built = 0;
    for (const auto& target_name : build_order_) {
        if (cancelled_) {
            add_error("Build cancelled");
            return false;
        }

        if (dirty_targets_.find(target_name) == dirty_targets_.end()) {
            continue;
        }

        const BuildTarget* target = nullptr;
        for (const auto& t : targets_) {
            if (t.name == target_name) {
                target = &t;
                break;
            }
        }
        if (!target) continue;

        report_progress(BuildPhase::COMPILING, built, dirty_targets_.size(), target_name,
                        "Building " + target_name + "...");

        if (!config_.dry_run) {
            if (!build_single_target(*target)) {
                if (config_.fail_fast) return false;
            }
        } else {
            if (config_.verbose) {
                std::cout << "[DRY RUN] Would build: " << target_name << "\n";
                for (const auto& src : target->sources) {
                    std::cout << "  Source: " << src << "\n";
                }
                std::cout << "  Output: " << target->output_path << "\n";
            }
            result_.built_targets++;
        }
        built++;
    }
    return result_.failed_targets == 0;
}

bool BuildOrchestrator::execute_builds_parallel() {
    // Build dependency count map (how many deps each target has)
    std::unordered_map<std::string, std::atomic<int>> dep_count;
    std::unordered_map<std::string, std::vector<std::string>> reverse_deps;

    // Initialize dependency counts for dirty targets only
    for (const auto& target_name : dirty_targets_) {
        dep_count[target_name] = 0;
    }

    // Count dependencies (only count dirty deps)
    for (const auto& target_name : dirty_targets_) {
        if (dependencies_.count(target_name)) {
            for (const auto& dep : dependencies_[target_name]) {
                if (dirty_targets_.count(dep)) {
                    dep_count[target_name]++;
                    reverse_deps[dep].push_back(target_name);
                }
            }
        }
    }

    // Thread-safe state for parallel execution
    std::mutex result_mutex;
    std::atomic<size_t> built_count{0};
    std::atomic<bool> has_failure{false};
    std::condition_variable ready_cv;
    std::mutex ready_mutex;
    std::queue<std::string> ready_queue;

    // Find initially ready targets (no dirty deps)
    for (const auto& target_name : dirty_targets_) {
        if (dep_count[target_name] == 0) {
            std::lock_guard<std::mutex> lock(ready_mutex);
            ready_queue.push(target_name);
        }
    }

    // Create thread pool
    ThreadPool pool(config_.num_threads);
    size_t total_dirty = dirty_targets_.size();

    // Worker function to build a single target
    auto build_task = [&](const std::string& target_name) {
        if (cancelled_ || (config_.fail_fast && has_failure)) {
            return;
        }

        // Find target
        const BuildTarget* target = nullptr;
        for (const auto& t : targets_) {
            if (t.name == target_name) {
                target = &t;
                break;
            }
        }
        if (!target) return;

        // Report progress
        {
            std::lock_guard<std::mutex> lock(result_mutex);
            report_progress(BuildPhase::COMPILING, built_count, total_dirty,
                            target_name, "Building " + target_name + "...");
        }

        // Build the target
        bool success = build_single_target(*target);

        {
            std::lock_guard<std::mutex> lock(result_mutex);
            if (!success) {
                has_failure = true;
            }
        }

        built_count++;

        // Notify dependents that this target is complete
        if (reverse_deps.count(target_name)) {
            for (const auto& dependent : reverse_deps[target_name]) {
                int remaining = --dep_count[dependent];
                if (remaining == 0) {
                    // This dependent is now ready to build
                    {
                        std::lock_guard<std::mutex> lock(ready_mutex);
                        ready_queue.push(dependent);
                    }
                    ready_cv.notify_one();
                }
            }
        }
    };

    // Main scheduling loop
    while (built_count < total_dirty && !cancelled_ &&
           !(config_.fail_fast && has_failure)) {

        std::string next_target;
        {
            std::unique_lock<std::mutex> lock(ready_mutex);
            if (ready_queue.empty()) {
                // Wait for more work or completion
                ready_cv.wait_for(lock, std::chrono::milliseconds(100));
                continue;
            }
            next_target = ready_queue.front();
            ready_queue.pop();
        }

        pool.enqueue([&, target = next_target]() {
            build_task(target);
            ready_cv.notify_one();
        });
    }

    // Wait for all in-flight builds to complete
    pool.wait_all();

    if (cancelled_) {
        add_error("Build cancelled");
        return false;
    }

    return result_.failed_targets == 0;
}

bool BuildOrchestrator::build_single_target(const BuildTarget& target) {
    auto compile_start = std::chrono::steady_clock::now();

    std::string stdout_out, stderr_out;
    std::vector<std::string> all_flags = config_.global_flags;
    all_flags.insert(all_flags.end(), target.flags.begin(), target.flags.end());

    int result;
    
    // Route to appropriate compiler based on target type
    if (target.type == "c_library") {
        // C/C++ library compilation
        result = build_c_library(target, all_flags, stdout_out, stderr_out);
    } else if (target.type == "library") {
        // Aria library (requires ariac -c support)
        result = build_library(target, all_flags, stdout_out, stderr_out);
    } else {
        // Binary target - may need linking flags
        std::vector<std::string> link_flags = all_flags;
        
        // Add linking flags for FFI
        for (const auto& lib_path : target.link_paths) {
            // Convert relative paths to absolute based on project root
            fs::path full_path;
            if (fs::path(lib_path).is_absolute()) {
                full_path = lib_path;
            } else {
                full_path = config_.project_root / lib_path;
            }
            link_flags.push_back("-L" + full_path.string());
        }
        for (const auto& lib : target.link_libraries) {
            link_flags.push_back("-l" + lib);
        }
        
        result = execute_compile(
            target.name,
            target.sources,
            target.output_path,
            link_flags,
            stdout_out,
            stderr_out
        );
    }

    auto compile_end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        compile_end - compile_start);

    if (result != 0) {
        add_error("Failed to build " + target.name + ": " + stderr_out);
        result_.failed_targets++;
        return false;
    }

    // Update state (thread-safe - StateManager uses mutex)
    std::vector<DependencyInfo> deps;
    std::vector<std::string> impl_deps;

    state_.update_record(
        target.name,
        target.output_path,
        target.sources,
        deps,
        impl_deps,
        all_flags,
        duration.count()
    );

    result_.built_targets++;
    result_.target_times.emplace_back(target.name, duration);
    return true;
}

bool BuildOrchestrator::save_state() {
    return state_.save();
}

// =============================================================================
// Helper Functions
// =============================================================================

int BuildOrchestrator::execute_compile(
    const std::string& target_name,
    const std::vector<std::string>& sources,
    const fs::path& output,
    const std::vector<std::string>& flags,
    std::string& stdout_out,
    std::string& stderr_out) {

    try {
        // Create compiler interface
        aria_make::CompilerInterface compiler(config_.compiler);

        // Build compilation task
        aria_make::CompilerInterface::CompileTask task;
        task.sources = sources;
        task.output = output.string();
        task.flags = flags;

        if (config_.verbose) {
            std::cout << "[CMD] " << config_.compiler;
            for (const auto& src : sources) {
                std::cout << " " << src;
            }
            std::cout << " -o " << output.string();
            for (const auto& flag : flags) {
                std::cout << " " << flag;
            }
            std::cout << "\n";
        }

        // Execute compilation
        auto result = compiler.compile(task);

        stdout_out = result.stdout_output;
        stderr_out = result.stderr_output;

        if (config_.verbose && result.exit_code == 0) {
            std::cout << "[OK] " << target_name << " compiled in " 
                      << result.duration.count() << "ms\n";
        }

        return result.exit_code;

    } catch (const std::exception& e) {
        stderr_out = std::string("Compiler invocation failed: ") + e.what();
        return -1;
    }
}

int BuildOrchestrator::build_library(
    const BuildTarget& target,
    const std::vector<std::string>& flags,
    std::string& stdout_out,
    std::string& stderr_out) {

    // Create objects directory for intermediate files
    fs::path obj_dir = config_.output_dir / "obj" / target.name;
    std::error_code ec;
    fs::create_directories(obj_dir, ec);

    std::vector<std::string> object_files;

    // Step 1: Compile each source to object file
    try {
        aria_make::CompilerInterface compiler(config_.compiler);

        for (const auto& source : target.sources) {
            fs::path src_path(source);
            fs::path obj_path = obj_dir / (src_path.stem().string() + ".o");

            // Build compilation task for object file
            aria_make::CompilerInterface::CompileTask task;
            task.sources = {source};
            task.output = obj_path.string();
            
            // Add -c flag for object file compilation (if supported by ariac)
            task.flags = flags;
            task.flags.push_back("-c");

            if (config_.verbose) {
                std::cout << "[CMD] " << config_.compiler << " -c";
                for (const auto& flag : flags) {
                    std::cout << " " << flag;
                }
                std::cout << " -o " << obj_path.string();
                std::cout << " " << source << "\n";
            }

            // Execute compilation
            auto result = compiler.compile(task);

            if (result.exit_code != 0) {
                stderr_out = result.stderr_output;
                return result.exit_code;
            }

            object_files.push_back(obj_path.string());
        }

    } catch (const std::exception& e) {
        stderr_out = std::string("Library compilation failed: ") + e.what();
        return -1;
    }

    // Step 2: Create static library with ar
    std::ostringstream ar_cmd;
    ar_cmd << "ar rcs " << target.output_path.string();

    for (const auto& obj : object_files) {
        ar_cmd << " " << obj;
    }

    ar_cmd << " 2>&1";

    if (config_.verbose) {
        std::cout << "[CMD] " << ar_cmd.str() << "\n";
    }

    // Execute ar
    std::array<char, 128> buffer;
    std::string result;

    FILE* pipe = popen(ar_cmd.str().c_str(), "r");
    if (!pipe) {
        stderr_out = "Failed to execute ar archiver";
        return -1;
    }

    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        result += buffer.data();
    }

    int status = pclose(pipe);
    if (status != 0) {
        stderr_out = result;
    } else {
        stdout_out = "Created library: " + target.output_path.string();
    }

    return status;
}

std::vector<std::string> BuildOrchestrator::build_command(const BuildTarget& target) {
    std::vector<std::string> cmd;
    cmd.push_back(config_.compiler);

    // Add global flags
    cmd.insert(cmd.end(), config_.global_flags.begin(), config_.global_flags.end());

    // Add target flags
    cmd.insert(cmd.end(), target.flags.begin(), target.flags.end());

    // Add output
    cmd.push_back("-o");
    cmd.push_back(target.output_path.string());

    // Add sources
    cmd.insert(cmd.end(), target.sources.begin(), target.sources.end());

    return cmd;
}

void BuildOrchestrator::report_progress(BuildPhase phase, size_t current, size_t total,
                                         const std::string& target,
                                         const std::string& message) {
    if (progress_cb_) {
        BuildProgress progress;
        progress.phase = phase;
        progress.current = current;
        progress.total = total;
        progress.current_target = target;
        progress.message = message;
        progress_cb_(progress);
    }
}

void BuildOrchestrator::add_error(const std::string& error) {
    result_.errors.push_back(error);
}

// =============================================================================
// C/C++ Compilation Support
// =============================================================================

bool BuildOrchestrator::is_c_source(const std::string& path) const {
    fs::path p(path);
    std::string ext = p.extension().string();
    return ext == ".c" || ext == ".C";
}

bool BuildOrchestrator::is_cpp_source(const std::string& path) const {
    fs::path p(path);
    std::string ext = p.extension().string();
    return ext == ".cpp" || ext == ".cc" || ext == ".cxx" || 
           ext == ".C++" || ext == ".CPP";
}

bool BuildOrchestrator::is_aria_source(const std::string& path) const {
    fs::path p(path);
    return p.extension() == ".aria";
}

std::string BuildOrchestrator::detect_c_compiler(const BuildTarget& target) const {
    // Explicit compiler specified
    if (!target.compiler.empty()) {
        // If it's a full path, use it directly
        if (target.compiler[0] == '/' || target.compiler.find('/') != std::string::npos) {
            return target.compiler;
        }
        // Otherwise map to full path
        if (target.compiler == "gcc") return "/usr/bin/gcc";
        if (target.compiler == "g++") return "/usr/bin/g++";
        if (target.compiler == "clang") return "/usr/bin/clang";
        if (target.compiler == "clang++") return "/usr/bin/clang++";
        // Fallback: assume it's a full path
        return target.compiler;
    }
    
    // Detect from source files
    for (const auto& src : target.sources) {
        if (is_cpp_source(src)) {
            return "/usr/bin/g++";  // C++ sources need g++/clang++
        }
    }
    
    return "/usr/bin/gcc";  // Default to gcc for C
}

int BuildOrchestrator::build_c_library(
    const BuildTarget& target,
    const std::vector<std::string>& flags,
    std::string& stdout_out,
    std::string& stderr_out) {
    
    try {
        std::string compiler_path = detect_c_compiler(target);
        bool is_cpp = is_cpp_source(target.sources[0]);
        
        aria_make::CCompilerInterface compiler(compiler_path, is_cpp);
        
        // Create objects directory
        fs::path obj_dir = config_.output_dir / "obj" / target.name;
        std::error_code ec;
        fs::create_directories(obj_dir, ec);
        
        std::vector<std::string> object_files;
        
        // Step 1: Compile each source to object file
        for (const auto& source : target.sources) {
            fs::path src_path(source);
            fs::path obj_path = obj_dir / (src_path.stem().string() + ".o");
            
            aria_make::CCompilerInterface::CompileTask task;
            task.sources = {source};
            task.output = obj_path.string();
            task.compile_only = true;
            task.position_independent = true;  // -fPIC for libraries
            task.flags = flags;
            
            if (config_.verbose) {
                std::cout << "[C] " << compiler_path << " -c -fPIC";
                for (const auto& flag : flags) {
                    std::cout << " " << flag;
                }
                std::cout << " -o " << obj_path.string();
                std::cout << " " << source << "\n";
            }
            
            auto result = compiler.compile(task);
            
            if (result.exit_code != 0) {
                stderr_out = result.stderr_output;
                return result.exit_code;
            }
            
            object_files.push_back(obj_path.string());
        }
        
        // Step 2: Create static library from objects
        aria_make::CCompilerInterface::LibraryTask lib_task;
        lib_task.objects = object_files;
        lib_task.output = target.output_path.string();
        
        if (config_.verbose) {
            std::cout << "[AR] ar rcs " << target.output_path.string();
            for (const auto& obj : object_files) {
                std::cout << " " << obj;
            }
            std::cout << "\n";
        }
        
        auto lib_result = compiler.create_static_library(lib_task);
        
        stdout_out = lib_result.stdout_output;
        stderr_out = lib_result.stderr_output;
        
        if (config_.verbose && lib_result.exit_code == 0) {
            std::cout << "[OK] " << target.name << " (C library) built\n";
        }
        
        return lib_result.exit_code;
        
    } catch (const std::exception& e) {
        stderr_out = std::string("C compiler invocation failed: ") + e.what();
        return -1;
    }
}

// =============================================================================
// Convenience Functions
// =============================================================================

BuildResult build_project(const fs::path& project_dir, const BuildConfig& config) {
    BuildConfig cfg = config;
    cfg.project_root = project_dir;

    BuildOrchestrator orchestrator(cfg);
    return orchestrator.build();
}

bool clean_project(const fs::path& project_dir) {
    BuildConfig cfg;
    cfg.project_root = project_dir;

    BuildOrchestrator orchestrator(cfg);
    return orchestrator.clean();
}

} // namespace aria::make
