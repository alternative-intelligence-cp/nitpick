// Implementation of Variable Declaration Parsing with Colon Syntax Anchor
#include "parser.h"
#include "ast.h"
#include "ast/stmt.h"

namespace aria {
namespace frontend {

// Parser constructors
Parser::Parser(AriaLexer& lex) : lexer(lex), context() {
    advance(); // Load first token
}

Parser::Parser(AriaLexer& lex, ParserContext ctx) : lexer(lex), context(ctx) {
    advance(); // Load first token
}

void Parser::advance() {
    current = lexer.nextToken();
}

bool Parser::match(TokenType type) {
    if (current.type == type) {
        advance();
        return true;
    }
    return false;
}

Token Parser::expect(TokenType type) {
    if (current.type != type) {
        throw std::runtime_error("Unexpected token");
    }
    Token tok = current;
    advance();
    return tok;
}

Token Parser::consume(TokenType type, const std::string& message) {
    if (current.type != type) {
        throw std::runtime_error(message);
    }
    Token tok = current;
    advance();
    return tok;
}

bool Parser::check(TokenType type) {
    return current.type == type;
}

Token Parser::peek() {
    return current;
}

bool Parser::isType(const Token& token) {
    // Check if token could be a type name
    // This includes built-in types and user-defined types (identifiers)
    switch (token.type) {
        case TOKEN_TYPE_INT8:
        case TOKEN_TYPE_INT16:
        case TOKEN_TYPE_INT32:
        case TOKEN_TYPE_INT64:
        case TOKEN_TYPE_INT128:
        case TOKEN_TYPE_INT256:
        case TOKEN_TYPE_INT512:
        case TOKEN_TYPE_UINT8:
        case TOKEN_TYPE_UINT16:
        case TOKEN_TYPE_UINT32:
        case TOKEN_TYPE_UINT64:
        case TOKEN_TYPE_UINT128:
        case TOKEN_TYPE_UINT256:
        case TOKEN_TYPE_UINT512:
        case TOKEN_TYPE_FLT32:
        case TOKEN_TYPE_FLT64:
        case TOKEN_TYPE_STRING:
        case TOKEN_TYPE_BOOL:
        case TOKEN_TYPE_FUNC:
        case TOKEN_TYPE_RESULT:
        case TOKEN_TYPE_DYN:
        case TOKEN_IDENTIFIER:  // User-defined types (structs, type aliases, function types)
            return true;
        default:
            return false;
    }
}

std::string Parser::parseTypeName() {
    // Parse a complete type name, including built-in types and identifiers
    std::string typeName;
    
    // Check if it's a built-in type keyword (func, result, int8, etc.)
    if (current.type >= TOKEN_TYPE_VOID && current.type <= TOKEN_TYPE_STRING) {
        typeName = current.value;
        advance();
    } else if (current.type == TOKEN_IDENTIFIER) {
        typeName = current.value;
        advance();
    } else {
        throw std::runtime_error("Expected type name");
    }
    
    // Handle pointer suffix (@)
    while (match(TOKEN_AT)) {
        typeName += "@";
    }
    
    // Handle array suffix ([size] or [])
    if (match(TOKEN_LEFT_BRACKET)) {
        typeName += "[";
        if (!check(TOKEN_RIGHT_BRACKET)) {
            Token sizeTok = expect(TOKEN_INT_LITERAL);
            typeName += sizeTok.value;
        }
        expect(TOKEN_RIGHT_BRACKET);
        typeName += "]";
    }
    
    return typeName;
}

// Parses: [const] [wild|wildx|stack|gc] Type:Identifier [= Expression];
// Grammar:
//   VarDecl -> "const"? ( "wild" | "wildx" | "stack" | "gc" )? TypeIdentifier ":" Identifier ( "=" Expression )? ";"
std::unique_ptr<Statement> Parser::parseVarDecl() {
   bool is_const = false;
   bool is_wild = false;
   bool is_wildx = false;
   bool is_stack = false;

   // 1. Check for const keyword (Bug #72)
   if (match(TOKEN_KW_CONST)) {
       is_const = true;
   }

   // 2. Check for Memory Strategy Keywords
   if (match(TOKEN_KW_WILD) || match(TOKEN_WILD)) {
       is_wild = true;
   } else if (match(TOKEN_KW_WILDX) || match(TOKEN_WILDX)) {
       is_wildx = true;
   } else if (match(TOKEN_KW_STACK) || match(TOKEN_STACK)) {
       is_stack = true;
   } else if (match(TOKEN_KW_GC) || match(TOKEN_GC)) {
       // Explicitly gc (default anyway)
   }

   // 3. Parse Type
   std::string type_name;
   if (isTypeToken(current.type)) {
       type_name = current.value;
       advance();
   } else {
       throw std::runtime_error("Expected type in variable declaration");
   }
   
   // Handle array types: int8[256] or int8[]
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
   
   // 4. Expect colon
   expect(TOKEN_COLON);
   
   // 5. Parse variable name
   Token name_tok = expect(TOKEN_IDENTIFIER);
   std::string var_name = name_tok.value;
   
   // 6. Create AST Node
   std::unique_ptr<Expression> initializer = nullptr;
   
   // 7. Handle Optional Assignment
   if (match(TOKEN_ASSIGN)) {
       initializer = parseExpr();
   } else {
       // Validation: Wild pointers MUST be initialized
       if (is_wild && context.strictMode) {
            throw std::runtime_error("Wild variables must be initialized immediately.");
       }
       // Const variables MUST be initialized
       if (is_const) {
            throw std::runtime_error("Const variables must be initialized.");
       }
   }

   auto decl = std::make_unique<VarDecl>(type_name, var_name, std::move(initializer));
   decl->is_wild = is_wild;
   decl->is_wildx = is_wildx;
   decl->is_stack = is_stack;
   decl->is_const = is_const;

   // 8. Terminator
   match(TOKEN_SEMICOLON);  // Optional semicolon

   return decl;
}

} // namespace frontend
} // namespace aria
