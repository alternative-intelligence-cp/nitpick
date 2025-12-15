/**
 * src/backend/vtable.h
 *
 * Virtual Table Generation for Trait Dynamic Dispatch
 * 
 * Generates vtables for trait objects and implements fat pointer representation.
 */

#ifndef ARIA_BACKEND_VTABLE_H
#define ARIA_BACKEND_VTABLE_H

#include "../frontend/ast.h"
#include "../frontend/ast/stmt.h"
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <memory>
#include <string>
#include <map>
#include <vector>

namespace aria {
namespace backend {

// Vtable layout for a trait
struct VtableLayout {
    std::string trait_name;
    std::vector<std::string> method_names;  // Ordered list of method names
    std::map<std::string, size_t> method_indices;  // method_name -> index in vtable
};

// Fat pointer structure for trait objects
// Represented in LLVM as: { i8* data, vtable_type* vtable }
struct TraitObjectLayout {
    std::string trait_name;
    llvm::StructType* llvm_type;  // LLVM struct type for the fat pointer
    llvm::StructType* vtable_type;  // LLVM struct type for the vtable
};

// Vtable generator
class VtableGenerator {
private:
    llvm::LLVMContext& llvm_context;
    llvm::Module& llvm_module;
    llvm::IRBuilder<>& builder;
    
    // Map from trait name to vtable layout
    std::map<std::string, VtableLayout> vtable_layouts;
    
    // Map from trait name to fat pointer layout
    std::map<std::string, TraitObjectLayout> trait_object_layouts;
    
    // Map from (trait_name, type_name) to vtable global variable
    std::map<std::pair<std::string, std::string>, llvm::GlobalVariable*> vtable_instances;
    
    // Trait and impl tables
    std::map<std::string, frontend::TraitDecl*> trait_table;
    std::multimap<std::string, frontend::ImplDecl*> impl_table;
    
    // Create LLVM function pointer type for a trait method
    llvm::FunctionType* createMethodFunctionType(
        const frontend::TraitMethod& method,
        llvm::Type* self_type
    );
    
public:
    VtableGenerator(
        llvm::LLVMContext& ctx,
        llvm::Module& mod,
        llvm::IRBuilder<>& b
    ) : llvm_context(ctx), llvm_module(mod), builder(b) {}
    
    // Register a trait declaration
    void registerTrait(frontend::TraitDecl* trait);
    
    // Register a trait implementation
    void registerImpl(frontend::ImplDecl* impl);
    
    // Generate vtable layout for a trait
    VtableLayout generateVtableLayout(const std::string& trait_name);
    
    // Generate vtable LLVM type for a trait
    llvm::StructType* generateVtableType(const std::string& trait_name);
    
    // Generate fat pointer type for a trait object
    llvm::StructType* generateTraitObjectType(const std::string& trait_name);
    
    // Generate vtable instance for a specific trait/type combination
    llvm::GlobalVariable* generateVtableInstance(
        const std::string& trait_name,
        const std::string& type_name
    );
    
    // Create a trait object (fat pointer) from a concrete value
    // Returns LLVM value of type { i8* data, vtable* vtable }
    llvm::Value* createTraitObject(
        llvm::Value* concrete_value,
        const std::string& concrete_type,
        const std::string& trait_name
    );
    
    // Call a method on a trait object (dynamic dispatch)
    llvm::Value* callTraitMethod(
        llvm::Value* trait_object,
        const std::string& trait_name,
        const std::string& method_name,
        const std::vector<llvm::Value*>& args
    );
    
    // Generate all vtables for all registered implementations
    void generateAllVtables();
};

} // namespace backend
} // namespace aria

#endif // ARIA_BACKEND_VTABLE_H
