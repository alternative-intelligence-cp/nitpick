#include "parser.h"
#include "ast/stmt.h"
#include <stdexcept>
#include <iostream>

namespace aria {
namespace frontend {

// Parse struct declaration
// Grammar: const StructName = struct { field: type, field: type, };
// Example: const Point = struct { x: int64, y: int64, };
std::unique_ptr<StructDecl> Parser::parseStructDecl() {
    // 1. Check for optional 'const' (structs are typically const type definitions)
    bool is_const = false;
    if (match(TOKEN_KW_CONST)) {
        is_const = true;
    }
    
    // 2. Parse struct name (identifier)
    Token name_tok = expect(TOKEN_IDENTIFIER);
    std::string struct_name = name_tok.value;
    
    // 3. Expect assignment operator
    expect(TOKEN_ASSIGN);
    
    // 4. Expect 'struct' keyword
    expect(TOKEN_KW_STRUCT);
    
    // 5. Expect opening brace
    expect(TOKEN_LEFT_BRACE);
    
    // 6. Parse fields and methods
    std::vector<StructField> fields;
    std::vector<std::unique_ptr<VarDecl>> methods;
    
    while (!check(TOKEN_RIGHT_BRACE) && current.type != TOKEN_EOF) {
        // Check if this is a method declaration
        // Methods use func:name = returnType() { body } syntax
        // Fields use name:type syntax
        if (current.type != TOKEN_IDENTIFIER) {
            // Assume it's a method declaration (func:name = ...)
            // The inline struct parser in parser.cpp handles this,
            // so this path might not be used. Keep for completeness.
            auto stmt = parseVarDecl();
            auto* varDecl = dynamic_cast<VarDecl*>(stmt.get());
            
            if (varDecl && varDecl->initializer) {
                // Store the VarDecl (no type verification needed)
                methods.push_back(std::unique_ptr<VarDecl>(static_cast<VarDecl*>(stmt.release())));
            }
            
            continue;
        }
        
        // Otherwise, parse as a field
        // Parse field name
        Token field_name = expect(TOKEN_IDENTIFIER);
        
        // Expect colon
        expect(TOKEN_COLON);
        
        // Parse field type - can be any type token or identifier (for user-defined types)
        if (current.type < TOKEN_TYPE_VOID || current.type > TOKEN_TYPE_STRING) {
            if (current.type != TOKEN_IDENTIFIER) {
                throw std::runtime_error("Expected type for struct field at line " + 
                                       std::to_string(current.line));
            }
        }
        Token field_type = current;
        advance();  // consume type token
        
        // Handle array types: field: int8[256] or int64[]
        std::string type_name = field_type.value;
        if (check(TOKEN_LEFT_BRACKET)) {
            advance(); // consume [
            type_name += "[";
            
            // Check for array size
            if (!check(TOKEN_RIGHT_BRACKET)) {
                Token size_tok = expect(TOKEN_INT_LITERAL);
                type_name += size_tok.value;
            }
            
            expect(TOKEN_RIGHT_BRACKET);
            type_name += "]";
        }
        
        // Add field to list
        fields.emplace_back(type_name, field_name.value);
        
        // Expect comma unless this is the last field (next token is })
        if (!check(TOKEN_RIGHT_BRACE)) {
            expect(TOKEN_COMMA);
        }
    }
    
    // 7. Expect closing brace
    expect(TOKEN_RIGHT_BRACE);
    
    // 8. Expect semicolon (end of statement)
    expect(TOKEN_SEMICOLON);
    
    // 9. Create and return AST node
    auto decl = std::make_unique<StructDecl>(struct_name, std::move(fields));
    decl->is_const = is_const;
    decl->methods = std::move(methods);  // Add methods to struct
    
    return decl;
}

} // namespace frontend
} // namespace aria
