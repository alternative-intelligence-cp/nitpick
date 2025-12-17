#include "frontend/ast/expr.h"
#include <sstream>

namespace aria {

std::string LiteralExpr::toString() const {
    std::ostringstream oss;
    oss << "Literal(";
    
    std::visit([&oss](auto&& arg) {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, int64_t>) {
            oss << arg;
        } else if constexpr (std::is_same_v<T, double>) {
            oss << arg;
        } else if constexpr (std::is_same_v<T, std::string>) {
            oss << "\"" << arg << "\"";
        } else if constexpr (std::is_same_v<T, bool>) {
            oss << (arg ? "true" : "false");
        } else if constexpr (std::is_same_v<T, std::monostate>) {
            oss << "null";
        }
    }, value);
    
    oss << ")";
    return oss.str();
}

std::string IdentifierExpr::toString() const {
    return "Identifier(" + name + ")";
}

std::string BinaryExpr::toString() const {
    return "Binary(" + left->toString() + " " + aria::frontend::tokenTypeToString(op.type) + " " + right->toString() + ")";
}

std::string UnaryExpr::toString() const {
    return "Unary(" + aria::frontend::tokenTypeToString(op.type) + " " + operand->toString() + ")";
}

std::string CallExpr::toString() const {
    std::ostringstream oss;
    oss << "Call(" << callee->toString();
    
    // Display explicit type arguments if present (turbofish)
    if (!explicitTypeArgs.empty()) {
        oss << "::<";
        for (size_t i = 0; i < explicitTypeArgs.size(); ++i) {
            if (i > 0) oss << ", ";
            oss << explicitTypeArgs[i];
        }
        oss << ">";
    }
    
    oss << ", [";
    for (size_t i = 0; i < arguments.size(); ++i) {
        if (i > 0) oss << ", ";
        oss << arguments[i]->toString();
    }
    oss << "])";
    return oss.str();
}

std::string IndexExpr::toString() const {
    return "Index(" + array->toString() + "[" + index->toString() + "])";
}

std::string MemberAccessExpr::toString() const {
    std::string op = isPointerAccess ? "->" : ".";
    return "MemberAccess(" + object->toString() + op + member + ")";
}

std::string TernaryExpr::toString() const {
    return "Ternary(" + condition->toString() + " ? " + 
           trueValue->toString() + " : " + falseValue->toString() + ")";
}

std::string AssignmentExpr::toString() const {
    return "Assignment(" + target->toString() + " " + 
           aria::frontend::tokenTypeToString(op.type) + " " + value->toString() + ")";
}

std::string ArrayLiteralExpr::toString() const {
    std::ostringstream oss;
    oss << "ArrayLiteral([";
    for (size_t i = 0; i < elements.size(); ++i) {
        if (i > 0) oss << ", ";
        oss << elements[i]->toString();
    }
    oss << "])";
    return oss.str();
}

std::string LambdaExpr::toString() const {
    std::ostringstream oss;
    oss << "Lambda(";
    
    // Parameters
    oss << "params=[";
    for (size_t i = 0; i < parameters.size(); ++i) {
        if (i > 0) oss << ", ";
        oss << parameters[i]->toString();
    }
    oss << "]";
    
    // Return type
    if (!returnTypeName.empty()) {
        oss << ", returnType=" << returnTypeName;
    }
    
    // Captured variables
    if (!capturedVars.empty()) {
        oss << ", captures=[";
        for (size_t i = 0; i < capturedVars.size(); ++i) {
            if (i > 0) oss << ", ";
            oss << capturedVars[i].name;
            switch (capturedVars[i].mode) {
                case CaptureMode::BY_VALUE: oss << "(copy)"; break;
                case CaptureMode::BY_REFERENCE: oss << "(ref)"; break;
                case CaptureMode::BY_MOVE: oss << "(move)"; break;
            }
        }
        oss << "]";
    }
    
    // Async flag
    if (isAsync) {
        oss << ", async";
    }
    
    // Body
    oss << ", body=" << body->toString();
    
    oss << ")";
    return oss.str();
}

} // namespace aria
