# Aria Fuzzing Quick Reference

## 🚀 Quick Start (Full-Stack Fuzzing)

```bash
# Recommended: 100 programs with full execution (~8 sec)
python3 tests/fuzz/fullstack_fuzzer.py --iterations 100 --execute

# Or use the campaign script with logging
./tests/fuzz/run_fullstack_campaign.sh 200
```

## 📊 What Gets Tested

| Generator | Feature | Example |
|-----------|---------|---------|
| Basic | Minimal programs | `int32:x = 42i32;` |
| P2.7 Structs | Complex initialization | `{count: 3, points: [{x:1, y:2}]}` |
| Collections | Vec, HashMap | `vec_push(@numbers, 10i32)` |
| Integer Math | All operators | `+`, `-`, `*`, `/`, `%`, `&`, `\|`, `^`, `<<`, `>>` |
| Async | Future syntax | `async func:f = Future<T>() {...}` |

## 🎯 Which Fuzzer to Use?

| Scenario | Fuzzer | Command |
|----------|--------|---------|
| **Release validation** | Full-Stack | `./tests/fuzz/run_fullstack_campaign.sh 1000 --execute` |
| **Runtime testing** | Full-Stack | `python3 tests/fuzz/fullstack_fuzzer.py --iterations 500 --execute` |
| **Fast parser testing** | V2 | `cd tests/fuzz/v2 && ./run_fuzzer.sh` |
| **Grammar testing** | Grammar | `python3 tools/fuzzer/grammar_fuzzer.py` |

## 📈 Performance

| Mode | Speed | What's Tested |
|------|-------|---------------|
| V2 (legacy) | ~70/sec | Parse + typecheck only |
| Full-Stack (no exec) | ~60/sec | Compile + link |
| Full-Stack (with exec) | ~14/sec | Full pipeline including execution |

## ✅ Success Metrics

**Good**:
```
📊 Tests: 200 | Rate: 14.2/sec | Compile: 70 | Link: 70 | Run: 70
   Errors: C:130 L:0 R:0 | Crash:0 Timeout:0
```
- Crashes = 0 (most important!)
- Compile errors are expected (random programs)
- Link/Runtime errors depend on stdlib implementation

**Bad**:
```
❌ CRASH detected in P2.7 Structs test (iteration 42)
```
- Any crash count > 0 indicates a compiler or runtime bug

## 🔧 Options

```bash
# Full-Stack Fuzzer
python3 tests/fuzz/fullstack_fuzzer.py \
    --iterations 1000         # Number of tests
    --execute                 # Actually run executables (slower but thorough)
    --ariac ./build/ariac     # Compiler path
    --runtime ./build/libaria_runtime.a  # Runtime library

# Campaign Script  
./tests/fuzz/run_fullstack_campaign.sh ITERATIONS [--execute]
```

## 📁 Files

```
tests/fuzz/
├── fullstack_fuzzer.py          ⭐ NEW: Full pipeline testing
├── run_fullstack_campaign.sh    ⭐ Campaign launcher with logging
├── FULLSTACK_FUZZER.md          ⭐ Complete documentation
├── FUZZER_UPDATE_FEB18_2026.md  ⭐ Update summary
├── v2/                          Legacy: Compile-only fuzzer
│   └── README.md
└── fuzz_logs/                   Campaign results
    └── campaign_TIMESTAMP.log

tools/fuzzer/
├── grammar_fuzzer.py            Grammar-based generator (to be updated)
├── mutation_fuzzer.py           Mutation-based fuzzer
└── continuous_fuzz.sh           Multi-fuzzer campaign
```

## 🐛 Troubleshooting

### "Runtime library not found"
```bash
./build.sh  # Rebuild compiler and runtime
ls -lh build/libaria_runtime.a  # Should show ~8.7MB
```

### "All tests compile-error"
This is normal! The fuzzer generates many invalid programs intentionally.  
**Focus on crashes (should be 0), not compile errors.**

### "Low throughput"
Use `--execute` only for thorough testing. Omit it for faster iteration:
```bash
python3 tests/fuzz/fullstack_fuzzer.py --iterations 1000  # No --execute
```

## 📝 Example Session

```bash
$ ./tests/fuzz/run_fullstack_campaign.sh 200

╔════════════════════════════════════════════════════════════════════╗
║         ARIA FULL-STACK FUZZING CAMPAIGN (Linking Enabled)          ║
╚════════════════════════════════════════════════════════════════════╝

✓ Compiler:  ./build/ariac
✓ Runtime:   ./build/libaria_runtime.a (8.7M, Feb 17 11:18)

Configuration:
  - Iterations: 200
  - Mode: Full execution
  - Testing: P2.7 structs, stdlib, async, math

📊 Tests: 200 | Rate: 14.2/sec | Compile: 70 | Link: 70 | Run: 70
   Errors: C:130 L:0 R:0 | Crash:0 Timeout:0

✅ Campaign completed successfully - No compiler or runtime crashes detected
```

## 🔄 Continuous Fuzzing

For long campaigns (overnight/24+ hours):

```bash
# Start in screen session
screen -S aria-fuzz
./tests/fuzz/run_fullstack_campaign.sh 100000 --execute

# Detach: Ctrl+A, D
# Reattach: screen -r aria-fuzz
```

Check progress:
```bash
tail -f fuzz_logs/campaign_*.log
```

## 📚 Documentation

- **Complete Guide**: [FULLSTACK_FUZZER.md](FULLSTACK_FUZZER.md)
- **Update Summary**: [FUZZER_UPDATE_FEB18_2026.md](FUZZER_UPDATE_FEB18_2026.md)
- **Legacy Fuzzer**: [v2/README.md](v2/README.md)

## 🎯 Next Steps

After stdlib audit completion (Feb 17), recommended validation:

```bash
# Short validation (~1 minute)
./tests/fuzz/run_fullstack_campaign.sh 500 --execute

# Full validation (~30 minutes)
./tests/fuzz/run_fullstack_campaign.sh 10000 --execute

# Release campaign (overnight)
screen -S aria-fuzz
./tests/fuzz/run_fullstack_campaign.sh 100000 --execute
```

---

**Status**: ✅ Full-stack fuzzing enabled (Feb 18, 2026)  
**Runtime**: libaria_runtime.a (8.7MB, stable)  
**Validated**: 200 iterations, 0 crashes
