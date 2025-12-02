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
    
    // Handle postfix increment
    if (match(TOKEN_INCREMENT)) {
        return std::make_unique<UnaryOp>(UnaryOp::POST_INC, std::move(expr));
    }
    
    // Handle postfix decrement
    if (match(TOKEN_DECREMENT)) {
        return std::make_unique<UnaryOp>(UnaryOp::POST_DEC, std::move(expr));
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
    
    return parseLogicalOr();
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
    if (current.type == TOKEN_KW_CONST || current.type == TOKEN_KW_WILD || 
        current.type == TOKEN_KW_STACK || 
        (current.type >= TOKEN_TYPE_VOID && current.type <= TOKEN_TYPE_STRING)) {
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
    
    // Type
    Token typeToken = current;
    advance();

    // Colon (Aria syntax: type:name)
    expect(TOKEN_COLON);

    // Name
    Token nameToken = expect(TOKEN_IDENTIFIER);

    // Initializer
    std::unique_ptr<Expression> init = nullptr;
    if (match(TOKEN_ASSIGN)) {
        init = parseExpr();
    }

    expect(TOKEN_SEMICOLON);

    auto varDecl = std::make_unique<VarDecl>(typeToken.value, nameToken.value, std::move(init));
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

// Parse function declaration with optional async keyword
// Syntax: [async] [pub] func:name = (params) -> return_type { body }
std::unique_ptr<FuncDecl> Parser::parseFuncDecl() {
    bool is_async = false;
    bool is_pub = false;
    
    // Check for async keyword
    if (match(TOKEN_KW_ASYNC)) {
        is_async = true;
    }
    
    // Check for pub keyword
    if (match(TOKEN_KW_PUB)) {
        is_pub = true;
    }
    
    // Expect func keyword
    expect(TOKEN_KW_FUNC);
    expect(TOKEN_COLON);
    
    // Parse function name
    Token name_tok = expect(TOKEN_IDENTIFIER);
    std::string func_name = name_tok.value;
    
    // Expect assignment
    expect(TOKEN_ASSIGN);
    
    // Parse parameter list
    expect(TOKEN_LPAREN);
    std::vector<FuncParam> parameters;
    
    while (current.type != TOKEN_RPAREN && current.type != TOKEN_EOF) {
        // Parse parameter type
        Token param_type = expect(TOKEN_IDENTIFIER);
        expect(TOKEN_COLON);
        
        // Parse parameter name
        Token param_name = expect(TOKEN_IDENTIFIER);
        
        // Check for default value
        std::unique_ptr<Expression> default_val = nullptr;
        if (match(TOKEN_ASSIGN)) {
            default_val = parseExpr();
        }
        
        parameters.emplace_back(param_type.value, param_name.value, std::move(default_val));
        
        if (!match(TOKEN_COMMA)) {
            break;
        }
    }
    
    expect(TOKEN_RPAREN);
    
    // Parse return type
    std::string return_type = "void";
    if (match(TOKEN_ARROW)) {
        Token ret_tok = expect(TOKEN_IDENTIFIER);
        return_type = ret_tok.value;
    }
    
    // Parse function body
    auto body = parseBlock();
    
    auto func_decl = std::make_unique<FuncDecl>(func_name, std::move(parameters), return_type, std::move(body));
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

} // namespace frontend
} // namespace aria
