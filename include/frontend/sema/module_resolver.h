#ifndef ARIA_FRONTEND_SEMA_MODULE_RESOLVER_H
#define ARIA_FRONTEND_SEMA_MODULE_RESOLVER_H

#include "frontend/sema/module_table.h"
#include "frontend/ast/stmt.h"
#include <string>
#include <vector>
#include <unordered_set>
#include <filesystem>

namespace aria {
namespace sema {

/**
 * ModuleResolver - Handles module discovery and import resolution
 * Based on research_028 Section 3.2 (Import Resolution Algorithm)
 * 
 * Responsibilities:
 * - Resolve use statements to module files
 * - Handle relative paths (./file.aria, ../other.aria)
 * - Handle absolute paths (/usr/lib/aria/module.aria)
 * - Handle logical paths (std.io, math.calc)
 * - Support selective imports: use std.{array, map}
 * - Support wildcard imports: use math.*
 * - Support aliasing: use file as alias
 * - Detect circular dependencies
 * - Search ARIA_PATH for module resolution
 */
class ModuleResolver {
public:
    /**
     * Constructor
     * @param rootPath The root directory of the project (where main.aria or lib.aria is)
     */
    explicit ModuleResolver(const std::string& rootPath);
    
    /**
     * Resolve a use statement to a module file path
     * Implements the algorithm from research_028 Section 3.2
     * 
     * @param useStmt The use statement to resolve
     * @param currentModulePath Path of the module containing this use statement
     * @return Resolved file path, or empty string if not found
     */
    std::string resolveImport(const UseStmt* useStmt, const std::string& currentModulePath);
    
    /**
     * Resolve a module name to a file path
     * Handles both file paths (./file.aria) and logical paths (std.io)
     * 
     * @param path The module path components (e.g., ["std", "io"])
     * @param isFilePath True if this is a quoted file path
     * @param currentModulePath Path of the module making the import
     * @return Resolved file path, or empty string if not found
     */
    std::string resolveModulePath(const std::vector<std::string>& path,
                                   bool isFilePath,
                                   const std::string& currentModulePath);
    
    /**
     * Check if we're currently loading a module (for circular dependency detection)
     * @param modulePath The module file path to check
     * @return True if the module is currently being loaded
     */
    bool isCurrentlyLoading(const std::string& modulePath) const;
    
    /**
     * Mark a module as being loaded (push onto loading stack)
     * @param modulePath The module file path
     */
    void beginLoading(const std::string& modulePath);
    
    /**
     * Mark a module as finished loading (pop from loading stack)
     * @param modulePath The module file path
     */
    void endLoading(const std::string& modulePath);
    
    /**
     * Get the current loading stack (for circular dependency error messages)
     * @return Vector of module paths currently being loaded
     */
    const std::vector<std::string>& getLoadingStack() const { return loadingStack; }
    
    /**
     * Add a search path for module resolution
     * These paths are checked after the project root
     * Equivalent to adding to ARIA_PATH
     * 
     * @param path Directory path to search
     */
    void addSearchPath(const std::string& path);
    
    /**
     * Get all search paths (includes root path + added paths + ARIA_PATH)
     * @return Vector of directory paths to search for modules
     */
    std::vector<std::string> getSearchPaths() const;
    
    /**
     * Check if a file exists and is a valid Aria source file
     * @param path File path to check
     * @return True if file exists and has .aria extension
     */
    static bool isValidAriaFile(const std::string& path);
    
    /**
     * Convert a logical module path to a file system path
     * Example: ["std", "io"] -> "std/io.aria" or "std/io/mod.aria"
     * 
     * @param components Module path components
     * @param baseDir Base directory to resolve from
     * @return File system path, or empty string if not found
     */
    std::string logicalToFilePath(const std::vector<std::string>& components,
                                   const std::string& baseDir);
    
    /**
     * Normalize a file path (resolve .., ., make absolute)
     * @param path Path to normalize
     * @param relativeTo Base path for relative resolution
     * @return Normalized absolute path
     */
    static std::string normalizePath(const std::string& path, 
                                      const std::string& relativeTo = "");
    
    /**
     * Extract directory from file path
     * @param filePath File path
     * @return Directory containing the file
     */
    static std::string getDirectory(const std::string& filePath);
    
    /**
     * Check if path is absolute
     * @param path Path to check
     * @return True if path is absolute (starts with / or drive letter)
     */
    static bool isAbsolutePath(const std::string& path);
    
    /**
     * Check if path is relative
     * @param path Path to check
     * @return True if path starts with ./ or ../
     */
    static bool isRelativePath(const std::string& path);
    
    /**
     * Get errors accumulated during resolution
     * @return Vector of error messages
     */
    const std::vector<std::string>& getErrors() const { return errors; }
    
    /**
     * Check if any errors occurred
     * @return True if there are errors
     */
    bool hasErrors() const { return !errors.empty(); }
    
    /**
     * Clear accumulated errors
     */
    void clearErrors() { errors.clear(); }

private:
    std::string rootPath;                      // Project root directory
    std::vector<std::string> searchPaths;      // Additional search paths
    std::vector<std::string> loadingStack;     // Stack for circular dependency detection
    std::unordered_set<std::string> loadingSet; // Set for O(1) lookup
    std::vector<std::string> errors;           // Accumulated error messages
    
    /**
     * Try to find a module file given a base path and module components
     * Checks both file.aria and file/mod.aria patterns
     * 
     * @param baseDir Base directory to search in
     * @param components Module path components
     * @return File path if found, empty string otherwise
     */
    std::string tryFindModule(const std::string& baseDir, 
                              const std::vector<std::string>& components);
    
    /**
     * Build a file path from directory and components
     * @param baseDir Base directory
     * @param components Path components
     * @param extension File extension to add
     * @return Constructed path
     */
    std::string buildPath(const std::string& baseDir,
                          const std::vector<std::string>& components,
                          const std::string& extension = ".aria");
    
    /**
     * Add an error message
     * @param message Error description
     */
    void addError(const std::string& message);
    
    /**
     * Read ARIA_PATH environment variable
     * @return Vector of paths from ARIA_PATH
     */
    static std::vector<std::string> readAriaPath();
};

} // namespace sema
} // namespace aria

#endif // ARIA_FRONTEND_SEMA_MODULE_RESOLVER_H
