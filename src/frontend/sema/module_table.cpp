#include "frontend/sema/module_table.h"
#include <sstream>
#include <algorithm>

namespace aria {
namespace sema {

// ============================================================================
// Module Implementation
// ============================================================================

Module::Module(const std::string& name, const std::string& path, Module* parent)
    : name(name), path(path), parent(parent), fullyResolved(false) {}

std::string Module::getFullPath() const {
    if (!parent) {
        return name;  // Root module
    }
    return parent->getFullPath() + "." + name;
}

void Module::addSubmodule(std::unique_ptr<Module> submodule) {
    submodules.push_back(std::move(submodule));
}

Module* Module::getSubmodule(const std::string& name) const {
    for (const auto& submodule : submodules) {
        if (submodule->getName() == name) {
            return submodule.get();
        }
    }
    return nullptr;
}

void Module::addImport(const Import& import) {
    imports.push_back(import);
}

void Module::exportSymbol(const std::string& name, Symbol* symbol, Visibility visibility) {
    exports[name] = ExportEntry{symbol, visibility, false};
}

void Module::reexportSymbol(const std::string& name, Symbol* symbol, Visibility visibility) {
    exports[name] = ExportEntry{symbol, visibility, true};
}

Symbol* Module::lookupExport(const std::string& name) const {
    auto it = exports.find(name);
    if (it != exports.end()) {
        return it->second.symbol;
    }
    return nullptr;
}

bool Module::isExported(const std::string& name) const {
    return exports.find(name) != exports.end();
}

Visibility Module::getExportVisibility(const std::string& name) const {
    auto it = exports.find(name);
    if (it != exports.end()) {
        return it->second.visibility;
    }
    return Visibility::PRIVATE;
}

std::string Module::toString(int indent) const {
    std::stringstream ss;
    std::string indentStr(indent * 2, ' ');
    
    ss << indentStr << "Module: " << name << " (" << path << ")\n";
    ss << indentStr << "  Full Path: " << getFullPath() << "\n";
    
    // Imports
    if (!imports.empty()) {
        ss << indentStr << "  Imports:\n";
        for (const auto& import : imports) {
            ss << indentStr << "    - " << import.path;
            if (!import.alias.empty()) {
                ss << " as " << import.alias;
            }
            if (import.isWildcard) {
                ss << " (wildcard)";
            }
            if (!import.selectiveItems.empty()) {
                ss << " {";
                for (size_t i = 0; i < import.selectiveItems.size(); i++) {
                    if (i > 0) ss << ", ";
                    ss << import.selectiveItems[i];
                }
                ss << "}";
            }
            ss << "\n";
        }
    }
    
    // Exports
    if (!exports.empty()) {
        ss << indentStr << "  Exports:\n";
        for (const auto& pair : exports) {
            ss << indentStr << "    - " << pair.first;
            switch (pair.second.visibility) {
                case Visibility::PUBLIC: ss << " (pub)"; break;
                case Visibility::PACKAGE: ss << " (pub(package))"; break;
                case Visibility::SUPER: ss << " (pub(super))"; break;
                case Visibility::PRIVATE: ss << " (private)"; break;
            }
            if (pair.second.isReexport) {
                ss << " [re-export]";
            }
            ss << "\n";
        }
    }
    
    // Submodules
    if (!submodules.empty()) {
        ss << indentStr << "  Submodules:\n";
        for (const auto& submodule : submodules) {
            ss << submodule->toString(indent + 2);
        }
    }
    
    return ss.str();
}

// ============================================================================
// ModuleTable Implementation
// ============================================================================

ModuleTable::ModuleTable() {
    // Create root module (global scope)
    rootModule = std::make_unique<Module>("root", "<root>", nullptr);
    moduleRegistry["root"] = rootModule.get();
}

Module* ModuleTable::createModule(const std::string& name, const std::string& path, Module* parent) {
    auto module = std::make_unique<Module>(name, path, parent);
    Module* modulePtr = module.get();
    
    // Register module by full path
    std::string fullPath = module->getFullPath();
    moduleRegistry[fullPath] = modulePtr;
    
    // Add as submodule to parent
    if (parent) {
        parent->addSubmodule(std::move(module));
    } else {
        // If no parent specified, add to root
        rootModule->addSubmodule(std::move(module));
    }
    
    return modulePtr;
}

Module* ModuleTable::getModule(const std::string& fullPath) const {
    auto it = moduleRegistry.find(fullPath);
    if (it != moduleRegistry.end()) {
        return it->second;
    }
    return nullptr;
}

Module* ModuleTable::resolveImport(const std::string& importPath, Module* fromModule) {
    // research_028 Section 3.2: Import Resolution Algorithm
    
    // Handle relative imports (starting with ./ or ../)
    if (importPath.find("./") == 0 || importPath.find("../") == 0) {
        // TODO: In Phase 3.2, implement file-based resolution
        // For now, just error
        error("Relative imports not yet implemented: " + importPath);
        return nullptr;
    }
    
    // Handle logical path (e.g., "std.io")
    // Tokenize path by '.'
    std::vector<std::string> segments;
    std::stringstream ss(importPath);
    std::string segment;
    while (std::getline(ss, segment, '.')) {
        segments.push_back(segment);
    }
    
    if (segments.empty()) {
        error("Invalid import path: " + importPath);
        return nullptr;
    }
    
    // Try to resolve from root
    Module* current = rootModule.get();
    for (const auto& seg : segments) {
        current = current->getSubmodule(seg);
        if (!current) {
            error("Module not found: " + importPath + " (failed at segment: " + seg + ")");
            return nullptr;
        }
    }
    
    return current;
}

Symbol* ModuleTable::resolveImportedSymbol(const std::string& symbolName, Module* fromModule) {
    // Search through imports in the current module
    for (const auto& import : fromModule->getImports()) {
        Module* importedModule = resolveImport(import.path, fromModule);
        if (!importedModule) {
            continue;  // Skip failed imports (error already reported)
        }
        
        // Check if this import provides the symbol
        if (import.isWildcard) {
            // Wildcard import - check all exports
            Symbol* symbol = importedModule->lookupExport(symbolName);
            if (symbol && canAccess(symbol, fromModule)) {
                return symbol;
            }
        } else if (!import.selectiveItems.empty()) {
            // Selective import - check if symbol is in the list
            auto it = std::find(import.selectiveItems.begin(), import.selectiveItems.end(), symbolName);
            if (it != import.selectiveItems.end()) {
                Symbol* symbol = importedModule->lookupExport(symbolName);
                if (symbol && canAccess(symbol, fromModule)) {
                    return symbol;
                }
            }
        } else {
            // Regular import - symbol must be qualified with module name or alias
            std::string moduleName = import.alias.empty() ? 
                                    importedModule->getName() : import.alias;
            // This case is handled by qualified lookup (module.symbol)
            // Not applicable for simple symbol lookup
        }
    }
    
    return nullptr;
}

bool ModuleTable::canAccess(Symbol* symbol, Module* fromModule) const {
    // research_028 Section 5.3: Visibility Enforcement
    
    if (!symbol || !symbol->scope) {
        return false;
    }
    
    // Find which module owns this symbol by checking symbol tables
    Module* owningModule = nullptr;
    for (const auto& pair : moduleRegistry) {
        // Check if this module's symbol table contains the symbol's scope
        const SymbolTable* moduleSymbolTable = pair.second->getSymbolTable();
        Scope* moduleScope = moduleSymbolTable->getRootScope();
        
        // Walk up the scope hierarchy from symbol's scope to see if it's in this module
        Scope* currentScope = symbol->scope;
        while (currentScope) {
            if (currentScope == moduleScope) {
                owningModule = pair.second;
                break;
            }
            currentScope = currentScope->getParent();
        }
        
        if (owningModule) {
            break;
        }
    }
    
    if (!owningModule) {
        return false;  // Can't determine ownership
    }
    
    // Check if symbol is exported
    if (!owningModule->isExported(symbol->name)) {
        // Not exported - only accessible from within owning module
        return owningModule == fromModule;
    }
    
    // Check visibility level
    Visibility visibility = owningModule->getExportVisibility(symbol->name);
    
    switch (visibility) {
        case Visibility::PUBLIC:
            return true;  // Accessible from anywhere
            
        case Visibility::PRIVATE:
            return owningModule == fromModule;
            
        case Visibility::PACKAGE:
            // TODO: In Phase 3.2, implement package-level access
            // For now, treat as public
            return true;
            
        case Visibility::SUPER:
            // Accessible from parent module
            return fromModule == owningModule->getParent();
    }
    
    return false;
}

bool ModuleTable::hasCircularDependency(Module* module) {
    // research_028 Section 3.3: Circular Dependency Detection
    
    visitedModules.clear();
    recursionStack.clear();
    
    return checkCircularDependencyRecursive(module);
}

bool ModuleTable::checkCircularDependencyRecursive(Module* module) {
    if (!module) {
        return false;
    }
    
    // If in recursion stack, we found a cycle
    if (recursionStack.find(module) != recursionStack.end()) {
        error("Circular dependency detected: " + module->getFullPath());
        return true;
    }
    
    // If already visited and not in stack, no cycle from this path
    if (visitedModules.find(module) != visitedModules.end()) {
        return false;
    }
    
    // Add to stack and visited
    recursionStack.insert(module);
    visitedModules.insert(module);
    
    // Check all imported modules
    for (const auto& import : module->getImports()) {
        Module* importedModule = resolveImport(import.path, module);
        if (importedModule && checkCircularDependencyRecursive(importedModule)) {
            return true;  // Cycle found
        }
    }
    
    // Remove from recursion stack (backtrack)
    recursionStack.erase(module);
    
    return false;
}

void ModuleTable::error(const std::string& message) {
    errors.push_back(message);
}

std::string ModuleTable::toString() const {
    std::stringstream ss;
    ss << "ModuleTable:\n";
    ss << "  Total Modules: " << moduleRegistry.size() << "\n";
    ss << "\nModule Tree:\n";
    ss << rootModule->toString(1);
    
    if (!errors.empty()) {
        ss << "\nErrors:\n";
        for (const auto& err : errors) {
            ss << "  - " << err << "\n";
        }
    }
    
    return ss.str();
}

} // namespace sema
} // namespace aria
