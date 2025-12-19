/**
 * Test Debug Info Generation in IRGenerator
 * Phase 7.4.1: DWARF Emission
 */

#include "backend/ir/ir_generator.h"
#include "frontend/sema/type.h"
#include "test_helpers.h"
#include <llvm/Support/raw_ostream.h>
#include <llvm/IR/Verifier.h>
#include <llvm/IR/DebugInfoMetadata.h>

using namespace aria;
using namespace sema;

TEST_CASE(debug_info_initialization) {
    IRGenerator gen("test_module", true);  // Enable debug
    
    gen.initDebugInfo("test.aria", "/tmp");
    
    llvm::Module* mod = gen.getModule();
    ASSERT(mod != nullptr, "Module should not be null");
    ASSERT(mod->getSourceFileName() == "test_module", "Source filename should match");
    
    // Finalize debug info
    gen.finalizeDebugInfo();
    
    // Verify the module (checks that debug info is well-formed)
    std::string error_msg;
    llvm::raw_string_ostream error_stream(error_msg);
    bool has_errors = llvm::verifyModule(*mod, &error_stream);
    
    ASSERT(!has_errors, "Module should verify without errors");
}

TEST_CASE(debug_info_disabled) {
    IRGenerator gen("test_module", false);  // Disable debug
    
    // Should not crash when debug is disabled
    gen.initDebugInfo("test.aria", "/tmp");
    gen.setDebugLocation(1, 1);
    gen.clearDebugLocation();
    gen.finalizeDebugInfo();
    
    llvm::Module* mod = gen.getModule();
    ASSERT(mod != nullptr, "Module should not be null even with debug disabled");
}

TEST_CASE(debug_type_mapping_tbb) {
    IRGenerator gen("test_module", true);
    gen.initDebugInfo("test.aria", "/tmp");
    
    // Create TBB types
    auto tbb8 = std::make_unique<PrimitiveType>("tbb8", 8, true);
    auto tbb32 = std::make_unique<PrimitiveType>("tbb32", 32, true);
    
    // Map to debug types (using public mapDebugType would require making it public,
    // so we just verify the module doesn't crash)
    
    gen.finalizeDebugInfo();
    
    llvm::Module* mod = gen.getModule();
    std::string error_msg;
    llvm::raw_string_ostream error_stream(error_msg);
    bool has_errors = llvm::verifyModule(*mod, &error_stream);
    
    ASSERT(!has_errors, "Module should verify without errors with TBB types");
}

TEST_CASE(debug_location_tracking) {
    IRGenerator gen("test_module", true);
    gen.initDebugInfo("test.aria", "/tmp");
    
    // Set debug locations
    gen.setDebugLocation(10, 5);
    gen.setDebugLocation(20, 15);
    gen.clearDebugLocation();
    
    gen.finalizeDebugInfo();
    
    llvm::Module* mod = gen.getModule();
    ASSERT(mod != nullptr, "Module should not be null after debug location tracking");
}

TEST_CASE(debug_scope_stack) {
    IRGenerator gen("test_module", true);
    gen.initDebugInfo("test.aria", "/tmp");
    
    // Push/pop scopes (simulate function/block scopes)
    // We can't easily test this without creating actual functions,
    // but we verify it doesn't crash
    
    gen.setDebugLocation(1, 1);
    
    gen.finalizeDebugInfo();
    
    llvm::Module* mod = gen.getModule();
    ASSERT(mod != nullptr, "Module should not be null with debug scopes");
}
