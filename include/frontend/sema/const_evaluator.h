#ifndef ARIA_CONST_EVALUATOR_H
#define ARIA_CONST_EVALUATOR_H

#include "frontend/ast/ast_node.h"
#include "frontend/ast/expr.h"
#include <memory>
#include <string>
#include <map>
#include <unordered_map>
#include <vector>
#include <stdint.h>
#include <variant>

// Forward declarations to avoid circular dependency
namespace aria {
    class FuncDeclStmt;
    class ParameterNode;
    class ReturnStmt;
}

namespace aria {
namespace sema {

/**
 * ComptimeValue - Represents a value computed at compile time
 * 
 * Reference: research_030 Section 4.1
 * This is the value type used by the CTFE interpreter.
 * Supports all Aria types including TBB with ERR sentinels.
 */
class ComptimeValue {
public:
    enum class Kind {
        INTEGER,      // Signed integers (int8-512)
        UNSIGNED,     // Unsigned integers (uint8-512)
        TBB,          // Twisted Balanced Binary (tbb8-64)
        FLOAT,        // Floating point (flt32-512)
        BOOL,         // Boolean
        STRING,       // String literal
        ARRAY,        // Array of values
        STRUCT,       // Struct with fields
        POINTER,      // Virtual heap pointer
        FUNCTION,     // Function reference
        NULL_VALUE,   // NULL
        ERR_SENTINEL  // TBB ERR sentinel
    };
    
    // Pointer representation: opaque handle {AllocID, Offset}
    // NOT raw addresses (research_030 Section 7)
    struct PointerHandle {
        uint32_t allocId;  // Allocation ID in Virtual Heap
        uint32_t offset;   // Byte offset within allocation
        
        PointerHandle() : allocId(0), offset(0) {}
        PointerHandle(uint32_t id, uint32_t off) : allocId(id), offset(off) {}
        
        bool operator==(const PointerHandle& other) const {
            return allocId == other.allocId && offset == other.offset;
        }
    };
    
private:
    Kind kind;
    
    // Value storage (using variant for type safety)
    std::variant<
        int64_t,      // INTEGER, UNSIGNED, TBB (stored as int64)
        double,       // FLOAT
        bool,         // BOOL
        std::string,  // STRING
        std::vector<ComptimeValue>, // ARRAY
        std::map<std::string, ComptimeValue>, // STRUCT
        PointerHandle // POINTER (virtual heap handle {AllocID, Offset})
    > value;
    
    std::string typeName;  // Aria type name (e.g., "int32", "tbb8")
    int bitWidth;          // Bit width for integer/TBB types
    
public:
    ComptimeValue() : kind(Kind::NULL_VALUE), bitWidth(0) {}
    
    // Factory methods
    static ComptimeValue makeInteger(int64_t val, const std::string& type, int bits);
    static ComptimeValue makeUnsigned(uint64_t val, const std::string& type, int bits);
    static ComptimeValue makeTBB(int64_t val, const std::string& type, int bits);
    static ComptimeValue makeFloat(double val, const std::string& type);
    static ComptimeValue makeBool(bool val);
    static ComptimeValue makeString(const std::string& val);
    static ComptimeValue makePointer(uint32_t allocId, uint32_t offset, const std::string& type);
    static ComptimeValue makeERR(const std::string& type, int bits);
    
    // Type queries
    Kind getKind() const { return kind; }
    const std::string& getTypeName() const { return typeName; }
    int getBitWidth() const { return bitWidth; }
    
    bool isInteger() const { return kind == Kind::INTEGER || kind == Kind::UNSIGNED; }
    bool isTBB() const { return kind == Kind::TBB; }
    bool isFloat() const { return kind == Kind::FLOAT; }
    bool isBool() const { return kind == Kind::BOOL; }
    bool isString() const { return kind == Kind::STRING; }
    bool isPointer() const { return kind == Kind::POINTER; }
    bool isERR() const { return kind == Kind::ERR_SENTINEL; }
    
    // Comparison operator for memoization cache keys (research_030 Section 5.2)
    bool operator<(const ComptimeValue& other) const;
    bool operator==(const ComptimeValue& other) const;
    
    // Value accessors
    int64_t getInt() const;
    uint64_t getUint() const;
    double getFloat() const;
    bool getBool() const;
    const std::string& getString() const;
    const PointerHandle& getPointer() const;
    
    // TBB-specific queries
    bool isTBBInRange() const;  // Check if TBB value is not ERR
    int64_t getTBBMin() const;  // Get min value for TBB type (excluding ERR)
    int64_t getTBBMax() const;  // Get max value for TBB type
    int64_t getTBBERR() const;  // Get ERR sentinel for TBB type
    
    std::string toString() const;
};

/**
 * ConstEvaluator - Compile-Time Function Evaluation (CTFE) Interpreter
 * 
 * Reference: research_030 Section 4
 * This is the core of Aria's const/comptime system. It evaluates AST nodes
 * at compile time, with full support for:
 * - TBB arithmetic with sticky error propagation
 * - Virtual Heap for safe pointer operations
 * - Recursion with memoization
 * - Resource limits (instruction budget, stack depth)
 */
class ConstEvaluator {
private:
    // === Evaluation Context ===
    std::map<std::string, ComptimeValue> constants;  // Named const values
    std::vector<std::map<std::string, ComptimeValue>> scopeStack;  // Local scopes
    
    // === Function Registry (research_030 Section 5) ===
    // Stores const-evaluable functions for CTFE
    // TODO: Task 8 will integrate with full symbol table
    std::map<std::string, FuncDeclStmt*> functions;  // Function name -> AST node
    
    // === Memoization Cache ===
    // Maps (function_name, arg_values) -> result
    std::map<std::string, std::map<std::vector<ComptimeValue>, ComptimeValue>> memoCache;
    
    // === Resource Limits (research_030 Section 4.3) ===
    static constexpr size_t DEFAULT_INSTRUCTION_LIMIT = 1000000;
    static constexpr size_t DEFAULT_STACK_DEPTH_LIMIT = 512;
    static constexpr size_t DEFAULT_HEAP_SIZE_LIMIT = 1024 * 1024 * 1024;  // 1GB
    
    size_t instructionCount;
    size_t instructionLimit;
    size_t stackDepth;
    size_t stackDepthLimit;
    
    // === Virtual Heap (research_030 Section 7 & 13.2) ===
    // Sandboxed memory simulation using opaque handles {AllocID, Offset}
    struct Allocation {
        std::vector<uint8_t> data;
        bool isMutable;           // Can be written to
        bool isStaticPromotable;  // Can move to .rodata
        bool isWild;              // Manual memory (wild)
        bool isGC;                // GC memory
        bool isWildX;             // Executable memory (FORBIDDEN in CTFE)
    };
    std::unordered_map<uint32_t, Allocation> virtualHeap;
    uint32_t nextAllocId;
    size_t virtualHeapSize;
    size_t virtualHeapLimit;
    
    // === Error Handling ===
    std::vector<std::string> errors;
    
public:
    ConstEvaluator();
    
    // === Main Evaluation Interface ===
    ComptimeValue evaluate(ASTNode* node);
    ComptimeValue evaluateExpr(ASTNode* node);
    ComptimeValue evaluateStmt(ASTNode* stmt);
    
    // === Expression Evaluation ===
    ComptimeValue evalLiteral(LiteralExpr* lit);
    ComptimeValue evalIdentifier(IdentifierExpr* ident);
    ComptimeValue evalBinaryOp(BinaryExpr* binOp);
    ComptimeValue evalUnaryOp(UnaryExpr* unOp);
    ComptimeValue evalTernary(TernaryExpr* ternary);
    ComptimeValue evalFunctionCall(CallExpr* call);
    
    // === TBB Arithmetic (research_030 Section 4.2) ===
    ComptimeValue tbbAdd(const ComptimeValue& a, const ComptimeValue& b);
    ComptimeValue tbbSub(const ComptimeValue& a, const ComptimeValue& b);
    ComptimeValue tbbMul(const ComptimeValue& a, const ComptimeValue& b);
    ComptimeValue tbbDiv(const ComptimeValue& a, const ComptimeValue& b);
    ComptimeValue tbbMod(const ComptimeValue& a, const ComptimeValue& b);
    ComptimeValue tbbNeg(const ComptimeValue& a);
    
    // === Standard Arithmetic ===
    ComptimeValue intAdd(const ComptimeValue& a, const ComptimeValue& b);
    ComptimeValue intSub(const ComptimeValue& a, const ComptimeValue& b);
    ComptimeValue intMul(const ComptimeValue& a, const ComptimeValue& b);
    ComptimeValue intDiv(const ComptimeValue& a, const ComptimeValue& b);
    ComptimeValue intMod(const ComptimeValue& a, const ComptimeValue& b);
    ComptimeValue intNeg(const ComptimeValue& a);
    
    ComptimeValue floatAdd(const ComptimeValue& a, const ComptimeValue& b);
    ComptimeValue floatSub(const ComptimeValue& a, const ComptimeValue& b);
    ComptimeValue floatMul(const ComptimeValue& a, const ComptimeValue& b);
    ComptimeValue floatDiv(const ComptimeValue& a, const ComptimeValue& b);
    ComptimeValue floatNeg(const ComptimeValue& a);
    
    // === Comparison Operations ===
    ComptimeValue compare(const ComptimeValue& a, const ComptimeValue& b, const std::string& op);
    
    // === Logical Operations ===
    ComptimeValue logicalAnd(const ComptimeValue& a, const ComptimeValue& b);
    ComptimeValue logicalOr(const ComptimeValue& a, const ComptimeValue& b);
    ComptimeValue logicalNot(const ComptimeValue& a);
    
    // === Virtual Heap Operations (research_030 Section 7 & 13.2) ===
    // Sandboxed memory simulation for compile-time pointer operations
    ComptimeValue allocate(size_t sizeBytes, bool isMutable = true, bool isWild = false);
    void deallocate(uint32_t allocId);
    uint8_t readByte(uint32_t allocId, uint32_t offset);
    void writeByte(uint32_t allocId, uint32_t offset, uint8_t value);
    bool isValidAllocation(uint32_t allocId) const;
    size_t getAllocationSize(uint32_t allocId) const;
    
    // Higher-level pointer operations
    ComptimeValue dereference(const ComptimeValue& ptr);
    void store(const ComptimeValue& ptr, const ComptimeValue& value);
    
    // === Scope Management ===
    void pushScope();
    void popScope();
    void defineConstant(const std::string& name, const ComptimeValue& value);
    ComptimeValue lookupConstant(const std::string& name);
    
    // === Function Registration (research_030 Section 5) ===
    // Register a function for const evaluation
    void registerFunction(const std::string& name, FuncDeclStmt* funcDecl);
    FuncDeclStmt* lookupFunction(const std::string& name);
    
    // === Resource Management ===
    void resetLimits();
    void setInstructionLimit(size_t limit) { instructionLimit = limit; }
    void setStackDepthLimit(size_t limit) { stackDepthLimit = limit; }
    void setHeapSizeLimit(size_t limit) { virtualHeapLimit = limit; }
    
    bool checkInstructionLimit();
    bool checkStackDepth();
    bool checkHeapSize(size_t additionalBytes);
    
    // Stack frame management for function calls (research_030 Section 5.2)
    bool pushStackFrame();   // Returns false if limit exceeded
    void popStackFrame();
    
    // === Error Handling ===
    bool hasErrors() const { return !errors.empty(); }
    const std::vector<std::string>& getErrors() const { return errors; }
    void clearErrors() { errors.clear(); }
    
    // === Memoization (research_030 Section 5.2) ===
    // Infrastructure ready; will be activated in Step 7 (const functions)
    void clearMemoCache() { memoCache.clear(); }
    bool hasMemoizedResult(const std::string& funcName, const std::vector<ComptimeValue>& args) const;
    ComptimeValue getMemoizedResult(const std::string& funcName, const std::vector<ComptimeValue>& args);
    void memoizeResult(const std::string& funcName, const std::vector<ComptimeValue>& args, const ComptimeValue& result);
    
private:
    void addError(const std::string& msg);
    void incrementInstructions();
    
    // Helper for type coercion
    ComptimeValue coerce(const ComptimeValue& val, const std::string& targetType);
};

} // namespace sema
} // namespace aria

#endif // ARIA_CONST_EVALUATOR_H
