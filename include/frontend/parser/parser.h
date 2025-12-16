#ifndef ARIA_PARSER_H
#define ARIA_PARSER_H

#include <vector>
#include <string>
#include <memory>
#include <unordered_map>
#include "frontend/token.h"
#include "frontend/lexer/lexer.h"
#include "frontend/ast/ast_node.h"
#include "frontend/ast/expr.h"
#include "frontend/ast/stmt.h"

namespace aria {

class Parser {
private:
    std::vector<frontend::Token> tokens;
    size_t current;
    std::vector<std::string> errors;
    
    // Operator precedence map (higher number = higher precedence)
    static const std::unordered_map<frontend::TokenType, int> precedence;
    
    // Utility methods
    frontend::Token peek() const;
    frontend::Token previous() const;
    frontend::Token advance();
    bool isAtEnd() const;
    bool check(frontend::TokenType type) const;
    bool match(frontend::TokenType type);
    bool match(const std::vector<frontend::TokenType>& types);
    frontend::Token consume(frontend::TokenType type, const std::string& message);
    void synchronize();  // Error recovery
    void error(const std::string& message);
    
    // Get operator precedence
    int getPrecedence(frontend::TokenType type) const;
    bool isBinaryOperator(frontend::TokenType type) const;
    bool isUnaryOperator(frontend::TokenType type) const;
    bool isAssignmentOperator(frontend::TokenType type) const;
    
    // Expression parsing (precedence climbing)
    ASTNodePtr parseExpression(int minPrecedence = 0);
    ASTNodePtr parsePrimary();
    ASTNodePtr parseUnary();
    ASTNodePtr parsePostfix(ASTNodePtr expr);
    ASTNodePtr parseAssignment();
    
    // Helper for specific expressions
    ASTNodePtr parseCallExpression(ASTNodePtr callee);
    ASTNodePtr parseIndexExpression(ASTNodePtr array);
    ASTNodePtr parseMemberExpression(ASTNodePtr object);
    ASTNodePtr parseArrayLiteral();
    ASTNodePtr parseObjectLiteral();
    ASTNodePtr parseTemplateLiteral();
    ASTNodePtr parseLambda();
    
    // Phase 2.4: Statement parsing
    ASTNodePtr parseStatement();
    ASTNodePtr parseVarDecl();
    ASTNodePtr parseFuncDecl();
    ASTNodePtr parseBlock();
    
    // Phase 2.5: Type and module parsing
    ASTNodePtr parseType();
    ASTNodePtr parseUseStatement();
    ASTNodePtr parseModStatement();
    ASTNodePtr parseExternStatement();
    ASTNodePtr parseExpressionStmt();
    ASTNodePtr parseReturn();
    ASTNodePtr parsePassStatement();
    ASTNodePtr parseFailStatement();
    ASTNodePtr parseIfStatement();
    ASTNodePtr parseWhileStatement();
    ASTNodePtr parseForStatement();
    ASTNodePtr parseBreakStatement();
    ASTNodePtr parseContinueStatement();
    ASTNodePtr parseDeferStatement();
    ASTNodePtr parseTillStatement();
    ASTNodePtr parseLoopStatement();
    ASTNodePtr parseWhenStatement();
    ASTNodePtr parsePickStatement();
    ASTNodePtr parseFallStatement();
    bool isTypeKeyword(frontend::TokenType type) const;
    
public:
    Parser(const std::vector<frontend::Token>& tokens);
    
    // Main parse entry point
    ASTNodePtr parse();  // Returns ProgramNode
    
    // Error handling
    bool hasErrors() const;
    const std::vector<std::string>& getErrors() const;
};

} // namespace aria

#endif // ARIA_PARSER_H
