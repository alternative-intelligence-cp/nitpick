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

// Consume token with custom error message
Token Parser::consume(TokenType type, const std::string& message) {
    if (current.type != type) {
        std::stringstream ss;
        ss << message << " (expected token type " << type << " but got " << current.type
           << ") at line " << current.line << ", col " << current.col;
        throw std::runtime_error(ss.str());
    }
    Token tok = current;
    advance();
    return tok;
}

// Check if current token matches type without consuming
bool Parser::check(TokenType type) {
    return current.type == type;
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

    // String literal
    if (current.type == TOKEN_STRING_LITERAL) {
        std::string value = current.value;
        advance();
        return std::make_unique<StringLiteral>(value);
    }

    // Template string literal with interpolation: `text &{expr} more`
    if (current.type == TOKEN_BACKTICK) {
        return parseTemplateString();
    }

    // Boolean literals
    if (current.type == TOKEN_KW_TRUE) {
        advance();
        return std::make_unique<BoolLiteral>(true);
    }
    if (current.type == TOKEN_KW_FALSE) {
        advance();
        return std::make_unique<BoolLiteral>(false);
    }

    // Identifier (variable reference or function call)
    if (current.type == TOKEN_IDENTIFIER) {
        std::string name = current.value;
        advance();
        
        // Check for function call: identifier(args)
        if (current.type == TOKEN_LPAREN) {
            advance(); // consume (
            
            auto call = std::make_unique<CallExpr>(name);
            
            // Parse arguments
            while (current.type != TOKEN_RPAREN && current.type != TOKEN_EOF) {
                call->arguments.push_back(parseExpr());
                
                if (!match(TOKEN_COMMA)) {
                    break;
                }
            }
            
            expect(TOKEN_RPAREN);
            return call;
        }
        
        // Just a variable reference
        return std::make_unique<VarExpr>(name);
    }

    // Parenthesized expression
    if (match(TOKEN_LPAREN)) {
        auto expr = parseExpr();
        expect(TOKEN_RPAREN);
        return expr;
    }
    
    // Array literal: [1, 2, 3]
    if (match(TOKEN_LBRACKET)) {
        auto array_lit = std::make_unique<ArrayLiteral>();
        
        // Parse comma-separated elements until ]
        while (current.type != TOKEN_RBRACKET && current.type != TOKEN_EOF) {
            array_lit->elements.push_back(parseExpr());
            
            if (!match(TOKEN_COMMA)) {
                break;
            }
        }
        
        expect(TOKEN_RBRACKET);
        return array_lit;
    }
    
    // Object literal: { field: value, field: value, ... }
    if (match(TOKEN_LBRACE)) {
        auto obj_lit = std::make_unique<ObjectLiteral>();
        
        // Parse comma-separated field:value pairs until }
        while (current.type != TOKEN_RBRACE && current.type != TOKEN_EOF) {
            // Parse field name
            Token field_name = expect(TOKEN_IDENTIFIER);
            
            // Expect colon
            expect(TOKEN_COLON);
            
            // Parse field value
            auto field_value = parseExpr();
            
            obj_lit->fields.push_back({field_name.value, std::move(field_value)});
            
            if (!match(TOKEN_COMMA)) {
                break;
            }
        }
        
        expect(TOKEN_RBRACE);
        return obj_lit;
    }
    
    // Lambda Expression: returnType(params) { body } or returnType(params){body}(args)
    // SPEC: func:name = returnType(params) { return { err:NULL, val:value }; };
    // Check if current token is a type followed by (
    if (isTypeToken(current.type)) {
        // Lookahead to see if this is a lambda (type followed by LPAREN)
        Token type_token = current;
        
        // Try to parse as lambda
        std::string return_type = current.value;
        advance();  // consume type token
        
        if (current.type == TOKEN_LPAREN) {
            // This is a lambda! Parse it
            auto params = parseParams();
            
            // Parse lambda body
            auto body = parseBlock();
            
            auto lambda = std::make_unique<LambdaExpr>(return_type, std::move(params), std::move(body));
            
            // Check for immediate invocation: lambda(args)
            if (current.type == TOKEN_LPAREN) {
                lambda->is_immediately_invoked = true;
                advance();  // consume (
                
                // Parse arguments
                while (current.type != TOKEN_RPAREN && current.type != TOKEN_EOF) {
                    lambda->call_arguments.push_back(parseExpr());
                    
                    if (!match(TOKEN_COMMA)) {
                        break;
                    }
                }
                
                expect(TOKEN_RPAREN);
            }
            
            return lambda;
        } else {
            // Not a lambda, this was a mistake - we need to handle this case
            // This shouldn't happen in valid Aria code, but let's throw an error
            std::stringstream ss;
            ss << "Unexpected token after type identifier: " << current.value
               << " at line " << current.line << " (expected '(' for lambda or variable declaration)";
            throw std::runtime_error(ss.str());
        }
    }
    
    // Await expression (Bug #70)
    if (match(TOKEN_KW_AWAIT)) {
        auto expr = parseUnary();  // Parse the expression to await
        return std::make_unique<AwaitExpr>(std::move(expr));
    }

    // Error: unexpected token
    std::stringstream ss;
    ss << "Unexpected token in expression: " << current.value
       << " at line " << current.line;
    throw std::runtime_error(ss.str());
}

// Parse postfix expressions: expr++, expr--
std::unique_ptr<Expression> Parser::parsePostfix() {
    auto expr = parsePrimary();
    
    // Loop to handle chained postfix operations: obj.field, obj.field++, array[index].field, etc.
    while (true) {
        // Handle member access: obj.field or obj?.field (safe navigation)
        if (current.type == TOKEN_DOT || current.type == TOKEN_SAFE_NAV) {
            bool is_safe = (current.type == TOKEN_SAFE_NAV);
            advance();  // consume . or ?.
            
            Token member_name = expect(TOKEN_IDENTIFIER);
            expr = std::make_unique<MemberAccess>(std::move(expr), member_name.value, is_safe);
            continue;
        }
        
        // Handle postfix increment
        if (match(TOKEN_INCREMENT)) {
            expr = std::make_unique<UnaryOp>(UnaryOp::POST_INC, std::move(expr));
            continue;
        }
        
        // Handle postfix decrement
        if (match(TOKEN_DECREMENT)) {
            expr = std::make_unique<UnaryOp>(UnaryOp::POST_DEC, std::move(expr));
            continue;
        }
        
        // No more postfix operations
        break;
    }
    
    return expr;
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

    // Address-of operator (@)
    if (match(TOKEN_AT)) {
        auto operand = parseUnary();
        return std::make_unique<UnaryOp>(UnaryOp::ADDRESS_OF, std::move(operand));
    }

    // Pin operator (#)
    if (match(TOKEN_HASH)) {
        auto operand = parseUnary();
        return std::make_unique<UnaryOp>(UnaryOp::PIN, std::move(operand));
    }

    return parsePostfix();
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

// Parse ternary expressions: is condition : true_expr : false_expr
std::unique_ptr<Expression> Parser::parseTernary() {
    // Check for 'is' ternary operator at the beginning
    if (match(TOKEN_KW_IS)) {
        auto condition = parseLogicalOr();
        expect(TOKEN_COLON);
        auto true_expr = parseLogicalOr();
        expect(TOKEN_COLON);
        auto false_expr = parseTernary();  // Right-associative
        return std::make_unique<TernaryExpr>(std::move(condition), std::move(true_expr), std::move(false_expr));
    }
    
    // Parse base expression
    auto expr = parseLogicalOr();
    
    // Check for unwrap operator: expr ? default
    if (match(TOKEN_QUESTION)) {
        auto default_value = parseLogicalOr();
        return std::make_unique<UnwrapExpr>(std::move(expr), std::move(default_value));
    }
    
    return expr;
}

// Parse assignment expressions (lowest precedence, right-associative)
// Handles: identifier = expr, identifier += expr, etc.
std::unique_ptr<Expression> Parser::parseAssignment() {
    auto left = parseTernary();
    
    // Check for assignment operators
    if (match(TOKEN_ASSIGN)) {
        auto right = parseAssignment();  // Right-associative
        return std::make_unique<BinaryOp>(BinaryOp::ASSIGN, std::move(left), std::move(right));
    }
    
    if (match(TOKEN_PLUS_ASSIGN)) {
        auto right = parseAssignment();
        return std::make_unique<BinaryOp>(BinaryOp::PLUS_ASSIGN, std::move(left), std::move(right));
    }
    
    if (match(TOKEN_MINUS_ASSIGN)) {
        auto right = parseAssignment();
        return std::make_unique<BinaryOp>(BinaryOp::MINUS_ASSIGN, std::move(left), std::move(right));
    }
    
    if (match(TOKEN_STAR_ASSIGN)) {
        auto right = parseAssignment();
        return std::make_unique<BinaryOp>(BinaryOp::STAR_ASSIGN, std::move(left), std::move(right));
    }
    
    if (match(TOKEN_SLASH_ASSIGN)) {
        auto right = parseAssignment();
        return std::make_unique<BinaryOp>(BinaryOp::SLASH_ASSIGN, std::move(left), std::move(right));
    }
    
    if (match(TOKEN_MOD_ASSIGN)) {
        auto right = parseAssignment();
        return std::make_unique<BinaryOp>(BinaryOp::MOD_ASSIGN, std::move(left), std::move(right));
    }
    
    return left;
}

// Top-level expression parser
std::unique_ptr<Expression> Parser::parseExpr() {
    return parseAssignment();
}

// =============================================================================
// Program and Block Parsing
// =============================================================================

// Parse top-level program (file contents without { } wrapper)
// SPEC: All functions are lambdas assigned to func-type variables
// Example: func:add = (int32:a, int32:b) { return a + b };
std::unique_ptr<Block> Parser::parseProgram() {
    auto block = std::make_unique<Block>();
    
    // Parse top-level declarations until EOF
    while (current.type != TOKEN_EOF) {
        // Skip any stray semicolons
        if (match(TOKEN_SEMICOLON)) {
            continue;
        }
        
        // Global variable declarations: [const|wild|stack] type:name = value;
        // This includes func:name = (params) { body }; (lambdas)
        if (current.type == TOKEN_KW_CONST || current.type == TOKEN_KW_WILD || current.type == TOKEN_KW_STACK) {
            block->statements.push_back(parseVarDecl());
            continue;
        }
        
        // Type token - variable declaration (including func-type for lambdas)
        if (isTypeToken(current.type)) {
            block->statements.push_back(parseVarDecl());
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

    // Variable declaration: [const|wild|stack] type:name = expr;
    // BUT: type( is a lambda expression, not a variable declaration!
    if (current.type == TOKEN_KW_CONST || current.type == TOKEN_KW_WILD || 
        current.type == TOKEN_KW_STACK) {
        return parseVarDecl();
    }
    
    // Check if this is a type token - could be var decl OR lambda
    if (current.type >= TOKEN_TYPE_VOID && current.type <= TOKEN_TYPE_STRING) {
        // Lookahead: if next token is '(', this is a lambda expression statement
        // Save current position
        Token saved = current;
        advance();
        
        if (current.type == TOKEN_LPAREN) {
            // This is a lambda! Backtrack and parse as expression statement
            // Put the type token back (simple backtrack)
            // Actually we can't backtrack easily, so just handle it differently
            // Let's NOT advance - check the NEXT token without advancing
            // We already advanced, so current is now the token AFTER the type
            // We need to go back - but parser doesn't support backtracking
            // So let's change approach: just check without consuming
            // 
            // Problem: we already called advance()! 
            // Solution: Create a temp parser state or handle both cases
            // For now, let's just parse the lambda manually here
            
            // We've consumed the type token and seen LPAREN
            // current is now LPAREN
            // saved has the type token
            
            // Parse as lambda expression statement
            std::string return_type = saved.value;
            auto params = parseParams();  // This will consume the ( and params and )
            auto body = parseBlock();
            
            auto lambda = std::make_unique<LambdaExpr>(return_type, std::move(params), std::move(body));
            
            // Check for immediate invocation
            if (current.type == TOKEN_LPAREN) {
                lambda->is_immediately_invoked = true;
                advance();  // consume (
                
                while (current.type != TOKEN_RPAREN && current.type != TOKEN_EOF) {
                    lambda->call_arguments.push_back(parseExpr());
                    if (!match(TOKEN_COMMA)) break;
                }
                
                expect(TOKEN_RPAREN);
            }
            
            expect(TOKEN_SEMICOLON);
            return std::make_unique<ExpressionStmt>(std::move(lambda));
        } else {
            // Not a lambda - it's a variable declaration
            // We've advanced past the type token, current is NOT '('
            // We need to handle this as a variable declaration
            // But parseVarDecl expects to START at the type token
            // So we can't call it directly
            
            // Parse array/pointer suffixes
            std::string fullType = parseTypeSuffixes(saved.value);
            
            // Check if current is ':'
            if (current.type == TOKEN_COLON) {
                // Variable declaration: we already consumed the type
                advance();  // consume :
                
                Token name_tok = expect(TOKEN_IDENTIFIER);
                
                std::unique_ptr<Expression> init = nullptr;
                if (match(TOKEN_ASSIGN)) {
                    init = parseExpr();
                }
                
                expect(TOKEN_SEMICOLON);
                
                return std::make_unique<VarDecl>(fullType, name_tok.value, std::move(init));
            } else {
                std::stringstream ss;
                ss << "Expected ':' or '(' after type token at line " << current.line;
                throw std::runtime_error(ss.str());
            }
        }
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
    
    // For loop (Bug #67)
    if (current.type == TOKEN_KW_FOR) {
        return parseForLoop();
    }
    
    // While loop (Bug #68)
    if (current.type == TOKEN_KW_WHILE) {
        return parseWhileLoop();
    }
    
    // Till loop (Spec 8.2: Automatic iterator with $)
    if (current.type == TOKEN_KW_TILL) {
        return parseTillLoop();
    }
    
    // When loop (Spec 8.2: Loop with completion blocks)
    if (current.type == TOKEN_KW_WHEN) {
        return parseWhenLoop();
    }
    
    // Break statement (Bug #71)
    if (current.type == TOKEN_KW_BREAK) {
        auto stmt = parseBreak();
        expect(TOKEN_SEMICOLON);
        return stmt;
    }
    
    // Continue statement (Bug #71)
    if (current.type == TOKEN_KW_CONTINUE) {
        auto stmt = parseContinue();
        expect(TOKEN_SEMICOLON);
        return stmt;
    }
    
    // Use statement (Bug #73)
    if (current.type == TOKEN_KW_USE) {
        auto stmt = parseUseStmt();
        expect(TOKEN_SEMICOLON);
        return stmt;
    }
    
    // Extern block (Bug #74)
    if (current.type == TOKEN_KW_EXTERN) {
        return parseExternBlock();
    }
    
    // Module definition (Bug #75)
    if (current.type == TOKEN_KW_MOD) {
        return parseModDef();
    }
    
    // Fall statement (Bug #66)
    if (current.type == TOKEN_KW_FALL) {
        auto stmt = parseFallStmt();
        expect(TOKEN_SEMICOLON);
        return stmt;
    }
    
    // Note: Functions are lambdas (func:name = (params) { body };)
    // No special function declaration syntax needed

    // Expression statement (e.g., function call)
    auto expr = parseExpr();
    expect(TOKEN_SEMICOLON);
    return std::make_unique<ExpressionStmt>(std::move(expr));
}

// Parse variable declaration: [const|wild|stack] type:name = value;
std::unique_ptr<VarDecl> Parser::parseVarDecl() {
    // Optional const/wild/stack prefix
    bool is_const = false;
    bool is_wild = false;
    bool is_stack = false;
    
    if (current.type == TOKEN_KW_CONST) {
        is_const = true;
        advance();
    } else if (current.type == TOKEN_KW_WILD) {
        is_wild = true;
        advance();
    } else if (current.type == TOKEN_KW_STACK) {
        is_stack = true;
        advance();
    }
    
    // Type (with optional array/pointer suffix)
    Token typeToken = current;
    advance();
    std::string fullType = parseTypeSuffixes(typeToken.value);

    // Colon (Aria syntax: type:name or type[]:name or type@:name)
    expect(TOKEN_COLON);

    // Name
    Token nameToken = expect(TOKEN_IDENTIFIER);

    // Initializer
    std::unique_ptr<Expression> init = nullptr;
    if (match(TOKEN_ASSIGN)) {
        init = parseExpr();
    }

    expect(TOKEN_SEMICOLON);

    auto varDecl = std::make_unique<VarDecl>(fullType, nameToken.value, std::move(init));
    varDecl->is_const = is_const;
    varDecl->is_wild = is_wild;
    varDecl->is_stack = is_stack;
    
    return varDecl;
}

// Parse defer statement: defer { ... }
std::unique_ptr<Statement> Parser::parseDeferStmt() {
    expect(TOKEN_KW_DEFER);
    auto body = parseBlock();
    return std::make_unique<DeferStmt>(std::move(body));
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

// =============================================================================
// Control Flow Parsing (Bug #67-71)
// =============================================================================

// Parse for loop: for x in iterable { ... }
std::unique_ptr<Statement> Parser::parseForLoop() {
    expect(TOKEN_KW_FOR);
    
    // Parse iterator variable name
    Token iter_tok = expect(TOKEN_IDENTIFIER);
    std::string iterator_name = iter_tok.value;
    
    // Expect 'in' keyword
    expect(TOKEN_KW_IN);
    
    // Parse iterable expression
    auto iterable = parseExpr();
    
    // Parse body block
    auto body = parseBlock();
    
    return std::make_unique<ForLoop>(iterator_name, std::move(iterable), std::move(body));
}

// Parse while loop: while condition { ... }
std::unique_ptr<Statement> Parser::parseWhileLoop() {
    expect(TOKEN_KW_WHILE);
    
    // Parse condition expression
    auto condition = parseExpr();
    
    // Parse body block
    auto body = parseBlock();
    
    return std::make_unique<WhileLoop>(std::move(condition), std::move(body));
}

// Parse when loop: when(condition) { body } then { success } end { failure }
// Spec Section 8.2: Loop with completion blocks
std::unique_ptr<Statement> Parser::parseWhenLoop() {
    expect(TOKEN_KW_WHEN);
    expect(TOKEN_LPAREN);
    
    // Parse condition expression
    auto condition = parseExpr();
    
    expect(TOKEN_RPAREN);
    
    // Parse main loop body
    auto body = parseBlock();
    
    // Parse optional 'then' block (runs after successful completion)
    std::unique_ptr<Block> then_block = nullptr;
    if (match(TOKEN_KW_THEN)) {
        then_block = parseBlock();
    }
    
    // Parse optional 'end' block (runs if loop didn't run or broke early)
    std::unique_ptr<Block> end_block = nullptr;
    if (match(TOKEN_KW_END)) {
        end_block = parseBlock();
    }
    
    return std::make_unique<WhenLoop>(std::move(condition), std::move(body), 
                                       std::move(then_block), std::move(end_block));
}

// Parse till loop: till(max, step) { body }
// Spec Section 8.2: Automatic iterator with $ variable
// Positive step: counts from 0 to max
// Negative step: counts from max to 0
std::unique_ptr<Statement> Parser::parseTillLoop() {
    expect(TOKEN_KW_TILL);
    expect(TOKEN_LPAREN);
    
    // Parse limit expression
    auto limit = parseExpr();
    
    expect(TOKEN_COMMA);
    
    // Parse step expression
    auto step = parseExpr();
    
    expect(TOKEN_RPAREN);
    
    // Parse loop body
    auto body = parseBlock();
    
    return std::make_unique<TillLoop>(std::move(limit), std::move(step), std::move(body));
}

// =============================================================================
// Pattern Matching: pick/fall (Spec Section 8.3)
// =============================================================================

// Parse pick statement: pick(expr) { cases... }
// Spec Section 8.3: Pattern Matching with Fallthrough
std::unique_ptr<PickStmt> Parser::parsePickStmt() {
    expect(TOKEN_KW_PICK);
    expect(TOKEN_LPAREN);
    
    // Parse selector expression
    auto selector = parseExpr();
    
    expect(TOKEN_RPAREN);
    expect(TOKEN_LBRACE);
    
    auto pick = std::make_unique<PickStmt>(std::move(selector));
    
    // Parse cases until closing brace
    while (current.type != TOKEN_RBRACE && current.type != TOKEN_EOF) {
        // Check for labeled case: label:(!)
        std::string label;
        if (current.type == TOKEN_IDENTIFIER) {
            // Look ahead for label syntax
            Token maybe_label = current;
            advance();
            
            if (current.type == TOKEN_COLON) {
                // It's a label
                label = maybe_label.value;
                advance();  // consume colon
                
                // Expect (!) for labeled unreachable case
                expect(TOKEN_LPAREN);
                expect(TOKEN_LOGICAL_NOT);
                expect(TOKEN_RPAREN);
                
                // Parse body
                auto body = parseBlock();
                
                PickCase pcase(PickCase::UNREACHABLE, std::move(body));
                pcase.label = label;
                pick->cases.push_back(std::move(pcase));
                
                // Optional comma
                match(TOKEN_COMMA);
                continue;
            } else {
                // Not a label, we need to reparse this as a case
                // Put the token back and reparse properly
                // For simplicity, error for now - labels must be explicit
                throw std::runtime_error("Expected ':' after identifier in pick statement");
            }
        }
        
        // Parse regular case: (pattern) { body }
        expect(TOKEN_LPAREN);
        
        PickCase::CaseType case_type;
        std::unique_ptr<Expression> value_start;
        std::unique_ptr<Expression> value_end;
        bool is_range_exclusive = false;
        
        // Check for comparison operators
        if (match(TOKEN_LT)) {
            // (<expr) - less than
            case_type = PickCase::LESS_THAN;
            value_start = parseExpr();
        } 
        else if (match(TOKEN_GT)) {
            // (>expr) - greater than
            case_type = PickCase::GREATER_THAN;
            value_start = parseExpr();
        }
        else if (match(TOKEN_LE)) {
            // (<=expr) - less or equal
            case_type = PickCase::LESS_EQUAL;
            value_start = parseExpr();
        }
        else if (match(TOKEN_GE)) {
            // (>=expr) - greater or equal
            case_type = PickCase::GREATER_EQUAL;
            value_start = parseExpr();
        }
        else if (match(TOKEN_STAR)) {
            // (*) - wildcard (default case)
            case_type = PickCase::WILDCARD;
        }
        else {
            // Exact match or range
            value_start = parseExpr();
            
            // Check for range: (start..end) or (start...end)
            if (current.type == TOKEN_DOT) {
                advance();
                if (match(TOKEN_DOT)) {
                    // Inclusive range: start..end
                    if (match(TOKEN_DOT)) {
                        // Exclusive range: start...end
                        is_range_exclusive = true;
                    }
                    case_type = PickCase::RANGE;
                    value_end = parseExpr();
                } else {
                    throw std::runtime_error("Invalid range syntax in pick case");
                }
            } else {
                // Exact match
                case_type = PickCase::EXACT;
            }
        }
        
        expect(TOKEN_RPAREN);
        
        // Parse case body
        auto body = parseBlock();
        
        // Create case
        PickCase pcase(case_type, std::move(body));
        pcase.value_start = std::move(value_start);
        pcase.value_end = std::move(value_end);
        pcase.is_range_exclusive = is_range_exclusive;
        
        pick->cases.push_back(std::move(pcase));
        
        // Optional comma between cases
        match(TOKEN_COMMA);
    }
    
    expect(TOKEN_RBRACE);
    
    return pick;
}

// Parse fall statement: fall(label);
// Spec Section 8.3: Explicit fallthrough in pick
std::unique_ptr<Statement> Parser::parseFallStmt() {
    expect(TOKEN_KW_FALL);
    expect(TOKEN_LPAREN);
    
    Token label_tok = expect(TOKEN_IDENTIFIER);
    std::string target_label = label_tok.value;
    
    expect(TOKEN_RPAREN);
    
    return std::make_unique<FallStmt>(target_label);
}

// Parse break statement: break; or break(label);
std::unique_ptr<Statement> Parser::parseBreak() {
    expect(TOKEN_KW_BREAK);
    
    std::string label;
    
    // Check for optional label in parentheses
    if (match(TOKEN_LPAREN)) {
        Token label_tok = expect(TOKEN_IDENTIFIER);
        label = label_tok.value;
        expect(TOKEN_RPAREN);
    }
    
    return std::make_unique<BreakStmt>(label);
}

// Parse continue statement: continue; or continue(label);
std::unique_ptr<Statement> Parser::parseContinue() {
    expect(TOKEN_KW_CONTINUE);
    
    std::string label;
    
    // Check for optional label in parentheses
    if (match(TOKEN_LPAREN)) {
        Token label_tok = expect(TOKEN_IDENTIFIER);
        label = label_tok.value;
        expect(TOKEN_RPAREN);
    }
    
    return std::make_unique<ContinueStmt>(label);
}

// =============================================================================
// Module System Parsing (Bug #73-75)
// =============================================================================

// Parse use statement: use module.path; or use module.{item1, item2};
std::unique_ptr<Statement> Parser::parseUseStmt() {
    expect(TOKEN_KW_USE);
    
    // Parse module path (e.g., std.io)
    std::string module_path;
    Token first = expect(TOKEN_IDENTIFIER);
    module_path = first.value;
    
    // Handle dotted path
    while (match(TOKEN_DOT)) {
        if (current.type == TOKEN_LBRACE) {
            break;  // Start of selective imports
        }
        Token part = expect(TOKEN_IDENTIFIER);
        module_path += "." + part.value;
    }
    
    // Check for selective imports: use mod.{a, b, c}
    std::vector<std::string> imports;
    if (match(TOKEN_LBRACE)) {
        while (current.type != TOKEN_RBRACE && current.type != TOKEN_EOF) {
            Token item = expect(TOKEN_IDENTIFIER);
            imports.push_back(item.value);
            
            if (!match(TOKEN_COMMA)) {
                break;
            }
        }
        expect(TOKEN_RBRACE);
    }
    
    return std::make_unique<UseStmt>(module_path, imports);
}

// Parse extern block: extern { fn declarations... }
std::unique_ptr<Statement> Parser::parseExternBlock() {
    expect(TOKEN_KW_EXTERN);
    expect(TOKEN_LBRACE);
    
    auto extern_block = std::make_unique<ExternBlock>();
    
    // Parse function declarations until closing brace
    while (current.type != TOKEN_RBRACE && current.type != TOKEN_EOF) {
        // For now, just parse statements (should be function declarations)
        auto decl = parseStmt();
        extern_block->declarations.push_back(std::move(decl));
    }
    
    expect(TOKEN_RBRACE);
    
    return extern_block;
}

// Parse module definition: mod name { ... }
std::unique_ptr<Statement> Parser::parseModDef() {
    expect(TOKEN_KW_MOD);
    
    // Parse module name
    Token name_tok = expect(TOKEN_IDENTIFIER);
    std::string module_name = name_tok.value;
    
    // Parse module body
    auto body = parseBlock();
    
    return std::make_unique<ModDef>(module_name, std::move(body));
}

// =============================================================================
// Async Function Parsing (Bug #70)
// =============================================================================

// Helper: check if token is a valid type token
bool Parser::isTypeToken(TokenType type) {
    // Check if token is a primitive or compound type
    // Types are in the range TOKEN_TYPE_VOID to TOKEN_TYPE_STRING
    return (type >= TOKEN_TYPE_VOID && type <= TOKEN_TYPE_STRING) || 
           type == TOKEN_IDENTIFIER;  // user-defined types
}

// Helper: parse array/pointer type modifiers ([], [256], @)
std::string Parser::parseTypeSuffixes(const std::string& baseType) {
    std::string fullType = baseType;
    
    // Check for array or pointer type modifiers
    // Array syntax: int8[], int8[256]
    // Pointer syntax: int64@
    while (current.type == TOKEN_LBRACKET || current.type == TOKEN_AT) {
        if (current.type == TOKEN_LBRACKET) {
            fullType += "[";
            advance();
            
            // Check for array size: [256] or empty []
            if (current.type == TOKEN_INT_LITERAL) {
                fullType += current.value;
                advance();
            }
            
            expect(TOKEN_RBRACKET);
            fullType += "]";
        } else if (current.type == TOKEN_AT) {
            fullType += "@";
            advance();
        }
    }
    
    return fullType;
}

// Parse function parameters: (type:name, type:name, ...)
std::vector<FuncParam> Parser::parseParams() {
    std::vector<FuncParam> params;
    
    expect(TOKEN_LPAREN);
    
    while (current.type != TOKEN_RPAREN && current.type != TOKEN_EOF) {
        // Parse param_type:param_name (with optional array/pointer suffixes)
        if (!isTypeToken(current.type)) {
            throw std::runtime_error("Expected type token in parameter list");
        }
        
        std::string param_type = current.value;
        advance();
        param_type = parseTypeSuffixes(param_type);
        
        expect(TOKEN_COLON);
        
        Token param_name = expect(TOKEN_IDENTIFIER);
        params.push_back(FuncParam(param_type, param_name.value));
        
        if (!match(TOKEN_COMMA)) {
            break;
        }
    }
    
    expect(TOKEN_RPAREN);
    return params;
}

// Parse function declaration
// ARIA SYNTAX (v0.0.6): fn name(params) -> type { body }
// Example: fn add(int:a, int:b) -> int { return a + b; }
std::unique_ptr<FuncDecl> Parser::parseFuncDecl() {
    bool is_async = false;
    bool is_pub = false;
    
    // Check for async keyword (already consumed by parseProgram if present)
    // Check for pub keyword (already consumed by parseProgram if present)
    
    // Consume 'fn' keyword
    consume(TOKEN_KW_FUNC, "Expected 'fn' keyword");
    
    // Parse function name
    Token name_tok = consume(TOKEN_IDENTIFIER, "Expected function name");
    std::string name = name_tok.value;
    
    // Parse parameters: (type:name, type:name, ...)
    auto params = parseParams();
    
    // Parse return type: -> type
    std::string return_type = "void";  // Default
    if (match(TOKEN_ARROW)) {
        if (!isTypeToken(current.type)) {
            std::stringstream ss;
            ss << "Expected return type after '->' at line " << current.line << ", col " << current.col;
            throw std::runtime_error(ss.str());
        }
        return_type = current.value;
        advance();  // consume the return type token
    }
    
    // Parse body block
    auto body = parseBlock();
    
    // Create FuncDecl node
    auto func_decl = std::make_unique<FuncDecl>(name, std::move(params), return_type, std::move(body));
    func_decl->is_async = is_async;
    func_decl->is_pub = is_pub;
    
    return func_decl;
}

// Parse async block with catch clause
// Syntax: async { statements } catch (error:e) { handler }
std::unique_ptr<Statement> Parser::parseAsyncBlock() {
    expect(TOKEN_KW_ASYNC);
    
    // Parse async body
    auto async_body = parseBlock();
    
    // Check for catch clause
    std::unique_ptr<Block> catch_body = nullptr;
    std::string error_var;
    
    if (match(TOKEN_KW_CATCH)) {
        expect(TOKEN_LPAREN);
        
        // Parse error variable: error:e or just e
        if (current.type == TOKEN_IDENTIFIER) {
            Token error_type = current;
            advance();
            
            if (match(TOKEN_COLON)) {
                // Type annotation: error:e
                Token error_name = expect(TOKEN_IDENTIFIER);
                error_var = error_name.value;
            } else {
                // Just variable name: e
                error_var = error_type.value;
            }
        }
        
        expect(TOKEN_RPAREN);
        catch_body = parseBlock();
    }
    
    // Create AsyncBlock AST node with optional catch handler
    return std::make_unique<AsyncBlock>(std::move(async_body), std::move(catch_body), error_var);
}

// =============================================================================
// Pattern Matching Enhancements (Bug #64-66)
// =============================================================================

// Parse destructuring pattern for pick cases (Bug #64)
// Handles: { key: var }, [a, b, c], ...rest
std::unique_ptr<DestructurePattern> Parser::parseDestructurePattern() {
    auto pattern = std::make_unique<DestructurePattern>();
    
    // Object destructuring: { key: value, ... }
    if (match(TOKEN_LBRACE)) {
        pattern->type = DestructurePattern::OBJECT;
        
        while (current.type != TOKEN_RBRACE && current.type != TOKEN_EOF) {
            Token key = expect(TOKEN_IDENTIFIER);
            expect(TOKEN_COLON);
            
            // Parse the binding (could be another pattern or identifier)
            std::unique_ptr<DestructurePattern> value_pattern;
            if (current.type == TOKEN_LBRACE || current.type == TOKEN_LBRACKET) {
                value_pattern = parseDestructurePattern();  // Nested pattern
            } else {
                Token value = expect(TOKEN_IDENTIFIER);
                value_pattern = std::make_unique<DestructurePattern>(DestructurePattern::IDENTIFIER, value.value);
            }
            
            pattern->object_fields.push_back({key.value, std::move(*value_pattern)});
            
            if (!match(TOKEN_COMMA)) {
                break;
            }
        }
        
        expect(TOKEN_RBRACE);
        return pattern;
    }
    
    // Array destructuring: [a, b, c, ...rest]
    if (match(TOKEN_LBRACKET)) {
        pattern->type = DestructurePattern::ARRAY;
        
        while (current.type != TOKEN_RBRACKET && current.type != TOKEN_EOF) {
            // Check for rest pattern: ...rest
            if (current.type == TOKEN_RANGE_EXCLUSIVE) {
                advance();
                Token rest_name = expect(TOKEN_IDENTIFIER);
                auto rest_pattern = std::make_unique<DestructurePattern>(DestructurePattern::REST, rest_name.value);
                pattern->array_elements.push_back(std::move(*rest_pattern));
                break;  // Rest must be last
            }
            
            // Regular element (could be nested pattern)
            if (current.type == TOKEN_LBRACE || current.type == TOKEN_LBRACKET) {
                auto elem_pattern = parseDestructurePattern();
                pattern->array_elements.push_back(std::move(*elem_pattern));
            } else {
                Token elem = expect(TOKEN_IDENTIFIER);
                pattern->array_elements.push_back(DestructurePattern(DestructurePattern::IDENTIFIER, elem.value));
            }
            
            if (!match(TOKEN_COMMA)) {
                break;
            }
        }
        
        expect(TOKEN_RBRACKET);
        return pattern;
    }
    
    // Simple identifier pattern
    if (current.type == TOKEN_IDENTIFIER) {
        Token name = current;
        advance();
        pattern->type = DestructurePattern::IDENTIFIER;
        pattern->name = name.value;
        return pattern;
    }
    
    throw std::runtime_error("Expected destructuring pattern");
}

// Parse template string with interpolation: `text &{expr} more`
// Example: `Value is &{val}`, `Result: &{x + y}`
std::unique_ptr<Expression> Parser::parseTemplateString() {
    expect(TOKEN_BACKTICK);
    
    auto templateStr = std::make_unique<TemplateString>();
    
    // Parse alternating string content and interpolations
    while (current.type != TOKEN_BACKTICK && current.type != TOKEN_EOF) {
        
        // Parse string content
        if (current.type == TOKEN_STRING_CONTENT) {
            templateStr->parts.push_back(TemplatePart(current.value));
            advance();
        }
        
        // Parse interpolation: &{expr}
        else if (current.type == TOKEN_INTERP_START) {
            advance();  // Consume &{
            
            // Parse the expression inside &{...}
            auto expr = parseExpr();
            templateStr->parts.push_back(TemplatePart(std::move(expr)));
            
            expect(TOKEN_RBRACE);  // Consume }
        }
        
        else {
            std::stringstream ss;
            ss << "Unexpected token in template string: " << current.value
               << " at line " << current.line;
            throw std::runtime_error(ss.str());
        }
    }
    
    expect(TOKEN_BACKTICK);
    return templateStr;
}

// =============================================================================
// Function Declaration Parsing
// =============================================================================

} // namespace frontend
} // namespace aria
