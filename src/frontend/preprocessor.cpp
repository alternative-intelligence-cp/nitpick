#include "preprocessor.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <cctype>
#include <sys/stat.h>

namespace aria {
namespace frontend {

Preprocessor::Preprocessor() 
    : pos(0), line(1), col(1), context_counter(0) {
    // Initialize with empty context stack
}

char Preprocessor::peek() {
    return pos < source.length() ? source[pos] : 0;
}

char Preprocessor::peekNext() {
    return (pos + 1 < source.length()) ? source[pos + 1] : 0;
}

void Preprocessor::advance() {
    if (peek() == '\n') {
        line++;
        col = 1;
    } else {
        col++;
    }
    pos++;
}

void Preprocessor::skipWhitespace() {
    while (peek() == ' ' || peek() == '\t') {
        advance();
    }
}

std::string Preprocessor::readLine() {
    std::string result;
    while (peek() != '\n' && peek() != 0) {
        result += peek();
        advance();
    }
    return result;
}

std::string Preprocessor::readWord() {
    std::string word;
    while (isalnum(peek()) || peek() == '_') {
        word += peek();
        advance();
    }
    return word;
}

std::string Preprocessor::readUntilNewline() {
    std::string result;
    while (peek() != '\n' && peek() != 0) {
        result += peek();
        advance();
    }
    // Trim trailing whitespace
    while (!result.empty() && (result.back() == ' ' || result.back() == '\t')) {
        result.pop_back();
    }
    return result;
}

void Preprocessor::error(const std::string& message) {
    std::cerr << "Preprocessor Error at " << current_file << ":" << line << ":" << col 
              << " - " << message << std::endl;
    throw std::runtime_error("Preprocessor error: " + message);
}

void Preprocessor::warning(const std::string& message) {
    std::cerr << "Preprocessor Warning at " << current_file << ":" << line << ":" << col 
              << " - " << message << std::endl;
}

// Main preprocessing function
std::string Preprocessor::process(const std::string& source_text, const std::string& file_path) {
    current_file = file_path;
    source = source_text;
    pos = 0;
    line = 1;
    col = 1;
    
    std::stringstream output;
    std::vector<std::string> current_macro_args;  // For tracking macro expansion context
    
    while (peek() != 0) {
        char c = peek();
        
        // Handle preprocessor directives (% followed by alpha, can be indented)
        if (c == '%') {
            // Peek ahead to see if it's a directive or a context-local label
            if (peekNext() == '$') {
                // It's a context-local label definition (%$label:), not a directive
                // Let it fall through to normal code processing
            } else if (isalpha(peekNext())) {
                // It's a directive
                advance(); // Skip %
                std::string directive = readWord();
            
            if (directive == "macro") {
                handleMacroDefinition();
            } else if (directive == "endmacro") {
                handleMacroEnd();
            } else if (directive == "define") {
                handleDefine();
            } else if (directive == "undef") {
                handleUndef();
            } else if (directive == "ifdef") {
                handleIfdef();
            } else if (directive == "ifndef") {
                handleIfndef();
            } else if (directive == "if") {
                handleIf();
            } else if (directive == "elif") {
                handleElif();
            } else if (directive == "else") {
                handleElse();
            } else if (directive == "endif") {
                handleEndif();
            } else if (directive == "include") {
                handleInclude();
                // Don't skip to end of line - handleInclude already positioned us correctly
                continue;
            } else if (directive == "push") {
                handlePush();
            } else if (directive == "pop") {
                handlePop();
            } else if (directive == "context") {
                handleContext();
            } else if (directive == "rep") {
                handleRep();
                // Don't skip to end of line - handleRep already positioned us correctly
                continue;
            } else if (directive == "endrep") {
                error("%endrep without matching %rep");
            } else {
                    error("Unknown preprocessor directive: %" + directive);
                }
                
                // Skip to end of line
                while (peek() != '\n' && peek() != 0) advance();
                if (peek() == '\n') advance();
                continue;
            } else {
                // Not a directive, fall through to normal code processing
            }
        }
        
        // Check if we're in an inactive conditional block
        if (!conditional_stack.empty() && !conditional_stack.top().is_active) {
            // Skip this line
            while (peek() != '\n' && peek() != 0) advance();
            if (peek() == '\n') advance();
            continue;
        }
        
        // Handle macro expansion in code
        // Check if we're at the start of an identifier (potential macro call)
        if (isalpha(c) || c == '_') {
            size_t save_pos = pos;
            int save_line = line;
            int save_col = col;
            
            std::string identifier = readWord();
            
            // Check if it's a macro
            if (macros.find(identifier) != macros.end()) {
                // It's a macro call - expand it
                std::vector<std::string> args;
                skipWhitespace();
                
                // Read macro arguments (if any)
                const Macro& macro = macros[identifier];
                if (macro.param_count > 0) {
                    // Check for opening parenthesis
                    if (peek() == '(') {
                        advance(); // Skip (
                        
                        for (int i = 0; i < macro.param_count; i++) {
                            skipWhitespace();
                            
                            // Read argument
                            std::string arg;
                            int paren_depth = 0;
                            bool in_quotes = false;
                            
                            while (peek() != 0) {
                                char ch = peek();
                                
                                // Handle quoted strings
                                if (ch == '"' && (arg.empty() || arg.back() != '\\')) {
                                    in_quotes = !in_quotes;
                                    arg += ch;
                                    advance();
                                    continue;
                                }
                                
                                // Track parentheses depth
                                if (!in_quotes) {
                                    if (ch == '(') paren_depth++;
                                    if (ch == ')') {
                                        if (paren_depth == 0) break; // End of argument list
                                        paren_depth--;
                                    }
                                    
                                    // Comma separates arguments (at depth 0)
                                    if (ch == ',' && paren_depth == 0) {
                                        advance(); // Skip comma
                                        break;
                                    }
                                }
                                
                                arg += ch;
                                advance();
                            }
                            
                            // Trim whitespace from argument
                            while (!arg.empty() && (arg.front() == ' ' || arg.front() == '\t')) {
                                arg.erase(0, 1);
                            }
                            while (!arg.empty() && (arg.back() == ' ' || arg.back() == '\t')) {
                                arg.pop_back();
                            }
                            
                            args.push_back(arg);
                        }
                        
                        // Expect closing parenthesis
                        if (peek() == ')') {
                            advance();
                        } else {
                            error("Expected ')' after macro arguments");
                        }
                    } else {
                        // Space-separated arguments (NASM style)
                        for (int i = 0; i < macro.param_count; i++) {
                            skipWhitespace();
                            
                            // Read argument until whitespace, comma, or newline
                            std::string arg;
                            bool in_quotes = false;
                            
                            while (peek() != 0 && peek() != '\n') {
                                if (peek() == '"') in_quotes = !in_quotes;
                                
                                if (!in_quotes && (peek() == ' ' || peek() == '\t' || peek() == ',')) {
                                    if (!arg.empty()) break;
                                    advance();
                                    continue;
                                }
                                
                                arg += peek();
                                advance();
                            }
                            
                            args.push_back(arg);
                        }
                    }
                }
                
                // Expand the macro
                std::string expanded = expandMacro(identifier, args);
                output << expanded;
                continue;
            }
            
            // Check if it's a constant
            if (constants.find(identifier) != constants.end()) {
                // Substitute constant value
                output << constants[identifier];
                continue;
            }
            
            // Not a macro or constant - restore position and output identifier
            pos = save_pos;
            line = save_line;
            col = save_col;
            
            output << c;
            advance();
            continue;
        }
        
        // Handle %$label (context-local labels) in code
        if (c == '%' && peekNext() == '$') {
            advance(); // Skip %
            advance(); // Skip $
            
            std::string label = readWord();
            std::string expanded = expandContextLocal(label);
            output << expanded;
            continue;
        }
        
        // Regular code - pass through
        output << c;
        advance();
    }
    
    // Check for unclosed blocks
    if (!conditional_stack.empty()) {
        error("Unclosed %if/%ifdef/%ifndef block");
    }
    
    if (!context_stack.empty()) {
        error("Unclosed context (missing %pop)");
    }
    
    return output.str();
}

void Preprocessor::handleMacroDefinition() {
    skipWhitespace();
    std::string macro_name = readWord();
    
    if (macro_name.empty()) {
        error("%macro requires a name");
    }
    
    skipWhitespace();
    std::string param_count_str = readWord();
    int param_count = 0;
    
    if (!param_count_str.empty()) {
        try {
            param_count = std::stoi(param_count_str);
        } catch (...) {
            error("%macro parameter count must be a number");
        }
    }
    
    // Read macro body until %endmacro
    std::stringstream body;
    int start_line = line;
    
    while (peek() != 0) {
        if (peek() == '\n') {
            body << '\n';
            advance();
            
            // Check for %endmacro
            if (peek() == '%') {
                size_t save_pos = pos;
                int save_line = line;
                int save_col = col;
                
                advance();
                std::string word = readWord();
                
                if (word == "endmacro") {
                    // Found end of macro
                    break;
                } else {
                    // Not endmacro, restore position and continue
                    pos = save_pos;
                    line = save_line;
                    col = save_col;
                }
            }
        } else {
            body << peek();
            advance();
        }
    }
    
    // Store macro definition
    Macro macro;
    macro.name = macro_name;
    macro.param_count = param_count;
    macro.body = body.str();
    macro.line_defined = start_line;
    
    macros[macro_name] = macro;
}

void Preprocessor::handleMacroEnd() {
    // This should only be encountered when we're not inside a macro definition
    // If we get here, it means there's a stray %endmacro
    error("Unexpected %endmacro (no matching %macro)");
}

void Preprocessor::handleDefine() {
    skipWhitespace();
    std::string name = readWord();
    
    if (name.empty()) {
        error("%define requires a name");
    }
    
    skipWhitespace();
    std::string value = readUntilNewline();
    
    constants[name] = value;
}

void Preprocessor::handleUndef() {
    skipWhitespace();
    std::string name = readWord();
    
    if (name.empty()) {
        error("%undef requires a name");
    }
    
    constants.erase(name);
}

void Preprocessor::handleIfdef() {
    skipWhitespace();
    std::string name = readWord();
    
    if (name.empty()) {
        error("%ifdef requires a name");
    }
    
    bool is_defined = constants.find(name) != constants.end() || 
                      macros.find(name) != macros.end();
    
    ConditionalState state;
    state.is_active = is_defined;
    state.has_matched = is_defined;
    state.line = line;
    
    conditional_stack.push(state);
}

void Preprocessor::handleIfndef() {
    skipWhitespace();
    std::string name = readWord();
    
    if (name.empty()) {
        error("%ifndef requires a name");
    }
    
    bool is_defined = constants.find(name) != constants.end() || 
                      macros.find(name) != macros.end();
    
    ConditionalState state;
    state.is_active = !is_defined;
    state.has_matched = !is_defined;
    state.line = line;
    
    conditional_stack.push(state);
}

void Preprocessor::handleIf() {
    skipWhitespace();
    std::string expr = readUntilNewline();
    
    if (expr.empty()) {
        error("%if requires an expression");
    }
    
    bool result = evaluateCondition(expr);
    
    ConditionalState state;
    state.is_active = result;
    state.has_matched = result;
    state.line = line;
    
    conditional_stack.push(state);
}

void Preprocessor::handleElif() {
    if (conditional_stack.empty()) {
        error("%elif without matching %if");
    }
    
    skipWhitespace();
    std::string expr = readUntilNewline();
    
    if (expr.empty()) {
        error("%elif requires an expression");
    }
    
    ConditionalState& state = conditional_stack.top();
    
    if (!state.has_matched) {
        bool result = evaluateCondition(expr);
        state.is_active = result;
        state.has_matched = result;
    } else {
        state.is_active = false;
    }
}

void Preprocessor::handleElse() {
    if (conditional_stack.empty()) {
        error("%else without matching %if");
    }
    
    ConditionalState& state = conditional_stack.top();
    state.is_active = !state.has_matched;
}

void Preprocessor::handleEndif() {
    if (conditional_stack.empty()) {
        error("%endif without matching %if");
    }
    
    conditional_stack.pop();
}

void Preprocessor::handleInclude() {
    // Save start position of the include directive (right after %)
    size_t directive_start = pos;
    
    skipWhitespace();
    
    // Read filename (can be "file" or <file>)
    std::string filename;
    bool is_system_include = false;
    char quote = peek();
    
    if (quote == '"' || quote == '<') {
        is_system_include = (quote == '<');
        advance();
        char end_quote = (quote == '"') ? '"' : '>';
        
        while (peek() != end_quote && peek() != '\n' && peek() != 0) {
            filename += peek();
            advance();
        }
        
        if (peek() != end_quote) {
            error("Unterminated filename in %include");
        }
        advance();
    } else {
        filename = readWord();
    }
    
    if (filename.empty()) {
        error("%include requires a filename");
    }
    
    // Skip to end of line
    skipWhitespace();
    if (peek() == '\n') advance();
    
    // Now pos is right after the %include line
    size_t after_directive = pos;
    
    // Resolve file path
    std::string resolved_path = resolveIncludePath(filename, is_system_include);
    
    if (resolved_path.empty()) {
        error("Cannot find include file: " + filename);
    }
    
    // Check for circular include using resolved path
    if (included_files.find(resolved_path) != included_files.end()) {
        warning("Circular include detected: " + filename + " (skipping)");
        return;
    }
    
    // Read the file
    std::ifstream file(resolved_path);
    if (!file.is_open()) {
        error("Cannot open include file: " + resolved_path);
    }
    
    std::string file_contents((std::istreambuf_iterator<char>(file)),
                               std::istreambuf_iterator<char>());
    file.close();
    
    // Mark as included
    included_files.insert(resolved_path);
    
    // Save current state (source, position, file)
    std::string saved_source = source;
    size_t saved_pos = pos;
    int saved_line = line;
    int saved_col = col;
    std::string saved_file = current_file;
    
    // Process the included file recursively
    std::string processed_content = process(file_contents, resolved_path);
    
    // Restore state
    source = saved_source;
    pos = saved_pos;
    line = saved_line;
    col = saved_col;
    current_file = saved_file;
    
    // Insert the processed content after the %include directive
    source.insert(after_directive, processed_content);
    
    // Don't advance pos - let the main loop output the inserted content
    // pos is still at the position it was when we started (after reading the filename)
    // But we need it to be at after_directive so we skip the %include line itself
    // Actually, we want to position right at after_directive so the next iteration
    // starts reading the inserted content
    pos = after_directive;
}

void Preprocessor::handlePush() {
    skipWhitespace();
    std::string context_name = readWord();
    
    if (context_name.empty()) {
        context_name = "__anonymous_" + std::to_string(context_counter++);
    }
    
    MacroContext ctx;
    ctx.name = context_name;
    ctx.depth = context_stack.size();
    
    context_stack.push(ctx);
}

void Preprocessor::handlePop() {
    if (context_stack.empty()) {
        error("%pop without matching %push");
    }
    
    context_stack.pop();
}

void Preprocessor::handleContext() {
    skipWhitespace();
    std::string context_name = readWord();
    
    if (context_name.empty()) {
        error("%context requires a name");
    }
    
    if (context_stack.empty()) {
        error("%context used without active context (use %push first)");
    }
    
    context_stack.top().name = context_name;
}

void Preprocessor::handleRep() {
    skipWhitespace();
    std::string count_str = readWord();
    
    if (count_str.empty()) {
        error("%rep requires a count");
    }
    
    // Evaluate count - could be a constant or number
    int count = 0;
    
    // Check if it's a constant first
    auto const_it = constants.find(count_str);
    if (const_it != constants.end()) {
        try {
            count = std::stoi(const_it->second);
        } catch (...) {
            error("%rep count must be a number, got constant: " + const_it->second);
        }
    } else {
        // Try to parse as number
        try {
            count = std::stoi(count_str);
        } catch (...) {
            error("%rep count must be a number or defined constant, got: " + count_str);
        }
    }
    
    if (count < 0) {
        error("%rep count must be non-negative");
    }
    
    // Skip to end of %rep line
    while (peek() != '\n' && peek() != 0) advance();
    if (peek() == '\n') advance();
    
    // Read block until %endrep (handle nesting)
    std::string block;
    int nesting = 1;  // We're inside one %rep already
    
    while (nesting > 0 && peek() != 0) {
        // Check if this is a preprocessor directive (% followed by alpha)
        if (peek() == '%') {
            size_t saved_pos = pos;
            int saved_line = line;
            int saved_col = col;
            
            advance();  // Skip %
            
            // Check if it's a directive
            if (isalpha(peek())) {
                std::string directive = readWord();
                
                if (directive == "rep") {
                    // Nested %rep
                    nesting++;
                    block += '%';
                    block += directive;
                    // Add rest of line
                    while (peek() != '\n' && peek() != 0) {
                        block += peek();
                        advance();
                    }
                    if (peek() == '\n') {
                        block += '\n';
                        advance();
                    }
                } else if (directive == "endrep") {
                    nesting--;
                    if (nesting > 0) {
                        // Still inside nested block
                        block += '%';
                        block += directive;
                        // Skip rest of line
                        while (peek() != '\n' && peek() != 0) {
                            block += peek();
                            advance();
                        }
                        if (peek() == '\n') {
                            block += '\n';
                            advance();
                        }
                    } else {
                        // Found matching %endrep - skip rest of line and break
                        while (peek() != '\n' && peek() != 0) advance();
                        if (peek() == '\n') advance();
                        break;
                    }
                } else {
                    // Other directive, add to block
                    block += '%';
                    block += directive;
                    while (peek() != '\n' && peek() != 0) {
                        block += peek();
                        advance();
                    }
                    if (peek() == '\n') {
                        block += '\n';
                        advance();
                    }
                }
            } else {
                // % not followed by alpha, restore and treat as regular char
                pos = saved_pos;
                line = saved_line;
                col = saved_col;
                block += peek();
                advance();
            }
        } else {
            // Regular character
            block += peek();
            advance();
        }
    }
    
    if (nesting > 0) {
        error("Unclosed %rep block (missing %endrep)");
    }
    
    // Expand the block 'count' times
    std::string expanded;
    for (int i = 0; i < count; i++) {
        expanded += block;
    }
    
    // Insert expanded text at current position and rewind to process it
    // This allows nested %rep blocks to be processed
    size_t insert_pos = pos;
    int insert_line = line;
    source.insert(pos, expanded);
    
    // Rewind to start of inserted text so the main loop will process it
    pos = insert_pos;
    line = insert_line;
    col = 1;
}

bool Preprocessor::evaluateCondition(const std::string& expr) {
    // Simple expression evaluator
    // For now, just check if it's a defined constant with non-zero value
    
    std::string trimmed = expr;
    // Trim whitespace
    trimmed.erase(0, trimmed.find_first_not_of(" \t"));
    trimmed.erase(trimmed.find_last_not_of(" \t") + 1);
    
    // Check if it's a constant
    auto it = constants.find(trimmed);
    if (it != constants.end()) {
        // Check if value is non-zero
        try {
            int val = std::stoi(it->second);
            return val != 0;
        } catch (...) {
            // Non-numeric constant, treat as true if non-empty
            return !it->second.empty();
        }
    }
    
    // Try to evaluate as numeric literal
    try {
        int val = std::stoi(trimmed);
        return val != 0;
    } catch (...) {
        // Not a number, treat as false
        return false;
    }
}

// Macro expansion implementation
std::string Preprocessor::expandMacro(const std::string& macro_name, const std::vector<std::string>& args) {
    auto it = macros.find(macro_name);
    if (it == macros.end()) {
        error("Macro not defined: " + macro_name);
    }
    
    const Macro& macro = it->second;
    
    // Check argument count
    if (args.size() != static_cast<size_t>(macro.param_count)) {
        error("Macro " + macro_name + " expects " + std::to_string(macro.param_count) + 
              " arguments, got " + std::to_string(args.size()));
    }
    
    // Expand macro body, substituting parameters
    std::string result;
    std::string body = macro.body;
    
    for (size_t i = 0; i < body.length(); i++) {
        // Check for %N parameter references
        if (body[i] == '%' && i + 1 < body.length() && isdigit(body[i + 1])) {
            i++; // Skip %
            
            // Read parameter number
            std::string param_num;
            while (i < body.length() && isdigit(body[i])) {
                param_num += body[i];
                i++;
            }
            i--; // Back up one since loop will increment
            
            int param_index = std::stoi(param_num);
            
            // Substitute parameter (1-indexed)
            if (param_index >= 1 && param_index <= static_cast<int>(args.size())) {
                result += args[param_index - 1];
            } else {
                error("Macro parameter %" + param_num + " out of range");
            }
        }
        // Check for %$label context-local references
        else if (body[i] == '%' && i + 1 < body.length() && body[i + 1] == '$') {
            i += 2; // Skip %$
            
            // Read label name
            std::string label;
            while (i < body.length() && (isalnum(body[i]) || body[i] == '_')) {
                label += body[i];
                i++;
            }
            i--; // Back up one
            
            result += expandContextLocal(label);
        }
        else {
            result += body[i];
        }
    }
    
    return result;
}

std::string Preprocessor::expandContextLocal(const std::string& label) {
    if (context_stack.empty()) {
        error("Context-local label %" + label + " used outside of context");
    }
    
    MacroContext& ctx = context_stack.top();
    
    // Check if we've already generated a unique name for this label
    auto it = ctx.local_labels.find(label);
    if (it != ctx.local_labels.end()) {
        return it->second;
    }
    
    // Generate unique label: context_name_depth_label
    std::string unique_label = ctx.name + "_" + std::to_string(ctx.depth) + "_" + label;
    ctx.local_labels[label] = unique_label;
    
    return unique_label;
}

std::string Preprocessor::expandMacroParam(int param_index, const std::vector<std::string>& current_args) {
    // 1-indexed parameters
    if (param_index < 1 || param_index > static_cast<int>(current_args.size())) {
        error("Macro parameter %" + std::to_string(param_index) + " out of range");
    }
    
    return current_args[param_index - 1];
}

std::string Preprocessor::resolveIncludePath(const std::string& filename, bool is_system) {
    // Helper to check if file exists
    auto file_exists = [](const std::string& path) {
        struct stat buffer;
        return (stat(path.c_str(), &buffer) == 0);
    };
    
    // For quoted includes ("file"), try relative to current file first
    if (!is_system && !current_file.empty()) {
        // Get directory of current file
        size_t last_slash = current_file.find_last_of("/\\");
        if (last_slash != std::string::npos) {
            std::string dir = current_file.substr(0, last_slash + 1);
            std::string candidate = dir + filename;
            if (file_exists(candidate)) {
                return candidate;
            }
        }
    }
    
    // Try include paths
    for (const auto& include_path : include_paths) {
        std::string candidate = include_path;
        if (!candidate.empty() && candidate.back() != '/' && candidate.back() != '\\') {
            candidate += '/';
        }
        candidate += filename;
        
        if (file_exists(candidate)) {
            return candidate;
        }
    }
    
    // Try as-is (absolute or relative to working directory)
    if (file_exists(filename)) {
        return filename;
    }
    
    return "";  // Not found
}

void Preprocessor::addIncludePath(const std::string& path) {
    include_paths.push_back(path);
}

void Preprocessor::defineConstant(const std::string& name, const std::string& value) {
    constants[name] = value;
}

// Query methods
bool Preprocessor::isMacroDefined(const std::string& name) const {
    return macros.find(name) != macros.end();
}

bool Preprocessor::isConstantDefined(const std::string& name) const {
    return constants.find(name) != constants.end();
}

const Macro* Preprocessor::getMacro(const std::string& name) const {
    auto it = macros.find(name);
    return (it != macros.end()) ? &it->second : nullptr;
}

} // namespace frontend
} // namespace aria
