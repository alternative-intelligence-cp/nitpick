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
    return "Defer(" + block->toString() + ")";
}

std::string BreakStmt::toString() const {
    if (!label.empty()) {
        return "Break(" + label + ")";
    }
    return "Break";
}

std::string ContinueStmt::toString() const {
    if (!label.empty()) {
        return "Continue(" + label + ")";
    }
    return "Continue";
}

std::string TillStmt::toString() const {
    return "Till(" + limit->toString() + ", " + step->toString() + ", " + body->toString() + ")";
}

std::string LoopStmt::toString() const {
    return "Loop(" + start->toString() + ", " + limit->toString() + ", " + 
           step->toString() + ", " + body->toString() + ")";
}

std::string WhenStmt::toString() const {
    std::string result = "When(" + condition->toString() + ", " + body->toString();
    if (then_block) {
        result += ", then: " + then_block->toString();
    }
    if (end_block) {
        result += ", end: " + end_block->toString();
    }
    result += ")";
    return result;
}

std::string PickCase::toString() const {
    std::ostringstream oss;
    oss << "PickCase(";
    if (!label.empty()) {
        oss << label << ":";
    }
    if (is_unreachable) {
        oss << "(!)" << " { " << body->toString() << " }";
    } else {
        oss << pattern->toString() << " { " << body->toString() << " }";
    }
    oss << ")";
    return oss.str();
}

std::string PickStmt::toString() const {
    std::ostringstream oss;
    oss << "Pick(" << selector->toString() << ", [";
    for (size_t i = 0; i < cases.size(); ++i) {
        if (i > 0) oss << ", ";
        oss << cases[i]->toString();
    }
    oss << "])";
    return oss.str();
}

std::string FallStmt::toString() const {
    return "Fall(" + target_label + ")";
}

std::string UseStmt::toString() const {
    std::ostringstream oss;
    oss << "Use(";
    
    // Path
    if (isFilePath) {
        oss << "\"" << path[0] << "\"";
    } else {
        for (size_t i = 0; i < path.size(); ++i) {
            if (i > 0) oss << ".";
            oss << path[i];
        }
    }
    
    // Items or wildcard
    if (isWildcard) {
        oss << ".*";
    } else if (!items.empty()) {
        oss << ".{";
        for (size_t i = 0; i < items.size(); ++i) {
            if (i > 0) oss << ", ";
            oss << items[i];
        }
        oss << "}";
    }
    
    // Alias
    if (!alias.empty()) {
        oss << " as " << alias;
    }
    
    oss << ")";
    return oss.str();
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
