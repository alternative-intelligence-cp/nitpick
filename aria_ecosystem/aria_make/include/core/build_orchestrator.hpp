/**
 * build_orchestrator.hpp
 * Central Build Orchestrator for aria_make
 *
 * Integrates:
 * - ABC ConfigParser for reading build.abc files
 * - StateManager for incremental build state tracking
 * - DependencyGraph for dependency analysis and cycle detection
 * - Parallel execution via thread pool
 * - Compiler API integration (ariac --emit-deps) for accurate dependency extraction
 *
 * Build Flow:
 * 1. Parse build.abc -> BuildFileNode AST
 * 2. Load previous build state from .aria_make/state.json
 * 3. Build DependencyGraph from targets + extract deps via compiler --emit-deps (ARIA-011)
 * 4. Detect cycles (abort if found)
 * 5. Mark dirty nodes based on StateManager analysis (content hashing)
 * 6. Topological sort for build order
 * 7. Execute parallel builds via thread pool
 * 8. Update and save build state
 *
 * Ecosystem Integration:
 * - Uses ariac --emit-deps for accurate `use` statement parsing (ecosystem/03_DependencyGraph)
 * - Falls back to regex if compiler unavailable (for testing without compiler)
 * - StateManager uses content hashing for cache correctness (ecosystem/02_StateManager)
 *
 * Copyright (c) 2025 Aria Language Project
 */

#ifndef ARIA_MAKE_BUILD_ORCHESTRATOR_HPP
#define ARIA_MAKE_BUILD_ORCHESTRATOR_HPP

#include "state/state_manager.hpp"
#include <filesystem>
#include <vector>
#include <string>
#include <functional>
#include <memory>
#include <chrono>
#include <atomic>

namespace aria::make {

namespace fs = std::filesystem;

// =============================================================================
// Forward Declarations (avoid header dependencies)
// =============================================================================
namespace abc {
    struct BuildFileNode;
    struct ObjectNode;
    struct ArrayNode;
}

// =============================================================================
// Build Configuration
// =============================================================================
struct BuildConfig {
    // Project root directory
    fs::path project_root;

    // Build file path (default: build.abc)
    fs::path build_file = "build.abc";

    // Output directory (default: .aria_make/build)
    fs::path output_dir = ".aria_make/build";

    // State directory (default: .aria_make)
    fs::path state_dir = ".aria_make";

    // Compiler path (default: search PATH for ariac, or use full path)
    std::string compiler = "/home/randy/._____RANDY_____/REPOS/aria/build/ariac";

    // Global compiler flags
    std::vector<std::string> global_flags;

    // Parallel execution
    size_t num_threads = 0;  // 0 = auto (hardware_concurrency)

    // Build behavior
    bool force_rebuild = false;       // Ignore incremental state
    bool fail_fast = true;            // Stop on first error
    bool continue_on_error = false;   // Build as much as possible
    bool dry_run = false;             // Print commands, don't execute
    bool verbose = false;             // Detailed output
    bool quiet = false;               // Minimal output

    // Target selection (empty = build all)
    std::vector<std::string> targets;
};

// =============================================================================
// Build Target (extracted from ABC AST)
// =============================================================================
struct BuildTarget {
    std::string name;
    std::string type;                      // "binary", "library", "object", "c_library"
    std::vector<std::string> sources;      // Source patterns (globs or files)
    std::vector<std::string> dependencies; // Other targets this depends on
    std::vector<std::string> flags;        // Target-specific flags
    fs::path output_path;                  // Computed output path
    
    // Linking support (for FFI and C libraries)
    std::vector<std::string> link_libraries; // Libraries to link (-l)
    std::vector<std::string> link_paths;     // Library search paths (-L)
    
    // C/C++ compilation support
    std::string compiler;                  // "gcc", "g++", "clang", "clang++" (for c_library)
    std::string output;                    // Explicit output filename (overrides computed path)
};

// =============================================================================
// Build Result
// =============================================================================
struct BuildResult {
    bool success = false;

    size_t total_targets = 0;
    size_t built_targets = 0;
    size_t skipped_targets = 0;  // Up-to-date
    size_t failed_targets = 0;

    std::chrono::milliseconds total_time{0};
    std::chrono::milliseconds compile_time{0};  // Actual compilation time

    // Errors encountered
    std::vector<std::string> errors;

    // Cycle information (if detected)
    bool has_cycle = false;
    std::vector<std::string> cycle_path;

    // Per-target timing (for profiling)
    std::vector<std::pair<std::string, std::chrono::milliseconds>> target_times;

    // Cache statistics
    double cache_hit_rate() const {
        if (total_targets == 0) return 0.0;
        return static_cast<double>(skipped_targets) / total_targets;
    }
};

// =============================================================================
// Progress Callback
// =============================================================================
enum class BuildPhase {
    PARSING,           // Reading build.abc
    LOADING_STATE,     // Loading previous build state
    ANALYZING,         // Building dependency graph
    CHECKING_DIRTY,    // Determining what needs rebuild
    COMPILING,         // Executing builds
    SAVING_STATE,      // Saving new build state
    COMPLETE
};

struct BuildProgress {
    BuildPhase phase = BuildPhase::PARSING;
    size_t current = 0;
    size_t total = 0;
    std::string current_target;
    std::string message;
};

using ProgressCallback = std::function<void(const BuildProgress&)>;

// =============================================================================
// Build Orchestrator
// =============================================================================
class BuildOrchestrator {
public:
    /**
     * Create orchestrator with configuration.
     */
    explicit BuildOrchestrator(BuildConfig config);

    ~BuildOrchestrator();

    // No copying
    BuildOrchestrator(const BuildOrchestrator&) = delete;
    BuildOrchestrator& operator=(const BuildOrchestrator&) = delete;

    // =========================================================================
    // Build Operations
    // =========================================================================

    /**
     * Execute the build.
     * Returns BuildResult with success/failure and statistics.
     */
    BuildResult build();

    /**
     * Clean build artifacts.
     * Removes output directory and state file.
     */
    bool clean();

    /**
     * Rebuild all (clean + build).
     */
    BuildResult rebuild();

    /**
     * Check what would be built (dry run).
     */
    BuildResult check();

    // =========================================================================
    // Configuration
    // =========================================================================

    /**
     * Set progress callback for UI updates.
     */
    void set_progress_callback(ProgressCallback cb) { progress_cb_ = std::move(cb); }

    /**
     * Get current configuration.
     */
    const BuildConfig& config() const { return config_; }

    /**
     * Get the state manager (for external queries).
     */
    StateManager& state_manager() { return state_; }
    const StateManager& state_manager() const { return state_; }

    // =========================================================================
    // Queries
    // =========================================================================

    /**
     * Get list of all targets from build.abc.
     */
    std::vector<BuildTarget> list_targets() const;

    /**
     * Get dependency graph as DOT format for visualization.
     */
    std::string dependency_graph_dot() const;

    /**
     * Cancel the current build.
     */
    void cancel();

    /**
     * Check if build was cancelled.
     */
    bool cancelled() const { return cancelled_; }

private:
    // =========================================================================
    // Build Pipeline Stages
    // =========================================================================

    // Stage 1: Parse build.abc
    bool parse_build_file();

    // Stage 2: Extract targets from AST
    bool extract_targets();

    // Stage 3: Expand source patterns (glob)
    bool expand_sources();

    // Stage 4: Scan .aria files for 'use' dependencies
    bool scan_dependencies();

    // Stage 5: Build dependency graph
    bool build_dependency_graph();

    // Stage 6: Detect cycles
    bool detect_cycles();

    // Stage 7: Mark dirty targets
    bool mark_dirty_targets();

    // Stage 8: Execute builds
    bool execute_builds();
    bool execute_builds_sequential();  // Single-threaded fallback
    bool execute_builds_parallel();    // Multi-threaded with dependency tracking

    // Build a single target (used by both sequential and parallel)
    bool build_single_target(const BuildTarget& target);

    // Stage 9: Save build state
    bool save_state();

    // =========================================================================
    // Helper Functions
    // =========================================================================

    // Extract dependencies using compiler's --emit-deps API (ARIA-011)
    // This uses the same parser as the compiler for accurate dependency detection
    std::vector<std::string> extract_dependencies_from_compiler(const std::string& source_file);

    // Fallback regex-based dependency extraction (when compiler unavailable)
    std::vector<std::string> extract_dependencies_fallback(const std::string& source_file);

    // Execute a single compile command
    int execute_compile(const std::string& target_name,
                        const std::vector<std::string>& sources,
                        const fs::path& output,
                        const std::vector<std::string>& flags,
                        std::string& stdout_out,
                        std::string& stderr_out);

    // Build a static library (compile sources + ar archive)
    int build_library(const BuildTarget& target,
                      const std::vector<std::string>& flags,
                      std::string& stdout_out,
                      std::string& stderr_out);
    
    // Build a C/C++ library (compile C sources + ar archive)
    int build_c_library(const BuildTarget& target,
                        const std::vector<std::string>& flags,
                        std::string& stdout_out,
                        std::string& stderr_out);

    // Build the compile command for a target
    std::vector<std::string> build_command(const BuildTarget& target);
    
    // File type detection
    bool is_c_source(const std::string& path) const;
    bool is_cpp_source(const std::string& path) const;
    bool is_aria_source(const std::string& path) const;
    
    // Detect appropriate C/C++ compiler for target
    std::string detect_c_compiler(const BuildTarget& target) const;

    // Report progress to callback
    void report_progress(BuildPhase phase, size_t current, size_t total,
                         const std::string& target = "",
                         const std::string& message = "");

    // Add an error to the result
    void add_error(const std::string& error);

    // =========================================================================
    // Member Data
    // =========================================================================

    BuildConfig config_;
    StateManager state_;
    ProgressCallback progress_cb_;

    // Parsed build file
    std::unique_ptr<abc::BuildFileNode> build_ast_;

    // Extracted targets
    std::vector<BuildTarget> targets_;

    // Dependency information (target -> dependencies)
    std::unordered_map<std::string, std::vector<std::string>> dependencies_;

    // Reverse dependencies (target -> dependents)
    std::unordered_map<std::string, std::vector<std::string>> dependents_;

    // Targets that need rebuilding
    std::unordered_set<std::string> dirty_targets_;

    // Build order (topologically sorted)
    std::vector<std::string> build_order_;

    // Current result being built
    BuildResult result_;

    // Cancellation flag
    std::atomic<bool> cancelled_{false};

    // Build start time
    std::chrono::steady_clock::time_point start_time_;
};

// =============================================================================
// Convenience Functions
// =============================================================================

/**
 * Build a project in the given directory.
 */
BuildResult build_project(const fs::path& project_dir,
                          const BuildConfig& config = {});

/**
 * Clean a project.
 */
bool clean_project(const fs::path& project_dir);

} // namespace aria::make

#endif // ARIA_MAKE_BUILD_ORCHESTRATOR_HPP
