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

// Parses: [const] [wild|stack|gc] Type:Identifier [= Expression];
// Grammar:
//   VarDecl -> "const"? ( "wild" | "stack" | "gc" )? TypeIdentifier ":" Identifier ( "=" Expression )? ";"
std::unique_ptr<VarDecl> Parser::parseVarDecl() {
   bool is_const = false;
   bool is_wild = false;
   bool is_stack = false;

   // 1. Check for const keyword (Bug #72)
   if (match(TOKEN_KW_CONST)) {
       is_const = true;
   }

   // 2. Check for Memory Strategy Keywords
   if (match(TOKEN_KW_WILD) || match(TOKEN_WILD)) {
       is_wild = true;
   } else if (match(TOKEN_KW_STACK) || match(TOKEN_STACK)) {
       is_stack = true;
   } else if (match(TOKEN_KW_GC) || match(TOKEN_GC)) {
       // Explicitly gc (default anyway)
   }

   // 3. Parse Type
   Token type_tok = expect(TOKEN_IDENTIFIER);
   std::string type_name = type_tok.value;
   
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
   decl->is_stack = is_stack;
   decl->is_const = is_const;

   // 8. Terminator
   match(TOKEN_SEMICOLON);  // Optional semicolon

   return decl;
}

} // namespace frontend
} // namespace aria
