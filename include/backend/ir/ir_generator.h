#ifndef ARIA_IR_GENERATOR_H
#define ARIA_IR_GENERATOR_H

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/DIBuilder.h>
#include <llvm/IR/DebugInfoMetadata.h>
#include <map>
#include <string>
#include <memory>
#include <vector>

namespace aria {

// Forward declarations
class ASTNode;  // ASTNode is in aria namespace
namespace sema {
    class Type;  // Forward declaration in correct namespace
}
using sema::Type;  // Make Type available in aria namespace

/**
 * IRGenerator - Generates LLVM IR from Aria AST
 * 
 * This is the main backend class that translates validated AST nodes
 * into LLVM Intermediate Representation for optimization and code generation.
 * 
 * Reference: Phase 4.1 - LLVM Infrastructure Setup
 */
class IRGenerator {
private:
    llvm::LLVMContext context;
    std::unique_ptr<llvm::Module> module;
    llvm::IRBuilder<> builder;
    
    // Symbol table for LLVM values (maps variable names to LLVM Values)
    std::map<std::string, llvm::Value*> named_values;
    
    // Type mapping cache (Aria types -> LLVM types)
    std::map<std::string, llvm::Type*> type_map;
    
    // Debug info generation (Phase 7.4.1)
    std::unique_ptr<llvm::DIBuilder> di_builder;
    llvm::DICompileUnit* di_compile_unit;
    llvm::DIFile* di_file;
    std::vector<llvm::DIScope*> di_scope_stack;  // Stack of lexical scopes
    std::map<std::string, llvm::DIType*> di_type_map;  // Aria types -> DWARF types
    bool debug_enabled;
    
    /**
     * Map Aria type to LLVM type
     * Reference: research_012-017 for type specifications
     */
    llvm::Type* mapType(Type* aria_type);
    
    /**
     * Map Aria type to DWARF debug type
     * Creates typedef for TBB types to enable custom formatters
     */
    llvm::DIType* mapDebugType(Type* aria_type);
    
    /**
     * Push a new lexical scope onto the debug scope stack
     */
    void pushDebugScope(llvm::DIScope* scope);
    
    /**
     * Pop the current lexical scope from the stack
     */
    void popDebugScope();
    
    /**
     * Get the current debug scope (top of stack)
     */
    llvm::DIScope* getCurrentDebugScope();
    
public:
    /**
     * Constructor
     * @param module_name Name of the LLVM module to generate
     * @param enable_debug Enable DWARF debug info emission (default: true)
     */
    explicit IRGenerator(const std::string& module_name, bool enable_debug = true);
    
    /**
     * Initialize debug info generation
     * Must be called before codegen if debug is enabled
     * @param filename Source filename (e.g., "main.aria")
     * @param directory Source directory (absolute path)
     */
    void initDebugInfo(const std::string& filename, const std::string& directory);
    
    /**
     * Finalize debug info generation
     * Must be called after all codegen is complete
     */
    void finalizeDebugInfo();
    
    /**
     * Set current source location for debug info
     * @param line Line number (1-based)
     * @param column Column number (1-based)
     */
    void setDebugLocation(unsigned line, unsigned column);
    
    /**
     * Clear debug location (for compiler-generated code)
     */
    void clearDebugLocation();
    
    /**
     * Generate LLVM IR for an AST node
     * @param node AST node to generate code for
     * @return LLVM Value representing the generated code
     */
    llvm::Value* codegen(ASTNode* node);
    
    /**
     * Get the generated LLVM module
     * @return Pointer to LLVM Module (ownership retained)
     */
    llvm::Module* getModule();
    
    /**
     * Take ownership of the generated LLVM module
     * @return unique_ptr to LLVM Module (ownership transferred)
     */
    std::unique_ptr<llvm::Module> takeModule();
    
    /**
     * Dump the generated IR to stdout (for debugging)
     */
    void dump();
};

} // namespace aria

#endif // ARIA_IR_GENERATOR_H
