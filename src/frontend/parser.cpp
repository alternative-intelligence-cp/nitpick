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

// Parse a complete type name, including built-in types, identifiers, and suffixes
std::string Parser::parseTypeName() {
    std::string typeName;
    
    // Check if it's a built-in type keyword (func, result, int8, etc.)
    if (current.type >= TOKEN_TYPE_VOID && current.type <= TOKEN_TYPE_STRING) {
        typeName = current.value;
        advance();
    } else if (current.type == TOKEN_IDENTIFIER) {
        typeName = current.value;
        advance();
    } else {
        throw std::runtime_error("Expected type name");
    }
    
    // Handle function signature: func<returnType(paramTypes)> or func<T, U> (generic params)
    if (typeName == "func" && current.type == TOKEN_LT) {
        typeName += "<";
        advance(); // consume <
        
        // Distinguishing generic params from function signature:
        // - Generic params: func<T, U> (identifiers only, followed by , or >)
        // - Function signature: func<returnType(params)> (type followed by lparen)
        // 
        // Start parsing and check what we find
        std::vector<std::string> tokens_seen;
        bool looks_like_generic_params = true;
        
        // Parse first element
        if (current.type == TOKEN_IDENTIFIER) {
            tokens_seen.push_back(current.value);
            advance();
            
            // If followed by LPAREN, it's a function signature (returnType is the identifier)
            if (current.type == TOKEN_LPAREN) {
                looks_like_generic_params = false;
            }
        } else {
            // Starts with a non-identifier (like int8), must be function signature
            looks_like_generic_params = false;
        }
        
        if (looks_like_generic_params && !tokens_seen.empty()) {
            // Parse as generic params: we've already consumed first identifier
            typeName += tokens_seen[0];
            
            while (current.type == TOKEN_COMMA) {
                typeName += ",";
                advance(); // consume comma
                
                Token typeParam = expect(TOKEN_IDENTIFIER);
                typeName += typeParam.value;
            }
            
            expect(TOKEN_GT);
            typeName += ">";
        } else {
            // Parse as function type signature
            // We may have consumed one identifier already, need to handle that
            std::string returnType;
            if (!tokens_seen.empty()) {
                returnType = tokens_seen[0];
                // current is now at LPAREN
            } else {
                // Parse return type normally
                returnType = parseTypeName();
            }
            typeName += returnType;
            
            // Expect (
            if (current.type != TOKEN_LPAREN) {
                throw std::runtime_error("Expected '(' after return type in function signature");
            }
            typeName += "(";
            advance(); // consume (
        
            // Parse parameter types
            bool first = true;
            while (current.type != TOKEN_RPAREN && current.type != TOKEN_EOF) {
                if (!first) {
                    if (current.type != TOKEN_COMMA) {
                        throw std::runtime_error("Expected ',' between parameter types");
                    }
                    typeName += ",";
                    advance(); // consume ,
                }
                first = false;
                
                std::string paramType = parseTypeName();
                typeName += paramType;
            }
            
            // Expect )
            if (current.type != TOKEN_RPAREN) {
                throw std::runtime_error("Expected ')' after parameter types");
            }
            typeName += ")";
            advance(); // consume )
            
            // Expect >
            if (current.type != TOKEN_GT) {
                throw std::runtime_error("Expected '>' after function signature");
            }
            typeName += ">";
            advance(); // consume >
        }
    }
    
    // Handle pointer suffix (@)
    while (match(TOKEN_AT)) {
        typeName += "@";
    }
    
    // Handle array suffix ([size] or [])
    if (match(TOKEN_LEFT_BRACKET)) {
        typeName += "[";
        if (!check(TOKEN_RIGHT_BRACKET)) {
            Token sizeTok = expect(TOKEN_INT_LITERAL);
            typeName += sizeTok.value;
        }
        expect(TOKEN_RIGHT_BRACKET);
        typeName += "]";
    }
    
    return typeName;
}

// =============================================================================
// Expression Parsing (Recursive Descent with Precedence Climbing)
// =============================================================================

// Parse primary expressions: literals, variables, parenthesized expressions
std::unique_ptr<Expression> Parser::parsePrimary() {
    // Integer literal
    if (current.type == TOKEN_INT_LITERAL) {
        // Use base 0 to auto-detect: decimal, hex (0x), octal (0), binary (0b)
        int64_t value = std::stoll(current.value, nullptr, 0);
        advance();
        return std::make_unique<IntLiteral>(value);
    }

    // Float literal
    if (current.type == TOKEN_FLOAT_LITERAL) {
        double value = std::stod(current.value);
        advance();
        return std::make_unique<FloatLiteral>(value);
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
    
    // NULL literal
    if (current.type == TOKEN_KW_NULL) {
        advance();
        return std::make_unique<NullLiteral>();
    }

    // Identifier (variable reference, function call, or struct constructor)
    if (current.type == TOKEN_IDENTIFIER) {
        std::string name = current.value;
        advance();
        
        // Check for struct constructor: StructName{field1: value1, field2: value2}
        if (current.type == TOKEN_LEFT_BRACE || current.type == TOKEN_LBRACE) {
            advance(); // consume {
            
            auto obj = std::make_unique<ObjectLiteral>();
            
            // Parse field initializers
            while (current.type != TOKEN_RIGHT_BRACE && current.type != TOKEN_RBRACE && current.type != TOKEN_EOF) {
                Token field_name = expect(TOKEN_IDENTIFIER);
                expect(TOKEN_COLON);
                auto field_value = parseExpr();
                
                ObjectLiteral::Field field;
                field.name = field_name.value;
                field.value = std::move(field_value);
                obj->fields.push_back(std::move(field));
                
                if (!match(TOKEN_COMMA)) {
                    break;
                }
            }
            
            expect(TOKEN_RIGHT_BRACE);
            // Store the struct type name for codegen
            obj->type_name = name;
            return obj;
        }
        
        // Check for function call with optional generic type arguments: identifier<T>(args) or identifier(args)
        if (current.type == TOKEN_LT || current.type == TOKEN_LPAREN) {
            // Parse generic type arguments if present: func<int8, int32>
            if (current.type == TOKEN_LT) {
                advance(); // consume <
                
                std::vector<std::string> typeArgs;
                
                // Parse type argument list
                while (current.type != TOKEN_GT && current.type != TOKEN_EOF) {
                    std::string typeArg = parseTypeName();
                    typeArgs.push_back(typeArg);
                    
                    if (!match(TOKEN_COMMA)) {
                        break;
                    }
                }
                
                expect(TOKEN_GT);
                
                // Now expect function call with arguments
                expect(TOKEN_LPAREN);
                
                auto call = std::make_unique<CallExpr>(name);
                call->type_arguments = typeArgs;
                
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
            
            // Regular function call without generic arguments
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

    // Dollar variable ($) - used in till loops as iterator
    // Example: till(100, 1) { sum = sum + $; }
    if (current.type == TOKEN_DOLLAR || current.type == TOKEN_ITERATION) {
        advance();
        return std::make_unique<VarExpr>("$");
    }

    // Array literal: [1, 2, 3, 4]
    if (current.type == TOKEN_LEFT_BRACKET || current.type == TOKEN_LBRACKET) {
        advance(); // consume [
        
        auto array = std::make_unique<ArrayLiteral>();
        
        // Parse elements
        while (current.type != TOKEN_RIGHT_BRACKET && current.type != TOKEN_RBRACKET && current.type != TOKEN_EOF) {
            array->elements.push_back(parseExpr());
            
            if (!match(TOKEN_COMMA)) {
                break;
            }
        }
        
        expect(TOKEN_RIGHT_BRACKET);
        return array;
    }

    // Parenthesized expression OR cast: (expr) OR (Type)expr
    if (match(TOKEN_LPAREN)) {
        // Lookahead to see if this is a cast (Type) or just a grouped expression
        // Cast pattern: ( TypeName ) Expression
        // We need to check if the token after LPAREN is a type
        
        if (isTypeToken(current.type)) {
            // Could be a cast! Try to parse it
            std::string type_name = parseTypeName();  // This handles @ and [] suffixes too
            
            if (current.type == TOKEN_RPAREN) {
                // Definitely a cast: (Type)
                advance();  // consume )
                
                // Parse the expression to cast
                auto expr = parseUnary();  // Use parseUnary to get the next value
                
                // Create cast expression
                return std::make_unique<CastExpr>(type_name, std::move(expr));
            } else {
                // Not a cast - it's something else
                // This shouldn't happen in well-formed code
                throw std::runtime_error("Unexpected token after type in parentheses");
            }
        } else {
            // Not a type, so it's a normal grouped expression
            auto expr = parseExpr();
            expect(TOKEN_RPAREN);
            return expr;
        }
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
    
    // Vector/Matrix Literal Constructors (GLSL-style)
    // Example: vec4(1.0, 2.0, 3.0, 4.0), ivec3(10, 20, 30), mat4(...)
    // These take EXPRESSIONS as arguments, not parameter declarations (unlike lambdas)
    if (current.type >= TOKEN_TYPE_VEC2 && current.type <= TOKEN_TYPE_DMAT4X3) {
        // It's a vector or matrix type token
        std::string typeName = current.value;
        advance();  // consume the type token (vec4, ivec3, etc.)
        
        // Expect opening parenthesis
        if (current.type != TOKEN_LPAREN) {
            // Not a constructor - might be a variable or type name in another context
            // Create a VarExpr for this (e.g., for variable names that happen to be vec4)
            // Actually, this shouldn't happen since vec4/vec3 are reserved type keywords
            // But to be safe, handle it gracefully
            throw std::runtime_error("Expected '(' after " + typeName + " for constructor");
        }
        
        advance();  // consume (
        
        auto vecLit = std::make_unique<VectorLiteral>(typeName);
        
        // Parse constructor arguments (comma-separated expressions)
        if (current.type != TOKEN_RPAREN) {
            do {
                auto element = parseExpr();
                vecLit->elements.push_back(std::move(element));
            } while (match(TOKEN_COMMA));
        }
        
        // Expect closing parenthesis
        expect(TOKEN_RPAREN);
        
        return vecLit;
    }
    
    // Lambda Expression: returnType(params) { body } or returnType(params){body}(args)
    // SPEC: func:name = returnType(params) { return { err:NULL, val:value }; };
    // SPEC with auto-wrap: func:name = *returnType(params) { return value; };
    // The * prefix ENABLES auto-wrap (compiler wraps return values automatically)
    // Example: *int8(...) means return raw int8, compiler wraps in {err:NULL, val:...}
    bool auto_wrap = false;  // DEFAULT: no auto-wrap (must return explicit Result)
    if (current.type == TOKEN_STAR) {
        auto_wrap = true;  // * enables auto-wrap (raw value returns allowed)
        advance();  // consume *
    }
    
    // Check if current token is a type followed by (
    // IMPORTANT: Identifiers can be types OR variables, so we need to be careful
    // Only treat as lambda if it's definitely "Type(" pattern
    if (isTypeToken(current.type)) {
        // Save the token in case it's actually a variable name, not a lambda type
        Token saved_token = current;
        std::string return_type = current.value;
        advance();  // consume type/identifier token
        
        // NOW check if we have LPAREN (lambda) or something else (variable/expression)
        if (current.type == TOKEN_LPAREN) {
            // This is a lambda! Parse it
            auto params = parseParams();
            
            // Parse lambda body
            auto body = parseBlock();
            
            auto lambda = std::make_unique<LambdaExpr>(return_type, std::move(params), std::move(body));
            lambda->auto_wrap = auto_wrap;
            
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
            // Not a lambda - it's actually a variable reference
            // The "type" token was actually just an identifier (variable name)
            // We've already consumed it, so return it as VarExpr
            return std::make_unique<VarExpr>(saved_token.value);
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
        // Handle array indexing: arr[index]
        if (current.type == TOKEN_LEFT_BRACKET || current.type == TOKEN_LBRACKET) {
            advance(); // consume [
            auto index = parseExpr();
            expect(TOKEN_RIGHT_BRACKET);
            expr = std::make_unique<IndexExpr>(std::move(expr), std::move(index));
            continue;
        }
        
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
        
        // Handle unwrap operator (?): result ? default_value
        // Example: test2(3,5) ? -1  // If test2 returns error, use -1 as default
        if (current.type == TOKEN_UNWRAP || current.type == TOKEN_QUESTION) {
            advance();  // consume ?
            auto default_value = parseUnary();  // Parse the default value
            expr = std::make_unique<UnwrapExpr>(std::move(expr), std::move(default_value));
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
    if (match(TOKEN_UNWRAP)) {
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
    
    if (match(TOKEN_AND_ASSIGN)) {
        auto right = parseAssignment();
        return std::make_unique<BinaryOp>(BinaryOp::AND_ASSIGN, std::move(left), std::move(right));
    }
    
    if (match(TOKEN_OR_ASSIGN)) {
        auto right = parseAssignment();
        return std::make_unique<BinaryOp>(BinaryOp::OR_ASSIGN, std::move(left), std::move(right));
    }
    
    if (match(TOKEN_XOR_ASSIGN)) {
        auto right = parseAssignment();
        return std::make_unique<BinaryOp>(BinaryOp::XOR_ASSIGN, std::move(left), std::move(right));
    }
    
    if (match(TOKEN_LSHIFT_ASSIGN)) {
        auto right = parseAssignment();
        return std::make_unique<BinaryOp>(BinaryOp::LSHIFT_ASSIGN, std::move(left), std::move(right));
    }
    
    if (match(TOKEN_RSHIFT_ASSIGN)) {
        auto right = parseAssignment();
        return std::make_unique<BinaryOp>(BinaryOp::RSHIFT_ASSIGN, std::move(left), std::move(right));
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
    
    // Handle module-level code block wrapper: { ... }
    // Many tests wrap code in top-level braces for module initialization
    if (current.type == TOKEN_LBRACE) {
        advance();  // consume {
        
        // Parse all statements inside the block
        while (current.type != TOKEN_RBRACE && current.type != TOKEN_EOF) {
            // Skip semicolons
            if (match(TOKEN_SEMICOLON)) {
                continue;
            }
            
            // Parse statement and add to block
            auto stmt = parseStmt();
            if (stmt) {
                block->statements.push_back(std::move(stmt));
            }
        }
        
        expect(TOKEN_RBRACE);  // consume }
        return block;
    }
    
    // Parse top-level declarations until EOF
    while (current.type != TOKEN_EOF) {
        // Skip any stray semicolons
        if (match(TOKEN_SEMICOLON)) {
            continue;
        }
        
        // Global variable declarations: [const|wild|stack] type:name = value;
        // This includes func:name = (params) { body }; (lambdas)
        if (current.type == TOKEN_KW_CONST || current.type == TOKEN_KW_WILD || current.type == TOKEN_KW_STACK) {
            // Check if this might be a struct declaration
            // Struct pattern: const StructName = struct { ... };
            // VarDecl pattern: const type:name = value;
            //
            // Peek ahead: const -> identifier -> = -> struct means struct decl
            //             const -> type -> : means var decl
            //
            // Since both start with const (or wild/stack), we need lookahead
            // But our parser doesn't support true backtracking
            //
            // SOLUTION: Make parseVarDecl smart enough to detect and delegate to parseStructDecl
            //           OR make a parseDecl() that chooses
            //
            // For now, let's just always call parseVarDecl and make it handle structs
            block->statements.push_back(parseVarDecl());
            continue;
        }
        
        // Type token - variable declaration (including func-type for lambdas)
        if (isTypeToken(current.type)) {
            block->statements.push_back(parseVarDecl());
            continue;
        }
        
        // Allow module-level statements (for initialization code)
        // This enables tests with top-level pick, print, assignments, etc.
        try {
            auto stmt = parseStmt();
            if (stmt) {
                block->statements.push_back(std::move(stmt));
                continue;
            }
        } catch (...) {
            // If parseStmt fails, fall through to error
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
    
    // fail(errorCode) - Syntactic sugar for return {err:errorCode, val:0}
    if (match(TOKEN_KW_FAIL)) {
        expect(TOKEN_LPAREN);
        auto errorCode = parseExpr();
        expect(TOKEN_RPAREN);
        expect(TOKEN_SEMICOLON);
        
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
        expect(TOKEN_SEMICOLON);
        
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

    // Break statement
    if (match(TOKEN_KW_BREAK)) {
        expect(TOKEN_SEMICOLON);
        return std::make_unique<BreakStmt>();
    }

    // Continue statement
    if (match(TOKEN_KW_CONTINUE)) {
        expect(TOKEN_SEMICOLON);
        return std::make_unique<ContinueStmt>();
    }

    // Variable declaration: [const|wild|stack] type:name = expr;
    // BUT: type( is a lambda expression, not a variable declaration!
    // AND: *type( is an auto-wrap lambda!
    if (current.type == TOKEN_KW_CONST || current.type == TOKEN_KW_WILD || 
        current.type == TOKEN_KW_WILDX || current.type == TOKEN_KW_STACK) {
        return parseVarDecl();
    }
    
    // Check for type:name variable declaration
    // This includes both user-defined types (identifier) and built-in types (uint8, int64, etc.)
    if (current.type == TOKEN_IDENTIFIER || 
        (current.type >= TOKEN_TYPE_VOID && current.type <= TOKEN_TYPE_STRING)) {
        // Lookahead to distinguish variable declaration from expression statement
        Token saved = current;
        advance();
        
        // Parse type suffixes (arrays [], pointers @) before checking for colon
        // We need to check if current is @ or [ BEFORE checking for :
        bool has_type_suffix = (current.type == TOKEN_LBRACKET || current.type == TOKEN_AT);
        bool is_var_decl = has_type_suffix;
        
        // Also check for direct colon (no suffix case)
        if (!has_type_suffix && current.type == TOKEN_COLON) {
            is_var_decl = true;
        }
        
        if (is_var_decl) {
            // This is a variable declaration with type:name pattern
            // Build the full type with suffixes
            std::string type_name = saved.value;
            type_name = parseTypeSuffixes(type_name);
            
            expect(TOKEN_COLON);
            
            Token name_tok = expect(TOKEN_IDENTIFIER);
            
            // Check for array size syntax: name[size]
            if (current.type == TOKEN_LBRACKET) {
                advance();  // consume [
                // Parse array size
                auto size_expr = parseExpr();
                expect(TOKEN_RBRACKET);
                
                // Extract size if it's an integer literal
                if (auto* lit = dynamic_cast<IntLiteral*>(size_expr.get())) {
                    // Append array size to type: "uint8" -> "uint8[10]"
                    type_name += "[" + std::to_string(lit->value) + "]";
                } else {
                    throw std::runtime_error("Array size must be a constant integer");
                }
            }
            
            std::unique_ptr<Expression> init = nullptr;
            if (match(TOKEN_ASSIGN)) {
                init = parseExpr();
            }
            
            expect(TOKEN_SEMICOLON);
            
            return std::make_unique<VarDecl>(type_name, name_tok.value, std::move(init));
        } else {
            // Not a type:name pattern - this is an expression statement
            // We've consumed the identifier (saved) and are now at the next token
            // We need to parse this as a complete expression, which may include:
            // - Assignment: x = value
            // - Compound assignment: x += value  
            // - Function call: func()
            // - Member access: obj.field
            // - Array indexing: arr[i]
            // etc.
            
            // Problem: We've already consumed the identifier, but parseExpr() 
            // expects to start from the current token. We need to reconstruct
            // the expression starting from the identifier we already consumed.
            
            // Solution: Create the base expression (VarExpr) and then manually
            // continue parsing postfix operations and assignments
            
            std::unique_ptr<Expression> expr = std::make_unique<VarExpr>(saved.value);
            
            // Handle postfix operations (function calls, member access, array indexing)
            while (true) {
                if (current.type == TOKEN_LT || current.type == TOKEN_LPAREN) {
                    // Function call with optional generic type arguments
                    
                    std::vector<std::string> typeArgs;
                    
                    // Parse generic type arguments if present
                    if (current.type == TOKEN_LT) {
                        advance(); // consume <
                        
                        while (current.type != TOKEN_GT && current.type != TOKEN_EOF) {
                            std::string typeArg = parseTypeName();
                            typeArgs.push_back(typeArg);
                            
                            if (!match(TOKEN_COMMA)) {
                                break;
                            }
                        }
                        
                        expect(TOKEN_GT);
                        expect(TOKEN_LPAREN);  // Now expect (
                    } else {
                        advance(); // consume (
                    }
                    
                    auto call = std::make_unique<CallExpr>(saved.value);
                    if (!typeArgs.empty()) {
                        call->type_arguments = typeArgs;
                    }
                    
                    while (current.type != TOKEN_RPAREN && current.type != TOKEN_EOF) {
                        call->arguments.push_back(parseExpr());
                        if (!match(TOKEN_COMMA)) break;
                    }
                    expect(TOKEN_RPAREN);
                    expr = std::move(call);
                } else if (current.type == TOKEN_DOT || current.type == TOKEN_SAFE_NAV) {
                    // Member access
                    bool is_safe = (current.type == TOKEN_SAFE_NAV);
                    advance();
                    Token member_tok = expect(TOKEN_IDENTIFIER);
                    auto member_access = std::make_unique<MemberAccess>(std::move(expr), member_tok.value, is_safe);
                    expr = std::move(member_access);
                } else if (current.type == TOKEN_LBRACKET) {
                    // Array indexing
                    advance();
                    auto index = parseExpr();
                    expect(TOKEN_RBRACKET);
                    expr = std::make_unique<IndexExpr>(std::move(expr), std::move(index));
                } else {
                    // No more postfix operators
                    break;
                }
            }
            
            // Handle assignment operators
            if (current.type == TOKEN_ASSIGN || 
                current.type == TOKEN_PLUS_ASSIGN ||
                current.type == TOKEN_MINUS_ASSIGN ||
                current.type == TOKEN_STAR_ASSIGN ||
                current.type == TOKEN_SLASH_ASSIGN ||
                current.type == TOKEN_MOD_ASSIGN ||
                current.type == TOKEN_AND_ASSIGN ||
                current.type == TOKEN_OR_ASSIGN ||
                current.type == TOKEN_XOR_ASSIGN) {
                
                // Convert token type to BinaryOp::OpType
                BinaryOp::OpType op;
                switch (current.type) {
                    case TOKEN_ASSIGN: op = BinaryOp::ASSIGN; break;
                    case TOKEN_PLUS_ASSIGN: op = BinaryOp::PLUS_ASSIGN; break;
                    case TOKEN_MINUS_ASSIGN: op = BinaryOp::MINUS_ASSIGN; break;
                    case TOKEN_STAR_ASSIGN: op = BinaryOp::STAR_ASSIGN; break;
                    case TOKEN_SLASH_ASSIGN: op = BinaryOp::SLASH_ASSIGN; break;
                    case TOKEN_MOD_ASSIGN: op = BinaryOp::MOD_ASSIGN; break;
                    case TOKEN_AND_ASSIGN: op = BinaryOp::AND_ASSIGN; break;
                    case TOKEN_OR_ASSIGN: op = BinaryOp::OR_ASSIGN; break;
                    case TOKEN_XOR_ASSIGN: op = BinaryOp::XOR_ASSIGN; break;
                    default: 
                        throw std::runtime_error("Unexpected assignment operator");
                }
                
                advance();
                auto rhs = parseExpr();
                expr = std::make_unique<BinaryOp>(op, std::move(expr), std::move(rhs));
            }
            
            expect(TOKEN_SEMICOLON);
            return std::make_unique<ExpressionStmt>(std::move(expr));
        }
    }
    
    // Check for * prefix - now means "generic type follows" not autowrap
    // Example: *T(x) means T is a generic type parameter, not autowrap
    bool is_generic_type_marker = false;
    if (current.type == TOKEN_STAR) {
        is_generic_type_marker = true;
        advance();  // consume *
    }
    
    // Check if this is a type token OR a generic type parameter (with * prefix)
    bool isTypeOrGeneric = (current.type >= TOKEN_TYPE_VOID && current.type <= TOKEN_TYPE_STRING);
    if (!isTypeOrGeneric && current.type == TOKEN_IDENTIFIER && is_generic_type_marker) {
        // * followed by identifier means it's a generic type parameter
        // Check if identifier is a known generic type parameter
        for (const auto& param : context.genericTypeParams) {
            if (current.value == param) {
                isTypeOrGeneric = true;
                break;
            }
        }
    }
    
    if (isTypeOrGeneric) {
        // Lookahead: if next token is '(', this is a lambda expression statement
        // BUT: for func<...> signatures, we need to parse the full type first
        
        Token saved = current;
        advance();
        
        // Check for function signature: func<...>
        if (saved.value == "func" && current.type == TOKEN_LT) {
            // This is a func signature variable declaration, not a lambda
            // Put token back (we need to re-parse the full type)
            // Problem: can't backtrack easily
            // Solution: Parse the signature here, then continue
            
            // We're at '<' after 'func'
            // Build the full type string by manually parsing the signature
            std::string fullType = "func<";
            advance(); // consume <
            
            // Parse return type (might be complex, so need recursive parsing)
            // For simplicity, assume return type is simple for now
            // TODO: Make this properly recursive
            if (current.type >= TOKEN_TYPE_VOID && current.type <= TOKEN_TYPE_STRING) {
                fullType += current.value;
                advance();
            } else if (current.type == TOKEN_IDENTIFIER) {
                fullType += current.value;
                advance();
            } else {
                throw std::runtime_error("Expected return type in function signature");
            }
            
            // Parse parameter list
            expect(TOKEN_LPAREN);
            fullType += "(";
            
            bool first = true;
            while (current.type != TOKEN_RPAREN) {
                if (!first) {
                    expect(TOKEN_COMMA);
                    fullType += ",";
                }
                first = false;
                
                // Parse parameter type
                if (current.type >= TOKEN_TYPE_VOID && current.type <= TOKEN_TYPE_STRING) {
                    fullType += current.value;
                    advance();
                } else if (current.type == TOKEN_IDENTIFIER) {
                    fullType += current.value;
                    advance();
                } else {
                    throw std::runtime_error("Expected parameter type in function signature");
                }
            }
            
            expect(TOKEN_RPAREN);
            fullType += ")";
            expect(TOKEN_GT);
            fullType += ">";
            
            // Now expect ':' for variable declaration
            expect(TOKEN_COLON);
            Token name_tok = expect(TOKEN_IDENTIFIER);
            
            std::unique_ptr<Expression> init = nullptr;
            if (match(TOKEN_ASSIGN)) {
                init = parseExpr();
            }
            
            expect(TOKEN_SEMICOLON);
            return std::make_unique<VarDecl>(fullType, name_tok.value, std::move(init));
        }
        
        if (current.type == TOKEN_LPAREN) {
            // This is a lambda! Parse it here
            // We've consumed the type token and seen LPAREN
            // saved has the type token
            
            std::string return_type = saved.value;
            auto params = parseParams();  // This will consume the ( and params and )
            auto body = parseBlock();
            
            auto lambda = std::make_unique<LambdaExpr>(return_type, std::move(params), std::move(body));
            // Note: auto_wrap removed - now use explicit pass()/fail() syntax
            
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
        auto thenBranch = parseBlockOrStatement();
        std::unique_ptr<Block> elseBranch = nullptr;
        if (match(TOKEN_KW_ELSE)) {
            elseBranch = parseBlockOrStatement();
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

// Parse either a block {...} or a single statement
// Used for if/else/while/for bodies to allow one-liner syntax
std::unique_ptr<Block> Parser::parseBlockOrStatement() {
    if (current.type == TOKEN_LBRACE) {
        // It's a block - need to parse it manually since parseBlock doesn't exist in parser.cpp
        expect(TOKEN_LBRACE);
        auto block = std::make_unique<Block>();
        
        while (current.type != TOKEN_RBRACE && current.type != TOKEN_EOF) {
            auto stmt = parseStmt();
            if (stmt) {
                block->statements.push_back(std::move(stmt));
            }
        }
        
        expect(TOKEN_RBRACE);
        return block;
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

// Parse variable declaration: [const|wild|wildx|stack] type:name = value;
// OR struct declaration: const StructName = struct { ... };
std::unique_ptr<Statement> Parser::parseVarDecl() {
    // Optional const/wild/wildx/stack prefix
    bool is_const = false;
    bool is_wild = false;
    bool is_wildx = false;
    bool is_stack = false;
    
    if (current.type == TOKEN_KW_CONST) {
        is_const = true;
        advance();
    } else if (current.type == TOKEN_KW_WILD) {
        is_wild = true;
        advance();
    } else if (current.type == TOKEN_KW_WILDX) {
        is_wildx = true;
        advance();
    } else if (current.type == TOKEN_KW_STACK) {
        is_stack = true;
        advance();
    }
    
    // Now we need to distinguish between:
    // - Struct declaration: identifier = struct { ... }
    // - Variable declaration: type:name = value
    
    // Check for struct pattern: IDENTIFIER = struct (struct name, not type)
    // Important: Type tokens like TOKEN_TYPE_FUNC won't match this pattern
    if (current.type == TOKEN_IDENTIFIER) {
        Token maybe_struct_name = current;
        advance();
        
        if (current.type == TOKEN_ASSIGN) {
            // This is a struct declaration!
            advance(); // consume =
            expect(TOKEN_KW_STRUCT);
            expect(TOKEN_LEFT_BRACE);
            
            std::vector<StructField> fields;
            while (!check(TOKEN_RIGHT_BRACE)) {
                Token field_name = expect(TOKEN_IDENTIFIER);
                expect(TOKEN_COLON);
                
                // Parse field type using parseTypeName for complex types
                std::string type_name = parseTypeName();
                
                fields.emplace_back(type_name, field_name.value);
                expect(TOKEN_COMMA);
            }
            
            expect(TOKEN_RIGHT_BRACE);
            expect(TOKEN_SEMICOLON);
            
            auto decl = std::make_unique<StructDecl>(maybe_struct_name.value, fields);
            decl->is_const = is_const;
            return decl;
        }
        
        // Not a struct - it's a variable with simple type
        // We already consumed the type identifier and advanced
        // Handle suffixes (@ for pointer, [] for array)
        std::string fullType = parseTypeSuffixes(maybe_struct_name.value);
        
        // Current should now be at colon
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
        varDecl->is_wildx = is_wildx;
        varDecl->is_stack = is_stack;
        
        return varDecl;
    }
    
    // Not a simple identifier - must be a built-in type or complex type like func<...>
    
    // Use parseTypeName to handle all types including func<...>
    std::string fullType = parseTypeName();

    // Colon (Aria syntax: type:name)
    expect(TOKEN_COLON);

    // Name
    Token nameToken = expect(TOKEN_IDENTIFIER);

    // Extract generic parameters from type if present and add to context
    std::vector<std::string> savedGenericParams = context.genericTypeParams;
    if (fullType.substr(0, 5) == "func<") {
        size_t start = 5;  // After "func<"
        size_t end = fullType.find('>');
        if (end != std::string::npos) {
            std::string params_str = fullType.substr(start, end - start);
            
            // Split by comma
            size_t pos = 0;
            while (pos < params_str.length()) {
                size_t comma = params_str.find(',', pos);
                if (comma == std::string::npos) {
                    comma = params_str.length();
                }
                std::string param = params_str.substr(pos, comma - pos);
                // Trim whitespace
                size_t first = param.find_first_not_of(" \t");
                size_t last = param.find_last_not_of(" \t");
                if (first != std::string::npos && last != std::string::npos) {
                    param = param.substr(first, last - first + 1);
                    context.genericTypeParams.push_back(param);
                }
                pos = comma + 1;
            }
        }
    }

    // Initializer
    std::unique_ptr<Expression> init = nullptr;
    if (match(TOKEN_ASSIGN)) {
        init = parseExpr();
    }

    // Restore original generic params
    context.genericTypeParams = savedGenericParams;

    expect(TOKEN_SEMICOLON);

    auto varDecl = std::make_unique<VarDecl>(fullType, nameToken.value, std::move(init));
    varDecl->is_const = is_const;
    varDecl->is_wild = is_wild;
    varDecl->is_wildx = is_wildx;
    varDecl->is_stack = is_stack;
    
    // Store generic parameters in VarDecl for codegen
    if (fullType.substr(0, 5) == "func<") {
        size_t start = 5;
        size_t end = fullType.find('>');
        if (end != std::string::npos) {
            std::string params_str = fullType.substr(start, end - start);
            size_t pos = 0;
            while (pos < params_str.length()) {
                size_t comma = params_str.find(',', pos);
                if (comma == std::string::npos) {
                    comma = params_str.length();
                }
                std::string param = params_str.substr(pos, comma - pos);
                size_t first = param.find_first_not_of(" \t");
                size_t last = param.find_last_not_of(" \t");
                if (first != std::string::npos && last != std::string::npos) {
                    param = param.substr(first, last - first + 1);
                    varDecl->generic_params.push_back(param);
                }
                pos = comma + 1;
            }
        }
    }
    
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

// Parse while loop: while (condition) { ... }
std::unique_ptr<Statement> Parser::parseWhileLoop() {
    expect(TOKEN_KW_WHILE);
    expect(TOKEN_LPAREN);
    
    // Parse condition expression
    auto condition = parseExpr();
    
    expect(TOKEN_RPAREN);
    
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

// Parse till loop: till(max, step) { body } OR till(condition) { body }
// Spec Section 8.2: Automatic iterator with $ variable OR condition loop
// Two forms:
// 1. till(limit, step) - iterator form with $ variable
// 2. till(condition) - condition form (synonym for while)
std::unique_ptr<Statement> Parser::parseTillLoop() {
    expect(TOKEN_KW_TILL);
    expect(TOKEN_LPAREN);
    
    // Parse first expression (could be limit or condition)
    auto first_expr = parseExpr();
    
    // Check if there's a comma (iterator form) or closing paren (condition form)
    if (match(TOKEN_COMMA)) {
        // Iterator form: till(limit, step)
        auto limit = std::move(first_expr);
        
        // Parse step expression
        auto step = parseExpr();
        
        expect(TOKEN_RPAREN);
        
        // Parse loop body
        auto body = parseBlock();
        
        return std::make_unique<TillLoop>(std::move(limit), std::move(step), std::move(body));
    } else {
        // Condition form: till(condition) - create WhileLoop
        expect(TOKEN_RPAREN);
        
        // Parse loop body
        auto body = parseBlock();
        
        return std::make_unique<WhileLoop>(std::move(first_expr), std::move(body));
    }
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
        // Check for optional label: label=>(pattern) or label=>(!)
        std::string label;
        if (current.type == TOKEN_IDENTIFIER) {
            // Save the identifier in case it's a label
            std::string potential_label = current.value;
            advance();
            
            if (current.type == TOKEN_FAT_ARROW) {
                // It's a label! (using => operator)
                label = potential_label;
                advance();  // consume =>
            } else {
                // Not a label - error, identifiers not allowed here
                throw std::runtime_error(
                    "Unexpected identifier '" + potential_label + 
                    "' in pick statement at line " + std::to_string(current.line) +
                    ". Expected '=>' for label or '(' for case pattern."
                );
            }
        }
        
        // Parse pattern: (pattern) or (!)
        expect(TOKEN_LPAREN);
        
        PickCase::CaseType case_type;
        std::unique_ptr<Expression> value_start;
        std::unique_ptr<Expression> value_end;
        bool is_range_exclusive = false;
        
        // Check for unreachable pattern (!)
        if (match(TOKEN_LOGICAL_NOT)) {
            // Unreachable/fallthrough-only case
            case_type = PickCase::UNREACHABLE;
            expect(TOKEN_RPAREN);
            
            // Parse body
            auto body = parseBlock();
            
            PickCase pcase(case_type, std::move(body));
            pcase.label = label;
            pick->cases.push_back(std::move(pcase));
            
            // Optional comma
            match(TOKEN_COMMA);
            continue;
        }
        
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
            if (current.type == TOKEN_RANGE) {
                // Inclusive range: start..end
                advance();
                case_type = PickCase::RANGE;
                is_range_exclusive = false;
                value_end = parseExpr();
            } else if (current.type == TOKEN_RANGE_EXCLUSIVE) {
                // Exclusive range: start...end
                advance();
                case_type = PickCase::RANGE;
                is_range_exclusive = true;
                value_end = parseExpr();
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
        pcase.label = label;
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
        // Parse param_type:param_name (with optional memory qualifiers and array/pointer suffixes)
        // Can have optional wild/wildx qualifier: "wild int8@:ptr"
        // NEW: Can have * prefix for generic types: "*T:value"
        
        std::string param_type = "";
        
        // Check for * prefix (generic type marker)
        bool has_generic_marker = false;
        if (current.type == TOKEN_STAR) {
            has_generic_marker = true;
            advance();
        }
        
        // Check for memory qualifier (wild, wildx)
        if (current.type == TOKEN_KW_WILD) {
            param_type = "wild ";
            advance();
        } else if (current.type == TOKEN_KW_WILDX) {
            param_type = "wildx ";
            advance();
        }
        
        // Now parse the base type
        // If has_generic_marker, identifier is allowed (generic type param)
        bool is_valid_type = isTypeToken(current.type);
        if (!is_valid_type && has_generic_marker && current.type == TOKEN_IDENTIFIER) {
            // Check if it's a known generic type parameter
            for (const auto& param : context.genericTypeParams) {
                if (current.value == param) {
                    is_valid_type = true;
                    break;
                }
            }
        }
        
        if (!is_valid_type) {
            throw std::runtime_error("Expected type token in parameter list");
        }
        
        param_type += current.value;
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
    
    // Create FuncDecl node (no generics in old-style parser)
    auto func_decl = std::make_unique<FuncDecl>(name, std::vector<std::string>(), std::move(params), return_type, std::move(body));
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
