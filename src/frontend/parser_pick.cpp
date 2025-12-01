// Implementation of the 'pick' statement parser
// Handles: pick(val) { (<9) {... }, success:(!) {... } }
#include "parser.h"
#include "ast/control_flow.h"
#include <memory>

std::unique_ptr<PickStmt> Parser::parsePickStmt() {
   consume(TOKEN_PICK, "Expected 'pick'");
   consume(TOKEN_LEFT_PAREN, "Expected '(' after 'pick'");
   
   auto stmt = std::make_unique<PickStmt>();
   stmt->selector = parseExpression();
   
   consume(TOKEN_RIGHT_PAREN, "Expected ')'");
   consume(TOKEN_LEFT_BRACE, "Expected '{' to begin pick body");

   // Parse Cases
   while (!check(TOKEN_RIGHT_BRACE) &&!check(TOKEN_EOF)) {
       PickCase pcase;
       // 1. Detect Match Type
       if (match(TOKEN_LEFT_PAREN)) {
           // Value or Range Match: (9), (<9), (>9), (*)
           if (match(TOKEN_LESS_THAN)) {
               pcase.type = PickCase::RANGE;
               pcase.value_end = parseExpression(); // Upper bound
               // Implicitly implies -Infinity to value_end
           } 
           else if (match(TOKEN_GREATER_THAN)) {
               pcase.type = PickCase::RANGE;
               pcase.value_start = parseExpression(); // Lower bound
           }
           else if (match(TOKEN_MULTIPLY)) { // '*' is wildcard
               pcase.type = PickCase::WILDCARD;
           }
           else {
               // Exact match
               pcase.type = PickCase::EXACT;
               pcase.value_start = parseExpression();
           }
           consume(TOKEN_RIGHT_PAREN, "Expected ')' after case condition");
       } 
       else if (check(TOKEN_IDENTIFIER)) {
           // Label match: success:(!)
           Token label = advance();
           pcase.label = label.lexeme;
           consume(TOKEN_COLON, "Expected ':' after label");
           consume(TOKEN_LEFT_PAREN, "Expected '('");
           consume(TOKEN_LOGICAL_NOT, "Expected '!' in label definition");
           consume(TOKEN_RIGHT_PAREN, "Expected ')'");
           pcase.type = PickCase::EXACT; // Logically treated as jump target
       }

       // 2. Parse Body
       consume(TOKEN_LEFT_BRACE, "Expected '{' for case body");
       pcase.body = parseBlock(); 
       // Note: parseBlock consumes the closing brace '}'

       // 3. Register Label mapping for CFG
       if (!pcase.label.empty()) {
           stmt->label_map[pcase.label] = stmt->cases.size();
       }

       stmt->cases.push_back(std::move(pcase));
       
       // Optional comma between cases
       match(TOKEN_COMMA);
   }

   consume(TOKEN_RIGHT_BRACE, "Expected '}' to end pick statement");
   return stmt;
}

