/**
 * node.hpp
 * Dependency Node for AriaBuild Dependency Graph Engine
 *
 * Represents a single entity in the build graph:
 * - Source files (.aria)
 * - Intermediate artifacts (.ll, .bc)
 * - Final targets (binaries, libraries)
 *
 * Copyright (c) 2025 Aria Language Project
 */

#ifndef ARIA_DEPGRAPH_NODE_HPP
#define ARIA_DEPGRAPH_NODE_HPP

#include <string>
#include <vector>
#include <filesystem>
#include <atomic>
#include <chrono>

namespace aria {
namespace depgraph {

namespace fs = std::filesystem;

// -----------------------------------------------------------------------------
// Node Types
// -----------------------------------------------------------------------------
enum class NodeType {
    SOURCE,         // Raw .aria source file
    INTERMEDIATE,   // Generated .ll or .bc file
    TARGET          // Final binary or library
};

const char* node_type_string(NodeType type);

// -----------------------------------------------------------------------------
// Build Status
// -----------------------------------------------------------------------------
enum class BuildStatus {
    PENDING,        // Not yet processed
    QUEUED,         // In the ready queue
    BUILDING,       // Currently being built
    COMPLETE,       // Successfully built
    SKIPPED,        // Up-to-date, no rebuild needed
    FAILED          // Build failed
};

const char* build_status_string(BuildStatus status);

// -----------------------------------------------------------------------------
// Tri-Color for Cycle Detection
// -----------------------------------------------------------------------------
enum class NodeColor {
    WHITE = 0,      // Unvisited
    GRAY = 1,       // In current DFS path (visiting)
    BLACK = 2       // Fully processed
};

// -----------------------------------------------------------------------------
// Dependency Node
// -----------------------------------------------------------------------------
struct DependencyNode {
    // Identity
    std::string id;                     // Unique identifier (target name or file path)
    fs::path path;                      // Filesystem path
    NodeType type = NodeType::SOURCE;

    // Graph structure (adjacency lists)
    std::vector<DependencyNode*> dependencies;  // Nodes this depends on (outgoing)
    std::vector<DependencyNode*> dependents;    // Nodes that depend on this (incoming)

    // Topological sort state
    std::atomic<int> in_degree{0};      // Unsatisfied dependency count (thread-safe)
    int original_in_degree = 0;         // For reset after failed builds

    // Cycle detection state
    NodeColor color = NodeColor::WHITE;

    // Build state
    BuildStatus status = BuildStatus::PENDING;
    std::string error_message;          // Error details if failed

    // Timestamps for incremental builds
    fs::file_time_type timestamp;       // Last modification time
    bool timestamp_valid = false;       // Whether timestamp was successfully read

    // Performance tracking
    std::chrono::milliseconds build_time{0};

    // Target-specific data (only for TARGET nodes)
    std::vector<std::string> source_patterns;   // Glob patterns for sources
    std::vector<fs::path> source_files;         // Expanded source files
    fs::path output_path;                       // Output artifact path
    std::vector<std::string> flags;             // Compiler flags
    std::string target_type;                    // "binary", "library", etc.

    // Constructors
    DependencyNode() = default;

    explicit DependencyNode(std::string node_id)
        : id(std::move(node_id)) {}

    DependencyNode(std::string node_id, fs::path node_path, NodeType node_type)
        : id(std::move(node_id)), path(std::move(node_path)), type(node_type) {}

    // Check if this node needs rebuilding
    bool is_dirty() const {
        return status == BuildStatus::PENDING || status == BuildStatus::QUEUED;
    }

    // Check if build completed (success or skip)
    bool is_complete() const {
        return status == BuildStatus::COMPLETE || status == BuildStatus::SKIPPED;
    }

    // Check if build failed
    bool is_failed() const {
        return status == BuildStatus::FAILED;
    }

    // Reset state for new build
    void reset() {
        in_degree.store(original_in_degree);
        color = NodeColor::WHITE;
        status = BuildStatus::PENDING;
        error_message.clear();
        build_time = std::chrono::milliseconds{0};
    }

    // Add a dependency (this node depends on other)
    void add_dependency(DependencyNode* other) {
        dependencies.push_back(other);
        other->dependents.push_back(this);
        in_degree.fetch_add(1);
        original_in_degree++;
    }

    // Thread-safe decrement of in-degree
    // Returns true if this was the last dependency (in-degree became 0)
    bool satisfy_dependency() {
        return in_degree.fetch_sub(1) == 1;
    }
};

} // namespace depgraph
} // namespace aria

#endif // ARIA_DEPGRAPH_NODE_HPP
