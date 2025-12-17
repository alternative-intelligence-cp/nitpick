#ifndef ARIA_IR_GENERATOR_H
#define ARIA_IR_GENERATOR_H

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/Type.h>
#include <map>
#include <string>
#include <memory>

// Forward declarations
class ASTNode;  // ASTNode is in global namespace

namespace aria {
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
    
    /**
     * Map Aria type to LLVM type
     * Reference: research_012-017 for type specifications
     */
    llvm::Type* mapType(Type* aria_type);
    
public:
    /**
     * Constructor
     * @param module_name Name of the LLVM module to generate
     */
    explicit IRGenerator(const std::string& module_name);
    
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
     * Dump the generated IR to stdout (for debugging)
     */
    void dump();
};

} // namespace aria

#endif // ARIA_IR_GENERATOR_H
