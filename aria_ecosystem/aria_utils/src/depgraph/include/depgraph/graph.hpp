/**
 * graph.hpp
 * Dependency Graph for AriaBuild
 *
 * Core graph structure that models build dependencies as a DAG.
 * Provides O(1) node lookup and efficient traversal.
 *
 * Copyright (c) 2025 Aria Language Project
 */

#ifndef ARIA_DEPGRAPH_GRAPH_HPP
#define ARIA_DEPGRAPH_GRAPH_HPP

#include "node.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <functional>

namespace aria {
namespace depgraph {

// -----------------------------------------------------------------------------
// Graph Error
// -----------------------------------------------------------------------------
struct GraphError {
    enum class Kind {
        DUPLICATE_NODE,
        NODE_NOT_FOUND,
        INVALID_EDGE,
        CYCLE_DETECTED,
        VALIDATION_ERROR
    };

    Kind kind;
    std::string message;
    std::vector<std::string> cycle_path;  // For CYCLE_DETECTED

    std::string to_string() const;
};

// -----------------------------------------------------------------------------
// Dependency Graph
// -----------------------------------------------------------------------------
class DependencyGraph {
public:
    DependencyGraph() = default;
    ~DependencyGraph() = default;

    // No copying (owns nodes)
    DependencyGraph(const DependencyGraph&) = delete;
    DependencyGraph& operator=(const DependencyGraph&) = delete;

    // Move OK
    DependencyGraph(DependencyGraph&&) = default;
    DependencyGraph& operator=(DependencyGraph&&) = default;

    // ---------------------------------------------------------------------
    // Node Management
    // ---------------------------------------------------------------------

    /**
     * Add a new node to the graph.
     * Returns pointer to the node, or nullptr if duplicate.
     */
    DependencyNode* add_node(const std::string& id, NodeType type = NodeType::SOURCE);

    /**
     * Add a node with path.
     */
    DependencyNode* add_node(const std::string& id, const fs::path& path, NodeType type);

    /**
     * Get a node by ID.
     * Returns nullptr if not found.
     */
    DependencyNode* get_node(const std::string& id);
    const DependencyNode* get_node(const std::string& id) const;

    /**
     * Check if node exists.
     */
    bool has_node(const std::string& id) const;

    /**
     * Get or create a node.
     */
    DependencyNode* get_or_create(const std::string& id, NodeType type = NodeType::SOURCE);

    /**
     * Remove a node (also removes all edges).
     */
    bool remove_node(const std::string& id);

    // ---------------------------------------------------------------------
    // Edge Management
    // ---------------------------------------------------------------------

    /**
     * Add an edge: from_id depends on to_id.
     * Creates the dependency relationship.
     */
    bool add_edge(const std::string& from_id, const std::string& to_id);

    /**
     * Add edge using node pointers.
     */
    void add_edge(DependencyNode* from, DependencyNode* to);

    // ---------------------------------------------------------------------
    // Graph Properties
    // ---------------------------------------------------------------------

    /**
     * Number of nodes.
     */
    size_t node_count() const { return nodes_.size(); }

    /**
     * Number of edges (sum of all dependency counts).
     */
    size_t edge_count() const;

    /**
     * Check if graph is empty.
     */
    bool empty() const { return nodes_.empty(); }

    // ---------------------------------------------------------------------
    // Traversal
    // ---------------------------------------------------------------------

    /**
     * Get all nodes.
     */
    std::vector<DependencyNode*> all_nodes();
    std::vector<const DependencyNode*> all_nodes() const;

    /**
     * Get nodes of a specific type.
     */
    std::vector<DependencyNode*> nodes_of_type(NodeType type);

    /**
     * Get all target nodes.
     */
    std::vector<DependencyNode*> targets() {
        return nodes_of_type(NodeType::TARGET);
    }

    /**
     * Get all source nodes.
     */
    std::vector<DependencyNode*> sources() {
        return nodes_of_type(NodeType::SOURCE);
    }

    /**
     * Get nodes with no dependencies (roots for bottom-up processing).
     */
    std::vector<DependencyNode*> root_nodes();

    /**
     * Get nodes with no dependents (leaves - final targets).
     */
    std::vector<DependencyNode*> leaf_nodes();

    /**
     * Visit all nodes in dependency order (callback per node).
     */
    void visit_dependencies(DependencyNode* node,
                           std::function<void(DependencyNode*)> visitor);

    /**
     * Visit all dependents (reverse order).
     */
    void visit_dependents(DependencyNode* node,
                         std::function<void(DependencyNode*)> visitor);

    // ---------------------------------------------------------------------
    // State Management
    // ---------------------------------------------------------------------

    /**
     * Reset all nodes to initial state (for new build).
     */
    void reset();

    /**
     * Reset only color flags (for new traversal).
     */
    void reset_colors();

    /**
     * Update timestamps for all nodes from filesystem.
     */
    void refresh_timestamps();

    // ---------------------------------------------------------------------
    // Validation
    // ---------------------------------------------------------------------

    /**
     * Validate graph integrity.
     * Checks for:
     * - Dangling references
     * - Invalid paths
     * - Missing dependencies
     */
    std::vector<GraphError> validate() const;

    // ---------------------------------------------------------------------
    // Debug/Diagnostics
    // ---------------------------------------------------------------------

    /**
     * Get a text representation of the graph.
     */
    std::string dump() const;

    /**
     * Get DOT format for visualization.
     */
    std::string to_dot() const;

private:
    std::unordered_map<std::string, std::unique_ptr<DependencyNode>> nodes_;
};

} // namespace depgraph
} // namespace aria

#endif // ARIA_DEPGRAPH_GRAPH_HPP
