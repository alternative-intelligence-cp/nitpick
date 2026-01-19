/**
 * scheduler.cpp
 * Topological Sort and Cycle Detection implementation
 *
 * Copyright (c) 2025 Aria Language Project
 */

#include "depgraph/scheduler.hpp"
#include <algorithm>

namespace aria {
namespace depgraph {

// -----------------------------------------------------------------------------
// TopologicalSorter
// -----------------------------------------------------------------------------

ScheduleResult TopologicalSorter::sort(DependencyGraph& graph) {
    ScheduleResult result;

    if (graph.empty()) {
        return result;  // Empty graph is valid
    }

    // Reset graph state
    graph.reset();

    // Get all nodes
    auto nodes = graph.all_nodes();
    size_t total = nodes.size();

    // Initialize ready queue with nodes that have no dependencies
    std::queue<DependencyNode*> ready;
    for (DependencyNode* node : nodes) {
        if (node->in_degree.load() == 0) {
            ready.push(node);
            node->status = BuildStatus::QUEUED;
        }
    }

    // Process nodes in topological order
    while (!ready.empty()) {
        DependencyNode* node = ready.front();
        ready.pop();

        result.order.push_back(node);

        // Notify all dependents
        for (DependencyNode* dependent : node->dependents) {
            if (dependent->satisfy_dependency()) {
                // All dependencies satisfied - ready to process
                ready.push(dependent);
                dependent->status = BuildStatus::QUEUED;
            }
        }
    }

    // Check for cycle
    if (result.order.size() != total) {
        result.has_cycle = true;
        result.cycle_path = CycleDetector::find_cycle(graph);
        result.error_message = CycleDetector::format_cycle(result.cycle_path);
    }

    return result;
}

ScheduleResult TopologicalSorter::sort_targets(DependencyGraph& graph,
                                               const std::vector<std::string>& target_ids) {
    ScheduleResult result;

    if (target_ids.empty()) {
        return result;
    }

    // Mark all nodes as unvisited
    graph.reset_colors();

    // Collect all nodes reachable from targets
    std::vector<DependencyNode*> reachable;
    std::function<void(DependencyNode*)> collect = [&](DependencyNode* node) {
        if (!node || node->color == NodeColor::BLACK) return;

        node->color = NodeColor::GRAY;

        // Visit dependencies first
        for (DependencyNode* dep : node->dependencies) {
            collect(dep);
        }

        node->color = NodeColor::BLACK;
        reachable.push_back(node);
    };

    // Collect from each target
    for (const std::string& id : target_ids) {
        DependencyNode* target = graph.get_node(id);
        if (target) {
            collect(target);
        } else {
            result.error_message = "Target not found: " + id;
            return result;
        }
    }

    // Reset and compute in-degrees for reachable subgraph
    for (DependencyNode* node : reachable) {
        node->color = NodeColor::WHITE;
        node->in_degree.store(0);
    }

    for (DependencyNode* node : reachable) {
        for (DependencyNode* dep : node->dependencies) {
            // Only count edges within the reachable set
            if (dep->color == NodeColor::WHITE) {
                node->in_degree.fetch_add(1);
            }
        }
    }

    // Standard Kahn's on the subgraph
    std::queue<DependencyNode*> ready;
    for (DependencyNode* node : reachable) {
        if (node->in_degree.load() == 0) {
            ready.push(node);
        }
    }

    while (!ready.empty()) {
        DependencyNode* node = ready.front();
        ready.pop();

        result.order.push_back(node);
        node->color = NodeColor::BLACK;

        for (DependencyNode* dependent : node->dependents) {
            if (dependent->color != NodeColor::BLACK) {
                if (dependent->satisfy_dependency()) {
                    ready.push(dependent);
                }
            }
        }
    }

    // Check for cycle in subgraph
    if (result.order.size() != reachable.size()) {
        result.has_cycle = true;
        result.cycle_path = CycleDetector::find_cycle(graph);
        result.error_message = CycleDetector::format_cycle(result.cycle_path);
    }

    return result;
}

// -----------------------------------------------------------------------------
// CycleDetector
// -----------------------------------------------------------------------------

bool CycleDetector::has_cycle(DependencyGraph& graph) {
    graph.reset_colors();

    for (DependencyNode* node : graph.all_nodes()) {
        if (node->color == NodeColor::WHITE) {
            std::vector<DependencyNode*> path;
            std::vector<std::string> cycle;
            if (dfs_visit(node, path, cycle)) {
                return true;
            }
        }
    }

    return false;
}

std::vector<std::string> CycleDetector::find_cycle(DependencyGraph& graph) {
    graph.reset_colors();

    for (DependencyNode* node : graph.all_nodes()) {
        if (node->color == NodeColor::WHITE) {
            std::vector<DependencyNode*> path;
            std::vector<std::string> cycle;
            if (dfs_visit(node, path, cycle)) {
                return cycle;
            }
        }
    }

    return {};
}

bool CycleDetector::dfs_visit(DependencyNode* node,
                              std::vector<DependencyNode*>& path,
                              std::vector<std::string>& cycle_path) {
    node->color = NodeColor::GRAY;
    path.push_back(node);

    for (DependencyNode* dep : node->dependencies) {
        if (dep->color == NodeColor::GRAY) {
            // Found a back edge - we have a cycle!
            // Extract the cycle path from the current node back to dep
            bool in_cycle = false;
            for (DependencyNode* n : path) {
                if (n == dep) {
                    in_cycle = true;
                }
                if (in_cycle) {
                    cycle_path.push_back(n->id);
                }
            }
            cycle_path.push_back(dep->id);  // Complete the cycle
            return true;
        }

        if (dep->color == NodeColor::WHITE) {
            if (dfs_visit(dep, path, cycle_path)) {
                return true;
            }
        }
    }

    node->color = NodeColor::BLACK;
    path.pop_back();
    return false;
}

std::string CycleDetector::format_cycle(const std::vector<std::string>& cycle) {
    if (cycle.empty()) {
        return "";
    }

    std::string result;
    for (size_t i = 0; i < cycle.size(); ++i) {
        if (i > 0) {
            result += " -> ";
        }
        result += cycle[i];
    }
    return result;
}

// -----------------------------------------------------------------------------
// ReadyQueue
// -----------------------------------------------------------------------------

void ReadyQueue::initialize(DependencyGraph& graph) {
    clear();

    for (DependencyNode* node : graph.all_nodes()) {
        if (node->in_degree.load() == 0 && node->status == BuildStatus::PENDING) {
            push(node);
        }
    }
}

void ReadyQueue::push(DependencyNode* node) {
    node->status = BuildStatus::QUEUED;
    queue_.push(node);
}

DependencyNode* ReadyQueue::pop() {
    if (queue_.empty()) {
        return nullptr;
    }
    DependencyNode* node = queue_.front();
    queue_.pop();
    return node;
}

std::vector<DependencyNode*> ReadyQueue::notify_complete(DependencyNode* node) {
    std::vector<DependencyNode*> newly_ready;

    node->status = BuildStatus::COMPLETE;

    for (DependencyNode* dependent : node->dependents) {
        if (dependent->satisfy_dependency()) {
            push(dependent);
            newly_ready.push_back(dependent);
        }
    }

    return newly_ready;
}

} // namespace depgraph
} // namespace aria
