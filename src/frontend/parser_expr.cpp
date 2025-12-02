/**
 * src/frontend/parser_expr.cpp
 * 
 * Aria Compiler - Expression Parser (Pratt Implementation)
 * Version: 0.0.6
 * 
 * Implements the parsing of expressions using Top-Down Operator Precedence (Pratt Parsing).
 * This handles Aria's complex 22-level operator table, including:
 * - Pipeline operators (|>, <|)
 * - Spaceship operator (<=>)
 * - Exotic Ternary Logic operators (is... :...)
 * - Memory operators (#, @, $)
 */

#include "parser.h"
#include "ast.h"
#include "tokens.h"
#include <map>
#include <functional>
#include <string>

using namespace aria;

// =============================================================================
// 1. Precedence Table Definition (Aria Spec v0.0.6)
// =============================================================================

// Precedence levels corresponding to C++20 and Aria Spec [1, 6]
enum Precedence {
    PREC_NONE = 0,
    PREC_COMMA,         // ,
    PREC_ASSIGNMENT,    // = += -=
    PREC_TERNARY,       // is? :
    PREC_PIPELINE,      // |> <|
    PREC_LOGICAL_OR,    // ||
    PREC_LOGICAL_AND,   // &&
    PREC_EQUALITY,      // ==!=
    PREC_RELATIONAL,    // < > <= >=
    PREC_SPACESHIP,     // <=> (Level 8 in spec implies high priority comparison)
    PREC_BITWISE_OR,    // |
    PREC_BITWISE_XOR,   // ^
    PREC_BITWISE_AND,   // &
    PREC_SHIFT,         // << >>
    PREC_ADD,           // + -
    PREC_MULT,          // * / %
    PREC_UNARY,         //! - ++ -- @ # $
    PREC_CALL,          // ().?.
    PREC_PRIMARY
};

// Map Token Type to Precedence
static Precedence getPrecedence(TokenType type) {
    switch (type) {
        case TOKEN_COMMA:           return PREC_COMMA;
        case TOKEN_ASSIGN:
        case TOKEN_PLUS_ASSIGN:
        case TOKEN_MINUS_ASSIGN:    return PREC_ASSIGNMENT;
        case TOKEN_TERNARY_IS:      return PREC_TERNARY; // 'is' keyword
        case TOKEN_PIPE_FORWARD:
        case TOKEN_PIPE_BACKWARD:   return PREC_PIPELINE;
        case TOKEN_LOGICAL_OR:      return PREC_LOGICAL_OR;
        case TOKEN_LOGICAL_AND:     return PREC_LOGICAL_AND;
        case TOKEN_EQUAL:
        case TOKEN_NOT_EQUAL:       return PREC_EQUALITY;
        case TOKEN_LESS_THAN:
        case TOKEN_GREATER_THAN:
        case TOKEN_LESS_EQUAL:
        case TOKEN_GREATER_EQUAL:   return PREC_RELATIONAL;
        case TOKEN_SPACESHIP:       return PREC_SPACESHIP;
        case TOKEN_PLUS:
        case TOKEN_MINUS:           return PREC_ADD;
        case TOKEN_MULTIPLY:
        case TOKEN_DIVIDE:
        case TOKEN_MODULO:          return PREC_MULT;
        case TOKEN_LEFT_PAREN:      return PREC_CALL; // Function Call
        case TOKEN_DOT:
        case TOKEN_SAFE_NAV:        return PREC_CALL; // Member access
        case TOKEN_LEFT_BRACKET:    return PREC_CALL; // Index
        default:                    return PREC_NONE;
    }
}

// =============================================================================
// 2. Core Pratt Parser Loop
// =============================================================================

// Parses an expression with precedence >= minPrec
std::unique_ptr<Expression> Parser::parseExpression(int minPrec) {
    // 1. Parse Prefix (Left-hand side / NUD)
    // This handles literals, variables, and unary operators (e.g., -5,!x, #ptr)
    auto left = parsePrefix();

    // 2. Precedence Climbing Loop (LED)
    // While the next token is an operator with higher precedence than our current context...
    while (true) {
        TokenType nextType = peek().type;
        int nextPrec = getPrecedence(nextType);

        // Stop if next token has lower precedence or isn't an operator
        if (nextPrec < minPrec) break;

        // Consume the operator and parse the infix expression
        Token opToken = advance();
        left = parseInfix(std::move(left), opToken);
    }

    return left;
}

// Wrapper for top-level calls (min precedence 1)
std::unique_ptr<Expression> Parser::parseExpression() {
    return parseExpression(PREC_COMMA + 1);
}

// =============================================================================
// 3. Prefix Handlers (Nud - Null Denotation)
// =============================================================================

std::unique_ptr<Expression> Parser::parsePrefix() {
    Token token = advance();

    switch (token.type) {
        // --- Literals ---
        case TOKEN_INTEGER_LITERAL: {
            // Need to handle parsing of int64 vs int512 here strictly
            // For now assuming standard unsigned long long parsing
            uint64_t val = std::stoull(token.lexeme); 
            return std::make_unique<IntLiteral>(val);
        }
        case TOKEN_FLOAT_LITERAL:
            return std::make_unique<FloatLiteral>(std::stod(token.lexeme));
        case TOKEN_STRING_LITERAL:
            return std::make_unique<StringLiteral>(token.lexeme);
        case TOKEN_BOOLEAN_LITERAL:
            return std::make_unique<BoolLiteral>(token.lexeme == "true");
        case TOKEN_NULL_LITERAL:
            return std::make_unique<NullLiteral>();
        case TOKEN_IDENTIFIER:
            return std::make_unique<VarExpr>(token.lexeme);
        
        // --- Grouping ---
        case TOKEN_LEFT_PAREN: {
            auto expr = parseExpression();
            consume(TOKEN_RIGHT_PAREN, "Expected ')' after expression");
            return expr;
        }
        
        // --- Object Literal (for Result type) ---
        // Example: { err: NULL, val: 42 }
        case TOKEN_LEFT_BRACE: {
            auto obj = std::make_unique<ObjectLiteral>();
            
            // Parse field: value pairs
            if (!check(TOKEN_RIGHT_BRACE)) {
                do {
                    // Parse field name
                    Token fieldName = consume(TOKEN_IDENTIFIER, "Expected field name in object literal");
                    consume(TOKEN_COLON, "Expected ':' after field name");
                    
                    // Parse field value
                    auto value = parseExpression();
                    
                    // Add field to object
                    ObjectLiteral::Field field;
                    field.name = fieldName.lexeme;
                    field.value = std::move(value);
                    obj->fields.push_back(std::move(field));
                    
                } while (match(TOKEN_COMMA));
            }
            
            consume(TOKEN_RIGHT_BRACE, "Expected '}' after object literal");
            return obj;
        }

        // --- Unary Operators ---
        // Includes Memory operators: # (Pin), @ (Addr), $ (SafeRef) 
        case TOKEN_MINUS:
        case TOKEN_LOGICAL_NOT:
        case TOKEN_BITWISE_NOT:
        case TOKEN_PIN:       // #
        case TOKEN_ADDRESS:   // @
        case TOKEN_ITERATION: // $
        {
            // Recursive call with UNARY precedence to bind tight
            auto operand = parseExpression(PREC_UNARY);
            return std::make_unique<UnaryOp>(token.type, std::move(operand));
        }

        // --- Ternary Start ---
        // Spec Example: int8:t = is r.err == NULL : r.val : -1;
        // 'is' introduces the expression.
        case TOKEN_TERNARY_IS: {
            // Parse Condition
            auto condition = parseExpression(PREC_TERNARY);
            
            consume(TOKEN_COLON, "Expected ':' after ternary condition");
            
            // Parse True Branch
            auto trueBranch = parseExpression(PREC_TERNARY);
            
            consume(TOKEN_COLON, "Expected ':' after ternary true branch");
            
            // Parse False Branch
            auto falseBranch = parseExpression(PREC_TERNARY);
            
            return std::make_unique<TernaryExpr>(
                std::move(condition), 
                std::move(trueBranch), 
                std::move(falseBranch)
            );
        }

        default:
            // Error handling via the Parser context
            error("Unexpected token in expression: " + token.lexeme);
            return nullptr; // Unreachable
    }
}

// =============================================================================
// 4. Infix Handlers (Led - Left Denotation)
// =============================================================================

std::unique_ptr<Expression> Parser::parseInfix(std::unique_ptr<Expression> left, Token op) {
    switch (op.type) {
        // --- Binary Operators ---
        case TOKEN_PLUS:
        case TOKEN_MINUS:
        case TOKEN_MULTIPLY:
        case TOKEN_DIVIDE:
        case TOKEN_MODULO:
        case TOKEN_EQUAL:
        case TOKEN_NOT_EQUAL:
        case TOKEN_LESS_THAN:
        case TOKEN_GREATER_THAN:
        case TOKEN_SPACESHIP:       // <=>
        case TOKEN_PIPE_FORWARD:    // |>
        case TOKEN_PIPE_BACKWARD:   // <|
        {
            // Most Aria ops are Left-Associative.
            int prec = getPrecedence(op.type);
            // Parse right side with slightly higher precedence (+1) for left-associativity
            // This ensures 1 + 2 + 3 parses as (1 + 2) + 3
            auto right = parseExpression(prec + 1);
            return std::make_unique<BinaryOp>(op.type, std::move(left), std::move(right));
        }

        // --- Call Expression (foo()) ---
        case TOKEN_LEFT_PAREN: {
            // 'left' is the callee
            auto call = std::make_unique<CallExpr>(std::move(left));
            if (!check(TOKEN_RIGHT_PAREN)) {
                do {
                    call->args.push_back(parseExpression());
                } while (match(TOKEN_COMMA));
            }
            consume(TOKEN_RIGHT_PAREN, "Expected ')' after arguments");
            return call;
        }

        // --- Member Access (obj.prop or obj?.prop) ---
        case TOKEN_DOT:
        case TOKEN_SAFE_NAV: {
            Token name = consume(TOKEN_IDENTIFIER, "Expected property name after '.'");
            bool isSafe = (op.type == TOKEN_SAFE_NAV);
            return std::make_unique<MemberAccess>(std::move(left), name.lexeme, isSafe);
        }

        // --- Index Access (arr) ---
        case TOKEN_LEFT_BRACKET: {
            auto index = parseExpression();
            consume(TOKEN_RIGHT_BRACKET, "Expected ']' after index");
            return std::make_unique<IndexExpr>(std::move(left), std::move(index));
        }

        default:
            error("Unknown infix operator");
            return nullptr;
    }
}
