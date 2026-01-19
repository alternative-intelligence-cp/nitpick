# Lexer

**Category**: Advanced Features → Compiler  
**Purpose**: Tokenize source code into lexical units

---

## Overview

The **lexer** (lexical analyzer) converts source text into **tokens** - the first compilation phase.

---

## Lexical Analysis Process

```
Source Code:  "fn add(x: i32, y: i32) -> i32 { return x + y; }"

Tokens:
  [KEYWORD: fn]
  [IDENTIFIER: add]
  [LPAREN: (]
  [IDENTIFIER: x]
  [COLON: :]
  [TYPE: i32]
  [COMMA: ,]
  [IDENTIFIER: y]
  [COLON: :]
  [TYPE: i32]
  [RPAREN: )]
  [ARROW: ->]
  [TYPE: i32]
  [LBRACE: {]
  [KEYWORD: return]
  [IDENTIFIER: x]
  [PLUS: +]
  [IDENTIFIER: y]
  [SEMICOLON: ;]
  [RBRACE: }]
```

---

## Token Types

```aria
enum TokenType {
    // Keywords
    Fn, Let, If, Else, While, For, Return,
    Struct, Enum, Impl, Pub, Mod, Use,
    
    // Literals
    Integer(i64),
    Float(f64),
    String(string),
    Char(char),
    Bool(bool),
    
    // Identifiers
    Identifier(string),
    
    // Operators
    Plus, Minus, Star, Slash,
    Eq, EqEq, Ne, Lt, Gt, Le, Ge,
    And, Or, Not,
    
    // Delimiters
    LParen, RParen,
    LBrace, RBrace,
    LBracket, RBracket,
    Comma, Semicolon, Colon,
    Dot, Arrow,
    
    // Special
    Eof,
    Unknown,
}
```

---

## Token Structure

```aria
struct Token {
    type: TokenType,
    lexeme: string,      // Original text
    line: i32,           // Line number
    column: i32,         // Column number
    file: string,        // Source file
}

// Example
token: Token = Token {
    type: TokenType.Identifier("add"),
    lexeme: "add",
    line: 1,
    column: 4,
    file: "main.aria",
};
```

---

## Lexer Implementation

```aria
struct Lexer {
    source: string,
    position: i32,
    line: i32,
    column: i32,
}

impl Lexer {
    pub fn new(source: string) -> Lexer {
        return Lexer {
            source: source,
            position: 0,
            line: 1,
            column: 1,
        };
    }
    
    pub fn next_token() -> Token {
        self.skip_whitespace();
        
        if self.is_at_end() {
            return self.make_token(TokenType.Eof);
        }
        
        c: char = self.peek();
        
        match c {
            '(' => return self.make_token(TokenType.LParen),
            ')' => return self.make_token(TokenType.RParen),
            '{' => return self.make_token(TokenType.LBrace),
            '}' => return self.make_token(TokenType.RBrace),
            '+' => return self.make_token(TokenType.Plus),
            '-' => return self.scan_arrow_or_minus(),
            '=' => return self.scan_eq_or_eqeq(),
            _ => {
                if c.is_digit() {
                    return self.scan_number();
                } else if c.is_alpha() {
                    return self.scan_identifier_or_keyword();
                } else {
                    return self.make_token(TokenType.Unknown);
                }
            }
        }
    }
}
```

---

## Scanning Numbers

```aria
impl Lexer {
    fn scan_number() -> Token {
        start: i32 = self.position;
        
        // Integer part
        while self.peek().is_digit() {
            self.advance();
        }
        
        // Decimal part
        if self.peek() == '.' && self.peek_next().is_digit() {
            self.advance();  // Skip '.'
            while self.peek().is_digit() {
                self.advance();
            }
            
            // Float literal
            value: f64 = self.source[start..self.position].parse();
            return Token {
                type: TokenType.Float(value),
                lexeme: self.source[start..self.position],
                // ... position info
            };
        }
        
        // Integer literal
        value: i64 = self.source[start..self.position].parse();
        return Token {
            type: TokenType.Integer(value),
            lexeme: self.source[start..self.position],
            // ... position info
        };
    }
}
```

---

## Scanning Identifiers and Keywords

```aria
impl Lexer {
    fn scan_identifier_or_keyword() -> Token {
        start: i32 = self.position;
        
        // Scan identifier
        while self.peek().is_alphanumeric() || self.peek() == '_' {
            self.advance();
        }
        
        text: string = self.source[start..self.position];
        
        // Check if keyword
        token_type: TokenType = match text {
            "fn" => TokenType.Fn,
            "let" => TokenType.Let,
            "if" => TokenType.If,
            "else" => TokenType.Else,
            "return" => TokenType.Return,
            "true" => TokenType.Bool(true),
            "false" => TokenType.Bool(false),
            _ => TokenType.Identifier(text),
        };
        
        return Token {
            type: token_type,
            lexeme: text,
            // ... position info
        };
    }
}
```

---

## Scanning Strings

```aria
impl Lexer {
    fn scan_string() -> Token {
        self.advance();  // Skip opening quote
        start: i32 = self.position;
        
        while !self.is_at_end() && self.peek() != '"' {
            if self.peek() == '\\' {
                self.advance();  // Skip escape
                self.advance();  // Skip escaped char
            } else {
                self.advance();
            }
        }
        
        if self.is_at_end() {
            return self.error_token("Unterminated string");
        }
        
        value: string = self.source[start..self.position];
        self.advance();  // Skip closing quote
        
        return Token {
            type: TokenType.String(value),
            lexeme: "\"$value\"",
            // ... position info
        };
    }
}
```

---

## Handling Comments

```aria
impl Lexer {
    fn skip_whitespace() {
        while true {
            c: char = self.peek();
            
            match c {
                ' ' | '\t' | '\r' => self.advance(),
                '\n' => {
                    self.line += 1;
                    self.column = 1;
                    self.advance();
                }
                '/' => {
                    if self.peek_next() == '/' {
                        // Line comment
                        while !self.is_at_end() && self.peek() != '\n' {
                            self.advance();
                        }
                    } else if self.peek_next() == '*' {
                        // Block comment
                        self.skip_block_comment();
                    } else {
                        return;
                    }
                }
                _ => return,
            }
        }
    }
    
    fn skip_block_comment() {
        self.advance();  // Skip /
        self.advance();  // Skip *
        
        depth: i32 = 1;
        
        while !self.is_at_end() && depth > 0 {
            if self.peek() == '/' && self.peek_next() == '*' {
                depth += 1;  // Nested comment
                self.advance();
                self.advance();
            } else if self.peek() == '*' && self.peek_next() == '/' {
                depth -= 1;
                self.advance();
                self.advance();
            } else {
                self.advance();
            }
        }
    }
}
```

---

## Error Reporting

```aria
impl Lexer {
    fn error_token(message: string) -> Token {
        return Token {
            type: TokenType.Unknown,
            lexeme: message,
            line: self.line,
            column: self.column,
            file: self.file,
        };
    }
}

// Usage
fn compile(source: string) {
    lexer: Lexer = Lexer.new(source);
    
    while true {
        token: Token = lexer.next_token();
        
        if token.type == TokenType.Unknown {
            stderr << "Lexer error at ${token.line}:${token.column}: ${token.lexeme}";
            return;
        }
        
        if token.type == TokenType.Eof {
            break;
        }
        
        // Process token
    }
}
```

---

## Best Practices

### ✅ DO: Track Position Information

```aria
struct Token {
    type: TokenType,
    lexeme: string,
    line: i32,
    column: i32,
    file: string,
    span: (i32, i32),  // Start and end positions
}
```

### ✅ DO: Handle Unicode

```aria
fn scan_identifier() -> Token {
    // Support Unicode identifiers
    while self.peek().is_alphabetic() || self.peek() == '_' {
        self.advance();
    }
}
```

### ✅ DO: Provide Good Error Messages

```aria
fn error(message: string) {
    stderr << "Error at ${self.line}:${self.column}";
    stderr << "  ${message}";
    stderr << "  ${self.get_line_text()}";
    stderr << "  ${' ' * (self.column - 1)}^";
}
```

---

## Related

- [tokens](tokens.md) - Token types
- [parser](parser.md) - Syntax analysis
- [ast](ast.md) - Abstract syntax tree

---

**Remember**: The lexer is the **first compilation phase** - it converts text into structured tokens!
