# Aria Fuzzer Suite

**Mission Critical: Child Safety**

This fuzzing infrastructure exists for one purpose: to ensure that no bug, no edge case, no bit flip can ever harm a child who trusts the systems built with Aria.

## Philosophy

- **Defense in Depth**: Multiple fuzzing strategies running continuously
- **Zero Tolerance**: Any crash, any undefined behavior, any ERR loss is unacceptable
- **Exhaustive Testing**: We have unlimited time to test. Children's safety is not negotiable.

## Fuzzing Strategies

### Phase 1: Parser Layer (Current)
1. **Grammar Fuzzer** (`grammar_fuzzer.py`) - Generate valid Aria programs
2. **Mutation Fuzzer** (`mutation_fuzzer.py`) - Mutate existing tests
3. **Boundary Fuzzer** (`boundary_fuzzer.py`) - Numeric limits, TBB edge cases
4. **Adversarial Fuzzer** (`adversarial_fuzzer.py`) - Malformed inputs, stress cases

### Phase 2: Semantic Layer (Future)
5. Type system fuzzing
6. Memory safety fuzzing
7. ERR propagation verification

### Phase 3: Runtime Layer (Future)
8. Bit-flip simulation (cosmic rays, hardware faults)
9. Long-running stability tests
10. Formal verification

## Usage

```bash
# Run all fuzzers
./run_all.sh

# Run specific fuzzer
python3 grammar_fuzzer.py --iterations 10000
python3 mutation_fuzzer.py --test-dir ../tests
python3 boundary_fuzzer.py --focus tbb

# Continuous fuzzing (never stops)
./continuous_fuzz.sh
```

## Success Criteria

- **Parser**: 0 crashes on valid input, meaningful errors on invalid input
- **Semantic**: 0 type violations slip through
- **Runtime**: 0 memory safety violations, 100% ERR propagation

## Remember

Every bug we catch is a child we protect.
Every edge case we handle is trust we preserve.
There is no acceptable failure rate when a child's safety is at stake.
