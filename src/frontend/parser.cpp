/**
 * src/frontend/parser.cpp
 *
 * Aria Compiler - Core Parser Implementation
 * Version: 0.0.6
 *
 * Implements the basic parser infrastructure and core parsing methods.
 * This provides the foundation for expression, statement, and declaration parsing.
 */

#include "parser.h"
#include "ast.h"
#include "ast/expr.h"
#include "ast/stmt.h"
#include "tokens.h"
#include <stdexcept>
#include <sstream>

namespace aria {
namespace frontend {

// Constructor
Parser::Parser(AriaLexer& lex) : lexer(lex) {
    advance(); // Load first token
}

Parser::Parser(AriaLexer& lex, ParserContext ctx) : lexer(lex), context(ctx) {
    advance(); // Load first token
}

// Advance to next token
void Parser::advance() {
    current = lexer.nextToken();
}

// Check if current token matches type
bool Parser::match(TokenType type) {
    if (current.type == type) {
        advance();
        return true;
    }
    return false;
}

// Expect a token and advance, or error
Token Parser::expect(TokenType type) {
    if (current.type != type) {
        std::stringstream ss;
        ss << "Expected token type " << type << " but got " << current.type
           << " at line " << current.line << ", col " << current.col;
        throw std::runtime_error(ss.str());
    }
    Token tok = current;
    advance();
    return tok;
}

// =============================================================================
// Expression Parsing (Recursive Descent with Precedence Climbing)
// =============================================================================

// Parse primary expressions: literals, variables, parenthesized expressions
std::unique_ptr<Expression> Parser::parsePrimary() {
    // Integer literal
    if (current.type == TOKEN_INT_LITERAL) {
        int64_t value = std::stoll(current.value);
        advance();
        return std::make_unique<IntLiteral>(value);
    }

    // Identifier (variable reference)
    if (current.type == TOKEN_IDENTIFIER) {
        std::string name = current.value;
        advance();
        return std::make_unique<VarExpr>(name);
    }

    // Parenthesized expression
    if (match(TOKEN_LPAREN)) {
        auto expr = parseExpr();
        expect(TOKEN_RPAREN);
        return expr;
    }

    // Error: unexpected token
    std::stringstream ss;
    ss << "Unexpected token in expression: " << current.value
       << " at line " << current.line;
    throw std::runtime_error(ss.str());
}

// Parse unary expressions: -expr, !expr, ~expr
std::unique_ptr<Expression> Parser::parseUnary() {
    // Unary minus
    if (match(TOKEN_MINUS)) {
        auto operand = parseUnary();
        return std::make_unique<UnaryOp>(UnaryOp::NEG, std::move(operand));
    }

    // Logical not
    if (match(TOKEN_LOGICAL_NOT)) {
        auto operand = parseUnary();
        return std::make_unique<UnaryOp>(UnaryOp::LOGICAL_NOT, std::move(operand));
    }

    // Bitwise not
    if (match(TOKEN_TILDE)) {
        auto operand = parseUnary();
        return std::make_unique<UnaryOp>(UnaryOp::BITWISE_NOT, std::move(operand));
    }

    return parsePrimary();
}

// Parse multiplicative expressions: * / %
std::unique_ptr<Expression> Parser::parseMultiplicative() {
    auto left = parseUnary();

    while (current.type == TOKEN_STAR || current.type == TOKEN_SLASH || current.type == TOKEN_PERCENT) {
        TokenType op = current.type;
        advance();
        auto right = parseUnary();

        BinaryOp::OpType binOp;
        if (op == TOKEN_STAR) binOp = BinaryOp::MUL;
        else if (op == TOKEN_SLASH) binOp = BinaryOp::DIV;
        else binOp = BinaryOp::MOD;

        left = std::make_unique<BinaryOp>(binOp, std::move(left), std::move(right));
    }

    return left;
}

// Parse additive expressions: + -
std::unique_ptr<Expression> Parser::parseAdditive() {
    auto left = parseMultiplicative();

    while (current.type == TOKEN_PLUS || current.type == TOKEN_MINUS) {
        TokenType op = current.type;
        advance();
        auto right = parseMultiplicative();

        BinaryOp::OpType binOp = (op == TOKEN_PLUS) ? BinaryOp::ADD : BinaryOp::SUB;
        left = std::make_unique<BinaryOp>(binOp, std::move(left), std::move(right));
    }

    return left;
}

// Parse shift expressions: << >>
std::unique_ptr<Expression> Parser::parseShift() {
    auto left = parseAdditive();

    while (current.type == TOKEN_LSHIFT || current.type == TOKEN_RSHIFT) {
        TokenType op = current.type;
        advance();
        auto right = parseAdditive();

        BinaryOp::OpType binOp = (op == TOKEN_LSHIFT) ? BinaryOp::LSHIFT : BinaryOp::RSHIFT;
        left = std::make_unique<BinaryOp>(binOp, std::move(left), std::move(right));
    }

    return left;
}

// Parse relational expressions: < > <= >=
std::unique_ptr<Expression> Parser::parseRelational() {
    auto left = parseShift();

    while (current.type == TOKEN_LT || current.type == TOKEN_GT ||
           current.type == TOKEN_LE || current.type == TOKEN_GE) {
        TokenType op = current.type;
        advance();
        auto right = parseShift();

        BinaryOp::OpType binOp;
        if (op == TOKEN_LT) binOp = BinaryOp::LT;
        else if (op == TOKEN_GT) binOp = BinaryOp::GT;
        else if (op == TOKEN_LE) binOp = BinaryOp::LE;
        else binOp = BinaryOp::GE;

        left = std::make_unique<BinaryOp>(binOp, std::move(left), std::move(right));
    }

    return left;
}

// Parse equality expressions: == !=
std::unique_ptr<Expression> Parser::parseEquality() {
    auto left = parseRelational();

    while (current.type == TOKEN_EQ || current.type == TOKEN_NE) {
        TokenType op = current.type;
        advance();
        auto right = parseRelational();

        BinaryOp::OpType binOp = (op == TOKEN_EQ) ? BinaryOp::EQ : BinaryOp::NE;
        left = std::make_unique<BinaryOp>(binOp, std::move(left), std::move(right));
    }

    return left;
}

// Parse bitwise AND expressions: &
std::unique_ptr<Expression> Parser::parseBitwiseAnd() {
    auto left = parseEquality();

    while (match(TOKEN_AMPERSAND)) {
        auto right = parseEquality();
        left = std::make_unique<BinaryOp>(BinaryOp::BITWISE_AND, std::move(left), std::move(right));
    }

    return left;
}

// Parse bitwise XOR expressions: ^
std::unique_ptr<Expression> Parser::parseBitwiseXor() {
    auto left = parseBitwiseAnd();

    while (match(TOKEN_CARET)) {
        auto right = parseBitwiseAnd();
        left = std::make_unique<BinaryOp>(BinaryOp::BITWISE_XOR, std::move(left), std::move(right));
    }

    return left;
}

// Parse bitwise OR expressions: |
std::unique_ptr<Expression> Parser::parseBitwiseOr() {
    auto left = parseBitwiseXor();

    while (match(TOKEN_PIPE)) {
        auto right = parseBitwiseXor();
        left = std::make_unique<BinaryOp>(BinaryOp::BITWISE_OR, std::move(left), std::move(right));
    }

    return left;
}

// Parse logical AND expressions: &&
std::unique_ptr<Expression> Parser::parseLogicalAnd() {
    auto left = parseBitwiseOr();

    while (match(TOKEN_LOGICAL_AND)) {
        auto right = parseBitwiseOr();
        left = std::make_unique<BinaryOp>(BinaryOp::LOGICAL_AND, std::move(left), std::move(right));
    }

    return left;
}

// Parse logical OR expressions: ||
std::unique_ptr<Expression> Parser::parseLogicalOr() {
    auto left = parseLogicalAnd();

    while (match(TOKEN_LOGICAL_OR)) {
        auto right = parseLogicalAnd();
        left = std::make_unique<BinaryOp>(BinaryOp::LOGICAL_OR, std::move(left), std::move(right));
    }

    return left;
}

// Top-level expression parser
std::unique_ptr<Expression> Parser::parseExpr() {
    return parseLogicalOr();
}

// =============================================================================
// Statement Parsing
// =============================================================================

// Parse a single statement
std::unique_ptr<Statement> Parser::parseStmt() {
    // Return statement
    if (match(TOKEN_KW_RETURN)) {
        std::unique_ptr<Expression> expr = nullptr;
        if (current.type != TOKEN_SEMICOLON) {
            expr = parseExpr();
        }
        expect(TOKEN_SEMICOLON);
        return std::make_unique<ReturnStmt>(std::move(expr));
    }

    // Variable declaration: type name = expr;
    if (current.type >= TOKEN_TYPE_VOID && current.type <= TOKEN_TYPE_STRING) {
        return parseVarDecl();
    }

    // Defer statement
    if (current.type == TOKEN_KW_DEFER) {
        return parseDeferStmt();
    }

    // If statement
    if (match(TOKEN_KW_IF)) {
        expect(TOKEN_LPAREN);
        auto condition = parseExpr();
        expect(TOKEN_RPAREN);
        auto thenBranch = parseBlock();
        std::unique_ptr<Block> elseBranch = nullptr;
        if (match(TOKEN_KW_ELSE)) {
            elseBranch = parseBlock();
        }
        return std::make_unique<IfStmt>(std::move(condition), std::move(thenBranch), std::move(elseBranch));
    }

    // Pick statement
    if (current.type == TOKEN_KW_PICK) {
        return parsePickStmt();
    }

    // Expression statement (e.g., function call)
    auto expr = parseExpr();
    expect(TOKEN_SEMICOLON);
    return std::make_unique<ExpressionStmt>(std::move(expr));
}

// Parse variable declaration: type name = value;
std::unique_ptr<VarDecl> Parser::parseVarDecl() {
    // Type
    Token typeToken = current;
    advance();

    // Name
    Token nameToken = expect(TOKEN_IDENTIFIER);

    // Initializer
    std::unique_ptr<Expression> init = nullptr;
    if (match(TOKEN_ASSIGN)) {
        init = parseExpr();
    }

    expect(TOKEN_SEMICOLON);

    return std::make_unique<VarDecl>(typeToken.value, nameToken.value, std::move(init));
}

// Parse defer statement: defer { ... }
std::unique_ptr<Statement> Parser::parseDeferStmt() {
    expect(TOKEN_KW_DEFER);
    auto body = parseBlock();
    return std::make_unique<DeferStmt>(std::move(body));
}

// Parse pick statement (pattern matching)
std::unique_ptr<PickStmt> Parser::parsePickStmt() {
    expect(TOKEN_KW_PICK);
    expect(TOKEN_LPAREN);
    auto expr = parseExpr();
    expect(TOKEN_RPAREN);
    expect(TOKEN_LBRACE);

    auto pickStmt = std::make_unique<PickStmt>(std::move(expr));

    // Parse cases until we hit closing brace
    while (current.type != TOKEN_RBRACE && current.type != TOKEN_EOF) {
        // Parse pattern (for now, just integer literals)
        auto pattern = parseExpr();
        expect(TOKEN_COLON);

        // Parse body (must be a block or single statement)
        std::unique_ptr<Block> caseBody;
        if (current.type == TOKEN_LBRACE) {
            caseBody = parseBlock();
        } else {
            // Wrap single statement in a block
            caseBody = std::make_unique<Block>();
            caseBody->statements.push_back(parseStmt());
        }

        // Create and add the case
        PickCase pickCase(PickCase::EXACT, std::move(caseBody));
        pickCase.value_start = std::move(pattern);
        pickStmt->cases.push_back(std::move(pickCase));
    }

    expect(TOKEN_RBRACE);
    return pickStmt;
}

// Parse a block: { statement; statement; ... }
std::unique_ptr<Block> Parser::parseBlock() {
    auto block = std::make_unique<Block>();

    // If block starts with {, consume it
    bool hasBraces = match(TOKEN_LBRACE);

    // Parse statements until } or EOF
    while (current.type != TOKEN_EOF) {
        // If we have braces, stop at closing brace
        if (hasBraces && current.type == TOKEN_RBRACE) {
            break;
        }

        // If no braces (top-level), parse all statements
        if (!hasBraces && current.type == TOKEN_EOF) {
            break;
        }

        try {
            auto stmt = parseStmt();
            block->statements.push_back(std::move(stmt));
        } catch (const std::exception& e) {
            // On parse error, skip to next statement
            // For now, just rethrow
            throw;
        }
    }

    // Consume closing brace if present
    if (hasBraces) {
        expect(TOKEN_RBRACE);
    }

    return block;
}

} // namespace frontend
} // namespace aria
