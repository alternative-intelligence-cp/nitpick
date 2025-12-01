#include "lexer.h"
#include <map>
#include <vector>
#include <cctype>

namespace aria {
namespace frontend {

// Constructor
AriaLexer::AriaLexer(std::string src) : source(src), pos(0), line(1), col(1) {
    stateStack.push(STATE_ROOT);
}

// Returns the current character without advancing position.
// Returns 0 as EOF sentinel. Note: This means source files containing
// null bytes (\0) are not supported - they will be treated as EOF.
// This is acceptable since null bytes are not valid in Aria source code.
char AriaLexer::peek() {
    return pos < source.length() ? source[pos] : 0;
}

// Returns the next character (peek + 1) without advancing position.
// Returns 0 as EOF sentinel.
char AriaLexer::peekNext() {
    return (pos < source.length() && pos + 1 < source.length()) ? source[pos + 1] : 0;
}

void AriaLexer::advance() {
    if (peek() == '\n') {
        line++;
        col = 1;
    } else {
        col++;
    }
    pos++;
}

// Helper to parse identifier for sanitization check
std::string AriaLexer::parseIdentifier() {
    size_t start = pos;
    while (isalnum(peek()) || peek() == '_') advance();
    return source.substr(start, pos - start);
}

Token AriaLexer::nextToken() {
       char c = peek();
       if (c == 0) return {TOKEN_EOF, "", line, col};

       // Skip Whitespace
       while (isspace(c)) {
           advance();
           c = peek();
           if (c == 0) return {TOKEN_EOF, "", line, col};
       }

       // Handle Comments
       // Line comments: // text until newline
       // Block comments: /* text */ (can span multiple lines)
       if (c == '/') {
           char next = peekNext();
           
           // Line comment: // ...
           if (next == '/') {
               advance(); // Skip first /
               advance(); // Skip second /
               // Consume until newline or EOF
               while (peek() != '\n' && peek() != 0) {
                   advance();
               }
               // Recursively get next token (after the comment)
               return nextToken();
           }
           
           // Block comment: /* ... */
           if (next == '*') {
               advance(); // Skip /
               advance(); // Skip *
               
               // Consume until we find */
               while (true) {
                   c = peek();
                   if (c == 0) {
                       // Unterminated block comment
                       return {TOKEN_INVALID, "UNTERMINATED_BLOCK_COMMENT", line, col};
                   }
                   if (c == '*' && peekNext() == '/') {
                       advance(); // Skip *
                       advance(); // Skip /
                       break;
                   }
                   advance();
               }
               // Recursively get next token (after the comment)
               return nextToken();
           }
           // If it's not a comment, fall through to handle / as division operator
       }

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

           // Consume string content between interpolations
           // This handles escape sequences and regular characters
           std::string content;
           while (c != '`' && c != '&' && c != 0) {
               if (c == '\\') {
                   // Escape Sequence Handling
                   advance();
                   char next = peek();
                   if (next == 'n') content += '\n';
                   else if (next == 't') content += '\t';
                   else if (next == '\\') content += '\\';
                   else if (next == '`') content += '`';
                   else content += next; // Unknown escape, preserve as-is
                   advance();
               } else {
                   content += c;
                   advance();
               }
               c = peek();
           }

           if (!content.empty()) {
               return {TOKEN_STRING_CONTENT, content, line, col};
           }
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

       // Identifiers and Keywords
       if (isalpha(c) || c == '_') {
           size_t start_line = line, start_col = col;
           std::string identifier = parseIdentifier();
           
           // Keyword lookup table
           static const std::map<std::string, TokenType> keywords = {
               // Control flow
               {"func", TOKEN_KW_FUNC},
               {"return", TOKEN_KW_RETURN},
               {"if", TOKEN_KW_IF},
               {"else", TOKEN_KW_ELSE},
               {"pick", TOKEN_KW_PICK},
               {"when", TOKEN_KW_WHEN},
               {"till", TOKEN_KW_TILL},
               {"defer", TOKEN_KW_DEFER},
               
               // Memory management
               {"wild", TOKEN_KW_WILD},
               {"stack", TOKEN_KW_STACK},
               {"pin", TOKEN_KW_PIN},
               {"unpin", TOKEN_KW_UNPIN},
               
               // Type system
               {"Result", TOKEN_KW_RESULT},
               {"struct", TOKEN_KW_STRUCT},
               {"enum", TOKEN_KW_ENUM},
               {"type", TOKEN_KW_TYPE},
               {"mut", TOKEN_KW_MUT},
               {"pub", TOKEN_KW_PUB},
               
               // Module system
               {"import", TOKEN_KW_IMPORT},
               {"export", TOKEN_KW_EXPORT},
               
               // Primitive types
               {"void", TOKEN_TYPE_VOID},
               {"bool", TOKEN_TYPE_BOOL},
               {"int1", TOKEN_TYPE_INT1},
               {"int8", TOKEN_TYPE_INT8},
               {"int16", TOKEN_TYPE_INT16},
               {"int32", TOKEN_TYPE_INT32},
               {"int64", TOKEN_TYPE_INT64},
               {"int128", TOKEN_TYPE_INT128},
               {"int256", TOKEN_TYPE_INT256},
               {"int512", TOKEN_TYPE_INT512},
               {"trit", TOKEN_TYPE_TRIT},
               {"tryte", TOKEN_TYPE_TRYTE},
               {"byte", TOKEN_TYPE_BYTE},
               {"flt32", TOKEN_TYPE_FLT32},
               {"flt64", TOKEN_TYPE_FLT64},
               {"string", TOKEN_TYPE_STRING}
           };
           
           // Check if identifier is a keyword
           auto it = keywords.find(identifier);
           if (it != keywords.end()) {
               return {it->second, identifier, start_line, start_col};
           }
           
           // Not a keyword, return as identifier
           return {TOKEN_IDENTIFIER, identifier, start_line, start_col};
       }

       // Numbers (basic integer literal parsing)
       if (isdigit(c)) {
           size_t start_line = line, start_col = col;
           std::string number;
           while (isdigit(peek())) {
               number += peek();
               advance();
           }
           return {TOKEN_INT_LITERAL, number, start_line, start_col};
       }

       //... Standard tokenization logic...
       return {TOKEN_INVALID, "UNKNOWN", line, col};
}

} // namespace frontend
} // namespace aria
