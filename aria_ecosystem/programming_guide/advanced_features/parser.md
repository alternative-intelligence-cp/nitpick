# Parser

**Category**: Advanced Features → Compiler  
**Purpose**: Build abstract syntax tree from tokens

---

## Overview

The **parser** performs **syntax analysis**, converting tokens into an **Abstract Syntax Tree (AST)**.

---

## Parsing Process

```
Tokens:       [FN] [IDENTIFIER:add] [(] [IDENTIFIER:x] [:] [TYPE:i32] ... [)]
              ↓
Parser:       Build syntax tree
              ↓
AST:          FunctionDecl {
                name: "add",
                parameters: [
                  Param { name: "x", type: "i32" },
                  Param { name: "y", type: "i32" }
                ],
                return_type: "i32",
                body: Block { ... }
              }
```

---

## Parser Structure

```aria
struct Parser {
    tokens: []Token,
    current: i32,
}

impl Parser {
    pub fn new(tokens: []Token) -> Parser {
        return Parser {
            tokens: tokens,
            current: 0,
        };
    }
    
    pub fn parse() -> Result<AST> {
        return self.parse_program();
    }
    
    fn peek() -> Token {
        return self.tokens[self.current];
    }
    
    fn advance() -> Token {
        token: Token = self.peek();
        if !self.is_at_end() {
            self.current += 1;
        }
        return token;
    }
    
    fn is_at_end() -> bool {
        return self.peek().type == TokenType.Eof;
    }
    
    fn check(type: TokenType) -> bool {
        return self.peek().type == type;
    }
    
    fn expect(type: TokenType, message: string) -> Result<Token> {
        if self.check(type) {
            return Ok(self.advance());
        }
        return Err(message);
    }
}
```

---

## Recursive Descent Parsing

```aria
impl Parser {
    // program → declaration* EOF
    fn parse_program() -> Result<Program> {
        declarations: []Declaration = [];
        
        while !self.is_at_end() {
            decl: Declaration = self.parse_declaration()?;
            declarations.push(decl);
        }
        
        return Ok(Program { declarations });
    }
    
    // declaration → function_decl | var_decl | struct_decl
    fn parse_declaration() -> Result<Declaration> {
        if self.check(TokenType.Fn) {
            return self.parse_function();
        } else if self.check(TokenType.Struct) {
            return self.parse_struct();
        } else if self.check(TokenType.Let) {
            return self.parse_variable();
        }
        
        return Err("Expected declaration");
    }
}
```

---

## Function Parsing

```aria
impl Parser {
    // fn name(params) -> return_type { body }
    fn parse_function() -> Result<FunctionDecl> {
        self.expect(TokenType.Fn, "Expected 'fn'")?;
        
        name: Token = self.expect(TokenType.Identifier, "Expected function name")?;
        
        self.expect(TokenType.LParen, "Expected '('")?;
        params: []Parameter = self.parse_parameters()?;
        self.expect(TokenType.RParen, "Expected ')'")?;
        
        return_type: ?Type = None;
        if self.check(TokenType.Arrow) {
            self.advance();
            return_type = Some(self.parse_type()?);
        }
        
        body: Block = self.parse_block()?;
        
        return Ok(FunctionDecl {
            name: name.lexeme,
            parameters: params,
            return_type: return_type,
            body: body,
        });
    }
    
    fn parse_parameters() -> Result<[]Parameter> {
        params: []Parameter = [];
        
        if !self.check(TokenType.RParen) {
            loop {
                name: Token = self.expect(TokenType.Identifier, "Expected parameter name")?;
                self.expect(TokenType.Colon, "Expected ':'")?;
                type: Type = self.parse_type()?;
                
                params.push(Parameter {
                    name: name.lexeme,
                    type: type,
                });
                
                if !self.check(TokenType.Comma) {
                    break;
                }
                self.advance();
            }
        }
        
        return Ok(params);
    }
}
```

---

## Expression Parsing (Precedence Climbing)

```aria
impl Parser {
    // Operator precedence table
    fn get_precedence(op: TokenType) -> i32 {
        match op {
            TokenType.Or => return 1,
            TokenType.And => return 2,
            TokenType.EqEq | TokenType.Ne => return 3,
            TokenType.Lt | TokenType.Le | TokenType.Gt | TokenType.Ge => return 4,
            TokenType.Plus | TokenType.Minus => return 5,
            TokenType.Star | TokenType.Slash => return 6,
            _ => return 0,
        }
    }
    
    fn parse_expression() -> Result<Expression> {
        return self.parse_binary_expression(0);
    }
    
    fn parse_binary_expression(min_precedence: i32) -> Result<Expression> {
        left: Expression = self.parse_primary()?;
        
        while self.get_precedence(self.peek().type) >= min_precedence {
            op: Token = self.advance();
            precedence: i32 = self.get_precedence(op.type);
            right: Expression = self.parse_binary_expression(precedence + 1)?;
            
            left = BinaryExpression {
                left: left,
                operator: op,
                right: right,
            };
        }
        
        return Ok(left);
    }
    
    fn parse_primary() -> Result<Expression> {
        if self.check(TokenType.Integer) {
            value: Token = self.advance();
            return Ok(IntegerLiteral { value: value.value });
        } else if self.check(TokenType.Identifier) {
            name: Token = self.advance();
            return Ok(Variable { name: name.lexeme });
        } else if self.check(TokenType.LParen) {
            self.advance();
            expr: Expression = self.parse_expression()?;
            self.expect(TokenType.RParen, "Expected ')'")?;
            return Ok(expr);
        }
        
        return Err("Expected expression");
    }
}
```

---

## Statement Parsing

```aria
impl Parser {
    fn parse_statement() -> Result<Statement> {
        if self.check(TokenType.Return) {
            return self.parse_return();
        } else if self.check(TokenType.If) {
            return self.parse_if();
        } else if self.check(TokenType.While) {
            return self.parse_while();
        } else if self.check(TokenType.LBrace) {
            return self.parse_block();
        } else {
            return self.parse_expression_statement();
        }
    }
    
    fn parse_return() -> Result<ReturnStatement> {
        self.expect(TokenType.Return, "Expected 'return'")?;
        
        value: ?Expression = None;
        if !self.check(TokenType.Semicolon) {
            value = Some(self.parse_expression()?);
        }
        
        self.expect(TokenType.Semicolon, "Expected ';'")?;
        
        return Ok(ReturnStatement { value });
    }
    
    fn parse_if() -> Result<IfStatement> {
        self.expect(TokenType.If, "Expected 'if'")?;
        
        condition: Expression = self.parse_expression()?;
        then_block: Block = self.parse_block()?;
        
        else_block: ?Block = None;
        if self.check(TokenType.Else) {
            self.advance();
            else_block = Some(self.parse_block()?);
        }
        
        return Ok(IfStatement {
            condition: condition,
            then_block: then_block,
            else_block: else_block,
        });
    }
}
```

---

## Error Recovery

```aria
impl Parser {
    fn synchronize() {
        // Skip tokens until we find a statement boundary
        self.advance();
        
        while !self.is_at_end() {
            if self.peek(-1).type == TokenType.Semicolon {
                return;
            }
            
            match self.peek().type {
                TokenType.Fn | TokenType.Let | TokenType.If | 
                TokenType.While | TokenType.Return => return,
                _ => self.advance(),
            }
        }
    }
    
    fn parse_declaration_with_recovery() -> ?Declaration {
        match self.parse_declaration() {
            Ok(decl) => return Some(decl),
            Err(e) => {
                self.report_error(e);
                self.synchronize();
                return None;
            }
        }
    }
}
```

---

## Best Practices

### ✅ DO: Provide Clear Error Messages

```aria
fn expect(type: TokenType, message: string) -> Result<Token> {
    if !self.check(type) {
        token: Token = self.peek();
        return Err(
            "Parse error at ${token.line}:${token.column}\n" ++
            "  $message\n" ++
            "  Found: ${token.lexeme}"
        );
    }
    return Ok(self.advance());
}
```

### ✅ DO: Implement Error Recovery

```aria
fn parse_program() -> Result<Program> {
    declarations: []Declaration = [];
    errors: []string = [];
    
    while !self.is_at_end() {
        match self.parse_declaration() {
            Ok(decl) => declarations.push(decl),
            Err(e) => {
                errors.push(e);
                self.synchronize();
            }
        }
    }
    
    if errors.len() > 0 {
        return Err(errors.join("\n"));
    }
    
    return Ok(Program { declarations });
}
```

### ✅ DO: Support Operator Precedence

```aria
// Use precedence climbing or Pratt parsing
// 1 + 2 * 3 should parse as 1 + (2 * 3), not (1 + 2) * 3
```

---

## Related

- [lexer](lexer.md) - Lexical analysis
- [tokens](tokens.md) - Token types
- [ast](ast.md) - Abstract syntax tree

---

**Remember**: The parser converts **flat tokens** into a **hierarchical AST** representing program structure!
