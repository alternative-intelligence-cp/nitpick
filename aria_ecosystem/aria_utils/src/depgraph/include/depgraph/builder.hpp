/**
 * builder.hpp
 * Build Scheduler and Executor for AriaBuild
 *
 * Orchestrates parallel builds using:
 * - Topological ordering
 * - Thread pool execution
 * - Progress reporting
 * - Error handling
 *
 * Copyright (c) 2025 Aria Language Project
 */

#ifndef ARIA_DEPGRAPH_BUILDER_HPP
#define ARIA_DEPGRAPH_BUILDER_HPP

#include "graph.hpp"
#include "scheduler.hpp"
#include "incremental.hpp"
#include "threadpool.hpp"
#include <functional>
#include <chrono>
#include <atomic>

namespace aria {
namespace depgraph {

// -----------------------------------------------------------------------------
// Build Options
// -----------------------------------------------------------------------------
struct BuildOptions {
    // Parallel execution
    size_t num_threads = 0;         // 0 = auto (hardware_concurrency)

    // Error handling
    bool fail_fast = true;          // Stop on first error
    bool continue_on_error = false; // Build as much as possible

    // Incremental builds
    bool force_rebuild = false;     // Ignore timestamps
    bool check_timestamps = true;   // Use incremental logic

    // Output
    bool verbose = false;           // Detailed logging
    bool quiet = false;             // Minimal output
    bool show_timing = true;        // Show build times

    // Compiler
    std::string compiler = "ariac";
    std::vector<std::string> extra_flags;
};

// -----------------------------------------------------------------------------
// Build Statistics
// -----------------------------------------------------------------------------
struct BuildStats {
    size_t total_targets = 0;
    size_t built = 0;
    size_t skipped = 0;
    size_t failed = 0;

    std::chrono::milliseconds total_time{0};
    std::chrono::milliseconds longest_task{0};
    std::string longest_task_name;

    bool success() const { return failed == 0; }
};

// -----------------------------------------------------------------------------
// Build Progress Callback
// -----------------------------------------------------------------------------
struct BuildProgress {
    size_t current = 0;
    size_t total = 0;
    std::string target_name;
    BuildStatus status = BuildStatus::PENDING;
    std::chrono::milliseconds elapsed{0};
};

using ProgressCallback = std::function<void(const BuildProgress&)>;

// -----------------------------------------------------------------------------
// Build Task (what gets executed for each target)
// -----------------------------------------------------------------------------
struct BuildTask {
    DependencyNode* node = nullptr;
    std::string command;
    std::vector<std::string> args;
    fs::path working_dir;
};

using TaskExecutor = std::function<int(const BuildTask&, std::string& output, std::string& error)>;

// -----------------------------------------------------------------------------
// Build Scheduler
// -----------------------------------------------------------------------------
class BuildScheduler {
public:
    /**
     * Create scheduler for a graph.
     */
    explicit BuildScheduler(DependencyGraph& graph, BuildOptions opts = {});

    ~BuildScheduler();

    /**
     * Set progress callback.
     */
    void set_progress_callback(ProgressCallback cb) { progress_cb_ = std::move(cb); }

    /**
     * Set custom task executor.
     * Default: invokes ariac via system().
     */
    void set_executor(TaskExecutor exec) { executor_ = std::move(exec); }

    /**
     * Build all targets.
     */
    BuildStats build_all();

    /**
     * Build specific targets (and their dependencies).
     */
    BuildStats build_targets(const std::vector<std::string>& target_ids);

    /**
     * Cancel the current build.
     */
    void cancel();

    /**
     * Check if build was cancelled.
     */
    bool cancelled() const { return cancelled_.load(); }

    /**
     * Get current statistics.
     */
    BuildStats current_stats() const;

private:
    DependencyGraph& graph_;
    BuildOptions options_;
    std::unique_ptr<ThreadPool> pool_;

    ProgressCallback progress_cb_;
    TaskExecutor executor_;

    std::atomic<bool> cancelled_{false};
    std::atomic<bool> failed_{false};
    std::atomic<size_t> completed_{0};
    std::atomic<size_t> skipped_{0};
    std::atomic<size_t> errors_{0};

    mutable std::mutex stats_mutex_;
    BuildStats stats_;
    std::chrono::steady_clock::time_point start_time_;

    // Execute a single build task
    void execute_task(DependencyNode* node, ReadyQueue& queue);

    // Default task executor (calls ariac)
    static int default_executor(const BuildTask& task, std::string& output, std::string& error);

    // Build the command for a target
    BuildTask make_task(DependencyNode* node);

    // Report progress
    void report_progress(DependencyNode* node, BuildStatus status);

    // Update statistics
    void update_stats(DependencyNode* node);
};

// -----------------------------------------------------------------------------
// Convenience Functions
// -----------------------------------------------------------------------------

/**
 * Simple build all targets in a graph.
 */
BuildStats build_graph(DependencyGraph& graph, const BuildOptions& opts = {});

} // namespace depgraph
} // namespace aria

#endif // ARIA_DEPGRAPH_BUILDER_HPP
