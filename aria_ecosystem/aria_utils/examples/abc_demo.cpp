/**
 * abc_demo.cpp
 * Interactive demonstration of the abc (Aria Build Configuration) utility
 *
 * Shows parsing build.abc files with variable interpolation and hermetic scoping
 */

#include "abc/parser.hpp"
#include "abc/ast.hpp"
#include "abc/interpolator.hpp"
#include <iostream>
#include <iomanip>

using namespace aria::abc;

void print_section(const std::string& title) {
    std::cout << "\n╔════════════════════════════════════════════════════════════╗\n";
    std::cout << "║  " << std::left << std::setw(56) << title << "  ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════╝\n\n";
}

// Demo 1: Parse simple build configuration
void demo_simple_config() {
    print_section("Demo 1: Simple Build Configuration");

    const char* config = R"({
project: {
    name: "hello_world"
    version: "0.1.0"
}

targets: [
    {
        name: "main"
        type: "binary"
        sources: ["src/main.aria"]
    }
]
})";

    auto result = parse_abc(config);

    if (result.success()) {
        std::cout << "✓ Parsed successfully!\n\n";
        std::cout << "Project: " << result.ast->project_name() << "\n";
        std::cout << "Version: " << result.ast->project_version() << "\n";
        
        if (result.ast->has_targets()) {
            std::cout << "Targets: " << result.ast->targets->size() << "\n";
        }
    } else {
        std::cout << "✗ Parse failed:\n";
        for (const auto& err : result.errors) {
            std::cout << "  " << err.to_string() << "\n";
        }
    }
}

// Demo 2: Parse with dependencies
void demo_with_dependencies() {
    print_section("Demo 2: Target Dependencies");

    const char* config = R"({
project: {
    name: "my_app"
    version: "1.0.0"
}

targets: [
    {
        name: "utils"
        type: "library"
        sources: ["src/utils.aria"]
        deps: []
    }
    {
        name: "main"
        type: "binary"
        sources: ["src/main.aria"]
        deps: ["utils"]
    }
]
})";

    auto result = parse_abc(config);

    if (result.success() && result.ast->has_targets()) {
        std::cout << "Build order analysis:\n\n";
        
        for (const auto& target_node : result.ast->targets->elements) {
            if (target_node->is_object()) {
                const auto& target = target_node->as_object();
                std::string name = target.get_string("name");
                std::string type = target.get_string("type");
                
                std::cout << "  " << name << " (" << type << ")\n";
                
                const auto* deps = target.get_array("deps");
                if (deps && !deps->empty()) {
                    std::cout << "    Dependencies:\n";
                    for (const auto& dep : deps->elements) {
                        if (dep->is_string()) {
                            std::cout << "      - " << dep->as_string() << "\n";
                        }
                    }
                } else {
                    std::cout << "    No dependencies (build first)\n";
                }
            }
        }
    }
}

// Demo 3: Variable interpolation
void demo_variable_interpolation() {
    print_section("Demo 3: Variable Interpolation");

    const char* config = R"({
project: {
    name: "interpolation_demo"
    version: "1.0.0"
}

variables: {
    SRC_DIR: "src"
    BUILD_DIR: "build"
    OUTPUT: "&{BUILD_DIR}/bin"
}

targets: [
    {
        name: "test"
        sources: ["&{SRC_DIR}/test.aria"]
        output: "&{OUTPUT}/test"
    }
]
})";

    auto result = parse_abc(config);

    if (result.success()) {
        std::cout << "Variables defined:\n";
        if (result.ast->has_variables()) {
            for (const auto& var : result.ast->variables->members) {
                std::cout << "  " << var.key << " = ";
                if (var.value->is_string()) {
                    std::cout << var.value->as_string();
                }
                std::cout << "\n";
            }
        }

        std::cout << "\nInterpolating target values:\n";
        
        Interpolator interp;
        
        // Load global variables
        if (result.ast->has_variables()) {
            for (const auto& var : result.ast->variables->members) {
                if (var.value->is_string()) {
                    interp.set_variable(var.key, var.value->as_string());
                }
            }
        }

        // Interpolate first target
        if (result.ast->has_targets() && !result.ast->targets->empty()) {
            const auto& target = result.ast->targets->at(0);
            if (target.is_object()) {
                const auto& obj = target.as_object();
                
                if (const auto* sources = obj.get_array("sources")) {
                    if (!sources->empty() && sources->at(0).is_string()) {
                        auto src_result = interp.interpolate(sources->at(0).as_string());
                        std::cout << "  sources[0]: " << src_result.value << "\n";
                    }
                }
                
                std::string output = obj.get_string("output");
                if (!output.empty()) {
                    auto out_result = interp.interpolate(output);
                    std::cout << "  output: " << out_result.value << "\n";
                }
            }
        }
    }
}

// Demo 4: Environment variable access
void demo_env_variables() {
    print_section("Demo 4: Environment Variables");

    const char* config = R"({
project: {
    name: "env_demo"
}

variables: {
    HOME_DIR: "&{ENV.HOME}"
    BUILD_JOBS: "&{ENV.NPROC}"
}
})";

    auto result = parse_abc(config);

    if (result.success() && result.ast->has_variables()) {
        Interpolator interp;
        
        std::cout << "Accessing environment variables:\n\n";
        
        for (const auto& var : result.ast->variables->members) {
            if (var.value->is_string()) {
                std::cout << "  " << var.key << " = \"" << var.value->as_string() << "\"\n";
                
                auto resolved = interp.interpolate(var.value->as_string());
                if (resolved.success()) {
                    std::cout << "    → " << resolved.value << "\n";
                } else {
                    std::cout << "    → (undefined)\n";
                }
            }
        }
    }
}

// Demo 5: Hermetic scope resolution
void demo_scoped_resolution() {
    print_section("Demo 5: Hermetic Scope Resolution");

    std::cout << "Scope priority: LOCAL → GLOBAL → ENV\n\n";

    Interpolator interp;
    
    // Set global variable
    interp.set_variable("BUILD_MODE", "debug");
    
    std::cout << "Global scope: BUILD_MODE = \"debug\"\n";
    std::cout << "Local scope:  BUILD_MODE = \"release\"\n\n";
    
    // Interpolate with global only
    auto global_result = interp.interpolate("Mode is &{BUILD_MODE}");
    std::cout << "With global scope: " << global_result.value << "\n";
    
    // Push local scope and override
    interp.push_scope();
    interp.set_local("BUILD_MODE", "release");
    
    // Interpolate with local scope (higher priority)
    auto local_result = interp.interpolate("Mode is &{BUILD_MODE}");
    std::cout << "With local scope:  " << local_result.value << "\n";
    
    interp.pop_scope();
}

// Demo 6: Cycle detection
void demo_cycle_detection() {
    print_section("Demo 6: Circular Reference Detection");

    const char* config = R"({
variables: {
    A: "&{B}"
    B: "&{C}"
    C: "&{A}"
}
})";

    auto result = parse_abc(config);

    if (result.success() && result.ast->has_variables()) {
        Interpolator interp;
        
        // Load variables
        for (const auto& var : result.ast->variables->members) {
            if (var.value->is_string()) {
                interp.set_variable(var.key, var.value->as_string());
            }
        }

        std::cout << "Attempting to resolve circular dependency:\n";
        std::cout << "  A → B → C → A (cycle!)\n\n";
        
        auto cycle_result = interp.interpolate("&{A}");
        
        if (cycle_result.has_errors()) {
            std::cout << "✓ Cycle detected!\n";
            for (const auto& err : cycle_result.errors) {
                std::cout << "\nError: " << err.to_string() << "\n";
                if (!err.cycle_path.empty()) {
                    std::cout << "Cycle path:\n";
                    for (const auto& var : err.cycle_path) {
                        std::cout << "  → " << var << "\n";
                    }
                }
            }
        } else {
            std::cout << "✗ Should have detected cycle\n";
        }
    }
}

// Demo 7: Build flags and compiler options
void demo_build_flags() {
    print_section("Demo 7: Build Flags & Compiler Options");

    const char* config = R"({
project: {
    name: "optimized_build"
}

variables: {
    OPT_LEVEL: "-O2"
    DEBUG_FLAGS: "--debug-info"
}

targets: [
    {
        name: "myapp"
        type: "binary"
        sources: ["src/main.aria"]
        flags: ["&{OPT_LEVEL}", "&{DEBUG_FLAGS}", "--strict"]
    }
]
})";

    auto result = parse_abc(config);

    if (result.success() && result.ast->has_targets()) {
        Interpolator interp;
        
        // Load variables
        if (result.ast->has_variables()) {
            for (const auto& var : result.ast->variables->members) {
                if (var.value->is_string()) {
                    interp.set_variable(var.key, var.value->as_string());
                }
            }
        }

        std::cout << "Compiler invocation for 'myapp':\n\n";
        
        const auto& target = result.ast->targets->at(0);
        if (target.is_object()) {
            const auto& obj = target.as_object();
            
            std::cout << "  ariac";
            
            // Sources
            if (const auto* sources = obj.get_array("sources")) {
                for (const auto& src : sources->elements) {
                    if (src->is_string()) {
                        std::cout << " " << src->as_string();
                    }
                }
            }
            
            // Flags
            if (const auto* flags = obj.get_array("flags")) {
                std::cout << " \\\n    ";
                for (const auto& flag : flags->elements) {
                    if (flag->is_string()) {
                        auto resolved = interp.interpolate(flag->as_string());
                        std::cout << resolved.value << " ";
                    }
                }
            }
            
            std::cout << "\n";
        }
    }
}

// Demo 8: Error handling
void demo_error_handling() {
    print_section("Demo 8: Parse Error Handling");

    const char* bad_config = R"({
project: {
    name: "broken"
    version: "1.0.0"
    // Missing closing brace

targets: [
    {
        name: "test"
    }
]
})";

    std::cout << "Parsing intentionally malformed config:\n\n";
    
    auto result = parse_abc(bad_config);

    if (result.has_errors()) {
        std::cout << "✓ Parser caught " << result.errors.size() << " error(s):\n\n";
        
        for (const auto& err : result.errors) {
            std::cout << "Error at line " << err.loc.line 
                      << ", column " << err.loc.column << ":\n";
            std::cout << "  " << err.message << "\n";
            if (!err.context.empty()) {
                std::cout << "\nContext:\n" << err.context << "\n";
            }
        }
        
        if (result.ast) {
            std::cout << "\n✓ Partial AST recovered (panic-mode recovery)\n";
        }
    }
}

int main() {
    std::cout << "╔════════════════════════════════════════════════════════════╗\n";
    std::cout << "║   ABC Parser - Interactive Demonstration                  ║\n";
    std::cout << "║                                                            ║\n";
    std::cout << "║  Aria Build Configuration with hermetic scoping           ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════╝\n";

    try {
        demo_simple_config();
        demo_with_dependencies();
        demo_variable_interpolation();
        demo_env_variables();
        demo_scoped_resolution();
        demo_cycle_detection();
        demo_build_flags();
        demo_error_handling();

        std::cout << "\n✓ All 8 demonstrations completed successfully!\n\n";

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
