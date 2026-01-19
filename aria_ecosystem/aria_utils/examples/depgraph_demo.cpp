/**
 * depgraph_demo.cpp
 * Interactive demonstration of the depgraph (Dependency Graph) utility
 *
 * Shows build dependency analysis with cycle detection and topological sorting
 */

#include "depgraph/graph.hpp"
#include "depgraph/node.hpp"
#include <iostream>
#include <iomanip>

using namespace aria::depgraph;

void print_section(const std::string& title) {
    std::cout << "\n╔════════════════════════════════════════════════════════════╗\n";
    std::cout << "║  " << std::left << std::setw(56) << title << "  ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════╝\n\n";
}

// Demo 1: Build simple graph
void demo_simple_graph() {
    print_section("Demo 1: Simple Dependency Graph");

    DependencyGraph graph;

    // Add nodes
    auto* main = graph.add_node("main.aria", NodeType::SOURCE);
    auto* utils = graph.add_node("utils.aria", NodeType::SOURCE);
    auto* types = graph.add_node("types.aria", NodeType::SOURCE);
    auto* binary = graph.add_node("myapp", NodeType::TARGET);

    // Add dependencies (main depends on utils and types)
    graph.add_edge(main, utils);
    graph.add_edge(main, types);
    graph.add_edge(binary, main);

    std::cout << "Created graph with " << graph.node_count() << " nodes\n";
    std::cout << "Total edges: " << graph.edge_count() << "\n\n";

    std::cout << "Node structure:\n";
    for (auto* node : graph.all_nodes()) {
        std::cout << "  " << node->id << " (" << node_type_string(node->type) << ")\n";
        std::cout << "    Dependencies: " << node->dependencies.size() << "\n";
        std::cout << "    Dependents: " << node->dependents.size() << "\n";
    }
}

// Demo 2: Find root and leaf nodes
void demo_roots_and_leaves() {
    print_section("Demo 2: Root and Leaf Nodes");

    DependencyGraph graph;

    graph.add_node("core.aria", NodeType::SOURCE);
    graph.add_node("utils.aria", NodeType::SOURCE);
    graph.add_node("main.aria", NodeType::SOURCE);
    graph.add_node("myapp", NodeType::TARGET);

    graph.add_edge("main.aria", "utils.aria");
    graph.add_edge("utils.aria", "core.aria");
    graph.add_edge("myapp", "main.aria");

    auto roots = graph.root_nodes();
    auto leaves = graph.leaf_nodes();

    std::cout << "Root nodes (no dependencies):\n";
    for (auto* node : roots) {
        std::cout << "  - " << node->id << "\n";
    }

    std::cout << "\nLeaf nodes (no dependents):\n";
    for (auto* node : leaves) {
        std::cout << "  - " << node->id << "\n";
    }
}

// Demo 3: Topological traversal
void demo_dependency_order() {
    print_section("Demo 3: Dependency Order Traversal");

    DependencyGraph graph;

    graph.add_node("a.aria", NodeType::SOURCE);
    graph.add_node("b.aria", NodeType::SOURCE);
    graph.add_node("c.aria", NodeType::SOURCE);
    graph.add_node("d.aria", NodeType::SOURCE);

    // a depends on b and c
    // b depends on d
    // c depends on d
    graph.add_edge("a.aria", "b.aria");
    graph.add_edge("a.aria", "c.aria");
    graph.add_edge("b.aria", "d.aria");
    graph.add_edge("c.aria", "d.aria");

    std::cout << "Dependency chain: a → (b,c) → d\n\n";

    auto* a = graph.get_node("a.aria");
    
    std::cout << "Dependencies of 'a.aria' (build order):\n";
    int order = 1;
    graph.visit_dependencies(a, [&order](DependencyNode* node) {
        std::cout << "  " << order++ << ". " << node->id << "\n";
    });

    std::cout << "\nDependents of 'd.aria' (affected if changed):\n";
    auto* d = graph.get_node("d.aria");
    order = 1;
    graph.visit_dependents(d, [&order](DependencyNode* node) {
        std::cout << "  " << order++ << ". " << node->id << "\n";
    });
}

// Demo 4: Cycle detection
void demo_cycle_detection() {
    print_section("Demo 4: Cycle Detection");

    DependencyGraph graph;

    graph.add_node("a.aria", NodeType::SOURCE);
    graph.add_node("b.aria", NodeType::SOURCE);
    graph.add_node("c.aria", NodeType::SOURCE);

    // Create a cycle: a → b → c → a
    graph.add_edge("a.aria", "b.aria");
    graph.add_edge("b.aria", "c.aria");
    graph.add_edge("c.aria", "a.aria");

    std::cout << "Created graph with cycle: a → b → c → a\n\n";

    auto errors = graph.validate();

    if (!errors.empty()) {
        std::cout << "✓ Cycle detected!\n\n";
        for (const auto& err : errors) {
            std::cout << "Error: " << err.to_string() << "\n";
            if (!err.cycle_path.empty()) {
                std::cout << "Cycle path:\n";
                for (const auto& node : err.cycle_path) {
                    std::cout << "  → " << node << "\n";
                }
            }
        }
    } else {
        std::cout << "✗ Should have detected cycle\n";
    }
}

// Demo 5: Filter by node type
void demo_filter_by_type() {
    print_section("Demo 5: Filter Nodes by Type");

    DependencyGraph graph;

    graph.add_node("main.aria", NodeType::SOURCE);
    graph.add_node("utils.aria", NodeType::SOURCE);
    graph.add_node("main.ll", NodeType::INTERMEDIATE);
    graph.add_node("utils.ll", NodeType::INTERMEDIATE);
    graph.add_node("myapp", NodeType::TARGET);
    graph.add_node("libutils.so", NodeType::TARGET);

    auto sources = graph.sources();
    auto targets = graph.targets();
    auto intermediates = graph.nodes_of_type(NodeType::INTERMEDIATE);

    std::cout << "Source files (" << sources.size() << "):\n";
    for (auto* node : sources) {
        std::cout << "  - " << node->id << "\n";
    }

    std::cout << "\nIntermediate files (" << intermediates.size() << "):\n";
    for (auto* node : intermediates) {
        std::cout << "  - " << node->id << "\n";
    }

    std::cout << "\nTarget files (" << targets.size() << "):\n";
    for (auto* node : targets) {
        std::cout << "  - " << node->id << "\n";
    }
}

// Demo 6: Build status tracking
void demo_build_status() {
    print_section("Demo 6: Build Status Tracking");

    DependencyGraph graph;

    auto* a = graph.add_node("a.aria", NodeType::SOURCE);
    auto* b = graph.add_node("b.aria", NodeType::SOURCE);
    auto* c = graph.add_node("c.aria", NodeType::SOURCE);

    a->status = BuildStatus::COMPLETE;
    b->status = BuildStatus::BUILDING;
    c->status = BuildStatus::FAILED;
    c->error_message = "Syntax error at line 42";

    std::cout << "Build status:\n\n";
    for (auto* node : graph.all_nodes()) {
        std::cout << "  " << std::setw(15) << std::left << node->id 
                  << " : " << build_status_string(node->status);
        if (node->status == BuildStatus::FAILED) {
            std::cout << " (" << node->error_message << ")";
        }
        std::cout << "\n";
    }
}

// Demo 7: Graph visualization (DOT format)
void demo_visualization() {
    print_section("Demo 7: Graph Visualization (DOT format)");

    DependencyGraph graph;

    graph.add_node("main.aria", NodeType::SOURCE);
    graph.add_node("utils.aria", NodeType::SOURCE);
    graph.add_node("core.aria", NodeType::SOURCE);
    graph.add_node("myapp", NodeType::TARGET);

    graph.add_edge("main.aria", "utils.aria");
    graph.add_edge("utils.aria", "core.aria");
    graph.add_edge("myapp", "main.aria");

    std::cout << "DOT format output (for graphviz):\n\n";
    std::cout << graph.to_dot() << "\n";
    std::cout << "Use: dot -Tpng graph.dot -o graph.png\n";
}

// Demo 8: In-degree for topological sort
void demo_in_degree() {
    print_section("Demo 8: In-Degree Analysis");

    DependencyGraph graph;

    graph.add_node("a.aria", NodeType::SOURCE);
    graph.add_node("b.aria", NodeType::SOURCE);
    graph.add_node("c.aria", NodeType::SOURCE);
    graph.add_node("d.aria", NodeType::SOURCE);

    graph.add_edge("a.aria", "b.aria");
    graph.add_edge("a.aria", "c.aria");
    graph.add_edge("b.aria", "d.aria");
    graph.add_edge("c.aria", "d.aria");

    std::cout << "In-degree (number of dependencies):\n\n";
    for (auto* node : graph.all_nodes()) {
        std::cout << "  " << std::setw(10) << std::left << node->id 
                  << " : " << node->in_degree.load() << " dependencies\n";
    }

    std::cout << "\nNodes with in-degree 0 (ready to build):\n";
    for (auto* node : graph.root_nodes()) {
        std::cout << "  - " << node->id << "\n";
    }
}

int main() {
    std::cout << "╔════════════════════════════════════════════════════════════╗\n";
    std::cout << "║   Dependency Graph - Interactive Demonstration            ║\n";
    std::cout << "║                                                            ║\n";
    std::cout << "║  Build dependency analysis with cycle detection           ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════╝\n";

    try {
        demo_simple_graph();
        demo_roots_and_leaves();
        demo_dependency_order();
        demo_cycle_detection();
        demo_filter_by_type();
        demo_build_status();
        demo_visualization();
        demo_in_degree();

        std::cout << "\n✓ All 8 demonstrations completed successfully!\n\n";

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
