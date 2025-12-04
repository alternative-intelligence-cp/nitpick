// Implementation of Statement Parsing
// Handles: defer statements and block parsing
#include "parser.h"
#include "ast.h"
#include "ast/stmt.h"
#include "ast/defer.h"
#include <memory>
#include <sstream>
#include <stdexcept>

namespace aria {
namespace frontend {

// Parse top-level program (file contents without { } wrapper)
std::unique_ptr<Block> Parser::parseProgram() {
    auto block = std::make_unique<Block>();
    
    // Parse top-level declarations until EOF
    while (current.type != TOKEN_EOF) {
        // Skip any stray semicolons
        if (match(TOKEN_SEMICOLON)) {
            continue;
        }
        
        // Check for function declaration (fn keyword)
        if (current.type == TOKEN_KW_FUNC) {
            auto func = parseFuncDecl();
            if (func) {
                block->statements.push_back(std::move(func));
            }
            continue;
        }
        
        // Check for public function
        if (current.type == TOKEN_KW_PUB) {
            advance();
            if (current.type == TOKEN_KW_FUNC) {
                auto func = parseFuncDecl();
                if (func) {
                    func->is_pub = true;
                    block->statements.push_back(std::move(func));
                }
            }
            continue;
        }
        
        // Check for async function
        if (current.type == TOKEN_KW_ASYNC) {
            advance();
            if (current.type == TOKEN_KW_FUNC) {
                auto func = parseFuncDecl();
                if (func) {
                    func->is_async = true;
                    block->statements.push_back(std::move(func));
                }
            }
            continue;
        }
        
        // TODO: struct, type, const, use, mod, extern declarations
        // For now, treat unrecognized top-level as error
        std::stringstream ss;
        ss << "Unexpected token at top level: " << current.value
           << " (type " << current.type << ") at line " << current.line;
        throw std::runtime_error(ss.str());
    }
    
    return block;
}

std::unique_ptr<Statement> Parser::parseDeferStmt() {
    // defer { ... }
    expect(TOKEN_KW_DEFER);
    
    // Parse the defer body as a block
    auto body = parseBlock();
    
    return std::make_unique<DeferStmt>(std::move(body));
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

// Parse variable declaration: type:name = value;
std::unique_ptr<VarDecl> Parser::parseVarDecl() {
    // Current token should be a type
    if (!isTypeToken(current.type)) {
        std::stringstream ss;
        ss << "Expected type token for variable declaration at line " << current.line;
        throw std::runtime_error(ss.str());
    }
    
    std::string type = current.value;
    advance();
    
    // Expect colon
    expect(TOKEN_COLON);
    
    // Expect identifier
    Token name_token = expect(TOKEN_IDENTIFIER);
    std::string name = name_token.value;
    
    // Optional initializer
    std::unique_ptr<Expression> initializer = nullptr;
    if (match(TOKEN_ASSIGN)) {
        initializer = parseExpr();
    }
    
    // Optional semicolon
    match(TOKEN_SEMICOLON);
    
    return std::make_unique<VarDecl>(type, name, std::move(initializer));
}

std::unique_ptr<Statement> Parser::parseStmt() {
    // Handle various statement types
    
    // Variable declaration: type:name = value;
    if (isTypeToken(current.type)) {
        return parseVarDecl();
    }
    
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
    
    // Loop statements
    if (current.type == TOKEN_KW_WHILE) {
        return parseWhileLoop();
    }
    if (current.type == TOKEN_KW_FOR) {
        return parseForLoop();
    }
    if (current.type == TOKEN_KW_TILL) {
        return parseTillLoop();
    }
    if (current.type == TOKEN_KW_WHEN) {
        return parseWhenLoop();
    }
    if (current.type == TOKEN_KW_BREAK) {
        return parseBreak();
    }
    if (current.type == TOKEN_KW_CONTINUE) {
        return parseContinue();
    }
    
    // Pick statement (pattern matching)
    if (current.type == TOKEN_KW_PICK) {
        return parsePickStmt();
    }
    
    // Defer statement
    if (current.type == TOKEN_KW_DEFER) {
        return parseDeferStmt();
    }
    
    // Variable declaration or expression statement
    // Try to parse as expression first
    auto expr = parseExpr();
    match(TOKEN_SEMICOLON);  // Optional semicolon
    return std::make_unique<ExpressionStmt>(std::move(expr));
}

} // namespace frontend
} // namespace aria
