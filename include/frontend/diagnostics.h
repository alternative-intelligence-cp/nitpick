#ifndef ARIA_DIAGNOSTICS_H
#define ARIA_DIAGNOSTICS_H

#include <string>
#include <vector>
#include <memory>
#include <sstream>
#include <iostream>

namespace aria {

// ============================================================================
// Diagnostic Severity Levels
// ============================================================================

enum class DiagnosticLevel {
    NOTE,       // Informational message (blue)
    WARNING,    // Warning that doesn't stop compilation (yellow)
    ERROR,      // Error that prevents compilation (red)
    FATAL       // Fatal error that stops compilation immediately (bright red)
};

// Helper function to print DiagnosticLevel
inline std::ostream& operator<<(std::ostream& os, DiagnosticLevel level) {
    switch (level) {
        case DiagnosticLevel::NOTE:    return os << "NOTE";
        case DiagnosticLevel::WARNING: return os << "WARNING";
        case DiagnosticLevel::ERROR:   return os << "ERROR";
        case DiagnosticLevel::FATAL:   return os << "FATAL";
        default:                       return os << "UNKNOWN";
    }
}

// ============================================================================
// Source Location - Where the diagnostic occurred
// ============================================================================

struct SourceLocation {
    std::string filename;
    int line;           // 1-indexed
    int column;         // 1-indexed
    int length;         // Length of the token/span
    
    SourceLocation(const std::string& file, int ln, int col, int len = 1)
        : filename(file), line(ln), column(col), length(len) {}
};

// ============================================================================
// Diagnostic - A single compiler message (error/warning/note)
// ============================================================================

class Diagnostic {
public:
    Diagnostic(DiagnosticLevel level, const SourceLocation& loc,
               const std::string& message)
        : level_(level), location_(loc), message_(message) {}
    
    // Add a note to this diagnostic (helpful context)
    void addNote(const std::string& note) {
        notes_.push_back(note);
    }
    
    // Add a suggestion for fixing the issue
    void addSuggestion(const std::string& suggestion) {
        suggestions_.push_back(suggestion);
    }
    
    // Getters
    DiagnosticLevel level() const { return level_; }
    const SourceLocation& location() const { return location_; }
    const std::string& message() const { return message_; }
    const std::vector<std::string>& notes() const { return notes_; }
    const std::vector<std::string>& suggestions() const { return suggestions_; }
    
private:
    DiagnosticLevel level_;
    SourceLocation location_;
    std::string message_;
    std::vector<std::string> notes_;
    std::vector<std::string> suggestions_;
};

// ============================================================================
// DiagnosticEngine - Collects and formats diagnostics
// ============================================================================

class DiagnosticEngine {
public:
    DiagnosticEngine();
    
    // Add a diagnostic
    void report(DiagnosticLevel level, const SourceLocation& loc,
                const std::string& message);
    
    // Convenience methods
    void error(const SourceLocation& loc, const std::string& message);
    void warning(const SourceLocation& loc, const std::string& message);
    void note(const SourceLocation& loc, const std::string& message);
    void fatal(const SourceLocation& loc, const std::string& message);
    
    // Add a note to the last diagnostic
    void addNote(const std::string& note);
    
    // Add a suggestion to the last diagnostic
    void addSuggestion(const std::string& suggestion);
    
    // Check if we have errors
    bool hasErrors() const { return error_count_ > 0; }
    bool hasWarnings() const { return warning_count_ > 0; }
    
    // Get counts
    int errorCount() const { return error_count_; }
    int warningCount() const { return warning_count_; }
    
    // Get all diagnostics
    const std::vector<std::unique_ptr<Diagnostic>>& diagnostics() const {
        return diagnostics_;
    }
    
    // Print all diagnostics to stderr
    void printAll() const;
    
    // Print a single diagnostic
    void print(const Diagnostic& diag) const;
    
    // Configuration
    void setColorEnabled(bool enabled) { color_enabled_ = enabled; }
    void setShowSourceContext(bool enabled) { show_source_context_ = enabled; }
    void setWarningsAsErrors(bool enabled) { warnings_as_errors_ = enabled; }
    
    // Clear all diagnostics
    void clear();
    
private:
    std::vector<std::unique_ptr<Diagnostic>> diagnostics_;
    int error_count_ = 0;
    int warning_count_ = 0;
    bool color_enabled_ = true;
    bool show_source_context_ = true;
    bool warnings_as_errors_ = false;
    
    // ANSI color codes
    const char* RED = "\033[1;31m";
    const char* YELLOW = "\033[1;33m";
    const char* BLUE = "\033[1;34m";
    const char* GREEN = "\033[1;32m";
    const char* BOLD = "\033[1m";
    const char* RESET = "\033[0m";
    
    // Helper methods
    std::string getLevelString(DiagnosticLevel level) const;
    std::string getLevelColor(DiagnosticLevel level) const;
    std::string readSourceLine(const std::string& filename, int line) const;
    void printSourceContext(const Diagnostic& diag) const;
};

} // namespace aria

#endif // ARIA_DIAGNOSTICS_H
