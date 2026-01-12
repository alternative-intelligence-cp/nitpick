# Bug Report: SEGFAULT with int128 return type on main

## Summary

The Aria compiler crashes with SEGFAULT when `main` returns `int128` instead of `int32`.

## Reproduction

```bash
echo 'func:main = int128() { return 0; };' > /tmp/crash.aria
./build/ariac /tmp/crash.aria
```

## Impact

- **Severity**: High (SEGFAULT)
- **Type**: Null pointer dereference or invalid memory access
- **Attack surface**: Any malformed source file

## Root Cause (Hypothesis)

The runtime or codegen likely assumes `main` returns a 32-bit or 64-bit value. A 128-bit return type may cause:
- Invalid stack layout
- Missing 128-bit return value handling
- Null pointer in type lookup

## Recommended Fix

1. Validate that `main` returns `int32` during semantic analysis
2. Or properly handle arbitrary return types for `main`

## Found By

Phase 4.2 Adversarial Fuzzing Campaign
Date: December 28, 2025
Hash: a1d99c9d33c28804
