# Bug Report: SEGFAULT with deeply nested statements

## Summary

The Aria compiler crashes with SEGFAULT when processing deeply nested if statements (90+ levels).

## Reproduction

```bash
# Create deeply nested code
echo 'func:main = int32() {' > /tmp/crash.aria
for i in $(seq 1 100); do echo 'if (true) {' >> /tmp/crash.aria; done
echo 'return 0;' >> /tmp/crash.aria
for i in $(seq 1 100); do echo '}' >> /tmp/crash.aria; done
echo '};' >> /tmp/crash.aria

./build/ariac /tmp/crash.aria
```

## Impact

- **Severity**: High (SEGFAULT)
- **Type**: Stack overflow during recursive AST processing
- **Attack surface**: Maliciously crafted source files

## Root Cause

Recursive descent parsing or AST traversal uses the call stack for nesting. Deep nesting exceeds stack limits, causing:
- Stack overflow
- SEGFAULT when accessing beyond stack bounds

## Recommended Fix

1. Add a nesting depth limit (e.g., max 256 levels)
2. Use iterative traversal with explicit stack for deep structures
3. Increase stack size for compiler (workaround)

## Found By

Phase 4.2 Adversarial Fuzzing Campaign
Date: December 28, 2025
Hash: faa1da690b748017
