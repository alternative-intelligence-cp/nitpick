#include "backend/ir/codegen_expr.h"
#include "backend/ir/codegen_stmt.h"
#include "frontend/ast/expr.h"
#include "frontend/ast/stmt.h"
#include "frontend/ast/ast_node.h"
#include "frontend/sema/type.h"
#include "frontend/token.h"
#include "semantic/literal_converter.h"
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Intrinsics.h>  // Phase 4.5.3: Coroutine intrinsics for await
#include <llvm/Support/raw_ostream.h>
#include <stdexcept>
#include <iostream>

using namespace aria;
using namespace aria::frontend;
using namespace aria::backend;
using namespace aria::sema;

ExprCodegen::ExprCodegen(llvm::LLVMContext& ctx, llvm::IRBuilder<>& bldr,
                         llvm::Module* mod, std::map<std::string, llvm::Value*>& values)
    : context(ctx), builder(bldr), module(mod), named_values(values), 
      stmt_codegen(nullptr), tbb_codegen(ctx, bldr) {}

void ExprCodegen::setStmtCodegen(StmtCodegen* stmt_gen) {
    stmt_codegen = stmt_gen;
}

// Helper: Get LLVM type from Aria type
llvm::Type* ExprCodegen::getLLVMType(Type* type) {
    if (!type) {
        return llvm::Type::getVoidTy(context);
    }
    
    // Handle optional types: T? -> { i1, T }
    if (type->getKind() == TypeKind::OPTIONAL) {
        OptionalType* optType = static_cast<OptionalType*>(type);
        Type* wrappedType = optType->getWrappedType();
        llvm::Type* wrappedLLVMType = getLLVMType(wrappedType);
        return getOptionalType(wrappedLLVMType);
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

// Helper: Get LLVM type from Aria type name string
// Complete mapping for all Aria primitive types from specs
llvm::Type* ExprCodegen::getLLVMTypeFromString(const std::string& typeName) {
    // Integer types
    if (typeName == "int1") return llvm::Type::getInt1Ty(context);
    if (typeName == "int2") return llvm::Type::getInt8Ty(context);   // Smallest LLVM int
    if (typeName == "int4") return llvm::Type::getInt8Ty(context);   // Smallest LLVM int
    if (typeName == "int8") return llvm::Type::getInt8Ty(context);
    if (typeName == "int16") return llvm::Type::getInt16Ty(context);
    if (typeName == "int32") return llvm::Type::getInt32Ty(context);
    if (typeName == "int64") return llvm::Type::getInt64Ty(context);
    if (typeName == "int128") return llvm::Type::getInt128Ty(context);
    if (typeName == "int256") return llvm::IntegerType::get(context, 256);
    if (typeName == "int512") return llvm::IntegerType::get(context, 512);
    
    // Unsigned integer types
    if (typeName == "uint8") return llvm::Type::getInt8Ty(context);
    if (typeName == "uint16") return llvm::Type::getInt16Ty(context);
    if (typeName == "uint32") return llvm::Type::getInt32Ty(context);
    if (typeName == "uint64") return llvm::Type::getInt64Ty(context);
    if (typeName == "uint128") return llvm::Type::getInt128Ty(context);
    if (typeName == "uint256") return llvm::IntegerType::get(context, 256);
    if (typeName == "uint512") return llvm::IntegerType::get(context, 512);
    
    // TBB (Twisted Balanced Binary) - symmetric signed with error sentinel
    if (typeName == "tbb8") return llvm::Type::getInt8Ty(context);
    if (typeName == "tbb16") return llvm::Type::getInt16Ty(context);
    if (typeName == "tbb32") return llvm::Type::getInt32Ty(context);
    if (typeName == "tbb64") return llvm::Type::getInt64Ty(context);
    
    // Floating point types
    if (typeName == "flt32") return llvm::Type::getFloatTy(context);
    if (typeName == "flt64") return llvm::Type::getDoubleTy(context);
    if (typeName == "flt128") return llvm::Type::getFP128Ty(context);
    if (typeName == "flt256") return llvm::Type::getFP128Ty(context);  // Use FP128 for now
    if (typeName == "flt512") return llvm::Type::getFP128Ty(context);  // Use FP128 for now
    
    // Boolean
    if (typeName == "bool") return llvm::Type::getInt1Ty(context);
    
    // Void (FFI/C interop) / NIL (native Aria - function with no return value)
    if (typeName == "void") return llvm::Type::getVoidTy(context);
    if (typeName == "NIL") return llvm::Type::getVoidTy(context);
    
    // Ternary/Nonary (stored as uint16)
    if (typeName == "trit") return llvm::Type::getInt8Ty(context);   // -1,0,1
    if (typeName == "tryte") return llvm::Type::getInt16Ty(context); // 10 trits in uint16
    if (typeName == "nit") return llvm::Type::getInt8Ty(context);    // -4...4
    if (typeName == "nyte") return llvm::Type::getInt16Ty(context);  // 5 nits in uint16
    
    // Compound/reference types (pointers)
    if (typeName == "string") return llvm::PointerType::get(context, 0);
    if (typeName == "dyn") return llvm::PointerType::get(context, 0);
    if (typeName == "obj") return llvm::PointerType::get(context, 0);
    if (typeName == "struct") return llvm::PointerType::get(context, 0);
    if (typeName == "result") return llvm::PointerType::get(context, 0);
    if (typeName == "func") return llvm::PointerType::get(context, 0);
    if (typeName == "array") return llvm::PointerType::get(context, 0);
    if (typeName == "buffer") return llvm::PointerType::get(context, 0);
    if (typeName == "stream") return llvm::PointerType::get(context, 0);
    if (typeName == "process") return llvm::PointerType::get(context, 0);
    if (typeName == "pipe") return llvm::PointerType::get(context, 0);
    
    // Vector types (future expansion)
    if (typeName == "vec2") return llvm::PointerType::get(context, 0);
    if (typeName == "vec3") return llvm::PointerType::get(context, 0);
    if (typeName == "vec9") return llvm::PointerType::get(context, 0);
    if (typeName == "tensor") return llvm::PointerType::get(context, 0);
    if (typeName == "matrix") return llvm::PointerType::get(context, 0);
    
    // Unknown type - throw error instead of defaulting
    throw std::runtime_error("Unknown Aria type: " + typeName);
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

// Helper: Check if type is TBB type
bool ExprCodegen::isTBBType(Type* type) {
    if (!type || !type->isPrimitive()) {
        return false;
    }
    
    PrimitiveType* prim_type = static_cast<PrimitiveType*>(type);
    const std::string& type_name = prim_type->getName();
    
    return type_name == "tbb8" || type_name == "tbb16" || 
           type_name == "tbb32" || type_name == "tbb64";
}

/**
 * Generate code for literal expressions
 * Handles: integers, floats, strings, booleans, null
 * Phase 3.2.5: High-precision literals use String-as-Truth architecture
 */
llvm::Value* ExprCodegen::codegenLiteral(LiteralExpr* expr) {
    if (!expr) {
        throw std::runtime_error("Null literal expression");
    }
    
    std::cerr << "[DEBUG] codegenLiteral called, hasRawValue=" << expr->hasRawValue() << std::endl;
    
    // Phase 3.2.5: Check for high-precision literal with raw string value
    // For now, we'll infer the type from the variant - in future we could pass target type as parameter
    if (expr->hasRawValue()) {
        const std::string& raw = expr->getRawValue();
        
        // Detect if this is a float or int from the raw string
        bool is_float = (raw.find('.') != std::string::npos || 
                        raw.find('e') != std::string::npos || 
                        raw.find('E') != std::string::npos ||
                        raw == "inf" || raw == "-inf" || raw == "nan");
        
        if (is_float) {
            // High-precision float literal - for now use FLT128 if many digits, else FLT64
            aria::semantic::FloatPrecision precision;
            if (raw.length() > 17) {  // More digits than float64 can represent
                precision = aria::semantic::FloatPrecision::FLT128;
            } else {
                precision = aria::semantic::FloatPrecision::FLT64;
            }
            
            // Convert using LiteralConverter
            auto apfloat_opt = aria::semantic::LiteralConverter::convertFloatLiteral(raw, precision);
            if (apfloat_opt) {
                llvm::Value* result = llvm::ConstantFP::get(context, *apfloat_opt);
                std::cerr << "[DEBUG] codegenLiteral: high-precision float '" << raw << "' -> type: ";
                result->getType()->print(llvm::errs());
                std::cerr << std::endl;
                return result;
            }
            // Fall through to variant handling on conversion failure
        } else {
            // High-precision integer literal - use INT128 if large, else INT64
            unsigned bit_width = 64;
            bool is_signed = true;
            
            // Check if value needs more than 64 bits
            // For now, simple heuristic: if more than 19 digits, use 128 bits
            if (raw.length() > 19) {
                bit_width = 128;
            }
            
            // Convert using LiteralConverter
            auto apint_opt = aria::semantic::LiteralConverter::convertIntLiteral(raw, bit_width, is_signed);
            if (apint_opt) {
                llvm::Value* result = llvm::ConstantInt::get(context, *apint_opt);
                std::cerr << "[DEBUG] codegenLiteral: high-precision int '" << raw << "' -> type: ";
                result->getType()->print(llvm::errs());
                std::cerr << std::endl;
                return result;
            }
            // Fall through to variant handling on conversion failure
        }
    }
    
    // Standard precision: Use existing variant-based logic
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
        llvm::Value* result = llvm::ConstantFP::get(context, llvm::APFloat(val));
        std::cerr << "[DEBUG] codegenLiteral: float " << val << " -> type: ";
        result->getType()->print(llvm::errs());
        std::cerr << std::endl;
        return result;
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
    
    // Check if this is an alloca (stack variable) that needs loading
    // In LLVM 20+ with opaque pointers, we use the alloca's allocated type
    if (llvm::isa<llvm::AllocaInst>(var_ptr)) {
        llvm::AllocaInst* alloca = llvm::cast<llvm::AllocaInst>(var_ptr);
        llvm::Type* allocated_type = alloca->getAllocatedType();
        
        // Create a load instruction
        llvm::Value* loaded = builder.CreateLoad(allocated_type, var_ptr, expr->name);
        std::cerr << "[DEBUG] codegenIdentifier: " << expr->name << " -> type: ";
        loaded->getType()->print(llvm::errs());
        std::cerr << std::endl;
        return loaded;
    }
    
    // If it's not an alloca, return the value directly
    // (could be a function parameter or constant)
    return var_ptr;
}

/**
 * Generate code for template literals with interpolation
 * Handles: `text &{expr} more text`
 * 
 * Strategy:
 * 1. Convert each string part to LLVM string constant
 * 2. Evaluate each interpolated expression
 * 3. Convert non-string expressions to strings (using sprintf for numbers)
 * 4. Concatenate all parts using aria_string_concat runtime function
 */
llvm::Value* ExprCodegen::codegenTemplateLiteral(TemplateLiteralExpr* expr) {
    if (!expr) {
        throw std::runtime_error("Null template literal expression");
    }
    
    // Helper function: Create an AriaString struct from a string constant
    auto createAriaString = [this](const std::string& str) -> llvm::Value* {
        // Create string constant
        llvm::Constant* strConstant = llvm::ConstantDataArray::getString(context, str, true);
        llvm::GlobalVariable* gv = new llvm::GlobalVariable(
            *module,
            strConstant->getType(),
            true,
            llvm::GlobalValue::PrivateLinkage,
            strConstant,
            ".str"
        );
        llvm::Value* strPtr = builder.CreatePointerCast(gv, llvm::PointerType::get(context, 0));
        
        // Create AriaString struct { const char* data, int64_t length }
        llvm::StructType* ariaStringType = llvm::StructType::get(
            context,
            { llvm::PointerType::get(context, 0), llvm::Type::getInt64Ty(context) }
        );
        
        // Allocate on stack
        llvm::Value* ariaStr = builder.CreateAlloca(ariaStringType, nullptr, "aria_str");
        
        // Set data field
        llvm::Value* dataPtr = builder.CreateStructGEP(ariaStringType, ariaStr, 0, "data_ptr");
        builder.CreateStore(strPtr, dataPtr);
        
        // Set length field  
        llvm::Value* lengthPtr = builder.CreateStructGEP(ariaStringType, ariaStr, 1, "length_ptr");
        builder.CreateStore(llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), str.length()), lengthPtr);
        
        // Load and return the struct value
        return builder.CreateLoad(ariaStringType, ariaStr, "aria_str_val");
    };
    
    // Helper function: Convert int64 to string using sprintf
    auto int64ToString = [this](llvm::Value* intVal) -> llvm::Value* {
        // Allocate buffer for sprintf (32 bytes is enough for int64)
        llvm::ArrayType* bufferType = llvm::ArrayType::get(llvm::Type::getInt8Ty(context), 32);
        llvm::Value* buffer = builder.CreateAlloca(bufferType, nullptr, "int_str_buffer");
        llvm::Value* bufferPtr = builder.CreatePointerCast(buffer, llvm::PointerType::get(context, 0));
        
        // Create format string "%lld"
        llvm::Constant* formatStr = llvm::ConstantDataArray::getString(context, "%lld", true);
        llvm::GlobalVariable* formatGV = new llvm::GlobalVariable(
            *module,
            formatStr->getType(),
            true,
            llvm::GlobalValue::PrivateLinkage,
            formatStr,
            ".fmt_lld"
        );
        llvm::Value* formatPtr = builder.CreatePointerCast(formatGV, llvm::PointerType::get(context, 0));
        
        // Declare sprintf if not already declared
        llvm::Function* sprintfFn = module->getFunction("sprintf");
        if (!sprintfFn) {
            llvm::FunctionType* sprintfType = llvm::FunctionType::get(
                llvm::Type::getInt32Ty(context),
                { llvm::PointerType::get(context, 0), llvm::PointerType::get(context, 0) },
                true  // varargs
            );
            sprintfFn = llvm::Function::Create(
                sprintfType,
                llvm::Function::ExternalLinkage,
                "sprintf",
                module
            );
        }
        
        // Call sprintf(buffer, "%lld", intVal)
        builder.CreateCall(sprintfFn, { bufferPtr, formatPtr, intVal });
        
        // Declare strlen if not already declared
        llvm::Function* strlenFn = module->getFunction("strlen");
        if (!strlenFn) {
            llvm::FunctionType* strlenType = llvm::FunctionType::get(
                llvm::Type::getInt64Ty(context),
                { llvm::PointerType::get(context, 0) },
                false
            );
            strlenFn = llvm::Function::Create(
                strlenType,
                llvm::Function::ExternalLinkage,
                "strlen",
                module
            );
        }
        
        // Get length with strlen(buffer)
        llvm::Value* length = builder.CreateCall(strlenFn, { bufferPtr });
        
        // Create AriaString struct
        llvm::StructType* ariaStringType = llvm::StructType::get(
            context,
            { llvm::PointerType::get(context, 0), llvm::Type::getInt64Ty(context) }
        );
        llvm::Value* ariaStr = builder.CreateAlloca(ariaStringType, nullptr, "int_aria_str");
        
        // Set fields
        llvm::Value* dataPtr = builder.CreateStructGEP(ariaStringType, ariaStr, 0);
        builder.CreateStore(bufferPtr, dataPtr);
        llvm::Value* lengthPtr = builder.CreateStructGEP(ariaStringType, ariaStr, 1);
        builder.CreateStore(length, lengthPtr);
        
        // Load and return
        return builder.CreateLoad(ariaStringType, ariaStr, "int_str_val");
    };
    
    // Helper function: Convert float to string using sprintf
    auto floatToString = [this](llvm::Value* floatVal) -> llvm::Value* {
        // Allocate buffer for sprintf (64 bytes for floating point)
        llvm::ArrayType* bufferType = llvm::ArrayType::get(llvm::Type::getInt8Ty(context), 64);
        llvm::Value* buffer = builder.CreateAlloca(bufferType, nullptr, "float_str_buffer");
        llvm::Value* bufferPtr = builder.CreatePointerCast(buffer, llvm::PointerType::get(context, 0));
        
        // Create format string "%g" (shortest representation)
        llvm::Constant* formatStr = llvm::ConstantDataArray::getString(context, "%g", true);
        llvm::GlobalVariable* formatGV = new llvm::GlobalVariable(
            *module,
            formatStr->getType(),
            true,
            llvm::GlobalValue::PrivateLinkage,
            formatStr,
            ".fmt_g"
        );
        llvm::Value* formatPtr = builder.CreatePointerCast(formatGV, llvm::PointerType::get(context, 0));
        
        // Declare sprintf if not already declared
        llvm::Function* sprintfFn = module->getFunction("sprintf");
        if (!sprintfFn) {
            llvm::FunctionType* sprintfType = llvm::FunctionType::get(
                llvm::Type::getInt32Ty(context),
                { llvm::PointerType::get(context, 0), llvm::PointerType::get(context, 0) },
                true  // varargs
            );
            sprintfFn = llvm::Function::Create(
                sprintfType,
                llvm::Function::ExternalLinkage,
                "sprintf",
                module
            );
        }
        
        // Convert float to double if needed (sprintf expects double for %g)
        llvm::Value* doubleVal = floatVal;
        if (floatVal->getType()->isFloatTy()) {
            doubleVal = builder.CreateFPExt(floatVal, llvm::Type::getDoubleTy(context), "to_double");
        }
        
        // Call sprintf(buffer, "%g", doubleVal)
        builder.CreateCall(sprintfFn, { bufferPtr, formatPtr, doubleVal });
        
        // Declare strlen if not already declared
        llvm::Function* strlenFn = module->getFunction("strlen");
        if (!strlenFn) {
            llvm::FunctionType* strlenType = llvm::FunctionType::get(
                llvm::Type::getInt64Ty(context),
                { llvm::PointerType::get(context, 0) },
                false
            );
            strlenFn = llvm::Function::Create(
                strlenType,
                llvm::Function::ExternalLinkage,
                "strlen",
                module
            );
        }
        
        // Get length with strlen(buffer)
        llvm::Value* length = builder.CreateCall(strlenFn, { bufferPtr });
        
        // Create AriaString struct
        llvm::StructType* ariaStringType = llvm::StructType::get(
            context,
            { llvm::PointerType::get(context, 0), llvm::Type::getInt64Ty(context) }
        );
        llvm::Value* ariaStr = builder.CreateAlloca(ariaStringType, nullptr, "float_aria_str");
        
        // Set fields
        llvm::Value* dataPtr = builder.CreateStructGEP(ariaStringType, ariaStr, 0);
        builder.CreateStore(bufferPtr, dataPtr);
        llvm::Value* lengthPtr = builder.CreateStructGEP(ariaStringType, ariaStr, 1);
        builder.CreateStore(length, lengthPtr);
        
        // Load and return
        return builder.CreateLoad(ariaStringType, ariaStr, "float_str_val");
    };
    
    // Helper function: Convert bool to string ("true" or "false")
    auto boolToString = [this, &createAriaString](llvm::Value* boolVal) -> llvm::Value* {
        // Create basic blocks for conditional
        llvm::Function* func = builder.GetInsertBlock()->getParent();
        llvm::BasicBlock* trueBB = llvm::BasicBlock::Create(context, "bool_true", func);
        llvm::BasicBlock* falseBB = llvm::BasicBlock::Create(context, "bool_false", func);
        llvm::BasicBlock* mergeBB = llvm::BasicBlock::Create(context, "bool_merge", func);
        
        // Convert to i1 if needed
        if (!boolVal->getType()->isIntegerTy(1)) {
            boolVal = builder.CreateICmpNE(boolVal, llvm::ConstantInt::get(boolVal->getType(), 0), "tobool");
        }
        
        // Branch based on boolean value
        builder.CreateCondBr(boolVal, trueBB, falseBB);
        
        // True branch: create "true" string
        builder.SetInsertPoint(trueBB);
        llvm::Value* trueStr = createAriaString("true");
        builder.CreateBr(mergeBB);
        
        // False branch: create "false" string
        builder.SetInsertPoint(falseBB);
        llvm::Value* falseStr = createAriaString("false");
        builder.CreateBr(mergeBB);
        
        // Merge with PHI node
        builder.SetInsertPoint(mergeBB);
        llvm::StructType* ariaStringType = llvm::StructType::get(
            context,
            { llvm::PointerType::get(context, 0), llvm::Type::getInt64Ty(context) }
        );
        llvm::PHINode* phi = builder.CreatePHI(ariaStringType, 2, "bool_str");
        phi->addIncoming(trueStr, trueBB);
        phi->addIncoming(falseStr, falseBB);
        
        return phi;
    };
    
    // Helper function: Convert string pointer to AriaString struct
    auto ptrToAriaString = [this](llvm::Value* strPtr) -> llvm::Value* {
        // Declare strlen if not already declared
        llvm::Function* strlenFn = module->getFunction("strlen");
        if (!strlenFn) {
            llvm::FunctionType* strlenType = llvm::FunctionType::get(
                llvm::Type::getInt64Ty(context),
                { llvm::PointerType::get(context, 0) },
                false
            );
            strlenFn = llvm::Function::Create(
                strlenType,
                llvm::Function::ExternalLinkage,
                "strlen",
                module
            );
        }
        
        // Get length with strlen(strPtr)
        llvm::Value* length = builder.CreateCall(strlenFn, { strPtr });
        
        // Create AriaString struct
        llvm::StructType* ariaStringType = llvm::StructType::get(
            context,
            { llvm::PointerType::get(context, 0), llvm::Type::getInt64Ty(context) }
        );
        llvm::Value* ariaStr = builder.CreateAlloca(ariaStringType, nullptr, "ptr_aria_str");
        
        // Set fields
        llvm::Value* dataPtr = builder.CreateStructGEP(ariaStringType, ariaStr, 0);
        builder.CreateStore(strPtr, dataPtr);
        llvm::Value* lengthPtr = builder.CreateStructGEP(ariaStringType, ariaStr, 1);
        builder.CreateStore(length, lengthPtr);
        
        // Load and return
        return builder.CreateLoad(ariaStringType, ariaStr, "ptr_str_val");
    };
    
    // Start with first part (empty string if no parts)
    llvm::Value* result;
    if (expr->parts.empty()) {
        result = createAriaString("");
    } else {
        result = createAriaString(expr->parts[0]);
    }
    
    // Declare aria_string_concat runtime function
    llvm::StructType* ariaStringType = llvm::StructType::get(
        context,
        { llvm::PointerType::get(context, 0), llvm::Type::getInt64Ty(context) }
    );
    
    // aria_string_concat returns AriaResultPtr (void*)
    llvm::FunctionType* concatType = llvm::FunctionType::get(
        llvm::PointerType::get(context, 0),  // Returns AriaResultPtr
        { ariaStringType, ariaStringType },   // Takes two AriaString values
        false
    );
    
    llvm::Function* concatFn = module->getFunction("aria_string_concat");
    if (!concatFn) {
        concatFn = llvm::Function::Create(
            concatType,
            llvm::Function::ExternalLinkage,
            "aria_string_concat",
            module
        );
    }
    
    // Iterate through interpolations and remaining parts
    for (size_t i = 0; i < expr->interpolations.size(); i++) {
        // Evaluate interpolated expression
        llvm::Value* interpVal = codegenExpressionNode(expr->interpolations[i].get(), this);
        
        // Convert to AriaString based on type
        llvm::Value* interpStr;
        llvm::Type* valType = interpVal->getType();
        
        if (valType->isIntegerTy(1)) {
            // Boolean type
            interpStr = boolToString(interpVal);
        } else if (valType->isIntegerTy()) {
            // Integer type (int8, int16, int32, int64, etc.)
            interpStr = int64ToString(interpVal);
        } else if (valType->isFloatingPointTy()) {
            // Float or double type
            interpStr = floatToString(interpVal);
        } else if (valType->isPointerTy()) {
            // String pointer - convert to AriaString
            interpStr = ptrToAriaString(interpVal);
        } else {
            throw std::runtime_error("Unsupported type in template literal interpolation");
        }
        
        // Concatenate: result = concat(result, interpStr)
        llvm::Value* concatResultPtr = builder.CreateCall(concatFn, { result, interpStr });
        
        // Extract AriaString* from AriaResultPtr
        // AriaResultPtr is a void* pointing to AriaResult struct: { void* val, AriaError* err }
        // We need to:
        // 1. Cast to AriaResult*
        // 2. Check if err is NULL
        // 3. Extract val and cast to AriaString*
        
        // For now, simplified version - assume success and load directly
        // TODO: Full error handling would check err field
        llvm::StructType* ariaResultType = llvm::StructType::get(
            context,
            { llvm::PointerType::get(context, 0), llvm::PointerType::get(context, 0) }
        );
        
        // Cast AriaResultPtr to AriaResult*
        llvm::Value* resultStructPtr = concatResultPtr;
        
        // Extract val field (first field of AriaResult)
        llvm::Value* valFieldPtr = builder.CreateStructGEP(ariaResultType, resultStructPtr, 0, "result_val_ptr");
        llvm::Value* ariaStringPtr = builder.CreateLoad(llvm::PointerType::get(context, 0), valFieldPtr, "aria_str_ptr");
        
        // Load the AriaString struct
        result = builder.CreateLoad(ariaStringType, ariaStringPtr, "concat_str");
        
        // If there's another part after this interpolation, concatenate it
        if (i + 1 < expr->parts.size()) {
            llvm::Value* partStr = createAriaString(expr->parts[i + 1]);
            llvm::Value* partConcatResultPtr = builder.CreateCall(concatFn, { result, partStr });
            
            // Extract result again
            llvm::Value* partValFieldPtr = builder.CreateStructGEP(ariaResultType, partConcatResultPtr, 0, "result_val_ptr");
            llvm::Value* partAriaStringPtr = builder.CreateLoad(llvm::PointerType::get(context, 0), partValFieldPtr, "aria_str_ptr");
            result = builder.CreateLoad(ariaStringType, partAriaStringPtr, "concat_str");
        }
    }
    
    // For now, return the result struct directly
    // In the future, we might want to extract just the char* pointer
    // Extract data pointer from AriaString struct
    llvm::Value* resultAlloca = builder.CreateAlloca(ariaStringType, nullptr, "result_tmp");
    builder.CreateStore(result, resultAlloca);
    llvm::Value* dataPtr = builder.CreateStructGEP(ariaStringType, resultAlloca, 0);
    return builder.CreateLoad(llvm::PointerType::get(context, 0), dataPtr, "result_str_ptr");
}

/**
 * Helper: Recursively generate code for any expression node
 * This is a simplified dispatcher for testing - full integration in Phase 4.3+
 */
llvm::Value* ExprCodegen::codegenExpressionNode(ASTNode* node, ExprCodegen* codegen) {
    if (!node) {
        throw std::runtime_error("Null expression node");
    }
    
    // Dispatch based on node type
    switch (node->type) {
        case ASTNode::NodeType::LITERAL:
            return codegen->codegenLiteral(static_cast<LiteralExpr*>(node));
        case ASTNode::NodeType::IDENTIFIER:
            return codegen->codegenIdentifier(static_cast<IdentifierExpr*>(node));
        case ASTNode::NodeType::BINARY_OP:
            return codegen->codegenBinary(static_cast<BinaryExpr*>(node));
        case ASTNode::NodeType::UNARY_OP:
            return codegen->codegenUnary(static_cast<UnaryExpr*>(node));
        case ASTNode::NodeType::CALL:
            return codegen->codegenCall(static_cast<CallExpr*>(node));
        case ASTNode::NodeType::VECTOR_CONSTRUCTOR:
            return codegen->codegenVectorConstructor(static_cast<VectorConstructorExpr*>(node));
        case ASTNode::NodeType::TERNARY:
            return codegen->codegenTernary(static_cast<TernaryExpr*>(node));
        case ASTNode::NodeType::TEMPLATE_LITERAL:
            return codegen->codegenTemplateLiteral(static_cast<TemplateLiteralExpr*>(node));
        case ASTNode::NodeType::LAMBDA:
            return codegen->codegenLambda(static_cast<LambdaExpr*>(node));
        case ASTNode::NodeType::AWAIT:
            return codegen->codegenAwait(node);
        default:
            throw std::runtime_error("Unsupported expression node type in operation");
    }
}

/**
 * Generate code for binary operations
 * Handles: arithmetic, comparison, logical, bitwise operators
 */
llvm::Value* ExprCodegen::codegenBinary(BinaryExpr* expr) {
    if (!expr) {
        throw std::runtime_error("Null binary expression");
    }
    
    // Generate code for left and right operands
    llvm::Value* left = codegenExpressionNode(expr->left.get(), this);
    llvm::Value* right = codegenExpressionNode(expr->right.get(), this);
    
    if (!left || !right) {
        throw std::runtime_error("Failed to generate code for binary operation operands");
    }
    
    // Get the operator type
    TokenType op = expr->op.type;
    
    // Check if operands are vectors
    bool leftIsVector = left->getType()->isVectorTy();
    bool rightIsVector = right->getType()->isVectorTy();
    bool isVector = leftIsVector || rightIsVector;
    
    // For vector-scalar operations, broadcast scalar to vector
    if (leftIsVector && !rightIsVector) {
        // Right is scalar, left is vector - broadcast right
        llvm::VectorType* vecType = llvm::cast<llvm::VectorType>(left->getType());
        llvm::Value* vec = llvm::UndefValue::get(vecType);
        unsigned numElements = vecType->getElementCount().getKnownMinValue();
        for (unsigned i = 0; i < numElements; ++i) {
            vec = builder.CreateInsertElement(vec, right, i);
        }
        right = vec;
        rightIsVector = true;
    } else if (!leftIsVector && rightIsVector) {
        // Left is scalar, right is vector - broadcast left
        llvm::VectorType* vecType = llvm::cast<llvm::VectorType>(right->getType());
        llvm::Value* vec = llvm::UndefValue::get(vecType);
        unsigned numElements = vecType->getElementCount().getKnownMinValue();
        for (unsigned i = 0; i < numElements; ++i) {
            vec = builder.CreateInsertElement(vec, left, i);
        }
        left = vec;
        leftIsVector = true;
    }
    
    // Check if operands are floating point (check AFTER broadcast)
    bool isFloat = false;
    if (leftIsVector) {
        llvm::VectorType* vecType = llvm::cast<llvm::VectorType>(left->getType());
        isFloat = vecType->getElementType()->isFloatingPointTy();
    } else if (rightIsVector) {
        llvm::VectorType* vecType = llvm::cast<llvm::VectorType>(right->getType());
        isFloat = vecType->getElementType()->isFloatingPointTy();
    } else {
        // Both are scalars - check either one
        isFloat = left->getType()->isFloatingPointTy() || right->getType()->isFloatingPointTy();
    }
    
    // DEBUG: Print types
    std::cerr << "Binary op: left type = ";
    left->getType()->print(llvm::errs());
    std::cerr << ", right type = ";
    right->getType()->print(llvm::errs());
    std::cerr << ", isFloat = " << isFloat << std::endl;
    
    // ARITHMETIC OPERATORS
    if (op == TokenType::TOKEN_PLUS) {
        if (isFloat) {
            return builder.CreateFAdd(left, right, "addtmp");
        } else {
            return builder.CreateAdd(left, right, "addtmp");
        }
    }
    
    if (op == TokenType::TOKEN_MINUS) {
        if (isFloat) {
            return builder.CreateFSub(left, right, "subtmp");
        } else {
            return builder.CreateSub(left, right, "subtmp");
        }
    }
    
    if (op == TokenType::TOKEN_STAR) {
        if (isFloat) {
            return builder.CreateFMul(left, right, "multmp");
        } else {
            return builder.CreateMul(left, right, "multmp");
        }
    }
    
    if (op == TokenType::TOKEN_SLASH) {
        if (isFloat) {
            return builder.CreateFDiv(left, right, "divtmp");
        } else {
            // For integers, use signed division
            return builder.CreateSDiv(left, right, "divtmp");
        }
    }
    
    if (op == TokenType::TOKEN_PERCENT) {
        if (isFloat) {
            return builder.CreateFRem(left, right, "modtmp");
        } else {
            return builder.CreateSRem(left, right, "modtmp");
        }
    }
    
    // COMPARISON OPERATORS
    if (op == TokenType::TOKEN_EQUAL_EQUAL) {
        if (isFloat) {
            return builder.CreateFCmpOEQ(left, right, "eqtmp");
        } else {
            return builder.CreateICmpEQ(left, right, "eqtmp");
        }
    }
    
    if (op == TokenType::TOKEN_BANG_EQUAL) {
        if (isFloat) {
            return builder.CreateFCmpONE(left, right, "netmp");
        } else {
            return builder.CreateICmpNE(left, right, "netmp");
        }
    }
    
    if (op == TokenType::TOKEN_LESS) {
        if (isFloat) {
            return builder.CreateFCmpOLT(left, right, "lttmp");
        } else {
            return builder.CreateICmpSLT(left, right, "lttmp");
        }
    }
    
    if (op == TokenType::TOKEN_LESS_EQUAL) {
        if (isFloat) {
            return builder.CreateFCmpOLE(left, right, "letmp");
        } else {
            return builder.CreateICmpSLE(left, right, "letmp");
        }
    }
    
    if (op == TokenType::TOKEN_GREATER) {
        if (isFloat) {
            return builder.CreateFCmpOGT(left, right, "gttmp");
        } else {
            return builder.CreateICmpSGT(left, right, "gttmp");
        }
    }
    
    if (op == TokenType::TOKEN_GREATER_EQUAL) {
        if (isFloat) {
            return builder.CreateFCmpOGE(left, right, "getmp");
        } else {
            return builder.CreateICmpSGE(left, right, "getmp");
        }
    }
    
    // SPACESHIP OPERATOR (<=>)
    // Three-way comparison: returns -1 if left < right, 0 if equal, 1 if left > right
    if (op == TokenType::TOKEN_SPACESHIP) {
        llvm::Value* ltCmp, *gtCmp;
        
        if (isFloat) {
            ltCmp = builder.CreateFCmpOLT(left, right, "spaceship.lt");
            gtCmp = builder.CreateFCmpOGT(left, right, "spaceship.gt");
        } else {
            ltCmp = builder.CreateICmpSLT(left, right, "spaceship.lt");
            gtCmp = builder.CreateICmpSGT(left, right, "spaceship.gt");
        }
        
        // Use select instructions:
        // result = select(lt, -1, select(gt, 1, 0))
        llvm::Value* negOne = llvm::ConstantInt::getSigned(llvm::Type::getInt64Ty(context), -1);
        llvm::Value* zero = llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), 0);
        llvm::Value* one = llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), 1);
        
        llvm::Value* gtResult = builder.CreateSelect(gtCmp, one, zero, "spaceship.gt_sel");
        llvm::Value* result = builder.CreateSelect(ltCmp, negOne, gtResult, "spaceship.result");
        
        return result;
    }
    
    // LOGICAL OPERATORS (short-circuit evaluation with phi nodes)
    if (op == TokenType::TOKEN_AND_AND) {
        // Convert to i1 if needed
        if (!left->getType()->isIntegerTy(1)) {
            left = builder.CreateICmpNE(left, llvm::ConstantInt::get(left->getType(), 0), "tobool");
        }
        
        // Create blocks for short-circuit evaluation
        llvm::Function* func = builder.GetInsertBlock()->getParent();
        llvm::BasicBlock* evalRightBB = llvm::BasicBlock::Create(context, "and_eval_right", func);
        llvm::BasicBlock* mergeBB = llvm::BasicBlock::Create(context, "and_merge", func);
        
        // If left is false, skip right and return false
        builder.CreateCondBr(left, evalRightBB, mergeBB);
        
        // Evaluate right
        builder.SetInsertPoint(evalRightBB);
        if (!right->getType()->isIntegerTy(1)) {
            right = builder.CreateICmpNE(right, llvm::ConstantInt::get(right->getType(), 0), "tobool");
        }
        builder.CreateBr(mergeBB);
        
        // Merge
        builder.SetInsertPoint(mergeBB);
        llvm::PHINode* phi = builder.CreatePHI(llvm::Type::getInt1Ty(context), 2, "and_result");
        phi->addIncoming(llvm::ConstantInt::get(llvm::Type::getInt1Ty(context), 0), evalRightBB->getSinglePredecessor());
        phi->addIncoming(right, evalRightBB);
        
        return phi;
    }
    
    if (op == TokenType::TOKEN_OR_OR) {
        // Convert to i1 if needed
        if (!left->getType()->isIntegerTy(1)) {
            left = builder.CreateICmpNE(left, llvm::ConstantInt::get(left->getType(), 0), "tobool");
        }
        
        // Create blocks for short-circuit evaluation
        llvm::Function* func = builder.GetInsertBlock()->getParent();
        llvm::BasicBlock* evalRightBB = llvm::BasicBlock::Create(context, "or_eval_right", func);
        llvm::BasicBlock* mergeBB = llvm::BasicBlock::Create(context, "or_merge", func);
        
        // If left is true, skip right and return true
        builder.CreateCondBr(left, mergeBB, evalRightBB);
        
        // Evaluate right
        builder.SetInsertPoint(evalRightBB);
        if (!right->getType()->isIntegerTy(1)) {
            right = builder.CreateICmpNE(right, llvm::ConstantInt::get(right->getType(), 0), "tobool");
        }
        builder.CreateBr(mergeBB);
        
        // Merge
        builder.SetInsertPoint(mergeBB);
        llvm::PHINode* phi = builder.CreatePHI(llvm::Type::getInt1Ty(context), 2, "or_result");
        phi->addIncoming(llvm::ConstantInt::get(llvm::Type::getInt1Ty(context), 1), evalRightBB->getSinglePredecessor());
        phi->addIncoming(right, evalRightBB);
        
        return phi;
    }
    
    // NULL COALESCING OPERATOR (??) with short-circuit evaluation
    // Pattern: optional ?? default returns value if has value, otherwise default
    if (op == TokenType::TOKEN_NULL_COALESCE) {
        // Save left value and block
        llvm::Value* leftVal = left;
        llvm::BasicBlock* leftBB = builder.GetInsertBlock();
        
        // Create blocks for conditional evaluation
        llvm::Function* func = builder.GetInsertBlock()->getParent();
        llvm::BasicBlock* useRightBB = llvm::BasicBlock::Create(context, "coalesce_right", func);
        llvm::BasicBlock* mergeBB = llvm::BasicBlock::Create(context, "coalesce_merge", func);
        
        // Check if left is optional type
        llvm::Value* isNull;
        llvm::Value* unwrappedLeft;
        
        if (left->getType()->isStructTy()) {
            // Left is optional type { i1 hasValue, T value }
            // Extract hasValue field
            llvm::Value* hasValue = isOptionalSome(left);
            isNull = builder.CreateNot(hasValue, "isNone");
            
            // Extract value for use if has value
            unwrappedLeft = unwrapOptional(left);
        } else if (left->getType()->isPointerTy()) {
            // For pointers, check against nullptr
            isNull = builder.CreateIsNull(left, "isnull");
            unwrappedLeft = left;
        } else if (left->getType()->isIntegerTy()) {
            // For integers, check against 0 (NIL sentinel)
            isNull = builder.CreateICmpEQ(left, llvm::ConstantInt::get(left->getType(), 0), "isnull");
            unwrappedLeft = left;
        } else {
            // For other types, assume not null
            isNull = llvm::ConstantInt::get(llvm::Type::getInt1Ty(context), 0);
            unwrappedLeft = left;
        }
        
        // If left is null, evaluate and use right; otherwise use unwrapped left
        builder.CreateCondBr(isNull, useRightBB, mergeBB);
        
        // Evaluate right side only if left was null
        builder.SetInsertPoint(useRightBB);
        llvm::Value* rightVal = right;
        llvm::BasicBlock* rightBB = builder.GetInsertBlock();
        builder.CreateBr(mergeBB);
        
        // Merge with phi node - use unwrapped type
        builder.SetInsertPoint(mergeBB);
        llvm::Type* resultType = unwrappedLeft->getType();
        llvm::PHINode* phi = builder.CreatePHI(resultType, 2, "coalesce_result");
        phi->addIncoming(unwrappedLeft, leftBB);
        phi->addIncoming(rightVal, rightBB);
        
        return phi;
    }
    
    // BITWISE OPERATORS
    if (op == TokenType::TOKEN_AMPERSAND) {
        return builder.CreateAnd(left, right, "andtmp");
    }
    
    if (op == TokenType::TOKEN_PIPE) {
        return builder.CreateOr(left, right, "ortmp");
    }
    
    if (op == TokenType::TOKEN_CARET) {
        return builder.CreateXor(left, right, "xortmp");
    }
    
    if (op == TokenType::TOKEN_SHIFT_LEFT) {
        return builder.CreateShl(left, right, "shltmp");
    }
    
    if (op == TokenType::TOKEN_SHIFT_RIGHT) {
        // Arithmetic right shift (sign extension)
        return builder.CreateAShr(left, right, "shrtmp");
    }
    
    // SAFE NAVIGATION OPERATOR (?.)
    // Pattern: obj?.member returns optional<T> (Some(member) if obj not NIL, None otherwise)
    if (op == TokenType::TOKEN_SAFE_NAV) {
        // Check if left (object) is NIL/null
        llvm::Value* isNull;
        
        if (left->getType()->isPointerTy()) {
            // For pointers, check against nullptr
            isNull = builder.CreateIsNull(left, "obj.isnull");
        } else if (left->getType()->isStructTy()) {
            // If left is already optional, check hasValue
            isNull = builder.CreateNot(isOptionalSome(left), "obj.isnone");
        } else {
            // For primitive types (shouldn't normally happen), assume not null
            isNull = llvm::ConstantInt::get(llvm::Type::getInt1Ty(context), 0);
        }
        
        // Create basic blocks for conditional member access
        llvm::Function* func = builder.GetInsertBlock()->getParent();
        llvm::BasicBlock* accessBB = llvm::BasicBlock::Create(context, "safeNav.access", func);
        llvm::BasicBlock* nilBB = llvm::BasicBlock::Create(context, "safeNav.nil", func);
        llvm::BasicBlock* mergeBB = llvm::BasicBlock::Create(context, "safeNav.merge", func);
        
        // Branch based on null check
        builder.CreateCondBr(isNull, nilBB, accessBB);
        
        // Access block: evaluate member access (right side)
        builder.SetInsertPoint(accessBB);
        llvm::Value* memberVal = right;  // Right is the member access result
        
        // Wrap result in optional Some
        llvm::Value* someResult = createOptionalSome(memberVal, memberVal->getType());
        builder.CreateBr(mergeBB);
        llvm::BasicBlock* afterAccessBB = builder.GetInsertBlock();
        
        // NIL block: return None
        builder.SetInsertPoint(nilBB);
        llvm::Value* noneResult = createOptionalNone(memberVal->getType());
        builder.CreateBr(mergeBB);
        
        // Merge results
        builder.SetInsertPoint(mergeBB);
        llvm::PHINode* phi = builder.CreatePHI(someResult->getType(), 2, "safeNav.result");
        phi->addIncoming(someResult, afterAccessBB);
        phi->addIncoming(noneResult, nilBB);
        
        return phi;
    }
    
    // PIPELINE FORWARD OPERATOR (|>)
    // Pattern: value |> function becomes function(value)
    if (op == TokenType::TOKEN_PIPE_RIGHT) {
        // Right side should be a function or callable
        // Left side is the value to pass as first argument
        
        // For now, assume right is a function pointer or callable
        // We need to call it with left as the argument
        
        if (right->getType()->isPointerTy()) {
            // Right is likely a function pointer
            // Create call with left as argument
            llvm::FunctionType* funcType = llvm::FunctionType::get(
                left->getType(),  // Return type (assumed same as input for now)
                {left->getType()}, // Parameter types
                false  // Not variadic
            );
            
            return builder.CreateCall(funcType, right, {left}, "pipe.result");
        } else {
            // For non-function types, just return the right side
            // TODO: Full implementation needs function type analysis
            return right;
        }
    }
    
    // PIPELINE BACKWARD OPERATOR (<|)
    // Pattern: function <| value becomes function(value)  
    if (op == TokenType::TOKEN_PIPE_LEFT) {
        // Left side should be a function or callable
        // Right side is the value to pass as first argument
        
        if (left->getType()->isPointerTy()) {
            // Left is likely a function pointer
            llvm::FunctionType* funcType = llvm::FunctionType::get(
                right->getType(),  // Return type
                {right->getType()}, // Parameter types
                false  // Not variadic
            );
            
            return builder.CreateCall(funcType, left, {right}, "pipe.result");
        } else {
            // For non-function types, just return the left side
            return left;
        }
    }
    
    // Unknown operator
    throw std::runtime_error("Unknown binary operator: " + expr->op.lexeme);
}

/**
 * Generate code for unary operations
 * Handles: neg, not, address, deref
 */
llvm::Value* ExprCodegen::codegenUnary(UnaryExpr* expr) {
    if (!expr) {
        throw std::runtime_error("Null unary expression");
    }
    
    // Generate code for the operand recursively
    llvm::Value* operand = codegenExpressionNode(expr->operand.get(), this);
    if (!operand) {
        throw std::runtime_error("Failed to generate code for unary operand");
    }
    
    TokenType op = expr->op.type;
    bool isFloat = operand->getType()->isFloatingPointTy();
    
    // Arithmetic negation: -x
    if (op == TokenType::TOKEN_MINUS) {
        if (isFloat) {
            return builder.CreateFNeg(operand, "negtmp");
        } else {
            return builder.CreateNeg(operand, "negtmp");
        }
    }
    
    // Logical NOT: !x
    if (op == TokenType::TOKEN_BANG) {
        // If operand is not i1, need to compare with zero
        if (operand->getType()->isIntegerTy(1)) {
            // Already boolean, just XOR with true
            return builder.CreateNot(operand, "nottmp");
        } else if (isFloat) {
            // Compare float with 0.0
            llvm::Value* zero = llvm::ConstantFP::get(operand->getType(), 0.0);
            return builder.CreateFCmpOEQ(operand, zero, "nottmp");
        } else {
            // Compare integer with 0
            llvm::Value* zero = llvm::ConstantInt::get(operand->getType(), 0);
            return builder.CreateICmpEQ(operand, zero, "nottmp");
        }
    }
    
    // Bitwise NOT: ~x
    if (op == TokenType::TOKEN_TILDE) {
        if (isFloat) {
            throw std::runtime_error("Bitwise NOT cannot be applied to floating-point types");
        }
        return builder.CreateNot(operand, "bnottmp");
    }
    
    // Address-of operator: @x
    if (op == TokenType::TOKEN_AT) {
        // For address-of, we need the address, not the loaded value
        // This would require integration with symbol table to get alloca
        // For now, we'll return the operand itself if it's already a pointer
        // Full implementation requires lvalue handling in Phase 4.3+
        throw std::runtime_error("Address-of operator (@) requires lvalue support (Phase 4.3+)");
    }
    
    // Dereference operator: * (when TOKEN_STAR used as unary)
    if (op == TokenType::TOKEN_STAR) {
        // Dereference a pointer: load from the pointer
        if (!operand->getType()->isPointerTy()) {
            throw std::runtime_error("Dereference operator (*) can only be applied to pointer types");
        }
        return builder.CreateLoad(llvm::Type::getInt32Ty(context), operand, "dereftmp");
    }
    
    // Increment/decrement operators (++, --)
    if (op == TokenType::TOKEN_PLUS_PLUS || op == TokenType::TOKEN_MINUS_MINUS) {
        throw std::runtime_error("Increment/decrement operators (++/--) require lvalue support (Phase 4.3+)");
    }
    
    throw std::runtime_error("Unknown unary operator: " + std::to_string(static_cast<int>(op)));
}

/**
 * Generate code for function calls
 */
llvm::Value* ExprCodegen::codegenCall(CallExpr* expr) {
    if (!expr) {
        throw std::runtime_error("Null call expression");
    }
    
    // The callee should be an identifier (function name or func variable)
    IdentifierExpr* callee_ident = dynamic_cast<IdentifierExpr*>(expr->callee.get());
    if (!callee_ident) {
        throw std::runtime_error("Function callee must be an identifier");
    }
    
    // ====================================================================
    // BUILTIN FUNCTION: print()
    // ====================================================================
    if (callee_ident->name == "print") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("print() requires exactly one argument");
        }
        
        // Evaluate the argument
        llvm::Value* arg = codegenExpressionNode(expr->arguments[0].get(), this);
        if (!arg) {
            throw std::runtime_error("Failed to generate code for print argument");
        }
        
        // Declare printf if not already declared
        llvm::Function* printf_func = module->getFunction("printf");
        if (!printf_func) {
            // printf signature: i32 (i8*, ...)
            llvm::FunctionType* printf_type = llvm::FunctionType::get(
                builder.getInt32Ty(),
                llvm::PointerType::get(context, 0),
                true  // vararg
            );
            printf_func = llvm::Function::Create(
                printf_type,
                llvm::Function::ExternalLinkage,
                "printf",
                module
            );
        }
        
        // Handle different argument types
        llvm::Type* arg_type = arg->getType();
        llvm::Value* format_str = nullptr;
        std::vector<llvm::Value*> printf_args;
        
        if (arg_type->isPointerTy()) {
            // Assume it's a string - use "%s" format
            format_str = builder.CreateGlobalStringPtr("%s", "str_fmt");
            printf_args.push_back(format_str);
            printf_args.push_back(arg);
        } else if (arg_type->isIntegerTy()) {
            // Integer type - use appropriate format based on bit width
            unsigned bit_width = arg_type->getIntegerBitWidth();
            
            // Convert smaller ints to i32 or i64 for printf
            if (bit_width < 32) {
                arg = builder.CreateSExt(arg, builder.getInt32Ty(), "print_ext");
                format_str = builder.CreateGlobalStringPtr("%d", "int_fmt");
            } else if (bit_width == 32) {
                format_str = builder.CreateGlobalStringPtr("%d", "int_fmt");
            } else {
                // 64-bit or larger
                if (bit_width > 64) {
                    arg = builder.CreateTrunc(arg, builder.getInt64Ty(), "print_trunc");
                }
                format_str = builder.CreateGlobalStringPtr("%lld", "int64_fmt");
            }
            
            printf_args.push_back(format_str);
            printf_args.push_back(arg);
        } else if (arg_type->isFloatingPointTy()) {
            // Float type - convert to double for printf
            if (arg_type->isFloatTy()) {
                arg = builder.CreateFPExt(arg, builder.getDoubleTy(), "print_fpext");
            }
            format_str = builder.CreateGlobalStringPtr("%g", "float_fmt");
            printf_args.push_back(format_str);
            printf_args.push_back(arg);
        } else {
            throw std::runtime_error("print() does not support this type yet");
        }
        
        // Call printf and return the result
        return builder.CreateCall(printf_func, printf_args, "print_call");
    }
    
    // ====================================================================
    // BUILTIN FUNCTIONS: 6-Stream I/O System
    // ====================================================================
    
    // Helper lambda to create stream write functions
    auto create_stream_write = [&](const std::string& func_name, const std::string& stream_func) -> llvm::Value* {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error(func_name + " requires exactly one argument");
        }
        
        llvm::Value* arg = codegenExpressionNode(expr->arguments[0].get(), this);
        if (!arg) {
            throw std::runtime_error("Failed to generate code for " + func_name + " argument");
        }
        
        // Ensure argument is a string
        if (!arg->getType()->isPointerTy()) {
            throw std::runtime_error(func_name + " requires a string argument");
        }
        
        // Declare the C runtime function if not already declared
        // Signature: int64_t aria_xxx_write(const char* str)
        llvm::Function* stream_func_ptr = module->getFunction(stream_func);
        if (!stream_func_ptr) {
            llvm::FunctionType* func_type = llvm::FunctionType::get(
                builder.getInt64Ty(),  // returns int64_t
                {llvm::PointerType::get(context, 0)},  // takes const char*
                false  // not vararg
            );
            stream_func_ptr = llvm::Function::Create(
                func_type,
                llvm::Function::ExternalLinkage,
                stream_func,
                module
            );
        }
        
        // Call the stream function
        return builder.CreateCall(stream_func_ptr, {arg}, func_name + "_call");
    };
    
    // stdout_write(string) -> int64
    if (callee_ident->name == "stdout_write") {
        return create_stream_write("stdout_write", "aria_stdout_write");
    }
    
    // stderr_write(string) -> int64
    if (callee_ident->name == "stderr_write") {
        return create_stream_write("stderr_write", "aria_stderr_write");
    }
    
    // stddbg_write(string) -> int64
    if (callee_ident->name == "stddbg_write") {
        return create_stream_write("stddbg_write", "aria_stddbg_write");
    }
    
    // stdin_read_line() -> string
    if (callee_ident->name == "stdin_read_line") {
        if (expr->arguments.size() != 0) {
            throw std::runtime_error("stdin_read_line() takes no arguments");
        }
        
        llvm::Function* read_func = module->getFunction("aria_stdin_read_line");
        if (!read_func) {
            llvm::FunctionType* func_type = llvm::FunctionType::get(
                llvm::PointerType::get(context, 0),  // returns char*
                {},  // no arguments
                false
            );
            read_func = llvm::Function::Create(
                func_type,
                llvm::Function::ExternalLinkage,
                "aria_stdin_read_line",
                module
            );
        }
        
        return builder.CreateCall(read_func, {}, "stdin_read_call");
    }
    
    // ====================================================================
    // ARENA ALLOCATOR BUILTINS (Phase 4.2.5.2)
    // ====================================================================
    
    if (callee_ident->name == "arena_new") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("arena_new() requires exactly one argument");
        }
        
        llvm::Function* arena_new_func = module->getFunction("aria_arena_new_handle");
        if (!arena_new_func) {
            llvm::FunctionType* arena_new_type = llvm::FunctionType::get(
                builder.getInt64Ty(), {builder.getInt64Ty()}, false);
            arena_new_func = llvm::Function::Create(arena_new_type,
                llvm::Function::ExternalLinkage, "aria_arena_new_handle", module);
        }
        
        llvm::Value* capacity = codegenExpressionNode(expr->arguments[0].get(), this);
        if (!capacity->getType()->isIntegerTy(64)) {
            capacity = builder.CreateSExtOrTrunc(capacity, builder.getInt64Ty());
        }
        return builder.CreateCall(arena_new_func, {capacity}, "arena_handle");
    }
    
    if (callee_ident->name == "arena_alloc") {
        if (expr->arguments.size() != 2) {
            throw std::runtime_error("arena_alloc() requires two arguments");
        }
        
        llvm::Function* arena_alloc_func = module->getFunction("aria_arena_alloc_handle");
        if (!arena_alloc_func) {
            llvm::FunctionType* arena_alloc_type = llvm::FunctionType::get(
                builder.getInt64Ty(), {builder.getInt64Ty(), builder.getInt64Ty()}, false);
            arena_alloc_func = llvm::Function::Create(arena_alloc_type,
                llvm::Function::ExternalLinkage, "aria_arena_alloc_handle", module);
        }
        
        llvm::Value* handle = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* size = codegenExpressionNode(expr->arguments[1].get(), this);
        if (!handle->getType()->isIntegerTy(64)) {
            handle = builder.CreateSExtOrTrunc(handle, builder.getInt64Ty());
        }
        if (!size->getType()->isIntegerTy(64)) {
            size = builder.CreateSExtOrTrunc(size, builder.getInt64Ty());
        }
        return builder.CreateCall(arena_alloc_func, {handle, size}, "alloc_ptr");
    }
    
    if (callee_ident->name == "arena_reset") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("arena_reset() requires one argument");
        }
        
        llvm::Function* arena_reset_func = module->getFunction("aria_arena_reset_handle");
        if (!arena_reset_func) {
            llvm::FunctionType* arena_reset_type = llvm::FunctionType::get(
                builder.getVoidTy(), {builder.getInt64Ty()}, false);
            arena_reset_func = llvm::Function::Create(arena_reset_type,
                llvm::Function::ExternalLinkage, "aria_arena_reset_handle", module);
        }
        
        llvm::Value* handle = codegenExpressionNode(expr->arguments[0].get(), this);
        if (!handle->getType()->isIntegerTy(64)) {
            handle = builder.CreateSExtOrTrunc(handle, builder.getInt64Ty());
        }
        builder.CreateCall(arena_reset_func, {handle});
        return builder.getInt32(0);
    }
    
    if (callee_ident->name == "arena_destroy") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("arena_destroy() requires one argument");
        }
        
        llvm::Function* arena_destroy_func = module->getFunction("aria_arena_destroy_handle");
        if (!arena_destroy_func) {
            llvm::FunctionType* arena_destroy_type = llvm::FunctionType::get(
                builder.getVoidTy(), {builder.getInt64Ty()}, false);
            arena_destroy_func = llvm::Function::Create(arena_destroy_type,
                llvm::Function::ExternalLinkage, "aria_arena_destroy_handle", module);
        }
        
        llvm::Value* handle = codegenExpressionNode(expr->arguments[0].get(), this);
        if (!handle->getType()->isIntegerTy(64)) {
            handle = builder.CreateSExtOrTrunc(handle, builder.getInt64Ty());
        }
        builder.CreateCall(arena_destroy_func, {handle});
        return builder.getInt32(0);
    }
    
    if (callee_ident->name == "arena_get_allocated") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("arena_get_allocated() requires one argument");
        }
        
        llvm::Function* func = module->getFunction("aria_arena_get_allocated_handle");
        if (!func) {
            llvm::FunctionType* func_type = llvm::FunctionType::get(
                builder.getInt64Ty(), {builder.getInt64Ty()}, false);
            func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                "aria_arena_get_allocated_handle", module);
        }
        
        llvm::Value* handle = codegenExpressionNode(expr->arguments[0].get(), this);
        if (!handle->getType()->isIntegerTy(64)) {
            handle = builder.CreateSExtOrTrunc(handle, builder.getInt64Ty());
        }
        return builder.CreateCall(func, {handle}, "allocated_bytes");
    }
    
    if (callee_ident->name == "arena_get_reserved") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("arena_get_reserved() requires one argument");
        }
        
        llvm::Function* func = module->getFunction("aria_arena_get_reserved_handle");
        if (!func) {
            llvm::FunctionType* func_type = llvm::FunctionType::get(
                builder.getInt64Ty(), {builder.getInt64Ty()}, false);
            func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                "aria_arena_get_reserved_handle", module);
        }
        
        llvm::Value* handle = codegenExpressionNode(expr->arguments[0].get(), this);
        if (!handle->getType()->isIntegerTy(64)) {
            handle = builder.CreateSExtOrTrunc(handle, builder.getInt64Ty());
        }
        return builder.CreateCall(func, {handle}, "reserved_bytes");
    }
    
    // ====================================================================
    // END ARENA BUILTINS
    // ====================================================================
    
    // ====================================================================
    // POOL ALLOCATOR BUILTINS (Phase 4.2.5.3)
    // ====================================================================
    
    if (callee_ident->name == "pool_new") {
        if (expr->arguments.size() != 2) {
            throw std::runtime_error("pool_new() requires exactly two arguments");
        }
        
        llvm::Function* pool_new_func = module->getFunction("aria_pool_new_handle");
        if (!pool_new_func) {
            llvm::FunctionType* pool_new_type = llvm::FunctionType::get(
                builder.getInt64Ty(), {builder.getInt64Ty(), builder.getInt64Ty()}, false);
            pool_new_func = llvm::Function::Create(pool_new_type,
                llvm::Function::ExternalLinkage, "aria_pool_new_handle", module);
        }
        
        llvm::Value* block_size = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* capacity = codegenExpressionNode(expr->arguments[1].get(), this);
        if (!block_size->getType()->isIntegerTy(64)) {
            block_size = builder.CreateSExtOrTrunc(block_size, builder.getInt64Ty());
        }
        if (!capacity->getType()->isIntegerTy(64)) {
            capacity = builder.CreateSExtOrTrunc(capacity, builder.getInt64Ty());
        }
        return builder.CreateCall(pool_new_func, {block_size, capacity}, "pool_handle");
    }
    
    if (callee_ident->name == "pool_alloc") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("pool_alloc() requires one argument");
        }
        
        llvm::Function* pool_alloc_func = module->getFunction("aria_pool_alloc_handle");
        if (!pool_alloc_func) {
            llvm::FunctionType* pool_alloc_type = llvm::FunctionType::get(
                builder.getInt64Ty(), {builder.getInt64Ty()}, false);
            pool_alloc_func = llvm::Function::Create(pool_alloc_type,
                llvm::Function::ExternalLinkage, "aria_pool_alloc_handle", module);
        }
        
        llvm::Value* handle = codegenExpressionNode(expr->arguments[0].get(), this);
        if (!handle->getType()->isIntegerTy(64)) {
            handle = builder.CreateSExtOrTrunc(handle, builder.getInt64Ty());
        }
        return builder.CreateCall(pool_alloc_func, {handle}, "alloc_ptr");
    }
    
    if (callee_ident->name == "pool_free") {
        if (expr->arguments.size() != 2) {
            throw std::runtime_error("pool_free() requires two arguments");
        }
        
        llvm::Function* pool_free_func = module->getFunction("aria_pool_free_handle");
        if (!pool_free_func) {
            llvm::FunctionType* pool_free_type = llvm::FunctionType::get(
                builder.getVoidTy(), {builder.getInt64Ty(), builder.getInt64Ty()}, false);
            pool_free_func = llvm::Function::Create(pool_free_type,
                llvm::Function::ExternalLinkage, "aria_pool_free_handle", module);
        }
        
        llvm::Value* handle = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* ptr = codegenExpressionNode(expr->arguments[1].get(), this);
        if (!handle->getType()->isIntegerTy(64)) {
            handle = builder.CreateSExtOrTrunc(handle, builder.getInt64Ty());
        }
        if (!ptr->getType()->isIntegerTy(64)) {
            ptr = builder.CreateSExtOrTrunc(ptr, builder.getInt64Ty());
        }
        builder.CreateCall(pool_free_func, {handle, ptr});
        return builder.getInt32(0);
    }
    
    if (callee_ident->name == "pool_reset") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("pool_reset() requires one argument");
        }
        
        llvm::Function* pool_reset_func = module->getFunction("aria_pool_reset_handle");
        if (!pool_reset_func) {
            llvm::FunctionType* pool_reset_type = llvm::FunctionType::get(
                builder.getVoidTy(), {builder.getInt64Ty()}, false);
            pool_reset_func = llvm::Function::Create(pool_reset_type,
                llvm::Function::ExternalLinkage, "aria_pool_reset_handle", module);
        }
        
        llvm::Value* handle = codegenExpressionNode(expr->arguments[0].get(), this);
        if (!handle->getType()->isIntegerTy(64)) {
            handle = builder.CreateSExtOrTrunc(handle, builder.getInt64Ty());
        }
        builder.CreateCall(pool_reset_func, {handle});
        return builder.getInt32(0);
    }
    
    if (callee_ident->name == "pool_destroy") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("pool_destroy() requires one argument");
        }
        
        llvm::Function* pool_destroy_func = module->getFunction("aria_pool_destroy_handle");
        if (!pool_destroy_func) {
            llvm::FunctionType* pool_destroy_type = llvm::FunctionType::get(
                builder.getVoidTy(), {builder.getInt64Ty()}, false);
            pool_destroy_func = llvm::Function::Create(pool_destroy_type,
                llvm::Function::ExternalLinkage, "aria_pool_destroy_handle", module);
        }
        
        llvm::Value* handle = codegenExpressionNode(expr->arguments[0].get(), this);
        if (!handle->getType()->isIntegerTy(64)) {
            handle = builder.CreateSExtOrTrunc(handle, builder.getInt64Ty());
        }
        builder.CreateCall(pool_destroy_func, {handle});
        return builder.getInt32(0);
    }
    
    if (callee_ident->name == "pool_get_total_blocks") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("pool_get_total_blocks() requires one argument");
        }
        
        llvm::Function* func = module->getFunction("aria_pool_get_total_blocks_handle");
        if (!func) {
            llvm::FunctionType* func_type = llvm::FunctionType::get(
                builder.getInt64Ty(), {builder.getInt64Ty()}, false);
            func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                "aria_pool_get_total_blocks_handle", module);
        }
        
        llvm::Value* handle = codegenExpressionNode(expr->arguments[0].get(), this);
        if (!handle->getType()->isIntegerTy(64)) {
            handle = builder.CreateSExtOrTrunc(handle, builder.getInt64Ty());
        }
        return builder.CreateCall(func, {handle}, "total_blocks");
    }
    
    if (callee_ident->name == "pool_get_used_blocks") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("pool_get_used_blocks() requires one argument");
        }
        
        llvm::Function* func = module->getFunction("aria_pool_get_used_blocks_handle");
        if (!func) {
            llvm::FunctionType* func_type = llvm::FunctionType::get(
                builder.getInt64Ty(), {builder.getInt64Ty()}, false);
            func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                "aria_pool_get_used_blocks_handle", module);
        }
        
        llvm::Value* handle = codegenExpressionNode(expr->arguments[0].get(), this);
        if (!handle->getType()->isIntegerTy(64)) {
            handle = builder.CreateSExtOrTrunc(handle, builder.getInt64Ty());
        }
        return builder.CreateCall(func, {handle}, "used_blocks");
    }
    
    // ====================================================================
    // END POOL BUILTINS
    // ====================================================================
    
    // ====================================================================
    // SLAB ALLOCATOR BUILTINS (Phase 4.2.5.4)
    // ====================================================================
    
    if (callee_ident->name == "slab_new") {
        if (expr->arguments.size() != 2) {
            throw std::runtime_error("slab_new() requires exactly two arguments");
        }
        
        llvm::Function* slab_new_func = module->getFunction("aria_slab_cache_new_handle");
        if (!slab_new_func) {
            llvm::FunctionType* slab_new_type = llvm::FunctionType::get(
                builder.getInt64Ty(), {builder.getInt64Ty(), builder.getInt64Ty()}, false);
            slab_new_func = llvm::Function::Create(slab_new_type,
                llvm::Function::ExternalLinkage, "aria_slab_cache_new_handle", module);
        }
        
        llvm::Value* object_size = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* slab_size = codegenExpressionNode(expr->arguments[1].get(), this);
        if (!object_size->getType()->isIntegerTy(64)) {
            object_size = builder.CreateSExtOrTrunc(object_size, builder.getInt64Ty());
        }
        if (!slab_size->getType()->isIntegerTy(64)) {
            slab_size = builder.CreateSExtOrTrunc(slab_size, builder.getInt64Ty());
        }
        return builder.CreateCall(slab_new_func, {object_size, slab_size}, "slab_handle");
    }
    
    if (callee_ident->name == "slab_alloc") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("slab_alloc() requires one argument");
        }
        
        llvm::Function* slab_alloc_func = module->getFunction("aria_slab_cache_alloc_handle");
        if (!slab_alloc_func) {
            llvm::FunctionType* slab_alloc_type = llvm::FunctionType::get(
                builder.getInt64Ty(), {builder.getInt64Ty()}, false);
            slab_alloc_func = llvm::Function::Create(slab_alloc_type,
                llvm::Function::ExternalLinkage, "aria_slab_cache_alloc_handle", module);
        }
        
        llvm::Value* handle = codegenExpressionNode(expr->arguments[0].get(), this);
        if (!handle->getType()->isIntegerTy(64)) {
            handle = builder.CreateSExtOrTrunc(handle, builder.getInt64Ty());
        }
        return builder.CreateCall(slab_alloc_func, {handle}, "alloc_ptr");
    }
    
    if (callee_ident->name == "slab_free") {
        if (expr->arguments.size() != 2) {
            throw std::runtime_error("slab_free() requires two arguments");
        }
        
        llvm::Function* slab_free_func = module->getFunction("aria_slab_cache_free_handle");
        if (!slab_free_func) {
            llvm::FunctionType* slab_free_type = llvm::FunctionType::get(
                builder.getVoidTy(), {builder.getInt64Ty(), builder.getInt64Ty()}, false);
            slab_free_func = llvm::Function::Create(slab_free_type,
                llvm::Function::ExternalLinkage, "aria_slab_cache_free_handle", module);
        }
        
        llvm::Value* handle = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* ptr = codegenExpressionNode(expr->arguments[1].get(), this);
        if (!handle->getType()->isIntegerTy(64)) {
            handle = builder.CreateSExtOrTrunc(handle, builder.getInt64Ty());
        }
        if (!ptr->getType()->isIntegerTy(64)) {
            ptr = builder.CreateSExtOrTrunc(ptr, builder.getInt64Ty());
        }
        builder.CreateCall(slab_free_func, {handle, ptr});
        return builder.getInt32(0);
    }
    
    if (callee_ident->name == "slab_destroy") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("slab_destroy() requires one argument");
        }
        
        llvm::Function* slab_destroy_func = module->getFunction("aria_slab_cache_destroy_handle");
        if (!slab_destroy_func) {
            llvm::FunctionType* slab_destroy_type = llvm::FunctionType::get(
                builder.getVoidTy(), {builder.getInt64Ty()}, false);
            slab_destroy_func = llvm::Function::Create(slab_destroy_type,
                llvm::Function::ExternalLinkage, "aria_slab_cache_destroy_handle", module);
        }
        
        llvm::Value* handle = codegenExpressionNode(expr->arguments[0].get(), this);
        if (!handle->getType()->isIntegerTy(64)) {
            handle = builder.CreateSExtOrTrunc(handle, builder.getInt64Ty());
        }
        builder.CreateCall(slab_destroy_func, {handle});
        return builder.getInt32(0);
    }
    
    if (callee_ident->name == "slab_get_total_objects") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("slab_get_total_objects() requires one argument");
        }
        
        llvm::Function* func = module->getFunction("aria_slab_cache_get_total_objects_handle");
        if (!func) {
            llvm::FunctionType* func_type = llvm::FunctionType::get(
                builder.getInt64Ty(), {builder.getInt64Ty()}, false);
            func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                "aria_slab_cache_get_total_objects_handle", module);
        }
        
        llvm::Value* handle = codegenExpressionNode(expr->arguments[0].get(), this);
        if (!handle->getType()->isIntegerTy(64)) {
            handle = builder.CreateSExtOrTrunc(handle, builder.getInt64Ty());
        }
        return builder.CreateCall(func, {handle}, "total_objects");
    }
    
    if (callee_ident->name == "slab_get_allocated_objects") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("slab_get_allocated_objects() requires one argument");
        }
        
        llvm::Function* func = module->getFunction("aria_slab_cache_get_allocated_objects_handle");
        if (!func) {
            llvm::FunctionType* func_type = llvm::FunctionType::get(
                builder.getInt64Ty(), {builder.getInt64Ty()}, false);
            func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                "aria_slab_cache_get_allocated_objects_handle", module);
        }
        
        llvm::Value* handle = codegenExpressionNode(expr->arguments[0].get(), this);
        if (!handle->getType()->isIntegerTy(64)) {
            handle = builder.CreateSExtOrTrunc(handle, builder.getInt64Ty());
        }
        return builder.CreateCall(func, {handle}, "allocated_objects");
    }
    
    // ====================================================================
    // END SLAB BUILTINS
    // ====================================================================
    
    // Check if this is a direct function call or a closure call
    // Try to find a direct function first
    llvm::Function* direct_func = module->getFunction(callee_ident->name);
    
    // Check if it's a closure (func variable in named_values)
    auto it = named_values.find(callee_ident->name);
    bool is_closure_call = (direct_func == nullptr && it != named_values.end());
    
    if (is_closure_call) {
        // ====================================================================
        // CLOSURE CALLING CONVENTION (Fat Pointer Call)
        // ====================================================================
        // Fat pointer struct: { i8* method_ptr, i8* env_ptr }
        // Calling convention: call method_ptr(env_ptr, explicit_args...)
        
        llvm::Value* fat_ptr_alloca = it->second;
        
        // Load the fat pointer struct from memory
        // Define the fat pointer struct type
        std::vector<llvm::Type*> fat_ptr_fields = {
            llvm::PointerType::get(context, 0),  // method_ptr
            llvm::PointerType::get(context, 0)   // env_ptr
        };
        llvm::StructType* fat_ptr_type = llvm::StructType::get(context, fat_ptr_fields);
        
        llvm::Value* fat_ptr_value = builder.CreateLoad(fat_ptr_type, fat_ptr_alloca, "fat_ptr");
        
        // Extract method_ptr (field 0)
        llvm::Value* method_ptr = builder.CreateExtractValue(fat_ptr_value, 0, "method_ptr");
        
        // Extract env_ptr (field 1)
        llvm::Value* env_ptr = builder.CreateExtractValue(fat_ptr_value, 1, "env_ptr");
        
        // Evaluate explicit arguments
        std::vector<llvm::Value*> args;
        
        // Hidden first argument: env_ptr
        args.push_back(env_ptr);
        
        // Explicit arguments
        for (size_t i = 0; i < expr->arguments.size(); i++) {
            llvm::Value* arg_value = codegenExpressionNode(expr->arguments[i].get(), this);
            if (!arg_value) {
                throw std::runtime_error("Failed to generate code for closure argument " + std::to_string(i));
            }
            args.push_back(arg_value);
        }
        
        // Build function type for the call
        // Return type: Need type inference - for now try to infer from variable or default to i64
        // Parameters: env_ptr (i8*) + explicit arg types
        std::vector<llvm::Type*> param_types;
        param_types.push_back(llvm::PointerType::get(context, 0));  // env_ptr
        for (size_t i = 1; i < args.size(); i++) {
            param_types.push_back(args[i]->getType());
        }
        
        // Try to determine return type
        // Strategy: Extract type from method_ptr if it's been cast from a known function
        // For now, default to i64 (will be fixed with full type system integration)
        llvm::Type* return_type = llvm::Type::getInt64Ty(context);
        
        // TODO: Full type system integration - store function type with closure
        // For proper type inference, we need to:
        // 1. Store the FunctionType in the closure metadata OR
        // 2. Query the type system for the variable's type OR
        // 3. Perform type inference analysis on the lambda body
        //
        // Current limitation: All closures assumed to return i64
        // This works for now but will need fixing for:
        // - void returns
        // - non-i64 primitive returns  
        // - struct/array returns
        
        llvm::FunctionType* closure_func_type = llvm::FunctionType::get(
            return_type,
            param_types,
            false  // not vararg
        );
        
        // Cast method_ptr (i8*) to typed function pointer
        llvm::Value* typed_func_ptr = builder.CreateBitCast(
            method_ptr,
            llvm::PointerType::get(closure_func_type, 0),
            "typed_method_ptr"
        );
        
        // Create indirect call through function pointer
        return builder.CreateCall(closure_func_type, typed_func_ptr, args, "closure_call");
        
    } else if (direct_func) {
        // ====================================================================
        // DIRECT FUNCTION CALL (Standard calling convention)
        // ====================================================================
        
        // Verify argument count matches
        if (direct_func->arg_size() != expr->arguments.size()) {
            throw std::runtime_error("Incorrect number of arguments passed to function " + 
                                    callee_ident->name + ": expected " + 
                                    std::to_string(direct_func->arg_size()) + 
                                    ", got " + std::to_string(expr->arguments.size()));
        }
        
        // Evaluate all arguments recursively
        std::vector<llvm::Value*> args;
        for (size_t i = 0; i < expr->arguments.size(); i++) {
            llvm::Value* arg_value = codegenExpressionNode(expr->arguments[i].get(), this);
            if (!arg_value) {
                throw std::runtime_error("Failed to generate code for argument " + std::to_string(i));
            }
            args.push_back(arg_value);
        }
        
        // Generate the call instruction
        return builder.CreateCall(direct_func, args, "calltmp");
        
    } else {
        throw std::runtime_error("Unknown function or closure: " + callee_ident->name);
    }
}

/**
 * Generate code for ternary expressions (is ? :)
 * Syntax: is condition : true_value : false_value
 * Generates branching control flow with PHI node for result merging
 */
llvm::Value* ExprCodegen::codegenTernary(TernaryExpr* expr) {
    if (!expr) {
        throw std::runtime_error("Null ternary expression");
    }
    
    // Get current function for creating basic blocks
    llvm::Function* func = builder.GetInsertBlock()->getParent();
    
    // Evaluate the condition
    llvm::Value* condition = codegenExpressionNode(expr->condition.get(), this);
    if (!condition) {
        throw std::runtime_error("Failed to generate code for ternary condition");
    }
    
    // If condition is not i1, convert to boolean (compare with zero)
    if (!condition->getType()->isIntegerTy(1)) {
        if (condition->getType()->isFloatingPointTy()) {
            llvm::Value* zero = llvm::ConstantFP::get(condition->getType(), 0.0);
            condition = builder.CreateFCmpONE(condition, zero, "ternary_cond");
        } else {
            llvm::Value* zero = llvm::ConstantInt::get(condition->getType(), 0);
            condition = builder.CreateICmpNE(condition, zero, "ternary_cond");
        }
    }
    
    // Create basic blocks for control flow
    llvm::BasicBlock* true_bb = llvm::BasicBlock::Create(context, "ternary_true", func);
    llvm::BasicBlock* false_bb = llvm::BasicBlock::Create(context, "ternary_false", func);
    llvm::BasicBlock* merge_bb = llvm::BasicBlock::Create(context, "ternary_merge", func);
    
    // Branch based on condition
    builder.CreateCondBr(condition, true_bb, false_bb);
    
    // Generate code for true branch
    builder.SetInsertPoint(true_bb);
    llvm::Value* true_value = codegenExpressionNode(expr->trueValue.get(), this);
    if (!true_value) {
        throw std::runtime_error("Failed to generate code for ternary true value");
    }
    builder.CreateBr(merge_bb);
    // Update true_bb in case code generation changed the current block
    true_bb = builder.GetInsertBlock();
    
    // Generate code for false branch
    builder.SetInsertPoint(false_bb);
    llvm::Value* false_value = codegenExpressionNode(expr->falseValue.get(), this);
    if (!false_value) {
        throw std::runtime_error("Failed to generate code for ternary false value");
    }
    builder.CreateBr(merge_bb);
    // Update false_bb in case code generation changed the current block
    false_bb = builder.GetInsertBlock();
    
    // Verify both branches produce the same type
    if (true_value->getType() != false_value->getType()) {
        throw std::runtime_error("Ternary branches must produce values of the same type");
    }
    
    // Create merge point with PHI node
    builder.SetInsertPoint(merge_bb);
    llvm::PHINode* phi = builder.CreatePHI(true_value->getType(), 2, "ternary_result");
    phi->addIncoming(true_value, true_bb);
    phi->addIncoming(false_value, false_bb);
    
    return phi;
}

/**
 * Generate code for array indexing
 */
llvm::Value* ExprCodegen::codegenIndex(IndexExpr* expr) {
    if (!expr) {
        throw std::runtime_error("Null index expression");
    }
    
    // Generate code for the array/vector
    llvm::Value* arrayVal = codegenExpressionNode(expr->array.get(), this);
    if (!arrayVal) {
        throw std::runtime_error("Failed to generate code for indexed object");
    }
    
    // Generate code for the index
    llvm::Value* indexVal = codegenExpressionNode(expr->index.get(), this);
    if (!indexVal) {
        throw std::runtime_error("Failed to generate code for index");
    }
    
    llvm::Type* arrayType = arrayVal->getType();
    
    // Handle vector indexing
    if (arrayType->isVectorTy() || (arrayType->isStructTy() && arrayType->getStructNumElements() == 9)) {
        // For vec2/vec3 (LLVM FixedVectorType): use ExtractElement
        if (arrayType->isVectorTy()) {
            return builder.CreateExtractElement(arrayVal, indexVal, "vec.index");
        }
        
        // For vec9 (struct): Extract with constant index
        // Note: vec9[i] where i is not constant requires a different approach
        if (llvm::ConstantInt* constIndex = llvm::dyn_cast<llvm::ConstantInt>(indexVal)) {
            int idx = constIndex->getSExtValue();
            return builder.CreateExtractValue(arrayVal, idx, "vec9.index");
        } else {
            // Dynamic index into vec9 struct - needs switch/select pattern
            throw std::runtime_error("Dynamic indexing into vec9 not yet implemented");
        }
    }
    
    // TODO: Implement array indexing when arrays are added
    throw std::runtime_error("Array indexing not yet implemented");
}

/**
 * Generate code for member access
 */
llvm::Value* ExprCodegen::codegenMemberAccess(MemberAccessExpr* expr) {
    if (!expr) {
        throw std::runtime_error("Null member access expression");
    }
    
    // Generate code for the object
    llvm::Value* objectVal = codegenExpressionNode(expr->object.get(), this);
    if (!objectVal) {
        throw std::runtime_error("Failed to generate code for member access object");
    }
    
    llvm::Type* objectType = objectVal->getType();
    
    // Handle vector component access (.x, .y, .z)
    if (objectType->isVectorTy() || (objectType->isStructTy() && objectType->getStructNumElements() == 9)) {
        int index = -1;
        
        if (expr->member == "x") index = 0;
        else if (expr->member == "y") index = 1;
        else if (expr->member == "z") index = 2;
        else {
            throw std::runtime_error("Invalid vector component: " + expr->member);
        }
        
        // For vec2/vec3 (LLVM FixedVectorType): use ExtractElement
        if (objectType->isVectorTy()) {
            return builder.CreateExtractElement(objectVal, index, expr->member);
        }
        
        // For vec9 (struct): use ExtractValue
        if (objectType->isStructTy()) {
            return builder.CreateExtractValue(objectVal, index, expr->member);
        }
    }
    
    // For safe navigation (?.), we need to check if the object is null
    if (expr->isSafeNavigation) {
        // Create blocks for conditional access
        llvm::Function* func = builder.GetInsertBlock()->getParent();
        llvm::BasicBlock* checkBB = builder.GetInsertBlock();
        llvm::BasicBlock* accessBB = llvm::BasicBlock::Create(context, "safe_nav_access", func);
        llvm::BasicBlock* mergeBB = llvm::BasicBlock::Create(context, "safe_nav_merge", func);
        
        // Check if object is null
        llvm::Value* isNull;
        if (objectVal->getType()->isPointerTy()) {
            // For pointers, check against nullptr
            isNull = builder.CreateIsNull(objectVal, "is_null");
        } else {
            // For other types, assume not null (TODO: handle optional types)
            isNull = llvm::ConstantInt::get(llvm::Type::getInt1Ty(context), 0);
        }
        
        // Branch: if null, skip to merge with null result; otherwise access member
        builder.CreateCondBr(isNull, mergeBB, accessBB);
        
        // Access block: perform the actual member access
        builder.SetInsertPoint(accessBB);
        llvm::Value* memberVal = nullptr;
        
        // TODO: Implement actual member access based on type
        // For now, this is a placeholder that needs struct support
        // For structs, we would use getelementptr to get the field offset
        
        // Temporary: just return a zero value
        // In reality, we need to:
        // 1. Get the struct type from type system
        // 2. Find the member field index
        // 3. Use getelementptr to compute field address
        // 4. Load the value from that address
        llvm::Type* resultType = llvm::Type::getInt64Ty(context); // Placeholder
        memberVal = llvm::ConstantInt::get(resultType, 0);
        
        llvm::BasicBlock* accessEndBB = builder.GetInsertBlock();
        builder.CreateBr(mergeBB);
        
        // Merge block: use phi node to select between null and actual value
        builder.SetInsertPoint(mergeBB);
        llvm::PHINode* phi = builder.CreatePHI(resultType, 2, "safe_nav_result");
        phi->addIncoming(llvm::ConstantInt::get(resultType, 0), checkBB); // NIL/null case
        phi->addIncoming(memberVal, accessEndBB); // Successful access
        
        return phi;
    }
    
    // For regular member access (. or ->)
    // TODO: Implement struct field access with getelementptr
    // For now, this needs struct type support to be fully implemented
    throw std::runtime_error("Member access not yet fully implemented - needs struct support");
}

// ============================================================================
// Phase 4.5.2: LAMBDA/CLOSURE CODE GENERATION
// ============================================================================
//
// Implements fat pointer representation for closures: { method_ptr, env_ptr }
// Reference: research_016 (Functional Types)
//
// Fat Pointer Layout (16 bytes on 64-bit):
//   struct FuncFatPtr {
//       void* method_ptr;  // Pointer to lambda body machine code
//       void* env_ptr;     // Pointer to captured environment (or NULL)
//   };
//
// Calling Convention:
//   1. Load method_ptr into temp register
//   2. Load env_ptr into dedicated register (hidden first argument)
//   3. Call method_ptr with env_ptr + explicit arguments
//   4. Inside lambda: access captures via env_ptr offset
//
// Capture Strategies:
//   - BY_VALUE: Copy primitives into environment struct
//   - BY_REFERENCE: Store pointer to variable in environment
//   - BY_MOVE: Transfer ownership (invalidate original)
//
// ============================================================================

/**
 * Generate code for lambda expression (closure)
 * Creates a fat pointer with method_ptr and env_ptr
 * 
 * Example Aria code:
 *   func:add = int8(int8:a, int8:b) { return a + b; };
 *   
 *   int8:x = 10;
 *   func:addX = int8(int8:y) { return x + y; };  // Captures x
 *
 * Generated LLVM IR (non-capturing):
 *   %lambda_body_1 = function returning i8, taking (i8*, i8, i8)
 *   %fat_ptr = alloca { i8*, i8* }
 *   %method_ptr = bitcast %lambda_body_1 to i8*
 *   %gep_0 = getelementptr { i8*, i8* }, %fat_ptr, 0, 0
 *   store i8* %method_ptr, i8** %gep_0
 *   %gep_1 = getelementptr { i8*, i8* }, %fat_ptr, 0, 1
 *   store i8* null, i8** %gep_1  ; No environment
 *   
 * Generated LLVM IR (capturing x):
 *   %env = alloca { i8 }  ; Environment with one i8 capture
 *   %x_val = load i8, i8* %x
 *   %env_field_0 = getelementptr { i8 }, %env, 0, 0
 *   store i8 %x_val, i8* %env_field_0
 *   %lambda_body_2 = function returning i8, taking (i8*, i8)
 *   %fat_ptr = alloca { i8*, i8* }
 *   %method_ptr = bitcast %lambda_body_2 to i8*
 *   %gep_0 = getelementptr { i8*, i8* }, %fat_ptr, 0, 0
 *   store i8* %method_ptr, i8** %gep_0
 *   %env_ptr = bitcast { i8 }* %env to i8*
 *   %gep_1 = getelementptr { i8*, i8* }, %fat_ptr, 0, 1
 *   store i8* %env_ptr, i8** %gep_1
 */
llvm::Value* ExprCodegen::codegenLambda(LambdaExpr* expr) {
    if (!expr) {
        throw std::runtime_error("Null lambda expression");
    }
    
    // ========================================================================
    // STEP 1: CREATE ENVIRONMENT STRUCT FOR CAPTURED VARIABLES
    // ========================================================================
    
    llvm::StructType* env_struct_type = nullptr;
    llvm::Value* env_alloca = nullptr;
    
    if (!expr->capturedVars.empty()) {
        // Build environment struct type: { type0, type1, ... }
        std::vector<llvm::Type*> env_field_types;
        
        for (const auto& captured : expr->capturedVars) {
            // For now, assume all captures are i64 (will be refined later)
            // TODO: Determine actual type from symbol table
            llvm::Type* field_type = llvm::Type::getInt64Ty(context);
            env_field_types.push_back(field_type);
        }
        
        // Create anonymous struct type for environment
        env_struct_type = llvm::StructType::create(context, env_field_types, "lambda_env");
        
        // Allocate environment on stack
        env_alloca = builder.CreateAlloca(env_struct_type, nullptr, "env");
        
        // Populate environment with captured values
        for (size_t i = 0; i < expr->capturedVars.size(); ++i) {
            const auto& captured = expr->capturedVars[i];
            
            // Look up captured variable in symbol table
            auto it = named_values.find(captured.name);
            if (it == named_values.end()) {
                throw std::runtime_error("Captured variable not found: " + captured.name);
            }
            
            llvm::Value* captured_value = it->second;
            
            // Handle capture mode
            if (captured.mode == LambdaExpr::CaptureMode::BY_VALUE) {
                // Load value and store into environment
                // Assuming it's an alloca
                if (llvm::isa<llvm::AllocaInst>(captured_value)) {
                    llvm::AllocaInst* alloca = llvm::cast<llvm::AllocaInst>(captured_value);
                    llvm::Type* allocated_type = alloca->getAllocatedType();
                    llvm::Value* loaded_val = builder.CreateLoad(allocated_type, captured_value, captured.name + "_val");
                    
                    // Get pointer to environment field
                    llvm::Value* env_field_ptr = builder.CreateStructGEP(env_struct_type, env_alloca, i, "env_field_" + std::to_string(i));
                    builder.CreateStore(loaded_val, env_field_ptr);
                } else {
                    // Direct value, store as-is
                    llvm::Value* env_field_ptr = builder.CreateStructGEP(env_struct_type, env_alloca, i, "env_field_" + std::to_string(i));
                    builder.CreateStore(captured_value, env_field_ptr);
                }
            } else if (captured.mode == LambdaExpr::CaptureMode::BY_REFERENCE) {
                // Store pointer to variable
                llvm::Value* env_field_ptr = builder.CreateStructGEP(env_struct_type, env_alloca, i, "env_field_" + std::to_string(i));
                // Cast to i64* and store (pointer to original variable)
                llvm::Value* ptr_as_i64 = builder.CreatePtrToInt(captured_value, llvm::Type::getInt64Ty(context));
                builder.CreateStore(ptr_as_i64, env_field_ptr);
            } else {
                // BY_MOVE: Transfer ownership (for now, treat like BY_VALUE)
                throw std::runtime_error("BY_MOVE capture not yet implemented");
            }
        }
    }
    
    // ========================================================================
    // STEP 2: GENERATE LAMBDA FUNCTION BODY
    // ========================================================================
    
    // Generate unique name for lambda function
    static int lambda_counter = 0;
    std::string lambda_name = "lambda_" + std::to_string(lambda_counter++);
    
    // Build parameter types: env_ptr (i8*) + explicit parameters
    std::vector<llvm::Type*> param_types;
    param_types.push_back(llvm::PointerType::get(context, 0));  // i8* env_ptr (hidden first parameter)
    
    for (const auto& param : expr->parameters) {
        // Extract parameter type from AST
        if (auto paramNode = std::dynamic_pointer_cast<ParameterNode>(param)) {
            llvm::Type* param_type = getLLVMTypeFromString(paramNode->typeName);
            param_types.push_back(param_type);
        } else {
            throw std::runtime_error("Invalid parameter node in lambda");
        }
    }
    
    // Determine return type from explicit annotation (required by Aria specs)
    llvm::Type* return_type = llvm::Type::getVoidTy(context);
    if (!expr->returnTypeName.empty()) {
        return_type = getLLVMTypeFromString(expr->returnTypeName);
    } else {
        throw std::runtime_error("Lambda missing required return type annotation");
    }
    
    // Create function type
    llvm::FunctionType* lambda_func_type = llvm::FunctionType::get(return_type, param_types, false);
    
    // Create lambda function
    llvm::Function* lambda_func = llvm::Function::Create(
        lambda_func_type,
        llvm::Function::InternalLinkage,  // Lambda bodies are internal
        lambda_name,
        module
    );
    
    // Create entry basic block
    llvm::BasicBlock* entry_block = llvm::BasicBlock::Create(context, "entry", lambda_func);
    
    // Save current insertion point and named_values (lexical scope)
    llvm::BasicBlock* saved_insert_block = builder.GetInsertBlock();
    std::map<std::string, llvm::Value*> saved_named_values = named_values;
    named_values.clear();
    
    // Set insertion point to lambda body
    builder.SetInsertPoint(entry_block);
    
    // ========================================================================
    // STEP 2a: MAP LAMBDA PARAMETERS
    // ========================================================================
    
    // First argument is the hidden env_ptr (i8*)
    auto arg_it = lambda_func->arg_begin();
    llvm::Argument* env_arg = &(*arg_it);
    env_arg->setName("env");
    ++arg_it;
    
    // Map explicit parameters to allocas
    // This allows parameters to be mutable inside the lambda body
    size_t param_idx = 0;
    for (; arg_it != lambda_func->arg_end(); ++arg_it, ++param_idx) {
        llvm::Argument* arg = &(*arg_it);
        
        // Get parameter name from AST
        if (param_idx < expr->parameters.size()) {
            // Extract parameter name from ParameterNode
            ParameterNode* param_node = static_cast<ParameterNode*>(expr->parameters[param_idx].get());
            std::string param_name = param_node->paramName;
            
            arg->setName(param_name);
            
            // Create alloca for parameter
            llvm::AllocaInst* param_alloca = builder.CreateAlloca(arg->getType(), nullptr, param_name);
            builder.CreateStore(arg, param_alloca);
            
            // Add to named_values so lambda body can reference it
            named_values[param_name] = param_alloca;
        }
    }
    
    // ========================================================================
    // STEP 2b: MAP CAPTURED VARIABLES (Extract from environment)
    // ========================================================================
    
    // If we have captured variables, extract them from the environment pointer
    if (!expr->capturedVars.empty() && env_struct_type) {
        // Cast env_ptr (i8*) back to the environment struct type
        llvm::Value* env_ptr_typed = builder.CreateBitCast(
            env_arg,
            llvm::PointerType::get(env_struct_type, 0),
            "env_typed"
        );
        
        // Extract each captured variable from the environment
        for (size_t i = 0; i < expr->capturedVars.size(); ++i) {
            const auto& captured = expr->capturedVars[i];
            
            // Get pointer to field in environment struct
            llvm::Value* field_ptr = builder.CreateStructGEP(
                env_struct_type,
                env_ptr_typed,
                i,
                captured.name + "_ptr"
            );
            
            if (captured.mode == LambdaExpr::CaptureMode::BY_VALUE) {
                // BY_VALUE: Load the value from environment
                llvm::Type* field_type = env_struct_type->getElementType(i);
                llvm::Value* captured_value = builder.CreateLoad(field_type, field_ptr, captured.name);
                
                // Create alloca and store value (so it can be mutable in lambda)
                llvm::AllocaInst* capture_alloca = builder.CreateAlloca(field_type, nullptr, captured.name);
                builder.CreateStore(captured_value, capture_alloca);
                
                // Add to named_values
                named_values[captured.name] = capture_alloca;
                
            } else if (captured.mode == LambdaExpr::CaptureMode::BY_REFERENCE) {
                // BY_REFERENCE: Environment contains pointer to original variable
                // Load the pointer from environment (stored as i64)
                llvm::Value* ptr_as_i64 = builder.CreateLoad(
                    llvm::Type::getInt64Ty(context),
                    field_ptr,
                    captured.name + "_ptr_val"
                );
                
                // Convert i64 back to pointer
                // TODO: Get actual type from symbol table
                llvm::Value* original_ptr = builder.CreateIntToPtr(
                    ptr_as_i64,
                    llvm::PointerType::get(context, 0),
                    captured.name + "_ptr"
                );
                
                // Add to named_values (as pointer, so loads/stores go to original)
                named_values[captured.name] = original_ptr;
                
            } else {
                // BY_MOVE: Not yet implemented
                throw std::runtime_error("BY_MOVE capture mode not yet implemented in lambda body");
            }
        }
    }
    
    // ========================================================================
    // STEP 2c: GENERATE LAMBDA BODY
    // ========================================================================
    
    if (expr->body && stmt_codegen) {
        // Generate code for lambda body using StmtCodegen
        BlockStmt* body_block = static_cast<BlockStmt*>(expr->body.get());
        stmt_codegen->codegenBlock(body_block);
        
        // If body doesn't have a terminator, add default return
        if (!builder.GetInsertBlock()->getTerminator()) {
            if (return_type->isVoidTy()) {
                builder.CreateRetVoid();
            } else {
                // Return zero/null for non-void functions without explicit return
                if (return_type->isIntegerTy()) {
                    builder.CreateRet(llvm::ConstantInt::get(return_type, 0));
                } else if (return_type->isFloatingPointTy()) {
                    builder.CreateRet(llvm::ConstantFP::get(return_type, 0.0));
                } else {
                    builder.CreateRet(llvm::ConstantPointerNull::get(llvm::cast<llvm::PointerType>(return_type)));
                }
            }
        }
    } else {
        // No body or no stmt_codegen - generate placeholder return
        if (return_type->isVoidTy()) {
            builder.CreateRetVoid();
        } else {
            builder.CreateRet(llvm::ConstantInt::get(return_type, 0));
        }
    }
    
    // Restore insertion point and named_values (return to outer scope)
    if (saved_insert_block) {
        builder.SetInsertPoint(saved_insert_block);
    }
    named_values = saved_named_values;
    
    // ========================================================================
    // STEP 3: CREATE FAT POINTER STRUCT
    // ========================================================================
    
    // Define fat pointer struct type: { i8* method_ptr, i8* env_ptr }
    std::vector<llvm::Type*> fat_ptr_fields = {
        llvm::PointerType::get(context, 0),  // method_ptr (function pointer as i8*)
        llvm::PointerType::get(context, 0)   // env_ptr (environment pointer as i8*)
    };
    llvm::StructType* fat_ptr_type = llvm::StructType::create(context, fat_ptr_fields, "func_fat_ptr");
    
    // Allocate fat pointer on stack
    llvm::Value* fat_ptr_alloca = builder.CreateAlloca(fat_ptr_type, nullptr, "fat_ptr");
    
    // Store method_ptr (function pointer)
    llvm::Value* method_ptr_field = builder.CreateStructGEP(fat_ptr_type, fat_ptr_alloca, 0, "method_ptr_field");
    llvm::Value* func_ptr_as_i8 = builder.CreateBitCast(lambda_func, llvm::PointerType::get(context, 0));
    builder.CreateStore(func_ptr_as_i8, method_ptr_field);
    
    // Store env_ptr (environment pointer or NULL)
    llvm::Value* env_ptr_field = builder.CreateStructGEP(fat_ptr_type, fat_ptr_alloca, 1, "env_ptr_field");
    if (env_alloca) {
        // We have captured variables, store environment pointer
        llvm::Value* env_ptr_as_i8 = builder.CreateBitCast(env_alloca, llvm::PointerType::get(context, 0));
        builder.CreateStore(env_ptr_as_i8, env_ptr_field);
    } else {
        // No captures, store NULL
        llvm::Value* null_ptr = llvm::ConstantPointerNull::get(llvm::PointerType::get(context, 0));
        builder.CreateStore(null_ptr, env_ptr_field);
    }
    
    // Return fat pointer (as struct value, not pointer)
    // Load the struct from stack
    llvm::Value* fat_ptr_value = builder.CreateLoad(fat_ptr_type, fat_ptr_alloca, "fat_ptr_val");
    
    return fat_ptr_value;
}

// ============================================================================
// SPECIAL OPERATORS - FUTURE IMPLEMENTATION NOTES
// ============================================================================
//
// The following special operators from research_026 require additional language
// features to be fully implemented:
//
// 1. Unwrap Operator (?) - Postfix unary operator
//    - Requires: result<T> type implementation
//    - Purpose: Early return on error, monadic bind operation
//    - Will be implemented with: Phase 4.5+ (Result type support)
//
// 2. Safe Navigation Operator (?.)
//    - Requires: Null pointer tracking, optional types
//    - Purpose: Safe member/method access with null propagation
//    - Implementation: Branching control flow similar to ternary
//    - Will be implemented with: Phase 4.4+ (Struct/member access) + null handling
//
// 3. Null Coalescing Operator (??)
//    - Requires: Null value representation, optional types
//    - Purpose: Provide default value when expression is null/ERR
//    - Implementation: Similar to ternary with null check
//    - Will be implemented with: Phase 4.4+ (null handling)
//
// 4. Pipeline Operators (|>, <|)
//    - Forward pipeline (|>): data |> func desugars to func(data)
//    - Reverse pipeline (<|): func <| data desugars to func(data)
//    - Requires: AST transformation during parsing (desugaring)
//    - Will be implemented with: Phase 2+ (Parser enhancement for operator desugaring)
//
// 5. Range Operators (.., ...)
//    - Inclusive range (..): start..end includes both boundaries
//    - Exclusive range (...): start...end excludes end
//    - Requires: Range type implementation, iterator support
//    - Will be implemented with: Phase 4.7+ (Range and iterator support)
//
// Note: The ternary operator (is ? :) has been implemented in Phase 4.2.6
//       as it only requires basic control flow without additional type system
//       features.
//
// ============================================================================

// ============================================================================
// Phase 4.5.3: Await Expression Code Generation (Coroutine Suspension)
// ============================================================================

/**
 * Generate code for await expression
 * 
 * Creates a suspension point in the coroutine where execution can be paused
 * and later resumed. The LLVM coroutine transformation pass will split this
 * into a state machine.
 * 
 * Algorithm (from research_029):
 * 1. Evaluate the operand (expression yielding Future<T>)
 * 2. Call @llvm.coro.save to capture coroutine state
 * 3. Call @llvm.coro.suspend with save token
 * 4. Switch on suspend result:
 *    - 0 (resume): Continue to resume block
 *    - 1 (destroy): Jump to cleanup
 * 5. Resume block: Extract value from Future and continue
 * 
 * Example Aria code:
 *   async func:fetchData = int64() {
 *       int64:result = await someAsyncOp();
 *       pass(result);
 *   };
 * 
 * Generated LLVM IR:
 *   %future = call i8* @someAsyncOp()
 *   %save = call token @llvm.coro.save(i8* %handle)
 *   %suspend = call i8 @llvm.coro.suspend(token %save, i1 false)
 *   switch i8 %suspend, label %suspend.resume [
 *     i8 1, label %coro.cleanup
 *   ]
 * suspend.resume:
 *   %result = ... extract value from future ...
 *   ; continue execution
 * 
 * Reference: research_029 Section 5 (Compiler Lowering & State Machine)
 */
llvm::Value* ExprCodegen::codegenAwait(ASTNode* node) {
    AwaitExpr* expr = static_cast<AwaitExpr*>(node);
    
    // Check that we're in an async function context
    llvm::Function* current_func = builder.GetInsertBlock()->getParent();
    if (!current_func) {
        throw std::runtime_error("await outside of function context");
    }
    
    // TODO: Add runtime check that current function is async
    // For now, semantic analysis ensures this
    
    // Step 1: Evaluate the operand (should return a Future or coroutine handle)
    llvm::Value* future_value = codegenExpressionNode(expr->operand.get(), this);
    
    // For now, assume operand returns i8* (coroutine handle)
    // TODO: Proper Future<T> type handling when we implement the trait system
    
    // Step 2: Get coroutine intrinsics
    llvm::Function* coro_save = llvm::Intrinsic::getDeclaration(
        module,
        llvm::Intrinsic::coro_save
    );
    
    llvm::Function* coro_suspend = llvm::Intrinsic::getDeclaration(
        module,
        llvm::Intrinsic::coro_suspend
    );
    
    llvm::Function* coro_resume = llvm::Intrinsic::getDeclaration(
        module,
        llvm::Intrinsic::coro_resume
    );
    
    // Step 3: If the operand is a coroutine handle (i8*), resume it
    if (future_value->getType()->isPointerTy()) {
        // Resume the awaited coroutine
        builder.CreateCall(coro_resume, {future_value});
    }
    
    // Get the current coroutine handle
    // LLVM's coroutine pass will insert this properly during transformation
    // For now, we use the awaited handle as placeholder
    llvm::Value* current_handle = future_value;
    
    // Step 4: Save state and suspend
    llvm::Value* save_token = builder.CreateCall(coro_save, {current_handle}, "await.save");
    
    // Suspend (not final - this is an intermediate suspension point)
    llvm::Value* is_final = llvm::ConstantInt::get(llvm::Type::getInt1Ty(context), 0);
    llvm::Value* suspend_result = builder.CreateCall(
        coro_suspend,
        {save_token, is_final},
        "await.suspend"
    );
    
    // Step 5: Create resume and cleanup blocks
    llvm::BasicBlock* resume_block = llvm::BasicBlock::Create(
        context,
        "await.resume",
        current_func
    );
    
    llvm::BasicBlock* cleanup_block = llvm::BasicBlock::Create(
        context,
        "await.cleanup",
        current_func
    );
    
    // Step 6: Switch on suspend result
    // 0 = resume (continue execution)
    // 1 = destroy (cleanup)
    llvm::SwitchInst* suspend_switch = builder.CreateSwitch(suspend_result, resume_block, 1);
    suspend_switch->addCase(
        llvm::ConstantInt::get(llvm::Type::getInt8Ty(context), 1),
        cleanup_block
    );
    
    // Step 7: Generate cleanup block (jump to function's main cleanup)
    builder.SetInsertPoint(cleanup_block);
    // TODO: Jump to function's coro.cleanup block when we refactor to share state
    // For now, just create unreachable as LLVM's pass will fix this
    builder.CreateUnreachable();
    
    // Step 8: Generate resume block - continue execution here
    builder.SetInsertPoint(resume_block);
    
    // For now, return the future value itself
    // TODO: Extract actual result from Future<T> when trait system is ready
    // This will involve calling Future::poll() and handling Ready/Pending states
    
    return future_value;
}

// ============================================================================
// Phase 2: Optional Types & Special Operators
// ============================================================================

/**
 * Get LLVM struct type for optional<T>: { i1 hasValue, T value }
 * Uses cached types to avoid recreation
 */
llvm::StructType* ExprCodegen::getOptionalType(llvm::Type* wrappedType) {
    // Create struct type: { i1 hasValue, T value }
    std::vector<llvm::Type*> fields = {
        llvm::Type::getInt1Ty(context),  // hasValue flag
        wrappedType                       // the wrapped value
    };
    
    // Create named struct for debugging
    std::string name = "optional." + std::string(wrappedType->getStructName());
    return llvm::StructType::create(context, fields, name);
}

/**
 * Create an optional value with hasValue=true
 */
llvm::Value* ExprCodegen::createOptionalSome(llvm::Value* value, llvm::Type* wrappedType) {
    llvm::StructType* optType = getOptionalType(wrappedType);
    
    // Create undef struct
    llvm::Value* opt = llvm::UndefValue::get(optType);
    
    // Set hasValue = true
    opt = builder.CreateInsertValue(opt, llvm::ConstantInt::getTrue(context), 0);
    
    // Set value
    opt = builder.CreateInsertValue(opt, value, 1);
    
    return opt;
}

/**
 * Create an optional value with hasValue=false (NIL)
 */
llvm::Value* ExprCodegen::createOptionalNone(llvm::Type* wrappedType) {
    llvm::StructType* optType = getOptionalType(wrappedType);
    
    // Create undef struct
    llvm::Value* opt = llvm::UndefValue::get(optType);
    
    // Set hasValue = false
    opt = builder.CreateInsertValue(opt, llvm::ConstantInt::getFalse(context), 0);
    
    // Value field is undef (won't be accessed)
    
    return opt;
}

/**
 * Check if optional hasValue (is not NIL)
 */
llvm::Value* ExprCodegen::isOptionalSome(llvm::Value* optional) {
    // Extract hasValue field (index 0)
    return builder.CreateExtractValue(optional, 0, "optional.hasValue");
}

/**
 * Extract value from optional (unwrap)
 * Note: Caller must check hasValue first!
 */
llvm::Value* ExprCodegen::unwrapOptional(llvm::Value* optional) {
    // Extract value field (index 1)
    return builder.CreateExtractValue(optional, 1, "optional.value");
}

/**
 * Generate code for vector constructor
 * vec2(x, y), vec3(x, y, z), vec9(c0, ..., c8)
 */
llvm::Value* ExprCodegen::codegenVectorConstructor(VectorConstructorExpr* expr) {
    if (!expr) {
        throw std::runtime_error("Null vector constructor expression");
    }
    
    int dimension = expr->dimension;
    
    // Generate code for all components
    std::vector<llvm::Value*> componentValues;
    llvm::Type* componentType = nullptr;
    
    for (const auto& comp : expr->components) {
        llvm::Value* val = codegenExpressionNode(comp.get(), this);
        if (!val) {
            throw std::runtime_error("Failed to generate code for vector component");
        }
        componentValues.push_back(val);
        
        // Get component type from first element
        if (!componentType) {
            componentType = val->getType();
        }
    }
    
    // For vec2 and vec3: Use LLVM FixedVectorType (SIMD)
    if (dimension == 2 || dimension == 3) {
        llvm::Type* vecType = llvm::FixedVectorType::get(componentType, dimension);
        
        // Start with undef vector
        llvm::Value* vec = llvm::UndefValue::get(vecType);
        
        // Insert each component
        for (int i = 0; i < dimension; ++i) {
            vec = builder.CreateInsertElement(vec, componentValues[i], i, "vec.insert");
        }
        
        return vec;
    }
    
    // For vec9: Use struct with 9 components
    if (dimension == 9) {
        std::vector<llvm::Type*> componentTypes(9, componentType);
        llvm::StructType* vec9Type = llvm::StructType::get(context, componentTypes);
        
        // Start with undef struct
        llvm::Value* vec = llvm::UndefValue::get(vec9Type);
        
        // Insert each component
        for (int i = 0; i < 9; ++i) {
            vec = builder.CreateInsertValue(vec, componentValues[i], i, "vec9.insert");
        }
        
        return vec;
    }
    
    throw std::runtime_error("Unsupported vector dimension: " + std::to_string(dimension));
}


