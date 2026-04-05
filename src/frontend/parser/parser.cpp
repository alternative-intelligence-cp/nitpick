#include "frontend/parser/parser.h"
#include "frontend/ast/type.h"
#include "frontend/ast/expr.h"
#include <sstream>
#include <iostream>
#include <cctype>

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
    
    // Defaults / scoped expression fallback (v0.4.3)
    {TokenType::TOKEN_QUESTION_PIPE, 1},
    {TokenType::TOKEN_KW_DEFAULTS, 1},
    
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
    
    // Cast shorthand (binds tighter than pipeline, looser than postfix)
    {TokenType::TOKEN_FAT_ARROW, 15},
    
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

// Accept TOKEN_IDENTIFIER or any contextual keyword that may reasonably be used
// as a variable/parameter name (allocation qualifiers, common soft keywords).
Token Parser::consumeName(const std::string& context) {
    Token t = peek();
    switch (t.type) {
        case TokenType::TOKEN_IDENTIFIER:
        case TokenType::TOKEN_KW_RESULT:
        case TokenType::TOKEN_KW_FUNC:
        case TokenType::TOKEN_KW_OBJ:
        case TokenType::TOKEN_KW_BUFFER:
        case TokenType::TOKEN_KW_PROCESS:
        case TokenType::TOKEN_KW_STACK:   // allocation qualifier — contextual
        case TokenType::TOKEN_KW_GC:      // allocation qualifier — contextual
            return advance();
        default:
            error("Expected " + context + " name");
            return t;
    }
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
            case TokenType::TOKEN_KW_COMPTIME:
            case TokenType::TOKEN_KW_INLINE:
            case TokenType::TOKEN_KW_NOINLINE:
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
                    call->isPipelineCall = true;
                    left = funcExpr;
                } else {
                    // Create new CallExpr: data |> func → func(data)
                    std::vector<ASTNodePtr> args;
                    args.push_back(left);
                    auto pipeCall = std::make_shared<CallExpr>(funcExpr, args, op.line, op.column);
                    pipeCall->isPipelineCall = true;
                    left = pipeCall;
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
                auto pipeCall = std::make_shared<CallExpr>(left, args, op.line, op.column);
                pipeCall->isPipelineCall = true;
                left = pipeCall;
                continue;
            }
            
            // Special case: Defaults operator (?| or defaults keyword)
            // Scoped expression fallback (v0.4.3) — wraps entire preceding sub-expression.
            if (op.type == TokenType::TOKEN_QUESTION_PIPE || op.type == TokenType::TOKEN_KW_DEFAULTS) {
                // Parse restricted fallback: only literals, identifiers, or 'unknown'
                Token fallbackToken = peek();
                ASTNodePtr fallback = nullptr;
                
                if (fallbackToken.type == TokenType::TOKEN_KW_UNKNOWN) {
                    fallback = parsePrimary();
                } else if (fallbackToken.isLiteral() || fallbackToken.type == TokenType::TOKEN_IDENTIFIER ||
                           fallbackToken.type == TokenType::TOKEN_KW_ERR ||
                           fallbackToken.type == TokenType::TOKEN_KW_TRUE ||
                           fallbackToken.type == TokenType::TOKEN_KW_FALSE) {
                    fallback = parsePrimary();
                } else {
                    error("Fallback for 'defaults'/'?|' must be a literal, variable, or 'unknown' — "
                          "not a function call or compound expression");
                    return nullptr;
                }
                
                if (!fallback) {
                    error("Expected fallback value after '?|' / 'defaults'");
                    return nullptr;
                }
                
                left = std::make_shared<DefaultsExpr>(left, std::move(fallback), op.line, op.column);
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
        
        // Cast shorthand: expr => TypeName
        // Right-hand side is a type, not an expression, so handled separately.
        if (op.type == TokenType::TOKEN_FAT_ARROW) {
            advance(); // consume '=>'
            ASTNodePtr typeNode = parseType();
            if (!typeNode) {
                error("Expected type name after '=>'");
                return nullptr;
            }
            std::string targetTypeName = typeNode->toString();
            left = std::make_shared<CastExpr>(left, typeNode, targetTypeName, false, op.line, op.column);
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
        token.type == TokenType::TOKEN_INTEGER_U2048 ||
        token.type == TokenType::TOKEN_INTEGER_U4096 ||
        // Signed integers
        token.type == TokenType::TOKEN_INTEGER_I8 ||
        token.type == TokenType::TOKEN_INTEGER_I16 ||
        token.type == TokenType::TOKEN_INTEGER_I32 ||
        token.type == TokenType::TOKEN_INTEGER_I64 ||
        token.type == TokenType::TOKEN_INTEGER_I128 ||
        token.type == TokenType::TOKEN_INTEGER_I256 ||
        token.type == TokenType::TOKEN_INTEGER_I512 ||
        token.type == TokenType::TOKEN_INTEGER_I1024 ||
        token.type == TokenType::TOKEN_INTEGER_I2048 ||
        token.type == TokenType::TOKEN_INTEGER_I4096 ||
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
        token.type == TokenType::TOKEN_FLOAT_FIX256 ||
        token.type == TokenType::TOKEN_FLOAT_TFP32 ||
        token.type == TokenType::TOKEN_FLOAT_TFP64) {
        
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
        auto lit = std::make_shared<LiteralExpr>(std::monostate{}, line, col);
        lit->explicit_type = "NULL";  // Mark as NULL literal for type checking
        return lit;
    }
    
    // NIL literal (Aria native - absence of value)
    if (token.type == TokenType::TOKEN_KW_NIL) {
        int line = token.line;
        int col = token.column;
        advance();
        auto lit = std::make_shared<LiteralExpr>(std::monostate{}, line, col);
        lit->explicit_type = "NIL";  // Mark as NIL literal for type checking
        return lit;
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
        auto lit = std::make_shared<LiteralExpr>(std::string("ERR"), line, col);
        lit->explicit_type = "ERR";  // Mark as keyword sentinel, not string literal
        return lit;
    }
    
    // unknown literal (indeterminate value)
    if (token.type == TokenType::TOKEN_KW_UNKNOWN) {
        int line = token.line;
        int col = token.column;
        advance();
        // unknown is represented as a special literal
        // Similar to ERR but semantically different: unknown = indeterminate/not-yet-known
        // The semantic analyzer will handle type-specific unknown values
        auto lit = std::make_shared<LiteralExpr>(std::string("unknown"), line, col);
        lit->explicit_type = "UNKNOWN";  // Mark as keyword sentinel, not string literal
        return lit;
    }
    
    // Comptime expression: comptime(expr) — evaluate at compile time
    if (token.type == TokenType::TOKEN_KW_COMPTIME) {
        int line = token.line;
        int col = token.column;
        advance(); // consume 'comptime'
        
        consume(TokenType::TOKEN_LEFT_PAREN, "Expected '(' after 'comptime'");
        ASTNodePtr expr = parseExpression();
        if (!expr) {
            error("Expected expression inside comptime()");
            return nullptr;
        }
        consume(TokenType::TOKEN_RIGHT_PAREN, "Expected ')' after comptime expression");
        
        return std::make_shared<ComptimeExpr>(expr, line, col);
    }
    
    // Identifier
    if (token.type == TokenType::TOKEN_IDENTIFIER) {
        std::string lexeme = token.lexeme;  // Save before advance()
        int line = token.line;
        int col = token.column;
        advance();
        
        // v0.8.3: Check for macro invocation: name!(args)
        if (check(TokenType::TOKEN_BANG) && 
            current + 1 < tokens.size() && tokens[current + 1].type == TokenType::TOKEN_LEFT_PAREN) {
            advance();  // consume !
            advance();  // consume (
            
            std::vector<ASTNodePtr> arguments;
            if (!check(TokenType::TOKEN_RIGHT_PAREN)) {
                do {
                    ASTNodePtr arg = parseExpression();
                    if (arg) arguments.push_back(arg);
                } while (match(TokenType::TOKEN_COMMA));
            }
            
            consume(TokenType::TOKEN_RIGHT_PAREN, "Expected ')' after macro arguments");
            return std::make_shared<MacroInvocationExpr>(lexeme, arguments, line, col);
        }
        
        // Check for instance<T>(...) constructor syntax
        if (lexeme == "instance" && check(TokenType::TOKEN_LESS)) {
            advance(); // consume '<'
            
            // Parse type name
            Token typeToken = consume(TokenType::TOKEN_IDENTIFIER, "Expected type name after 'instance<'");
            std::string typeName = typeToken.lexeme;
            
            consume(TokenType::TOKEN_GREATER, "Expected '>' after type name in instance<T>");
            
            // Now we should have '(' for the argument list
            // This will be parsed as a call expression in parsePostfix/parseCallExpression
            // Transform instance<TypeName> to TypeName_create as identifier
            std::string createFuncName = typeName + "_create";
            return std::make_shared<IdentifierExpr>(createFuncName, line, col);
        }
        
        // Check for sys() / sys!!() / sys!!!() — tiered syscall builtin
        if (lexeme == "sys") {
            std::string calleeName = "sys";
            
            // Check for tier modifiers: !! (full) or !!! (raw)
            if (check(TokenType::TOKEN_BANG_BANG_BANG)) {
                advance(); // consume !!!
                calleeName = "sys!!!";
            } else if (check(TokenType::TOKEN_BANG_BANG)) {
                advance(); // consume !!
                calleeName = "sys!!";
            }
            
            consume(TokenType::TOKEN_LEFT_PAREN, "Expected '(' after " + calleeName);
            
            std::vector<ASTNodePtr> arguments;
            if (!check(TokenType::TOKEN_RIGHT_PAREN)) {
                do {
                    ASTNodePtr arg = parseExpression();
                    if (arg) {
                        arguments.push_back(arg);
                    }
                } while (match(TokenType::TOKEN_COMMA));
            }
            
            consume(TokenType::TOKEN_RIGHT_PAREN, "Expected ')' after " + calleeName + " arguments");
            
            auto identExpr = std::make_shared<IdentifierExpr>(calleeName, line, col);
            return std::make_shared<CallExpr>(identExpr, arguments, line, col);
        }
        
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

    // Lambda expression: retType(params) { body }
    // Detect: typeKw ( [typeKw : name | ')'] ) { ...
    // Only trigger parseLambda if params look like typed parameters (not plain expression)
    if (isTypeKeyword(token.type) && peekNext().type == TokenType::TOKEN_LEFT_PAREN) {
        // Lookahead: after '(', is there a type keyword (typed param) or ')'?
        // This distinguishes lambda from a hypothetical function call starting with a type keyword
        if (current + 2 < tokens.size()) {
            TokenType afterParen = tokens[current + 2].type;
            if (isTypeKeyword(afterParen) || afterParen == TokenType::TOKEN_RIGHT_PAREN) {
                return parseLambda();
            }
        }
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
    
    // Allow 'buffer' keyword to be used as identifier (common variable name)
    if (token.type == TokenType::TOKEN_KW_BUFFER) {
        std::string lexeme = token.lexeme;  // Save before advance()
        int line = token.line;
        int col = token.column;
        advance();
        return std::make_shared<IdentifierExpr>(lexeme, line, col);
    }

    // Allow 'stack' keyword as identifier (allocation qualifier, contextual)
    // e.g.  int32:stack = 42;  or  push(stack, value)
    if (token.type == TokenType::TOKEN_KW_STACK) {
        std::string lexeme = token.lexeme;
        int line = token.line;
        int col = token.column;
        advance();
        return std::make_shared<IdentifierExpr>(lexeme, line, col);
    }

    // Allow 'gc' keyword as identifier (allocation qualifier, contextual)
    if (token.type == TokenType::TOKEN_KW_GC) {
        std::string lexeme = token.lexeme;
        int line = token.line;
        int col = token.column;
        advance();
        return std::make_shared<IdentifierExpr>(lexeme, line, col);
    }

    // Parenthesized expression or type cast: (expr) or (expr:type)
    if (token.type == TokenType::TOKEN_LEFT_PAREN) {
        int parenLine = token.line;
        int parenCol = token.column;
        advance(); // consume '('
        
        ASTNodePtr expr = parseExpression();
        if (!expr) {
            error("Expected expression inside parentheses");
            return nullptr;
        }
        
        // Check for type cast syntax: (expr:type)
        if (check(TokenType::TOKEN_COLON)) {
            advance(); // consume ':'
            
            ASTNodePtr typeNode = parseType();
            if (!typeNode) {
                error("Expected type after ':' in cast expression");
                return nullptr;
            }
            
            consume(TokenType::TOKEN_RIGHT_PAREN, "Expected ')' after type cast");
            
            // Create cast expression - this is explicit type annotation
            std::string targetTypeName = typeNode->toString();
            return std::make_shared<CastExpr>(expr, typeNode, targetTypeName, 
                                             false, parenLine, parenCol);
        }
        
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
    
    // Check for @sizeof(type_or_expr) — must come before general @ handling
    if (token.type == TokenType::TOKEN_AT) {
        Token next = peekNext();
        if (next.type == TokenType::TOKEN_IDENTIFIER && next.lexeme == "sizeof") {
            int line = token.line;
            int col = token.column;
            advance(); // consume '@'
            advance(); // consume 'sizeof'
            
            consume(TokenType::TOKEN_LEFT_PAREN, "Expected '(' after @sizeof");
            
            // The argument can be a type keyword (int64, uint32, etc.) or an expression
            ASTNodePtr arg;
            Token argToken = peek();
            if (isTypeKeyword(argToken.type)) {
                // Type keyword — convert to identifier with type name
                std::string typeName = argToken.lexeme;
                advance(); // consume the type keyword
                arg = std::make_shared<IdentifierExpr>(typeName, argToken.line, argToken.column);
            } else {
                arg = parseExpression();
            }
            if (!arg) {
                error("Expected type or expression in @sizeof()");
                return nullptr;
            }
            
            consume(TokenType::TOKEN_RIGHT_PAREN, "Expected ')' after @sizeof argument");
            
            // Build as CallExpr with callee "@sizeof" so type checker and codegen recognize it
            std::vector<ASTNodePtr> args;
            args.push_back(arg);
            auto identExpr = std::make_shared<IdentifierExpr>("@sizeof", line, col);
            return std::make_shared<CallExpr>(identExpr, args, line, col);
        }
    }
    
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
    
    // Check for _? (drop shorthand): _? <expression> → drop(expression)
    if (token.type == TokenType::TOKEN_UNDERSCORE_QUESTION) {
        advance(); // consume '_?'
        
        ASTNodePtr operand = parseUnary();
        if (!operand) {
            error("Expected expression after '_?'");
            return nullptr;
        }
        
        // Apply postfix operators so _? myFunc().field works correctly
        operand = parsePostfix(operand);
        
        // Desugar to drop(operand)
        std::vector<ASTNodePtr> args;
        args.push_back(operand);
        auto identExpr = std::make_shared<IdentifierExpr>("drop", token.line, token.column);
        return std::make_shared<CallExpr>(identExpr, args, token.line, token.column);
    }
    
    // Check for _! (raw shorthand): _! <expression> → raw(expression)
    if (token.type == TokenType::TOKEN_UNDERSCORE_BANG) {
        advance(); // consume '_!'
        
        // B9 FIX: Parse at pipeline precedence so _! captures pipe chains
        ASTNodePtr operand = parseExpression(14);
        if (!operand) {
            error("Expected expression after '_!'");
            return nullptr;
        }
        
        // Desugar to raw(operand)
        std::vector<ASTNodePtr> args;
        args.push_back(operand);
        auto identExpr = std::make_shared<IdentifierExpr>("raw", token.line, token.column);
        return std::make_shared<CallExpr>(identExpr, args, token.line, token.column);
    }
    
    // raw expr — keyword form (same as _! shorthand)
    if (token.type == TokenType::TOKEN_KW_RAW) {
        advance(); // consume 'raw'
        // B9 FIX: Parse at pipeline precedence so raw captures pipe chains
        ASTNodePtr operand = parseExpression(14);
        if (!operand) {
            error("Expected expression after 'raw'");
            return nullptr;
        }
        std::vector<ASTNodePtr> args;
        args.push_back(operand);
        auto identExpr = std::make_shared<IdentifierExpr>("raw", token.line, token.column);
        return std::make_shared<CallExpr>(identExpr, args, token.line, token.column);
    }
    
    // drop expr — keyword form (same as _? shorthand)
    if (token.type == TokenType::TOKEN_KW_DROP) {
        advance(); // consume 'drop'
        ASTNodePtr operand = parseUnary();
        if (!operand) {
            error("Expected expression after 'drop'");
            return nullptr;
        }
        operand = parsePostfix(operand);
        std::vector<ASTNodePtr> args;
        args.push_back(operand);
        auto identExpr = std::make_shared<IdentifierExpr>("drop", token.line, token.column);
        return std::make_shared<CallExpr>(identExpr, args, token.line, token.column);
    }
    
    // ok expr — keyword form, returns bool
    if (token.type == TokenType::TOKEN_KW_OK) {
        advance(); // consume 'ok'
        ASTNodePtr operand = parseUnary();
        if (!operand) {
            error("Expected expression after 'ok'");
            return nullptr;
        }
        operand = parsePostfix(operand);
        std::vector<ASTNodePtr> args;
        args.push_back(operand);
        auto identExpr = std::make_shared<IdentifierExpr>("ok", token.line, token.column);
        return std::make_shared<CallExpr>(identExpr, args, token.line, token.column);
    }
    
    // apop — keyword or paren form, no arguments
    if (token.type == TokenType::TOKEN_KW_APOP) {
        advance(); // consume 'apop'
        if (check(TokenType::TOKEN_LEFT_PAREN)) {
            advance(); // consume '('
            consume(TokenType::TOKEN_RIGHT_PAREN, "Expected ')' after 'apop('");
        }
        std::vector<ASTNodePtr> args;
        auto identExpr = std::make_shared<IdentifierExpr>("apop", token.line, token.column);
        return std::make_shared<CallExpr>(identExpr, args, token.line, token.column);
    }
    
    // apeek — keyword or paren form, no arguments
    if (token.type == TokenType::TOKEN_KW_APEEK) {
        advance(); // consume 'apeek'
        if (check(TokenType::TOKEN_LEFT_PAREN)) {
            advance(); // consume '('
            consume(TokenType::TOKEN_RIGHT_PAREN, "Expected ')' after 'apeek('");
        }
        std::vector<ASTNodePtr> args;
        auto identExpr = std::make_shared<IdentifierExpr>("apeek", token.line, token.column);
        return std::make_shared<CallExpr>(identExpr, args, token.line, token.column);
    }
    
    // acap — keyword or paren form, no arguments (return stack capacity in bytes)
    if (token.type == TokenType::TOKEN_KW_ACAP) {
        advance(); // consume 'acap'
        if (check(TokenType::TOKEN_LEFT_PAREN)) {
            advance(); // consume '('
            consume(TokenType::TOKEN_RIGHT_PAREN, "Expected ')' after 'acap('");
        }
        std::vector<ASTNodePtr> args;
        auto identExpr = std::make_shared<IdentifierExpr>("acap", token.line, token.column);
        return std::make_shared<CallExpr>(identExpr, args, token.line, token.column);
    }
    
    // asize — keyword or paren form, no arguments (return bytes used on stack)
    if (token.type == TokenType::TOKEN_KW_ASIZE) {
        advance(); // consume 'asize'
        if (check(TokenType::TOKEN_LEFT_PAREN)) {
            advance(); // consume '('
            consume(TokenType::TOKEN_RIGHT_PAREN, "Expected ')' after 'asize('");
        }
        std::vector<ASTNodePtr> args;
        auto identExpr = std::make_shared<IdentifierExpr>("asize", token.line, token.column);
        return std::make_shared<CallExpr>(identExpr, args, token.line, token.column);
    }
    
    // afits — keyword or paren form, single argument (check if value fits on stack)
    if (token.type == TokenType::TOKEN_KW_AFITS) {
        advance(); // consume 'afits'
        bool hasParen = check(TokenType::TOKEN_LEFT_PAREN);
        if (hasParen) advance(); // consume '('
        ASTNodePtr operand = parseExpression();
        if (!operand) {
            error("Expected expression after 'afits'");
            return nullptr;
        }
        if (hasParen) {
            consume(TokenType::TOKEN_RIGHT_PAREN, "Expected ')' after afits argument");
        }
        std::vector<ASTNodePtr> args;
        args.push_back(operand);
        auto identExpr = std::make_shared<IdentifierExpr>("afits", token.line, token.column);
        return std::make_shared<CallExpr>(identExpr, args, token.line, token.column);
    }
    
    // atype — keyword or paren form, no arguments (return type tag of top stack item)
    if (token.type == TokenType::TOKEN_KW_ATYPE) {
        advance(); // consume 'atype'
        if (check(TokenType::TOKEN_LEFT_PAREN)) {
            advance(); // consume '('
            consume(TokenType::TOKEN_RIGHT_PAREN, "Expected ')' after 'atype('");
        }
        std::vector<ASTNodePtr> args;
        auto identExpr = std::make_shared<IdentifierExpr>("atype", token.line, token.column);
        return std::make_shared<CallExpr>(identExpr, args, token.line, token.column);
    }
    
    // ====================================================================
    // ahash builtins — paren syntax: ahash(cap), ahset(h,k,v), etc.
    // ====================================================================
    
    // ahash(cap) — create hash table, returns handle
    if (token.type == TokenType::TOKEN_KW_AHASH) {
        advance(); // consume 'ahash'
        consume(TokenType::TOKEN_LEFT_PAREN, "Expected '(' after 'ahash'");
        ASTNodePtr cap = parseExpression();
        if (!cap) { error("Expected capacity expression in ahash()"); return nullptr; }
        consume(TokenType::TOKEN_RIGHT_PAREN, "Expected ')' after ahash argument");
        std::vector<ASTNodePtr> args;
        args.push_back(cap);
        auto identExpr = std::make_shared<IdentifierExpr>("ahash", token.line, token.column);
        return std::make_shared<CallExpr>(identExpr, args, token.line, token.column);
    }
    
    // ahset(handle, key, val) — set key/value pair, returns int32 (0=ok, -1=overflow)
    if (token.type == TokenType::TOKEN_KW_AHSET) {
        advance(); // consume 'ahset'
        consume(TokenType::TOKEN_LEFT_PAREN, "Expected '(' after 'ahset'");
        ASTNodePtr handle = parseExpression();
        if (!handle) { error("Expected handle in ahset()"); return nullptr; }
        consume(TokenType::TOKEN_COMMA, "Expected ',' after handle in ahset()");
        ASTNodePtr key = parseExpression();
        if (!key) { error("Expected key in ahset()"); return nullptr; }
        consume(TokenType::TOKEN_COMMA, "Expected ',' after key in ahset()");
        ASTNodePtr val = parseExpression();
        if (!val) { error("Expected value in ahset()"); return nullptr; }
        consume(TokenType::TOKEN_RIGHT_PAREN, "Expected ')' after ahset arguments");
        std::vector<ASTNodePtr> args;
        args.push_back(handle);
        args.push_back(key);
        args.push_back(val);
        auto identExpr = std::make_shared<IdentifierExpr>("ahset", token.line, token.column);
        return std::make_shared<CallExpr>(identExpr, args, token.line, token.column);
    }
    
    // ahget(handle, key) — get value by key, type from receiving variable
    if (token.type == TokenType::TOKEN_KW_AHGET) {
        advance(); // consume 'ahget'
        consume(TokenType::TOKEN_LEFT_PAREN, "Expected '(' after 'ahget'");
        ASTNodePtr handle = parseExpression();
        if (!handle) { error("Expected handle in ahget()"); return nullptr; }
        consume(TokenType::TOKEN_COMMA, "Expected ',' after handle in ahget()");
        ASTNodePtr key = parseExpression();
        if (!key) { error("Expected key in ahget()"); return nullptr; }
        consume(TokenType::TOKEN_RIGHT_PAREN, "Expected ')' after ahget arguments");
        std::vector<ASTNodePtr> args;
        args.push_back(handle);
        args.push_back(key);
        auto identExpr = std::make_shared<IdentifierExpr>("ahget", token.line, token.column);
        return std::make_shared<CallExpr>(identExpr, args, token.line, token.column);
    }
    
    // ahcount(handle) — return entry count
    if (token.type == TokenType::TOKEN_KW_AHCOUNT) {
        advance(); // consume 'ahcount'
        consume(TokenType::TOKEN_LEFT_PAREN, "Expected '(' after 'ahcount'");
        ASTNodePtr handle = parseExpression();
        if (!handle) { error("Expected handle in ahcount()"); return nullptr; }
        consume(TokenType::TOKEN_RIGHT_PAREN, "Expected ')' after ahcount argument");
        std::vector<ASTNodePtr> args;
        args.push_back(handle);
        auto identExpr = std::make_shared<IdentifierExpr>("ahcount", token.line, token.column);
        return std::make_shared<CallExpr>(identExpr, args, token.line, token.column);
    }
    
    // ahsize(handle) — return current payload bytes used
    if (token.type == TokenType::TOKEN_KW_AHSIZE) {
        advance(); // consume 'ahsize'
        consume(TokenType::TOKEN_LEFT_PAREN, "Expected '(' after 'ahsize'");
        ASTNodePtr handle = parseExpression();
        if (!handle) { error("Expected handle in ahsize()"); return nullptr; }
        consume(TokenType::TOKEN_RIGHT_PAREN, "Expected ')' after ahsize argument");
        std::vector<ASTNodePtr> args;
        args.push_back(handle);
        auto identExpr = std::make_shared<IdentifierExpr>("ahsize", token.line, token.column);
        return std::make_shared<CallExpr>(identExpr, args, token.line, token.column);
    }
    
    // ahfits(handle, val) — check if value fits within remaining capacity
    if (token.type == TokenType::TOKEN_KW_AHFITS) {
        advance(); // consume 'ahfits'
        consume(TokenType::TOKEN_LEFT_PAREN, "Expected '(' after 'ahfits'");
        ASTNodePtr handle = parseExpression();
        if (!handle) { error("Expected handle in ahfits()"); return nullptr; }
        consume(TokenType::TOKEN_COMMA, "Expected ',' after handle in ahfits()");
        ASTNodePtr val = parseExpression();
        if (!val) { error("Expected value in ahfits()"); return nullptr; }
        consume(TokenType::TOKEN_RIGHT_PAREN, "Expected ')' after ahfits arguments");
        std::vector<ASTNodePtr> args;
        args.push_back(handle);
        args.push_back(val);
        auto identExpr = std::make_shared<IdentifierExpr>("ahfits", token.line, token.column);
        return std::make_shared<CallExpr>(identExpr, args, token.line, token.column);
    }
    
    // ahtype(handle, key) — return type tag of value at key (-1 if not found)
    if (token.type == TokenType::TOKEN_KW_AHTYPE) {
        advance(); // consume 'ahtype'
        consume(TokenType::TOKEN_LEFT_PAREN, "Expected '(' after 'ahtype'");
        ASTNodePtr handle = parseExpression();
        if (!handle) { error("Expected handle in ahtype()"); return nullptr; }
        consume(TokenType::TOKEN_COMMA, "Expected ',' after handle in ahtype()");
        ASTNodePtr key = parseExpression();
        if (!key) { error("Expected key in ahtype()"); return nullptr; }
        consume(TokenType::TOKEN_RIGHT_PAREN, "Expected ')' after ahtype arguments");
        std::vector<ASTNodePtr> args;
        args.push_back(handle);
        args.push_back(key);
        auto identExpr = std::make_shared<IdentifierExpr>("ahtype", token.line, token.column);
        return std::make_shared<CallExpr>(identExpr, args, token.line, token.column);
    }

    // ahdelete(handle, key) — delete key from hash table, returns int32 (0=ok, -1=not found)
    if (token.type == TokenType::TOKEN_KW_AHDELETE) {
        advance(); // consume 'ahdelete'
        consume(TokenType::TOKEN_LEFT_PAREN, "Expected '(' after 'ahdelete'");
        ASTNodePtr handle = parseExpression();
        if (!handle) { error("Expected handle in ahdelete()"); return nullptr; }
        consume(TokenType::TOKEN_COMMA, "Expected ',' after handle in ahdelete()");
        ASTNodePtr key = parseExpression();
        if (!key) { error("Expected key in ahdelete()"); return nullptr; }
        consume(TokenType::TOKEN_RIGHT_PAREN, "Expected ')' after ahdelete arguments");
        std::vector<ASTNodePtr> args;
        args.push_back(handle);
        args.push_back(key);
        auto identExpr = std::make_shared<IdentifierExpr>("ahdelete", token.line, token.column);
        return std::make_shared<CallExpr>(identExpr, args, token.line, token.column);
    }

    // ahhas(handle, key) — check if key exists, returns int32 (1=exists, 0=not)
    if (token.type == TokenType::TOKEN_KW_AHHAS) {
        advance(); // consume 'ahhas'
        consume(TokenType::TOKEN_LEFT_PAREN, "Expected '(' after 'ahhas'");
        ASTNodePtr handle = parseExpression();
        if (!handle) { error("Expected handle in ahhas()"); return nullptr; }
        consume(TokenType::TOKEN_COMMA, "Expected ',' after handle in ahhas()");
        ASTNodePtr key = parseExpression();
        if (!key) { error("Expected key in ahhas()"); return nullptr; }
        consume(TokenType::TOKEN_RIGHT_PAREN, "Expected ')' after ahhas arguments");
        std::vector<ASTNodePtr> args;
        args.push_back(handle);
        args.push_back(key);
        auto identExpr = std::make_shared<IdentifierExpr>("ahhas", token.line, token.column);
        return std::make_shared<CallExpr>(identExpr, args, token.line, token.column);
    }

    // ahclear(handle) — clear all entries, returns void
    if (token.type == TokenType::TOKEN_KW_AHCLEAR) {
        advance(); // consume 'ahclear'
        consume(TokenType::TOKEN_LEFT_PAREN, "Expected '(' after 'ahclear'");
        ASTNodePtr handle = parseExpression();
        if (!handle) { error("Expected handle in ahclear()"); return nullptr; }
        consume(TokenType::TOKEN_RIGHT_PAREN, "Expected ')' after ahclear argument");
        std::vector<ASTNodePtr> args;
        args.push_back(handle);
        auto identExpr = std::make_shared<IdentifierExpr>("ahclear", token.line, token.column);
        return std::make_shared<CallExpr>(identExpr, args, token.line, token.column);
    }

    // ahkeys(handle) — get all keys, returns wild int8@ (pointer to char** array)
    if (token.type == TokenType::TOKEN_KW_AHKEYS) {
        advance(); // consume 'ahkeys'
        consume(TokenType::TOKEN_LEFT_PAREN, "Expected '(' after 'ahkeys'");
        ASTNodePtr handle = parseExpression();
        if (!handle) { error("Expected handle in ahkeys()"); return nullptr; }
        consume(TokenType::TOKEN_RIGHT_PAREN, "Expected ')' after ahkeys argument");
        std::vector<ASTNodePtr> args;
        args.push_back(handle);
        auto identExpr = std::make_shared<IdentifierExpr>("ahkeys", token.line, token.column);
        return std::make_shared<CallExpr>(identExpr, args, token.line, token.column);
    }
    
    // Check for failsafe call: !!! <expression>
    if (token.type == TokenType::TOKEN_BANG_BANG_BANG) {
        advance(); // consume '!!!'
        
        // Parse the error code expression
        ASTNodePtr errorCode = parseUnary();
        if (!errorCode) {
            error("Expected error code after '!!!'");
            return nullptr;
        }
        
        // Desugar to failsafe(errorCode) call
        std::vector<ASTNodePtr> args;
        args.push_back(errorCode);
        
        auto identExpr = std::make_shared<IdentifierExpr>("failsafe", token.line, token.column);
        return std::make_shared<CallExpr>(identExpr, args, token.line, token.column);
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
        
        // PRECEDENCE FIX: Apply postfix operators (member access, calls, indexing)
        // to the operand BEFORE creating the unary expression.
        // This makes !r.is_error parse as !(r.is_error) instead of (!r).is_error
        operand = parsePostfix(operand);
        
        // FIX: Explicitly pass false for isPostfix, otherwise token.line (int) is converted to true
        auto unaryExpr = std::make_shared<UnaryExpr>(token, operand, false, token.line, token.column);
        
        // Set borrow checker annotations for $ and # operators
        if (token.type == TokenType::TOKEN_DOLLAR) {
            unaryExpr->creates_loan = true;
            unaryExpr->is_mutable_loan = true;  // $x = mutable borrow
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
        
        // Check for struct literal: TypeName{ field: value, ... }
        // Only if expr is an IdentifierExpr that starts with uppercase (type name convention)
        if (token.type == TokenType::TOKEN_LEFT_BRACE && 
            expr && expr->type == ASTNode::NodeType::IDENTIFIER) {
            auto identExpr = std::static_pointer_cast<IdentifierExpr>(expr);
            std::string typeName = identExpr->name;
            
            // Only treat as struct literal if identifier starts with uppercase (type name)
            // This prevents ambiguity with blocks: max{...} vs { block }
            if (typeName.empty() || !std::isupper(typeName[0])) {
                break; // Not a struct literal, stop postfix parsing
            }
            
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
                false,  // Not null coalesce, it's Result unwrap
                false   // Not failsafe, it's regular unwrap
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
                true,  // This is null coalesce (??), not Result unwrap (?) or failsafe (!!)
                false  // Not failsafe
            );
            continue;
        }
        
        // Emphatic unwrap operator: result ?! default_value
        if (token.type == TokenType::TOKEN_QUESTION_BANG) {
            Token failsafeToken = advance(); // consume '?!'
            
            // Parse the default value expression
            ASTNodePtr defaultValue = parseExpression();
            if (!defaultValue) {
                error("Expected default value after '?!' operator");
                return nullptr;
            }
            
            // Create UnwrapExpr with isFailsafe=true
            expr = std::make_shared<UnwrapExpr>(
                expr, 
                defaultValue,
                failsafeToken.line,
                failsafeToken.column,
                false,  // Not null coalesce
                true    // This is emphatic unwrap (?!)
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
            // Check for spread operator: ..^expr
            if (check(TokenType::TOKEN_SPREAD)) {
                Token spreadToken = advance();  // consume ..^
                ASTNodePtr operand = parseExpression();
                if (!operand) {
                    error("Expected expression after '..^' spread operator");
                    return callee;
                }
                arguments.push_back(std::make_shared<SpreadExpr>(operand, spreadToken.line, spreadToken.column));
            } else {
                ASTNodePtr arg = parseExpression();
                if (arg) {
                    arguments.push_back(arg);
                }
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
    
    // Allow member name to be identifier OR certain keywords (like ERR for static members)
    Token memberToken = peek();
    std::string memberName;
    
    if (memberToken.type == TokenType::TOKEN_IDENTIFIER) {
        memberName = memberToken.lexeme;
        advance();
    } else if (memberToken.type == TokenType::TOKEN_KW_ERR) {
        // Allow ERR as static member (e.g., int1024.ERR, tbb8.ERR)
        memberName = "ERR";
        advance();
    } else if (isTypeKeyword(memberToken.type)) {
        // Allow type keywords as member names (for method chaining, etc.)
        memberName = memberToken.lexeme;
        advance();
    } else {
        error("Expected member name after '.' or '->'");
        return object;
    }
    
    // Check for Type.MEMBER static access (Type:Name pattern)
    // If object is an identifier that is a type name and not pointer/safe-nav access,
    // transform Type.MEMBER into Type_MEMBER identifier (will become call if needed)
    if (!isPointerAccess && !isSafeNav && 
        object && object->type == ASTNode::NodeType::IDENTIFIER) {
        auto identExpr = std::static_pointer_cast<IdentifierExpr>(object);
        std::string typeName = identExpr->name;
        
        // Check if identifier is a type name:
        // - Starts with uppercase (struct/class convention), OR
        // - Is a primitive type keyword (tbb8, fix256, int1024, trit, etc.)
        bool isTypeName = (!typeName.empty() && std::isupper(typeName[0]));
        if (!isTypeName) {
            // Check known primitive type families that support static constants
            isTypeName = (typeName.find("tbb") == 0 || typeName.find("fix") == 0 ||
                         typeName.find("int") == 0 || typeName.find("uint") == 0 ||
                         typeName.find("flt") == 0 || typeName.find("frac") == 0 ||
                         typeName == "trit" || typeName == "tryte" ||
                         typeName == "nit" || typeName == "nyte");
        }
        
        if (isTypeName && knownEnumNames.find(typeName) == knownEnumNames.end()) {
            // This is Type.MEMBER static access (NOT enum access)
            // Transform to Type_MEMBER identifier
            // If followed by (), it becomes Type_MEMBER(...)
            // If not followed by (), we wrap it in a call: Type_MEMBER()
            std::string staticMemberFunc = typeName + "_" + memberName;
            
            if (!check(TokenType::TOKEN_LEFT_PAREN)) {
                // Type.MEMBER without () → call Type_MEMBER()
                auto funcIdent = std::make_shared<IdentifierExpr>(staticMemberFunc, op.line, op.column);
                return std::make_shared<CallExpr>(funcIdent, std::vector<ASTNodePtr>{}, op.line, op.column);
            } else {
                // Type.MEMBER(...) → return identifier Type_MEMBER, let parseCallExpression handle the call
                return std::make_shared<IdentifierExpr>(staticMemberFunc, op.line, op.column);
            }
        }
    }
    
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
    
    // Check if this is a fraction literal {val, val, val} vs struct literal {name: val}
    // Fraction literals have positional values, struct literals have named fields
    // Peek ahead to distinguish: if we see a non-identifier or no colon after identifier, it's positional
    bool isFractionLiteral = false;
    if (!check(TokenType::TOKEN_RIGHT_BRACE) && !isAtEnd()) {
        // Look ahead to see if this is positional (fraction) or named (struct)
        size_t saved = current;
        
        // If first token is not an identifier, it's definitely positional
        if (!check(TokenType::TOKEN_IDENTIFIER)) {
            isFractionLiteral = true;
        } else {
            // It's an identifier - check if followed by colon
            advance(); // skip identifier
            if (!check(TokenType::TOKEN_COLON)) {
                // No colon means positional, not named field
                isFractionLiteral = true;
            }
        }
        
        current = saved; // restore position
    }
    
    // Handle fraction literal {whole, num, denom}
    if (isFractionLiteral) {
        std::vector<ASTNodePtr> values;
        
        // Parse comma-separated values (flexible count for different types)
        // frac types: 3 values (whole, num, denom)
        // tfp types: 2 values (exponent, mantissa)
        // Could be used for other positional initializations
        do {
            ASTNodePtr value = parseExpression();
            if (!value) {
                error("Expected expression in positional literal");
                return nullptr;
            }
            values.push_back(value);
            
            if (!check(TokenType::TOKEN_RIGHT_BRACE)) {
                if (!match(TokenType::TOKEN_COMMA)) {
                    error("Expected ',' or '}' in positional literal");
                    return nullptr;
                }
            }
        } while (!check(TokenType::TOKEN_RIGHT_BRACE) && !isAtEnd());
        
        consume(TokenType::TOKEN_RIGHT_BRACE, "Expected '}' after positional literal");
        
        // Convert to struct literal with appropriate field names based on count
        std::vector<ObjectLiteralExpr::Field> fields;
        
        if (values.size() == 2) {
            // TFP types: {exponent, mantissa}
            fields.push_back(ObjectLiteralExpr::Field("exponent", values[0]));
            fields.push_back(ObjectLiteralExpr::Field("mantissa", values[1]));
        } else if (values.size() == 3) {
            // Frac types: {whole, num, denom}
            fields.push_back(ObjectLiteralExpr::Field("whole", values[0]));
            fields.push_back(ObjectLiteralExpr::Field("num", values[1]));
            fields.push_back(ObjectLiteralExpr::Field("denom", values[2]));
        } else {
            // Generic positional literal - use indices as field names
            for (size_t i = 0; i < values.size(); i++) {
                fields.push_back(ObjectLiteralExpr::Field("_" + std::to_string(i), values[i]));
            }
        }
        
        return std::make_shared<ObjectLiteralExpr>(
            fields,
            "",  // Type will be inferred from context
            braceToken.line,
            braceToken.column
        );
    }
    
    // Handle regular struct/object literal {name: value, ...}
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
        
        Token nameToken = consumeName("parameter");
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
    advance(); // consume '{' -- parseBlock() requires '{' already consumed
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
        type == TokenType::TOKEN_KW_INT2048 || type == TokenType::TOKEN_KW_INT4096 ||
        // Unsigned integers
        type == TokenType::TOKEN_KW_UINT1 || type == TokenType::TOKEN_KW_UINT2 ||
        type == TokenType::TOKEN_KW_UINT4 || type == TokenType::TOKEN_KW_UINT8 ||
        type == TokenType::TOKEN_KW_UINT16 || type == TokenType::TOKEN_KW_UINT32 ||
        type == TokenType::TOKEN_KW_UINT64 || type == TokenType::TOKEN_KW_UINT128 ||
        type == TokenType::TOKEN_KW_UINT256 || type == TokenType::TOKEN_KW_UINT512 ||
        type == TokenType::TOKEN_KW_UINT1024 ||
        type == TokenType::TOKEN_KW_UINT2048 || type == TokenType::TOKEN_KW_UINT4096 ||
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
        type == TokenType::TOKEN_KW_ANY ||
        type == TokenType::TOKEN_KW_RESULT || type == TokenType::TOKEN_KW_ARRAY ||
        type == TokenType::TOKEN_KW_STRUCT || type == TokenType::TOKEN_KW_NIL ||
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
    if (type == TokenType::TOKEN_INTEGER_U2048) return "u2048";
    if (type == TokenType::TOKEN_INTEGER_U4096) return "u4096";
    
    // Signed integers
    if (type == TokenType::TOKEN_INTEGER_I8) return "i8";
    if (type == TokenType::TOKEN_INTEGER_I16) return "i16";
    if (type == TokenType::TOKEN_INTEGER_I32) return "i32";
    if (type == TokenType::TOKEN_INTEGER_I64) return "i64";
    if (type == TokenType::TOKEN_INTEGER_I128) return "i128";
    if (type == TokenType::TOKEN_INTEGER_I256) return "i256";
    if (type == TokenType::TOKEN_INTEGER_I512) return "i512";
    if (type == TokenType::TOKEN_INTEGER_I1024) return "i1024";
    if (type == TokenType::TOKEN_INTEGER_I2048) return "i2048";
    if (type == TokenType::TOKEN_INTEGER_I4096) return "i4096";
    
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
    if (type == TokenType::TOKEN_FLOAT_TFP32) return "tfp32";
    if (type == TokenType::TOKEN_FLOAT_TFP64) return "tfp64";
    
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
        
        // Check for pub inline/noinline/comptime func
        bool pubInline = false, pubNoinline = false, pubComptime = false;
        if (peek().type == TokenType::TOKEN_KW_INLINE) {
            pubInline = true;
            advance(); // consume 'inline'
        } else if (peek().type == TokenType::TOKEN_KW_NOINLINE) {
            pubNoinline = true;
            advance(); // consume 'noinline'
        } else if (peek().type == TokenType::TOKEN_KW_COMPTIME) {
            pubComptime = true;
            advance(); // consume 'comptime'
        }
        
        // Check for pub func (or pub inline/noinline/comptime func)
        if (match(TokenType::TOKEN_KW_FUNC)) {
            auto funcStmt = parseFuncDecl();
            // Set public flag and modifiers
            if (funcStmt && funcStmt->type == ASTNode::NodeType::FUNC_DECL) {
                auto func = std::static_pointer_cast<FuncDeclStmt>(funcStmt);
                func->isPublic = true;
                if (pubInline) func->isInline = true;
                if (pubNoinline) func->isNoInline = true;
                if (pubComptime) func->isComptime = true;
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
    
    // v0.8.3: Parse #[...] attributes before declarations
    // Attributes can appear before struct, func, enum, trait, impl
    std::vector<Attribute> pendingAttrs;
    if (check(TokenType::TOKEN_HASH)) {
        // Save position — might not be an attribute (could be # pin operator)
        size_t savedForAttr = current;
        if (current + 1 < tokens.size() && tokens[current + 1].type == TokenType::TOKEN_LEFT_BRACKET) {
            pendingAttrs = parseAttributes();
            // If we parsed attributes but no declaration follows, restore
            if (pendingAttrs.empty()) {
                current = savedForAttr;
            }
        }
    }
    
    // Check for struct declarations
    if (match(TokenType::TOKEN_KW_STRUCT)) {
        auto structDecl = parseStructDecl();
        if (structDecl && !pendingAttrs.empty()) {
            auto* sd = static_cast<StructDeclStmt*>(structDecl.get());
            sd->attributes = std::move(pendingAttrs);
            // Extract alignment from #[align(N)] attribute
            for (const auto& attr : sd->attributes) {
                if (attr.name == "align" && !attr.args.empty()) {
                    try { sd->alignment = std::stoull(attr.args[0]); } catch (...) {}
                }
            }
        }
        return structDecl;
    }
    
    // Check for enum declarations
    if (match(TokenType::TOKEN_KW_ENUM)) {
        auto enumDecl = parseEnumDecl();
        if (enumDecl && !pendingAttrs.empty()) {
            auto* ed = static_cast<EnumDeclStmt*>(enumDecl.get());
            ed->attributes = std::move(pendingAttrs);
        }
        return enumDecl;
    }

    // Check for Rules declarations (refinement types)
    if (match(TokenType::TOKEN_KW_RULES)) {
        return parseRulesDecl();
    }

    // Check for Type declarations (composable units)
    if (match(TokenType::TOKEN_KW_TYPE)) {
        return parseTypeDecl();
    }

    // Check for trait declarations
    if (match(TokenType::TOKEN_KW_TRAIT)) {
        return parseTraitDecl();
    }

    // Check for impl declarations
    if (match(TokenType::TOKEN_KW_IMPL)) {
        return parseImplDecl();
    }
    
    // v0.8.3: Check for AST-level macro declarations
    if (match(TokenType::TOKEN_KW_MACRO)) {
        return parseMacroDecl();
    }

    // Check for limit<> variable declarations (refinement types)
    if (peek().type == TokenType::TOKEN_KW_LIMIT) {
        return parseVarDecl();
    }

    // Check for qualifiers (wild, wildx, const, fixed, stack, gc, $$i, $$m) followed by type
    if (peek().type == TokenType::TOKEN_KW_WILD ||
        peek().type == TokenType::TOKEN_KW_WILDX ||
        peek().type == TokenType::TOKEN_KW_CONST ||
        peek().type == TokenType::TOKEN_KW_FIXED ||
        peek().type == TokenType::TOKEN_KW_STACK ||
        peek().type == TokenType::TOKEN_KW_GC ||
        peek().type == TokenType::TOKEN_KW_BORROW_IMM ||
        peek().type == TokenType::TOKEN_KW_BORROW_MUT) {
        return parseVarDecl();
    }
    
    // Check for function pointer variable declaration: (retType)(params):name = lambda
    // Syntax: (int32)(int32):square = int32(int32:x) { pass(x * x); };
    if (peek().type == TokenType::TOKEN_LEFT_PAREN) {
        size_t saved = current;
        advance(); // consume '('
        // Scan for matching ')' of the return type group
        int depth = 1;
        while (depth > 0 && !isAtEnd()) {
            if (check(TokenType::TOKEN_LEFT_PAREN)) depth++;
            else if (check(TokenType::TOKEN_RIGHT_PAREN)) depth--;
            if (depth > 0) advance();
        }
        // Now at ')' of return type — advance past it
        if (check(TokenType::TOKEN_RIGHT_PAREN)) {
            advance(); // consume ')'
            // Should be '(' for parameter types group
            if (check(TokenType::TOKEN_LEFT_PAREN)) {
                advance(); // consume '('
                // Scan for matching ')' of param types group
                depth = 1;
                while (depth > 0 && !isAtEnd()) {
                    if (check(TokenType::TOKEN_LEFT_PAREN)) depth++;
                    else if (check(TokenType::TOKEN_RIGHT_PAREN)) depth--;
                    if (depth > 0) advance();
                }
                if (check(TokenType::TOKEN_RIGHT_PAREN)) {
                    advance(); // consume ')'
                    // Should be ':' for variable name
                    if (check(TokenType::TOKEN_COLON)) {
                        current = saved;
                        return parseVarDecl();
                    }
                }
            }
        }
        current = saved; // not a func ptr var decl, fall through
    }

    // P0: Check for alignment attribute #[align(N)] - variable declaration
    if (peek().type == TokenType::TOKEN_HASH) {
        size_t saved = current;
        advance(); // consume '#'
        
        if (check(TokenType::TOKEN_LEFT_BRACKET)) {
            advance(); // consume '['
            
            if (check(TokenType::TOKEN_IDENTIFIER) && peek().lexeme == "align") {
                // It's an alignment attribute, restore position and parse as var decl
                current = saved;
                return parseVarDecl();
            }
        }
        
        // Not an alignment attribute, restore position and fall through
        current = saved;
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
        
        // Check for array type suffix: CustomType[] or CustomType[size]
        if (check(TokenType::TOKEN_LEFT_BRACKET)) {
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
            }
        }
        
        // Now check for colon (variable declaration) or other tokens
        if (check(TokenType::TOKEN_COLON)) {
            // It's a type annotation: CustomType:name or Box<int64>:name or CustomType[N]:name
            current = saved; // reset position
            return parseVarDecl();
        } else if (check(TokenType::TOKEN_QUESTION)) {
            advance(); // consume '?'
            if (check(TokenType::TOKEN_COLON)) {
                // Optional type annotation: CustomType?:name
                current = saved;
                return parseVarDecl();
            }
            // Not a variable declaration, restore position
            current = saved;
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
        
        // Handle 'dyn TraitName' compound type — advance past trait name
        if (tokens[saved].type == TokenType::TOKEN_KW_DYN &&
            check(TokenType::TOKEN_IDENTIFIER)) {
            advance(); // consume trait name identifier
        }
        
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
                
                // Consume any additional array dimensions [M][K]... before the colon check
                // This handles multi-dimensional array declarations: flt64[3][4]:matrix
                while (check(TokenType::TOKEN_LEFT_BRACKET)) {
                    advance(); // consume '['
                    int innerDepth = 1;
                    while (innerDepth > 0 && !isAtEnd()) {
                        if (check(TokenType::TOKEN_LEFT_BRACKET)) innerDepth++;
                        if (check(TokenType::TOKEN_RIGHT_BRACKET)) innerDepth--;
                        if (innerDepth > 0) advance();
                    }
                    if (check(TokenType::TOKEN_RIGHT_BRACKET)) advance(); // consume ']'
                }
                
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
    
    // Check for inline/noinline/comptime function declarations or comptime blocks
    // inline func:name = ..., noinline func:name = ..., comptime func:name = ...
    // comptime { ... }  (comptime block statement)
    if (peek().type == TokenType::TOKEN_KW_INLINE || 
        peek().type == TokenType::TOKEN_KW_NOINLINE ||
        peek().type == TokenType::TOKEN_KW_COMPTIME) {
        
        size_t saved = current;
        TokenType modifier = peek().type;
        advance(); // consume inline/noinline/comptime
        
        // Check for func keyword after modifier → function with modifier
        if (peek().type == TokenType::TOKEN_KW_FUNC) {
            match(TokenType::TOKEN_KW_FUNC); // consume 'func'
            auto funcDecl = parseFuncDecl();
            if (funcDecl && funcDecl->type == ASTNode::NodeType::FUNC_DECL) {
                auto func = std::static_pointer_cast<FuncDeclStmt>(funcDecl);
                if (modifier == TokenType::TOKEN_KW_INLINE) func->isInline = true;
                else if (modifier == TokenType::TOKEN_KW_NOINLINE) func->isNoInline = true;
                else if (modifier == TokenType::TOKEN_KW_COMPTIME) func->isComptime = true;
            }
            return funcDecl;
        }
        
        // comptime { ... } → comptime block statement
        if (modifier == TokenType::TOKEN_KW_COMPTIME && check(TokenType::TOKEN_LEFT_BRACE)) {
            int line = tokens[saved].line;
            int col = tokens[saved].column;
            advance(); // consume '{'
            ASTNodePtr body = parseBlock();
            consume(TokenType::TOKEN_SEMICOLON, "Expected ';' after comptime block");
            return std::make_shared<ComptimeBlockStmt>(body, line, col);
        }
        
        // Not a modifier+func or comptime block, restore position
        current = saved;
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
            auto funcDecl = parseFuncDecl();
            // v0.8.3: Attach any pending attributes
            if (funcDecl && !pendingAttrs.empty()) {
                auto* fd = static_cast<FuncDeclStmt*>(funcDecl.get());
                fd->attributes = std::move(pendingAttrs);
                // Apply well-known attribute effects
                for (const auto& attr : fd->attributes) {
                    if (attr.name == "inline") fd->isInline = true;
                    else if (attr.name == "noinline") fd->isNoInline = true;
                    else if (attr.name == "gpu_kernel") fd->isGPUKernel = true;
                    else if (attr.name == "gpu_device") fd->isGPUDevice = true;
                }
            }
            return funcDecl;
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
    
    if (match(TokenType::TOKEN_KW_PROVE)) {
        return parseProveStatement();
    }
    
    if (match(TokenType::TOKEN_KW_ASSERT_STATIC)) {
        return parseAssertStaticStatement();
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
    
    // exit code; — terminate program
    if (match(TokenType::TOKEN_KW_EXIT)) {
        return parseExitStatement();
    }
    
    // apush val; — push to user stack
    if (match(TokenType::TOKEN_KW_APUSH)) {
        return parseApushStatement();
    }
    
    // astack size; — set user stack size
    if (match(TokenType::TOKEN_KW_ASTACK)) {
        return parseAstackStatement();
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
    
    // P0: Parse #[align(N)] attribute if present
    uint64_t alignment = parseAlignmentAttribute();
    
    // v0.2.41: Parse limit<RulesName> prefix if present
    std::string limitRulesName;
    if (match(TokenType::TOKEN_KW_LIMIT)) {
        consume(TokenType::TOKEN_LESS, "Expected '<' after 'limit'");
        Token rulesToken = consume(TokenType::TOKEN_IDENTIFIER, "Expected rules name inside limit<>");
        limitRulesName = rulesToken.lexeme;
        if (knownRulesNames.find(limitRulesName) == knownRulesNames.end()) {
            error("Unknown rules name '" + limitRulesName + "' in limit<>. Did you declare Rules:" + limitRulesName + "?");
            return nullptr;
        }
        consume(TokenType::TOKEN_GREATER, "Expected '>' after rules name in limit<>");
    }
    
    bool isWild = false;
    bool isWildx = false;
    bool isConst = false;
    bool isFixed = false;
    bool isStack = false;
    bool isGC = false;
    bool isBorrowImm = false;
    bool isBorrowMut = false;
    
    // Handle qualifiers
    while (peek().type == TokenType::TOKEN_KW_WILD ||
           peek().type == TokenType::TOKEN_KW_WILDX ||
           peek().type == TokenType::TOKEN_KW_CONST ||
           peek().type == TokenType::TOKEN_KW_FIXED ||
           peek().type == TokenType::TOKEN_KW_STACK ||
           peek().type == TokenType::TOKEN_KW_GC ||
           peek().type == TokenType::TOKEN_KW_BORROW_IMM ||
           peek().type == TokenType::TOKEN_KW_BORROW_MUT) {
        if (match(TokenType::TOKEN_KW_WILD)) {
            isWild = true;
        } else if (match(TokenType::TOKEN_KW_WILDX)) {
            isWildx = true;
            isWild = true;  // wildx implies wild
        } else if (match(TokenType::TOKEN_KW_CONST)) {
            isConst = true;
        } else if (match(TokenType::TOKEN_KW_FIXED)) {
            isFixed = true;
        } else if (match(TokenType::TOKEN_KW_STACK)) {
            isStack = true;
        } else if (match(TokenType::TOKEN_KW_GC)) {
            isGC = true;
        } else if (match(TokenType::TOKEN_KW_BORROW_IMM)) {
            isBorrowImm = true;
        } else if (match(TokenType::TOKEN_KW_BORROW_MUT)) {
            isBorrowMut = true;
        }
    }
    
    // Parse the type (handles simple types, arrays, pointers, generics, etc.)
    ASTNodePtr typeNode = parseType();
    if (!typeNode) {
        error("Expected type in variable declaration");
        return nullptr;
    }
    
    // Check for invalid variable types (void can only be used in return types)
    std::string typeName = typeNode->toString();
    if (typeName == "void") {
        error("Cannot declare variables of type 'void'. The 'void' type is only valid for function return types (or in extern blocks).");
        return nullptr;
    }
    if (typeName == "impl") {
        error("Syntax error: 'impl' is a keyword for implementation blocks, not a type. Did you mean to use a struct or trait type?");
        return nullptr;
    }
    if (typeName == "trait" || typeName == "func") {
        error("Syntax error: '" + typeName + "' is a keyword, not a type. Use struct types or trait objects instead.");
        return nullptr;
    }
    
    // Consume colon
    consume(TokenType::TOKEN_COLON, "Expected ':' after type in variable declaration");
    
    // Get variable name (can be identifier or certain keywords used as names)
    // Contextual allocation-qualifier keywords (stack, gc) are valid variable names
    // since in the post-':' position there is no ambiguity — qualifiers were already consumed.
    Token nameToken = peek();
    if (nameToken.type == TokenType::TOKEN_IDENTIFIER ||
        nameToken.type == TokenType::TOKEN_KW_RESULT ||
        nameToken.type == TokenType::TOKEN_KW_FUNC ||
        nameToken.type == TokenType::TOKEN_KW_OBJ ||
        nameToken.type == TokenType::TOKEN_KW_BUFFER ||
        nameToken.type == TokenType::TOKEN_KW_STACK ||  // 'stack' is contextual (allocation qualifier)
        nameToken.type == TokenType::TOKEN_KW_GC) {     // 'gc'    is contextual (allocation qualifier)
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
    varDecl->isWildx = isWildx;
    varDecl->isConst = isConst;
    varDecl->isFixed = isFixed;
    varDecl->isStack = isStack;
    varDecl->isGC = isGC;
    varDecl->isBorrowImm = isBorrowImm;
    varDecl->isBorrowMut = isBorrowMut;
    varDecl->alignment = alignment;  // P0: Set explicit alignment if specified
    varDecl->limitRulesName = limitRulesName;  // v0.2.41: Set limit rules if specified
    
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

    // Parse optional return type qualifiers (wild, wildx, const, $$i, $$m) for extern functions
    bool returnIsWild = false, returnIsWildx = false, returnIsConst = false;
    bool returnIsBorrowImm = false, returnIsBorrowMut = false;
    while (peek().type == TokenType::TOKEN_KW_WILD || peek().type == TokenType::TOKEN_KW_WILDX || peek().type == TokenType::TOKEN_KW_CONST ||
           peek().type == TokenType::TOKEN_KW_BORROW_IMM || peek().type == TokenType::TOKEN_KW_BORROW_MUT) {
        if (match(TokenType::TOKEN_KW_WILD)) returnIsWild = true;
        else if (match(TokenType::TOKEN_KW_WILDX)) { returnIsWildx = true; returnIsWild = true; }
        else if (match(TokenType::TOKEN_KW_CONST)) returnIsConst = true;
        else if (match(TokenType::TOKEN_KW_BORROW_IMM)) returnIsBorrowImm = true;
        else if (match(TokenType::TOKEN_KW_BORROW_MUT)) returnIsBorrowMut = true;
    }

    // Parse return type (supports generics, pointers, etc.)
    ASTNodePtr returnTypeNode = parseType();
    if (!returnTypeNode) {
        error("Expected return type");
        return nullptr;
    }
    
    // Parse parameters: (type:name, type:name, ...)
    consume(TokenType::TOKEN_LEFT_PAREN, "Expected '(' after return type");
    
    std::vector<ASTNodePtr> parameters;
    bool isVariadicFunc = false;
    if (!check(TokenType::TOKEN_RIGHT_PAREN)) {
        do {
            // Check for variadic/rest marker: ..?
            if (check(TokenType::TOKEN_VARIADIC)) {
                Token variadicToken = advance();  // consume ..?
                isVariadicFunc = true;
                
                // Bare ..? before ) → extern-style variadic marker (no rest param)
                if (check(TokenType::TOKEN_RIGHT_PAREN)) {
                    break;
                }
                
                // ..?Type:name → rest parameter (collects remaining args)
                ASTNodePtr restTypeNode = parseType();
                if (!restTypeNode) {
                    error("Expected type after '..?' for rest parameter");
                    return nullptr;
                }
                consume(TokenType::TOKEN_COLON, "Expected ':' after rest parameter type");
                Token restNameToken = consumeName("rest parameter");
                
                auto restParam = std::make_shared<ParameterNode>(
                    restTypeNode, restNameToken.lexeme, nullptr,
                    variadicToken.line, variadicToken.column
                );
                restParam->isRest = true;
                parameters.push_back(restParam);
                break;  // Rest param must be last
            }
            
            // Handle parameter qualifiers (wild, wildx, const, stack, gc, $$i, $$m)
            bool isWild = false;
            bool isWildx = false;
            bool isConst = false;
            bool isStack = false;
            bool isGC = false;
            bool isBorrowImm = false;
            bool isBorrowMut = false;
            
            while (peek().type == TokenType::TOKEN_KW_WILD ||
                   peek().type == TokenType::TOKEN_KW_WILDX ||
                   peek().type == TokenType::TOKEN_KW_CONST ||
                   peek().type == TokenType::TOKEN_KW_STACK ||
                   peek().type == TokenType::TOKEN_KW_GC ||
                   peek().type == TokenType::TOKEN_KW_BORROW_IMM ||
                   peek().type == TokenType::TOKEN_KW_BORROW_MUT) {
                if (match(TokenType::TOKEN_KW_WILD)) {
                    isWild = true;
                } else if (match(TokenType::TOKEN_KW_WILDX)) {
                    isWildx = true;
                    isWild = true;  // wildx implies wild
                } else if (match(TokenType::TOKEN_KW_CONST)) {
                    isConst = true;
                } else if (match(TokenType::TOKEN_KW_STACK)) {
                    isStack = true;
                } else if (match(TokenType::TOKEN_KW_GC)) {
                    isGC = true;
                } else if (match(TokenType::TOKEN_KW_BORROW_IMM)) {
                    isBorrowImm = true;
                } else if (match(TokenType::TOKEN_KW_BORROW_MUT)) {
                    isBorrowMut = true;
                }
            }
            
            // Parse parameter type using parseType() for full type support (including generics)
            ASTNodePtr paramTypeNode = parseType();
            if (!paramTypeNode) {
                error("Expected parameter type");
                return nullptr;
            }
            
            consume(TokenType::TOKEN_COLON, "Expected ':' after parameter type");
            
            Token paramNameToken = consumeName("parameter");
            
            auto param = std::make_shared<ParameterNode>(
                paramTypeNode,  // Pass ASTNodePtr instead of string
                paramNameToken.lexeme,
                nullptr, // No default values for now
                funcToken.line,
                funcToken.column
            );
            
            // Set qualifier flags
            param->isWild = isWild;
            param->isWildx = isWildx;
            param->isBorrowImm = isBorrowImm;
            param->isBorrowMut = isBorrowMut;
            
            parameters.push_back(param);
            
        } while (match(TokenType::TOKEN_COMMA));
    }
    
    consume(TokenType::TOKEN_RIGHT_PAREN, "Expected ')' after parameters");
    
    // P1-4: Parse contract clauses (requires/ensures) if present
    std::vector<ASTNodePtr> preconditions;
    std::vector<ASTNodePtr> postconditions;
    
    // Parse requires clauses (preconditions)
    if (match(TokenType::TOKEN_KW_REQUIRES)) {
        do {
            ASTNodePtr condition = parseExpression();
            if (!condition) {
                error("Expected expression after 'requires'");
                return nullptr;
            }
            preconditions.push_back(condition);
        } while (match(TokenType::TOKEN_COMMA));
    }
    
    // Parse ensures clauses (postconditions)
    if (match(TokenType::TOKEN_KW_ENSURES)) {
        do {
            ASTNodePtr condition = parseExpression();
            if (!condition) {
                error("Expected expression after 'ensures'");
                return nullptr;
            }
            postconditions.push_back(condition);
        } while (match(TokenType::TOKEN_COMMA));
    }
    
    // For extern functions (declarations without body), consume semicolon
    // For normal functions, parse body
    ASTNodePtr body = nullptr;
    bool isExternDecl = isInsideExternBlock;
    
    if (check(TokenType::TOKEN_SEMICOLON)) {
        // Extern function declaration (no body)
        consume(TokenType::TOKEN_SEMICOLON, "Expected ';' after function declaration");
        isExternDecl = true;
    } else {
        // Normal function with body
        consume(TokenType::TOKEN_LEFT_BRACE, "Expected '{' before function body");
        body = parseBlock();
        
        // Consume semicolon after closing brace
        consume(TokenType::TOKEN_SEMICOLON, "Expected ';' after function declaration");
    }
    
    auto funcDecl = std::make_shared<FuncDeclStmt>(
        nameToken.lexeme,
        returnTypeNode,  // Changed from returnTypeName to returnTypeNode
        parameters,
        body,
        funcToken.line,
        funcToken.column
    );
    
    // Mark as extern if it's a declaration without body
    funcDecl->isExtern = isExternDecl;
    funcDecl->isVariadic = isVariadicFunc;
    
    // Set wild qualifier flag for FFI return type
    funcDecl->returnIsWild = returnIsWild;
    funcDecl->returnIsWildx = returnIsWildx;
    funcDecl->returnIsBorrowImm = returnIsBorrowImm;
    funcDecl->returnIsBorrowMut = returnIsBorrowMut;
    
    // Set generic parameters if present
    funcDecl->genericParams = genericParams;
    
    // P1-4: Set contract clauses (Design by Contract)
    funcDecl->preconditions = preconditions;
    funcDecl->postconditions = postconditions;
    
    // GPU/PTX Backend - Phase 3: Temporary gpu_ prefix detection
    // TODO(Phase 3): Replace with proper #[gpu_kernel] attribute parsing
    if (nameToken.lexeme.find("gpu_") == 0) {
        funcDecl->isGPUKernel = true;
    }
    
    return funcDecl;
}

// Parse struct declaration: struct Name { type:field1; type:field2; };
// Or generic: struct<T, E>:Name { *T:field1; *E:field2; };
ASTNodePtr Parser::parseStructDecl() {
    using namespace frontend;
    
    // v0.8.3: Parse all #[...] attributes (including #[align(N)], #[derive(...)])
    // NOTE: parseAlignmentAttribute was called before 'struct' was consumed.
    // Now attributes are parsed before the 'struct' keyword match in parseStatement().
    // The alignment is extracted from the attributes vector if #[align(N)] is present.
    uint64_t structAlignment = parseAlignmentAttribute();
    
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
        // P0: Parse #[align(N)] attribute for struct field if present
        uint64_t fieldAlignment = parseAlignmentAttribute();
        
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
        
        fieldDecl->alignment = fieldAlignment;  // P0: Set field alignment if specified
        
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
    structDecl->alignment = structAlignment;  // P0: Set struct alignment if specified
    
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
    int64_t nextAutoValue = 0;  // v0.2.39: Auto-numbering from 0
    while (!check(TokenType::TOKEN_RIGHT_BRACE) && !isAtEnd()) {
        // Get variant name (should be an identifier)
        Token variantToken = consume(TokenType::TOKEN_IDENTIFIER, "Expected variant name");
        std::string variantName = variantToken.lexeme;
        
        // Check for duplicate variant names
        if (variants.find(variantName) != variants.end()) {
            error("Duplicate variant name '" + variantName + "' in enum");
            return nullptr;
        }
        
        // v0.2.39: Value assignment is now optional (auto-numbering)
        int64_t value = nextAutoValue;
        if (match(TokenType::TOKEN_EQUAL)) {
            // Explicit value provided
            Token valueToken = consume(TokenType::TOKEN_INTEGER, "Expected integer value for enum variant");
            try {
                value = std::stoll(valueToken.lexeme);
            } catch (const std::invalid_argument& e) {
                error("Invalid integer value for enum variant: '" + valueToken.lexeme + "'");
                return nullptr;
            } catch (const std::out_of_range& e) {
                error("Enum variant value out of range: '" + valueToken.lexeme + "'");
                return nullptr;
            }
        }
        
        // Store the variant
        variants[variantName] = value;
        nextAutoValue = value + 1;  // Next auto value is always last+1
        
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

// v0.2.42: Parse Rules declaration: Rules<T1,T2>:Name = { ... }; or Rules:Name = { ... };
ASTNodePtr Parser::parseRulesDecl() {
    using namespace frontend;
    
    Token rulesToken = previous(); // 'Rules' keyword
    
    // v0.2.42: Parse optional type parameters: Rules<T1,T2,...>:Name
    std::vector<std::string> typeParams;
    if (match(TokenType::TOKEN_LESS)) {
        // Parse comma-separated type names until >
        // Type names can be keywords (int32, uint8, flt64...) or identifiers (struct/Type names)
        // v0.2.44: Also accepts array suffix [] and pointer suffix ->
        do {
            std::string typeName;
            if (isTypeKeyword(peek().type)) {
                typeName = peek().lexeme;
                advance();
            } else if (check(TokenType::TOKEN_IDENTIFIER)) {
                typeName = peek().lexeme;
                advance();
            } else {
                error("Expected type name inside Rules<>");
                return nullptr;
            }
            // v0.2.44: Check for array suffix []
            if (check(TokenType::TOKEN_LEFT_BRACKET)) {
                advance(); // consume [
                consume(TokenType::TOKEN_RIGHT_BRACKET, "Expected ']' after '[' in Rules<T[]>");
                typeName += "[]";
            }
            // v0.2.44: Check for pointer suffix ->
            else if (check(TokenType::TOKEN_ARROW)) {
                advance(); // consume ->
                typeName += "@"; // normalize to match PointerType::toString()
            }
            typeParams.push_back(typeName);
        } while (match(TokenType::TOKEN_COMMA));
        consume(TokenType::TOKEN_GREATER, "Expected '>' after type parameters in Rules<>");
    }
    
    // Consume colon before rules name
    consume(TokenType::TOKEN_COLON, "Expected ':' after 'Rules' or 'Rules<T>'");
    
    // Get rules name
    Token nameToken = consume(TokenType::TOKEN_IDENTIFIER, "Expected rules name");
    
    // Consume equals sign
    consume(TokenType::TOKEN_EQUAL, "Expected '=' after rules name");
    
    // Parse rules body: { $ condition1, $ condition2, limit<other>, ... }
    consume(TokenType::TOKEN_LEFT_BRACE, "Expected '{' after '='");
    
    std::vector<ASTNodePtr> conditions;
    std::vector<std::string> cascadedRules;
    
    while (!check(TokenType::TOKEN_RIGHT_BRACE) && !isAtEnd()) {
        if (match(TokenType::TOKEN_KW_LIMIT)) {
            // Cascading rule: limit<other_rules_name>
            consume(TokenType::TOKEN_LESS, "Expected '<' after 'limit' in cascading rule");
            Token cascadedToken = consume(TokenType::TOKEN_IDENTIFIER, "Expected rules name in limit<>");
            cascadedRules.push_back(cascadedToken.lexeme);
            consume(TokenType::TOKEN_GREATER, "Expected '>' after rules name in limit<>");
        } else if (match(TokenType::TOKEN_DOLLAR)) {
            // Condition: $ <expression>
            // Parse a comparison expression with $ as the LHS placeholder
            // Create a placeholder node for $
            auto dollarNode = std::make_shared<IdentifierExpr>("$", previous().line, previous().column);
            
            // Parse the rest as a binary expression: operator + RHS
            // We support: <, >, <=, >=, ==, !=, %, and compound like $ % 2 == 0
            ASTNodePtr condition = parseDollarCondition(dollarNode);
            if (condition) {
                conditions.push_back(condition);
            }
        } else {
            error("Expected '$' condition or 'limit<>' cascade in Rules body");
            return nullptr;
        }
        
        // Check for comma (optional for last entry)
        if (!check(TokenType::TOKEN_RIGHT_BRACE)) {
            consume(TokenType::TOKEN_COMMA, "Expected ',' or '}' after rule condition");
        }
    }
    
    consume(TokenType::TOKEN_RIGHT_BRACE, "Expected '}' after Rules body");
    consume(TokenType::TOKEN_SEMICOLON, "Expected ';' after Rules declaration");
    
    auto rulesDecl = std::make_shared<RulesDeclStmt>(
        nameToken.lexeme,
        typeParams,
        conditions,
        cascadedRules,
        rulesToken.line,
        rulesToken.column
    );
    
    return rulesDecl;
}

// v0.2.41: Parse a condition expression after $ placeholder in Rules body.
// Examples: $ < 100  |  $ % 2 == 0  |  $ != 50  |  $ >= -2
// The dollarNode is the IdentifierExpr("$") representing the value placeholder.
// We run the standard Pratt precedence-climbing loop treating dollarNode as the left operand.
ASTNodePtr Parser::parseDollarCondition(ASTNodePtr dollarNode) {
    using namespace frontend;
    
    // v0.2.43: Apply postfix parsing to $ so that $.field, $.method(), etc.
    // chain through parseMemberExpression / parseCallExpression automatically.
    ASTNodePtr left = parsePostfix(dollarNode);
    
    // Climb precedence for binary operators until we hit ',' or '}'
    while (!isAtEnd() && !check(TokenType::TOKEN_COMMA) && !check(TokenType::TOKEN_RIGHT_BRACE)) {
        Token op = peek();
        int prec = getPrecedence(op.type);
        
        if (prec <= 0) break;  // Not a binary operator
        
        if (!isBinaryOperator(op.type)) break;
        
        advance(); // consume operator
        
        // Parse right-hand side (use parseUnary to handle negative literals like -2)
        ASTNodePtr right = parseUnary();
        if (!right) {
            error("Expected expression after operator in rule condition");
            return nullptr;
        }
        right = parsePostfix(right);
        
        // Continue climbing if next operator has higher precedence
        while (!isAtEnd() && !check(TokenType::TOKEN_COMMA) && !check(TokenType::TOKEN_RIGHT_BRACE)) {
            Token nextOp = peek();
            int nextPrec = getPrecedence(nextOp.type);
            if (nextPrec <= prec) break;
            if (!isBinaryOperator(nextOp.type)) break;
            
            advance(); // consume higher-precedence operator
            ASTNodePtr right2 = parseUnary();
            if (!right2) {
                error("Expected expression after operator in rule condition");
                return nullptr;
            }
            right2 = parsePostfix(right2);
            right = std::make_shared<BinaryExpr>(right, nextOp, right2, nextOp.line, nextOp.column);
        }
        
        left = std::make_shared<BinaryExpr>(left, op, right, op.line, op.column);
    }
    
    return left;
}

// Parse Type declaration: Type:Name = { func:create=...; func:destroy=...; struct:internal=...; struct:interface=...; struct:type=...; };
// Provides organized composable units with struct data + UFCS methods
ASTNodePtr Parser::parseTypeDecl() {
    using namespace frontend;
    
    Token typeToken = previous(); // 'Type' keyword
    
    // Consume colon before type name (consistent with struct:name syntax)
    consume(TokenType::TOKEN_COLON, "Expected ':' after 'Type' keyword");
    
    // Get Type name
    Token nameToken = consume(TokenType::TOKEN_IDENTIFIER, "Expected Type name");
    
    // Consume equals sign before Type body
    consume(TokenType::TOKEN_EQUAL, "Expected '=' after Type name");
    
    // Parse Type body: { func:create=...; func:destroy=...; struct:internal=...; struct:interface=...; struct:type=...; other_funcs };
    consume(TokenType::TOKEN_LEFT_BRACE, "Expected '{' after '='");
    
    // Create TypeDeclStmt to hold components
    auto typeDecl = std::make_shared<TypeDeclStmt>(
        nameToken.lexeme,
        typeToken.line,
        typeToken.column
    );
    
    // Parse declarations inside Type body
    while (!check(TokenType::TOKEN_RIGHT_BRACE) && !isAtEnd()) {
        // Each declaration is either func:name or struct:name
        if (match(TokenType::TOKEN_KW_FUNC)) {
            // Parse function declaration
            ASTNodePtr funcDecl = parseFuncDecl();
            if (!funcDecl) {
                error("Failed to parse function in Type body");
                synchronize();
                continue;
            }
            
            // Categorize function by name
            auto funcDeclStmt = std::static_pointer_cast<FuncDeclStmt>(funcDecl);
            std::string funcName = funcDeclStmt->funcName;
            
            if (funcName == "create") {
                if (typeDecl->createFunc) {
                    error("Duplicate func:create in Type declaration");
                } else {
                    typeDecl->createFunc = funcDecl;
                }
            } else if (funcName == "destroy") {
                if (typeDecl->destroyFunc) {
                    error("Duplicate func:destroy in Type declaration");
                } else {
                    typeDecl->destroyFunc = funcDecl;
                }
            } else {
                // Regular method
                typeDecl->methods.push_back(funcDecl);
            }
            
        } else if (match(TokenType::TOKEN_KW_STRUCT)) {
            // Parse struct declaration
            ASTNodePtr structDecl = parseStructDecl();
            if (!structDecl) {
                error("Failed to parse struct in Type body");
                synchronize();
                continue;
            }
            
            // Categorize struct by name
            auto structDeclStmt = std::static_pointer_cast<StructDeclStmt>(structDecl);
            std::string structName = structDeclStmt->structName;
            
            if (structName == "internal") {
                if (typeDecl->internalStruct) {
                    error("Duplicate struct:internal in Type declaration");
                } else {
                    typeDecl->internalStruct = structDecl;
                }
            } else if (structName == "interface") {
                if (typeDecl->interfaceStruct) {
                    error("Duplicate struct:interface in Type declaration");
                } else {
                    typeDecl->interfaceStruct = structDecl;
                }
            } else if (structName == "type") {
                if (typeDecl->typeStruct) {
                    error("Duplicate struct:type in Type declaration");
                } else {
                    typeDecl->typeStruct = structDecl;
                }
            } else {
                error("Unknown struct name '" + structName + "' in Type declaration. Expected 'internal', 'interface', or 'type'");
            }
            
        } else {
            error("Expected 'func' or 'struct' in Type body");
            synchronize();
            break;
        }
    }
    
    consume(TokenType::TOKEN_RIGHT_BRACE, "Expected '}' after Type body");
    
    // Consume semicolon after Type declaration
    consume(TokenType::TOKEN_SEMICOLON, "Expected ';' after Type declaration");
    
    return typeDecl;
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

    // Check if they mistakenly used function syntax: trait:name = returnType(params) { ... }
    // This is a common error when the fuzzer or user confuses 'trait' with 'func'
    if (!check(TokenType::TOKEN_LEFT_BRACE)) {
        error("Syntax error: 'trait' keyword cannot be used like 'func'. Did you mean 'func:" + nameToken.lexeme + "'? "
              "Traits define interfaces with method signatures only, not implementations. "
              "Use 'func:name = returnType(params) { body };' for function definitions.");
        synchronize();
        return nullptr;
    }

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
        bool usedFuncPrefix = false;
        if (match(TokenType::TOKEN_KW_FUNC)) {
            consume(TokenType::TOKEN_COLON, "Expected ':' after 'func'");
            Token methodToken = consume(TokenType::TOKEN_IDENTIFIER, "Expected method name");
            methodName = methodToken.lexeme;
            usedFuncPrefix = true;
        } else {
            Token methodToken = consume(TokenType::TOKEN_IDENTIFIER, "Expected method name in trait");
            methodName = methodToken.lexeme;
        }

        // Expect separator before signature: ':' or '=' (func:name = RetType(...) or name:RetType(...))
        if (usedFuncPrefix && check(TokenType::TOKEN_EQUAL)) {
            advance(); // consume '=' — RFC syntax: func:method = RetType(params)
        } else {
            consume(TokenType::TOKEN_COLON, "Expected ':' after method name");
        }

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
            size_t paramPosBefore = current;

            // Skip commas
            if (match(TokenType::TOKEN_COMMA)) {
                continue;
            }

            // v0.6.2: Parse $$i/$$m borrow qualifiers on trait method params
            bool paramIsBorrowImm = false;
            bool paramIsBorrowMut = false;
            if (check(TokenType::TOKEN_KW_BORROW_IMM)) {
                paramIsBorrowImm = true;
                advance(); // consume $$i
            } else if (check(TokenType::TOKEN_KW_BORROW_MUT)) {
                paramIsBorrowMut = true;
                advance(); // consume $$m
            }

            // Parse parameter type
            ASTNodePtr paramTypeNode = parseType();

            // Expect colon
            consume(TokenType::TOKEN_COLON, "Expected ':' after parameter type");

            // Parse parameter name
            Token paramName = consumeName("parameter");

            ParameterNode pnode(paramTypeNode, paramName.lexeme, nullptr, paramName.line, paramName.column);
            pnode.isBorrowImm = paramIsBorrowImm;
            pnode.isBorrowMut = paramIsBorrowMut;
            params.push_back(std::move(pnode));

            // Safety: if no progress was made, break to avoid infinite loop
            if (current == paramPosBefore) {
                error("Unexpected token in trait method parameter list");
                advance(); // Force progress
                break;
            }
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

    // Detect if 'impl' is being used incorrectly as a type (common fuzzer error)
    // Example: impl int8@:ptr = alloc(1024);
    // This happens when they confuse impl with a type name
    if (!check(TokenType::TOKEN_COLON)) {
        error("Syntax error: 'impl' is a keyword for implementation blocks, not a type. "
              "Use 'impl:TraitName:for:TypeName = { methods };' to implement a trait for a type.");
        synchronize();
        return nullptr;
    }
    
    // Expect colon
    consume(TokenType::TOKEN_COLON, "Expected ':' after 'impl'");

    // Check if next token is a type keyword (would indicate impl being used as type)
    if (isTypeKeyword(peek().type) || check(TokenType::TOKEN_IDENTIFIER)) {
        // Look ahead for :for: pattern
        size_t saved = current;
        advance(); // skip trait name
        
        // If next is not :for:, this is a syntax error
        if (!check(TokenType::TOKEN_COLON)) {
            current = saved;
            error("Syntax error: Invalid impl declaration. Expected 'impl:TraitName:for:TypeName = { ... }'");
            synchronize();
            return nullptr;
        }
        advance(); // consume ':'
        
        if (!check(TokenType::TOKEN_KW_FOR)) {
            current = saved;
            error("Syntax error: Invalid impl syntax. Did you try to use 'impl' as a variable type? "
                  "'impl' is a keyword for trait implementations, not a type.");
            synchronize();
            return nullptr;
        }
        
        // Valid impl:Trait:for: pattern, restore position
        current = saved;
    }

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
            error("Expected method implementation (func:name = ...) in impl block");
            synchronize();
            break;
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

// v0.8.3: Parse AST-level macro declaration
// Syntax: macro:name = (param1, param2) { body };
ASTNodePtr Parser::parseMacroDecl() {
    using namespace frontend;
    
    Token macroToken = previous();  // 'macro' keyword
    
    // Consume colon before macro name (consistent with func:name syntax)
    consume(TokenType::TOKEN_COLON, "Expected ':' after 'macro' keyword");
    
    // Get macro name
    Token nameToken = consume(TokenType::TOKEN_IDENTIFIER, "Expected macro name");
    
    // Consume equals sign
    consume(TokenType::TOKEN_EQUAL, "Expected '=' after macro name");
    
    // Parse parameter list: (param1, param2, ...)
    consume(TokenType::TOKEN_LEFT_PAREN, "Expected '(' for macro parameters");
    
    std::vector<std::string> paramNames;
    while (!check(TokenType::TOKEN_RIGHT_PAREN) && !isAtEnd()) {
        Token paramToken = consume(TokenType::TOKEN_IDENTIFIER, "Expected parameter name");
        paramNames.push_back(paramToken.lexeme);
        
        if (check(TokenType::TOKEN_COMMA)) {
            advance();  // consume comma
        }
    }
    
    consume(TokenType::TOKEN_RIGHT_PAREN, "Expected ')' after macro parameters");
    
    // Parse macro body: { ... }
    consume(TokenType::TOKEN_LEFT_BRACE, "Expected '{' for macro body");
    ASTNodePtr body = parseBlock();
    
    // Consume semicolon after macro declaration
    consume(TokenType::TOKEN_SEMICOLON, "Expected ';' after macro declaration");
    
    return std::make_shared<MacroDeclStmt>(
        nameToken.lexeme,
        paramNames,
        body,
        false,  // statement macro by default
        macroToken.line,
        macroToken.column
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

    // Handle function pointer type: (retType)(paramType1, paramType2, ...)
    // Syntax: (int32)(int32, int32) means a function taking two int32 and returning int32
    if (typeToken.type == TokenType::TOKEN_LEFT_PAREN) {
        int fpLine = typeToken.line, fpCol = typeToken.column;
        advance(); // consume '('
        ASTNodePtr retType = parseType();
        if (!retType) {
            error("Expected return type in function pointer type");
            return nullptr;
        }
        consume(TokenType::TOKEN_RIGHT_PAREN, "Expected ')' after return type in function pointer type");
        consume(TokenType::TOKEN_LEFT_PAREN, "Expected '(' for parameter types in function pointer type");
        std::vector<ASTNodePtr> paramTypes;
        while (!check(TokenType::TOKEN_RIGHT_PAREN) && !isAtEnd()) {
            ASTNodePtr pType = parseType();
            if (!pType) break;
            paramTypes.push_back(pType);
            if (!check(TokenType::TOKEN_RIGHT_PAREN)) {
                consume(TokenType::TOKEN_COMMA, "Expected ',' between parameter types in function pointer type");
            }
        }
        consume(TokenType::TOKEN_RIGHT_PAREN, "Expected ')' after parameter types in function pointer type");
        return std::make_shared<FunctionType>(retType, paramTypes, fpLine, fpCol);
    }

    // ARIA-P3: ?-> / ?* — type-erased pointer
    // "?" in type position followed by "->" or "*" creates an erased pointer.
    // - ?->  : fat erased pointer  (Aria code)     — honest replacement for void*
    // - ?*   : thin erased pointer (extern blocks) — C-ABI compatible opaque ptr
    // Internal serialisation: "?@" (fat) / "?*" (thin), both map to LLVM opaque ptr.
    if (typeToken.type == TokenType::TOKEN_QUESTION) {
        Token nextTok = peekNext();
        if (nextTok.type == TokenType::TOKEN_ARROW || nextTok.type == TokenType::TOKEN_STAR) {
            advance(); // consume '?'
            advance(); // consume '->' or '*'
            bool native = (nextTok.type == TokenType::TOKEN_ARROW);
            if (native && isInsideExternBlock) {
                error("Use '?*' for type-erased pointers inside extern blocks, not '?->'.");
            } else if (!native && !isInsideExternBlock) {
                error("Use '?->' for type-erased pointers in Aria code, not '?*'.");
            }
            auto erasedPtr = std::make_shared<PointerType>(nullptr, typeToken.line, typeToken.column);
            erasedPtr->isNative  = native;
            erasedPtr->isErased  = true;
            prefixPointer = false; // prefix '*' + '?->' is nonsensical; ignore it
            return erasedPtr;
        }
    }

    // Check for type keyword or identifier (for generic types)
    if (isTypeKeyword(typeToken.type) || typeToken.type == TokenType::TOKEN_IDENTIFIER) {
        advance(); // Consume the type token
        
        // For generic type parameters with prefix *, store "*T" in the typename
        // This distinguishes generic references from actual pointers
        std::string typeName = typeToken.lexeme;
        if (prefixPointer && typeToken.type == TokenType::TOKEN_IDENTIFIER) {
            // *T is a generic type reference, not a pointer
            typeName = "*" + typeName;
            prefixPointer = false;  // Don't wrap in PointerType later
        }
        
        // Handle 'dyn TraitName' compound type for dynamic trait objects
        if (typeToken.type == TokenType::TOKEN_KW_DYN) {
            if (check(TokenType::TOKEN_IDENTIFIER)) {
                Token traitToken = advance();
                typeName = "dyn " + traitToken.lexeme;
            } else {
                error("Expected trait name after 'dyn'");
                return nullptr;
            }
        }
        
        // Create simple type
        baseType = std::make_shared<SimpleType>(typeName, typeToken.line, typeToken.column);
        
        // Check for generic parameters: Array<int8>, Map<K, V>, simd<int32, 4>
        if (check(TokenType::TOKEN_LESS)) {
            advance(); // consume '<'
            
            std::vector<ASTNodePtr> typeArgs;
            
            // Parse type arguments
            do {
                if (check(TokenType::TOKEN_GREATER)) break;
                
                // P1-2 Phase 2: Support integer literals in generic type args (for simd<T, N>)
                // Check if this is an integer literal (for lane count, etc.)
                Token argToken = peek();
                bool isIntegerLiteral = (argToken.type == TokenType::TOKEN_INTEGER ||
                                        argToken.type == TokenType::TOKEN_INTEGER_U8 ||
                                        argToken.type == TokenType::TOKEN_INTEGER_U16 ||
                                        argToken.type == TokenType::TOKEN_INTEGER_U32 ||
                                        argToken.type == TokenType::TOKEN_INTEGER_U64 ||
                                        argToken.type == TokenType::TOKEN_INTEGER_I8 ||
                                        argToken.type == TokenType::TOKEN_INTEGER_I16 ||
                                        argToken.type == TokenType::TOKEN_INTEGER_I32 ||
                                        argToken.type == TokenType::TOKEN_INTEGER_I64);
                
                ASTNodePtr typeArg;
                if (isIntegerLiteral) {
                    // Parse integer literal as a type argument (wrap in SimpleType)
                    // The type checker will handle extracting the integer value
                    advance(); // consume the integer token
                    typeArg = std::make_shared<SimpleType>(argToken.lexeme, argToken.line, argToken.column);
                } else {
                    // Parse regular type argument
                    typeArg = parseType(); // Recursive call for nested generics
                }
                
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
    
    // Check for array suffix(es): type[], type[size], type[M][N] (multi-dimensional).
    // Collect all dimension sizes, then build type from innermost to outermost so that
    // flt64[3][4] produces ArrayType(ArrayType(flt64,4),3) → LLVM [3 x [4 x double]]
    // (C-style semantics: first bracket = outermost/row dimension).
    {
        std::vector<ASTNodePtr> dims;
        while (match(TokenType::TOKEN_LEFT_BRACKET)) {
            ASTNodePtr sizeExpr = nullptr;
            if (!check(TokenType::TOKEN_RIGHT_BRACKET)) {
                sizeExpr = parseExpression();
            }
            consume(TokenType::TOKEN_RIGHT_BRACKET, "Expected ']' after array type");
            dims.push_back(std::move(sizeExpr));
        }
        // Build from innermost to outermost:
        // for flt64[3][4] dims=[3,4] → first wrap flt64 with [4], then wrap with [3]
        for (int i = (int)dims.size() - 1; i >= 0; --i) {
            baseType = std::make_shared<ArrayType>(baseType, dims[i], typeToken.line, typeToken.column);
        }
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
    
    // Check if this is a standalone extern func declaration: extern func:name = ...;
    // vs extern block: extern "lib" { ... }
    if (check(TokenType::TOKEN_KW_FUNC)) {
        // Parse standalone extern function declaration
        match(TokenType::TOKEN_KW_FUNC); // consume 'func'
        
        // Set FFI context for C-style pointer syntax
        bool wasInExternBlock = isInsideExternBlock;
        isInsideExternBlock = true;
        
        // Parse the function declaration
        ASTNodePtr funcDecl = parseFuncDecl();
        
        // Restore FFI context
        isInsideExternBlock = wasInExternBlock;
        
        if (funcDecl && funcDecl->type == ASTNode::NodeType::FUNC_DECL) {
            auto func = std::static_pointer_cast<FuncDeclStmt>(funcDecl);
            func->isExtern = true; // Mark as extern function
        }
        
        return funcDecl;
    }
    
    // Otherwise, parse extern block: extern "lib" { ... }
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
        // Check for pub visibility modifier
        bool isPublic = false;
        if (match(TokenType::TOKEN_KW_PUB)) {
            isPublic = true;
        }
        
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
                // Parse qualifiers (wild, wildx, const, etc.)
                std::vector<std::string> qualifiers;
                while (peek().type == TokenType::TOKEN_KW_WILD ||
                       peek().type == TokenType::TOKEN_KW_WILDX ||
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
                    else if (qual == "wildx") { fieldDecl->isWildx = true; fieldDecl->isWild = true; }
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
            bool isVariadicExtern = false;
            if (!check(TokenType::TOKEN_RIGHT_PAREN)) {
                do {
                    // Check for variadic marker: ..?
                    if (check(TokenType::TOKEN_VARIADIC)) {
                        advance();  // consume ..?
                        isVariadicExtern = true;
                        break;  // ..? must be last in param list
                    }
                    
                    // Parse parameter qualifiers (wild, wildx, const, etc.)
                    while (peek().type == TokenType::TOKEN_KW_WILD ||
                           peek().type == TokenType::TOKEN_KW_WILDX ||
                           peek().type == TokenType::TOKEN_KW_CONST ||
                           peek().type == TokenType::TOKEN_KW_STACK ||
                           peek().type == TokenType::TOKEN_KW_GC ||
                           peek().type == TokenType::TOKEN_KW_BORROW_IMM ||
                           peek().type == TokenType::TOKEN_KW_BORROW_MUT) {
                        advance(); // consume qualifier (we'll handle semantics later)
                    }
                    
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
                    
                    Token paramNameToken = consumeName("parameter");
                    
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
            
            // Set visibility and extern flags
            funcDecl->isPublic = isPublic;
            funcDecl->isExtern = true;
            funcDecl->isVariadic = isVariadicExtern;
            
            externStmt->declarations.push_back(funcDecl);
        }
        // Check for variable declaration: [qualifier] type:name;
        else if (peek().type == TokenType::TOKEN_KW_WILD ||
                 peek().type == TokenType::TOKEN_KW_WILDX ||
                 peek().type == TokenType::TOKEN_KW_CONST ||
                 peek().type == TokenType::TOKEN_KW_STACK ||
                 peek().type == TokenType::TOKEN_KW_GC ||
                 isTypeKeyword(peek().type)) {
            
            // Parse qualifiers
            std::vector<std::string> qualifiers;
            while (peek().type == TokenType::TOKEN_KW_WILD ||
                   peek().type == TokenType::TOKEN_KW_WILDX ||
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
                else if (qual == "wildx") { varDecl->isWildx = true; varDecl->isWild = true; }
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

    // pass; (no argument — void return)
    if (check(TokenType::TOKEN_SEMICOLON)) {
        advance(); // consume ';'
        // Create PassStmt with nullptr value → void pass
        return std::make_shared<PassStmt>(nullptr, passToken.line, passToken.column);
    }

    // Parse: pass expr;
    ASTNodePtr value = parseExpression();
    if (!value) {
        error("Expected expression after 'pass'");
        return nullptr;
    }

    consume(TokenType::TOKEN_SEMICOLON, "Expected ';' after pass statement");

    // Create PassStmt - type checker and IR generator handle Result building
    return std::make_shared<PassStmt>(value, passToken.line, passToken.column);
}

ASTNodePtr Parser::parseFailStatement() {
    using namespace frontend;

    Token failToken = previous(); // We already consumed 'fail'

    // Parse: fail error_code;
    ASTNodePtr errorCode = parseExpression();
    if (!errorCode) {
        error("Expected error code expression after 'fail'");
        return nullptr;
    }

    consume(TokenType::TOKEN_SEMICOLON, "Expected ';' after fail statement");

    // Create FailStmt - type checker and IR generator handle Result building
    return std::make_shared<FailStmt>(errorCode, failToken.line, failToken.column);
}

ASTNodePtr Parser::parseProveStatement() {
    using namespace frontend;

    Token proveToken = previous(); // We already consumed 'prove'

    // Parse: prove(expr); or prove expr;
    ASTNodePtr condition = parseExpression();
    if (!condition) {
        error("Expected boolean expression after 'prove'");
        return nullptr;
    }

    consume(TokenType::TOKEN_SEMICOLON, "Expected ';' after prove statement");

    return std::make_shared<ProveStmt>(std::move(condition), proveToken.line, proveToken.column);
}

ASTNodePtr Parser::parseAssertStaticStatement() {
    using namespace frontend;

    Token assertToken = previous(); // We already consumed 'assert_static'

    // Parse: assert_static(expr); or assert_static expr;
    ASTNodePtr condition = parseExpression();
    if (!condition) {
        error("Expected boolean expression after 'assert_static'");
        return nullptr;
    }

    consume(TokenType::TOKEN_SEMICOLON, "Expected ';' after assert_static statement");

    return std::make_shared<AssertStaticStmt>(std::move(condition), assertToken.line, assertToken.column);
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
    
    // Parse optional invariant clauses (v0.5.2)
    std::vector<ASTNodePtr> invariants;
    if (match(TokenType::TOKEN_KW_INVARIANT)) {
        do {
            ASTNodePtr inv = parseExpression();
            if (!inv) {
                error("Expected expression after 'invariant'");
                return nullptr;
            }
            invariants.push_back(inv);
        } while (match(TokenType::TOKEN_COMMA));
    }
    
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
    
    auto stmt = std::make_shared<WhileStmt>(condition, body, whileToken.line, whileToken.column);
    stmt->invariants = std::move(invariants);
    return stmt;
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
        
        // Parse optional invariant clauses (v0.5.2)
        std::vector<ASTNodePtr> invariants;
        if (match(TokenType::TOKEN_KW_INVARIANT)) {
            do {
                ASTNodePtr inv = parseExpression();
                if (!inv) {
                    error("Expected expression after 'invariant'");
                    return nullptr;
                }
                invariants.push_back(inv);
            } while (match(TokenType::TOKEN_COMMA));
        }
        
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
        
        auto rangeFor = ForStmt::createRangeBased(iteratorName, iteratorType, rangeExpr, body,
                                          forToken.line, forToken.column);
        rangeFor->invariants = std::move(invariants);
        return rangeFor;
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
    
    // Parse optional invariant clauses (v0.5.2)
    std::vector<ASTNodePtr> invariants;
    if (match(TokenType::TOKEN_KW_INVARIANT)) {
        do {
            ASTNodePtr inv = parseExpression();
            if (!inv) {
                error("Expected expression after 'invariant'");
                return nullptr;
            }
            invariants.push_back(inv);
        } while (match(TokenType::TOKEN_COMMA));
    }
    
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
    
    auto forStmt = std::make_shared<ForStmt>(initializer, condition, update, body, forToken.line, forToken.column);
    forStmt->invariants = std::move(invariants);
    return forStmt;
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
    
    // Parse optional invariant clauses (v0.5.2)
    std::vector<ASTNodePtr> invariants;
    if (match(TokenType::TOKEN_KW_INVARIANT)) {
        do {
            ASTNodePtr inv = parseExpression();
            if (!inv) {
                error("Expected expression after 'invariant'");
                return nullptr;
            }
            invariants.push_back(inv);
        } while (match(TokenType::TOKEN_COMMA));
    }
    
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
    
    auto tillStmt = std::make_shared<TillStmt>(limit, step, body, tillToken.line, tillToken.column);
    tillStmt->invariants = std::move(invariants);
    return tillStmt;
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
    
    // Parse optional invariant clauses (v0.5.2)
    std::vector<ASTNodePtr> invariants;
    if (match(TokenType::TOKEN_KW_INVARIANT)) {
        do {
            ASTNodePtr inv = parseExpression();
            if (!inv) {
                error("Expected expression after 'invariant'");
                return nullptr;
            }
            invariants.push_back(inv);
        } while (match(TokenType::TOKEN_COMMA));
    }
    
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
    
    auto loopStmt = std::make_shared<LoopStmt>(start, limit, step, body, loopToken.line, loopToken.column);
    loopStmt->invariants = std::move(invariants);
    return loopStmt;
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
    
    // Parse optional invariant clauses (v0.5.2)
    std::vector<ASTNodePtr> invariants;
    if (match(TokenType::TOKEN_KW_INVARIANT)) {
        do {
            ASTNodePtr inv = parseExpression();
            if (!inv) {
                error("Expected expression after 'invariant'");
                return nullptr;
            }
            invariants.push_back(inv);
        } while (match(TokenType::TOKEN_COMMA));
    }
    
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
    
    auto whenStmt = std::make_shared<WhenStmt>(condition, body, then_block, end_block, 
                                      whenToken.line, whenToken.column);
    whenStmt->invariants = std::move(invariants);
    return whenStmt;
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
                // Use std::string to force string constructor (not bool from const char*)
                pattern = std::make_shared<LiteralExpr>(std::string("*"), 
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
    
    // Parse: fall label;
    if (!check(TokenType::TOKEN_IDENTIFIER)) {
        error("Expected label identifier after 'fall'");
        return nullptr;
    }
    
    Token labelToken = advance();
    std::string label = labelToken.lexeme;
    
    // Expect ';' to end statement
    if (!match(TokenType::TOKEN_SEMICOLON)) {
        error("Expected ';' after fall statement");
        return nullptr;
    }
    
    return std::make_shared<FallStmt>(label, fallToken.line, fallToken.column);
}

// Parse exit statement: exit code;
// Produces same AST as old exit(code) — CallExpr(IdentifierExpr("exit"), [code])
ASTNodePtr Parser::parseExitStatement() {
    using namespace frontend;
    Token exitToken = previous(); // We already consumed 'exit'

    ASTNodePtr code = parseExpression();
    if (!code) {
        error("Expected exit code expression after 'exit'");
        return nullptr;
    }

    consume(TokenType::TOKEN_SEMICOLON, "Expected ';' after exit statement");

    auto callee = std::make_shared<IdentifierExpr>("exit", exitToken.line, exitToken.column);
    std::vector<ASTNodePtr> args;
    args.push_back(code);
    return std::make_shared<ExpressionStmt>(
        std::make_shared<CallExpr>(callee, args, exitToken.line, exitToken.column),
        exitToken.line, exitToken.column);
}

// Parse apush statement: apush val; OR apush(val);
// Produces same AST as old apush(val) — CallExpr(IdentifierExpr("apush"), [val])
ASTNodePtr Parser::parseApushStatement() {
    using namespace frontend;
    Token apushToken = previous(); // We already consumed 'apush'

    bool hasParen = check(TokenType::TOKEN_LEFT_PAREN);
    if (hasParen) advance(); // consume '('

    ASTNodePtr value = parseExpression();
    if (!value) {
        error("Expected value expression after 'apush'");
        return nullptr;
    }

    if (hasParen) {
        consume(TokenType::TOKEN_RIGHT_PAREN, "Expected ')' after apush argument");
    }

    consume(TokenType::TOKEN_SEMICOLON, "Expected ';' after apush statement");

    auto callee = std::make_shared<IdentifierExpr>("apush", apushToken.line, apushToken.column);
    std::vector<ASTNodePtr> args;
    args.push_back(value);
    return std::make_shared<ExpressionStmt>(
        std::make_shared<CallExpr>(callee, args, apushToken.line, apushToken.column),
        apushToken.line, apushToken.column);
}

// Parse astack statement: astack size; OR astack(size);
// Produces same AST as old astack(size) — CallExpr(IdentifierExpr("astack"), [size])
ASTNodePtr Parser::parseAstackStatement() {
    using namespace frontend;
    Token astackToken = previous(); // We already consumed 'astack'

    bool hasParen = check(TokenType::TOKEN_LEFT_PAREN);
    if (hasParen) advance(); // consume '('

    ASTNodePtr size = parseExpression();
    if (!size) {
        error("Expected size expression after 'astack'");
        return nullptr;
    }

    if (hasParen) {
        consume(TokenType::TOKEN_RIGHT_PAREN, "Expected ')' after astack argument");
    }

    consume(TokenType::TOKEN_SEMICOLON, "Expected ';' after astack statement");

    auto callee = std::make_shared<IdentifierExpr>("astack", astackToken.line, astackToken.column);
    std::vector<ASTNodePtr> args;
    args.push_back(size);
    return std::make_shared<ExpressionStmt>(
        std::make_shared<CallExpr>(callee, args, astackToken.line, astackToken.column),
        astackToken.line, astackToken.column);
}

void Parser::collectEnumNames() {
    using namespace frontend;
    for (size_t i = 0; i + 2 < tokens.size(); ++i) {
        if (tokens[i].type == TokenType::TOKEN_KW_ENUM &&
            tokens[i + 1].type == TokenType::TOKEN_COLON &&
            tokens[i + 2].type == TokenType::TOKEN_IDENTIFIER) {
            knownEnumNames.insert(tokens[i + 2].lexeme);
        }
    }
}

void Parser::collectRulesNames() {
    using namespace frontend;
    for (size_t i = 0; i + 2 < tokens.size(); ++i) {
        if (tokens[i].type == TokenType::TOKEN_KW_RULES) {
            size_t j = i + 1;
            // v0.2.42: Skip optional <T1,T2,...> type parameters
            if (j < tokens.size() && tokens[j].type == TokenType::TOKEN_LESS) {
                j++; // skip <
                // Skip tokens until we find >
                while (j < tokens.size() && tokens[j].type != TokenType::TOKEN_GREATER) {
                    j++;
                }
                if (j < tokens.size()) j++; // skip >
            }
            // Now expect : Name
            if (j + 1 < tokens.size() &&
                tokens[j].type == TokenType::TOKEN_COLON &&
                tokens[j + 1].type == TokenType::TOKEN_IDENTIFIER) {
                knownRulesNames.insert(tokens[j + 1].lexeme);
            }
        }
    }
}

ASTNodePtr Parser::parse() {
    // Pre-pass: collect enum names so parseMemberExpression can distinguish
    // enum member access (Color.RED) from UFCS static calls (Struct.method)
    collectEnumNames();
    collectRulesNames();
    
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
/**
 * P0: Parse alignment attribute #[align(N)] before variable or struct declarations
 * Returns alignment in bytes, or 0 if no attribute present
 * 
 * Syntax: #[align(64)] or #[align(16)]
 * Examples:
 *   #[align(64)] flt64[1024]:buffer = ...;  // Cache line aligned array
 *   #[align(64)] struct:WaveData = { ... };  // Entire struct aligned
 */
uint64_t Parser::parseAlignmentAttribute() {
    using namespace frontend;
    
    // Check for #[
    if (!check(TokenType::TOKEN_HASH)) {
        return 0;  // No attribute
    }
    
    // Save position in case this isn't an align attribute
    size_t saved = current;
    
    advance();  // Consume HASH
    
    // Must be followed by '['
    if (!check(TokenType::TOKEN_LEFT_BRACKET)) {
        current = saved;  // Not an attribute, restore position
        return 0;
    }
    
    advance();  // Consume LEFT_BRACKET
    
    // Must be 'align' identifier
    if (!check(TokenType::TOKEN_IDENTIFIER) || peek().lexeme != "align") {
        error("Unknown attribute. Only #[align(N)] is currently supported");
        current = saved;
        return 0;
    }
    
    advance();  // Consume 'align'
    
    // Must have (
    consume(TokenType::TOKEN_LEFT_PAREN, "Expected '(' after 'align'");
    
    // Parse alignment value (must be integer literal)
    Token alignToken = peek();
    if (alignToken.type != TokenType::TOKEN_INTEGER) {
        error("Expected integer literal for alignment value");
        return 0;
    }
    
    advance();  // Consume number
    
    // Convert to uint64_t
    uint64_t alignment = 0;
    try {
        alignment = std::stoull(alignToken.lexeme);
    } catch (...) {
        error("Invalid alignment value: " + alignToken.lexeme);
        return 0;
    }
    
    // Validate alignment is power of 2
    if (alignment == 0 || (alignment & (alignment - 1)) != 0) {
        error("Alignment must be a non-zero power of 2 (1, 2, 4, 8, 16, 32, 64, 128, ...)");
        return 0;
    }
    
    // Validate reasonable maximum (4096 = page size)
    if (alignment > 4096) {
        error("Alignment cannot exceed 4096 bytes (page size)");
        return 0;
    }
    
    // Must have )
    consume(TokenType::TOKEN_RIGHT_PAREN, "Expected ')' after alignment value");
    
    // Must have ]
    consume(TokenType::TOKEN_RIGHT_BRACKET, "Expected ']' after attribute");
    
    return alignment;
}

std::vector<Attribute> Parser::parseAttributes() {
    using namespace frontend;
    std::vector<Attribute> attrs;
    
    // Parse zero or more #[name(args)] attributes
    while (check(TokenType::TOKEN_HASH)) {
        size_t saved = current;
        advance();  // consume #
        
        if (!check(TokenType::TOKEN_LEFT_BRACKET)) {
            current = saved;  // Not an attribute, restore
            break;
        }
        advance();  // consume [
        
        // Parse attribute name (identifier or keyword like 'derive', 'inline')
        std::string attrName;
        int attrLine = peek().line;
        int attrCol = peek().column;
        
        if (check(TokenType::TOKEN_IDENTIFIER)) {
            attrName = peek().lexeme;
            advance();
        } else if (check(TokenType::TOKEN_KW_DERIVE)) {
            attrName = "derive";
            advance();
        } else if (check(TokenType::TOKEN_KW_INLINE)) {
            attrName = "inline";
            advance();
        } else if (check(TokenType::TOKEN_KW_NOINLINE)) {
            attrName = "noinline";
            advance();
        } else {
            error("Expected attribute name after '#['");
            current = saved;
            break;
        }
        
        // Parse optional arguments: (arg1, arg2, ...)
        std::vector<std::string> args;
        if (check(TokenType::TOKEN_LEFT_PAREN)) {
            advance();  // consume (
            
            while (!check(TokenType::TOKEN_RIGHT_PAREN) && !isAtEnd()) {
                // Arguments can be identifiers, keywords, or integer literals
                if (check(TokenType::TOKEN_IDENTIFIER)) {
                    args.push_back(peek().lexeme);
                    advance();
                } else if (check(TokenType::TOKEN_INTEGER)) {
                    args.push_back(peek().lexeme);
                    advance();
                } else if (peek().type >= TokenType::TOKEN_KW_INT1 && 
                           peek().type <= TokenType::TOKEN_KW_UINT4096) {
                    // Type keywords as arguments
                    args.push_back(peek().lexeme);
                    advance();
                } else {
                    // Try consuming as an identifier-like token
                    args.push_back(peek().lexeme);
                    advance();
                }
                
                if (check(TokenType::TOKEN_COMMA)) {
                    advance();  // consume comma
                }
            }
            
            consume(TokenType::TOKEN_RIGHT_PAREN, "Expected ')' after attribute arguments");
        }
        
        consume(TokenType::TOKEN_RIGHT_BRACKET, "Expected ']' after attribute");
        
        attrs.emplace_back(attrName, args, attrLine, attrCol);
    }
    
    return attrs;
}

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
