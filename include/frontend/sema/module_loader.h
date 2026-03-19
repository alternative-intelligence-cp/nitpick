#ifndef ARIA_FRONTEND_SEMA_MODULE_LOADER_H
#define ARIA_FRONTEND_SEMA_MODULE_LOADER_H

#include "frontend/sema/module_resolver.h"
#include "frontend/sema/module_table.h"
#include "frontend/ast/stmt.h"
#include <string>
#include <memory>
#include <unordered_map>
#include <vector>

namespace aria {
namespace sema {

// Forward declarations
class Lexer;
class Parser;

/**
 * Module Loading State
 */
enum class ModuleState {
    NOT_LOADED,     // Module hasn't been loaded yet
    LOADING,        // Currently being parsed/loaded (for cycle detection)
    PARSED,         // AST has been created
    RESOLVED,       // Imports have been resolved
    CHECKED,        // Type checking complete
    FULLY_LOADED    // Ready for use
};

/**
 * Loaded Module Container
 * Based on research_module_loading_system Section 3.2
 */
struct LoadedModule {
    std::string canonicalPath;              // Absolute filesystem path
    std::unique_ptr<ProgramNode> ast;       // Parsed AST
    std::unique_ptr<Module> moduleInfo;     // Symbol table and exports
    ModuleState state;                      // Current loading state
    std::vector<std::string> dependencies;  // Resolved dependency paths
    
    LoadedModule(const std::string& path)
        : canonicalPath(path), state(ModuleState::NOT_LOADED) {}
};

/**
 * ModuleLoader - Loads and manages module dependencies
 * Based on research_module_loading_system Sections 3 and 6
 * 
 * Implements the recursive loading pipeline:
 * 1. Resolve logical path to filesystem path (via ModuleResolver)
 * 2. Check cache - return if already loaded
 * 3. Check loading stack - error if circular dependency
 * 4. Parse the file into AST
 * 5. Discover imports in AST
 * 6. Recursively load all dependencies
 * 7. Perform type checking
 * 8. Populate export table
 * 9. Mark as FULLY_LOADED
 */
class ModuleLoader {
public:
    /**
     * Constructor
     * @param rootPath Project root directory
     */
    explicit ModuleLoader(const std::string& rootPath);
    
    /**
     * Load a module and all its dependencies
     * This is the main entry point for module loading
     * 
     * @param logicalPath Module path (e.g., "std.io" or "./file.aria")
     * @param fromPath Path of the file making the import (for relative resolution)
     * @return Loaded module, or nullptr on error
     */
    LoadedModule* loadModule(const std::string& logicalPath, 
                              const std::string& fromPath = "");
    
    /**
     * Load a module by UseStmt
     * Wrapper around loadModule that extracts the path from the AST node
     * 
     * @param useStmt The use statement to process
     * @param fromPath Path of the file containing this use statement
     * @return Loaded module, or nullptr on error
     */
    LoadedModule* loadModule(const UseStmt* useStmt, const std::string& fromPath);
    
    /**
     * Get a loaded module by canonical path
     * @param canonicalPath Absolute filesystem path
     * @return Loaded module, or nullptr if not loaded
     */
    LoadedModule* getModule(const std::string& canonicalPath) const;
    
    /**
     * Check if a module is currently being loaded (for cycle detection)
     * @param canonicalPath Absolute filesystem path
     * @return True if module is in the loading stack
     */
    bool isLoading(const std::string& canonicalPath) const;
    
    /**
     * Get the loading stack (for circular dependency error messages)
     * @return Vector of module paths currently being loaded
     */
    const std::vector<std::string>& getLoadingStack() const { 
        return loadingStack; 
    }
    
    /**
     * Get all loaded modules
     * @return Map of canonical path -> loaded module
     */
    const std::unordered_map<std::string, std::unique_ptr<LoadedModule>>& 
    getLoadedModules() const { 
        return moduleCache; 
    }
    
    /**
     * Get module canonical paths in load order (first-loaded first).
     * Using this order for IR generation ensures dependencies are compiled
     * before the modules that depend on them.
     */
    const std::vector<std::string>& getLoadOrder() const {
        return loadOrder;
    }
    
    /**
     * Get the module resolver
     * @return Reference to the resolver
     */
    ModuleResolver& getResolver() { return resolver; }
    const ModuleResolver& getResolver() const { return resolver; }
    
    /**
     * Check if loading has errors
     * @return True if any errors occurred
     */
    bool hasErrors() const { return !errors.empty(); }
    
    /**
     * Get error messages
     * @return Vector of error strings
     */
    const std::vector<std::string>& getErrors() const { return errors; }
    
    /**
     * Clear error messages
     */
    void clearErrors() { errors.clear(); }
    
    /**
     * Print module dependency graph for debugging
     */
    std::string dumpDependencyGraph() const;
    
private:
    /**
     * Internal recursive loader
     * Implements the algorithm from research_module_loading_system Section 3.2
     * 
     * @param canonicalPath Resolved absolute filesystem path
     * @return Loaded module, or nullptr on error
     */
    LoadedModule* loadModuleFile(const std::string& canonicalPath);
    
    /**
     * Parse a source file into an AST
     * @param filePath Absolute path to .aria file
     * @return Parsed program node, or nullptr on error
     */
    std::unique_ptr<ProgramNode> parseFile(const std::string& filePath);
    
    /**
     * Discover all imports in an AST
     * Scans the program node for UseStmt declarations
     * 
     * @param program The program AST
     * @return Vector of UseStmt pointers
     */
    std::vector<UseStmt*> discoverImports(ProgramNode* program);
    
    /**
     * Process a single import (loads dependency)
     * @param useStmt The use statement
     * @param fromPath Path of the file containing the import
     * @return True if successful
     */
    bool processImport(UseStmt* useStmt, const std::string& fromPath);
    
    /**
     * Add an error message
     * @param message Error description
     */
    void addError(const std::string& message);
    
    /**
     * Add an error with location context
     * @param message Error description
     * @param modulePath Path of module where error occurred
     */
    void addError(const std::string& message, const std::string& modulePath);
    
private:
    ModuleResolver resolver;                                // Path resolution
    std::unordered_map<std::string, std::unique_ptr<LoadedModule>> moduleCache;  // Path -> Module
    std::vector<std::string> loadingStack;                  // For cycle detection
    std::vector<std::string> loadOrder;                     // Insertion order (first-loaded first)
    std::vector<std::string> errors;                        // Error messages
    std::string rootPath;                                   // Project root
};

} // namespace sema
} // namespace aria

#endif // ARIA_FRONTEND_SEMA_MODULE_LOADER_H
