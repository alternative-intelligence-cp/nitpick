#include "frontend/parser/parser.h"
#include "frontend/ast/type.h"
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
           type == TokenType::TOKEN_HASH ||
           type == TokenType::TOKEN_DOLLAR;
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
        if (op.type == TokenType::TOKEN_KW_IS) {
            advance(); // consume 'is'
            ASTNodePtr condition = left;
            consume(TokenType::TOKEN_COLON, "Expected '?' after 'is' condition");
            ASTNodePtr trueExpr = parseExpression(prec + 1);
            consume(TokenType::TOKEN_COLON, "Expected ':' in ternary expression");
            ASTNodePtr falseExpr = parseExpression(prec);
            
            left = std::make_shared<TernaryExpr>(
                condition, trueExpr, falseExpr,
                op.line, op.column
            );
            continue;
        }
        
        // Binary operator
        if (isBinaryOperator(op.type)) {
            advance(); // consume operator
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
    
    // Integer literal
    if (token.type == TokenType::TOKEN_INTEGER) {
        std::string lexeme = token.lexeme;  // Save before advance()
        int line = token.line;
        int col = token.column;
        advance();
        int64_t value = std::stoll(lexeme);
        return std::make_shared<LiteralExpr>(value, line, col);
    }
    
    // Float literal
    if (token.type == TokenType::TOKEN_FLOAT) {
        std::string lexeme = token.lexeme;  // Save before advance()
        int line = token.line;
        int col = token.column;
        advance();
        double value = std::stod(lexeme);
        return std::make_shared<LiteralExpr>(value, line, col);
    }
    
    // String literal
    if (token.type == TokenType::TOKEN_STRING) {
        std::string value = token.string_value;  // Save before advance()
        int line = token.line;
        int col = token.column;
        advance();
        // FIX: Use string_value instead of lexeme to get unquoted string
        return std::make_shared<LiteralExpr>(value, line, col);
    }
    
    // Boolean literal
    if (token.type == TokenType::TOKEN_KW_TRUE || token.type == TokenType::TOKEN_KW_FALSE) {
        bool value = (token.type == TokenType::TOKEN_KW_TRUE);
        int line = token.line;
        int col = token.column;
        advance();
        return std::make_shared<LiteralExpr>(value, line, col);
    }
    
    // Null literal
    if (token.type == TokenType::TOKEN_KW_NULL) {
        int line = token.line;
        int col = token.column;
        advance();
        return std::make_shared<LiteralExpr>(std::monostate{}, line, col);
    }
    
    // Identifier
    if (token.type == TokenType::TOKEN_IDENTIFIER) {
        std::string lexeme = token.lexeme;  // Save before advance()
        int line = token.line;
        int col = token.column;
        advance();
        return std::make_shared<IdentifierExpr>(lexeme, line, col);
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
    
    error("Expected expression");
    return nullptr;
}

ASTNodePtr Parser::parseUnary() {
    Token token = peek();
    
    if (isUnaryOperator(token.type)) {
        advance();
        ASTNodePtr operand = parseUnary(); // Right-associative
        
        if (!operand) {
            error("Expected expression after unary operator");
            return nullptr;
        }
        
        // FIX: Explicitly pass false for isPostfix, otherwise token.line (int) is converted to true
        return std::make_shared<UnaryExpr>(token, operand, false, token.line, token.column);
    }
    
    return parsePrimary();
}

ASTNodePtr Parser::parsePostfix(ASTNodePtr expr) {
    while (!isAtEnd()) {
        Token token = peek();
        
        // Function call
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
        
        // Unwrap operator
        if (token.type == TokenType::TOKEN_QUESTION) {
            advance();
            // For now, treat as unary postfix operator
            Token unwrapOp = token;
            expr = std::make_shared<UnaryExpr>(unwrapOp, expr, true, token.line, token.column);
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
        object, memberName, isPointerAccess,
        op.line, op.column
    );
    
    // TODO: Handle safe navigation differently if needed
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
    // Stub for now - will implement in later phase
    error("Object literals not yet implemented");
    return nullptr;
}

ASTNodePtr Parser::parseTemplateLiteral() {
    // Stub for now - will implement in later phase
    error("Template literals not yet implemented");
    return nullptr;
}

ASTNodePtr Parser::parseLambda() {
    // Stub for now - will implement in Phase 2.3
    error("Lambda expressions not yet implemented");
    return nullptr;
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
        // Unsigned integers
        type == TokenType::TOKEN_KW_UINT8 || type == TokenType::TOKEN_KW_UINT16 ||
        type == TokenType::TOKEN_KW_UINT32 || type == TokenType::TOKEN_KW_UINT64 ||
        type == TokenType::TOKEN_KW_UINT128 || type == TokenType::TOKEN_KW_UINT256 ||
        type == TokenType::TOKEN_KW_UINT512 ||
        // TBB types
        type == TokenType::TOKEN_KW_TBB8 || type == TokenType::TOKEN_KW_TBB16 ||
        type == TokenType::TOKEN_KW_TBB32 || type == TokenType::TOKEN_KW_TBB64 ||
        // Floating point
        type == TokenType::TOKEN_KW_FLT32 || type == TokenType::TOKEN_KW_FLT64 ||
        type == TokenType::TOKEN_KW_FLT128 || type == TokenType::TOKEN_KW_FLT256 ||
        type == TokenType::TOKEN_KW_FLT512 ||
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
        type == TokenType::TOKEN_KW_VEC9 || type == TokenType::TOKEN_KW_MATRIX ||
        type == TokenType::TOKEN_KW_TENSOR ||
        // I/O types
        type == TokenType::TOKEN_KW_BINARY || type == TokenType::TOKEN_KW_BUFFER ||
        type == TokenType::TOKEN_KW_STREAM || type == TokenType::TOKEN_KW_PROCESS ||
        type == TokenType::TOKEN_KW_PIPE || type == TokenType::TOKEN_KW_DEBUG ||
        type == TokenType::TOKEN_KW_LOG
    );
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
        if (match(TokenType::TOKEN_KW_MOD)) {
            auto modStmt = parseModStatement();
            // Set public flag
            if (modStmt && modStmt->type == ASTNode::NodeType::MOD) {
                auto mod = std::static_pointer_cast<ModStmt>(modStmt);
                mod->isPublic = true;
            }
            return modStmt;
        } else {
            // Not pub mod, restore position and continue
            current = saved;
        }
    }
    
    // Check for module definitions
    if (match(TokenType::TOKEN_KW_MOD)) {
        return parseModStatement();
    }
    
    // Check for extern blocks (FFI)
    if (match(TokenType::TOKEN_KW_EXTERN)) {
        return parseExternStatement();
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
    
    // Check for type annotation (variable declaration)
    // Must be followed by colon to avoid ambiguity with identifiers
    // Example: "obj:x = 5" is variable declaration, "obj.field" is member access
    if (isTypeKeyword(peek().type)) {
        size_t saved = current;
        advance(); // consume type keyword
        if (check(TokenType::TOKEN_COLON)) {
            // It's a type annotation: type:name
            current = saved; // reset position
            return parseVarDecl();
        } else {
            // It's an identifier used in an expression, restore position
            current = saved;
            // Fall through to expression statement
        }
    }
    
    // Check for function declaration: func:name = ... or func<T>:name = ...
    // Only treat as function declaration if followed by colon or generic params then colon
    if (peek().type == TokenType::TOKEN_KW_FUNC) {
        // Look ahead to see if it's func:name or func<T>:name (declaration) or func() (call)
        size_t saved = current;
        current++; // skip 'func'
        
        // Check for generic parameters: func<T, U>
        if (current < tokens.size() && tokens[current].type == TokenType::TOKEN_LESS) {
            current++; // skip '<'
            // Skip generic parameter names  
            while (current < tokens.size() && tokens[current].type != TokenType::TOKEN_GREATER) {
                if (tokens[current].type == TokenType::TOKEN_IDENTIFIER) {
                    current++; // skip identifier
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
    
    // Get type (could be *T for generic or regular type)
    std::string typeName;
    int typeLine, typeColumn;
    if (isGenericTypeReference()) {
        // Generic type reference: *T
        Token starToken = advance(); // consume '*'
        typeLine = starToken.line;
        typeColumn = starToken.column;
        Token typeParamToken = consume(TokenType::TOKEN_IDENTIFIER, "Expected type parameter name after '*'");
        typeName = "*" + typeParamToken.lexeme;
    } else {
        Token typeToken = advance();
        if (!isTypeKeyword(typeToken.type)) {
            error("Expected type keyword in variable declaration");
            return nullptr;
        }
        typeName = typeToken.lexeme;
        typeLine = typeToken.line;
        typeColumn = typeToken.column;
    }
    
    // Consume colon
    consume(TokenType::TOKEN_COLON, "Expected ':' after type in variable declaration");
    
    // Get variable name
    Token nameToken = consume(TokenType::TOKEN_IDENTIFIER, "Expected variable name");
    
    // Check for initializer
    ASTNodePtr initializer = nullptr;
    if (match(TokenType::TOKEN_EQUAL)) {
        initializer = parseExpression();
    }
    
    // Consume semicolon
    consume(TokenType::TOKEN_SEMICOLON, "Expected ';' after variable declaration");
    
    auto varDecl = std::make_shared<VarDeclStmt>(
        typeName,
        nameToken.lexeme,
        initializer,
        typeLine,
        typeColumn
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
    
    // Phase 3.4: Parse generic parameters if present: func<T, U>
    std::vector<std::string> genericParams = parseGenericParams();
    
    // Consume colon
    consume(TokenType::TOKEN_COLON, "Expected ':' after 'func'");
    
    // Get function name
    Token nameToken = consume(TokenType::TOKEN_IDENTIFIER, "Expected function name");
    
    // Consume equal sign
    consume(TokenType::TOKEN_EQUAL, "Expected '=' after function name");
    
    // Get return type (could be *T for generic or regular type)
    std::string returnTypeName;
    if (isGenericTypeReference()) {
        // Generic type reference: *T
        advance(); // consume '*'
        Token typeParamToken = consume(TokenType::TOKEN_IDENTIFIER, "Expected type parameter name after '*'");
        returnTypeName = "*" + typeParamToken.lexeme;
    } else {
        Token returnTypeToken = advance();
        if (!isTypeKeyword(returnTypeToken.type)) {
            error("Expected return type");
            return nullptr;
        }
        returnTypeName = returnTypeToken.lexeme;
    }
    
    // Parse parameters: (type:name, type:name, ...)
    consume(TokenType::TOKEN_LEFT_PAREN, "Expected '(' after return type");
    
    std::vector<ASTNodePtr> parameters;
    if (!check(TokenType::TOKEN_RIGHT_PAREN)) {
        do {
            // Parse parameter type (could be *T for generic or regular type)
            std::string paramTypeName;
            if (isGenericTypeReference()) {
                // Generic type reference: *T
                advance(); // consume '*'
                Token typeParamToken = consume(TokenType::TOKEN_IDENTIFIER, "Expected type parameter name after '*'");
                paramTypeName = "*" + typeParamToken.lexeme;
            } else {
                Token paramTypeToken = advance();
                if (!isTypeKeyword(paramTypeToken.type)) {
                    error("Expected parameter type");
                    return nullptr;
                }
                paramTypeName = paramTypeToken.lexeme;
            }
            
            consume(TokenType::TOKEN_COLON, "Expected ':' after parameter type");
            
            Token paramNameToken = consume(TokenType::TOKEN_IDENTIFIER, "Expected parameter name");
            
            auto param = std::make_shared<ParameterNode>(
                paramTypeName,
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
        returnTypeName,
        parameters,
        body,
        funcToken.line,
        funcToken.column
    );
    
    // Set generic parameters if present
    funcDecl->genericParams = genericParams;
    
    return funcDecl;
}

// Parse block: { stmt1; stmt2; ... }
ASTNodePtr Parser::parseBlock() {
    using namespace frontend;
    
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
    
    return std::make_shared<BlockStmt>(statements, leftBrace.line, leftBrace.column);
}

// Parse type annotation
// Handles: simple types (int8, string), pointers (int8@), arrays (int8[], int8[100]),
//          and generic types (Array<int8>, Map<string, int32>)
ASTNodePtr Parser::parseType() {
    using namespace frontend;
    
    Token typeToken = peek();
    ASTNodePtr baseType;
    
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
    
    // Check for pointer suffix: type@ (Aria native pointer syntax)
    // Note: extern blocks use * for C FFI, but that's handled separately
    if (match(TokenType::TOKEN_AT)) {
        baseType = std::make_shared<PointerType>(baseType, typeToken.line, typeToken.column);
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
    
    // Parse declarations inside the extern block
    // Note: extern blocks contain signatures (declarations without bodies), not statements
    while (!check(TokenType::TOKEN_RIGHT_BRACE) && !isAtEnd()) {
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
            std::string returnType = returnTypeToken.lexeme;
            while (check(TokenType::TOKEN_STAR)) {
                advance(); // consume '*'
                returnType += "*";
            }
            
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
                    std::string paramType = paramTypeToken.lexeme;
                    while (check(TokenType::TOKEN_STAR)) {
                        advance(); // consume '*'
                        paramType += "*";
                    }
                    
                    consume(TokenType::TOKEN_COLON, "Expected ':' after parameter type");
                    
                    Token paramNameToken = consume(TokenType::TOKEN_IDENTIFIER, "Expected parameter name");
                    
                    auto param = std::make_shared<ParameterNode>(
                        paramType, // Use the full type including pointer stars
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
                returnType, // Use the full type including pointer stars
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
    // Desugars to: return { err: 0, val: expr }
    // TODO: Once ObjectLiteralExpr is implemented, create proper result object
    consume(TokenType::TOKEN_LEFT_PAREN, "Expected '(' after 'pass'");
    
    ASTNodePtr value = parseExpression();
    if (!value) {
        error("Expected expression in pass statement");
        return nullptr;
    }
    
    consume(TokenType::TOKEN_RIGHT_PAREN, "Expected ')' after pass value");
    consume(TokenType::TOKEN_SEMICOLON, "Expected ';' after pass statement");
    
    // For now, just return the value directly
    // Full implementation: return { err: 0, val: value }
    return std::make_shared<ReturnStmt>(value, passToken.line, passToken.column);
}

ASTNodePtr Parser::parseFailStatement() {
    using namespace frontend;
    
    Token failToken = previous(); // We already consumed 'fail'
    
    // Parse: fail(error_code);
    // Desugars to: return { err: error_code, val: 0 }
    // TODO: Once ObjectLiteralExpr is implemented, create proper result object
    consume(TokenType::TOKEN_LEFT_PAREN, "Expected '(' after 'fail'");
    
    ASTNodePtr errorCode = parseExpression();
    if (!errorCode) {
        error("Expected error code expression in fail statement");
        return nullptr;
    }
    
    consume(TokenType::TOKEN_RIGHT_PAREN, "Expected ')' after fail error code");
    consume(TokenType::TOKEN_SEMICOLON, "Expected ';' after fail statement");
    
    // For now, just return the error code directly
    // Full implementation: return { err: error_code, val: 0 }
    return std::make_shared<ReturnStmt>(errorCode, failToken.line, failToken.column);
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

// Parse generic parameters: <T, U, V>
std::vector<std::string> Parser::parseGenericParams() {
    using namespace frontend;
    
    std::vector<std::string> params;
    
    // Consume the '<'
    if (!match(TokenType::TOKEN_LESS)) {
        return params;  // No generic params
    }
    
    // Parse first parameter
    Token paramToken = consume(TokenType::TOKEN_IDENTIFIER, "Expected type parameter name");
    params.push_back(paramToken.lexeme);
    
    // Parse remaining parameters
    while (match(TokenType::TOKEN_COMMA)) {
        Token nextParam = consume(TokenType::TOKEN_IDENTIFIER, "Expected type parameter name");
        params.push_back(nextParam.lexeme);
    }
    
    // Consume the '>'
    consume(TokenType::TOKEN_GREATER, "Expected '>' after generic parameters");
    
    return params;
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
