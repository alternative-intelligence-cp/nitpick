/**
 * src/frontend/diagnostic.h
 * 
 * Diagnostic Engine for Aria Compiler
 * 
 * Provides structured error/warning reporting with:
 * - Multi-error collection (continue after first error)
 * - Source location highlighting
 * - Color-coded output
 * - Severity levels (error/warning/note)
 * - Did-you-mean suggestions
 * 
 * Replaces throw runtime_error() pattern with structured diagnostics.
 */

#ifndef ARIA_FRONTEND_DIAGNOSTIC_H
#define ARIA_FRONTEND_DIAGNOSTIC_H

#include <string>
#include <vector>
#include <memory>
#include <iostream>

namespace aria {
namespace frontend {

/**
 * Diagnostic Severity Levels
 */
enum class DiagnosticLevel {
    NOTE,     // Informational (blue)
    WARNING,  // Potential issue (yellow)
    ERROR     // Compilation error (red)
};

/**
 * Single Diagnostic Message
 * 
 * Represents one error/warning with:
 * - Location (line, column)
 * - Message text
 * - Severity level
 * - Optional suggestion
 */
struct Diagnostic {
    DiagnosticLevel level;
    int line;
    int column;
    std::string message;
    std::string suggestion;  // Optional "did you mean?" hint
    
    Diagnostic(DiagnosticLevel lvl, int ln, int col, const std::string& msg, const std::string& hint = "")
        : level(lvl), line(ln), column(col), message(msg), suggestion(hint) {}
};

/**
 * Diagnostic Engine
 * 
 * Central error reporting system for the compiler.
 * Collects errors without stopping compilation, allowing
 * multiple errors to be reported in a single pass.
 */
class DiagnosticEngine {
private:
    std::string filename;
    std::string source_code;
    std::vector<std::unique_ptr<Diagnostic>> diagnostics;
    bool use_color;
    int error_count;
    int warning_count;
    
    // ANSI Color Codes
    static constexpr const char* COLOR_RED    = "\033[1;31m";
    static constexpr const char* COLOR_YELLOW = "\033[1;33m";
    static constexpr const char* COLOR_BLUE   = "\033[1;34m";
    static constexpr const char* COLOR_CYAN   = "\033[1;36m";
    static constexpr const char* COLOR_RESET  = "\033[0m";
    static constexpr const char* COLOR_BOLD   = "\033[1m";
    
public:
    /**
     * Constructor
     * 
     * @param file: Source filename (for display)
     * @param source: Full source code text (for context highlighting)
     * @param color: Enable ANSI color codes (default: true)
     */
    DiagnosticEngine(const std::string& file, const std::string& source, bool color = true)
        : filename(file), source_code(source), use_color(color), error_count(0), warning_count(0) {}
    
    /**
     * Report an error
     * 
     * Adds an error diagnostic to the collection.
     * Does NOT stop compilation - allows multiple errors to be reported.
     * 
     * @param line: Line number (1-indexed)
     * @param col: Column number (1-indexed)
     * @param message: Error description
     * @param suggestion: Optional "did you mean?" hint
     */
    void error(int line, int col, const std::string& message, const std::string& suggestion = "");
    
    /**
     * Report a warning
     * 
     * Adds a warning diagnostic to the collection.
     * Warnings do not prevent compilation.
     * 
     * @param line: Line number (1-indexed)
     * @param col: Column number (1-indexed)
     * @param message: Warning description
     * @param suggestion: Optional hint
     */
    void warning(int line, int col, const std::string& message, const std::string& suggestion = "");
    
    /**
     * Report an informational note
     * 
     * Adds a note diagnostic (typically follows an error/warning).
     * 
     * @param line: Line number (1-indexed)
     * @param col: Column number (1-indexed)
     * @param message: Note text
     */
    void note(int line, int col, const std::string& message);
    
    /**
     * Print all collected diagnostics
     * 
     * Outputs formatted error/warning messages to stderr.
     * Shows source context with highlighting.
     * 
     * @param out: Output stream (default: std::cerr)
     */
    void print_diagnostics(std::ostream& out = std::cerr) const;
    
    /**
     * Check if compilation should fail
     * 
     * @return: true if any errors were reported
     */
    bool has_errors() const { return error_count > 0; }
    
    /**
     * Get error count
     */
    int get_error_count() const { return error_count; }
    
    /**
     * Get warning count
     */
    int get_warning_count() const { return warning_count; }
    
    /**
     * Get total diagnostic count
     */
    int get_diagnostic_count() const { return diagnostics.size(); }
    
    /**
     * Clear all diagnostics
     */
    void clear();
    
private:
    /**
     * Get line from source code
     * 
     * @param line: Line number (1-indexed)
     * @return: Text of that line (empty if out of bounds)
     */
    std::string get_source_line(int line) const;
    
    /**
     * Print source context with highlighting
     * 
     * Shows:
     * - Line number
     * - Source text
     * - Column indicator (^)
     * 
     * @param out: Output stream
     * @param line: Line number
     * @param col: Column number
     * @param color: Color code for highlight
     */
    void print_source_context(std::ostream& out, int line, int col, const char* color) const;
    
    /**
     * Get color code for diagnostic level
     */
    const char* get_color_for_level(DiagnosticLevel level) const;
    
    /**
     * Get label string for diagnostic level
     */
    const char* get_label_for_level(DiagnosticLevel level) const;
};

} // namespace frontend
} // namespace aria

#endif // ARIA_FRONTEND_DIAGNOSTIC_H
