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

using namespace ariash;

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
    std::cout << "  Ctrl+L        - Clear screen\n\n";
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
