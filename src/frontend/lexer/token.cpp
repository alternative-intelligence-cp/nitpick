#include "frontend/token.h"
#include <sstream>

namespace aria {
namespace frontend {

// ============================================================================
// Token Constructors
// ============================================================================

Token::Token() 
    : type(TokenType::TOKEN_ERROR), lexeme(""), line(0), column(0) {
    value.int_value = 0;
}

Token::Token(TokenType t, const std::string& lex, int ln, int col)
    : type(t), lexeme(lex), line(ln), column(col) {
    value.int_value = 0;
}

Token::Token(TokenType t, const std::string& lex, int ln, int col, int64_t val)
    : type(t), lexeme(lex), line(ln), column(col) {
    value.int_value = val;
}

Token::Token(TokenType t, const std::string& lex, int ln, int col, double val)
    : type(t), lexeme(lex), line(ln), column(col) {
    value.float_value = val;
}

Token::Token(TokenType t, const std::string& lex, int ln, int col, bool val)
    : type(t), lexeme(lex), line(ln), column(col) {
    value.bool_value = val;
}

Token::Token(TokenType t, const std::string& lex, int ln, int col, const std::string& str_val)
    : type(t), lexeme(lex), line(ln), column(col), value{0}, string_value(str_val) {
}

// Constructors for high-precision numeric literals (Phase 3.2.5)
Token::Token(TokenType t, const std::string& lex, int ln, int col, int64_t val, const std::string& raw_text)
    : type(t), lexeme(lex), line(ln), column(col), raw_literal_text(raw_text) {
    value.int_value = val;
}

Token::Token(TokenType t, const std::string& lex, int ln, int col, double val, const std::string& raw_text)
    : type(t), lexeme(lex), line(ln), column(col), raw_literal_text(raw_text) {
    value.float_value = val;
}

// ============================================================================
// Token Helper Methods
// ============================================================================

bool Token::isKeyword() const {
    return type >= TokenType::TOKEN_KW_WILD && type <= TokenType::TOKEN_KW_CAST_UNCHECKED;
}

bool Token::isOperator() const {
    return (type >= TokenType::TOKEN_PLUS && type <= TokenType::TOKEN_DOT_DOT_DOT) ||
           (type >= TokenType::TOKEN_AMPERSAND && type <= TokenType::TOKEN_SHIFT_RIGHT);
}

bool Token::isLiteral() const {
    // Legacy untyped literals (deprecated)
    if (type == TokenType::TOKEN_INTEGER || type == TokenType::TOKEN_FLOAT) {
        return true;
    }
    
    // Typed integer literals
    if (type >= TokenType::TOKEN_INTEGER_U8 && type <= TokenType::TOKEN_INTEGER_TBB64) {
        return true;
    }
    
    // Typed float literals
    if (type >= TokenType::TOKEN_FLOAT_F32 && type <= TokenType::TOKEN_FLOAT_FIX256) {
        return true;
    }
    
    // Other literals
    return type == TokenType::TOKEN_STRING ||
           type == TokenType::TOKEN_CHAR ||
           type == TokenType::TOKEN_TERNARY ||
           type == TokenType::TOKEN_NONARY ||
           type == TokenType::TOKEN_KW_TRUE ||
           type == TokenType::TOKEN_KW_FALSE ||
           type == TokenType::TOKEN_KW_NULL ||
           type == TokenType::TOKEN_KW_ERR ||
           type == TokenType::TOKEN_KW_UNKNOWN;
}

bool Token::isType() const {
    return (type >= TokenType::TOKEN_KW_INT1 && type <= TokenType::TOKEN_KW_INT4096) ||
           (type >= TokenType::TOKEN_KW_UINT1 && type <= TokenType::TOKEN_KW_UINT4096) ||
           (type >= TokenType::TOKEN_KW_TBB8 && type <= TokenType::TOKEN_KW_TBB64) ||
           (type >= TokenType::TOKEN_KW_FRAC8 && type <= TokenType::TOKEN_KW_FRAC64) ||
           (type >= TokenType::TOKEN_KW_TFP32 && type <= TokenType::TOKEN_KW_TFP64) ||
           (type >= TokenType::TOKEN_KW_FLT32 && type <= TokenType::TOKEN_KW_FLT512) ||
           (type >= TokenType::TOKEN_KW_BOOL && type <= TokenType::TOKEN_KW_ARRAY) ||
           (type >= TokenType::TOKEN_KW_TRIT && type <= TokenType::TOKEN_KW_NYTE) ||
           (type >= TokenType::TOKEN_KW_VEC2 && type <= TokenType::TOKEN_KW_TTENSOR) ||
           (type >= TokenType::TOKEN_KW_BINARY && type <= TokenType::TOKEN_KW_LOG);
}

std::string Token::toString() const {
    std::ostringstream oss;
    oss << "Token(" << tokenTypeToString(type) 
        << ", \"" << lexeme << "\""
        << ", line=" << line 
        << ", col=" << column;
    
    // Add value if present
    if (type == TokenType::TOKEN_INTEGER) {
        if (!raw_literal_text.empty()) {
            oss << ", raw=\"" << raw_literal_text << "\"";
        } else {
            oss << ", value=" << value.int_value;
        }
    } else if (type == TokenType::TOKEN_FLOAT) {
        if (!raw_literal_text.empty()) {
            oss << ", raw=\"" << raw_literal_text << "\"";
        } else {
            oss << ", value=" << value.float_value;
        }
    } else if (type == TokenType::TOKEN_KW_TRUE || type == TokenType::TOKEN_KW_FALSE) {
        oss << ", value=" << (value.bool_value ? "true" : "false");
    } else if (type == TokenType::TOKEN_STRING || type == TokenType::TOKEN_CHAR) {
        oss << ", value=\"" << string_value << "\"";
    }
    
    oss << ")";
    return oss.str();
}

// ============================================================================
// TokenType to String Conversion
// ============================================================================

std::string tokenTypeToString(TokenType type) {
    switch (type) {
        // Memory qualifiers
        case TokenType::TOKEN_KW_WILD: return "WILD";
        case TokenType::TOKEN_KW_WILDX: return "WILDX";
        case TokenType::TOKEN_KW_STACK: return "STACK";
        case TokenType::TOKEN_KW_GC: return "GC";
        case TokenType::TOKEN_KW_DEFER: return "DEFER";
        
        // Memory ordering
        case TokenType::TOKEN_KW_RELAXED: return "RELAXED";
        case TokenType::TOKEN_KW_ACQUIRE: return "ACQUIRE";
        case TokenType::TOKEN_KW_RELEASE: return "RELEASE";
        case TokenType::TOKEN_KW_ACQ_REL: return "ACQ_REL";
        case TokenType::TOKEN_KW_SEQ_CST: return "SEQ_CST";
        
        // Control flow
        case TokenType::TOKEN_KW_IF: return "IF";
        case TokenType::TOKEN_KW_ELSE: return "ELSE";
        case TokenType::TOKEN_KW_WHILE: return "WHILE";
        case TokenType::TOKEN_KW_FOR: return "FOR";
        case TokenType::TOKEN_KW_LOOP: return "LOOP";
        case TokenType::TOKEN_KW_TILL: return "TILL";
        case TokenType::TOKEN_KW_WHEN: return "WHEN";
        case TokenType::TOKEN_KW_THEN: return "THEN";
        case TokenType::TOKEN_KW_END: return "END";
        case TokenType::TOKEN_KW_PICK: return "PICK";
        case TokenType::TOKEN_KW_FALL: return "FALL";
        case TokenType::TOKEN_KW_BREAK: return "BREAK";
        case TokenType::TOKEN_KW_CONTINUE: return "CONTINUE";
        case TokenType::TOKEN_KW_RETURN: return "RETURN";
        case TokenType::TOKEN_KW_PASS: return "PASS";
        case TokenType::TOKEN_KW_FAIL: return "FAIL";
        
        // Async
        case TokenType::TOKEN_KW_ASYNC: return "ASYNC";
        case TokenType::TOKEN_KW_AWAIT: return "AWAIT";
        case TokenType::TOKEN_KW_CATCH: return "CATCH";
        case TokenType::TOKEN_KW_IN: return "IN";
        
        // Declarations
        case TokenType::TOKEN_KW_FUNC: return "FUNC";
        case TokenType::TOKEN_KW_STRUCT: return "STRUCT";
        case TokenType::TOKEN_KW_ENUM: return "ENUM";
        case TokenType::TOKEN_KW_TYPE: return "TYPE";
        case TokenType::TOKEN_KW_OPAQUE: return "OPAQUE";
        case TokenType::TOKEN_KW_TRAIT: return "TRAIT";
        case TokenType::TOKEN_KW_IMPL: return "IMPL";
        case TokenType::TOKEN_KW_USE: return "USE";
        case TokenType::TOKEN_KW_MOD: return "MOD";
        case TokenType::TOKEN_KW_PUB: return "PUB";
        case TokenType::TOKEN_KW_EXTERN: return "EXTERN";
        case TokenType::TOKEN_KW_CONST: return "CONST";
        case TokenType::TOKEN_KW_FIXED: return "FIXED";
        case TokenType::TOKEN_KW_CFG: return "CFG";
        
        // Integer types
        case TokenType::TOKEN_KW_INT1: return "INT1";
        case TokenType::TOKEN_KW_INT2: return "INT2";
        case TokenType::TOKEN_KW_INT4: return "INT4";
        case TokenType::TOKEN_KW_INT8: return "INT8";
        case TokenType::TOKEN_KW_INT16: return "INT16";
        case TokenType::TOKEN_KW_INT32: return "INT32";
        case TokenType::TOKEN_KW_INT64: return "INT64";
        case TokenType::TOKEN_KW_INT128: return "INT128";
        case TokenType::TOKEN_KW_INT256: return "INT256";
        case TokenType::TOKEN_KW_INT512: return "INT512";
        case TokenType::TOKEN_KW_INT1024: return "INT1024";
        case TokenType::TOKEN_KW_INT2048: return "INT2048";
        case TokenType::TOKEN_KW_INT4096: return "INT4096";
        
        // Unsigned integer types
        case TokenType::TOKEN_KW_UINT1: return "UINT1";
        case TokenType::TOKEN_KW_UINT2: return "UINT2";
        case TokenType::TOKEN_KW_UINT4: return "UINT4";
        case TokenType::TOKEN_KW_UINT8: return "UINT8";
        case TokenType::TOKEN_KW_UINT16: return "UINT16";
        case TokenType::TOKEN_KW_UINT32: return "UINT32";
        case TokenType::TOKEN_KW_UINT64: return "UINT64";
        case TokenType::TOKEN_KW_UINT128: return "UINT128";
        case TokenType::TOKEN_KW_UINT256: return "UINT256";
        case TokenType::TOKEN_KW_UINT512: return "UINT512";
        case TokenType::TOKEN_KW_UINT1024: return "UINT1024";
        case TokenType::TOKEN_KW_UINT2048: return "UINT2048";
        case TokenType::TOKEN_KW_UINT4096: return "UINT4096";
        
        // TBB types
        case TokenType::TOKEN_KW_TBB8: return "TBB8";
        case TokenType::TOKEN_KW_TBB16: return "TBB16";
        case TokenType::TOKEN_KW_TBB32: return "TBB32";
        case TokenType::TOKEN_KW_TBB64: return "TBB64";
        
        // Fraction types
        case TokenType::TOKEN_KW_FRAC8: return "FRAC8";
        case TokenType::TOKEN_KW_FRAC16: return "FRAC16";
        case TokenType::TOKEN_KW_FRAC32: return "FRAC32";
        case TokenType::TOKEN_KW_FRAC64: return "FRAC64";
        
        // TFP types
        case TokenType::TOKEN_KW_TFP32: return "TFP32";
        case TokenType::TOKEN_KW_TFP64: return "TFP64";
        
        // Float types
        case TokenType::TOKEN_KW_FLT32: return "FLT32";
        case TokenType::TOKEN_KW_FLT64: return "FLT64";
        case TokenType::TOKEN_KW_FLT128: return "FLT128";
        case TokenType::TOKEN_KW_FLT256: return "FLT256";
        case TokenType::TOKEN_KW_FLT512: return "FLT512";
        
        // Special types
        case TokenType::TOKEN_KW_BOOL: return "BOOL";
        case TokenType::TOKEN_KW_STRING: return "STRING";
        case TokenType::TOKEN_KW_DYN: return "DYN";
        case TokenType::TOKEN_KW_OBJ: return "OBJ";
        case TokenType::TOKEN_KW_RESULT: return "RESULT";
        case TokenType::TOKEN_KW_ARRAY: return "ARRAY";
        
        // Balanced ternary/nonary
        case TokenType::TOKEN_KW_TRIT: return "TRIT";
        case TokenType::TOKEN_KW_TRYTE: return "TRYTE";
        case TokenType::TOKEN_KW_NIT: return "NIT";
        case TokenType::TOKEN_KW_NYTE: return "NYTE";
        
        // Mathematical types
        case TokenType::TOKEN_KW_VEC2: return "VEC2";
        case TokenType::TOKEN_KW_VEC3: return "VEC3";
        case TokenType::TOKEN_KW_VEC9: return "VEC9";
        case TokenType::TOKEN_KW_MATRIX: return "MATRIX";
        case TokenType::TOKEN_KW_TMATRIX: return "TMATRIX";
        case TokenType::TOKEN_KW_TENSOR: return "TENSOR";
        case TokenType::TOKEN_KW_TTENSOR: return "TTENSOR";
        
        // I/O and system types
        case TokenType::TOKEN_KW_BINARY: return "BINARY";
        case TokenType::TOKEN_KW_BUFFER: return "BUFFER";
        case TokenType::TOKEN_KW_STREAM: return "STREAM";
        case TokenType::TOKEN_KW_PROCESS: return "PROCESS";
        case TokenType::TOKEN_KW_PIPE: return "PIPE";
        case TokenType::TOKEN_KW_DEBUG: return "DEBUG";
        case TokenType::TOKEN_KW_LOG: return "LOG";
        
        // Special keywords
        case TokenType::TOKEN_KW_IS: return "IS";
        case TokenType::TOKEN_KW_NULL: return "NULL";
        case TokenType::TOKEN_KW_NIL: return "NIL";
        case TokenType::TOKEN_KW_TRUE: return "TRUE";
        case TokenType::TOKEN_KW_FALSE: return "FALSE";
        case TokenType::TOKEN_KW_ERR: return "ERR";
        case TokenType::TOKEN_KW_UNKNOWN: return "unknown";
        
        // Type casting
        case TokenType::TOKEN_KW_CAST: return "CAST";
        case TokenType::TOKEN_KW_CAST_UNCHECKED: return "CAST_UNCHECKED";
        
        // Arithmetic operators
        case TokenType::TOKEN_PLUS: return "PLUS";
        case TokenType::TOKEN_MINUS: return "MINUS";
        case TokenType::TOKEN_STAR: return "STAR";
        case TokenType::TOKEN_SLASH: return "SLASH";
        case TokenType::TOKEN_PERCENT: return "PERCENT";
        case TokenType::TOKEN_PLUS_PLUS: return "PLUS_PLUS";
        case TokenType::TOKEN_MINUS_MINUS: return "MINUS_MINUS";
        
        // Assignment operators
        case TokenType::TOKEN_EQUAL: return "EQUAL";
        case TokenType::TOKEN_PLUS_EQUAL: return "PLUS_EQUAL";
        case TokenType::TOKEN_MINUS_EQUAL: return "MINUS_EQUAL";
        case TokenType::TOKEN_STAR_EQUAL: return "STAR_EQUAL";
        case TokenType::TOKEN_SLASH_EQUAL: return "SLASH_EQUAL";
        case TokenType::TOKEN_PERCENT_EQUAL: return "PERCENT_EQUAL";
        
        // Comparison operators
        case TokenType::TOKEN_EQUAL_EQUAL: return "EQUAL_EQUAL";
        case TokenType::TOKEN_BANG_EQUAL: return "BANG_EQUAL";
        case TokenType::TOKEN_LESS: return "LESS";
        case TokenType::TOKEN_LESS_EQUAL: return "LESS_EQUAL";
        case TokenType::TOKEN_GREATER: return "GREATER";
        case TokenType::TOKEN_GREATER_EQUAL: return "GREATER_EQUAL";
        case TokenType::TOKEN_SPACESHIP: return "SPACESHIP";
        
        // Logical operators
        case TokenType::TOKEN_AND_AND: return "AND_AND";
        case TokenType::TOKEN_OR_OR: return "OR_OR";
        case TokenType::TOKEN_BANG: return "BANG";
        
        // Bitwise operators
        case TokenType::TOKEN_AMPERSAND: return "AMPERSAND";
        case TokenType::TOKEN_PIPE: return "PIPE";
        case TokenType::TOKEN_CARET: return "CARET";
        case TokenType::TOKEN_TILDE: return "TILDE";
        case TokenType::TOKEN_SHIFT_LEFT: return "SHIFT_LEFT";
        case TokenType::TOKEN_SHIFT_RIGHT: return "SHIFT_RIGHT";
        
        // Special operators
        case TokenType::TOKEN_AT: return "AT";
        case TokenType::TOKEN_DOLLAR: return "DOLLAR";
        case TokenType::TOKEN_HASH: return "HASH";
        case TokenType::TOKEN_ARROW: return "ARROW";
        case TokenType::TOKEN_LEFT_ARROW: return "LEFT_ARROW";
        case TokenType::TOKEN_SAFE_NAV: return "SAFE_NAV";
        case TokenType::TOKEN_NULL_COALESCE: return "NULL_COALESCE";
        case TokenType::TOKEN_QUESTION: return "QUESTION";
        case TokenType::TOKEN_PIPE_RIGHT: return "PIPE_RIGHT";
        case TokenType::TOKEN_PIPE_LEFT: return "PIPE_LEFT";
        case TokenType::TOKEN_DOT_DOT: return "DOT_DOT";
        case TokenType::TOKEN_DOT_DOT_DOT: return "DOT_DOT_DOT";
        
        // Template literals
        case TokenType::TOKEN_BACKTICK: return "BACKTICK";
        case TokenType::TOKEN_TEMPLATE_START: return "TEMPLATE_START";
        case TokenType::TOKEN_TEMPLATE_PART: return "TEMPLATE_PART";
        case TokenType::TOKEN_INTERP_START: return "INTERP_START";
        case TokenType::TOKEN_INTERP_END: return "INTERP_END";
        case TokenType::TOKEN_TEMPLATE_END: return "TEMPLATE_END";
        
        // Punctuation
        case TokenType::TOKEN_DOT: return "DOT";
        case TokenType::TOKEN_COMMA: return "COMMA";
        case TokenType::TOKEN_COLON: return "COLON";
        case TokenType::TOKEN_SEMICOLON: return "SEMICOLON";
        case TokenType::TOKEN_LEFT_PAREN: return "LEFT_PAREN";
        case TokenType::TOKEN_RIGHT_PAREN: return "RIGHT_PAREN";
        case TokenType::TOKEN_LEFT_BRACE: return "LEFT_BRACE";
        case TokenType::TOKEN_RIGHT_BRACE: return "RIGHT_BRACE";
        case TokenType::TOKEN_LEFT_BRACKET: return "LEFT_BRACKET";
        case TokenType::TOKEN_RIGHT_BRACKET: return "RIGHT_BRACKET";
        
        // Literals (deprecated - use typed variants below)
        case TokenType::TOKEN_INTEGER: return "INTEGER";
        case TokenType::TOKEN_FLOAT: return "FLOAT";
        
        // Typed integer literals (unsigned)
        case TokenType::TOKEN_INTEGER_U8: return "INTEGER_U8";
        case TokenType::TOKEN_INTEGER_U16: return "INTEGER_U16";
        case TokenType::TOKEN_INTEGER_U32: return "INTEGER_U32";
        case TokenType::TOKEN_INTEGER_U64: return "INTEGER_U64";
        case TokenType::TOKEN_INTEGER_U128: return "INTEGER_U128";
        case TokenType::TOKEN_INTEGER_U256: return "INTEGER_U256";
        case TokenType::TOKEN_INTEGER_U512: return "INTEGER_U512";
        case TokenType::TOKEN_INTEGER_U1024: return "INTEGER_U1024";
        case TokenType::TOKEN_INTEGER_U2048: return "INTEGER_U2048";
        case TokenType::TOKEN_INTEGER_U4096: return "INTEGER_U4096";
        
        // Typed integer literals (signed)
        case TokenType::TOKEN_INTEGER_I8: return "INTEGER_I8";
        case TokenType::TOKEN_INTEGER_I16: return "INTEGER_I16";
        case TokenType::TOKEN_INTEGER_I32: return "INTEGER_I32";
        case TokenType::TOKEN_INTEGER_I64: return "INTEGER_I64";
        case TokenType::TOKEN_INTEGER_I128: return "INTEGER_I128";
        case TokenType::TOKEN_INTEGER_I256: return "INTEGER_I256";
        case TokenType::TOKEN_INTEGER_I512: return "INTEGER_I512";
        case TokenType::TOKEN_INTEGER_I1024: return "INTEGER_I1024";
        case TokenType::TOKEN_INTEGER_I2048: return "INTEGER_I2048";
        case TokenType::TOKEN_INTEGER_I4096: return "INTEGER_I4096";
        
        // Typed integer literals (TBB)
        case TokenType::TOKEN_INTEGER_TBB8: return "INTEGER_TBB8";
        case TokenType::TOKEN_INTEGER_TBB16: return "INTEGER_TBB16";
        case TokenType::TOKEN_INTEGER_TBB32: return "INTEGER_TBB32";
        case TokenType::TOKEN_INTEGER_TBB64: return "INTEGER_TBB64";
        
        // Typed float literals
        case TokenType::TOKEN_FLOAT_F32: return "FLOAT_F32";
        case TokenType::TOKEN_FLOAT_F64: return "FLOAT_F64";
        case TokenType::TOKEN_FLOAT_F128: return "FLOAT_F128";
        case TokenType::TOKEN_FLOAT_F256: return "FLOAT_F256";
        case TokenType::TOKEN_FLOAT_F512: return "FLOAT_F512";
        case TokenType::TOKEN_FLOAT_FIX256: return "FLOAT_FIX256";
        
        // Other literals
        case TokenType::TOKEN_STRING: return "STRING";
        case TokenType::TOKEN_CHAR: return "CHAR";
        case TokenType::TOKEN_TERNARY: return "TERNARY";
        case TokenType::TOKEN_NONARY: return "NONARY";
        
        // Special tokens
        case TokenType::TOKEN_IDENTIFIER: return "IDENTIFIER";
        case TokenType::TOKEN_EOF: return "EOF";
        case TokenType::TOKEN_ERROR: return "ERROR";
        case TokenType::TOKEN_COMMENT: return "COMMENT";
        case TokenType::TOKEN_WHITESPACE: return "WHITESPACE";
        
        default: return "UNKNOWN";
    }
}

} // namespace frontend
} // namespace aria
