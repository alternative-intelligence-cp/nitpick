#ifndef ARIA_FRONTEND_SEMA_VISIBILITY_CHECKER_H
#define ARIA_FRONTEND_SEMA_VISIBILITY_CHECKER_H

#include "frontend/sema/symbol_table.h"
#include "frontend/sema/module_table.h"
#include <string>
#include <vector>

namespace aria {
namespace sema {

/**
 * VisibilityLevel - Defines access control levels for symbols
 * Based on research_028 Section 5.1
 */
enum class VisibilityLevel {
    PRIVATE,        // Visible only within defining module (default)
    PUBLIC,         // Visible to any module that imports (pub)
    PACKAGE,        // Visible within compilation unit only (pub(package))
    SUPER,          // Visible to parent module only (pub(super))
};

/**
 * VisibilityChecker - Enforces visibility/access control rules
 * Based on research_028 Section 5.3
 * 
 * Responsibilities:
 * - Check visibility when accessing symbols from other modules
 * - Enforce private-by-default policy
 * - Generate E002 errors for access violations
 * - Handle visibility modifiers: pub, pub(package), pub(super)
 * 
 * Algorithm (from research_028 Section 5.3):
 * 1. Access: Expression module.item encountered in current_scope
 * 2. Lookup: Symbol item retrieved from module's symbol table
 * 3. Check:
 *    - If item.visibility == PUB: Access Granted
 *    - If item.visibility == PRIVATE:
 *      * If module == current_module: Access Granted
 *      * Else: Access Denied (Error E002)
 *    - If item.visibility == PUB(PACKAGE):
 *      * If module.package_id == current_module.package_id: Access Granted
 *      * Else: Access Denied (Error E002)
 *    - If item.visibility == PUB(SUPER):
 *      * If current_module.parent == symbol's module: Access Granted
 *      * Else: Access Denied (Error E002)
 */
class VisibilityChecker {
public:
    /**
     * Constructor
     * @param moduleTable Reference to module table for module lookups
     */
    explicit VisibilityChecker(ModuleTable* moduleTable);
    
    /**
     * Check if a symbol can be accessed from the current context
     * @param symbol The symbol being accessed
     * @param symbolModule The module where the symbol is defined
     * @param currentModule The module making the access
     * @param accessLine Line number where access occurs (for error reporting)
     * @param accessColumn Column number where access occurs
     * @return True if access is allowed, false otherwise
     */
    bool checkAccess(const Symbol* symbol,
                     const Module* symbolModule,
                     const Module* currentModule,
                     int accessLine,
                     int accessColumn);
    
    /**
     * Check if access is allowed based on visibility level
     * @param visibility The visibility level of the symbol
     * @param symbolModule The module where the symbol is defined
     * @param currentModule The module making the access
     * @return True if access is allowed
     */
    bool isAccessAllowed(VisibilityLevel visibility,
                         const Module* symbolModule,
                         const Module* currentModule) const;
    
    /**
     * Get visibility level from symbol
     * Maps Symbol.isPublic flag to VisibilityLevel enum
     * Future expansion: Parse pub(package), pub(super) modifiers
     * @param symbol The symbol to check
     * @return Visibility level
     */
    VisibilityLevel getVisibility(const Symbol* symbol) const;
    
    /**
     * Check if two modules are in the same package (compilation unit)
     * @param module1 First module
     * @param module2 Second module
     * @return True if same package
     */
    bool isSamePackage(const Module* module1, const Module* module2) const;
    
    /**
     * Check if module2 is a parent of module1
     * @param child Child module
     * @param parent Potential parent module
     * @return True if parent relationship exists
     */
    bool isParentModule(const Module* child, const Module* parent) const;
    
    /**
     * Generate E002 error for access violation
     * @param symbolName Name of the inaccessible symbol
     * @param symbolModule Module where symbol is defined
     * @param currentModule Module attempting access
     * @param line Line number of access attempt
     * @param column Column number of access attempt
     */
    void reportAccessError(const std::string& symbolName,
                          const Module* symbolModule,
                          const Module* currentModule,
                          int line,
                          int column);
    
    /**
     * Get all error messages
     * @return Vector of error messages
     */
    const std::vector<std::string>& getErrors() const { return errors; }
    
    /**
     * Check if any errors occurred
     * @return True if errors exist
     */
    bool hasErrors() const { return !errors.empty(); }
    
    /**
     * Clear all errors
     */
    void clearErrors() { errors.clear(); }
    
private:
    ModuleTable* moduleTable;          // Module system for lookups
    std::vector<std::string> errors;   // Collected error messages
    
    /**
     * Format visibility level as string for error messages
     */
    std::string visibilityToString(VisibilityLevel visibility) const;
};

} // namespace sema
} // namespace aria

#endif // ARIA_FRONTEND_SEMA_VISIBILITY_CHECKER_H
