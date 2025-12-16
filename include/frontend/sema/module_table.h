#ifndef ARIA_FRONTEND_SEMA_MODULE_TABLE_H
#define ARIA_FRONTEND_SEMA_MODULE_TABLE_H

#include "frontend/sema/symbol_table.h"
#include <string>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace aria {
namespace sema {

// Forward declarations
class ModuleTable;
class Module;

/**
 * Visibility levels for exported symbols
 * Based on research_028 Section 5.1
 */
enum class Visibility {
    PRIVATE,       // Default - visible only within module
    PUBLIC,        // pub - visible to all importers
    PACKAGE,       // pub(package) - visible within same compilation unit
    SUPER          // pub(super) - visible to parent module
};

/**
 * Represents an import declaration (use statement)
 * Based on research_028 Section 3
 */
struct Import {
    std::string path;           // Import path (e.g., "std.io" or "./file.aria")
    std::string alias;          // Alias if using 'as' keyword (empty if none)
    bool isWildcard;            // True if using '*' wildcard
    std::vector<std::string> selectiveItems;  // Items imported with {item1, item2}
    int line;                   // Source location
    int column;
    
    Import(const std::string& path, int line, int column)
        : path(path), alias(""), isWildcard(false), line(line), column(column) {}
};

/**
 * Represents a module in the program
 * Based on research_028 Section 4
 * 
 * Each module has:
 * - A unique name and path
 * - Its own symbol table for local symbols
 * - Public exports (symbols visible to importers)
 * - Import declarations
 * - Parent/child relationships for submodules
 */
class Module {
public:
    Module(const std::string& name, const std::string& path, Module* parent = nullptr);
    
    // Module identification
    const std::string& getName() const { return name; }
    const std::string& getPath() const { return path; }
    std::string getFullPath() const;  // Returns hierarchical path (e.g., "std.io.file")
    
    // Parent/child relationships
    Module* getParent() const { return parent; }
    void addSubmodule(std::unique_ptr<Module> submodule);
    Module* getSubmodule(const std::string& name) const;
    const std::vector<std::unique_ptr<Module>>& getSubmodules() const { return submodules; }
    
    // Symbol table access
    SymbolTable* getSymbolTable() { return &symbolTable; }
    const SymbolTable* getSymbolTable() const { return &symbolTable; }
    
    // Import management
    void addImport(const Import& import);
    const std::vector<Import>& getImports() const { return imports; }
    
    // Export management (pub symbols)
    void exportSymbol(const std::string& name, Symbol* symbol, Visibility visibility = Visibility::PUBLIC);
    Symbol* lookupExport(const std::string& name) const;
    bool isExported(const std::string& name) const;
    Visibility getExportVisibility(const std::string& name) const;
    
    // Re-export support (pub use pattern from research_028 Section 5.2)
    void reexportSymbol(const std::string& name, Symbol* symbol, Visibility visibility = Visibility::PUBLIC);
    
    // Module state
    bool isFullyResolved() const { return fullyResolved; }
    void markResolved() { fullyResolved = true; }
    
    std::string toString(int indent = 0) const;
    
private:
    std::string name;           // Module name (e.g., "io")
    std::string path;           // File system path
    Module* parent;             // Parent module (nullptr for root)
    
    std::vector<std::unique_ptr<Module>> submodules;  // Child modules
    SymbolTable symbolTable;    // Local symbol table
    std::vector<Import> imports;  // Import declarations
    
    // Exported symbols with visibility
    struct ExportEntry {
        Symbol* symbol;
        Visibility visibility;
        bool isReexport;  // True if this is a pub use re-export
    };
    std::unordered_map<std::string, ExportEntry> exports;
    
    bool fullyResolved;  // True when all imports are resolved
};

/**
 * Module Table - manages all modules in the program
 * Based on research_028 Section 7
 * 
 * Responsibilities:
 * - Module registration and lookup
 * - Import resolution
 * - Circular dependency detection
 * - Visibility enforcement
 */
class ModuleTable {
public:
    ModuleTable();
    
    // Module management
    Module* createModule(const std::string& name, const std::string& path, Module* parent = nullptr);
    Module* getModule(const std::string& fullPath) const;
    Module* getRootModule() const { return rootModule.get(); }
    
    // Import resolution (research_028 Section 3.2)
    Module* resolveImport(const std::string& importPath, Module* fromModule);
    Symbol* resolveImportedSymbol(const std::string& symbolName, Module* fromModule);
    
    // Visibility checking (research_028 Section 5.3)
    bool canAccess(Symbol* symbol, Module* fromModule) const;
    
    // Circular dependency detection (research_028 Section 3.3)
    bool hasCircularDependency(Module* module);
    
    // Error handling
    bool hasErrors() const { return !errors.empty(); }
    const std::vector<std::string>& getErrors() const { return errors; }
    void clearErrors() { errors.clear(); }
    
    std::string toString() const;
    
private:
    std::unique_ptr<Module> rootModule;  // Root module (global scope)
    std::unordered_map<std::string, Module*> moduleRegistry;  // Fast lookup by full path
    std::vector<std::string> errors;
    
    // Circular dependency detection state
    std::unordered_set<Module*> visitedModules;
    std::unordered_set<Module*> recursionStack;
    
    void error(const std::string& message);
    bool checkCircularDependencyRecursive(Module* module);
};

} // namespace sema
} // namespace aria

#endif // ARIA_FRONTEND_SEMA_MODULE_TABLE_H
