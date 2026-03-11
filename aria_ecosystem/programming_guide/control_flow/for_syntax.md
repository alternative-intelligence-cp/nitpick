# For Loop Syntax Reference

**Category**: Control Flow → Syntax  
**Topic**: Complete `for` loop syntax — both C-style and range-based forms

---

## C-Style For Loop

```aria
for(type:var = init_expr; condition; update_expr) {
    // body
}
```

| Clause | Required | Description |
|---|---|---|
| `type:var = init_expr` | No | Variable declaration with Aria type notation |
| `condition` | No | Boolean expression; loop runs while true |
| `update_expr` | No | Statement evaluated after each body execution |

All three clauses are optional. Semicolons are required even when clauses are omitted.

### Examples

```aria
// Standard counter
for(int32:i = 0i32; i < 10i32; i += 1i32) { }

// No initializer (use existing variable)
int32:n = 5i32;
for(; n > 0i32; n = n - 1i32) { }

// No update (manual update inside body)
for(int32:i = 0i32; i < 10i32;) {
    i += 2i32;
}
```

> **Note**: `i++` and `++i` are **not** supported. Use `i += 1i32` or `i = i + 1i32`.

---

## Range-Based For Loop

### Without Type Annotation

```aria
for (var in range_expr) {
    // var is int64 by default
}
```

### With Type Annotation

```aria
for (type:var in range_expr) {
    // var is declared with explicit type
}
```

### Range Expressions

| Syntax | Meaning | Example | Iterates |
|---|---|---|---|
| `a..b` | Inclusive | `1..5` | 1, 2, 3, 4, 5 |
| `a...b` | Exclusive (upper) | `1...5` | 1, 2, 3, 4 |

Both `a` and `b` can be literals or `int64` variables.

### Examples

```aria
// Inclusive, untyped (i is int64)
for (i in 0..9) { }

// Exclusive, untyped
for (i in 0...10) { }

// Variables as bounds
int64:lo = 1;
int64:hi = 100;
for (i in lo..hi) { }

// Explicit int64 type
for (int64:i in 1..5) { }

// Explicit int32 type (values narrowed)
for (int32:j in 0...8) { }
```

---

## Loop Control

```aria
for(int32:i = 0i32; i < 10i32; i += 1i32) {
    if (some_condition) { break; }    // exit loop immediately
    if (other_condition) { continue; } // skip to next iteration
}
```

`break` and `continue` work identically in both C-style and range-based forms.

---

## EBNF Summary

```
for_stmt      ::= "for" "(" c_clauses ")" block
               | "for" "(" range_clause ")" block

c_clauses     ::= [init_clause] ";" [expr] ";" [expr]
init_clause   ::= type_name ":" ident "=" expr

range_clause  ::= [type_name ":"] ident "in" range_expr
range_expr    ::= expr (".." | "...") expr
```

---

## Related Topics

- [For](for.md) — Full for loop guide with examples
- [While](while.md) — Condition-based loops
- [Loop](loop.md) — Counted loop with auto `$` index
- [Till](till.md) — 0-based counted loop with auto `$` index
- [Break](break.md) — Exit loops early
- [Continue](continue.md) — Skip to next iteration
