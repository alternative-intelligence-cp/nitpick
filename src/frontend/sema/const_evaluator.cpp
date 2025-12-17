#include "frontend/sema/const_evaluator.h"
#include "frontend/ast/expr.h"
#include "frontend/ast/stmt.h"
#include <sstream>
#include <cmath>
#include <limits>

namespace aria {
namespace sema {

// ============================================================================
// ComptimeValue Implementation
// ============================================================================

ComptimeValue ComptimeValue::makeInteger(int64_t val, const std::string& type, int bits) {
    ComptimeValue result;
    result.kind = Kind::INTEGER;
    result.value = val;
    result.typeName = type;
    result.bitWidth = bits;
    return result;
}

ComptimeValue ComptimeValue::makeUnsigned(uint64_t val, const std::string& type, int bits) {
    ComptimeValue result;
    result.kind = Kind::UNSIGNED;
    result.value = static_cast<int64_t>(val);
    result.typeName = type;
    result.bitWidth = bits;
    return result;
}

ComptimeValue ComptimeValue::makeTBB(int64_t val, const std::string& type, int bits) {
    ComptimeValue result;
    result.kind = Kind::TBB;
    result.value = val;
    result.typeName = type;
    result.bitWidth = bits;
    return result;
}

ComptimeValue ComptimeValue::makeFloat(double val, const std::string& type) {
    ComptimeValue result;
    result.kind = Kind::FLOAT;
    result.value = val;
    result.typeName = type;
    result.bitWidth = (type == "flt32") ? 32 : (type == "flt64") ? 64 : 128;
    return result;
}

ComptimeValue ComptimeValue::makeBool(bool val) {
    ComptimeValue result;
    result.kind = Kind::BOOL;
    result.value = val;
    result.typeName = "bool";
    result.bitWidth = 1;
    return result;
}

ComptimeValue ComptimeValue::makeString(const std::string& val) {
    ComptimeValue result;
    result.kind = Kind::STRING;
    result.value = val;
    result.typeName = "string";
    result.bitWidth = 0;
    return result;
}

ComptimeValue ComptimeValue::makeERR(const std::string& type, int bits) {
    ComptimeValue result;
    result.kind = Kind::ERR_SENTINEL;
    result.typeName = type;
    result.bitWidth = bits;
    // Store the ERR value (minimum value for the type)
    if (bits == 8) result.value = static_cast<int64_t>(-128);
    else if (bits == 16) result.value = static_cast<int64_t>(-32768);
    else if (bits == 32) result.value = static_cast<int64_t>(-2147483648LL);
    else if (bits == 64) result.value = std::numeric_limits<int64_t>::min();
    return result;
}

int64_t ComptimeValue::getInt() const {
    return std::get<int64_t>(value);
}

uint64_t ComptimeValue::getUint() const {
    return static_cast<uint64_t>(std::get<int64_t>(value));
}

double ComptimeValue::getFloat() const {
    return std::get<double>(value);
}

bool ComptimeValue::getBool() const {
    return std::get<bool>(value);
}

const std::string& ComptimeValue::getString() const {
    return std::get<std::string>(value);
}

bool ComptimeValue::isTBBInRange() const {
    if (!isTBB()) return false;
    int64_t val = getInt();
    int64_t err = getTBBERR();
    return val != err;
}

int64_t ComptimeValue::getTBBMin() const {
    // TBB min is -max (symmetric range, excluding ERR)
    if (bitWidth == 8) return -127;
    if (bitWidth == 16) return -32767;
    if (bitWidth == 32) return -2147483647LL;
    if (bitWidth == 64) return -9223372036854775807LL;
    return 0;
}

int64_t ComptimeValue::getTBBMax() const {
    // TBB max is the maximum positive value
    if (bitWidth == 8) return 127;
    if (bitWidth == 16) return 32767;
    if (bitWidth == 32) return 2147483647;
    if (bitWidth == 64) return 9223372036854775807LL;
    return 0;
}

int64_t ComptimeValue::getTBBERR() const {
    // ERR is the minimum two's complement value
    if (bitWidth == 8) return -128;
    if (bitWidth == 16) return -32768;
    if (bitWidth == 32) return -2147483648LL;
    if (bitWidth == 64) return std::numeric_limits<int64_t>::min();
    return 0;
}

std::string ComptimeValue::toString() const {
    std::ostringstream oss;
    switch (kind) {
        case Kind::INTEGER:
        case Kind::UNSIGNED:
        case Kind::TBB:
            oss << getInt();
            break;
        case Kind::FLOAT:
            oss << getFloat();
            break;
        case Kind::BOOL:
            oss << (getBool() ? "true" : "false");
            break;
        case Kind::STRING:
            oss << "\"" << getString() << "\"";
            break;
        case Kind::ERR_SENTINEL:
            oss << "ERR";
            break;
        case Kind::NULL_VALUE:
            oss << "NULL";
            break;
        default:
            oss << "<complex value>";
    }
    return oss.str();
}

// ============================================================================
// ConstEvaluator Implementation
// ============================================================================

ConstEvaluator::ConstEvaluator()
    : instructionCount(0),
      instructionLimit(DEFAULT_INSTRUCTION_LIMIT),
      stackDepth(0),
      stackDepthLimit(DEFAULT_STACK_DEPTH_LIMIT),
      virtualHeapSize(0),
      virtualHeapLimit(DEFAULT_HEAP_SIZE_LIMIT) {
}

ComptimeValue ConstEvaluator::evaluate(ASTNode* node) {
    if (!node) {
        addError("Cannot evaluate null AST node");
        return ComptimeValue();
    }
    
    // If it's a statement (like const declaration), handle it
    if (node->isStatement()) {
        return evaluateStmt(node);
    }
    
    // Otherwise treat as expression
    return evaluateExpr(node);
}

ComptimeValue ConstEvaluator::evaluateExpr(ASTNode* node) {
    if (!node) {
        addError("Cannot evaluate null expression");
        return ComptimeValue();
    }
    
    incrementInstructions();
    
    // Dispatch based on expression type
    switch (node->type) {
        case ASTNode::NodeType::LITERAL:
            return evalLiteral(static_cast<LiteralExpr*>(node));
        case ASTNode::NodeType::IDENTIFIER:
            return evalIdentifier(static_cast<IdentifierExpr*>(node));
        case ASTNode::NodeType::BINARY_OP:
            return evalBinaryOp(static_cast<BinaryExpr*>(node));
        case ASTNode::NodeType::UNARY_OP:
            return evalUnaryOp(static_cast<UnaryExpr*>(node));
        case ASTNode::NodeType::TERNARY:
            return evalTernary(static_cast<TernaryExpr*>(node));
        case ASTNode::NodeType::CALL:
            return evalFunctionCall(static_cast<CallExpr*>(node));
        default:
            addError("Unsupported expression type in const evaluation");
            return ComptimeValue();
    }
}

ComptimeValue ConstEvaluator::evaluateStmt(ASTNode* stmt) {
    // For now, only variable declarations are evaluated
    if (stmt->type == ASTNode::NodeType::VAR_DECL) {
        auto* varDecl = static_cast<VarDeclStmt*>(stmt);
        if (varDecl->isConst && varDecl->initializer) {
            ComptimeValue value = evaluateExpr(varDecl->initializer.get());
            defineConstant(varDecl->varName, value);
            return value;
        }
    }
    
    addError("Statement cannot be evaluated at compile time");
    return ComptimeValue();
}

ComptimeValue ConstEvaluator::evalLiteral(LiteralExpr* lit) {
    // LiteralExpr uses std::variant<int64_t, double, std::string, bool, std::monostate>
    if (std::holds_alternative<int64_t>(lit->value)) {
        // Integer literal - default to int64
        return ComptimeValue::makeInteger(std::get<int64_t>(lit->value), "int64", 64);
    } 
    else if (std::holds_alternative<double>(lit->value)) {
        // Float literal - default to flt64
        return ComptimeValue::makeFloat(std::get<double>(lit->value), "flt64");
    }
    else if (std::holds_alternative<bool>(lit->value)) {
        // Boolean literal
        return ComptimeValue::makeBool(std::get<bool>(lit->value));
    }
    else if (std::holds_alternative<std::string>(lit->value)) {
        // String literal
        return ComptimeValue::makeString(std::get<std::string>(lit->value));
    }
    else if (std::holds_alternative<std::monostate>(lit->value)) {
        // NULL literal
        return ComptimeValue();
    }
    
    addError("Unsupported literal type");
    return ComptimeValue();
}

ComptimeValue ConstEvaluator::evalIdentifier(IdentifierExpr* ident) {
    return lookupConstant(ident->name);
}

ComptimeValue ConstEvaluator::evalBinaryOp(BinaryExpr* binOp) {
    // Evaluate operands
    ComptimeValue left = evaluateExpr(binOp->left.get());
    ComptimeValue right = evaluateExpr(binOp->right.get());
    
    if (hasErrors()) return ComptimeValue();
    
    // Determine if we're dealing with TBB types
    bool isTBB = left.isTBB() || right.isTBB();
    bool isFloat = left.isFloat() || right.isFloat();
    
    // Handle TBB sticky errors
    if (isTBB && (left.isERR() || right.isERR())) {
        // Sticky error propagation: ERR + anything = ERR
        int bits = std::max(left.getBitWidth(), right.getBitWidth());
        return ComptimeValue::makeERR("tbb" + std::to_string(bits), bits);
    }
    
    const std::string& op = binOp->op.lexeme;
    
    // Dispatch arithmetic operations
    if (op == "+" || op == "-" || op == "*" || op == "/" || op == "%") {
        
        if (isTBB) {
            if (op == "+") return tbbAdd(left, right);
            if (op == "-") return tbbSub(left, right);
            if (op == "*") return tbbMul(left, right);
            if (op == "/") return tbbDiv(left, right);
            if (op == "%") return tbbMod(left, right);
        } else if (isFloat) {
            if (op == "+") return floatAdd(left, right);
            if (op == "-") return floatSub(left, right);
            if (op == "*") return floatMul(left, right);
            if (op == "/") return floatDiv(left, right);
        } else {
            if (op == "+") return intAdd(left, right);
            if (op == "-") return intSub(left, right);
            if (op == "*") return intMul(left, right);
            if (op == "/") return intDiv(left, right);
            if (op == "%") return intMod(left, right);
        }
    }
    
    // Comparison operations
    if (op == "==" || op == "!=" || op == "<" || op == "<=" || op == ">" || op == ">=") {
        return compare(left, right, op);
    }
    
    // Logical operations
    if (op == "&&") return logicalAnd(left, right);
    if (op == "||") return logicalOr(left, right);
    
    addError("Unsupported binary operator: " + op);
    return ComptimeValue();
}

ComptimeValue ConstEvaluator::evalUnaryOp(UnaryExpr* unOp) {
    ComptimeValue operand = evaluateExpr(unOp->operand.get());
    
    if (hasErrors()) return ComptimeValue();
    
    const std::string& op = unOp->op.lexeme;
    
    if (op == "-") {
        if (operand.isTBB()) return tbbNeg(operand);
        if (operand.isFloat()) return floatNeg(operand);
        return intNeg(operand);
    }
    
    if (op == "!") {
        return logicalNot(operand);
    }
    
    addError("Unsupported unary operator: " + op);
    return ComptimeValue();
}

ComptimeValue ConstEvaluator::evalTernary(TernaryExpr* ternary) {
    ComptimeValue cond = evaluateExpr(ternary->condition.get());
    
    if (hasErrors()) return ComptimeValue();
    
    if (!cond.isBool()) {
        addError("Ternary condition must be boolean");
        return ComptimeValue();
    }
    
    if (cond.getBool()) {
        return evaluateExpr(ternary->trueValue.get());
    } else {
        return evaluateExpr(ternary->falseValue.get());
    }
}

ComptimeValue ConstEvaluator::evalFunctionCall(CallExpr* call) {
    (void)call; // unused for now
    // TODO: Implement const function evaluation in Step 7
    addError("Const function evaluation not yet implemented");
    return ComptimeValue();
}

// ============================================================================
// TBB Arithmetic Implementation (research_030 Section 4.2)
// ============================================================================

ComptimeValue ConstEvaluator::tbbAdd(const ComptimeValue& a, const ComptimeValue& b) {
    // Sticky error check
    if (a.isERR() || b.isERR()) {
        int bits = std::max(a.getBitWidth(), b.getBitWidth());
        return ComptimeValue::makeERR("tbb" + std::to_string(bits), bits);
    }
    
    int64_t aVal = a.getInt();
    int64_t bVal = b.getInt();
    int64_t result = aVal + bVal;
    int bits = std::max(a.getBitWidth(), b.getBitWidth());
    
    // Overflow check: if result exceeds max or goes below -max, return ERR
    int64_t tbbMax = (bits == 8) ? 127 : (bits == 16) ? 32767 : (bits == 32) ? 2147483647 : 9223372036854775807LL;
    int64_t tbbMin = -tbbMax;
    
    if (result > tbbMax || result < tbbMin) {
        return ComptimeValue::makeERR("tbb" + std::to_string(bits), bits);
    }
    
    return ComptimeValue::makeTBB(result, "tbb" + std::to_string(bits), bits);
}

ComptimeValue ConstEvaluator::tbbSub(const ComptimeValue& a, const ComptimeValue& b) {
    if (a.isERR() || b.isERR()) {
        int bits = std::max(a.getBitWidth(), b.getBitWidth());
        return ComptimeValue::makeERR("tbb" + std::to_string(bits), bits);
    }
    
    int64_t aVal = a.getInt();
    int64_t bVal = b.getInt();
    int64_t result = aVal - bVal;
    int bits = std::max(a.getBitWidth(), b.getBitWidth());
    
    int64_t tbbMax = (bits == 8) ? 127 : (bits == 16) ? 32767 : (bits == 32) ? 2147483647 : 9223372036854775807LL;
    int64_t tbbMin = -tbbMax;
    
    if (result > tbbMax || result < tbbMin) {
        return ComptimeValue::makeERR("tbb" + std::to_string(bits), bits);
    }
    
    return ComptimeValue::makeTBB(result, "tbb" + std::to_string(bits), bits);
}

ComptimeValue ConstEvaluator::tbbMul(const ComptimeValue& a, const ComptimeValue& b) {
    if (a.isERR() || b.isERR()) {
        int bits = std::max(a.getBitWidth(), b.getBitWidth());
        return ComptimeValue::makeERR("tbb" + std::to_string(bits), bits);
    }
    
    int64_t aVal = a.getInt();
    int64_t bVal = b.getInt();
    int bits = std::max(a.getBitWidth(), b.getBitWidth());
    
    // Check for overflow before multiplication
    int64_t tbbMax = (bits == 8) ? 127 : (bits == 16) ? 32767 : (bits == 32) ? 2147483647 : 9223372036854775807LL;
    
    if (aVal != 0 && bVal != 0) {
        // Check if multiplication would overflow
        if (std::abs(aVal) > tbbMax / std::abs(bVal)) {
            return ComptimeValue::makeERR("tbb" + std::to_string(bits), bits);
        }
    }
    
    int64_t result = aVal * bVal;
    int64_t tbbMin = -tbbMax;
    
    if (result > tbbMax || result < tbbMin) {
        return ComptimeValue::makeERR("tbb" + std::to_string(bits), bits);
    }
    
    return ComptimeValue::makeTBB(result, "tbb" + std::to_string(bits), bits);
}

ComptimeValue ConstEvaluator::tbbDiv(const ComptimeValue& a, const ComptimeValue& b) {
    if (a.isERR() || b.isERR()) {
        int bits = std::max(a.getBitWidth(), b.getBitWidth());
        return ComptimeValue::makeERR("tbb" + std::to_string(bits), bits);
    }
    
    int64_t bVal = b.getInt();
    if (bVal == 0) {
        // Division by zero returns ERR
        int bits = std::max(a.getBitWidth(), b.getBitWidth());
        return ComptimeValue::makeERR("tbb" + std::to_string(bits), bits);
    }
    
    int64_t aVal = a.getInt();
    int64_t result = aVal / bVal;
    int bits = std::max(a.getBitWidth(), b.getBitWidth());
    
    return ComptimeValue::makeTBB(result, "tbb" + std::to_string(bits), bits);
}

ComptimeValue ConstEvaluator::tbbMod(const ComptimeValue& a, const ComptimeValue& b) {
    if (a.isERR() || b.isERR()) {
        int bits = std::max(a.getBitWidth(), b.getBitWidth());
        return ComptimeValue::makeERR("tbb" + std::to_string(bits), bits);
    }
    
    int64_t bVal = b.getInt();
    if (bVal == 0) {
        int bits = std::max(a.getBitWidth(), b.getBitWidth());
        return ComptimeValue::makeERR("tbb" + std::to_string(bits), bits);
    }
    
    int64_t aVal = a.getInt();
    int64_t result = aVal % bVal;
    int bits = std::max(a.getBitWidth(), b.getBitWidth());
    
    return ComptimeValue::makeTBB(result, "tbb" + std::to_string(bits), bits);
}

ComptimeValue ConstEvaluator::tbbNeg(const ComptimeValue& a) {
    if (a.isERR()) {
        return ComptimeValue::makeERR(a.getTypeName(), a.getBitWidth());
    }
    
    int64_t aVal = a.getInt();
    int bits = a.getBitWidth();
    
    // Negation is safe in TBB because the range is symmetric
    // -127 negates to 127, 127 negates to -127
    int64_t result = -aVal;
    
    return ComptimeValue::makeTBB(result, a.getTypeName(), bits);
}

// ============================================================================
// Standard Integer Arithmetic
// ============================================================================

ComptimeValue ConstEvaluator::intAdd(const ComptimeValue& a, const ComptimeValue& b) {
    int64_t result = a.getInt() + b.getInt();
    int bits = std::max(a.getBitWidth(), b.getBitWidth());
    return ComptimeValue::makeInteger(result, "int" + std::to_string(bits), bits);
}

ComptimeValue ConstEvaluator::intSub(const ComptimeValue& a, const ComptimeValue& b) {
    int64_t result = a.getInt() - b.getInt();
    int bits = std::max(a.getBitWidth(), b.getBitWidth());
    return ComptimeValue::makeInteger(result, "int" + std::to_string(bits), bits);
}

ComptimeValue ConstEvaluator::intMul(const ComptimeValue& a, const ComptimeValue& b) {
    int64_t result = a.getInt() * b.getInt();
    int bits = std::max(a.getBitWidth(), b.getBitWidth());
    return ComptimeValue::makeInteger(result, "int" + std::to_string(bits), bits);
}

ComptimeValue ConstEvaluator::intDiv(const ComptimeValue& a, const ComptimeValue& b) {
    if (b.getInt() == 0) {
        addError("Division by zero in const evaluation");
        return ComptimeValue();
    }
    int64_t result = a.getInt() / b.getInt();
    int bits = std::max(a.getBitWidth(), b.getBitWidth());
    return ComptimeValue::makeInteger(result, "int" + std::to_string(bits), bits);
}

ComptimeValue ConstEvaluator::intMod(const ComptimeValue& a, const ComptimeValue& b) {
    if (b.getInt() == 0) {
        addError("Modulo by zero in const evaluation");
        return ComptimeValue();
    }
    int64_t result = a.getInt() % b.getInt();
    int bits = std::max(a.getBitWidth(), b.getBitWidth());
    return ComptimeValue::makeInteger(result, "int" + std::to_string(bits), bits);
}

ComptimeValue ConstEvaluator::intNeg(const ComptimeValue& a) {
    int64_t result = -a.getInt();
    return ComptimeValue::makeInteger(result, a.getTypeName(), a.getBitWidth());
}

// ============================================================================
// Float Arithmetic
// ============================================================================

ComptimeValue ConstEvaluator::floatAdd(const ComptimeValue& a, const ComptimeValue& b) {
    double result = a.getFloat() + b.getFloat();
    return ComptimeValue::makeFloat(result, "flt64");
}

ComptimeValue ConstEvaluator::floatSub(const ComptimeValue& a, const ComptimeValue& b) {
    double result = a.getFloat() - b.getFloat();
    return ComptimeValue::makeFloat(result, "flt64");
}

ComptimeValue ConstEvaluator::floatMul(const ComptimeValue& a, const ComptimeValue& b) {
    double result = a.getFloat() * b.getFloat();
    return ComptimeValue::makeFloat(result, "flt64");
}

ComptimeValue ConstEvaluator::floatDiv(const ComptimeValue& a, const ComptimeValue& b) {
    if (b.getFloat() == 0.0) {
        addError("Division by zero in const evaluation");
        return ComptimeValue();
    }
    double result = a.getFloat() / b.getFloat();
    return ComptimeValue::makeFloat(result, "flt64");
}

ComptimeValue ConstEvaluator::floatNeg(const ComptimeValue& a) {
    double result = -a.getFloat();
    return ComptimeValue::makeFloat(result, a.getTypeName());
}

// ============================================================================
// Comparison & Logical Operations
// ============================================================================

ComptimeValue ConstEvaluator::compare(const ComptimeValue& a, const ComptimeValue& b, const std::string& op) {
    bool result = false;
    
    if (a.isInteger() || a.isTBB()) {
        int64_t aVal = a.getInt();
        int64_t bVal = b.getInt();
        
        if (op == "==") result = (aVal == bVal);
        else if (op == "!=") result = (aVal != bVal);
        else if (op == "<") result = (aVal < bVal);
        else if (op == "<=") result = (aVal <= bVal);
        else if (op == ">") result = (aVal > bVal);
        else if (op == ">=") result = (aVal >= bVal);
    } else if (a.isFloat()) {
        double aVal = a.getFloat();
        double bVal = b.getFloat();
        
        if (op == "==") result = (aVal == bVal);
        else if (op == "!=") result = (aVal != bVal);
        else if (op == "<") result = (aVal < bVal);
        else if (op == "<=") result = (aVal <= bVal);
        else if (op == ">") result = (aVal > bVal);
        else if (op == ">=") result = (aVal >= bVal);
    } else if (a.isBool()) {
        bool aVal = a.getBool();
        bool bVal = b.getBool();
        
        if (op == "==") result = (aVal == bVal);
        else if (op == "!=") result = (aVal != bVal);
    }
    
    return ComptimeValue::makeBool(result);
}

ComptimeValue ConstEvaluator::logicalAnd(const ComptimeValue& a, const ComptimeValue& b) {
    if (!a.isBool() || !b.isBool()) {
        addError("Logical AND requires boolean operands");
        return ComptimeValue();
    }
    return ComptimeValue::makeBool(a.getBool() && b.getBool());
}

ComptimeValue ConstEvaluator::logicalOr(const ComptimeValue& a, const ComptimeValue& b) {
    if (!a.isBool() || !b.isBool()) {
        addError("Logical OR requires boolean operands");
        return ComptimeValue();
    }
    return ComptimeValue::makeBool(a.getBool() || b.getBool());
}

ComptimeValue ConstEvaluator::logicalNot(const ComptimeValue& a) {
    if (!a.isBool()) {
        addError("Logical NOT requires boolean operand");
        return ComptimeValue();
    }
    return ComptimeValue::makeBool(!a.getBool());
}

// ============================================================================
// Scope Management
// ============================================================================

void ConstEvaluator::pushScope() {
    scopeStack.push_back(std::map<std::string, ComptimeValue>());
}

void ConstEvaluator::popScope() {
    if (!scopeStack.empty()) {
        scopeStack.pop_back();
    }
}

void ConstEvaluator::defineConstant(const std::string& name, const ComptimeValue& value) {
    if (scopeStack.empty()) {
        // Global scope
        constants[name] = value;
    } else {
        // Local scope
        scopeStack.back()[name] = value;
    }
}

ComptimeValue ConstEvaluator::lookupConstant(const std::string& name) {
    // Check local scopes (innermost first)
    for (auto it = scopeStack.rbegin(); it != scopeStack.rend(); ++it) {
        auto found = it->find(name);
        if (found != it->end()) {
            return found->second;
        }
    }
    
    // Check global scope
    auto found = constants.find(name);
    if (found != constants.end()) {
        return found->second;
    }
    
    addError("Undefined constant: " + name);
    return ComptimeValue();
}

// ============================================================================
// Resource Management
// ============================================================================

void ConstEvaluator::resetLimits() {
    instructionCount = 0;
    stackDepth = 0;
    virtualHeapSize = 0;
}

bool ConstEvaluator::checkInstructionLimit() {
    if (instructionCount >= instructionLimit) {
        addError("Const evaluation exceeded instruction limit of " + 
                 std::to_string(instructionLimit));
        return false;
    }
    return true;
}

bool ConstEvaluator::checkStackDepth() {
    if (stackDepth >= stackDepthLimit) {
        addError("Const evaluation exceeded stack depth limit of " + 
                 std::to_string(stackDepthLimit));
        return false;
    }
    return true;
}

bool ConstEvaluator::checkHeapSize(size_t additionalBytes) {
    if (virtualHeapSize + additionalBytes > virtualHeapLimit) {
        addError("Const evaluation exceeded heap size limit of " + 
                 std::to_string(virtualHeapLimit) + " bytes");
        return false;
    }
    return true;
}

void ConstEvaluator::incrementInstructions() {
    instructionCount++;
    checkInstructionLimit();
}

void ConstEvaluator::addError(const std::string& msg) {
    errors.push_back(msg);
}

// ============================================================================
// Virtual Heap Operations (Stub - to be implemented in Step 5)
// ============================================================================

ComptimeValue ConstEvaluator::allocate(const std::string& typeName, size_t count, bool isWild) {
    (void)typeName; // unused for now
    (void)count;    // unused for now
    (void)isWild;   // unused for now
    // TODO: Implement virtual heap allocation
    addError("Virtual heap allocation not yet implemented");
    return ComptimeValue();
}

void ConstEvaluator::deallocate(void* ptr) {
    (void)ptr; // unused for now
    // TODO: Implement virtual heap deallocation
}

ComptimeValue ConstEvaluator::dereference(const ComptimeValue& ptr) {
    (void)ptr; // unused for now
    // TODO: Implement pointer dereference
    addError("Pointer dereference not yet implemented");
    return ComptimeValue();
}

void ConstEvaluator::store(const ComptimeValue& ptr, const ComptimeValue& value) {
    (void)ptr;   // unused for now
    (void)value; // unused for now
    // TODO: Implement store through pointer
}

} // namespace sema
} // namespace aria
