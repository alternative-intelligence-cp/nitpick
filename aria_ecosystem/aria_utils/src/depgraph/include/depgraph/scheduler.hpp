/**
 * scheduler.hpp
 * Topological Sort and Cycle Detection for AriaBuild
 *
 * Implements:
 * - Kahn's algorithm for O(V+E) topological sorting
 * - Tri-Color DFS for cycle detection and path reporting
 * - Ready queue for parallel execution scheduling
 *
 * Copyright (c) 2025 Aria Language Project
 */

#ifndef ARIA_DEPGRAPH_SCHEDULER_HPP
#define ARIA_DEPGRAPH_SCHEDULER_HPP

#include "graph.hpp"
#include <vector>
#include <queue>
#include <optional>

namespace aria {
namespace depgraph {

// -----------------------------------------------------------------------------
// Schedule Result
// -----------------------------------------------------------------------------
struct ScheduleResult {
    std::vector<DependencyNode*> order;     // Build order (topologically sorted)
    bool has_cycle = false;
    std::vector<std::string> cycle_path;    // Nodes in the cycle (if detected)
    std::string error_message;

    bool success() const { return !has_cycle && error_message.empty(); }
};

// -----------------------------------------------------------------------------
// Topological Sorter (Kahn's Algorithm)
// -----------------------------------------------------------------------------
class TopologicalSorter {
public:
    /**
     * Perform topological sort on the graph.
     *
     * Algorithm (Kahn's):
     * 1. Find all nodes with in-degree 0 (no dependencies)
     * 2. Add them to ready queue
     * 3. While queue not empty:
     *    - Dequeue node N
     *    - Add N to result
     *    - For each dependent M of N:
     *      - Decrement M's in-degree
     *      - If in-degree becomes 0, enqueue M
     * 4. If result size != node count: cycle exists
     *
     * Time: O(V + E)
     * Space: O(V)
     */
    static ScheduleResult sort(DependencyGraph& graph);

    /**
     * Sort only specific target nodes and their dependencies.
     */
    static ScheduleResult sort_targets(DependencyGraph& graph,
                                       const std::vector<std::string>& target_ids);

private:
    // Initialize in-degree counters
    static void initialize_in_degrees(DependencyGraph& graph);
};

// -----------------------------------------------------------------------------
// Cycle Detector (Tri-Color DFS)
// -----------------------------------------------------------------------------
class CycleDetector {
public:
    /**
     * Check if the graph contains cycles.
     * Uses tri-color marking for efficiency.
     *
     * Colors:
     * - WHITE: Unvisited
     * - GRAY: Currently in DFS path (visiting)
     * - BLACK: Fully processed
     *
     * A cycle is detected when we encounter a GRAY node
     * (we've come back to a node still in our current path).
     */
    static bool has_cycle(DependencyGraph& graph);

    /**
     * Find the actual cycle path for error reporting.
     * Returns the cycle as a vector of node IDs.
     */
    static std::vector<std::string> find_cycle(DependencyGraph& graph);

    /**
     * Format cycle path as user-friendly string.
     * Example: "A -> B -> C -> A"
     */
    static std::string format_cycle(const std::vector<std::string>& cycle);

private:
    // DFS visit with cycle detection
    static bool dfs_visit(DependencyNode* node,
                         std::vector<DependencyNode*>& path,
                         std::vector<std::string>& cycle_path);
};

// -----------------------------------------------------------------------------
// Ready Queue (for parallel scheduling)
// -----------------------------------------------------------------------------
class ReadyQueue {
public:
    ReadyQueue() = default;

    /**
     * Initialize from a graph (add all nodes with in-degree 0).
     */
    void initialize(DependencyGraph& graph);

    /**
     * Add a node that's ready to build.
     */
    void push(DependencyNode* node);

    /**
     * Get next ready node (nullptr if empty).
     */
    DependencyNode* pop();

    /**
     * Check if queue is empty.
     */
    bool empty() const { return queue_.empty(); }

    /**
     * Number of ready nodes.
     */
    size_t size() const { return queue_.size(); }

    /**
     * Notify that a node completed.
     * Updates dependents and enqueues any that become ready.
     * Returns the newly ready nodes.
     */
    std::vector<DependencyNode*> notify_complete(DependencyNode* node);

    /**
     * Clear the queue.
     */
    void clear() { while (!queue_.empty()) queue_.pop(); }

private:
    std::queue<DependencyNode*> queue_;
};

} // namespace depgraph
} // namespace aria

#endif // ARIA_DEPGRAPH_SCHEDULER_HPP
