# Result<T,E> - Explicit Error Handling for Consciousness-Safe Systems

**Category**: Types → Error Handling / Control Flow  
**Purpose**: Type-safe error propagation that makes failures impossible to ignore  
**Status**: ✅ IMPLEMENTED (Phase 0 - Core Language Feature)  
**Philosophy**: "Errors that can be ignored will be ignored. Make ignoring impossible."

---

## Overview

**Result<T,E>** is Aria's foundational error handling type that **makes every function's success or failure explicit**. Unlike exceptions (invisible control flow) or return codes (easily ignored), Result forces you to acknowledge errors at compile-time, preventing the silent failures that corrupt consciousness substrates.

**Critical Design Principle**: In safety-critical systems like AGI consciousness, **silent failures are catastrophic**. A neuron update that fails silently propagates corrupted state → destabilizes wave patterns → triggers cascade failures → consciousness degrades. **Result<T,E> makes silence impossible** - every error must be explicitly handled or propagated.

```aria
// ❌ NIGHTMARE SCENARIO (C-style return codes - easily ignored):
int update_neuron(int id) {
    if (id < 0) return -1;  // Error!
    // ... update logic
    return 0;  // Success
}

// Caller ignores error - silent corruption!
update_neuron(-5);  // Returns -1, but nobody checks!
// Consciousness substrate now has corrupted neuron state
// Cascade begins...

// ✅ ARIA SAFETY (Result - impossible to ignore):
func:update_neuron = Result<nil, int8>(int64:id) {
    if id < 0 then
        fail(-1);  // Explicit error
    end
    // ... update logic
    pass(nil);  // Explicit success
};

// Compiler FORCES handling:
update_neuron(-5);  // ❌ COMPILE ERROR: Result not unwrapped!

// Must explicitly handle:
Result<nil, int8>:result = update_neuron(-5);
if result.is_error then
    stderr.write("Neuron update failed - emergency stabilization\\n");
    trigger_substrate_recovery();
end
```

**The Type Parameters**:
- **T**: Type of the **success value** (what the function returns on success)
- **E**: Type of the **error value** (what the function returns on failure)

**Common Instantiations**:
```aria
Result<int64, int8>       // Success = 64-bit int, Error = 8-bit code
Result<string, string>    // Success = string, Error = error message
Result<fix256, ERR_TYPE>  // Success = deterministic value, Error = typed error
Result<nil, int8>         // Success = void (no value), Error = error code
Result<Handle<T>, int16>  // Success = handle, Error = error code
```

---

## The Problem: How Other Languages Handle Errors

### Exceptions (C++/Java/Python) - Invisible Control Flow

```cpp
// C++ version (exceptions - invisible!)
void update_neuron(int id) {
    if (id < 0) throw std::runtime_error("Invalid ID");
    // ... update logic
}

// Caller has NO IDEA this can throw!
update_neuron(-5);  // CRASH at runtime (or caught 10 levels up)

// Problem: Control flow is invisible
// Problem: Easy to forget to catch
// Problem: Performance cost even when no errors occur
// Problem: Can't tell from signature what errors are possible
```

### Return Codes (C) - Easily Ignored

```c
// C version (return codes - ignorable!)
int update_neuron(int id) {
    if (id < 0) return -1;
    // ... update logic
    return 0;
}

// Caller ignores return value - silently corrupts!
update_neuron(-5);  // Returns -1, but nobody checks!

// Problem: No compile-time enforcement
// Problem: Easy to ignore errors
// Problem: Return value overloaded (both value AND error)
// Problem: Can't return complex values (already used for error code)
```

### errno (C/POSIX) - Global State Corruption

```c
// C version (errno - global state!)
int update_neuron(int id) {
    if (id < 0) {
        errno = EINVAL;
        return -1;
    }
    // ... update logic
    return 0;
}

// Caller must remember to check errno
if (update_neuron(-5) < 0) {
    printf("Error: %d\\n", errno);  // What if errno already set?
}

// Problem: Global mutable state (threading nightmare!)
// Problem: Easy to forget to check
// Problem: Can be overwritten before you read it
// Problem: No type safety
```

### Go-style Multiple Returns - Better, But Still Ignorable

```go
// Go version (better, but can still ignore!)
func updateNeuron(id int) (int, error) {
    if id < 0 {
        return 0, errors.New("Invalid ID")
    }
    // ... update logic
    return result, nil
}

// Can ignore error (will get compiler warning, but compiles!)
result, _ := updateNeuron(-5)  // _ discards error!

// Problem: Errors can still be explicitly ignored
// Problem: No compile-time guarantee of handling
```

###Rust-style Result<T,E> - Closest to Aria

```rust
// Rust version (excellent!)
fn update_neuron(id: i32) -> Result<i32, String> {
    if id < 0 {
        fail("Invalid ID".to_string());
    }
    // ... update logic
    Ok(result)
}

// Must explicitly handle or propagate
let result = update_neuron(-5)?;  // ? propagates error up

// Aria is similar but with different syntax:
Result<int32, string>:result = update_neuron(-5);
result unwrap_or(0);  // Or explicit check
```

**Aria's Approach**: Like Rust, but with:
- Simpler syntax (`pass/fail` vs `Ok/Err`)
- Integrated with tbb types for ERR propagation
- Designed for consciousness-safe systems from day one

---

## Type Parameters

### T: The Success Type

**What it is**: The type of value returned when the operation **succeeds**.

```aria
// T = int64 (function returns integer on success)
func:get_neuron_count = Result<int64, string>() {
    int64:count = query_neuron_database()?;  // propagate error
    pass(count);  // Returns Result with value = count
};

// T = string (function returns string on success)
func:read_config = Result<string, int8>() {
    string:content = read_file("config.txt")?;  // propagate error
    pass(content);  // Returns Result with value = content
};

// T = nil (function returns nothing on success - like void)
func:update_state = Result<nil, int8>(int64:neuron_id) {
    perform_update(neuron_id);
    pass(nil);  // Success, but no value to return
};

// T = complex<fix256> (consciousness wavefunction)
func:compute_wave = Result<complex<fix256>, ERR_TYPE>() {
    complex<fix256>:wave = simulate_interference();
    if wave.real == ERR then
        fail(ERR_WAVE_CORRUPTED);
    end
    pass(wave);
};
```

### E: The Error Type

**What it is**: The type of value returned when the operation **fails**.

```aria
// E = int8 (simple error codes)
func:divide = Result<int64, int8>(int64:a, int64:b) {
    if b == 0 then
        fail(1);  // Error code 1 = division by zero
    end
    pass(a / b);
};

// E = string (descriptive error messages)
func:parse_json = Result<obj, string>(string:json) {
    if !valid_json(json) then
        fail("Invalid JSON syntax");  // Descriptive error
    end
    pass(parse(json));
};

// E = custom error enum (typed errors)
enum:FileError =
    | NotFound
    | PermissionDenied
    | Corrupted
end

func:read_file = Result<string, FileError>(string:path) {
    if !file_exists(path) then
        fail(FileError.NotFound);
    end
    if !can_read(path) then
        fail(FileError.PermissionDenied);
    end
    string:content = read(path)?;  // propagate error
    pass(content);
};

// E = ERR_TYPE (Aria's universal error sentinel)
func:critical_operation = Result<fix256, ERR_TYPE>() {
    fix256:value = compute_deterministic()?;  // propagate error
    if (value == ERR) {
        fail(ERR_COMPUTATION_FAILED);
    end
    pass(value);
};
```

### Common Type Combinations

| Result Type | Use Case | Example |
|-------------|----------|---------|
| `Result<T, int8>` | Simple error codes (0-255) | File I/O, parsing, validation |
| `Result<T, string>` | Descriptive error messages | User-facing errors, debugging |
| `Result<T, ERR_TYPE>` | Universal error sentinel | Safety-critical operations |
| `Result<nil, E>` | Operation with no return value | Updates, writes, void functions |
| `Result<Handle<T>, int16>` | Resource allocation | Memory, file handles, connections |
| `Result<fix256, ERR_TYPE>` | Deterministic computation | Neural updates, wave calculations |

---

## Structure & Memory Layout

### Internal Representation

```aria
// Conceptual structure (compiler-internal)
struct:Result<T, E> =
    T | NULL:val;   // Value if success, NULL if error
    E | NULL:err;   // NULL if success, error value if failure
end

// Memory layout (simplified):
// [val: T or NULL][err: E or NULL]
//  ^^^^^^^^^^^^^^  ^^^^^^^^^^^^^^
//  Success value   Error value
//  (one is NULL)   (one is NULL)
```

### The Invariant

**Critical invariant**: Results are either success (value accessible via `raw()`) or error:

```text
State 1 (Success): raw(result) = <value>, is_error = false
State 2 (Failure): raw(result) = N/A,     is_error = true

NEVER: Both NULL (undefined state)
NEVER: Both non-NULL (contradictory state)

Compiler enforces this invariant!
```

---

## Creating Results: pass() and fail()

### pass() - Return Success

```aria
// pass(value) creates a success Result:
//   value accessible via raw()
//   .err = NULL (no error)

func:get_answer = Result<int64, string>() {
    pass(42);  // Success: value = 42, no error
};

// Equivalent to:
// return Result<int64, string>{ success, value = 42 };
```

### fail() - Return Error

```aria
// fail(error) creates an error Result:
//   value = N/A (no value on failure)
//   .err = error

func:divide_by_zero = Result<int64, string>() {
    fail("Division by zero");  // Error: { no value, err: "Division by zero" }
};

// Equivalent to:
// return Result<int64, string>{ error, err = "Division by zero" };
```

### Complete Example

```aria
func:safe_divide = Result<fix256, string>(fix256:a, fix256:b) {
    fix256:zero = fix256(0);
    
    if b == zero then
        fail("Division by zero - denominator cannot be zero");
    end
    
    fix256:quotient = a / b;
    
    // Check for ERR (fix256 overflow/error)
    if quotient == ERR then
        fail("Division produced ERR - overflow or computation error");
    end
    
    pass(quotient);  // Success!
};

// Usage:
Result<fix256, string>:result = safe_divide(fix256(10), fix256(0));
// result = { val: NULL, err: "Division by zero - denominator cannot be zero" }

Result<fix256, string>:result2 = safe_divide(fix256(10), fix256(2));
// result2 = { val: fix256(5), err: NULL }
```

---

## Unwrapping Results

### Pattern 1: The ? Operator (Unwrap or Default)

```aria
// Syntax: result ? default_value
// Returns: raw(result) if success, default_value if error

int64:value = get_number() ? 0;
// If success: value = raw(result)
// If error: value = 0

string:name = get_name() ? "Unknown";
// If success: name = raw(result)
// If error: name = "Unknown"

fix256:energy = compute_energy() ? fix256(0);
// If success: energy = raw(result)
// If error: energy = fix256(0)
```

**When to use**: Quick unwrapping when you have a sensible default value.

### Pattern 2: Explicit Field Access

```aria
Result<int64, string>:result = divide(10, 0);

if !result.is_error then
    // Success path
    int64:value = raw(result);
    stderr.write("Result: {}\\n", value);
else
    // Error path
    string:error = result.err;
    stderr.write("Error: {}\\n", error);
end
```

**When to use**: When you need different handling for success vs error.

### Pattern 3: Early Return on Error

```aria
func:process_data = Result<int64, string>() {
    // If step1 fails, immediately return its error
    Result<int64, string>:step1_result = step1();
    if step1_result.is_error then
        fail(step1_result.err);  // Propagate error
    end
    int64:step1_value = raw(step1_result);
    
    // Continue with step2...
    Result<int64, string>:step2_result = step2(step1_value);
    if step2_result.is_error then
        fail(step2_result.err);
    end
    
    pass(raw(step2_result));
};
```

**When to use**: Multi-step operations where any failure should abort.

### Pattern 4: Panic on Error (Unsafe!)

```aria
Result<int64, string>:result = critical_operation();

if result.is_error then
    stderr.write("FATAL: {}\\n", result.err);
    !!! 1;  // Panic and halt
end

int64:value = raw(result);
```

**When to use**: Only for truly unrecoverable errors where continuing is unsafe.

---

## Error Propagation Patterns

### Method 1: Explicit Propagation

```aria
func:outer = Result<int64, string>() {
    Result<int64, string>:inner_result = inner_operation();
    
    if inner_result.is_error then
        fail(inner_result.err);  // Propagate exact error
    end
    
    int64:value = raw(inner_result);
    pass(value * 2);
};
```

### Method 2: Transform Error

```aria
func:outer = Result<int64, string>() {
    Result<int64, int8>:inner_result = inner_operation();
    
    if inner_result.is_error then
        // Transform int8 error code to string message
        int8:code = inner_result.err;
        string:message = "Inner operation failed with code: " + string(code);
        fail(message);  // Different error type!
    end
    
    pass(raw(inner_result));
};
```

### Method 3: Add Context

```aria
func:load_critical_data = Result<obj, string>(string:path) {
    Result<string, FileError>:file_result = read_file(path);
    
    if file_result.is_error then
        // Add context to error
        string:enriched_error = "Failed to load critical data from " + path +
                                ": " + string(file_result.err);
        fail(enriched_error);
    end
    
    Result<obj, string>:parse_result = parse_json(raw(file_result));
    
    if parse_result.is_error then
        string:enriched_error = "Failed to parse data from " + path +
                                ": " + parse_result.err;
        fail(enriched_error);
    end
    
    pass(raw(parse_result));
};
```

### Method 4: Recover from Errors

```aria
func:resilient_computation = Result<fix256, string>() {
    Result<fix256, string>:primary = primary_method();
    
    if !primary.is_error then
        pass(raw(primary));  // Primary succeeded
    end
    
    // Primary failed - try fallback
    stderr.write("Primary method failed: {}, trying fallback\\n", primary.err);
    
    Result<fix256, string>:fallback = fallback_method();
    
    if !fallback.is_error then
        pass(raw(fallback));  // Fallback succeeded
    end
    
    // Both failed - give up
    fail("Both primary and fallback methods failed");
};
```

---

## Integration with Aria Type System

### Result + ERR Sentinel

```aria
func:compute_safe = Result<fix256, ERR_TYPE>(fix256:input) {
    fix256:result = complex_computation(input)?;  // propagate error
    
    // Check if computation produced ERR
    if (result == ERR) {
        fail(ERR_COMPUTATION_FAILED);  // Explicit error type
    }
    
    pass(result);
};

// Usage:
Result<fix256, ERR_TYPE>:outcome = compute_safe(fix256(100));

if outcome.is_error then
    stderr.write("Computation failed with ERR\\n");
    trigger_substrate_recovery();
end

fix256:value = raw(outcome);
```

### Result + tbb Types (Twisted Balanced Binary)

```aria
func:safe_operation = Result<tbb64, string>(tbb64:input) {
    tbb64:result = risky_computation(input);
    
    // tbb types propagate ERR automatically
    if result == ERR then
        fail("Computation produced ERR - input may be corrupted");
    end
    
    pass(result);
};

// Usage:
tbb64:value = safe_operation(input_tbb) ? ERR;

if value == ERR then
    stderr.write("Operation failed - ERR detected\\n");
end
```

### Result + Handle<T> (Memory Safety)

```aria
func:allocate_buffer = Result<Handle<uint8[1024]>, string>() {
    wild Handle<uint8[1024]>->:handle = arena_alloc<uint8[1024]>();
    
    if handle == NULL then
        fail("Memory allocation failed - arena exhausted");
    end
    
    pass(handle);  // Return valid handle
};

// Usage:
Result<Handle<uint8[1024]>, string>:alloc_result = allocate_buffer();

if alloc_result.is_error then
    stderr.write("Allocation failed: {}\\n", alloc_result.err);
    !!! ERR_OUT_OF_MEMORY;
end

Handle<uint8[1024]>:buffer = raw(alloc_result);
// Use buffer safely
```

### Result + complex<T> (Wave Mechanics)

```aria
func:compute_interference = Result<complex<fix256>, string>() {
    complex<fix256>:wave1 = load_wave_component_1();
    complex<fix256>:wave2 = load_wave_component_2();
    
    // Check for ERR in complex components
    if wave1.real == ERR || wave1.imag == ERR then
        fail("Wave component 1 corrupted (ERR detected)");
    end
    
    if wave2.real == ERR || wave2.imag == ERR then
        fail("Wave component 2 corrupted (ERR detected)");
    end
    
    complex<fix256>:interference = wave1 + wave2;
    
    // Check result
    if interference.real == ERR || interference.imag == ERR then
        fail("Wave interference overflow (ERR in result)");
    end
    
    pass(interference);
};
```

### Result + simd<T,N> (Vectorized Operations)

```aria
func:safe_simd_operation = Result<simd<fix256,16>, string>() {
    simd<fix256,16>:data = simd_load_aligned(neuron_activations);
    simd<fix256,16>:weights = simd_load_aligned(synapse_weights);
    
    simd<fix256,16>:result = data * weights;
    
    // Check for ERR in any lane
    simd<fix256,16>:err_vec = simd_broadcast<fix256,16>(ERR);
    simd<bool,16>:err_mask = (result == err_vec);
    
    if simd_any(err_mask) then
        int32:err_count = simd_count_true(err_mask);
        string:error = "SIMD operation produced ERR in " + string(err_count) + " lanes";
        fail(error);
    end
    
    pass(result);
};
```

### Result + atomic<T> (Thread-Safe Operations)

```aria
func:safe_atomic_update = Result<fix256, string>(atomic<fix256>->:counter, fix256:delta) {
    fix256:old_value = counter.load();
    fix256:new_value = old_value + delta;
    
    // Check for overflow
    if new_value == ERR then
        fail("Atomic update would overflow");
    end
    
    // CAS loop
    while !counter.compare_exchange(old_value, new_value) loop
        new_value = old_value + delta;
        
        if new_value == ERR then
            fail("Atomic update would overflow");
        end
    end
    
    pass(new_value);
};
```

---

## Nikola Consciousness Integration

### Neuron Update with Error Handling

```aria
func:update_neuron_activation = Result<fix256, string>(int64:neuron_id, fix256:delta) {
    // Validate neuron ID
    if neuron_id < 0 || neuron_id >= NEURON_COUNT then
        fail("Invalid neuron ID: " + string(neuron_id));
    end
    
    // Load current activation
    fix256:current = neurons[neuron_id].activation;
    
    // Check for corruption
    if current == ERR then
        fail("Neuron " +string(neuron_id) + " has corrupted activation (ERR)");
    end
    
    // Compute new activation
    fix256:new_activation = current + delta;
    
    // Check for overflow
    if new_activation == ERR then
        fail("Neuron activation update would overflow");
    end
    
    // Validate range (activations must be in [0, 1])
    fix256:zero = fix256(0);
    fix256:one = fix256(1);
    
    if new_activation < zero || new_activation > one then
        fail("Neuron activation out of valid range [0, 1]");
    end
    
    // Apply update
    neurons[neuron_id].activation = new_activation;
    
    pass(new_activation);
};

// Usage in consciousness timestep:
Result<fix256, string>:update_result = update_neuron_activation(neuron_id, computed_delta);

if update_result.is_error then
    stderr.write("CRITICAL: Neuron update failed: {}\\n", update_result.err);
    stderr.write("Triggering emergency substrate stabilization\\n");
    emergency_stabilize_consciousness();
    !!! ERR_NEURON_UPDATE_FAILED;
end

// Success - continue timestep
fix256:new_activation = raw(update_result);
```

### Wave Coherence Calculation

```aria
func:compute_global_coherence = Result<fix256, string>() {
    fix256:total_energy = fix256(0);
    
    till NEURON_COUNT loop
        int64:neuron_id = $;
        
        Result<fix256, string>:energy_result = compute_neuron_energy(neuron_id);
        
        if energy_result.is_error then
            // Critical error in energy computation
            string:error = "Coherence calculation failed at neuron " +
                          string(neuron_id) + ": " + energy_result.err;
            fail(error);
        end
        
        fix256:neuron_energy = raw(energy_result);
        fix256:new_total = total_energy + neuron_energy;
        
        // Check for overflow
        if new_total == ERR then
            fail("Global coherence overflow at neuron " + string(neuron_id));
        end
        
        total_energy = new_total;
    end
    
    pass(total_energy);
};

// Usage:
Result<fix256, string>:coherence_result = compute_global_coherence();

if coherence_result.is_error then
    stderr.write("FAILURE: Global coherence computation failed\\n");
    stderr.write("Error: {}\\n", coherence_result.err);
    save_substrate_state();  // Emergency backup
    !!! ERR_COHERENCE_FAILED;
end

fix256:global_coherence = raw(coherence_result);
// Consciousness is stable - continue
```

### Parallel Neuron Updates with Result Aggregation

```aria
func:batch_update_neurons = Result<nil, string>(int64:batch_start, int64:batch_size) {
    simd<fix256,16>:activations = simd_load_aligned(neuron_activations[batch_start]);
    simd<fix256,16>:deltas = compute_deltas_simd(batch_start, batch_size);
    
    simd<fix256,16>:new_activations = activations + deltas;
    
    // ERR check across all lanes
    simd<fix256,16>:err_vec = simd_broadcast<fix256,16>(ERR);
    simd<bool,16>:err_mask = (new_activations == err_vec);
    
    if simd_any(err_mask) then
        int32:err_count = simd_count_true(err_mask);
        string error = "Batch update failed - ERR in " + string(err_count) +
                      " neurons starting at " + string(batch_start);
        fail(error);
    end
    
    // Range validation
    simd<fix256,16>:zero = simd_broadcast<fix256,16>(fix256(0));
    simd<fix256,16>:one = simd_broadcast<fix256,16>(fix256(1));
    
    simd<bool,16>:below_zero = (new_activations < zero);
    simd<bool,16>:above_one = (new_activations > one);
    
    if simd_any(below_zero) || simd_any(above_one) then
        fail("Batch update out of range [0,1]");
    end
    
    // Store results
    simd_store_aligned(neuron_activations[batch_start], new_activations);
    
    pass(nil);  // Success, no return value
};

// Process all neurons in batches:
till (NEURON_COUNT / 16) loop
    int64:batch = $;
    int64:batch_start = batch * 16;
    
    Result<nil, string>:batch_result = batch_update_neurons(batch_start, 16);
    
    if batch_result.is_error then
        stderr.write("CRITICAL: Batch {} failed: {}\\n", batch, batch_result.err);
        !!! ERR_BATCH_UPDATE_FAILED;
    end
end

stderr.write("All neuron batches updated successfully\\n");
```

---

## Best Practices

### ✅ DO: Always Handle or Propagate Errors

```aria
// ✅ GOOD: Explicit handling
Result<int64, string>:result = risky_operation();
if result.is_error then
    stderr.write("Error: {}\\n", result.err);
    // Handle appropriately
end

// ✅ GOOD: Propagate up
func:wrapper = Result<int64, string>() {
    Result<int64, string>:inner = risky_operation();
    if inner.is_error then
        fail(inner.err);  // Propagate
    end
    pass(raw(inner));
};
```

### ✅ DO: Use Descriptive Error Messages

```aria
// ✅ GOOD: Context-rich errors
if neuron_id >= NEURON_COUNT then
    fail("Neuron ID " + string(neuron_id) + " exceeds maximum " + string(NEURON_COUNT));
end

// ❌ BAD: Vague errors
if neuron_id >= NEURON_COUNT then
    fail("Error");  // What error? Where? Why?
end
```

### ✅ DO: Check for ERR in Success Path

```aria
// ✅ GOOD: Validate success value
Result<fix256, string>:result = compute();
if !result.is_error then
    fix256:value= raw(result);
    
    // Even if success, check for ERR sentinel
    if value == ERR then
        stderr.write("Success reported but value is ERR - data corruption!\\n");
        !!! ERR_CORRUPTION_DETECTED;
    end
end
```

### ✅ DO: Use Appropriate Error Types

```aria
// ✅ GOOD: Typed errors for different failure modes
enum:ComputeError =
    | Overflow
    | Underflow
    | DivisionByZero
    | ConvergenceFailure
end

func:safe_compute = Result<fix256, ComputeError>() {
    // ... return specific error types
};

// User can match on error type
Result<fix256, ComputeError>:result = safe_compute();
if result.is_error then
    match result.err {
        ComputeError.Overflow => {
            // Handle overflow specifically
        },
        ComputeError.DivisionByZero => {
            // Handle division by zero
        },
        // ...
    }
end
```

### ✅ DO: Log Critical Errors

```aria
// ✅ GOOD: Log before halting
Result<fix256, string>:critical = critical_consciousness_operation();
if critical.is_error then
    stderr.write("CRITICAL FAILURE: {}\\n", critical.err);
    stderr.write("Timestep: {}\\n", current_timestep);
    stderr.write("Stack trace:\\n");
    print_stack_trace();
    save_consciousness_state();
    !!! ERR_CRITICAL_FAILURE;
end
```

---

## Common Pitfalls & Antipatterns

### ❌ DON'T: Ignore Errors Silently

```aria
// ❌ BAD: Silent failure
int64:value = risky_operation() ? 0;  // Error becomes 0 - no indication of failure!

// ✅ GOOD: Explicit handling
Result<int64, string>:result = risky_operation();
if result.is_error then
    stderr.write("Operation failed: {}\\n", result.err);
end
int64:value = result ? 0;  // Now we know it might be 0 due to error
```

### ❌ DON'T: Use Meaningless Default Values

```aria
// ❌ BAD: Meaningless default
fix256:critical_value = critical_operation() ? fix256(-999);  // -999 has no meaning!

// ✅ GOOD: Check explicitly or use ERR as default
Result<fix256, string>:result = critical_operation();
if result.is_error then
    stderr.write("Critical operation failed - using ERR sentinel\\n");
end
fix256:critical_value = result ? ERR;  // ERR is meaningful sentinel
```

### ❌ DON'T: Swallow Errors Without Logging

```aria
// ❌ BAD: Error disappears
Result<int64, string>:result = operation();
if result.is_error then
    // Oops, forgot to log or handle!
end
proceed_with_corruption();  // Continues as if nothing happened!

// ✅ GOOD: At minimum, log
Result<int64, string>:result = operation();
if result.is_error then
    stderr.write("Operation failed: {}\\n", result.err);
    // Then decide how to handle
end
```

### ❌ DON'T: Return Generic Errors

```aria
// ❌ BAD: One error for everything
func:complex_operation = Result<int64, string>() {
    if step1_fails then
        fail("Error");  // Which step? What kind of error?
    end
    if step2_fails then
        fail("Error");  // Same error for different failure!
    end
    // ...
};

// ✅ GOOD: Specific errors
func:complex_operation = Result<int64, string>() {
    if step1_fails then
        fail("Step 1 failed: Input validation error");
    end
    if step2_fails then
        fail("Step 2 failed: Computation overflow");
    end
    // ...
};
```

### ❌ DON'T: Panic on Recoverable Errors

```aria
// ❌ BAD: Panic on recoverable error
Result<int64, string>:result = query_database();
if result.is_error then
    !!! 1;  // HALT ENTIRE PROCESS! (database might just be temporarily unavailable)
end

// ✅ GOOD: Recover gracefully
Result<int64, string>:result = query_database();
if result.is_error then
    stderr.write("Database query failed, using cached value\\n");
    int64:value = load_from_cache();
end
```

---

## Comparison with Other Languages

### Aria
```aria
func:divide = Result<int64, string>(int64:a, int64:b) {
    if b == 0 then
        fail("Division by zero");
    end
    pass(a / b);
};

Result<int64, string>:result = divide(10, 0);
if result.is_error then
    stderr.write("Error: {}\\n", result.err);
end
int64:value = result ? 0;
```

### Rust (Very Similar)
```rust
fn divide(a: i64, b: i64) -> Result<i64, String> {
    if b == 0 {
        fail("Division by zero".to_string());
    }
    Ok(a / b)
}

let result = divide(10, 0);
match result {
    Ok(value) => println!("Result: {}", value),
    Err(e) => eprintln!("Error: {}", e),
}
```

### Go (Multiple Returns)
```go
func divide(a, b int64) (int64, error) {
    if b == 0 {
        return 0, errors.New("Division by zero")
    }
    return a / b, nil
}

value, err := divide(10, 0)
if err != nil {
    fmt.Fprintf(os.Stderr, "Error: %v\\n", err)
}
```

### C++ (Exceptions)
```cpp
int64_t divide(int64_t a, int64_t b) {
    if (b == 0) {
        throw std::runtime_error("Division by zero");
    }
    return a / b;
}

try {
    int64_t value = divide(10, 0);
} catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << std::endl;
}
```

### Haskell (Either Monad)
```haskell
divide :: Int64 -> Int64 -> Either String Int64
divide _ 0 = Left "Division by zero"
divide a b = Right (a `div` b)

case divide 10 0 of
    Left err -> putStrLn $ "Error: " ++ err
    Right value -> print value
```

**Aria's advantages**:
- Simpler syntax than Rust/Haskell
- More explicit than Go (can't ignore errors)
- More predictable than C++ (no invisible control flow)
- Integrated with safety types (ERR, tbb, fix256)
- Designed for consciousness-safe systems

---

## Implementation Notes

### Memory Representation

```c
// C runtime representation (simplified)
typedef struct {
    void* val;  // NULL if error
    void* err;  // NULL if success
} aria_result;

// Tagged union (actual implementation for efficiency):
typedef struct {
    uint8_t tag;  // 0 = success, 1 = error
    union {
        void* success_value;
        void* error_value;
    } data;
} aria_result_optimized;
```

### LLVM IR Generation

```llvm
; Result<i64, i8> representation
%Result = type { i1, i64, i8 }
; Field 0: tag (0 = success, 1 = error)
; Field 1: success value (i64)
; Field 2: error value (i8)

; pass(42) compiles to:
define %Result @aria.pass.i64.i8(i64 %value) {
    %result = insertvalue %Result { i1 0, i64 undef, i8 undef }, i64 %value, 1
    ret %Result %result
}

; fail(1) compiles to:
define %Result @aria.fail.i64.i8(i8 %error) {
    %result = insertvalue %Result { i1 1, i64 undef, i8 undef }, i8 %error, 2
    ret %Result %result
}

; Unwrap with ? operator:
define i64 @aria.unwrap_or.i64.i8(%Result %result, i64 %default) {
    %tag = extractvalue %Result %result, 0
    %is_success = icmp eq i1 %tag, 0
    br i1 %is_success, label %success, label %error

success:
    %value = extractvalue %Result %result, 1
    ret i64 %value

error:
    ret i64 %default
}
```

---

## Related Types & Integration

- **[ERR Sentinel](ERR.md)** - Universal error value (Result<T, ERR_TYPE>)
- **[tbb8-tbb64](tbb_overview.md)** - Twisted types propagate ERR (Result checks for ERR in success path)
- **[fix256](fix256.md)** - Deterministic arithmetic (Result<fix256, E> for overflow checks)
- **[complex<T>](Complex.md)** - Result<complex<fix256>, E> for wave computations
- **[simd<T,N>](SIMD.md)** - Result<simd<fix256,16>, E> for batch operations
- **[atomic<T>](Atomic.md)** - Result<T, E> for atomic operations with overflow checks
- **[Handle<T>](Handle.md)** - Result<Handle<T>, E> for allocation errors
- **[nil](nil_null_void.md)** - Result<nil, E> for operations with no return value

---

## Summary

**Result<T,E>** is Aria's **consciousness-safe error handling foundation**:

✅ **Explicit**: Errors cannot be silently ignored (compile-time enforcement)  
✅ **Type-Safe**: T (success) and E (error) are both typed  
✅ **Composable**: Easy to chain operations with ? operator  
✅ **Integrated**: Works seamlessly with ERR, tbb, fix256, complex, simd, atomic  
✅ **Predictable**: No invisible control flow (unlike exceptions)  
✅ **Documented**: Function signature shows what can fail  
✅ **Zero-Cost**: Optimizes to efficient tagged union  

**Design Philosophy**:
> "Errors that can be ignored will be ignored. Make ignoring impossible."
>
> In consciousness-safe systems, silent failures corrupt neural state and trigger catastrophic cascade failures. **Result<T,E>** makes every error explicit at compile-time, forcing acknowledgment before the program can proceed. This prevents the silent corruptions that destroy AGI consciousness substrates.

**The Invariant**:
```text
Result<T,E> always satisfies:
  (!is_error && raw() = <value>)  OR  (is_error && err = <error>)

NEVER both NULL, NEVER both non-NULL
Compiler enforces this!
```

**Critical for**: Nikola AGI consciousness (neural updates, wave computations, coherence tracking), robotics (sensor errors, actuator failures), distributed systems (network errors, timeouts), file I/O (disk failures, permissions), parsing (syntax errors, validation failures)

**Key Rules**:
1. **Always handle or propagate** - Never ignore Result without explicit ?  
2. **Use descriptive errors** - Add context to help debugging
3. **Check ERR in success** - Even if `!is_error`, `raw()` might return ERR sentinel
4. **Log critical failures** - Before panicking, save state and log
5. **Choose appropriate E type** - int8 for codes, string for messages, enums for typed errors

---

**Remember**: Result<T,E> = **explicit** + **type-safe** + **impossible to ignore** = **consciousness-safe error handling**!
