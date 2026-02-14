# Q3 and Q9 - Quantum/Speculative Types

Superposition types that track dual hypotheses with confidence for evidence-based decision making.

## Overview

`Q3<T>` and `Q9<T>` are quantum/speculative types that maintain TWO hypotheses simultaneously with a confidence gradient. Unlike traditional types that force immediate decisions, quantum types let you defer commitment while accumulating evidence.

**Core concept:**
- Track hypothesis A and hypothesis B simultaneously
- Maintain confidence level that evolves with evidence
- Crystallize to single value when confidence threshold met
- Continue computation on both paths until decision required

**Use quantum types when:**
- Decision requires evidence accumulation over time
- Want to explore two paths before committing
- Fuzzing/testing needs dual-path execution tracking
- Speculative execution or branch prediction
- AI reasoning with confidence gradients (Nikola)
- Uncertainty propagation in scientific computing

**Don't use quantum types when:**
- Decision is immediate (no evidence accumulation)
- Only one path is viable (no speculation needed)
- Binary choice sufficient (no confidence gradient)
- Classical control flow adequate

**Key characteristics:**
- **Superposition**: Two hypotheses exist simultaneously
- **Confidence**: Gradient tracking from strongly A → strongly B
- **Evidence accumulation**: Confidence evolves with new information
- **Crystallization**: Convert Q<T> → T when confident
- **Safety integration**: c=0 requires ok() (prevents accidental use)

## Q3 vs Q9

Two variants based on confidence precision needed:

### Q9<T> - Fine-Grained Confidence

```aria
struct Q9<T> {
    a: T,      // Hypothesis A
    b: T,      // Hypothesis B
    c: nit,    // Confidence: -4 to +4 (9 values)
}
```

**Confidence scale (9 levels)**:
- `-4`: Strongly confident in A
- `-3`: Very confident in A
- `-2`: Confident in A
- `-1`: Slightly lean toward A
- `0`: Both/Neither/Unknown (superposition)
- `+1`: Slightly lean toward B
- `+2`: Confident in B
- `+3`: Very confident in B
- `+4`: Strongly confident in B

**Use Q9 when:**
- Need precise confidence tracking (7 gradient levels)
- Evidence accumulates gradually
- Fine-grained decision thresholds
- Modeling human-like reasoning (how beliefs actually change)
- Nikola AI cognition (gradient thinking, not binary)

### Q3<T> - Binary Confidence

```aria
struct Q3<T> {
    a: T,      // Hypothesis A
    b: T,      // Hypothesis B
    c: trit,   // Confidence: -1, 0, +1 (3 values)
}
```

**Confidence scale (3 levels)**:
- `-1`: Lean toward A
- `0`: Equal/Unknown (superposition)
- `+1`: Lean toward B

**Use Q3 when:**
- Simple lean/unknown/lean sufficient
- Memory efficiency critical (trit smaller than nit)
- Binary confidence adequate
- Minimal overhead needed

**Recommendation**: Start with Q9 unless memory constraints demand Q3. The gradient provides much better modeling of real-world evidence accumulation.

## Confidence Semantics

### Zero's Triple Personality (c = 0)

When confidence is zero, the type is in true superposition with three possible interpretations:

**1. Both (True Superposition)**
```aria
// Schrödinger's cat - literally both states
quantum_state: Q9<State> = { a: ALIVE, b: DEAD, c: 0 };
// Cat is both alive AND dead until observed
```

**2. Neither (Complete Rejection)**
```aria
// Both hypotheses rejected
validation: Q9<Config> = { a: config_a, b: config_b, c: 0 };
// Neither configuration valid
```

**3. Unknown (Insufficient Data)**
```aria
// Equal confidence = total uncertainty
prediction: Q9<Result> = { a: result_a, b: result_b, c: 0 };
// Not enough evidence to prefer either
```

**Context determines which interpretation applies**. The compiler cannot distinguish - only the programmer knows the semantic meaning.

### The Confidence Gradient (Q9 Details)

**Negative values** (confidence in hypothesis A):
- `-1`: Slightly lean A (weak evidence)
- `-2`: Confident in A (solid evidence)
- `-3`: Very confident in A (strong evidence)
- `-4`: Strongly confident in A (overwhelming evidence)

**Positive values** (confidence in hypothesis B):
- `+1`: Slightly lean B (weak evidence)
- `+2`: Confident in B (solid evidence)
- `+3`: Very confident in B (strong evidence)
- `+4`: Strongly confident in B (overwhelming evidence)

**Magnitude = strength of belief**:
```aria
abs(c) < 2   // Low confidence (keep exploring)
abs(c) >= 2  // Moderate confidence (can consider crystallizing)
abs(c) >= 3  // High confidence (safe to crystallize)
abs(c) == 4  // Maximum confidence (overwhelmingly confident)
```

### Saturation Arithmetic (Critical!)

Confidence using **saturating arithmetic**, not wrapping:

```aria
value: Q9<i32> = { a: 100, b: 200, c: -3 };  // Very confident in A

value.c -= 5;  // Does NOT wrap to +2!
               // Saturates at -4 (maximum confidence in A)

value.c += 10; // Saturates at +3 (cannot exceed maximum)
```

**Why saturation?**
- Going from "very confident in A" (-3) should NOT suddenly mean "confident in B" (+2)
- Prevents nonsensical confidence flips
- Extreme states (-4, +4) must be explicit, not accidental

**Reaching extremes**:
```aria
// Must be explicit
q.c = -4;  // Explicitly set to maximum A confidence
q.c = +4;  // Explicitly set to maximum B confidence

// Or special constructors
rejected: Q9<Config> = Q9::neither(config_a, config_b);  // c = -4 or +4 depending on semantics
accepted: Q9<Solution> = Q9::both(solution_a, solution_b);
```

## Crystallization - The Core Concept

**The Problem**: Traditional code forces immediate decisions

**The Solution**: Q<T> defers decisions while accumulating evidence

**Crystallization Model**:
1. **Start in superposition**: Create Q<T> with two hypotheses, low/zero confidence
2. **Operate on both simultaneously**: Arithmetic/operations work on both paths
3. **Accumulate confidence**: Evidence shifts confidence toward one hypothesis
4. **Crystallize when ready**: Convert Q<T> → T when confidence threshold met

### The Philosophy

> "You don't know the answer, so you go about your business like you do know, while treating both values as true and operating on them as such, and only 'crystallize' from a Q<T> to a T when the confidence level is high enough for your particular usage."

**This is not syntax sugar** - it's a way of thinking about speculative computation.

### Crystallization Example

```aria
// Start uncertain (superposition)
temperature: Q9<i16> = { a: 20, b: 25, c: 0 };  // Could be either

// Operate on both paths (superposition maintained)
temperature.a = temperature.a * 2;  // A path: 40
temperature.b = temperature.b * 2;  // B path: 50

temperature.a = temperature.a + 5;  // A path: 45
temperature.b = temperature.b + 5;  // B path: 55

// Gather evidence, shift confidence
if sensor_reading > 48 {
    temperature.c += 2;  // Shift toward B (confidence = +2)
}

if thermometer_check > 52 {
    temperature.c += 1;  // More evidence for B (confidence = +3)
}

// Crystallize when confidence sufficient
if abs(temperature.c) >= 3 {
    final_temp: i16 = if temperature.c > 0 {
        temperature.b  // High confidence in B
    } else {
        temperature.a  // High confidence in A
    };
    
    // Superposition collapsed, committed to single value
}
```

**Evidence accumulation is automatic** - the confidence tracker prevents evidence from being lost.

## Q-Functions - Conditional Operations

Q-functions operate on quantum values with special semantics for hypothesis testing.

### Syntax

Inside q-functions, use shorthand:
- `q.a` = hypothesis A value
- `q.b` = hypothesis B value
- `q.c` = confidence level
- `q.q` = both A and B (apply operation to both)
- `q.qq` = all three (A, B, and C)

**Note**: `q` is a reserved keyword (cannot be variable name) inside q-functions.

### qor - Either Hypothesis Meets Condition

Apply operation if EITHER option meets condition:

```aria
qor(temperature, (q > 22)) {
    // Fires if temperature.a > 22 OR temperature.b > 22
    q.a += 1;   // Modify hypothesis A
    q.b += 2;   // Modify hypothesis B
    q.c += 1;   // Shift confidence toward B
    q.q += 5;   // Add 5 to BOTH a and b
    q.qq *= 2;  // Double ALL THREE (a, b, c)
}
```

**Use case**: At least one path meets criteria (explore both if either promising)

### qand - Both Hypotheses Meet Condition

Apply only if BOTH options meet condition:

```aria
qand(temperature, (q > 15)) {
    // Fires only if temperature.a > 15 AND temperature.b > 15
    q.q += 10;  // Add 10 to both
}
```

**Use case**: Both paths viable (reinforces both hypotheses)

### qxor - Exactly One Hypothesis Meets Condition

Apply only if ONE but NOT BOTH meet condition:

```aria
qxor(result, is_err) {
    // One path errored, one didn't - asymmetric behavior detected!
    log("Asymmetric behavior detected!");
    q.c += 3;  // Shift confidence toward whichever worked
}
```

**Use case**: Fuzzing edge cases (one path succeeds, other fails)

### qnor - Neither Hypothesis Meets Condition

Apply only if NEITHER option meets condition:

```aria
qnor(temperature, (q < 0)) {
    // Fires only if temperature.a >= 0 AND temperature.b >= 0
    q.q -= 5;  // Reduce both
}
```

**Use case**: Both paths fail same test (both hypotheses need adjustment)

### qconf - Condition Plus Confidence Threshold

Apply if condition matches AND confidence >= threshold:

```aria
qconf(temperature, (q > 20), 2) {
    // Fires if (a > 20 OR b > 20) AND abs(confidence) >= 2
    commit_to_hypothesis();
}
```

**Use case**: High-confidence crystallization (only act when sufficiently certain)

### qnconf - Condition Plus Low Confidence

Apply if condition matches AND confidence <= threshold:

```aria
qnconf(temperature, (q > 15), 1) {
    // Fires if condition met but confidence low (between -1 and +1)
    gather_more_data();
}
```

**Use case**: Detect uncertainty (need more evidence before deciding)

## Arithmetic Propagation

Operations on Q<T> return Q<T> (superposition maintained):

```aria
my_q: Q9<i8> = { a: 5, b: 7, c: 0 };

my_q.a += 4;  // { a: 9, b: 7, c: 0 }
my_q.b += 4;  // { a: 9, b: 11, c: 0 }

// Or modify both paths:
my_q.q += 4;  // { a: 13, b: 15, c: 0 } (both incremented)

result: Q9<i8> = { a: my_q.a * 2, b: my_q.b * 2, c: my_q.c };  // { a: 26, b: 30, c: 0 }
```

**Confidence unchanged by arithmetic** (unless explicitly modified):
- Operations on `a` or `b` don't change `c`
- Confidence only changes when explicitly assigned
- This separates computation from evidence accumulation

## Crystallization Operator (q.#)

**The crystallization operator** (`q.#`) collapses quantum superposition into a single value when confidence is sufficient. The `#` symbol visually represents a **lattice** - an ordered, crystalline structure - reflecting the transition from fluid superposition to solid decision.

**Semantic connection to memory pinning**: Just as `#` pins memory from fluid (movable by GC) to solid (fixed address), `q.#` pins a quantum state from fluid (superposition) to solid (single value).

### Syntax and Behavior

```aria
q: Q9<i32> = { a: 100, b: 200, c: -3 };  // Confident in A

value: i32 = q.#;  // Returns 100 (copy of hypothesis A)
```

**Return type**: `T | unknown`
- Returns `T` (copy of winner) when confidence sufficient
- Returns `unknown` when confidence insufficient or ambiguous
- **Copy semantics**: Q<T> preserved, allows continued exploration

### Crystallization Threshold

**Q9 (fine-grained confidence)**:
- **Minimum threshold: abs(c) >= 2** ("probably or better")
- Confidence scale: possibly (±1), **probably (±2)**, definitely (±3)
- Forces decision past "slightly lean" into "confident" territory

**Q3 (binary confidence)**:
- **Minimum threshold: abs(c) >= 1** (any non-zero)
- Only one level per side (lean A or lean B)
- Zero = unknown/superposition

### When Crystallization Returns unknown

**Three cases return unknown** (no clear winner):

1. **c = 0 (Superposition/Unknown)**
   - Neither hypothesis confirmed
   - Equal confidence = total uncertainty
   - Cannot pick winner from tie

2. **c = BOTH (Both valid)**
   - Both hypotheses equally valid
   - No single "correct" answer
   - Requires external criteria to decide

3. **c = NEITHER (Both invalid)**
   - Both hypotheses rejected
   - No valid option to return
   - Need new hypotheses

### Examples

**Q9 Crystallization (probably threshold)**:

```aria
temp: Q9<i16> = { a: 20, b: 25, c: 0 };  // Unknown initially

// Gather evidence
temp.c += 1;  // c = +1 (possibly B)
result: i16 | unknown = temp.#;  // Returns unknown (threshold not met!)

temp.c += 1;  // c = +2 (probably B)
result = temp.#;  // Returns 25 (copy of B, threshold met!)

// Q9 continues to exist (copy semantics)
temp.c += 1;  // c = +3 (definitely B)
result = temp.#;  // Returns 25 (higher confidence, same result)
```

**Q3 Crystallization (any non-zero)**:

```aria
choice: Q3<string> = { a: "slow", b: "fast", c: 0 };  // Unknown

result: string | unknown = choice.#;  // Returns unknown (c = 0)

choice.c = 1;  // Lean toward B
result = choice.#;  // Returns "fast" (threshold met: abs(c) >= 1)
```

**Confidence insufficient (returns unknown)**:

```aria
q: Q9<i32> = { a: 10, b: 20, c: 1 };  // Only "possibly" confident

value: i32 | unknown = q.#;  // Returns unknown (abs(1) < 2)

// Must either:
// 1. Gather more evidence
q.c += 1;  // c = 2 (probably)
value = q.#;  // Returns 20 (threshold met)

// 2. Explicitly acknowledge uncertainty
tentative: i32 = ok(q.b);  // ✅ "I know this is uncertain"
```

**Ambiguous cases (returns unknown)**:

```aria
// Case 1: Superposition (c = 0)
superposed: Q9<i32> = { a: 5, b: 7, c: 0 };
val: i32 | unknown = superposed.#;  // Returns unknown (tie)

// Case 2: Both valid
validated: Q9<Config> = { a: config1, b: config2, c: BOTH };
cfg: Config | unknown = validated.#;  // Returns unknown (both valid)

// Case 3: Neither valid
rejected: Q9<Parse> = { a: parse1, b: parse2, c: NEITHER };
ast: Parse | unknown = rejected.#;  // Returns unknown (both invalid)
```

### Why Copy Semantics?

**Crystallization copies the winner** (doesn't consume the Q<T>):

```aria
exploration: Q9<Algorithm> = { a: old, b: new, c: 2 };  // Probably new

current_best: Algorithm | unknown = exploration.#;  // Copy of new algorithm

// Q9 still exists - can continue exploring!
exploration.c += 1;  // More evidence
if abs(exploration.c) >= 3 {
    // High confidence now - can commit
    final: Algorithm = exploration.#;  // Another copy (definitely new)
}
```

**Benefits**:
- Use current best guess while gathering more evidence
- Non-destructive peek at crystallization
- Continue refining confidence after initial read
- Safe speculative execution (doesn't commit permanently)

### Integration with Safety System

**Crystallization respects unknown/ok() rules**:

```aria
uncertain: Q9<i32> = { a: 5, b: 7, c: 1 };  // "Possibly" confident

// q.# returns unknown (threshold not met)
value: i32 = uncertain.#;  // COMPILE ERROR! Type is i32 | unknown

// Must handle unknown case:
when (val = uncertain.#) is i32 then
    log("Crystallized: ${val}");
when val is unknown then
    log("Insufficient confidence - gathering more evidence");
end

// Or use ok() for explicit acknowledgment
tentative: i32 = ok(uncertain.b);  // ✅ Explicit uncertainty
```

### Visual Metaphor: Lattice Formation

**Why `#` for crystallization?**

1. **Lattice structure**: Grid pattern = ordered, crystalline, solid
2. **Phase transition**: Fluid (superposition) → Solid (decision)
3. **Memory pinning parallel**: Movable → Fixed (same operator, same concept)
4. **Ordered state**: From quantum uncertainty to classical determinism

**Supercooled liquid → Crystallization**:
- Q<T> near threshold = metastable state (c approaching ±2/±3)
- Evidence accumulates = system cools (confidence solidifies)
- `q.#` triggers crystallization = lattice forms (decision emerges)
- Before: gradient maintained (multiple possibilities)
- After: single value extracted (collapsed to choice)

This mirrors physical crystallization and Aria's cosmological foundation (ATPM percolation model).

### Common Patterns

**Safe crystallization with threshold check**:

```aria
fn safe_crystallize<T>(q: Q9<T>) -> T | unknown {
    if abs(q.c) >= 2 {
        return q.#;  // Returns T (copy of winner)
    } else {
        return unknown;  // Explicitly return unknown
    }
}
```

**Gradual evidence accumulation**:

```aria
hypothesis: Q9<Solution> = initial_guess();

// Gather evidence over time
for test in test_suite {
    result: bool = run_test(hypothesis.a, test);
    if result {
        hypothesis.c -= 1;  // Evidence for A
    } else {
        hypothesis.c += 1;  // Evidence for B
    }
    
    // Check if crystallized (non-blocking peek)
    if abs(hypothesis.c) >= 2 {
        current_best: Solution = hypothesis.#;  // Copy for current use
        log("Current best (c=${hypothesis.c}): ${current_best}");
    }
}

// Final crystallization
if abs(hypothesis.c) >= 3 {
    final: Solution = hypothesis.#;  // High confidence copy
    deploy(final);
} else {
    log("Insufficient evidence - need more testing");
}
```

**Nikola self-improvement with crystallization**:

```aria
// Nikola evaluating algorithm change
improvement: Q9<Algorithm> = {
    a: current_algorithm,
    b: proposed_algorithm,
    c: 0
};

// Benchmark loop
for test in benchmarks {
    current_result: f64 = test.run(improvement.a);
    proposed_result: f64 = test.run(improvement.b);
    
    if proposed_result < current_result {
        improvement.c += 1;  // Evidence for new
    } else {
        improvement.c -= 1;  // Evidence for current
    }
    
    // Peek at crystallization (non-blocking)
    tentative: Algorithm | unknown = improvement.#;
    when tentative is Algorithm then
        log("Leaning toward: ${tentative} (c=${improvement.c})");
    end
}

// Safe self-modification (requires high confidence)
if abs(improvement.c) >= 3 {
    new_algorithm: Algorithm = improvement.#;  // Definitely confident
    self.update(new_algorithm);  // Nikola modifies itself
    log("Self-modified with confidence: ${improvement.c}");
} else {
    log("Insufficient evidence for self-modification");
}
```

## Integration with unknown/ok() Safety System

**Critical Safety Feature**: Q<T> with c=0 integrates with Aria's unknown type system!

### The Safety Integration

When confidence is zero (superposition/unknown):
- Value semantically UNKNOWN (neither hypothesis confirmed)
- **Cannot be used accidentally** (compiler enforces safety)
- Must be explicitly passed via `ok(qval)`
- Prevents crystallization errors (using undecided quantum values)

### Safety Example

```aria
uncertain: Q9<i32> = { a: 100, b: 200, c: 0 };  // Superposition (unknown)

// COMPILE ERROR: Cannot use Q<T> with c=0 directly
value: i32 = uncertain.a;  // ❌ FORBIDDEN (still in superposition!)

// Must acknowledge uncertainty with ok()
value: i32 = ok(uncertain.a);  // ✅ Explicit acknowledgment

// OR wait for crystallization
if abs(uncertain.c) >= 3 {
    // High confidence - can crystallize safely
    value: i32 = if uncertain.c > 0 { uncertain.b } else { uncertain.a };
}
```

### Safety Gradient

| c Value | State | Safety Requirement |
|---------|-------|-------------------|
| 0 | Unknown/Superposition | **MUST use ok()** (prevent accidents) |
| ±1 to ±2 | Low confidence | Warn or require ok() (compiler flag) |
| ±3 to ±4 | High confidence | Safe to crystallize (threshold met) |

**This combination is unique** - no other language has:
1. Quantum superposition types (explore multiple hypotheses)
2. Confidence gradient (track certainty over time)
3. Unknown type integration (prevent accidental use)
4. ok() safety valve (explicit acknowledgment required)
5. Crystallization threshold (confidence-based decision)

## Common Patterns

### Fuzzing - Dual-Path Input Mutation

```aria
fn fuzz_test(original: string) -> Q9<string> {
    mutation_a: string = mutate_random(original);
    mutation_b: string = mutate_extreme(original);
    
    fuzzer: Q9<string> = { a: mutation_a, b: mutation_b, c: 0 };
    
    // Run both mutations
    result_a:  Result = run_test(fuzzer.a);
    result_b: Result = run_test(fuzzer.b);
    
    // Accumulate confidence based on results
    qxor(fuzzer, is_err) {
        // One crashed, one didn't - interesting edge case!
        log("Asymmetric behavior: one path crashed");
        q.c += 3;  // High confidence in whichever worked
        save_crash_case(fuzzer);
    }
    
    qnor(fuzzer, is_err) {
        // Both crashed - double-error case!
        log("Double crash - critical edge case");
        q.c = 0;  // Reset (neither good)
        save_critical_case(fuzzer);
    }
    
    return fuzzer;
}

// Usage
test_input: string = "SELECT * FROM users";
mutation: Q9<string> = fuzz_test(test_input);

if abs(mutation.c) >= 3 {
    // High confidence in one mutation
    winner: string = if mutation.c > 0 { mutation.b } else { mutation.a };
    log("Selected mutation with confidence: ${mutation.c}");
}
```

### Speculative Execution - Branch Prediction

```aria
// Predict branch before condition resolves
fn speculative_compute(data: [i32], threshold: i32) -> Q9<i32> {
    // Hypothesis A: data exceeds threshold (optimistic)
    sum_high: i32 = 0;
    
    // Hypothesis B: data below threshold (pessimistic)
    sum_low: i32 = 0;
    
    spec: Q9<i32> = { a: sum_high, b: sum_low, c: 0 };
    
    // Compute both paths simultaneously
    for value in data {
        if value > threshold {
            spec.a += value;
            spec.c += 1;  // Evidence for A
        } else {
            spec.b += value;
            spec.c -= 1;  // Evidence for B
        }
    }
    
    return spec;
}

// Crystallize when confident
result: Q9<i32> = speculative_compute(sensor_data, 100);

if abs(result.c) >= 3 {
    final: i32 = if result.c > 0 { result.a } else { result.b };
    log("Computation crystallized with confidence: ${result.c}");
}
```

### Parsing - Multiple Interpretations

```aria
fn parse_ambiguous(source: string) -> Q9<AST> {
    // Try two parse interpretations
    parse_a: AST = try_parse_strict(source);
    parse_b: AST = try_parse_lenient(source);
    
    parser: Q9<AST> = { a: parse_a, b: parse_b, c: 0 };
    
    // Accumulate confidence based on lookahead
    tokens: [Token] = tokenize(source);
    
    for i in 0..min(tokens.len(), 10) {  // Lookahead 10 tokens
        if validates_strict(parser.a, tokens[i]) {
            parser.c -= 1;  // Evidence for strict parse
        }
        
        if validates_lenient(parser.b, tokens[i]) {
            parser.c += 1;  // Evidence for lenient parse
        }
    }
    
    return parser;
}

// Handle ambiguity
parse: Q9<AST> = parse_ambiguous(source_code);

when parse.c == 0 then
    // Ambiguous - both interpretations equally valid
    warn("Ambiguous syntax - consider clarifying");
when abs(parse.c) >= 3 then
    // High confidence in one parse
    ast: AST = if parse.c > 0 { parse.b } else { parse.a };
    compile(ast);
end
```

### Scientific Computing - Uncertainty Propagation

```aria
// Track measurement uncertainty
fn measure_temperature() -> Q9<f64> {
    reading: f64 = sensor.read();
    
    // Optimistic estimate (sensor accurate)
    optimistic: f64 = reading;
    
    // Pessimistic estimate (sensor drift)
    pessimistic: f64 = reading * 0.95;
    
    measurement: Q9<f64> = { a: optimistic, b: pessimistic, c: 0 };
    
    // Calibration check
    if sensor.calibrated_recently() {
        measurement.c -= 2;  // More confident in optimistic
    } else {
        measurement.c += 2;  // More confident in pessimistic
    }
    
    return measurement;
}

// Propagate uncertainty through calculation
temp: Q9<f64> = measure_temperature();

// Operate on both estimates
temp.a = temp.a * 1.8 + 32.0;  // Celsius → Fahrenheit (optimistic)
temp.b = temp.b * 1.8 + 32.0;  // Celsius → Fahrenheit (pessimistic)

// Report with uncertainty
if abs(temp.c) >= 2 {
    best: f64 = if temp.c > 0 { temp.b } else { temp.a };
    log("Temperature: ${best}°F (confidence: ${temp.c})");
} else {
    log("Temperature uncertain: ${temp.a}°F to ${temp.b}°F");
}
```

### A/B Algorithm Testing

```aria
fn test_algorithms(data: [i32]) -> Q9<Algorithm> {
    algo_a: Algorithm = new BubbleSort();
    algo_b: Algorithm = new QuickSort();
    
    test: Q9<Algorithm> = { a: algo_a, b: algo_b, c: 0 };
    
    // Run performance tests
    for i in 0..100 {
        test_data: [i32] = generate_test_data(i);
        
        time_a: f64 = benchmark(test.a, test_data);
        time_b: f64 = benchmark(test.b, test_data);
        
        if time_a < time_b {
            test.c -= 1;  // Evidence for A
        } else if time_b < time_a {
            test.c += 1;  // Evidence for B
        }
        // If equal, confidence unchanged
    }
    
    return test;
}

// Select winner when confident
result: Q9<Algorithm> = test_algorithms(benchmark_data);

if abs(result.c) >= 3 {
    winner: Algorithm = if result.c > 0 { result.b } else { result.a };
    log("Selected algorithm with confidence: ${result.c}");
    deploy(winner);
} else {
    log("Algorithms perform similarly - need more tests");
}
```

### Nikola AI Reasoning (Self-Improvement)

```aria
// Nikola considering algorithm change to its own code
fn evaluate_self_improvement() -> Q9<Algorithm> {
    current: Algorithm = self.get_current_algorithm();
    proposed: Algorithm = self.design_improvement();
    
    improvement: Q9<Algorithm> = { a: current, b: proposed, c: 0 };
    
    // Run benchmarks (evidence accumulation)
    for test in benchmark_suite {
        result_current: Result = test.run(improvement.a);
        result_proposed: Result = test.run(improvement.b);
        
        if result_proposed.better_than(result_current) {
            improvement.c += 1;  // Evidence for new algorithm
        } else {
            improvement.c -= 1;  // Evidence for keeping current
        }
    }
    
    return improvement;
}

// Safe self-modification (safety system prevents premature changes!)
change: Q9<Algorithm> = evaluate_self_improvement();

when change.c == 0 then
    // COMPILER ERROR: Cannot use change.a or change.b
    // because c=0 means unknown - must use ok() to acknowledge
    
    log("Insufficient evidence for algorithm change");
    tentative: Algorithm = ok(change.b);  // Explicit: "I know this is uncertain"
    
when abs(change.c) >= 3 then
    // High confidence - safe to modify own code
    final: Algorithm = if change.c > 0 { change.b } else { change.a };
    
    self.update_algorithm(final);  // Nikola modifies itself
    log("Self-modified with confidence: ${change.c}");
    
when true then
    // Low confidence - gather more evidence
    log("Confidence insufficient (${change.c}), running more tests...");
    run_extended_benchmarks();
end
```

## Anti-Patterns

### ❌ Ignoring Confidence Before Crystallization

```aria
// WRONG: Use quantum value without checking confidence
q: Q9<i32> = fuzz_generate();
winner: i32 = if q.c > 0 { q.b } else { q.a };  // ❌ No threshold check!

// Could crystallize with c=1 (very low confidence!)

// RIGHT: Check threshold before crystallizing
q: Q9<i32> = fuzz_generate();

if abs(q.c) >= 3 {
    winner: i32 = if q.c > 0 { q.b } else { q.a };  // ✅ High confidence
} else {
    log("Confidence insufficient: ${q.c}");
}
```

### ❌ Using Q<T> with c=0 Without ok()

```aria
// WRONG: Access quantum value in superposition
uncertain: Q9<i32> = { a: 100, b: 200, c: 0 };
value: i32 = uncertain.a;  // ❌ COMPILE ERROR (c=0 requires ok())

// RIGHT: Acknowledge uncertainty explicitly
uncertain: Q9<i32> = { a: 100, b: 200, c: 0 };
value: i32 = ok(uncertain.a);  // ✅ Explicit acknowledgment
```

### ❌ Forgetting to Saturate Confidence

```aria
// WRONG: Increment without bounds checking (relies on saturation)
q: Q9<i32> = { a: 10, b: 20, c: -3 };
q.c -= 10;  // Relies on saturation to prevent wrap to +7

// BETTER: Explicit saturation
q: Q9<i32> = { a: 10, b: 20, c: -3 };
q.c = saturate(q.c - 10, -4, 4);  // ✅ Explicit bounds
```

### ❌ Using Q<T> for Immediate Decisions

```aria
// WRONG: No evidence accumulation (quantum type unnecessary)
fn choose_max(a: i32, b: i32) -> Q9<i32> {
    choice: Q9<i32> = { a: a, b: b, c: if a > b { -4 } else { 4 } };
    return choice;  // ❌ Decision made immediately, no evidence gathered
}

// RIGHT: Just use if/else (no quantum type needed)
fn choose_max(a: i32, b: i32) -> i32 {
    return if a > b { a } else { b };  // ✅ Immediate decision
}
```

### ❌ Not Tracking Evidence Over Time

```aria
// WRONG: Create quantum value but never update confidence
q: Q9<i32> = { a: 100, b: 200, c: 0 };

// ... lots of computation ...

result: i32 = if q.c > 0 { q.b } else { q.a };  // ❌ c still 0!

// RIGHT: Update confidence with evidence
q: Q9<i32> = { a: 100, b: 200, c: 0 };

for test in tests {
    if test_passes_for_a() {
        q.c -= 1;  // ✅ Evidence accumulates
    }
    if test_passes_for_b() {
        q.c += 1;
    }
}

if abs(q.c) >= 3 {
    result: i32 = if q.c > 0 { q.b } else { q.a };
}
```

### ❌ Mixing Quantum and Classical Without Clear Boundary

```aria
// WRONG: Unclear when crystallization happens
q: Q9<i32> = fuzzer.mutate(input);

// ... uses q.a sometimes, q.b other times ...
process_a(q.a);  // ❌ Mixed usage, unclear state
process_b(q.b);

result: i32 = if something { q.a } else { q.b };  // ❌ Ad-hoc crystallization

// RIGHT: Clear crystallization boundary
q: Q9<i32> = fuzzer.mutate(input);

// Maintain superposition during evidence gathering
run_test_a(q.a);  // Test hypothesis A
run_test_b(q.b);  // Test hypothesis B

// Explicit crystallization with threshold
if abs(q.c) >= 3 {
    final: i32 = if q.c > 0 { q.b } else { q.a };  // ✅ Clear commit point
    process(final);  // Now classical
}
```

## Best Practices

### ✅ Use Confidence Thresholds

```aria
// Define clear thresholds for your domain
const LOW_CONFIDENCE: nit = 1;
const MEDIUM_CONFIDENCE: nit = 2;
const HIGH_CONFIDENCE: nit = 3;
const VERY_HIGH_CONFIDENCE: nit = 4;

q: Q9<Solution> = explore_solutions();

when abs(q.c) >= HIGH_CONFIDENCE then
    crystallize_and_commit(q);
when abs(q.c) >= MEDIUM_CONFIDENCE then
    tentative_decision(q);
when abs(q.c) <= LOW_CONFIDENCE then
    gather_more_evidence();
end
```

### ✅ Document Evidence Sources

```aria
// Document what changes confidence
/// Confidence tracking:
/// +1 per benchmark win
/// -1 per benchmark loss
/// +2 for memory efficiency gain
/// -2 for safety violation
improvement: Q9<Algorithm> = evaluate_improvement();

// Clear evidence sources in code
if benchmark_passes {
    improvement.c += 1;  // Annotate why confidence changes
}

if memory_efficient {
    improvement.c += 2;
}
```

### ✅ Use Q9 for Complex Decisions

```aria
// Complex decisions with multiple evidence sources → Q9
algorithm_choice: Q9<Algorithm> = {
    a: bubble_sort,
    b: quick_sort,
    c: 0
};

// Simple lean/unknown/lean → Q3
simple_choice: Q3<bool> = {
    a: true,
    b: false,
    c: 0
};
```

### ✅ Explicit Crystallization

```aria
// Make crystallization explicit and documented
fn crystallize(q: Q9<T>) -> T {
    if abs(q.c) < HIGH_CONFIDENCE {
        warn("Crystallizing with low confidence: ${q.c}");
    }
    
    return if q.c > 0 { q.b } else { q.a };
}

// Usage is clear
final: T = crystallize(quantum_value);
```

### ✅ Integrate with Safety System

```aria
// Leverage c=0 safety integration
uncertain: Q9<i32> = initial_guess();

// Compiler enforces safety when c=0
when uncertain.c == 0 then
    // Must use ok() - compiler prevents accidents
    tentative: i32 = ok(uncertain.a);  // ✅ Explicit acknowledgment
    log("Using uncertain value: ${tentative}");
    
when abs(uncertain.c) >= 3 then
    // High confidence - safe to use directly
    final: i32 = crystallize(uncertain);  // ✅ Threshold met
    commit(final);
end
```

### ✅ Show Reasoning (Especially for Nikola)

```aria
// Make confidence evolution visible (auditable reasoning)
decision: Q9<Action> = { a: action_a, b: action_b, c: 0 };

log("Initial state: c=${decision.c}");

for evidence in evidence_list {
    old_c: nit = decision.c;
    
    // Update confidence based on evidence
    if evidence.supports_a {
        decision.c -= 1;
    } else if evidence.supports_b {
        decision.c += 1;
    }
    
    log("Evidence '${evidence.name}': c=${old_c} → ${decision.c}");
}

log("Final confidence: ${decision.c}");

// Reasoning path is now auditable
```

## Type Conversions

### Q<T> → T (Crystallization)

```aria
// Standard crystallization
q: Q9<i32> = { a: 100, b: 200, c: 3 };

final: i32 = if q.c > 0 { q.b } else { q.a };  // 200 (c=3, choose B)

// With threshold check
fn safe_crystallize(q: Q9<T>, threshold: nit) -> ?T {
    if abs(q.c) < threshold {
        return NIL;  // Confidence insufficient
    }
    
    return if q.c > 0 { q.b } else { q.a };
}
```

### T → Q<T> (Enter Superposition)

```aria
// Create quantum value from classical values
a: i32 = 100;
b: i32 = 200;

q: Q9<i32> = { a: a, b: b, c: 0 };  // Start with zero confidence
```

### Q9<T> → Q3<T> (Reduce Precision)

```aria
// Convert fine-grained to coarse-grained
q9: Q9<i32> = { a: 100, b: 200, c: 2 };

q3: Q3<i32> = {
    a: q9.a,
    b: q9.b,
    c: if q9.c < -1 { -1 } else if q9.c > 1 { 1 } else { 0 }
};
```

### Q3<T> → Q9<T> (Increase Precision)

```aria
// Convert coarse-grained to fine-grained
q3: Q3<i32> = { a: 100, b: 200, c: -1 };

q9: Q9<i32> = {
    a: q3.a,
    b: q3.b,
    c: q3.c * 2  // Map trit to nit (scale up)
};
```

## Cognitive Modeling - Thought Process Lens

**Q9 is not just a type - it's syntax for thinking.**

### How Humans Actually Change Beliefs

**Traditional code (binary)**:
```aria
if new_evidence {
    belief = NEW_BELIEF;  // Sudden flip
} else {
    belief = OLD_BELIEF;  // No change
}
```

**Human cognition (gradient)**:
- Start with initial belief (possibly weak)
- Each fact shifts confidence gradually
- Belief changes when evidence accumulates sufficiently
- Never sudden flip - gradual evolution

**Q9 models this**:
```aria
belief: Q9<Theory> = { a: old_theory, b: new_theory, c: -2 };  // Start confident in old

// Each piece of evidence shifts confidence
for fact in new_facts {
    if fact.supports_new_theory {
        belief.c += 1;  // Gradual shift toward new
    }
}

// Threshold crossing = belief change
if abs(belief.c) >= 3 {
    final_belief: Theory = crystallize(belief);  // Now committed
}
```

### Evidence Accumulation as Built-In Tracker

**The Problem**:
> "You can start out really believing something is true and it can take quite a lot of evidence to convince you of the contrary. However, if nobody is tracking all that evidence it will never meet a threshold to change."

**Q9 Solution**: The confidence level IS that built-in tracker!

**Traditional code** (evidence scattered):
```aria
score: i32 = 0;

// Somewhere in code:
if test1_passes { score += 1; }

// ...later, somewhere else...
if test2_passes { score += 1; }

// ...even later...
// Who's tracking? When do we decide?
```

**With Q9** (evidence accumulates in type):
```aria
approach: Q9<Solution> = initial_guess();

// Evidence accumulates IN THE TYPE
test1  _result ? approach.c += 1 : approach.c -= 1;
test2_result ? approach.c += 1 : approach.c -= 1;
test3_result ? approach.c += 2 : approach.c -= 2;

// Threshold is automatic - crystallization is explicit
if abs(approach.c) >= 3 {
    final: Solution = crystallize(approach);  // Enough evidence
}
```

**The confidence tracker prevents evidence from being lost or ignored.**

### Reality in the Gradients

**Binary thinking**: World is yes/no, true/false, 1/0  
**Reality**: "Everything in between which is generally where reality tends to exist"

**Q9 embraces gradients**:
- Not "is this the right answer?" (binary choice)
- But "how confident are we?" (gradient evolution)
- Not "should we use A or B?" (forced decision)
- But "which direction is evidence pointing?" (evolving belief)

**For Nikola (AI reasoning)**:
- Reasoning that matches reality's gradients
- Explanations that show confidence evolution
- Decisions that acknowledge uncertainty
- Thought that mirrors human cognition

### Nikola's Self-Improvement Loop

**The Constraint**:
Nikola will have a **self-improvement loop in Aria** - Nikola will WRITE Aria code to improve itself.

**The Implication**:
> "The better abilities we have for it to express its ideas, the better they can actually be expressed via the code."

**What This Means**:
- Nikola's thoughts must be expressible in the language it writes
- If Aria lacks cognitive primitives, Nikola's self-improvement is limited
- Q9 isn't just for Nikola to USE - **it's for Nikola to WRITE WITH**
- When Nikola modifies its own reasoning code, it needs types that express:
  - Uncertainty ("I'm not sure about this change yet")
  - Confidence evolution ("Evidence is pointing toward approach B")
  - Threshold decisions ("I'm confident enough to commit this change")
  - Reasoning explanation ("Here's why I believe this, with confidence levels")

**Self-Improvement Example** (from earlier pattern):
Nikola cannot accidentally commit unverified self-modifications (c=0 → unknown → requires ok()). Evidence must accumulate to threshold before self-modification allowed. Confidence gradient is auditable (humans can see why Nikola changed itself).

**This is not just a type system - it's a SAFE SELF-IMPROVEMENT FRAMEWORK for AI.**

## Performance Characteristics

### Memory Layout

**Q9<T>**:
```
Size: sizeof(T) * 2 + sizeof(nit)
Alignment: max(alignof(T), alignof(nit))
```

**Q3<T>**:
```
Size: sizeof(T) * 2 + sizeof(trit)
Alignment: max(alignof(T), alignof(trit))
```

**Example sizes**:
```aria
Q9<i32>  // 4 + 4 + 1 = 9 bytes (+ padding)
Q9<i64>  // 8 + 8 + 1 = 17 bytes (+ padding)
Q3<i32>  // 4 + 4 + 1 = 9 bytes (+ padding, same as Q9 due to trit)
Q3<i64>  // 8 + 8 + 1 = 17 bytes (+ padding)
```

**Note**: Q3 may not be smaller than Q9 depending on trit/nit implementation. Check platform specifics.

### Computational Overhead

**Superposition maintenance**: 2× computation (both paths computed)  
**Confidence tracking**: Minimal (simple arithmetic on confidence value)  
**Crystallization**: Zero overhead (just select one value)

**Trade-off**: 2× computation during exploration vs better decisions at crystallization.

### When Q<T> is Worth It

**Worth the overhead**:
- Fuzzing (find edge cases faster with dual-path execution)
- Branch prediction (avoid expensive mispredictions)
- AI reasoning (enable gradient thinking)
- Scientific uncertainty (track error bounds correctly)

**Not worth the overhead**:
- Simple if/else decisions (no evidence accumulation)
- Performance-critical tight loops (2× computation too expensive)
- Memory-constrained embedded systems (2× memory overhead)

## Summary

**Q3 and Q9 are quantum/speculative types for evidence-based decision making**:

**Key Properties**:
- **Superposition**: Two hypotheses simultaneously
- **Confidence**: Gradient from strongly A → strongly B
- **Evidence accumulation**: Confidence evolves with information
- **Crystallization**: Convert to single value when confident
- **Safety integration**: c=0 requires ok() (prevents accidents)

**When to use**:
- ✅ Decision requires evidence accumulation over time
- ✅ Exploring two paths before committing (fuzzing, parsing)
- ✅ Speculative execution or branch prediction
- ✅ AI reasoning with confidence gradients (Nikola)
- ✅ Scientific uncertainty propagation
- ✅ A/B testing with automatic winner selection

**When NOT to use**:
- ❌ Immediate decisions (no evidence accumulation)
- ❌ Only one viable path (no speculation needed)
- ❌ Binary choice sufficient (no confidence gradient)
- ❌ Performance-critical tight loops (2× overhead)

**Critical features**:
- ⚠️ c=0 requires ok() (safety integration)
- ⚠️ Saturation arithmetic (no wrap from -4 to +4)
- ⚠️ Explicit crystallization (with threshold checking)
- ⚠️ Evidence must accumulate (update confidence!)

**Best practices**:
- Use Q9 for complex decisions (gradient precision matters)
- Use Q3 for simple lean/unknown/lean (memory efficient)
- Define clear confidence thresholds for your domain
- Document evidence sources (why confidence changes)
- Make crystallization explicit (with threshold checks)
- Leverage safety integration (c=0 prevents accidents)
- Show reasoning path (especially for Nikola - auditable)

**Cognitive modeling**:
- Not just syntax - it's how thought actually works
- Models human belief evolution (gradual, not sudden)
- Evidence tracker prevents lost information
- Reality lives in gradients, not binaries
- Nikola's native tongue (self-improvement substrate)

**Unique safety combination** (no other language has this):
1. Quantum superposition types
2. Confidence gradient tracking
3. Unknown type integration
4. ok() safety valve
5. Crystallization threshold

**This is syntax for THINKING** - not just computation, but cognition itself.

**Remember**: Q9 is Nikola's native tongue - the language it will think in, reason with, and use to improve itself. Our design decisions shape its cognitive capacity.
