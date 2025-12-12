#include "borrow_checker.h"
#include "../ast.h"
#include "../ast/stmt.h"
#include "../ast/expr.h"
#include "../ast/control_flow.h"
#include "../ast/defer.h"
#include "../ast/loops.h"
#include <iostream>
#include <sstream>

namespace aria {

// ============================================================================
// Constructor
// ============================================================================

BorrowChecker::BorrowChecker() 
    : loop_depth_(0), in_defer_(false), temp_var_counter_(0) {
}

// ============================================================================
// Main Analysis Entry Point
// ============================================================================

bool BorrowChecker::analyze(frontend::AstNode* root) {
    if (!root) {
        return true;
    }

    errors_.clear();
    context_ = LifetimeContext(); // Reset context
    
    // Visit the root node
    root->accept(*this);
    
    return !has_errors();
}

// ============================================================================
// Error Reporting
// ============================================================================

void BorrowChecker::report_error(const std::string& message, frontend::AstNode* node) {
    std::ostringstream oss;
    oss << "Borrow Check Error: " << message;
    // TODO: Add line number from node when available
    errors_.push_back(oss.str());
    std::cerr << oss.str() << std::endl;
}

// ============================================================================
// Helper Methods - Variable Name Extraction
// ============================================================================

std::string BorrowChecker::get_var_name(frontend::AstNode* node) {
    if (auto* var_expr = dynamic_cast<frontend::VarExpr*>(node)) {
        return var_expr->name;
    }
    return "";
}

// ============================================================================
// Helper Methods - Borrow Operations
// ============================================================================

std::string BorrowChecker::handle_borrow_operator(frontend::AstNode* operand, 
                                                  bool is_mutable, 
                                                  frontend::AstNode* node) {
    std::string host_name = get_var_name(operand);
    if (host_name.empty()) {
        report_error("Can only borrow variables (use $ on variable names)", node);
        return "";
    }

    // Check if host exists
    if (!context_.variable_exists(host_name)) {
        report_error("Cannot borrow undefined variable '" + host_name + "'", node);
        return "";
    }

    // Generate temporary reference variable name
    std::ostringstream ref_name;
    ref_name << "__ref_" << temp_var_counter_++;
    std::string ref_var_name = ref_name.str();

    // Get current depth (reference is at current scope)
    int ref_depth = context_.current_depth();

    // Create the borrow using LifetimeContext
    BorrowKind kind = is_mutable ? BorrowKind::MUTABLE : BorrowKind::IMMUTABLE;
    bool success = context_.create_borrow(host_name, kind, ref_var_name, ref_depth, nullptr);

    if (!success) {
        std::string error_msg = context_.get_borrow_error_message(host_name, kind);
        report_error("Failed to create borrow: " + error_msg, node);
        return "";
    }

    return ref_var_name;
}

std::string BorrowChecker::handle_pin_operator(frontend::AstNode* operand, 
                                               frontend::AstNode* node) {
    std::string var_name = get_var_name(operand);
    if (var_name.empty()) {
        report_error("Can only pin variables (use # on variable names)", node);
        return "";
    }

    // Check if variable exists
    if (!context_.variable_exists(var_name)) {
        report_error("Cannot pin undefined variable '" + var_name + "'", node);
        return "";
    }

    // Pin the variable
    bool success = context_.pin_variable(var_name);
    if (!success) {
        report_error("Cannot pin variable '" + var_name + "' (must be GC heap object)", node);
        return "";
    }

    // Generate temporary wild pointer variable name
    std::ostringstream wild_name;
    wild_name << "__wild_" << temp_var_counter_++;
    return wild_name.str();
}

void BorrowChecker::handle_address_operator(frontend::AstNode* operand, 
                                           frontend::AstNode* node) {
    std::string var_name = get_var_name(operand);
    if (var_name.empty()) {
        report_error("Can only take address of variables", node);
        return;
    }

    if (!context_.variable_exists(var_name)) {
        report_error("Cannot take address of undefined variable '" + var_name + "'", node);
        return;
    }

    // TODO: Track pointer creation for wild memory tracking
}

void BorrowChecker::handle_dereference_operator(frontend::AstNode* operand, 
                                               frontend::AstNode* node) {
    // Visit operand to ensure it's valid
    if (operand) {
        operand->accept(*this);
    }
}

// ============================================================================
// Helper Methods - Variable Access Validation
// ============================================================================

bool BorrowChecker::validate_read_access(const std::string& var_name, 
                                        frontend::AstNode* node) {
    if (!context_.variable_exists(var_name)) {
        report_error("Cannot read undefined variable '" + var_name + "'", node);
        return false;
    }

    if (!context_.is_valid_for_use(var_name)) {
        report_error("Cannot read from variable '" + var_name + "' (moved or uninitialized)", node);
        return false;
    }

    return true;
}

bool BorrowChecker::validate_write_access(const std::string& var_name, 
                                         frontend::AstNode* node) {
    if (!context_.variable_exists(var_name)) {
        report_error("Cannot write to undefined variable '" + var_name + "'", node);
        return false;
    }

    // Check if variable is pinned (cannot write to pinned variables)
    if (context_.is_pinned(var_name)) {
        report_error("Cannot write to pinned variable '" + var_name + "'", node);
        return false;
    }

    // Check if variable is borrowed (cannot write if borrowed)
    VarInfo* var = context_.lookup_variable(var_name);
    if (var && !var->active_loans.empty()) {
        std::string error_msg = context_.get_borrow_error_message(var_name, BorrowKind::MUTABLE);
        report_error("Cannot write to borrowed variable: " + error_msg, node);
        return false;
    }

    return true;
}

bool BorrowChecker::validate_move_access(const std::string& var_name, 
                                        frontend::AstNode* node) {
    if (!context_.variable_exists(var_name)) {
        report_error("Cannot move undefined variable '" + var_name + "'", node);
        return false;
    }

    VarInfo* var = context_.lookup_variable(var_name);
    if (!var) {
        return false;
    }

    // Cannot move if borrowed
    if (!var->active_loans.empty()) {
        std::string error_msg = context_.get_borrow_error_message(var_name, BorrowKind::MUTABLE);
        report_error("Cannot move borrowed variable: " + error_msg, node);
        return false;
    }

    // Cannot move if already moved
    if (var->state == VarState::MOVED) {
        report_error("Cannot move variable '" + var_name + "' (already moved)", node);
        return false;
    }

    return true;
}

// ============================================================================
// Visitor Methods - Expressions
// ============================================================================

void BorrowChecker::visit(frontend::VarExpr* node) {
    if (node) {
        validate_read_access(node->name, node);
    }
}

void BorrowChecker::visit(frontend::IntLiteral* node) {
    // Literals don't require borrow checking
}

void BorrowChecker::visit(frontend::FloatLiteral* node) {
    // Literals don't require borrow checking
}

void BorrowChecker::visit(frontend::BoolLiteral* node) {
    // Literals don't require borrow checking
}

void BorrowChecker::visit(frontend::NullLiteral* node) {
    // Literals don't require borrow checking
}

void BorrowChecker::visit(frontend::StringLiteral* node) {
    // Literals don't require borrow checking
}

void BorrowChecker::visit(frontend::TemplateString* node) {
    // TODO: Check interpolated expressions
}

void BorrowChecker::visit(frontend::TernaryExpr* node) {
    if (!node) return;
    
    if (node->condition) {
        node->condition->accept(*this);
    }
    if (node->true_expr) {
        node->true_expr->accept(*this);
    }
    if (node->false_expr) {
        node->false_expr->accept(*this);
    }
}

void BorrowChecker::visit(frontend::BinaryOp* node) {
    if (!node) return;
    
    if (node->left) {
        node->left->accept(*this);
    }
    if (node->right) {
        node->right->accept(*this);
    }
    
    // Check for assignment operators
    if (node->op == frontend::BinaryOp::ASSIGN) {
        std::string lhs_name = get_var_name(node->left.get());
        if (!lhs_name.empty()) {
            validate_write_access(lhs_name, node);
        }
    }
}

void BorrowChecker::visit(frontend::UnaryOp* node) {
    if (!node) return;
    
    // Handle special operators
    // TODO: Add BORROW and BORROW_MUT to UnaryOp enum in expr.h
    // For now, PIN and ADDRESS_OF are available
    
    if (node->op == frontend::UnaryOp::PIN) {
        handle_pin_operator(node->operand.get(), node);
        return;
    }
    
    if (node->op == frontend::UnaryOp::ADDRESS_OF) {
        handle_address_operator(node->operand.get(), node);
        return;
    }
    
    // For other operators, just visit operand
    if (node->operand) {
        node->operand->accept(*this);
    }
}

void BorrowChecker::visit(frontend::CallExpr* node) {
    if (!node) return;
    
    // Check for aria.free() calls
    if (node->function_name == "aria.free" || node->function_name == "free") {
        if (!node->arguments.empty()) {
            std::string var_name = get_var_name(node->arguments[0].get());
            if (!var_name.empty()) {
                track_wild_free(var_name, node);
            }
        }
    }
    
    // Check all arguments
    for (auto& arg : node->arguments) {
        if (arg) {
            arg->accept(*this);
        }
    }
}

void BorrowChecker::visit(frontend::ObjectLiteral* node) {
    // TODO: Check field initializers
}

void BorrowChecker::visit(frontend::MemberAccess* node) {
    if (node && node->object) {
        node->object->accept(*this);
    }
}

void BorrowChecker::visit(frontend::VectorLiteral* node) {
    // TODO: Check vector elements
}

void BorrowChecker::visit(frontend::ArrayLiteral* node) {
    // TODO: Check array elements
}

void BorrowChecker::visit(frontend::IndexExpr* node) {
    if (!node) return;
    
    if (node->array) {
        node->array->accept(*this);
    }
    if (node->index) {
        node->index->accept(*this);
    }
}

void BorrowChecker::visit(frontend::UnwrapExpr* node) {
    if (node && node->expression) {
        node->expression->accept(*this);
    }
}

void BorrowChecker::visit(frontend::LambdaExpr* node) {
    if (!node) return;
    
    // Enter lambda scope
    context_.enter_scope();
    
    // TODO: Track closure captures
    
    // Check lambda body
    if (node->body) {
        node->body->accept(*this);
    }
    
    // Exit lambda scope
    context_.exit_scope();
}

void BorrowChecker::visit(frontend::CastExpr* node) {
    if (node && node->expression) {
        node->expression->accept(*this);
    }
}

// ============================================================================
// Visitor Methods - Statements
// ============================================================================

void BorrowChecker::visit(frontend::VarDecl* node) {
    if (!node) return;
    
    // Determine memory region based on declaration
    MemoryRegion region = MemoryRegion::GC_HEAP; // Default
    if (node->is_wild) {
        region = MemoryRegion::WILD_HEAP;
    } else if (node->is_stack) {
        region = MemoryRegion::STACK;
    }
    
    // Declare variable
    VarInfo* var = context_.declare_variable(node->name, region, nullptr);
    
    // If there's an initializer, initialize the variable
    if (node->initializer) {
        node->initializer->accept(*this);
        context_.initialize_variable(node->name);
        
        // Track wild allocations
        if (region == MemoryRegion::WILD_HEAP) {
            track_wild_allocation(node->name, node);
        }
    }
}

void BorrowChecker::visit(frontend::FuncDecl* node) {
    if (!node) return;
    
    // Save current function name
    std::string prev_function = current_function_;
    current_function_ = node->name;
    
    // Enter function scope
    context_.enter_scope();
    
    // TODO: Declare parameters
    
    // Check function body
    if (node->body) {
        node->body->accept(*this);
    }
    
    // Check for wild memory leaks before exiting
    check_wild_leaks(current_function_);
    
    // Exit function scope
    context_.exit_scope();
    
    // Restore previous function name
    current_function_ = prev_function;
}

void BorrowChecker::visit(frontend::StructDecl* node) {
    // Struct declarations don't need borrow checking
}

void BorrowChecker::visit(frontend::ReturnStmt* node) {
    if (!node) return;
    
    if (node->value) {
        node->value->accept(*this);
        
        // TODO: Check if returning a borrow or reference
        // that outlives its scope
    }
}

void BorrowChecker::visit(frontend::ExpressionStmt* node) {
    if (node && node->expression) {
        node->expression->accept(*this);
    }
}

void BorrowChecker::visit(frontend::IfStmt* node) {
    if (!node) return;
    
    // Check condition
    if (node->condition) {
        node->condition->accept(*this);
    }
    
    // Check then branch
    if (node->then_block) {
        context_.enter_scope();
        node->then_block->accept(*this);
        context_.exit_scope();
    }
    
    // Check else branch
    if (node->else_block) {
        context_.enter_scope();
        node->else_block->accept(*this);
        context_.exit_scope();
    }
    
    // TODO: Merge borrow states from branches
}

void BorrowChecker::visit(frontend::Block* node) {
    if (!node) return;
    
    for (auto& stmt : node->statements) {
        if (stmt) {
            stmt->accept(*this);
        }
    }
}

// ============================================================================
// Visitor Methods - Control Flow
// ============================================================================

void BorrowChecker::visit(frontend::PickStmt* node) {
    // TODO: Implement pick statement checking
}

void BorrowChecker::visit(frontend::FallStmt* node) {
    // Fall statements don't require checking
}

void BorrowChecker::visit(frontend::TillLoop* node) {
    if (!node) return;
    
    loop_depth_++;
    context_.enter_scope();
    
    if (node->body) {
        node->body->accept(*this);
    }
    
    context_.exit_scope();
    loop_depth_--;
}

void BorrowChecker::visit(frontend::WhenLoop* node) {
    if (!node) return;
    
    loop_depth_++;
    
    if (node->condition) {
        node->condition->accept(*this);
    }
    
    context_.enter_scope();
    if (node->body) {
        node->body->accept(*this);
    }
    context_.exit_scope();
    
    loop_depth_--;
}

void BorrowChecker::visit(frontend::DeferStmt* node) {
    if (!node) return;
    
    bool prev_in_defer = in_defer_;
    in_defer_ = true;
    
    if (node->body) {
        node->body->accept(*this);
    }
    
    in_defer_ = prev_in_defer;
}

void BorrowChecker::visit(frontend::ForLoop* node) {
    if (!node) return;
    
    loop_depth_++;
    context_.enter_scope();
    
    // TODO: Check ForLoop structure in loops.h for actual fields
    // For now, just check the body
    if (node->body) {
        node->body->accept(*this);
    }
    
    context_.exit_scope();
    loop_depth_--;
}

void BorrowChecker::visit(frontend::WhileLoop* node) {
    if (!node) return;
    
    loop_depth_++;
    
    if (node->condition) {
        node->condition->accept(*this);
    }
    
    context_.enter_scope();
    if (node->body) {
        node->body->accept(*this);
    }
    context_.exit_scope();
    
    loop_depth_--;
}

void BorrowChecker::visit(frontend::BreakStmt* node) {
    if (loop_depth_ == 0) {
        report_error("'break' outside loop", node);
    }
}

void BorrowChecker::visit(frontend::ContinueStmt* node) {
    if (loop_depth_ == 0) {
        report_error("'continue' outside loop", node);
    }
}

// ============================================================================
// Visitor Methods - Async/Module System (Stubs)
// ============================================================================

void BorrowChecker::visit(frontend::WhenExpr* node) {
    // TODO: Implement
}

void BorrowChecker::visit(frontend::AwaitExpr* node) {
    // TODO: Implement
}

void BorrowChecker::visit(frontend::SpawnExpr* node) {
    // TODO: Implement
}

void BorrowChecker::visit(frontend::AsyncBlock* node) {
    // TODO: Implement
}

void BorrowChecker::visit(frontend::UseStmt* node) {
    // No borrow checking needed
}

void BorrowChecker::visit(frontend::ModDef* node) {
    // TODO: Implement
}

void BorrowChecker::visit(frontend::ExternBlock* node) {
    // No borrow checking needed
}

void BorrowChecker::visit(frontend::TraitDecl* node) {
    // No borrow checking needed
}

void BorrowChecker::visit(frontend::ImplDecl* node) {
    // TODO: Implement
}

// ============================================================================
// Helper Methods - Wild Memory Tracking
// ============================================================================

void BorrowChecker::track_wild_allocation(const std::string& var_name, 
                                         frontend::AstNode* node) {
    // Wild allocations are tracked via VarInfo in LifetimeContext
    // Additional tracking can be added here if needed
}

void BorrowChecker::track_wild_free(const std::string& var_name, 
                                   frontend::AstNode* node) {
    if (!context_.variable_exists(var_name)) {
        report_error("Cannot free undefined variable '" + var_name + "'", node);
        return;
    }
    
    // Mark as moved (wild pointers become invalid after free)
    context_.move_variable(var_name);
}

void BorrowChecker::check_wild_leaks(const std::string& scope_name) {
    // TODO: Check for wild allocations without corresponding frees/defers
    // This requires tracking aria.alloc() calls and matching them with
    // aria.free() or defer statements
}

} // namespace aria
