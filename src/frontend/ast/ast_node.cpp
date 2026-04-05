#include "frontend/ast/ast_node.h"
#include <ostream>

// Stream output operator for NodeType (for testing)
std::ostream& operator<<(std::ostream& os, aria::ASTNode::NodeType type) {
    os << aria::ASTNode::nodeTypeToString(type);
    return os;
}

namespace aria {

std::string ASTNode::nodeTypeToString(NodeType type) {
    switch (type) {
        // Expressions
        case NodeType::LITERAL: return "LITERAL";
        case NodeType::IDENTIFIER: return "IDENTIFIER";
        case NodeType::BINARY_OP: return "BINARY_OP";
        case NodeType::UNARY_OP: return "UNARY_OP";
        case NodeType::CALL: return "CALL";
        case NodeType::INDEX: return "INDEX";
        case NodeType::MEMBER_ACCESS: return "MEMBER_ACCESS";
        case NodeType::POINTER_MEMBER: return "POINTER_MEMBER";
        case NodeType::LAMBDA: return "LAMBDA";
        case NodeType::TEMPLATE_LITERAL: return "TEMPLATE_LITERAL";
        case NodeType::RANGE: return "RANGE";
        case NodeType::TERNARY: return "TERNARY";
        case NodeType::SAFE_NAV: return "SAFE_NAV";
        case NodeType::NULL_COALESCE: return "NULL_COALESCE";
        case NodeType::PIPELINE: return "PIPELINE";
        case NodeType::UNWRAP: return "UNWRAP";
        case NodeType::DEFAULTS: return "DEFAULTS";
        case NodeType::ARRAY_LITERAL: return "ARRAY_LITERAL";
        case NodeType::OBJECT_LITERAL: return "OBJECT_LITERAL";
        case NodeType::AWAIT: return "AWAIT";
        case NodeType::MOVE: return "MOVE";
        case NodeType::VECTOR_CONSTRUCTOR: return "VECTOR_CONSTRUCTOR";
        case NodeType::CAST: return "CAST";
        case NodeType::SPREAD: return "SPREAD";
        
        // Statements
        case NodeType::VAR_DECL: return "VAR_DECL";
        case NodeType::FUNC_DECL: return "FUNC_DECL";
        case NodeType::STRUCT_DECL: return "STRUCT_DECL";
        case NodeType::ENUM_DECL: return "ENUM_DECL";
        case NodeType::RULES_DECL: return "RULES_DECL";
        case NodeType::TYPE_DECL: return "TYPE_DECL";
        case NodeType::OPAQUE_STRUCT: return "OPAQUE_STRUCT";
        case NodeType::TRAIT_DECL: return "TRAIT_DECL";
        case NodeType::IMPL_DECL: return "IMPL_DECL";
        case NodeType::RETURN: return "RETURN";
        case NodeType::PASS: return "PASS";
        case NodeType::FAIL: return "FAIL";
        case NodeType::PROVE: return "PROVE";
        case NodeType::ASSERT_STATIC: return "ASSERT_STATIC";
        case NodeType::BREAK: return "BREAK";
        case NodeType::CONTINUE: return "CONTINUE";
        case NodeType::DEFER: return "DEFER";
        case NodeType::BLOCK: return "BLOCK";
        case NodeType::EXPRESSION_STMT: return "EXPRESSION_STMT";
        
        // Control Flow
        case NodeType::IF: return "IF";
        case NodeType::WHILE: return "WHILE";
        case NodeType::FOR: return "FOR";
        case NodeType::LOOP: return "LOOP";
        case NodeType::TILL: return "TILL";
        case NodeType::WHEN: return "WHEN";
        case NodeType::PICK: return "PICK";
        case NodeType::PICK_CASE: return "PICK_CASE";
        case NodeType::FALL: return "FALL";
        
        // Types
        case NodeType::TYPE_ANNOTATION: return "TYPE_ANNOTATION";
        case NodeType::GENERIC_TYPE: return "GENERIC_TYPE";
        case NodeType::ARRAY_TYPE: return "ARRAY_TYPE";
        case NodeType::POINTER_TYPE: return "POINTER_TYPE";
        case NodeType::SAFE_REF_TYPE: return "SAFE_REF_TYPE";
        case NodeType::OPTIONAL_TYPE: return "OPTIONAL_TYPE";
        case NodeType::FUNCTION_TYPE: return "FUNCTION_TYPE";
        
        // Modules
        case NodeType::USE: return "USE";
        case NodeType::MOD: return "MOD";
        case NodeType::EXTERN: return "EXTERN";
        case NodeType::PROGRAM: return "PROGRAM";
        
        // Comptime
        case NodeType::COMPTIME_BLOCK: return "COMPTIME_BLOCK";
        case NodeType::COMPTIME_EXPR: return "COMPTIME_EXPR";
        
        // Macro system (v0.8.3)
        case NodeType::MACRO_DECL: return "MACRO_DECL";
        case NodeType::MACRO_INVOCATION: return "MACRO_INVOCATION";
        
        // Special
        case NodeType::ASSIGNMENT: return "ASSIGNMENT";
        case NodeType::PARAMETER: return "PARAMETER";
        case NodeType::ARGUMENT: return "ARGUMENT";
        
        default: return "UNKNOWN";
    }
}

bool ASTNode::isExpression() const {
    return (type >= NodeType::LITERAL && type <= NodeType::SPREAD) ||
           type == NodeType::COMPTIME_EXPR ||
           type == NodeType::MACRO_INVOCATION;
}

bool ASTNode::isStatement() const {
    return (type >= NodeType::VAR_DECL && type <= NodeType::EXPRESSION_STMT) ||
           (type >= NodeType::IF && type <= NodeType::PICK_CASE) ||
           type == NodeType::COMPTIME_BLOCK ||
           type == NodeType::MACRO_DECL;
}

bool ASTNode::isType() const {
    return type >= NodeType::TYPE_ANNOTATION && type <= NodeType::FUNCTION_TYPE;
}

} // namespace aria
