// Implementation of Variable Declaration Parsing with Colon Syntax Anchor
#include "parser.h"
#include "ast.h"
#include "lexer_utils.h"

// Parses: [wild|stack] Type:Identifier [= Expression];
// Grammar:
//   VarDecl -> ( "wild" | "stack" )? TypeIdentifier ":" Identifier ( "=" Expression )? ";"
std::unique_ptr<VarDecl> Parser::parseVarDecl() {
   bool is_wild = false;
   bool is_stack = false;

   // 1. Check for Memory Strategy Keywords
   // We explicitly check for 'wild' and 'stack'. If neither is present,
   // the variable defaults to 'managed' (GC).
   if (match(TOKEN_WILD)) {
       is_wild = true;
   } else if (match(TOKEN_STACK)) {
       is_stack = true;
   }

   // 2. Parse Type (e.g., int8, string, or user-defined struct)
   // We expect a TYPE_IDENTIFIER or an existing struct name.
   // In Aria, types are distinct tokens or identifiers known to be types.
   Token typeToken = consume(TOKEN_TYPE_IDENTIFIER, "Expected type name");
   
   // 3. The Anchor: Require Colon
   // This is the syntactic sugar that removes C++ style ambiguity.
   // If the colon is missing, this is NOT a declaration.
   consume(TOKEN_COLON, "Expected ':' after type in variable declaration");
   
   // 4. Parse Variable Name
   Token nameToken = consume(TOKEN_IDENTIFIER, "Expected variable name after ':'");
   
   // 5. Create AST Node
   auto decl = std::make_unique<VarDecl>(nameToken.lexeme, typeToken.lexeme);
   decl->is_wild = is_wild;
   decl->is_stack = is_stack;

   // 6. Handle Optional Assignment
   if (match(TOKEN_ASSIGN)) {
       decl->initializer = parseExpression();
   } else {
       // Validation: Wild pointers MUST be initialized or immediately unsafe.
       // Aria strict mode requires initialization to prevent using garbage values
       // from the unmanaged heap.
       if (is_wild && context->strictMode) {
            error("Wild variables must be initialized immediately.");
       }
   }

   // 7. Terminator
   consume(TOKEN_SEMICOLON, "Expected ';' after variable declaration");

   // 8. Register with Symbol Table for Scope Analysis
   // This allows subsequent code to resolve 'nameToken' as a variable.
   currentScope->define(decl->name, decl->type);

   return decl;
}

