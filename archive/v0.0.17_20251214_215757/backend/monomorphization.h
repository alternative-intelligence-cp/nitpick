/**
 * src/backend/monomorphization.h
 *
 * Monomorphization Engine for Trait Static Dispatch
 * 
 * Clones trait method implementations and specializes them for concrete types.
 * Generates specialized function names with type mangling.
 */

#ifndef ARIA_BACKEND_MONOMORPHIZATION_H
#define ARIA_BACKEND_MONOMORPHIZATION_H

#include "../frontend/ast.h"
#include "../frontend/ast/stmt.h"
#include <memory>
#include <string>
#include <map>
#include <vector>

namespace aria {
namespace backend {

// Monomorphization context - tracks specializations
struct MonomorphizationContext {
    // Map from (trait_name, type_name, method_name) -> specialized_function_name
    std::map<std::tuple<std::string, std::string, std::string>, std::string> specialization_map;
    
    // Cache of generated specialized functions
    std::vector<std::unique_ptr<frontend::FuncDecl>> specialized_functions;
    
    // Map from trait name to trait declaration
    std::map<std::string, frontend::TraitDecl*> trait_table;
    
    // Map from trait name to implementations
    std::multimap<std::string, frontend::ImplDecl*> impl_table;
};

// Monomorphization engine
class Monomorphizer {
private:
    MonomorphizationContext& context;
    
    // Generate specialized function name
    // Format: {trait}_{type}_{method}
    std::string generateSpecializedName(
        const std::string& trait_name,
        const std::string& type_name,
        const std::string& method_name
    );
    
    // Clone a function declaration for specialization
    std::unique_ptr<frontend::FuncDecl> cloneFuncDecl(frontend::FuncDecl* original);
    
    // Clone an expression (deep copy)
    std::unique_ptr<frontend::Expression> cloneExpr(frontend::Expression* expr);
    
    // Clone a statement (deep copy)
    std::unique_ptr<frontend::Statement> cloneStmt(frontend::Statement* stmt);
    
    // Clone a block (deep copy)
    std::unique_ptr<frontend::Block> cloneBlock(frontend::Block* block);
    
public:
    Monomorphizer(MonomorphizationContext& ctx) : context(ctx) {}
    
    // Register a trait declaration
    void registerTrait(frontend::TraitDecl* trait) {
        context.trait_table[trait->name] = trait;
    }
    
    // Register a trait implementation
    void registerImpl(frontend::ImplDecl* impl) {
        context.impl_table.insert({impl->trait_name, impl});
    }
    
    // Get or create specialized function for a trait method call
    // Returns the specialized function name
    std::string getOrCreateSpecialization(
        const std::string& trait_name,
        const std::string& type_name,
        const std::string& method_name
    );
    
    // Monomorphize all registered trait implementations
    // Returns vector of specialized function declarations
    std::vector<frontend::FuncDecl*> monomorphizeAll();
};

} // namespace backend
} // namespace aria

#endif // ARIA_BACKEND_MONOMORPHIZATION_H
