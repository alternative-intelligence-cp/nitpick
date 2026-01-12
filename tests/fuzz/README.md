# Phase 4.2: Adversarial Fuzzing

## Overview

This framework performs mutation-based fuzzing of the Aria compiler to find:

1. **Crashes** - Segfaults, aborts, assertion failures
2. **Hangs** - Infinite loops, excessive resource usage
3. **ICE** - Internal Compiler Errors
4. **Memory Issues** - ASAN/UBSAN violations

## Quick Start

```bash
# Quick test (5 minutes)
python3 aria_fuzzer.py --quick

# Standard campaign (72 hours)
./run_campaign.sh

# Custom duration (24 hours)
./run_campaign.sh 24
```

## Directory Structure

```
fuzz/
├── aria_fuzzer.py     # Main fuzzer
├── run_campaign.sh    # Campaign runner
├── corpus/            # Seed inputs
│   ├── seed_minimal.aria
│   ├── seed_variables.aria
│   └── ...
├── crashes/           # Crash-inducing inputs
│   ├── segfault/
│   ├── abort/
│   ├── timeout/
│   └── ...
├── coverage/          # Coverage data
└── mutations/         # Mutation history
```

## Fuzzing Strategies

### Mutation-Based

The fuzzer applies random mutations to valid programs:
- Character deletion/insertion/replacement
- Line duplication/deletion/swapping
- Keyword/type replacement
- Number boundary testing
- Unicode injection
- Deep nesting insertion
- Long identifier generation

### Generation-Based

Random program generation from grammar:
- Minimal programs
- Variable declarations
- Loops and conditionals
- Memory allocation
- Function definitions
- Adversarial edge cases

## Crash Classification

| Type | Description |
|------|-------------|
| SEGFAULT | Segmentation fault (SIGSEGV) |
| ABORT | Abort signal (SIGABRT) |
| TIMEOUT | Compilation took too long |
| ASSERTION | Assertion failure |
| ICE | Internal Compiler Error |
| ASAN | AddressSanitizer violation |
| UBSAN | Undefined Behavior Sanitizer |
| OOM | Out of Memory |

## Campaign Goals

For v0.1.0 release validation:

| Metric | Target |
|--------|--------|
| Duration | 72+ hours |
| Unique crashes | 0 |
| Timeouts | < 0.1% |
| Exec/sec | > 50 |

## Reproducing Crashes

Each crash is saved with its input and metadata:

```bash
# View crash input
cat crashes/segfault/abc123.aria

# View crash metadata
cat crashes/segfault/abc123.json

# Reproduce
./build/ariac crashes/segfault/abc123.aria
```

## ASAN/UBSAN Testing

For more thorough testing, build with sanitizers:

```bash
cd build
cmake -DCMAKE_BUILD_TYPE=Debug -DUSE_ASAN=ON ..
make
```

Then run fuzzing with the sanitized compiler.

## Adding Seeds

Add new seed files to `corpus/` to improve fuzzing:

```aria
// corpus/seed_myfeature.aria
func:main = int32() {
    // Test your feature here
    return 0;
};
```

## Interpreting Results

After a campaign:

1. Check `crashes/` for any crash-inducing inputs
2. Review `campaign_stats.json` for summary
3. Triage unique crashes by type
4. Create bug reports for reproducible issues

## Integration with CI

```yaml
- name: Quick Fuzz Test
  run: |
    python3 tests/fuzz/aria_fuzzer.py --quick --seed 12345
```
