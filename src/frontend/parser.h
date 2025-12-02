#ifndef ARIA_FRONTEND_PARSER_H
#define ARIA_FRONTEND_PARSER_H

#include "tokens.h"
#include "ast.h"
#include "lexer.h"
#include "ast/expr.h"
#include "ast/stmt.h"
#include "ast/control_flow.h"
#include "ast/defer.h"
#include "ast/loops.h"
#include "ast/module.h"
#include <memory>
#include <vector>

namespace aria {
namespace frontend {

// Parser Context
// Stores configuration and state for parsing
struct ParserContext {
    bool strictMode = false;  // Enable strict type checking
    // Future: Add more parser configuration options
};

// Aria Parser
// Parses a stream of tokens into an Abstract Syntax Tree (AST)
class Parser {
private:
    AriaLexer& lexer;
    Token current;
    ParserContext context;

    void advance();
    bool match(TokenType type);
    Token expect(TokenType type);
    Token consume(TokenType type, const std::string& message);  // Alias for expect with custom error
    bool check(TokenType type);  // Lookahead without consuming
    bool isTypeToken(TokenType type);  // Helper to check if token is a valid type
    std::vector<FuncParam> parseParams();  // Parse function parameters

    // Expression parsing (precedence climbing)
    std::unique_ptr<Expression> parsePrimary();
    std::unique_ptr<Expression> parsePostfix();
    std::unique_ptr<Expression> parseUnary();
    std::unique_ptr<Expression> parseMultiplicative();
    std::unique_ptr<Expression> parseAdditive();
    std::unique_ptr<Expression> parseShift();
    std::unique_ptr<Expression> parseRelational();
    std::unique_ptr<Expression> parseEquality();
    std::unique_ptr<Expression> parseBitwiseAnd();
    std::unique_ptr<Expression> parseBitwiseXor();
    std::unique_ptr<Expression> parseBitwiseOr();
    std::unique_ptr<Expression> parseLogicalAnd();
    std::unique_ptr<Expression> parseLogicalOr();
    std::unique_ptr<Expression> parseTernary();
    std::unique_ptr<Expression> parseAssignment();

public:
    Parser(AriaLexer& lex);
    Parser(AriaLexer& lex, ParserContext ctx);

    std::unique_ptr<Block> parseBlock();
    std::unique_ptr<Expression> parseExpr();
    std::unique_ptr<Expression> parseTemplateString();
    std::unique_ptr<Statement> parseStmt();
    std::unique_ptr<VarDecl> parseVarDecl();
    std::unique_ptr<FuncDecl> parseFuncDecl();  // Bug #70: async functions
    std::unique_ptr<Statement> parseAsyncBlock();  // Bug #70: async blocks
    std::unique_ptr<PickStmt> parsePickStmt();
    std::unique_ptr<DestructurePattern> parseDestructurePattern();  // Bug #64
    std::unique_ptr<Statement> parseDeferStmt();
    std::unique_ptr<Statement> parseFallStmt();  // Bug #66
    
    // Control flow (Bug #67-71)
    std::unique_ptr<Statement> parseForLoop();
    std::unique_ptr<Statement> parseWhileLoop();
    std::unique_ptr<Statement> parseTillLoop();
    std::unique_ptr<Statement> parseWhenLoop();  // Changed: when is a loop, not expression
    std::unique_ptr<Statement> parseBreak();
    std::unique_ptr<Statement> parseContinue();
    
    // Module system (Bug #73-75)
    std::unique_ptr<Statement> parseUseStmt();
    std::unique_ptr<Statement> parseExternBlock();
    std::unique_ptr<Statement> parseModDef();
};

} // namespace frontend
} // namespace aria

#endif // ARIA_FRONTEND_PARSER_H
