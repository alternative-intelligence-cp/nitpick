/**
 * AST Executor - Interprets and executes parsed shell programs
 * 
 * Implements the visitor pattern to traverse and execute AST nodes.
 * Maintains runtime state including:
 * - Variable bindings (symbol table)
 * - Expression evaluation results
 * - Process execution via HexStreamProcess
 * - Pipeline construction
 * - Control flow execution
 */

#ifndef ARIASH_EXECUTOR_HPP
#define ARIASH_EXECUTOR_HPP

#include "parser/ast.hpp"
#include "hexstream/process.hpp"
#include <unordered_map>
#include <string>
#include <memory>
#include <optional>
#include <variant>

namespace ariash {
namespace executor {

// Runtime value types
using Value = std::variant<
    int64_t,           // Integer
    double,            // Float
    std::string,       // String
    bool               // Boolean
>;

/**
 * Converts value to string representation
 */
std::string valueToString(const Value& val);

/**
 * Runtime environment - manages variable bindings
 */
class Environment {
public:
    void define(const std::string& name, const Value& value);
    void assign(const std::string& name, const Value& value);
    Value get(const std::string& name) const;
    bool exists(const std::string& name) const;
    
private:
    std::unordered_map<std::string, Value> bindings_;
};

/**
 * Executor - interprets AST and produces side effects
 */
class Executor : public parser::ASTVisitor {
public:
    explicit Executor(Environment& env) : env_(env) {}
    
    // Execute a program
    void execute(parser::Program& program);
    
    // Get last expression result
    std::optional<Value> getLastResult() const { return lastResult_; }
    
    // Expression visitors (produce values)
    void visit(parser::IntegerLiteral& node) override;
    void visit(parser::StringLiteral& node) override;
    void visit(parser::VariableExpr& node) override;
    void visit(parser::BinaryOpExpr& node) override;
    void visit(parser::UnaryOpExpr& node) override;
    void visit(parser::CallExpr& node) override;
    
    // Statement visitors (produce side effects)
    void visit(parser::BlockStmt& node) override;
    void visit(parser::VarDeclStmt& node) override;
    void visit(parser::AssignStmt& node) override;
    void visit(parser::IfStmt& node) override;
    void visit(parser::WhileStmt& node) override;
    void visit(parser::ForStmt& node) override;
    void visit(parser::ReturnStmt& node) override;
    void visit(parser::ExprStmt& node) override;
    void visit(parser::CommandStmt& node) override;
    void visit(parser::PipelineStmt& node) override;
    void visit(parser::Program& node) override;
    
private:
    Environment& env_;
    std::optional<Value> exprResult_;  // Result of last expression
    std::optional<Value> lastResult_;  // Last statement result
    bool hasReturned_ = false;         // Return flag for early exit
    
    // Helper methods
    Value evaluateExpr(parser::ExprNode& expr);
    bool isTruthy(const Value& val);
    
    // Binary operations
    Value applyArithmetic(parser::TokenType op, const Value& left, const Value& right);
    Value applyComparison(parser::TokenType op, const Value& left, const Value& right);
    Value applyLogical(parser::TokenType op, const Value& left, const Value& right);
    
    // Process execution
    void executeCommand(parser::CommandStmt& cmd);
    void executePipeline(parser::PipelineStmt& pipeline);
    
    // File descriptor setup
    void setupRedirections(const std::vector<parser::Redirection>& redirects,
                          hexstream::ProcessConfig& config);
};

} // namespace executor
} // namespace ariash

#endif // ARIASH_EXECUTOR_HPP
