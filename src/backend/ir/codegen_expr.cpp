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
#include <llvm/IR/InlineAsm.h>
#include <llvm/IR/Intrinsics.h>  // Phase 4.5.3: Coroutine intrinsics for await
#include <llvm/Support/raw_ostream.h>
#include <stdexcept>
#include <iostream>
#include <cmath>  // For ldexp in fix256_from_float
#include "debug_log.h"

using namespace npk;
using namespace npk::frontend;
using namespace npk::backend;
using namespace npk::sema;

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
        llvm::StructType* strType = llvm::StructType::getTypeByName(context, "struct.NpkString");
        if (!strType) {
            std::vector<llvm::Type*> fields = {
                llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0),
                llvm::Type::getInt64Ty(context)
            };
            strType = llvm::StructType::create(context, fields, "struct.NpkString");
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
    
    // Pointer types with @ suffix (e.g., "int8@", "string@", "int64@")
    // Wild/native Aria pointers — map to LLVM opaque pointer
    if (!typeName.empty() && typeName.back() == '@') {
        return llvm::PointerType::get(context, 0);
    }

    // Unknown type - throw error instead of defaulting
    throw std::runtime_error("Unknown Nitpick type: " + typeName);
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

// Helper: Resolve the Aria type name of a struct field from a MEMBER_ACCESS expression.
// Returns empty string if the field type cannot be determined.
std::string ExprCodegen::getMemberAccessFieldTypeName(ASTNode* expr) {
    if (!expr || expr->type != ASTNode::NodeType::MEMBER_ACCESS) return "";
    MemberAccessExpr* memberExpr = static_cast<MemberAccessExpr*>(expr);
    if (memberExpr->object->type != ASTNode::NodeType::IDENTIFIER) return "";
    IdentifierExpr* objIdent = static_cast<IdentifierExpr*>(memberExpr->object.get());
    auto it = var_aria_types.find(objIdent->name);
    if (it == var_aria_types.end()) return "";
    if (!type_system) return "";
    auto* structTy = type_system->getStructType(it->second);
    if (!structTy) return "";
    auto* field = structTy->getField(memberExpr->member);
    if (!field || !field->type) return "";
    return field->type->toString();
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

    // For member access (e.g., a.x), resolve the field's type
    if (expr->type == ASTNode::NodeType::MEMBER_ACCESS) {
        std::string fieldType = getMemberAccessFieldTypeName(expr);
        if (isTBBTypeName(fieldType)) return fieldType;
        return "";
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
        ARIA_DBG_STREAM << "[EXOTIC_CHECK] expr is null" << std::endl;
        return "";
    }

    ARIA_DBG_STREAM << "[EXOTIC_CHECK] expr type = " << static_cast<int>(expr->type) << std::endl;

    // For identifiers, look up in var_aria_types
    if (expr->type == ASTNode::NodeType::IDENTIFIER) {
        IdentifierExpr* ident = static_cast<IdentifierExpr*>(expr);
        ARIA_DBG_STREAM << "[EXOTIC_CHECK] identifier name = " << ident->name << std::endl;
        auto it = var_aria_types.find(ident->name);
        if (it != var_aria_types.end()) {
            const std::string& typeName = it->second;
            ARIA_DBG_STREAM << "[DEBUG] Checking identifier '" << ident->name << "' type: " << typeName << std::endl;
            if (isExoticTypeName(typeName)) {
                ARIA_DBG_STREAM << "[DEBUG] Detected exotic type: " << typeName << std::endl;
                return typeName;
            }
        } else {
            ARIA_DBG_STREAM << "[EXOTIC_CHECK] identifier '" << ident->name << "' NOT in var_aria_types!" << std::endl;
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

    // For member access (e.g., a.x), resolve the field's type
    if (expr->type == ASTNode::NodeType::MEMBER_ACCESS) {
        std::string fieldType = getMemberAccessFieldTypeName(expr);
        if (isExoticTypeName(fieldType)) return fieldType;
        return "";
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

    // For member access (e.g., a.x), resolve the field's type
    if (expr->type == ASTNode::NodeType::MEMBER_ACCESS) {
        std::string fieldType = getMemberAccessFieldTypeName(expr);
        if (fieldType.find("frac") == 0 || fieldType.find("tfp") == 0 ||
            fieldType == "vec9" || fieldType == "dvec9") {
            return fieldType;
        }
        return "";
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
                typeName == "fix256" ||
                typeName == "flt256" || typeName == "flt512") {
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

    // For member access (e.g., a.x), resolve the field's type
    if (expr->type == ASTNode::NodeType::MEMBER_ACCESS) {
        std::string fieldType = getMemberAccessFieldTypeName(expr);
        if (fieldType == "int128" || fieldType == "uint128" ||
            fieldType == "int256" || fieldType == "uint256" ||
            fieldType == "int512" || fieldType == "uint512" ||
            fieldType == "int1024" || fieldType == "uint1024" ||
            fieldType == "fix256" ||
            fieldType == "flt256" || fieldType == "flt512") {
            return fieldType;
        }
        return "";
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
    (void)tbbType;
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
        llvm::Function* saddFunc = llvm::Intrinsic::getOrInsertDeclaration(
            module,
            llvm::Intrinsic::sadd_with_overflow,
            {type}
        );
        llvm::Value* resStruct = builder.CreateCall(saddFunc, {left, right}, "sadd_ovf");
        mathResult = builder.CreateExtractValue(resStruct, 0, "sum");
        overflow = builder.CreateExtractValue(resStruct, 1, "ovf");
    } else if (op == frontend::TokenType::TOKEN_MINUS) {
        // Use llvm.ssub.with.overflow for subtraction
        llvm::Function* ssubFunc = llvm::Intrinsic::getOrInsertDeclaration(
            module,
            llvm::Intrinsic::ssub_with_overflow,
            {type}
        );
        llvm::Value* resStruct = builder.CreateCall(ssubFunc, {left, right}, "ssub_ovf");
        mathResult = builder.CreateExtractValue(resStruct, 0, "diff");
        overflow = builder.CreateExtractValue(resStruct, 1, "ovf");
    } else if (op == frontend::TokenType::TOKEN_STAR) {
        // Use llvm.smul.with.overflow for multiplication
        llvm::Function* smulFunc = llvm::Intrinsic::getOrInsertDeclaration(
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
    bool isComparison = false;
    if (op == frontend::TokenType::TOKEN_PLUS) {
        opSuffix = "_add";
    } else if (op == frontend::TokenType::TOKEN_MINUS) {
        opSuffix = "_sub";
    } else if (op == frontend::TokenType::TOKEN_STAR) {
        opSuffix = "_mul";
    } else if (op == frontend::TokenType::TOKEN_SLASH) {
        opSuffix = "_div";
    } else if (op == frontend::TokenType::TOKEN_EQUAL_EQUAL ||
               op == frontend::TokenType::TOKEN_BANG_EQUAL ||
               op == frontend::TokenType::TOKEN_LESS ||
               op == frontend::TokenType::TOKEN_LESS_EQUAL ||
               op == frontend::TokenType::TOKEN_GREATER ||
               op == frontend::TokenType::TOKEN_GREATER_EQUAL) {
        opSuffix = "_cmp";
        isComparison = true;
    } else {
        return nullptr;  // Unsupported operation
    }
    
    // For frac types: use sret/pointer ABI matching the C runtime
    // Runtime: void npk_frac32_add(Frac32* result, const Frac32* a, const Frac32* b)
    // Comparison: int32_t npk_frac32_cmp(const Frac32* a, const Frac32* b) → -1/0/1
    if (numericType.find("frac") == 0) {
        std::string runtimeFunc = "npk_" + numericType + opSuffix;
        llvm::StructType* fracType = llvm::cast<llvm::StructType>(left->getType());
        
        if (isComparison) {
            // Comparison: int32_t npk_fracN_cmp(const FracN* a, const FracN* b)
            llvm::Function* cmp_func = module->getFunction(runtimeFunc);
            if (!cmp_func) {
                llvm::Type* ptrType = llvm::PointerType::get(fracType, 0);
                llvm::FunctionType* funcType = llvm::FunctionType::get(
                    builder.getInt32Ty(),
                    {ptrType, ptrType},
                    false
                );
                cmp_func = llvm::Function::Create(funcType,
                    llvm::Function::ExternalLinkage, runtimeFunc, module);
            }
            
            llvm::Value* a_alloca = builder.CreateAlloca(fracType, nullptr, "frac_cmp_a");
            llvm::Value* b_alloca = builder.CreateAlloca(fracType, nullptr, "frac_cmp_b");
            builder.CreateStore(left, a_alloca);
            builder.CreateStore(right, b_alloca);
            
            llvm::Value* cmp_result = builder.CreateCall(cmp_func, {a_alloca, b_alloca}, "frac_cmp");
            
            // Convert three-way compare to boolean based on operator
            llvm::Value* zero = llvm::ConstantInt::get(builder.getInt32Ty(), 0);
            if (op == frontend::TokenType::TOKEN_EQUAL_EQUAL) {
                return builder.CreateICmpEQ(cmp_result, zero, "frac_eq");
            } else if (op == frontend::TokenType::TOKEN_BANG_EQUAL) {
                return builder.CreateICmpNE(cmp_result, zero, "frac_ne");
            } else if (op == frontend::TokenType::TOKEN_LESS) {
                return builder.CreateICmpSLT(cmp_result, zero, "frac_lt");
            } else if (op == frontend::TokenType::TOKEN_LESS_EQUAL) {
                return builder.CreateICmpSLE(cmp_result, zero, "frac_le");
            } else if (op == frontend::TokenType::TOKEN_GREATER) {
                return builder.CreateICmpSGT(cmp_result, zero, "frac_gt");
            } else if (op == frontend::TokenType::TOKEN_GREATER_EQUAL) {
                return builder.CreateICmpSGE(cmp_result, zero, "frac_ge");
            }
        }
        
        llvm::Function* c_func = module->getFunction(runtimeFunc);
        if (!c_func) {
            llvm::Type* ptrType = llvm::PointerType::get(fracType, 0);
            llvm::FunctionType* funcType = llvm::FunctionType::get(
                builder.getVoidTy(),
                {ptrType, ptrType, ptrType},
                false
            );
            c_func = llvm::Function::Create(funcType,
                llvm::Function::ExternalLinkage, runtimeFunc, module);
        }
        
        // Allocate temps and store arguments
        llvm::Value* result_alloca = builder.CreateAlloca(fracType, nullptr, "frac_result");
        llvm::Value* a_alloca = builder.CreateAlloca(fracType, nullptr, "frac_a");
        llvm::Value* b_alloca = builder.CreateAlloca(fracType, nullptr, "frac_b");
        builder.CreateStore(left, a_alloca);
        builder.CreateStore(right, b_alloca);
        
        builder.CreateCall(c_func, {result_alloca, a_alloca, b_alloca});
        return builder.CreateLoad(fracType, result_alloca, numericType + "_result");
    }
    
    // For other numeric types (tfp*, vec9): by-value calling convention
    std::string funcName = numericType + opSuffix;
    llvm::Type* returnType = left->getType();
    
    llvm::FunctionType* funcType = llvm::FunctionType::get(
        returnType,
        {returnType, returnType},
        false
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
               op == frontend::TokenType::TOKEN_GREATER_EQUAL ||
               op == frontend::TokenType::TOKEN_SPACESHIP) {
        opSuffix = "_cmp";  // Comparison returns -1/0/1
        isComparison = true;
    } else {
        return nullptr;  // Unsupported operation
    }
    
    // Build function name based on type
    std::string funcName;
    
    // fix256 uses npk_fix256_* functions (deterministic fixed-point)
    if (lbimType == "fix256") {
        funcName = "npk_fix256" + opSuffix;
    }
    // Signed LBIM integers use npk_lbim_s* functions (signed division, etc.)
    else if (lbimType.find("int") == 0) {  // int128, int256, int512, int1024
        // Extract bit width: int1024 → 1024
        std::string bitWidth = lbimType.substr(3);
        
        // Use signed functions for division/modulo, unsigned for add/sub/mul
        if (op == frontend::TokenType::TOKEN_SLASH) {
            funcName = "npk_lbim_sdiv" + bitWidth;
        } else if (op == frontend::TokenType::TOKEN_PERCENT) {
            funcName = "npk_lbim_smod" + bitWidth;
        } else if (opSuffix == "_cmp") {
            // Signed comparison
            funcName = "npk_lbim_scmp" + bitWidth;
        } else if (opSuffix == "_eq") {
            // Equality works same for signed/unsigned
            funcName = "npk_lbim_eq" + bitWidth;
        } else {
            funcName = "npk_lbim" + opSuffix + bitWidth;
        }
    }
    // Unsigned LBIM integers use npk_lbim_u* for div/mod
    else if (lbimType.find("uint") == 0) {  // uint128, uint256, uint512, uint1024
        std::string bitWidth = lbimType.substr(4);
        
        if (op == frontend::TokenType::TOKEN_SLASH) {
            funcName = "npk_lbim_udiv" + bitWidth;
        } else if (op == frontend::TokenType::TOKEN_PERCENT) {
            funcName = "npk_lbim_umod" + bitWidth;
        } else if (opSuffix == "_cmp") {
            // Unsigned comparison
            funcName = "npk_lbim_ucmp" + bitWidth;
        } else if (opSuffix == "_eq") {
            // Equality works same for signed/unsigned
            funcName = "npk_lbim_eq" + bitWidth;
        } else {
            funcName = "npk_lbim" + opSuffix + bitWidth;
        }
    }
    // LBIM float types: flt256/flt512 (software-emulated extended precision)
    else if (lbimType == "flt256" || lbimType == "flt512") {
        std::string bitWidth = lbimType.substr(3);  // "256" or "512"

        if (opSuffix == "_cmp") {
            funcName = "npk_lbim_fcmp" + bitWidth;
        } else if (opSuffix == "_eq") {
            funcName = "npk_lbim_feq" + bitWidth;
        } else {
            funcName = "npk_lbim_f" + opSuffix.substr(1) + bitWidth;  // e.g. npk_lbim_fadd256
        }
    }
    else {
        return nullptr;  // Unknown LBIM type
    }
    
    // Get the struct type for this LBIM type
    llvm::Type* structType = left->getType();
    
    // SysV x86-64 ABI: structs >16 bytes are passed in MEMORY (on stack via pointer).
    // Check struct size to determine if we need byval/sret ABI treatment.
    uint64_t structSize = module->getDataLayout().getTypeStoreSize(structType);
    bool needsByvalABI = (structSize > 16);
    
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
    
    llvm::Value* result;
    
    if (needsByvalABI) {
        // Large struct ABI: use byval for params, sret for struct returns
        llvm::Type* ptrType = llvm::PointerType::getUnqual(context);
        
        if (!isComparison) {
            // Arithmetic: void func(struct* sret, struct* byval, struct* byval)
            llvm::FunctionType* funcType = llvm::FunctionType::get(
                llvm::Type::getVoidTy(context),
                {ptrType, ptrType, ptrType},
                false
            );
            
            llvm::Function* runtimeFunc = module->getFunction(funcName);
            if (!runtimeFunc) {
                runtimeFunc = llvm::Function::Create(
                    funcType,
                    llvm::Function::ExternalLinkage,
                    funcName,
                    module
                );
                runtimeFunc->addParamAttr(0, llvm::Attribute::getWithStructRetType(context, structType));
                runtimeFunc->addParamAttr(1, llvm::Attribute::getWithByValType(context, structType));
                runtimeFunc->addParamAttr(2, llvm::Attribute::getWithByValType(context, structType));
            }
            
            llvm::Value* resultAlloca = builder.CreateAlloca(structType, nullptr, "lbim_sret");
            llvm::Value* leftAlloca = builder.CreateAlloca(structType, nullptr, "lbim_lhs");
            llvm::Value* rightAlloca = builder.CreateAlloca(structType, nullptr, "lbim_rhs");
            builder.CreateStore(left, leftAlloca);
            builder.CreateStore(right, rightAlloca);
            
            auto* call = builder.CreateCall(runtimeFunc, {resultAlloca, leftAlloca, rightAlloca});
            call->addParamAttr(0, llvm::Attribute::getWithStructRetType(context, structType));
            call->addParamAttr(1, llvm::Attribute::getWithByValType(context, structType));
            call->addParamAttr(2, llvm::Attribute::getWithByValType(context, structType));
            
            result = builder.CreateLoad(structType, resultAlloca, lbimType + "_result");
        } else {
            // Comparison: returnType func(struct* byval, struct* byval)
            llvm::FunctionType* funcType = llvm::FunctionType::get(
                returnType,
                {ptrType, ptrType},
                false
            );
            
            llvm::Function* runtimeFunc = module->getFunction(funcName);
            if (!runtimeFunc) {
                runtimeFunc = llvm::Function::Create(
                    funcType,
                    llvm::Function::ExternalLinkage,
                    funcName,
                    module
                );
                runtimeFunc->addParamAttr(0, llvm::Attribute::getWithByValType(context, structType));
                runtimeFunc->addParamAttr(1, llvm::Attribute::getWithByValType(context, structType));
            }
            
            llvm::Value* leftAlloca = builder.CreateAlloca(structType, nullptr, "lbim_lhs");
            llvm::Value* rightAlloca = builder.CreateAlloca(structType, nullptr, "lbim_rhs");
            builder.CreateStore(left, leftAlloca);
            builder.CreateStore(right, rightAlloca);
            
            auto* call = builder.CreateCall(runtimeFunc, {leftAlloca, rightAlloca}, lbimType + "_result");
            call->addParamAttr(0, llvm::Attribute::getWithByValType(context, structType));
            call->addParamAttr(1, llvm::Attribute::getWithByValType(context, structType));
            
            result = call;
        }
    } else {
        // Small struct ABI (<=16 bytes): pass by value directly (e.g. int128)
        llvm::FunctionType* funcType = llvm::FunctionType::get(
            returnType,
            {structType, structType},
            false
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
        
        result = builder.CreateCall(runtimeFunc, {left, right}, lbimType + "_result");
    }
    
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
        } else if (op == frontend::TokenType::TOKEN_SPACESHIP) {
            // <=> : return raw cmp_result as i64 (-1/0/1)
            result = builder.CreateSExt(result, llvm::Type::getInt64Ty(context), "spaceship_result");
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
    
    ARIA_DBG_STREAM << "[LBIM_PROMOTE] Promoting literal to " << structName << " (" << numLimbs << " limbs)" << std::endl;

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
    
    ARIA_DBG_STREAM << "[DEBUG] codegenLiteral called, hasRawValue=" << expr->hasRawValue() << std::endl;
    ARIA_DBG_STREAM << "[DEBUG] codegenLiteral variant index: " << expr->value.index() << std::endl;
    if (std::holds_alternative<std::string>(expr->value)) {
        ARIA_DBG_STREAM << "[DEBUG] codegenLiteral string value: " << std::get<std::string>(expr->value) << std::endl;
    }
    
    // Phase 4: If literal has explicit float type suffix (f32, f64), use it directly.
    // This must come BEFORE the hasRawValue() check which always produces double.
    if (!expr->explicit_type.empty()) {
        llvm::Type* llvm_type = getLLVMTypeFromString(expr->explicit_type);
        if (llvm_type && llvm_type->isFloatingPointTy() && std::holds_alternative<double>(expr->value)) {
            double val = std::get<double>(expr->value);
            return llvm::ConstantFP::get(llvm_type, val);
        }
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
            npk::semantic::FloatPrecision precision;
            if (raw.length() > 17) {  // More digits than float64 can represent
                precision = npk::semantic::FloatPrecision::FLT128;
            } else {
                precision = npk::semantic::FloatPrecision::FLT64;
            }
            
            // Convert using LiteralConverter
            auto apfloat_opt = npk::semantic::LiteralConverter::convertFloatLiteral(raw, precision);
            if (apfloat_opt) {
                llvm::Value* result = llvm::ConstantFP::get(context, *apfloat_opt);
                ARIA_DBG_STREAM << "[DEBUG] codegenLiteral: high-precision float '" << raw << "' -> type: ";
#ifdef ARIA_DEBUG_CODEGEN
                result->getType()->print(llvm::errs());
                std::cerr << std::endl;
#endif
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
            auto apint_opt = npk::semantic::LiteralConverter::convertIntLiteral(raw, bit_width, is_signed);
            if (apint_opt) {
                llvm::Value* result = llvm::ConstantInt::get(context, *apint_opt);
                ARIA_DBG_STREAM << "[DEBUG] codegenLiteral: high-precision int '" << raw << "' -> type: ";
#ifdef ARIA_DEBUG_CODEGEN
                result->getType()->print(llvm::errs());
                std::cerr << std::endl;
#endif
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
        ARIA_DBG_STREAM << "[DEBUG] codegenLiteral: float " << val << " -> type: ";
#ifdef ARIA_DEBUG_CODEGEN
        result->getType()->print(llvm::errs());
        std::cerr << std::endl;
#endif
        return result;
    }
    
    // String literal - create global AriaString struct
    if (std::holds_alternative<std::string>(expr->value)) {
        const std::string& str = std::get<std::string>(expr->value);
        ARIA_DBG_STREAM << "[DEBUG] String literal detected: \"" << str << "\"" << std::endl;
        
        // Check for special literals: ERR and unknown KEYWORDS (not string literals)
        // Only treat as sentinels if explicit_type is set (keyword form, not "quoted string")
        if (str == "ERR" && expr->explicit_type == "ERR") {
            // ERR sentinel - need to know target type from context
            // For now, default to int32 (will be handled better with type inference)
            llvm::Type* target_type = llvm::Type::getInt32Ty(context);
            ARIA_DBG_STREAM << "[DEBUG] ERR literal - generating sentinel for type: ";
#ifdef ARIA_DEBUG_CODEGEN
            target_type->print(llvm::errs());
            std::cerr << std::endl;
#endif
            return getTBBSentinel(target_type);
        }
        
        if (str == "unknown" && expr->explicit_type == "UNKNOWN") {
            // unknown sentinel - similar to ERR but uses max value instead of min
            llvm::Type* target_type = llvm::Type::getInt32Ty(context);
            ARIA_DBG_STREAM << "[DEBUG] unknown literal - generating sentinel for type: ";
#ifdef ARIA_DEBUG_CODEGEN
            target_type->print(llvm::errs());
            std::cerr << std::endl;
#endif
            llvm::Value* sentinel = getUnknownSentinel(target_type);
            ARIA_DBG_STREAM << "[DEBUG] unknown sentinel generated, result type: ";
#ifdef ARIA_DEBUG_CODEGEN
            sentinel->getType()->print(llvm::errs());
            std::cerr << std::endl;
#endif
            return sentinel;
        }
        
        // ARIA-026: String interning - check pool before creating new global
        // Gemini Safety Audit Fix #5: Prevents OOM from duplicate literals
        llvm::GlobalVariable* str_gv = nullptr;
        auto pool_it = string_pool_.find(str);
        if (pool_it != string_pool_.end()) {
            // Reuse existing string global
            str_gv = pool_it->second;
            ARIA_DBG_STREAM << "[STRING POOL] Reusing pooled string: \"" << str << "\"" << std::endl;
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
            ARIA_DBG_STREAM << "[STRING POOL] Created new pooled string: \"" << str << "\"" << std::endl;
        }
        
        // Get or create AriaString struct type
        llvm::StructType* npk_string_type = llvm::StructType::getTypeByName(context, "struct.NpkString");
        if (!npk_string_type) {
            std::vector<llvm::Type*> fields = {
                llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0),
                llvm::Type::getInt64Ty(context)
            };
            npk_string_type = llvm::StructType::create(context, fields, "struct.NpkString");
        }
        
        // Create a global AriaString struct constant
        std::vector<llvm::Constant*> struct_values = {
            llvm::ConstantExpr::getPointerCast(str_gv, llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0)),
            builder.getInt64(str.length())
        };
        llvm::Constant* string_struct = llvm::ConstantStruct::get(npk_string_type, struct_values);
        
        llvm::GlobalVariable* string_gv = new llvm::GlobalVariable(
            *module,
            npk_string_type,
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

    // v0.18.0: $$m function parameters are bound to the caller's storage as
    // pointer arguments. Reading the parameter should load the pointed-to value;
    // writing is handled by the assignment path in IRGenerator.
    if (llvm::isa<llvm::Argument>(var_ptr) && var_ptr->getType()->isPointerTy() &&
        var_aria_types.count("__borrow_param_mut:" + expr->name)) {
        llvm::Type* loadType = llvm::Type::getInt32Ty(context);
        auto typeIt = var_aria_types.find(expr->name);
        if (typeIt != var_aria_types.end()) {
            llvm::Type* mapped = getLLVMTypeFromString(typeIt->second);
            if (mapped) loadType = mapped;
        }
        return builder.CreateLoad(loadType, var_ptr, expr->name);
    }
    
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
        ARIA_DBG_STREAM << "[DEBUG] codegenIdentifier: " << expr->name << " -> type: ";
#ifdef ARIA_DEBUG_CODEGEN
        loaded->getType()->print(llvm::errs());
        std::cerr << std::endl;
#endif
        return loaded;
    }

    // v0.18.0: local mutable borrow aliases over struct fields are stored as
    // GEP pointers into the borrowed aggregate, not as allocas. Expression
    // helpers (notably exit(...)) still need identifier reads to load through
    // those pointers instead of treating the pointer itself as the value.
    if (var_ptr->getType()->isPointerTy() && !llvm::isa<llvm::Argument>(var_ptr)) {
        auto typeIt = var_aria_types.find(expr->name);
        if (typeIt != var_aria_types.end()) {
            llvm::Type* loadType = getLLVMTypeFromString(typeIt->second);
            if (loadType) {
                return builder.CreateLoad(loadType, var_ptr, expr->name);
            }
        }
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
 * 4. Concatenate all parts using npk_string_concat runtime function
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
        llvm::Value* ariaStr = builder.CreateAlloca(ariaStringType, nullptr, "npk_str");
        
        // Set data field
        llvm::Value* dataPtr = builder.CreateStructGEP(ariaStringType, ariaStr, 0, "data_ptr");
        builder.CreateStore(strPtr, dataPtr);
        
        // Set length field  
        llvm::Value* lengthPtr = builder.CreateStructGEP(ariaStringType, ariaStr, 1, "length_ptr");
        builder.CreateStore(llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), str.length()), lengthPtr);
        
        // Load and return the struct value
        return builder.CreateLoad(ariaStringType, ariaStr, "npk_str_val");
    };
    
    // ARIA-026: SAFETY FIX - Use deterministic npk_int64_to_str instead of sprintf
    // Gemini Safety Audit Fix #3: Non-Deterministic Serialization
    // Risk: sprintf is locale-dependent, violates bit-identical requirement for AGI logs
    // Helper function: Convert int64 to string using deterministic runtime
    auto int64ToString = [this](llvm::Value* intVal) -> llvm::Value* {
        // ARIA-026 FIX: Use GC heap instead of stack to prevent use-after-return
        // Stack buffers are deallocated on function return, creating dangling pointers
        // if these helpers are ever used in return statements or stored in variables
        
        // Allocate 24-byte buffer on GC heap (sufficient for "-9223372036854775808\0")
        llvm::FunctionCallee gcAllocCallee = module->getOrInsertFunction("npk_gc_alloc",
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
        llvm::Function* panicOOM = module->getFunction("npk_panic_oom");
        if (!panicOOM) {
            llvm::FunctionType* panicType = llvm::FunctionType::get(
                llvm::Type::getVoidTy(context),
                { llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0) },
                false
            );
            panicOOM = llvm::Function::Create(
                panicType,
                llvm::Function::ExternalLinkage,
                "npk_panic_oom",
                module
            );
        }
        llvm::Value* panicMsg = builder.CreateGlobalString(
            "Out of memory in int64 to string conversion",
            "int_oom_msg"
        );
        builder.CreateCall(panicOOM, { panicMsg });
        builder.CreateUnreachable();
        
        // Valid path: continue
        builder.SetInsertPoint(validBB);
        
        // Declare npk_int64_to_str (deterministic, locale-independent runtime function)
        // Signature: int64_t npk_int64_to_str(int64_t value, char* buffer)
        // Returns: length of resulting string (excluding null terminator)
        llvm::Function* toStrFn = module->getFunction("npk_int64_to_str");
        if (!toStrFn) {
            llvm::FunctionType* fnType = llvm::FunctionType::get(
                llvm::Type::getInt64Ty(context),  // Returns length
                { llvm::Type::getInt64Ty(context), llvm::PointerType::get(context, 0) },
                false  // Not varargs (fixed signature for safety)
            );
            toStrFn = llvm::Function::Create(
                fnType,
                llvm::Function::ExternalLinkage,
                "npk_int64_to_str",
                module
            );
        }
        
        // Call npk_int64_to_str(intVal, buffer)
        // Returns length directly - no need for strlen()
        // Widen to i64 if needed (e.g., int32 values)
        if (intVal->getType() != llvm::Type::getInt64Ty(context)) {
            intVal = builder.CreateSExt(intVal, llvm::Type::getInt64Ty(context), "int_to_i64");
        }
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
        llvm::FunctionCallee gcAllocCallee = module->getOrInsertFunction("npk_gc_alloc",
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
        llvm::Function* panicOOM = module->getFunction("npk_panic_oom");
        if (!panicOOM) {
            llvm::FunctionType* panicType = llvm::FunctionType::get(
                llvm::Type::getVoidTy(context),
                { llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0) },
                false
            );
            panicOOM = llvm::Function::Create(
                panicType,
                llvm::Function::ExternalLinkage,
                "npk_panic_oom",
                module
            );
        }
        llvm::Value* panicMsg = builder.CreateGlobalString(
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
        
        // Declare npk_snprintf_c_locale (our deterministic wrapper)
        // This is a runtime function that forces C locale for formatting
        llvm::Function* snprintfFn = module->getFunction("npk_snprintf_c_locale");
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
                "npk_snprintf_c_locale",
                module
            );
        }
        
        // Convert float to double if needed (snprintf expects double for %g)
        llvm::Value* doubleVal = floatVal;
        if (floatVal->getType()->isFloatTy()) {
            doubleVal = builder.CreateFPExt(floatVal, llvm::Type::getDoubleTy(context), "to_double");
        }
        
        // Call npk_snprintf_c_locale(buffer, 64, "%.17g", doubleVal)
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
    
    // OPTIMIZED: Use npk_string_concat_n for O(n) instead of O(n²) concatenation
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
        llvm::FunctionCallee gcAllocCallee = module->getOrInsertFunction("npk_gc_alloc",
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
        // ARIA-026 FIX: Must use npk_gc_alloc instead of npk_alloc so GC can see string refs
        // during construction. Wild memory would be invisible to GC -> mid-construction corruption.
        
        // Calculate size: totalSegments * sizeof(AriaString)
        // AriaString = {ptr, i64} = 16 bytes (on 64-bit)
        llvm::Value* arraySize = llvm::ConstantInt::get(
            llvm::Type::getInt64Ty(context),
            totalSegments * 16  // sizeof(AriaString)
        );
        
        // Call npk_gc_alloc (GC-visible memory) for temporary buffer
        llvm::FunctionCallee npk_gc_alloc_callee = module->getOrInsertFunction("npk_gc_alloc",
            llvm::FunctionType::get(
                llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0),
                {llvm::Type::getInt64Ty(context)},
                false
            )
        );
        llvm::Function* npk_gc_alloc = llvm::cast<llvm::Function>(npk_gc_alloc_callee.getCallee());
        llvm::Value* heapMem = builder.CreateCall(npk_gc_alloc, { arraySize }, "heap_strings_gc");
        
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
        llvm::Function* npk_panic_oom = module->getFunction("npk_panic_oom");
        if (!npk_panic_oom) {
            llvm::FunctionType* panicType = llvm::FunctionType::get(
                llvm::Type::getVoidTy(context),
                { llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0) },
                false
            );
            npk_panic_oom = llvm::Function::Create(
                panicType,
                llvm::Function::ExternalLinkage,
                "npk_panic_oom",
                module
            );
        }
        llvm::Value* panicMsg = builder.CreateGlobalString(
            "Out of memory allocating template literal array",
            "gc_oom_msg"
        );
        builder.CreateCall(npk_panic_oom, { panicMsg });
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
                std::string formatterName = "npk_format_" + ariaType;

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
            } else if (ariaType == "string" && valType->isPointerTy()) {
                // String variable: already an AriaString*, just load the struct
                interpStr = builder.CreateLoad(ariaStringType, interpVal, "str_interp");
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

    // Declare npk_string_concat_n_simple runtime function
    // AriaString* npk_string_concat_n_simple(AriaString* strings, int64_t count)
    llvm::FunctionType* concatNType = llvm::FunctionType::get(
        llvm::PointerType::get(context, 0),  // Returns AriaString*
        { llvm::PointerType::get(context, 0), llvm::Type::getInt64Ty(context) },
        false
    );

    llvm::Function* concatNFn = module->getFunction("npk_string_concat_n_simple");
    if (!concatNFn) {
        concatNFn = llvm::Function::Create(
            concatNType,
            llvm::Function::ExternalLinkage,
            "npk_string_concat_n_simple",
            module
        );
    }

    // Get pointer to first element of the array
    llvm::Value* arrayPtr = builder.CreateGEP(arrayType, stringsArray,
        { llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), 0),
          llvm::ConstantInt::get(llvm::Type::getInt64Ty(context), 0) },
        "array_start");

    // Call npk_string_concat_n_simple(strings, count)
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
    llvm::Function* npk_panic_oom = module->getFunction("npk_panic_oom");
    if (!npk_panic_oom) {
        llvm::FunctionType* panicType = llvm::FunctionType::get(
            llvm::Type::getVoidTy(context),
            { llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0) },
            false
        );
        npk_panic_oom = llvm::Function::Create(
            panicType,
            llvm::Function::ExternalLinkage,
            "npk_panic_oom",
            module
        );
    }
    llvm::Value* panicMsg = builder.CreateGlobalString(
        "Out of memory in string concatenation (template literal)",
        "oom_msg"
    );
    builder.CreateCall(npk_panic_oom, { panicMsg });
    builder.CreateUnreachable();
    
    // Valid path: continue with normal flow
    builder.SetInsertPoint(validBB);

    // Cleanup: Free heap-allocated array if needed
    if (totalSegments > MAX_STACK_SEGMENTS) {
        // Call npk_free on the heap-allocated array
        llvm::FunctionCallee npk_free_callee = module->getOrInsertFunction("npk_wild_free",
            llvm::FunctionType::get(
                llvm::Type::getVoidTy(context),
                {llvm::PointerType::get(context, 0)},
                false
            )
        );
        llvm::Function* npk_free = llvm::cast<llvm::Function>(npk_free_callee.getCallee());
        llvm::Value* heapPtr = builder.CreateBitCast(
            stringsArray,
            llvm::PointerType::get(context, 0),
            "heap_ptr"
        );
        builder.CreateCall(npk_free, { heapPtr });
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
    ARIA_DBG_STREAM << "[DEBUG codegenExpressionNode] NodeType: " << static_cast<int>(node->type) << std::endl;
    
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
            
            // Extract is_error field (field 2) — stored as i8, must compare to get i1 for branch
            llvm::Value* isErrorRaw = codegen->builder.CreateExtractValue(resultVal, 2, "is_error");
            llvm::Value* isError = codegen->builder.CreateICmpNE(isErrorRaw,
                llvm::ConstantInt::get(isErrorRaw->getType(), 0), "is_error_bool");
            
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
        case ASTNode::NodeType::DEFAULTS: {
            // Defaults operator: expr ?| fallback (v0.4.3)
            // Wraps the sub-expression. If any Result in the sub-expression produces ERR,
            // the whole chain short-circuits to the fallback value.
            DefaultsExpr* defExpr = static_cast<DefaultsExpr*>(node);

            // v0.5.0: If the sub-expression is provably infallible, skip fallback entirely
            if (codegen->defaults_safe_ptr && codegen->defaults_safe_ptr->count(node)) {
                llvm::Value* exprVal = codegenExpressionNode(defExpr->expr.get(), codegen);
                if (!exprVal) return nullptr;
                // If it's still a Result struct, extract the value — error path is dead
                if (exprVal->getType()->isStructTy()) {
                    llvm::StructType* st = llvm::cast<llvm::StructType>(exprVal->getType());
                    if (st->getNumElements() == 3 &&
                        st->getElementType(1)->isPointerTy() &&
                        st->getElementType(2)->isIntegerTy(8)) {
                        return codegen->builder.CreateExtractValue(exprVal, 0, "defaults_unwrap_safe");
                    }
                }
                return exprVal;
            }

            llvm::Function* currentFunc = codegen->builder.GetInsertBlock()->getParent();
            
            // Create fallback and merge blocks
            llvm::BasicBlock* fallbackBlock = llvm::BasicBlock::Create(codegen->context, "defaults_fallback", currentFunc);
            llvm::BasicBlock* mergeBlock = llvm::BasicBlock::Create(codegen->context, "defaults_merge", currentFunc);
            
            // Push fallback block onto the stack — arg auto-unwrap ERR checks will branch here
            codegen->defaults_fallback_stack_.push_back(fallbackBlock);
            
            // Generate the sub-expression (may contain function calls that produce Result)
            llvm::Value* exprVal = codegenExpressionNode(defExpr->expr.get(), codegen);
            if (!exprVal) return nullptr;
            
            // Pop the fallback block
            codegen->defaults_fallback_stack_.pop_back();
            
            // If the sub-expression itself returned a Result struct, unwrap it
            llvm::Value* successVal = exprVal;
            if (exprVal->getType()->isStructTy()) {
                llvm::StructType* st = llvm::cast<llvm::StructType>(exprVal->getType());
                if (st->getNumElements() == 3 &&
                    st->getElementType(1)->isPointerTy() &&
                    st->getElementType(2)->isIntegerTy(8)) {
                    // Check is_error on the final result
                    llvm::Value* isError = codegen->builder.CreateExtractValue(exprVal, 2, "defaults_final_err");
                    llvm::Value* isErr = codegen->builder.CreateICmpNE(isError,
                        llvm::ConstantInt::get(llvm::Type::getInt8Ty(codegen->context), 0), "defaults_err_check");
                    
                    llvm::BasicBlock* successBlock = llvm::BasicBlock::Create(codegen->context, "defaults_ok", currentFunc);
                    codegen->builder.CreateCondBr(isErr, fallbackBlock, successBlock);
                    
                    codegen->builder.SetInsertPoint(successBlock);
                    successVal = codegen->builder.CreateExtractValue(exprVal, 0, "defaults_unwrap");
                }
            }
            
            // Record the success end block (may have changed due to above unwrap)
            llvm::BasicBlock* successEndBlock = codegen->builder.GetInsertBlock();
            codegen->builder.CreateBr(mergeBlock);
            
            // Fallback block: generate the fallback value
            codegen->builder.SetInsertPoint(fallbackBlock);
            llvm::Value* fallbackVal = codegenExpressionNode(defExpr->fallback.get(), codegen);
            if (!fallbackVal) return nullptr;
            
            // Type-match the fallback to the success value if needed
            if (fallbackVal->getType() != successVal->getType()) {
                if (fallbackVal->getType()->isIntegerTy() && successVal->getType()->isIntegerTy()) {
                    fallbackVal = codegen->builder.CreateSExtOrTrunc(fallbackVal, successVal->getType(), "defaults_cast");
                } else if (fallbackVal->getType()->isFloatingPointTy() && successVal->getType()->isFloatingPointTy()) {
                    fallbackVal = codegen->builder.CreateFPCast(fallbackVal, successVal->getType(), "defaults_fcast");
                }
            }
            
            llvm::BasicBlock* fallbackEndBlock = codegen->builder.GetInsertBlock();
            codegen->builder.CreateBr(mergeBlock);
            
            // Merge block: PHI node
            codegen->builder.SetInsertPoint(mergeBlock);
            llvm::PHINode* phi = codegen->builder.CreatePHI(successVal->getType(), 2, "defaults_result");
            phi->addIncoming(successVal, successEndBlock);
            phi->addIncoming(fallbackVal, fallbackEndBlock);
            
            return phi;
        }
        default:
            std::cerr << "[ERROR] Unhandled NodeType: " << static_cast<int>(node->type) << std::endl;
            throw std::runtime_error("Unsupported expression node type in operation");
    }
}

