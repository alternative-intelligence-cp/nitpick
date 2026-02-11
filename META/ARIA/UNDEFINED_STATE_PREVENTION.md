# Undefined State Prevention: A Multi-Layered Approach

**Design Goal**: Never reach an undefined state. Ever.

**Key Insight**: No single mechanism can prevent all undefined states. Different kinds of "wrongness" require different safety layers.

## The Three Safety Layers

### Layer 1: Unknown (Computational Uncertainty)

**Purpose**: Handle indeterminate mathematical values

**Scope**: Operations that have no well-defined mathematical answer
- Division by zero: `5/0`, `0/0`
- Infinity arithmetic: `∞ - ∞`, `∞ / ∞`
- Complex number edge cases: `sqrt(-1)` (in real mode)
- Invalid operations on special numbers

**Behavior**: 
- Operation produces `Unknown` value instead of crashing
- Unknown propagates through calculations: `(5 + Unknown) → Unknown`
- Type system tracks: `ok(value)` returns `false` for Unknown
- Compiler enforces checking before use in critical contexts

**Philosophy**: These aren't errors - they're valid mathematical states representing "genuinely unknowable." The computation continues safely until the Unknown is checked.

**Example**:
```aria
func:safe_divide = int32(int32:a, int32:b) {
    int32:result = a / b;  // If b==0, result becomes Unknown
    
    if (!ok(result)) {
        pass(-1i32);  // Handle the Unknown case
    }
    
    pass(result);
}
```

**Implementation**: Division, modulo, and other unsafe operations inject runtime checks that produce Unknown sentinel values instead of undefined behavior.

---

### Layer 2: Result<T> (Expected Failures)

**Purpose**: Handle operations that can legitimately fail

**Scope**: I/O, parsing, resource allocation, validation
- File operations: file not found, permission denied
- Network operations: timeout, connection refused
- Parsing: invalid format, out of range
- Optional computations: lookup not found

**Behavior**:
- Functions return `Result<T>` wrapper
- Caller must handle both success and error cases
- Unwrap operators provide ergonomic extraction:
  - `?` - casual unwrap with default
  - `??` - null coalesce for pointers/optionals
  - `?!` - emphatic unwrap (semantically stronger intent)
- Compiler enforces "no checky no val" - can't access `.value` without checking `.is_error`

**Philosophy**: Failure is a normal part of the operation's contract, not an exceptional circumstance. The type system makes this explicit.

**Example**:
```aria
func:load_config = Result<Config>() {
    // Might fail - that's expected
    pass(readFile("config.json"));
}

func:main = int32() {
    // LOCAL handling: use default if loading fails
    Config:cfg = load_config() ?! getDefaultConfig();
    pass(0i32);
}
```

---

### Layer 3: Failsafe (Catastrophic Failures)

**Purpose**: Emergency handler for unrecoverable errors

**Scope**: System-level failures that cannot be locally handled
- Out of memory
- Corrupted program state
- Assertion failures
- Stack overflow
- Hardware faults

**Behavior**:
- Every program **must** define `failsafe(int32:err_code)` function
- Compiler enforces presence - won't compile without it
- Explicit escalation via `!!!` operator
- Empty failsafe is valid - documents conscious decision
- Last line of defense before program termination

**Philosophy**: "Life preserver on the boat even if you can swim." You might never need it, but when the ship sinks, you'll be glad it's there. The requirement creates accountability - you cannot claim "I forgot."

**Example**:
```aria
// REQUIRED - every program must have this
func:failsafe = int32(int32:err_code) {
    // Log error, cleanup resources, notify operators
    // Even empty is valid - it documents your choice
    logCriticalError(err_code);
    cleanupResources();
    pass(err_code);
};

func:check_invariants = int32() {
    if (critical_system_invariant_violated) {
        // EMERGENCY: trigger global handler
        !!! INVARIANT_VIOLATION;
    }
    pass(0i32);
}
```

---

## Why Multiple Layers?

**Single-layer approaches fail:**

1. **"Just use exceptions"**: 
   - Hidden control flow
   - Performance overhead
   - Doesn't distinguish expected vs catastrophic
   - Can be ignored

2. **"Just use Result everywhere"**:
   - Verbose for simple math
   - Wrong abstraction for "unknowable" values
   - Doesn't handle system-level failures

3. **"Just crash on errors"**:
   - Unsafe for production systems
   - No graceful degradation
   - Lost opportunity to handle recoverable cases

**Our approach**: Each layer handles what it's designed for

```
┌─────────────────────────────────────┐
│ Layer 3: Failsafe (catastrophic)   │  System-level, unrecoverable
│  !!!  operator, mandatory function  │  "Ship is sinking"
├─────────────────────────────────────┤
│ Layer 2: Result<T> (expected fail) │  Normal error conditions
│  ?, ??, ?! operators                │  "Operation might not work"
├─────────────────────────────────────┤
│ Layer 1: Unknown (indeterminate)   │  Mathematical uncertainty
│  ok() checker, propagation          │  "Answer is unknowable"
└─────────────────────────────────────┘
```

---

## The `!` Connection

The exclamation mark semantically means "not right" across all uses:

- `!` - logical NOT: "this is not true"
- `?!` - emphatic unwrap: "this isn't right, but I have a fallback"
- `!!!` - failsafe call: "VERY MUCH NOT RIGHT - EMERGENCY"

This creates intuitive escalation from local to global error handling.

---

## Design Guarantees

**No undefined states possible because:**

1. **Mathematical operations** → Unknown (never undefined)
2. **Expected failures** → Result<T> (type system enforces handling)
3. **Catastrophic failures** → failsafe() (compiler enforces presence)
4. **Null pointers** → Eliminated via optional types and null coalescing
5. **Uninitialized variables** → Compiler enforces initialization
6. **Type confusion** → Static type system prevents

**The programmer cannot:**
- Forget to implement failsafe (compiler error)
- Access Result value without checking (compile error)
- Use Unknown in critical context unchecked (type system)
- Dereference null (no null in type system)
- Reach undefined behavior (all paths covered)

---

## Implementation Status

- [x] Layer 1: Unknown literal and propagation
- [x] Layer 2: Result<T> with unwrap operators (?, ??, ?!)
- [x] Layer 3: Mandatory failsafe() and !!! operator
- [ ] Divide-by-zero → Unknown conversion
- [ ] Overflow → Unknown conversion
- [ ] Integration testing across all layers

---

## Usage Guidelines

**When to use Unknown:**
- Mathematical operations that can be indeterminate
- Computations that might not have answers
- Continue execution with "I don't know" values

**When to use Result<T>:**
- File I/O, network operations
- Parsing and validation
- Resource allocation
- Any operation where failure is a normal possibility

**When to use failsafe (!!!):**
- Assertion violations
- Invariant corruption
- Out of memory
- Unrecoverable system errors
- "This should never happen" scenarios

---

## The Life Preserver Analogy

**Unknown** = Swim training
- Learn to handle rough conditions
- Part of normal operations
- Expected to encounter sometimes

**Result<T>** = Life jacket
- Comfortable to wear always
- Expected to need occasionally
- Normal safety equipment

**Failsafe** = Emergency life raft
- Mandatory on every vessel
- Hope to never use
- Critical when needed
- Required by regulation (compiler)

You can be an expert swimmer, but the boat still needs emergency equipment. The compiler enforces this because lives (or at least data) depend on it.

---

**Author's Note**: "I don't think one approach can cover it all. The unknown was the original idea, and I think it can cover most if we can somehow make things like divide by zero turn into unknown rather than undefined behind the scenes. Then they can actually handle that instead of crashing you know. But the failsafe was like... requiring people to have a life preserver on the boat even if they can swim. It's for when things don't go as planned."
