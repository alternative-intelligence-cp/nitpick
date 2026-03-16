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
#include <cmath>  // For ldexp in fix256_from_float

using namespace aria;
using namespace aria::frontend;
using namespace aria::backend;
using namespace aria::sema;

ExprCodegen::ExprCodegen(llvm::LLVMContext& ctx, llvm::IRBuilder<>& bldr,
                         llvm::Module* mod, std::map<std::string, llvm::Value*>& values,
                         std::map<std::string, std::string>& types,
                         sema::TypeSystem* ts)
    : context(ctx), builder(bldr), module(mod), named_values(values),
      var_aria_types(types), type_system(ts), stmt_codegen(nullptr), tbb_codegen(ctx, bldr),
      ternary_codegen(ctx, bldr) {
    // Set module for ternary codegen to declare runtime intrinsics
    ternary_codegen.setModule(mod);
}

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
    
    // Handle user-defined struct types
    // GEMINI BATCH 03 FIX: Struct layout generation (BUG-07-001)
    // Previously: Structs fell through to ARIA-026 ICE (safe but non-functional)
    // Now: Generate proper LLVM struct with correct field order and alignment
    if (type->getKind() == TypeKind::STRUCT) {
        StructType* structType = static_cast<StructType*>(type);
        std::string structName = structType->getName();
        
        // Check cache to handle recursive types (e.g., linked lists)
        // This prevents infinite recursion during type generation
        llvm::StructType* existingStruct = llvm::StructType::getTypeByName(context, structName);
        if (existingStruct) {
            return existingStruct;
        }
        
        // Create opaque struct first to support self-referential types
        // Example: struct Node { next: Node* } needs the type to exist before we can reference it
        llvm::StructType* llvmStruct = llvm::StructType::create(context, structName);
        
        // Build field type list by recursively mapping each field
        std::vector<llvm::Type*> fieldTypes;
        for (const auto& field : structType->getFields()) {
            llvm::Type* fieldLLVMType = getLLVMType(field.type);
            fieldTypes.push_back(fieldLLVMType);
        }
        
        // Set struct body with natural ABI padding (isPacked=false)
        // This ensures platform-consistent alignment without manual padding calculation
        llvmStruct->setBody(fieldTypes, /*isPacked=*/false);
        
        return llvmStruct;
    }
    
    // Handle array types: T[N] -> [N x T]
    if (type->getKind() == TypeKind::ARRAY) {
        ArrayType* arrayType = static_cast<ArrayType*>(type);
        llvm::Type* elementLLVMType = getLLVMType(arrayType->getElementType());
        
        if (arrayType->isFixedSize()) {
            // Fixed-size array: T[N] -> [N x T]
            return llvm::ArrayType::get(elementLLVMType, arrayType->getSize());
        } else {
            // Dynamic array: T[] -> T* (pointer to first element)
            // Dynamic arrays require runtime allocation
            return llvm::PointerType::get(context, 0);
        }
    }
    
    // ARIA-026: SAFETY FIX - Fail hard on non-primitive types instead of defaulting to i32
    // Gemini Safety Audit Fix #1: Type System Breach
    // Risk: 16-byte struct defaulting to 4-byte i32 could cause stack overflow
    if (!type->isPrimitive()) {
        if (type->isPointer()) {
            return llvm::PointerType::get(context, 0);
        }
        throw std::runtime_error("ICE: Code generation encountered unmapped non-primitive type kind: " + 
                                std::to_string(static_cast<int>(type->getKind())) + 
                                ". This is a compiler bug - all types must be explicitly mapped.");
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
    
    // CRITICAL FIX 1: str type must be AriaString struct, not opaque pointer
    // This was causing FFI bug where C functions received metadata address instead of char*
    if (type_name == "str") {
        // str is a fat pointer: { i8* data, i64 length }
        // Return the struct type, not a single pointer
        llvm::StructType* strType = llvm::StructType::getTypeByName(context, "struct.AriaString");
        if (!strType) {
            std::vector<llvm::Type*> fields = {
                llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0),
                llvm::Type::getInt64Ty(context)
            };
            strType = llvm::StructType::create(context, fields, "struct.AriaString");
        }
        
        return strType;
    }
    
    // Other pointer types (not str)
    if (type_name.find('*') != std::string::npos) {
        return llvm::PointerType::get(context, 0);  // LLVM 20.x opaque pointers
    }
    
    // TFP (Twisted Floating Point) types - deterministic floats
    if (type_name == "tfp32") {
        // tfp32: {i16 exp, i16 mant}
        std::vector<llvm::Type*> fields = {
            llvm::Type::getInt16Ty(context),  // exponent (tbb16)
            llvm::Type::getInt16Ty(context)   // mantissa (tbb16)
        };
        return llvm::StructType::get(context, fields);
    }
    if (type_name == "tfp64") {
        // tfp64: {i16 exp, i64 mant}
        // Note: mantissa is i64 but only 48 bits used (tbb48)
        std::vector<llvm::Type*> fields = {
            llvm::Type::getInt16Ty(context),  // exponent (tbb16)
            llvm::Type::getInt64Ty(context)   // mantissa (tbb48 in i64)
        };
        return llvm::StructType::get(context, fields);
    }
    
    // ARIA-026: SAFETY FIX - No silent i32 default for unknown types
    // Gemini Safety Audit Fix #1: Type System Breach (continued)
    throw std::runtime_error("ICE: Code generation encountered unknown primitive type: '" + type_name + 
                            "'. This is a compiler bug - all primitive types must be explicitly mapped.");
}

// Helper: Get LLVM type from Aria type name string
// Complete mapping for all Aria primitive types from specs
llvm::Type* ExprCodegen::getLLVMTypeFromString(const std::string& typeName) {
    // TEMP DEBUG for fix256 investigation
    if (typeName == "fix256" || typeName.find("fix") != std::string::npos) {
        printf("[GET_LLVM_TYPE DEBUG] Called with typeName='%s'\n", typeName.c_str());
        fflush(stdout);
    }
    
    // Integer types
    if (typeName == "int1") return llvm::Type::getInt1Ty(context);
    if (typeName == "int2") return llvm::Type::getInt8Ty(context);   // Smallest LLVM int
    if (typeName == "int4") return llvm::Type::getInt8Ty(context);   // Smallest LLVM int
    if (typeName == "int8"  || typeName == "i8")  return llvm::Type::getInt8Ty(context);
    if (typeName == "int16" || typeName == "i16") return llvm::Type::getInt16Ty(context);
    if (typeName == "int32" || typeName == "i32") return llvm::Type::getInt32Ty(context);
    if (typeName == "int64" || typeName == "i64") return llvm::Type::getInt64Ty(context);

    // Short-form LBIM type names: return plain i128/i256 scalar for literal promotion.
    // These are used by the lexer for type suffixes (u128, i128); variable storage still
    // uses the LBIM struct, but literal generation uses the scalar form.
    if (typeName == "i128" || typeName == "u128") return llvm::Type::getInt128Ty(context);

    // ARIA-024: Large integers use Limb-Based Integral Model (LBIM)
    // Stored as structs of i64 limbs to bypass LLVM IPSCCP/ConstraintElimination bugs
    // See: github.com/llvm/llvm-project/issues/68751, #56351
    if (typeName == "int128" || typeName == "uint128") {
        llvm::StructType* int128Struct = llvm::StructType::getTypeByName(context, "struct.int128");
        if (!int128Struct) {
            llvm::Type* limbArray = llvm::ArrayType::get(llvm::Type::getInt64Ty(context), 2);
            int128Struct = llvm::StructType::create(context, {limbArray}, "struct.int128");
            // CRITICAL FIX 2: Set minimum 16-byte alignment for System V ABI compliance
            // Without this, C functions using movdqa (aligned SIMD) trigger #GP fault
            // Note: This sets the type's natural alignment in the DataLayout
            // Individual allocas will inherit this, but we also force it explicitly in codegen_stmt
        }
        
        return int128Struct;
    }
    if (typeName == "int256" || typeName == "uint256") {
        llvm::StructType* int256Struct = llvm::StructType::getTypeByName(context, "struct.int256");
        if (!int256Struct) {
            llvm::Type* limbArray = llvm::ArrayType::get(llvm::Type::getInt64Ty(context), 4);
            int256Struct = llvm::StructType::create(context, {limbArray}, "struct.int256");
        }
        return int256Struct;
    }
    if (typeName == "int512" || typeName == "uint512") {
        llvm::StructType* int512Struct = llvm::StructType::getTypeByName(context, "struct.int512");
        if (!int512Struct) {
            llvm::Type* limbArray = llvm::ArrayType::get(llvm::Type::getInt64Ty(context), 8);
            int512Struct = llvm::StructType::create(context, {limbArray}, "struct.int512");
        }
        return int512Struct;
    }
    if (typeName == "int1024" || typeName == "uint1024") {
        llvm::StructType* int1024Struct = llvm::StructType::getTypeByName(context, "struct.int1024");
        if (!int1024Struct) {
            llvm::Type* limbArray = llvm::ArrayType::get(llvm::Type::getInt64Ty(context), 16);
            int1024Struct = llvm::StructType::create(context, {limbArray}, "struct.int1024");
        }
        return int1024Struct;
    }
    if (typeName == "int2048" || typeName == "uint2048") {
        llvm::StructType* int2048Struct = llvm::StructType::getTypeByName(context, "struct.int2048");
        if (!int2048Struct) {
            llvm::Type* limbArray = llvm::ArrayType::get(llvm::Type::getInt64Ty(context), 32);
            int2048Struct = llvm::StructType::create(context, {limbArray}, "struct.int2048");
        }
        return int2048Struct;
    }
    if (typeName == "int4096" || typeName == "uint4096") {
        llvm::StructType* int4096Struct = llvm::StructType::getTypeByName(context, "struct.int4096");
        if (!int4096Struct) {
            llvm::Type* limbArray = llvm::ArrayType::get(llvm::Type::getInt64Ty(context), 64);
            int4096Struct = llvm::StructType::create(context, {limbArray}, "struct.int4096");
        }
        return int4096Struct;
    }

    // ARIA-025: fix256 deterministic fixed-point (Q128.128 for physics simulations)
    // See Report 7 § "Deterministic Physics", Report 8 § "fix256 Runtime"
    if (typeName == "fix256") {
        llvm::StructType* fix256Struct = llvm::StructType::getTypeByName(context, "struct.fix256");
        if (!fix256Struct) {
            // fix256 uses 4-limb structure: [2 x i64] integer part + [2 x i64] fractional part
            llvm::Type* limbArray = llvm::ArrayType::get(llvm::Type::getInt64Ty(context), 4);
            fix256Struct = llvm::StructType::create(context, {limbArray}, "struct.fix256");
        }
        return fix256Struct;
    }

    // Unsigned integer types (standard sizes)
    if (typeName == "uint8"  || typeName == "u8")  return llvm::Type::getInt8Ty(context);
    if (typeName == "uint16" || typeName == "u16") return llvm::Type::getInt16Ty(context);
    if (typeName == "uint32" || typeName == "u32") return llvm::Type::getInt32Ty(context);
    if (typeName == "uint64" || typeName == "u64") return llvm::Type::getInt64Ty(context);
    
    // TBB (Twisted Balanced Binary) - symmetric signed with error sentinel
    if (typeName == "tbb8")  return llvm::Type::getInt8Ty(context);
    if (typeName == "tbb16") return llvm::Type::getInt16Ty(context);
    if (typeName == "tbb32") return llvm::Type::getInt32Ty(context);
    if (typeName == "tbb64") return llvm::Type::getInt64Ty(context);
    
    // Floating point types
    if (typeName == "flt32"  || typeName == "f32" || typeName == "float32") return llvm::Type::getFloatTy(context);
    if (typeName == "flt64"  || typeName == "f64" || typeName == "float64") return llvm::Type::getDoubleTy(context);
    if (typeName == "flt128" || typeName == "f128") return llvm::Type::getFP128Ty(context);
    // ARIA-017: Extended precision floats (library-based, stored as limb arrays)
    if (typeName == "flt256") {
        llvm::Type* limbArray = llvm::ArrayType::get(llvm::Type::getInt64Ty(context), 4);
        return llvm::StructType::get(context, {limbArray}, false);
    }
    if (typeName == "flt512") {
        llvm::Type* limbArray = llvm::ArrayType::get(llvm::Type::getInt64Ty(context), 8);
        return llvm::StructType::get(context, {limbArray}, false);
    }
    
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
    if (typeName == "Result") return llvm::PointerType::get(context, 0);
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
    
    // FFI void pointer (C interop) - P3 fix
    if (typeName == "void*" || typeName == "wild void*") {
        return llvm::PointerType::get(context, 0);  // Opaque pointer (i8* equivalent)
    }
    
    // SIMD types: simd<T, N> for explicit vectorization (P1-2 Phase 3)
    // Example: simd<int32, 4> -> <4 x i32>
    //          simd<int64, 8> -> <8 x i64>
    if (typeName.rfind("simd<", 0) == 0 && typeName.back() == '>') {
        // Parse "simd<elementType, laneCount>"
        size_t comma_pos = typeName.find(',');
        if (comma_pos != std::string::npos) {
            // Extract element type: "simd<int32, 4>" -> "int32"
            std::string elem_type_str = typeName.substr(5, comma_pos - 5);
            // Trim whitespace
            elem_type_str.erase(0, elem_type_str.find_first_not_of(" \t"));
            elem_type_str.erase(elem_type_str.find_last_not_of(" \t") + 1);
            
            // Extract lane count: "simd<int32, 4>" -> "4"
            std::string lane_count_str = typeName.substr(comma_pos + 1, typeName.length() - comma_pos - 2);
            // Trim whitespace
            lane_count_str.erase(0, lane_count_str.find_first_not_of(" \t"));
            lane_count_str.erase(lane_count_str.find_last_not_of(" \t") + 1);
            
            // Convert lane count to integer
            size_t lane_count = std::stoull(lane_count_str);
            
            // Recursively get element type
            llvm::Type* elem_type = getLLVMTypeFromString(elem_type_str);
            
            // Create LLVM fixed vector type
            return llvm::FixedVectorType::get(elem_type, lane_count);
        }
    }
    
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

// Helper: Check if type name is a TBB type
static bool isTBBTypeName(const std::string& typeName) {
    return typeName == "tbb8" || typeName == "tbb16" ||
           typeName == "tbb32" || typeName == "tbb64";
}

// Helper: Check if type name is an exotic balanced base type
static bool isExoticTypeName(const std::string& typeName) {
    return typeName == "tryte" || typeName == "nyte" || typeName == "trit" || typeName == "nit";
}

// Helper: Get TBB type name from an expression (returns empty string if not TBB)
std::string ExprCodegen::getExprTBBTypeName(ASTNode* expr) {
    if (!expr) return "";

    // For identifiers, look up in var_aria_types
    if (expr->type == ASTNode::NodeType::IDENTIFIER) {
        IdentifierExpr* ident = static_cast<IdentifierExpr*>(expr);
        auto it = var_aria_types.find(ident->name);
        if (it != var_aria_types.end()) {
            const std::string& typeName = it->second;
            if (isTBBTypeName(typeName)) {
                return typeName;
            }
        }
    }

    // For binary operations on TBB types, result is same TBB type
    if (expr->type == ASTNode::NodeType::BINARY_OP) {
        BinaryExpr* binExpr = static_cast<BinaryExpr*>(expr);
        std::string leftType = getExprTBBTypeName(binExpr->left.get());
        std::string rightType = getExprTBBTypeName(binExpr->right.get());
        if (!leftType.empty() && leftType == rightType) {
            return leftType;
        }
    }

    // For unary operations on TBB types, result is same TBB type
    if (expr->type == ASTNode::NodeType::UNARY_OP) {
        UnaryExpr* unaryExpr = static_cast<UnaryExpr*>(expr);
        return getExprTBBTypeName(unaryExpr->operand.get());
    }

    return "";
}

// Helper: Get exotic type name from an expression (returns empty string if not exotic)
std::string ExprCodegen::getExprExoticTypeName(ASTNode* expr) {
    if (!expr) {
        std::cerr << "[EXOTIC_CHECK] expr is null" << std::endl;
        return "";
    }

    std::cerr << "[EXOTIC_CHECK] expr type = " << static_cast<int>(expr->type) << std::endl;

    // For identifiers, look up in var_aria_types
    if (expr->type == ASTNode::NodeType::IDENTIFIER) {
        IdentifierExpr* ident = static_cast<IdentifierExpr*>(expr);
        std::cerr << "[EXOTIC_CHECK] identifier name = " << ident->name << std::endl;
        auto it = var_aria_types.find(ident->name);
        if (it != var_aria_types.end()) {
            const std::string& typeName = it->second;
            std::cerr << "[DEBUG] Checking identifier '" << ident->name << "' type: " << typeName << std::endl;
            if (isExoticTypeName(typeName)) {
                std::cerr << "[DEBUG] Detected exotic type: " << typeName << std::endl;
                return typeName;
            }
        } else {
            std::cerr << "[EXOTIC_CHECK] identifier '" << ident->name << "' NOT in var_aria_types!" << std::endl;
        }
    }

    // For binary operations on exotic types, result is same exotic type
    if (expr->type == ASTNode::NodeType::BINARY_OP) {
        BinaryExpr* binExpr = static_cast<BinaryExpr*>(expr);
        std::string leftType = getExprExoticTypeName(binExpr->left.get());
        std::string rightType = getExprExoticTypeName(binExpr->right.get());
        if (!leftType.empty() && leftType == rightType) {
            return leftType;
        }
    }

    // For unary operations on exotic types, result is same exotic type
    if (expr->type == ASTNode::NodeType::UNARY_OP) {
        UnaryExpr* unaryExpr = static_cast<UnaryExpr*>(expr);
        return getExprExoticTypeName(unaryExpr->operand.get());
    }

    return "";
}

// Helper: Get numeric type name from an expression (frac*, tfp*, vec9)
std::string ExprCodegen::getExprNumericTypeName(ASTNode* expr) {
    if (!expr) return "";

    // For identifiers, look up in var_aria_types
    if (expr->type == ASTNode::NodeType::IDENTIFIER) {
        IdentifierExpr* ident = static_cast<IdentifierExpr*>(expr);
        auto it = var_aria_types.find(ident->name);
        if (it != var_aria_types.end()) {
            const std::string& typeName = it->second;
            // Check if it's a frac, tfp, or vec9 type
            if (typeName.find("frac") == 0 || typeName.find("tfp") == 0 ||
                typeName == "vec9" || typeName == "dvec9") {
                return typeName;
            }
        }
    }

    // For binary operations on numeric types, result is same numeric type
    if (expr->type == ASTNode::NodeType::BINARY_OP) {
        BinaryExpr* binExpr = static_cast<BinaryExpr*>(expr);
        std::string leftType = getExprNumericTypeName(binExpr->left.get());
        std::string rightType = getExprNumericTypeName(binExpr->right.get());
        if (!leftType.empty() && leftType == rightType) {
            return leftType;
        }
    }

    // For unary operations on numeric types, result is same numeric type
    if (expr->type == ASTNode::NodeType::UNARY_OP) {
        UnaryExpr* unaryExpr = static_cast<UnaryExpr*>(expr);
        return getExprNumericTypeName(unaryExpr->operand.get());
    }

    return "";
}

// Helper: Get LBIM type name for expression (int128, int256, int512, int1024, uint*, fix256)
std::string ExprCodegen::getExprLBIMTypeName(ASTNode* expr) {
    if (!expr) return "";

    // For identifiers, look up in var_aria_types
    if (expr->type == ASTNode::NodeType::IDENTIFIER) {
        IdentifierExpr* ident = static_cast<IdentifierExpr*>(expr);
        auto it = var_aria_types.find(ident->name);
        if (it != var_aria_types.end()) {
            const std::string& typeName = it->second;
            // Check if it's an LBIM type (large integers or fix256)
            if (typeName == "int128" || typeName == "uint128" ||
                typeName == "int256" || typeName == "uint256" ||
                typeName == "int512" || typeName == "uint512" ||
                typeName == "int1024" || typeName == "uint1024" ||
                typeName == "fix256") {
                return typeName;
            }
        }
    }

    // For binary operations on LBIM types, result is same LBIM type
    if (expr->type == ASTNode::NodeType::BINARY_OP) {
        BinaryExpr* binExpr = static_cast<BinaryExpr*>(expr);
        std::string leftType = getExprLBIMTypeName(binExpr->left.get());
        std::string rightType = getExprLBIMTypeName(binExpr->right.get());
        if (!leftType.empty() && leftType == rightType) {
            return leftType;
        }
    }

    // For unary operations on LBIM types, result is same LBIM type
    if (expr->type == ASTNode::NodeType::UNARY_OP) {
        UnaryExpr* unaryExpr = static_cast<UnaryExpr*>(expr);
        return getExprLBIMTypeName(unaryExpr->operand.get());
    }

    return "";
}

// Helper: Get Aria type name for any expression (for type-aware formatting)
static std::string getExprAriaTypeName(ASTNode* expr, const std::map<std::string, std::string>& var_aria_types) {
    if (!expr) return "";

    // For identifiers, look up in var_aria_types
    if (expr->type == ASTNode::NodeType::IDENTIFIER) {
        IdentifierExpr* ident = static_cast<IdentifierExpr*>(expr);
        auto it = var_aria_types.find(ident->name);
        if (it != var_aria_types.end()) {
            return it->second;
        }
    }

    // For binary operations, infer type from operands (left takes precedence)
    if (expr->type == ASTNode::NodeType::BINARY_OP) {
        BinaryExpr* binExpr = static_cast<BinaryExpr*>(expr);
        std::string leftType = getExprAriaTypeName(binExpr->left.get(), var_aria_types);
        if (!leftType.empty()) return leftType;
        return getExprAriaTypeName(binExpr->right.get(), var_aria_types);
    }

    // For unary operations, propagate operand type
    if (expr->type == ASTNode::NodeType::UNARY_OP) {
        UnaryExpr* unaryExpr = static_cast<UnaryExpr*>(expr);
        return getExprAriaTypeName(unaryExpr->operand.get(), var_aria_types);
    }

    return "";
}

// Helper: Get ERR sentinel constant for TBB type
llvm::Value* ExprCodegen::getTBBSentinel(llvm::Type* type) {
    unsigned width = type->getIntegerBitWidth();
    // TBB sentinel is signed minimum: 0b1000...0000
    return llvm::ConstantInt::get(context, llvm::APInt::getSignedMinValue(width));
}

// Helper: Get unknown sentinel constant
llvm::Value* ExprCodegen::getUnknownSentinel(llvm::Type* type) {
    unsigned width = type->getIntegerBitWidth();
    // unknown sentinel is signed maximum: 0b0111...1111
    // Opposite of ERR (min value), keeps them distinct
    return llvm::ConstantInt::get(context, llvm::APInt::getSignedMaxValue(width));
}

// ARIA-018: Sentinel-Preserving TBB Widening
// Implements branchless widening that preserves error sentinels across bit widths
// Pattern: icmp (check source sentinel) → sext (speculative extend) → select (choose result)
// This prevents "sentinel healing" where tbb8 ERR (-128) becomes valid in tbb16
llvm::Value* ExprCodegen::generateTBBWiden(llvm::Value* srcVal, llvm::Type* dstType) {
    llvm::Type* srcType = srcVal->getType();
    unsigned srcWidth = srcType->getIntegerBitWidth();
    unsigned dstWidth = dstType->getIntegerBitWidth();
    
    // 1. Define source and destination sentinels using APInt
    //    getSignedMinValue creates 0b1000...0 (sign bit only)
    llvm::APInt srcSentinelInt = llvm::APInt::getSignedMinValue(srcWidth);
    llvm::APInt dstSentinelInt = llvm::APInt::getSignedMinValue(dstWidth);
    
    llvm::Value* srcSentinel = llvm::ConstantInt::get(context, srcSentinelInt);
    llvm::Value* dstSentinel = llvm::ConstantInt::get(context, dstSentinelInt);
    
    // 2. Emit Branchless Logic (translates to cmov/csel on x86/ARM)
    //    Check: Is source value the ERR sentinel?
    llvm::Value* isErr = builder.CreateICmpEQ(srcVal, srcSentinel, "tbb.is_err");
    
    //    Speculative Extension: Extend the value (this computes the "healed" value)
    //    Safe because sext has no side effects
    llvm::Value* extended = builder.CreateSExt(srcVal, dstType, "tbb.sext");
    
    //    Conditional Selection: If ERR detected, inject destination sentinel
    //    Otherwise use the extended value
    return builder.CreateSelect(isErr, dstSentinel, extended, "tbb.widen");
}

// Helper: Generate intrinsic-based TBB binary operation (branchless using select)
// Implements sticky error propagation following Gemini's recommendation
llvm::Value* ExprCodegen::generateTBBBinaryOp(const std::string& tbbType,
                                               frontend::TokenType op,
                                               llvm::Value* left,
                                               llvm::Value* right) {
    llvm::Type* type = left->getType();
    llvm::Value* sentinel = getTBBSentinel(type);
    
    // CRITICAL FIX: Coerce right operand to match left operand type
    // Integer literals default to i64, but we need matching types for intrinsics
    if (left->getType() != right->getType()) {
        if (left->getType()->isIntegerTy() && right->getType()->isIntegerTy()) {
            llvm::IntegerType* leftIntTy = llvm::cast<llvm::IntegerType>(left->getType());
            llvm::IntegerType* rightIntTy = llvm::cast<llvm::IntegerType>(right->getType());
            
            if (rightIntTy->getBitWidth() > leftIntTy->getBitWidth()) {
                // Right is wider - truncate to match left
                right = builder.CreateTrunc(right, left->getType(), "trunc_right");
            } else if (rightIntTy->getBitWidth() < leftIntTy->getBitWidth()) {
                // Right is narrower - extend to match left
                right = builder.CreateSExt(right, left->getType(), "sext_right");
            }
        }
    }
    
    // Step 1: Input validation - check if either operand is ERR
    llvm::Value* lhsIsErr = builder.CreateICmpEQ(left, sentinel, "lhs_is_err");
    llvm::Value* rhsIsErr = builder.CreateICmpEQ(right, sentinel, "rhs_is_err");
    llvm::Value* inputInvalid = builder.CreateOr(lhsIsErr, rhsIsErr, "input_invalid");
    
    // Step 2: Perform operation with overflow intrinsic
    llvm::Value* mathResult = nullptr;
    llvm::Value* overflow = nullptr;
    
    if (op == frontend::TokenType::TOKEN_PLUS) {
        // Use llvm.sadd.with.overflow for addition
        llvm::Function* saddFunc = llvm::Intrinsic::getDeclaration(
            module,
            llvm::Intrinsic::sadd_with_overflow,
            {type}
        );
        llvm::Value* resStruct = builder.CreateCall(saddFunc, {left, right}, "sadd_ovf");
        mathResult = builder.CreateExtractValue(resStruct, 0, "sum");
        overflow = builder.CreateExtractValue(resStruct, 1, "ovf");
    } else if (op == frontend::TokenType::TOKEN_MINUS) {
        // Use llvm.ssub.with.overflow for subtraction
        llvm::Function* ssubFunc = llvm::Intrinsic::getDeclaration(
            module,
            llvm::Intrinsic::ssub_with_overflow,
            {type}
        );
        llvm::Value* resStruct = builder.CreateCall(ssubFunc, {left, right}, "ssub_ovf");
        mathResult = builder.CreateExtractValue(resStruct, 0, "diff");
        overflow = builder.CreateExtractValue(resStruct, 1, "ovf");
    } else if (op == frontend::TokenType::TOKEN_STAR) {
        // Use llvm.smul.with.overflow for multiplication
        llvm::Function* smulFunc = llvm::Intrinsic::getDeclaration(
            module,
            llvm::Intrinsic::smul_with_overflow,
            {type}
        );
        llvm::Value* resStruct = builder.CreateCall(smulFunc, {left, right}, "smul_ovf");
        mathResult = builder.CreateExtractValue(resStruct, 0, "prod");
        overflow = builder.CreateExtractValue(resStruct, 1, "ovf");
    } else if (op == frontend::TokenType::TOKEN_SLASH) {
        // Division: check for zero divisor AND INT_MIN / -1 overflow (SIGFPE trap)
        llvm::Value* zero = llvm::ConstantInt::get(type, 0);
        llvm::Value* one = llvm::ConstantInt::get(type, 1);
        llvm::Value* minusOne = llvm::ConstantInt::get(type, -1, true);
        
        // Get INT_MIN for this type
        unsigned bitWidth = type->getIntegerBitWidth();
        llvm::APInt intMinValue = llvm::APInt::getSignedMinValue(bitWidth);
        llvm::Value* intMin = llvm::ConstantInt::get(type, intMinValue);
        
        // Check both failure conditions
        llvm::Value* divByZero = builder.CreateICmpEQ(right, zero, "div_by_zero");
        llvm::Value* lhsIsIntMin = builder.CreateICmpEQ(left, intMin, "lhs_is_intmin");
        llvm::Value* rhsIsMinusOne = builder.CreateICmpEQ(right, minusOne, "rhs_is_minus_one");
        llvm::Value* wouldOverflow = builder.CreateAnd(lhsIsIntMin, rhsIsMinusOne, "overflow_case");
        
        // Combine both failure modes
        overflow = builder.CreateOr(divByZero, wouldOverflow, "div_overflow");
        
        // Use safe divisor (1) if either condition true
        llvm::Value* safeRhs = builder.CreateSelect(overflow, one, right, "safe_divisor");
        mathResult = builder.CreateSDiv(left, safeRhs, "quot");
    } else if (op == frontend::TokenType::TOKEN_PERCENT) {
        // Modulo: check for zero divisor
        llvm::Value* zero = llvm::ConstantInt::get(type, 0);
        llvm::Value* one = llvm::ConstantInt::get(type, 1);
        llvm::Value* modByZero = builder.CreateICmpEQ(right, zero, "mod_by_zero");
        llvm::Value* safeRhs = builder.CreateSelect(modByZero, one, right, "safe_divisor");
        mathResult = builder.CreateSRem(left, safeRhs, "rem");
        overflow = modByZero;  // Treat modulo by zero as overflow
    } else {
        // Unsupported operator - fall through
        return nullptr;
    }
    
    // Step 3: Sentinel collision check - if result == sentinel (but inputs were valid)
    llvm::Value* resIsSentinel = builder.CreateICmpEQ(mathResult, sentinel, "res_is_sentinel");
    
    // Step 4: Combine all failure conditions
    llvm::Value* failTemp = builder.CreateOr(inputInvalid, overflow, "fail_temp");
    llvm::Value* failFinal = builder.CreateOr(failTemp, resIsSentinel, "fail_final");
    
    // Step 5: Branchless select - return sentinel if any failure, else return mathResult
    // Compiles to cmov on x86-64, csel on ARM64
    return builder.CreateSelect(failFinal, sentinel, mathResult, "tbb_result");
}

// Generate exotic type binary operation (tryte/nyte)
// These use runtime library functions with biased representation
llvm::Value* ExprCodegen::generateExoticBinaryOp(const std::string& exoticType,
                                                  frontend::TokenType op,
                                                  llvm::Value* left,
                                                  llvm::Value* right) {
    // ARIA SAFETY FIX (Gemini Batch 02, BUG-005/006):
    // Use TernaryCodegen class to generate runtime calls for ALL exotic types
    // This ensures proper Kleene logic for AND/OR and prevents sentinel healing
    
    // Create a temporary PrimitiveType for the exotic type
    // The ternary_codegen only uses type->toString(), so we don't need the full TypeSystem
    sema::PrimitiveType tempType(exoticType);
    
    // Map operator to TernaryCodegen method
    if (op == frontend::TokenType::TOKEN_PLUS) {
        return ternary_codegen.generateAdd(left, right, &tempType);
    } else if (op == frontend::TokenType::TOKEN_MINUS) {
        return ternary_codegen.generateSub(left, right, &tempType);
    } else if (op == frontend::TokenType::TOKEN_STAR) {
        return ternary_codegen.generateMul(left, right, &tempType);
    } else if (op == frontend::TokenType::TOKEN_SLASH) {
        return ternary_codegen.generateDiv(left, right, &tempType);
    } else if (op == frontend::TokenType::TOKEN_PERCENT) {
        return ternary_codegen.generateMod(left, right, &tempType);
    } else if (op == frontend::TokenType::TOKEN_AND_AND) {
        // Logical AND (Kleene logic for trit/nit)
        return ternary_codegen.generateAnd(left, right, &tempType);
    } else if (op == frontend::TokenType::TOKEN_OR_OR) {
        // Logical OR (Kleene logic for trit/nit)
        return ternary_codegen.generateOr(left, right, &tempType);
    } else {
        return nullptr;  // Unsupported operation
    }
}

// Helper: Generate numeric type binary operation (frac*, tfp*, vec9 runtime calls)
llvm::Value* ExprCodegen::generateNumericBinaryOp(const std::string& numericType,
                                                   frontend::TokenType op,
                                                   llvm::Value* left,
                                                   llvm::Value* right) {
    // Map operator to function suffix
    std::string opSuffix;
    if (op == frontend::TokenType::TOKEN_PLUS) {
        opSuffix = "_add";
    } else if (op == frontend::TokenType::TOKEN_MINUS) {
        opSuffix = "_sub";
    } else if (op == frontend::TokenType::TOKEN_STAR) {
        opSuffix = "_mul";
    } else if (op == frontend::TokenType::TOKEN_SLASH) {
        opSuffix = "_div";
    } else {
        return nullptr;  // Unsupported operation
    }
    
    // Build function name: <type>_<op> (e.g., frac32_add, tfp64_mul)
    std::string funcName = numericType + opSuffix;
    
    // Get the LLVM type for this numeric type
    llvm::Type* returnType = left->getType();  // Same type as operands
    
    // Get or create the runtime function
    llvm::FunctionType* funcType = llvm::FunctionType::get(
        returnType,             // return type (same as input)
        {returnType, returnType}, // parameters
        false                   // not variadic
    );
    
    llvm::Function* runtimeFunc = module->getFunction(funcName);
    if (!runtimeFunc) {
        runtimeFunc = llvm::Function::Create(
            funcType,
            llvm::Function::ExternalLinkage,
            funcName,
            module
        );
    }
    
    // Generate the call
    llvm::Value* result = builder.CreateCall(runtimeFunc, {left, right}, numericType + "_result");
    return result;
}

// Helper: Generate LBIM type binary operation (int128/256/512/1024, uint*, fix256)
// ARIA-024: Large integers use runtime library (limb-based arithmetic)
llvm::Value* ExprCodegen::generateLBIMBinaryOp(const std::string& lbimType,
                                                frontend::TokenType op,
                                                llvm::Value* left,
                                                llvm::Value* right) {
    // Map operator to function suffix
    std::string opSuffix;
    bool isComparison = false;
    
    // Arithmetic operators
    if (op == frontend::TokenType::TOKEN_PLUS) {
        opSuffix = "_add";
    } else if (op == frontend::TokenType::TOKEN_MINUS) {
        opSuffix = "_sub";
    } else if (op == frontend::TokenType::TOKEN_STAR) {
        opSuffix = "_mul";
    } else if (op == frontend::TokenType::TOKEN_SLASH) {
        opSuffix = "_div";
    } else if (op == frontend::TokenType::TOKEN_PERCENT) {
        opSuffix = "_mod";
    }
    // Bitwise operators (sign-independent: AND/OR/XOR same for int* and uint*)
    else if (op == frontend::TokenType::TOKEN_AMPERSAND) {
        opSuffix = "_and";
    } else if (op == frontend::TokenType::TOKEN_PIPE) {
        opSuffix = "_or";
    } else if (op == frontend::TokenType::TOKEN_CARET) {
        opSuffix = "_xor";
    }
    // Comparison operators
    else if (op == frontend::TokenType::TOKEN_EQUAL_EQUAL) {
        opSuffix = "_eq";
        isComparison = true;
    } else if (op == frontend::TokenType::TOKEN_BANG_EQUAL) {
        opSuffix = "_eq";  // Will negate result
        isComparison = true;
    } else if (op == frontend::TokenType::TOKEN_LESS ||
               op == frontend::TokenType::TOKEN_LESS_EQUAL ||
               op == frontend::TokenType::TOKEN_GREATER ||
               op == frontend::TokenType::TOKEN_GREATER_EQUAL) {
        opSuffix = "_cmp";  // Comparison returns -1/0/1
        isComparison = true;
    } else {
        return nullptr;  // Unsupported operation
    }
    
    // Build function name based on type
    std::string funcName;
    
    // fix256 uses aria_fix256_* functions (deterministic fixed-point)
    if (lbimType == "fix256") {
        funcName = "aria_fix256" + opSuffix;
    }
    // Signed LBIM integers use aria_lbim_s* functions (signed division, etc.)
    else if (lbimType.find("int") == 0) {  // int128, int256, int512, int1024
        // Extract bit width: int1024 → 1024
        std::string bitWidth = lbimType.substr(3);
        
        // Use signed functions for division/modulo, unsigned for add/sub/mul
        if (op == frontend::TokenType::TOKEN_SLASH) {
            funcName = "aria_lbim_sdiv" + bitWidth;
        } else if (op == frontend::TokenType::TOKEN_PERCENT) {
            funcName = "aria_lbim_smod" + bitWidth;
        } else if (opSuffix == "_cmp") {
            // Signed comparison
            funcName = "aria_lbim_scmp" + bitWidth;
        } else if (opSuffix == "_eq") {
            // Equality works same for signed/unsigned
            funcName = "aria_lbim_eq" + bitWidth;
        } else {
            funcName = "aria_lbim" + opSuffix + bitWidth;
        }
    }
    // Unsigned LBIM integers use aria_lbim_u* for div/mod
    else if (lbimType.find("uint") == 0) {  // uint128, uint256, uint512, uint1024
        std::string bitWidth = lbimType.substr(4);
        
        if (op == frontend::TokenType::TOKEN_SLASH) {
            funcName = "aria_lbim_udiv" + bitWidth;
        } else if (op == frontend::TokenType::TOKEN_PERCENT) {
            funcName = "aria_lbim_umod" + bitWidth;
        } else if (opSuffix == "_cmp") {
            // Unsigned comparison
            funcName = "aria_lbim_ucmp" + bitWidth;
        } else if (opSuffix == "_eq") {
            // Equality works same for signed/unsigned
            funcName = "aria_lbim_eq" + bitWidth;
        } else {
            funcName = "aria_lbim" + opSuffix + bitWidth;
        }
    }
    else {
        return nullptr;  // Unknown LBIM type
    }
    
    // Get the struct type for this LBIM type
    llvm::Type* structType = left->getType();
    
    // Determine return type and function signature
    llvm::Type* returnType;
    if (isComparison) {
        // Comparison operators return bool (i1) or int32
        if (opSuffix == "_eq") {
            returnType = llvm::Type::getInt1Ty(context);  // bool for equality
        } else {  // _cmp
            returnType = llvm::Type::getInt32Ty(context);  // int32 for three-way comparison
        }
    } else {
        // Arithmetic operators return the same struct type
        returnType = structType;
    }
    
    // Get or create the runtime function
    // Signature: returnType func(struct.intN a, struct.intN b)
    llvm::FunctionType* funcType = llvm::FunctionType::get(
        returnType,              // return type (struct for arithmetic, i1/i32 for comparison)
        {structType, structType}, // parameters (both structs)
        false                    // not variadic
    );
    
    llvm::Function* runtimeFunc = module->getFunction(funcName);
    if (!runtimeFunc) {
        runtimeFunc = llvm::Function::Create(
            funcType,
            llvm::Function::ExternalLinkage,
            funcName,
            module
        );
    }
    
    // Generate the call
    llvm::Value* result = builder.CreateCall(runtimeFunc, {left, right}, lbimType + "_result");
    
    // Post-process comparison results
    if (isComparison) {
        if (op == frontend::TokenType::TOKEN_BANG_EQUAL) {
            // != : negate the equality result
            result = builder.CreateNot(result, "ne_result");
        } else if (op == frontend::TokenType::TOKEN_LESS) {
            // < : cmp_result < 0
            llvm::Value* zero = llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0);
            result = builder.CreateICmpSLT(result, zero, "lt_result");
        } else if (op == frontend::TokenType::TOKEN_LESS_EQUAL) {
            // <= : cmp_result <= 0
            llvm::Value* zero = llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0);
            result = builder.CreateICmpSLE(result, zero, "le_result");
        } else if (op == frontend::TokenType::TOKEN_GREATER) {
            // > : cmp_result > 0
            llvm::Value* zero = llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0);
            result = builder.CreateICmpSGT(result, zero, "gt_result");
        } else if (op == frontend::TokenType::TOKEN_GREATER_EQUAL) {
            // >= : cmp_result >= 0
            llvm::Value* zero = llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0);
            result = builder.CreateICmpSGE(result, zero, "ge_result");
        }
        // TOKEN_EQUAL_EQUAL returns the bool directly
    }
    
    return result;
}

// Helper: Promote int64 literal to LBIM struct type (int128/256/512/1024/2048/4096)
// Used when assigning literals to large integer variables
llvm::Value* ExprCodegen::promoteLiteralToLBIM(llvm::Value* literal, llvm::Type* targetType) {
    // Verify target is a struct type
    if (!targetType->isStructTy()) {
        return literal;  // Not an LBIM type, no promotion needed
    }
    
    llvm::StructType* structType = llvm::cast<llvm::StructType>(targetType);
    std::string structName = structType->getName().str();
    
    // Check if this is an LBIM struct (int128, int256, etc.)
    if (structName.find("struct.int") != 0 && structName.find("struct.uint") != 0) {
        return literal;  // Not an LBIM type
    }
    
    // Extract the literal value as int64
    if (!literal->getType()->isIntegerTy()) {
        std::cerr << "[ERROR] promoteLiteralToLBIM: literal is not an integer type" << std::endl;
        return literal;
    }
    
    // Determine the number of limbs based on struct name
    int numLimbs = 0;
    if (structName.find("128") != std::string::npos) numLimbs = 2;
    else if (structName.find("256") != std::string::npos) numLimbs = 4;
    else if (structName.find("512") != std::string::npos) numLimbs = 8;
    else if (structName.find("1024") != std::string::npos) numLimbs = 16;
    else if (structName.find("2048") != std::string::npos) numLimbs = 32;
    else if (structName.find("4096") != std::string::npos) numLimbs = 64;
    else {
        std::cerr << "[ERROR] promoteLiteralToLBIM: unknown LBIM type " << structName << std::endl;
        return literal;
    }
    
    std::cerr << "[LBIM_PROMOTE] Promoting literal to " << structName << " (" << numLimbs << " limbs)" << std::endl;

    // Wide-path: for i128+ inputs, split into 64-bit limbs via shift+trunc
    if (literal->getType()->getIntegerBitWidth() > 64) {
        llvm::Value* wideAlloca = builder.CreateAlloca(targetType, nullptr, "lbim_wide");
        llvm::Type* i64TypeW = llvm::Type::getInt64Ty(context);
        unsigned inBits = literal->getType()->getIntegerBitWidth();
        for (int i = 0; i < numLimbs; i++) {
            llvm::Value* limbVal;
            if ((unsigned)i * 64 < inBits) {
                if (i == 0) {
                    limbVal = builder.CreateTrunc(literal, i64TypeW, "wide_lo");
                } else {
                    llvm::Value* shifted = builder.CreateLShr(
                        literal,
                        llvm::ConstantInt::get(literal->getType(), (uint64_t)i * 64),
                        "wide_shift");
                    limbVal = builder.CreateTrunc(shifted, i64TypeW, "wide_limb");
                }
            } else {
                limbVal = llvm::ConstantInt::get(i64TypeW, 0);
            }
            std::vector<llvm::Value*> wideIdx = {
                llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0),
                llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0),
                llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), i)
            };
            llvm::Value* limbPtr = builder.CreateGEP(targetType, wideAlloca, wideIdx, "wide_limb_ptr");
            builder.CreateStore(limbVal, limbPtr);
        }
        return builder.CreateLoad(targetType, wideAlloca, "lbim_wide_val");
    }

    // Create a zero-initialized struct on the stack
    llvm::Value* structAlloca = builder.CreateAlloca(targetType, nullptr, "lbim_promoted");
    
    // Initialize all limbs to 0
    llvm::Type* i64Type = llvm::Type::getInt64Ty(context);
    llvm::Value* zero = llvm::ConstantInt::get(i64Type, 0);
    
    for (int i = 0; i < numLimbs; i++) {
        // Get pointer to limbs[i]: GEP into struct, then into array, then index i
        std::vector<llvm::Value*> indices = {
            llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0),  // struct index
            llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0),  // array field index
            llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), i)   // limb index
        };
        llvm::Value* limbPtr = builder.CreateGEP(targetType, structAlloca, indices, "limb_ptr");
        builder.CreateStore(zero, limbPtr);
    }
    
    // Convert literal to i64 (sign-extend if necessary)
    llvm::Value* literalI64 = literal;
    if (literal->getType() != i64Type) {
        if (literal->getType()->getIntegerBitWidth() < 64) {
            // Sign-extend for signed types
            literalI64 = builder.CreateSExt(literal, i64Type, "literal_i64");
        } else {
            // Truncate if somehow larger
            literalI64 = builder.CreateTrunc(literal, i64Type, "literal_i64");
        }
    }
    
    // Store the literal value in limbs[0] (LSB)
    std::vector<llvm::Value*> indices0 = {
        llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0),  // struct index
        llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0),  // array field index  
        llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0)   // limb 0
    };
    llvm::Value* limb0Ptr = builder.CreateGEP(targetType, structAlloca, indices0, "limb0_ptr");
    builder.CreateStore(literalI64, limb0Ptr);
    
    // If the literal is negative (signed), sign-extend by filling high limbs with all 1s
    if (structName.find("uint") == 0) {
        // Unsigned type - already zero-extended, done
        llvm::Value* result = builder.CreateLoad(targetType, structAlloca, "lbim_promoted_val");
        return result;
    }
    
    // Signed type - check if negative and sign-extend
    llvm::Value* isNegative = builder.CreateICmpSLT(
        literalI64,
        llvm::ConstantInt::get(i64Type, 0),
        "is_negative"
    );
    
    // Create blocks for sign extension
    llvm::Function* func = builder.GetInsertBlock()->getParent();
    llvm::BasicBlock* signExtendBB = llvm::BasicBlock::Create(context, "sign_extend", func);
    llvm::BasicBlock* continueBB = llvm::BasicBlock::Create(context, "continue", func);
    
    builder.CreateCondBr(isNegative, signExtendBB, continueBB);
    
    // Sign extend block: fill high limbs with 0xFFFFFFFFFFFFFFFF
    builder.SetInsertPoint(signExtendBB);
    llvm::Value* allOnes = llvm::ConstantInt::get(i64Type, 0xFFFFFFFFFFFFFFFFULL);
    for (int i = 1; i < numLimbs; i++) {
        std::vector<llvm::Value*> indices = {
            llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0),
            llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0),
            llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), i)
        };
        llvm::Value* limbPtr = builder.CreateGEP(targetType, structAlloca, indices, "limb_ptr");
        builder.CreateStore(allOnes, limbPtr);
    }
    builder.CreateBr(continueBB);
    
    // Continue block: load and return the struct
    builder.SetInsertPoint(continueBB);
    llvm::Value* result = builder.CreateLoad(targetType, structAlloca, "lbim_promoted_val");
    return result;
}

// Helper: Check if type needs special formatting (TBB, exotic, or LBIM)
static bool needsSpecialFormatter(const std::string& typeName) {
    // TBB types
    if (typeName == "tbb8" || typeName == "tbb16" ||
        typeName == "tbb32" || typeName == "tbb64") return true;

    // Exotic types (balanced ternary/nonary)
    if (typeName == "trit" || typeName == "tryte" ||
        typeName == "nit" || typeName == "nyte") return true;

    // LBIM large integers
    if (typeName == "int128" || typeName == "uint128" ||
        typeName == "int256" || typeName == "uint256" ||
        typeName == "int512" || typeName == "uint512" ||
        typeName == "int1024" || typeName == "uint1024" ||
        typeName == "int2048" || typeName == "uint2048" ||
        typeName == "int4096" || typeName == "uint4096") return true;

    // Fixed-point deterministic types
    if (typeName == "fix256") return true;

    return false;
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
    std::cerr << "[DEBUG] codegenLiteral variant index: " << expr->value.index() << std::endl;
    if (std::holds_alternative<std::string>(expr->value)) {
        std::cerr << "[DEBUG] codegenLiteral string value: " << std::get<std::string>(expr->value) << std::endl;
    }
    
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

        // BUG-FIX (Zero Implicit Conversion): Honor explicit type suffix first.
        // Without this, -1i64 in a function call arg becomes i32(-1) which then
        // gets zero-extended to 0x00000000ffffffff at the FFI boundary.
        if (expr->hasExplicitType()) {
            const std::string& et = expr->getExplicitType();
            llvm::Type* explicit_llvm_type = getLLVMTypeFromString(et);
            if (explicit_llvm_type && explicit_llvm_type->isIntegerTy()) {
                unsigned bits = explicit_llvm_type->getIntegerBitWidth();
                // Signed if suffix starts with 'i', "int", or "tbb"; unsigned if 'u'
                bool is_signed = !et.empty() &&
                                 (et[0] == 'i' ||
                                  et.substr(0, 3) == "int" ||
                                  et.substr(0, 3) == "tbb");
                return llvm::ConstantInt::get(context,
                    llvm::APInt(bits, static_cast<uint64_t>(val), is_signed));
            }
        }

        // Range-based fallback for untyped (legacy) literals.
        // Default to i32 for small values, i64 for larger.
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
    
    // String literal - create global AriaString struct
    if (std::holds_alternative<std::string>(expr->value)) {
        const std::string& str = std::get<std::string>(expr->value);
        std::cerr << "[DEBUG] String literal detected: \"" << str << "\"" << std::endl;
        
        // Check for special literals: ERR and unknown
        // These are represented as string literals by the parser but need special handling
        if (str == "ERR") {
            // ERR sentinel - need to know target type from context
            // For now, default to int32 (will be handled better with type inference)
            llvm::Type* target_type = llvm::Type::getInt32Ty(context);
            std::cerr << "[DEBUG] ERR literal - generating sentinel for type: ";
            target_type->print(llvm::errs());
            std::cerr << std::endl;
            return getTBBSentinel(target_type);
        }
        
        if (str == "unknown") {
            // unknown sentinel - similar to ERR but uses max value instead of min
            llvm::Type* target_type = llvm::Type::getInt32Ty(context);
            std::cerr << "[DEBUG] unknown literal - generating sentinel for type: ";
            target_type->print(llvm::errs());
            std::cerr << std::endl;
            llvm::Value* sentinel = getUnknownSentinel(target_type);
            std::cerr << "[DEBUG] unknown sentinel generated, result type: ";
            sentinel->getType()->print(llvm::errs());
            std::cerr << std::endl;
            return sentinel;
        }
        
        // ARIA-026: String interning - check pool before creating new global
        // Gemini Safety Audit Fix #5: Prevents OOM from duplicate literals
        llvm::GlobalVariable* str_gv = nullptr;
        auto pool_it = string_pool_.find(str);
        if (pool_it != string_pool_.end()) {
            // Reuse existing string global
            str_gv = pool_it->second;
            std::cerr << "[STRING POOL] Reusing pooled string: \"" << str << "\"" << std::endl;
        } else {
            // Create new string global and add to pool
            llvm::Constant* str_data = llvm::ConstantDataArray::getString(context, str, true);
            str_gv = new llvm::GlobalVariable(
                *module,
                str_data->getType(),
                true,  // isConstant
                llvm::GlobalValue::PrivateLinkage,
                str_data,
                ".str.data"
            );
            string_pool_[str] = str_gv;
            std::cerr << "[STRING POOL] Created new pooled string: \"" << str << "\"" << std::endl;
        }
        
        // Get or create AriaString struct type
        llvm::StructType* aria_string_type = llvm::StructType::getTypeByName(context, "struct.AriaString");
        if (!aria_string_type) {
            std::vector<llvm::Type*> fields = {
                llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0),
                llvm::Type::getInt64Ty(context)
            };
            aria_string_type = llvm::StructType::create(context, fields, "struct.AriaString");
        }
        
        // Create a global AriaString struct constant
        std::vector<llvm::Constant*> struct_values = {
            llvm::ConstantExpr::getPointerCast(str_gv, llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0)),
            builder.getInt64(str.length())
        };
        llvm::Constant* string_struct = llvm::ConstantStruct::get(aria_string_type, struct_values);
        
        llvm::GlobalVariable* string_gv = new llvm::GlobalVariable(
            *module,
            aria_string_type,
            true,  // isConstant
            llvm::GlobalValue::PrivateLinkage,
            string_struct,
            ".str"
        );
        
        // Return pointer to the global AriaString struct
        return string_gv;
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
    
    // ARIA-026: SAFETY FIX - Handle both AllocaInst AND GlobalVariable
    // Gemini Safety Audit Fix #2: Global Variable Pointer Corruption
    // Risk: Global variables were returning pointer address instead of value
    // Example: MAX_FORCE = 10 could become ~93 trillion (pointer address)
    
    bool is_stack_var = llvm::isa<llvm::AllocaInst>(var_ptr);
    bool is_global_var = llvm::isa<llvm::GlobalVariable>(var_ptr);
    
    if (is_stack_var || is_global_var) {
        // Determine the type to load
        llvm::Type* loadType = nullptr;
        
        if (is_stack_var) {
            llvm::AllocaInst* alloca = llvm::cast<llvm::AllocaInst>(var_ptr);
            loadType = alloca->getAllocatedType();
        } else {
            // GlobalVariable - look up type from var_aria_types
            auto typeIt = var_aria_types.find(expr->name);
            if (typeIt != var_aria_types.end()) {
                loadType = getLLVMTypeFromString(typeIt->second);
            } else {
                // Fallback: try to get type from GlobalVariable itself
                llvm::GlobalVariable* gv = llvm::cast<llvm::GlobalVariable>(var_ptr);
                loadType = gv->getValueType();
            }
        }
        
        // Create a load instruction
        llvm::Value* loaded = builder.CreateLoad(loadType, var_ptr, expr->name);
        std::cerr << "[DEBUG] codegenIdentifier: " << expr->name << " -> type: ";
        loaded->getType()->print(llvm::errs());
        std::cerr << std::endl;
        return loaded;
    }
    
    // If it's not an alloca or global, return the value directly
    // (could be a function parameter or SSA value)  
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
        // ARIA-026: Use string pool for template literal parts too
        llvm::GlobalVariable* gv = nullptr;
        auto pool_it = string_pool_.find(str);
        if (pool_it != string_pool_.end()) {
            gv = pool_it->second;
        } else {
            llvm::Constant* strConstant = llvm::ConstantDataArray::getString(context, str, true);
            gv = new llvm::GlobalVariable(
                *module,
                strConstant->getType(),
                true,
                llvm::GlobalValue::PrivateLinkage,
                strConstant,
                ".str"
            );
            string_pool_[str] = gv;
        }
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
    
    // ARIA-026: SAFETY FIX - Use deterministic aria_int64_to_str instead of sprintf
    // Gemini Safety Audit Fix #3: Non-Deterministic Serialization
    // Risk: sprintf is locale-dependent, violates bit-identical requirement for AGI logs
    // Helper function: Convert int64 to string using deterministic runtime
    auto int64ToString = [this](llvm::Value* intVal) -> llvm::Value* {
        // ARIA-026 FIX: Use GC heap instead of stack to prevent use-after-return
        // Stack buffers are deallocated on function return, creating dangling pointers
        // if these helpers are ever used in return statements or stored in variables
        
        // Allocate 24-byte buffer on GC heap (sufficient for "-9223372036854775808\0")
        llvm::FunctionCallee gcAllocCallee = module->getOrInsertFunction("aria_gc_alloc",
            llvm::FunctionType::get(
                llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0),
                {llvm::Type::getInt64Ty(context)},
                false
            )
        );
        llvm::Function* gcAlloc = llvm::cast<llvm::Function>(gcAllocCallee.getCallee());
        llvm::Value* bufferSize = llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), 24);
        llvm::Value* bufferPtr = builder.CreateCall(gcAlloc, { bufferSize }, "int_str_gc_buffer");
        
        // Null check for allocation failure
        llvm::BasicBlock* currentBB = builder.GetInsertBlock();
        llvm::Function* currentFunc = currentBB->getParent();
        llvm::BasicBlock* nullBB = llvm::BasicBlock::Create(context, "int_str_null", currentFunc);
        llvm::BasicBlock* validBB = llvm::BasicBlock::Create(context, "int_str_valid", currentFunc);
        
        llvm::Value* isNull = builder.CreateICmpEQ(
            bufferPtr,
            llvm::ConstantPointerNull::get(llvm::PointerType::get(context, 0)),
            "is_null"
        );
        builder.CreateCondBr(isNull, nullBB, validBB);
        
        // Null path: panic
        builder.SetInsertPoint(nullBB);
        llvm::Function* panicOOM = module->getFunction("aria_panic_oom");
        if (!panicOOM) {
            llvm::FunctionType* panicType = llvm::FunctionType::get(
                llvm::Type::getVoidTy(context),
                { llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0) },
                false
            );
            panicOOM = llvm::Function::Create(
                panicType,
                llvm::Function::ExternalLinkage,
                "aria_panic_oom",
                module
            );
        }
        llvm::Value* panicMsg = builder.CreateGlobalStringPtr(
            "Out of memory in int64 to string conversion",
            "int_oom_msg"
        );
        builder.CreateCall(panicOOM, { panicMsg });
        builder.CreateUnreachable();
        
        // Valid path: continue
        builder.SetInsertPoint(validBB);
        
        // Declare aria_int64_to_str (deterministic, locale-independent runtime function)
        // Signature: int64_t aria_int64_to_str(int64_t value, char* buffer)
        // Returns: length of resulting string (excluding null terminator)
        llvm::Function* toStrFn = module->getFunction("aria_int64_to_str");
        if (!toStrFn) {
            llvm::FunctionType* fnType = llvm::FunctionType::get(
                llvm::Type::getInt64Ty(context),  // Returns length
                { llvm::Type::getInt64Ty(context), llvm::PointerType::get(context, 0) },
                false  // Not varargs (fixed signature for safety)
            );
            toStrFn = llvm::Function::Create(
                fnType,
                llvm::Function::ExternalLinkage,
                "aria_int64_to_str",
                module
            );
        }
        
        // Call aria_int64_to_str(intVal, buffer)
        // Returns length directly - no need for strlen()
        llvm::Value* length = builder.CreateCall(toStrFn, { intVal, bufferPtr });
        
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
    
    // Helper function: Convert float to string using locale-independent formatting
    auto floatToString = [this](llvm::Value* floatVal) -> llvm::Value* {
        // ARIA-026 FIX: Use GC heap instead of stack to prevent use-after-return
        // Allocate 64-byte buffer on GC heap (sufficient for floating point)
        llvm::FunctionCallee gcAllocCallee = module->getOrInsertFunction("aria_gc_alloc",
            llvm::FunctionType::get(
                llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0),
                {llvm::Type::getInt64Ty(context)},
                false
            )
        );
        llvm::Function* gcAlloc = llvm::cast<llvm::Function>(gcAllocCallee.getCallee());
        llvm::Value* floatBufferSize = llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), 64);
        llvm::Value* bufferPtr = builder.CreateCall(gcAlloc, { floatBufferSize }, "float_str_gc_buffer");
        
        // Null check for allocation failure
        llvm::BasicBlock* currentBB = builder.GetInsertBlock();
        llvm::Function* currentFunc = currentBB->getParent();
        llvm::BasicBlock* nullBB = llvm::BasicBlock::Create(context, "float_str_null", currentFunc);
        llvm::BasicBlock* validBB = llvm::BasicBlock::Create(context, "float_str_valid", currentFunc);
        
        llvm::Value* isNull = builder.CreateICmpEQ(
            bufferPtr,
            llvm::ConstantPointerNull::get(llvm::PointerType::get(context, 0)),
            "is_null"
        );
        builder.CreateCondBr(isNull, nullBB, validBB);
        
        // Null path: panic
        builder.SetInsertPoint(nullBB);
        llvm::Function* panicOOM = module->getFunction("aria_panic_oom");
        if (!panicOOM) {
            llvm::FunctionType* panicType = llvm::FunctionType::get(
                llvm::Type::getVoidTy(context),
                { llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0) },
                false
            );
            panicOOM = llvm::Function::Create(
                panicType,
                llvm::Function::ExternalLinkage,
                "aria_panic_oom",
                module
            );
        }
        llvm::Value* panicMsg = builder.CreateGlobalStringPtr(
            "Out of memory in float to string conversion",
            "float_oom_msg"
        );
        builder.CreateCall(panicOOM, { panicMsg });
        builder.CreateUnreachable();
        
        // Valid path: continue
        builder.SetInsertPoint(validBB);
        
        // Create format string "%.17g" (full precision, shortest representation)
        // Use %.17g for doubles (all significant digits) to ensure determinism
        llvm::Constant* formatStr = llvm::ConstantDataArray::getString(context, "%.17g", true);
        llvm::GlobalVariable* formatGV = new llvm::GlobalVariable(
            *module,
            formatStr->getType(),
            true,
            llvm::GlobalValue::PrivateLinkage,
            formatStr,
            ".fmt_g_c_locale"
        );
        llvm::Value* formatPtr = builder.CreatePointerCast(formatGV, llvm::PointerType::get(context, 0));
        
        // Declare aria_snprintf_c_locale (our deterministic wrapper)
        // This is a runtime function that forces C locale for formatting
        llvm::Function* snprintfFn = module->getFunction("aria_snprintf_c_locale");
        if (!snprintfFn) {
            llvm::FunctionType* snprintfType = llvm::FunctionType::get(
                llvm::Type::getInt32Ty(context),
                { llvm::PointerType::get(context, 0),  // buffer
                  llvm::Type::getInt64Ty(context),      // size
                  llvm::PointerType::get(context, 0),  // format
                  llvm::Type::getDoubleTy(context) },  // value
                false  // not varargs (fixed signature for safety)
            );
            snprintfFn = llvm::Function::Create(
                snprintfType,
                llvm::Function::ExternalLinkage,
                "aria_snprintf_c_locale",
                module
            );
        }
        
        // Convert float to double if needed (snprintf expects double for %g)
        llvm::Value* doubleVal = floatVal;
        if (floatVal->getType()->isFloatTy()) {
            doubleVal = builder.CreateFPExt(floatVal, llvm::Type::getDoubleTy(context), "to_double");
        }
        
        // Call aria_snprintf_c_locale(buffer, 64, "%.17g", doubleVal)
        // This always uses '.' as decimal separator regardless of system locale
        builder.CreateCall(snprintfFn, { bufferPtr, floatBufferSize, formatPtr, doubleVal });
        
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
    
    // OPTIMIZED: Use aria_string_concat_n for O(n) instead of O(n²) concatenation
    // The template literal structure is: parts[0], interp[0], parts[1], interp[1], parts[2], ...
    // Total segments = parts.size() + interpolations.size()

    llvm::StructType* ariaStringType = llvm::StructType::get(
        context,
        { llvm::PointerType::get(context, 0), llvm::Type::getInt64Ty(context) }
    );

    size_t totalSegments = expr->parts.size() + expr->interpolations.size();

    // Handle empty template literal
    if (totalSegments == 0) {
        llvm::Value* emptyStr = createAriaString("");
        
        // FIX: Allocate on GC heap and return pointer (matches string literal behavior)
        // Allocate AriaString struct on GC heap
        llvm::FunctionCallee gcAllocCallee = module->getOrInsertFunction("aria_gc_alloc",
            llvm::FunctionType::get(
                llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0),
                {llvm::Type::getInt64Ty(context)},
                false
            )
        );
        llvm::Function* gcAlloc = llvm::cast<llvm::Function>(gcAllocCallee.getCallee());
        llvm::Value* structSize = llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), 16); // sizeof(AriaString)
        llvm::Value* heapPtr = builder.CreateCall(gcAlloc, { structSize }, "empty_str_gc");
        
        // Cast to AriaString* and store the empty string value
        llvm::Value* strPtr = builder.CreateBitCast(heapPtr, llvm::PointerType::get(ariaStringType, 0), "empty_str_ptr");
        builder.CreateStore(emptyStr, strPtr);
        
        // Return the pointer (AriaString*)
        return strPtr;
    }

    // Stack allocation limit to prevent DoS attacks
    // Template literals with more than 100 segments use heap allocation
    constexpr size_t MAX_STACK_SEGMENTS = 100;
    
    llvm::Value* stringsArray;
    llvm::ArrayType* arrayType = llvm::ArrayType::get(ariaStringType, totalSegments);
    
    if (totalSegments <= MAX_STACK_SEGMENTS) {
        // Small template literals: use stack allocation (fast path)
        stringsArray = builder.CreateAlloca(arrayType, nullptr, "template_strings");
    } else {
        // Large template literals: use GC heap allocation
        // ARIA-026 FIX: Must use aria_gc_alloc instead of aria_alloc so GC can see string refs
        // during construction. Wild memory would be invisible to GC -> mid-construction corruption.
        
        // Calculate size: totalSegments * sizeof(AriaString)
        // AriaString = {ptr, i64} = 16 bytes (on 64-bit)
        llvm::Value* arraySize = llvm::ConstantInt::get(
            llvm::Type::getInt64Ty(context),
            totalSegments * 16  // sizeof(AriaString)
        );
        
        // Call aria_gc_alloc (GC-visible memory) for temporary buffer
        llvm::FunctionCallee aria_gc_alloc_callee = module->getOrInsertFunction("aria_gc_alloc",
            llvm::FunctionType::get(
                llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0),
                {llvm::Type::getInt64Ty(context)},
                false
            )
        );
        llvm::Function* aria_gc_alloc = llvm::cast<llvm::Function>(aria_gc_alloc_callee.getCallee());
        llvm::Value* heapMem = builder.CreateCall(aria_gc_alloc, { arraySize }, "heap_strings_gc");
        
        // ARIA-026 FIX: Add null check for GC allocation failure
        llvm::BasicBlock* currentBB = builder.GetInsertBlock();
        llvm::Function* currentFunc = currentBB->getParent();
        llvm::BasicBlock* gcNullBB = llvm::BasicBlock::Create(context, "gc_alloc_null", currentFunc);
        llvm::BasicBlock* gcValidBB = llvm::BasicBlock::Create(context, "gc_alloc_valid", currentFunc);
        
        llvm::Value* isNull = builder.CreateICmpEQ(
            heapMem,
            llvm::ConstantPointerNull::get(llvm::PointerType::get(context, 0)),
            "gc_is_null"
        );
        builder.CreateCondBr(isNull, gcNullBB, gcValidBB);
        
        // Null path: panic
        builder.SetInsertPoint(gcNullBB);
        llvm::Function* aria_panic_oom = module->getFunction("aria_panic_oom");
        if (!aria_panic_oom) {
            llvm::FunctionType* panicType = llvm::FunctionType::get(
                llvm::Type::getVoidTy(context),
                { llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0) },
                false
            );
            aria_panic_oom = llvm::Function::Create(
                panicType,
                llvm::Function::ExternalLinkage,
                "aria_panic_oom",
                module
            );
        }
        llvm::Value* panicMsg = builder.CreateGlobalStringPtr(
            "Out of memory allocating template literal array",
            "gc_oom_msg"
        );
        builder.CreateCall(aria_panic_oom, { panicMsg });
        builder.CreateUnreachable();
        
        // Valid path: continue
        builder.SetInsertPoint(gcValidBB);
        
        // Cast void* to AriaString[]* type
        stringsArray = builder.CreateBitCast(
            heapMem,
            llvm::PointerType::get(arrayType, 0),
            "strings_array"
        );
    }

    // Populate the array with all segments
    // Segments are interleaved: part[0], interp[0], part[1], interp[1], ...
    size_t arrayIndex = 0;

    for (size_t i = 0; i < expr->parts.size(); i++) {
        // Add the literal part
        llvm::Value* partStr = createAriaString(expr->parts[i]);
        llvm::Value* slotPtr = builder.CreateGEP(arrayType, stringsArray,
            { llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), 0),
              llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), arrayIndex) },
            "part_slot");
        builder.CreateStore(partStr, slotPtr);
        arrayIndex++;

        // Add the interpolation if it exists
        if (i < expr->interpolations.size()) {
            ASTNode* interpExpr = expr->interpolations[i].get();
            llvm::Value* interpVal = codegenExpressionNode(interpExpr, this);

            // Convert to AriaString based on type
            llvm::Value* interpStr;
            llvm::Type* valType = interpVal->getType();

            // Check for special Aria types that need custom formatters
            std::string ariaType = getExprAriaTypeName(interpExpr, var_aria_types);

            if (needsSpecialFormatter(ariaType)) {
                // Use type-aware formatter from runtime/fmt/formatters.cpp
                std::string formatterName = "aria_format_" + ariaType;

                // Get or declare the formatter function
                // For large structs (int256, etc.), pass by pointer per x86-64 ABI
                llvm::Type* argType;
                llvm::Value* argVal;
                
                if (interpVal->getType()->isStructTy()) {
                    // Large structs: pass pointer to formatter
                    llvm::Value* tempAlloca = builder.CreateAlloca(interpVal->getType(), nullptr, "struct_temp");
                    builder.CreateStore(interpVal, tempAlloca);
                    argType = llvm::PointerType::get(context, 0);
                    argVal = tempAlloca;
                } else {
                    // Primitives: pass by value
                    argType = interpVal->getType();
                    argVal = interpVal;
                }

                llvm::Function* formatterFn = module->getFunction(formatterName);
                if (!formatterFn) {
                    llvm::FunctionType* formatterType = llvm::FunctionType::get(
                        llvm::PointerType::get(context, 0),  // Returns AriaString*
                        { argType },
                        false
                    );
                    formatterFn = llvm::Function::Create(
                        formatterType,
                        llvm::Function::ExternalLinkage,
                        formatterName,
                        module
                    );
                }

                // Call formatter and load the returned AriaString
                llvm::Value* strPtr = builder.CreateCall(formatterFn, { argVal }, "fmt_result");
                interpStr = builder.CreateLoad(ariaStringType, strPtr, "fmt_str");
            } else if (valType->isIntegerTy(1)) {
                interpStr = boolToString(interpVal);
            } else if (valType->isIntegerTy()) {
                interpStr = int64ToString(interpVal);
            } else if (valType->isFloatingPointTy()) {
                interpStr = floatToString(interpVal);
            } else if (valType->isPointerTy()) {
                interpStr = ptrToAriaString(interpVal);
            } else {
                throw std::runtime_error("Unsupported type in template literal interpolation");
            }

            llvm::Value* interpSlotPtr = builder.CreateGEP(arrayType, stringsArray,
                { llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), 0),
                  llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), arrayIndex) },
                "interp_slot");
            builder.CreateStore(interpStr, interpSlotPtr);
            arrayIndex++;
        }
    }

    // Declare aria_string_concat_n_simple runtime function
    // AriaString* aria_string_concat_n_simple(AriaString* strings, int64_t count)
    llvm::FunctionType* concatNType = llvm::FunctionType::get(
        llvm::PointerType::get(context, 0),  // Returns AriaString*
        { llvm::PointerType::get(context, 0), llvm::Type::getInt64Ty(context) },
        false
    );

    llvm::Function* concatNFn = module->getFunction("aria_string_concat_n_simple");
    if (!concatNFn) {
        concatNFn = llvm::Function::Create(
            concatNType,
            llvm::Function::ExternalLinkage,
            "aria_string_concat_n_simple",
            module
        );
    }

    // Get pointer to first element of the array
    llvm::Value* arrayPtr = builder.CreateGEP(arrayType, stringsArray,
        { llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), 0),
          llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), 0) },
        "array_start");

    // Call aria_string_concat_n_simple(strings, count)
    llvm::Value* count = llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), totalSegments);
    llvm::Value* resultPtr = builder.CreateCall(concatNFn, { arrayPtr, count }, "concat_result");

    // ARIA-026 FIX: Add null check for allocation failure
    // Runtime allocators can return NULL on OOM -> immediate dereference = segfault
    llvm::BasicBlock* currentBB = builder.GetInsertBlock();
    llvm::Function* currentFunc = currentBB->getParent();
    llvm::BasicBlock* nullCheckBB = llvm::BasicBlock::Create(context, "concat_null_check", currentFunc);
    llvm::BasicBlock* validBB = llvm::BasicBlock::Create(context, "concat_valid", currentFunc);
    
    // Check if resultPtr == NULL
    llvm::Value* isNull = builder.CreateICmpEQ(
        resultPtr,
        llvm::ConstantPointerNull::get(llvm::PointerType::get(context, 0)),
        "is_null"
    );
    builder.CreateCondBr(isNull, nullCheckBB, validBB);
    
    // Null path: panic with OOM message
    builder.SetInsertPoint(nullCheckBB);
    llvm::Function* aria_panic_oom = module->getFunction("aria_panic_oom");
    if (!aria_panic_oom) {
        llvm::FunctionType* panicType = llvm::FunctionType::get(
            llvm::Type::getVoidTy(context),
            { llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0) },
            false
        );
        aria_panic_oom = llvm::Function::Create(
            panicType,
            llvm::Function::ExternalLinkage,
            "aria_panic_oom",
            module
        );
    }
    llvm::Value* panicMsg = builder.CreateGlobalStringPtr(
        "Out of memory in string concatenation (template literal)",
        "oom_msg"
    );
    builder.CreateCall(aria_panic_oom, { panicMsg });
    builder.CreateUnreachable();
    
    // Valid path: continue with normal flow
    builder.SetInsertPoint(validBB);

    // Cleanup: Free heap-allocated array if needed
    if (totalSegments > MAX_STACK_SEGMENTS) {
        // Call aria_free on the heap-allocated array
        llvm::FunctionCallee aria_free_callee = module->getOrInsertFunction("aria_wild_free",
            llvm::FunctionType::get(
                llvm::Type::getVoidTy(context),
                {llvm::PointerType::get(context, 0)},
                false
            )
        );
        llvm::Function* aria_free = llvm::cast<llvm::Function>(aria_free_callee.getCallee());
        llvm::Value* heapPtr = builder.CreateBitCast(
            stringsArray,
            llvm::PointerType::get(context, 0),
            "heap_ptr"
        );
        builder.CreateCall(aria_free, { heapPtr });
    }

    // FIX: Return the AriaString* pointer directly (matches string literal behavior)
    // println() expects either GlobalVariable* or AriaString* pointer, not raw char*
    return resultPtr;
}

/**
 * Helper: Recursively generate code for any expression node
 * This is a simplified dispatcher for testing - full integration in Phase 4.3+
 */
llvm::Value* ExprCodegen::codegenExpressionNode(ASTNode* node, ExprCodegen* codegen) {
    if (!node) {
        throw std::runtime_error("Null expression node");
    }
    
    // ARIA-026: SAFETY FIX - Recursion depth guard to prevent stack overflow DoS
    // Gemini Safety Audit Fix #4: Expression Recursion Vulnerability
    // Risk: Deeply nested expressions could crash Teacher system, preventing student input processing
    if (++codegen->expr_depth_ > MAX_EXPR_DEPTH) {
        --codegen->expr_depth_;  // Unwind before throwing
        throw std::runtime_error("Expression nesting limit exceeded (" + 
                                std::to_string(MAX_EXPR_DEPTH) + 
                                " levels). Simplify complex expressions to prevent stack overflow.");
    }
    
    // RAII-style depth decrement on scope exit
    struct DepthGuard {
        size_t& depth;
        DepthGuard(size_t& d) : depth(d) {}
        ~DepthGuard() { --depth; }
    } guard(codegen->expr_depth_);
    
    // Dispatch based on node type
    std::cerr << "[DEBUG codegenExpressionNode] NodeType: " << static_cast<int>(node->type) << std::endl;
    
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
            return codegen-> codegenAwait(node);
        case ASTNode::NodeType::CAST:
            return codegen->codegenCast(static_cast<CastExpr*>(node));
        case ASTNode::NodeType::INDEX:
            return codegen->codegenIndex(static_cast<IndexExpr*>(node));
        case ASTNode::NodeType::OBJECT_LITERAL:
            return codegen->codegenObjectLiteral(static_cast<ObjectLiteralExpr*>(node));
        case ASTNode::NodeType::MEMBER_ACCESS:
        case ASTNode::NodeType::POINTER_MEMBER:
            return codegen->codegenMemberAccess(static_cast<MemberAccessExpr*>(node));
        case ASTNode::NodeType::UNWRAP: {
            // Unwrap operator: result ? default (for Result<T>)
            // Result struct: { value_type, ptr, i1 } where field 2 is is_error
            UnwrapExpr* unwrap = static_cast<UnwrapExpr*>(node);
            
            // Generate left side (the Result)
            llvm::Value* resultVal = codegenExpressionNode(unwrap->result.get(), codegen);
            if (!resultVal) return nullptr;
            
            // Extract is_error field (field 2)
            llvm::Value* isError = codegen->builder.CreateExtractValue(resultVal, 2, "is_error");
            
            // Create blocks for control flow
            llvm::Function* currentFunc = codegen->builder.GetInsertBlock()->getParent();
            llvm::BasicBlock* errorBlock = llvm::BasicBlock::Create(codegen->context, "error_block", currentFunc);
            llvm::BasicBlock* successBlock = llvm::BasicBlock::Create(codegen->context, "success_block", currentFunc);
            llvm::BasicBlock* mergeBlock = llvm::BasicBlock::Create(codegen->context, "merge_block", currentFunc);
            
            // Branch on is_error
            codegen->builder.CreateCondBr(isError, errorBlock, successBlock);
            
            // Error block: return default value
            codegen->builder.SetInsertPoint(errorBlock);
            llvm::Value* defaultVal = codegenExpressionNode(unwrap->defaultValue.get(), codegen);
            if (!defaultVal) return nullptr;
            llvm::BasicBlock* errorEndBlock = codegen->builder.GetInsertBlock();  // May have changed
            codegen->builder.CreateBr(mergeBlock);
            
            // Success block: extract and return value field (field 0)
            codegen->builder.SetInsertPoint(successBlock);
            llvm::Value* successVal = codegen->builder.CreateExtractValue(resultVal, 0, "unwrap_value");
            llvm::BasicBlock* successEndBlock = codegen->builder.GetInsertBlock();
            codegen->builder.CreateBr(mergeBlock);
            
            // Merge block: phi node
            codegen->builder.SetInsertPoint(mergeBlock);
            llvm::PHINode* phi = codegen->builder.CreatePHI(defaultVal->getType(), 2, "unwrap_result");
            phi->addIncoming(defaultVal, errorEndBlock);
            phi->addIncoming(successVal, successEndBlock);
            
            return phi;
        }
        default:
            std::cerr << "[ERROR] Unhandled NodeType: " << static_cast<int>(node->type) << std::endl;
            throw std::runtime_error("Unsupported expression node type in operation");
    }
}

/**
 * Generate code for binary operations
 * Handles: arithmetic, comparison, logical, bitwise operators
 * Phase 1 Task 1: TBB arithmetic automatically lowered to safe runtime functions
 */
llvm::Value* ExprCodegen::codegenBinary(BinaryExpr* expr) {
    if (!expr) {
        throw std::runtime_error("Null binary expression");
    }

    // Get the operator type early for TBB check
    TokenType op = expr->op.type;
    
    std::cerr << "[CODEGEN_BINARY] ENTRY - operator type: " << static_cast<int>(op) << std::endl;
    
    std::cerr << "[DEBUG] codegenBinary called, op type: " << static_cast<int>(op) << std::endl;

    // ========================================================================
    // TBB AUTOMATIC LOWERING (Phase 1 Task 1)
    // Check if this is a TBB arithmetic operation and use safe runtime functions
    // ========================================================================
    std::string leftTBBType = getExprTBBTypeName(expr->left.get());
    std::string rightTBBType = getExprTBBTypeName(expr->right.get());
    
    std::cerr << "[DEBUG] TBB types: left='" << leftTBBType << "', right='" << rightTBBType << "'" << std::endl;

    if (!leftTBBType.empty() && !rightTBBType.empty() && leftTBBType == rightTBBType) {
        // Both operands are the same TBB type - use safe TBB arithmetic
        // Only for arithmetic operations: +, -, *, /, %
        if (op == TokenType::TOKEN_PLUS || op == TokenType::TOKEN_MINUS ||
            op == TokenType::TOKEN_STAR || op == TokenType::TOKEN_SLASH ||
            op == TokenType::TOKEN_PERCENT) {

            std::cerr << "[TBB] Lowering " << leftTBBType << " arithmetic (intrinsic)" << std::endl;

            // Generate code for operands
            llvm::Value* left = codegenExpressionNode(expr->left.get(), this);
            llvm::Value* right = codegenExpressionNode(expr->right.get(), this);

            if (!left || !right) {
                throw std::runtime_error("Failed to generate code for TBB operation operands");
            }

            // Generate TBB runtime function call
            llvm::Value* result = generateTBBBinaryOp(leftTBBType, op, left, right);
            if (result) {
                return result;
            }
            // Fall through to standard codegen if TBB call failed
        }
    }

    // ========================================================================
    // EXOTIC TYPE AUTOMATIC LOWERING
    // Check if this is an exotic type arithmetic operation (tryte/nyte)
    // ========================================================================
    std::string leftExoticType = getExprExoticTypeName(expr->left.get());
    std::string rightExoticType = getExprExoticTypeName(expr->right.get());
    
    std::cerr << "[DEBUG] Exotic types: left='" << leftExoticType << "', right='" << rightExoticType << "'" << std::endl;

    if (!leftExoticType.empty() && !rightExoticType.empty() && leftExoticType == rightExoticType) {
        // Both operands are the same exotic type - use runtime functions
        // Only for arithmetic operations: +, -, *, /, %
        if (op == TokenType::TOKEN_PLUS || op == TokenType::TOKEN_MINUS ||
            op == TokenType::TOKEN_STAR || op == TokenType::TOKEN_SLASH ||
            op == TokenType::TOKEN_PERCENT) {

            std::cerr << "[EXOTIC] Lowering " << leftExoticType << " arithmetic (runtime)" << std::endl;

            // Generate code for operands
            llvm::Value* left = codegenExpressionNode(expr->left.get(), this);
            llvm::Value* right = codegenExpressionNode(expr->right.get(), this);

            if (!left || !right) {
                throw std::runtime_error("Failed to generate code for exotic operation operands");
            }

            // Generate exotic runtime function call
            llvm::Value* result = generateExoticBinaryOp(leftExoticType, op, left, right);
            if (result) {
                return result;
            }
            // Fall through to standard codegen if exotic call failed
        }
    }

    // ========================================================================
    // NUMERIC TYPE AUTOMATIC LOWERING (Session 3)
    // Check if this is a numeric type arithmetic operation (frac*, tfp*, vec9)
    // ========================================================================
    std::string leftNumericType = getExprNumericTypeName(expr->left.get());
    std::string rightNumericType = getExprNumericTypeName(expr->right.get());
    
    std::cerr << "[DEBUG] Numeric types: left='" << leftNumericType << "', right='" << rightNumericType << "'" << std::endl;

    if (!leftNumericType.empty() && !rightNumericType.empty() && leftNumericType == rightNumericType) {
        // Both operands are the same numeric type - use runtime functions
        // Only for arithmetic operations: +, -, *, /
        if (op == TokenType::TOKEN_PLUS || op == TokenType::TOKEN_MINUS ||
            op == TokenType::TOKEN_STAR || op == TokenType::TOKEN_SLASH) {

            std::cerr << "[NUMERIC] Lowering " << leftNumericType << " arithmetic (runtime)" << std::endl;

            // Generate code for operands
            llvm::Value* left = codegenExpressionNode(expr->left.get(), this);
            llvm::Value* right = codegenExpressionNode(expr->right.get(), this);

            if (!left || !right) {
                throw std::runtime_error("Failed to generate code for numeric operation operands");
            }

            // Generate numeric runtime function call
            llvm::Value* result = generateNumericBinaryOp(leftNumericType, op, left, right);
            if (result) {
                return result;
            }
            // Fall through to standard codegen if numeric call failed
        }
    }

    // ========================================================================
    // LBIM TYPE AUTOMATIC LOWERING (ARIA-024, ARIA-025)
    // Check if this is a LBIM type arithmetic operation (int128/256/512/1024, uint*, fix256)
    // Large integers stored as structs - MUST use runtime library
    // ========================================================================
    std::string leftLBIMType = getExprLBIMTypeName(expr->left.get());
    std::string rightLBIMType = getExprLBIMTypeName(expr->right.get());
    
    std::cerr << "[DEBUG] LBIM types: left='" << leftLBIMType << "', right='" << rightLBIMType << "'" << std::endl;

    if (!leftLBIMType.empty() && !rightLBIMType.empty() && leftLBIMType == rightLBIMType) {
        // Both operands are the same LBIM type - use runtime functions
        // For arithmetic operations: +, -, *, /, %
        // And comparison operations: ==, !=, <, <=, >, >=
        if (op == TokenType::TOKEN_PLUS || op == TokenType::TOKEN_MINUS ||
            op == TokenType::TOKEN_STAR || op == TokenType::TOKEN_SLASH ||
            op == TokenType::TOKEN_PERCENT ||
            op == TokenType::TOKEN_EQUAL_EQUAL || op == TokenType::TOKEN_BANG_EQUAL ||
            op == TokenType::TOKEN_LESS || op == TokenType::TOKEN_LESS_EQUAL ||
            op == TokenType::TOKEN_GREATER || op == TokenType::TOKEN_GREATER_EQUAL ||
            op == TokenType::TOKEN_AMPERSAND ||
            op == TokenType::TOKEN_PIPE ||
            op == TokenType::TOKEN_CARET) {

            std::cerr << "[LBIM] Lowering " << leftLBIMType << " operation (runtime)" << std::endl;

            // Generate code for operands
            llvm::Value* left = codegenExpressionNode(expr->left.get(), this);
            llvm::Value* right = codegenExpressionNode(expr->right.get(), this);

            if (!left || !right) {
                throw std::runtime_error("Failed to generate code for LBIM operation operands");
            }

            // Generate LBIM runtime function call
            llvm::Value* result = generateLBIMBinaryOp(leftLBIMType, op, left, right);
            if (result) {
                return result;
            }
            // Fall through to standard codegen if LBIM call failed
        }
    }

    // Generate code for left and right operands
    llvm::Value* left = codegenExpressionNode(expr->left.get(), this);
    llvm::Value* right = codegenExpressionNode(expr->right.get(), this);

    if (!left || !right) {
        throw std::runtime_error("Failed to generate code for binary operation operands");
    }

    // SPECIAL HANDLING: NIL comparison with optional types
    // If one side is NIL literal and other is optional, create proper OptionalNone struct
    auto* leftLiteral = dynamic_cast<LiteralExpr*>(expr->left.get());
    auto* rightLiteral = dynamic_cast<LiteralExpr*>(expr->right.get());
    bool leftIsNIL = (leftLiteral && leftLiteral->explicit_type == "NIL");
    bool rightIsNIL = (rightLiteral && rightLiteral->explicit_type == "NIL");
    
    if (leftIsNIL || rightIsNIL) {
        // Check if the other operand is an optional (struct with 2 fields: {i1, T})
        llvm::Type* optType = nullptr;
        llvm::Type* wrappedType = nullptr;
        
        if (leftIsNIL && right->getType()->isStructTy()) {
            llvm::StructType* structType = llvm::cast<llvm::StructType>(right->getType());
            if (structType->getNumElements() == 2 && 
                structType->getElementType(0)->isIntegerTy(1)) {
                // Right is optional - replace left NIL with OptionalNone
                wrappedType = structType->getElementType(1);
                left = createOptionalNone(wrappedType);
                std::cerr << "[DEBUG] Replaced left NIL with OptionalNone for comparison" << std::endl;
            }
        } else if (rightIsNIL && left->getType()->isStructTy()) {
            llvm::StructType* structType = llvm::cast<llvm::StructType>(left->getType());
            if (structType->getNumElements() == 2 && 
                structType->getElementType(0)->isIntegerTy(1)) {
                // Left is optional - replace right NIL with OptionalNone
                wrappedType = structType->getElementType(1);
                right = createOptionalNone(wrappedType);
                std::cerr << "[DEBUG] Replaced right NIL with OptionalNone for comparison" << std::endl;
            }
        }
    }

    // Check if operands are vectors
    bool leftIsVector = left->getType()->isVectorTy();
    bool rightIsVector = right->getType()->isVectorTy();
    bool isVector = leftIsVector || rightIsVector;
    
    std::cerr << "[BROADCAST DEBUG] leftIsVector=" << leftIsVector 
              << ", rightIsVector=" << rightIsVector << std::endl;
    
    // For vector-scalar operations, broadcast scalar to vector
    if (leftIsVector && !rightIsVector) {
        std::cerr << "[BROADCAST] Broadcasting right scalar to vector" << std::endl;
        // Right is scalar, left is vector - broadcast right
        llvm::VectorType* vecType = llvm::cast<llvm::VectorType>(left->getType());
        llvm::Value* vec = llvm::UndefValue::get(vecType);
        unsigned numElements = vecType->getElementCount().getKnownMinValue();
        std::cerr << "[BROADCAST] numElements=" << numElements << std::endl;
        for (unsigned i = 0; i < numElements; ++i) {
            vec = builder.CreateInsertElement(vec, right, i);
        }
        right = vec;
        rightIsVector = true;
        std::cerr << "[BROADCAST] Broadcast complete" << std::endl;
    } else if (!leftIsVector && rightIsVector) {
        std::cerr << "[BROADCAST] Broadcasting left scalar to vector" << std::endl;
        // Left is scalar, right is vector - broadcast left
        llvm::VectorType* vecType = llvm::cast<llvm::VectorType>(right->getType());
        llvm::Value* vec = llvm::UndefValue::get(vecType);
        unsigned numElements = vecType->getElementCount().getKnownMinValue();
        std::cerr << "[BROADCAST] numElements=" << numElements << std::endl;
        for (unsigned i = 0; i < numElements; ++i) {
            vec = builder.CreateInsertElement(vec, left, i);
        }
        left = vec;
        leftIsVector = true;
        std::cerr << "[BROADCAST] Broadcast complete" << std::endl;
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
    // Detect unsigned types so ordered comparisons use ICmpU* not ICmpS*
    bool isUnsigned = false;
    auto isUnsignedTypeName = [](const std::string& n) {
        return n == "uint8" || n == "uint16" || n == "uint32" || n == "uint64"
            || n == "u8" || n == "u16" || n == "u32" || n == "u64";
    };
    if (expr->left->type == ASTNode::NodeType::IDENTIFIER) {
        auto* id = static_cast<IdentifierExpr*>(expr->left.get());
        auto it = var_aria_types.find(id->name);
        if (it != var_aria_types.end() && isUnsignedTypeName(it->second)) isUnsigned = true;
    }
    if (!isUnsigned && expr->right->type == ASTNode::NodeType::IDENTIFIER) {
        auto* id = static_cast<IdentifierExpr*>(expr->right.get());
        auto it = var_aria_types.find(id->name);
        if (it != var_aria_types.end() && isUnsignedTypeName(it->second)) isUnsigned = true;
    }
    if (!isUnsigned && leftLiteral && isUnsignedTypeName(leftLiteral->explicit_type)) isUnsigned = true;
    if (!isUnsigned && rightLiteral && isUnsignedTypeName(rightLiteral->explicit_type)) isUnsigned = true;

    if (op == TokenType::TOKEN_EQUAL_EQUAL) {
        // Debug: Check type compatibility
        std::cerr << "[DEBUG COMPARISON] Left type: ";
        left->getType()->print(llvm::errs());
        std::cerr << ", Right type: ";
        right->getType()->print(llvm::errs());
        std::cerr << std::endl;
        
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
        } else if (isUnsigned) {
            return builder.CreateICmpULT(left, right, "lttmp");
        } else {
            return builder.CreateICmpSLT(left, right, "lttmp");
        }
    }
    
    if (op == TokenType::TOKEN_LESS_EQUAL) {
        if (isFloat) {
            return builder.CreateFCmpOLE(left, right, "letmp");
        } else if (isUnsigned) {
            return builder.CreateICmpULE(left, right, "letmp");
        } else {
            return builder.CreateICmpSLE(left, right, "letmp");
        }
    }
    
    if (op == TokenType::TOKEN_GREATER) {
        std::cerr << "[COMPARISON >] About to create ICmp/FCmp" << std::endl;
        std::cerr << "[COMPARISON >] isFloat=" << isFloat << std::endl;
        std::cerr << "[COMPARISON >] Left LLVM type: ";
        left->getType()->print(llvm::errs());
        std::cerr << std::endl;
        std::cerr << "[COMPARISON >] Right LLVM type: ";
        right->getType()->print(llvm::errs());
        std::cerr << std::endl;
        
        if (isFloat) {
            return builder.CreateFCmpOGT(left, right, "gttmp");
        } else if (isUnsigned) {
            return builder.CreateICmpUGT(left, right, "gttmp");
        } else {
            return builder.CreateICmpSGT(left, right, "gttmp");
        }
    }
    
    if (op == TokenType::TOKEN_GREATER_EQUAL) {
        if (isFloat) {
            return builder.CreateFCmpOGE(left, right, "getmp");
        } else if (isUnsigned) {
            return builder.CreateICmpUGE(left, right, "getmp");
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
    // Guard: LBIM extended types (int2048/int4096/uint2048/uint4096) are stored as
    // structs but were not caught by getExprLBIMTypeName above. Route them to runtime.
    auto lbimBitwiseGuard = [&](const char* opSuffix) -> llvm::Value* {
        if (!left->getType()->isStructTy()) return nullptr;
        auto* st = llvm::cast<llvm::StructType>(left->getType());
        std::string sname = st->getName().str();
        // Struct name is e.g. "struct.int2048" — drop "struct." prefix
        if (sname.size() > 7) {
            std::string lbimTypeName = sname.substr(7);
            llvm::Value* res = generateLBIMBinaryOp(lbimTypeName, op, left, right);
            if (res) return res;
        }
        return nullptr;
    };

    if (op == TokenType::TOKEN_AMPERSAND) {
        if (auto* r = lbimBitwiseGuard("_and")) return r;
        return builder.CreateAnd(left, right, "andtmp");
    }
    
    if (op == TokenType::TOKEN_PIPE) {
        if (auto* r = lbimBitwiseGuard("_or")) return r;
        return builder.CreateOr(left, right, "ortmp");
    }
    
    if (op == TokenType::TOKEN_CARET) {
        if (auto* r = lbimBitwiseGuard("_xor")) return r;
        return builder.CreateXor(left, right, "xortmp");
    }
    
    if (op == TokenType::TOKEN_SHIFT_LEFT) {
        return builder.CreateShl(left, right, "shltmp");
    }
    
    if (op == TokenType::TOKEN_SHIFT_RIGHT) {
        // Bug #20 fix: always use logical shift right for >>.
        // The type checker rejects >> on signed types ("bitwise requires unsigned types"),
        // so any >> that reaches codegen has unsigned operands and must fill with 0s.
        return builder.CreateLShr(left, right, "shrtmp");
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
 * Phase 1 Task 1: TBB negation automatically lowered to safe runtime function
 */
llvm::Value* ExprCodegen::codegenUnary(UnaryExpr* expr) {
    if (!expr) {
        throw std::runtime_error("Null unary expression");
    }

    TokenType op = expr->op.type;

    // ========================================================================
    // ========================================================================
    // TBB UNARY NEGATION LOWERING (Optimized Intrinsic Version)
    // Inline negation with sticky error check - no overflow possible
    // ========================================================================
    if (op == TokenType::TOKEN_MINUS) {
        std::string operandTBBType = getExprTBBTypeName(expr->operand.get());
        if (!operandTBBType.empty()) {
            std::cerr << "[TBB] Lowering " << operandTBBType << " negation (intrinsic)" << std::endl;

            llvm::Value* operand = codegenExpressionNode(expr->operand.get(), this);
            if (!operand) {
                throw std::runtime_error("Failed to generate code for TBB negation operand");
            }

            // Sticky error check
            llvm::Type* type = operand->getType();
            llvm::Value* sentinel = getTBBSentinel(type);
            llvm::Value* isErr = builder.CreateICmpEQ(operand, sentinel, "is_err");
            
            // Negation is safe with symmetric range:
            // -127 → +127, +127 → -127 (no overflow)
            llvm::Value* rawRes = builder.CreateNeg(operand, "neg_raw");
            
            // Return sentinel if input was ERR, else return negated value
            return builder.CreateSelect(isErr, sentinel, rawRes, "tbb_neg");
        }
    }

    // Generate code for the operand recursively
    llvm::Value* operand = codegenExpressionNode(expr->operand.get(), this);
    if (!operand) {
        throw std::runtime_error("Failed to generate code for unary operand");
    }

    bool isFloat = operand->getType()->isFloatingPointTy();

    // Arithmetic negation: -x
    if (op == TokenType::TOKEN_MINUS) {
        // ARIA SAFETY FIX: Check for exotic types first (runtime negation)
        std::string exoticType = getExprExoticTypeName(expr->operand.get());
        if (!exoticType.empty()) {
            std::cerr << "[EXOTIC] Generating negation for " << exoticType << " type" << std::endl;
            sema::PrimitiveType tempType(exoticType);
            return ternary_codegen.generateNeg(operand, &tempType);
        }
        
        if (isFloat) {
            return builder.CreateFNeg(operand, "negtmp");
        } else {
            return builder.CreateNeg(operand, "negtmp");
        }
    }
    
    // Logical NOT: !x
    if (op == TokenType::TOKEN_BANG) {
        // ARIA SAFETY FIX: Check for exotic types first (Kleene logic NOT)
        std::string exoticType = getExprExoticTypeName(expr->operand.get());
        if (!exoticType.empty()) {
            std::cerr << "[EXOTIC] Generating NOT for " << exoticType << " type" << std::endl;
            sema::PrimitiveType tempType(exoticType);
            return ternary_codegen.generateNot(operand, &tempType);
        }
        
        // Standard boolean NOT for non-exotic types
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
        // Address-of: @variable
        // Operand must be an lvalue (identifier, array element, or struct field)
        
        // Case 1: Simple variable - @x
        if (expr->operand->type == ASTNode::NodeType::IDENTIFIER) {
            IdentifierExpr* ident = static_cast<IdentifierExpr*>(expr->operand.get());
            auto it = named_values.find(ident->name);
            if (it == named_values.end()) {
                throw std::runtime_error("Variable '" + ident->name + "' not found for address-of (@)");
            }
            
            // Return the alloca pointer itself (the ADDRESS), not the loaded value
            std::cerr << "[DEBUG] Address-of operator @ for variable: " << ident->name << std::endl;
            return it->second;
        }
        
        // Case 2: Array element - @arr[i]
        // Case 3: Struct field - @obj.field
        // TODO Phase 4.3+: Implement @ for array elements and struct fields
        
        throw std::runtime_error("Address-of operator (@) requires lvalue - only variables supported currently");
    }
    
    // Dereference operator (blueprint style): value <- ptr
    // Arrow points FROM pointer TO value (showing data flow direction)
    if (op == TokenType::TOKEN_LEFT_ARROW) {
        // Dereference a pointer: load the value it points to
        if (!operand->getType()->isPointerTy()) {
            throw std::runtime_error("Dereference operator (<-) can only be applied to pointer types");
        }
        // With LLVM 20 opaque pointers, we load as pointer type
        // The actual element type will be determined by usage context
        return builder.CreateLoad(builder.getPtrTy(), operand, "deref");
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
    
    // Handle module member function calls (e.g., math_utils.add(...))
    // Also handles namespace-qualified static method calls (e.g., string.from_char(...))
    // And UFCS instance method calls (e.g., s.length() -> string_length(s))
    MemberAccessExpr* member_access = dynamic_cast<MemberAccessExpr*>(expr->callee.get());
    if (member_access) {
        // The object should be an identifier (the module/type name or variable)
        IdentifierExpr* ident = dynamic_cast<IdentifierExpr*>(member_access->object.get());
        if (!ident) {
            throw std::runtime_error("Member access must have identifier as base");
        }
        
        // GPU/PTX Backend - Phase 4: GPU Intrinsics
        // Intercept gpu.* calls and map to LLVM NVVM intrinsics
        if (ident->name == "gpu") {
            return codegenGPUIntrinsic(member_access->member, expr);
        }

        std::string mangled_func_name;
        std::vector<ASTNodePtr> call_args = expr->arguments;

        // FIRST: Check if this is a MODULE namespace (e.g., helper.add() where helper is a loaded module)
        // Module functions are called directly by name, not with type prefixes
        // This check must come before UFCS/static method checks
        // Note: We detect modules by checking if there's a function with the exact member name
        // because IR generation doesn't have access to the symbol table's MODULE symbols
        llvm::Function* direct_func = module->getFunction(member_access->member);
        if (direct_func) {
            // This is likely a module function call - use the member name directly
            mangled_func_name = member_access->member;
            // Don't modify arguments - module functions take their arguments normally
        } else {
            // Check if this is a variable (UFCS) or a type name (static method)
            // If it's in named_values, it's a variable
            auto var_it = named_values.find(ident->name);
            if (var_it != named_values.end()) {
            // UFCS: variable.method() -> TypeName_method(variable)
            // Get the LLVM type and map it to a type name
            llvm::Value* var_value = var_it->second;
            llvm::Type* var_type = var_value->getType();

            // Determine the type name from Aria type tracking or LLVM type
            std::string type_name;

            // First, check if we have the Aria type name tracked
            auto type_it = var_aria_types.find(ident->name);
            if (type_it != var_aria_types.end()) {
                // Use the exact Aria type name (e.g., "array", "string", "MyStruct")
                type_name = type_it->second;
            } else {
                // Fallback to LLVM type heuristics for older code paths
                if (var_type->isPointerTy()) {
                    // Pointer types are usually string, array, or user structs
                    // Default to "string" for pointer types without tracked Aria type
                    type_name = "string";
                } else if (var_type->isIntegerTy(64)) {
                    type_name = "int64";
                } else if (var_type->isIntegerTy(32)) {
                    type_name = "int32";
                } else if (var_type->isIntegerTy(16)) {
                    type_name = "int16";
                } else if (var_type->isIntegerTy(8)) {
                    type_name = "int8";
                } else if (var_type->isIntegerTy(1)) {
                    type_name = "bool";
                } else if (var_type->isDoubleTy()) {
                    type_name = "flt64";
                } else if (var_type->isFloatTy()) {
                    type_name = "flt32";
                } else {
                    // Fallback: use identifier name (old behavior - won't work for UFCS)
                    type_name = ident->name;
                }
            }

            // ====================================================================
            // ATOMIC<T> METHOD DISPATCH
            // ====================================================================
            // Handle atomic<T> method calls by dispatching to runtime C functions
            // counter.load() → aria_atomic_TYPE_load(&counter, SEQCST)
            // Supports both "atomic<int32>" and "atomic_int32" type name formats
            if (type_name.find("atomic<") == 0 || type_name.find("atomic_") == 0) {
                // Extract the inner type from atomic<TYPE> or atomic_TYPE
                std::string inner_type;
                if (type_name.find("atomic<") == 0) {
                    size_t start = type_name.find('<') + 1;
                    size_t end = type_name.find('>');
                    if (start == std::string::npos || end == std::string::npos) {
                        throw std::runtime_error("Malformed atomic type: " + type_name);
                    }
                    inner_type = type_name.substr(start, end - start);
                } else {
                    // atomic_int32 → int32
                    inner_type = type_name.substr(7);  // Skip "atomic_"
                }
                
                // Map Aria type to runtime type name
                std::string runtime_type = inner_type;  // int32 → int32, tbb32 → tbb32, etc.
                
                // Get the atomic variable (already a pointer from atomic_new)
                llvm::Value* atomic_ptr = var_value;
                
                // Dispatch based on method name
                std::string method_name = member_access->member;
                
                if (method_name == "load") {
                    // atomic.load() → aria_atomic_TYPE_load(atomic_ptr, SEQCST)
                    std::string func_name = "aria_atomic_" + runtime_type + "_load";
                    llvm::Function* load_func = module->getFunction(func_name);
                    
                    if (!load_func) {
                        // Determine return type based on inner type
                        llvm::Type* ret_type = nullptr;
                        if (inner_type == "int8") ret_type = builder.getInt8Ty();
                        else if (inner_type == "int16") ret_type = builder.getInt16Ty();
                        else if (inner_type == "int32") ret_type = builder.getInt32Ty();
                        else if (inner_type == "int64") ret_type = builder.getInt64Ty();
                        else if (inner_type == "bool") ret_type = builder.getInt1Ty();
                        else throw std::runtime_error("Unsupported atomic type: " + inner_type);
                        
                        // Signature: TYPE aria_atomic_TYPE_load(AriaAtomicTYPE*, int32)
                        llvm::FunctionType* func_type = llvm::FunctionType::get(
                            ret_type,
                            {llvm::PointerType::get(context, 0),  // atomic pointer
                             builder.getInt32Ty()},               // memory order
                            false
                        );
                        
                        load_func = llvm::Function::Create(
                            func_type,
                            llvm::Function::ExternalLinkage,
                            func_name,
                            module
                        );
                    }
                    
                    // Memory order: ARIA_MEMORY_ORDER_SEQ_CST = 4
                    llvm::Value* order = llvm::ConstantInt::get(builder.getInt32Ty(), 4);
                    return builder.CreateCall(load_func, {atomic_ptr, order}, "atomic_load");
                }
                else if (method_name == "store") {
                    // atomic.store(value) → aria_atomic_TYPE_store(atomic_ptr, value, SEQCST)
                    if (expr->arguments.size() != 1) {
                        throw std::runtime_error("atomic.store() requires exactly one argument");
                    }
                    
                    std::string func_name = "aria_atomic_" + runtime_type + "_store";
                    llvm::Function* store_func = module->getFunction(func_name);
                    
                    llvm::Value* value_arg = codegenExpressionNode(expr->arguments[0].get(), this);
                    
                    if (!store_func) {
                        // Signature: void aria_atomic_TYPE_store(AriaAtomicTYPE*, TYPE, int32)
                        llvm::FunctionType* func_type = llvm::FunctionType::get(
                            builder.getVoidTy(),
                            {llvm::PointerType::get(context, 0),  // atomic pointer
                             value_arg->getType(),                // value type
                             builder.getInt32Ty()},               // memory order
                            false
                        );
                        
                        store_func = llvm::Function::Create(
                            func_type,
                            llvm::Function::ExternalLinkage,
                            func_name,
                            module
                        );
                    }
                    
                    llvm::Value* order = llvm::ConstantInt::get(builder.getInt32Ty(), 4);
                    return builder.CreateCall(store_func, {atomic_ptr, value_arg, order}, "atomic_store");
                }
                else if (method_name == "swap") {
                    // atomic.swap(value) → aria_atomic_TYPE_exchange(atomic_ptr, value, SEQCST)
                    if (expr->arguments.size() != 1) {
                        throw std::runtime_error("atomic.swap() requires exactly one argument");
                    }
                    
                    std::string func_name = "aria_atomic_" + runtime_type + "_exchange";
                    llvm::Value* value_arg = codegenExpressionNode(expr->arguments[0].get(), this);
                    llvm::Function* swap_func = module->getFunction(func_name);
                    
                    if (!swap_func) {
                        // Signature: TYPE aria_atomic_TYPE_exchange(AriaAtomicTYPE*, TYPE, int32)
                        llvm::FunctionType* func_type = llvm::FunctionType::get(
                            value_arg->getType(),                // returns old value
                            {llvm::PointerType::get(context, 0), // atomic pointer
                             value_arg->getType(),               // new value
                             builder.getInt32Ty()},              // memory order
                            false
                        );
                        
                        swap_func = llvm::Function::Create(
                            func_type,
                            llvm::Function::ExternalLinkage,
                            func_name,
                            module
                        );
                    }
                    
                    llvm::Value* order = llvm::ConstantInt::get(builder.getInt32Ty(), 4);
                    return builder.CreateCall(swap_func, {atomic_ptr, value_arg, order}, "atomic_swap");
                }
                else if (method_name == "fetch_add") {
                    // atomic.fetch_add(delta) → aria_atomic_TYPE_fetch_add(atomic_ptr, delta, SEQCST)
                    if (expr->arguments.size() != 1) {
                        throw std::runtime_error("atomic.fetch_add() requires exactly one argument");
                    }
                    
                    std::string func_name = "aria_atomic_" + runtime_type + "_fetch_add";
                    llvm::Value* delta_arg = codegenExpressionNode(expr->arguments[0].get(), this);
                    llvm::Function* add_func = module->getFunction(func_name);
                    
                    if (!add_func) {
                        llvm::FunctionType* func_type = llvm::FunctionType::get(
                            delta_arg->getType(),
                            {llvm::PointerType::get(context, 0),
                             delta_arg->getType(),
                             builder.getInt32Ty()},
                            false
                        );
                        
                        add_func = llvm::Function::Create(
                            func_type,
                            llvm::Function::ExternalLinkage,
                            func_name,
                            module
                        );
                    }
                    
                    llvm::Value* order = llvm::ConstantInt::get(builder.getInt32Ty(), 4);
                    return builder.CreateCall(add_func, {atomic_ptr, delta_arg, order}, "atomic_fetch_add");
                }
                else if (method_name == "fetch_sub") {
                    // atomic.fetch_sub(delta) → aria_atomic_TYPE_fetch_sub(atomic_ptr, delta, SEQCST)
                    if (expr->arguments.size() != 1) {
                        throw std::runtime_error("atomic.fetch_sub() requires exactly one argument");
                    }
                    
                    std::string func_name = "aria_atomic_" + runtime_type + "_fetch_sub";
                    llvm::Value* delta_arg = codegenExpressionNode(expr->arguments[0].get(), this);
                    llvm::Function* sub_func = module->getFunction(func_name);
                    
                    if (!sub_func) {
                        llvm::FunctionType* func_type = llvm::FunctionType::get(
                            delta_arg->getType(),
                            {llvm::PointerType::get(context, 0),
                             delta_arg->getType(),
                             builder.getInt32Ty()},
                            false
                        );
                        
                        sub_func = llvm::Function::Create(
                            func_type,
                            llvm::Function::ExternalLinkage,
                            func_name,
                            module
                        );
                    }
                    
                    llvm::Value* order = llvm::ConstantInt::get(builder.getInt32Ty(), 4);
                    return builder.CreateCall(sub_func, {atomic_ptr, delta_arg, order}, "atomic_fetch_sub");
                }
                else {
                    throw std::runtime_error("Unknown atomic method: " + method_name);
                }
            }

            mangled_func_name = type_name + "_" + member_access->member;

            // Inject the object as the first argument (UFCS transformation)
            std::vector<ASTNodePtr> new_args;
            new_args.push_back(member_access->object);  // Add object as first arg
            for (auto& arg : expr->arguments) {
                new_args.push_back(arg);
            }
            call_args = new_args;
            } else {
                // Static method call: Type.method() -> Type_method()
                mangled_func_name = ident->name + "_" + member_access->member;
            }
        }

        // Create a temporary IdentifierExpr with the mangled name
        // and recursively call codegenCall to handle it as a regular function
        auto temp_ident = std::make_shared<IdentifierExpr>(mangled_func_name, expr->line, expr->column);
        CallExpr temp_call(temp_ident, call_args, expr->line, expr->column);

        // Recursively generate code for the call with the mangled name
        return codegenCall(&temp_call);
    }
    
    // The callee should be an identifier (function name or func variable)
    IdentifierExpr* callee_ident = dynamic_cast<IdentifierExpr*>(expr->callee.get());
    if (!callee_ident) {
        std::cerr << "[DEBUG] Callee is not an IdentifierExpr! Type: " << static_cast<int>(expr->callee->type) << std::endl;
        throw std::runtime_error("Function callee must be an identifier or module member access");
    }
    
    // ====================================================================
    // BUILTIN FUNCTION: print() - Minimal Core Primitive
    // ====================================================================
    // Design Philosophy: Core language provides minimal, safe primitives.
    // Type-to-string conversions belong in stdlib (to_string() overloads).
    // Complex formatting via printf FFI or future stdlib printf-like function.
    // This keeps core simple, debuggable, and forces explicit formatting choices.
    
    // ====================================================================
    // BUILTIN FUNCTION: ok() - Unknown State Detection (Phase 5.2 + Layer 1 Safety)
    // ====================================================================
    // ok(value) -> int32 - Check if value is Unknown
    // Purpose: Detect Unknown sentinel values from divide-by-zero, overflow, etc.
    // Returns: 1 if value is valid, 0 if value is Unknown
    // Unknown sentinel = signed maximum value (INT_MAX for given bit width)
    
    if (callee_ident->name == "ok") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("ok() requires exactly one argument");
        }

        // ok() is a pure pass-through: it strips the compile-time "unknown taint"
        // and returns the value unchanged. The programmer is explicitly acknowledging
        // that the value may be Unknown and choosing to propagate it anyway.
        // Example: int32:y = ok(result_that_might_be_unknown);
        llvm::Value* arg = codegenExpressionNode(expr->arguments[0].get(), this);
        if (!arg) {
            throw std::runtime_error("Failed to generate code for ok() argument");
        }
        std::cerr << "[DEBUG] ok() pass-through" << std::endl;
        return arg;
    }
    
    // ====================================================================
    // SIMD HORIZONTAL REDUCTIONS (P1-2 Phase 5)
    // ====================================================================
    // Reduce SIMD vector to scalar using LLVM reduction intrinsics
    // Maps directly to hardware instructions (SSE, AVX, AVX-512)
    
    if (callee_ident->name == "@simd_sum" || callee_ident->name == "simd_sum") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("@simd_sum() requires exactly one SIMD vector argument");
        }
        
        llvm::Value* vec = codegenExpressionNode(expr->arguments[0].get(), this);
        if (!vec || !vec->getType()->isVectorTy()) {
            throw std::runtime_error("@simd_sum() argument must be a SIMD vector");
        }
        
        llvm::VectorType* vecType = llvm::cast<llvm::VectorType>(vec->getType());
        llvm::Type* elemType = vecType->getElementType();
        
        // Use appropriate reduction based on element type
        if (elemType->isFloatingPointTy()) {
            // Float reduction: llvm.vector.reduce.fadd
            llvm::Function* reduceFunc = llvm::Intrinsic::getOrInsertDeclaration(
                module, llvm::Intrinsic::vector_reduce_fadd, {vecType});
            llvm::Value* zero = llvm::ConstantFP::get(elemType, 0.0);
            return builder.CreateCall(reduceFunc, {zero, vec}, "simd.sum");
        } else {
            // Integer reduction: llvm.vector.reduce.add
            llvm::Function* reduceFunc = llvm::Intrinsic::getOrInsertDeclaration(
                module, llvm::Intrinsic::vector_reduce_add, {vecType});
            return builder.CreateCall(reduceFunc, {vec}, "simd.sum");
        }
    }
    
    if (callee_ident->name == "@simd_product" || callee_ident->name == "simd_product") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("@simd_product() requires exactly one SIMD vector argument");
        }
        
        llvm::Value* vec = codegenExpressionNode(expr->arguments[0].get(), this);
        if (!vec || !vec->getType()->isVectorTy()) {
            throw std::runtime_error("@simd_product() argument must be a SIMD vector");
        }
        
        llvm::VectorType* vecType = llvm::cast<llvm::VectorType>(vec->getType());
        llvm::Type* elemType = vecType->getElementType();
        
        // Use appropriate reduction based on element type
        if (elemType->isFloatingPointTy()) {
            // Float reduction: llvm.vector.reduce.fmul
            llvm::Function* reduceFunc = llvm::Intrinsic::getOrInsertDeclaration(
                module, llvm::Intrinsic::vector_reduce_fmul, {vecType});
            llvm::Value* one = llvm::ConstantFP::get(elemType, 1.0);
            return builder.CreateCall(reduceFunc, {one, vec}, "simd.product");
        } else {
            // Integer reduction: llvm.vector.reduce.mul
            llvm::Function* reduceFunc = llvm::Intrinsic::getOrInsertDeclaration(
                module, llvm::Intrinsic::vector_reduce_mul, {vecType});
            return builder.CreateCall(reduceFunc, {vec}, "simd.product");
        }
    }
    
    if (callee_ident->name == "@simd_min" || callee_ident->name == "simd_min") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("@simd_min() requires exactly one SIMD vector argument");
        }
        
        llvm::Value* vec = codegenExpressionNode(expr->arguments[0].get(), this);
        if (!vec || !vec->getType()->isVectorTy()) {
            throw std::runtime_error("@simd_min() argument must be a SIMD vector");
        }
        
        llvm::VectorType* vecType = llvm::cast<llvm::VectorType>(vec->getType());
        llvm::Type* elemType = vecType->getElementType();
        
        // Use appropriate reduction based on element type
        if (elemType->isFloatingPointTy()) {
            // Float min: llvm.vector.reduce.fmin
            llvm::Function* reduceFunc = llvm::Intrinsic::getOrInsertDeclaration(
                module, llvm::Intrinsic::vector_reduce_fmin, {vecType});
            return builder.CreateCall(reduceFunc, {vec}, "simd.min");
        } else {
            // Signed integer min: llvm.vector.reduce.smin
            llvm::Function* reduceFunc = llvm::Intrinsic::getOrInsertDeclaration(
                module, llvm::Intrinsic::vector_reduce_smin, {vecType});
            return builder.CreateCall(reduceFunc, {vec}, "simd.min");
        }
    }
    
    if (callee_ident->name == "@simd_max" || callee_ident->name == "simd_max") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("@simd_max() requires exactly one SIMD vector argument");
        }
        
        llvm::Value* vec = codegenExpressionNode(expr->arguments[0].get(), this);
        if (!vec || !vec->getType()->isVectorTy()) {
            throw std::runtime_error("@simd_max() argument must be a SIMD vector");
        }
        
        llvm::VectorType* vecType = llvm::cast<llvm::VectorType>(vec->getType());
        llvm::Type* elemType = vecType->getElementType();
        
        // Use appropriate reduction based on element type
        if (elemType->isFloatingPointTy()) {
            // Float max: llvm.vector.reduce.fmax
            llvm::Function* reduceFunc = llvm::Intrinsic::getOrInsertDeclaration(
                module, llvm::Intrinsic::vector_reduce_fmax, {vecType});
            return builder.CreateCall(reduceFunc, {vec}, "simd.max");
        } else {
            // Signed integer max: llvm.vector.reduce.smax
            llvm::Function* reduceFunc = llvm::Intrinsic::getOrInsertDeclaration(
                module, llvm::Intrinsic::vector_reduce_smax, {vecType});
            return builder.CreateCall(reduceFunc, {vec}, "simd.max");
        }
    }
    
    // ====================================================================
    // SIMD Boolean Operations (P1-2 Phase 6)
    // ====================================================================
    // Boolean reductions for SIMD mask operations
    
    if (callee_ident->name == "@simd_any" || callee_ident->name == "simd_any") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("simd_any() requires exactly one SIMD boolean vector argument");
        }
        
        llvm::Value* vec = codegenExpressionNode(expr->arguments[0].get(), this);
        if (!vec || !vec->getType()->isVectorTy()) {
            throw std::runtime_error("simd_any() argument must be a SIMD vector");
        }
        
        llvm::VectorType* vecType = llvm::cast<llvm::VectorType>(vec->getType());
        
        // Use llvm.vector.reduce.or to check if ANY bit is set
        // For bool vectors (i1), this gives us true if any lane is true
        llvm::Function* reduceFunc = llvm::Intrinsic::getOrInsertDeclaration(
            module, llvm::Intrinsic::vector_reduce_or, {vecType});
        return builder.CreateCall(reduceFunc, {vec}, "simd.any");
    }
    
    if (callee_ident->name == "@simd_all" || callee_ident->name == "simd_all") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("simd_all() requires exactly one SIMD boolean vector argument");
        }
        
        llvm::Value* vec = codegenExpressionNode(expr->arguments[0].get(), this);
        if (!vec || !vec->getType()->isVectorTy()) {
            throw std::runtime_error("simd_all() argument must be a SIMD vector");
        }
        
        llvm::VectorType* vecType = llvm::cast<llvm::VectorType>(vec->getType());
        
        // Use llvm.vector.reduce.and to check if ALL bits are set
        // For bool vectors (i1), this gives us true if all lanes are true
        llvm::Function* reduceFunc = llvm::Intrinsic::getOrInsertDeclaration(
            module, llvm::Intrinsic::vector_reduce_and, {vecType});
        return builder.CreateCall(reduceFunc, {vec}, "simd.all");
    }
    
    // simd_select(mask, true_vals, false_vals) - Masked SIMD selection
    // Per-lane ternary: returns true_vals[i] if mask[i] else false_vals[i]
    // Maps directly to LLVM's select instruction for branchless conditional operations
    if (callee_ident->name == "@simd_select" || callee_ident->name == "simd_select") {
        if (expr->arguments.size() != 3) {
            throw std::runtime_error("simd_select() requires exactly three arguments (mask, true_vals, false_vals)");
        }
        
        llvm::Value* mask = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* trueVals = codegenExpressionNode(expr->arguments[1].get(), this);
        llvm::Value* falseVals = codegenExpressionNode(expr->arguments[2].get(), this);
        
        if (!mask || !trueVals || !falseVals) {
            throw std::runtime_error("Failed to generate code for simd_select arguments");
        }
        
        // LLVM select: per-lane conditional selection (branchless)
        // For each lane i: result[i] = mask[i] ? trueVals[i] : falseVals[i]
        return builder.CreateSelect(mask, trueVals, falseVals, "simd.select");
    }
    
    // simd_broadcast(scalar, lanes) - Broadcast scalar to all SIMD lanes
    // Creates a uniform SIMD vector where all lanes contain the same value
    if (callee_ident->name == "@simd_broadcast" || callee_ident->name == "simd_broadcast") {
        if (expr->arguments.size() != 2) {
            throw std::runtime_error("simd_broadcast() requires exactly two arguments (value, lane_count)");
        }
        
        llvm::Value* scalarValue = codegenExpressionNode(expr->arguments[0].get(), this);
        if (!scalarValue) {
            throw std::runtime_error("Failed to generate code for simd_broadcast scalar value");
        }
        
        // Extract lane count from integer literal
        if (expr->arguments[1]->type != aria::ASTNode::NodeType::LITERAL) {
            throw std::runtime_error("simd_broadcast() lane count must be a compile-time integer literal");
        }
        aria::LiteralExpr* laneCountLit = static_cast<aria::LiteralExpr*>(expr->arguments[1].get());
        if (!std::holds_alternative<int64_t>(laneCountLit->value)) {
            throw std::runtime_error("simd_broadcast() lane count must be an integer");
        }
        unsigned laneCount = std::get<int64_t>(laneCountLit->value);
        
        // Get the scalar type
        llvm::Type* scalarType = scalarValue->getType();
        
        // Create vector type
        llvm::VectorType* vecType = llvm::VectorType::get(scalarType, laneCount, false);
        
        // Method 1: Use a splat constant (works for constants)
        // Method 2: Insert into undef, then shuffle (works for all values)
        
        // Create undef vector
        llvm::Value* undefVec = llvm::UndefValue::get(vecType);
        
        // Insert scalar at position 0
        llvm::Value* vec0 = builder.CreateInsertElement(undefVec, scalarValue, 
                                                        uint64_t(0), "broadcast.insert");
        
        // Create shuffle mask: all zeros (broadcast lane 0 to all lanes)
        std::vector<int> shuffleMask(laneCount, 0);
        
        // Shuffle to broadcast
        llvm::Value* result = builder.CreateShuffleVector(vec0, undefVec, shuffleMask, "simd.broadcast");
        
        return result;
    }
    
    // simd_load(ptr, lanes) - Load SIMD vector from aligned memory
    // Loads N elements from memory into a SIMD vector
    if (callee_ident->name == "@simd_load" || callee_ident->name == "simd_load") {
        if (expr->arguments.size() != 2) {
            throw std::runtime_error("simd_load() requires exactly two arguments (pointer, lane_count)");
        }
        
        llvm::Value* ptr = codegenExpressionNode(expr->arguments[0].get(), this);
        if (!ptr || !ptr->getType()->isPointerTy()) {
            throw std::runtime_error("simd_load() first argument must be a pointer");
        }
        
        // Extract lane count from literal
        if (expr->arguments[1]->type != aria::ASTNode::NodeType::LITERAL) {
            throw std::runtime_error("simd_load() lane count must be a compile-time integer literal");
        }
        aria::LiteralExpr* laneCountLit = static_cast<aria::LiteralExpr*>(expr->arguments[1].get());
        if (!std::holds_alternative<int64_t>(laneCountLit->value)) {
            throw std::runtime_error("simd_load() lane count must be an integer");
        }
        unsigned laneCount = std::get<int64_t>(laneCountLit->value);
        
        // For LLVM 20 opaque pointers, we need to determine element type from Aria type or context
        // Get the Aria type of the pointer argument
        std::string ptrTypeName = "unknown";
        if (var_aria_types.count(expr->arguments[0]->toString())) {
            ptrTypeName = var_aria_types[expr->arguments[0]->toString()];
        }
        
        // For now, default to int32 if we can't determine the type
        // In practice, the type will be known from the function signature
        llvm::Type* elementType = builder.getInt32Ty();  // Default
        
        // Try to extract element type from pointer type annotation if available
        // This is a simplified approach - full implementation would query the type system
        if (ptrTypeName.find("int32*") != std::string::npos) {
            elementType = builder.getInt32Ty();
        } else if (ptrTypeName.find("float32*") != std::string::npos || ptrTypeName.find("flt32*") != std::string::npos) {
            elementType = builder.getFloatTy();
        } else if (ptrTypeName.find("int64*") != std::string::npos) {
            elementType = builder.getInt64Ty();
        } else if (ptrTypeName.find("float64*") != std::string::npos || ptrTypeName.find("flt64*") != std::string::npos) {
            elementType = builder.getDoubleTy();
        }
        // Add more type mappings as needed
        
        // Create vector type
        llvm::VectorType* vecType = llvm::VectorType::get(elementType, laneCount, false);
        
        // Load the vector using modern LLVM load instruction
        llvm::LoadInst* loadInst = builder.CreateLoad(vecType, ptr, "simd.load");
        
        // Set alignment (element size for natural alignment)
        unsigned elementSizeBytes = elementType->getPrimitiveSizeInBits() / 8;
        loadInst->setAlignment(llvm::Align(elementSizeBytes));
        
        return loadInst;
    }
    
    // simd_store(ptr, vec) - Store SIMD vector to aligned memory
    // Stores all elements of a SIMD vector to memory
    if (callee_ident->name == "@simd_store" || callee_ident->name == "simd_store") {
        if (expr->arguments.size() != 2) {
            throw std::runtime_error("simd_store() requires exactly two arguments (pointer, vector)");
        }
        
        llvm::Value* ptr = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* vec = codegenExpressionNode(expr->arguments[1].get(), this);
        
        if (!ptr || !ptr->getType()->isPointerTy()) {
            throw std::runtime_error("simd_store() first argument must be a pointer");
        }
        
        if (!vec || !vec->getType()->isVectorTy()) {
            throw std::runtime_error("simd_store() second argument must be a SIMD vector");
        }
        
        // Get vector type info
        llvm::VectorType* vecType = llvm::cast<llvm::VectorType>(vec->getType());
        llvm::Type* elementType = vecType->getElementType();
        
        // Store the vector using modern LLVM store instruction
        llvm::StoreInst* storeInst = builder.CreateStore(vec, ptr);
        
        // Set alignment (element size for natural alignment)
        unsigned elementSizeBytes = elementType->getPrimitiveSizeInBits() / 8;
        storeInst->setAlignment(llvm::Align(elementSizeBytes));
        
        // Return void (store instruction itself, but it's unused)
        return storeInst;
    }
    
    // print(string) - write string as-is (no newline)
    // println(string) - write string + newline (convenience)
    // Both minimal primitives with same interface, different intent
    if (callee_ident->name == "print" || callee_ident->name == "println") {
        bool add_newline = (callee_ident->name == "println");
        
        if (expr->arguments.size() != 1) {
            throw std::runtime_error(callee_ident->name + "() requires exactly one string argument");
        }
        
        // Evaluate the argument
        llvm::Value* arg = codegenExpressionNode(expr->arguments[0].get(), this);
        if (!arg) {
            throw std::runtime_error("Failed to generate code for print argument");
        }
        
        // Auto-unwrap Result<T> = { T, ptr, i8 } from user-defined function calls.
        // All Aria user functions return a Result struct; extract field 0 (the value).
        if (arg->getType()->isStructTy()) {
            llvm::StructType* struct_type = llvm::cast<llvm::StructType>(arg->getType());
            if (struct_type->getNumElements() == 3 &&
                struct_type->getElementType(1)->isPointerTy() &&
                struct_type->getElementType(2)->isIntegerTy(8)) {
                arg = builder.CreateExtractValue(arg, {0}, "result_val");
            }
        }
        
        llvm::Value* str_ptr = nullptr;
        
        // Check if argument is an AriaString struct (not a pointer, but the struct itself)
        // String literals return GlobalVariable pointers to AriaString structs
        if (llvm::isa<llvm::GlobalVariable>(arg)) {
            llvm::GlobalVariable* gv = llvm::cast<llvm::GlobalVariable>(arg);
            llvm::Type* gv_type = gv->getValueType();
            
            if (gv_type->isStructTy()) {
                llvm::StructType* struct_ty = llvm::cast<llvm::StructType>(gv_type);
                std::string struct_name = struct_ty->hasName() ? struct_ty->getName().str() : "";
                
                if (struct_name == "struct.AriaString") {
                    // Extract the data field (char* at index 0) from the global
                    llvm::Value* data_gep = builder.CreateStructGEP(
                        struct_ty,
                        arg,
                        0,  // data field is first
                        "str_data_ptr"
                    );
                    str_ptr = builder.CreateLoad(
                        llvm::PointerType::get(context, 0),
                        data_gep,
                        "str_data"
                    );
                } else {
                    throw std::runtime_error("print() cannot print this struct type");
                }
            } else {
                // Global non-struct - assume it's a raw string
                str_ptr = arg;
            }
        } else if (arg->getType()->isPointerTy()) {
            // Handle AriaString* pointers from string variables and runtime functions
            // (e.g. string_from_int, string_concat return AriaString*)
            // We need to load the AriaString struct and extract the char* data field.

            // Get or create AriaString struct type { i8*, i64 }
            // Must always create it here if missing — e.g. when there are no string
            // literals in the program so the type was never added to the module.
            llvm::StructType* ariaStringType = llvm::StructType::getTypeByName(context, "struct.AriaString");
            if (!ariaStringType) {
                ariaStringType = llvm::StructType::create(context, {
                    llvm::PointerType::get(context, 0),   // data: char*
                    llvm::Type::getInt64Ty(context)        // length: int64
                }, "struct.AriaString");
            }

            // Load the AriaString struct from the pointer, then extract data field
            llvm::Value* str_struct = builder.CreateLoad(ariaStringType, arg, "str_struct");
            str_ptr = builder.CreateExtractValue(str_struct, 0, "str_data");
        } else {
            std::string type_str;
            llvm::raw_string_ostream rso(type_str);
            arg->getType()->print(rso);
            rso.flush();
            throw std::runtime_error(
                callee_ident->name + "() requires a string argument. "
                "For other types, use to_string() from stdlib or printf via FFI."
                " [got LLVM type: " + type_str + "]"
            );
        }
        
        // Declare runtime function: aria_print_cstr or aria_println_cstr
        // Signature: int64_t aria_print[ln]_cstr(const char* str)
        // Returns: Number of bytes written, or -1 on error
        const char* func_name = add_newline ? "aria_println_cstr" : "aria_print_cstr";
        llvm::Function* aria_print = module->getFunction(func_name);
        if (!aria_print) {
            llvm::FunctionType* print_type = llvm::FunctionType::get(
                builder.getInt64Ty(),                    // returns bytes written
                {llvm::PointerType::get(context, 0)},    // takes const char*
                false                                     // not vararg
            );
            aria_print = llvm::Function::Create(
                print_type,
                llvm::Function::ExternalLinkage,
                func_name,
                module
            );
        }
        
        // Call aria_print[ln]_cstr(str) and return result
        return builder.CreateCall(aria_print, {str_ptr}, "print_call");
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
    // FRAC (EXACT RATIONAL) ARITHMETIC INTRINSICS
    // ====================================================================
    // Mixed-number fractions: frac32 = {tbb32 whole, tbb32 num, tbb32 denom}
    // All operations call C runtime for exact rational arithmetic with GCD reduction
    
    // Helper lambda to get frac LLVM type  
    auto get_frac_type = [&](const std::string& frac_name) -> llvm::StructType* {
        int bits = 0;
        if (frac_name == "frac8") bits = 8;
        else if (frac_name == "frac16") bits = 16;
        else if (frac_name == "frac32") bits = 32;
        else if (frac_name == "frac64") bits = 64;
        else throw std::runtime_error("Invalid frac type: " + frac_name);
        
        llvm::Type* tbb_type = llvm::Type::getIntNTy(context, bits);
        return llvm::StructType::get(context, {tbb_type, tbb_type, tbb_type});
    };
    
    // frac*_from_parts(whole, num, denom) -> frac*
    for (const char* width : {"8", "16", "32", "64"}) {
        std::string frac_name = "frac" + std::string(width);
        std::string func_name = frac_name + "_from_parts";
        
        if (callee_ident->name == func_name || callee_ident->name == "@" + func_name) {
            if (expr->arguments.size() != 3) {
                throw std::runtime_error(func_name + "() requires exactly 3 arguments");
            }
            
            // Get arguments
            llvm::Value* whole = codegenExpressionNode(expr->arguments[0].get(), this);
            llvm::Value* num = codegenExpressionNode(expr->arguments[1].get(), this);
            llvm::Value* denom = codegenExpressionNode(expr->arguments[2].get(), this);
            
            int bits = std::stoi(width);
            llvm::Type* tbb_type = llvm::Type::getIntNTy(context, bits);
            
            // Convert arguments to tbb type
            whole = builder.CreateIntCast(whole, tbb_type, true, "whole.cast");
            num = builder.CreateIntCast(num, tbb_type, true, "num.cast");
            denom = builder.CreateIntCast(denom, tbb_type, true, "denom.cast");
            
            // Create frac struct inline (no runtime call needed for constructor)
            llvm::StructType* frac_type = get_frac_type(frac_name);
            llvm::Value* result = llvm::UndefValue::get(frac_type);
            result = builder.CreateInsertValue(result, whole, 0, "frac.whole");
            result = builder.CreateInsertValue(result, num, 1, "frac.num");
            result = builder.CreateInsertValue(result, denom, 2, "frac.denom");
            
            return result;
        }
    }
    
    // frac*_add/sub/mul/div(a, b) -> frac*
    for (const char* width : {"8", "16", "32", "64"}) {
        std::string frac_name = "frac" + std::string(width);
        std::string struct_name = "Frac" + std::string(width);
        
        for (const char* op : {"add", "sub", "mul", "div"}) {
            std::string func_name = frac_name + "_" + op;
            std::string runtime_func = "aria_" + func_name;
            
            if (callee_ident->name == func_name || callee_ident->name == "@" + func_name) {
                if (expr->arguments.size() != 2) {
                    throw std::runtime_error(func_name + "() requires exactly 2 arguments");
                }
                
                // Get or create C runtime function
                llvm::StructType* frac_type = get_frac_type(frac_name);
                llvm::Function* c_func = module->getFunction(runtime_func);
                if (!c_func) {
                    llvm::FunctionType* func_type = llvm::FunctionType::get(
                        builder.getVoidTy(),  // void return (sret style)
                        {llvm::PointerType::get(frac_type, 0),  // result*
                         llvm::PointerType::get(frac_type, 0),  // a*
                         llvm::PointerType::get(frac_type, 0)}, // b*
                        false
                    );
                    c_func = llvm::Function::Create(func_type,
                        llvm::Function::ExternalLinkage, runtime_func, module);
                }
                
                // Get arguments
                llvm::Value* a = codegenExpressionNode(expr->arguments[0].get(), this);
                llvm::Value* b = codegenExpressionNode(expr->arguments[1].get(), this);
                
                // Create alloc as for passing to C runtime
                llvm::Value* result_alloca = builder.CreateAlloca(frac_type, nullptr, "result");
                llvm::Value* a_alloca = builder.CreateAlloca(frac_type, nullptr, "a.tmp");
                llvm::Value* b_alloca = builder.CreateAlloca(frac_type, nullptr, "b.tmp");
                
                // Store arguments
                builder.CreateStore(a, a_alloca);
                builder.CreateStore(b, b_alloca);
                
                // Call C runtime
                builder.CreateCall(c_func, {result_alloca, a_alloca, b_alloca});
                
                // Load and return result
                return builder.CreateLoad(frac_type, result_alloca, "frac.result");
            }
        }
    }
    
    // frac*_neg(a) -> frac*
    for (const char* width : {"8", "16", "32", "64"}) {
        std::string frac_name = "frac" + std::string(width);
        std::string func_name = frac_name + "_neg";
        std::string runtime_func = "aria_" + func_name;
        
        if (callee_ident->name == func_name || callee_ident->name == "@" + func_name) {
            if (expr->arguments.size() != 1) {
                throw std::runtime_error(func_name + "() requires exactly 1 argument");
            }
            
            llvm::StructType* frac_type = get_frac_type(frac_name);
            llvm::Function* c_func = module->getFunction(runtime_func);
            if (!c_func) {
                llvm::FunctionType* func_type = llvm::FunctionType::get(
                    builder.getVoidTy(),
                    {llvm::PointerType::get(frac_type, 0),  // result*
                     llvm::PointerType::get(frac_type, 0)}, // a*
                    false
                );
                c_func = llvm::Function::Create(func_type,
                    llvm::Function::ExternalLinkage, runtime_func, module);
            }
            
            llvm::Value* a = codegenExpressionNode(expr->arguments[0].get(), this);
            llvm::Value* result_alloca = builder.CreateAlloca(frac_type, nullptr, "result");
            llvm::Value* a_alloca = builder.CreateAlloca(frac_type, nullptr, "a.tmp");
            builder.CreateStore(a, a_alloca);
            builder.CreateCall(c_func, {result_alloca, a_alloca});
            return builder.CreateLoad(frac_type, result_alloca, "frac.neg");
        }
    }
    
    // frac*_cmp(a, b) -> int32
    for (const char* width : {"8", "16", "32", "64"}) {
        std::string frac_name = "frac" + std::string(width);
        std::string func_name = frac_name + "_cmp";
        std::string runtime_func = "aria_" + func_name;
        
        if (callee_ident->name == func_name || callee_ident->name == "@" + func_name) {
            if (expr->arguments.size() != 2) {
                throw std::runtime_error(func_name + "() requires exactly 2 arguments");
            }
            
            llvm::StructType* frac_type = get_frac_type(frac_name);
            llvm::Function* c_func = module->getFunction(runtime_func);
            if (!c_func) {
                llvm::FunctionType* func_type = llvm::FunctionType::get(
                    builder.getInt32Ty(),
                    {llvm::PointerType::get(frac_type, 0),  // a*
                     llvm::PointerType::get(frac_type, 0)}, // b*
                    false
                );
                c_func = llvm::Function::Create(func_type,
                    llvm::Function::ExternalLinkage, runtime_func, module);
            }
            
            llvm::Value* a = codegenExpressionNode(expr->arguments[0].get(), this);
            llvm::Value* b = codegenExpressionNode(expr->arguments[1].get(), this);
            llvm::Value* a_alloca = builder.CreateAlloca(frac_type, nullptr, "a.tmp");
            llvm::Value* b_alloca = builder.CreateAlloca(frac_type, nullptr, "b.tmp");
            builder.CreateStore(a, a_alloca);
            builder.CreateStore(b, b_alloca);
            return builder.CreateCall(c_func, {a_alloca, b_alloca}, "frac.cmp");
        }
    }
    
    // frac*_to_int(a) -> int32/int64
    for (const char* width : {"8", "16", "32", "64"}) {
        std::string frac_name = "frac" + std::string(width);
        std::string func_name = frac_name + "_to_int";
        std::string runtime_func = "aria_" + func_name;
        
        if (callee_ident->name == func_name || callee_ident->name == "@" + func_name) {
            if (expr->arguments.size() != 1) {
                throw std::runtime_error(func_name + "() requires exactly 1 argument");
            }
            
            llvm::Type* ret_type = (std::string(width) == "64") ? 
                builder.getInt64Ty() : builder.getInt32Ty();
            
            llvm::StructType* frac_type = get_frac_type(frac_name);
            llvm::Function* c_func = module->getFunction(runtime_func);
            if (!c_func) {
                llvm::FunctionType* func_type = llvm::FunctionType::get(
                    ret_type,
                    {llvm::PointerType::get(frac_type, 0)},  // a*
                    false
                );
                c_func = llvm::Function::Create(func_type,
                    llvm::Function::ExternalLinkage, runtime_func, module);
            }
            
            llvm::Value* a = codegenExpressionNode(expr->arguments[0].get(), this);
            llvm::Value* a_alloca = builder.CreateAlloca(frac_type, nullptr, "a.tmp");
            builder.CreateStore(a, a_alloca);
            return builder.CreateCall(c_func, {a_alloca}, "frac.to_int");
        }
    }
    
    // frac*_to_float(a) -> flt32/flt64
    for (const char* width : {"8", "16", "32", "64"}) {
        std::string frac_name = "frac" + std::string(width);
        std::string func_name = frac_name + "_to_float";
        std::string runtime_func = "aria_" + func_name;
        
        if (callee_ident->name == func_name || callee_ident->name == "@" + func_name) {
            if (expr->arguments.size() != 1) {
                throw std::runtime_error(func_name + "() requires exactly 1 argument");
            }
            
            llvm::Type* ret_type = (std::string(width) == "64") ? 
                builder.getDoubleTy() : builder.getFloatTy();
            
            llvm::StructType* frac_type = get_frac_type(frac_name);
            llvm::Function* c_func = module->getFunction(runtime_func);
            if (!c_func) {
                llvm::FunctionType* func_type = llvm::FunctionType::get(
                    ret_type,
                    {llvm::PointerType::get(frac_type, 0)},  // a*
                    false
                );
                c_func = llvm::Function::Create(func_type,
                    llvm::Function::ExternalLinkage, runtime_func, module);
            }
            
            llvm::Value* a = codegenExpressionNode(expr->arguments[0].get(), this);
            llvm::Value* a_alloca = builder.CreateAlloca(frac_type, nullptr, "a.tmp");
            builder.CreateStore(a, a_alloca);
            return builder.CreateCall(c_func, {a_alloca}, "frac.to_float");
        }
    }
    
    // ====================================================================
    // ATOMIC<T> OPERATIONS - Lock-free Concurrency Primitives
    // ====================================================================
    // P0 Feature: atomic<T> type and operations for thread-safe data structures
    // Runtime: C++11 std::atomic wrappers with TBB sticky error propagation
    // Memory Ordering: SeqCst (sequentially consistent) for safety - relaxed orderings future work
    //
    // Supported Types: int8-64, uint8-64, bool, tbb8-64 (with CAS-based ERR propagation)
    // Operations: load, store, swap, compare_exchange, fetch_add, fetch_sub
    //
    // Design: atomic_new(initial) creates atomic, methods dispatch to runtime:
    //   counter.load() → aria_atomic_TYPE_load(&counter, SEQCST)
    //   counter.store(val) → aria_atomic_TYPE_store(&counter, val, SEQCST)
    //   etc.
    
    // atomic_new(initial_value) -> atomic<T>*
    // Creates a new atomic with the given initial value
    // Type deduction: infer T from argument type (e.g., 0i32 → atomic<int32>)
    if (callee_ident->name == "atomic_new") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("atomic_new() requires exactly one argument");
        }
        
        // Evaluate the initial value argument
        llvm::Value* initial_val = codegenExpressionNode(expr->arguments[0].get(), this);
        if (!initial_val) {
            throw std::runtime_error("Failed to generate code for atomic_new() argument");
        }
        
        // Determine the type from the argument
        llvm::Type* arg_type = initial_val->getType();
        std::string runtime_type_name;
        std::string aria_type_name;
        
        // Map LLVM type to runtime type name
        if (arg_type->isIntegerTy(8)) {
            runtime_type_name = "int8";
            aria_type_name = "int8";
        } else if (arg_type->isIntegerTy(16)) {
            runtime_type_name = "int16";
            aria_type_name = "int16";
        } else if (arg_type->isIntegerTy(32)) {
            runtime_type_name = "int32";
            aria_type_name = "int32";
        } else if (arg_type->isIntegerTy(64)) {
            runtime_type_name = "int64";
            aria_type_name = "int64";
        } else if (arg_type->isIntegerTy(1)) {
            runtime_type_name = "bool";
            aria_type_name = "bool";
        } else {
            throw std::runtime_error("atomic_new() currently supports int8-64 and bool types");
        }
        
        // Construct runtime function name: aria_atomic_TYPE_create
        std::string runtime_func_name = "aria_atomic_" + runtime_type_name + "_create";
        
        // Get or declare the runtime function
        llvm::Function* create_func = module->getFunction(runtime_func_name);
        if (!create_func) {
            // Get the opaque atomic type
            std::string atomic_struct_name = "struct.AriaAtomic";
            for (char& c : runtime_type_name) {
                if (c >= 'a' && c <= 'z') c = c - 'a' + 'A';  // toupper
            }
            atomic_struct_name += runtime_type_name.substr(0, 1);
            atomic_struct_name += runtime_type_name.substr(1);
            
            // Get proper capitalization: int32 → Int32
            std::string capitalized = runtime_type_name;
            if (!capitalized.empty()) {
                capitalized[0] = std::toupper(capitalized[0]);
            }
            atomic_struct_name = "struct.AriaAtomic" + capitalized;
            
            llvm::StructType* atomic_type = llvm::StructType::create(context, atomic_struct_name);
            
            // Signature: AriaAtomicTYPE* aria_atomic_TYPE_create(TYPE initial)
            llvm::FunctionType* func_type = llvm::FunctionType::get(
                llvm::PointerType::get(atomic_type, 0),  // returns atomic ptr
                {arg_type},                              // takes initial value
                false
            );
            
            create_func = llvm::Function::Create(
                func_type,
                llvm::Function::ExternalLinkage,
                runtime_func_name,
                module
            );
        }
        
        // Call the runtime function
        return builder.CreateCall(create_func, {initial_val}, "atomic_new");
    }
    
    // ====================================================================
    // TFP (TWISTED FLOATING POINT) INTRINSICS
    // ====================================================================
    // Deterministic cross-platform floating-point
    // tfp32: {i16 exp, i16 mant}
    // tfp64: {i16 exp, i48 mant, i16 _pad}
    
    //tfp*_from_parts(exp, mant) -> tfp*
    for (const char* width : {"32", "64"}) {
        std::string funcName = "tfp" + std::string(width) + "_from_parts";
        if (callee_ident->name == funcName || callee_ident->name == "@" + funcName) {
            if (expr->arguments.size() != 2) {
                throw std::runtime_error(funcName + "() requires exactly 2 arguments");
            }
            
            llvm::Value* exp_val = codegenExpressionNode(expr->arguments[0].get(), this);
            llvm::Value* mant_val = codegenExpressionNode(expr->arguments[1].get(), this);
            
            // Build tfp struct inline
            llvm::Type* tfp_type = getLLVMType(type_system->getPrimitiveType("tfp" + std::string(width)));
            llvm::Value* tfp_val = llvm::UndefValue::get(tfp_type);
            tfp_val = builder.CreateInsertValue(tfp_val, exp_val, 0, "tfp.exp");
            tfp_val = builder.CreateInsertValue(tfp_val, mant_val, 1, "tfp.mant");
            return tfp_val;
        }
    }
    
    // tfp*_from_double(double) -> tfp*
    for (const char* width : {"32", "64"}) {
        std::string funcName = "aria_tfp" + std::string(width) + "_from_double";
        std::string ariaName = "tfp" + std::string(width) + "_from_double";
        if (callee_ident->name == ariaName || callee_ident->name == "@" + ariaName) {
            if (expr->arguments.size() != 1) {
                throw std::runtime_error(ariaName + "() requires exactly 1 argument");
            }
            
            llvm::Type* tfp_type = getLLVMType(type_system->getPrimitiveType("tfp" + std::string(width)));
            llvm::FunctionType* c_func_type = llvm::FunctionType::get(tfp_type, {builder.getDoubleTy()}, false);
            llvm::Function* c_func = module->getFunction(funcName);
            if (!c_func) {
                c_func = llvm::Function::Create(c_func_type, llvm::Function::ExternalLinkage, funcName, module);
            }
            
            llvm::Value* arg = codegenExpressionNode(expr->arguments[0].get(), this);
            return builder.CreateCall(c_func, {arg}, "tfp.from_double");
        }
    }
    
    // tfp*_to_double(tfp*) -> flt64
    for (const char* width : {"32", "64"}) {
        std::string funcName = "aria_tfp" + std::string(width) + "_to_double";
        std::string ariaName = "tfp" + std::string(width) + "_to_double";
        if (callee_ident->name == ariaName || callee_ident->name == "@" + ariaName) {
            if (expr->arguments.size() != 1) {
                throw std::runtime_error(ariaName + "() requires exactly 1 argument");
            }
            
            llvm::Type* tfp_type = getLLVMType(type_system->getPrimitiveType("tfp" + std::string(width)));
            llvm::FunctionType* c_func_type = llvm::FunctionType::get(builder.getDoubleTy(), {tfp_type}, false);
            llvm::Function* c_func = module->getFunction(funcName);
            if (!c_func) {
                c_func = llvm::Function::Create(c_func_type, llvm::Function::ExternalLinkage, funcName, module);
            }
            
            llvm::Value* arg = codegenExpressionNode(expr->arguments[0].get(), this);
            return builder.CreateCall(c_func, {arg}, "tfp.to_double");
        }
    }
    
    // tfp* arithmetic: add, sub, mul, div
    for (const char* width : {"32", "64"}) {
        for (const char* op : {"add", "sub", "mul", "div"}) {
            std::string funcName = "aria_tfp" + std::string(width) + "_" + op;
            std::string ariaName = "tfp" + std::string(width) + "_" + op;
            if (callee_ident->name == ariaName || callee_ident->name == "@" + ariaName) {
                if (expr->arguments.size() != 2) {
                    throw std::runtime_error(ariaName + "() requires exactly 2 arguments");
                }
                
                llvm::Type* tfp_type = getLLVMType(type_system->getPrimitiveType("tfp" + std::string(width)));
                llvm::FunctionType* c_func_type = llvm::FunctionType::get(tfp_type, {tfp_type, tfp_type}, false);
                llvm::Function* c_func = module->getFunction(funcName);
                if (!c_func) {
                    c_func = llvm::Function::Create(c_func_type, llvm::Function::ExternalLinkage, funcName, module);
                }
                
                llvm::Value* a = codegenExpressionNode(expr->arguments[0].get(), this);
                llvm::Value* b = codegenExpressionNode(expr->arguments[1].get(), this);
                return builder.CreateCall(c_func, {a, b}, "tfp." + std::string(op));
            }
        }
    }
    
    // tfp*_neg(tfp*) -> tfp*
    for (const char* width : {"32", "64"}) {
        std::string funcName = "aria_tfp" + std::string(width) + "_neg";
        std::string ariaName = "tfp" + std::string(width) + "_neg";
        if (callee_ident->name == ariaName || callee_ident->name == "@" + ariaName) {
            if (expr->arguments.size() != 1) {
                throw std::runtime_error(ariaName + "() requires exactly 1 argument");
            }
            
            llvm::Type* tfp_type = getLLVMType(type_system->getPrimitiveType("tfp" + std::string(width)));
            llvm::FunctionType* c_func_type = llvm::FunctionType::get(tfp_type, {tfp_type}, false);
            llvm::Function* c_func = module->getFunction(funcName);
            if (!c_func) {
                c_func = llvm::Function::Create(c_func_type, llvm::Function::ExternalLinkage, funcName, module);
            }
            
            llvm::Value* a = codegenExpressionNode(expr->arguments[0].get(), this);
            return builder.CreateCall(c_func, {a}, "tfp.neg");
        }
    }
    
    // tfp*_cmp(tfp*, tfp*) -> int32
    for (const char* width : {"32", "64"}) {
        std::string funcName = "aria_tfp" + std::string(width) + "_cmp";
        std::string ariaName = "tfp" + std::string(width) + "_cmp";
        if (callee_ident->name == ariaName || callee_ident->name == "@" + ariaName) {
            if (expr->arguments.size() != 2) {
                throw std::runtime_error(ariaName + "() requires exactly 2 arguments");
            }
            
            llvm::Type* tfp_type = getLLVMType(type_system->getPrimitiveType("tfp" + std::string(width)));
            llvm::FunctionType* c_func_type = llvm::FunctionType::get(builder.getInt32Ty(), {tfp_type, tfp_type}, false);
            llvm::Function* c_func = module->getFunction(funcName);
            if (!c_func) {
                c_func = llvm::Function::Create(c_func_type, llvm::Function::ExternalLinkage, funcName, module);
            }
            
            llvm::Value* a = codegenExpressionNode(expr->arguments[0].get(), this);
            llvm::Value* b = codegenExpressionNode(expr->arguments[1].get(), this);
            return builder.CreateCall(c_func, {a, b}, "tfp.cmp");
        }
    }
    
    // tfp* math functions: sqrt, sin, cos, exp, log (single arg)
    for (const char* width : {"32", "64"}) {
        for (const char* mathfunc : {"sqrt", "sin", "cos", "exp", "log"}) {
            std::string funcName = "aria_tfp" + std::string(width) + "_" + mathfunc;
            std::string ariaName = "tfp" + std::string(width) + "_" + mathfunc;
            if (callee_ident->name == ariaName || callee_ident->name == "@" + ariaName) {
                if (expr->arguments.size() != 1) {
                    throw std::runtime_error(ariaName + "() requires exactly 1 argument");
                }
                
                llvm::Type* tfp_type = getLLVMType(type_system->getPrimitiveType("tfp" + std::string(width)));
                llvm::FunctionType* c_func_type = llvm::FunctionType::get(tfp_type, {tfp_type}, false);
                llvm::Function* c_func = module->getFunction(funcName);
                if (!c_func) {
                    c_func = llvm::Function::Create(c_func_type, llvm::Function::ExternalLinkage, funcName, module);
                }
                
                llvm::Value* a = codegenExpressionNode(expr->arguments[0].get(), this);
                return builder.CreateCall(c_func, {a}, std::string("tfp.") + mathfunc);
            }
        }
    }
    
    // tfp*_pow(base, exp) -> tfp*
    for (const char* width : {"32", "64"}) {
        std::string funcName = "aria_tfp" + std::string(width) + "_pow";
        std::string ariaName = "tfp" + std::string(width) + "_pow";
        if (callee_ident->name == ariaName || callee_ident->name == "@" + ariaName) {
            if (expr->arguments.size() != 2) {
                throw std::runtime_error(ariaName + "() requires exactly 2 arguments");
            }
            
            llvm::Type* tfp_type = getLLVMType(type_system->getPrimitiveType("tfp" + std::string(width)));
            llvm::FunctionType* c_func_type = llvm::FunctionType::get(tfp_type, {tfp_type, tfp_type}, false);
            llvm::Function* c_func = module->getFunction(funcName);
            if (!c_func) {
                c_func = llvm::Function::Create(c_func_type, llvm::Function::ExternalLinkage, funcName, module);
            }
            
            llvm::Value* a = codegenExpressionNode(expr->arguments[0].get(), this);
            llvm::Value* b = codegenExpressionNode(expr->arguments[1].get(), this);
            return builder.CreateCall(c_func, {a, b}, "tfp.pow");
        }
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
    // WILD MEMORY BUILTINS (Phase 2.2 - Manual Memory Management)
    // ====================================================================
    // Primitive wild memory operations tracked by the borrow checker.
    // Runtime functions: aria_alloc, aria_free, aria_realloc (wild_alloc.cpp)

    // alloc(size: int64) -> wild int8@
    // Allocates 'size' bytes of wild (manual) memory
    if (callee_ident->name == "alloc") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("alloc() requires exactly one argument (size)");
        }

        // Get or declare aria_alloc: void* aria_alloc(size_t size)
        llvm::Function* alloc_func = module->getFunction("aria_alloc");
        if (!alloc_func) {
            llvm::FunctionType* alloc_type = llvm::FunctionType::get(
                builder.getPtrTy(),                    // Return: void* (opaque ptr)
                {builder.getInt64Ty()},                // Args: size_t size
                false);
            alloc_func = llvm::Function::Create(alloc_type,
                llvm::Function::ExternalLinkage, "aria_alloc", module);
        }

        llvm::Value* size = codegenExpressionNode(expr->arguments[0].get(), this);
        
        // Handle LBIM types (int128/256/512) which are represented as structs
        if (size->getType()->isStructTy()) {
            // Extract the first limb (low 64 bits) from the LBIM struct
            // struct.intN = { [M x i64] } where M = N/64
            size = builder.CreateExtractValue(size, {0, 0}, "size.limb0");
        } else if (!size->getType()->isIntegerTy()) {
            throw std::runtime_error("alloc() size must be an integer type");
        }
        
        // Now convert to i64 if needed
        if (!size->getType()->isIntegerTy(64)) {
            size = builder.CreateZExtOrTrunc(size, builder.getInt64Ty(), "size.i64");
        }
        
        return builder.CreateCall(alloc_func, {size}, "wild_ptr");
    }

    // free(ptr: wild any@) -> void
    // Frees a wild pointer allocated by alloc()
    if (callee_ident->name == "free") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("free() requires exactly one argument (pointer)");
        }

        // Get or declare aria_free: void aria_free(void* ptr)
        llvm::Function* free_func = module->getFunction("aria_free");
        if (!free_func) {
            llvm::FunctionType* free_type = llvm::FunctionType::get(
                builder.getVoidTy(),                   // Return: void
                {builder.getPtrTy()},                  // Args: void* ptr
                false);
            free_func = llvm::Function::Create(free_type,
                llvm::Function::ExternalLinkage, "aria_free", module);
        }

        llvm::Value* ptr = codegenExpressionNode(expr->arguments[0].get(), this);
        // Ensure ptr is a pointer type
        if (!ptr->getType()->isPointerTy()) {
            ptr = builder.CreateIntToPtr(ptr, builder.getPtrTy());
        }
        builder.CreateCall(free_func, {ptr});
        return builder.getInt32(0); // Return dummy value for void
    }

    // realloc(ptr: wild any@, new_size: int64) -> wild int8@
    // Reallocates a wild pointer to a new size
    if (callee_ident->name == "realloc") {
        if (expr->arguments.size() != 2) {
            throw std::runtime_error("realloc() requires exactly two arguments (ptr, new_size)");
        }

        // Get or declare aria_realloc: void* aria_realloc(void* ptr, size_t new_size)
        llvm::Function* realloc_func = module->getFunction("aria_realloc");
        if (!realloc_func) {
            llvm::FunctionType* realloc_type = llvm::FunctionType::get(
                builder.getPtrTy(),                              // Return: void*
                {builder.getPtrTy(), builder.getInt64Ty()},      // Args: void* ptr, size_t size
                false);
            realloc_func = llvm::Function::Create(realloc_type,
                llvm::Function::ExternalLinkage, "aria_realloc", module);
        }

        llvm::Value* ptr = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* new_size = codegenExpressionNode(expr->arguments[1].get(), this);

        if (!ptr->getType()->isPointerTy()) {
            ptr = builder.CreateIntToPtr(ptr, builder.getPtrTy());
        }
        if (!new_size->getType()->isIntegerTy(64)) {
            new_size = builder.CreateZExtOrTrunc(new_size, builder.getInt64Ty());
        }
        return builder.CreateCall(realloc_func, {ptr, new_size}, "realloc_ptr");
    }

    // ====================================================================
    // STRING LIBRARY BUILTINS (Phase 4.3)
    // ====================================================================
    
    // Note: AriaString struct is { const char* data, int64_t length }
    // In LLVM IR, this is { i8*, i64 }
    
    // Helper: Get or create AriaString struct type
    auto getAriaStringType = [&]() -> llvm::StructType* {
        llvm::StructType* strType = llvm::StructType::getTypeByName(context, "struct.AriaString");
        if (!strType) {
            std::vector<llvm::Type*> fields = {
                llvm::PointerType::get(builder.getInt8Ty(), 0),
                builder.getInt64Ty()
            };
            strType = llvm::StructType::create(context, fields, "struct.AriaString");
        }
        return strType;
    };
    
    // string_from_cstr(char*) -> string
    if (callee_ident->name == "string_from_cstr") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("string_from_cstr() requires one argument");
        }
        
        llvm::Function* func = module->getFunction("aria_string_from_cstr_simple");
        if (!func) {
            // aria_string_from_cstr_simple returns AriaString* directly (aborts on error)
            std::vector<llvm::Type*> params = {llvm::PointerType::get(builder.getInt8Ty(), 0)};
            llvm::FunctionType* func_type = llvm::FunctionType::get(
                llvm::PointerType::get(getAriaStringType(), 0), params, false);
            func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                "aria_string_from_cstr_simple", module);
        }
        
        llvm::Value* cstr = codegenExpressionNode(expr->arguments[0].get(), this);
        // Cast to i8* if needed
        if (!cstr->getType()->isPointerTy()) {
            cstr = builder.CreateIntToPtr(cstr, llvm::PointerType::get(builder.getInt8Ty(), 0));
        }
        return builder.CreateCall(func, {cstr}, "str_result");
    }
    
    // string_from_char(byte) -> string  
    if (callee_ident->name == "string_from_char") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("string_from_char() requires one argument");
        }
        
        llvm::Function* func = module->getFunction("aria_string_from_char_simple");
        if (!func) {
            // aria_string_from_char_simple returns AriaString* directly (aborts on error)
            std::vector<llvm::Type*> params = {builder.getInt8Ty()};  // uint8_t ch
            llvm::FunctionType* func_type = llvm::FunctionType::get(
                llvm::PointerType::get(getAriaStringType(), 0), params, false);
            func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                "aria_string_from_char_simple", module);
        }
        
        llvm::Value* ch = codegenExpressionNode(expr->arguments[0].get(), this);
        // Truncate to i8 if needed
        if (!ch->getType()->isIntegerTy(8)) {
            ch = builder.CreateTrunc(ch, builder.getInt8Ty());
        }
        
        return builder.CreateCall(func, {ch}, "char_str");
    }
    
    // string_length(string) -> int64
    if (callee_ident->name == "string_length") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("string_length() requires one argument");
        }

        // codegenExpressionNode returns the loaded value (AriaString* for string variables)
        llvm::Value* str_ptr = codegenExpressionNode(expr->arguments[0].get(), this);
        // Load the AriaString struct from that pointer
        llvm::Value* str_struct = builder.CreateLoad(getAriaStringType(), str_ptr, "str");
        // Extract length field (index 1)
        llvm::Value* length = builder.CreateExtractValue(str_struct, {1}, "length");
        return length;
    }
    
    // string_is_empty(string) -> bool
    if (callee_ident->name == "string_is_empty") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("string_is_empty() requires one argument");
        }
        
        llvm::Value* str_ptr = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* str_struct = builder.CreateLoad(getAriaStringType(), str_ptr, "str");
        llvm::Value* length = builder.CreateExtractValue(str_struct, {1}, "length");
        llvm::Value* zero = builder.getInt64(0);
        return builder.CreateICmpEQ(length, zero, "is_empty");
    }
    
    // string_equals(string, string) -> bool
    if (callee_ident->name == "string_equals") {
        if (expr->arguments.size() != 2) {
            throw std::runtime_error("string_equals() requires two arguments");
        }
        
        llvm::Function* func = module->getFunction("aria_string_equals");
        if (!func) {
            std::vector<llvm::Type*> params = {getAriaStringType(), getAriaStringType()};
            llvm::FunctionType* func_type = llvm::FunctionType::get(
                builder.getInt1Ty(), params, false);
            func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                "aria_string_equals", module);
        }
        
        llvm::Value* str1_ptr = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* str2_ptr = codegenExpressionNode(expr->arguments[1].get(), this);
        llvm::Value* str1 = builder.CreateLoad(getAriaStringType(), str1_ptr, "str1");
        llvm::Value* str2 = builder.CreateLoad(getAriaStringType(), str2_ptr, "str2");
        return builder.CreateCall(func, {str1, str2}, "equals");
    }
    
    // string_concat(string, string) -> string
    if (callee_ident->name == "string_concat") {
        if (expr->arguments.size() != 2) {
            throw std::runtime_error("string_concat() requires two arguments");
        }

        llvm::Function* func = module->getFunction("aria_string_concat_simple");
        if (!func) {
            // aria_string_concat_simple returns AriaString* directly (aborts on error)
            std::vector<llvm::Type*> params = {
                llvm::PointerType::get(getAriaStringType(), 0),  // AriaString* a
                llvm::PointerType::get(getAriaStringType(), 0)   // AriaString* b
            };
            llvm::FunctionType* func_type = llvm::FunctionType::get(
                llvm::PointerType::get(getAriaStringType(), 0), params, false);
            func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                "aria_string_concat_simple", module);
        }

        // codegenExpressionNode returns loaded pointers (AriaString* for string variables)
        llvm::Value* str1_ptr = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* str2_ptr = codegenExpressionNode(expr->arguments[1].get(), this);

        return builder.CreateCall(func, {str1_ptr, str2_ptr}, "concat_str");
    }
    
    // string_substring(string, int64, int64) -> string
    if (callee_ident->name == "string_substring") {
        if (expr->arguments.size() != 3) {
            throw std::runtime_error("string_substring() requires three arguments");
        }
        
        llvm::Function* func = module->getFunction("aria_string_substring_simple");
        if (!func) {
            // aria_string_substring_simple(AriaString*, i64, i64) -> AriaString*
            // aborts on out-of-bounds (matches the _simple wrapper convention)
            std::vector<llvm::Type*> params = {
                llvm::PointerType::get(getAriaStringType(), 0),  // str
                builder.getInt64Ty(),                            // start
                builder.getInt64Ty()                             // end
            };
            llvm::FunctionType* func_type = llvm::FunctionType::get(
                llvm::PointerType::get(getAriaStringType(), 0), params, false);
            func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                "aria_string_substring_simple", module);
        }
        
        llvm::Value* str_ptr = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* start = codegenExpressionNode(expr->arguments[1].get(), this);
        llvm::Value* end = codegenExpressionNode(expr->arguments[2].get(), this);
        return builder.CreateCall(func, {str_ptr, start, end}, "substr_result");
    }
    
    // string_contains(string, string) -> bool
    if (callee_ident->name == "string_contains") {
        if (expr->arguments.size() != 2) {
            throw std::runtime_error("string_contains() requires two arguments");
        }
        
        llvm::Function* func = module->getFunction("aria_string_contains");
        if (!func) {
            std::vector<llvm::Type*> params = {getAriaStringType(), getAriaStringType()};
            llvm::FunctionType* func_type = llvm::FunctionType::get(
                builder.getInt1Ty(), params, false);
            func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                "aria_string_contains", module);
        }
        
        llvm::Value* haystack_ptr = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* needle_ptr = codegenExpressionNode(expr->arguments[1].get(), this);
        llvm::Value* haystack = builder.CreateLoad(getAriaStringType(), haystack_ptr, "haystack");
        llvm::Value* needle = builder.CreateLoad(getAriaStringType(), needle_ptr, "needle");
        return builder.CreateCall(func, {haystack, needle}, "contains");
    }
    
    // string_starts_with(string, string) -> bool
    if (callee_ident->name == "string_starts_with") {
        if (expr->arguments.size() != 2) {
            throw std::runtime_error("string_starts_with() requires two arguments");
        }
        
        llvm::Function* func = module->getFunction("aria_string_starts_with");
        if (!func) {
            std::vector<llvm::Type*> params = {getAriaStringType(), getAriaStringType()};
            llvm::FunctionType* func_type = llvm::FunctionType::get(
                builder.getInt1Ty(), params, false);
            func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                "aria_string_starts_with", module);
        }
        
        llvm::Value* str_ptr = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* prefix_ptr = codegenExpressionNode(expr->arguments[1].get(), this);
        llvm::Value* str = builder.CreateLoad(getAriaStringType(), str_ptr, "str");
        llvm::Value* prefix = builder.CreateLoad(getAriaStringType(), prefix_ptr, "prefix");
        return builder.CreateCall(func, {str, prefix}, "starts_with");
    }
    
    // string_ends_with(string, string) -> bool
    if (callee_ident->name == "string_ends_with") {
        if (expr->arguments.size() != 2) {
            throw std::runtime_error("string_ends_with() requires two arguments");
        }
        
        llvm::Function* func = module->getFunction("aria_string_ends_with");
        if (!func) {
            std::vector<llvm::Type*> params = {getAriaStringType(), getAriaStringType()};
            llvm::FunctionType* func_type = llvm::FunctionType::get(
                builder.getInt1Ty(), params, false);
            func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                "aria_string_ends_with", module);
        }
        
        llvm::Value* str_ptr = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* suffix_ptr = codegenExpressionNode(expr->arguments[1].get(), this);
        llvm::Value* str = builder.CreateLoad(getAriaStringType(), str_ptr, "str");
        llvm::Value* suffix = builder.CreateLoad(getAriaStringType(), suffix_ptr, "suffix");
        return builder.CreateCall(func, {str, suffix}, "ends_with");
    }
    
    // string_trim(string) -> string
    if (callee_ident->name == "string_trim") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("string_trim() requires one argument");
        }
        
        llvm::Function* func = module->getFunction("aria_string_trim_simple");
        if (!func) {
            std::vector<llvm::Type*> params = {
                llvm::PointerType::get(getAriaStringType(), 0)  // str (ptr)
            };
            llvm::FunctionType* func_type = llvm::FunctionType::get(
                llvm::PointerType::get(getAriaStringType(), 0), params, false);
            func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                "aria_string_trim_simple", module);
        }
        
        llvm::Value* str_ptr = codegenExpressionNode(expr->arguments[0].get(), this);
        return builder.CreateCall(func, {str_ptr}, "trim_result");
    }
    
    // string_to_upper(string) -> string
    if (callee_ident->name == "string_to_upper") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("string_to_upper() requires one argument");
        }
        
        llvm::Function* func = module->getFunction("aria_string_to_upper_simple");
        if (!func) {
            std::vector<llvm::Type*> params = {
                llvm::PointerType::get(getAriaStringType(), 0)  // str (ptr)
            };
            llvm::FunctionType* func_type = llvm::FunctionType::get(
                llvm::PointerType::get(getAriaStringType(), 0), params, false);
            func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                "aria_string_to_upper_simple", module);
        }
        
        llvm::Value* str_ptr = codegenExpressionNode(expr->arguments[0].get(), this);
        return builder.CreateCall(func, {str_ptr}, "upper_result");
    }
    
    // string_to_lower(string) -> string
    if (callee_ident->name == "string_to_lower") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("string_to_lower() requires one argument");
        }

        llvm::Function* func = module->getFunction("aria_string_to_lower_simple");
        if (!func) {
            std::vector<llvm::Type*> params = {
                llvm::PointerType::get(getAriaStringType(), 0)  // str (ptr)
            };
            llvm::FunctionType* func_type = llvm::FunctionType::get(
                llvm::PointerType::get(getAriaStringType(), 0), params, false);
            func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                "aria_string_to_lower_simple", module);
        }

        llvm::Value* str_ptr = codegenExpressionNode(expr->arguments[0].get(), this);
        return builder.CreateCall(func, {str_ptr}, "lower_result");
    }

    // string_from_int(int64) -> string
    if (callee_ident->name == "string_from_int") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("string_from_int() requires one argument");
        }

        llvm::Function* func = module->getFunction("aria_string_from_int_simple");
        if (!func) {
            std::vector<llvm::Type*> params = {builder.getInt64Ty()};
            llvm::FunctionType* func_type = llvm::FunctionType::get(builder.getPtrTy(), params, false);
            func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                "aria_string_from_int_simple", module);
        }

        llvm::Value* val = codegenExpressionNode(expr->arguments[0].get(), this);
        // Ensure value is i64 (sign extend if smaller)
        if (val->getType() != builder.getInt64Ty()) {
            val = builder.CreateSExt(val, builder.getInt64Ty(), "val_i64");
        }
        return builder.CreateCall(func, {val}, "from_int_result");
    }

    // string_to_int(string) -> int64
    if (callee_ident->name == "string_to_int") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("string_to_int() requires one argument");
        }

        llvm::Function* func = module->getFunction("aria_string_to_int_simple");
        if (!func) {
            std::vector<llvm::Type*> params = {builder.getPtrTy()};
            llvm::FunctionType* func_type = llvm::FunctionType::get(builder.getInt64Ty(), params, false);
            func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                "aria_string_to_int_simple", module);
        }

        llvm::Value* str_ptr = codegenExpressionNode(expr->arguments[0].get(), this);
        return builder.CreateCall(func, {str_ptr}, "to_int_result");
    }

    // string_to_hex(string) -> string
    if (callee_ident->name == "string_to_hex") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("string_to_hex() requires one argument");
        }

        llvm::Function* func = module->getFunction("aria_string_to_hex_simple");
        if (!func) {
            std::vector<llvm::Type*> params = {builder.getPtrTy()};
            llvm::FunctionType* func_type = llvm::FunctionType::get(builder.getPtrTy(), params, false);
            func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                "aria_string_to_hex_simple", module);
        }

        llvm::Value* str_ptr = codegenExpressionNode(expr->arguments[0].get(), this);
        return builder.CreateCall(func, {str_ptr}, "to_hex_result");
    }

    // string_pad_right(string, int64, int8) -> string
    if (callee_ident->name == "string_pad_right") {
        if (expr->arguments.size() != 3) {
            throw std::runtime_error("string_pad_right() requires three arguments");
        }

        llvm::Function* func = module->getFunction("aria_string_pad_right_simple");
        if (!func) {
            std::vector<llvm::Type*> params = {builder.getPtrTy(), builder.getInt64Ty(), builder.getInt8Ty()};
            llvm::FunctionType* func_type = llvm::FunctionType::get(builder.getPtrTy(), params, false);
            func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                "aria_string_pad_right_simple", module);
        }

        llvm::Value* str_ptr = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* total_len = codegenExpressionNode(expr->arguments[1].get(), this);
        llvm::Value* pad_char = codegenExpressionNode(expr->arguments[2].get(), this);
        // Extend total_len to i64 if needed
        if (total_len->getType() != builder.getInt64Ty()) {
            total_len = builder.CreateSExt(total_len, builder.getInt64Ty(), "len_i64");
        }
        // Truncate pad_char to i8 if needed
        if (pad_char->getType() != builder.getInt8Ty()) {
            pad_char = builder.CreateTrunc(pad_char, builder.getInt8Ty(), "pad_i8");
        }
        return builder.CreateCall(func, {str_ptr, total_len, pad_char}, "pad_right_result");
    }

    // string_pad_left(string, int64, int8) -> string
    if (callee_ident->name == "string_pad_left") {
        if (expr->arguments.size() != 3) {
            throw std::runtime_error("string_pad_left() requires three arguments");
        }

        llvm::Function* func = module->getFunction("aria_string_pad_left_simple");
        if (!func) {
            std::vector<llvm::Type*> params = {builder.getPtrTy(), builder.getInt64Ty(), builder.getInt8Ty()};
            llvm::FunctionType* func_type = llvm::FunctionType::get(builder.getPtrTy(), params, false);
            func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                "aria_string_pad_left_simple", module);
        }

        llvm::Value* str_ptr = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* total_len = codegenExpressionNode(expr->arguments[1].get(), this);
        llvm::Value* pad_char = codegenExpressionNode(expr->arguments[2].get(), this);
        if (total_len->getType() != builder.getInt64Ty()) {
            total_len = builder.CreateSExt(total_len, builder.getInt64Ty(), "len_i64");
        }
        if (pad_char->getType() != builder.getInt8Ty()) {
            pad_char = builder.CreateTrunc(pad_char, builder.getInt8Ty(), "pad_i8");
        }
        return builder.CreateCall(func, {str_ptr, total_len, pad_char}, "pad_left_result");
    }

    // string_repeat(string, int64) -> string
    if (callee_ident->name == "string_repeat") {
        if (expr->arguments.size() != 2) {
            throw std::runtime_error("string_repeat() requires two arguments");
        }

        llvm::Function* func = module->getFunction("aria_string_repeat_simple");
        if (!func) {
            std::vector<llvm::Type*> params = {builder.getPtrTy(), builder.getInt64Ty()};
            llvm::FunctionType* func_type = llvm::FunctionType::get(builder.getPtrTy(), params, false);
            func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                "aria_string_repeat_simple", module);
        }

        llvm::Value* str_ptr = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* count = codegenExpressionNode(expr->arguments[1].get(), this);
        if (count->getType() != builder.getInt64Ty()) {
            count = builder.CreateSExt(count, builder.getInt64Ty(), "count_i64");
        }
        return builder.CreateCall(func, {str_ptr, count}, "repeat_result");
    }

    // string_trim_start(string) -> string
    if (callee_ident->name == "string_trim_start") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("string_trim_start() requires one argument");
        }

        llvm::Function* func = module->getFunction("aria_string_trim_start_simple");
        if (!func) {
            std::vector<llvm::Type*> params = {builder.getPtrTy()};
            llvm::FunctionType* func_type = llvm::FunctionType::get(builder.getPtrTy(), params, false);
            func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                "aria_string_trim_start_simple", module);
        }

        llvm::Value* str_ptr = codegenExpressionNode(expr->arguments[0].get(), this);
        return builder.CreateCall(func, {str_ptr}, "trim_start_result");
    }

    // string_trim_end(string) -> string
    if (callee_ident->name == "string_trim_end") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("string_trim_end() requires one argument");
        }

        llvm::Function* func = module->getFunction("aria_string_trim_end_simple");
        if (!func) {
            std::vector<llvm::Type*> params = {builder.getPtrTy()};
            llvm::FunctionType* func_type = llvm::FunctionType::get(builder.getPtrTy(), params, false);
            func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                "aria_string_trim_end_simple", module);
        }

        llvm::Value* str_ptr = codegenExpressionNode(expr->arguments[0].get(), this);
        return builder.CreateCall(func, {str_ptr}, "trim_end_result");
    }

    // string_index_of(string, string) -> int64  (-1 if not found)
    if (callee_ident->name == "string_index_of") {
        if (expr->arguments.size() != 2) {
            throw std::runtime_error("string_index_of() requires two arguments");
        }

        llvm::Function* func = module->getFunction("aria_string_index_of_simple");
        if (!func) {
            std::vector<llvm::Type*> params = {builder.getPtrTy(), builder.getPtrTy()};
            llvm::FunctionType* func_type = llvm::FunctionType::get(builder.getInt64Ty(), params, false);
            func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                "aria_string_index_of_simple", module);
        }

        llvm::Value* haystack_ptr = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* needle_ptr = codegenExpressionNode(expr->arguments[1].get(), this);
        return builder.CreateCall(func, {haystack_ptr, needle_ptr}, "index_of_result");
    }

    // string_from_int_hex(int64) -> string (lowercase hex digits, no prefix)
    if (callee_ident->name == "string_from_int_hex") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("string_from_int_hex() requires one argument");
        }

        llvm::Function* func = module->getFunction("aria_string_from_int_hex_simple");
        if (!func) {
            std::vector<llvm::Type*> params = {builder.getInt64Ty()};
            llvm::FunctionType* func_type = llvm::FunctionType::get(builder.getPtrTy(), params, false);
            func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                "aria_string_from_int_hex_simple", module);
        }

        llvm::Value* val = codegenExpressionNode(expr->arguments[0].get(), this);
        if (val->getType() != builder.getInt64Ty()) {
            val = builder.CreateSExt(val, builder.getInt64Ty(), "hex_val_ext");
        }
        return builder.CreateCall(func, {val}, "from_int_hex_result");
    }

    // string_format_float(float64, int32) -> string
    if (callee_ident->name == "string_format_float") {
        if (expr->arguments.size() != 2) {
            throw std::runtime_error("string_format_float() requires two arguments");
        }

        llvm::Function* func = module->getFunction("aria_string_format_float_simple");
        if (!func) {
            std::vector<llvm::Type*> params = {builder.getDoubleTy(), builder.getInt32Ty()};
            llvm::FunctionType* func_type = llvm::FunctionType::get(builder.getPtrTy(), params, false);
            func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                "aria_string_format_float_simple", module);
        }

        llvm::Value* val = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* precision = codegenExpressionNode(expr->arguments[1].get(), this);
        // Truncate precision to i32 if needed
        if (precision->getType() != builder.getInt32Ty()) {
            precision = builder.CreateTrunc(precision, builder.getInt32Ty(), "prec_i32");
        }
        return builder.CreateCall(func, {val, precision}, "format_float_result");
    }

    // ====================================================================
    // FILE I/O BUILTINS
    // ====================================================================

    // Helper to convert AriaString global to char*
    auto stringToCharPtr = [&](llvm::Value* str_val) -> llvm::Value* {
        // For global AriaString constants (string literals), extract the data field
        // Global AriaStrings are of type %struct.AriaString = { ptr, i64 }
        // We need the first field (the char* pointer)
        if (auto* gv = llvm::dyn_cast<llvm::GlobalVariable>(str_val)) {
            // Use GEP to get pointer to first field (data pointer)
            std::vector<llvm::Value*> indices = {
                builder.getInt32(0),  // Deref the global pointer
                builder.getInt32(0)   // Access first struct field (data)
            };
            llvm::Value* data_ptr_gep = builder.CreateInBoundsGEP(
                getAriaStringType(), gv, indices, "str_data_ptr");
            // Load the char* pointer from the struct
            return builder.CreateLoad(builder.getPtrTy(), data_ptr_gep, "str_data");
        }
        
        // For stack/heap AriaString* variables, load struct and extract data
        if (str_val->getType()->isPointerTy()) {
            llvm::Value* str_struct = builder.CreateLoad(getAriaStringType(), str_val, "str");
            return builder.CreateExtractValue(str_struct, {0}, "str_data");
        }
        
        return str_val;
    };

    // aria_write_file_simple(path: int8*, content: int8*) -> int64
    if (callee_ident->name == "aria_write_file_simple") {
        std::cerr << "[FS DEBUG] aria_write_file_simple codegen called" << std::endl;
        
        if (expr->arguments.size() != 2) {
            throw std::runtime_error("aria_write_file_simple() requires two arguments");
        }

        llvm::Function* func = module->getFunction("aria_write_file_simple");
        if (!func) {
            std::vector<llvm::Type*> params = {builder.getPtrTy(), builder.getPtrTy()};
            llvm::FunctionType* func_type = llvm::FunctionType::get(builder.getInt64Ty(), params, false);
            func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                "aria_write_file_simple", module);
        }

        llvm::Value* path = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* content = codegenExpressionNode(expr->arguments[1].get(), this);
        
        std::cerr << "[FS DEBUG] path type: " << path->getValueName()->getKey().str() << std::endl;
        
        // Convert AriaString to char* - for global constants, extract the data field
        if (auto* gv = llvm::dyn_cast<llvm::GlobalVariable>(path)) {
            std::cerr << "[FS DEBUG] path is GlobalVariable" << std::endl;
            if (auto* init = gv->getInitializer()) {
                std::cerr << "[FS DEBUG] has initializer" << std::endl;
                if (auto* cs = llvm::dyn_cast<llvm::ConstantStruct>(init)) {
                    std::cerr << "[FS DEBUG] is ConstantStruct, extracting field 0" << std::endl;
                    // Extract the first field (data pointer) from the struct constant
                    path = cs->getOperand(0);
                    std::cerr << "[FS DEBUG] extracted path" << std::endl;
                }
            }
        }
        if (auto* gv = llvm::dyn_cast<llvm::GlobalVariable>(content)) {
            if (auto* init = gv->getInitializer()) {
                if (auto* cs = llvm::dyn_cast<llvm::ConstantStruct>(init)) {
                    // Extract the first field (data pointer) from the struct constant
                    content = cs->getOperand(0);
                }
            }
        }
        
        std::cerr << "[FS DEBUG] calling with extracted values" << std::endl;
        return builder.CreateCall(func, {path, content}, "write_result");
    }

    // aria_file_exists(path: int8*) -> bool
    if (callee_ident->name == "aria_file_exists") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("aria_file_exists() requires one argument");
        }

        llvm::Function* func = module->getFunction("aria_file_exists");
        if (!func) {
            std::vector<llvm::Type*> params = {builder.getPtrTy()};
            llvm::FunctionType* func_type = llvm::FunctionType::get(builder.getInt1Ty(), params, false);
            func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                "aria_file_exists", module);
        }

        llvm::Value* path = codegenExpressionNode(expr->arguments[0].get(), this);
        path = stringToCharPtr(path);
        
        return builder.CreateCall(func, {path}, "exists_result");
    }

    // aria_delete_file_simple(path: int8*) -> int64
    if (callee_ident->name == "aria_delete_file_simple") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("aria_delete_file_simple() requires one argument");
        }

        llvm::Function* func = module->getFunction("aria_delete_file_simple");
        if (!func) {
            std::vector<llvm::Type*> params = {builder.getPtrTy()};
            llvm::FunctionType* func_type = llvm::FunctionType::get(builder.getInt64Ty(), params, false);
            func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                "aria_delete_file_simple", module);
        }

        llvm::Value* path = codegenExpressionNode(expr->arguments[0].get(), this);
        path = stringToCharPtr(path);
        
        return builder.CreateCall(func, {path}, "delete_result");
    }

    // aria_read_file_simple(path: int8*) -> string
    if (callee_ident->name == "aria_read_file_simple") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("aria_read_file_simple() requires one argument");
        }

        llvm::Function* func = module->getFunction("aria_read_file_simple");
        if (!func) {
            std::vector<llvm::Type*> params = {builder.getPtrTy()};
            llvm::FunctionType* func_type = llvm::FunctionType::get(builder.getPtrTy(), params, false);
            func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                "aria_read_file_simple", module);
        }

        llvm::Value* path = codegenExpressionNode(expr->arguments[0].get(), this);
        path = stringToCharPtr(path);
        
        return builder.CreateCall(func, {path}, "read_result");
    }

    // ====================================================================
    // TBB (Tagged Balanced Base) TYPE BUILTINS
    // ====================================================================

    // Helper lambda for TBB ERR sentinel values
    auto getTBBErrSentinel = [&](unsigned bits) -> llvm::Value* {
        llvm::IntegerType* ty = llvm::IntegerType::get(context, bits);
        int64_t errVal;
        switch (bits) {
            case 8:  errVal = -128; break;
            case 16: errVal = -32768; break;
            case 32: errVal = -2147483648LL; break;
            case 64: errVal = INT64_MIN; break;
            default: errVal = INT64_MIN;
        }
        return llvm::ConstantInt::get(ty, errVal, true);
    };

    // tbb64_from_int(int64) -> tbb64
    if (callee_ident->name == "tbb64_from_int") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("tbb64_from_int() requires one argument");
        }
        llvm::Value* val = codegenExpressionNode(expr->arguments[0].get(), this);
        // TBB64 uses INT64_MIN as ERR, so all int64 values except INT64_MIN are valid
        // If input is INT64_MIN, return ERR
        llvm::Value* errSentinel = getTBBErrSentinel(64);
        llvm::Value* isErr = builder.CreateICmpEQ(val, errSentinel, "is_min");
        return builder.CreateSelect(isErr, errSentinel, val, "tbb64_result");
    }

    // tbb64_is_err(tbb64) -> bool
    if (callee_ident->name == "tbb64_is_err") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("tbb64_is_err() requires one argument");
        }
        llvm::Value* val = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* errSentinel = getTBBErrSentinel(64);
        return builder.CreateICmpEQ(val, errSentinel, "is_err");
    }

    // tbb64_to_int(tbb64) -> int64
    if (callee_ident->name == "tbb64_to_int") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("tbb64_to_int() requires one argument");
        }
        // Simply return the value - caller should check is_err first
        return codegenExpressionNode(expr->arguments[0].get(), this);
    }

    // tbb32_from_int(int32) -> tbb32
    // ARIA SAFETY FIX (Gemini Batch 02, BUG-004): Range-check BEFORE truncation
    // Prevents wraparound (e.g., 2147483648 → -2147483648 instead of ERR)
    if (callee_ident->name == "tbb32_from_int") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("tbb32_from_int() requires one argument");
        }
        llvm::Value* val = codegenExpressionNode(expr->arguments[0].get(), this);
        
        // TBB32 valid range: [-2147483647, +2147483647] (symmetric)
        // Sentinel: -2147483648 (INT32_MIN)
        llvm::Value* errSentinel = getTBBErrSentinel(32);
        unsigned srcWidth = val->getType()->getIntegerBitWidth();
        
        if (srcWidth > 32) {
            // Source is wider than destination - MUST range check before truncation
            llvm::Value* tbb32_max = llvm::ConstantInt::get(val->getType(), 2147483647LL);
            llvm::Value* tbb32_min = llvm::ConstantInt::get(val->getType(), -2147483647LL);
            
            llvm::Value* tooHigh = builder.CreateICmpSGT(val, tbb32_max, "tbb32_too_high");
            llvm::Value* tooLow = builder.CreateICmpSLT(val, tbb32_min, "tbb32_too_low");
            llvm::Value* outOfRange = builder.CreateOr(tooHigh, tooLow, "tbb32_out_of_range");
            
            llvm::Value* truncated = builder.CreateTrunc(val, builder.getInt32Ty(), "trunc32");
            return builder.CreateSelect(outOfRange, errSentinel, truncated, "tbb32_safe");
        } else if (srcWidth < 32) {
            // Source is narrower - sign extend (safe, preserves value)
            val = builder.CreateSExt(val, builder.getInt32Ty(), "sext32");
        }
        // else: srcWidth == 32, already correct size
        
        // Final check: is the value itself the sentinel?
        llvm::Value* isErr = builder.CreateICmpEQ(val, errSentinel, "is_min");
        return builder.CreateSelect(isErr, errSentinel, val, "tbb32_result");
    }

    // tbb32_is_err(tbb32) -> bool
    if (callee_ident->name == "tbb32_is_err") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("tbb32_is_err() requires one argument");
        }
        llvm::Value* val = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* errSentinel = getTBBErrSentinel(32);
        return builder.CreateICmpEQ(val, errSentinel, "is_err");
    }

    // tbb32_to_int(tbb32) -> int32
    if (callee_ident->name == "tbb32_to_int") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("tbb32_to_int() requires one argument");
        }
        return codegenExpressionNode(expr->arguments[0].get(), this);
    }

    // tbb16_from_int(int16) -> tbb16
    // ARIA SAFETY FIX (Gemini Batch 02, BUG-004): Range-check BEFORE truncation
    // Prevents wraparound (e.g., 35000 → -30536 instead of ERR)
    if (callee_ident->name == "tbb16_from_int") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("tbb16_from_int() requires one argument");
        }
        llvm::Value* val = codegenExpressionNode(expr->arguments[0].get(), this);
        
        // TBB16 valid range: [-32767, +32767] (symmetric)
        // Sentinel: -32768 (INT16_MIN)
        llvm::Value* errSentinel = getTBBErrSentinel(16);
        unsigned srcWidth = val->getType()->getIntegerBitWidth();
        
        if (srcWidth > 16) {
            // Source is wider than destination - MUST range check before truncation
            llvm::Value* tbb16_max = llvm::ConstantInt::get(val->getType(), 32767);
            llvm::Value* tbb16_min = llvm::ConstantInt::get(val->getType(), -32767);
            
            llvm::Value* tooHigh = builder.CreateICmpSGT(val, tbb16_max, "tbb16_too_high");
            llvm::Value* tooLow = builder.CreateICmpSLT(val, tbb16_min, "tbb16_too_low");
            llvm::Value* outOfRange = builder.CreateOr(tooHigh, tooLow, "tbb16_out_of_range");
            
            llvm::Value* truncated = builder.CreateTrunc(val, builder.getInt16Ty(), "trunc16");
            return builder.CreateSelect(outOfRange, errSentinel, truncated, "tbb16_safe");
        } else if (srcWidth < 16) {
            // Source is narrower - sign extend (safe, preserves value)
            val = builder.CreateSExt(val, builder.getInt16Ty(), "sext16");
        }
        // else: srcWidth == 16, already correct size
        
        // Final check: is the value itself the sentinel?
        llvm::Value* isErr = builder.CreateICmpEQ(val, errSentinel, "is_min");
        return builder.CreateSelect(isErr, errSentinel, val, "tbb16_result");
    }

    // tbb16_is_err(tbb16) -> bool
    if (callee_ident->name == "tbb16_is_err") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("tbb16_is_err() requires one argument");
        }
        llvm::Value* val = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* errSentinel = getTBBErrSentinel(16);
        return builder.CreateICmpEQ(val, errSentinel, "is_err");
    }

    // tbb16_to_int(tbb16) -> int16
    if (callee_ident->name == "tbb16_to_int") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("tbb16_to_int() requires one argument");
        }
        return codegenExpressionNode(expr->arguments[0].get(), this);
    }

    // tbb8_from_int(int8) -> tbb8
    // ARIA SAFETY FIX (Gemini Batch 02, BUG-004): Range-check BEFORE truncation
    // Prevents wraparound (e.g., 300 → 44 instead of ERR)
    // Physical consequence prevented: Force limit bypass in robot control
    if (callee_ident->name == "tbb8_from_int") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("tbb8_from_int() requires one argument");
        }
        llvm::Value* val = codegenExpressionNode(expr->arguments[0].get(), this);
        
        // TBB8 valid range: [-127, +127] (symmetric)
        // Sentinel: -128 (INT8_MIN)
        llvm::Value* errSentinel = getTBBErrSentinel(8);
        unsigned srcWidth = val->getType()->getIntegerBitWidth();
        
        if (srcWidth > 8) {
            // Source is wider than destination - MUST range check before truncation
            llvm::Value* tbb8_max = llvm::ConstantInt::get(val->getType(), 127);
            llvm::Value* tbb8_min = llvm::ConstantInt::get(val->getType(), -127);
            
            llvm::Value* tooHigh = builder.CreateICmpSGT(val, tbb8_max, "tbb8_too_high");
            llvm::Value* tooLow = builder.CreateICmpSLT(val, tbb8_min, "tbb8_too_low");
            llvm::Value* outOfRange = builder.CreateOr(tooHigh, tooLow, "tbb8_out_of_range");
            
            llvm::Value* truncated = builder.CreateTrunc(val, builder.getInt8Ty(), "trunc8");
            return builder.CreateSelect(outOfRange, errSentinel, truncated, "tbb8_safe");
        }
        // else: srcWidth <= 8, already fits (i8 or smaller)
        
        // Final check: is the value itself the sentinel?
        llvm::Value* isErr = builder.CreateICmpEQ(val, errSentinel, "is_min");
        return builder.CreateSelect(isErr, errSentinel, val, "tbb8_result");
    }

    // tbb8_is_err(tbb8) -> bool
    if (callee_ident->name == "tbb8_is_err") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("tbb8_is_err() requires one argument");
        }
        llvm::Value* val = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* errSentinel = getTBBErrSentinel(8);
        return builder.CreateICmpEQ(val, errSentinel, "is_err");
    }

    // tbb8_to_int(tbb8) -> int8
    if (callee_ident->name == "tbb8_to_int") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("tbb8_to_int() requires one argument");
        }
        return codegenExpressionNode(expr->arguments[0].get(), this);
    }

    // ====================================================================
    // ARIA-018: TBB SENTINEL-PRESERVING WIDENING INTRINSICS
    // ====================================================================
    // These functions implement safe widening that prevents "sentinel healing"
    // where tbb8 ERR (-128) would become a valid value in tbb16 (-128 != -32768)

    // tbb16_from_tbb8(tbb8) -> tbb16
    if (callee_ident->name == "tbb16_from_tbb8") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("tbb16_from_tbb8() requires one argument");
        }
        llvm::Value* srcVal = codegenExpressionNode(expr->arguments[0].get(), this);
        PrimitiveType srcType("tbb8");
        PrimitiveType dstType("tbb16");
        return tbb_codegen.generateWiden(srcVal, &srcType, &dstType);
    }

    // tbb32_from_tbb8(tbb8) -> tbb32
    if (callee_ident->name == "tbb32_from_tbb8") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("tbb32_from_tbb8() requires one argument");
        }
        llvm::Value* srcVal = codegenExpressionNode(expr->arguments[0].get(), this);
        PrimitiveType srcType("tbb8");
        PrimitiveType dstType("tbb32");
        return tbb_codegen.generateWiden(srcVal, &srcType, &dstType);
    }

    // tbb32_from_tbb16(tbb16) -> tbb32
    if (callee_ident->name == "tbb32_from_tbb16") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("tbb32_from_tbb16() requires one argument");
        }
        llvm::Value* srcVal = codegenExpressionNode(expr->arguments[0].get(), this);
        PrimitiveType srcType("tbb16");
        PrimitiveType dstType("tbb32");
        return tbb_codegen.generateWiden(srcVal, &srcType, &dstType);
    }

    // tbb64_from_tbb8(tbb8) -> tbb64
    if (callee_ident->name == "tbb64_from_tbb8") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("tbb64_from_tbb8() requires one argument");
        }
        llvm::Value* srcVal = codegenExpressionNode(expr->arguments[0].get(), this);
        PrimitiveType srcType("tbb8");
        PrimitiveType dstType("tbb64");
        return tbb_codegen.generateWiden(srcVal, &srcType, &dstType);
    }

    // tbb64_from_tbb16(tbb16) -> tbb64
    if (callee_ident->name == "tbb64_from_tbb16") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("tbb64_from_tbb16() requires one argument");
        }
        llvm::Value* srcVal = codegenExpressionNode(expr->arguments[0].get(), this);
        PrimitiveType srcType("tbb16");
        PrimitiveType dstType("tbb64");
        return tbb_codegen.generateWiden(srcVal, &srcType, &dstType);
    }

    // tbb64_from_tbb32(tbb32) -> tbb64
    if (callee_ident->name == "tbb64_from_tbb32") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("tbb64_from_tbb32() requires one argument");
        }
        llvm::Value* srcVal = codegenExpressionNode(expr->arguments[0].get(), this);
        PrimitiveType srcType("tbb32");
        PrimitiveType dstType("tbb64");
        return tbb_codegen.generateWiden(srcVal, &srcType, &dstType);
    }

    // tbb_widen_8_to_16(tbb8) -> tbb16 (deprecated - use tbb16_from_tbb8)
    if (callee_ident->name == "tbb_widen_8_to_16") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("tbb_widen_8_to_16() requires one argument");
        }
        llvm::Value* val = codegenExpressionNode(expr->arguments[0].get(), this);
        return generateTBBWiden(val, builder.getInt16Ty());
    }

    // tbb_widen_8_to_32(tbb8) -> tbb32
    if (callee_ident->name == "tbb_widen_8_to_32") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("tbb_widen_8_to_32() requires one argument");
        }
        llvm::Value* val = codegenExpressionNode(expr->arguments[0].get(), this);
        return generateTBBWiden(val, builder.getInt32Ty());
    }

    // tbb_widen_8_to_64(tbb8) -> tbb64
    if (callee_ident->name == "tbb_widen_8_to_64") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("tbb_widen_8_to_64() requires one argument");
        }
        llvm::Value* val = codegenExpressionNode(expr->arguments[0].get(), this);
        return generateTBBWiden(val, builder.getInt64Ty());
    }

    // tbb_widen_16_to_32(tbb16) -> tbb32
    if (callee_ident->name == "tbb_widen_16_to_32") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("tbb_widen_16_to_32() requires one argument");
        }
        llvm::Value* val = codegenExpressionNode(expr->arguments[0].get(), this);
        return generateTBBWiden(val, builder.getInt32Ty());
    }

    // tbb_widen_16_to_64(tbb16) -> tbb64
    if (callee_ident->name == "tbb_widen_16_to_64") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("tbb_widen_16_to_64() requires one argument");
        }
        llvm::Value* val = codegenExpressionNode(expr->arguments[0].get(), this);
        return generateTBBWiden(val, builder.getInt64Ty());
    }

    // tbb_widen_32_to_64(tbb32) -> tbb64
    if (callee_ident->name == "tbb_widen_32_to_64") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("tbb_widen_32_to_64() requires one argument");
        }
        llvm::Value* val = codegenExpressionNode(expr->arguments[0].get(), this);
        return generateTBBWiden(val, builder.getInt64Ty());
    }

    // ====================================================================
    // TBB ARITHMETIC OPERATIONS
    // ====================================================================

    // Helper lambda to call TBB runtime functions
    auto callTBBRuntime = [&](const std::string& func_name, llvm::Type* tbbType, 
                              const std::vector<llvm::Value*>& args) -> llvm::Value* {
        llvm::Function* func = module->getFunction(func_name);
        if (!func) {
            std::vector<llvm::Type*> paramTypes;
            for (auto* arg : args) {
                paramTypes.push_back(arg->getType());
            }
            llvm::FunctionType* funcType = llvm::FunctionType::get(tbbType, paramTypes, false);
            func = llvm::Function::Create(funcType, llvm::Function::ExternalLinkage, func_name, module);
        }
        return builder.CreateCall(func, args, "tbb_result");
    };

    // tbb64 arithmetic operations
    if (callee_ident->name == "tbb64_add") {
        if (expr->arguments.size() != 2) {
            throw std::runtime_error("tbb64_add() requires two arguments");
        }
        llvm::Value* a = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* b = codegenExpressionNode(expr->arguments[1].get(), this);
        return callTBBRuntime("tbb64_add", builder.getInt64Ty(), {a, b});
    }

    if (callee_ident->name == "tbb64_sub") {
        if (expr->arguments.size() != 2) {
            throw std::runtime_error("tbb64_sub() requires two arguments");
        }
        llvm::Value* a = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* b = codegenExpressionNode(expr->arguments[1].get(), this);
        return callTBBRuntime("tbb64_sub", builder.getInt64Ty(), {a, b});
    }

    if (callee_ident->name == "tbb64_mul") {
        if (expr->arguments.size() != 2) {
            throw std::runtime_error("tbb64_mul() requires two arguments");
        }
        llvm::Value* a = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* b = codegenExpressionNode(expr->arguments[1].get(), this);
        return callTBBRuntime("tbb64_mul", builder.getInt64Ty(), {a, b});
    }

    if (callee_ident->name == "tbb64_div") {
        if (expr->arguments.size() != 2) {
            throw std::runtime_error("tbb64_div() requires two arguments");
        }
        llvm::Value* a = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* b = codegenExpressionNode(expr->arguments[1].get(), this);
        return callTBBRuntime("tbb64_div", builder.getInt64Ty(), {a, b});
    }

    if (callee_ident->name == "tbb64_neg") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("tbb64_neg() requires one argument");
        }
        llvm::Value* a = codegenExpressionNode(expr->arguments[0].get(), this);
        return callTBBRuntime("tbb64_neg", builder.getInt64Ty(), {a});
    }

    if (callee_ident->name == "tbb64_abs") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("tbb64_abs() requires one argument");
        }
        llvm::Value* a = codegenExpressionNode(expr->arguments[0].get(), this);
        return callTBBRuntime("tbb64_abs", builder.getInt64Ty(), {a});
    }

    // tbb32 arithmetic operations
    if (callee_ident->name == "tbb32_add") {
        if (expr->arguments.size() != 2) {
            throw std::runtime_error("tbb32_add() requires two arguments");
        }
        llvm::Value* a = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* b = codegenExpressionNode(expr->arguments[1].get(), this);
        return callTBBRuntime("tbb32_add", builder.getInt32Ty(), {a, b});
    }

    if (callee_ident->name == "tbb32_sub") {
        if (expr->arguments.size() != 2) {
            throw std::runtime_error("tbb32_sub() requires two arguments");
        }
        llvm::Value* a = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* b = codegenExpressionNode(expr->arguments[1].get(), this);
        return callTBBRuntime("tbb32_sub", builder.getInt32Ty(), {a, b});
    }

    if (callee_ident->name == "tbb32_mul") {
        if (expr->arguments.size() != 2) {
            throw std::runtime_error("tbb32_mul() requires two arguments");
        }
        llvm::Value* a = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* b = codegenExpressionNode(expr->arguments[1].get(), this);
        return callTBBRuntime("tbb32_mul", builder.getInt32Ty(), {a, b});
    }

    if (callee_ident->name == "tbb32_div") {
        if (expr->arguments.size() != 2) {
            throw std::runtime_error("tbb32_div() requires two arguments");
        }
        llvm::Value* a = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* b = codegenExpressionNode(expr->arguments[1].get(), this);
        return callTBBRuntime("tbb32_div", builder.getInt32Ty(), {a, b});
    }

    if (callee_ident->name == "tbb32_neg") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("tbb32_neg() requires one argument");
        }
        llvm::Value* a = codegenExpressionNode(expr->arguments[0].get(), this);
        return callTBBRuntime("tbb32_neg", builder.getInt32Ty(), {a});
    }

    if (callee_ident->name == "tbb32_abs") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("tbb32_abs() requires one argument");
        }
        llvm::Value* a = codegenExpressionNode(expr->arguments[0].get(), this);
        return callTBBRuntime("tbb32_abs", builder.getInt32Ty(), {a});
    }

    // tbb16 arithmetic operations
    if (callee_ident->name == "tbb16_add") {
        if (expr->arguments.size() != 2) {
            throw std::runtime_error("tbb16_add() requires two arguments");
        }
        llvm::Value* a = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* b = codegenExpressionNode(expr->arguments[1].get(), this);
        return callTBBRuntime("tbb16_add", builder.getInt16Ty(), {a, b});
    }

    if (callee_ident->name == "tbb16_sub") {
        if (expr->arguments.size() != 2) {
            throw std::runtime_error("tbb16_sub() requires two arguments");
        }
        llvm::Value* a = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* b = codegenExpressionNode(expr->arguments[1].get(), this);
        return callTBBRuntime("tbb16_sub", builder.getInt16Ty(), {a, b});
    }

    if (callee_ident->name == "tbb16_mul") {
        if (expr->arguments.size() != 2) {
            throw std::runtime_error("tbb16_mul() requires two arguments");
        }
        llvm::Value* a = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* b = codegenExpressionNode(expr->arguments[1].get(), this);
        return callTBBRuntime("tbb16_mul", builder.getInt16Ty(), {a, b});
    }

    if (callee_ident->name == "tbb16_div") {
        if (expr->arguments.size() != 2) {
            throw std::runtime_error("tbb16_div() requires two arguments");
        }
        llvm::Value* a = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* b = codegenExpressionNode(expr->arguments[1].get(), this);
        return callTBBRuntime("tbb16_div", builder.getInt16Ty(), {a, b});
    }

    if (callee_ident->name == "tbb16_neg") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("tbb16_neg() requires one argument");
        }
        llvm::Value* a = codegenExpressionNode(expr->arguments[0].get(), this);
        return callTBBRuntime("tbb16_neg", builder.getInt16Ty(), {a});
    }

    if (callee_ident->name == "tbb16_abs") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("tbb16_abs() requires one argument");
        }
        llvm::Value* a = codegenExpressionNode(expr->arguments[0].get(), this);
        return callTBBRuntime("tbb16_abs", builder.getInt16Ty(), {a});
    }

    // tbb8 arithmetic operations
    if (callee_ident->name == "tbb8_add") {
        if (expr->arguments.size() != 2) {
            throw std::runtime_error("tbb8_add() requires two arguments");
        }
        llvm::Value* a = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* b = codegenExpressionNode(expr->arguments[1].get(), this);
        return callTBBRuntime("tbb8_add", builder.getInt8Ty(), {a, b});
    }

    if (callee_ident->name == "tbb8_sub") {
        if (expr->arguments.size() != 2) {
            throw std::runtime_error("tbb8_sub() requires two arguments");
        }
        llvm::Value* a = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* b = codegenExpressionNode(expr->arguments[1].get(), this);
        return callTBBRuntime("tbb8_sub", builder.getInt8Ty(), {a, b});
    }

    if (callee_ident->name == "tbb8_mul") {
        if (expr->arguments.size() != 2) {
            throw std::runtime_error("tbb8_mul() requires two arguments");
        }
        llvm::Value* a = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* b = codegenExpressionNode(expr->arguments[1].get(), this);
        return callTBBRuntime("tbb8_mul", builder.getInt8Ty(), {a, b});
    }

    if (callee_ident->name == "tbb8_div") {
        if (expr->arguments.size() != 2) {
            throw std::runtime_error("tbb8_div() requires two arguments");
        }
        llvm::Value* a = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* b = codegenExpressionNode(expr->arguments[1].get(), this);
        return callTBBRuntime("tbb8_div", builder.getInt8Ty(), {a, b});
    }

    if (callee_ident->name == "tbb8_neg") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("tbb8_neg() requires one argument");
        }
        llvm::Value* a = codegenExpressionNode(expr->arguments[0].get(), this);
        return callTBBRuntime("tbb8_neg", builder.getInt8Ty(), {a});
    }

    if (callee_ident->name == "tbb8_abs") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("tbb8_abs() requires one argument");
        }
        llvm::Value* a = codegenExpressionNode(expr->arguments[0].get(), this);
        return callTBBRuntime("tbb8_abs", builder.getInt8Ty(), {a});
    }

    // ====================================================================
    // EXOTIC TYPE CONSTRUCTORS (LBIM, Fixed Point)
    // ====================================================================
    
    // uint1024_from_int(int64) -> uint1024
    if (callee_ident->name == "uint1024_from_int") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("uint1024_from_int() requires one argument");
        }
        llvm::Value* val = codegenExpressionNode(expr->arguments[0].get(), this);
        
        // Get uint1024 struct type: { [16 x i64] }
        llvm::Type* i64Type = builder.getInt64Ty();
        llvm::ArrayType* limbsArray = llvm::ArrayType::get(i64Type, 16);
        llvm::StructType* uint1024Type = llvm::StructType::get(context, {limbsArray}, false);
        
        // Create zero-initialized struct
        llvm::Value* result = llvm::ConstantAggregateZero::get(uint1024Type);
        
        // Set first limb to input value (little-endian)
        llvm::Value* firstLimb = builder.CreateZExtOrTrunc(val, i64Type);
        result = builder.CreateInsertValue(result, firstLimb, {0, 0});
        
        return result;
    }
    
    // int1024_from_int(int64) -> int1024
    if (callee_ident->name == "int1024_from_int") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("int1024_from_int() requires one argument");
        }
        llvm::Value* val = codegenExpressionNode(expr->arguments[0].get(), this);
        
        // int1024 is { [16 x i64] } - signed
        llvm::Type* i64Type = builder.getInt64Ty();
        llvm::ArrayType* limbsArray = llvm::ArrayType::get(i64Type, 16);
        llvm::StructType* int1024Type = llvm::StructType::get(context, {limbsArray}, false);
        
        // Sign-extend to first limb
        llvm::Value* signExtended = builder.CreateSExtOrTrunc(val, i64Type);
        
        // For negative numbers, fill high limbs with -1, otherwise 0
        llvm::Value* isNegative = builder.CreateICmpSLT(signExtended, 
            llvm::ConstantInt::get(i64Type, 0));
        llvm::Value* fillValue = builder.CreateSelect(isNegative,
            llvm::ConstantInt::get(i64Type, -1),
            llvm::ConstantInt::get(i64Type, 0));
        
        // Create result with all limbs set to fill value
        llvm::Value* result = llvm::UndefValue::get(int1024Type);
        for (unsigned i = 0; i < 16; i++) {
            llvm::Value* limbVal = (i == 0) ? signExtended : fillValue;
            result = builder.CreateInsertValue(result, limbVal, {0, i});
        }
        
        return result;
    }
    
    // fix256_from_int(int64) -> fix256
    // ARIA-025: Create deterministic fixed-point from integer
    if (callee_ident->name == "fix256_from_int") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("fix256_from_int() requires one argument");
        }
        llvm::Value* val = codegenExpressionNode(expr->arguments[0].get(), this);
        
        // Get THE SAME fix256 struct type used in getLLVMTypeFromString
        // CRITICAL: Must use named type "struct.fix256", not anonymous struct
        // Otherwise LLVM treats them as incompatible types!
        llvm::StructType* fix256Type = llvm::StructType::getTypeByName(context, "struct.fix256");
        if (!fix256Type) {
            llvm::Type* i64Type = builder.getInt64Ty();
            llvm::ArrayType* limbsArray = llvm::ArrayType::get(i64Type, 4);
            fix256Type = llvm::StructType::create(context, {limbsArray}, "struct.fix256");
        }
        
        // Call runtime conversion function: aria_fix256_from_i64(int64) -> fix256
        llvm::FunctionType* funcType = llvm::FunctionType::get(
            fix256Type,
            {builder.getInt64Ty()},
            false
        );
        
        llvm::Function* runtimeFunc = module->getFunction("aria_fix256_from_i64");
        if (!runtimeFunc) {
            runtimeFunc = llvm::Function::Create(
                funcType,
                llvm::Function::ExternalLinkage,
                "aria_fix256_from_i64",
                module
            );
        }
        
        // Convert argument to i64 if needed
        llvm::Value* i64Val = builder.CreateSExtOrTrunc(val, builder.getInt64Ty());
        
        return builder.CreateCall(runtimeFunc, {i64Val}, "fix256_from_int_result");
    }
    
    // fix256_from_float(flt64) -> fix256
    // ARIA-025: Create deterministic fixed-point from float (APPROXIMATE - for I/O only!)
    if (callee_ident->name == "fix256_from_float") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("fix256_from_float() requires one argument");
        }
        llvm::Value* val = codegenExpressionNode(expr->arguments[0].get(), this);
        
        // Get THE SAME fix256 struct type used in getLLVMTypeFromString
        llvm::StructType* fix256Type = llvm::StructType::getTypeByName(context, "struct.fix256");
        if (!fix256Type) {
            llvm::Type* i64Type = builder.getInt64Ty();
            llvm::ArrayType* limbsArray = llvm::ArrayType::get(i64Type, 4);
            fix256Type = llvm::StructType::create(context, {limbsArray}, "struct.fix256");
        }
        
        // Call runtime conversion function: aria_fix256_from_f64(double) -> fix256
        llvm::FunctionType* funcType = llvm::FunctionType::get(
            fix256Type,
            {builder.getDoubleTy()},
            false
        );
        
        llvm::Function* runtimeFunc = module->getFunction("aria_fix256_from_f64");
        if (!runtimeFunc) {
            runtimeFunc = llvm::Function::Create(
                funcType,
                llvm::Function::ExternalLinkage,
                "aria_fix256_from_f64",
                module
            );
        }
        
        // Convert argument to double if needed
        llvm::Value* doubleVal = val;
        if (val->getType()->isFloatTy()) {
            doubleVal = builder.CreateFPExt(val, builder.getDoubleTy());
        }
        
        return builder.CreateCall(runtimeFunc, {doubleVal}, "fix256_from_float_result");
    }
    
    // fix256_to_int(fix256) -> int64
    // ARIA-025: Extract integer part from deterministic fixed-point
    if (callee_ident->name == "fix256_to_int") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("fix256_to_int() requires one argument");
        }
        llvm::Value* val = codegenExpressionNode(expr->arguments[0].get(), this);
        
        // Get THE SAME fix256 struct type used in getLLVMTypeFromString
        llvm::StructType* fix256Type = llvm::StructType::getTypeByName(context, "struct.fix256");
        if (!fix256Type) {
            llvm::Type* i64Type = builder.getInt64Ty();
            llvm::ArrayType* limbsArray = llvm::ArrayType::get(i64Type, 4);
            fix256Type = llvm::StructType::create(context, {limbsArray}, "struct.fix256");
        }
        
        // Call runtime conversion function: aria_fix256_to_i64(fix256) -> int64
        llvm::FunctionType* funcType = llvm::FunctionType::get(
            builder.getInt64Ty(),
            {fix256Type},
            false
        );
        
        llvm::Function* runtimeFunc = module->getFunction("aria_fix256_to_i64");
        if (!runtimeFunc) {
            runtimeFunc = llvm::Function::Create(
                funcType,
                llvm::Function::ExternalLinkage,
                "aria_fix256_to_i64",
                module
            );
        }
        
        return builder.CreateCall(runtimeFunc, {val}, "fix256_to_int_result");
    }
    
    // fix256_to_float(fix256) -> flt64
    // ARIA-025: Convert fixed-point to float (APPROXIMATE - for display only!)
    if (callee_ident->name == "fix256_to_float") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("fix256_to_float() requires one argument");
        }
        llvm::Value* val = codegenExpressionNode(expr->arguments[0].get(), this);
        
        // Get THE SAME fix256 struct type used in getLLVMTypeFromString
        llvm::StructType* fix256Type = llvm::StructType::getTypeByName(context, "struct.fix256");
        if (!fix256Type) {
            llvm::Type* i64Type = builder.getInt64Ty();
            llvm::ArrayType* limbsArray = llvm::ArrayType::get(i64Type, 4);
            fix256Type = llvm::StructType::create(context, {limbsArray}, "struct.fix256");
        }
        
        // Call runtime conversion function: aria_fix256_to_f64(fix256) -> double
        llvm::FunctionType* funcType = llvm::FunctionType::get(
            builder.getDoubleTy(),
            {fix256Type},
            false
        );
        
        llvm::Function* runtimeFunc = module->getFunction("aria_fix256_to_f64");
        if (!runtimeFunc) {
            runtimeFunc = llvm::Function::Create(
                funcType,
                llvm::Function::ExternalLinkage,
                "aria_fix256_to_f64",
                module
            );
        }
        
        return builder.CreateCall(runtimeFunc, {val}, "fix256_to_float_result");
    }
    
    // trit_from_int(int64) -> trit
    if (callee_ident->name == "trit_from_int") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("trit_from_int() requires one argument");
        }
        llvm::Value* val = codegenExpressionNode(expr->arguments[0].get(), this);
        // Truncate to i8 (balanced ternary: -1, 0, 1)
        return builder.CreateTrunc(val, builder.getInt8Ty());
    }
    
    // nit_from_int(int64) -> nit
    if (callee_ident->name == "nit_from_int") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("nit_from_int() requires one argument");
        }
        llvm::Value* val = codegenExpressionNode(expr->arguments[0].get(), this);
        // Truncate to i8 (balanced nonary: -4 to 4)
        return builder.CreateTrunc(val, builder.getInt8Ty());
    }
    
    // tryte_from_int(int64) -> tryte (10 trits packed in uint16)
    if (callee_ident->name == "tryte_from_int") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("tryte_from_int() requires one argument");
        }
        llvm::Value* val = codegenExpressionNode(expr->arguments[0].get(), this);
        // Truncate to i16 (proper encoding needs runtime support)
        return builder.CreateTrunc(val, builder.getInt16Ty());
    }
    
    // nyte_from_int(int64) -> nyte (5 nits packed in uint16)
    if (callee_ident->name == "nyte_from_int") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("nyte_from_int() requires one argument");
        }
        llvm::Value* val = codegenExpressionNode(expr->arguments[0].get(), this);
        // Truncate to i16 (proper encoding needs runtime support)
        return builder.CreateTrunc(val, builder.getInt16Ty());
    }
    
    // ====================================================================
    // TRIT/NIT LOGIC AND ARITHMETIC INTRINSICS
    // ====================================================================
    // Trit: Single balanced ternary digit (-1, 0, 1, ERR=-128)
    // Nit: Single balanced nonary digit (-4 to +4, ERR=-128)
    
    // trit_add(trit, trit) -> trit
    if (callee_ident->name == "trit_add") {
        if (expr->arguments.size() != 2) {
            throw std::runtime_error("trit_add() requires two arguments");
        }
        llvm::Value* a = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* b = codegenExpressionNode(expr->arguments[1].get(), this);
        
        llvm::Function* func = module->getFunction("trit_add");
        if (!func) {
            std::vector<llvm::Type*> paramTypes = {builder.getInt8Ty(), builder.getInt8Ty()};
            llvm::FunctionType* funcType = llvm::FunctionType::get(builder.getInt8Ty(), paramTypes, false);
            func = llvm::Function::Create(funcType, llvm::Function::ExternalLinkage, "trit_add", module);
        }
        return builder.CreateCall(func, {a, b}, "trit_add_result");
    }
    
    // trit_mul(trit, trit) -> trit
    if (callee_ident->name == "trit_mul") {
        if (expr->arguments.size() != 2) {
            throw std::runtime_error("trit_mul() requires two arguments");
        }
        llvm::Value* a = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* b = codegenExpressionNode(expr->arguments[1].get(), this);
        
        llvm::Function* func = module->getFunction("trit_mul");
        if (!func) {
            std::vector<llvm::Type*> paramTypes = {builder.getInt8Ty(), builder.getInt8Ty()};
            llvm::FunctionType* funcType = llvm::FunctionType::get(builder.getInt8Ty(), paramTypes, false);
            func = llvm::Function::Create(funcType, llvm::Function::ExternalLinkage, "trit_mul", module);
        }
        return builder.CreateCall(func, {a, b}, "trit_mul_result");
    }
    
    // trit_neg(trit) -> trit
    if (callee_ident->name == "trit_neg") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("trit_neg() requires one argument");
        }
        llvm::Value* a = codegenExpressionNode(expr->arguments[0].get(), this);
        
        llvm::Function* func = module->getFunction("trit_neg");
        if (!func) {
            llvm::FunctionType* funcType = llvm::FunctionType::get(builder.getInt8Ty(), {builder.getInt8Ty()}, false);
            func = llvm::Function::Create(funcType, llvm::Function::ExternalLinkage, "trit_neg", module);
        }
        return builder.CreateCall(func, {a}, "trit_neg_result");
    }
    
    // REMOVED: Old trit_and - now handled in TERNARY LOGIC BUILTINS section below
    
    // REMOVED: Old trit_or - now handled in TERNARY LOGIC BUILTINS section below
    
    // REMOVED: Old trit_not - now handled in TERNARY LOGIC BUILTINS section below
    
    // trit_is_err(trit) -> bool
    if (callee_ident->name == "trit_is_err") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("trit_is_err() requires one argument");
        }
        llvm::Value* a = codegenExpressionNode(expr->arguments[0].get(), this);
        
        llvm::Function* func = module->getFunction("trit_is_err");
        if (!func) {
            llvm::FunctionType* funcType = llvm::FunctionType::get(builder.getInt1Ty(), {builder.getInt8Ty()}, false);
            func = llvm::Function::Create(funcType, llvm::Function::ExternalLinkage, "trit_is_err", module);
        }
        return builder.CreateCall(func, {a}, "trit_is_err_result");
    }
    
    // REMOVED: Old nit_and/nit_or - now handled in TERNARY LOGIC BUILTINS section
    
    
    // nit_is_err(nit) -> bool
    if (callee_ident->name == "nit_is_err") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("nit_is_err() requires one argument");
        }
        llvm::Value* a = codegenExpressionNode(expr->arguments[0].get(), this);
        
        // Call aria_nit_is_err (returns i8 0/1), truncate to i1 for bool
        llvm::Function* func = module->getFunction("aria_nit_is_err");
        if (!func) {
            llvm::FunctionType* funcType = llvm::FunctionType::get(builder.getInt8Ty(), {builder.getInt8Ty()}, false);
            func = llvm::Function::Create(funcType, llvm::Function::ExternalLinkage, "aria_nit_is_err", module);
        }
        llvm::Value* result_i8 = builder.CreateCall(func, {a}, "nit_is_err_i8");
        return builder.CreateTrunc(result_i8, builder.getInt1Ty(), "nit_is_err_result");
    }
    
    // ====================================================================
    // MATRIX AND TENSOR OPERATIONS (Exotic Types)
    // ====================================================================
    
    // tmatrix_identity(rows, cols) -> tmatrix
    if (callee_ident->name == "tmatrix_identity") {
        if (expr->arguments.size() != 2) {
            throw std::runtime_error("tmatrix_identity() requires two arguments (rows, cols)");
        }
        llvm::Value* rows = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* cols = codegenExpressionNode(expr->arguments[1].get(), this);
        
        // Create external function declaration for runtime implementation
        llvm::Function* func = module->getFunction("tmatrix_identity");
        if (!func) {
            // tmatrix is represented as an opaque pointer for now
            llvm::Type* tmatrixType = builder.getPtrTy();
            std::vector<llvm::Type*> paramTypes = {builder.getInt64Ty(), builder.getInt64Ty()};
            llvm::FunctionType* funcType = llvm::FunctionType::get(tmatrixType, paramTypes, false);
            func = llvm::Function::Create(funcType, llvm::Function::ExternalLinkage, 
                                         "tmatrix_identity", module);
        }
        return builder.CreateCall(func, {rows, cols}, "tmatrix");
    }
    
    // ttensor_zeros(shape) -> ttensor
    if (callee_ident->name == "ttensor_zeros") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("ttensor_zeros() requires one argument (9D shape)");
        }
        llvm::Value* shape = codegenExpressionNode(expr->arguments[0].get(), this);
        
        // Create external function declaration for runtime implementation
        llvm::Function* func = module->getFunction("ttensor_zeros");
        if (!func) {
            // ttensor is represented as an opaque pointer for now
            llvm::Type* ttensorType = builder.getPtrTy();
            // Shape is a vec9 (9 x int64 array)
            llvm::Type* shapeType = shape->getType();
            std::vector<llvm::Type*> paramTypes = {shapeType};
            llvm::FunctionType* funcType = llvm::FunctionType::get(ttensorType, paramTypes, false);
            func = llvm::Function::Create(funcType, llvm::Function::ExternalLinkage, 
                                         "ttensor_zeros", module);
        }
        return builder.CreateCall(func, {shape}, "ttensor");
    }
    
    // ====================================================================
    // MAP/DICTIONARY RUNTIME FUNCTIONS
    // ====================================================================
    
    // map_new(key_size, value_size) -> wild void->
    if (callee_ident->name == "map_new") {
        if (expr->arguments.size() != 2) {
            throw std::runtime_error("map_new() requires 2 arguments (key_size, value_size)");
        }
        llvm::Value* key_size = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* value_size = codegenExpressionNode(expr->arguments[1].get(), this);
        
        llvm::Function* func = module->getFunction("map_new");
        if (!func) {
            llvm::Type* ptrType = builder.getPtrTy();  // Returns opaque pointer
            std::vector<llvm::Type*> paramTypes = {builder.getInt64Ty(), builder.getInt64Ty()};
            llvm::FunctionType* funcType = llvm::FunctionType::get(ptrType, paramTypes, false);
            func = llvm::Function::Create(funcType, llvm::Function::ExternalLinkage, 
                                         "map_new", module);
        }
        return builder.CreateCall(func, {key_size, value_size}, "map");
    }
    
    // map_insert(map, key_ptr, value_ptr) -> void
    if (callee_ident->name == "map_insert") {
        if (expr->arguments.size() != 3) {
            throw std::runtime_error("map_insert() requires 3 arguments (map, key_ptr, value_ptr)");
        }
        llvm::Value* map = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* key_ptr = codegenExpressionNode(expr->arguments[1].get(), this);
        llvm::Value* value_ptr = codegenExpressionNode(expr->arguments[2].get(), this);
        
        llvm::Function* func = module->getFunction("map_insert");
        if (!func) {
            std::vector<llvm::Type*> paramTypes = {
                builder.getPtrTy(), 
                builder.getPtrTy(), 
                builder.getPtrTy()
            };
            llvm::FunctionType* funcType = llvm::FunctionType::get(builder.getVoidTy(), paramTypes, false);
            func = llvm::Function::Create(funcType, llvm::Function::ExternalLinkage, 
                                         "map_insert", module);
        }
        return builder.CreateCall(func, {map, key_ptr, value_ptr});
    }
    
    // map_get(map, key_ptr) -> wild void->
    if (callee_ident->name == "map_get") {
        if (expr->arguments.size() != 2) {
            throw std::runtime_error("map_get() requires 2 arguments (map, key_ptr)");
        }
        llvm::Value* map = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* key_ptr = codegenExpressionNode(expr->arguments[1].get(), this);
        
        llvm::Function* func = module->getFunction("map_get");
        if (!func) {
            llvm::Type* ptrType = builder.getPtrTy();
            std::vector<llvm::Type*> paramTypes = {builder.getPtrTy(), builder.getPtrTy()};
            llvm::FunctionType* funcType = llvm::FunctionType::get(ptrType, paramTypes, false);
            func = llvm::Function::Create(funcType, llvm::Function::ExternalLinkage, 
                                         "map_get", module);
        }
        return builder.CreateCall(func, {map, key_ptr}, "value_ptr");
    }
    
    // map_has(map, key_ptr) -> bool
    if (callee_ident->name == "map_has") {
        if (expr->arguments.size() != 2) {
            throw std::runtime_error("map_has() requires 2 arguments (map, key_ptr)");
        }
        llvm::Value* map = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* key_ptr = codegenExpressionNode(expr->arguments[1].get(), this);
        
        llvm::Function* func = module->getFunction("map_has");
        if (!func) {
            std::vector<llvm::Type*> paramTypes = {builder.getPtrTy(), builder.getPtrTy()};
            llvm::FunctionType* funcType = llvm::FunctionType::get(builder.getInt1Ty(), paramTypes, false);
            func = llvm::Function::Create(funcType, llvm::Function::ExternalLinkage, 
                                         "map_has", module);
        }
        return builder.CreateCall(func, {map, key_ptr}, "has_key");
    }
    
    // map_remove(map, key_ptr) -> void
    if (callee_ident->name == "map_remove") {
        if (expr->arguments.size() != 2) {
            throw std::runtime_error("map_remove() requires 2 arguments (map, key_ptr)");
        }
        llvm::Value* map = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* key_ptr = codegenExpressionNode(expr->arguments[1].get(), this);
        
        llvm::Function* func = module->getFunction("map_remove");
        if (!func) {
            std::vector<llvm::Type*> paramTypes = {builder.getPtrTy(), builder.getPtrTy()};
            llvm::FunctionType* funcType = llvm::FunctionType::get(builder.getVoidTy(), paramTypes, false);
            func = llvm::Function::Create(funcType, llvm::Function::ExternalLinkage, 
                                         "map_remove", module);
        }
        return builder.CreateCall(func, {map, key_ptr});
    }
    
    // map_length(map) -> int64
    if (callee_ident->name == "map_length") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("map_length() requires 1 argument (map)");
        }
        llvm::Value* map = codegenExpressionNode(expr->arguments[0].get(), this);
        
        llvm::Function* func = module->getFunction("map_length");
        if (!func) {
            std::vector<llvm::Type*> paramTypes = {builder.getPtrTy()};
            llvm::FunctionType* funcType = llvm::FunctionType::get(builder.getInt64Ty(), paramTypes, false);
            func = llvm::Function::Create(funcType, llvm::Function::ExternalLinkage, 
                                         "map_length", module);
        }
        return builder.CreateCall(func, {map}, "length");
    }
    
    // ====================================================================
    // OPTIONAL/RESULT HELPER FUNCTIONS
    // ====================================================================
    
    // get_optional_value(condition) -> int64?
    if (callee_ident->name == "get_optional_value") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("get_optional_value() requires 1 argument (condition)");
        }
        llvm::Value* condition = codegenExpressionNode(expr->arguments[0].get(), this);
        
        // Convert condition to i1 if needed
        if (condition->getType() != builder.getInt1Ty()) {
            if (condition->getType()->isIntegerTy()) {
                condition = builder.CreateICmpNE(condition, 
                    llvm::ConstantInt::get(condition->getType(), 0), "tobool");
            }
        }
        
        // Optional<int64> is { i1 hasValue, i64 value }
        llvm::StructType* optionalType = llvm::StructType::get(context, 
            {builder.getInt1Ty(), builder.getInt64Ty()}, false);
        
        llvm::Value* result = llvm::UndefValue::get(optionalType);
        
        // Set hasValue based on condition
        result = builder.CreateInsertValue(result, condition, 0, "optional.hasValue");
        
        // Set value to 123 if condition is true, otherwise 0
        llvm::Value* valueToStore = builder.CreateSelect(condition,
            llvm::ConstantInt::get(builder.getInt64Ty(), 123),
            llvm::ConstantInt::get(builder.getInt64Ty(), 0));
        result = builder.CreateInsertValue(result, valueToStore, 1, "optional.value");
        
        return result;
    }
    
    // ====================================================================
    // INTEGER CONVERSION BUILTINS
    // ====================================================================
    
    // int8_from_int64(int64) -> int8
    if (callee_ident->name == "int8_from_int64") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("int8_from_int64() requires one argument");
        }
        llvm::Value* val = codegenExpressionNode(expr->arguments[0].get(), this);
        return builder.CreateTrunc(val, builder.getInt8Ty());
    }
    
    // int16_from_int64(int64) -> int16
    if (callee_ident->name == "int16_from_int64") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("int16_from_int64() requires one argument");
        }
        llvm::Value* val = codegenExpressionNode(expr->arguments[0].get(), this);
        return builder.CreateTrunc(val, builder.getInt16Ty());
    }
    
    // int32_from_int64(int64) -> int32
    if (callee_ident->name == "int32_from_int64") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("int32_from_int64() requires one argument");
        }
        llvm::Value* val = codegenExpressionNode(expr->arguments[0].get(), this);
        return builder.CreateTrunc(val, builder.getInt32Ty());
    }
    
    // int64_from_int32(int32) -> int64
    if (callee_ident->name == "int64_from_int32") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("int64_from_int32() requires one argument");
        }
        llvm::Value* val = codegenExpressionNode(expr->arguments[0].get(), this);
        return builder.CreateSExt(val, builder.getInt64Ty());
    }
    
    // int64_from_int8(int8) -> int64
    if (callee_ident->name == "int64_from_int8") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("int64_from_int8() requires one argument");
        }
        llvm::Value* val = codegenExpressionNode(expr->arguments[0].get(), this);
        return builder.CreateSExt(val, builder.getInt64Ty());
    }
    
    // ====================================================================
    // MEMORY MANAGEMENT BUILTINS
    // ====================================================================
    
    // ====================================================================
    // EXPLICIT SAFETY BYPASS
    // ====================================================================

    // raw(Result<T>) -> T — Bypass "no checky no val", extract .value directly.
    // Type checker has already confirmed the argument is Result<T>.
    // We extract field 0 of the {T, ptr, i8} Result struct without any guard.
    if (callee_ident->name == "raw") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("raw() requires exactly one argument");
        }
        llvm::Value* arg = codegenExpressionNode(expr->arguments[0].get(), this);
        if (!arg) {
            throw std::runtime_error("Failed to generate code for raw() argument");
        }
        // arg is a Result<T> struct: { T value, ptr error, i8 is_error }
        // Extract field 0 (value) unconditionally.
        if (arg->getType()->isStructTy()) {
            llvm::StructType* sty = llvm::cast<llvm::StructType>(arg->getType());
            if (sty->getNumElements() == 3 &&
                sty->getElementType(1)->isPointerTy() &&
                sty->getElementType(2)->isIntegerTy(8)) {
                return builder.CreateExtractValue(arg, 0, "raw.value");
            }
        }
        // If for some reason it arrived already unwrapped (e.g. constant folding),
        // pass it straight through.
        return arg;
    }

    // drop(T) -> void - Evaluate expression and discard result
    if (callee_ident->name == "drop") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("drop() requires one argument");
        }
        
        // Just evaluate the expression (for side effects) and discard the result
        // Don't free anything - this is for discarding result<void> and similar
        codegenExpressionNode(expr->arguments[0].get(), this);
        
        // drop() returns void - LLVM represents this as no value
        return nullptr;
    }

    // ====================================================================
    // COLLECTION BUILTINS (Phase 6.2)
    // ====================================================================

    // array_new(element_size: int64) -> ptr
    if (callee_ident->name == "array_new") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("array_new() requires one argument (element_size)");
        }

        // Get or declare aria_array_new_simple
        llvm::Function* func = module->getFunction("aria_array_new_simple");
        if (!func) {
            // AriaArray* aria_array_new_simple(size_t element_size, int type_id)
            std::vector<llvm::Type*> params = {
                builder.getInt64Ty(),  // element_size (size_t)
                builder.getInt32Ty()   // type_id
            };
            llvm::FunctionType* func_type = llvm::FunctionType::get(
                builder.getPtrTy(), params, false);
            func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                "aria_array_new_simple", module);
        }

        llvm::Value* element_size = codegenExpressionNode(expr->arguments[0].get(), this);
        // Ensure it's i64
        if (element_size->getType()->getIntegerBitWidth() != 64) {
            element_size = builder.CreateZExtOrTrunc(element_size, builder.getInt64Ty());
        }
        llvm::Value* type_id = llvm::ConstantInt::get(builder.getInt32Ty(), 0); // Generic type

        return builder.CreateCall(func, {element_size, type_id}, "new_array");
    }

    // array_length(array: ptr) -> int64
    if (callee_ident->name == "array_length") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("array_length() requires one argument (array)");
        }

        // Get or declare aria_array_length
        llvm::Function* func = module->getFunction("aria_array_length");
        if (!func) {
            // size_t aria_array_length(const AriaArray* array)
            std::vector<llvm::Type*> params = {builder.getPtrTy()};
            llvm::FunctionType* func_type = llvm::FunctionType::get(
                builder.getInt64Ty(), params, false);
            func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                "aria_array_length", module);
        }

        llvm::Value* array_ptr = codegenExpressionNode(expr->arguments[0].get(), this);
        return builder.CreateCall(func, {array_ptr}, "array_len");
    }

    // array_push(array: ptr, value: ptr) -> void
    if (callee_ident->name == "array_push") {
        if (expr->arguments.size() != 2) {
            throw std::runtime_error("array_push() requires two arguments (array, value)");
        }

        // Get or declare aria_array_push_simple
        llvm::Function* func = module->getFunction("aria_array_push_simple");
        if (!func) {
            // void aria_array_push_simple(AriaArray* array, const void* value)
            std::vector<llvm::Type*> params = {builder.getPtrTy(), builder.getPtrTy()};
            llvm::FunctionType* func_type = llvm::FunctionType::get(
                builder.getVoidTy(), params, false);
            func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                "aria_array_push_simple", module);
        }

        llvm::Value* array_ptr = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* value_ptr = codegenExpressionNode(expr->arguments[1].get(), this);

        // If value is not already a pointer, we need to allocate and store it
        if (!value_ptr->getType()->isPointerTy()) {
            llvm::Value* alloca = builder.CreateAlloca(value_ptr->getType(), nullptr, "push_temp");
            builder.CreateStore(value_ptr, alloca);
            value_ptr = alloca;
        }

        builder.CreateCall(func, {array_ptr, value_ptr});
        return llvm::ConstantInt::get(builder.getInt32Ty(), 0);  // Return 0 on success
    }

    // array_get(array: ptr, index: int64) -> ptr
    if (callee_ident->name == "array_get") {
        if (expr->arguments.size() != 2) {
            throw std::runtime_error("array_get() requires two arguments (array, index)");
        }

        // Get or declare aria_array_get_simple
        llvm::Function* func = module->getFunction("aria_array_get_simple");
        if (!func) {
            // void* aria_array_get_simple(AriaArray* array, size_t index)
            std::vector<llvm::Type*> params = {builder.getPtrTy(), builder.getInt64Ty()};
            llvm::FunctionType* func_type = llvm::FunctionType::get(
                builder.getPtrTy(), params, false);
            func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                "aria_array_get_simple", module);
        }

        llvm::Value* array_ptr = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* index = codegenExpressionNode(expr->arguments[1].get(), this);
        // Ensure index is i64
        if (index->getType()->getIntegerBitWidth() != 64) {
            index = builder.CreateZExtOrTrunc(index, builder.getInt64Ty());
        }

        return builder.CreateCall(func, {array_ptr, index}, "array_elem");
    }

    // array_set(array: ptr, index: int64, value: ptr) -> void
    if (callee_ident->name == "array_set") {
        if (expr->arguments.size() != 3) {
            throw std::runtime_error("array_set() requires three arguments (array, index, value)");
        }

        // Get or declare aria_array_set_simple
        llvm::Function* func = module->getFunction("aria_array_set_simple");
        if (!func) {
            // void aria_array_set_simple(AriaArray* array, size_t index, const void* value)
            std::vector<llvm::Type*> params = {builder.getPtrTy(), builder.getInt64Ty(), builder.getPtrTy()};
            llvm::FunctionType* func_type = llvm::FunctionType::get(
                builder.getVoidTy(), params, false);
            func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                "aria_array_set_simple", module);
        }

        llvm::Value* array_ptr = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* index = codegenExpressionNode(expr->arguments[1].get(), this);
        llvm::Value* value_ptr = codegenExpressionNode(expr->arguments[2].get(), this);

        // Ensure index is i64
        if (index->getType()->getIntegerBitWidth() != 64) {
            index = builder.CreateZExtOrTrunc(index, builder.getInt64Ty());
        }

        // If value is not already a pointer, we need to allocate and store it
        if (!value_ptr->getType()->isPointerTy()) {
            llvm::Value* alloca = builder.CreateAlloca(value_ptr->getType(), nullptr, "set_temp");
            builder.CreateStore(value_ptr, alloca);
            value_ptr = alloca;
        }

        builder.CreateCall(func, {array_ptr, index, value_ptr});
        return llvm::ConstantInt::get(builder.getInt32Ty(), 0);  // Return 0 on success
    }

    // array_pop(array: ptr) -> ptr
    if (callee_ident->name == "array_pop") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("array_pop() requires one argument (array)");
        }

        // Get or declare aria_array_pop_simple
        llvm::Function* func = module->getFunction("aria_array_pop_simple");
        if (!func) {
            // void aria_array_pop_simple(AriaArray* array, void* out_value)
            std::vector<llvm::Type*> params = {builder.getPtrTy(), builder.getPtrTy()};
            llvm::FunctionType* func_type = llvm::FunctionType::get(
                builder.getVoidTy(), params, false);
            func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                "aria_array_pop_simple", module);
        }

        llvm::Value* array_ptr = codegenExpressionNode(expr->arguments[0].get(), this);

        // Allocate space for the popped value (we'll use an i64-sized buffer)
        llvm::Value* out_alloca = builder.CreateAlloca(builder.getInt64Ty(), nullptr, "pop_out");

        builder.CreateCall(func, {array_ptr, out_alloca});

        // Return the pointer to the buffer (caller should cast/load as appropriate)
        return out_alloca;
    }

    // ════════════════════════════════════════════════════════════════════
    // Map Functions
    // ════════════════════════════════════════════════════════════════════

    // map_new(key_size: int64, value_size: int64) -> ptr
    if (callee_ident->name == "map_new") {
        if (expr->arguments.size() != 2) {
            throw std::runtime_error("map_new() requires two arguments (key_size, value_size)");
        }

        // Get or declare aria_map_new_simple
        llvm::Function* func = module->getFunction("aria_map_new_simple");
        if (!func) {
            // AriaMap* aria_map_new_simple(size_t key_size, size_t value_size)
            std::vector<llvm::Type*> params = {builder.getInt64Ty(), builder.getInt64Ty()};
            llvm::FunctionType* func_type = llvm::FunctionType::get(
                builder.getPtrTy(), params, false);
            func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                "aria_map_new_simple", module);
        }

        llvm::Value* key_size = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* value_size = codegenExpressionNode(expr->arguments[1].get(), this);
        
        // Ensure they're i64
        if (key_size->getType()->getIntegerBitWidth() != 64) {
            key_size = builder.CreateZExtOrTrunc(key_size, builder.getInt64Ty());
        }
        if (value_size->getType()->getIntegerBitWidth() != 64) {
            value_size = builder.CreateZExtOrTrunc(value_size, builder.getInt64Ty());
        }

        return builder.CreateCall(func, {key_size, value_size}, "new_map");
    }

    // map_length(map: ptr) -> int64
    if (callee_ident->name == "map_length") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("map_length() requires one argument (map)");
        }

        // Get or declare aria_map_length
        llvm::Function* func = module->getFunction("aria_map_length");
        if (!func) {
            // size_t aria_map_length(const AriaMap* map)
            std::vector<llvm::Type*> params = {builder.getPtrTy()};
            llvm::FunctionType* func_type = llvm::FunctionType::get(
                builder.getInt64Ty(), params, false);
            func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                "aria_map_length", module);
        }

        llvm::Value* map_ptr = codegenExpressionNode(expr->arguments[0].get(), this);
        return builder.CreateCall(func, {map_ptr}, "map_len");
    }

    // map_insert(map: ptr, key: ptr, value: ptr) -> void
    if (callee_ident->name == "map_insert") {
        if (expr->arguments.size() != 3) {
            throw std::runtime_error("map_insert() requires three arguments (map, key, value)");
        }

        // Get or declare aria_map_insert_simple
        llvm::Function* func = module->getFunction("aria_map_insert_simple");
        if (!func) {
            // void aria_map_insert_simple(AriaMap* map, const void* key, const void* value)
            std::vector<llvm::Type*> params = {builder.getPtrTy(), builder.getPtrTy(), builder.getPtrTy()};
            llvm::FunctionType* func_type = llvm::FunctionType::get(
                builder.getVoidTy(), params, false);
            func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                "aria_map_insert_simple", module);
        }

        llvm::Value* map_ptr = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* key_ptr = codegenExpressionNode(expr->arguments[1].get(), this);
        llvm::Value* value_ptr = codegenExpressionNode(expr->arguments[2].get(), this);

        // If key is not already a pointer, allocate and store it
        if (!key_ptr->getType()->isPointerTy()) {
            llvm::Value* alloca = builder.CreateAlloca(key_ptr->getType(), nullptr, "key_temp");
            builder.CreateStore(key_ptr, alloca);
            key_ptr = alloca;
        }

        // If value is not already a pointer, allocate and store it
        if (!value_ptr->getType()->isPointerTy()) {
            llvm::Value* alloca = builder.CreateAlloca(value_ptr->getType(), nullptr, "value_temp");
            builder.CreateStore(value_ptr, alloca);
            value_ptr = alloca;
        }

        builder.CreateCall(func, {map_ptr, key_ptr, value_ptr});
        return llvm::ConstantInt::get(builder.getInt32Ty(), 0);  // Return 0 on success
    }

    // map_get(map: ptr, key: ptr) -> ptr
    if (callee_ident->name == "map_get") {
        if (expr->arguments.size() != 2) {
            throw std::runtime_error("map_get() requires two arguments (map, key)");
        }

        // Get or declare aria_map_get_simple
        llvm::Function* func = module->getFunction("aria_map_get_simple");
        if (!func) {
            // void* aria_map_get_simple(AriaMap* map, const void* key)
            std::vector<llvm::Type*> params = {builder.getPtrTy(), builder.getPtrTy()};
            llvm::FunctionType* func_type = llvm::FunctionType::get(
                builder.getPtrTy(), params, false);
            func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                "aria_map_get_simple", module);
        }

        llvm::Value* map_ptr = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* key_ptr = codegenExpressionNode(expr->arguments[1].get(), this);

        // If key is not already a pointer, allocate and store it
        if (!key_ptr->getType()->isPointerTy()) {
            llvm::Value* alloca = builder.CreateAlloca(key_ptr->getType(), nullptr, "key_temp");
            builder.CreateStore(key_ptr, alloca);
            key_ptr = alloca;
        }

        return builder.CreateCall(func, {map_ptr, key_ptr}, "map_value");
    }

    // map_has(map: ptr, key: ptr) -> bool
    if (callee_ident->name == "map_has") {
        if (expr->arguments.size() != 2) {
            throw std::runtime_error("map_has() requires two arguments (map, key)");
        }

        // Get or declare aria_map_has
        llvm::Function* func = module->getFunction("aria_map_has");
        if (!func) {
            // bool aria_map_has(const AriaMap* map, const void* key)
            std::vector<llvm::Type*> params = {builder.getPtrTy(), builder.getPtrTy()};
            llvm::FunctionType* func_type = llvm::FunctionType::get(
                builder.getInt1Ty(), params, false);
            func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                "aria_map_has", module);
        }

        llvm::Value* map_ptr = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* key_ptr = codegenExpressionNode(expr->arguments[1].get(), this);

        // If key is not already a pointer, allocate and store it
        if (!key_ptr->getType()->isPointerTy()) {
            llvm::Value* alloca = builder.CreateAlloca(key_ptr->getType(), nullptr, "key_temp");
            builder.CreateStore(key_ptr, alloca);
            key_ptr = alloca;
        }

        return builder.CreateCall(func, {map_ptr, key_ptr}, "map_has_key");
    }

    // map_remove(map: ptr, key: ptr) -> void
    if (callee_ident->name == "map_remove") {
        if (expr->arguments.size() != 2) {
            throw std::runtime_error("map_remove() requires two arguments (map, key)");
        }

        // Get or declare aria_map_remove (we'll use the full Result version since simple version doesn't exist)
        llvm::Function* func = module->getFunction("aria_map_remove");
        if (!func) {
            // AriaResultVoid aria_map_remove(AriaMap* map, const void* key)
            // For now, we'll create a void version wrapper
            std::vector<llvm::Type*> params = {builder.getPtrTy(), builder.getPtrTy()};
            llvm::FunctionType* func_type = llvm::FunctionType::get(
                builder.getVoidTy(), params, false);
            func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                "aria_map_remove", module);
        }

        llvm::Value* map_ptr = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* key_ptr = codegenExpressionNode(expr->arguments[1].get(), this);

        // If key is not already a pointer, allocate and store it
        if (!key_ptr->getType()->isPointerTy()) {
            llvm::Value* alloca = builder.CreateAlloca(key_ptr->getType(), nullptr, "key_temp");
            builder.CreateStore(key_ptr, alloca);
            key_ptr = alloca;
        }

        builder.CreateCall(func, {map_ptr, key_ptr});
        return llvm::ConstantInt::get(builder.getInt32Ty(), 0);  // Return 0 on success
    }

    // ====================================================================
    // TERNARY LOGIC BUILTINS
    // ====================================================================
    
    // trit_and(a: trit, b: trit) -> trit
    if (callee_ident->name == "trit_and") {
        if (expr->arguments.size() != 2) {
            throw std::runtime_error("trit_and() requires two arguments");
        }
        
        llvm::Function* func = module->getFunction("aria_trit_and");
        if (!func) {
            // int8_t aria_trit_and(int8_t a, int8_t b)
            std::vector<llvm::Type*> params = {builder.getInt8Ty(), builder.getInt8Ty()};
            llvm::FunctionType* func_type = llvm::FunctionType::get(
                builder.getInt8Ty(), params, false);
            func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                "aria_trit_and", module);
        }
        
        llvm::Value* a = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* b = codegenExpressionNode(expr->arguments[1].get(), this);
        
        // No type conversion needed - trit is now i8, matching runtime expectation
        llvm::Value* result = builder.CreateCall(func, {a, b}, "trit_and_call");
        return result;
    }
    
    // trit_or(a: trit, b: trit) -> trit
    if (callee_ident->name == "trit_or") {
        if (expr->arguments.size() != 2) {
            throw std::runtime_error("trit_or() requires two arguments");
        }
        
        llvm::Function* func = module->getFunction("aria_trit_or");
        if (!func) {
            std::vector<llvm::Type*> params = {builder.getInt8Ty(), builder.getInt8Ty()};
            llvm::FunctionType* func_type = llvm::FunctionType::get(
                builder.getInt8Ty(), params, false);
            func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                "aria_trit_or", module);
        }
        
        llvm::Value* a = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* b = codegenExpressionNode(expr->arguments[1].get(), this);
        
        // No type conversion needed - trit is now i8, matching runtime expectation
        llvm::Value* result = builder.CreateCall(func, {a, b}, "trit_or_call");
        return result;
    }
    
    // nit_and(a: nit, b: nit) -> nit
    if (callee_ident->name == "nit_and") {
        if (expr->arguments.size() != 2) {
            throw std::runtime_error("nit_and() requires two arguments");
        }
        
        llvm::Function* func = module->getFunction("aria_nit_and");
        if (!func) {
            std::vector<llvm::Type*> params = {builder.getInt8Ty(), builder.getInt8Ty()};
            llvm::FunctionType* func_type = llvm::FunctionType::get(
                builder.getInt8Ty(), params, false);
            func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                "aria_nit_and", module);
        }
        
        llvm::Value* a = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* b = codegenExpressionNode(expr->arguments[1].get(), this);
        
        // No type conversion needed - nit is now i8, matching runtime expectation
        llvm::Value* result = builder.CreateCall(func, {a, b}, "nit_and_call");
        return result;
    }
    
    // nit_or(a: nit, b: nit) -> nit
    if (callee_ident->name == "nit_or") {
        if (expr->arguments.size() != 2) {
            throw std::runtime_error("nit_or() requires two arguments");
        }
        
        llvm::Function* func = module->getFunction("aria_nit_or");
        if (!func) {
            std::vector<llvm::Type*> params = {builder.getInt8Ty(), builder.getInt8Ty()};
            llvm::FunctionType* func_type = llvm::FunctionType::get(
                builder.getInt8Ty(), params, false);
            func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                "aria_nit_or", module);
        }
        
        llvm::Value* a = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* b = codegenExpressionNode(expr->arguments[1].get(), this);
        
        // No type conversion needed - nit is now i8, matching runtime expectation
        llvm::Value* result = builder.CreateCall(func, {a, b}, "nit_or_call");
        return result;
    }
    
    // tbb8_from_int(value: int32) -> tbb8
    if (callee_ident->name == "tbb8_from_int") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("tbb8_from_int() requires one argument");
        }
        
        llvm::Function* func = module->getFunction("aria_tbb8_from_int");
        if (!func) {
            // int8_t aria_tbb8_from_int(int32_t value)
            std::vector<llvm::Type*> params = {builder.getInt32Ty()};
            llvm::FunctionType* func_type = llvm::FunctionType::get(
                builder.getInt8Ty(), params, false);
            func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                "aria_tbb8_from_int", module);
        }
        
        llvm::Value* value = codegenExpressionNode(expr->arguments[0].get(), this);
        // Ensure it's i32
        if (value->getType()->getIntegerBitWidth() != 32) {
            value = builder.CreateSExtOrTrunc(value, builder.getInt32Ty());
        }
        return builder.CreateCall(func, {value}, "tbb8_from_int");
    }
    
    // tbb8_to_int(value: tbb8) -> int32
    if (callee_ident->name == "tbb8_to_int") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("tbb8_to_int() requires one argument");
        }
        
        llvm::Function* func = module->getFunction("aria_tbb8_to_int");
        if (!func) {
            // int32_t aria_tbb8_to_int(int8_t value)
            std::vector<llvm::Type*> params = {builder.getInt8Ty()};
            llvm::FunctionType* func_type = llvm::FunctionType::get(
                builder.getInt32Ty(), params, false);
            func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                "aria_tbb8_to_int", module);
        }
        
        llvm::Value* value = codegenExpressionNode(expr->arguments[0].get(), this);
        return builder.CreateCall(func, {value}, "tbb8_to_int");
    }
    
    // tbb8_is_err(value: tbb8) -> bool
    if (callee_ident->name == "tbb8_is_err") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("tbb8_is_err() requires one argument");
        }
        
        llvm::Value* value = codegenExpressionNode(expr->arguments[0].get(), this);
        // Check if value == -128 (ERR sentinel)
        llvm::Value* err_sentinel = llvm::ConstantInt::get(builder.getInt8Ty(), -128);
        return builder.CreateICmpEQ(value, err_sentinel, "is_err");
    }
    
    // tbb16_from_int(value: int32) -> tbb16
    if (callee_ident->name == "tbb16_from_int") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("tbb16_from_int() requires one argument");
        }
        
        llvm::Value* value = codegenExpressionNode(expr->arguments[0].get(), this);
        // Check range [-32767, 32767], return -32768 (ERR) if out of range
        llvm::Value* min_val = llvm::ConstantInt::get(builder.getInt32Ty(), -32767);
        llvm::Value* max_val = llvm::ConstantInt::get(builder.getInt32Ty(), 32767);
        llvm::Value* err_val = llvm::ConstantInt::get(builder.getInt16Ty(), -32768);
        
        llvm::Value* is_too_low = builder.CreateICmpSLT(value, min_val);
        llvm::Value* is_too_high = builder.CreateICmpSGT(value, max_val);
        llvm::Value* is_out_of_range = builder.CreateOr(is_too_low, is_too_high);
        
        llvm::Value* truncated = builder.CreateTrunc(value, builder.getInt16Ty());
        return builder.CreateSelect(is_out_of_range, err_val, truncated, "tbb16_from_int");
    }
    
    // tbb16_is_err(value: tbb16) -> bool
    if (callee_ident->name == "tbb16_is_err") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("tbb16_is_err() requires one argument");
        }
        
        llvm::Value* value = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* err_sentinel = llvm::ConstantInt::get(builder.getInt16Ty(), -32768);
        return builder.CreateICmpEQ(value, err_sentinel, "is_err");
    }
    
    // tbb32_from_int(value: int32) -> tbb32
    if (callee_ident->name == "tbb32_from_int") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("tbb32_from_int() requires one argument");
        }
        
        // For tbb32, we can't overflow from int32 input
        // tbb32 range is [-2147483647, 2147483647], sentinel is -2147483648 (INT32_MIN)
        // Since input is int32, only INT32_MIN is the error case
        llvm::Value* value = codegenExpressionNode(expr->arguments[0].get(), this);
        return value;  // Direct passthrough for int32 -> tbb32
    }
    
    // tbb32_to_int(value: tbb32) -> int32
    if (callee_ident->name == "tbb32_to_int") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("tbb32_to_int() requires one argument");
        }
        
        llvm::Value* value = codegenExpressionNode(expr->arguments[0].get(), this);
        return value;  // Direct passthrough for tbb32 -> int32
    }

    // ====================================================================
    // FILE I/O BUILTINS (Phase 4.2)
    // ====================================================================
    
    // readFile(path: string) -> string
    // Calls aria_read_file_simple which returns AriaString directly
    if (callee_ident->name == "readFile") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("readFile() requires one argument (path)");
        }
        
        // Get or declare aria_read_file_simple
        llvm::Function* func = module->getFunction("aria_read_file_simple");
        if (!func) {
            // AriaString aria_read_file_simple(const char* path)
            std::vector<llvm::Type*> params = {
                llvm::PointerType::get(builder.getInt8Ty(), 0)  // const char*
            };
            llvm::FunctionType* func_type = llvm::FunctionType::get(
                llvm::PointerType::get(getAriaStringType(), 0), params, false);
            func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                "aria_read_file_simple", module);
        }
        
        // Get path argument (should be AriaString)
        llvm::Value* path_ptr = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* path_str = builder.CreateLoad(getAriaStringType(), path_ptr, "path_str");
        
        // Extract char* from AriaString
        llvm::Value* path_data_ptr = builder.CreateStructGEP(getAriaStringType(), 
            path_ptr, 0, "path_data_ptr");
        llvm::Value* path_cstr = builder.CreateLoad(
            llvm::PointerType::get(builder.getInt8Ty(), 0), path_data_ptr, "path_cstr");
        
        return builder.CreateCall(func, {path_cstr}, "read_result");
    }
    
    // writeFile(path: string, content: string) -> int64
    // Returns 0 on success, -1 on error
    if (callee_ident->name == "writeFile") {
        if (expr->arguments.size() != 2) {
            throw std::runtime_error("writeFile() requires two arguments (path, content)");
        }
        
        // Get or declare aria_write_file_simple
        llvm::Function* func = module->getFunction("aria_write_file_simple");
        if (!func) {
            // int64_t aria_write_file_simple(const char* path, const char* content)
            std::vector<llvm::Type*> params = {
                llvm::PointerType::get(builder.getInt8Ty(), 0),  // const char* path
                llvm::PointerType::get(builder.getInt8Ty(), 0)   // const char* content
            };
            llvm::FunctionType* func_type = llvm::FunctionType::get(
                builder.getInt64Ty(), params, false);
            func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                "aria_write_file_simple", module);
        }
        
        // Get path argument
        llvm::Value* path_ptr = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* path_data_ptr = builder.CreateStructGEP(getAriaStringType(), 
            path_ptr, 0, "path_data_ptr");
        llvm::Value* path_cstr = builder.CreateLoad(
            llvm::PointerType::get(builder.getInt8Ty(), 0), path_data_ptr, "path_cstr");
        
        // Get content argument
        llvm::Value* content_ptr = codegenExpressionNode(expr->arguments[1].get(), this);
        llvm::Value* content_data_ptr = builder.CreateStructGEP(getAriaStringType(), 
            content_ptr, 0, "content_data_ptr");
        llvm::Value* content_cstr = builder.CreateLoad(
            llvm::PointerType::get(builder.getInt8Ty(), 0), content_data_ptr, "content_cstr");
        
        return builder.CreateCall(func, {path_cstr, content_cstr}, "write_result");
    }
    
    // allocate(size: int32) -> buffer@ (void*)
    // Allocates wild memory heap allocation
    if (callee_ident->name == "allocate") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("allocate() requires one argument (size)");
        }
        
        // Get or declare aria_alloc
        llvm::Function* func = module->getFunction("aria_alloc");
        if (!func) {
            // void* aria_alloc(size_t size)
            std::vector<llvm::Type*> params = {
                builder.getInt64Ty()  // size_t size (using i64 for size_t)
            };
            llvm::FunctionType* func_type = llvm::FunctionType::get(
                llvm::PointerType::get(builder.getInt8Ty(), 0),  // void* return
                params, false);
            func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                "aria_alloc", module);
        }
        
        // Codegen size argument (should be int32/int64)
        llvm::Value* size = codegenExpressionNode(expr->arguments[0].get(), this);
        
        // Extend to i64 if needed (from int32)
        if (size->getType()->isIntegerTy(32)) {
            size = builder.CreateSExt(size, builder.getInt64Ty(), "size_i64");
        }
        
        return builder.CreateCall(func, {size}, "alloc_ptr");
    }
    
    // fileExists(path: string) -> bool
    if (callee_ident->name == "fileExists") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("fileExists() requires one argument (path)");
        }
        
        // Get or declare aria_file_exists
        llvm::Function* func = module->getFunction("aria_file_exists");
        if (!func) {
            // bool aria_file_exists(const char* path)
            std::vector<llvm::Type*> params = {
                llvm::PointerType::get(builder.getInt8Ty(), 0)
            };
            llvm::FunctionType* func_type = llvm::FunctionType::get(
                builder.getInt1Ty(), params, false);
            func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                "aria_file_exists", module);
        }
        
        // Get path argument
        llvm::Value* path_ptr = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* path_data_ptr = builder.CreateStructGEP(getAriaStringType(), 
            path_ptr, 0, "path_data_ptr");
        llvm::Value* path_cstr = builder.CreateLoad(
            llvm::PointerType::get(builder.getInt8Ty(), 0), path_data_ptr, "path_cstr");
        
        return builder.CreateCall(func, {path_cstr}, "exists_result");
    }
    
    // fileSize(path: string) -> int64
    if (callee_ident->name == "fileSize") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("fileSize() requires one argument (path)");
        }
        
        // Get or declare aria_file_size
        llvm::Function* func = module->getFunction("aria_file_size");
        if (!func) {
            // int64_t aria_file_size(const char* path)
            std::vector<llvm::Type*> params = {
                llvm::PointerType::get(builder.getInt8Ty(), 0)
            };
            llvm::FunctionType* func_type = llvm::FunctionType::get(
                builder.getInt64Ty(), params, false);
            func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                "aria_file_size", module);
        }
        
        // Get path argument
        llvm::Value* path_ptr = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* path_data_ptr = builder.CreateStructGEP(getAriaStringType(), 
            path_ptr, 0, "path_data_ptr");
        llvm::Value* path_cstr = builder.CreateLoad(
            llvm::PointerType::get(builder.getInt8Ty(), 0), path_data_ptr, "path_cstr");
        
        return builder.CreateCall(func, {path_cstr}, "size_result");
    }
    
    // deleteFile(path: string) -> int64
    if (callee_ident->name == "deleteFile") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("deleteFile() requires one argument (path)");
        }
        
        // Get or declare aria_delete_file_simple
        llvm::Function* func = module->getFunction("aria_delete_file_simple");
        if (!func) {
            // int64_t aria_delete_file_simple(const char* path)
            std::vector<llvm::Type*> params = {
                llvm::PointerType::get(builder.getInt8Ty(), 0)
            };
            llvm::FunctionType* func_type = llvm::FunctionType::get(
                builder.getInt64Ty(), params, false);
            func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                "aria_delete_file_simple", module);
        }
        
        // Get path argument
        llvm::Value* path_ptr = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* path_data_ptr = builder.CreateStructGEP(getAriaStringType(), 
            path_ptr, 0, "path_data_ptr");
        llvm::Value* path_cstr = builder.CreateLoad(
            llvm::PointerType::get(builder.getInt8Ty(), 0), path_data_ptr, "path_cstr");
        
        return builder.CreateCall(func, {path_cstr}, "delete_result");
    }
    
    // ====================================================================
    // COMPILER INTRINSICS - Bit Manipulation & Performance Hints
    // ====================================================================
    
    // @clz32/64 - Count leading zeros
    if (callee_ident->name == "@clz32" || callee_ident->name == "clz32") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("clz32() requires one argument");
        }
        llvm::Value* arg = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Function* ctlz = llvm::Intrinsic::getDeclaration(
            module, llvm::Intrinsic::ctlz, {builder.getInt32Ty()});
        llvm::Value* is_zero_undef = builder.getInt1(false);  // Return bit-width if zero
        return builder.CreateCall(ctlz, {arg, is_zero_undef}, "clz");
    }
    
    if (callee_ident->name == "@clz64" || callee_ident->name == "clz64") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("clz64() requires one argument");
        }
        llvm::Value* arg = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Function* ctlz = llvm::Intrinsic::getDeclaration(
            module, llvm::Intrinsic::ctlz, {builder.getInt64Ty()});
        llvm::Value* is_zero_undef = builder.getInt1(false);
        llvm::Value* result64 = builder.CreateCall(ctlz, {arg, is_zero_undef}, "clz");
        return builder.CreateTrunc(result64, builder.getInt32Ty(), "clz32");
    }
    
    // @ctz32/64 - Count trailing zeros
    if (callee_ident->name == "@ctz32" || callee_ident->name == "ctz32") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("ctz32() requires one argument");
        }
        llvm::Value* arg = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Function* cttz = llvm::Intrinsic::getDeclaration(
            module, llvm::Intrinsic::cttz, {builder.getInt32Ty()});
        llvm::Value* is_zero_undef = builder.getInt1(false);
        return builder.CreateCall(cttz, {arg, is_zero_undef}, "ctz");
    }
    
    if (callee_ident->name == "@ctz64" || callee_ident->name == "ctz64") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("ctz64() requires one argument");
        }
        llvm::Value* arg = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Function* cttz = llvm::Intrinsic::getDeclaration(
            module, llvm::Intrinsic::cttz, {builder.getInt64Ty()});
        llvm::Value* is_zero_undef = builder.getInt1(false);
        llvm::Value* result64 = builder.CreateCall(cttz, {arg, is_zero_undef}, "ctz");
        return builder.CreateTrunc(result64, builder.getInt32Ty(), "ctz32");
    }
    
    // @popcount32/64 - Count set bits
    if (callee_ident->name == "@popcount32" || callee_ident->name == "popcount32") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("popcount32() requires one argument");
        }
        llvm::Value* arg = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Function* ctpop = llvm::Intrinsic::getDeclaration(
            module, llvm::Intrinsic::ctpop, {builder.getInt32Ty()});
        return builder.CreateCall(ctpop, {arg}, "popcount");
    }
    
    if (callee_ident->name == "@popcount64" || callee_ident->name == "popcount64") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("popcount64() requires one argument");
        }
        llvm::Value* arg = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Function* ctpop = llvm::Intrinsic::getDeclaration(
            module, llvm::Intrinsic::ctpop, {builder.getInt64Ty()});
        llvm::Value* result64 = builder.CreateCall(ctpop, {arg}, "popcount");
        return builder.CreateTrunc(result64, builder.getInt32Ty(), "popcount32");
    }
    
    // @bswap16/32/64 - Byte swap
    if (callee_ident->name == "@bswap16" || callee_ident->name == "bswap16") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("bswap16() requires one argument");
        }
        llvm::Value* arg = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Function* bswap = llvm::Intrinsic::getDeclaration(
            module, llvm::Intrinsic::bswap, {builder.getInt16Ty()});
        return builder.CreateCall(bswap, {arg}, "bswap");
    }
    
    if (callee_ident->name == "@bswap32" || callee_ident->name == "bswap32") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("bswap32() requires one argument");
        }
        llvm::Value* arg = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Function* bswap = llvm::Intrinsic::getDeclaration(
            module, llvm::Intrinsic::bswap, {builder.getInt32Ty()});
        return builder.CreateCall(bswap, {arg}, "bswap");
    }
    
    if (callee_ident->name == "@bswap64" || callee_ident->name == "bswap64") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("bswap64() requires one argument");
        }
        llvm::Value* arg = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Function* bswap = llvm::Intrinsic::getDeclaration(
            module, llvm::Intrinsic::bswap, {builder.getInt64Ty()});
        return builder.CreateCall(bswap, {arg}, "bswap");
    }
    
    // @likely/unlikely - Branch prediction hints
    if (callee_ident->name == "@likely" || callee_ident->name == "likely") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("likely() requires one argument");
        }
        llvm::Value* arg = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Function* expect = llvm::Intrinsic::getDeclaration(
            module, llvm::Intrinsic::expect, {builder.getInt1Ty()});
        llvm::Value* true_val = builder.getInt1(true);
        return builder.CreateCall(expect, {arg, true_val}, "likely");
    }
    
    if (callee_ident->name == "@unlikely" || callee_ident->name == "unlikely") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("unlikely() requires one argument");
        }
        llvm::Value* arg = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Function* expect = llvm::Intrinsic::getDeclaration(
            module, llvm::Intrinsic::expect, {builder.getInt1Ty()});
        llvm::Value* false_val = builder.getInt1(false);
        return builder.CreateCall(expect, {arg, false_val}, "unlikely");
    }
    
    // @prefetch - Cache prefetch hint
    if (callee_ident->name == "@prefetch" || callee_ident->name == "prefetch") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("prefetch() requires one argument");
        }
        llvm::Value* ptr = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Function* prefetch = llvm::Intrinsic::getDeclaration(
            module, llvm::Intrinsic::prefetch, {ptr->getType()});
        // rw=0 (read), locality=3 (high), cache type=1 (data)
        llvm::Value* rw = builder.getInt32(0);
        llvm::Value* locality = builder.getInt32(3);
        llvm::Value* cache_type = builder.getInt32(1);
        builder.CreateCall(prefetch, {ptr, rw, locality, cache_type});
        // Return void - but need a placeholder value
        return llvm::ConstantInt::get(builder.getInt32Ty(), 0);
    }
    
    // @breakpoint - Debugger breakpoint
    if (callee_ident->name == "@breakpoint" || callee_ident->name == "breakpoint") {
        llvm::Function* debugtrap = llvm::Intrinsic::getDeclaration(
            module, llvm::Intrinsic::debugtrap);
        builder.CreateCall(debugtrap);
        return llvm::ConstantInt::get(builder.getInt32Ty(), 0);
    }
    
    // @trap - Intentional trap/abort
    if (callee_ident->name == "@trap" || callee_ident->name == "trap") {
        llvm::Function* trap = llvm::Intrinsic::getDeclaration(
            module, llvm::Intrinsic::trap);
        builder.CreateCall(trap);
        // Insert unreachable after trap
        builder.CreateUnreachable();
        // Return dummy value (dead code)
        return llvm::ConstantInt::get(builder.getInt32Ty(), 0);
    }
    
    // @unreachable - Mark code as unreachable
    if (callee_ident->name == "@unreachable" || callee_ident->name == "unreachable") {
        builder.CreateUnreachable();
        return llvm::ConstantInt::get(builder.getInt32Ty(), 0);
    }
    
    // ====================================================================
    // MATH LIBRARY BUILTINS (Phase 4.4)
    // ====================================================================
    
    // abs() - absolute value (works with int and float types)
    if (callee_ident->name == "abs") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("abs() requires one argument");
        }
        llvm::Value* arg = codegenExpressionNode(expr->arguments[0].get(), this);
        
        if (arg->getType()->isIntegerTy()) {
            // Integer absolute value: select(x < 0, -x, x)
            llvm::Value* zero = llvm::ConstantInt::get(arg->getType(), 0);
            llvm::Value* is_negative = builder.CreateICmpSLT(arg, zero, "is_neg");
            llvm::Value* negated = builder.CreateNeg(arg, "negated");
            return builder.CreateSelect(is_negative, negated, arg, "abs");
        } else if (arg->getType()->isDoubleTy()) {
            // Float absolute value: llvm.fabs intrinsic
            llvm::Function* fabs_func = llvm::Intrinsic::getDeclaration(
                module, llvm::Intrinsic::fabs, {arg->getType()});
            return builder.CreateCall(fabs_func, {arg}, "abs");
        } else if (arg->getType()->isFloatTy()) {
            llvm::Function* fabs_func = llvm::Intrinsic::getDeclaration(
                module, llvm::Intrinsic::fabs, {arg->getType()});
            return builder.CreateCall(fabs_func, {arg}, "abs");
        }
        throw std::runtime_error("abs() requires numeric type");
    }
    
    // min() and max()
    if (callee_ident->name == "min" || callee_ident->name == "max") {
        if (expr->arguments.size() != 2) {
            throw std::runtime_error(callee_ident->name + "() requires two arguments");
        }
        llvm::Value* arg1 = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* arg2 = codegenExpressionNode(expr->arguments[1].get(), this);
        
        bool is_min = (callee_ident->name == "min");
        
        if (arg1->getType()->isIntegerTy()) {
            llvm::Value* cmp = is_min ? 
                builder.CreateICmpSLT(arg1, arg2, "cmp") :
                builder.CreateICmpSGT(arg1, arg2, "cmp");
            return builder.CreateSelect(cmp, arg1, arg2, callee_ident->name);
        } else if (arg1->getType()->isFloatingPointTy()) {
            // Use LLVM intrinsics for float min/max
            llvm::Intrinsic::ID intrinsic_id = is_min ? 
                llvm::Intrinsic::minnum : llvm::Intrinsic::maxnum;
            llvm::Function* minmax_func = llvm::Intrinsic::getDeclaration(
                module, intrinsic_id, {arg1->getType()});
            return builder.CreateCall(minmax_func, {arg1, arg2}, callee_ident->name);
        }
        throw std::runtime_error(callee_ident->name + "() requires numeric types");
    }
    
    // Rounding functions: floor, ceil, round, trunc
    if (callee_ident->name == "floor" || callee_ident->name == "ceil" ||
        callee_ident->name == "round" || callee_ident->name == "trunc") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error(callee_ident->name + "() requires one argument");
        }
        llvm::Value* arg = codegenExpressionNode(expr->arguments[0].get(), this);
        
        // Convert to double if needed
        if (!arg->getType()->isDoubleTy()) {
            if (arg->getType()->isFloatTy()) {
                arg = builder.CreateFPExt(arg, builder.getDoubleTy());
            } else if (arg->getType()->isIntegerTy()) {
                arg = builder.CreateSIToFP(arg, builder.getDoubleTy());
            }
        }
        
        llvm::Intrinsic::ID intrinsic_id;
        if (callee_ident->name == "floor") intrinsic_id = llvm::Intrinsic::floor;
        else if (callee_ident->name == "ceil") intrinsic_id = llvm::Intrinsic::ceil;
        else if (callee_ident->name == "round") intrinsic_id = llvm::Intrinsic::round;
        else intrinsic_id = llvm::Intrinsic::trunc;
        
        llvm::Function* func = llvm::Intrinsic::getDeclaration(
            module, intrinsic_id, {builder.getDoubleTy()});
        return builder.CreateCall(func, {arg}, callee_ident->name);
    }
    
    // sqrt()
    if (callee_ident->name == "sqrt") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("sqrt() requires one argument");
        }
        llvm::Value* arg = codegenExpressionNode(expr->arguments[0].get(), this);
        if (!arg->getType()->isDoubleTy()) {
            if (arg->getType()->isFloatTy()) {
                arg = builder.CreateFPExt(arg, builder.getDoubleTy());
            } else if (arg->getType()->isIntegerTy()) {
                arg = builder.CreateSIToFP(arg, builder.getDoubleTy());
            }
        }
        llvm::Function* sqrt_func = llvm::Intrinsic::getDeclaration(
            module, llvm::Intrinsic::sqrt, {builder.getDoubleTy()});
        return builder.CreateCall(sqrt_func, {arg}, "sqrt");
    }

    // cbrt() - cube root via libm
    if (callee_ident->name == "cbrt") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("cbrt() requires one argument");
        }
        llvm::Value* arg = codegenExpressionNode(expr->arguments[0].get(), this);
        if (!arg->getType()->isDoubleTy()) {
            if (arg->getType()->isFloatTy()) {
                arg = builder.CreateFPExt(arg, builder.getDoubleTy());
            } else if (arg->getType()->isIntegerTy()) {
                arg = builder.CreateSIToFP(arg, builder.getDoubleTy());
            }
        }

        // Declare libm cbrt function
        llvm::Function* cbrt_func = module->getFunction("cbrt");
        if (!cbrt_func) {
            llvm::FunctionType* func_type = llvm::FunctionType::get(
                builder.getDoubleTy(), {builder.getDoubleTy()}, false);
            cbrt_func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                "cbrt", module);
        }
        return builder.CreateCall(cbrt_func, {arg}, "cbrt");
    }

    // pow()
    if (callee_ident->name == "pow") {
        if (expr->arguments.size() != 2) {
            throw std::runtime_error("pow() requires two arguments");
        }
        llvm::Value* base = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* exp = codegenExpressionNode(expr->arguments[1].get(), this);
        
        if (!base->getType()->isDoubleTy()) {
            if (base->getType()->isFloatTy()) {
                base = builder.CreateFPExt(base, builder.getDoubleTy());
            } else if (base->getType()->isIntegerTy()) {
                base = builder.CreateSIToFP(base, builder.getDoubleTy());
            }
        }
        if (!exp->getType()->isDoubleTy()) {
            if (exp->getType()->isFloatTy()) {
                exp = builder.CreateFPExt(exp, builder.getDoubleTy());
            } else if (exp->getType()->isIntegerTy()) {
                exp = builder.CreateSIToFP(exp, builder.getDoubleTy());
            }
        }
        
        llvm::Function* pow_func = llvm::Intrinsic::getDeclaration(
            module, llvm::Intrinsic::pow, {builder.getDoubleTy()});
        return builder.CreateCall(pow_func, {base, exp}, "pow");
    }
    
    // exp()
    if (callee_ident->name == "exp") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error("exp() requires one argument");
        }
        llvm::Value* arg = codegenExpressionNode(expr->arguments[0].get(), this);
        if (!arg->getType()->isDoubleTy()) {
            if (arg->getType()->isFloatTy()) {
                arg = builder.CreateFPExt(arg, builder.getDoubleTy());
            } else if (arg->getType()->isIntegerTy()) {
                arg = builder.CreateSIToFP(arg, builder.getDoubleTy());
            }
        }
        llvm::Function* exp_func = llvm::Intrinsic::getDeclaration(
            module, llvm::Intrinsic::exp, {builder.getDoubleTy()});
        return builder.CreateCall(exp_func, {arg}, "exp");
    }
    
    // log(), ln(), log10(), log2() - ln is alias for natural log
    if (callee_ident->name == "log" || callee_ident->name == "ln" ||
        callee_ident->name == "log10" || callee_ident->name == "log2") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error(callee_ident->name + "() requires one argument");
        }
        llvm::Value* arg = codegenExpressionNode(expr->arguments[0].get(), this);
        if (!arg->getType()->isDoubleTy()) {
            if (arg->getType()->isFloatTy()) {
                arg = builder.CreateFPExt(arg, builder.getDoubleTy());
            } else if (arg->getType()->isIntegerTy()) {
                arg = builder.CreateSIToFP(arg, builder.getDoubleTy());
            }
        }
        
        llvm::Intrinsic::ID intrinsic_id;
        if (callee_ident->name == "log" || callee_ident->name == "ln")
            intrinsic_id = llvm::Intrinsic::log;  // ln is alias for natural log
        else if (callee_ident->name == "log10") intrinsic_id = llvm::Intrinsic::log10;
        else intrinsic_id = llvm::Intrinsic::log2;
        
        llvm::Function* log_func = llvm::Intrinsic::getDeclaration(
            module, intrinsic_id, {builder.getDoubleTy()});
        return builder.CreateCall(log_func, {arg}, callee_ident->name);
    }
    
    // Trigonometric functions: sin, cos
    if (callee_ident->name == "sin" || callee_ident->name == "cos") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error(callee_ident->name + "() requires one argument");
        }
        llvm::Value* arg = codegenExpressionNode(expr->arguments[0].get(), this);
        if (!arg->getType()->isDoubleTy()) {
            if (arg->getType()->isFloatTy()) {
                arg = builder.CreateFPExt(arg, builder.getDoubleTy());
            } else if (arg->getType()->isIntegerTy()) {
                arg = builder.CreateSIToFP(arg, builder.getDoubleTy());
            }
        }
        
        llvm::Intrinsic::ID intrinsic_id = (callee_ident->name == "sin") ? 
            llvm::Intrinsic::sin : llvm::Intrinsic::cos;
        llvm::Function* trig_func = llvm::Intrinsic::getDeclaration(
            module, intrinsic_id, {builder.getDoubleTy()});
        return builder.CreateCall(trig_func, {arg}, callee_ident->name);
    }
    
    // Trigonometric functions requiring libm: tan, asin, acos, atan, atan2
    if (callee_ident->name == "tan" || callee_ident->name == "asin" || 
        callee_ident->name == "acos" || callee_ident->name == "atan") {
        if (expr->arguments.size() != 1) {
            throw std::runtime_error(callee_ident->name + "() requires one argument");
        }
        llvm::Value* arg = codegenExpressionNode(expr->arguments[0].get(), this);
        if (!arg->getType()->isDoubleTy()) {
            if (arg->getType()->isFloatTy()) {
                arg = builder.CreateFPExt(arg, builder.getDoubleTy());
            } else if (arg->getType()->isIntegerTy()) {
                arg = builder.CreateSIToFP(arg, builder.getDoubleTy());
            }
        }
        
        // Declare libm function
        llvm::Function* libm_func = module->getFunction(callee_ident->name);
        if (!libm_func) {
            llvm::FunctionType* func_type = llvm::FunctionType::get(
                builder.getDoubleTy(), {builder.getDoubleTy()}, false);
            libm_func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                callee_ident->name, module);
        }
        return builder.CreateCall(libm_func, {arg}, callee_ident->name);
    }
    
    if (callee_ident->name == "atan2") {
        if (expr->arguments.size() != 2) {
            throw std::runtime_error("atan2() requires two arguments");
        }
        llvm::Value* y = codegenExpressionNode(expr->arguments[0].get(), this);
        llvm::Value* x = codegenExpressionNode(expr->arguments[1].get(), this);
        
        if (!y->getType()->isDoubleTy()) {
            if (y->getType()->isFloatTy()) {
                y = builder.CreateFPExt(y, builder.getDoubleTy());
            } else if (y->getType()->isIntegerTy()) {
                y = builder.CreateSIToFP(y, builder.getDoubleTy());
            }
        }
        if (!x->getType()->isDoubleTy()) {
            if (x->getType()->isFloatTy()) {
                x = builder.CreateFPExt(x, builder.getDoubleTy());
            } else if (x->getType()->isIntegerTy()) {
                x = builder.CreateSIToFP(x, builder.getDoubleTy());
            }
        }
        
        llvm::Function* atan2_func = module->getFunction("atan2");
        if (!atan2_func) {
            llvm::FunctionType* func_type = llvm::FunctionType::get(
                builder.getDoubleTy(), {builder.getDoubleTy(), builder.getDoubleTy()}, false);
            atan2_func = llvm::Function::Create(func_type, llvm::Function::ExternalLinkage,
                "atan2", module);
        }
        return builder.CreateCall(atan2_func, {y, x}, "atan2");
    }
    
    // Mathematical constants: PI, E, TAU
    if (callee_ident->name == "PI") {
        if (expr->arguments.size() != 0) {
            throw std::runtime_error("PI() takes no arguments");
        }
        return llvm::ConstantFP::get(builder.getDoubleTy(), 3.14159265358979323846);
    }
    
    if (callee_ident->name == "E") {
        if (expr->arguments.size() != 0) {
            throw std::runtime_error("E() takes no arguments");
        }
        return llvm::ConstantFP::get(builder.getDoubleTy(), 2.71828182845904523536);
    }
    
    if (callee_ident->name == "TAU") {
        if (expr->arguments.size() != 0) {
            throw std::runtime_error("TAU() takes no arguments");
        }
        return llvm::ConstantFP::get(builder.getDoubleTy(), 6.28318530717958647692);
    }
    
    // ====================================================================
    // END MATH BUILTINS
    // ====================================================================
    
    // Check if this is a direct function call or a closure call
    // For generic functions, use the specialized mangled name if available
    std::string func_name = !expr->specializedMangledName.empty() 
                            ? expr->specializedMangledName 
                            : callee_ident->name;
    
    // Try to find a direct function first
    llvm::Function* direct_func = module->getFunction(func_name);
    
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
        
        // Try to determine return type from var_aria_types
        // func_ptr variables are stored as "func_ptr:retTypeName"
        llvm::Type* return_type = llvm::Type::getInt64Ty(context); // default fallback
        {
            auto type_it = var_aria_types.find(callee_ident->name);
            if (type_it != var_aria_types.end()) {
                const std::string& aria_type = type_it->second;
                if (aria_type.size() > 9 && aria_type.substr(0, 9) == "func_ptr:") {
                    std::string ret_str = aria_type.substr(9);
                    if (ret_str == "void") {
                        return_type = llvm::Type::getVoidTy(context);
                    } else {
                        llvm::Type* inner_type = getLLVMTypeFromString(ret_str);
                        if (inner_type) {
                            // Wrap in Result<T>: { T, i8*, i1 } matching module-level functions
                            return_type = llvm::StructType::get(context, {
                                inner_type,
                                llvm::PointerType::get(context, 0),
                                llvm::Type::getInt1Ty(context)
                            });
                        }
                    }
                }
            }
        }
        
        llvm::FunctionType* closure_func_type = llvm::FunctionType::get(
            return_type,
            param_types,
            false  // not vararg
        );
        
        // In LLVM 16+ (opaque pointer mode), all pointers are plain `ptr`.
        // No bitcast is needed — method_ptr is already `ptr`, and CreateCall
        // takes the FunctionType + a ptr callee directly.
        
        // Create indirect call through function pointer
        return builder.CreateCall(closure_func_type, method_ptr, args, "closure_call");
        
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
            
            // PIPELINE OPERATOR FIX: Auto-unwrap Result<T> → T for pipeline calls
            // When using |> or <|, if a function returns Result<T>, automatically 
            // extract the .val field when passing to the next function expecting T
            if (expr->isPipelineCall && arg_value->getType()->isStructTy()) {
                llvm::StructType* arg_struct = llvm::cast<llvm::StructType>(arg_value->getType());
                
                // Result types are structs with 3 fields: {T val, error* err, i1 is_error}
                // Check if this looks like a Result type by field count and types
                if (arg_struct->getNumElements() == 3 &&
                    arg_struct->getElementType(1)->isPointerTy() &&
                    arg_struct->getElementType(2)->isIntegerTy(1)) {
                    
                    // Extract the value field (field 0) from Result
                    arg_value = builder.CreateExtractValue(arg_value, 0, "pipeline_unwrap");
                }
            }
            
            // ARIA-026 FIX: LBIM ABI Scalarization for extern functions
            // CRITICAL FIX 1: AriaString data pointer extraction for FFI
            // Check if this is an extern function and the argument is an LBIM struct or AriaString
            bool is_extern = direct_func->hasExternalLinkage() && direct_func->isDeclaration();
            llvm::Type* arg_type = arg_value->getType();
            
            if (is_extern && arg_type->isStructTy()) {
                llvm::StructType* struct_type = llvm::cast<llvm::StructType>(arg_type);
                std::string struct_name = struct_type->hasName() ? struct_type->getName().str() : "";
                
                // CRITICAL FIX 1: Extract data pointer from AriaString when passing to C
                // AriaString is { i8* data, i64 length } but C functions expect char*
                // Without this, C receives address of struct instead of string data
                if (struct_name == "struct.AriaString") {
                    // Extract just the data pointer (field 0)
                    llvm::Value* data_ptr = builder.CreateExtractValue(arg_value, 0, "str_data");
                    arg_value = data_ptr;
                }
                // Check if this is an LBIM type (int128, uint128, int256, etc.)
                else if (struct_name.find("struct.int128") == 0 || struct_name.find("struct.uint128") == 0) {
                    // Scalarize int128: Extract 2 limbs and construct native i128
                    llvm::Value* limb0 = builder.CreateExtractValue(arg_value, {0, 0}, "limb0");
                    llvm::Value* limb1 = builder.CreateExtractValue(arg_value, {0, 1}, "limb1");
                    
                    // Construct i128: (limb1 << 64) | limb0
                    llvm::Type* i128Type = llvm::Type::getInt128Ty(context);
                    llvm::Value* limb0_ext = builder.CreateZExt(limb0, i128Type, "limb0_ext");
                    llvm::Value* limb1_ext = builder.CreateZExt(limb1, i128Type, "limb1_ext");
                    llvm::Value* limb1_shifted = builder.CreateShl(limb1_ext, 64, "limb1_shift");
                    llvm::Value* scalar_i128 = builder.CreateOr(limb0_ext, limb1_shifted, "scalar_i128");
                    
                    arg_value = scalar_i128;
                }
                // TODO: Add scalarization for int256, int512, int1024, int2048, int4096 if needed
                // These would require platform-specific handling or rejection at semantic analysis
            }
            
            // CRITICAL FIX 2: AriaString* pointer → char* for FFI (opaque pointer mode)
            // In LLVM opaque pointer mode, AriaString* has type 'ptr' — can't detect by type alone.
            // Detect via two sources:
            //   a) String LITERALS: arg_value is a GlobalVariable pointing to struct.AriaString
            //   b) String VARIABLES: identifier is in var_aria_types with type "string"
            if (is_extern && arg_type->isPointerTy()) {
                bool needs_data_extraction = false;
                
                // Case (a): String literal → GlobalVariable of type struct.AriaString
                if (auto* gv = llvm::dyn_cast<llvm::GlobalVariable>(arg_value)) {
                    llvm::Type* val_type = gv->getValueType();
                    if (val_type->isStructTy()) {
                        llvm::StructType* st = llvm::cast<llvm::StructType>(val_type);
                        if (st->hasName() && st->getName() == "struct.AriaString") {
                            needs_data_extraction = true;
                        }
                    }
                }
                
                // Case (b): String variable → identifier with var_aria_types["name"] == "string"
                if (!needs_data_extraction && i < expr->arguments.size()) {
                    ASTNode* arg_node = expr->arguments[i].get();
                    if (arg_node->type == aria::ASTNode::NodeType::IDENTIFIER) {
                        IdentifierExpr* ident = static_cast<IdentifierExpr*>(arg_node);
                        auto type_it = var_aria_types.find(ident->name);
                        if (type_it != var_aria_types.end() && type_it->second == "string") {
                            needs_data_extraction = true;
                        }
                    }
                }
                
                if (needs_data_extraction) {
                    // Load AriaString struct from the AriaString* pointer, then extract .data
                    llvm::StructType* aria_string_type = getAriaStringType();
                    llvm::Value* str_struct = builder.CreateLoad(aria_string_type, arg_value, "str_struct_ffi");
                    arg_value = builder.CreateExtractValue(str_struct, 0, "str_data_ffi");
                }
            }

            // Array-to-pointer decay: flt64[N] → flt64@ (like C implicit array decay)
            // If the argument is an array type [N x T] but the parameter expects a pointer,
            // store the array in a temp alloca and pass a pointer to its first element.
            if (i < direct_func->getFunctionType()->getNumParams()) {
                llvm::Type* param_type = direct_func->getFunctionType()->getParamType(i);
                if (arg_value->getType()->isArrayTy() && param_type->isPointerTy()) {
                    llvm::Type* arr_type = arg_value->getType();
                    llvm::AllocaInst* arr_tmp = builder.CreateAlloca(arr_type, nullptr, "arr_decay_tmp");
                    builder.CreateStore(arg_value, arr_tmp);
                    arg_value = builder.CreateInBoundsGEP(
                        arr_type, arr_tmp,
                        {builder.getInt64(0), builder.getInt64(0)},
                        "arr_decay_ptr"
                    );
                }
                // Auto-unwrap Result<T> → T when passing a function-call result directly
                // as an argument to a function expecting the raw type.
                // Aria result structs are { T, ptr, i8 } (3-element, ptr at index 1, i8 at index 2).
                if (arg_value->getType() != param_type &&
                    arg_value->getType()->isStructTy()) {
                    llvm::StructType* arg_struct = llvm::cast<llvm::StructType>(arg_value->getType());
                    if (arg_struct->getNumElements() == 3 &&
                        arg_struct->getElementType(1)->isPointerTy() &&
                        arg_struct->getElementType(2)->isIntegerTy(8) &&
                        arg_struct->getElementType(0) == param_type) {
                        arg_value = builder.CreateExtractValue(arg_value, {0}, "arg_unwrap");
                    }
                }
            }

            args.push_back(arg_value);
        }
        
        // Generate the call instruction
        llvm::Value* call_result = builder.CreateCall(direct_func, args, "calltmp");
        
        // BUG-09-001 FIX: Auto-wrap FFI pointer returns in Optional
        // Check if this is an extern function returning a pointer
        llvm::Type* return_type = direct_func->getReturnType();
        bool is_extern = direct_func->hasExternalLinkage() && direct_func->isDeclaration();
        
        if (is_extern && return_type->isPointerTy()) {
            // Wrap NULL-possible pointer in Optional { i1 hasValue, T* value }
            // This enforces null checks at the Aria boundary
            
            // Create null pointer constant for comparison
            llvm::Constant* null_ptr = llvm::ConstantPointerNull::get(
                llvm::cast<llvm::PointerType>(return_type)
            );
            
            // Check if result is NULL
            llvm::Value* is_null = builder.CreateICmpEQ(call_result, null_ptr, "ffi_is_null");
            llvm::Value* has_value = builder.CreateNot(is_null, "ffi_has_value");
            
            // Create Optional struct: { i1, T* }
            std::vector<llvm::Type*> optional_fields = { builder.getInt1Ty(), return_type };
            llvm::StructType* optional_type = llvm::StructType::get(context, optional_fields);
            
            // Build Optional value
            llvm::Value* optional_val = llvm::UndefValue::get(optional_type);
            optional_val = builder.CreateInsertValue(optional_val, has_value, 0, "optional_hasval");
            optional_val = builder.CreateInsertValue(optional_val, call_result, 1, "optional_ptr");
            
            return optional_val;
        }
        
        // PIPELINE OPERATOR FIX: Auto-unwrap Result<T> → T for pipeline call return values
        // When the final result of a pipeline (value |> f1 |> f2) is assigned to a T variable,
        // extract the .val field from the Result<T> return value
        if (expr->isPipelineCall && return_type->isStructTy()) {
            llvm::StructType* return_struct = llvm::cast<llvm::StructType>(return_type);
            
            // Result types are structs with 3 fields: {T val, error* err, i1 is_error}
            // Check if this looks like a Result type by field count and types
            if (return_struct->getNumElements() == 3 &&
                return_struct->getElementType(1)->isPointerTy() &&
                return_struct->getElementType(2)->isIntegerTy(1)) {
                
                // Extract the value field (field 0) from Result
                call_result = builder.CreateExtractValue(call_result, 0, "pipeline_result_unwrap");
            }
        }
        
        return call_result;
        
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
    
    // Special handling for identifier bases: avoid loading the whole array value.
    // For fixed-size arrays [N x T], we need the raw alloca pointer, not the loaded value.
    if (expr->array->type == ASTNode::NodeType::IDENTIFIER) {
        IdentifierExpr* ident = static_cast<IdentifierExpr*>(expr->array.get());
        auto it = named_values.find(ident->name);
        if (it != named_values.end()) {
            llvm::Value* var = it->second;
            if (auto* allocaInst = llvm::dyn_cast<llvm::AllocaInst>(var)) {
                llvm::Type* allocated_type = allocaInst->getAllocatedType();
                if (allocated_type->isArrayTy()) {
                    // Properly-typed fixed-size array [N x T]
                    auto* arrTy = llvm::cast<llvm::ArrayType>(allocated_type);
                    llvm::Type* elemType = arrTy->getElementType();
                    
                    llvm::Value* indexVal = codegenExpressionNode(expr->index.get(), this);
                    if (!indexVal) {
                        throw std::runtime_error("Failed to generate index expression");
                    }
                    if (!indexVal->getType()->isIntegerTy(64)) {
                        indexVal = builder.CreateSExtOrTrunc(indexVal,
                                       builder.getInt64Ty(), "idx.i64");
                    }
                    std::vector<llvm::Value*> gep_indices = {
                        llvm::ConstantInt::get(builder.getInt64Ty(), 0),
                        indexVal
                    };
                    llvm::Value* elem_ptr = builder.CreateInBoundsGEP(
                        allocated_type, var, gep_indices, "array.index.ptr");
                    return builder.CreateLoad(elemType, elem_ptr, "array.index.val");
                }
            }
        }
    }

    // Multi-dimensional array read: matrix[i][j] or deeper.
    // Walk the nested INDEX chain to find the root IDENTIFIER and collect all
    // dimension indices (outermost index last in the collection vector).
    if (expr->array->type == ASTNode::NodeType::INDEX) {
        std::vector<ASTNode*> all_indices;
        all_indices.push_back(expr->index.get()); // outermost index of this pair
        ASTNode* chain = expr->array.get();
        while (chain->type == ASTNode::NodeType::INDEX) {
            IndexExpr* inner = static_cast<IndexExpr*>(chain);
            all_indices.push_back(inner->index.get());
            chain = inner->array.get();
        }
        if (chain->type == ASTNode::NodeType::IDENTIFIER) {
            IdentifierExpr* root_ident = static_cast<IdentifierExpr*>(chain);
            auto nd_it = named_values.find(root_ident->name);
            if (nd_it != named_values.end()) {
                if (auto* nd_alloca = llvm::dyn_cast<llvm::AllocaInst>(nd_it->second)) {
                    llvm::Type* nd_alloc_ty = nd_alloca->getAllocatedType();
                    if (nd_alloc_ty->isArrayTy()) {
                        // all_indices = [outerIdx, ..., innerIdx] due to walk order.
                        // Iterate in REVERSE so GEP receives indices outermost-first:
                        // for matrix[i][j]: all_indices=[j,i], GEP needs [0, i, j].
                        std::vector<llvm::Value*> gep_idx = {
                            llvm::ConstantInt::get(builder.getInt64Ty(), 0)
                        };
                        for (int k = (int)all_indices.size() - 1; k >= 0; --k) {
                            llvm::Value* idx = codegenExpressionNode(all_indices[k], this);
                            if (!idx) throw std::runtime_error("Failed to codegen array index");
                            if (!idx->getType()->isIntegerTy(64))
                                idx = builder.CreateSExtOrTrunc(idx, builder.getInt64Ty(), "idx.i64");
                            gep_idx.push_back(idx);
                        }
                        // Descend the LLVM type hierarchy (same reverse order) to find element type
                        llvm::Type* elem_ty = nd_alloc_ty;
                        bool valid = true;
                        for (size_t k = 0; k < all_indices.size(); ++k) {
                            if (!elem_ty->isArrayTy()) { valid = false; break; }
                            elem_ty = llvm::cast<llvm::ArrayType>(elem_ty)->getElementType();
                        }
                        if (valid) {
                            llvm::Value* ptr = builder.CreateInBoundsGEP(
                                nd_alloc_ty, nd_alloca, gep_idx, "arrnd.ptr");
                            return builder.CreateLoad(elem_ty, ptr, "arrnd.val");
                        }
                    }
                }
            }
        }
    }

    // Struct field array access: obj.array_field[i]
    // e.g., gl.groups[0], c.nums[i]
    // We need a pointer to the element, so we use struct GEP + array GEP instead
    // of loading the whole array value (which would give us a non-pointer).
    if (expr->array->type == ASTNode::NodeType::MEMBER_ACCESS) {
        MemberAccessExpr* memberExpr = static_cast<MemberAccessExpr*>(expr->array.get());
        if (memberExpr->object->type == ASTNode::NodeType::IDENTIFIER) {
            IdentifierExpr* objIdent = static_cast<IdentifierExpr*>(memberExpr->object.get());
            auto obj_it = named_values.find(objIdent->name);
            if (obj_it != named_values.end()) {
                llvm::Value* structPtr = obj_it->second;
                llvm::Type* structAllocType = nullptr;
                if (auto* ai = llvm::dyn_cast<llvm::AllocaInst>(structPtr))
                    structAllocType = ai->getAllocatedType();
                else if (auto* gv = llvm::dyn_cast<llvm::GlobalVariable>(structPtr))
                    structAllocType = gv->getValueType();
                if (structAllocType && structAllocType->isStructTy()) {
                    int fieldIdx = -1;
                    if (type_system) {
                        auto ariaTypeIt = var_aria_types.find(objIdent->name);
                        if (ariaTypeIt != var_aria_types.end()) {
                            sema::StructType* ariaStructType =
                                type_system->getStructType(ariaTypeIt->second);
                            if (ariaStructType) {
                                fieldIdx = ariaStructType->getFieldIndex(memberExpr->member);
                            }
                        }
                    }
                    if (fieldIdx >= 0) {
                        llvm::StructType* llvmStructTy =
                            llvm::cast<llvm::StructType>(structAllocType);
                        llvm::Type* fieldLLVMType = llvmStructTy->getElementType(fieldIdx);
                        if (fieldLLVMType->isArrayTy()) {
                            auto* arrTy = llvm::cast<llvm::ArrayType>(fieldLLVMType);
                            llvm::Type* elemTy = arrTy->getElementType();
                            llvm::Value* fieldPtr = builder.CreateStructGEP(
                                structAllocType, structPtr, fieldIdx,
                                memberExpr->member + ".field.ptr");
                            llvm::Value* idxVal =
                                codegenExpressionNode(expr->index.get(), this);
                            if (!idxVal)
                                throw std::runtime_error(
                                    "Failed to codegen struct-field array index");
                            if (!idxVal->getType()->isIntegerTy(64))
                                idxVal = builder.CreateSExtOrTrunc(
                                    idxVal, builder.getInt64Ty(), "idx.i64");
                            std::vector<llvm::Value*> gepIdx = {
                                llvm::ConstantInt::get(builder.getInt64Ty(), 0), idxVal};
                            llvm::Value* elemPtr = builder.CreateInBoundsGEP(
                                fieldLLVMType, fieldPtr, gepIdx,
                                memberExpr->member + ".elem.ptr");
                            return builder.CreateLoad(elemTy, elemPtr,
                                                     memberExpr->member + ".elem");
                        }
                    }
                }
            }
        }
        // Fall through to general path for more complex cases
    }

    // General path: generate code for the array/vector
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
    
    // Handle pointer-to-array (e.g., int64[], string[])
    if (arrayType->isPointerTy()) {
        // Array indexing: arr[i]
        // Determine the element type from the Aria type tracked in var_aria_types.
        // For a wild T-> pointer, var_aria_types stores "T@"; strip the "@" suffix
        // to get T, then map to an LLVM type.  Fallback to i64 (old behaviour).
        llvm::Type* elemType = llvm::Type::getInt64Ty(context);  // fallback

        if (expr->array->type == ASTNode::NodeType::IDENTIFIER) {
            IdentifierExpr* ptr_ident = static_cast<IdentifierExpr*>(expr->array.get());
            auto typeIt = var_aria_types.find(ptr_ident->name);
            if (typeIt != var_aria_types.end()) {
                std::string ariaElemType = typeIt->second;
                // Strip trailing "@" (pointer indicator) to get the value type
                if (!ariaElemType.empty() && ariaElemType.back() == '@') {
                    ariaElemType.pop_back();
                }
                if (!ariaElemType.empty()) {
                    llvm::Type* resolved = getLLVMTypeFromString(ariaElemType);
                    if (resolved) elemType = resolved;
                }
            }
        }

        // Create GEP to access arr[index]
        llvm::Value* elemPtr = builder.CreateGEP(elemType, arrayVal, indexVal, "array.index.ptr");

        // Load the value from the pointer
        return builder.CreateLoad(elemType, elemPtr, "array.index.val");
    }
    
    // Handle vector indexing
    if (arrayType->isVectorTy() || (arrayType->isStructTy() && arrayType->getStructNumElements() == 9)) {
        // For vec2/vec3/SIMD (LLVM FixedVectorType): use ExtractElement
        if (arrayType->isVectorTy()) {
            // LLVM requires index to be i32 for ExtractElement
            if (!indexVal->getType()->isIntegerTy(32)) {
                if (indexVal->getType()->isIntegerTy()) {
                    // Cast to i32
                    indexVal = builder.CreateIntCast(indexVal, builder.getInt32Ty(), 
                                                    true, "idx.i32");
                } else {
                    throw std::runtime_error("Vector index must be an integer type");
                }
            }
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
    
    std::cerr << "[DEBUG codegenMemberAccess] member: " << expr->member << std::endl;
    
    // Generate code for the object
    llvm::Value* objectVal = codegenExpressionNode(expr->object.get(), this);
    if (!objectVal) {
        throw std::runtime_error("Failed to generate code for member access object");
    }
    
    std::cerr << "[DEBUG codegenMemberAccess] objectVal type: ";
    objectVal->getType()->print(llvm::errs());
    std::cerr << std::endl;
    
    // Handle pointer member access (ptr->member)
    // This is equivalent to (*ptr).member - dereference then access
    if (expr->isPointerAccess) {
        // objectVal should be a pointer type
        if (!objectVal->getType()->isPointerTy()) {
            throw std::runtime_error("Arrow operator (->) requires pointer type");
        }
        
        // With LLVM opaque pointers, we need to know the pointed-to type
        // For now, assume it's a struct pointer and load it
        // TODO: Get proper type from type system
        objectVal = builder.CreateLoad(objectVal->getType(), objectVal, "deref_for_member");
    }
    
    llvm::Type* objectType = objectVal->getType();
    
    // Handle Result type member access (.is_error, .value, .error)
    // Reference: include/runtime/result.h - layout is { T value, void* error, bool is_error }
    // The type checker has already validated that this is a Result type with valid members
    if (objectType->isStructTy() && objectType->getStructNumElements() == 3) {
        std::cerr << "[DEBUG_RESULT_ACCESS] Accessing member '" << expr->member << "' on 3-field struct" << std::endl;
        std::cerr << "[DEBUG_RESULT_ACCESS] objectVal is instruction: " << llvm::isa<llvm::Instruction>(objectVal) << std::endl;
        std::cerr << "[DEBUG_RESULT_ACCESS] objectVal is argument: " << llvm::isa<llvm::Argument>(objectVal) << std::endl;
        std::cerr << "[DEBUG_RESULT_ACCESS] objectVal type: ";
        objectVal->getType()->print(llvm::errs());
        std::cerr << std::endl;
        
        // Check if this looks like a Result struct based on member names
        // (The type checker already validated this is a Result type)
        if (expr->member == "is_error") {
            // Field 2: is_error (bool)
            std::cerr << "[DEBUG_RESULT_ACCESS] Extracting is_error (field 2)" << std::endl;
            llvm::Value* result = builder.CreateExtractValue(objectVal, 2, "is_error");
            std::cerr << "[DEBUG_RESULT_ACCESS] Extracted value type: ";
            result->getType()->print(llvm::errs());
            std::cerr << std::endl;
            return result;
        }
        else if (expr->member == "value") {
            // Field 0: value (T)
            std::cerr << "[DEBUG_RESULT_ACCESS] Extracting value (field 0)" << std::endl;
            llvm::Value* result = builder.CreateExtractValue(objectVal, 0, "value");
            std::cerr << "[DEBUG_RESULT_ACCESS] Extracted value type: ";
            result->getType()->print(llvm::errs());
            std::cerr << std::endl;
            return result;
        }
        else if (expr->member == "error") {
            // Field 1: error (void*)
            std::cerr << "[DEBUG_RESULT_ACCESS] Extracting error (field 1)" << std::endl;
            llvm::Value* result = builder.CreateExtractValue(objectVal, 1, "error");
            std::cerr << "[DEBUG_RESULT_ACCESS] Extracted value type: ";
            result->getType()->print(llvm::errs());
            std::cerr << std::endl;
            return result;
        }
    }
    
    // Handle vector component access (.x, .y, .z)
    // vec9 is an anonymous 9-element LLVM struct (no LLVM name).
    // User-defined 9-field structs (e.g. Wave9) ARE named; those must fall through
    // to the regular struct member-access path below.
    bool isAnonymousVec9Struct = objectType->isStructTy()
        && objectType->getStructNumElements() == 9
        && !llvm::cast<llvm::StructType>(objectType)->hasName();
    if (objectType->isVectorTy() || isAnonymousVec9Struct) {
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
    
    // Regular struct member access (. or ->)
    // Need to extract field from struct value
    if (objectType->isStructTy()) {
        llvm::StructType* structType = llvm::cast<llvm::StructType>(objectType);
        std::string structName = structType->hasName() ? structType->getName().str() : "";
        
        std::cerr << "[DEBUG codegenMemberAccess] Struct type: " << structName 
                  << ", num elements: " << structType->getNumElements() << std::endl;
        
        // Get the struct type info from type system to find field index
        // For now, we'll need to look up the field index based on the struct type name
        // This should consult the symbol table or type registry
        
        // Try to get field index from the object's expression type
        // First, check if the object is an identifier so we can look up its type
        IdentifierExpr* objIdent = dynamic_cast<IdentifierExpr*>(expr->object.get());
        if (objIdent) {
            // Look up the Aria type name for this variable
            auto typeIt = var_aria_types.find(objIdent->name);
            if (typeIt != var_aria_types.end()) {
                std::string ariaTypeName = typeIt->second;
                std::cerr << "[DEBUG codegenMemberAccess] Variable " << objIdent->name 
                          << " has Aria type: " << ariaTypeName << std::endl;
                
                // TODO: Look up struct definition in type registry to get field index
                // For now, hardcode based on common struct patterns
                
                // Try to get field index from type system
                if (type_system) {
                    Type* ariaType = type_system->getStructType(ariaTypeName);
                    if (ariaType && ariaType->getKind() == TypeKind::STRUCT) {
                        StructType* structType = static_cast<StructType*>(ariaType);
                        int fieldIndex = structType->getFieldIndex(expr->member);
                        if (fieldIndex >= 0) {
                            std::cerr << "[DEBUG codegenMemberAccess] Found field '" << expr->member 
                                     << "' at index " << fieldIndex << std::endl;
                            return builder.CreateExtractValue(objectVal, fieldIndex, expr->member);
                        } else {
                            throw std::runtime_error("Field '" + expr->member + "' not found in struct " + ariaTypeName);
                        }
                    }
                }
                
                // Try to get field index based on struct layout
                // Most structs have fields in declaration order (0, 1, 2, ...)
                // We need the struct definition to know the field index
                // For testing, let's use field index 0 for first field
                
                // TEMPORARY HACK: Assume single-field structs for now
                if (structType->getNumElements() == 1) {
                    std::cerr << "[DEBUG codegenMemberAccess] Single-field struct, extracting field 0" << std::endl;
                    return builder.CreateExtractValue(objectVal, 0, expr->member);
                }
                
                // For multi-field structs, we need proper field index lookup
                // This requires integration with the semantic analyzer's type system
                throw std::runtime_error("Multi-field struct member access requires type system integration (field: " + expr->member + ")");
            }
        }
        
        // Fallback: try to extract value at index 0
        std::cerr << "[DEBUG codegenMemberAccess] Unknown struct type, attempting index 0" << std::endl;
        if (structType->getNumElements() > 0) {
            return builder.CreateExtractValue(objectVal, 0, expr->member);
        }
    }
    
    throw std::runtime_error("Member access not supported for type: " + expr->member);
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
            std::string paramTypeStr = paramNode->typeNode ? paramNode->typeNode->toString() : "void";
            llvm::Type* param_type = getLLVMTypeFromString(paramTypeStr);
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
    
    std::cerr << "[DEBUG AWAIT] Entering codegenAwait" << std::endl;
    
    // Check that we're in an async function context
    llvm::Function* current_func = builder.GetInsertBlock()->getParent();
    if (!current_func) {
        throw std::runtime_error("await outside of function context");
    }
    
    // Check if current function is async
    // Async functions have special metadata or naming convention
    // For now, check if function name contains "async" or has async attribute
    std::string func_name = std::string(current_func->getName());
    std::cerr << "[DEBUG AWAIT] Function name: " << func_name << std::endl;
    bool is_async = func_name.find("async") != std::string::npos;
    
    std::cerr << "[DEBUG AWAIT] is_async (from name): " << is_async << std::endl;
    
    // Also check function metadata for async marker (if set by frontend)
    if (!is_async && current_func->hasMetadata("aria.async")) {
        is_async = true;
        std::cerr << "[DEBUG AWAIT] is_async (from metadata): true" << std::endl;
    }
    
    if (!is_async) {
        // ERROR: await in non-async function
        // Print proper error message to stderr (user-facing)
        std::cerr << "ERROR: 'await' can only be used in async functions (found in '" 
                  << func_name << "')" << std::endl;
        std::cerr << "  Hint: Change 'func:" << func_name << "' to 'async func:" << func_name << "'" << std::endl;
        
        // Return a dummy value to prevent LLVM crash
        // Return i32 0 (arbitrary - compilation will fail anyway due to error message)
        return llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0);
    }
    
    std::cerr << "[DEBUG AWAIT] Async check passed, continuing..." << std::endl;
    
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

// ============================================================================
// Cast Expression Code Generation (Zero Implicit Conversion)
// ============================================================================

/**
 * Generate code for cast expressions (@cast and @cast_unchecked)
 * Handles: safe widening, checked narrowing, unchecked truncation
 */
llvm::Value* ExprCodegen::codegenCast(CastExpr* expr) {
    if (!expr) {
        throw std::runtime_error("Null cast expression");
    }
    
    // Generate code for the expression being cast
    llvm::Value* sourceValue = codegenExpressionNode(expr->expression.get(), this);
    if (!sourceValue) {
        throw std::runtime_error("Failed to generate code for cast source expression");
    }
    
    // Get LLVM types
    llvm::Type* sourceLLVMType = sourceValue->getType();
    llvm::Type* targetLLVMType = getLLVMTypeFromString(expr->targetType);
    
    if (!targetLLVMType) {
        throw std::runtime_error("Unknown target type in cast: " + expr->targetType);
    }
    
    // If types are already the same, no cast needed
    if (sourceLLVMType == targetLLVMType) {
        return sourceValue;
    }
    
    // Determine cast operation based on type categories
    bool sourceIsInt = sourceLLVMType->isIntegerTy();
    bool targetIsInt = targetLLVMType->isIntegerTy();
    bool sourceIsFloat = sourceLLVMType->isFloatingPointTy();
    bool targetIsFloat = targetLLVMType->isFloatingPointTy();
    
    // Integer to Integer casts
    if (sourceIsInt && targetIsInt) {
        unsigned sourceBits = sourceLLVMType->getIntegerBitWidth();
        unsigned targetBits = targetLLVMType->getIntegerBitWidth();
        
        if (targetBits > sourceBits) {
            // Widening: safe cast - use sign/zero extension
            // Determine if source type is signed based on Aria type name
            bool isSigned = (expr->targetType.find("int") == 0 || 
                           expr->targetType.find("i8") == 0 ||
                           expr->targetType.find("i16") == 0 ||
                           expr->targetType.find("i32") == 0 ||
                           expr->targetType.find("i64") == 0);
            
            if (isSigned) {
                return builder.CreateSExt(sourceValue, targetLLVMType, "cast.sext");
            } else {
                return builder.CreateZExt(sourceValue, targetLLVMType, "cast.zext");
            }
        } else {
            // Narrowing: potentially unsafe
            if (expr->isUnchecked) {
                // @cast_unchecked: just truncate
                return builder.CreateTrunc(sourceValue, targetLLVMType, "cast.trunc");
            } else {
                // @cast: checked narrowing - add runtime overflow check
                // For now, just truncate (TODO: add runtime check and panic)
                // TODO: Implement overflow detection and call panic function
                return builder.CreateTrunc(sourceValue, targetLLVMType, "cast.checked_trunc");
            }
        }
    }
    
    // Float to Float casts
    if (sourceIsFloat && targetIsFloat) {
        unsigned sourceBits = sourceLLVMType->getPrimitiveSizeInBits();
        unsigned targetBits = targetLLVMType->getPrimitiveSizeInBits();
        
        if (targetBits > sourceBits) {
            // Widening: f32 -> f64 (safe)
            return builder.CreateFPExt(sourceValue, targetLLVMType, "cast.fpext");
        } else {
            // Narrowing: f64 -> f32
            if (expr->isUnchecked) {
                // @cast_unchecked: just truncate
                return builder.CreateFPTrunc(sourceValue, targetLLVMType, "cast.fptrunc");
            } else {
                // @cast: checked narrowing
                // TODO: Add range check for overflow/underflow
                return builder.CreateFPTrunc(sourceValue, targetLLVMType, "cast.checked_fptrunc");
            }
        }
    }
    
    // Integer to Float
    if (sourceIsInt && targetIsFloat) {
        // Determine if source is signed
        bool isSigned = true; // Default to signed
        // TODO: Get signedness from type system
        
        if (isSigned) {
            return builder.CreateSIToFP(sourceValue, targetLLVMType, "cast.sitofp");
        } else {
            return builder.CreateUIToFP(sourceValue, targetLLVMType, "cast.uitofp");
        }
    }
    
    // Float to Integer
    if (sourceIsFloat && targetIsInt) {
        // Determine if target is signed
        bool isSigned = (expr->targetType.find("int") == 0 || 
                       expr->targetType[0] == 'i');
        
        if (expr->isUnchecked) {
            // @cast_unchecked: direct conversion (may overflow)
            if (isSigned) {
                return builder.CreateFPToSI(sourceValue, targetLLVMType, "cast.fptosi");
            } else {
                return builder.CreateFPToUI(sourceValue, targetLLVMType, "cast.fptoui");
            }
        } else {
            // @cast: checked conversion
            // TODO: Add range check (ensure float value fits in target int range)
            if (isSigned) {
                return builder.CreateFPToSI(sourceValue, targetLLVMType, "cast.checked_fptosi");
            } else {
                return builder.CreateFPToUI(sourceValue, targetLLVMType, "cast.checked_fptoui");
            }
        }
    }
    
    // Pointer to Pointer casts (void* ↔ wild T->)
    // In LLVM opaque pointer model (20.x+), all pointers are compatible "ptr" type
    // Just return the source value - no bitcast needed with opaque pointers
    if (sourceLLVMType->isPointerTy() && targetLLVMType->isPointerTy()) {
        return sourceValue;  // No-op: ptr to ptr is identity in modern LLVM
    }
    
    throw std::runtime_error("Unsupported cast between types: " + 
                           std::string(sourceLLVMType->isIntegerTy() ? "int" : "float") +
                           " -> " + expr->targetType);
}

/**
 * Generate code for object literal expressions
 * Handles: {val1, val2, ...} syntax for struct-like initialization
 * Used for vec9, frac types, and other exotic composite types
 */
llvm::Value* ExprCodegen::codegenObjectLiteral(ObjectLiteralExpr* expr) {
    if (!expr) {
        throw std::runtime_error("Null object literal expression");
    }
    
    std::cerr << "[DEBUG] codegenObjectLiteral: fields=" << expr->fields.size() 
              << ", type_name=" << expr->type_name << std::endl;
    
    // For vec9 and other array-like types: create an array/struct with field values
    if (expr->fields.size() == 9) {
        // vec9: [9 x i64]
        llvm::ArrayType* arrayType = llvm::ArrayType::get(builder.getInt64Ty(), 9);
        llvm::Value* arrayAlloca = builder.CreateAlloca(arrayType, nullptr, "vec9.alloca");
        
        // Initialize each element
        for (size_t i = 0; i < expr->fields.size(); i++) {
            llvm::Value* elemValue = codegenExpressionNode(expr->fields[i].value.get(), this);
            if (!elemValue) {
                throw std::runtime_error("Failed to generate code for vec9 element " + std::to_string(i));
            }
            
            // Convert to i64 if needed
            if (elemValue->getType() != builder.getInt64Ty()) {
                elemValue = builder.CreateSExtOrTrunc(elemValue, builder.getInt64Ty(), "elem.ext");
            }
            
            // Get pointer to array element
            std::vector<llvm::Value*> indices = {
                llvm::ConstantInt::get(builder.getInt32Ty(), 0),
                llvm::ConstantInt::get(builder.getInt32Ty(), i)
            };
            llvm::Value* elemPtr = builder.CreateGEP(arrayType, arrayAlloca, indices, "elem.ptr");
            builder.CreateStore(elemValue, elemPtr);
        }
        
        // Load the complete array value
        return builder.CreateLoad(arrayType, arrayAlloca, "vec9.value");
    }
    
    // For frac types (3 fields): {whole, num, denom}
    if (expr->fields.size() == 3) {
        // Create struct with 3 i64 fields
        llvm::StructType* structType = llvm::StructType::get(
            context,
            {builder.getInt64Ty(), builder.getInt64Ty(), builder.getInt64Ty()},
            false
        );
        
        llvm::Value* structAlloca = builder.CreateAlloca(structType, nullptr, "frac.alloca");
        
        // Initialize each field
        for (size_t i = 0; i < 3; i++) {
            llvm::Value* fieldValue = codegenExpressionNode(expr->fields[i].value.get(), this);
            if (!fieldValue) {
                throw std::runtime_error("Failed to generate code for frac field " + std::to_string(i));
            }
            
            // Convert to i64 if needed
            if (fieldValue->getType() != builder.getInt64Ty()) {
                fieldValue = builder.CreateSExtOrTrunc(fieldValue, builder.getInt64Ty(), "field.ext");
            }
            
            llvm::Value* fieldPtr = builder.CreateStructGEP(structType, structAlloca, i, "field.ptr");
            builder.CreateStore(fieldValue, fieldPtr);
        }
        
        // Load the complete struct value
        return builder.CreateLoad(structType, structAlloca, "frac.value");
    }
    
    // For tfp types (2 fields): {exponent, mantissa}
    if (expr->fields.size() == 2) {
        // Create struct with 2 i64 fields
        llvm::StructType* structType = llvm::StructType::get(
            context,
            {builder.getInt64Ty(), builder.getInt64Ty()},
            false
        );
        
        llvm::Value* structAlloca = builder.CreateAlloca(structType, nullptr, "tfp.alloca");
        
        // Initialize each field
        for (size_t i = 0; i < 2; i++) {
            llvm::Value* fieldValue = codegenExpressionNode(expr->fields[i].value.get(), this);
            if (!fieldValue) {
                throw std::runtime_error("Failed to generate code for tfp field " + std::to_string(i));
            }
            
            // Convert to i64 if needed
            if (fieldValue->getType() != builder.getInt64Ty()) {
                fieldValue = builder.CreateSExtOrTrunc(fieldValue, builder.getInt64Ty(), "field.ext");
            }
            
            llvm::Value* fieldPtr = builder.CreateStructGEP(structType, structAlloca, i, "field.ptr");
            builder.CreateStore(fieldValue, fieldPtr);
        }
        
        // Load the complete struct value
        return builder.CreateLoad(structType, structAlloca, "tfp.value");
    }
    
    throw std::runtime_error("Unsupported object literal field count: " + 
                           std::to_string(expr->fields.size()));
}

// ============================================================================
// GPU/PTX Backend - Phase 4: GPU Intrinsics
// ============================================================================

/**
 * Generate code for GPU intrinsics
 * 
 * Maps Aria gpu.* calls to LLVM NVVM intrinsics for CUDA code generation.
 * These functions can only be used inside GPU kernels (marked with #[gpu_kernel]).
 * 
 * Supported intrinsics:
 * - gpu.thread_id() -> llvm.nvvm.read.ptx.sreg.tid.x (thread index in block)
 * - gpu.thread_id_y() -> llvm.nvvm.read.ptx.sreg.tid.y
 * - gpu.thread_id_z() -> llvm.nvvm.read.ptx.sreg.tid.z
 * - gpu.block_id() -> llvm.nvvm.read.ptx.sreg.ctaid.x (block index in grid)
 * - gpu.block_id_y() -> llvm.nvvm.read.ptx.sreg.ctaid.y
 * - gpu.block_id_z() -> llvm.nvvm.read.ptx.sreg.ctaid.z
 * - gpu.block_dim() -> llvm.nvvm.read.ptx.sreg.ntid.x (threads per block)
 * - gpu.block_dim_y() -> llvm.nvvm.read.ptx.sreg.ntid.y
 * - gpu.block_dim_z() -> llvm.nvvm.read.ptx.sreg.ntid.z
 * - gpu.grid_dim() -> llvm.nvvm.read.ptx.sreg.nctaid.x (blocks in grid)
 * - gpu.grid_dim_y() -> llvm.nvvm.read.ptx.sreg.nctaid.y
 * - gpu.grid_dim_z() -> llvm.nvvm.read.ptx.sreg.nctaid.z
 * - gpu.sync_threads() -> llvm.nvvm.barrier0 (block-level synchronization)
 * 
 * Example Aria code:
 * ```aria
 * #[gpu_kernel]
 * func:vector_add = void(int32:n) {
 *     int32:tid = gpu.thread_id();
 *     int32:bid = gpu.block_id();
 *     int32:idx = bid * gpu.block_dim() + tid;
 *     
 *     if (idx < n) {
 *         // Process element idx
 *     }
 *     
 *     gpu.sync_threads();  // Wait for all threads
 * }
 * ```
 * 
 * Generated LLVM IR:
 * ```llvm
 * %tid = call i32 @llvm.nvvm.read.ptx.sreg.tid.x()
 * %bid = call i32 @llvm.nvvm.read.ptx.sreg.ctaid.x()
 * call void @llvm.nvvm.barrier0()
 * ```
 */
llvm::Value* ExprCodegen::codegenGPUIntrinsic(const std::string& intrinsic_name,
                                               CallExpr* call_expr) {
    std::cerr << "[GPU] Generating intrinsic: gpu." << intrinsic_name << std::endl;
    
    // Helper lambda to get or declare NVVM intrinsic
    auto getOrDeclareNVVMIntrinsic = [this](const std::string& name, llvm::Type* returnType) -> llvm::Function* {
        if (llvm::Function* existing = module->getFunction(name)) {
            return existing;
        }
        llvm::FunctionType* funcType = llvm::FunctionType::get(returnType, {}, false);
        return llvm::Function::Create(funcType, llvm::Function::ExternalLinkage, name, module);
    };
    
    llvm::Type* i32Type = llvm::Type::getInt32Ty(context);
    llvm::Type* voidType = llvm::Type::getVoidTy(context);
    
    // Thread indexing intrinsics (X dimension - most common)
    if (intrinsic_name == "thread_id") {
        if (!call_expr->arguments.empty()) {
            throw std::runtime_error("gpu.thread_id() takes no arguments");
        }
        llvm::Function* intrinsic = getOrDeclareNVVMIntrinsic("llvm.nvvm.read.ptx.sreg.tid.x", i32Type);
        return builder.CreateCall(intrinsic, {}, "tid.x");
    }
    
    if (intrinsic_name == "thread_id_y") {
        if (!call_expr->arguments.empty()) {
            throw std::runtime_error("gpu.thread_id_y() takes no arguments");
        }
        llvm::Function* intrinsic = getOrDeclareNVVMIntrinsic("llvm.nvvm.read.ptx.sreg.tid.y", i32Type);
        return builder.CreateCall(intrinsic, {}, "tid.y");
    }
    
    if (intrinsic_name == "thread_id_z") {
        if (!call_expr->arguments.empty()) {
            throw std::runtime_error("gpu.thread_id_z() takes no arguments");
        }
        llvm::Function* intrinsic = getOrDeclareNVVMIntrinsic("llvm.nvvm.read.ptx.sreg.tid.z", i32Type);
        return builder.CreateCall(intrinsic, {}, "tid.z");
    }
    
    // Block indexing intrinsics
    if (intrinsic_name == "block_id") {
        if (!call_expr->arguments.empty()) {
            throw std::runtime_error("gpu.block_id() takes no arguments");
        }
        llvm::Function* intrinsic = getOrDeclareNVVMIntrinsic("llvm.nvvm.read.ptx.sreg.ctaid.x", i32Type);
        return builder.CreateCall(intrinsic, {}, "bid.x");
    }
    
    if (intrinsic_name == "block_id_y") {
        if (!call_expr->arguments.empty()) {
            throw std::runtime_error("gpu.block_id_y() takes no arguments");
        }
        llvm::Function* intrinsic = getOrDeclareNVVMIntrinsic("llvm.nvvm.read.ptx.sreg.ctaid.y", i32Type);
        return builder.CreateCall(intrinsic, {}, "bid.y");
    }
    
    if (intrinsic_name == "block_id_z") {
        if (!call_expr->arguments.empty()) {
            throw std::runtime_error("gpu.block_id_z() takes no arguments");
        }
        llvm::Function* intrinsic = getOrDeclareNVVMIntrinsic("llvm.nvvm.read.ptx.sreg.ctaid.z", i32Type);
        return builder.CreateCall(intrinsic, {}, "bid.z");
    }
    
    // Block dimension intrinsics
    if (intrinsic_name == "block_dim") {
        if (!call_expr->arguments.empty()) {
            throw std::runtime_error("gpu.block_dim() takes no arguments");
        }
        llvm::Function* intrinsic = getOrDeclareNVVMIntrinsic("llvm.nvvm.read.ptx.sreg.ntid.x", i32Type);
        return builder.CreateCall(intrinsic, {}, "bdim.x");
    }
    
    if (intrinsic_name == "block_dim_y") {
        if (!call_expr->arguments.empty()) {
            throw std::runtime_error("gpu.block_dim_y() takes no arguments");
        }
        llvm::Function* intrinsic = getOrDeclareNVVMIntrinsic("llvm.nvvm.read.ptx.sreg.ntid.y", i32Type);
        return builder.CreateCall(intrinsic, {}, "bdim.y");
    }
    
    if (intrinsic_name == "block_dim_z") {
        if (!call_expr->arguments.empty()) {
            throw std::runtime_error("gpu.block_dim_z() takes no arguments");
        }
        llvm::Function* intrinsic = getOrDeclareNVVMIntrinsic("llvm.nvvm.read.ptx.sreg.ntid.z", i32Type);
        return builder.CreateCall(intrinsic, {}, "bdim.z");
    }
    
    // Grid dimension intrinsics
    if (intrinsic_name == "grid_dim") {
        if (!call_expr->arguments.empty()) {
            throw std::runtime_error("gpu.grid_dim() takes no arguments");
        }
        llvm::Function* intrinsic = getOrDeclareNVVMIntrinsic("llvm.nvvm.read.ptx.sreg.nctaid.x", i32Type);
        return builder.CreateCall(intrinsic, {}, "gdim.x");
    }
    
    if (intrinsic_name == "grid_dim_y") {
        if (!call_expr->arguments.empty()) {
            throw std::runtime_error("gpu.grid_dim_y() takes no arguments");
        }
        llvm::Function* intrinsic = getOrDeclareNVVMIntrinsic("llvm.nvvm.read.ptx.sreg.nctaid.y", i32Type);
        return builder.CreateCall(intrinsic, {}, "gdim.y");
    }
    
    if (intrinsic_name == "grid_dim_z") {
        if (!call_expr->arguments.empty()) {
            throw std::runtime_error("gpu.grid_dim_z() takes no arguments");
        }
        llvm::Function* intrinsic = getOrDeclareNVVMIntrinsic("llvm.nvvm.read.ptx.sreg.nctaid.z", i32Type);
        return builder.CreateCall(intrinsic, {}, "gdim.z");
    }
    
    // Synchronization intrinsics
    if (intrinsic_name == "sync_threads") {
        if (!call_expr->arguments.empty()) {
            throw std::runtime_error("gpu.sync_threads() takes no arguments");
        }
        llvm::Function* intrinsic = getOrDeclareNVVMIntrinsic("llvm.nvvm.barrier0", voidType);
        builder.CreateCall(intrinsic);
        // Barrier intrinsic returns void, return nullptr for void expressions
        return llvm::ConstantInt::get(builder.getInt32Ty(), 0);
    }
    
    // Unknown GPU intrinsic
    throw std::runtime_error("Unknown GPU intrinsic: gpu." + intrinsic_name +
                           "\nSupported: thread_id, thread_id_y, thread_id_z, " +
                           "block_id, block_id_y, block_id_z, " +
                           "block_dim, block_dim_y, block_dim_z, " +
                           "grid_dim, grid_dim_y, grid_dim_z, " +
                           "sync_threads");
}


