#include "frontend/ast/expr.h"
#include <sstream>

namespace aria {

std::string LiteralExpr::toString() const {
    std::ostringstream oss;
    oss << "Literal(";
    
    // If we have a raw value string (high-precision literal), show that instead
    if (!raw_value_string.empty()) {
        oss << "raw=\"" << raw_value_string << "\"";
    } else {
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
    }
    
    // Show explicit type if present (Zero Implicit Conversion Policy)
    if (!explicit_type.empty()) {
        oss << ":" << explicit_type;
    }
    
    oss << ")";
    return oss.str();
}

std::string IdentifierExpr::toString() const {
    return "Identifier(" + name + ")";
}

std::string TemplateLiteralExpr::toString() const {
    std::ostringstream oss;
    oss << "TemplateLiteral(`";
    
    // Alternate between parts and interpolations
    for (size_t i = 0; i < parts.size(); ++i) {
        oss << parts[i];
        if (i < interpolations.size()) {
            oss << "&{" << interpolations[i]->toString() << "}";
        }
    }
    
    oss << "`)";
    return oss.str();
}

std::string BinaryExpr::toString() const {
    return "Binary(" + left->toString() + " " + aria::frontend::tokenTypeToString(op.type) + " " + right->toString() + ")";
}

std::string RangeExpr::toString() const {
    std::string op = isExclusive ? "..." : "..";
    return "Range(" + start->toString() + " " + op + " " + end->toString() + ")";
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

std::string ObjectLiteralExpr::toString() const {
    std::ostringstream oss;
    if (!type_name.empty()) {
        oss << type_name;
    } else {
        oss << "obj";
    }
    oss << "{ ";
    for (size_t i = 0; i < fields.size(); ++i) {
        if (i > 0) oss << ", ";
        oss << fields[i].name << ": " << fields[i].value->toString();
    }
    oss << " }";
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

std::string AwaitExpr::toString() const {
    std::ostringstream oss;
    oss << "Await(" << operand->toString() << ")";
    return oss.str();
}

std::string MoveExpr::toString() const {
    std::ostringstream oss;
    oss << "Move(" << variableName << ")";
    return oss.str();
}

std::string CastExpr::toString() const {
    std::ostringstream oss;
    if (isUnchecked) {
        oss << "@cast_unchecked<" << targetType << ">(";
    } else {
        oss << "@cast<" << targetType << ">(";
    }
    oss << expression->toString() << ")";
    return oss.str();
}

std::string UnwrapExpr::toString() const {
    std::ostringstream oss;
    oss << "Unwrap(" << result->toString() << " ? " << defaultValue->toString() << ")";
    return oss.str();
}

std::string VectorConstructorExpr::toString() const {
    std::ostringstream oss;
    oss << "vec" << dimension << "(";
    for (size_t i = 0; i < components.size(); ++i) {
        if (i > 0) oss << ", ";
        oss << components[i]->toString();
    }
    oss << ")";
    return oss.str();
}

std::string ComptimeExpr::toString() const {
    return "comptime(" + expr->toString() + ")";
}

std::string MacroInvocationExpr::toString() const {
    std::ostringstream oss;
    oss << macroName << "!(";
    for (size_t i = 0; i < arguments.size(); ++i) {
        if (i > 0) oss << ", ";
        oss << (arguments[i] ? arguments[i]->toString() : "null");
    }
    oss << ")";
    return oss.str();
}

std::string SpreadExpr::toString() const {
    return "..^" + (operand ? operand->toString() : "null");
}

} // namespace aria
