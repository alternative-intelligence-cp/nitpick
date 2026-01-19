/**
 * AST Implementation - Visitor Accept Methods
 */

#include "parser/ast.hpp"

namespace ariash {
namespace parser {

// Expressions
void IntegerLiteral::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void StringLiteral::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void VariableExpr::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void BinaryOpExpr::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void UnaryOpExpr::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void CallExpr::accept(ASTVisitor& visitor) { visitor.visit(*this); }

// Statements
void BlockStmt::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void VarDeclStmt::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void AssignStmt::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void IfStmt::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void WhileStmt::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void ForStmt::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void ReturnStmt::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void ExprStmt::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void CommandStmt::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void PipelineStmt::accept(ASTVisitor& visitor) { visitor.visit(*this); }
void Program::accept(ASTVisitor& visitor) { visitor.visit(*this); }

} // namespace parser
} // namespace ariash
