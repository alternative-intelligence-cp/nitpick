/**
 * incremental.cpp
 * Incremental Build Logic implementation
 *
 * Copyright (c) 2025 Aria Language Project
 */

#include "depgraph/incremental.hpp"
#include <algorithm>

namespace aria {
namespace depgraph {

const char* dirty_reason_string(DirtyReason reason) {
    switch (reason) {
        case DirtyReason::OUTPUT_MISSING:    return "OUTPUT_MISSING";
        case DirtyReason::SOURCE_NEWER:      return "SOURCE_NEWER";
        case DirtyReason::DEPENDENCY_DIRTY:  return "DEPENDENCY_DIRTY";
        case DirtyReason::FORCE_REBUILD:     return "FORCE_REBUILD";
        case DirtyReason::CONFIG_CHANGED:    return "CONFIG_CHANGED";
    }
    return "UNKNOWN";
}

IncrementalChecker::IncrementalChecker(DependencyGraph& graph)
    : graph_(graph) {}

IncrementalResult IncrementalChecker::check() {
    IncrementalResult result;

    // Refresh timestamps from filesystem
    graph_.refresh_timestamps();

    // Clear checked set for new check
    checked_.clear();
    dirty_reasons_.clear();

    auto nodes = graph_.all_nodes();
    result.total_nodes = nodes.size();

    for (DependencyNode* node : nodes) {
        if (is_dirty(node)) {
            result.dirty_nodes.push_back(node);
            result.rebuild_count++;
            node->status = BuildStatus::PENDING;
        } else {
            result.clean_nodes.push_back(node);
            result.skip_count++;
            node->status = BuildStatus::SKIPPED;
        }
    }

    return result;
}

bool IncrementalChecker::is_dirty(DependencyNode* node) {
    if (!node) return false;

    // Check cache
    if (checked_.count(node->id)) {
        return dirty_reasons_.count(node->id) > 0;
    }
    checked_.insert(node->id);

    // Force rebuild mode
    if (force_rebuild_) {
        dirty_reasons_[node->id] = DirtyReason::FORCE_REBUILD;
        return true;
    }

    // Only check targets and intermediates
    if (node->type == NodeType::SOURCE) {
        return false;  // Sources are never "built"
    }

    // Check if output exists
    if (!node->output_path.empty() && !fs::exists(node->output_path)) {
        dirty_reasons_[node->id] = DirtyReason::OUTPUT_MISSING;
        return true;
    }

    // Check timestamps
    if (check_timestamps(node)) {
        dirty_reasons_[node->id] = DirtyReason::SOURCE_NEWER;
        return true;
    }

    // Check dependencies
    if (check_dependencies(node)) {
        dirty_reasons_[node->id] = DirtyReason::DEPENDENCY_DIRTY;
        return true;
    }

    return false;
}

DirtyReason IncrementalChecker::get_dirty_reason(DependencyNode* node) {
    auto it = dirty_reasons_.find(node->id);
    if (it != dirty_reasons_.end()) {
        return it->second;
    }
    // If not dirty, return a default (shouldn't normally be called)
    return DirtyReason::OUTPUT_MISSING;
}

void IncrementalChecker::invalidate_all() {
    for (DependencyNode* node : graph_.all_nodes()) {
        dirty_reasons_[node->id] = DirtyReason::FORCE_REBUILD;
        node->status = BuildStatus::PENDING;
    }
}

bool IncrementalChecker::check_timestamps(DependencyNode* node) {
    fs::file_time_type output_time = get_output_timestamp(node);
    fs::file_time_type newest_source = get_newest_source(node);

    // If any source is newer than output, need rebuild
    return newest_source > output_time;
}

bool IncrementalChecker::check_dependencies(DependencyNode* node) {
    for (DependencyNode* dep : node->dependencies) {
        if (is_dirty(dep)) {
            return true;
        }
    }
    return false;
}

fs::file_time_type IncrementalChecker::get_newest_source(DependencyNode* node) {
    fs::file_time_type newest{};

    // Check source files
    for (const fs::path& source : node->source_files) {
        if (fs::exists(source)) {
            std::error_code ec;
            auto mtime = fs::last_write_time(source, ec);
            if (!ec && mtime > newest) {
                newest = mtime;
            }
        }
    }

    // Check dependencies' outputs
    for (DependencyNode* dep : node->dependencies) {
        if (dep->timestamp_valid && dep->timestamp > newest) {
            newest = dep->timestamp;
        }
    }

    return newest;
}

fs::file_time_type IncrementalChecker::get_output_timestamp(DependencyNode* node) {
    if (node->output_path.empty()) {
        return {};  // Epoch - always dirty
    }

    if (!fs::exists(node->output_path)) {
        return {};  // Epoch - always dirty
    }

    std::error_code ec;
    return fs::last_write_time(node->output_path, ec);
}

} // namespace depgraph
} // namespace aria
