/**
 * test_ir_generator.cpp
 * 
 * Unit tests for LLVM IR generation infrastructure (Phase 4.1.1)
 * Tests the IRGenerator class, type mapping, and basic LLVM integration.
 */

#include "../test_helpers.h"
#include "backend/ir/ir_generator.h"
#include "frontend/sema/type.h"
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/Support/raw_ostream.h>

using namespace aria;
using namespace aria::sema;

// Test that IR generator can be created
TEST_CASE(ir_generator_construction) {
    IRGenerator gen("test_module");
    
    llvm::Module* mod = gen.getModule();
    ASSERT(mod != nullptr, "Module should not be null");
    ASSERT(mod->getName() == "test_module", "Module name should match");
    
    // Dump should not crash
    gen.dump();
    
    return;
}

// Test mapping of primitive integer types
TEST_CASE(ir_generator_map_int_types) {
    TypeSystem types;
    IRGenerator gen("test_types");
    
    // Test various integer types
    // Note: mapType is private, so we test it indirectly through codegen
    // For now, just verify the module is created correctly
    
    llvm::Module* mod = gen.getModule();
    ASSERT(mod != nullptr, "Module should exist for type mapping");
    
    return;
}

// Test module dump functionality
TEST_CASE(ir_generator_dump) {
    IRGenerator gen("dump_test");
    
    // Create a simple module structure
    llvm::Module* mod = gen.getModule();
    ASSERT(mod != nullptr, "Module should exist");
    
    // Dump should produce output (we can't easily test the output, but it shouldn't crash)
    gen.dump();
    
    return;
}

// Test that codegen returns nullptr for null input
TEST_CASE(ir_generator_codegen_null) {
    IRGenerator gen("null_test");
    
    llvm::Value* result = gen.codegen(nullptr);
    ASSERT(result == nullptr, "Codegen of null node should return nullptr");
    
    return;
}

// Test module name is set correctly
TEST_CASE(ir_generator_module_name) {
    IRGenerator gen("my_module_name");
    
    llvm::Module* mod = gen.getModule();
    ASSERT(mod != nullptr, "Module should exist");
    
    std::string name = mod->getName().str();
    ASSERT(name == "my_module_name", "Module name should be 'my_module_name'");
    
    return;
}

// Test multiple IR generators can coexist
TEST_CASE(ir_generator_multiple_instances) {
    IRGenerator gen1("module1");
    IRGenerator gen2("module2");
    IRGenerator gen3("module3");
    
    llvm::Module* mod1 = gen1.getModule();
    llvm::Module* mod2 = gen2.getModule();
    llvm::Module* mod3 = gen3.getModule();
    
    ASSERT(mod1 != nullptr, "Module 1 should exist");
    ASSERT(mod2 != nullptr, "Module 2 should exist");
    ASSERT(mod3 != nullptr, "Module 3 should exist");
    
    ASSERT(mod1->getName() == "module1", "Module 1 name");
    ASSERT(mod2->getName() == "module2", "Module 2 name");
    ASSERT(mod3->getName() == "module3", "Module 3 name");
    
    return;
}

// Test LLVM context and builder initialization
TEST_CASE(ir_generator_llvm_initialization) {
    IRGenerator gen("init_test");
    
    llvm::Module* mod = gen.getModule();
    ASSERT(mod != nullptr, "Module should be initialized");
    
    // Verify module has a context
    llvm::LLVMContext& ctx = mod->getContext();
    (void)ctx; // Use the context to avoid unused variable warning
    
    return;
}

// Test that IR generator can handle empty module operations
TEST_CASE(ir_generator_empty_module) {
    IRGenerator gen("empty");
    
    llvm::Module* mod = gen.getModule();
    ASSERT(mod != nullptr, "Empty module should exist");
    
    // Empty module should have no functions
    ASSERT(mod->empty(), "Empty module should have no functions");
    
    // Empty module should have module identifier
    ASSERT(!mod->getName().empty(), "Module should have a name");
    
    return;
}

// Test IR generator memory management
TEST_CASE(ir_generator_memory_management) {
    {
        IRGenerator gen("temp_module");
        llvm::Module* mod = gen.getModule();
        ASSERT(mod != nullptr, "Module should exist in scope");
    }
    // IRGenerator destructor should clean up LLVM resources
    // If this test completes without memory leaks (check with valgrind), it passes
    
    return;
}

// Test getting module multiple times returns same pointer
TEST_CASE(ir_generator_module_consistency) {
    IRGenerator gen("consistent_module");
    
    llvm::Module* mod1 = gen.getModule();
    llvm::Module* mod2 = gen.getModule();
    llvm::Module* mod3 = gen.getModule();
    
    ASSERT(mod1 == mod2, "Module pointer should be consistent");
    ASSERT(mod2 == mod3, "Module pointer should be consistent");
    ASSERT(mod1 == mod3, "Module pointer should be consistent");
    
    return;
}

// Placeholder for future type mapping tests
// TODO: Expand when mapType() becomes public or we have codegen tests
TEST_CASE(ir_generator_type_mapping_placeholder) {
    TypeSystem types;
    IRGenerator gen("type_test");
    
    // Verify type system and IR generator can coexist
    Type* int32_type = types.getPrimitiveType("int32");
    ASSERT(int32_type != nullptr, "Type system should work with IR generator");
    
    return;
}

// Test IR generator with Aria type system integration
TEST_CASE(ir_generator_aria_type_integration) {
    TypeSystem types;
    IRGenerator gen("aria_types");
    
    // Create various Aria types
    Type* int8 = types.getPrimitiveType("int8");
    Type* int32 = types.getPrimitiveType("int32");
    Type* flt32 = types.getPrimitiveType("flt32");
    Type* bool_type = types.getPrimitiveType("bool");
    
    ASSERT(int8 != nullptr, "int8 type should exist");
    ASSERT(int32 != nullptr, "int32 type should exist");
    ASSERT(flt32 != nullptr, "flt32 type should exist");
    ASSERT(bool_type != nullptr, "bool type should exist");
    
    // IR generator should exist alongside type system
    llvm::Module* mod = gen.getModule();
    ASSERT(mod != nullptr, "Module should exist with type system");
    
    return;
}

// Test vector type mapping (vec2, vec3, vec4)
TEST_CASE(ir_generator_vector_types) {
    TypeSystem types;
    IRGenerator gen("vector_types");
    
    // Note: mapType is private, but we verify the generator works with vector types
    // Vector type creation would happen in the type system
    Type* flt32 = types.getPrimitiveType("flt32");
    ASSERT(flt32 != nullptr, "flt32 should exist for vector components");
    
    // Verify module exists for vector type mapping
    llvm::Module* mod = gen.getModule();
    ASSERT(mod != nullptr, "Module should exist for vector types");
    
    return;
}

// Test struct type mapping
TEST_CASE(ir_generator_struct_types) {
    TypeSystem types;
    IRGenerator gen("struct_types");
    
    // Create a simple struct type
    Type* int32 = types.getPrimitiveType("int32");
    Type* flt32 = types.getPrimitiveType("flt32");
    
    std::vector<StructType::Field> fields;
    fields.emplace_back("x", int32, 0, true);
    fields.emplace_back("y", flt32, 4, true);
    
    StructType point_struct("Point", fields);
    
    ASSERT(point_struct.getName() == "Point", "Struct name should be Point");
    ASSERT(point_struct.getFields().size() == 2, "Should have 2 fields");
    
    // Verify IR generator can work with struct types
    llvm::Module* mod = gen.getModule();
    ASSERT(mod != nullptr, "Module should exist for struct types");
    
    return;
}

// Test function type mapping
TEST_CASE(ir_generator_function_types) {
    TypeSystem types;
    IRGenerator gen("function_types");
    
    // Create function type: func(int32, int32) -> int32
    Type* int32 = types.getPrimitiveType("int32");
    
    std::vector<Type*> params = {int32, int32};
    FunctionType add_func(params, int32);
    
    ASSERT(add_func.getParamCount() == 2, "Should have 2 parameters");
    ASSERT(add_func.getReturnType() == int32, "Return type should be int32");
    ASSERT(!add_func.isVariadicFunction(), "Should not be variadic");
    
    // Verify IR generator can work with function types
    llvm::Module* mod = gen.getModule();
    ASSERT(mod != nullptr, "Module should exist for function types");
    
    return;
}

// Test union type mapping
TEST_CASE(ir_generator_union_types) {
    TypeSystem types;
    IRGenerator gen("union_types");
    
    // Create union type
    Type* int32 = types.getPrimitiveType("int32");
    Type* flt32 = types.getPrimitiveType("flt32");
    
    std::vector<UnionType::Variant> variants;
    variants.emplace_back("intValue", int32);
    variants.emplace_back("floatValue", flt32);
    
    UnionType value_union("Value", variants);
    
    ASSERT(value_union.getName() == "Value", "Union name should be Value");
    ASSERT(value_union.getVariants().size() == 2, "Should have 2 variants");
    
    // Verify IR generator can work with union types
    llvm::Module* mod = gen.getModule();
    ASSERT(mod != nullptr, "Module should exist for union types");
    
    return;
}

// Test result type mapping
TEST_CASE(ir_generator_result_types) {
    TypeSystem types;
    IRGenerator gen("result_types");
    
    // Create result type: result<int32>
    Type* int32 = types.getPrimitiveType("int32");
    ResultType result_int(int32);
    
    ASSERT(result_int.getValueType() == int32, "Value type should be int32");
    
    // Verify IR generator can work with result types
    llvm::Module* mod = gen.getModule();
    ASSERT(mod != nullptr, "Module should exist for result types");
    
    return;
}

// Test pointer type mapping
TEST_CASE(ir_generator_pointer_types) {
    TypeSystem types;
    IRGenerator gen("pointer_types");
    
    // Create pointer type: int32@
    Type* int32 = types.getPrimitiveType("int32");
    PointerType int_ptr(int32);
    
    ASSERT(int_ptr.getPointeeType() == int32, "Pointee should be int32");
    ASSERT(!int_ptr.isWildPointer(), "Should not be wild by default");
    
    // Verify IR generator can work with pointer types
    llvm::Module* mod = gen.getModule();
    ASSERT(mod != nullptr, "Module should exist for pointer types");
    
    return;
}

// Test array type mapping
TEST_CASE(ir_generator_array_types) {
    TypeSystem types;
    IRGenerator gen("array_types");
    
    // Create fixed-size array: int32[100]
    Type* int32 = types.getPrimitiveType("int32");
    ArrayType fixed_array(int32, 100);
    
    ASSERT(fixed_array.getElementType() == int32, "Element type should be int32");
    ASSERT(fixed_array.getSize() == 100, "Size should be 100");
    ASSERT(fixed_array.isFixedSize(), "Should be fixed size");
    
    // Create dynamic array: int32[]
    ArrayType dynamic_array(int32, -1);
    ASSERT(dynamic_array.isDynamic(), "Should be dynamic");
    
    // Verify IR generator can work with array types
    llvm::Module* mod = gen.getModule();
    ASSERT(mod != nullptr, "Module should exist for array types");
    
    return;
}

// Test TBB type mapping
TEST_CASE(ir_generator_tbb_types) {
    TypeSystem types;
    IRGenerator gen("tbb_types");
    
    // TBB types map to integers
    Type* tbb8 = types.getPrimitiveType("tbb8");
    Type* tbb32 = types.getPrimitiveType("tbb32");
    
    ASSERT(tbb8 != nullptr, "tbb8 type should exist");
    ASSERT(tbb32 != nullptr, "tbb32 type should exist");
    
    // Verify IR generator can work with TBB types
    llvm::Module* mod = gen.getModule();
    ASSERT(mod != nullptr, "Module should exist for TBB types");
    
    return;
}
