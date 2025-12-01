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

   // Returns the current character without advancing position.
   // Returns 0 as EOF sentinel. Note: This means source files containing
   // null bytes (\0) are not supported - they will be treated as EOF.
   // This is acceptable since null bytes are not valid in Aria source code.
   char peek() { return pos < source.length()? source[pos] : 0; }

   // Returns the next character (peek + 1) without advancing position.
   // Returns 0 as EOF sentinel.
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
      
       // Symbol Sanitization: Directive Validation
       // The '@' operator is used for:
       // 1. Taking addresses of pinned objects: @pinned_var
       // 2. Compiler directives: @inline, @noinline, etc.
       // The spec mentions rejecting unauthorized tokens like @tesla_sync.
       if (c == '@') {
           advance();
           // Check if what follows is an identifier (directive) or just the operator
           if (isalpha(peek())) {
               // Save all position state before lookahead
               size_t saved_pos = pos;
               size_t saved_line = line;
               size_t saved_col = col;

               std::string directive = parseIdentifier();

               // TODO: Implement proper directive whitelist
               // Currently blocks 'tesla' as placeholder (per spec: reject @tesla_sync)
               // Should be replaced with comprehensive directive validation:
               // - Known directives: inline, noinline, pack, align, etc.
               // - Everything else: check in parser if it's a valid @ address-of
               if (directive.find("tesla") != std::string::npos)
                   return {TOKEN_INVALID, "ILLEGAL_SYMBOL", line, col};

               // If it's just @varname, it's valid but we need to verify
               // in parser phase. For lexer, we just emit TOKEN_AT.
               // We reset all position state to let the parser consume the identifier next.
               pos = saved_pos;
               line = saved_line;
               col = saved_col;
           }
           return {TOKEN_AT, "@", line, col};
       }

       //... Standard tokenization logic...
       return {TOKEN_INVALID, "UNKNOWN", line, col};
   }
};

