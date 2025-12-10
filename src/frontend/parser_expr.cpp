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

using namespace aria::frontend;

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
        case TOKEN_UNWRAP:          return PREC_CALL; // Unwrap operator (?)
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
            uint64_t val = std::stoull(token.value); 
            return std::make_unique<IntLiteral>(val);
        }
        case TOKEN_FLOAT_LITERAL:
            return std::make_unique<FloatLiteral>(std::stod(token.value));
        case TOKEN_STRING_LITERAL:
            return std::make_unique<StringLiteral>(token.value);
        case TOKEN_BACKTICK:
            // Template string with interpolation: `text &{expr} more`
            // Note: TOKEN_BACKTICK is already consumed by parsePrefix's advance()
            // but we need to put it back for parseTemplateString to handle properly
            // For now, handle inline
            return parseTemplateString();
        case TOKEN_BOOLEAN_LITERAL:
            return std::make_unique<BoolLiteral>(token.value == "true");
        case TOKEN_NULL_LITERAL:
            return std::make_unique<NullLiteral>();
        case TOKEN_IDENTIFIER:
            return std::make_unique<VarExpr>(token.value);
        
        // --- Vector Literal Constructors (GLSL-style) ---
        // Example: vec4(1.0, 2.0, 3.0, 4.0), ivec2(10, 20), vec3(0.0)
        // Check all vector type tokens
        case TOKEN_TYPE_VEC2:
        case TOKEN_TYPE_VEC3:
        case TOKEN_TYPE_VEC4:
        case TOKEN_TYPE_VEC9:
        case TOKEN_TYPE_DVEC2:
        case TOKEN_TYPE_DVEC3:
        case TOKEN_TYPE_DVEC4:
        case TOKEN_TYPE_IVEC2:
        case TOKEN_TYPE_IVEC3:
        case TOKEN_TYPE_IVEC4:
        case TOKEN_TYPE_UVEC2:
        case TOKEN_TYPE_UVEC3:
        case TOKEN_TYPE_UVEC4:
        case TOKEN_TYPE_BVEC2:
        case TOKEN_TYPE_BVEC3:
        case TOKEN_TYPE_BVEC4:
        // Also handle matrix constructors
        case TOKEN_TYPE_MAT2:
        case TOKEN_TYPE_MAT3:
        case TOKEN_TYPE_MAT4:
        case TOKEN_TYPE_MAT2X3:
        case TOKEN_TYPE_MAT2X4:
        case TOKEN_TYPE_MAT3X2:
        case TOKEN_TYPE_MAT3X4:
        case TOKEN_TYPE_MAT4X2:
        case TOKEN_TYPE_MAT4X3:
        case TOKEN_TYPE_DMAT2:
        case TOKEN_TYPE_DMAT3:
        case TOKEN_TYPE_DMAT4:
        case TOKEN_TYPE_DMAT2X3:
        case TOKEN_TYPE_DMAT2X4:
        case TOKEN_TYPE_DMAT3X2:
        case TOKEN_TYPE_DMAT3X4:
        case TOKEN_TYPE_DMAT4X2:
        case TOKEN_TYPE_DMAT4X3: {
            // Store the vector/matrix type name
            std::string typeName = token.value;
            auto vecLit = std::make_unique<VectorLiteral>(typeName);
            
            // Consume opening parenthesis
            consume(TOKEN_LEFT_PAREN, "Expected '(' after " + typeName + " constructor");
            
            // Parse constructor arguments (comma-separated expressions)
            if (!check(TOKEN_RIGHT_PAREN)) {
                do {
                    auto element = parseExpression(PREC_COMMA + 1);
                    vecLit->elements.push_back(std::move(element));
                } while (match(TOKEN_COMMA));
            }
            
            // Consume closing parenthesis
            consume(TOKEN_RIGHT_PAREN, "Expected ')' after " + typeName + " constructor arguments");
            
            return vecLit;
        }
        
        // --- Special Variable ($) ---
        // $ is a special iterator variable in till loops: till(100, 1) { $ }
        case TOKEN_DOLLAR:
        case TOKEN_ITERATION:  // TOKEN_ITERATION is an alias for TOKEN_DOLLAR
            return std::make_unique<VarExpr>("$");
        
        // --- Grouping or Cast ---
        case TOKEN_LEFT_PAREN: {
            // Lookahead to distinguish between (expr) and (Type)expr
            // Check if next token could start a type name
            if (isType(peek())) {
                // This looks like a cast: (TypeName)expr
                std::string targetType = parseTypeName();
                consume(TOKEN_RIGHT_PAREN, "Expected ')' after cast type");
                auto expr = parsePrefix();  // Parse the expression being cast
                return std::make_unique<CastExpr>(targetType, std::move(expr));
            } else {
                // Regular grouping: (expr)
                auto expr = parseExpression();
                consume(TOKEN_RIGHT_PAREN, "Expected ')' after expression");
                return expr;
            }
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
                    field.name = fieldName.value;
                    field.value = std::move(value);
                    obj->fields.push_back(std::move(field));
                    
                } while (match(TOKEN_COMMA));
            }
            
            consume(TOKEN_RIGHT_BRACE, "Expected '}' after object literal");
            return obj;
        }

        // --- Array Literal ---
        // Example: [1, 2, 3, 4, 5]
        case TOKEN_LEFT_BRACKET: {
            auto arr = std::make_unique<ArrayLiteral>();
            
            // Parse elements
            if (!check(TOKEN_RIGHT_BRACKET)) {
                do {
                    auto element = parseExpression();
                    arr->elements.push_back(std::move(element));
                } while (match(TOKEN_COMMA));
            }
            
            consume(TOKEN_RIGHT_BRACKET, "Expected ']' after array literal");
            return arr;
        }

        // --- Async/Await Keywords ---
        case TOKEN_KW_AWAIT: {
            // await expression
            auto expr = parseExpression(PREC_UNARY);
            return std::make_unique<AwaitExpr>(std::move(expr));
        }
        
        case TOKEN_KW_SPAWN: {
            // spawn expression
            auto expr = parseExpression(PREC_UNARY);
            return std::make_unique<SpawnExpr>(std::move(expr));
        }

        // --- Unary Operators ---
        // Includes Memory operators: # (Pin), @ (Addr)
        // Note: $ (TOKEN_ITERATION) is NOT a unary operator - it's a variable in till loops
        case TOKEN_MINUS:
        case TOKEN_LOGICAL_NOT:
        case TOKEN_BITWISE_NOT:
        case TOKEN_PIN:       // #
        case TOKEN_ADDRESS:   // @
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

        // --- Call Expression (foo(), p.method(), etc.) ---
        case TOKEN_LEFT_PAREN: {
            // 'left' is the callee expression (can be identifier, member access, etc.)
            std::unique_ptr<CallExpr> call;
            
            // Check if left is a simple identifier (common case)
            if (auto* ident = dynamic_cast<Identifier*>(left.get())) {
                // Simple function call: foo()
                call = std::make_unique<CallExpr>(ident->name);
            } else {
                // Complex callee: p.method(), (get_fn())(), etc.
                call = std::make_unique<CallExpr>(std::move(left));
            }
            
            // Parse arguments
            if (!check(TOKEN_RIGHT_PAREN)) {
                do {
                    call->arguments.push_back(parseExpression());
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
            return std::make_unique<MemberAccess>(std::move(left), name.value, isSafe);
        }

        // --- Index Access (arr[i]) ---
        case TOKEN_LEFT_BRACKET: {
            auto index = parseExpression();
            consume(TOKEN_RIGHT_BRACKET, "Expected ']' after index");
            return std::make_unique<IndexExpr>(std::move(left), std::move(index));
        }

        // --- Unwrap Operator (result ? default) ---
        // Example: test2(3,5) ? -1  // If test2 returns error, use -1 as default
        case TOKEN_UNWRAP: {
            auto default_value = parseExpression(PREC_CALL + 1);
            return std::make_unique<UnwrapExpr>(std::move(left), std::move(default_value));
        }

        default:
            throw std::runtime_error("Unsupported infix operator: " + std::to_string(op.type));
    }
}

// =============================================================================
// 5. Template String Parser
// =============================================================================

// Parse template string: `text &{expr} more &{expr2}`
// The opening backtick has already been consumed by parsePrefix
std::unique_ptr<Expression> Parser::parseTemplateString() {
    auto templ = std::make_unique<TemplateString>();
    
    // Parse parts until we hit the closing backtick
    while (!check(TOKEN_BACKTICK) && !check(TOKEN_EOF)) {
        if (check(TOKEN_STRING_CONTENT)) {
            // Static string part
            Token content_token = advance();
            templ->parts.emplace_back(content_token.lexeme);
        }
        else if (match(TOKEN_INTERP_START)) {
            // Interpolated expression: &{expr}
            auto expr = parseExpression();
            templ->parts.emplace_back(std::move(expr));
            consume(TOKEN_RIGHT_BRACE, "Expected '}' after interpolation expression");
        }
        else {
            throw std::runtime_error("Unexpected token in template string");
        }
    }
    
    consume(TOKEN_BACKTICK, "Expected closing '`' for template string");
    return templ;
}

        default:
            error("Unknown infix operator");
            return nullptr;
    }
}
