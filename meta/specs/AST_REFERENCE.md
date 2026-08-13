# Nitpick Abstract Syntax Tree (AST) Reference

Because `nitpick-next` uses a "Full-Frontend, Incremental-Backend" bootstrap strategy, the AST must be perfectly defined to encapsulate the entire language grammar from Day 1. The code generators (backends) will traverse this AST and either lower it to LLVM IR or ignore unknown nodes.

This document outlines the core structural nodes of the Nitpick AST.

---

## 1. Top-Level Declarations (`Decl`)

Every Nitpick file is parsed into a list of Top-Level Declarations.

*   **`ModuleDecl`**: Represents a file or module scope. Contains a list of `use` imports and other `Decl` nodes.
*   **`ImportDecl`**: Represents a `use` statement (e.g., `use std.collections;`).
*   **`FunctionDecl`**: Represents a function definition.
    *   `name`: Identifier
    *   `visibility`: `pub` or internal
    *   `params`: List of `VarDecl` (parameters)
    *   `return_type`: `TypeNode` (Defaults to `Result<T>` in semantics)
    *   `contracts`: Optional list of `ContractNode` (`requires`, `ensures`)
    *   `body`: `BlockStmt`
*   **`StructDecl` / `EnumDecl`**: Represents custom type definitions.
*   **`RuleDecl`**: Represents a structural constraint definition (`Rules<T>:r = { ... }`).
*   **`MacroDecl`**: Represents an AST-aware macro definition.

---

## 2. Statements (`Stmt`)

Statements do not return values (they evaluate to `void`).

*   **`BlockStmt`**: A list of statements enclosed in `{ }`. Scopes variables.
*   **`VarDeclStmt`**: Variable declaration (e.g., `stack int32:x = 5i32;`).
    *   `memory_modifier`: Optional (`stack`, `gc`, `wild`, `wildx`)
    *   `drop_modifier`: Optional (`nodrop`)
    *   `limit`: Optional `LimitNode`
    *   `type`: `TypeNode`
    *   `identifier`: String
    *   `initializer`: Optional `Expr`
*   **`AssignmentStmt`**: Standard assignment (`=`, `+=`, etc.).
*   **`ControlFlowStmt`**: 
    *   **`IfStmt`**: Contains condition (`Expr`), `then_block`, optional `else_block`.
    *   **`WHEN_STMT`**:
    *   `.a` = The test expression.
    *   `.b` = The main `when` block (body).
    *   `.c` = A `GROUP_NODE` (list) containing up to two optional blocks: the `then` block, followed by the `end` block.
*   **`LOOP_STMT`**:
    *   `.a` = A `GROUP_NODE` containing the 3 operands: `start`, `limit`, and `step`.
    *   `.b` = The block (body) of the loop.
    *   `.c` = The optional `end` block (or `NIL`).
*   **`TILL_STMT`**:
    *   `.a` = A `GROUP_NODE` containing the 2 operands: `limit` and `step`.
    *   `.b` = The block (body) of the loop.
    *   `.c` = The optional `end` block (or `NIL`).
*   **`PICK_STMT`**:
    *   `.a` = The selector expression to pick against.
    *   `.b` = A `GROUP_NODE` list of `PICK_CASE` nodes.
    *   `.c` = Unused.
*   **`PICK_CASE`**:
    *   `.a` = A `GROUP_NODE` list of case match labels (e.g. `ERR:`, `(1):`).
    *   `.b` = The block (body) to execute.
    *   `.c` = Unused.
*   **`ResultExitStmt`**: 
    *   **`PassStmt`**: `pass expr;` (Returns success Result)
    *   **`FailStmt`**: `fail expr;` (Returns error Result)
*   **`TrapStmt`**: `!!! expr;` (Triggers `failsafe`)
*   **`DeferStmt`**: Defers execution of a block until scope exit.

---

## 3. Expressions (`Expr`)

Expressions evaluate to a value and have a computed `Type`.

*   **`LiteralExpr`**: 
    *   `IntegerLiteral`: (e.g., `42i32`)
    *   `FloatLiteral`: (e.g., `3.14flt32`)
    *   `CharLiteral`: (e.g., `'A'`)
    *   `StringLiteral`: (e.g., `"Hello"`)
    *   `TfpLiteral`: (e.g., `1.5tfp64`)
*   **`BinaryExpr`**: `+`, `-`, `<`, `==`, `&&`, etc.
*   **`UnaryExpr`**: `!`, `~`, `-` (negation).
*   **`IdentifierExpr`**: Variable lookup.
*   **`CallExpr`**: Function invocation (`foo(a, b)`).
*   **`MemberAccessExpr`**: `obj.field`. (Unified access, automatically dereferences if `obj` is a pointer).
*   **Pointer Expressions**:
    *   **`AddressOfExpr`**: `@val`
    *   **`DerefExpr`**: `<-ptr` (Full deep dereference)
    *   **`PinExpr`**: `#obj`
*   **Error Handling Expressions**:
    *   **`SafeUnwrapExpr`**: `expr ? default_expr`
    *   **`EmphaticUnwrapExpr`**: `expr ?! err_code`
    *   **`RawUnwrapExpr`**: `raw(expr)`
*   **Casting Expressions**:
    *   **`SafeCastExpr`**: `expr => TypeNode`
    *   **`UncheckedCastExpr`**: `expr =>! TypeNode`

---

## 4. Formal Verification Nodes (`VerifyNode`)

These nodes are attached to declarations and loops. They are passed directly to the Z3 SMT solver during Phase 3 semantic analysis.

*   **`ContractNode`**: Contains mathematical expressions bound to a `FunctionDecl`.
    *   `type`: `requires` or `ensures`.
    *   `condition`: `Expr` (the mathematical truth that must hold).
*   **`LimitNode`**: Bound to a `VarDeclStmt` or `TypeNode` (e.g., `limit<r_pos>`). Maps to a predefined `RuleDecl`.
*   **`InvariantNode`**: Bound to a loop statement. Holds the loop invariant expression.
*   **`ProveStmt` / `AssertStaticStmt`**: Explicit proof obligation statements injected by the developer into a `BlockStmt`.

---

## 5. Macro System (`MacroNode`)

Because `nitpick-next` dropped the `pre()` text processor, all macros must operate natively on the AST.

*   **`MacroInvocationExpr`**: E.g., `MyMacro!(a, b)`. The parser reads this as a macro invocation but defers expansion until the AST macro-expansion pass, converting it into standard `Expr` or `Stmt` nodes before semantic analysis.
