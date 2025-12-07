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

       // Recursive String Template Logic with Nesting Support (Bug #60)
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
       }
       
       // Handle closing brace in interpolation to support nesting
       // This allows: `outer &{`inner &{x}`} end` to work correctly
       if (stateStack.size() > 1 && stateStack.top() == STATE_INTERPOLATION) {
           if (c == '}') {
               advance();
               stateStack.pop();  // Pop interpolation state, return to template
               return {TOKEN_RBRACE, "}", line, col};
           }
           // Inside interpolation, can have nested backticks
           if (c == '`') {
               advance();
               stateStack.push(STATE_STRING_TEMPLATE);
               return {TOKEN_BACKTICK, "`", line, col};
           }
           // Otherwise, fall through to normal token parsing
       }
       
       // Continue with template literal content parsing
       if (stateStack.top() == STATE_STRING_TEMPLATE) {

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

               // DIRECTIVE WHITELIST (v0.0.7 Security Fix)
               // Comprehensive list of valid compiler directives
               // If a directive is not in this whitelist, we treat @ as address-of operator
               static const std::set<std::string> valid_directives = {
                   // Function attributes
                   "inline",           // Force inline expansion
                   "noinline",         // Prevent inlining
                   "pure",             // Function has no side effects
                   "const",            // Function result depends only on arguments
                   "volatile",         // Access volatile memory
                   
                   // Memory and alignment
                   "pack",             // Pack struct fields tightly
                   "align",            // Specify alignment
                   "section",          // Place symbol in specific section
                   
                   // Visibility and linkage
                   "export",           // Export symbol for dynamic linking
                   "import",           // Import external symbol
                   "external",         // External linkage
                   "internal",         // Internal linkage
                   
                   // Optimization hints
                   "optimize",         // Optimization level hints
                   "target",           // Target-specific code generation
                   "frame_pointer",    // Control frame pointer generation
                   "hot",              // Frequently executed code
                   "cold",             // Rarely executed code
                   "likely",           // Branch prediction hint
                   "unlikely",         // Branch prediction hint
                   
                   // Safety and analysis
                   "unsafe",           // Mark unsafe operations
                   "trusted",          // Trusted code (skip some checks)
                   "deprecated",       // Deprecated API
                   
                   // Platform-specific
                   "x86",              // x86-specific code
                   "arm",              // ARM-specific code
                   "riscv",            // RISC-V specific code
               };

               if (valid_directives.find(directive) != valid_directives.end()) {
                   // It is a valid directive - return as TOKEN_DIRECTIVE
                   return {TOKEN_DIRECTIVE, "@" + directive, saved_line, saved_col};
               }

               // Check for reserved/illegal directives (security)
               // Reject directives containing "tesla" or starting with "internal_"
               if (directive.find("tesla") != std::string::npos ||
                   directive.find("internal_") == 0) {
                   return {TOKEN_INVALID, "ILLEGAL_DIRECTIVE: @" + directive, saved_line, saved_col};
               }

               // If not a valid directive, treat @ as address-of operator
               // Backtrack to allow parser to handle @ as operator and identifier as operand
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
               {"return", TOKEN_KW_RETURN},
               {"if", TOKEN_KW_IF},
               {"else", TOKEN_KW_ELSE},
               {"pick", TOKEN_KW_PICK},
               {"when", TOKEN_KW_WHEN},
               {"till", TOKEN_KW_TILL},
               {"defer", TOKEN_KW_DEFER},
               {"for", TOKEN_KW_FOR},
               {"while", TOKEN_KW_WHILE},
               {"in", TOKEN_KW_IN},
               {"then", TOKEN_KW_THEN},
               {"end", TOKEN_KW_END},
               {"fall", TOKEN_KW_FALL},
               {"break", TOKEN_KW_BREAK},
               {"continue", TOKEN_KW_CONTINUE},
               {"async", TOKEN_KW_ASYNC},
               {"await", TOKEN_KW_AWAIT},
               {"catch", TOKEN_KW_CATCH},
               
               // Memory management
               {"wild", TOKEN_KW_WILD},
               {"wildx", TOKEN_KW_WILDX},
               {"stack", TOKEN_KW_STACK},
               {"gc", TOKEN_KW_GC},
               {"pin", TOKEN_KW_PIN},
               {"unpin", TOKEN_KW_UNPIN},
               {"const", TOKEN_KW_CONST},
               
               // Type system
               {"struct", TOKEN_KW_STRUCT},
               {"enum", TOKEN_KW_ENUM},
               {"type", TOKEN_KW_TYPE},
               {"mut", TOKEN_KW_MUT},
               {"pub", TOKEN_KW_PUB},

        // Boolean literals
        {"true", TOKEN_KW_TRUE},
        {"false", TOKEN_KW_FALSE},
        {"NULL", TOKEN_KW_NULL},        // Null pointer literal
        {"ERR", TOKEN_KW_ERR},          // TBB error sentinel
        
        // Result helpers (syntactic sugar)
        {"fail", TOKEN_KW_FAIL},
        {"pass", TOKEN_KW_PASS},

        // Ternary operator
        {"is", TOKEN_KW_IS},               // Module system
               {"use", TOKEN_KW_USE},
               {"mod", TOKEN_KW_MOD},
               {"extern", TOKEN_KW_EXTERN},
               {"cfg", TOKEN_KW_CFG},
               {"import", TOKEN_KW_IMPORT},
               {"export", TOKEN_KW_EXPORT},
               
               // Primitive types - Void and Bool
               {"void", TOKEN_TYPE_VOID},
               {"bool", TOKEN_TYPE_BOOL},
               
               // Integer types (signed)
               {"int1", TOKEN_TYPE_INT1},
               {"int2", TOKEN_TYPE_INT2},
               {"int4", TOKEN_TYPE_INT4},
               {"int8", TOKEN_TYPE_INT8},
               {"int16", TOKEN_TYPE_INT16},
               {"int32", TOKEN_TYPE_INT32},
               {"int64", TOKEN_TYPE_INT64},
               {"int128", TOKEN_TYPE_INT128},
               {"int256", TOKEN_TYPE_INT256},
               {"int512", TOKEN_TYPE_INT512},
               
               // Integer types (unsigned)
               {"uint1", TOKEN_TYPE_UINT1},
               {"uint2", TOKEN_TYPE_UINT2},
               {"uint4", TOKEN_TYPE_UINT4},
               {"uint8", TOKEN_TYPE_UINT8},
               {"uint16", TOKEN_TYPE_UINT16},
               {"uint32", TOKEN_TYPE_UINT32},
               {"uint64", TOKEN_TYPE_UINT64},
               {"uint128", TOKEN_TYPE_UINT128},
               {"uint256", TOKEN_TYPE_UINT256},
               {"uint512", TOKEN_TYPE_UINT512},
               
               // Exotic types (NON-NEGOTIABLE per spec)
               {"trit", TOKEN_TYPE_TRIT},
               {"tryte", TOKEN_TYPE_TRYTE},
               {"nit", TOKEN_TYPE_NIT},
               {"nyte", TOKEN_TYPE_NYTE},
               
               // Twisted Balanced Binary types (symmetric with error sentinel)
               {"tbb8", TOKEN_TYPE_TBB8},
               {"tbb16", TOKEN_TYPE_TBB16},
               {"tbb32", TOKEN_TYPE_TBB32},
               {"tbb64", TOKEN_TYPE_TBB64},
               
               // Float types
               {"flt32", TOKEN_TYPE_FLT32},
               {"flt64", TOKEN_TYPE_FLT64},
               {"flt128", TOKEN_TYPE_FLT128},
               {"flt256", TOKEN_TYPE_FLT256},
               {"flt512", TOKEN_TYPE_FLT512},
               
               // Vector types
               {"vec2", TOKEN_TYPE_VEC2},
               {"vec3", TOKEN_TYPE_VEC3},
               {"vec9", TOKEN_TYPE_VEC9},
               
               // Compound types
               {"byte", TOKEN_TYPE_BYTE},
               {"string", TOKEN_TYPE_STRING},
               {"func", TOKEN_TYPE_FUNC},
               {"result", TOKEN_TYPE_RESULT},
               {"dyn", TOKEN_TYPE_DYN},
               {"obj", TOKEN_TYPE_OBJ},
               {"array", TOKEN_TYPE_ARRAY},
               {"tensor", TOKEN_TYPE_TENSOR},
               {"matrix", TOKEN_TYPE_MATRIX},
               
               // System types
               {"binary", TOKEN_TYPE_BINARY},
               {"buffer", TOKEN_TYPE_BUFFER},
               {"stream", TOKEN_TYPE_STREAM},
               {"process", TOKEN_TYPE_PROCESS},
               {"pipe", TOKEN_TYPE_PIPE}
           };
           
           // Check if identifier is a keyword
           auto it = keywords.find(identifier);
           if (it != keywords.end()) {
               return {it->second, identifier, start_line, start_col};
           }
           
           // Not a keyword, return as identifier
           return {TOKEN_IDENTIFIER, identifier, start_line, start_col};
       }

       // Numeric Literals: Integers (decimal, hex, binary, octal) and Floating-point
       if (isdigit(c)) {
           size_t start_line = line, start_col = col;
           std::string number;

           // Special prefixes for non-decimal bases
           if (c == '0' && pos + 1 < source.length()) {
               char prefix = peekNext();

               // Hexadecimal: 0x[0-9a-fA-F]+
               if (prefix == 'x' || prefix == 'X') {
                   number += peek(); advance(); // consume '0'
                   number += peek(); advance(); // consume 'x'

                   if (!isxdigit(peek())) {
                       return {TOKEN_INVALID, "INVALID_HEX_LITERAL", start_line, start_col};
                   }

                   while (isxdigit(peek()) || peek() == '_') {
                       if (peek() != '_') number += peek();
                       advance();
                   }
                   return {TOKEN_INT_LITERAL, number, start_line, start_col};
               }

               // Binary: 0b[01]+
               if (prefix == 'b' || prefix == 'B') {
                   number += peek(); advance(); // consume '0'
                   number += peek(); advance(); // consume 'b'

                   if (peek() != '0' && peek() != '1') {
                       return {TOKEN_INVALID, "INVALID_BINARY_LITERAL", start_line, start_col};
                   }

                   while (peek() == '0' || peek() == '1' || peek() == '_') {
                       if (peek() != '_') number += peek();
                       advance();
                   }
                   return {TOKEN_INT_LITERAL, number, start_line, start_col};
               }

               // Octal: 0o[0-7]+
               if (prefix == 'o' || prefix == 'O') {
                   number += peek(); advance(); // consume '0'
                   number += peek(); advance(); // consume 'o'

                   if (peek() < '0' || peek() > '7') {
                       return {TOKEN_INVALID, "INVALID_OCTAL_LITERAL", start_line, start_col};
                   }

                   while ((peek() >= '0' && peek() <= '7') || peek() == '_') {
                       if (peek() != '_') number += peek();
                       advance();
                   }
                   return {TOKEN_INT_LITERAL, number, start_line, start_col};
               }
           }

           // Decimal integer or floating-point
           // Consume integer part
           while (isdigit(peek()) || peek() == '_') {
               if (peek() != '_') number += peek();
               advance();
           }

           // Check for decimal point (floating-point)
           if (peek() == '.' && isdigit(peekNext())) {
               number += peek(); advance(); // consume '.'

               // Consume fractional part
               while (isdigit(peek()) || peek() == '_') {
                   if (peek() != '_') number += peek();
                   advance();
               }

               // Check for exponent (e or E)
               if (peek() == 'e' || peek() == 'E') {
                   number += peek(); advance(); // consume 'e'

                   // Optional sign
                   if (peek() == '+' || peek() == '-') {
                       number += peek(); advance();
                   }

                   // Exponent digits
                   if (!isdigit(peek())) {
                       return {TOKEN_INVALID, "INVALID_FLOAT_EXPONENT", start_line, start_col};
                   }

                   while (isdigit(peek()) || peek() == '_') {
                       if (peek() != '_') number += peek();
                       advance();
                   }
               }

               return {TOKEN_FLOAT_LITERAL, number, start_line, start_col};
           }

           // Check for exponent without decimal point (e.g., 1e10)
           if (peek() == 'e' || peek() == 'E') {
               number += peek(); advance(); // consume 'e'

               // Optional sign
               if (peek() == '+' || peek() == '-') {
                   number += peek(); advance();
               }

               // Exponent digits
               if (!isdigit(peek())) {
                   return {TOKEN_INVALID, "INVALID_FLOAT_EXPONENT", start_line, start_col};
               }

               while (isdigit(peek()) || peek() == '_') {
                   if (peek() != '_') number += peek();
                   advance();
               }

               return {TOKEN_FLOAT_LITERAL, number, start_line, start_col};
           }

           // Plain decimal integer
           return {TOKEN_INT_LITERAL, number, start_line, start_col};
       }

       // Character literals (single-quoted)
       if (c == '\'') {
           size_t start_line = line, start_col = col;
           advance(); // Skip opening '
           
           if (peek() == '\'') {
               // Empty char literal - error
               return {TOKEN_INVALID, "EMPTY_CHAR_LITERAL", start_line, start_col};
           }
           
           std::string ch;
           if (peek() == '\\') {
               // Escape sequence
               advance(); // Skip backslash
               char next = peek();
               
               if (next == 'n') {
                   ch += '\n';
                   advance();
               }
               else if (next == 't') {
                   ch += '\t';
                   advance();
               }
               else if (next == 'r') {
                   ch += '\r';
                   advance();
               }
               else if (next == '\\') {
                   ch += '\\';
                   advance();
               }
               else if (next == '\'') {
                   ch += '\'';
                   advance();
               }
               else if (next == '0') {
                   ch += '\0';
                   advance();
               }
               else if (next == 'x') {
                   // Hex escape: \xHH
                   advance(); // Skip 'x'
                   std::string hex;
                   
                   // Collect up to 2 hex digits
                   if (isxdigit(peek())) {
                       hex += peek();
                       advance();
                   }
                   if (isxdigit(peek())) {
                       hex += peek();
                       advance();
                   }
                   
                   if (hex.empty()) {
                       return {TOKEN_INVALID, "INVALID_HEX_ESCAPE", start_line, start_col};
                   }
                   
                   // Convert hex string to character
                   int hex_value = std::stoi(hex, nullptr, 16);
                   ch += static_cast<char>(hex_value);
                   // Already advanced past hex digits
               }
               else {
                   // Unknown escape - just include the character
                   ch += next;
                   advance();
               }
           } else {
               // Regular character
               ch += peek();
               advance();
           }
           
           if (peek() != '\'') {
               // Check for multi-char or unterminated
               if (peek() == 0) {
                   return {TOKEN_INVALID, "UNTERMINATED_CHAR_LITERAL", start_line, start_col};
               }
               return {TOKEN_INVALID, "MULTI_CHAR_LITERAL", start_line, start_col};
           }
           
           advance(); // Skip closing '
           return {TOKEN_CHAR_LITERAL, ch, start_line, start_col};
       }

       // String literals (double-quoted strings)
       if (c == '"') {
           size_t start_line = line, start_col = col;
           advance(); // Skip opening "
           std::string str;
           while (peek() != '"' && peek() != 0) {
               if (peek() == '\\') {
                   advance();
                   char next = peek();
                   if (next == 'n') str += '\n';
                   else if (next == 't') str += '\t';
                   else if (next == 'r') str += '\r';
                   else if (next == '\\') str += '\\';
                   else if (next == '"') str += '"';
                   else if (next == '0') str += '\0';
                   else str += next;
                   advance();
               } else {
                   str += peek();
                   advance();
               }
           }
           if (peek() == 0) {
               return {TOKEN_INVALID, "UNTERMINATED_STRING", start_line, start_col};
           }
           advance(); // Skip closing "
           return {TOKEN_STRING_LITERAL, str, start_line, start_col};
       }

       // Template string literals (backtick)
       if (c == '`') {
           advance();
           stateStack.push(STATE_STRING_TEMPLATE);
           return {TOKEN_BACKTICK, "`", line, col};
       }

       // OPERATOR TOKENIZATION
       // Multi-character operators (maximal munch) and single-character operators
       size_t op_line = line, op_col = col;

       // Division and division-assign (/, /=)
       // Note: // and /* are handled earlier as comments
       if (c == '/') {
           advance();
           if (peek() == '=') {
               advance();
               return {TOKEN_SLASH_ASSIGN, "/=", op_line, op_col};
           }
           return {TOKEN_SLASH, "/", op_line, op_col};
       }

       // Plus, plus-assign, and increment (+, +=, ++)
       if (c == '+') {
           advance();
           if (peek() == '=') {
               advance();
               return {TOKEN_PLUS_ASSIGN, "+=", op_line, op_col};
           }
           if (peek() == '+') {
               advance();
               return {TOKEN_INCREMENT, "++", op_line, op_col};
           }
           return {TOKEN_PLUS, "+", op_line, op_col};
       }

       // Minus, minus-assign, arrow, and decrement (-, -=, ->, --)
       if (c == '-') {
           advance();
           if (peek() == '=') {
               advance();
               return {TOKEN_MINUS_ASSIGN, "-=", op_line, op_col};
           }
           if (peek() == '>') {
               advance();
               return {TOKEN_ARROW, "->", op_line, op_col};
           }
           if (peek() == '-') {
               advance();
               return {TOKEN_DECREMENT, "--", op_line, op_col};
           }
           return {TOKEN_MINUS, "-", op_line, op_col};
       }

       // Star and star-assign (*, *=)
       if (c == '*') {
           advance();
           if (peek() == '=') {
               advance();
               return {TOKEN_STAR_ASSIGN, "*=", op_line, op_col};
           }
           return {TOKEN_STAR, "*", op_line, op_col};
       }

       // Percent: either preprocessor directive or modulo operator
       if (c == '%') {
           size_t start_line = line, start_col = col;
           advance(); // consume '%'
           
           // Check for %=  (modulo-assign)
           if (peek() == '=') {
               advance();
               return {TOKEN_MOD_ASSIGN, "%=", start_line, start_col};
           }
           
           // Check for preprocessor directives:
           // - %$label (context-local)
           // - %1, %2, ... (macro parameters)
           // - %macro, %define, etc.
           
           // Context-local label: %$identifier
           if (peek() == '$') {
               advance(); // consume '$'
               std::string label;
               while (isalnum(peek()) || peek() == '_') {
                   label += peek();
                   advance();
               }
               if (label.empty()) {
                   return {TOKEN_INVALID, "INVALID_CONTEXT_LOCAL", start_line, start_col};
               }
               return {TOKEN_PREPROC_LOCAL, "%$" + label, start_line, start_col};
           }
           
           // Macro parameter: %1, %2, ...
           if (isdigit(peek())) {
               std::string param;
               while (isdigit(peek())) {
                   param += peek();
                   advance();
               }
               return {TOKEN_PREPROC_PARAM, "%" + param, start_line, start_col};
           }
           
           // Preprocessor directive: %macro, %define, etc.
           if (isalpha(peek()) || peek() == '_') {
               std::string directive;
               while (isalnum(peek()) || peek() == '_') {
                   directive += peek();
                   advance();
               }
               
               // Map directive name to token type
               if (directive == "macro") return {TOKEN_PREPROC_MACRO, "%macro", start_line, start_col};
               if (directive == "endmacro") return {TOKEN_PREPROC_ENDMACRO, "%endmacro", start_line, start_col};
               if (directive == "push") return {TOKEN_PREPROC_PUSH, "%push", start_line, start_col};
               if (directive == "pop") return {TOKEN_PREPROC_POP, "%pop", start_line, start_col};
               if (directive == "context") return {TOKEN_PREPROC_CONTEXT, "%context", start_line, start_col};
               if (directive == "define") return {TOKEN_PREPROC_DEFINE, "%define", start_line, start_col};
               if (directive == "undef") return {TOKEN_PREPROC_UNDEF, "%undef", start_line, start_col};
               if (directive == "ifdef") return {TOKEN_PREPROC_IFDEF, "%ifdef", start_line, start_col};
               if (directive == "ifndef") return {TOKEN_PREPROC_IFNDEF, "%ifndef", start_line, start_col};
               if (directive == "if") return {TOKEN_PREPROC_IF, "%if", start_line, start_col};
               if (directive == "elif") return {TOKEN_PREPROC_ELIF, "%elif", start_line, start_col};
               if (directive == "else") return {TOKEN_PREPROC_ELSE, "%else", start_line, start_col};
               if (directive == "endif") return {TOKEN_PREPROC_ENDIF, "%endif", start_line, start_col};
               if (directive == "include") return {TOKEN_PREPROC_INCLUDE, "%include", start_line, start_col};
               if (directive == "rep") return {TOKEN_PREPROC_REP, "%rep", start_line, start_col};
               if (directive == "endrep") return {TOKEN_PREPROC_ENDREP, "%endrep", start_line, start_col};
               
               // Unknown directive (could be a valid identifier extension in future)
               return {TOKEN_INVALID, "UNKNOWN_DIRECTIVE_%" + directive, start_line, start_col};
           }
           
           // Plain modulo operator: %
           return {TOKEN_PERCENT, "%", start_line, start_col};
       }

       // Ampersand and logical-and (&, &&, &=)
       // Note: &{ for template interpolation is handled in STATE_STRING_TEMPLATE
       if (c == '&') {
           advance();
           if (peek() == '&') {
               advance();
               return {TOKEN_LOGICAL_AND, "&&", op_line, op_col};
           }
           if (peek() == '=') {
               advance();
               return {TOKEN_AND_ASSIGN, "&=", op_line, op_col};
           }
           return {TOKEN_AMPERSAND, "&", op_line, op_col};
       }

       // Pipe, logical-or, and pipeline operators (|, ||, |>, |=)
       if (c == '|') {
           advance();
           if (peek() == '|') {
               advance();
               return {TOKEN_LOGICAL_OR, "||", op_line, op_col};
           }
           if (peek() == '>') {
               advance();
               return {TOKEN_PIPE_FORWARD, "|>", op_line, op_col};
           }
           if (peek() == '=') {
               advance();
               return {TOKEN_OR_ASSIGN, "|=", op_line, op_col};
           }
           return {TOKEN_PIPE, "|", op_line, op_col};
       }

       // Caret (^, ^=)
       if (c == '^') {
           advance();
           if (peek() == '=') {
               advance();
               return {TOKEN_XOR_ASSIGN, "^=", op_line, op_col};
           }
           return {TOKEN_CARET, "^", op_line, op_col};
       }

       // Tilde (~)
       if (c == '~') {
           advance();
           return {TOKEN_TILDE, "~", op_line, op_col};
       }
       
       // Less-than, less-or-equal, left-shift, spaceship, and pipeline backward (<, <=, <<, <<=, <=>, <|)
       if (c == '<') {
           advance();
           if (peek() == '=') {
               advance();
               // Check for spaceship operator <=>
               if (peek() == '>') {
                   advance();
                   return {TOKEN_SPACESHIP, "<=>", op_line, op_col};
               }
               return {TOKEN_LE, "<=", op_line, op_col};
           }
           if (peek() == '<') {
               advance();
               // Check for <<= (left shift assign)
               if (peek() == '=') {
                   advance();
                   return {TOKEN_LSHIFT_ASSIGN, "<<=", op_line, op_col};
               }
               return {TOKEN_LSHIFT, "<<", op_line, op_col};
           }
           if (peek() == '|') {
               advance();
               return {TOKEN_PIPE_BACKWARD, "<|", op_line, op_col};
           }
           return {TOKEN_LT, "<", op_line, op_col};
       }

       // Greater-than, greater-or-equal, right-shift (>, >=, >>, >>=)
       if (c == '>') {
           advance();
           if (peek() == '=') {
               advance();
               return {TOKEN_GE, ">=", op_line, op_col};
           }
           if (peek() == '>') {
               advance();
               // Check for >>= (right shift assign)
               if (peek() == '=') {
                   advance();
                   return {TOKEN_RSHIFT_ASSIGN, ">>=", op_line, op_col};
               }
               return {TOKEN_RSHIFT, ">>", op_line, op_col};
           }
           return {TOKEN_GT, ">", op_line, op_col};
       }

       // Equal and equals-comparison (=, ==, =>)
       if (c == '=') {
           advance();
           if (peek() == '=') {
               advance();
               return {TOKEN_EQ, "==", op_line, op_col};
           }
           if (peek() == '>') {
               advance();
               return {TOKEN_FAT_ARROW, "=>", op_line, op_col};
           }
           return {TOKEN_ASSIGN, "=", op_line, op_col};
       }

       // Not and not-equal (!, !=)
       if (c == '!') {
           advance();
           if (peek() == '=') {
               advance();
               return {TOKEN_NE, "!=", op_line, op_col};
           }
           return {TOKEN_LOGICAL_NOT, "!", op_line, op_col};
       }
       
       // Dot, range inclusive, and range exclusive (., .., ...)
       // Maximal munch: ... before ..
       if (c == '.') {
           advance();
           if (peek() == '.') {
               advance();
               // Check for exclusive range ...
               if (peek() == '.') {
                   advance();
                   return {TOKEN_RANGE_EXCLUSIVE, "...", op_line, op_col};
               }
               return {TOKEN_RANGE, "..", op_line, op_col};
           }
           return {TOKEN_DOT, ".", op_line, op_col};
       }

       // At sign / Address-of operator (@)
       if (c == '@') {
           advance();
           return {TOKEN_AT, "@", op_line, op_col};
       }

       // Hash/Pin operator (#)
       if (c == '#') {
           advance();
           return {TOKEN_HASH, "#", op_line, op_col};
       }

       // Dollar ($)
       if (c == '$') {
           advance();
           return {TOKEN_DOLLAR, "$", op_line, op_col};
       }

       // Question, unwrap, safe nav, null coalesce (?, ?., ??)
       // Maximal munch: ?? before ?. before ?
       if (c == '?') {
           advance();
           if (peek() == '?') {
               advance();
               return {TOKEN_NULL_COALESCE, "??", op_line, op_col};
           }
           if (peek() == '.') {
               advance();
               return {TOKEN_SAFE_NAV, "?.", op_line, op_col};
           }
           return {TOKEN_UNWRAP, "?", op_line, op_col};
       }

       // DELIMITERS
       if (c == '(') {
           advance();
           return {TOKEN_LPAREN, "(", op_line, op_col};
       }
       if (c == ')') {
           advance();
           return {TOKEN_RPAREN, ")", op_line, op_col};
       }
       if (c == '{') {
           advance();
           return {TOKEN_LBRACE, "{", op_line, op_col};
       }
       if (c == '}') {
           advance();
           return {TOKEN_RBRACE, "}", op_line, op_col};
       }
       if (c == '[') {
           advance();
           return {TOKEN_LBRACKET, "[", op_line, op_col};
       }
       if (c == ']') {
           advance();
           return {TOKEN_RBRACKET, "]", op_line, op_col};
       }
       if (c == ',') {
           advance();
           return {TOKEN_COMMA, ",", op_line, op_col};
       }
       if (c == ';') {
           advance();
           return {TOKEN_SEMICOLON, ";", op_line, op_col};
       }
       if (c == ':') {
           advance();
           // Check for double-colon ::
           if (peek() == ':') {
               advance();
               return {TOKEN_DOUBLE_COLON, "::", op_line, op_col};
           }
           return {TOKEN_COLON, ":", op_line, op_col};
       }

       // Template string literal start (backtick)
       if (c == '`') {
           size_t start_line = line, start_col = col;
           advance(); // consume opening backtick
           stateStack.push(STATE_STRING_TEMPLATE);
           return {TOKEN_BACKTICK, "`", start_line, start_col};
       }

       // Regular string literal (double quote)
       if (c == '"') {
           size_t start_line = line, start_col = col;
           advance(); // consume opening quote
           std::string content;
           while (peek() != '"' && peek() != 0) {
               if (peek() == '\\') {
                   advance();
                   char next = peek();
                   if (next == 'n') content += '\n';
                   else if (next == 't') content += '\t';
                   else if (next == 'r') content += '\r';
                   else if (next == '\\') content += '\\';
                   else if (next == '"') content += '"';
                   else if (next == '0') content += '\0';
                   else content += next; // Unknown escape
                   advance();
               } else {
                   content += peek();
                   advance();
               }
           }
           if (peek() == '"') advance(); // consume closing quote
           else return {TOKEN_INVALID, "UNTERMINATED_STRING", start_line, start_col};
           return {TOKEN_STRING_LITERAL, content, start_line, start_col};
       }

       // Unknown character
       return {TOKEN_INVALID, std::string(1, c), line, col};
}

} // namespace frontend
} // namespace aria
