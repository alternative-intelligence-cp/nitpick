# Aria Full-Stack Fuzzer

## Overview

The full-stack fuzzer tests the **complete Aria compilation pipeline**, including:

1. **Parsing** → AST generation
2. **Type checking** → Semantic validation
3. **Code generation** → LLVM IR
4. **Linking** → Executable with `libaria_runtime.a`
5. **Execution** → Runtime behavior validation

This replaces the previous compile-only approach now that the runtime library is stable.

## Status: Runtime Linking Re-Enabled ✅

**Update (Feb 2026)**: The runtime library (`libaria_runtime.a`, 8.7MB) is now production-ready. 
The fuzzer has been updated to test linking and execution, not just compilation.

### What Changed

**Before** (~2 weeks ago):
- Runtime library wasn't always available
- Fuzzer tested parse/typecheck/codegen only
- Used `--ast-dump` flag for fast iteration
- Linking failures were expected and ignored

**Now**:
- Runtime library exists and is stable (8.7MB)
- Fuzzer tests complete pipeline including execution
- Links against `libaria_runtime.a` by default
- Catches runtime bugs, not just compiler bugs

## Testing Scope

### Language Features Tested

1. **P2.7: Complex Struct Initialization** (Feb 2026)
   ```aria
   Container:c = {
       count: 3i32,
       points: [{x: 1i32, y: 2i32}, {x: 3i32, y: 4i32}]
   };
   ```

2. **String Interpolation** (recent fixes)

3. **Integer Math**: All operators (+, -, *, /, %, &, |, ^, <<, >>)

4. **Stdlib Collections**:
   - `Vec<T>`: Dynamic arrays
   - `HashMap<K,V>`: Hash tables
   - `HashSet<T>`: Sets

5. **Async/Await** (syntax testing):
   - `Future<T>` types
   - `async` functions
   - `await` expressions

## Usage

### Quick Test (100 iterations, ~8 seconds)

```bash
cd /path/to/aria
python3 tests/fuzz/fullstack_fuzzer.py --iterations 100 --execute
```

### Full Campaign (1000+ iterations)

```bash
./tests/fuzz/run_fullstack_campaign.sh 1000 --execute
```

### Compile+Link Only (no execution)

```bash
python3 tests/fuzz/fullstack_fuzzer.py --iterations 500
```

## Options

```
--iterations N    Number of test programs to generate (default: 1000)
--execute         Actually run the compiled executables
--ariac PATH      Path to ariac compiler (default: ./build/ariac)
--runtime PATH    Path to runtime library (default: ./build/libaria_runtime.a)
```

## Output

### Success Example

```
======================================================================
ARIA FULL-STACK FUZZING CAMPAIGN
======================================================================
Compiler: build/ariac
Runtime:  build/libaria_runtime.a
Iterations: 100
Execute: YES
======================================================================

📊 Tests: 100 | Rate: 12.7/sec | Compile: 39 | Link: 39 | Run: 39
   Errors: C:61 L:0 R:0 | Crash:0 Timeout:0

✅ NO CRASHES - Compiler and runtime stable
```

### Metrics Explained

- **Tests**: Total programs generated
- **Rate**: Tests per second
- **Compile**: Successfully compiled programs
- **Link**: Successfully linked executables
- **Run**: Successfully executed programs
- **Errors**: Expected failures (C=compile, L=link, R=runtime)
- **Crash**: Unexpected crashes (ICE, segfault, etc.)
- **Timeout**: Programs that took too long

## Expected vs. Unexpected Failures

### ✅ Expected (not bugs)

- **Compile errors**: Invalid syntax, type errors (intentional)
- **Link errors**: Missing stdlib functions (not implemented yet)
- Some runtime errors: Depends on test case

### ❌ Unexpected (bugs!)

- **Compiler crashes**: Segfault, assertion failure, ICE
- **Runtime crashes**: Unexpected panics, memory corruption
- **Timeouts**: Infinite loops in compiler (not test program)

## Architecture

### Program Generators

The fuzzer includes several generators:

1. **Basic**: Minimal valid programs
2. **P2.7 Structs**: Complex initialization patterns
3. **String Interp**: Template literal testing
4. **Collections**: Vec/HashMap operations
5. **Async**: Future/await syntax
6. **Integer Math**: Random arithmetic/bitwise ops

Each generator creates syntactically diverse programs to explore edge cases.

### Test Pipeline

```
Generate program
    ↓
Compile to LLVM IR
    ↓
Link with libaria_runtime.a  ← THE KEY DIFFERENCE
    ↓
Execute binary (if --execute)
    ↓
Classify result
```

## Comparison with V2 Fuzzer

| Feature | V2 (legacy) | Full-Stack (new) |
|---------|-------------|------------------|
| Runtime linking | ❌ Skipped | ✅ Enabled |
| Execution | ❌ No | ✅ Optional |
| Stdlib testing | ⚠️ Minimal | ✅ Comprehensive |
| P2.7 syntax | ❌ No | ✅ Yes |
| Speed | Fast (70/sec) | Medium (13/sec) |
| Coverage | Parser+typeck | Full pipeline |

**Use V2 for**: Fast parser/frontend testing  
**Use Full-Stack for**: Release validation, stdlib testing, runtime bugs

## Continuous Fuzzing

For long-running campaigns (24+ hours):

```bash
# Run in screen session
screen -S aria-fuzz
./tests/fuzz/run_fullstack_campaign.sh 100000 --execute
# Ctrl+A, D to detach

# Reattach later
screen -r aria-fuzz
```

Logs are saved to `fuzz_logs/campaign_TIMESTAMP.log`.

## Recent Updates

- **Feb 18, 2026**: Re-enabled runtime linking after stdlib audit
- **Feb 17, 2026**: Added P2.7 struct initialization tests
- **Feb 17, 2026**: Runtime library confirmed stable (8.7MB)

## Troubleshooting

### "Runtime library not found"

```bash
# Rebuild runtime
./build.sh
ls -lh build/libaria_runtime.a  # Should show ~8.7MB
```

### "All tests fail"

Check that basic compilation works:

```bash
echo 'func:failsafe = void(int32:e) {};' > /tmp/test.aria
echo 'func:main = int32() { pass(42i32); };' >> /tmp/test.aria
./build/ariac /tmp/test.aria -o /tmp/test && /tmp/test
```

### High compile error rate

This is normal! The fuzzer generates random/invalid programs intentionally.
Focus on the **crash count** (should be 0) not the error count.

## Future Work

- [ ] Mutation-based fuzzing (modify existing valid programs)
- [ ] Corpus minimization (reduce crash-inducing programs)
- [ ] Coverage-guided fuzzing (AFL/libFuzzer integration)
- [ ] Network/I/O testing (when stdlib adds those features)
- [ ] Multi-threaded execution testing
- [ ] Memory sanitizer integration (ASAN/MSAN)

## See Also

- `tests/fuzz/v2/` - Legacy compile-only fuzzer
- `tests/fuzz/aria_fuzzer.py` - Mutation-based fuzzer (to be updated)
- `tools/fuzzer/grammar_fuzzer.py` - Grammar-based generator (to be updated)
