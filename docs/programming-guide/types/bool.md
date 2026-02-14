# bool: Boolean Type

## Overview

**Purpose**: Logical true/false values for control flow, conditions, and binary state.

**Characteristics**:
- Two values: `true` and `false`
- Logical semantics (not numeric)
- 1 bit of information (typically 1 byte storage)
- Primary use: Conditions, flags, binary decisions

**When to Use**:
- Conditional logic (if, loop till, etc.)
- Flags and state (enabled/disabled, found/not-found)
- Function predicates (is_valid, has_permission, etc.)
- Boolean algebra (logical operations)

**When NOT to Use**:
- Numeric operations (use int1 if need arithmetic)
- Multi-state values (use int, enum, or trit for yes/maybe/no)
- Bit manipulation (use uint8, uint16, etc.)
- When NULL/unknown needed (use NIL or unknown)

---

## bool vs int1: Semantics Matter

### The Critical Distinction

**bool**: Logical semantics
- Values: `true` and `false` (not 0 and 1)
- Purpose: Represent truth values
- Operations: AND, OR, NOT, XOR (logical)
- Intent: "Is this condition satisfied?"

**int1**: Numeric semantics
- Values: `-1` and `0` (signed integer)
- Purpose: Smallest numeric type
- Operations: +, -, *, / (arithmetic with wrapping)
- Intent: "What is this integer value?"

### Examples of the Difference

```aria
// bool: Logical intent clear
bool:is_valid = check_input(data);
if (is_valid) {
    process(data);
}

// int1: Numeric intent (confusing for logic!)
int1:status = 0;  // -1 or 0, not true/false
if (status) {  // UNCLEAR: non-zero is truthy, but this is confusing
    // What does this mean? status == -1?
}

// NEVER do this
int1:flag = -1;  // "true"?
int1:flag = 0;   // "false"?
// This obscures intent - use bool!
```

### Conversion

```aria
// int1 → bool (any non-zero is true)
int1:value = -1;
bool:b = value != 0;  // true (explicit conversion)

// bool → int1 (rarely useful)
bool:flag = true;
int1:i = flag ? -1 : 0;  // Explicit, shows intent

// Don't mix them
bool:b = true;
int1:i = -1;
// b and i serve different purposes!
```

---

## Basic Operations

### Logical AND

```aria
bool:a = true;
bool:b = false;

bool:result = a && b;  // false (both must be true)

// Truth table:
// true  && true  = true
// true  && false = false
// false && true  = false
// false && false = false
```

**Short-Circuit Evaluation**:
```aria
// Right side NOT evaluated if left is false
bool:result = expensive_check() && another_check();
// If expensive_check() returns false, another_check() never runs

// Useful for safety
bool:safe = (pointer != NULL) && pointer.is_valid();
// Won't call is_valid() if pointer is NULL (avoids crash)
```

### Logical OR

```aria
bool:a = true;
bool:b = false;

bool:result = a || b;  // true (at least one must be true)

// Truth table:
// true  || true  = true
// true  || false = true
// false || true  = true
// false || false = false
```

**Short-Circuit Evaluation**:
```aria
// Right side NOT evaluated if left is true
bool:result = quick_check() || slow_check();
// If quick_check() returns true, slow_check() never runs

// Early exit pattern
bool:found = check_cache() || check_database();
// Only checks database if not in cache
```

### Logical NOT

```aria
bool:a = true;
bool:result = !a;  // false (inverts truth value)

// Truth table:
// !true  = false
// !false = true
```

**Double Negation**:
```aria
bool:a = true;
bool:b = !!a;  // true (back to original)

// Occasionally used for clarity
bool:is_not_empty = !list.is_empty();
bool:has_items = !!list.length;  // Convert to bool explicitly
```

### Logical XOR (Exclusive OR)

```aria
bool:a = true;
bool:b = false;

bool:result = a ^^ b;  // true (exactly one must be true)

// Truth table:
// true  ^^ true  = false (both true)
// true  ^^ false = true  (one true)
// false ^^ true  = true  (one true)
// false ^^ false = false (both false)
```

**Use Cases**:
```aria
// Toggle state
bool:flag = false;
flag ^^= true;  // Toggle (flag is now true)
flag ^^= true;  // Toggle again (flag is now false)

// Parity check (even number of trues = false)
bool:parity = bit1 ^^ bit2 ^^ bit3 ^^ bit4;

// Exactly one condition
bool:valid = has_password ^^ has_biometric;  // Must have one, not both
```

---

## Truthiness: Converting to bool

### Numeric Types

```aria
// Zero is false, non-zero is true
int32:value = 0;
bool:b1 = value != 0;  // false (explicit)

int32:count = 5;
bool:b2 = count != 0;  // true

// Explicit conversion required in Aria (no implicit)
if (count != 0) {  // Explicit check
    log.write("Count is non-zero\n");
}

// NOT allowed in Aria (safety)
// if (count) { ... }  // COMPILE ERROR: int32 is not bool
```

### Pointers and References

```aria
// NULL check
string:ptr = get_optional_string();

if (ptr != NULL) {  // Explicit NULL check
    log.write(ptr);
}

// NOT allowed
// if (ptr) { ... }  // COMPILE ERROR (must be explicit)
```

### Containers

```aria
// Empty check (explicit)
int32[]:array = {1, 2, 3};

if (array.length > 0) {  // Explicit non-empty check
    process(array);
}

// Clearer: Use semantic methods
if (!array.is_empty()) {
    process(array);
}
```

---

## Common Patterns

### Flags and State

```aria
// Feature flags
bool:feature_enabled = true;
bool:debug_mode = false;
bool:production = true;

// State tracking
bool:initialized = false;
bool:connected = false;
bool:ready = false;

void:initialize() {
    setup_resources();
    initialized = true;
}

void:connect() {
    if (!initialized) {
        log.write("ERROR: Not initialized\n");
        return;
    }
    establish_connection();
    connected = true;
}
```

### Guard Clauses

```aria
bool:is_valid_input(string:input) -> bool {
    if (input == NULL) return false;
    if (input.length == 0) return false;
    if (input.length > 1000) return false;
    return true;
}

void:process(string:input) {
    // Guard clauses for early exit
    if (!is_valid_input(input)) {
        log.write("ERROR: Invalid input\n");
        return;
    }
    
    // Main logic here (input known valid)
    do_work(input);
}
```

### Predicate Functions

```aria
// Functions returning bool (predicates)
bool:is_even(int32:n) -> bool {
    return (n % 2) == 0;
}

bool:is_prime(int32:n) -> bool {
    if (n < 2) return false;
    if (n == 2) return true;
    if (is_even(n)) return false;
    
    int32:limit = (n.to_float().sqrt()).to_int32();
    loop (limit - 2 times:i) {
        if ((n % (i + 3)) == 0) return false;
    }
    return true;
}

// Usage (clear intent)
if (is_prime(17)) {
    log.write("17 is prime\n");
}
```

### Boolean Accumulation

```aria
// Checking multiple conditions
bool:all_valid(int32[]:values) -> bool {
    bool:valid = true;
    loop (values.length:i) {
        valid = valid && is_valid(values[i]);
        if (!valid) break;  // Early exit
    }
    return valid;
}

bool:any_valid(int32[]:values) -> bool {
    bool:valid = false;
    loop (values.length:i) {
        valid = valid || is_valid(values[i]);
        if (valid) break;  // Early exit
    }
    return valid;
}
```

### State Machines

```aria
struct:Connection {
    bool:initialized;
    bool:connected;
    bool:authenticated;
    bool:ready;
}

void:advance_state(Connection:conn) {
    if (!conn.initialized) {
        initialize(conn);
        conn.initialized = true;
    } else if (!conn.connected) {
        connect(conn);
        conn.connected = true;
    } else if (!conn.authenticated) {
        authenticate(conn);
        conn.authenticated = true;
    } else {
        conn.ready = true;
    }
}
```

---

## Comparison Operations

### Relational Operators Return bool

```aria
int32:a = 5;
int32:b = 10;

bool:less = a < b;           // true
bool:greater = a > b;        // false
bool:less_equal = a <= b;    // true
bool:greater_equal = a >= b; // false
bool:equal = a == b;         // false
bool:not_equal = a != b;     // true
```

### Chaining Comparisons

```aria
// NOT allowed (for safety)
// bool:result = a < b < c;  // COMPILE ERROR

// Instead, explicit
bool:result = (a < b) && (b < c);

// Range check pattern
bool:in_range(int32:value, int32:min, int32:max) -> bool {
    return (value >= min) && (value <= max);
}
```

---

## Memory Representation

### Storage Size

**Logical**: 1 bit of information (true or false)

**Physical**: Typically 1 byte (8 bits)
- Can't address individual bits on most architectures
- CPU operates on byte-aligned data
- Cache lines work in multiples of bytes

```aria
// 1 bool: 1 byte
bool:flag;

// Array of 1000 bools: 1000 bytes (not 125 bytes!)
bool[1000]:flags;
```

### Bit Packing (Advanced)

**When Memory Critical**:
```aria
// Manually pack bools into bits (uint8)
struct:PackedFlags {
    uint8:bits;  // 8 flags in 1 byte
}

void:set_flag(PackedFlags:pf, uint8:index) {
    pf.bits |= (1u8 << index);
}

bool:get_flag(PackedFlags:pf, uint8:index) -> bool {
    return (pf.bits & (1u8 << index)) != 0;
}

// Usage
PackedFlags:flags = {bits: 0};
set_flag(flags, 3);  // Set bit 3
bool:is_set = get_flag(flags, 3);  // true
```

**Trade-offs**:
- Saves memory (8× reduction)
- More complex code
- Slower access (bit manipulation overhead)
- Only worth it for large arrays or tight memory constraints

---

## Control Flow Integration

### if Statements

```aria
bool:condition = check_something();

if (condition) {
    // Executes if true
    log.write("Condition met\n");
}

if (!condition) {
    // Executes if false
    log.write("Condition not met\n");
}

if (condition) {
    // True branch
} else {
    // False branch
}
```

### Loops

```aria
// Loop till (while false)
bool:done = false;
loop till done {
    work();
    done = check_completion();
}

// Loop while (custom)
bool:keep_going = true;
loop till !keep_going {
    process();
    keep_going = should_continue();
}
```

### Ternary Operator

```aria
bool:admin = user.has_permission("admin");
string:message = admin ? "Welcome, admin" : "Welcome, user";

// Can be nested (but keep readable)
int32:max = (a > b) ? a : b;
```

---

## Anti-Patterns

### Using Integers as Booleans

```aria
// BAD: Integer flags (ambiguous)
int32:flag = 1;  // What does 1 mean? true? Or just 1?
if (flag) {  // COMPILE ERROR in Aria (good!)
    // ...
}

// GOOD: Boolean flags (clear intent)
bool:enabled = true;
if (enabled) {
    // ...
}
```

### Magic Boolean Parameters

```aria
// BAD: Unclear what true/false means
process_data(data, true);   // What does true mean?
process_data(data, false);  // What does false mean?

// GOOD: Named parameters or enum
process_data(data, verbose: true);
process_data(data, verbose: false);

// EVEN BETTER: Separate functions
process_data_verbose(data);
process_data_quiet(data);
```

### Boolean Blindness

```aria
// BAD: Loses information
bool:result = parse_config(file);
if (!result) {
    log.write("Failed\n");  // Why did it fail?
}

// GOOD: Use result types
Result<Config>:result = parse_config(file);
if (result.is_error()) {
    log.write("Failed: ");
    log.write(result.error_message());
    log.write("\n");
}
```

### Redundant Comparisons

```aria
// BAD: Comparing bool to true/false
bool:is_valid = check();
if (is_valid == true) {  // Redundant!
    // ...
}

// GOOD: Use bool directly
if (is_valid) {
    // ...
}

// BAD: Double negative
if (!is_invalid == false) {  // Confusing!
    // ...
}

// GOOD: Simplify logic
if (is_valid) {
    // ...
}
```

### Over-Complex Boolean Expressions

```aria
// BAD: Hard to read
if (!(!a || !b) && (c || !d) && !(e && !f)) {
    // ...
}

// GOOD: Use intermediate variables
bool:a_and_b = a && b;
bool:c_or_not_d = c || !d;
bool:not_e_or_f = !e || f;

if (a_and_b && c_or_not_d && not_e_or_f) {
    // ...
}

// EVEN BETTER: Extract to function
bool:should_process(/* params */) -> bool {
    if (!(a && b)) return false;
    if (!(c || !d)) return false;
    if (e && !f) return false;
    return true;
}
```

---

## Common Pitfalls

### Accidental Assignment

```aria
// Aria prevents this at compile time
bool:flag = true;

// COMPILE ERROR: Cannot assign in condition
// if (flag = false) { ... }

// Must be explicit comparison (though unnecessary for bool)
if (flag == false) {  // Allowed but unnecessary
    // ...
}

// Better
if (!flag) {
    // ...
}
```

### Forgetting Short-Circuit

```aria
// Dangerous: May crash if ptr is NULL
if (ptr.is_valid() && ptr != NULL) {  // WRONG ORDER!
    // ...
}

// Safe: NULL check first
if (ptr != NULL && ptr.is_valid()) {  // Correct
    // ...
}
```

### Equality with NaN or Special Values

```aria
// Careful with floats
float:value = compute();

// This may be false even if value "is" NaN
if (value == float.nan) {  // WRONG! NaN != NaN
    // Never executes
}

// Correct
if (value.is_nan()) {
    // ...
}
```

---

## Best Practices

### Naming Conventions

**Use Question Form**:
```aria
bool:is_valid;
bool:has_permission;
bool:can_execute;
bool:should_retry;
bool:was_successful;
```

**Positive Names** (avoid negatives when possible):
```aria
// GOOD
bool:enabled;
if (enabled) { ... }

// LESS CLEAR (double negative)
bool:disabled;
if (!disabled) { ... }  // Harder to reason about
```

### Early Returns

```aria
// Use guard clauses with bool for clarity
bool:process(Data:data) -> bool {
    if (data == NULL) return false;
    if (!data.is_valid()) return false;
    if (data.size == 0) return false;
    
    // Main logic (all guards passed)
    do_work(data);
    return true;
}
```

### Explicit Intent

```aria
// Make boolean intent clear
bool:found = search(haystack, needle);
bool:success = save_to_disk(data);
bool:ready = initialize_subsystem();

// Not just:
bool:result = some_function();  // What does result mean?
```

---

## Comparison with Other Languages

### C/C++

**C**: No bool type (use int, 0=false, non-zero=true)
**C++**: Has bool, true/false keywords

**Aria Difference**:
- No implicit conversions (int → bool requires explicit check)
- NULL checks must be explicit
- Short-circuit evaluation guaranteed

### Python

**Python**: bool is subclass of int (True==1, False==0)

**Aria Difference**:
- bool and int are separate types (no inheritance)
- Cannot do arithmetic with bool directly
- Clearer logical/numeric distinction

### JavaScript

**JavaScript**: Truthy/falsy (0, "", null, undefined are falsy)

**Aria Difference**:
- ONLY true/false are boolean values
- No implicit conversions from other types
- Must explicitly check for NULL, zero, empty string

---

## Summary

**bool is for**:
- ✅ Logical conditions (if, loops)
- ✅ Flags and binary state
- ✅ Predicate functions (is_*, has_*, can_*)
- ✅ Boolean algebra
- ✅ Control flow decisions

**bool is NOT for**:
- ❌ Numeric operations (use int)
- ❌ Multi-state values (use enum, int, or trit)
- ❌ Bit manipulation (use uint)
- ❌ Nullable logic (use NIL or unknown)

**Key Principles**:
1. **Logical semantics** (not numeric)
2. **Two values**: true and false only
3. **Explicit conversions** (no implicit truthiness)
4. **Short-circuit evaluation** (guaranteed)
5. **Clear naming** (is_*, has_*, can_*)
6. **vs int1**: Same size, different purpose

**Best Practices**:
- Use bool for logical decisions
- Name clearly (question form)
- Avoid magic boolean parameters
- Use guard clauses for early returns
- Keep expressions simple (intermediate variables)
- Never compare bool to true/false (redundant)

**Remember**: bool represents **truth values**, not numbers. Choose bool when asking "is this true?" and int when asking "what is this value?".
