// Implementation of Statement Parsing
// Handles: defer statements and block parsing
#include "parser.h"
#include "ast.h"
#include "ast/stmt.h"
#include "ast/defer.h"
#include <memory>

namespace aria {
namespace frontend {

std::unique_ptr<Statement> Parser::parseDeferStmt() {
    // TODO: Implement defer statement parsing
    // This requires:
    // - consume(TOKEN_DEFER)
    // - parseStatement() or parseBlock() for the deferred statement
    // - Tracking defers in current block scope
    
    // Placeholder implementation
    auto placeholder_body = std::make_unique<Block>();
    return std::make_unique<DeferStmt>(std::move(placeholder_body));
}

std::unique_ptr<Block> Parser::parseBlock() {
    expect(TOKEN_LBRACE);
    
    auto block = std::make_unique<Block>();
    
    // Parse statements until we hit }
    while (current.type != TOKEN_RBRACE && current.type != TOKEN_EOF) {
        // Check for function declaration
        if (current.type == TOKEN_KW_FUNC || 
            current.type == TOKEN_KW_PUB ||
            current.type == TOKEN_KW_ASYNC) {
            auto func = parseFuncDecl();
            if (func) {
                block->statements.push_back(std::move(func));
            }
            continue;
        }
        
        auto stmt = parseStmt();
        if (stmt) {
            block->statements.push_back(std::move(stmt));
        }
    }
    
    expect(TOKEN_RBRACE);
    return block;
}

std::unique_ptr<Statement> Parser::parseStmt() {
    // Handle various statement types
    
    // Return statement
    if (match(TOKEN_KW_RETURN)) {
        std::unique_ptr<Expression> retVal = nullptr;
        if (current.type != TOKEN_SEMICOLON && current.type != TOKEN_RBRACE) {
            retVal = parseExpr();
        }
        match(TOKEN_SEMICOLON);  // Optional semicolon
        return std::make_unique<ReturnStmt>(std::move(retVal));
    }
    
    // If statement
    if (match(TOKEN_KW_IF)) {
        expect(TOKEN_LPAREN);
        auto condition = parseExpr();
        expect(TOKEN_RPAREN);
        auto then_block = parseBlock();
        
        std::unique_ptr<Block> else_block = nullptr;
        if (match(TOKEN_KW_ELSE)) {
            else_block = parseBlock();
        }
        
        return std::make_unique<IfStmt>(std::move(condition), std::move(then_block), std::move(else_block));
    }
    
    // Variable declaration or expression statement
    // Try to parse as expression first
    auto expr = parseExpr();
    match(TOKEN_SEMICOLON);  // Optional semicolon
    return std::make_unique<ExpressionStmt>(std::move(expr));
}

} // namespace frontend
} // namespace aria
