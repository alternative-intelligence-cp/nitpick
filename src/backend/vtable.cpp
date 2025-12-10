/**
 * src/backend/vtable.cpp
 *
 * Virtual Table Generation Implementation
 */

#include "vtable.h"
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Constants.h>
#include <sstream>
#include <stdexcept>

namespace aria {
namespace backend {

// Register trait
void VtableGenerator::registerTrait(frontend::TraitDecl* trait) {
    trait_table[trait->name] = trait;
}

// Register implementation
void VtableGenerator::registerImpl(frontend::ImplDecl* impl) {
    impl_table.insert({impl->trait_name, impl});
}

// Generate vtable layout for a trait
VtableLayout VtableGenerator::generateVtableLayout(const std::string& trait_name) {
    // Check if already generated
    auto it = vtable_layouts.find(trait_name);
    if (it != vtable_layouts.end()) {
        return it->second;
    }
    
    // Find trait declaration
    auto trait_it = trait_table.find(trait_name);
    if (trait_it == trait_table.end()) {
        throw std::runtime_error("Trait not found: " + trait_name);
    }
    
    frontend::TraitDecl* trait = trait_it->second;
    
    VtableLayout layout;
    layout.trait_name = trait_name;
    
    // Collect all methods from trait and super traits
    std::function<void(frontend::TraitDecl*)> collect_methods;
    collect_methods = [&](frontend::TraitDecl* t) {
        // Add methods from super traits first (inheritance order)
        for (const auto& super_name : t->super_traits) {
            auto super_it = trait_table.find(super_name);
            if (super_it != trait_table.end()) {
                collect_methods(super_it->second);
            }
        }
        
        // Add methods from this trait
        for (const auto& method : t->methods) {
            // Check for duplicate (override from super trait)
            if (layout.method_indices.find(method.name) == layout.method_indices.end()) {
                size_t index = layout.method_names.size();
                layout.method_names.push_back(method.name);
                layout.method_indices[method.name] = index;
            }
        }
    };
    
    collect_methods(trait);
    
    // Cache layout
    vtable_layouts[trait_name] = layout;
    
    return layout;
}

// Create LLVM function type for a trait method
llvm::FunctionType* VtableGenerator::createMethodFunctionType(
    const frontend::TraitMethod& method,
    llvm::Type* self_type
) {
    // TODO: Implement proper type mapping from Aria types to LLVM types
    // For now, use placeholder i8* for all types
    
    std::vector<llvm::Type*> param_types;
    
    // First parameter is always 'self' (pointer to the concrete type)
    param_types.push_back(self_type);
    
    // Add remaining parameters
    for (const auto& param : method.parameters) {
        param_types.push_back(llvm::PointerType::get(llvm_context, 0));
    }
    
    // Determine return type
    llvm::Type* return_type;
    if (method.return_type == "void") {
        return_type = llvm::Type::getVoidTy(llvm_context);
    } else {
        return_type = llvm::PointerType::get(llvm_context, 0);
    }
    
    return llvm::FunctionType::get(return_type, param_types, false);
}

// Generate vtable LLVM type
llvm::StructType* VtableGenerator::generateVtableType(const std::string& trait_name) {
    VtableLayout layout = generateVtableLayout(trait_name);
    
    // Create struct type with function pointers
    std::vector<llvm::Type*> method_ptr_types;
    
    // Get trait declaration for method signatures
    auto trait_it = trait_table.find(trait_name);
    if (trait_it == trait_table.end()) {
        throw std::runtime_error("Trait not found: " + trait_name);
    }
    
    frontend::TraitDecl* trait = trait_it->second;
    
    // For each method in vtable layout, create function pointer type
    for (const auto& method_name : layout.method_names) {
        // Find method signature in trait
        const frontend::TraitMethod* method_sig = nullptr;
        for (const auto& m : trait->methods) {
            if (m.name == method_name) {
                method_sig = &m;
                break;
            }
        }
        
        if (!method_sig) {
            throw std::runtime_error("Method not found in trait: " + method_name);
        }
        
        // Create function type (all methods take self as i8*)
        llvm::Type* self_type = llvm::PointerType::get(llvm_context, 0);
        llvm::FunctionType* func_type = createMethodFunctionType(*method_sig, self_type);
        
        // Create function pointer type
        llvm::PointerType* func_ptr_type = llvm::PointerType::get(func_type, 0);
        method_ptr_types.push_back(func_ptr_type);
    }
    
    // Create struct type for vtable
    std::string vtable_name = "vtable_" + trait_name;
    llvm::StructType* vtable_type = llvm::StructType::create(
        llvm_context,
        method_ptr_types,
        vtable_name
    );
    
    return vtable_type;
}

// Generate fat pointer type for trait object
llvm::StructType* VtableGenerator::generateTraitObjectType(const std::string& trait_name) {
    // Check cache
    auto it = trait_object_layouts.find(trait_name);
    if (it != trait_object_layouts.end()) {
        return it->second.llvm_type;
    }
    
    // Generate vtable type
    llvm::StructType* vtable_type = generateVtableType(trait_name);
    
    // Create fat pointer struct: { i8* data, vtable* vtable }
    std::vector<llvm::Type*> fat_ptr_fields = {
        llvm::PointerType::get(llvm_context, 0),  // data pointer (opaque ptr)
        llvm::PointerType::get(vtable_type, 0)   // vtable pointer
    };
    
    std::string fat_ptr_name = "trait_object_" + trait_name;
    llvm::StructType* fat_ptr_type = llvm::StructType::create(
        llvm_context,
        fat_ptr_fields,
        fat_ptr_name
    );
    
    // Cache layout
    TraitObjectLayout layout;
    layout.trait_name = trait_name;
    layout.llvm_type = fat_ptr_type;
    layout.vtable_type = vtable_type;
    trait_object_layouts[trait_name] = layout;
    
    return fat_ptr_type;
}

// Generate vtable instance for specific trait/type
llvm::GlobalVariable* VtableGenerator::generateVtableInstance(
    const std::string& trait_name,
    const std::string& type_name
) {
    // Check cache
    auto key = std::make_pair(trait_name, type_name);
    auto it = vtable_instances.find(key);
    if (it != vtable_instances.end()) {
        return it->second;
    }
    
    // Get vtable type
    llvm::StructType* vtable_type = generateVtableType(trait_name);
    
    // Get vtable layout
    VtableLayout layout = generateVtableLayout(trait_name);
    
    // Find implementation
    auto range = impl_table.equal_range(trait_name);
    frontend::ImplDecl* target_impl = nullptr;
    
    for (auto impl_it = range.first; impl_it != range.second; ++impl_it) {
        if (impl_it->second->type_name == type_name) {
            target_impl = impl_it->second;
            break;
        }
    }
    
    if (!target_impl) {
        std::stringstream ss;
        ss << "No implementation of trait '" << trait_name 
           << "' for type '" << type_name << "'";
        throw std::runtime_error(ss.str());
    }
    
    // Create vtable initializer with method pointers
    std::vector<llvm::Constant*> method_ptrs;
    
    for (const auto& method_name : layout.method_names) {
        // Find method in implementation
        llvm::Function* method_func = nullptr;
        
        // Generate specialized method name
        std::string specialized_name = trait_name + "_" + type_name + "_" + method_name;
        
        // Look up function in module
        method_func = llvm_module.getFunction(specialized_name);
        
        if (!method_func) {
            // Method not yet compiled - create a placeholder
            // This will be filled in during code generation
            throw std::runtime_error("Method function not found: " + specialized_name);
        }
        
        method_ptrs.push_back(method_func);
    }
    
    // Create vtable constant
    llvm::Constant* vtable_init = llvm::ConstantStruct::get(vtable_type, method_ptrs);
    
    // Create global variable for vtable
    std::string vtable_var_name = "vtable_" + trait_name + "_" + type_name;
    llvm::GlobalVariable* vtable_var = new llvm::GlobalVariable(
        llvm_module,
        vtable_type,
        true,  // isConstant
        llvm::GlobalValue::InternalLinkage,
        vtable_init,
        vtable_var_name
    );
    
    // Cache vtable instance
    vtable_instances[key] = vtable_var;
    
    return vtable_var;
}

// Create trait object from concrete value
llvm::Value* VtableGenerator::createTraitObject(
    llvm::Value* concrete_value,
    const std::string& concrete_type,
    const std::string& trait_name
) {
    // Get fat pointer type
    llvm::StructType* fat_ptr_type = generateTraitObjectType(trait_name);
    
    // Get vtable for this type
    llvm::GlobalVariable* vtable = generateVtableInstance(trait_name, concrete_type);
    
    // Allocate fat pointer on stack
    llvm::Value* fat_ptr = builder.CreateAlloca(fat_ptr_type, nullptr, "trait_obj");
    
    // Cast concrete value to i8*
    llvm::Value* data_ptr = builder.CreateBitCast(
        concrete_value,
        llvm::PointerType::get(llvm_context, 0),
        "data_ptr"
    );
    
    // Store data pointer in fat pointer
    llvm::Value* data_field_ptr = builder.CreateStructGEP(fat_ptr_type, fat_ptr, 0, "data_field");
    builder.CreateStore(data_ptr, data_field_ptr);
    
    // Store vtable pointer in fat pointer
    llvm::Value* vtable_field_ptr = builder.CreateStructGEP(fat_ptr_type, fat_ptr, 1, "vtable_field");
    builder.CreateStore(vtable, vtable_field_ptr);
    
    return builder.CreateLoad(fat_ptr_type, fat_ptr, "trait_object");
}

// Call method on trait object (dynamic dispatch)
llvm::Value* VtableGenerator::callTraitMethod(
    llvm::Value* trait_object,
    const std::string& trait_name,
    const std::string& method_name,
    const std::vector<llvm::Value*>& args
) {
    // Get vtable layout
    VtableLayout layout = generateVtableLayout(trait_name);
    
    // Get method index
    auto method_it = layout.method_indices.find(method_name);
    if (method_it == layout.method_indices.end()) {
        throw std::runtime_error("Method not found in trait: " + method_name);
    }
    size_t method_index = method_it->second;
    
    // Get fat pointer type
    llvm::StructType* fat_ptr_type = generateTraitObjectType(trait_name);
    
    // Extract vtable pointer from trait object
    llvm::Value* vtable_ptr = builder.CreateExtractValue(trait_object, 1, "vtable_ptr");
    
    // Extract data pointer from trait object
    llvm::Value* data_ptr = builder.CreateExtractValue(trait_object, 0, "data_ptr");
    
    // Get method pointer from vtable
    llvm::Value* method_ptr_ptr = builder.CreateStructGEP(
        trait_object_layouts[trait_name].vtable_type,
        vtable_ptr,
        method_index,
        "method_ptr_ptr"
    );
    
    llvm::Value* method_ptr = builder.CreateLoad(
        llvm::PointerType::get(llvm_context, 0),  // Opaque pointer type
        method_ptr_ptr,
        "method_ptr"
    );
    
    // Prepare arguments: self + args
    std::vector<llvm::Value*> call_args = {data_ptr};
    call_args.insert(call_args.end(), args.begin(), args.end());
    
    // Call method through function pointer
    // In LLVM 20 with opaque pointers, CreateCall infers the function type
    // We need to get the function type from the trait method signature
    llvm::FunctionType* func_type = llvm::FunctionType::get(
        llvm::PointerType::get(llvm_context, 0),  // return type (generic ptr)
        {llvm::PointerType::get(llvm_context, 0)},  // self parameter
        false  // not vararg
    );
    
    return builder.CreateCall(
        func_type,
        method_ptr,
        call_args,
        "trait_method_call"
    );
}

// Generate all vtables
void VtableGenerator::generateAllVtables() {
    // For each implementation, generate a vtable instance
    for (const auto& [trait_name, impl] : impl_table) {
        generateVtableInstance(trait_name, impl->type_name);
    }
}

} // namespace backend
} // namespace aria
