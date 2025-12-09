// Implementation of Statement Parsing
// Handles: defer statements, block parsing, and loop statements
#include "parser.h"
#include "ast.h"
#include "ast/stmt.h"
#include "ast/defer.h"
#include "ast/loops.h"
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
        
        // Check for struct declaration: const Name = struct { ... }
        if (current.type == TOKEN_KW_CONST || current.type == TOKEN_IDENTIFIER) {
            // Save position to check if this is a struct declaration
            Token saved = current;
            
            // Skip optional const
            if (current.type == TOKEN_KW_CONST) {
                advance();
            }
            
            // Check pattern: Identifier = struct
            if (current.type == TOKEN_IDENTIFIER) {
                Token name = current;
                advance();
                
                if (current.type == TOKEN_ASSIGN) {
                    advance();
                    
                    if (current.type == TOKEN_KW_STRUCT) {
                        // This is a struct declaration! Restore to saved position and parse
                        current = saved;  // Restore position
                        auto structDecl = parseStructDecl();
                        if (structDecl) {
                            block->statements.push_back(std::move(structDecl));
                        }
                        continue;
                    }
                }
            }
            
            // Not a struct, restore position and continue to error handler
            current = saved;
        }
        
        // TODO: type, use, mod, extern declarations
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

// Parse while loop: while (condition) { ... }
std::unique_ptr<Statement> Parser::parseWhileLoop() {
    expect(TOKEN_KW_WHILE);
    expect(TOKEN_LPAREN);
    auto condition = parseExpr();
    expect(TOKEN_RPAREN);
    auto body = parseBlockOrStatement();
    
    return std::make_unique<WhileLoop>(std::move(condition), std::move(body));
}

// Parse for loop: for x in collection { ... }
std::unique_ptr<Statement> Parser::parseForLoop() {
    expect(TOKEN_KW_FOR);
    
    Token iter_token = expect(TOKEN_IDENTIFIER);
    std::string iterator_name = iter_token.value;
    
    expect(TOKEN_KW_IN);
    auto iterable = parseExpr();
    
    auto body = parseBlockOrStatement();
    
    return std::make_unique<ForLoop>(iterator_name, std::move(iterable), std::move(body));
}

// Parse till loop: till(limit, step) { ... }
std::unique_ptr<Statement> Parser::parseTillLoop() {
    expect(TOKEN_KW_TILL);
    expect(TOKEN_LPAREN);
    
    auto limit = parseExpr();
    
    // Step is optional, defaults to 1
    std::unique_ptr<Expression> step = std::make_unique<IntLiteral>(1);
    if (match(TOKEN_COMMA)) {
        step = parseExpr();
    }
    
    expect(TOKEN_RPAREN);
    auto body = parseBlockOrStatement();
    
    return std::make_unique<TillLoop>(std::move(limit), std::move(step), std::move(body));
}

// Parse when loop: when(condition) { ... }
// Note: when is a conditional loop that keeps iterating while condition is true
std::unique_ptr<Statement> Parser::parseWhenLoop() {
    expect(TOKEN_KW_WHEN);
    expect(TOKEN_LPAREN);
    auto condition = parseExpr();
    expect(TOKEN_RPAREN);
    auto body = parseBlockOrStatement();
    
    // For now, treat when loop as while loop (they're semantically similar)
    return std::make_unique<WhileLoop>(std::move(condition), std::move(body));
}

// Parse break statement: break; or break(label);
std::unique_ptr<Statement> Parser::parseBreak() {
    expect(TOKEN_KW_BREAK);
    
    std::string label = "";
    if (match(TOKEN_LPAREN)) {
        Token label_token = expect(TOKEN_IDENTIFIER);
        label = label_token.value;
        expect(TOKEN_RPAREN);
    }
    
    match(TOKEN_SEMICOLON);  // Optional semicolon
    return std::make_unique<BreakStmt>(label);
}

// Parse continue statement: continue; or continue(label);
std::unique_ptr<Statement> Parser::parseContinue() {
    expect(TOKEN_KW_CONTINUE);
    
    std::string label = "";
    if (match(TOKEN_LPAREN)) {
        Token label_token = expect(TOKEN_IDENTIFIER);
        label = label_token.value;
        expect(TOKEN_RPAREN);
    }
    
    match(TOKEN_SEMICOLON);  // Optional semicolon
    return std::make_unique<ContinueStmt>(label);
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
        
        // Check for struct declaration: const Name = struct { ... }
        if (current.type == TOKEN_KW_CONST || current.type == TOKEN_IDENTIFIER) {
            Token saved = current;
            
            if (current.type == TOKEN_KW_CONST) {
                advance();
            }
            
            if (current.type == TOKEN_IDENTIFIER) {
                Token name = current;
                advance();
                
                if (current.type == TOKEN_ASSIGN) {
                    advance();
                    
                    if (current.type == TOKEN_KW_STRUCT) {
                        // This is a struct declaration
                        current = saved;
                        auto structDecl = parseStructDecl();
                        if (structDecl) {
                            block->statements.push_back(std::move(structDecl));
                        }
                        continue;
                    }
                }
            }
            
            // Not a struct, restore and parse as statement
            current = saved;
        }
        
        auto stmt = parseStmt();
        if (stmt) {
            block->statements.push_back(std::move(stmt));
        }
    }
    
    expect(TOKEN_RBRACE);
    return block;
}

// Parse either a block {...} or a single statement
// Used for if/else/while/for bodies to allow one-liner syntax
std::unique_ptr<Block> Parser::parseBlockOrStatement() {
    if (current.type == TOKEN_LBRACE) {
        return parseBlock();
    } else {
        // Single statement - wrap it in a block
        auto block = std::make_unique<Block>();
        auto stmt = parseStmt();
        if (stmt) {
            block->statements.push_back(std::move(stmt));
        }
        return block;
    }
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
    
    // Parse type suffixes (arrays [], pointers @)
    type = parseTypeSuffixes(type);
    
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
    
    // fail(errorCode) - Syntactic sugar for return {err:errorCode, val:0}
    if (match(TOKEN_KW_FAIL)) {
        expect(TOKEN_LPAREN);
        auto errorCode = parseExpr();
        expect(TOKEN_RPAREN);
        match(TOKEN_SEMICOLON);  // Optional semicolon
        
        // Create: {err: errorCode, val: 0}
        auto obj = std::make_unique<ObjectLiteral>();
        
        ObjectLiteral::Field errField;
        errField.name = "err";
        errField.value = std::move(errorCode);
        obj->fields.push_back(std::move(errField));
        
        ObjectLiteral::Field valField;
        valField.name = "val";
        valField.value = std::make_unique<IntLiteral>(0);
        obj->fields.push_back(std::move(valField));
        
        return std::make_unique<ReturnStmt>(std::move(obj));
    }
    
    // pass(value) - Syntactic sugar for return {err:0, val:value}
    if (match(TOKEN_KW_PASS)) {
        expect(TOKEN_LPAREN);
        auto value = parseExpr();
        expect(TOKEN_RPAREN);
        match(TOKEN_SEMICOLON);  // Optional semicolon
        
        // Create: {err: 0, val: value}
        auto obj = std::make_unique<ObjectLiteral>();
        
        ObjectLiteral::Field errField;
        errField.name = "err";
        errField.value = std::make_unique<IntLiteral>(0);
        obj->fields.push_back(std::move(errField));
        
        ObjectLiteral::Field valField;
        valField.name = "val";
        valField.value = std::move(value);
        obj->fields.push_back(std::move(valField));
        
        return std::make_unique<ReturnStmt>(std::move(obj));
    }
    
    // If statement
    if (match(TOKEN_KW_IF)) {
        expect(TOKEN_LPAREN);
        auto condition = parseExpr();
        expect(TOKEN_RPAREN);
        auto then_block = parseBlockOrStatement();
        
        std::unique_ptr<Block> else_block = nullptr;
        if (match(TOKEN_KW_ELSE)) {
            else_block = parseBlockOrStatement();
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
