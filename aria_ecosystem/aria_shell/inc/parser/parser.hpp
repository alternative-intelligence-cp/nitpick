/**
 * Recursive Descent Parser for Process Orchestration Language (POL)
 * 
 * Implements whitespace-insensitive parsing with dual-mode disambiguation:
 * - Command Mode: Bare words as commands (ls, grep, etc.)
 * - Expression Mode: Strict syntax (variables, operators, calls)
 * 
 * Disambiguation Strategy:
 * 1. If keyword (if/while/for) → Control flow
 * 2. If type (int8/string/tbb8) → Variable declaration
 * 3. If IDENTIFIER = → Assignment
 * 4. Else → Command invocation
 */

#ifndef ARIASH_PARSER_HPP
#define ARIASH_PARSER_HPP

#include "parser/token.hpp"
#include "parser/ast.hpp"
#include <vector>
#include <memory>
#include <optional>
#include <stdexcept>

namespace ariash {
namespace parser {

class ParseError : public std::runtime_error {
public:
    ParseError(const std::string& msg, const SourceLocation& loc)
        : std::runtime_error(formatError(msg, loc)), location(loc) {}
    
    SourceLocation location;
    
private:
    static std::string formatError(const std::string& msg, const SourceLocation& loc) {
        return "Parse error at line " + std::to_string(loc.line) + 
               ", column " + std::to_string(loc.column) + ": " + msg;
    }
};

class ShellParser {
public:
    explicit ShellParser(const std::vector<Token>& tokens)
        : tokens_(tokens), current_(0) {}
    
    // Entry point - parses complete program
    std::unique_ptr<Program> parseProgram();
    
private:
    // Token stream management
    const Token& peek(int offset = 0) const;
    const Token& consume();
    bool match(TokenType type);
    bool check(TokenType type) const;
    void expect(TokenType type, const std::string& message);
    bool isAtEnd() const;
    
    // Top-level parsing
    std::unique_ptr<StmtNode> parseStatement();
    
    // Expression parsing (precedence climbing)
    std::unique_ptr<ExprNode> parseExpression();
    std::unique_ptr<ExprNode> parseLogicalOr();
    std::unique_ptr<ExprNode> parseLogicalAnd();
    std::unique_ptr<ExprNode> parseEquality();
    std::unique_ptr<ExprNode> parseComparison();
    std::unique_ptr<ExprNode> parseAdditive();
    std::unique_ptr<ExprNode> parseMultiplicative();
    std::unique_ptr<ExprNode> parseUnary();
    std::unique_ptr<ExprNode> parsePrimary();
    std::unique_ptr<ExprNode> parseCallOrVariable();
    
    // Statement parsing
    std::unique_ptr<BlockStmt> parseBlock();
    std::unique_ptr<VarDeclStmt> parseVarDecl();
    std::unique_ptr<AssignStmt> parseAssignment(const std::string& varName);
    std::unique_ptr<IfStmt> parseIf();
    std::unique_ptr<WhileStmt> parseWhile();
    std::unique_ptr<ForStmt> parseFor();
    std::unique_ptr<ReturnStmt> parseReturn();
    
    // Command parsing (shell mode)
    std::unique_ptr<CommandStmt> parseCommand();
    std::unique_ptr<PipelineStmt> parsePipeline();
    std::vector<Redirection> parseRedirections();
    
    // Helpers
    bool isKeywordStart() const;
    bool isTypeKeyword() const;
    bool isAssignmentAhead() const;  // IDENTIFIER followed by =
    
    // State
    const std::vector<Token>& tokens_;
    size_t current_;
};

} // namespace parser
} // namespace ariash

#endif // ARIASH_PARSER_HPP
