/**
 * Recursive Descent Parser Implementation
 */

#include "parser/parser.hpp"
#include <iostream>

namespace ariash {
namespace parser {

// ============================================================================
// Token Stream Management
// ============================================================================

const Token& ShellParser::peek(int offset) const {
    size_t index = current_ + offset;
    if (index >= tokens_.size()) {
        return tokens_.back();  // Return EOF
    }
    return tokens_[index];
}

const Token& ShellParser::consume() {
    if (!isAtEnd()) current_++;
    return tokens_[current_ - 1];
}

bool ShellParser::match(TokenType type) {
    if (check(type)) {
        consume();
        return true;
    }
    return false;
}

bool ShellParser::check(TokenType type) const {
    if (isAtEnd()) return false;
    return peek().type == type;
}

void ShellParser::expect(TokenType type, const std::string& message) {
    if (!check(type)) {
        throw ParseError(message, peek().location);
    }
    consume();
}

bool ShellParser::isAtEnd() const {
    return peek().type == TokenType::END_OF_FILE;
}

// ============================================================================
// Disambiguation Helpers
// ============================================================================

bool ShellParser::isKeywordStart() const {
    TokenType t = peek().type;
    return t == TokenType::KW_IF || t == TokenType::KW_WHILE || 
           t == TokenType::KW_FOR || t == TokenType::KW_RETURN;
}

bool ShellParser::isTypeKeyword() const {
    return peek().isType();
}

bool ShellParser::isAssignmentAhead() const {
    // IDENTIFIER followed by =
    return check(TokenType::IDENTIFIER) && peek(1).type == TokenType::ASSIGN;
}

// ============================================================================
// Entry Point
// ============================================================================

std::unique_ptr<Program> ShellParser::parseProgram() {
    auto program = std::make_unique<Program>();
    program->location = peek().location;
    
    while (!isAtEnd()) {
        
        try {
            auto stmt = parseStatement();
            if (stmt) {  // Only add non-null statements
                program->statements.push_back(std::move(stmt));
            }
        } catch (const ParseError& e) {
            std::cerr << e.what() << std::endl;
            // Synchronize: skip to next statement
            while (!isAtEnd() && peek().type != TokenType::SEMICOLON && 
                   peek().type != TokenType::RBRACE) {
                consume();
            }
            if (match(TokenType::SEMICOLON)) continue;
            break;
        }
    }
    
    return program;
}

// ============================================================================
// Statement Parsing - Disambiguation Logic
// ============================================================================

std::unique_ptr<StmtNode> ShellParser::parseStatement() {
    
    // Consume optional leading semicolons
    while (match(TokenType::SEMICOLON)) {}
    
    if (isAtEnd()) return nullptr;
    
    // 1. Keyword check
    if (check(TokenType::KW_IF)) return parseIf();
    if (check(TokenType::KW_WHILE)) return parseWhile();
    if (check(TokenType::KW_FOR)) return parseFor();
    if (check(TokenType::KW_RETURN)) return parseReturn();
    if (check(TokenType::LBRACE)) return parseBlock();
    
    // 2. Type check (variable declaration)
    if (isTypeKeyword()) {
        return parseVarDecl();
    }
    
    // 3. Assignment check
    if (isAssignmentAhead()) {
        std::string varName = consume().lexeme;
        return parseAssignment(varName);
    }
    
    // 4. Expression statement (arithmetic/logical expressions to evaluate)
    // Only treat as expression if there are OPERATORS present
    if (check(TokenType::INTEGER) || check(TokenType::LPAREN)) {
        auto expr = parseExpression();
        match(TokenType::SEMICOLON);
        return std::make_unique<ExprStmt>(std::move(expr), expr->location);
    }
    
    if (check(TokenType::IDENTIFIER)) {
        // Peek ahead to check if this is an expression with operators
        TokenType next = peek(1).type;
        
        // If followed by arithmetic/logical operator, it's an expression
        if (next == TokenType::PLUS || next == TokenType::MINUS ||
            next == TokenType::STAR || next == TokenType::SLASH ||
            next == TokenType::EQ || next == TokenType::NE ||
            next == TokenType::LT || next == TokenType::GT ||
            next == TokenType::LE || next == TokenType::GE ||
            next == TokenType::AND || next == TokenType::OR ||
            next == TokenType::LPAREN) {  // Function call
            // Expression statement
            auto expr = parseExpression();
            match(TokenType::SEMICOLON);
            return std::make_unique<ExprStmt>(std::move(expr), expr->location);
        }
    }
    
    // 5. Default: Treat as command/pipeline
    return parsePipeline();
}

// ============================================================================
// Expression Parsing - Precedence Climbing
// ============================================================================

std::unique_ptr<ExprNode> ShellParser::parseExpression() {
    return parseLogicalOr();
}

std::unique_ptr<ExprNode> ShellParser::parseLogicalOr() {
    auto left = parseLogicalAnd();
    
    while (match(TokenType::OR)) {
        TokenType op = tokens_[current_ - 1].type;
        SourceLocation loc = tokens_[current_ - 1].location;
        auto right = parseLogicalAnd();
        left = std::make_unique<BinaryOpExpr>(op, std::move(left), std::move(right), loc);
    }
    
    return left;
}

std::unique_ptr<ExprNode> ShellParser::parseLogicalAnd() {
    auto left = parseEquality();
    
    while (match(TokenType::AND)) {
        TokenType op = tokens_[current_ - 1].type;
        SourceLocation loc = tokens_[current_ - 1].location;
        auto right = parseEquality();
        left = std::make_unique<BinaryOpExpr>(op, std::move(left), std::move(right), loc);
    }
    
    return left;
}

std::unique_ptr<ExprNode> ShellParser::parseEquality() {
    auto left = parseComparison();
    
    while (match(TokenType::EQ) || match(TokenType::NE)) {
        TokenType op = tokens_[current_ - 1].type;
        SourceLocation loc = tokens_[current_ - 1].location;
        auto right = parseComparison();
        left = std::make_unique<BinaryOpExpr>(op, std::move(left), std::move(right), loc);
    }
    
    return left;
}

std::unique_ptr<ExprNode> ShellParser::parseComparison() {
    auto left = parseAdditive();
    
    while (match(TokenType::LT) || match(TokenType::LE) || 
           match(TokenType::GT) || match(TokenType::GE)) {
        TokenType op = tokens_[current_ - 1].type;
        SourceLocation loc = tokens_[current_ - 1].location;
        auto right = parseAdditive();
        left = std::make_unique<BinaryOpExpr>(op, std::move(left), std::move(right), loc);
    }
    
    return left;
}

std::unique_ptr<ExprNode> ShellParser::parseAdditive() {
    auto left = parseMultiplicative();
    
    while (match(TokenType::PLUS) || match(TokenType::MINUS)) {
        TokenType op = tokens_[current_ - 1].type;
        SourceLocation loc = tokens_[current_ - 1].location;
        auto right = parseMultiplicative();
        left = std::make_unique<BinaryOpExpr>(op, std::move(left), std::move(right), loc);
    }
    
    return left;
}

std::unique_ptr<ExprNode> ShellParser::parseMultiplicative() {
    auto left = parseUnary();
    
    while (match(TokenType::STAR) || match(TokenType::SLASH)) {
        TokenType op = tokens_[current_ - 1].type;
        SourceLocation loc = tokens_[current_ - 1].location;
        auto right = parseUnary();
        left = std::make_unique<BinaryOpExpr>(op, std::move(left), std::move(right), loc);
    }
    
    return left;
}

std::unique_ptr<ExprNode> ShellParser::parseUnary() {
    if (match(TokenType::MINUS) || match(TokenType::NOT)) {
        TokenType op = tokens_[current_ - 1].type;
        SourceLocation loc = tokens_[current_ - 1].location;
        auto operand = parseUnary();
        return std::make_unique<UnaryOpExpr>(op, std::move(operand), loc);
    }
    
    return parsePrimary();
}

std::unique_ptr<ExprNode> ShellParser::parsePrimary() {
    // Integer literal
    if (match(TokenType::INTEGER)) {
        Token tok = tokens_[current_ - 1];
        return std::make_unique<IntegerLiteral>(tok.intValue, tok.location);
    }
    
    // String literal
    if (match(TokenType::STRING)) {
        Token tok = tokens_[current_ - 1];
        return std::make_unique<StringLiteral>(tok.lexeme, tok.location);
    }
    
    // Parenthesized expression
    if (match(TokenType::LPAREN)) {
        auto expr = parseExpression();
        expect(TokenType::RPAREN, "Expected ')' after expression");
        return expr;
    }
    
    // Variable or function call
    if (check(TokenType::IDENTIFIER)) {
        return parseCallOrVariable();
    }
    
    throw ParseError("Expected expression", peek().location);
}

std::unique_ptr<ExprNode> ShellParser::parseCallOrVariable() {
    Token nameToken = consume();
    
    // Function call
    if (match(TokenType::LPAREN)) {
        auto call = std::make_unique<CallExpr>(nameToken.lexeme, nameToken.location);
        
        if (!check(TokenType::RPAREN)) {
            do {
                call->arguments.push_back(parseExpression());
            } while (match(TokenType::COMMA));
        }
        
        expect(TokenType::RPAREN, "Expected ')' after arguments");
        return call;
    }
    
    // Variable
    return std::make_unique<VariableExpr>(nameToken.lexeme, nameToken.location);
}

// ============================================================================
// Statement Parsing - Control Flow
// ============================================================================

std::unique_ptr<BlockStmt> ShellParser::parseBlock() {
    SourceLocation loc = peek().location;
    auto block = std::make_unique<BlockStmt>(loc);
    
    expect(TokenType::LBRACE, "Expected '{'");
    
    while (!check(TokenType::RBRACE) && !isAtEnd()) {
        auto stmt = parseStatement();
        if (stmt) block->statements.push_back(std::move(stmt));
    }
    
    expect(TokenType::RBRACE, "Expected '}'");
    return block;
}

std::unique_ptr<VarDeclStmt> ShellParser::parseVarDecl() {
    SourceLocation loc = peek().location;
    
    // Type
    std::string type = consume().lexeme;
    
    // Variable name
    expect(TokenType::IDENTIFIER, "Expected variable name");
    std::string name = tokens_[current_ - 1].lexeme;
    
    auto decl = std::make_unique<VarDeclStmt>(type, name, loc);
    
    // Optional initializer
    if (match(TokenType::ASSIGN)) {
        decl->initializer = parseExpression();
    }
    
    match(TokenType::SEMICOLON);
    return decl;
}

std::unique_ptr<AssignStmt> ShellParser::parseAssignment(const std::string& varName) {
    SourceLocation loc = peek().location;
    
    expect(TokenType::ASSIGN, "Expected '='");
    auto value = parseExpression();
    
    auto assign = std::make_unique<AssignStmt>(varName, std::move(value), loc);
    
    match(TokenType::SEMICOLON);
    return assign;
}

std::unique_ptr<IfStmt> ShellParser::parseIf() {
    SourceLocation loc = peek().location;
    
    expect(TokenType::KW_IF, "Expected 'if'");
    expect(TokenType::LPAREN, "Expected '(' after 'if'");
    auto condition = parseExpression();
    expect(TokenType::RPAREN, "Expected ')' after condition");
    
    auto thenBranch = parseStatement();
    
    auto ifStmt = std::make_unique<IfStmt>(std::move(condition), std::move(thenBranch), loc);
    
    if (match(TokenType::KW_ELSE)) {
        ifStmt->elseBranch = parseStatement();
    }
    
    return ifStmt;
}

std::unique_ptr<WhileStmt> ShellParser::parseWhile() {
    SourceLocation loc = peek().location;
    
    expect(TokenType::KW_WHILE, "Expected 'while'");
    expect(TokenType::LPAREN, "Expected '(' after 'while'");
    auto condition = parseExpression();
    expect(TokenType::RPAREN, "Expected ')' after condition");
    
    auto body = parseStatement();
    
    return std::make_unique<WhileStmt>(std::move(condition), std::move(body), loc);
}

std::unique_ptr<ForStmt> ShellParser::parseFor() {
    SourceLocation loc = peek().location;
    
    expect(TokenType::KW_FOR, "Expected 'for'");
    expect(TokenType::LPAREN, "Expected '(' after 'for'");
    
    // Loop variable
    expect(TokenType::IDENTIFIER, "Expected loop variable");
    std::string variable = tokens_[current_ - 1].lexeme;
    
    expect(TokenType::KW_IN, "Expected 'in' in for loop");
    
    // Iterable expression
    auto iterable = parseExpression();
    
    expect(TokenType::RPAREN, "Expected ')' after for header");
    
    auto body = parseStatement();
    
    return std::make_unique<ForStmt>(variable, std::move(iterable), std::move(body), loc);
}

std::unique_ptr<ReturnStmt> ShellParser::parseReturn() {
    SourceLocation loc = peek().location;
    
    expect(TokenType::KW_RETURN, "Expected 'return'");
    
    auto ret = std::make_unique<ReturnStmt>(loc);
    
    if (!check(TokenType::SEMICOLON) && !isAtEnd()) {
        ret->value = parseExpression();
    }
    
    match(TokenType::SEMICOLON);
    return ret;
}

// ============================================================================
// Command Parsing - Shell Mode
// ============================================================================

std::unique_ptr<PipelineStmt> ShellParser::parsePipeline() {
    SourceLocation loc = peek().location;
    auto pipeline = std::make_unique<PipelineStmt>(loc);
    
    // Parse first command
    pipeline->commands.push_back(parseCommand());
    
    // Parse pipe chain
    while (match(TokenType::PIPE)) {
        pipeline->commands.push_back(parseCommand());
    }
    
    match(TokenType::SEMICOLON);
    
    // Single command - return it directly cast as pipeline
    return pipeline;
}

std::unique_ptr<CommandStmt> ShellParser::parseCommand() {
    SourceLocation loc = peek().location;
    
    // Command name
    if (!check(TokenType::IDENTIFIER)) {
        throw ParseError("Expected command name", peek().location);
    }
    std::string executable = consume().lexeme;
    
    auto cmd = std::make_unique<CommandStmt>(executable, loc);
    
    // Arguments (any identifier or string, not operators)
    while (check(TokenType::IDENTIFIER) || check(TokenType::STRING) || 
           check(TokenType::INTEGER) || check(TokenType::MINUS)) {
        cmd->arguments.push_back(consume().lexeme);
    }
    
    // Redirections
    cmd->redirections = parseRedirections();
    
    // Background
    if (match(TokenType::BACKGROUND)) {
        cmd->background = true;
    }
    
    return cmd;
}

std::vector<Redirection> ShellParser::parseRedirections() {
    std::vector<Redirection> redirects;
    
    while (check(TokenType::LT) || check(TokenType::GT) || 
           check(TokenType::REDIRECT_APPEND)) {
        Redirection redir;
        
        if (match(TokenType::LT)) {
            redir.type = RedirectionType::INPUT;
        } else if (match(TokenType::GT)) {
            redir.type = RedirectionType::OUTPUT;
        } else if (match(TokenType::REDIRECT_APPEND)) {
            redir.type = RedirectionType::APPEND;
        }
        
        // File descriptor (optional, e.g., 2>)
        // For now, assume default fds (0 for <, 1 for >, 2 for 2>)
        
        // Filename
        if (check(TokenType::STRING) || check(TokenType::IDENTIFIER)) {
            redir.target = consume().lexeme;
        } else {
            throw ParseError("Expected filename after redirection", peek().location);
        }
        
        redirects.push_back(redir);
    }
    
    return redirects;
}

} // namespace parser
} // namespace ariash
