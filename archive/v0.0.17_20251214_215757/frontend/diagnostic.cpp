/**
 * src/frontend/diagnostic.cpp
 * 
 * Diagnostic Engine Implementation
 */

#include "diagnostic.h"
#include <sstream>
#include <iomanip>

namespace aria {
namespace frontend {

void DiagnosticEngine::error(int line, int col, const std::string& message, const std::string& suggestion) {
    diagnostics.push_back(std::make_unique<Diagnostic>(DiagnosticLevel::ERROR, line, col, message, suggestion));
    error_count++;
}

void DiagnosticEngine::warning(int line, int col, const std::string& message, const std::string& suggestion) {
    diagnostics.push_back(std::make_unique<Diagnostic>(DiagnosticLevel::WARNING, line, col, message, suggestion));
    warning_count++;
}

void DiagnosticEngine::note(int line, int col, const std::string& message) {
    diagnostics.push_back(std::make_unique<Diagnostic>(DiagnosticLevel::NOTE, line, col, message));
}

void DiagnosticEngine::print_diagnostics(std::ostream& out) const {
    for (const auto& diag : diagnostics) {
        const char* color = get_color_for_level(diag->level);
        const char* label = get_label_for_level(diag->level);
        
        // Print header: "error: message"
        if (use_color) {
            out << COLOR_BOLD << color << label << COLOR_RESET << ": " << diag->message << "\n";
        } else {
            out << label << ": " << diag->message << "\n";
        }
        
        // Print location: "  --> file:line:col"
        out << "  ";
        if (use_color) out << COLOR_CYAN;
        out << "--> " << filename << ":" << diag->line << ":" << diag->column;
        if (use_color) out << COLOR_RESET;
        out << "\n";
        
        // Print source context with highlighting
        print_source_context(out, diag->line, diag->column, color);
        
        // Print suggestion if provided
        if (!diag->suggestion.empty()) {
            out << "  ";
            if (use_color) out << COLOR_BLUE;
            out << "help: " << diag->suggestion;
            if (use_color) out << COLOR_RESET;
            out << "\n";
        }
        
        out << "\n";
    }
    
    // Print summary
    if (error_count > 0 || warning_count > 0) {
        out << "Compilation ";
        if (error_count > 0) {
            if (use_color) out << COLOR_RED;
            out << "failed";
            if (use_color) out << COLOR_RESET;
        } else {
            if (use_color) out << COLOR_YELLOW;
            out << "completed with warnings";
            if (use_color) out << COLOR_RESET;
        }
        out << ": ";
        
        if (error_count > 0) {
            out << error_count << " error" << (error_count != 1 ? "s" : "");
        }
        if (warning_count > 0) {
            if (error_count > 0) out << ", ";
            out << warning_count << " warning" << (warning_count != 1 ? "s" : "");
        }
        out << "\n";
    }
}

void DiagnosticEngine::clear() {
    diagnostics.clear();
    error_count = 0;
    warning_count = 0;
}

std::string DiagnosticEngine::get_source_line(int line) const {
    if (line < 1 || source_code.empty()) return "";
    
    std::istringstream iss(source_code);
    std::string current_line;
    int current_line_num = 1;
    
    while (std::getline(iss, current_line)) {
        if (current_line_num == line) {
            return current_line;
        }
        current_line_num++;
    }
    
    return "";
}

void DiagnosticEngine::print_source_context(std::ostream& out, int line, int col, const char* color) const {
    std::string source_line = get_source_line(line);
    if (source_line.empty()) return;
    
    // Print line number gutter
    out << "   " << std::setw(4) << line << " | ";
    
    // Print source line
    out << source_line << "\n";
    
    // Print column indicator (^)
    out << "        | ";  // Align with source
    
    // Add spaces before indicator
    for (int i = 1; i < col; i++) {
        out << " ";
    }
    
    // Print indicator
    if (use_color) out << COLOR_BOLD << color;
    out << "^";
    if (use_color) out << COLOR_RESET;
    out << "\n";
}

const char* DiagnosticEngine::get_color_for_level(DiagnosticLevel level) const {
    if (!use_color) return "";
    
    switch (level) {
        case DiagnosticLevel::NOTE:    return COLOR_BLUE;
        case DiagnosticLevel::WARNING: return COLOR_YELLOW;
        case DiagnosticLevel::ERROR:   return COLOR_RED;
        default:                       return COLOR_RESET;
    }
}

const char* DiagnosticEngine::get_label_for_level(DiagnosticLevel level) const {
    switch (level) {
        case DiagnosticLevel::NOTE:    return "note";
        case DiagnosticLevel::WARNING: return "warning";
        case DiagnosticLevel::ERROR:   return "error";
        default:                       return "diagnostic";
    }
}

} // namespace frontend
} // namespace aria
