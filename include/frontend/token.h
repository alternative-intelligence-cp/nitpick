#ifndef ARIA_TOKEN_H
#define ARIA_TOKEN_H

#include <string>
#include <cstdint>
#include <ostream>

namespace aria {
namespace frontend {

// ============================================================================
// TokenType Enum - Complete token classification
// ============================================================================
// Reference: aria_specs.txt, research_012 (types), research_002 (TBB)
// All possible tokens in Aria language

enum class TokenType {
    // ========================================================================
    // Keywords - Memory Qualifiers
    // ========================================================================
    TOKEN_KW_WILD,      // wild - opt-out of GC
    TOKEN_KW_WILDX,     // wildx - executable memory allocation (JIT)
    TOKEN_KW_STACK,     // stack - explicit stack allocation
    TOKEN_KW_GC,        // gc - explicit GC allocation
    TOKEN_KW_DEFER,     // defer - RAII-style cleanup
    
    // ========================================================================
    // Keywords - Control Flow
    // ========================================================================
    TOKEN_KW_IF,        // if
    TOKEN_KW_ELSE,      // else
    TOKEN_KW_WHILE,     // while
    TOKEN_KW_FOR,       // for
    TOKEN_KW_LOOP,      // loop(start, limit, step)
    TOKEN_KW_TILL,      // till(limit, step)
    TOKEN_KW_WHEN,      // when - conditional loop
    TOKEN_KW_THEN,      // then - when success branch
    TOKEN_KW_END,       // end - when failure branch
    TOKEN_KW_PICK,      // pick - switch/match statement
    TOKEN_KW_FALL,      // fall() - explicit fallthrough in pick
    TOKEN_KW_BREAK,     // break
    TOKEN_KW_CONTINUE,  // continue
    TOKEN_KW_RETURN,    // return (legacy, use pass/fail)
    TOKEN_KW_PASS,      // pass() - successful return
    TOKEN_KW_FAIL,      // fail() - error return
    
    // ========================================================================
    // Keywords - Async/Await
    // ========================================================================
    TOKEN_KW_ASYNC,     // async
    TOKEN_KW_AWAIT,     // await
    TOKEN_KW_CATCH,     // catch
    
    // ========================================================================
    // Keywords - Declarations
    // ========================================================================
    TOKEN_KW_FUNC,      // func - function declaration
    TOKEN_KW_STRUCT,    // struct - structure declaration
    TOKEN_KW_USE,       // use - import module
    TOKEN_KW_MOD,       // mod - define module
    TOKEN_KW_PUB,       // pub - public visibility
    TOKEN_KW_EXTERN,    // extern - external C functions
    TOKEN_KW_CONST,     // const - compile-time constant
    TOKEN_KW_CFG,       // cfg - conditional compilation
    
    // ========================================================================
    // Type Keywords - Integers (Signed)
    // ========================================================================
    TOKEN_KW_INT1,      // int1 - 1-bit signed
    TOKEN_KW_INT2,      // int2 - 2-bit signed
    TOKEN_KW_INT4,      // int4 - 4-bit signed
    TOKEN_KW_INT8,      // int8 - 8-bit signed
    TOKEN_KW_INT16,     // int16 - 16-bit signed
    TOKEN_KW_INT32,     // int32 - 32-bit signed
    TOKEN_KW_INT64,     // int64 - 64-bit signed
    TOKEN_KW_INT128,    // int128 - 128-bit signed
    TOKEN_KW_INT256,    // int256 - 256-bit signed
    TOKEN_KW_INT512,    // int512 - 512-bit signed
    
    // ========================================================================
    // Type Keywords - Integers (Unsigned)
    // ========================================================================
    TOKEN_KW_UINT8,     // uint8 - 8-bit unsigned
    TOKEN_KW_UINT16,    // uint16 - 16-bit unsigned
    TOKEN_KW_UINT32,    // uint32 - 32-bit unsigned
    TOKEN_KW_UINT64,    // uint64 - 64-bit unsigned
    TOKEN_KW_UINT128,   // uint128 - 128-bit unsigned
    TOKEN_KW_UINT256,   // uint256 - 256-bit unsigned
    TOKEN_KW_UINT512,   // uint512 - 512-bit unsigned
    
    // ========================================================================
    // Type Keywords - TBB (Twisted Balanced Binary)
    // ========================================================================
    // Symmetric ranges with ERR sentinel at minimum value
    TOKEN_KW_TBB8,      // tbb8 - [-127, +127], ERR = -128
    TOKEN_KW_TBB16,     // tbb16 - [-32767, +32767], ERR = -32768
    TOKEN_KW_TBB32,     // tbb32 - symmetric 32-bit, ERR at min
    TOKEN_KW_TBB64,     // tbb64 - symmetric 64-bit, ERR at min
    
    // ========================================================================
    // Type Keywords - Floating Point
    // ========================================================================
    TOKEN_KW_FLT32,     // flt32 - 32-bit float
    TOKEN_KW_FLT64,     // flt64 - 64-bit float (double)
    TOKEN_KW_FLT128,    // flt128 - 128-bit float
    TOKEN_KW_FLT256,    // flt256 - 256-bit float
    TOKEN_KW_FLT512,    // flt512 - 512-bit float
    
    // ========================================================================
    // Type Keywords - Special/Composite
    // ========================================================================
    TOKEN_KW_BOOL,      // bool - boolean type
    TOKEN_KW_STRING,    // string - string type
    TOKEN_KW_DYN,       // dyn - dynamic type
    TOKEN_KW_OBJ,       // obj - object type
    TOKEN_KW_RESULT,    // result - result type with {err, val}
    TOKEN_KW_ARRAY,     // array - array type marker
    
    // ========================================================================
    // Type Keywords - Balanced Ternary/Nonary
    // ========================================================================
    TOKEN_KW_TRIT,      // trit - balanced ternary digit (-1, 0, 1)
    TOKEN_KW_TRYTE,     // tryte - 10 trits in uint16
    TOKEN_KW_NIT,       // nit - balanced nonary digit (-4..+4)
    TOKEN_KW_NYTE,      // nyte - 5 nits in uint16
    
    // ========================================================================
    // Type Keywords - Mathematical
    // ========================================================================
    TOKEN_KW_VEC2,      // vec2 - 2D vector
    TOKEN_KW_VEC3,      // vec3 - 3D vector
    TOKEN_KW_VEC9,      // vec9 - 9D vector
    TOKEN_KW_MATRIX,    // matrix - matrix type
    TOKEN_KW_TENSOR,    // tensor - tensor type
    
    // ========================================================================
    // Type Keywords - I/O and System
    // ========================================================================
    TOKEN_KW_BINARY,    // binary - binary data type
    TOKEN_KW_BUFFER,    // buffer - buffer type
    TOKEN_KW_STREAM,    // stream - stream type
    TOKEN_KW_PROCESS,   // process - process handle
    TOKEN_KW_PIPE,      // pipe - pipe handle
    TOKEN_KW_DEBUG,     // debug - debug session type
    TOKEN_KW_LOG,       // log - logger type
    
    // ========================================================================
    // Special Keywords
    // ========================================================================
    TOKEN_KW_IS,        // is - ternary condition keyword
    TOKEN_KW_NULL,      // NULL - null value
    TOKEN_KW_TRUE,      // true - boolean literal
    TOKEN_KW_FALSE,     // false - boolean literal
    TOKEN_KW_ERR,       // ERR - TBB error sentinel
    
    // ========================================================================
    // Operators - Arithmetic
    // ========================================================================
    TOKEN_PLUS,         // +
    TOKEN_MINUS,        // -
    TOKEN_STAR,         // *
    TOKEN_SLASH,        // /
    TOKEN_PERCENT,      // %
    TOKEN_PLUS_PLUS,    // ++
    TOKEN_MINUS_MINUS,  // --
    
    // ========================================================================
    // Operators - Assignment
    // ========================================================================
    TOKEN_EQUAL,        // =
    TOKEN_PLUS_EQUAL,   // +=
    TOKEN_MINUS_EQUAL,  // -=
    TOKEN_STAR_EQUAL,   // *=
    TOKEN_SLASH_EQUAL,  // /=
    TOKEN_PERCENT_EQUAL,// %=
    
    // ========================================================================
    // Operators - Comparison
    // ========================================================================
    TOKEN_EQUAL_EQUAL,  // ==
    TOKEN_BANG_EQUAL,   // !=
    TOKEN_LESS,         // <
    TOKEN_LESS_EQUAL,   // <=
    TOKEN_GREATER,      // >
    TOKEN_GREATER_EQUAL,// >=
    TOKEN_SPACESHIP,    // <=> (three-way comparison)
    
    // ========================================================================
    // Operators - Logical
    // ========================================================================
    TOKEN_AND_AND,      // &&
    TOKEN_OR_OR,        // ||
    TOKEN_BANG,         // !
    
    // ========================================================================
    // Operators - Bitwise
    // ========================================================================
    TOKEN_AMPERSAND,    // & (bitwise AND, string interpolation prefix)
    TOKEN_PIPE,         // | (bitwise OR)
    TOKEN_CARET,        // ^ (bitwise XOR)
    TOKEN_TILDE,        // ~ (bitwise NOT)
    TOKEN_SHIFT_LEFT,   // <<
    TOKEN_SHIFT_RIGHT,  // >>
    
    // ========================================================================
    // Operators - Special
    // ========================================================================
    TOKEN_AT,           // @ - address/pointer operator
    TOKEN_DOLLAR,       // $ - iteration variable, safe reference
    TOKEN_HASH,         // # - memory pinning operator
    TOKEN_ARROW,        // -> - pointer member dereference (ptr->member)
    TOKEN_SAFE_NAV,     // ?. - safe navigation
    TOKEN_NULL_COALESCE,// ?? - null coalescing
    TOKEN_QUESTION,     // ? - unwrap operator
    TOKEN_PIPE_RIGHT,   // |> - pipeline forward
    TOKEN_PIPE_LEFT,    // <| - pipeline backward
    TOKEN_DOT_DOT,      // .. - inclusive range
    TOKEN_DOT_DOT_DOT,  // ... - exclusive range
    
    // ========================================================================
    // Template Literals
    // ========================================================================
    TOKEN_BACKTICK,         // ` - template literal delimiter
    TOKEN_TEMPLATE_START,   // ` at start of template
    TOKEN_TEMPLATE_PART,    // text between interpolations
    TOKEN_INTERP_START,     // &{ - interpolation start
    TOKEN_INTERP_END,       // } - interpolation end (contextual)
    TOKEN_TEMPLATE_END,     // ` at end of template
    
    // ========================================================================
    // Punctuation
    // ========================================================================
    TOKEN_DOT,          // .
    TOKEN_COMMA,        // ,
    TOKEN_COLON,        // :
    TOKEN_SEMICOLON,    // ;
    TOKEN_LEFT_PAREN,   // (
    TOKEN_RIGHT_PAREN,  // )
    TOKEN_LEFT_BRACE,   // {
    TOKEN_RIGHT_BRACE,  // }
    TOKEN_LEFT_BRACKET, // [
    TOKEN_RIGHT_BRACKET,// ]
    
    // ========================================================================
    // Literals
    // ========================================================================
    TOKEN_INTEGER,      // Integer literal (decimal, hex, binary, octal)
    TOKEN_FLOAT,        // Float literal
    TOKEN_STRING,       // String literal "..."
    TOKEN_CHAR,         // Character literal '...'
    
    // ========================================================================
    // Identifiers and Special Tokens
    // ========================================================================
    TOKEN_IDENTIFIER,   // Variable names, function names
    TOKEN_EOF,          // End of file
    TOKEN_ERROR,        // Error token with message
    
    // ========================================================================
    // Comments and Whitespace (typically filtered)
    // ========================================================================
    TOKEN_COMMENT,      // Comment (if not skipped)
    TOKEN_WHITESPACE,   // Whitespace (if not skipped)
};

// ============================================================================
// Token - Represents a single token from source code
// ============================================================================

struct Token {
    TokenType type;
    std::string lexeme;      // Raw text from source
    int line;                // Line number (1-indexed)
    int column;              // Column number (1-indexed)
    
    // Value storage (union for efficiency)
    union {
        int64_t int_value;
        double float_value;
        bool bool_value;
    } value;
    
    std::string string_value; // For strings (separate due to complex type)
    
    // Constructors
    Token();
    explicit Token(TokenType t, const std::string& lex, int ln, int col);
    Token(TokenType t, const std::string& lex, int ln, int col, int64_t val);
    Token(TokenType t, const std::string& lex, int ln, int col, double val);
    Token(TokenType t, const std::string& lex, int ln, int col, bool val);
    Token(TokenType t, const std::string& lex, int ln, int col, const std::string& str_val);
    
    // Helper methods
    bool isKeyword() const;
    bool isOperator() const;
    bool isLiteral() const;
    bool isType() const;
    std::string toString() const;  // For debugging
};

// Helper function to convert TokenType to string
std::string tokenTypeToString(TokenType type);

} // namespace frontend
} // namespace aria

// Stream operator for TokenType (for test output)
inline std::ostream& operator<<(std::ostream& os, aria::frontend::TokenType type) {
    return os << aria::frontend::tokenTypeToString(type);
}

#endif // ARIA_TOKEN_H
