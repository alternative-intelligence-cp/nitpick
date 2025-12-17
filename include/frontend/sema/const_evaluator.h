#ifndef ARIA_CONST_EVALUATOR_H
#define ARIA_CONST_EVALUATOR_H

#include "frontend/ast/ast_node.h"
#include "frontend/ast/expr.h"
#include "frontend/ast/stmt.h"
#include <memory>
#include <string>
#include <map>
#include <vector>
#include <stdint.h>
#include <variant>

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
        void*         // POINTER (virtual heap address)
    > value;
    
    std::string typeName;  // Aria type name (e.g., "int32", "tbb8")
    int bitWidth;          // Bit width for integer/TBB types
    
public:
    ComptimeValue() : kind(Kind::NULL_VALUE), bitWidth(0) {}
    
    // Integer constructors
    static ComptimeValue makeInteger(int64_t val, const std::string& type, int bits);
    static ComptimeValue makeUnsigned(uint64_t val, const std::string& type, int bits);
    static ComptimeValue makeTBB(int64_t val, const std::string& type, int bits);
    static ComptimeValue makeFloat(double val, const std::string& type);
    static ComptimeValue makeBool(bool val);
    static ComptimeValue makeString(const std::string& val);
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
    bool isERR() const { return kind == Kind::ERR_SENTINEL; }
    
    // Value accessors
    int64_t getInt() const;
    uint64_t getUint() const;
    double getFloat() const;
    bool getBool() const;
    const std::string& getString() const;
    
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
    
    // === Virtual Heap (research_030 Section 7) ===
    struct VirtualHeapBlock {
        std::vector<uint8_t> data;
        std::string typeName;
        bool isWild;   // Manual memory (wild)
        bool isGC;     // GC memory
        bool isWildX;  // Executable memory (FORBIDDEN)
    };
    std::map<void*, VirtualHeapBlock> virtualHeap;
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
    
    // === Virtual Heap Operations (research_030 Section 7) ===
    ComptimeValue allocate(const std::string& typeName, size_t count, bool isWild);
    void deallocate(void* ptr);
    ComptimeValue dereference(const ComptimeValue& ptr);
    void store(const ComptimeValue& ptr, const ComptimeValue& value);
    
    // === Scope Management ===
    void pushScope();
    void popScope();
    void defineConstant(const std::string& name, const ComptimeValue& value);
    ComptimeValue lookupConstant(const std::string& name);
    
    // === Resource Management ===
    void resetLimits();
    void setInstructionLimit(size_t limit) { instructionLimit = limit; }
    void setStackDepthLimit(size_t limit) { stackDepthLimit = limit; }
    void setHeapSizeLimit(size_t limit) { virtualHeapLimit = limit; }
    
    bool checkInstructionLimit();
    bool checkStackDepth();
    bool checkHeapSize(size_t additionalBytes);
    
    // === Error Handling ===
    bool hasErrors() const { return !errors.empty(); }
    const std::vector<std::string>& getErrors() const { return errors; }
    void clearErrors() { errors.clear(); }
    
    // === Memoization ===
    void clearMemoCache() { memoCache.clear(); }
    
private:
    void addError(const std::string& msg);
    void incrementInstructions();
    
    // Helper for type coercion
    ComptimeValue coerce(const ComptimeValue& val, const std::string& targetType);
};

} // namespace sema
} // namespace aria

#endif // ARIA_CONST_EVALUATOR_H
