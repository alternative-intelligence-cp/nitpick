// Implementation of Trait System Parsing
// Handles trait declarations and trait implementations
#include "parser.h"
#include "ast.h"
#include "ast/stmt.h"
#include <memory>
#include <sstream>
#include <stdexcept>

namespace aria {
namespace frontend {

// Parse trait declaration
// Syntax: trait:Name = { method_name:(params) -> return_type, ... }
// Or with super traits: trait:Name:SuperTrait1:SuperTrait2 = { ... }
std::unique_ptr<TraitDecl> Parser::parseTraitDecl() {
    // Expect TOKEN_KW_TRAIT
    if (current.type != TOKEN_KW_TRAIT) {
        throw std::runtime_error("Expected 'trait' keyword");
    }
    advance();
    
    // Expect colon
    if (!match(TOKEN_COLON)) {
        throw std::runtime_error("Expected ':' after 'trait'");
    }
    
    // Parse trait name
    if (current.type != TOKEN_IDENTIFIER) {
        throw std::runtime_error("Expected trait name after 'trait:'");
    }
    std::string traitName = current.value;
    advance();
    
    // Parse optional super traits: :SuperTrait1:SuperTrait2
    std::vector<std::string> superTraits;
    while (current.type == TOKEN_COLON && peek().type != TOKEN_ASSIGN) {
        advance(); // consume ':'
        
        if (current.type != TOKEN_IDENTIFIER) {
            throw std::runtime_error("Expected super trait name after ':'");
        }
        
        superTraits.push_back(current.value);
        advance();
    }
    
    // Expect assignment
    if (!match(TOKEN_ASSIGN)) {
        throw std::runtime_error("Expected '=' after trait name");
    }
    
    // Expect opening brace
    if (!match(TOKEN_LBRACE)) {
        throw std::runtime_error("Expected '{' to begin trait body");
    }
    
    // Parse trait methods
    std::vector<TraitMethod> methods;
    while (current.type != TOKEN_RBRACE && current.type != TOKEN_EOF) {
        // Skip commas between method signatures
        if (match(TOKEN_COMMA)) {
            continue;
        }
        
        // Parse method name
        if (current.type != TOKEN_IDENTIFIER) {
            throw std::runtime_error("Expected method name in trait");
        }
        std::string methodName = current.value;
        advance();
        
        // Expect colon before parameters
        if (!match(TOKEN_COLON)) {
            throw std::runtime_error("Expected ':' after method name");
        }
        
        // Parse parameters
        if (!match(TOKEN_LPAREN)) {
            throw std::runtime_error("Expected '(' to begin method parameters");
        }
        
        // Parameter list
        std::vector<FuncParam> params;
        
        // Parse parameter list
        while (current.type != TOKEN_RPAREN && current.type != TOKEN_EOF) {
            // Skip commas
            if (match(TOKEN_COMMA)) {
                continue;
            }
            
            // Parse parameter name
            if (current.type != TOKEN_IDENTIFIER) {
                throw std::runtime_error("Expected parameter name");
            }
            std::string paramName = current.value;
            advance();
            
            // Expect colon
            if (!match(TOKEN_COLON)) {
                throw std::runtime_error("Expected ':' after parameter name");
            }
            
            // Parse parameter type
            std::string paramType = parseTypeName();
            
            // Add parameter (no auto_wrap at param level)
            params.emplace_back(paramType, paramName, nullptr);
        }
        
        // Expect closing paren
        if (!match(TOKEN_RPAREN)) {
            throw std::runtime_error("Expected ')' to end method parameters");
        }
        
        // Parse optional return type
        std::string returnType = "void";
        if (match(TOKEN_ARROW)) {
            returnType = parseTypeName();
        }
        
        // Create TraitMethod
        TraitMethod method(methodName, std::move(params), returnType);
        
        // Check if entire method signature is marked auto_wrap
        if (current.type == TOKEN_IDENTIFIER && current.value == "auto_wrap") {
            method.auto_wrap = true;
            advance();
        }
        
        methods.push_back(std::move(method));
    }
    
    // Expect closing brace
    if (!match(TOKEN_RBRACE)) {
        throw std::runtime_error("Expected '}' to end trait body");
    }
    
    auto trait = std::make_unique<TraitDecl>(traitName, std::move(methods));
    trait->super_traits = std::move(superTraits);
    return trait;
}

// Parse trait implementation
// Syntax: impl:TraitName:for:TypeName = { method implementations }
std::unique_ptr<ImplDecl> Parser::parseImplDecl() {
    // Expect TOKEN_KW_IMPL
    if (current.type != TOKEN_KW_IMPL) {
        throw std::runtime_error("Expected 'impl' keyword");
    }
    advance();
    
    // Expect colon
    if (!match(TOKEN_COLON)) {
        throw std::runtime_error("Expected ':' after 'impl'");
    }
    
    // Parse trait name
    if (current.type != TOKEN_IDENTIFIER) {
        throw std::runtime_error("Expected trait name after 'impl:'");
    }
    std::string traitName = current.value;
    advance();
    
    // Expect :for:
    if (!match(TOKEN_COLON)) {
        throw std::runtime_error("Expected ':for:' in impl declaration");
    }
    
    if (current.type != TOKEN_IDENTIFIER || current.value != "for") {
        throw std::runtime_error("Expected 'for' keyword in impl declaration");
    }
    advance();
    
    if (!match(TOKEN_COLON)) {
        throw std::runtime_error("Expected ':' after 'for'");
    }
    
    // Parse type name
    if (current.type != TOKEN_IDENTIFIER) {
        throw std::runtime_error("Expected type name after 'for:'");
    }
    std::string typeName = current.value;
    advance();
    
    // Expect assignment
    if (!match(TOKEN_ASSIGN)) {
        throw std::runtime_error("Expected '=' after impl declaration");
    }
    
    // Expect opening brace
    if (!match(TOKEN_LBRACE)) {
        throw std::runtime_error("Expected '{' to begin impl body");
    }
    
    // Parse method implementations
    std::vector<std::unique_ptr<FuncDecl>> methods;
    while (current.type != TOKEN_RBRACE && current.type != TOKEN_EOF) {
        // Skip commas
        if (match(TOKEN_COMMA)) {
            continue;
        }
        
        // Parse function declaration (method implementation)
        // Methods in impl blocks are regular function declarations
        if (current.type == TOKEN_KW_FUNC || current.type == TOKEN_IDENTIFIER) {
            auto method = parseFuncDecl();
            if (method) {
                methods.push_back(std::move(method));
            }
        } else {
            throw std::runtime_error("Expected method implementation in impl block");
        }
    }
    
    // Expect closing brace
    if (!match(TOKEN_RBRACE)) {
        throw std::runtime_error("Expected '}' to end impl body");
    }
    
    return std::make_unique<ImplDecl>(traitName, typeName, std::move(methods));
}

} // namespace frontend
} // namespace aria
