// Test suite for functional types (Result<T,E>, func, array)
#include "../src/frontend/sema/types.h"
#include <iostream>
#include <memory>
#include <cassert>

using namespace aria::sema;

void test_result_type_creation() {
    std::cout << "Testing Result<T,E> type creation..." << std::endl;
    
    // Create Result<int32, string>
    auto result_type = std::make_shared<Type>(TypeKind::RESULT);
    result_type->result_value_type = std::make_shared<Type>(TypeKind::INT32);
    result_type->result_error_type = std::make_shared<Type>(TypeKind::STRING);
    
    assert(result_type->kind == TypeKind::RESULT);
    assert(result_type->result_value_type->kind == TypeKind::INT32);
    assert(result_type->result_error_type->kind == TypeKind::STRING);
    
    std::cout << "✓ Result type creation passed" << std::endl;
}

void test_result_type_tostring() {
    std::cout << "Testing Result<T,E> toString()..." << std::endl;
    
    // Create Result<int32, string>
    auto result_type = std::make_shared<Type>(TypeKind::RESULT);
    result_type->result_value_type = std::make_shared<Type>(TypeKind::INT32);
    result_type->result_error_type = std::make_shared<Type>(TypeKind::STRING);
    
    std::string str = result_type->toString();
    assert(str == "Result<int32, string>");
    
    std::cout << "✓ Result toString: " << str << std::endl;
}

void test_result_type_equals() {
    std::cout << "Testing Result<T,E> equals()..." << std::endl;
    
    // Create Result<int32, string>
    auto result1 = std::make_shared<Type>(TypeKind::RESULT);
    result1->result_value_type = std::make_shared<Type>(TypeKind::INT32);
    result1->result_error_type = std::make_shared<Type>(TypeKind::STRING);
    
    // Create another Result<int32, string>
    auto result2 = std::make_shared<Type>(TypeKind::RESULT);
    result2->result_value_type = std::make_shared<Type>(TypeKind::INT32);
    result2->result_error_type = std::make_shared<Type>(TypeKind::STRING);
    
    // Create Result<int64, string> (different value type)
    auto result3 = std::make_shared<Type>(TypeKind::RESULT);
    result3->result_value_type = std::make_shared<Type>(TypeKind::INT64);
    result3->result_error_type = std::make_shared<Type>(TypeKind::STRING);
    
    assert(result1->equals(*result2));
    assert(!result1->equals(*result3));
    
    std::cout << "✓ Result equals() passed" << std::endl;
}

void test_function_type_creation() {
    std::cout << "Testing function type creation..." << std::endl;
    
    // Create func(int32, int32) -> int32
    auto func_type = std::make_shared<Type>(TypeKind::FUNCTION);
    func_type->param_types.push_back(std::make_shared<Type>(TypeKind::INT32));
    func_type->param_types.push_back(std::make_shared<Type>(TypeKind::INT32));
    func_type->return_type = std::make_shared<Type>(TypeKind::INT32);
    
    assert(func_type->kind == TypeKind::FUNCTION);
    assert(func_type->param_types.size() == 2);
    assert(func_type->return_type->kind == TypeKind::INT32);
    
    std::cout << "✓ Function type creation passed" << std::endl;
}

void test_function_type_tostring() {
    std::cout << "Testing function type toString()..." << std::endl;
    
    // Create func(int32, int32) -> int32
    auto func_type = std::make_shared<Type>(TypeKind::FUNCTION);
    func_type->param_types.push_back(std::make_shared<Type>(TypeKind::INT32));
    func_type->param_types.push_back(std::make_shared<Type>(TypeKind::INT32));
    func_type->return_type = std::make_shared<Type>(TypeKind::INT32);
    
    std::string str = func_type->toString();
    assert(str == "func(int32, int32) -> int32");
    
    std::cout << "✓ Function toString: " << str << std::endl;
}

void test_function_type_equals() {
    std::cout << "Testing function type equals()..." << std::endl;
    
    // Create func(int32, int32) -> int32
    auto func1 = std::make_shared<Type>(TypeKind::FUNCTION);
    func1->param_types.push_back(std::make_shared<Type>(TypeKind::INT32));
    func1->param_types.push_back(std::make_shared<Type>(TypeKind::INT32));
    func1->return_type = std::make_shared<Type>(TypeKind::INT32);
    
    // Create another func(int32, int32) -> int32
    auto func2 = std::make_shared<Type>(TypeKind::FUNCTION);
    func2->param_types.push_back(std::make_shared<Type>(TypeKind::INT32));
    func2->param_types.push_back(std::make_shared<Type>(TypeKind::INT32));
    func2->return_type = std::make_shared<Type>(TypeKind::INT32);
    
    // Create func(int32) -> int32 (different params)
    auto func3 = std::make_shared<Type>(TypeKind::FUNCTION);
    func3->param_types.push_back(std::make_shared<Type>(TypeKind::INT32));
    func3->return_type = std::make_shared<Type>(TypeKind::INT32);
    
    assert(func1->equals(*func2));
    assert(!func1->equals(*func3));
    
    std::cout << "✓ Function equals() passed" << std::endl;
}

void test_nested_result_types() {
    std::cout << "Testing nested Result types..." << std::endl;
    
    // Create Result<Result<int32, string>, string>
    auto inner_result = std::make_shared<Type>(TypeKind::RESULT);
    inner_result->result_value_type = std::make_shared<Type>(TypeKind::INT32);
    inner_result->result_error_type = std::make_shared<Type>(TypeKind::STRING);
    
    auto outer_result = std::make_shared<Type>(TypeKind::RESULT);
    outer_result->result_value_type = inner_result;
    outer_result->result_error_type = std::make_shared<Type>(TypeKind::STRING);
    
    std::string str = outer_result->toString();
    assert(str == "Result<Result<int32, string>, string>");
    
    std::cout << "✓ Nested Result toString: " << str << std::endl;
}

void test_function_returning_result() {
    std::cout << "Testing function returning Result..." << std::endl;
    
    // Create func(int32) -> Result<int32, string>
    auto result_type = std::make_shared<Type>(TypeKind::RESULT);
    result_type->result_value_type = std::make_shared<Type>(TypeKind::INT32);
    result_type->result_error_type = std::make_shared<Type>(TypeKind::STRING);
    
    auto func_type = std::make_shared<Type>(TypeKind::FUNCTION);
    func_type->param_types.push_back(std::make_shared<Type>(TypeKind::INT32));
    func_type->return_type = result_type;
    
    std::string str = func_type->toString();
    assert(str == "func(int32) -> Result<int32, string>");
    
    std::cout << "✓ Function with Result return: " << str << std::endl;
}

int main() {
    std::cout << "=== Functional Types Test Suite ===" << std::endl << std::endl;
    
    try {
        // Result<T,E> tests
        test_result_type_creation();
        test_result_type_tostring();
        test_result_type_equals();
        
        // Function type tests
        test_function_type_creation();
        test_function_type_tostring();
        test_function_type_equals();
        
        // Complex type tests
        test_nested_result_types();
        test_function_returning_result();
        
        std::cout << std::endl << "=== All tests passed! ===" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed: " << e.what() << std::endl;
        return 1;
    }
}
