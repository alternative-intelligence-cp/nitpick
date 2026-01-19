# Language Server ↔ Compiler Integration Guide

**Document Type**: Integration Guide  
**Version**: 1.0  
**Last Updated**: December 22, 2025  
**Status**: Reference Documentation

---

## Table of Contents

1. [Overview](#overview)
2. [Shared Frontend Architecture](#shared-frontend-architecture)
3. [Symbol Table Management](#symbol-table-management)
4. [Incremental Parsing](#incremental-parsing)
5. [Diagnostic Generation](#diagnostic-generation)
6. [Code Completion](#code-completion)
7. [Performance Optimization](#performance-optimization)

---

## Overview

The **aria-lsp** (Language Server Protocol implementation) shares the compiler's frontend to provide IDE features:

| Feature | Shared Component | Benefit |
|---------|------------------|---------|
| **Syntax Highlighting** | Lexer | Same tokenization as compiler |
| **Error Detection** | Parser + Semantic Analyzer | Same error messages |
| **Code Completion** | Symbol Table | Accurate symbol resolution |
| **Go-to-Definition** | AST + Symbol Table | Exact source locations |
| **Type Information** | Type Checker | Same type inference |

**Key Insight**: LSP uses **same code** as compiler → zero divergence in behavior

---

## Shared Frontend Architecture

### Library Structure

**Compiler Frontend** (libaria_frontend.a):
```
libaria_frontend.a
├── lexer.o         (tokenization)
├── parser.o        (AST construction)
├── semantic.o      (type checking, symbol resolution)
├── ast.o           (AST node definitions)
└── symbol_table.o  (symbol management)
```

**Built From**: `/home/randy/._____RANDY_____/REPOS/aria/src/frontend/`

---

### Compilation

**Compiler Uses Frontend**:
```cpp
// ariac main.cpp
#include "frontend/lexer.h"
#include "frontend/parser.h"
#include "frontend/semantic.h"

int main(int argc, char** argv) {
    // Lex
    Lexer lexer(source_code);
    std::vector<Token> tokens = lexer.tokenize();
    
    // Parse
    Parser parser(tokens);
    AST* ast = parser.parse();
    
    // Semantic analysis
    SemanticAnalyzer analyzer(ast);
    analyzer.analyze();
    
    // Generate IR (backend)
    IRGenerator ir_gen(ast);
    // ...
}
```

---

**LSP Uses Same Frontend**:
```cpp
// aria-lsp main.cpp
#include "frontend/lexer.h"
#include "frontend/parser.h"
#include "frontend/semantic.h"

class LanguageServer {
    void on_document_change(const std::string& uri, const std::string& text) {
        // Lex (same code as compiler)
        Lexer lexer(text);
        std::vector<Token> tokens = lexer.tokenize();
        
        // Parse (same code as compiler)
        Parser parser(tokens);
        AST* ast = parser.parse();
        
        // Semantic analysis (same code as compiler)
        SemanticAnalyzer analyzer(ast);
        analyzer.analyze();
        
        // Extract diagnostics
        send_diagnostics(uri, analyzer.get_errors());
        
        // Update symbol table
        document_symbols[uri] = analyzer.get_symbol_table();
    }
};
```

**Result**: LSP sees **exactly** what compiler sees

---

### Build Integration

**Compiler Build** (CMakeLists.txt):
```cmake
# Build frontend library
add_library(aria_frontend STATIC
    src/frontend/lexer.cpp
    src/frontend/parser.cpp
    src/frontend/semantic.cpp
    src/frontend/ast.cpp
    src/frontend/symbol_table.cpp
)

# Compiler links against frontend
add_executable(ariac src/main.cpp)
target_link_libraries(ariac aria_frontend aria_backend)
```

---

**LSP Build** (separate repository):
```cmake
# Link against compiler's frontend library
find_library(ARIA_FRONTEND aria_frontend
    PATHS /usr/local/lib/aria
)

add_executable(aria-lsp src/main.cpp src/lsp_server.cpp)
target_link_libraries(aria-lsp ${ARIA_FRONTEND})
```

**Installation**: Compiler install puts `libaria_frontend.a` in `/usr/local/lib/aria/`

---

## Symbol Table Management

### Symbol Table Structure

**Definition** (in frontend):
```cpp
// src/frontend/symbol_table.h
class SymbolTable {
public:
    struct Symbol {
        std::string name;
        SymbolKind kind;  // Variable, Function, Struct, etc.
        Type* type;
        SourceLocation location;  // File path + line + column
        Scope* scope;
    };
    
    void insert(const Symbol& symbol);
    Symbol* lookup(const std::string& name, Scope* scope);
    std::vector<Symbol*> get_all_symbols();
    std::vector<Symbol*> get_symbols_in_scope(Scope* scope);
};

enum class SymbolKind {
    Variable,
    Function,
    Struct,
    Enum,
    TypeAlias,
    Import
};
```

---

### Building the Symbol Table

**Example Aria Code**:
```aria
struct:point = { x: i32, y: i32 };

func:distance = f64(p1: point, p2: point) {
    var:dx = p1.x - p2.x;
    var:dy = p1.y - p2.y;
    pass(sqrt(dx * dx + dy * dy));
}
```

**Symbol Table**:
```
Global Scope:
  - point (Struct) @ line 1, col 7
  - distance (Function) @ line 3, col 6
    Function Scope:
      - p1 (Parameter) @ line 3, col 20
      - p2 (Parameter) @ line 3, col 30
      Local Scope:
        - dx (Variable) @ line 4, col 9
        - dy (Variable) @ line 5, col 9
```

---

### LSP Usage

**Go-to-Definition** (user clicks `p1.x`):
```cpp
// LSP handler
void on_goto_definition(const std::string& uri, int line, int col) {
    // Get symbol table for document
    SymbolTable* symtab = document_symbols[uri];
    
    // Find symbol at cursor position
    Symbol* sym = symtab->lookup_at_position(line, col);
    
    if (sym) {
        // Jump to definition
        send_location(sym->location.file, sym->location.line, sym->location.col);
    }
}
```

---

**Find References** (user clicks `distance`):
```cpp
void on_find_references(const std::string& uri, int line, int col) {
    // Find symbol definition
    Symbol* sym = document_symbols[uri]->lookup_at_position(line, col);
    
    // Search all documents for references
    std::vector<Location> references;
    for (const auto& [doc_uri, symtab] : document_symbols) {
        for (Symbol* ref : symtab->get_references(sym->name)) {
            references.push_back({ref->location.file, ref->location.line, ref->location.col});
        }
    }
    
    send_locations(references);
}
```

---

## Incremental Parsing

### Challenge

**Problem**: Re-parsing entire file on every keystroke is slow (100ms+ for large files)

**Solution**: Incremental parsing (only re-parse changed regions)

---

### Implementation Strategy

**1. Track Changes**:
```cpp
struct TextEdit {
    Range range;  // Start/end line & column
    std::string new_text;
};

void on_document_change(const std::string& uri, const TextEdit& edit) {
    // Update document text
    documents[uri].apply_edit(edit);
    
    // Incremental re-parse
    incremental_parse(uri, edit.range);
}
```

---

**2. Invalidate Affected AST Nodes**:
```cpp
void incremental_parse(const std::string& uri, const Range& changed_range) {
    AST* ast = document_asts[uri];
    
    // Find AST nodes overlapping changed range
    std::vector<ASTNode*> affected_nodes = ast->find_nodes_in_range(changed_range);
    
    // Mark nodes as invalid
    for (ASTNode* node : affected_nodes) {
        node->mark_invalid();
    }
    
    // Re-parse only affected nodes
    for (ASTNode* node : affected_nodes) {
        re_parse_node(node);
    }
    
    // Re-run semantic analysis on changed subtrees
    SemanticAnalyzer analyzer(ast);
    analyzer.analyze_incremental(affected_nodes);
}
```

---

**3. Preserve Valid Subtrees**:
```cpp
// Before re-parsing
AST (valid):
  FuncDecl (valid)
    |- ParamList (valid)
    |- Block (INVALID - user edited here)
       |- VarDecl (INVALID)
       |- IfStmt (valid)

// After re-parsing
AST (valid):
  FuncDecl (valid)
    |- ParamList (valid - preserved)
    |- Block (VALID - re-parsed)
       |- VarDecl (VALID - re-parsed)
       |- AssignStmt (VALID - new)
       |- IfStmt (valid - preserved)
```

**Benefit**: 10-100x speedup (1ms vs 100ms)

---

## Diagnostic Generation

### Error Extraction

**Compiler** (during semantic analysis):
```cpp
class SemanticAnalyzer {
    std::vector<Diagnostic> diagnostics;
    
    void check_type_mismatch(ASTNode* node, Type* expected, Type* actual) {
        if (expected != actual) {
            diagnostics.push_back({
                .severity = DiagnosticSeverity::Error,
                .message = "Type mismatch: expected " + expected->name + ", got " + actual->name,
                .location = node->location,
                .code = "E0308"
            });
        }
    }
};
```

---

**LSP** (convert to LSP format):
```cpp
void send_diagnostics(const std::string& uri, const std::vector<Diagnostic>& diags) {
    json lsp_diagnostics = json::array();
    
    for (const Diagnostic& diag : diags) {
        lsp_diagnostics.push_back({
            {"range", {
                {"start", {{"line", diag.location.line}, {"character", diag.location.col}}},
                {"end", {{"line", diag.location.line}, {"character", diag.location.col + diag.location.length}}}
            }},
            {"severity", severity_to_lsp(diag.severity)},
            {"message", diag.message},
            {"code", diag.code}
        });
    }
    
    // Send via LSP protocol
    send_notification("textDocument/publishDiagnostics", {
        {"uri", uri},
        {"diagnostics", lsp_diagnostics}
    });
}
```

---

### Real-Time Error Detection

**Scenario**: User types code with error

**Code**:
```aria
var:x = i32[0..10];
x = i32(20);  // Out of range!
```

**LSP Behavior**:
1. User types `x = i32(20);`
2. LSP re-parses document
3. Semantic analyzer detects TBB overflow
4. LSP sends diagnostic to editor
5. Editor shows red squiggly under `20`

**Timing**: <10ms from keystroke to error display

---

## Code Completion

### Completion Triggers

**Trigger Characters**: `.`, `::`, `:`, `<space>` (after keywords)

**Example**:
```aria
struct:point = { x: i32, y: i32 };
var:p = point { x: 10, y: 20 };
p.█  // User types '.' - trigger completion
```

---

### Completion Provider

**LSP Handler**:
```cpp
void on_completion(const std::string& uri, int line, int col) {
    // Get symbol table
    SymbolTable* symtab = document_symbols[uri];
    
    // Find context at cursor
    ASTNode* context_node = find_node_at_position(line, col);
    
    if (context_node->kind == ASTNodeKind::MemberAccess) {
        // Member access: p.█
        MemberAccessNode* member_access = (MemberAccessNode*)context_node;
        Type* object_type = member_access->object->type;
        
        if (object_type->kind == TypeKind::Struct) {
            // Suggest struct fields
            StructType* struct_type = (StructType*)object_type;
            std::vector<CompletionItem> items;
            
            for (Field* field : struct_type->fields) {
                items.push_back({
                    .label = field->name,
                    .kind = CompletionItemKind::Field,
                    .detail = field->type->name,
                    .insert_text = field->name
                });
            }
            
            send_completion_list(items);
        }
    } else {
        // Suggest all symbols in scope
        Scope* scope = context_node->scope;
        std::vector<Symbol*> symbols = symtab->get_symbols_in_scope(scope);
        
        std::vector<CompletionItem> items;
        for (Symbol* sym : symbols) {
            items.push_back({
                .label = sym->name,
                .kind = symbol_kind_to_lsp(sym->kind),
                .detail = sym->type->name
            });
        }
        
        send_completion_list(items);
    }
}
```

---

**Result** (user sees in editor):
```
p.█
  ↓ (completion menu appears)
┌────────────────────┐
│ x : i32            │
│ y : i32            │
└────────────────────┘
```

---

### Snippet Completion

**Function Calls**:
```aria
func:add = i32(a: i32, b: i32) {
    pass(a + b);
}

add█  // User types 'add' - trigger snippet
```

**Completion Item**:
```cpp
{
    .label = "add",
    .kind = CompletionItemKind::Function,
    .detail = "i32(a: i32, b: i32)",
    .insert_text = "add(${1:a}, ${2:b})",  // VSCode snippet syntax
    .insert_text_format = InsertTextFormat::Snippet
}
```

**Result**: User selects completion, cursor jumps to first parameter

```aria
add(█, b)  // Cursor at first parameter
```

---

## Performance Optimization

### Lazy Symbol Resolution

**Problem**: Resolving all symbols in large files is slow

**Solution**: Resolve symbols on-demand

---

**Implementation**:
```cpp
class SymbolTable {
    // Cache resolved symbols
    std::unordered_map<std::string, Symbol*> resolved_cache;
    
    Symbol* lookup(const std::string& name, Scope* scope) {
        // Check cache first
        std::string cache_key = name + "@" + scope->id;
        if (resolved_cache.count(cache_key)) {
            return resolved_cache[cache_key];
        }
        
        // Resolve symbol
        Symbol* sym = resolve_symbol(name, scope);
        
        // Cache result
        resolved_cache[cache_key] = sym;
        
        return sym;
    }
};
```

**Benefit**: 5-10x faster for go-to-definition, completion

---

### Parallel Analysis

**Strategy**: Analyze multiple files simultaneously (for multi-file projects)

**Implementation**:
```cpp
#include <thread>
#include <future>

void analyze_workspace(const std::vector<std::string>& file_uris) {
    std::vector<std::future<void>> futures;
    
    for (const std::string& uri : file_uris) {
        futures.push_back(std::async(std::launch::async, [uri] {
            // Parse and analyze in parallel
            std::string content = read_file(uri);
            
            Lexer lexer(content);
            auto tokens = lexer.tokenize();
            
            Parser parser(tokens);
            AST* ast = parser.parse();
            
            SemanticAnalyzer analyzer(ast);
            analyzer.analyze();
            
            // Store results
            document_asts[uri] = ast;
            document_symbols[uri] = analyzer.get_symbol_table();
        }));
    }
    
    // Wait for all to complete
    for (auto& future : futures) {
        future.get();
    }
}
```

**Benefit**: Workspace indexing 4-8x faster (on multi-core machines)

---

### Caching

**Strategy**: Cache parsed ASTs and symbol tables between requests

**Implementation**:
```cpp
// Global caches (per-document)
std::unordered_map<std::string, AST*> document_asts;
std::unordered_map<std::string, SymbolTable*> document_symbols;
std::unordered_map<std::string, std::string> document_content_hashes;

void on_document_change(const std::string& uri, const std::string& new_content) {
    // Compute content hash
    std::string new_hash = sha256(new_content);
    
    // Check if content actually changed
    if (document_content_hashes.count(uri) && document_content_hashes[uri] == new_hash) {
        // No change, use cached data
        return;
    }
    
    // Content changed, re-parse
    // ...
    
    // Update cache
    document_content_hashes[uri] = new_hash;
}
```

**Benefit**: Avoid re-parsing on save (if content unchanged)

---

## Related Documents

- **[ARIA_LSP](../components/ARIA_LSP.md)**: Language server architecture
- **[ARIA_COMPILER](../components/ARIA_COMPILER.md)**: Compiler frontend implementation
- **[TYPE_SYSTEM](../specs/TYPE_SYSTEM.md)**: Type checking details
- **[ERROR_HANDLING](../specs/ERROR_HANDLING.md)**: Diagnostic generation
- **[COMPILER_RUNTIME](./COMPILER_RUNTIME.md)**: Compiler ↔ Runtime integration

---

**Document Version**: 1.0  
**Author**: Aria Ecosystem Documentation  
**Status**: Reference guide (implementation planned, shared frontend architecture confirmed)
