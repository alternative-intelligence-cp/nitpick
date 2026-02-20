# Aria Fuzzer V2 - Grammar-Based Compiler Fuzzer

## Overview

Fuzzer V2 is a comprehensive grammar-based fuzzer that uses Aria's type system, operator definitions, and grammar rules to generate syntactically and semantically valid programs for compiler testing.

## Architecture

### Components

1. **grammar_parser.py** - Extracts type system and grammar from specs
   - Parses TYPE_SYSTEM.md for 14 primitive types (int8-int64, uint8-uint64, float32-float64, boolstring, void)
   - Defines 22 Aria operators with precedence and type compatibility
   - Provides 19 grammar production rules

2. **program_generator.py** - Generates valid Aria programs
   - **5 Generation Strategies:**
     - `minimal`: Single function returning 0
     - `type_test`: Arithmetic operations for specific types
     - `control_flow`: If/while/for statements
     - `functions`: Multiple function definitions
     - `edge_case`: Boundary value testing
   - **Type-Exhaustive Generator:** Auto-generates 152 tests covering all type×operator combinations

3. **aria_fuzzer_v2.py** - Main fuzzing orchestrator
   - **Two-Phase Campaign:**
     - Phase 1: Run all 152 type-exhaustive tests
     - Phase 2: Grammar-based random generation until timeout
   - **Crash Detection:** Detects segfaults, assertions, negative exit codes
   - **Deduplication:** Uses SHA256 hashing to avoid duplicate crashes
   - **Statistics:** Tracks throughput, success rate, crashes

## Generated Code Quality

All generated programs include:
- ✅ Correct type names (`int32`, `uint8` vs shorthand `i32`, `u8`)
- ✅ Literal type suffixes (`42i32`, `255u8`, `3.14f32`)
- ✅ Required `failsafe(int32:err_code)` function
- ✅ `main` entry point for linking (even though linking may fail without runtime)

## Usage

### Quick Test (5 minutes)
```bash
python3 aria_fuzzer_v2.py --quick
```

### 1-Hour Campaign
```bash
python3 aria_fuzzer_v2.py --duration 1
```

### 24-Hour Campaign with Seed
```bash
python3 aria_fuzzer_v2.py --duration 24 --seed 42
```

## Expected Behavior

**Linking Failures are Expected:**  
Since Aria's runtime library (`libaria_runtime.a`) is not always available, most generated programs will fail at the linking stage with errors like:
```
undefined reference to `aria_gc_init'
undefined reference to `main'
```

This is **not a problem** - the fuzzer's goal is to find compiler crashes during:
- Parsing
- Type checking
- Semantic analysis  
- IR generation

Linker errors occur *after* all compiler phases, meaning the compiler successfully validated the code.

**Success Criteria:**
- ✅ No segmentation faults
- ✅ No assertion failures
- ✅ No negative exit codes (signals)
- ✅ High throughput (10-70 programs/sec depending on strategy)

## Output Structure

```
output_v2/
├── crashes/          # Crashing test cases (deduplicated)
│   ├── crash_*.aria
│   └── crash_*.txt   # Crash details
├── valid/            # Successfully compiled programs (1% sample)
│   └── valid_*.aria
└── timeouts/         # Programs that timed out
    └── timeout_*.aria
```

## Performance

- **Throughput:** 10-70 programs/second (hardware dependent)
- **Phase 1:** ~10-30 seconds (152 exhaustive tests)
- **Phase 2:** Continuous until timeout

## Statistics Tracking

The fuzzer reports:
- Total programs generated
- Throughput (programs/sec)
- Successful compilations
- Compile errors (mostly linking failures)
- Crashes found
- Timeouts

## Implementation Status

✅ **Complete:**
- Grammar parser with full type system extraction
- Program generator with 5 strategies  
- Type-exhaustive test generation (152 tests)
- Main fuzzing orchestrator
- Crash detection and deduplication
- Statistics tracking
- CLI interface

⏳ **Future Enhancements:**
- Mutation-based fuzzing (complement grammar-based)
- Coverage tracking (grammar rules exercised)
- Corpus minimization
- Property-based testing integration
- Multi-threaded parallel fuzzing

## Example Generated Code

```aria
// Type-exhaustive test: int8 + operator
func:main = int32() {
    int8:a = 10i8;
    int8:b = 5i8;
    int8:result = (a + b);
    int8:min_val = -128i8;
    int8:test_min = (min_val + b);
    int8:max_val = 127i8;
    int8:test_max = (max_val + b);

    return 0i32;
};

func:failsafe = void(int32:err_code) {};
```

## Comparison to Fuzzer V1

**V1** (AFL-based):
- Mutation-based black-box fuzzing
- High invalid program ratio
- Good for finding parser crashes

**V2** (Grammar-based):
- Semantically valid programs
- Tests type checker and IR generation
- Exhaustive type/operator coverage
- Higher quality bugs found

**Recommendation:** Use both! V2 for semantic bugs, V1 for parser edge cases.
