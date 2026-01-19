/**
 * builder.cpp
 * Build Scheduler implementation
 *
 * Copyright (c) 2025 Aria Language Project
 */

#include "depgraph/builder.hpp"
#include <sstream>
#include <cstdlib>
#include <array>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <sys/wait.h>
#endif

namespace aria {
namespace depgraph {

BuildScheduler::BuildScheduler(DependencyGraph& graph, BuildOptions opts)
    : graph_(graph), options_(std::move(opts)) {

    size_t num_threads = options_.num_threads;
    if (num_threads == 0) {
        num_threads = std::thread::hardware_concurrency();
        if (num_threads == 0) num_threads = 4;
    }

    pool_ = std::make_unique<ThreadPool>(num_threads);

    if (!executor_) {
        executor_ = default_executor;
    }
}

BuildScheduler::~BuildScheduler() = default;

BuildStats BuildScheduler::build_all() {
    return build_targets({});  // Empty means all targets
}

BuildStats BuildScheduler::build_targets(const std::vector<std::string>& target_ids) {
    start_time_ = std::chrono::steady_clock::now();
    cancelled_.store(false);
    failed_.store(false);
    completed_.store(0);
    skipped_.store(0);
    errors_.store(0);

    // Topological sort
    ScheduleResult schedule;
    if (target_ids.empty()) {
        schedule = TopologicalSorter::sort(graph_);
    } else {
        schedule = TopologicalSorter::sort_targets(graph_, target_ids);
    }

    if (schedule.has_cycle) {
        stats_.total_targets = graph_.node_count();
        stats_.failed = 1;
        return stats_;
    }

    stats_.total_targets = schedule.order.size();

    // Check for incremental builds
    if (options_.check_timestamps && !options_.force_rebuild) {
        IncrementalChecker checker(graph_);
        auto inc_result = checker.check();

        for (DependencyNode* node : inc_result.clean_nodes) {
            node->status = BuildStatus::SKIPPED;
            skipped_.fetch_add(1);
        }
    }

    // Reset graph for this build
    graph_.reset();

    // Initialize ready queue
    ReadyQueue ready;
    ready.initialize(graph_);

    // Submit initial ready tasks
    std::mutex queue_mutex;

    while (!ready.empty() || pool_->active_count() > 0) {
        if (cancelled_.load()) break;
        if (options_.fail_fast && failed_.load()) break;

        DependencyNode* node = nullptr;
        {
            std::lock_guard<std::mutex> lock(queue_mutex);
            node = ready.pop();
        }

        if (node) {
            pool_->submit([this, node, &ready, &queue_mutex]() {
                execute_task(node, ready);
            });
        } else {
            // No ready tasks - wait a bit for running tasks to complete
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    // Wait for all tasks to complete
    pool_->wait_all();

    // Calculate final stats
    auto end_time = std::chrono::steady_clock::now();
    stats_.total_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        end_time - start_time_);
    stats_.built = completed_.load();
    stats_.skipped = skipped_.load();
    stats_.failed = errors_.load();

    return stats_;
}

void BuildScheduler::execute_task(DependencyNode* node, ReadyQueue& queue) {
    if (cancelled_.load()) {
        return;
    }

    // Check if already done
    if (node->is_complete()) {
        skipped_.fetch_add(1);
        queue.notify_complete(node);
        return;
    }

    // Check if should skip (up-to-date)
    if (node->status == BuildStatus::SKIPPED) {
        skipped_.fetch_add(1);
        report_progress(node, BuildStatus::SKIPPED);
        queue.notify_complete(node);
        return;
    }

    // Mark as building
    node->status = BuildStatus::BUILDING;
    report_progress(node, BuildStatus::BUILDING);

    auto task_start = std::chrono::steady_clock::now();

    // Execute the build task
    BuildTask task = make_task(node);
    std::string output, error;
    int result = executor_(task, output, error);

    auto task_end = std::chrono::steady_clock::now();
    node->build_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        task_end - task_start);

    if (result == 0) {
        node->status = BuildStatus::COMPLETE;
        completed_.fetch_add(1);
        report_progress(node, BuildStatus::COMPLETE);
        queue.notify_complete(node);

        // Track longest task
        if (node->build_time > stats_.longest_task) {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            if (node->build_time > stats_.longest_task) {
                stats_.longest_task = node->build_time;
                stats_.longest_task_name = node->id;
            }
        }
    } else {
        node->status = BuildStatus::FAILED;
        node->error_message = error;
        errors_.fetch_add(1);
        failed_.store(true);
        report_progress(node, BuildStatus::FAILED);

        // Don't notify dependents - they can't build
    }
}

BuildTask BuildScheduler::make_task(DependencyNode* node) {
    BuildTask task;
    task.node = node;

    // Build command based on target type
    std::ostringstream cmd;
    cmd << options_.compiler;

    // Add source files
    for (const fs::path& source : node->source_files) {
        task.args.push_back(source.string());
    }

    // Add output flag
    if (!node->output_path.empty()) {
        task.args.push_back("-o");
        task.args.push_back(node->output_path.string());
    }

    // Add include paths from dependencies
    for (DependencyNode* dep : node->dependencies) {
        if (!dep->output_path.empty()) {
            task.args.push_back("-I");
            task.args.push_back(dep->output_path.parent_path().string());
        }
    }

    // Add target-specific flags
    for (const std::string& flag : node->flags) {
        task.args.push_back(flag);
    }

    // Add extra flags from options
    for (const std::string& flag : options_.extra_flags) {
        task.args.push_back(flag);
    }

    task.command = options_.compiler;
    task.working_dir = fs::current_path();

    return task;
}

int BuildScheduler::default_executor(const BuildTask& task, std::string& output, std::string& error) {
    // Build command line
    std::ostringstream cmd;
    cmd << task.command;
    for (const std::string& arg : task.args) {
        cmd << " " << arg;
    }

    // Execute command
    std::string full_cmd = cmd.str() + " 2>&1";

#ifdef _WIN32
    FILE* pipe = _popen(full_cmd.c_str(), "r");
#else
    FILE* pipe = popen(full_cmd.c_str(), "r");
#endif

    if (!pipe) {
        error = "Failed to execute command: " + task.command;
        return -1;
    }

    std::array<char, 256> buffer;
    std::ostringstream result;
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        result << buffer.data();
    }

#ifdef _WIN32
    int status = _pclose(pipe);
#else
    int status = pclose(pipe);
    if (WIFEXITED(status)) {
        status = WEXITSTATUS(status);
    }
#endif

    output = result.str();
    if (status != 0) {
        error = output;
    }

    return status;
}

void BuildScheduler::report_progress(DependencyNode* node, BuildStatus status) {
    if (!progress_cb_) return;

    BuildProgress progress;
    progress.current = completed_.load() + skipped_.load() + errors_.load();
    progress.total = stats_.total_targets;
    progress.target_name = node->id;
    progress.status = status;
    progress.elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start_time_);

    progress_cb_(progress);
}

void BuildScheduler::cancel() {
    cancelled_.store(true);
    pool_->shutdown();
}

BuildStats BuildScheduler::current_stats() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    BuildStats current = stats_;
    current.built = completed_.load();
    current.skipped = skipped_.load();
    current.failed = errors_.load();
    current.total_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start_time_);
    return current;
}

// -----------------------------------------------------------------------------
// Convenience function
// -----------------------------------------------------------------------------

BuildStats build_graph(DependencyGraph& graph, const BuildOptions& opts) {
    BuildScheduler scheduler(graph, opts);
    return scheduler.build_all();
}

} // namespace depgraph
} // namespace aria
