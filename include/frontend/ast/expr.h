#ifndef ARIA_EXPR_H
#define ARIA_EXPR_H

#include "ast_node.h"
#include "frontend/token.h"
#include <variant>

namespace aria {

// Import Token from frontend namespace for convenience
using aria::frontend::Token;
using aria::frontend::TokenType;

/**
 * Literal expression node
 * Represents: integer, float, string, boolean, null literals
 */
class LiteralExpr : public ASTNode {
public:
    std::variant<int64_t, double, std::string, bool, std::monostate> value;
    
    explicit LiteralExpr(int64_t val, int line = 0, int column = 0)
        : ASTNode(NodeType::LITERAL, line, column), value(val) {}
    
    explicit LiteralExpr(double val, int line = 0, int column = 0)
        : ASTNode(NodeType::LITERAL, line, column), value(val) {}
    
    explicit LiteralExpr(const std::string& val, int line = 0, int column = 0)
        : ASTNode(NodeType::LITERAL, line, column), value(val) {}
    
    explicit LiteralExpr(bool val, int line = 0, int column = 0)
        : ASTNode(NodeType::LITERAL, line, column), value(val) {}
    
    // Null literal
    explicit LiteralExpr(std::monostate, int line = 0, int column = 0)
        : ASTNode(NodeType::LITERAL, line, column), value(std::monostate{}) {}
    
    std::string toString() const override;
};

/**
 * Identifier expression node
 * Represents: variable names, function names
 */
class IdentifierExpr : public ASTNode {
public:
    std::string name;
    
    IdentifierExpr(const std::string& n, int line = 0, int column = 0)
        : ASTNode(NodeType::IDENTIFIER, line, column), name(n) {}
    
    std::string toString() const override;
};

/**
 * Binary operation expression node
 * Represents: a + b, x * y, etc.
 */
class BinaryExpr : public ASTNode {
public:
    ASTNodePtr left;
    Token op;
    ASTNodePtr right;
    
    BinaryExpr(ASTNodePtr l, const Token& o, ASTNodePtr r, int line = 0, int column = 0)
        : ASTNode(NodeType::BINARY_OP, line, column), left(l), op(o), right(r) {}
    
    std::string toString() const override;
};

/**
 * Unary operation expression node
 * Represents: -x, !flag, ~bits, @ptr, #ref, $iter
 */
class UnaryExpr : public ASTNode {
public:
    Token op;
    ASTNodePtr operand;
    bool isPostfix;
    
    UnaryExpr(const Token& o, ASTNodePtr operand, bool isPost = false, int line = 0, int column = 0)
        : ASTNode(NodeType::UNARY_OP, line, column), op(o), operand(operand), isPostfix(isPost) {}
    
    std::string toString() const override;
};

/**
 * Function call expression node
 * Represents: func(arg1, arg2, ...) or func::<T, U>(arg1, arg2, ...)
 */
class CallExpr : public ASTNode {
public:
    ASTNodePtr callee;
    std::vector<ASTNodePtr> arguments;
    std::vector<std::string> explicitTypeArgs;  // For turbofish syntax: ::<T, U>
    
    CallExpr(ASTNodePtr callee, const std::vector<ASTNodePtr>& args, int line = 0, int column = 0)
        : ASTNode(NodeType::CALL, line, column), callee(callee), arguments(args) {}
    
    CallExpr(ASTNodePtr callee, const std::vector<ASTNodePtr>& args, 
             const std::vector<std::string>& typeArgs, int line = 0, int column = 0)
        : ASTNode(NodeType::CALL, line, column), callee(callee), arguments(args), 
          explicitTypeArgs(typeArgs) {}
    
    std::string toString() const override;
};

/**
 * Array index expression node
 * Represents: arr[index]
 */
class IndexExpr : public ASTNode {
public:
    ASTNodePtr array;
    ASTNodePtr index;
    
    IndexExpr(ASTNodePtr arr, ASTNodePtr idx, int line = 0, int column = 0)
        : ASTNode(NodeType::INDEX, line, column), array(arr), index(idx) {}
    
    std::string toString() const override;
};

/**
 * Member access expression node
 * Represents: obj.member
 */
class MemberAccessExpr : public ASTNode {
public:
    ASTNodePtr object;
    std::string member;
    bool isPointerAccess;  // true for ->, false for .
    
    MemberAccessExpr(ASTNodePtr obj, const std::string& mem, bool isPtr = false, int line = 0, int column = 0)
        : ASTNode(isPtr ? NodeType::POINTER_MEMBER : NodeType::MEMBER_ACCESS, line, column),
          object(obj), member(mem), isPointerAccess(isPtr) {}
    
    std::string toString() const override;
};

/**
 * Ternary expression node
 * Represents: is condition : true_value : false_value
 */
class TernaryExpr : public ASTNode {
public:
    ASTNodePtr condition;
    ASTNodePtr trueValue;
    ASTNodePtr falseValue;
    
    TernaryExpr(ASTNodePtr cond, ASTNodePtr trueVal, ASTNodePtr falseVal, int line = 0, int column = 0)
        : ASTNode(NodeType::TERNARY, line, column),
          condition(cond), trueValue(trueVal), falseValue(falseVal) {}
    
    std::string toString() const override;
};

/**
 * Assignment expression node
 * Represents: x = 5, y += 3, etc.
 */
class AssignmentExpr : public ASTNode {
public:
    ASTNodePtr target;
    Token op;  // =, +=, -=, *=, /=, %=
    ASTNodePtr value;
    
    AssignmentExpr(ASTNodePtr tgt, const Token& o, ASTNodePtr val, int line = 0, int column = 0)
        : ASTNode(NodeType::ASSIGNMENT, line, column), target(tgt), op(o), value(val) {}
    
    std::string toString() const override;
};

/**
 * Array literal expression node
 * Represents: [1, 2, 3, 4]
 */
class ArrayLiteralExpr : public ASTNode {
public:
    std::vector<ASTNodePtr> elements;
    
    ArrayLiteralExpr(const std::vector<ASTNodePtr>& elems, int line = 0, int column = 0)
        : ASTNode(NodeType::ARRAY_LITERAL, line, column), elements(elems) {}
    
    std::string toString() const override;
};

} // namespace aria

#endif // ARIA_EXPR_H
