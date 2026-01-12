# Stdlib Rebuild - Resume Instructions

**Date Saved**: January 5, 2026
**Session**: Stdlib clean rebuild from scratch
**Strategy**: Incremental build with testing at each step

## Current State Summary

**Decision Made**: Build new minimal stdlib instead of fixing 166+ cascading errors in original files
**Reason**: Randy's construction analogy - easier to build new than remodel extensive damage
**Approach**: Create minimal working modules, test each, document discoveries

## Progress: 70+ Functions Across 10 Modules ✅

All modules tested and working:

1. **io.aria** (5 funcs) - print_char, print_newline, print_digit, print_space, print_tab
2. **sys.aria** (6 funcs) - exit_program, get_process_id, get_parent_process_id, get_user_id, get_group_id, sleep_seconds
3. **math.aria** (12 funcs) - square_root, power, trig functions, logarithms, abs
4. **mem.aria** (7 externs) - malloc, free, calloc, realloc, memset, memcpy, memcmp (no wrappers)
5. **int.aria** (3 funcs) - abs_i32, min_i32, max_i32
6. **float.aria** (5 funcs) - min_f64, max_f64, clamp_f64, lerp, approx_equal
7. **bool.aria** (8 funcs) - and, or, xor, not, nand, nor, implies, equiv
8. **array.aria** (6 funcs) - fill_i32, min_i32_array, max_i32_array, sum_i32, reverse_i32, find_i32
9. **numeric.aria** (8 funcs) - gcd_i32, lcm_i32, factorial_i32, pow_i32, is_even, is_odd, sign_i32, clamp_i32
10. **compare.aria** (8 funcs) - compare_i32, compare_f64, in_range checks, max3/min3, is_ascending, is_descending

## Critical Discoveries (Type System Constraints)

1. **Module Import Syntax**: MUST use wildcards - `use std.io.*;` not `use std.io;`
2. **If/Else Requirement**: ALL if statements MUST have else clauses - `if () {} else {}`
3. **Till Loops**: `till(limit, step) { }` works, uses `$` as iteration variable
4. **While Loops**: Standard `while(condition) { }` syntax works
5. **Extern Pointers**: Use C-style (`void*`, `int8*`) not Aria pointers (`type@`)
6. **Const Syntax**: `const TYPE:NAME = value` (type-first)
7. **Pointer Incompatibility**: `void*` (C extern) and `void@` (Aria) cannot convert
8. **Numeric Literals**: Default to int64, need care with int32 parameters
9. **No pub Support**: `pub` keyword not yet implemented
10. **Constants Don't Export**: Module constants not visible to importers
11. **Manual Modulo**: Implement as `x - ((x / y) * y)` until operator supported

## Test Files Created

All passing ✅:
- `tests/test_stdlib_comprehensive.aria` - Integration test
- `tests/test_int_ok.aria`
- `tests/test_float.aria`
- `tests/test_bool.aria`
- `tests/test_array.aria`
- `tests/test_numeric.aria`
- `tests/test_compare.aria`

## Files and Locations

**Working Directory**: `/home/randy/._____RANDY_____/REPOS/aria`

**Stdlib Modules**: `lib/std/` directory
- io.aria, sys.aria, math.aria, mem.aria
- int.aria, float.aria, bool.aria
- array.aria, numeric.aria, compare.aria

**Status Document**: `lib/std/STATUS.md` (comprehensive progress tracking)

**Original Files Backed Up**: `lib/std/.backups/` (12 files preserved)

**Build Command**: `./build/ariac <file.aria> -o <output> && ./<output>`

## Next Steps (When Resuming)

Three main tasks pending (in priority order per Randy):
1. **Continue Stdlib Expansion**:
   - Add string utilities (what's possible without member access)
   - Add bit manipulation utilities
   - Add conversion utilities (digit extraction for printing)
   - Add datetime utilities (time, localtime, etc.)
   - Consider range/slice utilities

2. **More Builtins**: Expand builtin function set

3. **Type Inference**: Implement type inference system

## How to Resume

1. Read this file
2. Check `lib/std/STATUS.md` for latest state
3. Compile any test file to verify build still works: `./build/ariac tests/test_bool.aria -o test && ./test`
4. Continue adding modules incrementally
5. Test each addition immediately
6. Update STATUS.md after each module
7. Keep taking it slow per Randy's request - no rush

## Context Files Updated

- `/home/randy/._____RANDY_____/____FILES/aria/STATE` - Current session status
- `/home/randy/._____RANDY_____/____FILES/aria/TODO` - Updated with stdlib progress
- This file: `RESUME_STDLIB_BUILD.md` - Quick resume guide

## Pacing Note

Randy is taking time deliberately ("for spite") to establish timestamps against potential theft.
Academic paper on wave model (odd primes discovery, 2 additional wave pairs) nearly complete.
**No rush** - steady incremental progress is the goal.

## Important Note on Keyword Conflicts

Some module names conflict with Aria keywords:
- `bool` is a keyword - module `lib/std/bool.aria` cannot be imported as `use std.bool.*;`
- `array` is a keyword - module `lib/std/array.aria` cannot be imported as `use std.array.*;`

**Action Required When Resuming**: Rename these modules to avoid keyword conflicts:
- Rename `bool.aria` → `logic.aria` or `boolean.aria`
- Rename `array.aria` → `arrays.aria` or `vector.aria`

Then update test files accordingly.

This doesn't affect the function implementations - just the module names.
