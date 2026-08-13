# Nitpick Operator Reference

This document provides a comprehensive list of all operators available in the Nitpick programming language, categorized by their primary function.

> **⚠️ FORMAL VERIFICATION RULE**: Operator overloading is strictly forbidden in Nitpick. The meaning of an operator is fixed at the language level, ensuring deterministic, highly-auditable code.

---

## 1. Arithmetic & Mathematical

| Operator | Name | Description | Example |
|---|---|---|---|
| `+` | Add | Safe addition. | `a + b` |
| `-` | Subtract | Safe subtraction. | `a - b` |
| `*` | Multiply | Safe multiplication. Note: also used for pointer syntax in `extern` blocks. | `a * b` |
| `/` | Divide | Safe division. Triggers failsafe on divide-by-zero. | `a / b` |
| `%` | Modulo | Remainder operation. | `a % b` |
| `++` | Increment | Post/pre-increment. | `i++` or `++i` |
| `--` | Decrement | Post/pre-decrement. | `i--` or `--i` |
| `**` | Power | Exponentiation (Standard Library expansion). | `2 ** 8` |

---

## 2. Assignment

| Operator | Name | Description | Example |
|---|---|---|---|
| `=` | Assign | Standard assignment. | `x = 5i32;` |
| `+=` | Add & Assign | Add and assign in place. | `x += 5i32;` |
| `-=` | Subtract & Assign | Subtract and assign in place. | `x -= 5i32;` |
| `*=` | Multiply & Assign | Multiply and assign in place. | `x *= 5i32;` |
| `/=` | Divide & Assign | Divide and assign in place. | `x /= 5i32;` |
| `%=` | Modulo & Assign | Modulo and assign in place. | `x %= 5i32;` |

---

## 3. Comparison & Relational

| Operator | Name | Description | Example |
|---|---|---|---|
| `==` | Equality | Checks if two values are equal. | `a == b` |
| `!=` | Inequality | Checks if two values are not equal. | `a != b` |
| `<` | Less Than | Standard relational comparison. | `a < b` |
| `>` | Greater Than | Standard relational comparison. | `a > b` |
| `<=` | Less Than or Equal | Standard relational comparison. | `a <= b` |
| `>=` | Greater Than or Equal| Standard relational comparison. | `a >= b` |
| `<=>` | Spaceship | 3-way comparison. Returns `-1`, `0`, or `1`. | `a <=> b` |

---

## 4. Logical & Bitwise

| Operator | Name | Description | Example |
|---|---|---|---|
| `!` | Logical NOT | Inverts a boolean condition. | `!is_true` |
| `&&` | Logical AND | Short-circuiting logical AND. | `a && b` |
| `\|\|` | Logical OR | Short-circuiting logical OR. | `a \|\| b` |
| `~` | Bitwise NOT | Inverts the bits of an integer. | `~a` |
| `&` | Bitwise AND | Bitwise AND operation. | `a & b` |
| `\|` | Bitwise OR | Bitwise OR operation. | `a \| b` |
| `^` | Bitwise XOR | Bitwise XOR operation. | `a ^ b` |
| `<<` | Left Shift | Shifts bits left. | `a << 2` |
| `>>` | Right Shift | Shifts bits right (arithmetic/logical based on sign). | `a >> 2` |

---

## 5. Result & Safety (Error Handling)

| Operator | Name | Description | Example |
|---|---|---|---|
| `?` | Safe Unwrap | Unwraps a Result/Optional. If error/NIL, evaluates to right-hand side default. | `val = fn() ? 0i32;` |
| `??` | Null Coalesce | Unwraps an Optional. If NIL, evaluates to right-hand side default. | `val = opt ?? 0i32;` |
| `?!` | Emphatic Unwrap | Unwraps a Result. If error, triggers `failsafe`. | `val = fn() ?!;` |
| `?.` | Safe Navigation | Accesses a field of an Optional. Returns NIL if Optional is NIL. | `val = obj?.field;` |
| `?\|` | Defaults | Desugars to the `defaults` keyword at parse time. | `expr ?\| default;` |
| `_?` | Drop | Desugars to `drop expr` — discards the Result without checking it. | `_? my_func();` |
| `_!` | Raw | Desugars to `raw expr` — unsafely bypasses error checking. | `val = _! my_func();` |
| `_~` | Discard | Desugars to `discard(expr)` — suppresses unused variable warnings. | `_~ unused;` |
| `!!!` | Failsafe Shorthand | Immediately invokes `failsafe(err)`. | `!!! errCode;` |

---

## 6. Pointers & Memory

> **Note:** The `*` character is strictly reserved for `extern` blocks to maintain C ABI compatibility (e.g. `void*`). Inside Nitpick, pointers exclusively use the `->` operator.

| Operator | Name | Description | Example |
|---|---|---|---|
| `@` | Address-Of | Takes the memory address of an l-value. | `int32->:ptr = @val;` |
| `<-` | Dereference | Extracts the value FROM a pointer. | `int32:val = <-ptr;` |
| `->` | Pointer To | In types: pointer declaration ONLY. | `type->:p` |
| `.` | Member Access | Unified member access (automatically dereferences if pointer). | `my_struct.field` |
| `#` | Pin | Prevents the Garbage Collector from moving the memory. | `#obj` |

---

## 7. Casting & Type Operations

| Operator | Name | Description | Example |
|---|---|---|---|
| `=>` | Safe Cast | Checked cast. Triggers compiler warning/error on narrowing data loss. | `val => int32` |
| `=>!` | Unchecked Cast | Direct bit-cast/truncation without checking. Explicitly suppresses data loss warnings. | `val =>! int32` |
| `:` | Type Annotation | Used in variable declarations and ternary separators. | `int32:x` |
| `::<T>` | Turbofish | Provides explicit type parameters to generic functions. | `func::<int32>()` |
| `<T>?` | Optional Type | Declares a type as Optional. | `int64?` |

---

## 8. Control Flow & Pipelines

| Operator | Name | Description | Example |
|---|---|---|---|
| `is` | Ternary Conditional | Ternary branching. `is cond : then : else` | `is x > 0 : 1 : -1` |
| `..` | Inclusive Range | Inclusive range `[a, b]`. Used in `for` and `pick`. | `0..10` |
| `...` | Exclusive Range | Exclusive range `[a, b)`. Used in `for` and `pick`. | `0...10` |
| `\|>` | Pipe Forward | Passes the left expression as the first argument to the right function. | `val \|> func()` |
| `<\|` | Pipe Backward | Evaluates the right expression first, passes to the left function. | `func() <\| val` |
| `$` | Iteration Variable| Safe loop counter explicitly bound inside `till` and `loop`. | `x += $;` |

---

## 9. Literals & Strings

| Operator | Name | Description | Example |
|---|---|---|---|
| `""` | String Literal | Standard UTF-8 string literal. | `"Hello"` |
| `r""` | Raw String Literal| Raw string (no escape processing) useful for regex/paths. | `r"C:\Path"` |
| `""" """`| Triple Quote | Multi-line string literal, preserves indentation/newlines. | `"""Line 1..."""` |
| `''` | Char Literal | Single character literal. | `'A'` |
| ` `` ` | Template Literal | String template. | `` `Hello` `` |
| `&{ }` | Interpolation | Evaluates and interpolates an expression inside a template. | `` `x: &{x}` `` |
| `\` | Escape | Escape sequence character. | `\n`, `\t` |

---

## 10. Comments

| Operator | Name | Description | Example |
|---|---|---|---|
| `//` | Line Comment | Comments out the rest of the line. | `// this is a comment` |
| `/*` | Block Start | Begins a multi-line comment block. | `/* comment start` |
| `*/` | Block End | Ends a multi-line comment block. | `comment end */` |
