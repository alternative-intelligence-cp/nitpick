# Bug Report: std::stoi Overflow in Type Name Parsing

## Summary

The Aria compiler crashes when given type names with numeric suffixes that overflow `int32`, such as `int2147483648`.

## Reproduction

```bash
# Create test file
echo 'func:main = int2147483648() { return 0; };' > /tmp/crash.aria

# Run compiler
./build/ariac /tmp/crash.aria
```

## Error Output

```
terminate called after throwing an instance of 'std::out_of_range'
  what():  stoi
Aborted (core dumped)
```

## Root Cause

The type parser extracts the numeric suffix from type names (e.g., `32` from `int32`) using `std::stoi()`. When the number is larger than `INT_MAX` (2147483647), `std::stoi` throws `std::out_of_range`.

## Impact

- **Severity**: Medium
- **Type**: Uncaught exception causing crash
- **Attack surface**: Any malformed source file with large type numbers

## Recommended Fix

1. Use `std::stoll()` with bounds checking
2. Or catch `std::out_of_range` and emit a proper compiler error
3. Add validation that bit widths are in the expected range (8, 16, 32, 64, 128, 256, 512)

## Example Fix

```cpp
// Before
int bitWidth = std::stoi(typeStr.substr(3));

// After
try {
    long long bitWidth = std::stoll(typeStr.substr(3));
    if (bitWidth > 512 || bitWidth < 1) {
        error("Invalid bit width: " + std::to_string(bitWidth));
    }
} catch (const std::out_of_range&) {
    error("Bit width too large: " + typeStr);
}
```

## Found By

Phase 4.2 Adversarial Fuzzing Campaign
Date: December 28, 2025
Hash: 1d0db42587348c2f
