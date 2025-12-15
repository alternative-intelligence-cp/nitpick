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
        case NodeType::ARRAY_LITERAL: return "ARRAY_LITERAL";
        case NodeType::OBJECT_LITERAL: return "OBJECT_LITERAL";
        
        // Statements
        case NodeType::VAR_DECL: return "VAR_DECL";
        case NodeType::FUNC_DECL: return "FUNC_DECL";
        case NodeType::RETURN: return "RETURN";
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
        
        // Types
        case NodeType::TYPE_ANNOTATION: return "TYPE_ANNOTATION";
        case NodeType::GENERIC_TYPE: return "GENERIC_TYPE";
        case NodeType::ARRAY_TYPE: return "ARRAY_TYPE";
        case NodeType::POINTER_TYPE: return "POINTER_TYPE";
        case NodeType::FUNCTION_TYPE: return "FUNCTION_TYPE";
        
        // Modules
        case NodeType::USE: return "USE";
        case NodeType::MOD: return "MOD";
        case NodeType::EXTERN: return "EXTERN";
        case NodeType::PROGRAM: return "PROGRAM";
        
        // Special
        case NodeType::ASSIGNMENT: return "ASSIGNMENT";
        case NodeType::PARAMETER: return "PARAMETER";
        case NodeType::ARGUMENT: return "ARGUMENT";
        
        default: return "UNKNOWN";
    }
}

bool ASTNode::isExpression() const {
    return type >= NodeType::LITERAL && type <= NodeType::OBJECT_LITERAL;
}

bool ASTNode::isStatement() const {
    return (type >= NodeType::VAR_DECL && type <= NodeType::EXPRESSION_STMT) ||
           (type >= NodeType::IF && type <= NodeType::PICK_CASE);
}

bool ASTNode::isType() const {
    return type >= NodeType::TYPE_ANNOTATION && type <= NodeType::FUNCTION_TYPE;
}

} // namespace aria
