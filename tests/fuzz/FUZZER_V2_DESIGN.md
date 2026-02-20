# Aria Fuzzer V2 - Comprehensive Grammar-Based Design

**Goal**: Create the most thorough fuzzer possible using specs, programming guide, and test corpus

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    Fuzzer V2 Components                      │
├─────────────────────────────────────────────────────────────┤
│                                                               │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐       │
│  │   Grammar    │  │    Type      │  │   Pattern    │       │
│  │   Parser     │  │   System     │  │   Extractor  │       │
│  │              │  │              │  │              │       │
│  │ (From specs) │  │ (TYPE_SYSTEM)│  │ (Test corpus)│       │
│  └──────┬───────┘  └──────┬───────┘  └──────┬───────┘       │
│         │                 │                 │                │
│         └─────────────────┴─────────────────┘                │
│                           │                                  │
│                           ▼                                  │
│               ┌───────────────────────┐                      │
│               │  Program Generator    │                      │
│               │  ─────────────────    │                      │
│               │  • Type-aware         │                      │
│               │  • Semantically valid │                      │
│               │  • Edge case focused  │                      │
│               └───────────┬───────────┘                      │
│                           │                                  │
│         ┌─────────────────┼─────────────────┐                │
│         ▼                 ▼                 ▼                │
│  ┌──────────┐      ┌──────────┐      ┌──────────┐           │
│  │ Valid    │      │ Boundary │      │ Invalid  │           │
│  │ Programs │      │ Cases    │      │ Programs │           │
│  └──────────┘      └──────────┘      └──────────┘           │
│                                                               │
└─────────────────────────────────────────────────────────────┘
```

## Data Sources

### 1. Type System Spec (`TYPE_SYSTEM.md`)
Extract:
- All primitive types (i8, i16, i32, i64, i128, i256, i512, u*, f*, tbb*)
- Type ranges and bounds
- Valid operations per type
- Type conversion rules
- Result<T> patterns
- Wild memory rules

### 2. Programming Guide
Parse for:
- Statement syntax (func, if, while, for, return, defer)
- Expression grammar (operators, precedence)
- Declaration patterns
- Control flow structures
- Memory allocation syntax
- Generic syntax
- Trait/impl patterns

### 3. Test Corpus (693 files)
Extract patterns:
- Common idioms
- Function signatures
- Variable naming conventions
- Loop patterns
- Error handling styles
- GPU/SIMD usage

### 4. Error Handling Spec (`ERROR_HANDLING.md`)
- Result<T> usage
- Unwrap patterns
- Error propagation
- Failsafe patterns

### 5. Memory Model Spec (`MEMORY_MODEL.md`)
- Wild memory usage
- Arena/pool/slab patterns
- Allocation strategies
- Lifetime rules

## Generation Strategies

### Strategy 1: Type-Exhaustive Testing
**Goal**: Test every type with every operation

For each type T in {i8, i16, i32, i64, i128, i256, i512, u8-u512, f32, f64, tbb8-tbb64}:
- Arithmetic: +, -, *, /, %
- Comparison: ==, !=, <, >, <=, >=
- Bitwise: &, |, ^, ~, <<, >>
- Boundary values: MIN, MAX, 0, -1, 1
- Edge cases: overflow, underflow, division by zero

### Strategy 2: Grammar-Based Generation
**Recursive descent based on Aria grammar**

```python
Program := Function*
Function := "func" ":" Identifier "=" Type "(" Params? ")" Block ";"
Block := "{" Statement* "}"
Statement := VarDecl | Assignment | IfStmt | WhileStmt | ForStmt | Return | Call
VarDecl := Type ":" Identifier "=" Expr ";"
Expr := Literal | Variable | BinOp | Call | UnaryOp
BinOp := Expr Operator Expr
```

### Strategy 3: Property-Based Testing
Generate programs that test specific properties:
- **Type safety**: No type confusion
- **Memory safety**: No use-after-free
- **Overflow safety**: TBB bounds respected
- **Error handling**: Result<T> properly checked

### Strategy 4: Mutation with Context
Improve existing mutation fuzzer:
- Type-aware mutations (only mutate i32 → i64, not i32 → string)
- Syntax-preserving mutations
- Keep programs parseable (unlike current char-level mutations)

### Strategy 5: Template-Based Generation
Use templates from real code:

```aria
// Template: Loop with accumulator
func:FUNCNAME = RETTYPE() {
    TYPE:acc = INIT_VAL;
    for (i till COUNT) {
        acc = acc OP i;
    }
    return acc;
};
```

Fill in: FUNCNAME, RETTYPE, TYPE, INIT_VAL, COUNT, OP

### Strategy 6: Combinatorial Testing
Test combinations of features:
- Generics + Result<T>
- GPU kernels + SIMD
- Wild memory + arenas
- Async + error handling
- Traits + impl + generics

## Test Categories

### A. Positive Tests (Should Compile)
1. **Minimal valid programs**: Single function returning 0
2. **Type coverage**: Every type in simple operations
3. **Control flow**: If/else, while, for, nested
4. **Functions**: Parameters, recursion, overloading
5. **Generics**: Type parameters, constraints
6. **Memory**: Wild allocations, arenas, pools
7. **GPU**: Kernels, SIMD operations
8. **Contracts**: Requires, ensures, invariants

### B. Boundary Tests (Edge Cases)
1. **Integer bounds**: MIN_VALUE, MAX_VALUE, overflow
2. **TBB ranges**: Values at/near bounds
3. **Deep nesting**: 100+ levels of blocks
4. **Large identifiers**: 10,000 char names
5. **Many parameters**: Functions with 100+ params
6. **Long expressions**: Deeply nested arithmetic
7. **Unicode**: Valid UTF-8 in identifiers/strings

### C. Negative Tests (Should Error Gracefully)
1. **Type mismatches**: Assigning wrong types
2. **Undefined variables**: Using before declaration
3. **Invalid syntax**: Missing semicolons, unmatched braces
4. **Division by zero**: Compile-time detection
5. **TBB bounds violations**: Values outside range
6. **Memory errors**: Use after free (should be caught)
7. **Result<T> unwrap without check**

## Implementation Plan

### Phase 1: Grammar Parser (Week 1)
- Parse TYPE_SYSTEM.md for type definitions
- Build AST for Aria grammar
- Extract operators and precedence from programming guide

### Phase 2: Type System (Week 1-2)
- Model all primitive types
- Implement type checking rules
- Build operation compatibility matrix

### Phase 3: Generator Core (Week 2-3)
- Implement recursive descent generator
- Type-aware expression generation
- Statement generation with context

### Phase 4: Smart Mutations (Week 3)
- Parse-preserving mutations
- Type-aware replacements
- Semantic transformations

### Phase 5: Coverage Tracking (Week 3-4)
- Track which grammar rules are exercised
- Code coverage via compiler instrumentation
- Type combination coverage

### Phase 6: Integration (Week 4)
- Integrate with existing fuzzer infrastructure
- Parallel execution
- Crash deduplication
- Regression tests from crashes

## Success Metrics

| Metric | Current | Target |
|--------|---------|--------|
| **Grammar coverage** | ~30% | 95%+ |
| **Type combinations** | ~10% | 100% |
| **Valid program ratio** | ~1% | 80%+ |
| **Exec/sec** | 50 | 200+ |
| **Edge case detection** | Manual | Automated |
| **Crash dedup accuracy** | ~60% | 95%+ |

## Advanced Features

### 1. Differential Testing
Compare outputs against reference implementations:
- Different optimization levels
- CPU vs GPU execution
- Different allocators

### 2. Crash Minimization
Automatically reduce crashing inputs to minimal form:
```python
def minimize_crash(input: str) -> str:
    """Reduce input while preserving crash."""
    # Binary search on lines
    # Delta debugging on tokens
    # Structural reduction (remove unused vars)
```

### 3. Coverage-Guided Generation
Use compiler coverage to guide generation:
- Prioritize inputs that hit new code paths
- Track branch coverage
- Focus on under-tested areas

### 4. Semantic Awareness
Generate programs that are semantically interesting:
- Resource exhaustion (allocate until OOM)
- Concurrent access patterns (future async support)
- Complex type interactions

### 5. Oracle-Based Testing
Verify compiler behavior:
- Type checker should reject type errors
- Optimizer should not change semantics
- Code gen should match semantics

## File Structure

```
fuzz/
├── v2/
│   ├── grammar/
│   │   ├── parser.py          # Parse specs → grammar rules
│   │   ├── type_system.py     # Type definitions & rules
│   │   └── ast.py             # Aria AST representation
│   ├── generators/
│   │   ├── exhaustive.py      # Type-exhaustive tests
│   │   ├── grammar_based.py   # Recursive descent generation
│   │   ├── template_based.py  # Template filling
│   │   └── property_based.py  # Property-driven generation
│   ├── mutators/
│   │   ├── type_aware.py      # Type-preserving mutations
│   │   ├── semantic.py        # Semantic transformations
│   │   └── structural.py      # AST-level mutations
│   ├── oracles/
│   │   ├── type_checker.py    # Verify type errors caught
│   │   ├── optimizer.py       # Verify semantics preserved
│   │   └── codegen.py         # Verify correct execution
│   ├── coverage/
│   │   ├── grammar_coverage.py   # Grammar rule tracking
│   │   ├── type_coverage.py      # Type combination tracking
│   │   └── code_coverage.py      # Compiler coverage via lcov
│   ├── utils/
│   │   ├── minimizer.py       # Crash case reduction
│   │   ├── deduplicator.py    # Crash deduplication
│   │   └── corpus_analyzer.py # Analyze test patterns
│   └── aria_fuzzer_v2.py      # Main orchestrator
└── corpus_v2/                  # Structured corpus
    ├── types/                  # Per-type tests
    ├── operators/              # Per-operator tests
    ├── control_flow/           # If/while/for tests
    ├── functions/              # Function tests
    ├── generics/               # Generic tests
    └── edge_cases/             # Boundary conditions
```

## Example Generated Tests

### Type-Exhaustive Test (i64 arithmetic)
```aria
func:test_i64_add_basic = i32() {
    i64:a = 42;
    i64:b = 17;
    i64:sum = (a + b);
    return 0;
};

func:test_i64_add_overflow = i32() {
    i64:max = 9223372036854775807;
    i64:one = 1;
    // Should compile (runtime overflow)
    i64:overflow = (max + one);
    return 0;
};

func:test_i64_div_zero = i32() {
    i64:x = 10;
    i64:zero = 0;
    // Should catch at compile time if constant
    i64:result = (x / zero);
    return 0;
};
```

### Grammar-Based Generation
```aria
func:generated_12345 = i32() {
    i32:var_0 = 100;
    while ((var_0 > 0)) {
        if ((var_0 % 2) == 0) {
            var_0 = (var_0 - 1);
        } else {
            var_0 = (var_0 - 2);
        };
    };
    return var_0;
};
```

### Property Test (TBB bounds)
```aria
func:test_tbb8_bounds = i32() {
    // Should reject value outside [0, 255]
    tbb8<0, 255>:valid = 100;
    
    // Should reject at compile time
    tbb8<0, 255>:invalid = 300;  // ERROR: 300 > 255
    
    return 0;
};
```

### Template-Based (Accumulator pattern)
```aria
func:sum_squares = i64() {
    i64:acc = 0;
    for (i till 100) {
        i64:square = (i * i);
        acc = (acc + square);
    };
    return acc;
};
```

## Integration with Existing Infrastructure

### Backward Compatible
- Keep existing mutation fuzzer as fallback
- Reuse crash classification (CrashType enum)
- Maintain same directory structure (corpus/, crashes/)
- Same command-line interface

### Enhanced Reporting
```
Fuzzing Campaign Report
=======================
Duration: 24h 15m
Execs: 1,234,567 (14.3 execs/sec)

Coverage:
  Grammar rules: 142/150 (94.7%)
  Type combinations: 891/900 (99.0%)
  Code coverage: 87.3%

Generated Tests:
  Valid programs: 987,654 (80.0%)
  Boundary cases: 123,456 (10.0%)
  Invalid programs: 123,457 (10.0%)

Crashes: 0 ✓
Timeouts: 12 (0.001%)
Assertions: 0 ✓

Top uncovered grammar rules:
  - Generic trait constraints (5 rules)
  - GPU texture sampling (2 rules)
  - Async error handling (3 rules)
```

## Next Steps

1. **Review this design** with stakeholders
2. **Start Phase 1**: Grammar parser from specs
3. **Prototype** type-exhaustive generator
4. **Validate** against current test suite
5. **Iterate** based on findings

## Timeline

- **Week 1-2**: Grammar parser + type system
- **Week 3-4**: Core generators
- **Week 5-6**: Smart mutations + coverage
- **Week 7-8**: Integration + optimization
- **Total**: ~2 months to comprehensive fuzzer

## Resources Required

- **Compute**: Same as current (existing fuzzing infrastructure)
- **Storage**: ~10GB for enhanced corpus
- **Time**: Can run indefinitely with CI integration

---

This fuzzer will be the most comprehensive testing tool for Aria, leveraging everything we know about the language to systematically explore the space of possible programs and find bugs before users do.
