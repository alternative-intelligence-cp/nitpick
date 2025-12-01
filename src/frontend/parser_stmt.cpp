//
// Implementation of the 'defer' statement and General Statement Parsing.
// Integrates LIFO defer stack logic for scope exit cleanup.

#include "parser.h"
#include "ast/control_flow.h"
#include "ast/defer.h"
#include <vector>
#include <memory>

// Parses: defer <statement/expression>;
std::unique_ptr<Stmt> Parser::parseDeferStmt() {
    consume(TOKEN_DEFER, "Expected 'defer'");
    
    // Defer applies to the immediately following statement or expression
    auto stmt = parseStatement();
    
    // The parser adds this to a special list in the current block node
    if (!currentBlock) {
        error("defer must be used inside a block");
    }
    
    // Create Defer Node
    auto deferNode = std::make_unique<DeferStmt>(std::move(stmt));
    
    // Register with current block. The code generator will emit these
    // in reverse order at block exit.
    currentBlock->defers.push_back(deferNode.get());
    
    return deferNode;
}

// Parses a Block {... } and handles the implicit defer injection at the end
std::unique_ptr<Block> Parser::parseBlock() {
    consume(TOKEN_LEFT_BRACE, "Expected '{'");
    
    auto block = std::make_unique<Block>();
    
    // Push Scope
    Block* prevBlock = currentBlock;
    currentBlock = block.get();
    
    while (!check(TOKEN_RIGHT_BRACE) &&!check(TOKEN_EOF)) {
        // Dispatcher for statements
        if (match(TOKEN_DEFER)) {
            block->statements.push_back(parseDeferStmt());
        } else {
            block->statements.push_back(parseDeclaration());
        }
    }
    
    consume(TOKEN_RIGHT_BRACE, "Expected '}'");
    
    // Pop Scope
    currentBlock = prevBlock;
    return block;
}

// Handling 'return' with defers
std::unique_ptr<Stmt> Parser::parseReturnStmt() {
    consume(TOKEN_RETURN, "Expected 'return'");
    
    auto ret = std::make_unique<ReturnStmt>();
    if (!check(TOKEN_SEMICOLON)) {
        ret->value = parseExpression();
    }
    consume(TOKEN_SEMICOLON, "Expected ';'");
    
    // The AST for ReturnStmt will flag the backend to emit defers
    // for all scopes up to the function body.
    return ret;
}

