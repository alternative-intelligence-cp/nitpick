/**
 * incremental.hpp
 * Incremental Build Logic for AriaBuild
 *
 * Determines which targets need rebuilding based on:
 * - File timestamps
 * - Dependency changes
 * - Missing outputs
 *
 * Copyright (c) 2025 Aria Language Project
 */

#ifndef ARIA_DEPGRAPH_INCREMENTAL_HPP
#define ARIA_DEPGRAPH_INCREMENTAL_HPP

#include "graph.hpp"
#include <vector>
#include <unordered_set>

namespace aria {
namespace depgraph {

// -----------------------------------------------------------------------------
// Incremental Check Result
// -----------------------------------------------------------------------------
struct IncrementalResult {
    std::vector<DependencyNode*> dirty_nodes;   // Nodes that need rebuilding
    std::vector<DependencyNode*> clean_nodes;   // Nodes that are up-to-date
    size_t total_nodes = 0;
    size_t rebuild_count = 0;
    size_t skip_count = 0;

    bool needs_rebuild() const { return rebuild_count > 0; }
};

// -----------------------------------------------------------------------------
// Dirty Reason
// -----------------------------------------------------------------------------
enum class DirtyReason {
    OUTPUT_MISSING,         // Output file doesn't exist
    SOURCE_NEWER,          // Source modified after output
    DEPENDENCY_DIRTY,      // A dependency needs rebuilding
    FORCE_REBUILD,         // User requested full rebuild
    CONFIG_CHANGED         // Build configuration changed
};

const char* dirty_reason_string(DirtyReason reason);

// -----------------------------------------------------------------------------
// Incremental Checker
// -----------------------------------------------------------------------------
class IncrementalChecker {
public:
    /**
     * Construct checker for a graph.
     */
    explicit IncrementalChecker(DependencyGraph& graph);

    /**
     * Check all targets and mark dirty/clean.
     */
    IncrementalResult check();

    /**
     * Check a specific target and its dependencies.
     */
    bool is_dirty(DependencyNode* node);

    /**
     * Get reason why a node is dirty.
     */
    DirtyReason get_dirty_reason(DependencyNode* node);

    /**
     * Force rebuild all targets.
     */
    void set_force_rebuild(bool force) { force_rebuild_ = force; }

    /**
     * Mark all nodes as needing rebuild.
     */
    void invalidate_all();

private:
    DependencyGraph& graph_;
    bool force_rebuild_ = false;
    std::unordered_set<std::string> checked_;
    std::unordered_map<std::string, DirtyReason> dirty_reasons_;

    // Check if output is older than any input
    bool check_timestamps(DependencyNode* node);

    // Recursively check dependencies
    bool check_dependencies(DependencyNode* node);

    // Get newest source timestamp
    fs::file_time_type get_newest_source(DependencyNode* node);

    // Get output timestamp (returns epoch if missing)
    fs::file_time_type get_output_timestamp(DependencyNode* node);
};

} // namespace depgraph
} // namespace aria

#endif // ARIA_DEPGRAPH_INCREMENTAL_HPP
