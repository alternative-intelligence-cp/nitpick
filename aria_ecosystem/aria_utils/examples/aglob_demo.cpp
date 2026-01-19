/**
 * aglob_demo.cpp
 * Demonstrates glob pattern matching functionality
 * 
 * Build: g++ -std=c++17 aglob_demo.cpp -I../src/aglob/include -L../src/aglob/build -Wl,-rpath,../src/aglob/build -laglob -lstdc++fs -o aglob_demo
 * Run: ./aglob_demo
 */

#include <iostream>
#include <iomanip>
#include "aglob/glob_engine.hpp"

using namespace aria::glob;

void print_separator(const std::string& title) {
    std::cout << "\n" << std::string(60, '=') << "\n";
    std::cout << "  " << title << "\n";
    std::cout << std::string(60, '=') << "\n\n";
}

void demo_simple_wildcard() {
    print_separator("Demo 1: Simple Wildcard (*.cpp)");
    
    GlobEngine engine;
    auto [paths, err] = engine.match("../src/aglob/src", "*.cpp");
    
    if (err == GlobError::OK) {
        std::cout << "Found " << paths.size() << " C++ files:\n";
        for (const auto& path : paths) {
            std::cout << "  - " << path.string() << "\n";
        }
    } else {
        std::cout << "Error: " << glob_error_string(err) << "\n";
    }
}

void demo_recursive_wildcard() {
    print_separator("Demo 2: Recursive Wildcard (**/test_*.cpp)");
    
    GlobEngine engine;
    auto [paths, err] = engine.match("../src", "**/test_*.cpp");
    
    if (err == GlobError::OK) {
        std::cout << "Found " << paths.size() << " test files across all utilities:\n";
        for (size_t i = 0; i < std::min<size_t>(10, paths.size()); i++) {
            std::cout << "  - " << paths[i].string() << "\n";
        }
        if (paths.size() > 10) {
            std::cout << "  ... and " << (paths.size() - 10) << " more\n";
        }
    } else {
        std::cout << "Error: " << glob_error_string(err) << "\n";
    }
}

void demo_character_class() {
    print_separator("Demo 3: Character Class ([gt]*.cpp)");
    
    GlobEngine engine;
    auto [paths, err] = engine.match("../src/aglob/src", "[gt]*.cpp");
    
    if (err == GlobError::OK) {
        std::cout << "Found " << paths.size() << " files starting with 'g' or 't':\n";
        for (const auto& path : paths) {
            std::cout << "  - " << path.string() << "\n";
        }
    } else {
        std::cout << "Error: " << glob_error_string(err) << "\n";
    }
}

void demo_multiple_patterns() {
    print_separator("Demo 4: Multiple Patterns");
    
    GlobEngine engine;
    std::vector<std::string> patterns = {
        "*.cpp",
        "*.hpp",
        "*.h"
    };
    
    auto [paths, err] = engine.match_all("../src/aglob/src", patterns);
    
    if (err == GlobError::OK) {
        std::cout << "Found " << paths.size() << " source/header files:\n";
        for (const auto& path : paths) {
            std::cout << "  - " << path.string() << "\n";
        }
    } else {
        std::cout << "Error: " << glob_error_string(err) << "\n";
    }
}

void demo_path_matches() {
    print_separator("Demo 5: Path Matching (without filesystem)");
    
    std::vector<std::string> test_paths = {
        "src/core/main.aria",
        "src/utils/helpers.aria",
        "tests/test_core.aria",
        "build/output.o",
        "README.md"
    };
    
    std::string pattern = "src/**/*.aria";
    std::cout << "Testing pattern: " << pattern << "\n\n";
    
    for (const auto& path : test_paths) {
        fs::path p(path);
        bool matches = GlobEngine::path_matches(p, pattern);
        std::cout << "  " << std::setw(30) << std::left << path 
                  << " -> " << (matches ? "✓ MATCH" : "✗ NO MATCH") << "\n";
    }
}

void demo_error_handling() {
    print_separator("Demo 6: Error Handling");
    
    GlobEngine engine;
    
    // Test 1: Invalid base directory
    std::cout << "Test 1: Invalid base directory\n";
    auto [paths1, err1] = engine.match("/nonexistent/path", "*.txt");
    std::cout << "  Error: " << glob_error_string(err1) << "\n\n";
    
    // Test 2: Validate pattern syntax
    std::cout << "Test 2: Pattern validation\n";
    std::vector<std::string> test_patterns = {
        "*.cpp",           // Valid
        "**/*.aria",       // Valid
        "[a-z]*.txt",      // Valid
        "[z-a]*.txt",      // Invalid (reversed range)
    };
    
    for (const auto& pattern : test_patterns) {
        std::string err_msg = GlobPattern::validate(pattern);
        std::cout << "  " << std::setw(20) << std::left << pattern 
                  << " -> " << (err_msg.empty() ? "✓ Valid" : "✗ " + err_msg) << "\n";
    }
}

void demo_deterministic_ordering() {
    print_separator("Demo 7: Deterministic Ordering (reproducible builds)");
    
    GlobEngine engine;
    
    std::cout << "Running glob 3 times to verify deterministic output:\n\n";
    
    std::vector<std::vector<std::string>> results;
    for (int run = 1; run <= 3; run++) {
        auto [paths, err] = engine.match("../src", "**/*.cpp");
        if (err == GlobError::OK) {
            std::vector<std::string> str_paths;
            for (const auto& p : paths) {
                str_paths.push_back(p.string());
            }
            results.push_back(str_paths);
            std::cout << "  Run " << run << ": " << str_paths.size() << " files\n";
        }
    }
    
    // Verify all runs produced identical results
    bool identical = true;
    if (results.size() >= 2) {
        for (size_t i = 1; i < results.size(); i++) {
            if (results[i] != results[0]) {
                identical = false;
                break;
            }
        }
    }
    
    std::cout << "\n  Result: " << (identical ? "✓ Deterministic" : "✗ Non-deterministic") << "\n";
    std::cout << "\n  First 5 files (consistently ordered):\n";
    if (!results.empty() && !results[0].empty()) {
        for (size_t i = 0; i < std::min<size_t>(5, results[0].size()); i++) {
            std::cout << "    " << (i+1) << ". " << results[0][i] << "\n";
        }
    }
}

int main() {
    std::cout << "\n";
    std::cout << "╔════════════════════════════════════════════════════════════╗\n";
    std::cout << "║        Aria Glob Engine - Interactive Demonstration       ║\n";
    std::cout << "║                                                            ║\n";
    std::cout << "║  High-performance glob pattern matching for build systems ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════╝\n";
    
    try {
        demo_simple_wildcard();
        demo_recursive_wildcard();
        demo_character_class();
        demo_multiple_patterns();
        demo_path_matches();
        demo_error_handling();
        demo_deterministic_ordering();
        
        print_separator("All Demos Complete!");
        std::cout << "✓ All glob features demonstrated successfully!\n\n";
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "\n✗ Error: " << e.what() << "\n";
        return 1;
    }
}
