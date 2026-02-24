/**
 * Main REPL - Interactive Shell
 * 
 * Integrates all components:
 * - InputEngine: Multi-line editing with Ctrl+Enter
 * - Lexer: Tokenization
 * - Parser: AST construction
 * - Executor: Interpretation
 * 
 * This is the user-facing shell interface.
 */

#include "repl/input_engine.hpp"
#include "repl/terminal.hpp"
#include "parser/lexer.hpp"
#include "parser/parser.hpp"
#include "executor/executor.hpp"
#include <iostream>
#include <sstream>
#include <cstdio>
#include <cstdlib>
#include <array>
#include <string>

using namespace ariash;

// ---------------------------------------------------------------------------
// Aria Specialist integration
// ---------------------------------------------------------------------------

#ifndef ARIA_TOOLS_DIR
#define ARIA_TOOLS_DIR ""
#endif

/**
 * Query the Aria language specialist (Mistral-7B + LoRA checkpoint-200).
 * Prints the response to stdout with a leading newline.
 * Falls back to a helpful error if the specialist is not available.
 */
void specialistQuery(const std::string& question) {
    // Resolve tools directory: env var overrides baked-in cmake path
    const char* env_tools = std::getenv("ARIA_TOOLS");
    std::string tools_dir = env_tools ? std::string(env_tools) : ARIA_TOOLS_DIR;

    if (tools_dir.empty()) {
        std::cerr << "[specialist] ARIA_TOOLS not set and ARIA_TOOLS_DIR not baked in.\n"
                  << "            Set ARIA_TOOLS=/path/to/aria/tools and retry.\n";
        return;
    }

    std::string script = tools_dir + "/aria_specialist_infer.py";

    // Escape single quotes in the question for the shell
    std::string safe_q;
    safe_q.reserve(question.size() + 8);
    for (char c : question) {
        if (c == '\'') safe_q += "'\\''";
        else           safe_q += c;
    }

    // Build command: python3 <script> '<question>' 2>/dev/null
    std::string cmd = "python3 '" + script + "' '" + safe_q + "' 2>/dev/null";

    std::cout << "\n[aria-specialist]\n";
    std::cout.flush();

    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        std::cerr << "[specialist] Failed to launch specialist subprocess.\n";
        return;
    }

    std::array<char, 256> buf;
    while (fgets(buf.data(), buf.size(), pipe) != nullptr) {
        std::cout << buf.data();
    }
    std::cout << "\n";
    std::cout.flush();

    int exit_code = pclose(pipe);
    if (exit_code != 0) {
        std::cerr << "[specialist] Process exited with code " << exit_code
                  << ". Is aria_specialist_infer.py in " << tools_dir << " ?\n";
    }
}

void printBanner() {
    std::cout << "\n";
    std::cout << "╔═══════════════════════════════════════════════════════╗\n";
    std::cout << "║              AriaSH - Aria Interactive Shell          ║\n";
    std::cout << "║                    Version 0.1.0                      ║\n";
    std::cout << "╚═══════════════════════════════════════════════════════╝\n";
    std::cout << "\n";
    std::cout << "Modal Input System:\n";
    std::cout << "  🟢 RUN mode:   Enter submits immediately\n";
    std::cout << "  🔵 EDIT mode:  End with ;; and press Enter to submit\n";
    std::cout << "\n";
    std::cout << "Quick Start:\n";
    std::cout << "  • Type and Enter to run (RUN mode)\n";
    std::cout << "  • ESC toggles modes [RUN] ↔ [EDIT]\n";
    std::cout << "  • Multi-line: Type ;;  then Enter (EDIT mode)\n";
    std::cout << "  • 'help' for more, 'exit' to quit\n";
    std::cout << "  • ?<question> to ask the Aria language specialist\n";
    std::cout << "\n";
}

void printHelp() {
    std::cout << "\nAvailable Commands:\n";
    std::cout << "  help          - Show this help message\n";
    std::cout << "  exit / quit   - Exit the shell\n";
    std::cout << "  clear         - Clear the screen\n";
    std::cout << "\n";
    std::cout << "Modal Input System:\n";
    std::cout << "  ESC           - Toggle between RUN and EDIT mode\n";
    std::cout << "\n";
    std::cout << "  [RUN] mode (default):\n";
    std::cout << "    Enter       - Submit and execute immediately\n";
    std::cout << "\n";
    std::cout << "  [EDIT] mode (multi-line):\n";
    std::cout << "    Enter       - New line (continue editing)\n";
    std::cout << "    ;;          - Double semicolon then Enter submits\n";
    std::cout << "                  Example: int8 sum = x+y;;\n";
    std::cout << "                  Or split: int8 sum = x+y;\n";
    std::cout << "                            ;   (then Enter)\n";
    std::cout << "\n";
    std::cout << "Language Features:\n";
    std::cout << "  Variables:    int8 x = 10;\n";
    std::cout << "  Expressions:  x = x + 5;\n";
    std::cout << "  Control:      if (x > 5) { ... }\n";
    std::cout << "  Loops:        while (i < 10) { ... }\n";
    std::cout << "  Commands:     ls -la\n";
    std::cout << "  Pipelines:    ls | grep test\n";
    std::cout << "\n";
    std::cout << "Other Shortcuts:\n";
    std::cout << "  Ctrl+C        - Cancel current input\n";
    std::cout << "  Ctrl+D        - Exit shell\n";
    std::cout << "  Ctrl+L        - Clear screen\n";
    std::cout << "\n";
    std::cout << "Aria Specialist (AI):\n";
    std::cout << "  ?<question>   - Ask the Aria language specialist\n";
    std::cout << "  Example:  ?How do I write a function that returns a Result?\n";
    std::cout << "  Example:  ?What is the syntax for a struct?\n";
    std::cout << "  (Requires: ARIA_TOOLS env var or cmake ARIA_TOOLS_DIR baked in)\n\n";
}

int main() {
    // Print banner
    printBanner();
    
    // Initialize terminal
    repl::PlatformTerminal terminal;
    
    // Create global environment
    executor::Environment globalEnv;
    
    // Create input engine
    repl::InputEngine inputEngine(terminal);
    
    // Setup input callbacks
    auto onSubmit = [&](const std::string& input) {
        // Trim whitespace from input
        std::string trimmed = input;
        
        // Remove leading whitespace
        size_t start = trimmed.find_first_not_of(" \t\n\r");
        if (start == std::string::npos) {
            return;  // Empty input
        }
        trimmed = trimmed.substr(start);
        
        // Remove trailing whitespace
        size_t end = trimmed.find_last_not_of(" \t\n\r");
        if (end != std::string::npos) {
            trimmed = trimmed.substr(0, end + 1);
        }
        
        // Remove trailing semicolons for command matching
        while (!trimmed.empty() && trimmed.back() == ';') {
            trimmed.pop_back();
        }
        
        // Trim any trailing whitespace after semicolon removal
        end = trimmed.find_last_not_of(" \t\n\r");
        if (end != std::string::npos) {
            trimmed = trimmed.substr(0, end + 1);
        }
        
        // Handle built-in commands (case-sensitive for now)
        if (trimmed == "exit" || trimmed == "quit") {
            std::cout << "Goodbye!\n";
            inputEngine.requestExit();
            return;
        }
        
        if (trimmed == "help") {
            printHelp();
            return;
        }
        
        if (trimmed == "clear") {
            std::cout << "\033[2J\033[H";  // ANSI clear screen
            printBanner();
            return;
        }

        // Aria Specialist query: ?<question>
        if (!trimmed.empty() && trimmed[0] == '?') {
            std::string question = trimmed.substr(1);
            // Trim leading space after '?'
            size_t qs = question.find_first_not_of(" \t");
            if (qs != std::string::npos) question = question.substr(qs);
            if (question.empty()) {
                std::cout << "Usage: ?<question>  (e.g. ?How do I declare a pointer?)\n";
            } else {
                specialistQuery(question);
            }
            return;
        }

        // Execute code
        try {
            // Lex
            parser::ShellLexer lexer(trimmed);
            auto tokens = lexer.tokenize();
            
            // Parse
            parser::ShellParser parser(tokens);
            auto ast = parser.parseProgram();
            
            // Execute only if we have statements
            if (ast && !ast->statements.empty()) {
                executor::Executor exec(globalEnv);
                exec.execute(*ast);
                
                // Show result if there was one
                auto result = exec.getLastResult();
                if (result) {
                    std::cout << "=> " << executor::valueToString(*result) << "\n";
                }
            }
            
        } catch (const parser::ParseError& e) {
            std::cerr << "Parse error: " << e.what() << "\n";
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << "\n";
        } catch (...) {
            std::cerr << "Unknown error during execution\n";
        }
    };
    
    auto onExit = [&]() {
        // User pressed Ctrl+D
        std::cout << "\nGoodbye!\n";
    };
    
    inputEngine.onSubmission(onSubmit);
    inputEngine.onExit(onExit);
    
    // Run the REPL (blocks until exit)
    inputEngine.run();
    
    return 0;
}
