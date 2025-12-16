#include "frontend/ast/type.h"
#include <sstream>

namespace aria {

std::string SimpleType::toString() const {
    return typeName;
}

std::string PointerType::toString() const {
    std::ostringstream oss;
    if (baseType) {
        oss << baseType->toString() << "@";  // Aria uses @ for pointers
    } else {
        oss << "unknown@";
    }
    return oss.str();
}

std::string ArrayType::toString() const {
    std::ostringstream oss;
    if (elementType) {
        oss << elementType->toString();
    } else {
        oss << "unknown";
    }
    
    oss << "[";
    if (!isDynamic && sizeExpr) {
        oss << sizeExpr->toString();
    }
    oss << "]";
    
    return oss.str();
}

std::string GenericType::toString() const {
    std::ostringstream oss;
    oss << baseName << "<";
    
    for (size_t i = 0; i < typeArgs.size(); i++) {
        if (i > 0) oss << ", ";
        if (typeArgs[i]) {
            oss << typeArgs[i]->toString();
        }
    }
    
    oss << ">";
    return oss.str();
}

std::string FunctionType::toString() const {
    std::ostringstream oss;
    oss << "func(";
    
    for (size_t i = 0; i < paramTypes.size(); i++) {
        if (i > 0) oss << ", ";
        if (paramTypes[i]) {
            oss << paramTypes[i]->toString();
        }
    }
    
    oss << ") -> ";
    if (returnType) {
        oss << returnType->toString();
    } else {
        oss << "void";
    }
    
    return oss.str();
}

} // namespace aria
