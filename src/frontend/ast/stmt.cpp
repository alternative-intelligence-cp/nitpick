#include "frontend/ast/stmt.h"
#include <sstream>

namespace aria {

std::string VarDeclStmt::toString() const {
    std::ostringstream oss;
    oss << "VarDecl(";
    if (isWild) oss << "wild ";
    if (isConst) oss << "const ";
    if (isStack) oss << "stack ";
    if (isGC) oss << "gc ";
    oss << typeName << ":" << varName;
    if (initializer) {
        oss << " = " << initializer->toString();
    }
    oss << ")";
    return oss.str();
}

std::string FuncDeclStmt::toString() const {
    std::ostringstream oss;
    oss << "FuncDecl(";
    if (isAsync) oss << "async ";
    if (isPublic) oss << "pub ";
    oss << "func:" << funcName;
    
    if (!genericParams.empty()) {
        oss << "<";
        for (size_t i = 0; i < genericParams.size(); ++i) {
            if (i > 0) oss << ", ";
            oss << genericParams[i];
        }
        oss << ">";
    }
    
    oss << " = " << returnType << "(";
    for (size_t i = 0; i < parameters.size(); ++i) {
        if (i > 0) oss << ", ";
        oss << parameters[i]->toString();
    }
    oss << ") { ... })";
    return oss.str();
}

std::string ParameterNode::toString() const {
    std::string result = typeName + ":" + paramName;
    if (defaultValue) {
        result += " = " + defaultValue->toString();
    }
    return result;
}

std::string BlockStmt::toString() const {
    std::ostringstream oss;
    oss << "Block([";
    for (size_t i = 0; i < statements.size(); ++i) {
        if (i > 0) oss << ", ";
        oss << statements[i]->toString();
    }
    oss << "])";
    return oss.str();
}

std::string ExpressionStmt::toString() const {
    return "ExprStmt(" + expression->toString() + ")";
}

std::string ReturnStmt::toString() const {
    if (value) {
        return "Return(" + value->toString() + ")";
    }
    return "Return()";
}

std::string IfStmt::toString() const {
    std::string result = "If(" + condition->toString() + ", " + thenBranch->toString();
    if (elseBranch) {
        result += ", " + elseBranch->toString();
    }
    result += ")";
    return result;
}

std::string WhileStmt::toString() const {
    return "While(" + condition->toString() + ", " + body->toString() + ")";
}

std::string ForStmt::toString() const {
    std::ostringstream oss;
    oss << "For(";
    if (initializer) oss << initializer->toString();
    oss << "; ";
    if (condition) oss << condition->toString();
    oss << "; ";
    if (update) oss << update->toString();
    oss << ", " << body->toString() << ")";
    return oss.str();
}

std::string DeferStmt::toString() const {
    return "Defer(" + expression->toString() + ")";
}

std::string ProgramNode::toString() const {
    std::ostringstream oss;
    oss << "Program([";
    for (size_t i = 0; i < declarations.size(); ++i) {
        if (i > 0) oss << ", ";
        oss << declarations[i]->toString();
    }
    oss << "])";
    return oss.str();
}

} // namespace aria
