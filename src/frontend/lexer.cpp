#include <stack>
#include <string>
#include <map>
#include <vector>
#include "tokens.h" // Includes the complete token list defined in Section 9

enum LexerState { STATE_ROOT, STATE_STRING_TEMPLATE, STATE_INTERPOLATION };

class AriaLexer {
private:
   std::string source;
   size_t pos = 0;
   size_t line = 1, col = 1;
   std::stack<LexerState> stateStack;

   char peek() { return pos < source.length()? source[pos] : 0; }
   char peekNext() { return (pos < source.length() && pos + 1 < source.length())? source[pos + 1] : 0; }
   
   void advance() { 
       if (peek() == '\n') { line++; col=1; } else { col++; } 
       pos++;
   }

   // Helper to parse identifier for sanitization check
   std::string parseIdentifier() {
       size_t start = pos;
       while (isalnum(peek()) || peek() == '_') advance();
       return source.substr(start, pos - start);
   }

public:
   AriaLexer(std::string src) : source(src) { stateStack.push(STATE_ROOT); }

   Token nextToken() {
       char c = peek();
       if (c == 0) return {TOKEN_EOF, "", line, col};

       // Recursive String Template Logic
       if (stateStack.top() == STATE_STRING_TEMPLATE) {
           if (c == '`') {
               advance();
               stateStack.pop();
               return {TOKEN_BACKTICK, "`", line, col};
           }
           if (c == '&' && peekNext() == '{') {
               advance();
               advance();
               stateStack.push(STATE_INTERPOLATION);
               return {TOKEN_INTERP_START, "&{", line, col};
           }
           //... consume string content...
       }
      
       // Symbol Sanitization: Reject @tesla
       if (c == '@') {
           advance();
           // Check if what follows is an identifier (directive) or just the operator
           if (isalpha(peek())) {
               size_t reset_pos = pos;
               std::string directive = parseIdentifier();
               
               // Explicitly ban unauthorized symbols
               if (directive.find("tesla")!= std::string::npos) 
                   return {TOKEN_INVALID, "ILLEGAL_SYMBOL", line, col};

               // If it's just @varname, it's valid, but we need to verify 
               // in parser phase. For lexer, we just emit TOKEN_AT.
               // We reset pos to let the parser consume the identifier next.
               pos = reset_pos;
           }
           return {TOKEN_AT, "@", line, col};
       }

       //... Standard tokenization logic...
       return {TOKEN_INVALID, "UNKNOWN", line, col};
   }
};

