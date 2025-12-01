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

    // Boolean literals
    if (current.type == TOKEN_KW_TRUE) {
        advance();
        return std::make_unique<BoolLiteral>(true);
    }
    if (current.type == TOKEN_KW_FALSE) {
        advance();
        return std::make_unique<BoolLiteral>(false);
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
    
    // When expression (Bug #69)
    if (current.type == TOKEN_KW_WHEN) {
        return parseWhenExpr();
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
    
    // For loop (Bug #67)
    if (current.type == TOKEN_KW_FOR) {
        return parseForLoop();
    }
    
    // While loop (Bug #68)
    if (current.type == TOKEN_KW_WHILE) {
        return parseWhileLoop();
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

// Parse when expression: when { case1 then expr1; case2 then expr2; end }
std::unique_ptr<Expression> Parser::parseWhenExpr() {
    expect(TOKEN_KW_WHEN);
    expect(TOKEN_LBRACE);
    
    auto when_expr = std::make_unique<WhenExpr>();
    
    // Parse cases until 'end' keyword
    while (current.type != TOKEN_KW_END && current.type != TOKEN_EOF) {
        WhenCase case_item;
        
        // Parse condition
        case_item.condition = parseExpr();
        
        // Expect 'then' keyword
        expect(TOKEN_KW_THEN);
        
        // Parse result expression
        case_item.result = parseExpr();
        
        // Consume semicolon if present
        match(TOKEN_SEMICOLON);
        
        when_expr->cases.push_back(std::move(case_item));
    }
    
    expect(TOKEN_KW_END);
    expect(TOKEN_RBRACE);
    
    return when_expr;
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

} // namespace frontend
} // namespace aria
