#ifndef ARIA_TYPE_H
#define ARIA_TYPE_H

#include "ast_node.h"
#include <vector>

namespace aria {

/**
 * Base class for type annotations
 * Represents type information in variable/function declarations
 */
class TypeNode : public ASTNode {
public:
    TypeNode(NodeType type, int line = 0, int column = 0)
        : ASTNode(type, line, column) {}
    
    virtual ~TypeNode() = default;
};

/**
 * Simple type annotation
 * Represents: int8, string, bool, etc.
 */
class SimpleType : public TypeNode {
public:
    std::string typeName;  // e.g., "int8", "string", "bool"
    
    SimpleType(const std::string& name, int line = 0, int column = 0)
        : TypeNode(NodeType::TYPE_ANNOTATION, line, column), typeName(name) {}
    
    std::string toString() const override;
};

/**
 * Pointer type annotation
 * Represents: int8*, string*, obj*
 */
class PointerType : public TypeNode {
public:
    ASTNodePtr baseType;  // The type being pointed to
    
    PointerType(ASTNodePtr base, int line = 0, int column = 0)
        : TypeNode(NodeType::POINTER_TYPE, line, column), baseType(base) {}
    
    std::string toString() const override;
};

/**
 * Array type annotation
 * Represents: int8[], int8[100], string[]
 */
class ArrayType : public TypeNode {
public:
    ASTNodePtr elementType;  // Type of array elements
    ASTNodePtr sizeExpr;     // Size expression (nullptr for dynamic arrays)
    bool isDynamic;          // true for int8[], false for int8[100]
    
    ArrayType(ASTNodePtr elemType, ASTNodePtr size, int line = 0, int column = 0)
        : TypeNode(NodeType::ARRAY_TYPE, line, column),
          elementType(elemType), sizeExpr(size), isDynamic(size == nullptr) {}
    
    std::string toString() const override;
};

/**
 * Generic type annotation
 * Represents: Array<int8>, Map<string, int32>
 */
class GenericType : public TypeNode {
public:
    std::string baseName;              // e.g., "Array", "Map"
    std::vector<ASTNodePtr> typeArgs;  // Type arguments
    
    GenericType(const std::string& base, const std::vector<ASTNodePtr>& args,
                int line = 0, int column = 0)
        : TypeNode(NodeType::GENERIC_TYPE, line, column),
          baseName(base), typeArgs(args) {}
    
    std::string toString() const override;
};

/**
 * Function type annotation
 * Represents: func type in parameters or variables
 */
class FunctionType : public TypeNode {
public:
    ASTNodePtr returnType;              // Return type (can be another TypeNode)
    std::vector<ASTNodePtr> paramTypes; // Parameter types
    
    FunctionType(ASTNodePtr retType, const std::vector<ASTNodePtr>& params,
                 int line = 0, int column = 0)
        : TypeNode(NodeType::FUNCTION_TYPE, line, column),
          returnType(retType), paramTypes(params) {}
    
    std::string toString() const override;
};

} // namespace aria

#endif // ARIA_TYPE_H
