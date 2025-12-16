#ifndef ARIA_SYMBOL_TABLE_H
#define ARIA_SYMBOL_TABLE_H

#include <string>
#include <memory>
#include <unordered_map>
#include <vector>

namespace aria {
namespace sema {

// Forward declarations
class Type;
class Scope;
class SymbolTable;

// ============================================================================
// Symbol - Represents a named entity in the program
// ============================================================================
// A symbol can be a variable, function, type, or module
// Each symbol has a name, type, scope, and source location

enum class SymbolKind {
    VARIABLE,       // Variable declaration
    FUNCTION,       // Function declaration
    PARAMETER,      // Function parameter
    TYPE,           // Type definition (struct, enum, etc.)
    MODULE,         // Module declaration
    CONSTANT,       // Compile-time constant
};

struct Symbol {
    std::string name;           // Symbol identifier
    SymbolKind kind;            // What kind of symbol this is
    Type* type;                 // Type information (owned by TypeSystem)
    Scope* scope;               // Scope where this symbol is defined
    int line;                   // Source line number
    int column;                 // Source column number
    bool isPublic;              // Visibility (pub keyword)
    bool isMutable;             // Mutability (const vs mutable)
    bool isInitialized;         // Has been initialized
    
    // Constructor
    Symbol(const std::string& name, SymbolKind kind, Type* type,
           Scope* scope, int line = 0, int column = 0);
    
    // Helpers
    std::string toString() const;
};

// ============================================================================
// Scope - Represents a lexical scope with symbols
// ============================================================================
// Scopes form a tree structure where each scope can have:
// - Parent scope (enclosing scope)
// - Child scopes (nested scopes like blocks, functions)
// - Symbols defined in this scope

enum class ScopeKind {
    GLOBAL,         // Global/module scope
    FUNCTION,       // Function scope
    BLOCK,          // Block scope (if, while, for bodies, etc.)
    STRUCT,         // Struct/type scope
    MODULE,         // Module scope
};

class Scope {
private:
    ScopeKind kind;
    Scope* parent;
    std::vector<std::unique_ptr<Scope>> children;
    std::unordered_map<std::string, Symbol*> symbols;
    std::string name;           // Optional name (for functions, modules)
    int depth;                  // Nesting depth (0 = global)
    
public:
    // Constructor
    explicit Scope(ScopeKind kind, Scope* parent = nullptr, 
                   const std::string& name = "");
    
    // Scope management
    Scope* enterScope(ScopeKind kind, const std::string& name = "");
    Scope* exitScope();
    Scope* getParent() const { return parent; }
    ScopeKind getKind() const { return kind; }
    int getDepth() const { return depth; }
    const std::string& getName() const { return name; }
    
    // Symbol operations
    bool define(Symbol* symbol);                    // Add symbol to this scope
    Symbol* lookup(const std::string& name) const;  // Look up in this scope only
    Symbol* resolve(const std::string& name) const; // Look up in this and parent scopes
    bool isInScope(const std::string& name) const;  // Check if name exists in this scope
    bool isDuplicate(const std::string& name) const;// Check if symbol already defined
    
    // Iteration
    const std::unordered_map<std::string, Symbol*>& getSymbols() const { return symbols; }
    const std::vector<std::unique_ptr<Scope>>& getChildren() const { return children; }
    
    // Debugging
    std::string toString(int indent = 0) const;
};

// ============================================================================
// SymbolTable - Manages the entire symbol table hierarchy
// ============================================================================
// The symbol table maintains:
// - Root scope (global scope)
// - Current scope (for adding new symbols)
// - All allocated symbols (for memory management)

class SymbolTable {
private:
    std::unique_ptr<Scope> rootScope;
    Scope* currentScope;
    std::vector<std::unique_ptr<Symbol>> symbols;  // Owns all symbols
    
public:
    // Constructor
    SymbolTable();
    
    // Scope navigation
    void enterScope(ScopeKind kind, const std::string& name = "");
    void exitScope();
    Scope* getCurrentScope() const { return currentScope; }
    Scope* getRootScope() const { return rootScope.get(); }
    
    // Symbol operations
    Symbol* defineSymbol(const std::string& name, SymbolKind kind, Type* type,
                         int line = 0, int column = 0);
    Symbol* lookupSymbol(const std::string& name) const;      // Current scope only
    Symbol* resolveSymbol(const std::string& name) const;     // Current + parent scopes
    bool isDefined(const std::string& name) const;            // Check if already defined
    
    // Error handling
    std::vector<std::string> errors;
    void error(const std::string& message);
    bool hasErrors() const { return !errors.empty(); }
    const std::vector<std::string>& getErrors() const { return errors; }
    
    // Debugging
    std::string toString() const;
};

} // namespace sema
} // namespace aria

#endif // ARIA_SYMBOL_TABLE_H
