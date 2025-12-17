#include "backend/ir/codegen_expr.h"
#include "frontend/ast/expr.h"
#include "frontend/sema/type.h"
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/BasicBlock.h>
#include <stdexcept>

using namespace aria;
using namespace aria::backend;
using namespace aria::sema;

ExprCodegen::ExprCodegen(llvm::LLVMContext& ctx, llvm::IRBuilder<>& bldr,
                         llvm::Module* mod, std::map<std::string, llvm::Value*>& values)
    : context(ctx), builder(bldr), module(mod), named_values(values) {}

// Helper: Get LLVM type from Aria type
llvm::Type* ExprCodegen::getLLVMType(Type* type) {
    if (!type) {
        return llvm::Type::getVoidTy(context);
    }
    
    // Get type name - for now, we only handle primitive types
    if (!type->isPrimitive()) {
        // Non-primitive types will be handled later
        return llvm::Type::getInt32Ty(context);  // Default
    }
    
    PrimitiveType* prim_type = static_cast<PrimitiveType*>(type);
    std::string type_name = prim_type->getName();
    
    // Primitive types
    if (type_name == "i8" || type_name == "tbb8") return llvm::Type::getInt8Ty(context);
    if (type_name == "i16" || type_name == "tbb16") return llvm::Type::getInt16Ty(context);
    if (type_name == "i32" || type_name == "tbb32") return llvm::Type::getInt32Ty(context);
    if (type_name == "i64" || type_name == "tbb64") return llvm::Type::getInt64Ty(context);
    if (type_name == "u8") return llvm::Type::getInt8Ty(context);
    if (type_name == "u16") return llvm::Type::getInt16Ty(context);
    if (type_name == "u32") return llvm::Type::getInt32Ty(context);
    if (type_name == "u64") return llvm::Type::getInt64Ty(context);
    if (type_name == "f32") return llvm::Type::getFloatTy(context);
    if (type_name == "f64") return llvm::Type::getDoubleTy(context);
    if (type_name == "bool") return llvm::Type::getInt1Ty(context);
    if (type_name == "void") return llvm::Type::getVoidTy(context);
    
    // Pointer types (str, any reference)
    if (type_name == "str" || type_name.find('*') != std::string::npos) {
        return llvm::PointerType::get(context, 0);  // LLVM 20.x opaque pointers
    }
    
    // Default to i32 for unknown types (for now)
    return llvm::Type::getInt32Ty(context);
}

// Helper: Get size of Aria type in bytes
size_t ExprCodegen::getTypeSize(Type* type) {
    if (!type) return 0;
    
    if (!type->isPrimitive()) {
        return 8;  // Default pointer size
    }
    
    PrimitiveType* prim_type = static_cast<PrimitiveType*>(type);
    std::string type_name = prim_type->getName();
    
    if (type_name == "i8" || type_name == "u8" || type_name == "tbb8") return 1;
    if (type_name == "i16" || type_name == "u16" || type_name == "tbb16") return 2;
    if (type_name == "i32" || type_name == "u32" || type_name == "f32" || type_name == "tbb32") return 4;
    if (type_name == "i64" || type_name == "u64" || type_name == "f64" || type_name == "tbb64") return 8;
    if (type_name == "bool") return 1;
    if (type_name == "str") return 8;  // Pointer size on 64-bit
    
    return 8;  // Default pointer size
}

/**
 * Generate code for literal expressions
 * Handles: integers, floats, strings, booleans, null
 */
llvm::Value* ExprCodegen::codegenLiteral(LiteralExpr* expr) {
    if (!expr) {
        throw std::runtime_error("Null literal expression");
    }
    
    // Integer literal
    if (std::holds_alternative<int64_t>(expr->value)) {
        int64_t val = std::get<int64_t>(expr->value);
        
        // Determine appropriate integer type based on value range
        // Default to i32 for small values, i64 for larger
        if (val >= INT32_MIN && val <= INT32_MAX) {
            return llvm::ConstantInt::get(context, llvm::APInt(32, val, true));
        } else {
            return llvm::ConstantInt::get(context, llvm::APInt(64, val, true));
        }
    }
    
    // Float literal
    if (std::holds_alternative<double>(expr->value)) {
        double val = std::get<double>(expr->value);
        return llvm::ConstantFP::get(context, llvm::APFloat(val));
    }
    
    // String literal
    if (std::holds_alternative<std::string>(expr->value)) {
        const std::string& str = std::get<std::string>(expr->value);
        
        // Create a global constant for the string
        llvm::Constant* str_constant = llvm::ConstantDataArray::getString(context, str, true);
        
        // Create a global variable to hold the string
        llvm::GlobalVariable* gv = new llvm::GlobalVariable(
            *module,
            str_constant->getType(),
            true,  // isConstant
            llvm::GlobalValue::PrivateLinkage,
            str_constant,
            ".str"
        );
        
        // Return pointer to the string (cast to i8*)
        return builder.CreatePointerCast(gv, llvm::PointerType::get(context, 0));
    }
    
    // Boolean literal
    if (std::holds_alternative<bool>(expr->value)) {
        bool val = std::get<bool>(expr->value);
        return llvm::ConstantInt::get(llvm::Type::getInt1Ty(context), val ? 1 : 0);
    }
    
    // Null literal
    if (std::holds_alternative<std::monostate>(expr->value)) {
        // Null is represented as a null pointer
        return llvm::ConstantPointerNull::get(llvm::PointerType::get(context, 0));
    }
    
    throw std::runtime_error("Unknown literal type");
}

/**
 * Generate code for identifier (variable reference)
 * Loads the value from the symbol table
 */
llvm::Value* ExprCodegen::codegenIdentifier(IdentifierExpr* expr) {
    if (!expr) {
        throw std::runtime_error("Null identifier expression");
    }
    
    // Look up the variable in the symbol table
    auto it = named_values.find(expr->name);
    if (it == named_values.end()) {
        throw std::runtime_error("Undefined variable: " + expr->name);
    }
    
    llvm::Value* var_ptr = it->second;
    
    // If it's a pointer (alloca), we need to load the value
    // For now, we assume all variables in named_values are allocas that need loading
    // The type to load is determined by what's stored in the alloca
    // For LLVM 20+ with opaque pointers, we need to specify the type explicitly
    // This will be handled properly when we implement variable declarations
    // For now, just return the pointer itself (will be fixed in Phase 4.3)
    return var_ptr;
}

/**
 * Generate code for binary operations
 * Handles: arithmetic, comparison, logical, bitwise operators
 */
llvm::Value* ExprCodegen::codegenBinary(BinaryExpr* expr) {
    if (!expr) {
        throw std::runtime_error("Null binary expression");
    }
    
    // TODO: Implement in Phase 4.2.3
    throw std::runtime_error("Binary operations not yet implemented");
}

/**
 * Generate code for unary operations
 * Handles: neg, not, address, deref
 */
llvm::Value* ExprCodegen::codegenUnary(UnaryExpr* expr) {
    if (!expr) {
        throw std::runtime_error("Null unary expression");
    }
    
    // TODO: Implement in Phase 4.2.4
    throw std::runtime_error("Unary operations not yet implemented");
}

/**
 * Generate code for function calls
 */
llvm::Value* ExprCodegen::codegenCall(CallExpr* expr) {
    if (!expr) {
        throw std::runtime_error("Null call expression");
    }
    
    // TODO: Implement in Phase 4.2.5
    throw std::runtime_error("Function calls not yet implemented");
}

/**
 * Generate code for ternary expressions (is ? :)
 */
llvm::Value* ExprCodegen::codegenTernary(TernaryExpr* expr) {
    if (!expr) {
        throw std::runtime_error("Null ternary expression");
    }
    
    // TODO: Implement in Phase 4.2.6
    throw std::runtime_error("Ternary operations not yet implemented");
}

/**
 * Generate code for array indexing
 */
llvm::Value* ExprCodegen::codegenIndex(IndexExpr* expr) {
    if (!expr) {
        throw std::runtime_error("Null index expression");
    }
    
    // TODO: Implement later with array support
    throw std::runtime_error("Array indexing not yet implemented");
}

/**
 * Generate code for member access
 */
llvm::Value* ExprCodegen::codegenMemberAccess(MemberAccessExpr* expr) {
    if (!expr) {
        throw std::runtime_error("Null member access expression");
    }
    
    // TODO: Implement later with struct support
    throw std::runtime_error("Member access not yet implemented");
}
