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
    std::vector<std::unique_ptr<FuncDecl>> methods;
    
    while (!check(TOKEN_RIGHT_BRACE) && current.type != TOKEN_EOF) {
        // Peek ahead to determine if this is a field or method
        // Field syntax: name:type,
        // Method syntax: func:name = returnType(...) { ... },
        
        // Check for func:name pattern
        if (current.type == TOKEN_IDENTIFIER && current.value == "func") {
            // This is a method declaration
            advance(); // consume 'func'
            expect(TOKEN_COLON);
            
            // Parse method name
            Token method_name_tok = expect(TOKEN_IDENTIFIER);
            std::string method_name = method_name_tok.value;
            
            expect(TOKEN_ASSIGN);
            
            // Parse return type (before the parameter list)
            Token return_type_tok = current;
            std::string return_type = return_type_tok.value;
            advance(); // consume return type
            
            // Parse parameter list
            expect(TOKEN_LEFT_PAREN);
            std::vector<FuncParam> params;
            
            while (!check(TOKEN_RIGHT_PAREN) && current.type != TOKEN_EOF) {
                // Check for special 'self' parameter
                if (current.type == TOKEN_IDENTIFIER && current.value == "self") {
                    // self parameter - type is implicitly the struct being defined
                    advance(); // consume 'self'
                    
                    // Add self as first parameter with struct type
                    params.emplace_back(struct_name, "self", nullptr);
                } else {
                    // Regular parameter: type:name
                    Token param_type = current;
                    advance();
                    expect(TOKEN_COLON);
                    Token param_name = expect(TOKEN_IDENTIFIER);
                    
                    params.emplace_back(param_type.value, param_name.value, nullptr);
                }
                
                if (!check(TOKEN_RIGHT_PAREN)) {
                    expect(TOKEN_COMMA);
                }
            }
            
            expect(TOKEN_RIGHT_PAREN);
            
            // Parse method body
            auto body = parseBlock();
            
            // Create FuncDecl for the method
            std::vector<std::string> no_generics;  // Methods don't have generic params (struct might)
            auto method_decl = std::make_unique<FuncDecl>(
                method_name,
                no_generics,
                std::move(params),
                return_type,
                std::move(body)
            );
            
            methods.push_back(std::move(method_decl));
            
            // Expect comma after method (optional before closing brace)
            if (!check(TOKEN_RIGHT_BRACE)) {
                expect(TOKEN_COMMA);
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
