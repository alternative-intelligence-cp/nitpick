#include "test_helpers.h"
#include "frontend/diagnostics.h"
#include <sstream>
#include <fstream>

using namespace aria;

// ============================================================================
// Diagnostic System Tests
// ============================================================================

TEST_CASE(create_error_diagnostic) {
        DiagnosticEngine engine;
        SourceLocation loc("test.aria", 10, 5, 3);
        
        engine.error(loc, "unexpected token ';'");
        
        ASSERT(engine.hasErrors(), "Engine should have errors");
        ASSERT_EQ(engine.errorCount(), 1, "Should have exactly 1 error");
        ASSERT_EQ(engine.warningCount(), 0, "Should have no warnings");
}

TEST_CASE(create_warning_diagnostic) {
        DiagnosticEngine engine;
        SourceLocation loc("test.aria", 15, 12, 5);
        
        engine.warning(loc, "unused variable 'count'");
        
        ASSERT(!engine.hasErrors(), "Engine should not have errors");
        ASSERT(engine.hasWarnings(), "Engine should have warnings");
        ASSERT_EQ(engine.warningCount(), 1, "Should have exactly 1 warning");
}

TEST_CASE(multiple_diagnostics) {
        DiagnosticEngine engine;
        
        engine.error(SourceLocation("test.aria", 10, 5), "error 1");
        engine.error(SourceLocation("test.aria", 20, 10), "error 2");
        engine.warning(SourceLocation("test.aria", 30, 15), "warning 1");
        
        ASSERT_EQ(engine.errorCount(), 2, "Should have 2 errors");
        ASSERT_EQ(engine.warningCount(), 1, "Should have 1 warning");
        ASSERT_EQ(engine.diagnostics().size(), 3, "Should have 3 total diagnostics");
}

TEST_CASE(diagnostic_notes_and_suggestions) {
        DiagnosticEngine engine;
        
        engine.error(SourceLocation("test.aria", 10, 5), "type mismatch");
        engine.addNote("expected 'int32' but got 'string'");
        engine.addSuggestion("convert the value using 'int32(value)'");
        
        const auto& diags = engine.diagnostics();
        ASSERT_EQ(diags.size(), 1, "Should have 1 diagnostic");
        ASSERT_EQ(diags[0]->notes().size(), 1, "Should have 1 note");
        ASSERT_EQ(diags[0]->suggestions().size(), 1, "Should have 1 suggestion");
}

TEST_CASE(warnings_as_errors) {
        DiagnosticEngine engine;
        engine.setWarningsAsErrors(true);
        
        engine.warning(SourceLocation("test.aria", 10, 5), "unused variable");
        
        ASSERT(engine.hasErrors(), "Warning should be treated as error");
        ASSERT_EQ(engine.errorCount(), 1, "Should have 1 error");
        ASSERT_EQ(engine.warningCount(), 0, "Should have no warnings");
}

TEST_CASE(clear_diagnostics) {
        DiagnosticEngine engine;
        
        engine.error(SourceLocation("test.aria", 10, 5), "error");
        engine.warning(SourceLocation("test.aria", 20, 10), "warning");
        
        ASSERT(engine.hasErrors(), "Should have errors before clear");
        
        engine.clear();
        
        ASSERT(!engine.hasErrors(), "Should not have errors after clear");
        ASSERT(!engine.hasWarnings(), "Should not have warnings after clear");
        ASSERT_EQ(engine.diagnostics().size(), 0, "Should have no diagnostics");
}

TEST_CASE(diagnostic_levels) {
        DiagnosticEngine engine;
        
        engine.note(SourceLocation("test.aria", 10, 5), "informational note");
        engine.warning(SourceLocation("test.aria", 20, 10), "warning message");
        engine.error(SourceLocation("test.aria", 30, 15), "error message");
        engine.fatal(SourceLocation("test.aria", 40, 20), "fatal error");
        
        const auto& diags = engine.diagnostics();
        ASSERT_EQ(diags[0]->level(), DiagnosticLevel::NOTE, "First should be NOTE");
        ASSERT_EQ(diags[1]->level(), DiagnosticLevel::WARNING, "Second should be WARNING");
        ASSERT_EQ(diags[2]->level(), DiagnosticLevel::ERROR, "Third should be ERROR");
        ASSERT_EQ(diags[3]->level(), DiagnosticLevel::FATAL, "Fourth should be FATAL");
}

TEST_CASE(source_location) {
        SourceLocation loc("test.aria", 42, 15, 7);
        
        ASSERT_EQ(loc.filename, "test.aria", "Filename should match");
        ASSERT_EQ(loc.line, 42, "Line should be 42");
        ASSERT_EQ(loc.column, 15, "Column should be 15");
    ASSERT_EQ(loc.length, 7, "Length should be 7");
}
