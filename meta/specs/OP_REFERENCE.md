# Nitpick Operator Reference

This document provides a comprehensive list of all operators available in the Nitpick programming language, categorized by their primary function.

> **⚠️ FORMAL VERIFICATION RULE**: Operator overloading is strictly forbidden in Nitpick. The meaning of an operator is fixed at the language level, ensuring deterministic, highly-auditable code.

---

## 0. Precedence

Highest to lowest. Adopted from `FORMAL_DRAFT` 04 §4.2 with corrections.

| | Level | Operators |
|---|---|---|
| 1 | Postfix | `++` `--` `()` `[]` `.` `?.` |
| **2** | **Result unary** *(right-assoc)* | **`raw` `_!`** · **`drop` `_?`** · **`await`** · **`relay` `_^`** |
| 3 | Pipeline | `\|>` `<\|` |
| 4 | Cast | `=>` `=>!` |
| 5 | Unary | `!` `~` `-` `@` `<-` `$$i` `$$m` |
| 6 | Multiplicative | `*` `/` `%` |
| 7 | Additive | `+` `-` |
| 8 | Shift | `<<` `>>` |
| 9 | Range / Spread | `..` `...` `..*` `..^` |
| 10 | Relational | `<` `<=` `>` `>=` `<=>` |
| 11 | Equality | `==` `!=` |
| 12 | Bitwise AND | `&` |
| 13 | Bitwise XOR | `^` |
| 14 | Bitwise OR | `\|` |
| 15 | Logical AND | `&&` (short-circuiting) |
| 16 | Logical OR | `\|\|` (short-circuiting) |
| 17 | Null Coalescing | `??` |
| 18 | Ternary / Defaults | `is` `?\|` `defaults` |
| 19 | Assignment | `=` `+=` `-=` `*=` `/=` `%=` `&=` `\|=` `^=` `<<=` `>>=` |

> **Level 2 is new (D-081).** `raw`, `drop`, `await`, and `relay` previously
> appeared in **no** level, leaving their binding undefined — whether
> `raw a.eq(b)` takes the receiver or the call, whether `raw f() => int32` casts
> the `Result` or the value. The position is forced, not chosen: looser than
> Postfix so the operand is the whole call; tighter than Cast and Pipeline so it
> is the *value* that gets cast or piped, a `Result` being meaningless to either.
> `discard` / `_~` is absent because D-060 makes it a statement.


**Corrections against `FORMAL_DRAFT` 04 §4.2:**

- **`->` removed from level 1.** It was listed as a postfix member-access
  operator; `->` is type-position only, and `.` handles all member access with
  automatic dereference (D-006).
- **`=>!` added to the Cast level** (now level 4). Only `=>` was listed, but both
  are cast operators and they must share a precedence level (D-021).
- **`#` removed from the Unary level** (now level 5). It was the pin operator; pinning is obsolete
  without a collector, and `#` is now the compiler-directive sigil (D-020).

### 0.1 Expression semantics

- **Assignment is a statement, not an expression** (D-060). It yields nothing, so
  `int32:y = (x = 5i32) + 2i32;` does not parse.
  > ⚠️ **Corrected.** A previous revision stated the opposite — *"assignment is an
  > expression and evaluates to the assigned value"* — and gave that example as
  > working. D-060 settled the statement/expression split against `FORMAL_DRAFT`
  > 04 §4.1 and made assignment a statement, which is also what the prototype
  > does.
  >
  > A consequence worth keeping: `if (x = 3)` needs no dedicated rule rejecting
  > it. It is **not expressible**, so `NITPICK-IF-002` describes a diagnostic for
  > a program that cannot be written. Conditions must still be a strict `bool`.
- **`&&` and `||` short-circuit** and require strictly boolean operands.
- **`<=>`** (spaceship) yields `int32`: `-1`, `0`, or `1`.
- **`?|` / `defaults`** is a scoped fallback for an entire expression chain, where
  the fallback must be a literal or a simple identifier:
  ```nitpick
  int32:val = (complex_func() + 5i32) ?| 0i32;
  int32:val = (complex_func() + 5i32) defaults 0i32;
  ```

---

## 1. Arithmetic & Mathematical

| Operator | Name | Description | Example |
|---|---|---|---|
| `+` | Add | Safe addition. | `a + b` |
| `-` | Subtract | Safe subtraction. | `a - b` |
| `*` | Multiply | Safe multiplication. Note: also used for pointer syntax in `extern` blocks. | `a * b` |
| `/` | Divide | Safe division. Divide-by-zero behavior is **type-directed** — see below. | `a / b` |
| `%` | Modulo | Remainder operation. Same divide-by-zero rule as `/`. | `a % b` |
| `++` | Increment | Post/pre-increment. | `i++` or `++i` |
| `--` | Decrement | Post/pre-decrement. | `i--` or `--i` |
| `**` | Power | Exponentiation (Standard Library expansion). | `2 ** 8` |

### 1.1 Division by zero and overflow

Behavior is selected by **operand type**, not by context (D-007). The type is
written explicitly at every declaration, so which discipline applies is visible
at a glance.

**Divide by zero and overflow are different events and are not decided together**
(D-037). Overflow has a defined two's-complement result; division by zero has no
result at all.

| Operand type | **Overflow / underflow** | **Divide by zero** | Intended for |
|---|---|---|---|
| `tbb8`…`tbb256` | yields **ERR** — sticky, propagates, checkable | yields **ERR** | control loops, actuator paths, anything that must degrade rather than stop |
| `int32`, `uint64`, … | **wraps** — defined, no check, no trap | **traps to `failsafe`** | ordinary code, parsing, setup, tooling |
| `flt32`…`flt512` | **IEEE 754** — `inf` / `nan`, no trap | **IEEE 754** — `inf` / `nan` | numeric work |

> ⚠️ **Corrected.** A previous revision of this table gave one column for
> "divide by zero / overflow" and had plain integers **trapping on overflow**.
> That contradicts **D-037**, which struck exactly that reading: wrapping is
> *defined* behaviour in two's complement and is routinely what is wanted —
> hashing, checksums, PRNGs, modular arithmetic — so trapping there would make
> ordinary correct code unrunnable and leave no way to express wrapping at all.
> D-037 corrected `TYPE_REFERENCE.md` §1.2 and §1.3 at the time; this table was
> missed. The float row was wrong for the same reason — IEEE 754 produces `inf`
> and `nan` rather than trapping.
>
> **`tbb` is how you ask for overflow to be an error**, and that choice is made at
> the declaration, visible at every use.

ERR is **absorbing and overrides identities**: `ERR * 0` is `ERR`, not `0`; so is
`ERR - ERR`. Once a value is ERR, no arithmetic yields a non-ERR result from it.
Only an explicit check (`is_err`) or a fallback (`?`) leaves the state.
*(`ok()` was listed here and is removed — D-097.)*

Because `bool` has exactly two values and cannot represent ERR, **comparing or
branching on an ERR value traps to `failsafe`**. ERR flows freely through data
and stops at control flow — which is where a tainted value would otherwise steer
a decision. Use `is_err(x)` to test without trapping, or a `pick` with an explicit
`ERR:` arm.

Bitwise operators (`&`, `|`, `^`, `~`, `<<`, `>>`) are **rejected on `tbb` types**:
they can fabricate the ERR sentinel out of valid operands (`~127i8` is `-128`) or
destroy it (`ERR & 0` is `0`). Cast to a plain integer first — which traps if the
value is ERR, so the taint cannot cross silently. See D-008.

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
| `?!` | Emphatic Unwrap | Unwraps a Result. If error, calls `failsafe(errCode)`. **Takes exactly one argument.** | `val = fn() ?! 99i32;` |
| `?.` | Safe Navigation | Accesses a field of an Optional. Returns NIL if Optional is NIL. | `val = obj?.field;` |
| `?\|` | Defaults | Desugars to the `defaults` keyword at parse time. | `expr ?\| default;` |
| `_?` | Drop | Desugars to `drop expr` — discards the Result without checking it. | `_? my_func();` |
| `_!` | Raw | Desugars to `raw expr` — unsafely bypasses error checking. | `val = _! my_func();` |
| **`_^`** | **Relay** | Desugars to `relay expr` — **propagates the error to the caller, verbatim** (D-080). On error the enclosing function returns immediately with the same code; otherwise evaluates to `.value`. `defer` runs — it is a normal exit path, not a trap. Illegal in `main` / `failsafe`. | `val = _^ my_func();` |
| `_~` | Discard | **Two positions** (D-089). As a statement it desugars to `discard(expr)` and suppresses the unused-variable warning. At a **declaration site**, `Type:_~name` marks a parameter the body deliberately does not read — and reading it anyway is an error, not a warning. | `_~ unused;` / `cstring[]:_~argv` |
| `!!!` | Failsafe Shorthand | Immediately invokes `failsafe(err)`. | `!!! errCode;` |

> ### The two meanings of `!` (D-046)
>
> | Position | Meaning | Forms |
> |---|---|---|
> | **leading** | negation | `!x`, `!=` |
> | **trailing or repeated** | unchecked / emphatic | `?!`, `=>!`, `_!`, `!!!` |
>
> The distinction is **lexical, not contextual** — which meaning applies is
> visible from the token itself, without knowing the operand types or the
> surrounding code. That is what separates it from the `->` problem D-006 fixed,
> where one token in one position meant different things depending on its operand.
>
> **`!!` no longer exists** and **macro invocation is `#name(args)`**, not
> `name!(args)` — that spelling was indistinguishable from an emphatic operation
> and carried no positional cue.

---

## 6. Pointers & Memory

> **Note:** The `*` character is strictly reserved for `extern` blocks to maintain C ABI compatibility (e.g. `void*`). Inside Nitpick, pointers exclusively use the `->` operator.

| Operator | Name | Description | Example |
|---|---|---|---|
| `@` | Address-Of | Takes the memory address of an l-value. **This is `@`'s only meaning** — it is never a builtin prefix (D-020). | `int32->:ptr = @val;` |
| `<-` | Dereference | Extracts the value FROM a pointer. | `int32:val = <-ptr;` |
| `->` | Pointer To | In types: pointer declaration ONLY. | `type->:p` |
| `.` | Member Access | Unified member access (automatically dereferences if pointer). Handles **all** member access, including UFCS method calls. | `my_struct.field` |

> **`#` is no longer the pin operator.** Pinning existed to stop the garbage
> collector relocating memory; with no collector (D-003) nothing relocates
> implicitly, so the operator has no purpose. `#` is now exclusively the
> **compiler-directive sigil**: `#name<T>(...)` for builtins, `#[name]` for
> attributes (D-020).

> **Direction is semantic.** `->` points *to* a target, `<-` brings a value
> *back*, `=>` goes *from* one type *to* another. `->` was removed as a
> member-access operator because member access brings data *toward* the reader —
> the arrow pointed the wrong way, and the `->`-versus-`.` distinction bought
> nothing when the intent and outcome were identical.

---

## 7. Casting & Type Operations

| Operator | Name | Description | Example |
|---|---|---|---|
| `=>` | Safe Cast | Checked cast. **Compile-time error** if data loss is possible — not a runtime trap. | `val => int32` |
| `=>!` | Unchecked Cast | Direct bit-cast/truncation without checking. The explicit opt-out from the above. | `val =>! int32` |

> **Casts involving `tbb` are never straight bit operations.** The ERR sentinel is
> a different bit pattern at every width, so sign-extending `tbb8` ERR (`-128`)
> produces a *valid* `tbb32` value. Every `tbb` cast checks for the sentinel and
> maps it to the target's sentinel; `tbb`→plain-integer traps on ERR; and
> plain-integer→`tbb` traps on a source value that would forge one. `=>!`
> preserves the ERR *state*, not the bit pattern, so it cannot launder a taint.
> See D-008 §6.

> **Integer→pointer casting is illegal.** It is suspended only by
> `#wild_ptr<T>(addr)` in `wild` context (D-019).
| `:` | Type Annotation | Used in variable declarations and ternary separators. | `int32:x` |
| `::<T>` | Turbofish | Explicit type arguments in **expression position** — the only form there (D-064). Bare `<T>` is type-position only. | `func::<int32>()` |
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
