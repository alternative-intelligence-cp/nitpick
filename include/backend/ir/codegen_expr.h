#ifndef ARIA_CODEGEN_EXPR_H
#define ARIA_CODEGEN_EXPR_H

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/Module.h>
#include <map>
#include <string>

// Forward declarations
namespace aria {
    class ASTNode;
    class LiteralExpr;
    class IdentifierExpr;
    class BinaryExpr;
    class UnaryExpr;
    class CallExpr;
    class TernaryExpr;
    class IndexExpr;
    class MemberAccessExpr;
    class LambdaExpr;
    
    namespace sema {
        class Type;
    }
}

namespace aria {
namespace backend {

// Forward declaration for circular dependency
class StmtCodegen;

/**
 * ExprCodegen - Expression code generation
 * 
 * Generates LLVM IR for Aria expressions including literals, identifiers,
 * operators, and function calls.
 * 
 * Phase 4.2: Expression Code Generation
 */
class ExprCodegen {
private:
    llvm::LLVMContext& context;
    llvm::IRBuilder<>& builder;
    llvm::Module* module;
    
    // Symbol table (maps variable names to their LLVM values)
    std::map<std::string, llvm::Value*>& named_values;
    
    // Statement codegen (for lambda body generation)
    StmtCodegen* stmt_codegen;
    
    // Helper: Get LLVM type from Aria type
    llvm::Type* getLLVMType(sema::Type* type);
    
    // Helper: Get LLVM type from Aria type name string
    llvm::Type* getLLVMTypeFromString(const std::string& typeName);
    
    // Helper: Get size of Aria type in bytes
    size_t getTypeSize(sema::Type* type);
    
public:
    /**
     * Constructor
     * @param ctx LLVM context
     * @param bldr IR builder
     * @param mod LLVM module
     * @param values Symbol table for named values
     */
    ExprCodegen(llvm::LLVMContext& ctx, llvm::IRBuilder<>& bldr, 
                llvm::Module* mod, std::map<std::string, llvm::Value*>& values);
    
    /**
     * Set statement codegen helper (for lambda body generation)
     * @param stmt_gen Statement codegen instance
     */
    void setStmtCodegen(StmtCodegen* stmt_gen);
    
    /**
     * Generate code for a literal expression
     * Handles: int, float, string, bool, null
     * @param expr Literal expression node
     * @return LLVM constant value
     */
    llvm::Value* codegenLiteral(LiteralExpr* expr);
    
    /**
     * Generate code for an identifier (variable reference)
     * @param expr Identifier expression node
     * @return LLVM load instruction
     */
    llvm::Value* codegenIdentifier(IdentifierExpr* expr);
    
    /**
     * Generate code for a binary operation
     * Handles: arithmetic, comparison, logical, bitwise operators
     * @param expr Binary expression node
     * @return LLVM value of the operation result
     */
    llvm::Value* codegenBinary(BinaryExpr* expr);
    
    /**
     * Generate code for a unary operation
     * Handles: neg, not, address, deref
     * @param expr Unary expression node
     * @return LLVM value of the operation result
     */
    llvm::Value* codegenUnary(UnaryExpr* expr);
    
    /**
     * Generate code for a function call
     * @param expr Call expression node
     * @return LLVM call instruction result
     */
    llvm::Value* codegenCall(CallExpr* expr);
    
    /**
     * Generate code for a ternary expression (is ? : operator)
     * @param expr Ternary expression node
     * @return LLVM value selected by condition
     */
    llvm::Value* codegenTernary(TernaryExpr* expr);
    
    /**
     * Generate code for an array index operation
     * @param expr Index expression node
     * @return LLVM value at indexed location
     */
    llvm::Value* codegenIndex(IndexExpr* expr);
    
    /**
     * Generate code for member access
     * @param expr Member access expression node
     * @return LLVM value of the member
     */
    llvm::Value* codegenMemberAccess(MemberAccessExpr* expr);
    
    /**
     * Generate code for lambda expressions (closures)
     * Creates fat pointer: { method_ptr, env_ptr }
     * @param expr Lambda expression node
     * @return LLVM value of the fat pointer struct
     * Reference: research_016 (Functional Types)
     */
    llvm::Value* codegenLambda(LambdaExpr* expr);
    
    /**
     * Generate code for any expression (dispatcher)
     * @param node Expression node
     * @param codegen ExprCodegen instance
     * @return LLVM value of the expression
     */
    static llvm::Value* codegenExpressionNode(aria::ASTNode* node, ExprCodegen* codegen);
};

} // namespace backend
} // namespace aria

#endif // ARIA_CODEGEN_EXPR_H
