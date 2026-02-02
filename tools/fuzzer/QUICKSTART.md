# Fuzzing Quick Start

## Running Tests

### One-Time Fuzzing (Fast Check)
```bash
cd /home/randy/Workspace/REPOS/aria
./tools/fuzzer/run_all.sh
```
This runs:
- 500 random programs (grammar)
- 100 mutations per test file (mutation)  
- Comprehensive boundary testing

Takes ~2-3 minutes. Use before commits.

### Individual Fuzzers

**Grammar (Random Valid Programs):**
```bash
python3 tools/fuzzer/grammar_fuzzer.py --iterations 1000
```

**Mutation (Corrupt Existing Tests):**
```bash
python3 tools/fuzzer/mutation_fuzzer.py --test-dir ./tests --iterations 200
```

**Boundary (Edge Cases):**
```bash
python3 tools/fuzzer/boundary_fuzzer.py --output /tmp/boundary.aria
```

### Continuous Fuzzing (Background Safety Monitor)

Run this in a separate terminal and leave it running:
```bash
./tools/fuzzer/continuous_fuzz.sh
```

This runs indefinitely, logging results to `fuzz_logs/`.

Press Ctrl+C to stop.

## Interpreting Results

### ✅ Success Criteria
- Grammar: 100% of generated programs parse
- Mutation: 0 crashes, 0 timeouts (some errors are good!)
- Boundary: All edge cases parse successfully

### ❌ Failure Modes

**CRITICAL** (Stop everything):
- Parser crashes (segfault)
- Parser hangs (infinite loop)
- Undefined behavior

**WARNING** (Investigate):
- Grammar fuzzer generates unparseable program
- Boundary test fails on valid edge case

**INFORMATION** (Expected):
- Mutation fuzzer gets parse errors (most mutations break syntax)

## What We're Protecting Against

1. **Crashes** → Child's companion dies unexpectedly
2. **Hangs** → Companion becomes unresponsive  
3. **Silent Failures** → Bugs hide until critical moment
4. **Edge Cases** → Overflow/underflow in safety calculations
5. **Bit Flips** → Hardware faults cause behavior changes

## Files

- `grammar_fuzzer.py` - Generate random valid programs
- `mutation_fuzzer.py` - Corrupt existing programs
- `boundary_fuzzer.py` - Test numeric limits
- `run_all.sh` - Run all three fuzzers once
- `continuous_fuzz.sh` - Run fuzzers in infinite loop
- `FINDINGS.md` - Current test results and analysis
- `README.md` - Philosophy and detailed documentation

## Remember

Every bug we catch is a child we protect.

This isn't academic testing - it's building trust infrastructure for vulnerable children who will depend on systems built with Aria.

No acceptable failure rate. Test until certain.
