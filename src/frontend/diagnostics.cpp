#include "frontend/diagnostics.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>

namespace aria {

// ============================================================================
// DiagnosticEngine Implementation
// ============================================================================

DiagnosticEngine::DiagnosticEngine() 
    : error_count_(0), warning_count_(0), 
      color_enabled_(true), show_source_context_(true), 
      warnings_as_errors_(false) {
    // Check if stdout is a terminal (for color support)
    // On Unix: isatty(fileno(stderr)) would be more correct, but keeping simple
}

void DiagnosticEngine::report(DiagnosticLevel level, const SourceLocation& loc,
                                const std::string& message) {
    // Upgrade warnings to errors if configured
    if (level == DiagnosticLevel::WARNING && warnings_as_errors_) {
        level = DiagnosticLevel::ERROR;
    }
    
    // Create diagnostic
    auto diag = std::make_unique<Diagnostic>(level, loc, message);
    
    // Update counts
    if (level == DiagnosticLevel::ERROR || level == DiagnosticLevel::FATAL) {
        error_count_++;
    } else if (level == DiagnosticLevel::WARNING) {
        warning_count_++;
    }
    
    diagnostics_.push_back(std::move(diag));
}

void DiagnosticEngine::error(const SourceLocation& loc, const std::string& message) {
    report(DiagnosticLevel::ERROR, loc, message);
}

void DiagnosticEngine::warning(const SourceLocation& loc, const std::string& message) {
    report(DiagnosticLevel::WARNING, loc, message);
}

void DiagnosticEngine::note(const SourceLocation& loc, const std::string& message) {
    report(DiagnosticLevel::NOTE, loc, message);
}

void DiagnosticEngine::fatal(const SourceLocation& loc, const std::string& message) {
    report(DiagnosticLevel::FATAL, loc, message);
}

void DiagnosticEngine::addNote(const std::string& note) {
    if (!diagnostics_.empty()) {
        diagnostics_.back()->addNote(note);
    }
}

void DiagnosticEngine::addSuggestion(const std::string& suggestion) {
    if (!diagnostics_.empty()) {
        diagnostics_.back()->addSuggestion(suggestion);
    }
}

void DiagnosticEngine::clear() {
    diagnostics_.clear();
    error_count_ = 0;
    warning_count_ = 0;
}

void DiagnosticEngine::printAll() const {
    for (const auto& diag : diagnostics_) {
        print(*diag);
        std::cerr << "\n";  // Blank line between diagnostics
    }
    
    // Print summary if we have multiple diagnostics
    if (diagnostics_.size() > 1) {
        std::cerr << (color_enabled_ ? BOLD : "") << "Summary: "
                  << (color_enabled_ ? RESET : "");
        
        if (error_count_ > 0) {
            std::cerr << (color_enabled_ ? RED : "") << error_count_ 
                      << " error" << (error_count_ != 1 ? "s" : "")
                      << (color_enabled_ ? RESET : "");
        }
        
        if (error_count_ > 0 && warning_count_ > 0) {
            std::cerr << ", ";
        }
        
        if (warning_count_ > 0) {
            std::cerr << (color_enabled_ ? YELLOW : "") << warning_count_ 
                      << " warning" << (warning_count_ != 1 ? "s" : "")
                      << (color_enabled_ ? RESET : "");
        }
        
        std::cerr << "\n";
    }
}

void DiagnosticEngine::print(const Diagnostic& diag) const {
    const SourceLocation& loc = diag.location();
    
    // Format: filename:line:column: level: message
    // Example: main.aria:10:5: error: unexpected token ';'
    
    std::cerr << (color_enabled_ ? BOLD : "") << loc.filename 
              << ":" << loc.line << ":" << loc.column << ": "
              << (color_enabled_ ? RESET : "");
    
    std::cerr << getLevelColor(diag.level()) << getLevelString(diag.level()) 
              << (color_enabled_ ? RESET : "") << ": ";
    
    std::cerr << (color_enabled_ ? BOLD : "") << diag.message() 
              << (color_enabled_ ? RESET : "") << "\n";
    
    // Print source context if enabled
    if (show_source_context_) {
        printSourceContext(diag);
    }
    
    // Print notes
    for (const auto& note : diag.notes()) {
        std::cerr << (color_enabled_ ? BLUE : "") << "note: " 
                  << (color_enabled_ ? RESET : "") << note << "\n";
    }
    
    // Print suggestions
    for (const auto& suggestion : diag.suggestions()) {
        std::cerr << (color_enabled_ ? GREEN : "") << "suggestion: " 
                  << (color_enabled_ ? RESET : "") << suggestion << "\n";
    }
}

std::string DiagnosticEngine::getLevelString(DiagnosticLevel level) const {
    switch (level) {
        case DiagnosticLevel::NOTE:    return "note";
        case DiagnosticLevel::WARNING: return "warning";
        case DiagnosticLevel::ERROR:   return "error";
        case DiagnosticLevel::FATAL:   return "fatal error";
        default:                        return "unknown";
    }
}

std::string DiagnosticEngine::getLevelColor(DiagnosticLevel level) const {
    if (!color_enabled_) return "";
    
    switch (level) {
        case DiagnosticLevel::NOTE:    return BLUE;
        case DiagnosticLevel::WARNING: return YELLOW;
        case DiagnosticLevel::ERROR:   return RED;
        case DiagnosticLevel::FATAL:   return RED;
        default:                        return "";
    }
}

std::string DiagnosticEngine::readSourceLine(const std::string& filename, int line) const {
    std::ifstream file(filename);
    if (!file.is_open()) {
        return "";  // Can't read file, skip source context
    }
    
    std::string current_line;
    int current_line_num = 1;
    
    while (std::getline(file, current_line)) {
        if (current_line_num == line) {
            return current_line;
        }
        current_line_num++;
    }
    
    return "";  // Line not found
}

void DiagnosticEngine::printSourceContext(const Diagnostic& diag) const {
    const SourceLocation& loc = diag.location();
    
    // Read the source line
    std::string source_line = readSourceLine(loc.filename, loc.line);
    if (source_line.empty()) {
        return;  // Couldn't read source, skip context
    }
    
    // Print the source line with line number
    std::cerr << std::setw(5) << loc.line << " | " << source_line << "\n";
    
    // Print the caret indicator (^)
    // Calculate spacing: line number width (5) + " | " (3) + column position
    std::string spacing(5 + 3 + loc.column - 1, ' ');
    std::cerr << spacing << (color_enabled_ ? GREEN : "");
    
    // Print carets under the problem area
    for (int i = 0; i < loc.length; i++) {
        std::cerr << "^";
    }
    
    std::cerr << (color_enabled_ ? RESET : "") << "\n";
}

} // namespace aria
