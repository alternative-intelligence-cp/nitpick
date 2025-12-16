#include "frontend/sema/symbol_table.h"
#include "frontend/sema/type.h"
#include <sstream>

namespace aria {
namespace sema {

// ============================================================================
// Symbol Implementation
// ============================================================================

Symbol::Symbol(const std::string& name, SymbolKind kind, Type* type,
               Scope* scope, int line, int column)
    : name(name), kind(kind), type(type), scope(scope),
      line(line), column(column), isPublic(false),
      isMutable(true), isInitialized(false) {}

std::string Symbol::toString() const {
    std::stringstream ss;
    ss << name << ": ";
    if (type) {
        ss << type->toString();
    } else {
        ss << "<no type>";
    }
    ss << " (";
    switch (kind) {
        case SymbolKind::VARIABLE: ss << "variable"; break;
        case SymbolKind::FUNCTION: ss << "function"; break;
        case SymbolKind::PARAMETER: ss << "parameter"; break;
        case SymbolKind::TYPE: ss << "type"; break;
        case SymbolKind::MODULE: ss << "module"; break;
        case SymbolKind::CONSTANT: ss << "constant"; break;
    }
    ss << ")";
    return ss.str();
}

// ============================================================================
// Scope Implementation
// ============================================================================

Scope::Scope(ScopeKind kind, Scope* parent, const std::string& name)
    : kind(kind), parent(parent), name(name) {
    depth = parent ? parent->depth + 1 : 0;
}

Scope* Scope::enterScope(ScopeKind kind, const std::string& name) {
    auto child = std::make_unique<Scope>(kind, this, name);
    Scope* childPtr = child.get();
    children.push_back(std::move(child));
    return childPtr;
}

Scope* Scope::exitScope() {
    return parent;
}

bool Scope::define(Symbol* symbol) {
    if (isDuplicate(symbol->name)) {
        return false;  // Symbol already exists in this scope
    }
    symbols[symbol->name] = symbol;
    symbol->scope = this;
    return true;
}

Symbol* Scope::lookup(const std::string& name) const {
    auto it = symbols.find(name);
    if (it != symbols.end()) {
        return it->second;
    }
    return nullptr;
}

Symbol* Scope::resolve(const std::string& name) const {
    // First check this scope
    Symbol* symbol = lookup(name);
    if (symbol) {
        return symbol;
    }
    
    // Then check parent scopes
    if (parent) {
        return parent->resolve(name);
    }
    
    return nullptr;
}

bool Scope::isInScope(const std::string& name) const {
    return symbols.find(name) != symbols.end();
}

bool Scope::isDuplicate(const std::string& name) const {
    return isInScope(name);
}

std::string Scope::toString(int indent) const {
    std::stringstream ss;
    std::string indentStr(indent * 2, ' ');
    
    ss << indentStr << "Scope(";
    switch (kind) {
        case ScopeKind::GLOBAL: ss << "global"; break;
        case ScopeKind::FUNCTION: ss << "function"; break;
        case ScopeKind::BLOCK: ss << "block"; break;
        case ScopeKind::STRUCT: ss << "struct"; break;
        case ScopeKind::MODULE: ss << "module"; break;
    }
    if (!name.empty()) {
        ss << " " << name;
    }
    ss << ", depth=" << depth << ")\n";
    
    // Print symbols
    for (const auto& pair : symbols) {
        ss << indentStr << "  " << pair.second->toString() << "\n";
    }
    
    // Print child scopes
    for (const auto& child : children) {
        ss << child->toString(indent + 1);
    }
    
    return ss.str();
}

// ============================================================================
// SymbolTable Implementation
// ============================================================================

SymbolTable::SymbolTable() {
    rootScope = std::make_unique<Scope>(ScopeKind::GLOBAL, nullptr, "global");
    currentScope = rootScope.get();
}

void SymbolTable::enterScope(ScopeKind kind, const std::string& name) {
    currentScope = currentScope->enterScope(kind, name);
}

void SymbolTable::exitScope() {
    if (currentScope->getParent()) {
        currentScope = currentScope->exitScope();
    } else {
        error("Cannot exit global scope");
    }
}

Symbol* SymbolTable::defineSymbol(const std::string& name, SymbolKind kind,
                                   Type* type, int line, int column) {
    // Check for duplicates in current scope
    if (currentScope->isDuplicate(name)) {
        std::stringstream ss;
        ss << "Symbol '" << name << "' already defined in current scope";
        if (line > 0) {
            ss << " at line " << line << ", column " << column;
        }
        error(ss.str());
        return nullptr;
    }
    
    // Create new symbol
    auto symbol = std::make_unique<Symbol>(name, kind, type, currentScope, line, column);
    Symbol* symbolPtr = symbol.get();
    symbols.push_back(std::move(symbol));
    
    // Add to current scope
    currentScope->define(symbolPtr);
    
    return symbolPtr;
}

Symbol* SymbolTable::lookupSymbol(const std::string& name) const {
    return currentScope->lookup(name);
}

Symbol* SymbolTable::resolveSymbol(const std::string& name) const {
    return currentScope->resolve(name);
}

bool SymbolTable::isDefined(const std::string& name) const {
    return currentScope->resolve(name) != nullptr;
}

void SymbolTable::error(const std::string& message) {
    errors.push_back(message);
}

std::string SymbolTable::toString() const {
    return rootScope->toString(0);
}

} // namespace sema
} // namespace aria
