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
    TOKEN_KW_MOVE,      // move - transfer ownership
    TOKEN_KW_BORROW_IMM, // $$i - immutable borrow qualifier
    TOKEN_KW_BORROW_MUT, // $$m - mutable borrow qualifier
    
    // ========================================================================
    // Keywords - Memory Ordering (for atomic<T> operations)
    // ========================================================================
    TOKEN_KW_RELAXED,   // relaxed - no synchronization, only atomicity
    TOKEN_KW_ACQUIRE,   // acquire - load barrier
    TOKEN_KW_RELEASE,   // release - store barrier
    TOKEN_KW_ACQ_REL,   // acq_rel - both acquire and release (for RMW)
    TOKEN_KW_SEQ_CST,   // seq_cst - sequential consistency
    
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
    TOKEN_KW_IN,        // in - for range-based loops (for x in range)
    
    // ========================================================================
    // Keywords - Declarations
    // ========================================================================
    TOKEN_KW_FUNC,      // func - function declaration
    TOKEN_KW_STRUCT,    // struct - structure declaration
    TOKEN_KW_ENUM,      // enum - enumeration declaration
    TOKEN_KW_TYPE,      // Type - type declaration (organized composable unit)
    TOKEN_KW_OPAQUE,    // opaque - opaque type declaration (FFI)
    TOKEN_KW_TRAIT,     // trait - trait declaration
    TOKEN_KW_IMPL,      // impl - trait implementation
    TOKEN_KW_RULES,     // Rules - constraint rules declaration
    TOKEN_KW_LIMIT,     // limit - apply constraint rules to variable
    TOKEN_KW_USE,       // use - import module
    TOKEN_KW_MOD,       // mod - define module
    TOKEN_KW_PUB,       // pub - public visibility
    TOKEN_KW_EXTERN,    // extern - external C functions
    TOKEN_KW_CONST,     // const - compile-time constant
    TOKEN_KW_FIXED,     // fixed - runtime immutability (opt-in)
    TOKEN_KW_CFG,       // cfg - conditional compilation
    TOKEN_KW_AS,        // as - alias in use statement
    TOKEN_KW_COMPTIME,  // comptime - compile-time evaluation
    TOKEN_KW_INLINE,    // inline - inline hint for functions
    TOKEN_KW_NOINLINE,  // noinline - prevent inlining
    
    // ========================================================================
    // Keywords - Design by Contract (P1-4 Formal Verification)
    // ========================================================================
    TOKEN_KW_REQUIRES,  // requires - function precondition
    TOKEN_KW_ENSURES,   // ensures - function postcondition
    TOKEN_KW_INVARIANT, // invariant - loop/struct invariant
    
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
    TOKEN_KW_INT1024,   // int1024 - 1024-bit signed (post-quantum crypto)
    TOKEN_KW_INT2048,   // int2048 - 2048-bit signed (LBIM, quantum-resistant)
    TOKEN_KW_INT4096,   // int4096 - 4096-bit signed (LBIM, maximum security)
    
    // ========================================================================
    // Type Keywords - Integers (Unsigned)
    // ========================================================================
    TOKEN_KW_UINT1,     // uint1 - 1-bit unsigned
    TOKEN_KW_UINT2,     // uint2 - 2-bit unsigned
    TOKEN_KW_UINT4,     // uint4 - 4-bit unsigned
    TOKEN_KW_UINT8,     // uint8 - 8-bit unsigned
    TOKEN_KW_UINT16,    // uint16 - 16-bit unsigned
    TOKEN_KW_UINT32,    // uint32 - 32-bit unsigned
    TOKEN_KW_UINT64,    // uint64 - 64-bit unsigned
    TOKEN_KW_UINT128,   // uint128 - 128-bit unsigned
    TOKEN_KW_UINT256,   // uint256 - 256-bit unsigned
    TOKEN_KW_UINT512,   // uint512 - 512-bit unsigned
    TOKEN_KW_UINT1024,  // uint1024 - 1024-bit unsigned (post-quantum crypto)
    TOKEN_KW_UINT2048,  // uint2048 - 2048-bit unsigned (LBIM, quantum-resistant)
    TOKEN_KW_UINT4096,  // uint4096 - 4096-bit unsigned (LBIM, maximum security)
    
    // ========================================================================
    // Type Keywords - TBB (Twisted Balanced Binary)
    // ========================================================================
    // Symmetric ranges with ERR sentinel at minimum value
    TOKEN_KW_TBB8,      // tbb8 - [-127, +127], ERR = -128
    TOKEN_KW_TBB16,     // tbb16 - [-32767, +32767], ERR = -32768
    TOKEN_KW_TBB32,     // tbb32 - symmetric 32-bit, ERR at min
    TOKEN_KW_TBB64,     // tbb64 - symmetric 64-bit, ERR at min
    
    // ========================================================================
    // Type Keywords - Fraction (Exact Rational)
    // ========================================================================
    TOKEN_KW_FRAC8,     // frac8 - {tbb8 whole, tbb8 num, tbb8 denom}
    TOKEN_KW_FRAC16,    // frac16 - {tbb16 whole, tbb16 num, tbb16 denom}
    TOKEN_KW_FRAC32,    // frac32 - {tbb32 whole, tbb32 num, tbb32 denom}
    TOKEN_KW_FRAC64,    // frac64 - {tbb64 whole, tbb64 num, tbb64 denom}
    
    // ========================================================================
    // Type Keywords - TFP (Twisted Floating Point)
    // ========================================================================
    TOKEN_KW_TFP32,     // tfp32 - deterministic 32-bit float (tbb16 exp + tbb16 mant)
    TOKEN_KW_TFP64,     // tfp64 - deterministic 64-bit float (tbb16 exp + tbb48 mant)
    
    // ========================================================================
    // Type Keywords - Floating Point
    // ========================================================================
    TOKEN_KW_FLT32,     // flt32 - 32-bit float
    TOKEN_KW_FLT64,     // flt64 - 64-bit float (double)
    TOKEN_KW_FLT128,    // flt128 - 128-bit float
    TOKEN_KW_FLT256,    // flt256 - 256-bit float
    TOKEN_KW_FLT512,    // flt512 - 512-bit float
    
    // ========================================================================
    // Type Keywords - Fixed Point (Deterministic)
    // ========================================================================
    TOKEN_KW_FIX256,    // fix256 - Q128.128 deterministic physics (Report 7)
    
    // ========================================================================
    // Type Keywords - Special/Composite
    // ========================================================================
    TOKEN_KW_BOOL,      // bool - boolean type
    TOKEN_KW_STRING,    // string - string type
    TOKEN_KW_DYN,       // dyn - dynamic type
    TOKEN_KW_OBJ,       // obj - object type
    TOKEN_KW_ANY,       // any - type-erased pointer (safe void*)
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
    TOKEN_KW_VEC9,      // vec9 - 9D vector (toroidal AGI memory)
    TOKEN_KW_MATRIX,    // matrix - matrix type
    TOKEN_KW_TMATRIX,   // tmatrix - twisted matrix (sentinel-aware)
    TOKEN_KW_TENSOR,    // tensor - tensor type
    TOKEN_KW_TTENSOR,   // ttensor - 9D toroidal tensor (AGI memory)
    
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
    TOKEN_KW_NULL,      // NULL - null pointer (C FFI only)
    TOKEN_KW_NIL,       // NIL - absence of value (Aria native)
    TOKEN_KW_TRUE,      // true - boolean literal
    TOKEN_KW_FALSE,     // false - boolean literal
    TOKEN_KW_ERR,       // ERR - TBB error sentinel
    TOKEN_KW_UNKNOWN,   // unknown - indeterminate value literal
    
    // ========================================================================
    // Type Casting (Zero Implicit Conversion)
    // ========================================================================
    TOKEN_KW_CAST,           // @cast - checked type conversion
    TOKEN_KW_CAST_UNCHECKED, // @cast_unchecked - unchecked type conversion
    
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
    TOKEN_QUESTION_BANG,// ?! - emphatic unwrap operator
    TOKEN_BANG_BANG,    // !! - sys!! full-tier syscall modifier
    TOKEN_BANG_BANG_BANG,// !!! - failsafe call operator
    TOKEN_UNDERSCORE_QUESTION, // _? - drop shorthand (prefix, desugars to drop())
    TOKEN_UNDERSCORE_BANG,     // _! - raw shorthand (prefix, desugars to raw())
    
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
    TOKEN_AT,           // @ - address-of operator
    TOKEN_DOLLAR,       // $ - iteration variable, safe reference
    TOKEN_HASH,         // # - memory pinning operator
    TOKEN_ARROW,        // -> - pointer type & member access (int64->:ptr, ptr->member)
    TOKEN_LEFT_ARROW,   // <- - pointer dereference (value <- ptr)
    TOKEN_FAT_ARROW,    // => - cast/coerce operator  (expr => TargetType)
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
    TOKEN_INTEGER,      // Integer literal (decimal, hex, binary, octal) - DEPRECATED
    TOKEN_FLOAT,        // Float literal - DEPRECATED
    
    // ========================================================================
    // Typed Integer Literals (Zero Implicit Conversion Policy)
    // ========================================================================
    // Reference: docs/programming_guide/types/zero_implicit_conversion.md
    // All numeric literals MUST have explicit type suffixes (e.g., 1u32, 0i64)
    
    // Unsigned integer literals
    TOKEN_INTEGER_U8,       // 255u8
    TOKEN_INTEGER_U16,      // 65535u16
    TOKEN_INTEGER_U32,      // 4294967295u32
    TOKEN_INTEGER_U64,      // 18446744073709551615u64
    TOKEN_INTEGER_U128,     // ...u128
    TOKEN_INTEGER_U256,     // ...u256
    TOKEN_INTEGER_U512,     // ...u512
    TOKEN_INTEGER_U1024,    // ...u1024
    TOKEN_INTEGER_U2048,    // ...u2048 (LBIM)
    TOKEN_INTEGER_U4096,    // ...u4096 (LBIM)
    
    // Signed integer literals
    TOKEN_INTEGER_I8,       // -128i8, 127i8
    TOKEN_INTEGER_I16,      // -32768i16, 32767i16
    TOKEN_INTEGER_I32,      // -2147483648i32, 2147483647i32
    TOKEN_INTEGER_I64,      // -9223372036854775808i64, 9223372036854775807i64
    TOKEN_INTEGER_I128,     // ...i128
    TOKEN_INTEGER_I256,     // ...i256
    TOKEN_INTEGER_I512,     // ...i512
    TOKEN_INTEGER_I1024,    // ...i1024
    TOKEN_INTEGER_I2048,    // ...i2048 (LBIM)
    TOKEN_INTEGER_I4096,    // ...i4096 (LBIM)
    
    // TBB (Twisted Balanced Binary) integer literals
    TOKEN_INTEGER_TBB8,     // 127tbb8, -127tbb8 (ERR = -128)
    TOKEN_INTEGER_TBB16,    // 32767tbb16, -32767tbb16 (ERR = -32768)
    TOKEN_INTEGER_TBB32,    // ...tbb32
    TOKEN_INTEGER_TBB64,    // ...tbb64
    
    // ========================================================================
    // Typed Float Literals (Zero Implicit Conversion Policy)
    // ========================================================================
    
    // Standard floating-point literals
    TOKEN_FLOAT_F32,        // 3.14f32, -2.5f32
    TOKEN_FLOAT_F64,        // 3.14159265358979f64
    TOKEN_FLOAT_F128,       // ...f128
    TOKEN_FLOAT_F256,       // ...f256
    TOKEN_FLOAT_F512,       // ...f512
    
    // Fixed-point literal (deterministic physics)
    TOKEN_FLOAT_FIX256,     // 1.5fix256 (Q128.128 format from Report 7)
    
    // TFP (Twisted Floating Point) literals - FUTURE
    // TOKEN_FLOAT_TFP32,   // ...tfp32
    // TOKEN_FLOAT_TFP64,   // ...tfp64
    
    // ========================================================================
    // Other Literals
    // ========================================================================
    TOKEN_STRING,       // String literal "..."
    TOKEN_CHAR,         // Character literal '...'
    TOKEN_TERNARY,      // Ternary literal 0t[01T]+ (balanced ternary)
    TOKEN_NONARY,       // Nonary literal 0n[01234ABCD]+ (balanced nonary)
    
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
    
    std::string string_value;      // For strings (separate due to complex type)
    std::string raw_literal_text;  // For high-precision numeric literals (Phase 3.2.5)
    
    // Constructors
    Token();
    explicit Token(TokenType t, const std::string& lex, int ln, int col);
    Token(TokenType t, const std::string& lex, int ln, int col, int64_t val);
    Token(TokenType t, const std::string& lex, int ln, int col, double val);
    Token(TokenType t, const std::string& lex, int ln, int col, bool val);
    Token(TokenType t, const std::string& lex, int ln, int col, const std::string& str_val);
    
    // Constructor for high-precision numeric literals (Phase 3.2.5)
    Token(TokenType t, const std::string& lex, int ln, int col, int64_t val, const std::string& raw_text);
    Token(TokenType t, const std::string& lex, int ln, int col, double val, const std::string& raw_text);
    
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
