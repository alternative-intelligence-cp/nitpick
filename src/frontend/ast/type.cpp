#include "frontend/ast/type.h"
#include <sstream>

namespace aria {

std::string SimpleType::toString() const {
    return typeName;
}

std::string PointerType::toString() const {
    std::ostringstream oss;
    if (isErased) {
        // ARIA-P3: ?-> / ?* — type-erased pointer (Aria's honest void*)
        // Internally serialised as "?@" (fat) or "?*" (thin)
        oss << "?" << (isNative ? "@" : "*");
    } else if (baseType) {
        // ARIA-015: @ for native fat pointers, * for FFI thin pointers
        oss << baseType->toString() << (isNative ? "@" : "*");
    } else {
        oss << "unknown" << (isNative ? "@" : "*");
    }
    return oss.str();
}

std::string SafeRefType::toString() const {
    std::ostringstream oss;
    if (baseType) {
        oss << baseType->toString() << "$";  // Aria uses $ for safe references
    } else {
        oss << "unknown$";
    }
    return oss.str();
}

std::string OptionalTypeNode::toString() const {
    std::ostringstream oss;
    if (wrappedType) {
        oss << wrappedType->toString() << "?";  // Aria uses ? for optional types
    } else {
        oss << "unknown?";
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
