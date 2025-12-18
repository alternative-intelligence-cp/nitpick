#include "frontend/diagnostics.h"
#include <iostream>

using namespace aria;

int main() {
    DiagnosticEngine engine;
    
    std::cout << "=== Aria Diagnostic System Demo ===" << std::endl;
    std::cout << std::endl;
    
    // Example 1: Simple error
    engine.error(
        SourceLocation("example.aria", 10, 15, 5),
        "unexpected token 'const'"
    );
    engine.addNote("did you mean to use 'let' instead?");
    engine.addSuggestion("Replace 'const' with 'let'");
    
    // Example 2: Type error
    engine.error(
        SourceLocation("example.aria", 25, 20, 3),
        "type mismatch: expected 'int', found 'str'"
    );
    engine.addNote("in expression: let x: int = \"hello\"");
    engine.addSuggestion("Change the type annotation to 'str' or convert the value to 'int'");
    
    // Example 3: Warning
    engine.warning(
        SourceLocation("example.aria", 42, 9, 5),
        "unused variable 'count'"
    );
    engine.addSuggestion("Remove the variable or prefix with '_' to indicate intentional non-use");
    
    // Example 4: Fatal error
    engine.fatal(
        SourceLocation("example.aria", 100, 1, 0),
        "unexpected end of file while parsing function body"
    );
    engine.addNote("function 'main' started at line 95");
    
    // Print all diagnostics
    engine.printAll();
    
    return engine.hasErrors() ? 1 : 0;
}
