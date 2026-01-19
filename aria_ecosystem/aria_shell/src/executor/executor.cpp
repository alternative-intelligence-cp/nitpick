/**
 * AST Executor Implementation
 */

#include "executor/executor.hpp"
#include <iostream>
#include <stdexcept>
#include <sstream>
#include <thread>
#include <chrono>
#include <cstdlib>
#include <unistd.h>
#include <sys/stat.h>

namespace ariash {
namespace executor {

// =============================================================================
// Path Resolution
// =============================================================================

static std::string resolveExecutablePath(const std::string& command) {
    // If command contains a slash, it's a path - use as-is
    if (command.find('/') != std::string::npos) {
        return command;
    }
    
    // Search PATH
    const char* pathEnv = std::getenv("PATH");
    if (!pathEnv) {
        return command;  // No PATH, try command as-is
    }
    
    std::string path(pathEnv);
    size_t start = 0;
    size_t end;
    
    while ((end = path.find(':', start)) != std::string::npos) {
        std::string dir = path.substr(start, end - start);
        std::string fullPath = dir + "/" + command;
        
        // Check if file exists and is executable
        struct stat st;
        if (stat(fullPath.c_str(), &st) == 0 && (st.st_mode & S_IXUSR)) {
            return fullPath;
        }
        
        start = end + 1;
    }
    
    // Check last directory
    std::string dir = path.substr(start);
    std::string fullPath = dir + "/" + command;
    struct stat st;
    if (stat(fullPath.c_str(), &st) == 0 && (st.st_mode & S_IXUSR)) {
        return fullPath;
    }
    
    // Not found in PATH, return original command
    return command;
}

// =============================================================================
// Value Utilities
// =============================================================================

std::string valueToString(const Value& val) {
    return std::visit([](auto&& arg) -> std::string {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, int64_t>) {
            return std::to_string(arg);
        } else if constexpr (std::is_same_v<T, double>) {
            return std::to_string(arg);
        } else if constexpr (std::is_same_v<T, std::string>) {
            return arg;
        } else if constexpr (std::is_same_v<T, bool>) {
            return arg ? "true" : "false";
        }
    }, val);
}

// =============================================================================
// Environment
// =============================================================================

void Environment::define(const std::string& name, const Value& value) {
    bindings_[name] = value;
}

void Environment::assign(const std::string& name, const Value& value) {
    if (!exists(name)) {
        throw std::runtime_error("Undefined variable: " + name);
    }
    bindings_[name] = value;
}

Value Environment::get(const std::string& name) const {
    auto it = bindings_.find(name);
    if (it == bindings_.end()) {
        throw std::runtime_error("Undefined variable: " + name);
    }
    return it->second;
}

bool Environment::exists(const std::string& name) const {
    return bindings_.find(name) != bindings_.end();
}

// =============================================================================
// Executor
// =============================================================================

void Executor::execute(parser::Program& program) {
    program.accept(*this);
}

Value Executor::evaluateExpr(parser::ExprNode& expr) {
    expr.accept(*this);
    if (!exprResult_) {
        throw std::runtime_error("Expression did not produce a value");
    }
    Value result = *exprResult_;
    exprResult_.reset();
    return result;
}

bool Executor::isTruthy(const Value& val) {
    return std::visit([](auto&& arg) -> bool {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, bool>) {
            return arg;
        } else if constexpr (std::is_same_v<T, int64_t>) {
            return arg != 0;
        } else if constexpr (std::is_same_v<T, double>) {
            return arg != 0.0;
        } else if constexpr (std::is_same_v<T, std::string>) {
            return !arg.empty();
        }
    }, val);
}

// =============================================================================
// Expression Visitors
// =============================================================================

void Executor::visit(parser::IntegerLiteral& node) {
    exprResult_ = node.value;
}

void Executor::visit(parser::StringLiteral& node) {
    exprResult_ = node.value;
}

void Executor::visit(parser::VariableExpr& node) {
    exprResult_ = env_.get(node.name);
}

void Executor::visit(parser::BinaryOpExpr& node) {
    Value left = evaluateExpr(*node.left);
    Value right = evaluateExpr(*node.right);
    
    using parser::TokenType;
    
    // Arithmetic
    if (node.op == TokenType::PLUS || node.op == TokenType::MINUS ||
        node.op == TokenType::STAR || node.op == TokenType::SLASH) {
        exprResult_ = applyArithmetic(node.op, left, right);
    }
    // Comparison
    else if (node.op == TokenType::LT || node.op == TokenType::LE ||
             node.op == TokenType::GT || node.op == TokenType::GE ||
             node.op == TokenType::EQ || node.op == TokenType::NE) {
        exprResult_ = applyComparison(node.op, left, right);
    }
    // Logical
    else if (node.op == TokenType::AND || node.op == TokenType::OR) {
        exprResult_ = applyLogical(node.op, left, right);
    }
    else {
        throw std::runtime_error("Unknown binary operator");
    }
}

void Executor::visit(parser::UnaryOpExpr& node) {
    Value operand = evaluateExpr(*node.operand);
    
    using parser::TokenType;
    
    if (node.op == TokenType::MINUS) {
        if (std::holds_alternative<int64_t>(operand)) {
            exprResult_ = -std::get<int64_t>(operand);
        } else if (std::holds_alternative<double>(operand)) {
            exprResult_ = -std::get<double>(operand);
        } else {
            throw std::runtime_error("Cannot negate non-numeric value");
        }
    }
    else if (node.op == TokenType::NOT) {
        exprResult_ = !isTruthy(operand);
    }
    else {
        throw std::runtime_error("Unknown unary operator");
    }
}

void Executor::visit(parser::CallExpr& node) {
    // Built-in functions
    if (node.function == "print") {
        for (auto& arg : node.arguments) {
            Value val = evaluateExpr(*arg);
            std::cout << valueToString(val);
        }
        std::cout << std::endl;
        exprResult_ = static_cast<int64_t>(0);  // Return 0
    }
    else if (node.function == "len") {
        if (node.arguments.size() != 1) {
            throw std::runtime_error("len() expects 1 argument");
        }
        Value arg = evaluateExpr(*node.arguments[0]);
        if (std::holds_alternative<std::string>(arg)) {
            exprResult_ = static_cast<int64_t>(std::get<std::string>(arg).length());
        } else {
            throw std::runtime_error("len() expects string argument");
        }
    }
    else {
        throw std::runtime_error("Unknown function: " + node.function);
    }
}

// =============================================================================
// Binary Operations
// =============================================================================

Value Executor::applyArithmetic(parser::TokenType op, const Value& left, const Value& right) {
    // Integer arithmetic
    if (std::holds_alternative<int64_t>(left) && std::holds_alternative<int64_t>(right)) {
        int64_t l = std::get<int64_t>(left);
        int64_t r = std::get<int64_t>(right);
        
        switch (op) {
            case parser::TokenType::PLUS:  return l + r;
            case parser::TokenType::MINUS: return l - r;
            case parser::TokenType::STAR:  return l * r;
            case parser::TokenType::SLASH:
                if (r == 0) throw std::runtime_error("Division by zero");
                return l / r;
            default: throw std::runtime_error("Unknown arithmetic operator");
        }
    }
    
    // String concatenation
    if (op == parser::TokenType::PLUS) {
        if (std::holds_alternative<std::string>(left) || std::holds_alternative<std::string>(right)) {
            return valueToString(left) + valueToString(right);
        }
    }
    
    throw std::runtime_error("Type mismatch in arithmetic operation");
}

Value Executor::applyComparison(parser::TokenType op, const Value& left, const Value& right) {
    // Integer comparison
    if (std::holds_alternative<int64_t>(left) && std::holds_alternative<int64_t>(right)) {
        int64_t l = std::get<int64_t>(left);
        int64_t r = std::get<int64_t>(right);
        
        switch (op) {
            case parser::TokenType::LT: return l < r;
            case parser::TokenType::LE: return l <= r;
            case parser::TokenType::GT: return l > r;
            case parser::TokenType::GE: return l >= r;
            case parser::TokenType::EQ: return l == r;
            case parser::TokenType::NE: return l != r;
            default: throw std::runtime_error("Unknown comparison operator");
        }
    }
    
    // String comparison
    if (std::holds_alternative<std::string>(left) && std::holds_alternative<std::string>(right)) {
        std::string l = std::get<std::string>(left);
        std::string r = std::get<std::string>(right);
        
        switch (op) {
            case parser::TokenType::EQ: return l == r;
            case parser::TokenType::NE: return l != r;
            case parser::TokenType::LT: return l < r;
            case parser::TokenType::LE: return l <= r;
            case parser::TokenType::GT: return l > r;
            case parser::TokenType::GE: return l >= r;
            default: throw std::runtime_error("Unknown comparison operator");
        }
    }
    
    throw std::runtime_error("Type mismatch in comparison");
}

Value Executor::applyLogical(parser::TokenType op, const Value& left, const Value& right) {
    bool l = isTruthy(left);
    bool r = isTruthy(right);
    
    switch (op) {
        case parser::TokenType::AND: return l && r;
        case parser::TokenType::OR:  return l || r;
        default: throw std::runtime_error("Unknown logical operator");
    }
}

// =============================================================================
// Statement Visitors
// =============================================================================

void Executor::visit(parser::BlockStmt& node) {
    for (auto& stmt : node.statements) {
        if (hasReturned_) break;
        stmt->accept(*this);
    }
}

void Executor::visit(parser::VarDeclStmt& node) {
    Value initialValue;
    
    if (node.initializer) {
        initialValue = evaluateExpr(*node.initializer);
    } else {
        // Default initialization based on type
        if (node.type == "int8" || node.type == "int16" || node.type == "int32" || node.type == "int64") {
            initialValue = static_cast<int64_t>(0);
        } else if (node.type == "string") {
            initialValue = std::string("");
        } else if (node.type == "bool") {
            initialValue = false;
        } else {
            initialValue = static_cast<int64_t>(0);  // Default to 0
        }
    }
    
    env_.define(node.name, initialValue);
}

void Executor::visit(parser::AssignStmt& node) {
    Value value = evaluateExpr(*node.value);
    env_.assign(node.variable, value);
}

void Executor::visit(parser::IfStmt& node) {
    Value condition = evaluateExpr(*node.condition);
    
    if (isTruthy(condition)) {
        node.thenBranch->accept(*this);
    } else if (node.elseBranch) {
        node.elseBranch->accept(*this);
    }
}

void Executor::visit(parser::WhileStmt& node) {
    while (true) {
        Value condition = evaluateExpr(*node.condition);
        if (!isTruthy(condition)) break;
        
        node.body->accept(*this);
        
        if (hasReturned_) break;
    }
}

void Executor::visit(parser::ForStmt& node) {
    // For now, implement simple range iteration
    // TODO: Implement iterable protocol
    throw std::runtime_error("For loops not yet implemented");
}

void Executor::visit(parser::ReturnStmt& node) {
    if (node.value) {
        lastResult_ = evaluateExpr(*node.value);
    }
    hasReturned_ = true;
}

void Executor::visit(parser::ExprStmt& node) {
    // Evaluate expression and store result for REPL display
    lastResult_ = evaluateExpr(*node.expression);
}

void Executor::visit(parser::CommandStmt& node) {
    executeCommand(node);
}

void Executor::visit(parser::PipelineStmt& node) {
    executePipeline(node);
}

void Executor::visit(parser::Program& node) {
    for (auto& stmt : node.statements) {
        if (hasReturned_) break;
        stmt->accept(*this);
    }
}

// =============================================================================
// Process Execution
// =============================================================================

void Executor::executeCommand(parser::CommandStmt& cmd) {
    using namespace hexstream;
    using namespace job;
    
    ProcessConfig config;
    
    // Resolve executable path from PATH
    config.executable = resolveExecutablePath(cmd.executable);
    config.arguments = cmd.arguments;
    config.foregroundMode = false;  // Capture output via callbacks
    
    // Setup redirections
    setupRedirections(cmd.redirections, config);
    
    // Create process
    HexStreamProcess process(config);
    
    // Register callback to display output in real-time
    process.onData([](StreamIndex stream, const void* data, size_t size) {
        if (stream == StreamIndex::STDOUT) {
            // Write stdout directly to terminal
            std::cout.write(static_cast<const char*>(data), size);
            std::cout.flush();
        } else if (stream == StreamIndex::STDERR) {
            // Write stderr directly to terminal
            std::cerr.write(static_cast<const char*>(data), size);
            std::cerr.flush();
        }
        // Could also handle STDDBG for debug output
    });
    
    // Spawn the process
    if (!process.spawn()) {
        std::cerr << "Failed to spawn process: " << cmd.executable << std::endl;
        lastResult_ = static_cast<int64_t>(-1);
        return;
    }
    
    if (cmd.background) {
        // Background execution - don't wait
        std::cout << "[Background] Started PID " << process.getPid() << std::endl;
        lastResult_ = static_cast<int64_t>(0);
    } else {
        // Foreground execution - wait for completion
        int exitCode = process.wait();
        
        // Give drainers a moment to finish reading
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        
        // Flush buffers and invoke callbacks with remaining data
        process.flushBuffers();
        
        // Store exit code as last result
        lastResult_ = static_cast<int64_t>(exitCode);
    }
}

void Executor::executePipeline(parser::PipelineStmt& pipeline) {
    if (pipeline.commands.empty()) {
        return;
    }
    
    // Single command - execute directly
    if (pipeline.commands.size() == 1) {
        executeCommand(*pipeline.commands[0]);
        return;
    }
    
    // Multi-command pipeline - still disabled due to FD chaining issues
    std::cerr << "[WARNING] Multi-command pipelines not yet supported\n";
    std::cerr << "[DEBUG] Pipeline has " << pipeline.commands.size() << " commands\n";
    return;
    
    /* DISABLED - Multi-command pipeline
    if (pipeline.commands.empty()) {
        return;
    }
    
    // Single command - execute directly
    if (pipeline.commands.size() == 1) {
        executeCommand(*pipeline.commands[0]);
        return;
    }
    
    // Multi-command pipeline
    std::vector<std::unique_ptr<hexstream::HexStreamProcess>> processes;
    */
    
    /* DISABLED - REST OF FUNCTION
    for (size_t i = 0; i < pipeline.commands.size(); i++) {
        auto& cmd = pipeline.commands[i];
        
        hexstream::ProcessConfig config;
        config.executable = cmd->executable;
        config.arguments = cmd->arguments;  // Direct assignment
        
        // TODO: Connect stdout of process i to stdin of process i+1
        // This requires pipe file descriptor management
        
        setupRedirections(cmd->redirections, config);
        
        processes.push_back(std::make_unique<hexstream::HexStreamProcess>(config));
    }
    
    // Wait for all processes
    // TODO: Fix process hanging - temporarily disabled
    std::cerr << "[WARNING] Command execution disabled - would hang\n";
    std::cerr << "[DEBUG] Pipeline had " << processes.size() << " processes\n";
    
    // for (auto& proc : processes) {
    //     proc->wait();
    // }
    
    // Return exit code of last process
    // if (!processes.empty()) {
    //     int exitCode = processes.back()->wait();
    //     lastResult_ = static_cast<int64_t>(exitCode);
    // }
    END OF DISABLED CODE */
}

void Executor::setupRedirections(const std::vector<parser::Redirection>& redirects,
                                 hexstream::ProcessConfig& config) {
    for (const auto& redir : redirects) {
        // TODO: Implement file descriptor redirection
        // This requires opening files and setting up ProcessConfig streams
        
        switch (redir.type) {
            case parser::RedirectionType::INPUT:
                // config.stdinFile = redir.target;
                break;
            case parser::RedirectionType::OUTPUT:
                // config.stdoutFile = redir.target;
                break;
            case parser::RedirectionType::APPEND:
                // config.stdoutFile = redir.target;
                // config.stdoutAppend = true;
                break;
        }
    }
}

} // namespace executor
} // namespace ariash
