#ifndef ARIA_CODEGEN_STMT_H
#define ARIA_CODEGEN_STMT_H

#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Function.h>
#include <map>
#include <string>

// Forward declarations
namespace aria {
    class VarDeclStmt;
    class FuncDeclStmt;
    class IfStmt;
    class WhileStmt;
    class ForStmt;
    class LoopStmt;
    class TillStmt;
    class WhenStmt;
    class PickStmt;
    class FallStmt;
    class BlockStmt;
    class ReturnStmt;
    class BreakStmt;
    class ContinueStmt;
    class DeferStmt;
    class ExpressionStmt;
    class ASTNode;
    
    namespace sema {
        class Type;
    }
}

namespace aria {
namespace backend {

// Forward declaration for expression codegen
class ExprCodegen;

/**
 * StmtCodegen - Statement code generation
 * 
 * Generates LLVM IR for Aria statements including variable declarations,
 * function declarations, control flow (if/else, loops), and blocks.
 * 
 * Phase 4.3: Statement Code Generation
 */
/**
 * Loop context information for break/continue resolution
 */
struct LoopContext {
    std::string label;                         // Optional label for labeled break/continue
    llvm::BasicBlock* continue_block;         // Block to jump to for continue
    llvm::BasicBlock* break_block;            // Block to jump to for break
    
    LoopContext(const std::string& lbl, llvm::BasicBlock* cont, llvm::BasicBlock* brk)
        : label(lbl), continue_block(cont), break_block(brk) {}
};

class StmtCodegen {
private:
    llvm::LLVMContext& context;
    llvm::IRBuilder<>& builder;
    llvm::Module* module;
    
    // Symbol table (maps variable names to their LLVM values - allocas or params)
    std::map<std::string, llvm::Value*>& named_values;
    
    // Expression codegen helper
    ExprCodegen* expr_codegen;
    
    // Loop context stack for break/continue resolution
    std::vector<LoopContext> loop_stack;
    
    // Defer stack for block-scoped cleanup (RAII pattern)
    // Each scope has a vector of BlockStmt* to execute in LIFO order on exit
    std::vector<std::vector<BlockStmt*>> defer_stack;
    
    // Helper: Get LLVM type from Aria type string
    llvm::Type* getLLVMTypeFromString(const std::string& type_name);
    
    // Helper: Get LLVM type from Aria type
    llvm::Type* getLLVMType(sema::Type* type);
    
    // Helper: Execute all defers in current scope
    void executeScopeDefers();
    
    // Helper: Execute all defers up to function level
    void executeFunctionDefers();
    
public:
    /**
     * Constructor
     * @param ctx LLVM context
     * @param bldr IR builder
     * @param mod LLVM module
     * @param values Symbol table for named values
     */
    StmtCodegen(llvm::LLVMContext& ctx, llvm::IRBuilder<>& bldr,
                llvm::Module* mod, std::map<std::string, llvm::Value*>& values);
    
    /**
     * Set expression codegen helper
     * @param expr_gen Expression codegen instance
     */
    void setExprCodegen(ExprCodegen* expr_gen);
    
    /**
     * Generate code for a variable declaration
     * Creates alloca instruction and optional store for initialization
     * Handles: stack, wild, gc allocation modes
     * @param stmt Variable declaration statement
     */
    void codegenVarDecl(VarDeclStmt* stmt);
    
    /**
     * Generate code for a function declaration
     * Creates function with parameters and body
     * @param stmt Function declaration statement
     * @return LLVM Function pointer
     */
    llvm::Function* codegenFuncDecl(FuncDeclStmt* stmt);
    
    /**
     * Generate code for an if statement
     * Creates conditional branches with basic blocks
     * @param stmt If statement
     */
    void codegenIf(IfStmt* stmt);
    
    /**
     * Generate code for a while loop
     * Creates loop with condition check and body
     * @param stmt While statement
     */
    void codegenWhile(WhileStmt* stmt);
    
    /**
     * Generate code for a for loop
     * Creates initialization, condition, increment, and body
     * @param stmt For statement
     */
    void codegenFor(ForStmt* stmt);
    
    /**
     * Generate code for a till loop (Aria-specific)
     * Creates counted loop from 0 to limit with implicit $ variable
     * @param stmt Till statement
     */
    void codegenTill(TillStmt* stmt);
    
    /**
     * Generate code for a loop statement (Aria-specific)
     * Creates counted loop from start to limit with implicit $ variable
     * @param stmt Loop statement
     */
    void codegenLoop(LoopStmt* stmt);
    
    /**
     * Generate code for a when loop (Aria-specific)
     * Creates loop with then/end completion handling
     * @param stmt When statement
     */
    void codegenWhen(WhenStmt* stmt);
    
    /**
     * Generate code for a pick statement (pattern matching)
     * Creates cascading if-else structure for pattern matching
     * @param stmt Pick statement
     */
    void codegenPick(PickStmt* stmt);
    
    /**
     * Generate code for a fall statement (explicit fallthrough)
     * Transfers control to labeled case in pick
     * @param stmt Fall statement
     */
    void codegenFall(FallStmt* stmt);
    
    /**
     * Generate code for a block statement
     * Processes all statements in the block
     * @param stmt Block statement
     */
    void codegenBlock(BlockStmt* stmt);
    
    /**
     * Generate code for a return statement
     * Creates return instruction with defer cleanup
     * @param stmt Return statement
     */
    void codegenReturn(ReturnStmt* stmt);
    
    /**
     * Generate code for a break statement
     * Exits the current loop (or labeled loop)
     * @param stmt Break statement
     */
    void codegenBreak(BreakStmt* stmt);
    
    /**
     * Generate code for a continue statement
     * Skips to next iteration of current loop (or labeled loop)
     * @param stmt Continue statement
     */
    void codegenContinue(ContinueStmt* stmt);
    
    /**
     * Generate code for a defer statement
     * Registers block for LIFO execution at scope exit
     * @param stmt Defer statement
     */
    void codegenDefer(DeferStmt* stmt);
    
    /**
     * Generate code for an expression statement
     * Evaluates expression and discards result
     * @param stmt Expression statement
     */
    void codegenExpressionStmt(ExpressionStmt* stmt);
    
    /**
     * Generate code for any statement (dispatcher)
     * @param stmt Statement node
     */
    void codegenStatement(ASTNode* stmt);
};

} // namespace backend
} // namespace aria

#endif // ARIA_CODEGEN_STMT_H
