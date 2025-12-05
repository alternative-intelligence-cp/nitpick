# Aria Training Samples

**13 production-quality Aria programs for training Nikola AGI**

All samples compile cleanly with `ariac` v0.0.6 and execute successfully with `lli`.

## Samples

1. **01_arithmetic.aria** - Basic arithmetic and function calls
2. **02_pick_fall.aria** - Pattern matching with labeled cases
3. **03_factorial.aria** - While loops and iterative algorithms
4. **04_fibonacci.aria** - Fibonacci sequence (iterative)
5. **05_gcd.aria** - Greatest common divisor algorithm
6. **06_power.aria** - Integer exponentiation
7. **07_prime.aria** - Prime number checker
8. **08_closures.aria** - Module-scope globals and closures
9. **09_error_handling.aria** - Result type error handling
10. **10_composition.aria** - Nested function calls and composition
11. **11_recursive_fib.aria** - Recursive Fibonacci (classic recursion)
12. **12_recursive_factorial.aria** - Recursive factorial
13. **13_ackermann.aria** - Ackermann function (deeply recursive)

## Testing

```bash
cd /home/randy/._____RANDY_____/REPOS/aria
for file in training_samples/*.aria; do
    echo "Testing $(basename $file)..."
    ./build/ariac "$file" || exit 1
done
echo "All samples compile successfully!"
```

## Features Demonstrated

- ✅ Function declarations with Result type auto-wrapping
- ✅ **Recursive function calls** (self-calls work correctly)
- ✅ Pattern matching (pick/fall with labeled cases)
- ✅ While loops
- ✅ If/else conditionals
- ✅ Module-scope global variables
- ✅ Closures (functions accessing globals)
- ✅ Result type error handling
- ✅ Function composition
- ✅ Arithmetic operators (+, -, *, /, %)
- ✅ Comparison operators (<, >, <=, >=, ==, !=)
- ✅ Logical operators (&&, ||, !)

## Notes

- All variables must be declared at module scope or as function parameters
- Variable initialization with assignment (`int64:x = 5;`) inside functions not yet supported
- Use separate declaration and assignment:
  ```aria
  int64:x;
  x = 5;
  ```
- ✅ **Recursive calls NOW WORK** (fixed December 5, 2025)
- Structs and pointers not yet implemented

## Generated for Nikola

These samples serve as training data for the Nikola AGI, demonstrating:
- Real-world algorithms implemented in Aria
- Recursive and iterative solutions
- Proper error handling patterns
- Idiomatic Aria code style
- All working v0.0.6 language features

**Status:** ✅ All 13 samples compile and execute successfully (December 5, 2025)
