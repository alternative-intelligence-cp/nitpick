#include "frontend/parser/parser.h"
#include "frontend/ast/type.h"
#include "frontend/ast/expr.h"
#include <sstream>
#include <iostream>

using namespace aria;
using namespace aria::frontend;

namespace aria {

// Operator precedence table (higher = tighter binding)
const std::unordered_map<TokenType, int> Parser::precedence = {
    // Assignment (lowest precedence)
    {TokenType::TOKEN_EQUAL, 0},
    {TokenType::TOKEN_PLUS_EQUAL, 0},
    {TokenType::TOKEN_MINUS_EQUAL, 0},
    {TokenType::TOKEN_STAR_EQUAL, 0},
    {TokenType::TOKEN_SLASH_EQUAL, 0},
    {TokenType::TOKEN_PERCENT_EQUAL, 0},
    
    // Ternary
    {TokenType::TOKEN_KW_IS, 1},
    
    // Null coalescing
    {TokenType::TOKEN_NULL_COALESCE, 2},
    
    // Logical OR
    {TokenType::TOKEN_OR_OR, 3},
    
    // Logical AND
    {TokenType::TOKEN_AND_AND, 4},
    
    // Bitwise OR
    {TokenType::TOKEN_PIPE, 5},
    
    // Bitwise XOR
    {TokenType::TOKEN_CARET, 6},
    
    // Bitwise AND
    {TokenType::TOKEN_AMPERSAND, 7},
    
    // Equality
    {TokenType::TOKEN_EQUAL_EQUAL, 8},
    {TokenType::TOKEN_BANG_EQUAL, 8},
    
    // Comparison
    {TokenType::TOKEN_LESS, 9},
    {TokenType::TOKEN_LESS_EQUAL, 9},
    {TokenType::TOKEN_GREATER, 9},
    {TokenType::TOKEN_GREATER_EQUAL, 9},
    {TokenType::TOKEN_SPACESHIP, 9},
    
    // Range
    {TokenType::TOKEN_DOT_DOT, 10},
    {TokenType::TOKEN_DOT_DOT_DOT, 10},
    
    // Shift
    {TokenType::TOKEN_SHIFT_LEFT, 11},
    {TokenType::TOKEN_SHIFT_RIGHT, 11},
    
    // Additive
    {TokenType::TOKEN_PLUS, 12},
    {TokenType::TOKEN_MINUS, 12},
    
    // Multiplicative
    {TokenType::TOKEN_STAR, 13},
    {TokenType::TOKEN_SLASH, 13},
    {TokenType::TOKEN_PERCENT, 13},
    
    // Pipeline
    {TokenType::TOKEN_PIPE_RIGHT, 14},
    {TokenType::TOKEN_PIPE_LEFT, 14},
    
    // Postfix (handled specially)
    {TokenType::TOKEN_PLUS_PLUS, 16},
    {TokenType::TOKEN_MINUS_MINUS, 16},
    {TokenType::TOKEN_LEFT_PAREN, 16},    // Function call
    {TokenType::TOKEN_LEFT_BRACKET, 16},  // Array index
    {TokenType::TOKEN_DOT, 16},            // Member access
    {TokenType::TOKEN_ARROW, 16},          // Pointer member
    {TokenType::TOKEN_SAFE_NAV, 16},       // Safe navigation
};

Parser::Parser(const std::vector<Token>& tokens)
    : tokens(tokens), current(0) {}

// Utility methods
Token Parser::peek() const {
    if (current < tokens.size()) {
        return tokens[current];
    }
    return tokens.back(); // Return EOF
}

Token Parser::peekNext() const {
    if (current + 1 < tokens.size()) {
        return tokens[current + 1];
    }
    return tokens.back(); // Return EOF
}

Token Parser::previous() const {
    if (current > 0) {
        return tokens[current - 1];
    }
    return tokens[0];
}

Token Parser::advance() {
    if (!isAtEnd()) {
        current++;
    }
    return previous();
}

bool Parser::isAtEnd() const {
    return peek().type == TokenType::TOKEN_EOF;
}

bool Parser::check(TokenType type) const {
    if (isAtEnd()) return false;
    return peek().type == type;
}

bool Parser::match(TokenType type) {
    if (check(type)) {
        advance();
        return true;
    }
    return false;
}

bool Parser::match(const std::vector<TokenType>& types) {
    for (TokenType type : types) {
        if (check(type)) {
            advance();
            return true;
        }
    }
    return false;
}

Token Parser::consume(TokenType type, const std::string& message) {
    if (check(type)) {
        return advance();
    }
    error(message);
    return peek();
}

void Parser::error(const std::string& message) {
    Token token = peek();
    std::stringstream ss;
    
    // Main error message with location
    ss << "Parse error at line " << token.line << ", column " << token.column << ":\n";
    ss << "  " << message;
    
    // Add token context if available
    if (token.type != TokenType::TOKEN_EOF) {
        ss << "\n  Found: ";
        if (token.type == TokenType::TOKEN_IDENTIFIER) {
            ss << "identifier '" << token.lexeme << "'";
        } else if (token.type == TokenType::TOKEN_INTEGER) {
            ss << "integer literal";
        } else if (token.type == TokenType::TOKEN_FLOAT) {
            ss << "float literal";
        } else if (token.type == TokenType::TOKEN_STRING) {
            ss << "string literal";
        } else {
            ss << "token '" << token.lexeme << "'";
        }
    } else {
        ss << "\n  Found: end of file";
    }
    
    errors.push_back(ss.str());
}

void Parser::synchronize() {
    advance();
    
    while (!isAtEnd()) {
        // After a semicolon, we're at a safe point
        if (previous().type == TokenType::TOKEN_SEMICOLON) return;
        
        // These keywords start new statements - safe synchronization points
        switch (peek().type) {
            case TokenType::TOKEN_KW_FUNC:
            case TokenType::TOKEN_KW_IF:
            case TokenType::TOKEN_KW_ELSE:
            case TokenType::TOKEN_KW_WHILE:
            case TokenType::TOKEN_KW_FOR:
            case TokenType::TOKEN_KW_LOOP:
            case TokenType::TOKEN_KW_TILL:
            case TokenType::TOKEN_KW_WHEN:
            case TokenType::TOKEN_KW_PICK:
            case TokenType::TOKEN_KW_RETURN:
            case TokenType::TOKEN_KW_PASS:
            case TokenType::TOKEN_KW_FAIL:
            case TokenType::TOKEN_KW_BREAK:
            case TokenType::TOKEN_KW_CONTINUE:
            case TokenType::TOKEN_KW_DEFER:
            case TokenType::TOKEN_KW_USE:
            case TokenType::TOKEN_KW_MOD:
            case TokenType::TOKEN_KW_EXTERN:
            case TokenType::TOKEN_KW_STRUCT:
            case TokenType::TOKEN_KW_ENUM:
            case TokenType::TOKEN_KW_TRAIT:
                return;
            default:
                advance();
        }
    }
}

int Parser::getPrecedence(TokenType type) const {
    auto it = precedence.find(type);
    if (it != precedence.end()) {
        return it->second;
    }
    return -1; // Not an operator
}

bool Parser::isBinaryOperator(TokenType type) const {
    return getPrecedence(type) >= 0 && getPrecedence(type) <= 14;
}

bool Parser::isUnaryOperator(TokenType type) const {
    return type == TokenType::TOKEN_MINUS || 
           type == TokenType::TOKEN_BANG ||
           type == TokenType::TOKEN_TILDE ||
           type == TokenType::TOKEN_AT ||
           type == TokenType::TOKEN_LEFT_ARROW ||  // <- for pointer dereference
           type == TokenType::TOKEN_HASH ||
           type == TokenType::TOKEN_DOLLAR;  // $ for safe borrow/reference
           // Note: TOKEN_DOLLAR is BOTH a unary operator ($x for borrow) AND used as iteration variable in till loops
}

bool Parser::isAssignmentOperator(TokenType type) const {
    return type == TokenType::TOKEN_EQUAL ||
           type == TokenType::TOKEN_PLUS_EQUAL ||
           type == TokenType::TOKEN_MINUS_EQUAL ||
           type == TokenType::TOKEN_STAR_EQUAL ||
           type == TokenType::TOKEN_SLASH_EQUAL ||
           type == TokenType::TOKEN_PERCENT_EQUAL;
}

// Expression parsing (precedence climbing algorithm)
ASTNodePtr Parser::parseExpression(int minPrecedence) {
    // Start with unary or primary
    ASTNodePtr left = parseUnary();
    
    if (!left) return nullptr;
    
    // Handle postfix operators
    left = parsePostfix(left);
    
    // Climb precedence for binary operators
    while (!isAtEnd()) {
        Token op = peek();
        int prec = getPrecedence(op.type);
        
        if (prec < minPrecedence) break;
        
        // Special case: ternary operator
        // Syntax: is (condition) : true_expr : false_expr
        // NOT binary: the 'is' starts the whole ternary expression
        // This case should NOT be hit since 'is' is not a binary operator on left side
        if (op.type == TokenType::TOKEN_KW_IS) {
            // This should not happen in normal parsing
            // because 'is' is handled in parsePrimary/parseUnary
            error("Unexpected 'is' in expression - use: is (condition) : true_val : false_val");
            return left;
        }
        
        // Binary operator
        if (isBinaryOperator(op.type)) {
            advance(); // consume operator
            
            // Special case: Range operators (.., ...)
            if (op.type == TokenType::TOKEN_DOT_DOT || op.type == TokenType::TOKEN_DOT_DOT_DOT) {
                ASTNodePtr right = parseExpression(prec + 1);
                
                if (!right) {
                    error("Expected expression after range operator");
                    return nullptr;
                }
                
                bool isExclusive = (op.type == TokenType::TOKEN_DOT_DOT_DOT);
                left = std::make_shared<RangeExpr>(
                    left, right, isExclusive,
                    op.line, op.column
                );
                continue;
            }
            
            // Special case: Forward Pipeline (|>)
            // Transforms: data |> func into func(data)
            //            data |> func(args) into func(data, args)
            if (op.type == TokenType::TOKEN_PIPE_RIGHT) {
                ASTNodePtr funcExpr = parseExpression(prec + 1);
                
                if (!funcExpr) {
                    error("Expected function expression after |>");
                    return nullptr;
                }
                
                // Check if funcExpr is already a CallExpr
                if (CallExpr* call = dynamic_cast<CallExpr*>(funcExpr.get())) {
                    // Insert left as first argument: data |> func(a, b) → func(data, a, b)
                    call->arguments.insert(call->arguments.begin(), left);
                    left = funcExpr;
                } else {
                    // Create new CallExpr: data |> func → func(data)
                    std::vector<ASTNodePtr> args;
                    args.push_back(left);
                    left = std::make_shared<CallExpr>(funcExpr, args, op.line, op.column);
                }
                continue;
            }
            
            // Special case: Backward Pipeline (<|)
            // Transforms: func <| data into func(data)
            if (op.type == TokenType::TOKEN_PIPE_LEFT) {
                // Right-associative: parse with same precedence for chaining
                ASTNodePtr argExpr = parseExpression(prec);
                
                if (!argExpr) {
                    error("Expected argument expression after <|");
                    return nullptr;
                }
                
                // Left should be the function, right is the argument
                std::vector<ASTNodePtr> args;
                args.push_back(argExpr);
                left = std::make_shared<CallExpr>(left, args, op.line, op.column);
                continue;
            }
            
            ASTNodePtr right = parseExpression(prec + 1);
            
            if (!right) {
                error("Expected expression after operator");
                return nullptr;
            }
            
            left = std::make_shared<BinaryExpr>(
                left, op, right,
                op.line, op.column
            );
            continue;
        }
        
        break;
    }
    
    return left;
}

ASTNodePtr Parser::parsePrimary() {
    Token token = peek();
    
    // ========================================================================
    // TYPED INTEGER LITERALS (Zero Implicit Conversion Policy)
    // ========================================================================
    // All integer literals MUST have type suffixes: 42u32, -128i8, 0xFFu16, etc.
    // This prevents accidental type mismatches and implicit conversions
    
    // Unsigned integers
    if (token.type == TokenType::TOKEN_INTEGER_U8 || 
        token.type == TokenType::TOKEN_INTEGER_U16 ||
        token.type == TokenType::TOKEN_INTEGER_U32 ||
        token.type == TokenType::TOKEN_INTEGER_U64 ||
        token.type == TokenType::TOKEN_INTEGER_U128 ||
        token.type == TokenType::TOKEN_INTEGER_U256 ||
        token.type == TokenType::TOKEN_INTEGER_U512 ||
        token.type == TokenType::TOKEN_INTEGER_U1024 ||
        // Signed integers
        token.type == TokenType::TOKEN_INTEGER_I8 ||
        token.type == TokenType::TOKEN_INTEGER_I16 ||
        token.type == TokenType::TOKEN_INTEGER_I32 ||
        token.type == TokenType::TOKEN_INTEGER_I64 ||
        token.type == TokenType::TOKEN_INTEGER_I128 ||
        token.type == TokenType::TOKEN_INTEGER_I256 ||
        token.type == TokenType::TOKEN_INTEGER_I512 ||
        token.type == TokenType::TOKEN_INTEGER_I1024 ||
        // TBB integers
        token.type == TokenType::TOKEN_INTEGER_TBB8 ||
        token.type == TokenType::TOKEN_INTEGER_TBB16 ||
        token.type == TokenType::TOKEN_INTEGER_TBB32 ||
        token.type == TokenType::TOKEN_INTEGER_TBB64) {
        
        std::string type_str = tokenTypeToTypeString(token.type);
        std::string raw_text = token.raw_literal_text;
        int64_t value = token.value.int_value;
        int line = token.line;
        int col = token.column;
        advance();
        
        // If we have high-precision raw text, use factory with raw_text
        if (!raw_text.empty()) {
            return LiteralExpr::makeTypedIntWithRaw(value, raw_text, type_str, line, col);
        } else {
            return LiteralExpr::makeTypedInt(value, type_str, line, col);
        }
    }
    
    // ========================================================================
    // TYPED FLOAT LITERALS (Zero Implicit Conversion Policy)
    // ========================================================================
    // All float literals MUST have type suffixes: 3.14f32, 2.718f64, etc.
    
    if (token.type == TokenType::TOKEN_FLOAT_F32 ||
        token.type == TokenType::TOKEN_FLOAT_F64 ||
        token.type == TokenType::TOKEN_FLOAT_F128 ||
        token.type == TokenType::TOKEN_FLOAT_F256 ||
        token.type == TokenType::TOKEN_FLOAT_F512 ||
        token.type == TokenType::TOKEN_FLOAT_FIX256) {
        
        std::string type_str = tokenTypeToTypeString(token.type);
        std::string raw_text = token.raw_literal_text;
        double value = token.value.float_value;
        int line = token.line;
        int col = token.column;
        advance();
        
        // Use factory method for typed floats
        return LiteralExpr::makeTypedFloat(value, raw_text, type_str, line, col);
    }
    
    // ========================================================================
    // DEPRECATED UNTYPED LITERALS (backward compatibility only)
    // ========================================================================
    // These should trigger errors in lexer now, but keep for migration
    
    // Integer literal (DEPRECATED - lexer should require type suffix)
    if (token.type == TokenType::TOKEN_INTEGER) {
        int64_t value = token.value.int_value;  // Use lexer's parsed value
        std::string raw_text = token.raw_literal_text;
        int line = token.line;
        int col = token.column;
        advance();
        
        // Only use raw_text path if value couldn't fit in int64 (lexer sets value to 0 in that case)
        if (!raw_text.empty() && value == 0 && raw_text != "0") {
            // High-precision literal that exceeded int64 range
            return std::make_shared<LiteralExpr>((int64_t)0, raw_text, line, col, true);
        } else {
            // Normal literal, use parsed value
            return std::make_shared<LiteralExpr>(value, line, col);
        }
    }
    
    // Float literal (DEPRECATED - lexer should require type suffix)
    if (token.type == TokenType::TOKEN_FLOAT) {
        std::string raw_text = token.raw_literal_text;
        double value = token.value.float_value;
        int line = token.line;
        int col = token.column;
        advance();

        if (!raw_text.empty()) {
            return std::make_shared<LiteralExpr>(value, raw_text, line, col);
        } else {
            return std::make_shared<LiteralExpr>(value, line, col);
        }
    }
    
    // ========================================================================
    // SPECIAL NUMERIC LITERALS (ternary/nonary - not subject to suffix policy)
    // ========================================================================
    
    // Ternary literal (balanced ternary)
    if (token.type == TokenType::TOKEN_TERNARY) {
        int line = token.line;
        int col = token.column;
        advance();
        // The lexer has already converted the ternary string to int64_t
        // and stored it in the token's value.int_value
        int64_t value = token.value.int_value;
        return std::make_shared<LiteralExpr>(value, line, col);
    }
    
    // Nonary literal (balanced nonary)
    if (token.type == TokenType::TOKEN_NONARY) {
        int line = token.line;
        int col = token.column;
        advance();
        // The lexer has already converted the nonary string to int64_t
        // and stored it in the token's value.int_value
        int64_t value = token.value.int_value;
        return std::make_shared<LiteralExpr>(value, line, col);
    }
    
    // ========================================================================
    // OTHER LITERALS (strings, chars, booleans, null, etc.)
    // ========================================================================
    
    // String literal
    if (token.type == TokenType::TOKEN_STRING) {
        std::string value = token.string_value;  // Save before advance()
        int line = token.line;
        int col = token.column;
        advance();
        // FIX: Use string_value instead of lexeme to get unquoted string
        return std::make_shared<LiteralExpr>(value, line, col);
    }
    
    // Character literal: 'a', '+', '&', etc.
    // Convert to int64_t (byte value) for compatibility with type system
    if (token.type == TokenType::TOKEN_CHAR) {
        std::string char_str = token.string_value;  // Single character as string
        int line = token.line;
        int col = token.column;
        advance();
        
        // Convert character to its numeric byte value
        if (char_str.empty()) {
            error("Empty character literal");
            return std::make_shared<LiteralExpr>((int64_t)0, line, col);
        }
        int64_t byte_value = static_cast<int64_t>(static_cast<unsigned char>(char_str[0]));
        return std::make_shared<LiteralExpr>(byte_value, line, col);
    }
    
    // Template literal: `text &{expr} more text`
    if (token.type == TokenType::TOKEN_TEMPLATE_START) {
        return parseTemplateLiteral();
    }
    
    // Boolean literal
    if (token.type == TokenType::TOKEN_KW_TRUE || token.type == TokenType::TOKEN_KW_FALSE) {
        bool value = (token.type == TokenType::TOKEN_KW_TRUE);
        int line = token.line;
        int col = token.column;
        advance();
        return std::make_shared<LiteralExpr>(value, line, col);
    }
    
    // Null literal (C FFI only)
    if (token.type == TokenType::TOKEN_KW_NULL) {
        int line = token.line;
        int col = token.column;
        advance();
        return std::make_shared<LiteralExpr>(std::monostate{}, line, col);
    }
    
    // NIL literal (Aria native - absence of value)
    if (token.type == TokenType::TOKEN_KW_NIL) {
        int line = token.line;
        int col = token.column;
        advance();
        return std::make_shared<LiteralExpr>(std::monostate{}, line, col);
    }
    
    // ERR literal (TBB error sentinel value)
    if (token.type == TokenType::TOKEN_KW_ERR) {
        int line = token.line;
        int col = token.column;
        advance();
        // ERR is represented as a special literal
        // The semantic analyzer will handle type-specific ERR values
        // (e.g., -128 for tbb8, -32768 for tbb16, etc.)
        // IMPORTANT: Use std::string() constructor to avoid bool conversion!
        return std::make_shared<LiteralExpr>(std::string("ERR"), line, col);
    }
    
    // Identifier
    if (token.type == TokenType::TOKEN_IDENTIFIER) {
        std::string lexeme = token.lexeme;  // Save before advance()
        int line = token.line;
        int col = token.column;
        advance();
        return std::make_shared<IdentifierExpr>(lexeme, line, col);
    }
    
    // Type keywords as namespace expressions (for static method calls)
    // Allow: string.from_char(), array.new(), etc.
    if (isTypeKeyword(token.type) && peekNext().type == TokenType::TOKEN_DOT) {
        std::string typeName = token.lexeme;  // e.g., "string", "array", "int64"
        int line = token.line;
        int col = token.column;
        advance();  // consume type keyword
        // Return as identifier so it can be used in member access chain
        return std::make_shared<IdentifierExpr>(typeName, line, col);
    }
    
    // $ iteration variable (for till/loop constructs)
    if (token.type == TokenType::TOKEN_DOLLAR) {
        int line = token.line;
        int col = token.column;
        advance();
        return std::make_shared<IdentifierExpr>("$", line, col);
    }

    // Ternary expression: is (condition) : true_value : false_value
    if (token.type == TokenType::TOKEN_KW_IS) {
        int line = token.line;
        int col = token.column;
        advance(); // consume 'is'
        
        consume(TokenType::TOKEN_LEFT_PAREN, "Expected '(' after 'is'");
        ASTNodePtr condition = parseExpression();
        if (!condition) {
            error("Expected condition expression in ternary");
            return nullptr;
        }
        consume(TokenType::TOKEN_RIGHT_PAREN, "Expected ')' after ternary condition");
        
        consume(TokenType::TOKEN_COLON, "Expected ':' after ternary condition");
        ASTNodePtr trueValue = parseExpression();
        if (!trueValue) {
            error("Expected true value in ternary");
            return nullptr;
        }
        
        consume(TokenType::TOKEN_COLON, "Expected second ':' in ternary expression");
        ASTNodePtr falseValue = parseExpression();
        if (!falseValue) {
            error("Expected false value in ternary");
            return nullptr;
        }
        
        return std::make_shared<TernaryExpr>(
            condition, trueValue, falseValue,
            line, col
        );
    }

    // Allow 'func' keyword to be used as identifier (for function name in test cases)
    if (token.type == TokenType::TOKEN_KW_FUNC) {
        std::string lexeme = token.lexeme;  // Save before advance()
        int line = token.line;
        int col = token.column;
        advance();
        return std::make_shared<IdentifierExpr>(lexeme, line, col);
    }

    // FIX: Allow 'obj' keyword to be used as identifier (common name in tests)
    // The test case "obj.field" fails because 'obj' is parsed as TOKEN_KW_OBJ
    if (token.type == TokenType::TOKEN_KW_OBJ) {
        std::string lexeme = token.lexeme;  // Save before advance()
        int line = token.line;
        int col = token.column;
        advance();
        return std::make_shared<IdentifierExpr>(lexeme, line, col);
    }
    
    // Allow 'result' keyword to be used as identifier (common name in tests)
    if (token.type == TokenType::TOKEN_KW_RESULT) {
        std::string lexeme = token.lexeme;  // Save before advance()
        int line = token.line;
        int col = token.column;
        advance();
        return std::make_shared<IdentifierExpr>(lexeme, line, col);
    }

    // Allow 'process' keyword to be used as identifier (common function name)
    if (token.type == TokenType::TOKEN_KW_PROCESS) {
        std::string lexeme = token.lexeme;  // Save before advance()
        int line = token.line;
        int col = token.column;
        advance();
        return std::make_shared<IdentifierExpr>(lexeme, line, col);
    }

    // Parenthesized expression
    if (token.type == TokenType::TOKEN_LEFT_PAREN) {
        advance();
        ASTNodePtr expr = parseExpression();
        consume(TokenType::TOKEN_RIGHT_PAREN, "Expected ')' after expression");
        return expr;
    }
    
    // Array literal
    if (token.type == TokenType::TOKEN_LEFT_BRACKET) {
        return parseArrayLiteral();
    }
    
    // Object literal
    if (token.type == TokenType::TOKEN_LEFT_BRACE) {
        return parseObjectLiteral();
    }
    
    // Template literal
    if (token.type == TokenType::TOKEN_TEMPLATE_START) {
        return parseTemplateLiteral();
    }
    
    // Vector constructors: vec2(x, y), vec3(x, y, z), vec9(c0, ..., c8)
    if (token.type == TokenType::TOKEN_KW_VEC2 ||
        token.type == TokenType::TOKEN_KW_VEC3 ||
        token.type == TokenType::TOKEN_KW_VEC9) {
        
        int dimension = (token.type == TokenType::TOKEN_KW_VEC2) ? 2 :
                       (token.type == TokenType::TOKEN_KW_VEC3) ? 3 : 9;
        int line = token.line;
        int col = token.column;
        advance(); // consume vec2/vec3/vec9
        
        consume(TokenType::TOKEN_LEFT_PAREN, "Expected '(' after vector type");
        
        std::vector<ASTNodePtr> components;
        
        // Parse components
        if (peek().type != TokenType::TOKEN_RIGHT_PAREN) {
            do {
                ASTNodePtr component = parseExpression();
                if (!component) {
                    error("Expected component expression in vector constructor");
                    return nullptr;
                }
                components.push_back(component);
                
                if (peek().type == TokenType::TOKEN_COMMA) {
                    advance();
                } else {
                    break;
                }
            } while (true);
        }
        
        consume(TokenType::TOKEN_RIGHT_PAREN, "Expected ')' after vector components");
        
        // Validate component count
        if (components.size() != static_cast<size_t>(dimension)) {
            std::ostringstream oss;
            oss << "vec" << dimension << " requires " << dimension << " components, got " << components.size();
            error(oss.str());
            return nullptr;
        }
        
        return std::make_shared<VectorConstructorExpr>(dimension, components, line, col);
    }
    
    error("Expected expression");
    return nullptr;
}

ASTNodePtr Parser::parseUnary() {
    Token token = peek();
    
    // Check for await expression: await <expression>
    if (token.type == TokenType::TOKEN_KW_AWAIT) {
        advance(); // consume 'await'
        
        // Parse the operand as a complete postfix expression (including calls)
        // This ensures await wraps the entire expression, not just the identifier
        ASTNodePtr operand = parsePrimary();
        if (!operand) {
            error("Expected expression after 'await'");
            return nullptr;
        }
        
        // Apply postfix operators to the operand (function calls, indexing, etc.)
        operand = parsePostfix(operand);
        
        return std::make_shared<AwaitExpr>(operand, token.line, token.column);
    }
    
    // Check for cast expression: @cast<Type>(expr) or @cast_unchecked<Type>(expr)
    if (token.type == TokenType::TOKEN_KW_CAST || token.type == TokenType::TOKEN_KW_CAST_UNCHECKED) {
        int line = token.line;
        int col = token.column;
        bool isUnchecked = (token.type == TokenType::TOKEN_KW_CAST_UNCHECKED);
        advance(); // consume '@cast' or '@cast_unchecked'
        
        // Expect '<'
        consume(TokenType::TOKEN_LESS, "Expected '<' after @cast");
        
        // Parse target type
        ASTNodePtr targetTypeNode = parseType();
        if (!targetTypeNode) {
            error("Expected type after '<' in @cast");
            return nullptr;
        }
        
        // Extract type name from TypeAnnotation
        std::string targetTypeName = "";
        if (targetTypeNode->type == ASTNode::NodeType::TYPE_ANNOTATION) {
            targetTypeName = targetTypeNode->toString(); // Simple approach for now
        }
        
        // Expect '>'
        consume(TokenType::TOKEN_GREATER, "Expected '>' after type in @cast");
        
        // Expect '('
        consume(TokenType::TOKEN_LEFT_PAREN, "Expected '(' after @cast<Type>");
        
        // Parse expression to cast
        ASTNodePtr expr = parseExpression();
        if (!expr) {
            error("Expected expression in @cast()");
            return nullptr;
        }
        
        // Expect ')'
        consume(TokenType::TOKEN_RIGHT_PAREN, "Expected ')' after expression in @cast");
        
        return std::make_shared<CastExpr>(expr, targetTypeNode, targetTypeName, isUnchecked, line, col);
    }
    
    // Check for move expression: move(variable)
    if (token.type == TokenType::TOKEN_KW_MOVE) {
        int line = token.line;
        int col = token.column;
        advance(); // consume 'move'
        
        consume(TokenType::TOKEN_LEFT_PAREN, "Expected '(' after 'move'");
        
        // Parse the variable being moved
        ASTNodePtr varExpr = parseExpression();
        
        if (!varExpr) {
            error("Expected variable expression in move()");
            return nullptr;
        }
        
        // Extract variable name - must be an identifier
        std::string varName = "";
        if (IdentifierExpr* ident = dynamic_cast<IdentifierExpr*>(varExpr.get())) {
            varName = ident->name;
        } else {
            error("move() requires a simple variable name, not a complex expression");
            return nullptr;
        }
        
        consume(TokenType::TOKEN_RIGHT_PAREN, "Expected ')' after variable in move()");
        
        return std::make_shared<MoveExpr>(varName, varExpr, line, col);
    }
    
    // Special case: bare $ (iteration variable)
    // If we see $ followed by a token that can't be an operand (like ; , ) etc.),
    // then $ is the iteration variable, not a unary operator.
    if (token.type == TokenType::TOKEN_DOLLAR) {
        Token next_token = peekNext();
        // Check if next token can't be an operand
        if (next_token.type == TokenType::TOKEN_SEMICOLON ||
            next_token.type == TokenType::TOKEN_COMMA ||
            next_token.type == TokenType::TOKEN_RIGHT_PAREN ||
            next_token.type == TokenType::TOKEN_RIGHT_BRACE ||
            next_token.type == TokenType::TOKEN_RIGHT_BRACKET ||
            isBinaryOperator(next_token.type)) {
            // Bare $ - treat as iteration variable (let parsePrimary handle it)
            return parsePrimary();
        }
        // Otherwise, $ is a unary operator (fall through to normal handling)
    }
    
    if (isUnaryOperator(token.type)) {
        advance();
        ASTNodePtr operand = parseUnary(); // Right-associative
        
        if (!operand) {
            error("Expected expression after unary operator");
            return nullptr;
        }
        
        // FIX: Explicitly pass false for isPostfix, otherwise token.line (int) is converted to true
        auto unaryExpr = std::make_shared<UnaryExpr>(token, operand, false, token.line, token.column);
        
        // Set borrow checker annotations for $ and # operators
        if (token.type == TokenType::TOKEN_DOLLAR) {
            unaryExpr->creates_loan = true;
            // Extract target variable name from operand if it's an identifier
            if (operand && operand->type == ASTNode::NodeType::IDENTIFIER) {
                auto identExpr = std::static_pointer_cast<IdentifierExpr>(operand);
                unaryExpr->loan_target = identExpr->name;
            }
        } else if (token.type == TokenType::TOKEN_HASH) {
            unaryExpr->creates_pin = true;
            // Extract target variable name from operand if it's an identifier
            if (operand && operand->type == ASTNode::NodeType::IDENTIFIER) {
                auto identExpr = std::static_pointer_cast<IdentifierExpr>(operand);
                unaryExpr->pin_target = identExpr->name;
            }
        }
        
        return unaryExpr;
    }
    
    return parsePrimary();
}

ASTNodePtr Parser::parsePostfix(ASTNodePtr expr) {
    while (!isAtEnd()) {
        Token token = peek();
        
        // Check for struct literal: Identifier{ field: value, ... }
        // Only if expr is an IdentifierExpr
        if (token.type == TokenType::TOKEN_LEFT_BRACE && 
            expr && expr->type == ASTNode::NodeType::IDENTIFIER) {
            auto identExpr = std::static_pointer_cast<IdentifierExpr>(expr);
            std::string typeName = identExpr->name;
            
            advance(); // consume '{'
            
            std::vector<ObjectLiteralExpr::Field> fields;
            
            while (!check(TokenType::TOKEN_RIGHT_BRACE) && !isAtEnd()) {
                // Parse field name
                Token fieldNameToken = consume(TokenType::TOKEN_IDENTIFIER, "Expected field name in struct literal");
                std::string fieldName = fieldNameToken.lexeme;
                
                // Expect colon
                consume(TokenType::TOKEN_COLON, "Expected ':' after field name");
                
                // Parse field value
                ASTNodePtr fieldValue = parseExpression();
                if (!fieldValue) {
                    error("Expected field value");
                    return nullptr;
                }
                
                fields.push_back(ObjectLiteralExpr::Field(fieldName, fieldValue));
                
                // Allow trailing comma
                if (!check(TokenType::TOKEN_RIGHT_BRACE)) {
                    consume(TokenType::TOKEN_COMMA, "Expected ',' or '}' after field");
                }
            }
            
            consume(TokenType::TOKEN_RIGHT_BRACE, "Expected '}' after struct fields");
            
            // Create ObjectLiteralExpr with type_name set
            return std::make_shared<ObjectLiteralExpr>(
                fields,
                typeName,  // Set type name for struct literals
                token.line,
                token.column
            );
        }
        
        // Check for turbofish syntax: ::<T, U> followed by function call
        // This must be checked BEFORE normal function call to disambiguate from comparison
        // Look for two consecutive colons followed by <
        if (token.type == TokenType::TOKEN_COLON && 
            current + 1 < tokens.size() && tokens[current + 1].type == TokenType::TOKEN_COLON &&
            current + 2 < tokens.size() && tokens[current + 2].type == TokenType::TOKEN_LESS) {
            
            advance(); // consume first ':'
            advance(); // consume second ':'
            // Now parse the explicit type arguments
            std::vector<std::string> typeArgs = parseExplicitTypeArgs();
            
            // After turbofish, expect function call
            if (check(TokenType::TOKEN_LEFT_PAREN)) {
                Token leftParen = advance();
                std::vector<ASTNodePtr> arguments;
                
                if (!check(TokenType::TOKEN_RIGHT_PAREN)) {
                    do {
                        ASTNodePtr arg = parseExpression();
                        if (arg) {
                            arguments.push_back(arg);
                        }
                    } while (match(TokenType::TOKEN_COMMA));
                }
                
                consume(TokenType::TOKEN_RIGHT_PAREN, "Expected ')' after function arguments");
                
                // Create CallExpr with explicit type arguments
                expr = std::make_shared<CallExpr>(expr, arguments, typeArgs, leftParen.line, leftParen.column);
            } else {
                error("Expected '(' after turbofish type arguments");
            }
            continue;
        }
        
        // Function call (without turbofish)
        if (token.type == TokenType::TOKEN_LEFT_PAREN) {
            expr = parseCallExpression(expr);
            continue;
        }
        
        // Array index
        if (token.type == TokenType::TOKEN_LEFT_BRACKET) {
            expr = parseIndexExpression(expr);
            continue;
        }
        
        // Member access or safe navigation
        if (token.type == TokenType::TOKEN_DOT || 
            token.type == TokenType::TOKEN_ARROW ||
            token.type == TokenType::TOKEN_SAFE_NAV) {
            expr = parseMemberExpression(expr);
            continue;
        }
        
        // Postfix increment/decrement
        if (token.type == TokenType::TOKEN_PLUS_PLUS || token.type == TokenType::TOKEN_MINUS_MINUS) {
            advance();
            expr = std::make_shared<UnaryExpr>(token, expr, true, token.line, token.column);
            continue;
        }
        
        // Unwrap operator: result ? default_value
        if (token.type == TokenType::TOKEN_QUESTION) {
            Token unwrapToken = advance(); // consume '?'
            
            // Parse the default value expression
            ASTNodePtr defaultValue = parseExpression();
            if (!defaultValue) {
                error("Expected default value after '?' operator");
                return nullptr;
            }
            
            // Create UnwrapExpr with result on left, default on right
            expr = std::make_shared<UnwrapExpr>(
                expr, 
                defaultValue,
                unwrapToken.line,
                unwrapToken.column,
                false  // Not null coalesce, it's Result unwrap
            );
            continue;
        }
        
        // Null coalescing operator: nullable ?? default_value
        if (token.type == TokenType::TOKEN_NULL_COALESCE) {
            Token coalesceToken = advance(); // consume '??'
            
            // Parse the default value expression
            ASTNodePtr defaultValue = parseExpression();
            if (!defaultValue) {
                error("Expected default value after '??' operator");
                return nullptr;
            }
            
            // Create UnwrapExpr with isNullCoalesce=true
            expr = std::make_shared<UnwrapExpr>(
                expr, 
                defaultValue,
                coalesceToken.line,
                coalesceToken.column,
                true  // This is null coalesce (??), not Result unwrap (?)
            );
            continue;
        }
        
        break;
    }
    
    return expr;
}

ASTNodePtr Parser::parseCallExpression(ASTNodePtr callee) {
    Token leftParen = advance(); // consume '('
    
    std::vector<ASTNodePtr> arguments;
    
    if (!check(TokenType::TOKEN_RIGHT_PAREN)) {
        do {
            ASTNodePtr arg = parseExpression();
            if (arg) {
                arguments.push_back(arg);
            }
        } while (match(TokenType::TOKEN_COMMA));
    }
    
    consume(TokenType::TOKEN_RIGHT_PAREN, "Expected ')' after function arguments");
    
    return std::make_shared<CallExpr>(callee, arguments, leftParen.line, leftParen.column);
}

ASTNodePtr Parser::parseIndexExpression(ASTNodePtr array) {
    Token leftBracket = advance(); // consume '['
    
    ASTNodePtr index = parseExpression();
    if (!index) {
        error("Expected index expression");
        return array;
    }
    
    consume(TokenType::TOKEN_RIGHT_BRACKET, "Expected ']' after array index");
    
    return std::make_shared<IndexExpr>(array, index, leftBracket.line, leftBracket.column);
}

ASTNodePtr Parser::parseMemberExpression(ASTNodePtr object) {
    Token op = advance(); // consume '.', '->', or '?.'
    
    bool isPointerAccess = (op.type == TokenType::TOKEN_ARROW);
    bool isSafeNav = (op.type == TokenType::TOKEN_SAFE_NAV);
    
    Token memberToken = consume(TokenType::TOKEN_IDENTIFIER, "Expected member name after '.' or '->'");
    std::string memberName = memberToken.lexeme;
    
    auto memberExpr = std::make_shared<MemberAccessExpr>(
        object, memberName, isPointerAccess, isSafeNav,
        op.line, op.column
    );
    
    return memberExpr;
}

ASTNodePtr Parser::parseArrayLiteral() {
    Token leftBracket = advance(); // consume '['
    
    std::vector<ASTNodePtr> elements;
    
    if (!check(TokenType::TOKEN_RIGHT_BRACKET)) {
        do {
            ASTNodePtr element = parseExpression();
            if (element) {
                elements.push_back(element);
            }
        } while (match(TokenType::TOKEN_COMMA));
    }
    
    consume(TokenType::TOKEN_RIGHT_BRACKET, "Expected ']' after array elements");
    
    return std::make_shared<ArrayLiteralExpr>(elements, leftBracket.line, leftBracket.column);
}

ASTNodePtr Parser::parseObjectLiteral() {
    Token braceToken = advance(); // consume '{'
    
    std::vector<ObjectLiteralExpr::Field> fields;
    
    while (!check(TokenType::TOKEN_RIGHT_BRACE) && !isAtEnd()) {
        // Parse field name
        Token fieldNameToken = consume(TokenType::TOKEN_IDENTIFIER, "Expected field name in object literal");
        std::string fieldName = fieldNameToken.lexeme;
        
        // Expect colon
        consume(TokenType::TOKEN_COLON, "Expected ':' after field name");
        
        // Parse field value
        ASTNodePtr fieldValue = parseExpression();
        if (!fieldValue) {
            error("Expected field value");
            return nullptr;
        }
        
        fields.push_back(ObjectLiteralExpr::Field(fieldName, fieldValue));
        
        // Allow trailing comma
        if (!check(TokenType::TOKEN_RIGHT_BRACE)) {
            consume(TokenType::TOKEN_COMMA, "Expected ',' or '}' after field");
        }
    }
    
    consume(TokenType::TOKEN_RIGHT_BRACE, "Expected '}' after object fields");
    
    // Create ObjectLiteralExpr without type_name (dynamic object)
    return std::make_shared<ObjectLiteralExpr>(
        fields,
        "",  // No type name for dynamic objects
        braceToken.line,
        braceToken.column
    );
}

ASTNodePtr Parser::parseTemplateLiteral() {
    Token startToken = advance(); // consume TOKEN_TEMPLATE_START
    
    auto templateExpr = std::make_shared<TemplateLiteralExpr>(startToken.line, startToken.column);
    
    while (!check(TokenType::TOKEN_TEMPLATE_END) && !isAtEnd()) {
        // Expect TOKEN_TEMPLATE_PART (string part)
        if (check(TokenType::TOKEN_TEMPLATE_PART)) {
            Token partToken = advance();
            templateExpr->parts.push_back(partToken.string_value);
        } else {
            // Empty part (e.g., at the start or after interpolation)
            templateExpr->parts.push_back("");
        }
        
        // Check for interpolation
        if (check(TokenType::TOKEN_INTERP_START)) {
            advance(); // consume TOKEN_INTERP_START
            
            // Parse the expression inside the interpolation
            ASTNodePtr expr = parseExpression();
            if (!expr) {
                error("Expected expression inside template interpolation");
                return nullptr;
            }
            
            // Store the shared_ptr directly
            templateExpr->interpolations.push_back(expr);
            
            // Expect TOKEN_INTERP_END
            if (!check(TokenType::TOKEN_INTERP_END)) {
                error("Expected '}' to close template interpolation");
                return nullptr;
            }
            advance(); // consume TOKEN_INTERP_END
        }
    }
    
    // Add final part if we haven't added enough parts yet
    // (template can end with a part after the last interpolation)
    if (check(TokenType::TOKEN_TEMPLATE_PART)) {
        Token partToken = advance();
        templateExpr->parts.push_back(partToken.string_value);
    }
    
    // Consume TOKEN_TEMPLATE_END
    if (!check(TokenType::TOKEN_TEMPLATE_END)) {
        error("Expected '`' to close template literal");
        return nullptr;
    }
    advance();
    
    return templateExpr;
}

/**
 * Parse lambda expressions (Closures - Phase 4.5.2)
 * 
 * Syntax (from aria_specs.txt):
 * Lambda syntax is just: returnType(type:param, type:param) { body }
 * NO arrow operator (=>), NO special keywords
 * 
 * Examples:
 * - int8(int8:a, int8:b) { return a + b; }
 * - Anonymous inline: returnType(params) { body }
 * - With immediate execution: returnType(params) { body }(args);
 * 
 * Captures are inferred during semantic analysis.
 */
ASTNodePtr Parser::parseLambda() {
    using namespace frontend;
    
    // Lambda starts with optional return type, then (params) { body }
    // This is called when we've already determined we're parsing a lambda
    // (e.g., saw type keyword followed by left paren with typed params)
    
    std::string returnTypeName;
    std::vector<ASTNodePtr> parameters;
    
    // Check if we have a return type annotation
    // e.g., int64(int64:x, int64:y) { ... }
    if (isTypeKeyword(peek().type)) {
        Token retTypeToken = advance();
        returnTypeName = retTypeToken.lexeme;
    }
    
    // Expect opening parenthesis for parameters
    consume(TokenType::TOKEN_LEFT_PAREN, "Expected '(' for lambda parameters");
    
    // Parse parameters with required type annotations: type:name
    while (!check(TokenType::TOKEN_RIGHT_PAREN) && !isAtEnd()) {
        // Parameters MUST be typed: int64:x, string:name, etc.
        if (!isTypeKeyword(peek().type)) {
            error("Lambda parameters must have type annotations (type:name)");
            return nullptr;
        }
        
        // Parse parameter type using parseType() for full type support
        ASTNodePtr paramTypeNode = parseType();
        if (!paramTypeNode) {
            error("Expected parameter type in lambda");
            return nullptr;
        }
        
        consume(TokenType::TOKEN_COLON, "Expected ':' after parameter type");
        
        Token nameToken = consume(TokenType::TOKEN_IDENTIFIER, "Expected parameter name");
        std::string paramName = nameToken.lexeme;
        
        // Create parameter node: ParameterNode(typeNode, paramName, defaultValue, line, column)
        auto param = std::make_shared<ParameterNode>(
            paramTypeNode,  // Pass ASTNodePtr instead of string
            paramName, 
            nullptr,  // No default value for lambda params
            current > 0 ? tokens[current-1].line : 0, 
            current > 0 ? tokens[current-1].column : 0
        );
        parameters.push_back(param);
        
        // Check for more parameters
        if (!check(TokenType::TOKEN_RIGHT_PAREN)) {
            consume(TokenType::TOKEN_COMMA, "Expected ',' between parameters");
        }
    }
    
    consume(TokenType::TOKEN_RIGHT_PAREN, "Expected ')' after lambda parameters");
    
    // Parse body - must be a block { ... }
    // No arrow operator in Aria!
    if (!check(TokenType::TOKEN_LEFT_BRACE)) {
        error("Expected '{' for lambda body (no arrow operator in Aria)");
        return nullptr;
    }
    
    ASTNodePtr body = parseBlock();
    
    // Create and return the lambda node
    auto lambdaNode = std::make_shared<LambdaExpr>(
        parameters,
        returnTypeName,
        body,
        body->line,
        body->column
    );
    
    return lambdaNode;
}

// ============================================================================
// Phase 2.4: Statement Parsing
// ============================================================================

// Check if token is a type keyword
bool Parser::isTypeKeyword(frontend::TokenType type) const {
    using namespace frontend;
    return (
        // Signed integers
        type == TokenType::TOKEN_KW_INT1 || type == TokenType::TOKEN_KW_INT2 ||
        type == TokenType::TOKEN_KW_INT4 || type == TokenType::TOKEN_KW_INT8 ||
        type == TokenType::TOKEN_KW_INT16 || type == TokenType::TOKEN_KW_INT32 ||
        type == TokenType::TOKEN_KW_INT64 || type == TokenType::TOKEN_KW_INT128 ||
        type == TokenType::TOKEN_KW_INT256 || type == TokenType::TOKEN_KW_INT512 ||
        type == TokenType::TOKEN_KW_INT1024 ||
        // Unsigned integers
        type == TokenType::TOKEN_KW_UINT1 || type == TokenType::TOKEN_KW_UINT2 ||
        type == TokenType::TOKEN_KW_UINT4 || type == TokenType::TOKEN_KW_UINT8 ||
        type == TokenType::TOKEN_KW_UINT16 || type == TokenType::TOKEN_KW_UINT32 ||
        type == TokenType::TOKEN_KW_UINT64 || type == TokenType::TOKEN_KW_UINT128 ||
        type == TokenType::TOKEN_KW_UINT256 || type == TokenType::TOKEN_KW_UINT512 ||
        type == TokenType::TOKEN_KW_UINT1024 ||
        // TBB types
        type == TokenType::TOKEN_KW_TBB8 || type == TokenType::TOKEN_KW_TBB16 ||
        type == TokenType::TOKEN_KW_TBB32 || type == TokenType::TOKEN_KW_TBB64 ||
        // Frac types (exact rational arithmetic)
        type == TokenType::TOKEN_KW_FRAC8 || type == TokenType::TOKEN_KW_FRAC16 ||
        type == TokenType::TOKEN_KW_FRAC32 || type == TokenType::TOKEN_KW_FRAC64 ||
        // TFP types (twisted floating point)
        type == TokenType::TOKEN_KW_TFP32 || type == TokenType::TOKEN_KW_TFP64 ||
        // Floating point
        type == TokenType::TOKEN_KW_FLT32 || type == TokenType::TOKEN_KW_FLT64 ||
        type == TokenType::TOKEN_KW_FLT128 || type == TokenType::TOKEN_KW_FLT256 ||
        type == TokenType::TOKEN_KW_FLT512 ||
        // Fixed point (deterministic physics)
        type == TokenType::TOKEN_KW_FIX256 ||
        // Other types
        type == TokenType::TOKEN_KW_BOOL || type == TokenType::TOKEN_KW_STRING ||
        type == TokenType::TOKEN_KW_DYN || type == TokenType::TOKEN_KW_OBJ ||
        type == TokenType::TOKEN_KW_RESULT || type == TokenType::TOKEN_KW_ARRAY ||
        type == TokenType::TOKEN_KW_STRUCT ||
        // Note: TOKEN_KW_FUNC removed - handled as identifier in expressions
        // Balanced ternary/nonary
        type == TokenType::TOKEN_KW_TRIT || type == TokenType::TOKEN_KW_TRYTE ||
        type == TokenType::TOKEN_KW_NIT || type == TokenType::TOKEN_KW_NYTE ||
        // Math types
        type == TokenType::TOKEN_KW_VEC2 || type == TokenType::TOKEN_KW_VEC3 ||
        type == TokenType::TOKEN_KW_VEC9 || type == TokenType::TOKEN_KW_TMATRIX ||
        type == TokenType::TOKEN_KW_TTENSOR || type == TokenType::TOKEN_KW_MATRIX ||
        type == TokenType::TOKEN_KW_TENSOR ||
        // I/O types
        type == TokenType::TOKEN_KW_BINARY || type == TokenType::TOKEN_KW_BUFFER ||
        type == TokenType::TOKEN_KW_STREAM || type == TokenType::TOKEN_KW_PROCESS ||
        type == TokenType::TOKEN_KW_PIPE || type == TokenType::TOKEN_KW_DEBUG ||
        type == TokenType::TOKEN_KW_LOG
    );
}

// Convert typed literal TokenType to type string for AST nodes
// TOKEN_INTEGER_U32 -> "u32", TOKEN_FLOAT_F64 -> "f64", etc.
std::string Parser::tokenTypeToTypeString(frontend::TokenType type) const {
    using namespace frontend;
    
    // Unsigned integers
    if (type == TokenType::TOKEN_INTEGER_U8) return "u8";
    if (type == TokenType::TOKEN_INTEGER_U16) return "u16";
    if (type == TokenType::TOKEN_INTEGER_U32) return "u32";
    if (type == TokenType::TOKEN_INTEGER_U64) return "u64";
    if (type == TokenType::TOKEN_INTEGER_U128) return "u128";
    if (type == TokenType::TOKEN_INTEGER_U256) return "u256";
    if (type == TokenType::TOKEN_INTEGER_U512) return "u512";
    if (type == TokenType::TOKEN_INTEGER_U1024) return "u1024";
    
    // Signed integers
    if (type == TokenType::TOKEN_INTEGER_I8) return "i8";
    if (type == TokenType::TOKEN_INTEGER_I16) return "i16";
    if (type == TokenType::TOKEN_INTEGER_I32) return "i32";
    if (type == TokenType::TOKEN_INTEGER_I64) return "i64";
    if (type == TokenType::TOKEN_INTEGER_I128) return "i128";
    if (type == TokenType::TOKEN_INTEGER_I256) return "i256";
    if (type == TokenType::TOKEN_INTEGER_I512) return "i512";
    if (type == TokenType::TOKEN_INTEGER_I1024) return "i1024";
    
    // TBB (Twisted Balanced Binary) integers
    if (type == TokenType::TOKEN_INTEGER_TBB8) return "tbb8";
    if (type == TokenType::TOKEN_INTEGER_TBB16) return "tbb16";
    if (type == TokenType::TOKEN_INTEGER_TBB32) return "tbb32";
    if (type == TokenType::TOKEN_INTEGER_TBB64) return "tbb64";
    
    // Float types
    if (type == TokenType::TOKEN_FLOAT_F32) return "f32";
    if (type == TokenType::TOKEN_FLOAT_F64) return "f64";
    if (type == TokenType::TOKEN_FLOAT_F128) return "f128";
    if (type == TokenType::TOKEN_FLOAT_F256) return "f256";
    if (type == TokenType::TOKEN_FLOAT_F512) return "f512";
    if (type == TokenType::TOKEN_FLOAT_FIX256) return "fix256";
    
    // Should not reach here if called with proper typed literal token
    return "";
}

// Main statement dispatcher
ASTNodePtr Parser::parseStatement() {
    using namespace frontend;
    
    // Check for module imports
    if (match(TokenType::TOKEN_KW_USE)) {
        return parseUseStatement();
    }
    
    // Check for public module definitions: pub mod name
    if (peek().type == TokenType::TOKEN_KW_PUB) {
        size_t saved = current;
        advance(); // consume 'pub'
        
        // Check for pub mod
        if (match(TokenType::TOKEN_KW_MOD)) {
            auto modStmt = parseModStatement();
            // Set public flag
            if (modStmt && modStmt->type == ASTNode::NodeType::MOD) {
                auto mod = std::static_pointer_cast<ModStmt>(modStmt);
                mod->isPublic = true;
            }
            return modStmt;
        }
        
        // Check for pub func
        if (match(TokenType::TOKEN_KW_FUNC)) {
            auto funcStmt = parseFuncDecl();
            // Set public flag
            if (funcStmt && funcStmt->type == ASTNode::NodeType::FUNC_DECL) {
                auto func = std::static_pointer_cast<FuncDeclStmt>(funcStmt);
                func->isPublic = true;
            }
            return funcStmt;
        }
        
        // Not pub mod or pub func, restore position and continue
        current = saved;
    }
    
    // Check for module definitions
    if (match(TokenType::TOKEN_KW_MOD)) {
        return parseModStatement();
    }
    
    // Check for extern blocks (FFI)
    if (match(TokenType::TOKEN_KW_EXTERN)) {
        return parseExternStatement();
    }
    
    // Check for struct declarations
    if (match(TokenType::TOKEN_KW_STRUCT)) {
        return parseStructDecl();
    }
    
    // Check for enum declarations
    if (match(TokenType::TOKEN_KW_ENUM)) {
        return parseEnumDecl();
    }

    // Check for trait declarations
    if (match(TokenType::TOKEN_KW_TRAIT)) {
        return parseTraitDecl();
    }

    // Check for impl declarations
    if (match(TokenType::TOKEN_KW_IMPL)) {
        return parseImplDecl();
    }

    // Check for qualifiers (wild, const, stack, gc) followed by type
    if (peek().type == TokenType::TOKEN_KW_WILD ||
        peek().type == TokenType::TOKEN_KW_CONST ||
        peek().type == TokenType::TOKEN_KW_STACK ||
        peek().type == TokenType::TOKEN_KW_GC) {
        return parseVarDecl();
    }
    
    // Check for generic type reference (variable declaration): *T:name = ...
    if (isGenericTypeReference()) {
        return parseVarDecl();
    }
    
    // Check for custom type reference (variable declaration): CustomType:name = ...
    // Also handles generic type instantiation: Box<int64>:name = ...
    // This allows struct types and other custom types in variable declarations
    if (peek().type == TokenType::TOKEN_IDENTIFIER) {
        size_t saved = current;
        advance(); // consume identifier
        
        // Check for generic type arguments: <...>
        if (check(TokenType::TOKEN_LESS)) {
            advance(); // consume '<'
            
            // Skip generic type arguments
            int angleDepth = 1;
            while (angleDepth > 0 && !isAtEnd()) {
                if (check(TokenType::TOKEN_LESS)) angleDepth++;
                if (check(TokenType::TOKEN_GREATER)) angleDepth--;
                advance();
            }
        }
        
        // Now check for colon (variable declaration) or other tokens
        if (check(TokenType::TOKEN_COLON)) {
            // It's a type annotation: CustomType:name or Box<int64>:name
            current = saved; // reset position
            return parseVarDecl();
        } else {
            // It's an identifier used in an expression, restore position
            current = saved;
            // Fall through to expression statement
        }
    }
    
    // Check for type annotation (variable declaration)
    // Must be followed by colon or array bracket to avoid ambiguity with identifiers
    // Examples: "int32:x = 5", "int32[]:arr = ...", "int32[10]:arr = ...", "int64?:maybe = ...", "int32$:ref = ...", "int32@:ptr = ..."
    if (isTypeKeyword(peek().type)) {
        size_t saved = current;
        advance(); // consume type keyword
        
        // Check for generic type parameters: array<T>, result<int32>
        if (check(TokenType::TOKEN_LESS)) {
            advance(); // consume '<'
            
            // Skip generic type arguments
            int angleDepth = 1;
            while (angleDepth > 0 && !isAtEnd()) {
                if (check(TokenType::TOKEN_LESS)) angleDepth++;
                if (check(TokenType::TOKEN_GREATER)) angleDepth--;
                advance();
            }
            
            // Now check for colon after generic type
            if (check(TokenType::TOKEN_COLON)) {
                // It's a generic type annotation: array<string>:name or result<int32>:name
                current = saved; // reset position
                return parseVarDecl();
            }
            
            // Not a variable declaration, restore position
            current = saved;
            // Fall through to expression statement
        }
        // Check for pointer type suffix: type->
        if (check(TokenType::TOKEN_ARROW)) {
            advance(); // consume '->'
            
            // Now check for colon after pointer type
            if (check(TokenType::TOKEN_COLON)) {
                // It's a pointer type annotation: type->:name
                current = saved; // reset position
                return parseVarDecl();
            }
            
            // Not a variable declaration, restore position
            current = saved;
            // Fall through to expression statement
        }
        // Check for safe reference type suffix: type$
        else if (check(TokenType::TOKEN_DOLLAR)) {
            advance(); // consume '$'
            
            // Now check for colon after safe reference type
            if (check(TokenType::TOKEN_COLON)) {
                // It's a safe reference type annotation: type$:name
                current = saved; // reset position
                return parseVarDecl();
            }
            
            // Not a variable declaration, restore position
            current = saved;
            // Fall through to expression statement
        }
        // Check for optional type suffix: type?
        else if (check(TokenType::TOKEN_QUESTION)) {
            advance(); // consume '?'
            
            // Now check for colon after optional type
            if (check(TokenType::TOKEN_COLON)) {
                // It's an optional type annotation: type?:name
                current = saved; // reset position
                return parseVarDecl();
            }
            
            // Not a variable declaration, restore position
            current = saved;
            // Fall through to expression statement
        }
        // Check for array type suffix: type[] or type[size]
        else if (check(TokenType::TOKEN_LEFT_BRACKET)) {
            advance(); // consume '['
            
            // Skip size expression if present
            if (!check(TokenType::TOKEN_RIGHT_BRACKET)) {
                // Skip the size expression (simple lookahead, just scan tokens)
                int bracketDepth = 1;
                while (bracketDepth > 0 && !isAtEnd()) {
                    if (check(TokenType::TOKEN_LEFT_BRACKET)) bracketDepth++;
                    if (check(TokenType::TOKEN_RIGHT_BRACKET)) bracketDepth--;
                    if (bracketDepth > 0) advance();
                }
            }
            
            // Should be at ']' now
            if (check(TokenType::TOKEN_RIGHT_BRACKET)) {
                advance(); // consume ']'
                
                // Now check for colon after array type
                if (check(TokenType::TOKEN_COLON)) {
                    // It's an array type annotation: type[]:name or type[size]:name
                    current = saved; // reset position
                    return parseVarDecl();
                }
            }
        } else if (check(TokenType::TOKEN_COLON)) {
            // It's a simple type annotation: type:name
            current = saved; // reset position
            return parseVarDecl();
        }
        
        // Not a variable declaration, restore position
        current = saved;
        // Fall through to expression statement
    }
    
    // Check for async function declaration: async func:name = ...
    if (peek().type == TokenType::TOKEN_KW_ASYNC) {
        size_t saved = current;
        current++; // skip 'async'
        
        // Must be followed by 'func' keyword
        if (current < tokens.size() && tokens[current].type == TokenType::TOKEN_KW_FUNC) {
            current++; // skip 'func'
            
            // Check for generic parameters: func<T, U> or func<T: Trait, U>
            if (current < tokens.size() && tokens[current].type == TokenType::TOKEN_LESS) {
                current++; // skip '<'
                // Skip generic parameter list including trait constraints
                // Format: <T: Trait1 & Trait2, U: Trait3, V>
                while (current < tokens.size() && tokens[current].type != TokenType::TOKEN_GREATER) {
                    if (tokens[current].type == TokenType::TOKEN_IDENTIFIER) {
                        current++; // skip type parameter name (T, U, etc.)

                        // Check for trait constraints: T: Trait1 & Trait2
                        if (current < tokens.size() && tokens[current].type == TokenType::TOKEN_COLON) {
                            current++; // skip ':'
                            // Parse trait bounds
                            while (current < tokens.size()) {
                                if (tokens[current].type == TokenType::TOKEN_IDENTIFIER) {
                                    current++; // skip trait name
                                    // Check for additional trait bounds with '&'
                                    if (current < tokens.size() && tokens[current].type == TokenType::TOKEN_AMPERSAND) {
                                        current++; // skip '&'
                                        continue; // expect another trait name
                                    }
                                }
                                break; // done with this parameter's constraints
                            }
                        }

                        // Check for comma (more parameters follow)
                        if (current < tokens.size() && tokens[current].type == TokenType::TOKEN_COMMA) {
                            current++; // skip comma
                        }
                    } else {
                        break;
                    }
                }
                if (current < tokens.size() && tokens[current].type == TokenType::TOKEN_GREATER) {
                    current++; // skip '>'
                }
            }

            // Check for colon
            if (current < tokens.size() && tokens[current].type == TokenType::TOKEN_COLON) {
                // It's an async function declaration
                current = saved; // reset position
                match(TokenType::TOKEN_KW_ASYNC); // consume 'async'
                match(TokenType::TOKEN_KW_FUNC);  // consume 'func'
                auto funcDecl = parseFuncDecl();
                // Set isAsync flag
                if (funcDecl && funcDecl->type == ASTNode::NodeType::FUNC_DECL) {
                    auto func = std::static_pointer_cast<FuncDeclStmt>(funcDecl);
                    func->isAsync = true;
                }
                return funcDecl;
            } else {
                // Not async func declaration, restore and continue
                current = saved;
            }
        } else {
            // Not followed by 'func', restore and continue
            current = saved;
        }
    }
    
    // Check for function declaration: func:name = ... or func<T>:name = ...
    // Only treat as function declaration if followed by colon or generic params then colon
    if (peek().type == TokenType::TOKEN_KW_FUNC) {
        // Look ahead to see if it's func:name or func<T>:name (declaration) or func() (call)
        size_t saved = current;
        current++; // skip 'func'
        
        // Check for generic parameters: func<T, U> or func<T: Trait, U>
        if (current < tokens.size() && tokens[current].type == TokenType::TOKEN_LESS) {
            current++; // skip '<'
            // Skip generic parameter list including trait constraints
            // Format: <T: Trait1 & Trait2, U: Trait3, V>
            while (current < tokens.size() && tokens[current].type != TokenType::TOKEN_GREATER) {
                if (tokens[current].type == TokenType::TOKEN_IDENTIFIER) {
                    current++; // skip type parameter name (T, U, etc.)

                    // Check for trait constraints: T: Trait1 & Trait2
                    if (current < tokens.size() && tokens[current].type == TokenType::TOKEN_COLON) {
                        current++; // skip ':'
                        // Parse trait bounds
                        while (current < tokens.size()) {
                            if (tokens[current].type == TokenType::TOKEN_IDENTIFIER) {
                                current++; // skip trait name
                                // Check for additional trait bounds with '&'
                                if (current < tokens.size() && tokens[current].type == TokenType::TOKEN_AMPERSAND) {
                                    current++; // skip '&'
                                    continue; // expect another trait name
                                }
                            }
                            break; // done with this parameter's constraints
                        }
                    }

                    // Check for comma (more parameters follow)
                    if (current < tokens.size() && tokens[current].type == TokenType::TOKEN_COMMA) {
                        current++; // skip comma
                    }
                } else {
                    // Not an identifier, break
                    break;
                }
            }
            if (current < tokens.size() && tokens[current].type == TokenType::TOKEN_GREATER) {
                current++; // skip '>'
            }
        }
        
        // Check for colon
        if (current < tokens.size() && tokens[current].type == TokenType::TOKEN_COLON) {
            // It's a function declaration: func:name = ... or func<T>:name = ...
            current = saved; // reset position
            match(TokenType::TOKEN_KW_FUNC); // consume it properly
            return parseFuncDecl();
        } else {
            // It's a function call or expression, restore position
            current = saved;
            // Fall through to expression statement
        }
    }
    
    // Check for control flow keywords
    if (match(TokenType::TOKEN_KW_RETURN)) {
        return parseReturn();
    }
    
    if (match(TokenType::TOKEN_KW_PASS)) {
        return parsePassStatement();
    }
    
    if (match(TokenType::TOKEN_KW_FAIL)) {
        return parseFailStatement();
    }
    
    if (match(TokenType::TOKEN_KW_IF)) {
        return parseIfStatement();
    }
    
    if (match(TokenType::TOKEN_KW_WHILE)) {
        return parseWhileStatement();
    }
    
    if (match(TokenType::TOKEN_KW_FOR)) {
        return parseForStatement();
    }
    
    if (match(TokenType::TOKEN_KW_BREAK)) {
        return parseBreakStatement();
    }
    
    if (match(TokenType::TOKEN_KW_CONTINUE)) {
        return parseContinueStatement();
    }
    
    if (match(TokenType::TOKEN_KW_DEFER)) {
        return parseDeferStatement();
    }
    
    if (match(TokenType::TOKEN_KW_TILL)) {
        return parseTillStatement();
    }
    
    if (match(TokenType::TOKEN_KW_LOOP)) {
        return parseLoopStatement();
    }
    
    if (match(TokenType::TOKEN_KW_WHEN)) {
        return parseWhenStatement();
    }
    
    if (match(TokenType::TOKEN_KW_PICK)) {
        return parsePickStatement();
    }
    
    if (match(TokenType::TOKEN_KW_FALL)) {
        return parseFallStatement();
    }
    
    // Check for block
    if (match(TokenType::TOKEN_LEFT_BRACE)) {
        return parseBlock();
    }
    
    // Otherwise, expression statement
    return parseExpressionStmt();
}

// Parse variable declaration: type:name = value;
// Or with qualifiers: wild int8:x = 5;
ASTNodePtr Parser::parseVarDecl() {
    using namespace frontend;
    
    bool isWild = false;
    bool isConst = false;
    bool isStack = false;
    bool isGC = false;
    
    // Handle qualifiers
    while (peek().type == TokenType::TOKEN_KW_WILD ||
           peek().type == TokenType::TOKEN_KW_CONST ||
           peek().type == TokenType::TOKEN_KW_STACK ||
           peek().type == TokenType::TOKEN_KW_GC) {
        if (match(TokenType::TOKEN_KW_WILD)) {
            isWild = true;
        } else if (match(TokenType::TOKEN_KW_CONST)) {
            isConst = true;
        } else if (match(TokenType::TOKEN_KW_STACK)) {
            isStack = true;
        } else if (match(TokenType::TOKEN_KW_GC)) {
            isGC = true;
        }
    }
    
    // Parse the type (handles simple types, arrays, pointers, generics, etc.)
    ASTNodePtr typeNode = parseType();
    if (!typeNode) {
        error("Expected type in variable declaration");
        return nullptr;
    }
    
    // Consume colon
    consume(TokenType::TOKEN_COLON, "Expected ':' after type in variable declaration");
    
    // Get variable name (can be identifier or certain keywords used as names)
    Token nameToken = peek();
    if (nameToken.type == TokenType::TOKEN_IDENTIFIER ||
        nameToken.type == TokenType::TOKEN_KW_RESULT ||
        nameToken.type == TokenType::TOKEN_KW_FUNC ||
        nameToken.type == TokenType::TOKEN_KW_OBJ) {
        advance();
    } else {
        error("Expected variable name");
        return nullptr;
    }
    
    // Check for initializer
    ASTNodePtr initializer = nullptr;
    if (match(TokenType::TOKEN_EQUAL)) {
        initializer = parseExpression();
    }
    
    // Consume semicolon
    consume(TokenType::TOKEN_SEMICOLON, "Expected ';' after variable declaration");
    
    auto varDecl = std::make_shared<VarDeclStmt>(
        typeNode,
        nameToken.lexeme,
        initializer,
        typeNode->line,
        typeNode->column
    );
    
    varDecl->isWild = isWild;
    varDecl->isConst = isConst;
    varDecl->isStack = isStack;
    varDecl->isGC = isGC;
    
    return varDecl;
}

// Parse function declaration: func:name = returnType(params) { body };
ASTNodePtr Parser::parseFuncDecl() {
    using namespace frontend;

    Token funcToken = previous(); // 'func' keyword

    // Phase 3.4: Parse generic parameters if present: func<T: Trait, U>
    std::vector<GenericParamInfo> genericParams = parseGenericParams();

    // Consume colon
    consume(TokenType::TOKEN_COLON, "Expected ':' after 'func'");

    // Get function name (allow 'process' keyword as function name too)
    Token nameToken;
    if (peek().type == TokenType::TOKEN_KW_PROCESS) {
        nameToken = advance();
    } else {
        nameToken = consume(TokenType::TOKEN_IDENTIFIER, "Expected function name");
    }

    // Consume equal sign
    consume(TokenType::TOKEN_EQUAL, "Expected '=' after function name");

    // Parse return type (supports generics, pointers, etc.)
    ASTNodePtr returnTypeNode = parseType();
    if (!returnTypeNode) {
        error("Expected return type");
        return nullptr;
    }
    
    // Parse parameters: (type:name, type:name, ...)
    consume(TokenType::TOKEN_LEFT_PAREN, "Expected '(' after return type");
    
    std::vector<ASTNodePtr> parameters;
    if (!check(TokenType::TOKEN_RIGHT_PAREN)) {
        do {
            // Parse parameter type using parseType() for full type support (including generics)
            ASTNodePtr paramTypeNode = parseType();
            if (!paramTypeNode) {
                error("Expected parameter type");
                return nullptr;
            }
            
            consume(TokenType::TOKEN_COLON, "Expected ':' after parameter type");
            
            Token paramNameToken = consume(TokenType::TOKEN_IDENTIFIER, "Expected parameter name");
            
            auto param = std::make_shared<ParameterNode>(
                paramTypeNode,  // Pass ASTNodePtr instead of string
                paramNameToken.lexeme,
                nullptr, // No default values for now
                funcToken.line,
                funcToken.column
            );
            
            parameters.push_back(param);
            
        } while (match(TokenType::TOKEN_COMMA));
    }
    
    consume(TokenType::TOKEN_RIGHT_PAREN, "Expected ')' after parameters");
    
    // Parse function body: { ... }
    consume(TokenType::TOKEN_LEFT_BRACE, "Expected '{' before function body");
    ASTNodePtr body = parseBlock();
    
    // Consume semicolon after closing brace
    consume(TokenType::TOKEN_SEMICOLON, "Expected ';' after function declaration");
    
    auto funcDecl = std::make_shared<FuncDeclStmt>(
        nameToken.lexeme,
        returnTypeNode,  // Changed from returnTypeName to returnTypeNode
        parameters,
        body,
        funcToken.line,
        funcToken.column
    );
    
    // Set generic parameters if present
    funcDecl->genericParams = genericParams;
    
    return funcDecl;
}

// Parse struct declaration: struct Name { type:field1; type:field2; };
// Or generic: struct<T, E>:Name { *T:field1; *E:field2; };
ASTNodePtr Parser::parseStructDecl() {
    using namespace frontend;
    
    Token structToken = previous(); // 'struct' keyword
    
    // Phase 3.4: Parse generic parameters if present: struct<T, E>
    std::vector<GenericParamInfo> genericParams = parseGenericParams();
    
    // ALWAYS consume colon before struct name (consistent with func:name syntax)
    consume(TokenType::TOKEN_COLON, "Expected ':' after 'struct' keyword");
    
    // Get struct name
    Token nameToken = consume(TokenType::TOKEN_IDENTIFIER, "Expected struct name");
    
    // Consume equals sign before struct body (consistent with func:name = returnType() {} syntax)
    consume(TokenType::TOKEN_EQUAL, "Expected '=' after struct name");
    
    // Parse struct body: { type:field; type:field; ... }
    consume(TokenType::TOKEN_LEFT_BRACE, "Expected '{' after '='");
    
    std::vector<ASTNodePtr> fields;
    while (!check(TokenType::TOKEN_RIGHT_BRACE) && !isAtEnd()) {
        // Parse field type (use parseType to handle generics, pointers, etc.)
        ASTNodePtr fieldTypeNode = parseType();
        if (!fieldTypeNode) {
            error("Expected field type in struct");
            return nullptr;
        }
        
        // Consume colon
        consume(TokenType::TOKEN_COLON, "Expected ':' after field type");
        
        // Get field name
        Token fieldNameToken = consume(TokenType::TOKEN_IDENTIFIER, "Expected field name");
        
        // Create a VarDeclStmt for the field (without initializer)
        auto fieldDecl = std::make_shared<VarDeclStmt>(
            fieldTypeNode,
            fieldNameToken.lexeme,
            nullptr,  // No initializer for struct fields
            fieldTypeNode->line,
            fieldTypeNode->column
        );
        
        fields.push_back(fieldDecl);
        
        // Consume semicolon after field
        consume(TokenType::TOKEN_SEMICOLON, "Expected ';' after struct field");
    }
    
    consume(TokenType::TOKEN_RIGHT_BRACE, "Expected '}' after struct fields");
    
    // Consume semicolon after struct declaration
    consume(TokenType::TOKEN_SEMICOLON, "Expected ';' after struct declaration");
    
    auto structDecl = std::make_shared<StructDeclStmt>(
        nameToken.lexeme,
        fields,
        structToken.line,
        structToken.column
    );
    
    // Store generic parameters
    structDecl->genericParams = genericParams;
    
    return structDecl;
}

ASTNodePtr Parser::parseEnumDecl() {
    using namespace frontend;
    
    Token enumToken = previous(); // 'enum' keyword
    
    // ALWAYS consume colon before enum name (consistent with struct:name syntax)
    consume(TokenType::TOKEN_COLON, "Expected ':' after 'enum' keyword");
    
    // Get enum name
    Token nameToken = consume(TokenType::TOKEN_IDENTIFIER, "Expected enum name");
    
    // Consume equals sign before enum body
    consume(TokenType::TOKEN_EQUAL, "Expected '=' after enum name");
    
    // Parse enum body: { VARIANT1 = value1, VARIANT2 = value2, ... }
    consume(TokenType::TOKEN_LEFT_BRACE, "Expected '{' after '='");
    
    std::map<std::string, int64_t> variants;
    while (!check(TokenType::TOKEN_RIGHT_BRACE) && !isAtEnd()) {
        // Get variant name (should be an identifier)
        Token variantToken = consume(TokenType::TOKEN_IDENTIFIER, "Expected variant name");
        std::string variantName = variantToken.lexeme;
        
        // Check for duplicate variant names
        if (variants.find(variantName) != variants.end()) {
            error("Duplicate variant name '" + variantName + "' in enum");
            return nullptr;
        }
        
        // Consume equals sign
        consume(TokenType::TOKEN_EQUAL, "Expected '=' after variant name");
        
        // Get variant value (must be an integer literal)
        Token valueToken = consume(TokenType::TOKEN_INTEGER, "Expected integer value for enum variant");
        int64_t value = 0;
        try {
            value = std::stoll(valueToken.lexeme);
        } catch (const std::invalid_argument& e) {
            error("Invalid integer value for enum variant: '" + valueToken.lexeme + "'");
            return nullptr;
        } catch (const std::out_of_range& e) {
            error("Enum variant value out of range: '" + valueToken.lexeme + "'");
            return nullptr;
        }
        
        // Store the variant
        variants[variantName] = value;
        
        // Check for comma (optional for last variant)
        if (!check(TokenType::TOKEN_RIGHT_BRACE)) {
            consume(TokenType::TOKEN_COMMA, "Expected ',' or '}' after enum variant");
        }
    }
    
    consume(TokenType::TOKEN_RIGHT_BRACE, "Expected '}' after enum variants");
    
    // Consume semicolon after enum declaration
    consume(TokenType::TOKEN_SEMICOLON, "Expected ';' after enum declaration");
    
    auto enumDecl = std::make_shared<EnumDeclStmt>(
        nameToken.lexeme,
        variants,
        enumToken.line,
        enumToken.column
    );
    
    return enumDecl;
}

// Parse trait declaration: trait:Name = { method_sig1, method_sig2, ... };
// Or with super traits: trait:Name:SuperTrait1:SuperTrait2 = { ... };
ASTNodePtr Parser::parseTraitDecl() {
    using namespace frontend;

    Token traitToken = previous(); // We already consumed 'trait'

    // Expect colon
    if (!check(TokenType::TOKEN_COLON)) {
        error("Expected ':' after 'trait'");
        synchronize(); // Move past the error
        return nullptr;
    }
    advance(); // consume ':'

    // Parse trait name
    if (!check(TokenType::TOKEN_IDENTIFIER)) {
        error("Expected trait name after 'trait:'");
        synchronize();
        return nullptr;
    }
    Token nameToken = advance();

    // Parse optional super traits: :SuperTrait1:SuperTrait2
    std::vector<std::string> superTraits;
    while (check(TokenType::TOKEN_COLON) && peekNext().type != TokenType::TOKEN_EQUAL) {
        advance(); // consume ':'
        Token superTrait = consume(TokenType::TOKEN_IDENTIFIER, "Expected super trait name after ':'");
        superTraits.push_back(superTrait.lexeme);
    }

    // Expect assignment
    consume(TokenType::TOKEN_EQUAL, "Expected '=' after trait name");

    // Expect opening brace
    consume(TokenType::TOKEN_LEFT_BRACE, "Expected '{' to begin trait body");

    // Parse trait methods (method signatures only, no bodies)
    std::vector<TraitMethod> methods;
    int loopCount = 0;
    while (!check(TokenType::TOKEN_RIGHT_BRACE) && !isAtEnd()) {
        // DEBUG: Detect infinite loop
        if (++loopCount > 1000) {
            error("Internal error: Infinite loop detected in trait method parsing");
            break;
        }

        // Skip commas between method signatures
        if (match(TokenType::TOKEN_COMMA)) {
            continue;
        }

        // Skip semicolons
        if (match(TokenType::TOKEN_SEMICOLON)) {
            continue;
        }

        // Save current position to detect if we're making progress
        size_t positionBefore = current;

        // Parse method name (can be identifier or 'func:name')
        std::string methodName;
        if (match(TokenType::TOKEN_KW_FUNC)) {
            consume(TokenType::TOKEN_COLON, "Expected ':' after 'func'");
            Token methodToken = consume(TokenType::TOKEN_IDENTIFIER, "Expected method name");
            methodName = methodToken.lexeme;
        } else {
            Token methodToken = consume(TokenType::TOKEN_IDENTIFIER, "Expected method name in trait");
            methodName = methodToken.lexeme;
        }

        // Expect colon before signature
        consume(TokenType::TOKEN_COLON, "Expected ':' after method name");

        // Parse return type first (before parameters)
        std::string returnType = "void";

        // Check if we have a return type before '('
        if (!check(TokenType::TOKEN_LEFT_PAREN)) {
            ASTNodePtr returnTypeNode = parseType();
            if (returnTypeNode) {
                returnType = returnTypeNode->toString();
            }
        }

        // Parse parameters
        consume(TokenType::TOKEN_LEFT_PAREN, "Expected '(' to begin method parameters");

        std::vector<ParameterNode> params;
        while (!check(TokenType::TOKEN_RIGHT_PAREN) && !isAtEnd()) {
            // Skip commas
            if (match(TokenType::TOKEN_COMMA)) {
                continue;
            }

            // Parse parameter type
            ASTNodePtr paramTypeNode = parseType();

            // Expect colon
            consume(TokenType::TOKEN_COLON, "Expected ':' after parameter type");

            // Parse parameter name
            Token paramName = consume(TokenType::TOKEN_IDENTIFIER, "Expected parameter name");

            params.emplace_back(paramTypeNode, paramName.lexeme, nullptr, paramName.line, paramName.column);
        }

        // Expect closing paren
        consume(TokenType::TOKEN_RIGHT_PAREN, "Expected ')' to end method parameters");

        // Create TraitMethod
        methods.emplace_back(methodName, std::move(params), returnType);

        // If we didn't advance at all, skip the current token to avoid infinite loop
        if (current == positionBefore && !isAtEnd()) {
            error("Unexpected token in trait method declaration");
            advance(); // Force progress
        }
    }

    // Expect closing brace
    consume(TokenType::TOKEN_RIGHT_BRACE, "Expected '}' to end trait body");

    // Consume optional semicolon
    match(TokenType::TOKEN_SEMICOLON);

    auto traitDecl = std::make_shared<TraitDeclStmt>(
        nameToken.lexeme,
        std::move(methods),
        traitToken.line,
        traitToken.column
    );
    traitDecl->superTraits = std::move(superTraits);

    return traitDecl;
}

// Parse trait implementation: impl:Trait:for:Type = { method implementations };
ASTNodePtr Parser::parseImplDecl() {
    using namespace frontend;

    Token implToken = previous(); // We already consumed 'impl'

    // Expect colon
    consume(TokenType::TOKEN_COLON, "Expected ':' after 'impl'");

    // Parse trait name
    Token traitNameToken = consume(TokenType::TOKEN_IDENTIFIER, "Expected trait name after 'impl:'");

    // Expect :for:
    consume(TokenType::TOKEN_COLON, "Expected ':for:' in impl declaration");
    consume(TokenType::TOKEN_KW_FOR, "Expected 'for' keyword in impl declaration");
    consume(TokenType::TOKEN_COLON, "Expected ':' after 'for'");

    // Parse type name (can be identifier OR primitive type keyword like int64, string, etc.)
    Token typeNameToken = peek();
    if (check(TokenType::TOKEN_IDENTIFIER)) {
        advance();
    } else if (isTypeKeyword(peek().type)) {
        advance();
    } else {
        error("Expected type name after 'for:'");
        return nullptr;
    }

    // Expect assignment
    consume(TokenType::TOKEN_EQUAL, "Expected '=' after impl declaration");

    // Expect opening brace
    consume(TokenType::TOKEN_LEFT_BRACE, "Expected '{' to begin impl body");

    // Parse method implementations (full function declarations)
    std::vector<ASTNodePtr> methods;
    while (!check(TokenType::TOKEN_RIGHT_BRACE) && !isAtEnd()) {
        // Skip commas
        if (match(TokenType::TOKEN_COMMA)) {
            continue;
        }

        // Skip semicolons
        if (match(TokenType::TOKEN_SEMICOLON)) {
            continue;
        }

        // Parse function declaration (method implementation)
        if (match(TokenType::TOKEN_KW_FUNC)) {
            auto method = parseFuncDecl();
            if (method) {
                methods.push_back(std::move(method));
            }
        } else {
            throw std::runtime_error("Expected method implementation in impl block at line " +
                                    std::to_string(peek().line));
        }
    }

    // Expect closing brace
    consume(TokenType::TOKEN_RIGHT_BRACE, "Expected '}' to end impl body");

    // Consume optional semicolon
    match(TokenType::TOKEN_SEMICOLON);

    return std::make_shared<ImplDeclStmt>(
        traitNameToken.lexeme,
        typeNameToken.lexeme,
        std::move(methods),
        implToken.line,
        implToken.column
    );
}

// Parse block: { stmt1; stmt2; ... }
ASTNodePtr Parser::parseBlock() {
    using namespace frontend;

    // ARIA-020: Check nesting depth to prevent stack overflow
    if (currentNestingDepth >= MAX_NESTING_DEPTH) {
        error("Maximum nesting depth exceeded (" + std::to_string(MAX_NESTING_DEPTH) +
              " levels). Consider refactoring deeply nested code.");
        return nullptr;
    }
    currentNestingDepth++;

    Token leftBrace = previous(); // We already consumed '{'
    std::vector<ASTNodePtr> statements;

    while (!check(TokenType::TOKEN_RIGHT_BRACE) && !isAtEnd()) {
        if (auto stmt = parseStatement()) {
            statements.push_back(stmt);
        } else {
            synchronize();
        }
    }

    consume(TokenType::TOKEN_RIGHT_BRACE, "Expected '}' after block");

    currentNestingDepth--;
    return std::make_shared<BlockStmt>(statements, leftBrace.line, leftBrace.column);
}

// Parse type annotation
// Handles: simple types (int8, string), pointers (int8@), arrays (int8[], int8[100]),
//          and generic types (Array<int8>, Map<string, int32>)
ASTNodePtr Parser::parseType() {
    using namespace frontend;

    Token typeToken = peek();
    ASTNodePtr baseType;

    // Handle prefix pointer syntax for generic types: *T (pointer to generic T)
    // This is used in generic function signatures: func<T>:identity = *T(*T:value)
    bool prefixPointer = false;
    if (typeToken.type == TokenType::TOKEN_STAR) {
        advance(); // consume '*'
        prefixPointer = true;
        typeToken = peek(); // get the actual type token
    }

    // Check for type keyword or identifier (for generic types)
    if (isTypeKeyword(typeToken.type) || typeToken.type == TokenType::TOKEN_IDENTIFIER) {
        advance(); // Consume the type token
        
        // Create simple type
        baseType = std::make_shared<SimpleType>(typeToken.lexeme, typeToken.line, typeToken.column);
        
        // Check for generic parameters: Array<int8>, Map<K, V>
        if (check(TokenType::TOKEN_LESS)) {
            advance(); // consume '<'
            
            std::vector<ASTNodePtr> typeArgs;
            
            // Parse type arguments
            do {
                if (check(TokenType::TOKEN_GREATER)) break;
                
                ASTNodePtr typeArg = parseType(); // Recursive call for nested generics
                if (typeArg) {
                    typeArgs.push_back(typeArg);
                } else {
                    error("Expected type argument in generic type");
                    break;
                }
                
            } while (match(TokenType::TOKEN_COMMA));
            
            consume(TokenType::TOKEN_GREATER, "Expected '>' after generic type arguments");
            
            // Create generic type node
            baseType = std::make_shared<GenericType>(typeToken.lexeme, typeArgs, 
                                                     typeToken.line, typeToken.column);
        }
    } else {
        error("Expected type annotation");
        return nullptr;
    }
    
    // ARIA-015: Context-aware pointer syntax enforcement
    // - -> creates fat pointers (native Aria) - only outside extern blocks
    // - * creates thin pointers (FFI) - only inside extern blocks
    if (match(TokenType::TOKEN_ARROW)) {
        if (isInsideExternBlock) {
            error("Use '*' for pointers inside extern blocks, not '->'. "
                  "The '->' syntax creates fat pointers incompatible with C ABI.");
        }
        auto ptrType = std::make_shared<PointerType>(baseType, typeToken.line, typeToken.column);
        ptrType->isNative = true;  // Fat pointer (32 bytes)
        baseType = ptrType;
    }
    else if (match(TokenType::TOKEN_STAR)) {
        if (!isInsideExternBlock) {
            error("Use '->' for pointers in Aria code, not '*'. "
                  "The '*' syntax is only valid in extern blocks for FFI compatibility.");
        }
        auto ptrType = std::make_shared<PointerType>(baseType, typeToken.line, typeToken.column);
        ptrType->isNative = false;  // Thin pointer (8 bytes)
        baseType = ptrType;
    }
    
    // Check for safe reference suffix: type$ (safe reference for borrow checker)
    if (match(TokenType::TOKEN_DOLLAR)) {
        baseType = std::make_shared<SafeRefType>(baseType, typeToken.line, typeToken.column);
    }
    
    // Check for optional suffix: type? (optional types, can be NIL)
    if (match(TokenType::TOKEN_QUESTION)) {
        baseType = std::make_shared<OptionalTypeNode>(baseType, typeToken.line, typeToken.column);
    }
    
    // Check for array suffix: type[] or type[size]
    if (match(TokenType::TOKEN_LEFT_BRACKET)) {
        ASTNodePtr sizeExpr = nullptr;
        
        // Check if it's a sized array: int8[100]
        if (!check(TokenType::TOKEN_RIGHT_BRACKET)) {
            // Parse the size expression (could be literal or identifier)
            sizeExpr = parseExpression();
        }
        
        consume(TokenType::TOKEN_RIGHT_BRACKET, "Expected ']' after array type");
        
        baseType = std::make_shared<ArrayType>(baseType, sizeExpr, typeToken.line, typeToken.column);
    }

    // Apply prefix pointer if we saw '*' at the beginning (for generic type references)
    // This handles: *T where T is a generic type parameter
    if (prefixPointer) {
        auto ptrType = std::make_shared<PointerType>(baseType, typeToken.line, typeToken.column);
        ptrType->isNative = false;  // Prefix * is thin pointer (like FFI)
        baseType = ptrType;
    }

    return baseType;
}

// Parse use statement: use path.to.module;
//                       use path.{item1, item2};
//                       use path.*;
//                       use "file.aria" as alias;
ASTNodePtr Parser::parseUseStatement() {
    using namespace frontend;
    
    Token useToken = previous(); // 'use' keyword already consumed
    auto useStmt = std::make_shared<UseStmt>(std::vector<std::string>(), useToken.line, useToken.column);
    
    // Check for string literal (file path)
    if (check(TokenType::TOKEN_STRING)) {
        Token pathToken = advance();
        // Strip quotes from string literal
        std::string path = pathToken.lexeme;
        if (path.length() >= 2 && path.front() == '"' && path.back() == '"') {
            path = path.substr(1, path.length() - 2);
        }
        useStmt->path.push_back(path);
        useStmt->isFilePath = true;
        
        // Check for wildcard import: use "file.aria".*;
        if (match(TokenType::TOKEN_DOT)) {
            if (match(TokenType::TOKEN_STAR)) {
                useStmt->isWildcard = true;
                consume(TokenType::TOKEN_SEMICOLON, "Expected ';' after use statement");
                return useStmt;
            }
            
            // Check for selective import: use "file.aria".{a, b};
            if (match(TokenType::TOKEN_LEFT_BRACE)) {
                if (!check(TokenType::TOKEN_IDENTIFIER) && !isTypeKeyword(peek().type)) {
                    error("Expected identifier or keyword in import list");
                    return useStmt;
                }
                Token firstItem = advance();
                useStmt->items.push_back(firstItem.lexeme);
                
                while (match(TokenType::TOKEN_COMMA)) {
                    if (!check(TokenType::TOKEN_IDENTIFIER) && !isTypeKeyword(peek().type)) {
                        error("Expected identifier or keyword in import list");
                        break;
                    }
                    Token nextItem = advance();
                    useStmt->items.push_back(nextItem.lexeme);
                }
                
                consume(TokenType::TOKEN_RIGHT_BRACE, "Expected '}' after import list");
                consume(TokenType::TOKEN_SEMICOLON, "Expected ';' after use statement");
                return useStmt;
            }
            
            // If we got a dot but neither wildcard nor selective, that's an error
            error("Expected '*' or '{' after '.' in file path import");
            return useStmt;
        }
        
        // Check for 'as' alias
        if (match(TokenType::TOKEN_KW_AS)) {
            Token aliasToken = consume(TokenType::TOKEN_IDENTIFIER, "Expected identifier after 'as'");
            useStmt->alias = aliasToken.lexeme;
        }
        
        consume(TokenType::TOKEN_SEMICOLON, "Expected ';' after use statement");
        return useStmt;
    }
    
    // Parse logical path: std.io or std.collections.array
    do {
        Token segment = consume(TokenType::TOKEN_IDENTIFIER, "Expected identifier in module path");
        useStmt->path.push_back(segment.lexeme);
        
        // Check for continuation
        if (match(TokenType::TOKEN_DOT)) {
            // Check for wildcard: use math.*;
            if (match(TokenType::TOKEN_STAR)) {
                useStmt->isWildcard = true;
                consume(TokenType::TOKEN_SEMICOLON, "Expected ';' after use statement");
                return useStmt;
            }
            
            // Check for selective import: use std.{array, map};
            if (match(TokenType::TOKEN_LEFT_BRACE)) {
                // Parse first item (can be identifier or keyword)
                Token firstItem = peek();
                if (!check(TokenType::TOKEN_IDENTIFIER) && !isTypeKeyword(firstItem.type)) {
                    error("Expected identifier or keyword in import list");
                    return useStmt;
                }
                advance();
                useStmt->items.push_back(firstItem.lexeme);
                
                // Parse remaining items
                while (match(TokenType::TOKEN_COMMA)) {
                    Token nextItem = peek();
                    if (!check(TokenType::TOKEN_IDENTIFIER) && !isTypeKeyword(nextItem.type)) {
                        error("Expected identifier or keyword in import list");
                        break;
                    }
                    advance();
                    useStmt->items.push_back(nextItem.lexeme);
                }
                
                consume(TokenType::TOKEN_RIGHT_BRACE, "Expected '}' after import list");
                consume(TokenType::TOKEN_SEMICOLON, "Expected ';' after use statement");
                return useStmt;
            }
            
            // Continue with path (another segment coming)
            continue;
        }
        
        // No dot, so we're done with the path
        break;
    } while (true);
    
    // Check for 'as' alias
    if (match(TokenType::TOKEN_KW_AS)) {
        Token aliasToken = consume(TokenType::TOKEN_IDENTIFIER, "Expected identifier after 'as'");
        useStmt->alias = aliasToken.lexeme;
    }
    
    consume(TokenType::TOKEN_SEMICOLON, "Expected ';' after use statement");
    return useStmt;
}

// Parse mod statement: mod name; or mod name { ... }
ASTNodePtr Parser::parseModStatement() {
    using namespace frontend;
    
    Token modToken = previous(); // 'mod' keyword already consumed
    
    // Get module name
    Token nameToken = consume(TokenType::TOKEN_IDENTIFIER, "Expected module name after 'mod'");
    auto modStmt = std::make_shared<ModStmt>(nameToken.lexeme, modToken.line, modToken.column);
    
    // Check if it's an inline module with a body
    if (match(TokenType::TOKEN_LEFT_BRACE)) {
        modStmt->isInline = true;
        
        // Parse statements inside the module until we hit the closing brace
        while (!check(TokenType::TOKEN_RIGHT_BRACE) && !isAtEnd()) {
            ASTNodePtr stmt = parseStatement();
            if (stmt) {
                modStmt->body.push_back(stmt);
            }
        }
        
        consume(TokenType::TOKEN_RIGHT_BRACE, "Expected '}' after module body");
    } else {
        // External file module: just consume the semicolon
        consume(TokenType::TOKEN_SEMICOLON, "Expected ';' after module declaration");
    }
    
    return modStmt;
}

// Parse extern statement: extern "libname" { declarations }
ASTNodePtr Parser::parseExternStatement() {
    using namespace frontend;
    
    Token externToken = previous(); // 'extern' keyword already consumed
    
    // Get library name (must be a string literal)
    Token libNameToken = consume(TokenType::TOKEN_STRING, "Expected library name string after 'extern'");
    
    // Strip quotes from library name
    std::string libName = libNameToken.lexeme;
    if (libName.length() >= 2 && libName.front() == '"' && libName.back() == '"') {
        libName = libName.substr(1, libName.length() - 2);
    }
    
    auto externStmt = std::make_shared<ExternStmt>(libName, externToken.line, externToken.column);
    
    // Expect opening brace
    consume(TokenType::TOKEN_LEFT_BRACE, "Expected '{' after extern library name");

    // ARIA-015: Set FFI context - * pointer syntax is now allowed
    isInsideExternBlock = true;

    // Parse declarations inside the extern block
    // Note: extern blocks contain signatures (declarations without bodies), not statements
    while (!check(TokenType::TOKEN_RIGHT_BRACE) && !isAtEnd()) {
        // Check for opaque struct declaration: opaque struct:Name;
        if (match(TokenType::TOKEN_KW_OPAQUE)) {
            Token opaqueToken = previous();

            // Expect 'struct' keyword
            consume(TokenType::TOKEN_KW_STRUCT, "Expected 'struct' after 'opaque'");

            // Consume colon
            consume(TokenType::TOKEN_COLON, "Expected ':' after 'struct'");

            // Get struct name
            Token nameToken = consume(TokenType::TOKEN_IDENTIFIER, "Expected struct name");

            // Consume semicolon
            consume(TokenType::TOKEN_SEMICOLON, "Expected ';' after opaque struct declaration");

            // Create opaque struct declaration
            auto opaqueDecl = std::make_shared<OpaqueStructDecl>(
                nameToken.lexeme,
                opaqueToken.line,
                opaqueToken.column
            );

            externStmt->declarations.push_back(opaqueDecl);
            continue;
        }

        // Check for struct declaration in extern block: struct:Name = { fields };
        if (match(TokenType::TOKEN_KW_STRUCT)) {
            Token structToken = previous();

            // Consume colon
            consume(TokenType::TOKEN_COLON, "Expected ':' after 'struct'");

            // Get struct name
            Token nameToken = consume(TokenType::TOKEN_IDENTIFIER, "Expected struct name");

            // Consume equal sign
            consume(TokenType::TOKEN_EQUAL, "Expected '=' after struct name");

            // Consume opening brace
            consume(TokenType::TOKEN_LEFT_BRACE, "Expected '{' for struct fields");

            // Parse fields
            std::vector<ASTNodePtr> fields;
            while (!check(TokenType::TOKEN_RIGHT_BRACE) && !isAtEnd()) {
                // Parse qualifiers (wild, const, etc.)
                std::vector<std::string> qualifiers;
                while (peek().type == TokenType::TOKEN_KW_WILD ||
                       peek().type == TokenType::TOKEN_KW_CONST) {
                    qualifiers.push_back(advance().lexeme);
                }

                // Get type (can be identifier for FFI types)
                Token fieldTypeToken = advance();
                std::string fieldTypeName = fieldTypeToken.lexeme;

                // Handle pointer types
                while (check(TokenType::TOKEN_STAR)) {
                    advance();
                    fieldTypeName += "*";
                }

                // Consume colon
                consume(TokenType::TOKEN_COLON, "Expected ':' after field type");

                // Get field name
                Token fieldNameToken = consume(TokenType::TOKEN_IDENTIFIER, "Expected field name");

                // Consume semicolon
                consume(TokenType::TOKEN_SEMICOLON, "Expected ';' after field");

                // Create field as VarDeclStmt
                auto fieldDecl = std::make_shared<VarDeclStmt>(
                    fieldTypeName,
                    fieldNameToken.lexeme,
                    nullptr,
                    fieldTypeToken.line,
                    fieldTypeToken.column
                );

                // Apply qualifiers
                for (const auto& qual : qualifiers) {
                    if (qual == "wild") fieldDecl->isWild = true;
                    else if (qual == "const") fieldDecl->isConst = true;
                }

                fields.push_back(fieldDecl);
            }

            consume(TokenType::TOKEN_RIGHT_BRACE, "Expected '}' after struct fields");
            consume(TokenType::TOKEN_SEMICOLON, "Expected ';' after struct declaration");

            // Create struct declaration
            auto structDecl = std::make_shared<StructDeclStmt>(
                nameToken.lexeme,
                fields,
                structToken.line,
                structToken.column
            );

            externStmt->declarations.push_back(structDecl);
            continue;
        }

        // Check for function declaration: func:name = returnType(params);
        if (match(TokenType::TOKEN_KW_FUNC)) {
            Token funcToken = previous();

            // Consume colon
            consume(TokenType::TOKEN_COLON, "Expected ':' after 'func'");

            // Get function name
            Token nameToken = consume(TokenType::TOKEN_IDENTIFIER, "Expected function name");

            // Consume equal sign
            consume(TokenType::TOKEN_EQUAL, "Expected '=' after function name");
            
            // Get return type (can be Aria type or C type identifier for FFI)
            Token returnTypeToken = advance();
            if (!isTypeKeyword(returnTypeToken.type) && returnTypeToken.type != TokenType::TOKEN_IDENTIFIER) {
                error("Expected return type or type identifier in extern function");
                continue; // Skip this declaration
            }
            
            // Handle pointer types: void*, int8*, etc.
            std::string returnTypeName = returnTypeToken.lexeme;
            while (check(TokenType::TOKEN_STAR)) {
                advance(); // consume '*'
                returnTypeName += "*";
            }
            
            // Create a SimpleType from the string for extern functions
            auto returnType = std::make_shared<SimpleType>(returnTypeName, returnTypeToken.line, returnTypeToken.column);
            
            // Parse parameters: (type:name, type:name, ...)
            consume(TokenType::TOKEN_LEFT_PAREN, "Expected '(' after return type");
            
            std::vector<ASTNodePtr> parameters;
            if (!check(TokenType::TOKEN_RIGHT_PAREN)) {
                do {
                    // Parse parameter: type:name (can be Aria type or C type identifier for FFI)
                    Token paramTypeToken = advance();
                    if (!isTypeKeyword(paramTypeToken.type) && paramTypeToken.type != TokenType::TOKEN_IDENTIFIER) {
                        error("Expected parameter type or type identifier in extern function");
                        break;
                    }
                    
                    // Handle pointer types: void*, int8*, etc.
                    std::string paramTypeName = paramTypeToken.lexeme;
                    while (check(TokenType::TOKEN_STAR)) {
                        advance(); // consume '*'
                        paramTypeName += "*";
                    }
                    
                    // Create a SimpleType from the string for extern functions
                    auto paramType = std::make_shared<SimpleType>(paramTypeName, paramTypeToken.line, paramTypeToken.column);
                    
                    consume(TokenType::TOKEN_COLON, "Expected ':' after parameter type");
                    
                    Token paramNameToken = consume(TokenType::TOKEN_IDENTIFIER, "Expected parameter name");
                    
                    auto param = std::make_shared<ParameterNode>(
                        paramType,  // Pass SimpleType instead of string
                        paramNameToken.lexeme,
                        nullptr,
                        paramTypeToken.line,
                        paramTypeToken.column
                    );
                    
                    parameters.push_back(param);
                    
                } while (match(TokenType::TOKEN_COMMA));
            }
            
            consume(TokenType::TOKEN_RIGHT_PAREN, "Expected ')' after parameters");
            
            // No body for extern functions - just a semicolon
            consume(TokenType::TOKEN_SEMICOLON, "Expected ';' after extern function signature");
            
            // Create function declaration WITHOUT a body
            auto funcDecl = std::make_shared<FuncDeclStmt>(
                nameToken.lexeme,
                returnType,  // Now using ASTNodePtr (TypeNode) instead of string
                parameters,
                nullptr, // No body for extern functions
                funcToken.line,
                funcToken.column
            );
            
            externStmt->declarations.push_back(funcDecl);
        }
        // Check for variable declaration: [qualifier] type:name;
        else if (peek().type == TokenType::TOKEN_KW_WILD ||
                 peek().type == TokenType::TOKEN_KW_CONST ||
                 peek().type == TokenType::TOKEN_KW_STACK ||
                 peek().type == TokenType::TOKEN_KW_GC ||
                 isTypeKeyword(peek().type)) {
            
            // Parse qualifiers
            std::vector<std::string> qualifiers;
            while (peek().type == TokenType::TOKEN_KW_WILD ||
                   peek().type == TokenType::TOKEN_KW_CONST ||
                   peek().type == TokenType::TOKEN_KW_STACK ||
                   peek().type == TokenType::TOKEN_KW_GC) {
                qualifiers.push_back(advance().lexeme);
            }
            
            // Get type
            Token typeToken = advance();
            if (!isTypeKeyword(typeToken.type)) {
                error("Expected type in extern variable declaration");
                continue;
            }
            
            // Consume colon
            consume(TokenType::TOKEN_COLON, "Expected ':' after type");
            
            // Get variable name
            Token nameToken = consume(TokenType::TOKEN_IDENTIFIER, "Expected variable name");
            
            // No initializer for extern variables - just a semicolon
            consume(TokenType::TOKEN_SEMICOLON, "Expected ';' after extern variable declaration");
            
            // Create variable declaration WITHOUT an initializer
            auto varDecl = std::make_shared<VarDeclStmt>(
                typeToken.lexeme,
                nameToken.lexeme,
                nullptr, // No initializer for extern variables
                typeToken.line,
                typeToken.column
            );
            
            // Apply qualifiers
            for (const auto& qual : qualifiers) {
                if (qual == "wild") varDecl->isWild = true;
                else if (qual == "const") varDecl->isConst = true;
                else if (qual == "stack") varDecl->isStack = true;
                else if (qual == "gc") varDecl->isGC = true;
            }
            
            externStmt->declarations.push_back(varDecl);
        }
        else {
            error("Expected function or variable declaration in extern block");
            advance(); // Skip this token and continue
        }
    }
    
    consume(TokenType::TOKEN_RIGHT_BRACE, "Expected '}' after extern block");

    // ARIA-015: Exit FFI context - restore @ pointer syntax enforcement
    isInsideExternBlock = false;

    return externStmt;
}

// Parse expression statement: expr;
ASTNodePtr Parser::parseExpressionStmt() {
    using namespace frontend;
    
    ASTNodePtr expr = parseExpression();
    
    if (!expr) {
        return nullptr;  // Expression parsing failed
    }
    
    // For backward compatibility with Phase 2.1 expression-only tests,
    // don't require semicolon at EOF (allows bare expressions in test cases)
    if (!isAtEnd()) {
        consume(TokenType::TOKEN_SEMICOLON, "Expected ';' after expression");
        return std::make_shared<ExpressionStmt>(expr, expr->line, expr->column);
    }
    
    // At EOF, return bare expression (for Phase 2.1 tests)
    return expr;
}

// Parse return statement: return expr; or return;
ASTNodePtr Parser::parseReturn() {
    using namespace frontend;
    
    Token returnToken = previous(); // We already consumed 'return'
    
    ASTNodePtr value = nullptr;
    
    // Check if there's a return value
    if (!check(TokenType::TOKEN_SEMICOLON)) {
        value = parseExpression();
    }
    
    consume(TokenType::TOKEN_SEMICOLON, "Expected ';' after return statement");
    
    return std::make_shared<ReturnStmt>(value, returnToken.line, returnToken.column);
}

ASTNodePtr Parser::parsePassStatement() {
    using namespace frontend;

    Token passToken = previous(); // We already consumed 'pass'

    // Parse: pass(expr);
    consume(TokenType::TOKEN_LEFT_PAREN, "Expected '(' after 'pass'");

    ASTNodePtr value = parseExpression();
    if (!value) {
        error("Expected expression in pass statement");
        return nullptr;
    }

    consume(TokenType::TOKEN_RIGHT_PAREN, "Expected ')' after pass value");
    consume(TokenType::TOKEN_SEMICOLON, "Expected ';' after pass statement");

    // Create PassStmt - type checker and IR generator handle Result building
    return std::make_shared<PassStmt>(value, passToken.line, passToken.column);
}

ASTNodePtr Parser::parseFailStatement() {
    using namespace frontend;

    Token failToken = previous(); // We already consumed 'fail'

    // Parse: fail(error_code);
    consume(TokenType::TOKEN_LEFT_PAREN, "Expected '(' after 'fail'");

    ASTNodePtr errorCode = parseExpression();
    if (!errorCode) {
        error("Expected error code expression in fail statement");
        return nullptr;
    }

    consume(TokenType::TOKEN_RIGHT_PAREN, "Expected ')' after fail error code");
    consume(TokenType::TOKEN_SEMICOLON, "Expected ';' after fail statement");

    // Create FailStmt - type checker and IR generator handle Result building
    return std::make_shared<FailStmt>(errorCode, failToken.line, failToken.column);
}

// Parse if statement: if (condition) thenBranch [else elseBranch]
// thenBranch and elseBranch can be blocks or single statements
ASTNodePtr Parser::parseIfStatement() {
    using namespace frontend;
    
    Token ifToken = previous(); // We already consumed 'if'
    
    // Parse condition
    consume(TokenType::TOKEN_LEFT_PAREN, "Expected '(' after 'if'");
    ASTNodePtr condition = parseExpression();
    
    if (!condition) {
        error("Expected condition expression in if statement");
        return nullptr;
    }
    
    consume(TokenType::TOKEN_RIGHT_PAREN, "Expected ')' after if condition");
    
    // Parse then branch (can be block or single statement)
    ASTNodePtr thenBranch = nullptr;
    if (match(TokenType::TOKEN_LEFT_BRACE)) {
        thenBranch = parseBlock();
    } else {
        thenBranch = parseStatement();
    }
    
    if (!thenBranch) {
        error("Expected statement or block after if condition");
        return nullptr;
    }
    
    // Parse optional else branch
    ASTNodePtr elseBranch = nullptr;
    if (match(TokenType::TOKEN_KW_ELSE)) {
        // Check for else if
        if (match(TokenType::TOKEN_KW_IF)) {
            elseBranch = parseIfStatement(); // Recursively parse else if as a new if statement
        } else if (match(TokenType::TOKEN_LEFT_BRACE)) {
            elseBranch = parseBlock();
        } else {
            elseBranch = parseStatement();
        }
        
        if (!elseBranch) {
            error("Expected statement or block after 'else'");
            return nullptr;
        }
    }
    
    return std::make_shared<IfStmt>(condition, thenBranch, elseBranch, ifToken.line, ifToken.column);
}

// Parse while statement: while (condition) body
ASTNodePtr Parser::parseWhileStatement() {
    using namespace frontend;
    
    Token whileToken = previous(); // We already consumed 'while'
    
    // Parse condition
    consume(TokenType::TOKEN_LEFT_PAREN, "Expected '(' after 'while'");
    ASTNodePtr condition = parseExpression();
    
    if (!condition) {
        error("Expected condition expression in while statement");
        return nullptr;
    }
    
    consume(TokenType::TOKEN_RIGHT_PAREN, "Expected ')' after while condition");
    
    // Parse body (can be block or single statement)
    ASTNodePtr body = nullptr;
    if (match(TokenType::TOKEN_LEFT_BRACE)) {
        body = parseBlock();
    } else {
        body = parseStatement();
    }
    
    if (!body) {
        error("Expected statement or block after while condition");
        return nullptr;
    }
    
    return std::make_shared<WhileStmt>(condition, body, whileToken.line, whileToken.column);
}

// Parse for statement: for (init; condition; update) body
// init can be variable declaration or expression
// All three clauses are optional: for (;;) is valid (infinite loop)
ASTNodePtr Parser::parseForStatement() {
    using namespace frontend;
    
    Token forToken = previous(); // We already consumed 'for'
    
    consume(TokenType::TOKEN_LEFT_PAREN, "Expected '(' after 'for'");
    
    // Check if this is a range-based for loop: for (var in range) or for (type:var in range)
    // Look ahead to detect: IDENTIFIER IN or TYPE COLON IDENTIFIER IN
    bool isRangeBased = false;
    std::string iteratorType = "";
    std::string iteratorName = "";
    
    // Check for range-based loop pattern (peek only, don't consume yet)
    if (isTypeKeyword(peek().type)) {
        // Might be: for (int64:i in range)
        Token typeToken = peek();
        Token nextToken = peekNext();
        if (nextToken.type == TokenType::TOKEN_COLON) {
            // Look further ahead to check for IN keyword
            // Save current position
            size_t savedPos = current;
            
            advance(); // consume type
            consume(TokenType::TOKEN_COLON, "Expected ':' after type");
            Token nameToken = consume(TokenType::TOKEN_IDENTIFIER, "Expected identifier after ':'");
            
            if (check(TokenType::TOKEN_KW_IN)) {
                // Confirmed: range-based for loop with type annotation
                isRangeBased = true;
                iteratorType = typeToken.lexeme;
                iteratorName = nameToken.lexeme;
            } else {
                // Not range-based, restore position and parse as C-style
                current = savedPos;
            }
        }
    } else if (peek().type == TokenType::TOKEN_IDENTIFIER) {
        // Might be: for (i in range) without type annotation
        Token nameToken = peek();
        Token nextToken = peekNext();
        if (nextToken.type == TokenType::TOKEN_KW_IN) {
            isRangeBased = true;
            iteratorName = nameToken.lexeme;
            advance(); // consume identifier
        }
    }
    
    // Parse range-based for loop
    if (isRangeBased) {
        consume(TokenType::TOKEN_KW_IN, "Expected 'in' keyword");
        
        // Parse range expression
        ASTNodePtr rangeExpr = parseExpression();
        if (!rangeExpr) {
            error("Expected range expression after 'in'");
            return nullptr;
        }
        
        consume(TokenType::TOKEN_RIGHT_PAREN, "Expected ')' after range expression");
        
        // Parse body
        ASTNodePtr body = nullptr;
        if (match(TokenType::TOKEN_LEFT_BRACE)) {
            body = parseBlock();
        } else {
            body = parseStatement();
        }
        
        if (!body) {
            error("Expected statement or block after for clause");
            return nullptr;
        }
        
        return ForStmt::createRangeBased(iteratorName, iteratorType, rangeExpr, body,
                                          forToken.line, forToken.column);
    }
    
    // Parse C-style for loop: for (init; cond; update)
    // Parse initializer (optional)
    ASTNodePtr initializer = nullptr;
    if (match(TokenType::TOKEN_SEMICOLON)) {
        // No initializer
        initializer = nullptr;
    } else if (isTypeKeyword(peek().type)) {
        // Variable declaration
        initializer = parseVarDecl();
        // parseVarDecl consumes the semicolon
    } else {
        // Expression statement
        initializer = parseExpression();
        consume(TokenType::TOKEN_SEMICOLON, "Expected ';' after for loop initializer");
    }
    
    // Parse condition (optional)
    ASTNodePtr condition = nullptr;
    if (!check(TokenType::TOKEN_SEMICOLON)) {
        condition = parseExpression();
    }
    consume(TokenType::TOKEN_SEMICOLON, "Expected ';' after for loop condition");
    
    // Parse update (optional)
    ASTNodePtr update = nullptr;
    if (!check(TokenType::TOKEN_RIGHT_PAREN)) {
        update = parseExpression();
    }
    consume(TokenType::TOKEN_RIGHT_PAREN, "Expected ')' after for clauses");
    
    // Parse body (can be block or single statement)
    ASTNodePtr body = nullptr;
    if (match(TokenType::TOKEN_LEFT_BRACE)) {
        body = parseBlock();
    } else {
        body = parseStatement();
    }
    
    if (!body) {
        error("Expected statement or block after for clauses");
        return nullptr;
    }
    
    return std::make_shared<ForStmt>(initializer, condition, update, body, forToken.line, forToken.column);
}

ASTNodePtr Parser::parseBreakStatement() {
    using namespace frontend;
    
    Token breakToken = previous(); // We already consumed 'break'
    
    std::string label = "";
    
    // Check for optional label: break(identifier)
    if (match(TokenType::TOKEN_LEFT_PAREN)) {
        Token labelToken = consume(TokenType::TOKEN_IDENTIFIER, "Expected identifier after '(' in break statement");
        label = labelToken.lexeme;
        consume(TokenType::TOKEN_RIGHT_PAREN, "Expected ')' after break label");
    }
    
    consume(TokenType::TOKEN_SEMICOLON, "Expected ';' after break statement");
    
    return std::make_shared<BreakStmt>(label, breakToken.line, breakToken.column);
}

ASTNodePtr Parser::parseContinueStatement() {
    using namespace frontend;
    
    Token continueToken = previous(); // We already consumed 'continue'
    
    std::string label = "";
    
    // Check for optional label: continue(identifier)
    if (match(TokenType::TOKEN_LEFT_PAREN)) {
        Token labelToken = consume(TokenType::TOKEN_IDENTIFIER, "Expected identifier after '(' in continue statement");
        label = labelToken.lexeme;
        consume(TokenType::TOKEN_RIGHT_PAREN, "Expected ')' after continue label");
    }
    
    consume(TokenType::TOKEN_SEMICOLON, "Expected ';' after continue statement");
    
    return std::make_shared<ContinueStmt>(label, continueToken.line, continueToken.column);
}

ASTNodePtr Parser::parseDeferStatement() {
    using namespace frontend;
    
    Token deferToken = previous(); // We already consumed 'defer'
    
    // Parse: defer { block }
    // According to research_020, defer takes a block, not just an expression
    consume(TokenType::TOKEN_LEFT_BRACE, "Expected '{' after 'defer' - defer requires a block");
    
    ASTNodePtr block = parseBlock();
    if (!block) {
        error("Expected block after 'defer'");
        return nullptr;
    }
    
    // No semicolon needed after defer block (it's a block statement)
    
    return std::make_shared<DeferStmt>(block, deferToken.line, deferToken.column);
}

ASTNodePtr Parser::parseTillStatement() {
    using namespace frontend;
    
    Token tillToken = previous(); // We already consumed 'till'
    
    // Parse: till(limit, step) { body }
    consume(TokenType::TOKEN_LEFT_PAREN, "Expected '(' after 'till'");
    
    // Parse limit expression
    ASTNodePtr limit = parseExpression();
    if (!limit) {
        error("Expected limit expression in till statement");
        return nullptr;
    }
    
    consume(TokenType::TOKEN_COMMA, "Expected ',' after till limit");
    
    // Parse step expression
    ASTNodePtr step = parseExpression();
    if (!step) {
        error("Expected step expression in till statement");
        return nullptr;
    }
    
    consume(TokenType::TOKEN_RIGHT_PAREN, "Expected ')' after till parameters");
    
    // Parse body (must be a block with braces)
    if (!match(TokenType::TOKEN_LEFT_BRACE)) {
        error("Expected '{' after till parameters");
        return nullptr;
    }
    ASTNodePtr body = parseBlock();
    
    if (!body) {
        error("Expected block after till parameters");
        return nullptr;
    }
    
    return std::make_shared<TillStmt>(limit, step, body, tillToken.line, tillToken.column);
}

ASTNodePtr Parser::parseLoopStatement() {
    using namespace frontend;
    
    Token loopToken = previous(); // We already consumed 'loop'
    
    // Parse: loop(start, limit, step) { body }
    consume(TokenType::TOKEN_LEFT_PAREN, "Expected '(' after 'loop'");
    
    // Parse start expression
    ASTNodePtr start = parseExpression();
    if (!start) {
        error("Expected start expression in loop statement");
        return nullptr;
    }
    
    consume(TokenType::TOKEN_COMMA, "Expected ',' after loop start");
    
    // Parse limit expression
    ASTNodePtr limit = parseExpression();
    if (!limit) {
        error("Expected limit expression in loop statement");
        return nullptr;
    }
    
    consume(TokenType::TOKEN_COMMA, "Expected ',' after loop limit");
    
    // Parse step expression
    ASTNodePtr step = parseExpression();
    if (!step) {
        error("Expected step expression in loop statement");
        return nullptr;
    }
    
    consume(TokenType::TOKEN_RIGHT_PAREN, "Expected ')' after loop parameters");
    
    // Parse body (must be a block with braces)
    if (!match(TokenType::TOKEN_LEFT_BRACE)) {
        error("Expected '{' after loop parameters");
        return nullptr;
    }
    ASTNodePtr body = parseBlock();
    
    if (!body) {
        error("Expected block after loop parameters");
        return nullptr;
    }
    
    return std::make_shared<LoopStmt>(start, limit, step, body, loopToken.line, loopToken.column);
}

ASTNodePtr Parser::parseWhenStatement() {
    using namespace frontend;
    
    Token whenToken = previous(); // We already consumed 'when'
    
    // Parse: when(condition) { body } [then { then_block }] [end { end_block }]
    consume(TokenType::TOKEN_LEFT_PAREN, "Expected '(' after 'when'");
    
    // Parse condition
    ASTNodePtr condition = parseExpression();
    if (!condition) {
        error("Expected condition in when statement");
        return nullptr;
    }
    
    consume(TokenType::TOKEN_RIGHT_PAREN, "Expected ')' after when condition");
    
    // Parse body (must be a block)
    if (!match(TokenType::TOKEN_LEFT_BRACE)) {
        error("Expected '{' after when condition");
        return nullptr;
    }
    ASTNodePtr body = parseBlock();
    
    if (!body) {
        error("Expected block after when condition");
        return nullptr;
    }
    
    // Parse optional 'then' block
    ASTNodePtr then_block = nullptr;
    if (match(TokenType::TOKEN_KW_THEN)) {
        if (!match(TokenType::TOKEN_LEFT_BRACE)) {
            error("Expected '{' after 'then'");
            return nullptr;
        }
        then_block = parseBlock();
        if (!then_block) {
            error("Expected block after 'then'");
            return nullptr;
        }
    }
    
    // Parse optional 'end' block
    ASTNodePtr end_block = nullptr;
    if (match(TokenType::TOKEN_KW_END)) {
        if (!match(TokenType::TOKEN_LEFT_BRACE)) {
            error("Expected '{' after 'end'");
            return nullptr;
        }
        end_block = parseBlock();
        if (!end_block) {
            error("Expected block after 'end'");
            return nullptr;
        }
    }
    
    return std::make_shared<WhenStmt>(condition, body, then_block, end_block, 
                                      whenToken.line, whenToken.column);
}

// Parse pick statement: pick(selector) { case1, case2, ... }
// Cases: pattern { body } or label:pattern { body } or (!) { unreachable }
// Patterns: (< 10), (9), (10..20), (*), (!), etc.
ASTNodePtr Parser::parsePickStatement() {
    using namespace frontend;
    Token pickToken = previous();
    
    // Expect '(' after 'pick'
    if (!match(TokenType::TOKEN_LEFT_PAREN)) {
        error("Expected '(' after 'pick'");
        return nullptr;
    }
    
    // Parse selector expression
    ASTNodePtr selector = parseExpression();
    if (!selector) {
        error("Expected expression in pick selector");
        return nullptr;
    }
    
    // Expect ')' after selector
    if (!match(TokenType::TOKEN_RIGHT_PAREN)) {
        error("Expected ')' after pick selector");
        return nullptr;
    }
    
    // Expect '{' to start cases
    if (!match(TokenType::TOKEN_LEFT_BRACE)) {
        error("Expected '{' to start pick cases");
        return nullptr;
    }
    
    // Parse cases (comma-separated)
    std::vector<ASTNodePtr> cases;
    
    while (!check(TokenType::TOKEN_RIGHT_BRACE) && !isAtEnd()) {
        // Check for optional label (identifier or keyword followed by ':' before '(')
        // Labels can be identifiers or keywords used as labels
        std::string label;
        Token currentToken = peek();
        if (check(TokenType::TOKEN_IDENTIFIER) || 
            (currentToken.lexeme.length() > 0 && currentToken.type != TokenType::TOKEN_LEFT_PAREN)) {
            size_t savedPos = current;
            Token labelToken = advance(); // consume potential label
            if (check(TokenType::TOKEN_COLON)) {
                advance(); // consume ':'
                // This was a label
                label = labelToken.lexeme;
            } else {
                // Not a label, backtrack
                current = savedPos;
            }
        }
        
        // Expect '(' to start pattern
        if (!match(TokenType::TOKEN_LEFT_PAREN)) {
            error("Expected '(' to start pick case pattern");
            break;
        }
        
        // Check for unreachable marker (!)
        bool is_unreachable = false;
        if (match(TokenType::TOKEN_BANG)) {
            is_unreachable = true;
        }
        
        // Parse pattern (expression or wildcard *)
        ASTNodePtr pattern = nullptr;
        if (!is_unreachable) {
            if (match(TokenType::TOKEN_STAR)) {
                // Wildcard '*' - represented as string literal
                pattern = std::make_shared<LiteralExpr>("*", 
                                                        previous().line, previous().column);
            } else {
                // Regular pattern expression (comparison, range, value, etc.)
                pattern = parseExpression();
                if (!pattern) {
                    error("Expected pattern expression in pick case");
                    break;
                }
            }
        }
        
        // Expect ')' after pattern
        if (!match(TokenType::TOKEN_RIGHT_PAREN)) {
            error("Expected ')' after pick case pattern");
            break;
        }
        
        // Expect '{' to start case body
        if (!match(TokenType::TOKEN_LEFT_BRACE)) {
            error("Expected '{' to start pick case body");
            break;
        }
        
        // Parse case body block
        ASTNodePtr body = parseBlock();
        if (!body) {
            error("Expected block for pick case body");
            break;
        }
        
        // Create PickCase node
        cases.push_back(std::make_shared<PickCase>(label, pattern, body, is_unreachable,
                                                     pickToken.line, pickToken.column));
        
        // Cases are comma-separated
        if (!match(TokenType::TOKEN_COMMA)) {
            // No more cases
            break;
        }
    }
    
    // Expect '}' to close pick
    if (!match(TokenType::TOKEN_RIGHT_BRACE)) {
        error("Expected '}' to close pick statement");
        return nullptr;
    }
    
    return std::make_shared<PickStmt>(selector, cases, pickToken.line, pickToken.column);
}

// Parse fall statement: fall(label);
ASTNodePtr Parser::parseFallStatement() {
    using namespace frontend;
    Token fallToken = previous();
    
    // Expect '(' after 'fall'
    if (!match(TokenType::TOKEN_LEFT_PAREN)) {
        error("Expected '(' after 'fall'");
        return nullptr;
    }
    
    // Expect label identifier
    if (!check(TokenType::TOKEN_IDENTIFIER)) {
        error("Expected label identifier in fall statement");
        return nullptr;
    }
    
    Token labelToken = advance();
    std::string label = labelToken.lexeme;
    
    // Expect ')' after label
    if (!match(TokenType::TOKEN_RIGHT_PAREN)) {
        error("Expected ')' after fall label");
        return nullptr;
    }
    
    // Expect ';' to end statement
    if (!match(TokenType::TOKEN_SEMICOLON)) {
        error("Expected ';' after fall statement");
        return nullptr;
    }
    
    return std::make_shared<FallStmt>(label, fallToken.line, fallToken.column);
}

ASTNodePtr Parser::parse() {
    std::vector<ASTNodePtr> declarations;
    
    while (!isAtEnd()) {
        if (auto stmt = parseStatement()) {
            declarations.push_back(stmt);
        } else {
            synchronize(); // Error recovery
        }
    }
    
    return std::make_shared<ProgramNode>(declarations, 0, 0);
}

// ============================================================================
// Phase 3.4: Generic Syntax Parsing
// ============================================================================

// Parse generic parameters with optional trait constraints: <T: Trait1 & Trait2, U>
std::vector<GenericParamInfo> Parser::parseGenericParams() {
    using namespace frontend;
    
    std::vector<GenericParamInfo> params;
    
    // Consume the '<'
    if (!match(TokenType::TOKEN_LESS)) {
        return params;  // No generic params
    }
    
    // Parse first parameter
    Token paramToken = consume(TokenType::TOKEN_IDENTIFIER, "Expected type parameter name");
    
    // Check for trait constraints: T: Trait1 & Trait2
    std::vector<std::string> constraints;
    if (match(TokenType::TOKEN_COLON)) {
        // Parse first trait bound
        Token traitToken = consume(TokenType::TOKEN_IDENTIFIER, "Expected trait name after ':'");
        constraints.push_back(traitToken.lexeme);
        
        // Parse additional trait bounds with '&'
        while (match(TokenType::TOKEN_AMPERSAND)) {
            Token nextTrait = consume(TokenType::TOKEN_IDENTIFIER, "Expected trait name after '&'");
            constraints.push_back(nextTrait.lexeme);
        }
    }
    
    params.emplace_back(paramToken.lexeme, constraints);
    
    // Parse remaining parameters
    while (match(TokenType::TOKEN_COMMA)) {
        Token nextParam = consume(TokenType::TOKEN_IDENTIFIER, "Expected type parameter name");
        
        // Check for constraints on this parameter
        std::vector<std::string> nextConstraints;
        if (match(TokenType::TOKEN_COLON)) {
            // Parse first trait bound
            Token traitToken = consume(TokenType::TOKEN_IDENTIFIER, "Expected trait name after ':'");
            nextConstraints.push_back(traitToken.lexeme);
            
            // Parse additional trait bounds with '&'
            while (match(TokenType::TOKEN_AMPERSAND)) {
                Token nextTrait = consume(TokenType::TOKEN_IDENTIFIER, "Expected trait name after '&'");
                nextConstraints.push_back(nextTrait.lexeme);
            }
        }
        
        params.emplace_back(nextParam.lexeme, nextConstraints);
    }
    
    // Consume the '>'
    consume(TokenType::TOKEN_GREATER, "Expected '>' after generic parameters");
    
    return params;
}

// Parse explicit type arguments (turbofish syntax): ::<T, U, ...>
// This is called when we see :: followed by < in a call expression
std::vector<std::string> Parser::parseExplicitTypeArgs() {
    using namespace frontend;
    
    std::vector<std::string> typeArgs;
    
    // We've already consumed '::' at this point, now expect '<'
    if (!match(TokenType::TOKEN_LESS)) {
        error("Expected '<' after '::' in turbofish syntax");
        return typeArgs;
    }
    
    // Parse first type argument (can be type keyword or generic type parameter name)
    if (isAtEnd()) {
        error("Unexpected end of input in turbofish");
        return typeArgs;
    }
    Token typeToken = advance();
    if (!isTypeKeyword(typeToken.type) && typeToken.type != TokenType::TOKEN_IDENTIFIER) {
        error("Expected type name in turbofish, got: " + typeToken.lexeme);
        return typeArgs;
    }
    typeArgs.push_back(typeToken.lexeme);
    
    // Parse remaining type arguments
    while (match(TokenType::TOKEN_COMMA)) {
        if (isAtEnd()) {
            error("Unexpected end of input after ',' in turbofish");
            return typeArgs;
        }
        Token nextType = advance();
        if (!isTypeKeyword(nextType.type) && nextType.type != TokenType::TOKEN_IDENTIFIER) {
            error("Expected type name after ',', got: " + nextType.lexeme);
            return typeArgs;
        }
        typeArgs.push_back(nextType.lexeme);
    }
    
    // Consume the '>'
    consume(TokenType::TOKEN_GREATER, "Expected '>' after type arguments");
    
    return typeArgs;
}

// Check if current token is a generic type reference (*T syntax)
bool Parser::isGenericTypeReference() const {
    using namespace frontend;
    
    if (!check(TokenType::TOKEN_STAR)) {
        return false;
    }
    
    // Look ahead to see if * is followed by an identifier
    if (current + 1 < tokens.size()) {
        return tokens[current + 1].type == TokenType::TOKEN_IDENTIFIER;
    }
    
    return false;
}

bool Parser::hasErrors() const {
    return !errors.empty();
}

const std::vector<std::string>& Parser::getErrors() const {
    return errors;
}

} // namespace aria
