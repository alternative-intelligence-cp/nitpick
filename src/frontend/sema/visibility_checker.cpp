#include "frontend/sema/visibility_checker.h"
#include <sstream>

namespace aria {
namespace sema {

VisibilityChecker::VisibilityChecker(ModuleTable* moduleTable)
    : moduleTable(moduleTable) {
}

bool VisibilityChecker::checkAccess(const Symbol* symbol,
                                     const Module* symbolModule,
                                     const Module* currentModule,
                                     int accessLine,
                                     int accessColumn) {
    if (!symbol || !symbolModule || !currentModule) {
        errors.push_back("Internal error: null parameter in visibility check");
        return false;
    }
    
    // Get visibility level of the symbol
    VisibilityLevel visibility = getVisibility(symbol);
    
    // Check if access is allowed based on visibility rules
    bool allowed = isAccessAllowed(visibility, symbolModule, currentModule);
    
    if (!allowed) {
        reportAccessError(symbol->name, symbolModule, currentModule, 
                         accessLine, accessColumn);
    }
    
    return allowed;
}

bool VisibilityChecker::isAccessAllowed(VisibilityLevel visibility,
                                         const Module* symbolModule,
                                         const Module* currentModule) const {
    // Algorithm from research_028 Section 5.3
    
    switch (visibility) {
        case VisibilityLevel::PUBLIC:
            // Public symbols are accessible from anywhere
            return true;
            
        case VisibilityLevel::PRIVATE:
            // Private symbols only accessible within the same module
            return symbolModule == currentModule;
            
        case VisibilityLevel::PACKAGE:
            // Package-visible symbols accessible within same compilation unit
            return isSamePackage(symbolModule, currentModule);
            
        case VisibilityLevel::SUPER:
            // Super-visible symbols accessible to parent module only
            return isParentModule(currentModule, symbolModule);
            
        default:
            return false;
    }
}

VisibilityLevel VisibilityChecker::getVisibility(const Symbol* symbol) const {
    if (!symbol) {
        return VisibilityLevel::PRIVATE;
    }
    
    // Current implementation: isPublic flag maps to PUBLIC/PRIVATE
    // Future: Parse pub(package), pub(super) modifiers from AST
    if (symbol->isPublic) {
        return VisibilityLevel::PUBLIC;
    }
    
    // Default: private-by-default policy (research_028 Section 5)
    return VisibilityLevel::PRIVATE;
}

bool VisibilityChecker::isSamePackage(const Module* module1, const Module* module2) const {
    if (!module1 || !module2) {
        return false;
    }
    
    // Package detection: modules sharing the same top-level path component
    // are in the same package. e.g., "std.io" and "std.net" share package "std".
    // When aria.toml package manifests are available (research_028 Section 8.1),
    // this will be replaced with explicit package_id comparison.
    const std::string& path1 = module1->getPath();
    const std::string& path2 = module2->getPath();
    
    // Use getFullPath() for hierarchical comparison
    std::string full1 = module1->getFullPath();
    std::string full2 = module2->getFullPath();
    
    // Extract top-level package name (before first '.')
    auto dot1 = full1.find('.');
    auto dot2 = full2.find('.');
    std::string pkg1 = (dot1 != std::string::npos) ? full1.substr(0, dot1) : full1;
    std::string pkg2 = (dot2 != std::string::npos) ? full2.substr(0, dot2) : full2;
    
    // Same top-level module = same package
    if (pkg1 == pkg2 && !pkg1.empty()) {
        return true;
    }
    
    // Fallback: if both modules share a common filesystem path prefix
    // (for projects without hierarchical module names)
    if (!path1.empty() && !path2.empty()) {
        // Find common directory prefix
        auto lastSlash1 = path1.rfind('/');
        auto lastSlash2 = path2.rfind('/');
        std::string dir1 = (lastSlash1 != std::string::npos) ? path1.substr(0, lastSlash1) : "";
        std::string dir2 = (lastSlash2 != std::string::npos) ? path2.substr(0, lastSlash2) : "";
        if (!dir1.empty() && dir1 == dir2) {
            return true;
        }
    }
    
    return false;
}

bool VisibilityChecker::isParentModule(const Module* child, const Module* parent) const {
    if (!child || !parent) {
        return false;
    }
    
    // Check if parent is an ancestor of child in module hierarchy
    const Module* current = child->getParent();
    while (current) {
        if (current == parent) {
            return true;
        }
        current = current->getParent();
    }
    
    return false;
}

void VisibilityChecker::reportAccessError(const std::string& symbolName,
                                          const Module* symbolModule,
                                          const Module* currentModule,
                                          int line,
                                          int column) {
    std::ostringstream oss;
    
    // Error E002 from research_028 Section 5.3
    oss << "Error E002 at line " << line << ", column " << column << ":\n";
    oss << "  Cannot access private symbol '" << symbolName << "'\n";
    oss << "  Symbol is defined in module: " << symbolModule->getPath() << "\n";
    oss << "  Current module: " << currentModule->getPath() << "\n";
    oss << "  \n";
    oss << "  Help: Symbol '" << symbolName << "' is not public.\n";
    oss << "        To make it accessible, add 'pub' modifier:\n";
    oss << "        pub func:" << symbolName << " = ...\n";
    oss << "        pub " << symbolName << ":type = ...";
    
    errors.push_back(oss.str());
}

std::string VisibilityChecker::visibilityToString(VisibilityLevel visibility) const {
    switch (visibility) {
        case VisibilityLevel::PRIVATE:
            return "private";
        case VisibilityLevel::PUBLIC:
            return "public";
        case VisibilityLevel::PACKAGE:
            return "pub(package)";
        case VisibilityLevel::SUPER:
            return "pub(super)";
        default:
            return "unknown";
    }
}

} // namespace sema
} // namespace aria
