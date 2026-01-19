/**
 * node.cpp
 * DependencyNode implementation
 *
 * Copyright (c) 2025 Aria Language Project
 */

#include "depgraph/node.hpp"

namespace aria {
namespace depgraph {

const char* node_type_string(NodeType type) {
    switch (type) {
        case NodeType::SOURCE:       return "SOURCE";
        case NodeType::INTERMEDIATE: return "INTERMEDIATE";
        case NodeType::TARGET:       return "TARGET";
    }
    return "UNKNOWN";
}

const char* build_status_string(BuildStatus status) {
    switch (status) {
        case BuildStatus::PENDING:   return "PENDING";
        case BuildStatus::QUEUED:    return "QUEUED";
        case BuildStatus::BUILDING:  return "BUILDING";
        case BuildStatus::COMPLETE:  return "COMPLETE";
        case BuildStatus::SKIPPED:   return "SKIPPED";
        case BuildStatus::FAILED:    return "FAILED";
    }
    return "UNKNOWN";
}

} // namespace depgraph
} // namespace aria
