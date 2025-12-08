#ifndef ARIA_FRONTEND_SEMA_ESCAPE_ANALYSIS_H
#define ARIA_FRONTEND_SEMA_ESCAPE_ANALYSIS_H

#include "../ast.h"

namespace aria {
namespace sema {

// Escape Analysis Result
// Tracks which wild pointers escape their function scope
struct EscapeAnalysisResult {
    bool has_escapes;           // True if any wild pointers escape
    int escaped_count;          // Number of escaped pointers
    bool has_wildx_violations;  // True if wildx (executable memory) pointers escape (SECURITY CRITICAL)
    
    EscapeAnalysisResult() : has_escapes(false), escaped_count(0), has_wildx_violations(false) {}
};

// Run Escape Analysis on AST
// Detects when wild pointers escape function scope (safety violation)
EscapeAnalysisResult run_escape_analysis(frontend::Block* ast);

} // namespace sema
} // namespace aria

#endif // ARIA_FRONTEND_SEMA_ESCAPE_ANALYSIS_H
