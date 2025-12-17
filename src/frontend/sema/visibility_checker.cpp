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
    
    // For now, we consider all modules in the same compilation to be in the same package
    // Future enhancement: Extract package ID from aria.toml or module path
    // research_028 Section 8.1 discusses package manifests
    
    // Simple heuristic: Check if modules share a common root path prefix
    // This would be replaced with proper package_id comparison
    const std::string& path1 = module1->getPath();
    const std::string& path2 = module2->getPath();
    
    // For now, treat all modules as same package
    // TODO: Implement proper package detection based on aria.toml
    return true;
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
