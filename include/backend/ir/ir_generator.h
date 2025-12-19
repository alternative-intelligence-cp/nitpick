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
class BlockStmt;  // For defer stack support
namespace sema {
    class Type;  // Forward declaration in correct namespace
    class TypeSystem;  // Forward declaration for custom type lookups
    struct Specialization;  // Forward declaration for generic specializations
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
    
    // Type system for looking up struct types
    sema::TypeSystem* type_system;
    
    // Symbol table for LLVM values (maps variable names to LLVM Values)
    std::map<std::string, llvm::Value*> named_values;
    
    // Track Aria types for LLVM values (needed for member access with opaque pointers)
    std::map<llvm::Value*, Type*> value_types;
    
    // Type mapping cache (Aria types -> LLVM types)
    std::map<std::string, llvm::Type*> type_map;
    
    // Loop context stack for break/continue support
    struct LoopContext {
        llvm::BasicBlock* continueTarget;  // Where 'continue' jumps to
        llvm::BasicBlock* breakTarget;     // Where 'break' jumps to
        
        LoopContext(llvm::BasicBlock* cont, llvm::BasicBlock* brk)
            : continueTarget(cont), breakTarget(brk) {}
    };
    std::vector<LoopContext> loop_stack;
    
    // Defer statement support (RAII cleanup)
    std::vector<std::vector<BlockStmt*>> defer_stack;
    
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
     * Map type name string to LLVM Type
     * @param type_name String name like "int32", "flt64", "bool"
     * @return Corresponding LLVM type
     */
    llvm::Type* mapTypeFromName(const std::string& type_name);
    
    /**
     * Generate code for a statement node
     */
    llvm::Value* codegenStatement(ASTNode* stmt);
    
    /**
     * Generate code for an expression node
     */
    llvm::Value* codegenExpression(ASTNode* expr);
    
    /**
     * Execute defer blocks in the current scope (LIFO order)
     */
    void executeScopeDefers();
    
    /**
     * Execute all defer blocks up to function level (LIFO order)
     * Called by return statements
     */
    void executeFunctionDefers();
    
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
     * Set the TypeSystem for looking up custom types (structs, unions, etc.)
     * Must be called before codegen if the code uses custom types
     * @param ts Pointer to TypeSystem (must remain valid for lifetime of IR generation)
     */
    void setTypeSystem(sema::TypeSystem* ts);
    
    /**
     * Generate LLVM IR for an AST node
     * @param node AST node to generate code for
     * @return LLVM Value representing the generated code
     */
    llvm::Value* codegen(ASTNode* node);
    
    /**
     * Generate LLVM IR for specialized generic functions
     * @param specializations Vector of function specializations from Monomorphizer
     * @return Number of functions generated
     */
    size_t codegenSpecializedFunctions(const std::vector<sema::Specialization*>& specializations);
    
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
