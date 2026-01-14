#ifndef ARIA_AST_NODE_H
#define ARIA_AST_NODE_H

#include <string>
#include <memory>
#include <vector>

namespace aria {

/**
 * Base class for all Abstract Syntax Tree (AST) nodes.
 * 
 * The AST represents the hierarchical structure of Aria source code
 * after parsing. Each node corresponds to a construct in the language.
 */
class ASTNode {
public:
    enum class NodeType {
        // === EXPRESSIONS ===
        LITERAL,              // Integer, float, string, boolean, null literals
        IDENTIFIER,           // Variable/function names
        BINARY_OP,            // Binary operations: +, -, *, /, etc.
        UNARY_OP,             // Unary operations: -, !, ~, @, #, $
        CALL,                 // Function calls
        INDEX,                // Array indexing: arr[index]
        MEMBER_ACCESS,        // Object member access: obj.member
        POINTER_MEMBER,       // Pointer member access: ptr->member
        LAMBDA,               // Lambda expressions: returnType(params) { body }
        TEMPLATE_LITERAL,     // Template literals with interpolation
        RANGE,                // Range expressions: start..end, start...end
        TERNARY,              // Ternary operator: is cond : true_val : false_val
        SAFE_NAV,             // Safe navigation: obj?.member
        NULL_COALESCE,        // Null coalescing: value ?? default
        PIPELINE,             // Pipeline operators: |>, <|
        UNWRAP,               // Unwrap operator: result?
        ARRAY_LITERAL,        // Array literal: [1, 2, 3]
        OBJECT_LITERAL,       // Object literal: { key: value }
        AWAIT,                // Await expression: await future (async/await)
        MOVE,                 // Move expression: move(x) - ownership transfer
        VECTOR_CONSTRUCTOR,   // Vector constructor: vec2(x, y), vec3(x, y, z), vec9(...)
        CAST,                 // Type cast: @cast<TargetType>(expr), @cast_unchecked<T>(expr)
        
        // === STATEMENTS ===
        VAR_DECL,             // Variable declaration: type:name = value;
        FUNC_DECL,            // Function declaration: func:name = returnType(params) { body };
        STRUCT_DECL,          // Struct declaration: struct Name { fields };
        ENUM_DECL,            // Enum declaration: enum Name { VARIANT = value };
        OPAQUE_STRUCT,        // Opaque struct declaration: opaque struct:Name; (FFI)
        TRAIT_DECL,           // Trait declaration: trait:Name = { methods };
        IMPL_DECL,            // Trait implementation: impl:Trait:for:Type = { methods };
        RETURN,               // return statement
        PASS,                 // pass statement (result success)
        FAIL,                 // fail statement (result error)
        BREAK,                // break statement
        CONTINUE,             // continue statement
        DEFER,                // defer statement (RAII cleanup)
        BLOCK,                // Code block: { ... }
        EXPRESSION_STMT,      // Expression as statement
        
        // === CONTROL FLOW ===
        IF,                   // if/else if/else
        WHILE,                // while loop
        FOR,                  // for loop
        LOOP,                 // loop(start, limit, step)
        TILL,                 // till(limit, step)
        WHEN,                 // when/then/end loop
        PICK,                 // pick (pattern matching)
        PICK_CASE,            // Individual pick case
        FALL,                 // fall statement (explicit fallthrough)
        
        // === TYPES ===
        TYPE_ANNOTATION,      // Type annotation: int8, string, etc.
        GENERIC_TYPE,         // Generic type: Array<T>
        ARRAY_TYPE,           // Array type: int8[], int8[100]
        POINTER_TYPE,         // Pointer type: int8@
        SAFE_REF_TYPE,        // Safe reference type: int64$, string$ (borrow checker)
        OPTIONAL_TYPE,        // Optional type: int64?, string? (can be NIL)
        FUNCTION_TYPE,        // Function type: func
        
        // === MODULES ===
        USE,                  // use statement (import)
        MOD,                  // mod statement (module definition)
        EXTERN,               // extern block (FFI)
        PROGRAM,              // Root node (entire program)
        
        // === SPECIAL ===
        ASSIGNMENT,           // Assignment: =, +=, -=, etc.
        PARAMETER,            // Function parameter
        ARGUMENT,             // Function argument
    };
    
    NodeType type;
    int line;      // Source line number
    int column;    // Source column number
    
    ASTNode(NodeType t, int l = 0, int c = 0) 
        : type(t), line(l), column(c) {}
    
    virtual ~ASTNode() = default;
    
    /**
     * Convert the node to a string representation (for debugging/testing)
     */
    virtual std::string toString() const = 0;
    
    /**
     * Get the node type as a string
     */
    static std::string nodeTypeToString(NodeType type);
    
    /**
     * Helper to check node type
     */
    bool isExpression() const;
    bool isStatement() const;
    bool isType() const;
};

// Smart pointer typedef for convenience
using ASTNodePtr = std::shared_ptr<ASTNode>;

} // namespace aria

// Stream output operator for NodeType (for testing)
std::ostream& operator<<(std::ostream& os, aria::ASTNode::NodeType type);

#endif // ARIA_AST_NODE_H
