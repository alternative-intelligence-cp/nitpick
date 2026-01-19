# Tokens

**Category**: Advanced Features → Compiler  
**Purpose**: Lexical units representing source code elements

---

## Overview

**Tokens** are the fundamental units produced by the lexer, representing keywords, operators, literals, and identifiers.

---

## Token Categories

### Keywords

```aria
fn, let, mut, const, if, else, while, for, loop, break, continue,
return, struct, enum, impl, trait, pub, mod, use, extern,
async, await, match, as, in, true, false, null
```

---

### Operators

#### Arithmetic
```aria
+   Plus        (addition)
-   Minus       (subtraction)
*   Star        (multiplication)
/   Slash       (division)
%   Percent     (modulo)
**  StarStar    (exponentiation)
```

#### Comparison
```aria
==  EqEq        (equality)
!=  Ne          (not equal)
<   Lt          (less than)
<=  Le          (less than or equal)
>   Gt          (greater than)
>=  Ge          (greater than or equal)
```

#### Logical
```aria
&&  And         (logical AND)
||  Or          (logical OR)
!   Not         (logical NOT)
```

#### Bitwise
```aria
&   BitAnd      (bitwise AND)
|   BitOr       (bitwise OR)
^   BitXor      (bitwise XOR)
~   BitNot      (bitwise NOT)
<<  Shl         (shift left)
>>  Shr         (shift right)
```

#### Assignment
```aria
=   Eq          (assignment)
+=  PlusEq      (add assign)
-=  MinusEq     (subtract assign)
*=  StarEq      (multiply assign)
/=  SlashEq     (divide assign)
%=  PercentEq   (modulo assign)
&=  BitAndEq    (bitwise AND assign)
|=  BitOrEq     (bitwise OR assign)
^=  BitXorEq    (bitwise XOR assign)
<<=  ShlEq      (shift left assign)
>>=  ShrEq      (shift right assign)
```

---

### Delimiters

```aria
(   LParen      (left parenthesis)
)   RParen      (right parenthesis)
{   LBrace      (left brace)
}   RBrace      (right brace)
[   LBracket    (left bracket)
]   RBracket    (right bracket)
,   Comma       (comma)
;   Semicolon   (semicolon)
:   Colon       (colon)
.   Dot         (dot/period)
..  DotDot      (range)
->  Arrow       (function arrow)
=>  FatArrow    (match arm)
::  ColonColon  (path separator)
```

---

### Literals

#### Integer Literals
```aria
42          Decimal
0xFF        Hexadecimal
0o77        Octal
0b1010      Binary
1_000_000   With underscores
```

#### Float Literals
```aria
3.14
2.0
1.5e10
6.022e23
```

#### String Literals
```aria
"hello"                Normal string
"hello\nworld"         With escapes
"path\\to\\file"       Escaped backslash
r"C:\path\to\file"     Raw string (no escapes)
```

#### Character Literals
```aria
'a'
'\n'
'\t'
'\''
'\\'
```

#### Boolean Literals
```aria
true
false
```

---

## Token Structure

```aria
struct Token {
    type: TokenType,
    lexeme: string,
    literal: ?Value,
    line: i32,
    column: i32,
    file: string,
}

enum TokenType {
    // Keywords
    Fn, Let, Mut, Const, If, Else, While, For, Return,
    Struct, Enum, Impl, Trait, Pub, Mod, Use,
    
    // Literals
    Integer, Float, String, Char, True, False, Null,
    
    // Identifiers
    Identifier,
    
    // Operators
    Plus, Minus, Star, Slash, Percent,
    EqEq, Ne, Lt, Le, Gt, Ge,
    And, Or, Not,
    BitAnd, BitOr, BitXor, BitNot, Shl, Shr,
    
    // Delimiters
    LParen, RParen, LBrace, RBrace, LBracket, RBracket,
    Comma, Semicolon, Colon, Dot, DotDot, Arrow, FatArrow,
    
    // Special
    Eof, Unknown,
}
```

---

## Token Examples

```aria
// Source: fn add(x: i32, y: i32) -> i32 { return x + y; }

[
    Token { type: Fn,         lexeme: "fn",     line: 1, column: 1 },
    Token { type: Identifier, lexeme: "add",    line: 1, column: 4 },
    Token { type: LParen,     lexeme: "(",      line: 1, column: 7 },
    Token { type: Identifier, lexeme: "x",      line: 1, column: 8 },
    Token { type: Colon,      lexeme: ":",      line: 1, column: 9 },
    Token { type: Identifier, lexeme: "i32",    line: 1, column: 11 },
    Token { type: Comma,      lexeme: ",",      line: 1, column: 14 },
    Token { type: Identifier, lexeme: "y",      line: 1, column: 16 },
    Token { type: Colon,      lexeme: ":",      line: 1, column: 17 },
    Token { type: Identifier, lexeme: "i32",    line: 1, column: 19 },
    Token { type: RParen,     lexeme: ")",      line: 1, column: 22 },
    Token { type: Arrow,      lexeme: "->",     line: 1, column: 24 },
    Token { type: Identifier, lexeme: "i32",    line: 1, column: 27 },
    Token { type: LBrace,     lexeme: "{",      line: 1, column: 31 },
    Token { type: Return,     lexeme: "return", line: 1, column: 33 },
    Token { type: Identifier, lexeme: "x",      line: 1, column: 40 },
    Token { type: Plus,       lexeme: "+",      line: 1, column: 42 },
    Token { type: Identifier, lexeme: "y",      line: 1, column: 44 },
    Token { type: Semicolon,  lexeme: ";",      line: 1, column: 45 },
    Token { type: RBrace,     lexeme: "}",      line: 1, column: 47 },
    Token { type: Eof,        lexeme: "",       line: 1, column: 48 },
]
```

---

## Special Token Types

### Whitespace (Discarded)
```aria
' '  Space
'\t' Tab
'\n' Newline
'\r' Carriage return
```

### Comments (Discarded)
```aria
// Line comment
/* Block comment */
/* Nested /* comments */ allowed */
```

---

## Token Utilities

```aria
impl Token {
    pub fn is_keyword() -> bool {
        match self.type {
            TokenType.Fn | TokenType.Let | TokenType.If |
            TokenType.Else | TokenType.While | TokenType.Return => true,
            _ => false,
        }
    }
    
    pub fn is_operator() -> bool {
        match self.type {
            TokenType.Plus | TokenType.Minus | TokenType.Star |
            TokenType.Slash | TokenType.EqEq | TokenType.Ne => true,
            _ => false,
        }
    }
    
    pub fn is_literal() -> bool {
        match self.type {
            TokenType.Integer | TokenType.Float |
            TokenType.String | TokenType.Char |
            TokenType.True | TokenType.False => true,
            _ => false,
        }
    }
}
```

---

## Best Practices

### ✅ DO: Include Position Information

```aria
struct Token {
    type: TokenType,
    lexeme: string,
    line: i32,
    column: i32,
    file: string,
    span: (i32, i32),  // Start and end byte positions
}
```

### ✅ DO: Preserve Source Text

```aria
// Keep original lexeme for error messages
token: Token = Token {
    type: TokenType.Integer,
    lexeme: "1_000_000",  // Original with underscores
    literal: Some(1000000),  // Parsed value
};
```

### ✅ DO: Handle Multi-Character Operators

```aria
// Correctly distinguish:
// =   (assignment)
// ==  (equality)
// !=  (not equal)
// ->  (arrow)
// =>  (fat arrow)
```

---

## Related

- [lexer](lexer.md) - Tokenization
- [parser](parser.md) - Syntax analysis
- [ast](ast.md) - Abstract syntax tree

---

**Remember**: Tokens are the **building blocks** of syntax - each represents a meaningful unit in the source code!
