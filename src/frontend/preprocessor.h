#ifndef ARIA_FRONTEND_PREPROCESSOR_H
#define ARIA_FRONTEND_PREPROCESSOR_H

#include <string>
#include <map>
#include <vector>
#include <stack>
#include <set>

namespace aria {
namespace frontend {

// Macro Parameter
struct MacroParam {
    std::string name;
    int index;  // %1, %2, etc.
};

// Macro Definition
struct Macro {
    std::string name;
    int param_count;
    std::vector<std::string> param_names;  // Optional named parameters
    std::string body;  // Raw macro body text
    int line_defined;  // For error reporting
};

// Context for context-local symbols
struct MacroContext {
    std::string name;
    std::map<std::string, std::string> local_labels;   // %$label -> unique_label
    std::map<std::string, std::string> local_vars;     // %$var -> value
    int depth;  // Nesting depth for unique label generation
};

// Preprocessor State
class Preprocessor {
private:
    // Macro definitions
    std::map<std::string, Macro> macros;
    
    // Constants defined with %define
    std::map<std::string, std::string> constants;
    
    // Context stack for %push/%pop
    std::stack<MacroContext> context_stack;
    int context_counter;  // For generating unique context IDs
    
    // Include guard - prevent circular includes
    std::set<std::string> included_files;
    std::string current_file;
    std::vector<std::string> include_paths;  // Search paths for includes
    
    // Conditional compilation state
    struct ConditionalState {
        bool is_active;      // Is this branch active?
        bool has_matched;    // Has any branch matched yet?
        int line;            // Line number for error reporting
    };
    std::stack<ConditionalState> conditional_stack;
    
    // Source processing state
    std::string source;
    size_t pos;
    int line;
    int col;
    
    // Helper methods
    char peek();
    char peekNext();
    void advance();
    void skipWhitespace();
    std::string readLine();
    std::string readWord();
    std::string readUntilNewline();
    std::string resolveIncludePath(const std::string& filename, bool is_system);
    
    // Directive handlers
    void handleMacroDefinition();
    void handleMacroEnd();
    void handleDefine();
    void handleUndef();
    void handleIfdef();
    void handleIfndef();
    void handleIf();
    void handleElif();
    void handleElse();
    void handleEndif();
    void handleInclude();
    void handlePush();
    void handlePop();
    void handleContext();
    void handleRep();
    
    // Macro expansion
    std::string expandMacro(const std::string& macro_name, const std::vector<std::string>& args);
    std::string expandContextLocal(const std::string& label);
    std::string expandMacroParam(int param_index, const std::vector<std::string>& current_args);
    
    // Expression evaluation for %if
    bool evaluateCondition(const std::string& expr);
    int parseLogicalOr(const std::string& expr, size_t& pos);
    int parseLogicalAnd(const std::string& expr, size_t& pos);
    int parseComparison(const std::string& expr, size_t& pos);
    int parseAddSub(const std::string& expr, size_t& pos);
    int parseMulDiv(const std::string& expr, size_t& pos);
    int parseUnary(const std::string& expr, size_t& pos);
    int parsePrimary(const std::string& expr, size_t& pos);
    void skipExprWhitespace(const std::string& expr, size_t& pos);
    
    // Error reporting
    void error(const std::string& message);
    void warning(const std::string& message);

public:
    Preprocessor();
    
    // Main preprocessing entry point
    std::string process(const std::string& source_file, const std::string& file_path);
    
    // Configuration
    void addIncludePath(const std::string& path);
    void defineConstant(const std::string& name, const std::string& value);
    
    // State query (for debugging)
    bool isMacroDefined(const std::string& name) const;
    bool isConstantDefined(const std::string& name) const;
    const Macro* getMacro(const std::string& name) const;
};

} // namespace frontend
} // namespace aria

#endif // ARIA_FRONTEND_PREPROCESSOR_H
