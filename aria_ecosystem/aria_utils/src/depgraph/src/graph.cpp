/**
 * graph.cpp
 * DependencyGraph implementation
 *
 * Copyright (c) 2025 Aria Language Project
 */

#include "depgraph/graph.hpp"
#include <sstream>
#include <algorithm>

namespace aria {
namespace depgraph {

// -----------------------------------------------------------------------------
// GraphError
// -----------------------------------------------------------------------------
std::string GraphError::to_string() const {
    std::ostringstream ss;

    switch (kind) {
        case Kind::DUPLICATE_NODE:
            ss << "Duplicate node: " << message;
            break;
        case Kind::NODE_NOT_FOUND:
            ss << "Node not found: " << message;
            break;
        case Kind::INVALID_EDGE:
            ss << "Invalid edge: " << message;
            break;
        case Kind::CYCLE_DETECTED:
            ss << "Circular dependency detected: ";
            for (size_t i = 0; i < cycle_path.size(); ++i) {
                if (i > 0) ss << " -> ";
                ss << cycle_path[i];
            }
            break;
        case Kind::VALIDATION_ERROR:
            ss << "Validation error: " << message;
            break;
    }

    return ss.str();
}

// -----------------------------------------------------------------------------
// DependencyGraph
// -----------------------------------------------------------------------------

DependencyNode* DependencyGraph::add_node(const std::string& id, NodeType type) {
    if (has_node(id)) {
        return nullptr;  // Duplicate
    }

    auto node = std::make_unique<DependencyNode>(id);
    node->type = type;
    DependencyNode* ptr = node.get();
    nodes_[id] = std::move(node);
    return ptr;
}

DependencyNode* DependencyGraph::add_node(const std::string& id, const fs::path& path, NodeType type) {
    DependencyNode* node = add_node(id, type);
    if (node) {
        node->path = path;
    }
    return node;
}

DependencyNode* DependencyGraph::get_node(const std::string& id) {
    auto it = nodes_.find(id);
    return it != nodes_.end() ? it->second.get() : nullptr;
}

const DependencyNode* DependencyGraph::get_node(const std::string& id) const {
    auto it = nodes_.find(id);
    return it != nodes_.end() ? it->second.get() : nullptr;
}

bool DependencyGraph::has_node(const std::string& id) const {
    return nodes_.find(id) != nodes_.end();
}

DependencyNode* DependencyGraph::get_or_create(const std::string& id, NodeType type) {
    DependencyNode* node = get_node(id);
    if (!node) {
        node = add_node(id, type);
    }
    return node;
}

bool DependencyGraph::remove_node(const std::string& id) {
    auto it = nodes_.find(id);
    if (it == nodes_.end()) {
        return false;
    }

    DependencyNode* node = it->second.get();

    // Remove from dependencies of other nodes
    for (DependencyNode* dep : node->dependencies) {
        auto& deps = dep->dependents;
        deps.erase(std::remove(deps.begin(), deps.end(), node), deps.end());
    }

    // Remove from dependents of other nodes
    for (DependencyNode* dependent : node->dependents) {
        auto& deps = dependent->dependencies;
        deps.erase(std::remove(deps.begin(), deps.end(), node), deps.end());
        dependent->in_degree.fetch_sub(1);
        dependent->original_in_degree--;
    }

    nodes_.erase(it);
    return true;
}

bool DependencyGraph::add_edge(const std::string& from_id, const std::string& to_id) {
    DependencyNode* from = get_node(from_id);
    DependencyNode* to = get_node(to_id);

    if (!from || !to) {
        return false;
    }

    add_edge(from, to);
    return true;
}

void DependencyGraph::add_edge(DependencyNode* from, DependencyNode* to) {
    from->add_dependency(to);
}

size_t DependencyGraph::edge_count() const {
    size_t count = 0;
    for (const auto& [id, node] : nodes_) {
        count += node->dependencies.size();
    }
    return count;
}

std::vector<DependencyNode*> DependencyGraph::all_nodes() {
    std::vector<DependencyNode*> result;
    result.reserve(nodes_.size());
    for (auto& [id, node] : nodes_) {
        result.push_back(node.get());
    }
    return result;
}

std::vector<const DependencyNode*> DependencyGraph::all_nodes() const {
    std::vector<const DependencyNode*> result;
    result.reserve(nodes_.size());
    for (const auto& [id, node] : nodes_) {
        result.push_back(node.get());
    }
    return result;
}

std::vector<DependencyNode*> DependencyGraph::nodes_of_type(NodeType type) {
    std::vector<DependencyNode*> result;
    for (auto& [id, node] : nodes_) {
        if (node->type == type) {
            result.push_back(node.get());
        }
    }
    return result;
}

std::vector<DependencyNode*> DependencyGraph::root_nodes() {
    std::vector<DependencyNode*> result;
    for (auto& [id, node] : nodes_) {
        if (node->dependencies.empty()) {
            result.push_back(node.get());
        }
    }
    return result;
}

std::vector<DependencyNode*> DependencyGraph::leaf_nodes() {
    std::vector<DependencyNode*> result;
    for (auto& [id, node] : nodes_) {
        if (node->dependents.empty()) {
            result.push_back(node.get());
        }
    }
    return result;
}

void DependencyGraph::visit_dependencies(DependencyNode* node,
                                         std::function<void(DependencyNode*)> visitor) {
    if (!node || node->color == NodeColor::BLACK) return;

    node->color = NodeColor::GRAY;

    for (DependencyNode* dep : node->dependencies) {
        visit_dependencies(dep, visitor);
    }

    node->color = NodeColor::BLACK;
    visitor(node);
}

void DependencyGraph::visit_dependents(DependencyNode* node,
                                       std::function<void(DependencyNode*)> visitor) {
    if (!node || node->color == NodeColor::BLACK) return;

    node->color = NodeColor::GRAY;
    visitor(node);

    for (DependencyNode* dependent : node->dependents) {
        visit_dependents(dependent, visitor);
    }

    node->color = NodeColor::BLACK;
}

void DependencyGraph::reset() {
    for (auto& [id, node] : nodes_) {
        node->reset();
    }
}

void DependencyGraph::reset_colors() {
    for (auto& [id, node] : nodes_) {
        node->color = NodeColor::WHITE;
    }
}

void DependencyGraph::refresh_timestamps() {
    for (auto& [id, node] : nodes_) {
        if (fs::exists(node->path)) {
            std::error_code ec;
            node->timestamp = fs::last_write_time(node->path, ec);
            node->timestamp_valid = !ec;
        } else {
            node->timestamp_valid = false;
        }
    }
}

std::vector<GraphError> DependencyGraph::validate() const {
    std::vector<GraphError> errors;

    for (const auto& [id, node] : nodes_) {
        // Check for invalid paths on source nodes
        if (node->type == NodeType::SOURCE && !node->path.empty()) {
            if (!fs::exists(node->path)) {
                GraphError err;
                err.kind = GraphError::Kind::VALIDATION_ERROR;
                err.message = "Source file not found: " + node->path.string();
                errors.push_back(std::move(err));
            }
        }

        // Check for dangling dependency references
        for (const DependencyNode* dep : node->dependencies) {
            if (!has_node(dep->id)) {
                GraphError err;
                err.kind = GraphError::Kind::NODE_NOT_FOUND;
                err.message = "Dangling reference to: " + dep->id;
                errors.push_back(std::move(err));
            }
        }
    }

    return errors;
}

std::string DependencyGraph::dump() const {
    std::ostringstream ss;
    ss << "DependencyGraph (" << node_count() << " nodes, " << edge_count() << " edges)\n";

    for (const auto& [id, node] : nodes_) {
        ss << "  " << id << " [" << node_type_string(node->type) << "]";
        if (!node->dependencies.empty()) {
            ss << " depends on: ";
            for (size_t i = 0; i < node->dependencies.size(); ++i) {
                if (i > 0) ss << ", ";
                ss << node->dependencies[i]->id;
            }
        }
        ss << "\n";
    }

    return ss.str();
}

std::string DependencyGraph::to_dot() const {
    std::ostringstream ss;
    ss << "digraph DependencyGraph {\n";
    ss << "  rankdir=BT;\n";  // Bottom to top (dependencies at bottom)

    // Node definitions
    for (const auto& [id, node] : nodes_) {
        ss << "  \"" << id << "\" [label=\"" << id << "\"";
        switch (node->type) {
            case NodeType::SOURCE:
                ss << ", shape=box";
                break;
            case NodeType::INTERMEDIATE:
                ss << ", shape=ellipse, style=dashed";
                break;
            case NodeType::TARGET:
                ss << ", shape=box, style=bold";
                break;
        }
        ss << "];\n";
    }

    // Edges
    for (const auto& [id, node] : nodes_) {
        for (const DependencyNode* dep : node->dependencies) {
            ss << "  \"" << dep->id << "\" -> \"" << id << "\";\n";
        }
    }

    ss << "}\n";
    return ss.str();
}

} // namespace depgraph
} // namespace aria
