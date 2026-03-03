# Aria Compiler — Spec Coverage Audit Plan
**Created**: March 2026  
**Compiler commit**: e7d2f49 (544/544 tests pass)  
**Source of truth**: `aria_ecosystem/programming_guide/META/ARIA/SYNTAX_REFERENCE.md` and `aria_ecosystem/programming_guide/` (356 guide files)  
**Test directory**: `tests/`  
**Test runner**: `bash run_comprehensive_tests.sh`

---

## Purpose

For each language feature defined in the spec:

1. **Existence check** — does a test at all exercise this feature?  
2. **Correctness check** — does the test verify the *right* behaviour, not just that it compiles or exits 0?

The plan is ordered by risk: features used by Nikola's self-improvement pipeline come first; exotic research types come last.

---

## Legend

| Symbol | Meaning |
|--------|---------|
| ✅ | Has tests; correctness likely OK |
| ⚠️ | Test exists but coverage is thin or correctness unverified |
| ❌ | No targeted test — existence unknown |
| 🔄 | Scheduled in this plan |

---

## Category 1 — Primitives (Scalar Types)

These are the foundation. Every numeric test indirectly covers them, but explicit
type-specific tests for literal parsing, overflow, and edge values are needed.

| Feature | Existing tests | Status | Action needed |
|---------|---------------|--------|---------------|
| `int8` literals & overflow | `int8_basic`, `int8_simple` | ✅ | Verify min/max edge (-128, 127) |
| `int16` | `small_int_types`, `numeric_types_*` | ⚠️ | Explicit min/max test |
| `int32` | `math_utilities`, general use | ✅ | Verify suffix `i32` required |
| `int64` | heavy use throughout | ✅ | Confirm `-9223372036854775808` parses |
| `uint8`–`uint64` | `uint_token_test`, scattered | ⚠️ | Dedicated uint edge tests |
| `bool` | `test_bool_*`, `test_bool_negation` | ✅ | Check `!`, `&&`, `\|\|` combined |
| `flt32` | `flt64_*` plus float tests | ⚠️ | flt32-specific test |
| `flt64` | `flt64_add/compare/eq/lt` | ✅ | NaN/Inf handling test |
| `flt128` | `flt128_basic`, `flt128_arith` | ✅ | Basic 4-operation coverage |
| `flt256`, `flt512` | `flt256_basic`, `flt512_basic` | ⚠️ | Operations beyond construction |
| Type suffixes (`5i32`, `3.14f64`) | `phase4_typed_literals`, `literal_parsing_test` | ✅ | Cross-type suffix rejection |

**Plan test files to write:**
- `tests/primitives/int_edge_values.aria` — min/max for int8/int16/int32/int64 and underflow/overflow clamping
- `tests/primitives/uint_edge_values.aria` — 0-wrap and max for uint8-uint64
- `tests/primitives/float_special.aria` — inf, large values, flt32 vs flt64 precision

---

## Category 2 — Large & Exotic Integers

| Feature | Existing tests | Status | Action needed |
|---------|---------------|--------|---------------|
| `int128`, `uint128` | `int128_basic`, `uint128_basic` | ✅ | Verify multi-limb add/mul |
| `int256`, `uint256` | `int256_basic` | ⚠️ | Operations + overflow |
| `int512`, `uint512` | `int512_basic` | ⚠️ | Same |
| `int1024`, `uint1024` | `int1024_basic`, `uint1024_basic` | ✅ | Comparison ops |
| `int2048` | `int2048_basic/comprehensive` | ✅ | Good |
| `int4096` | `int4096_basic/div_test/eq_test` | ✅ | Division edge cases |
| TBB types (`tbb8`–`tbb64`) | `tbb_basic/overflow/division` | ✅ | ERR sentinel propagation |
| TBB sticky error | `tbb_sticky_error` | ✅ | Good |
| Ternary (`trit`, `tryte`) | `ternary_basic/comprehensive`, `balanced_ternary_basic` | ✅ | Logic ops |
| Nonary (`nit`, `nyte`) | `nonary_basic`, `test_nit_minimal` | ⚠️ | Arithmetic verification |
| `fix256` (Q128.128) | none | ❌ | **Create `tests/fix256_basic.aria`** |
| `frac8`–`frac64` | `debug_fractions` | ⚠️ | Operations (add, mul, reduce) |
| `tfp32`, `tfp64` | none | ❌ | **Create `tests/tfp_basic.aria`** |

**Plan test files to write:**
- `tests/fix256_basic.aria` — Q128.128 construction, add, multiply
- `tests/tfp_basic.aria` — tfp32 and tfp64 construction and comparison
- `tests/frac_operations.aria` — add/subtract/multiply fractions, GCD reduction

---

## Category 3 — String & NIL

| Feature | Existing tests | Status | Action needed |
|---------|---------------|--------|---------------|
| `string` literals | `string_comprehensive`, `string_executable` | ✅ | Good |
| String concat (`+`) | `test_string_concat_minimal` | ✅ | Multi-concat |
| String comparison | `test_conditional_string` | ⚠️ | All 6 comparison ops |
| Template literals ( `` ` `` `&{expr}`) | `template_basic/integers/floats/mixed/bools` | ✅ | Nested template |
| String functions (`length`, `substring`) | `test_string_functions` | ⚠️ | Return type correctness |
| `NIL` (unit type) | `nil_basic`, `nil_minimal` | ✅ | `failsafe` return type |
| `NULL` (nullable) | `nil_vs_null`, `test_null_coalesce` | ✅ | Nullable struct field |
| `?:` (null coalesce) | `null_coalesce_test`, `nil_coalesce_debug` | ✅ | Chained coalescence |

---

## Category 4 — Fixed-Size Arrays

| Feature | Existing tests | Status | Action needed |
|---------|---------------|--------|---------------|
| Local array declaration | `debug_array`, `const_array_test` | ✅ | Good |
| Zero-init literal | `debug_array` | ✅ | Good |
| Non-zero literal init | `array_param` (indirect) | ✅ | Exhaustive init check |
| Element read `arr[i]` | fixed by last session | ✅ | Good |
| Element write `arr[i] = v` | `debug_array` | ✅ | Good |
| Array by-value parameter | `array_param` | ✅ | **Just fixed — new test** |
| Pass-by-value isolation | `array_param` test 3 | ✅ | Good |
| Array slice `arr[a..b]` | `array_slice`, `array_slice_simple` | ⚠️ | Exclusive vs inclusive range |
| Global arrays | `test_global_array_workarounds`, `const_array_test` | ⚠️ | Write + read in multiple functions |
| Arrays inside structs | `META/ARIA/P0.1_ARRAYS_IN_STRUCTS_COMPLETE.md` | ⚠️ | Need runtime correctness test |
| Dynamic arrays `int32[]` | minimal coverage | ❌ | **Create `tests/dynamic_array.aria`** |

**Plan test files to write:**
- `tests/array_in_struct.aria` — struct with fixed array field; write then read back
- `tests/dynamic_array.aria` — heap-based array via `int32[]`, append, length

---

## Category 5 — Structs & Generics

| Feature | Existing tests | Status | Action needed |
|---------|---------------|--------|---------------|
| Struct declaration `%STRUCT` | `generic_struct_basic`, `test_struct_param` | ✅ | Good |
| Struct literal `Name { field: val }` | `struct_literal_comprehensive` | ✅ | Good |
| Field access `.field` | `test_struct_param`, arrow tests | ✅ | Read + write |
| Nested structs | none | ❌ | **Create `tests/nested_struct.aria`** |
| Struct with array field | none | ❌ | Merge with `array_in_struct.aria` |
| Generic struct `%STRUCT<T>` | `generic_struct_basic` | ⚠️ | Two different T instantiations |
| Struct pointer `@Struct` | `pointer_syntax_test`, `test_struct_param` | ⚠️ | Member access via `->` |
| Arrow operator `ptr->field` | `arrow_operator_full_test` | ✅ | Good |
| Static members | `debug_static_member` | ⚠️ | Verify counter semantics |
| `impl` blocks (UFCS methods) | scattered | ⚠️ | End-to-end method call |
| `obj` type (anonymous struct) | none direct | ❌ | **Create `tests/obj_type.aria`** |

**Plan test files to write:**
- `tests/nested_struct.aria` — struct containing another struct + nested field access
- `tests/struct_impl_method.aria` — impl block, `self` param, method call
- `tests/obj_type.aria` — anonymous struct construction and field access

---

## Category 6 — Result<T> & Error Handling

| Feature | Existing tests | Status | Action needed |
|---------|---------------|--------|---------------|
| `pass(v)` | universal use | ✅ | Good |
| `fail(e)` | `test_pass_fail`, `test_phase2_merge_fail` | ✅ | Good |
| `result ? fallback` (unwrap-or) | `test_unwrap_*` | ✅ | Good |
| `result.err != NULL` check | `test_result_*` | ✅ | Good |
| `Result<T>` enforcement (must check) | `test_result_enforcement_*` | ✅ | Good |
| `fail` propagation pattern | `test_phase2_merge_fail` | ⚠️ | Chain 3+ levels |
| `Result<T>` as function param | `test_result_param_*` | ✅ | Good |
| `failsafe` function | `test_failsafe_*` | ✅ | Good |
| `failsafe` invocation (unhandled err) | `test_failsafe_basic` | ⚠️ | Confirm failsafe fires |
| `? :` (emphatic unwrap) | `test_emphatic_*` | ⚠️ | Failure path |

---

## Category 7 — Functions

| Feature | Existing tests | Status | Action needed |
|---------|---------------|--------|---------------|
| Basic function declaration | universal | ✅ | Good |
| Multi-param function | `math_utilities`, many | ✅ | Good |
| No-param function | `test_simple_return` | ✅ | Good |
| Self-recursion | `math_utilities` (Fibonacci) | ✅ | Fixed last session |
| Mutual recursion | none | ❌ | **Create `tests/mutual_recursion.aria`** |
| Generic function `func:f = T(T:x)` | `debug_generic_lex` | ⚠️ | Full round-trip test |
| Higher-order function (pass lambda) | `test_phase4_*` (indirect) | ⚠️ | Explicit HO test |
| Function returning function | `closures` | ⚠️ | Verify return type |
| `failsafe` required at module level | `test_no_failsafe` | ✅ | Good |
| Contracts (`requires`) | `test_phase4_param_checked` | ⚠️ | Violation triggers failsafe |
| Varargs | not in spec | — | Skip |

**Plan test files to write:**
- `tests/mutual_recursion.aria` — f calls g calls f, terminates correctly
- `tests/generic_function.aria` — generic add, identity, max for int32 and flt64

---

## Category 8 — Lambdas & Closures

| Feature | Existing tests | Status | Action needed |
|---------|---------------|--------|---------------|
| Anonymous function literal | tests only mention them but don't isolate | ⚠️ | Explicit test |
| Lambda as variable | none | ❌ | **Create `tests/lambda_variable.aria`** |
| Lambda as function argument | none targeting this directly | ❌ | See filter/map |
| Closure over outer variable | `closure_capture` (in subdirs) | ⚠️ | Verify captured value |
| Returning a lambda | none | ❌ | **Create `tests/lambda_return.aria`** |
| `filter` stdlib with lambda | none standalone | ❌ | **Create `tests/stdlib_filter.aria`** |
| `transform` stdlib | none | ❌ | **Create `tests/stdlib_transform.aria`** |
| `reduce` stdlib | none | ❌ | **Create `tests/stdlib_reduce.aria`** |

**Plan test files to write (high priority — lambdas are central to functional idioms):**
- `tests/lambda_variable.aria` — assign lambda to variable, call it
- `tests/lambda_argument.aria` — pass inline lambda to a function that calls it
- `tests/lambda_capture.aria` — lambda captures local variable; verify correct value
- `tests/stdlib_filter.aria` — filter array of ints using predicate lambda
- `tests/stdlib_reduce.aria` — reduce to sum, product

---

## Category 9 — Control Flow

| Feature | Existing tests | Status | Action needed |
|---------|---------------|--------|---------------|
| `if/then/end` | `test_phase2_basic_if` | ✅ | Good |
| `if/then/else/end` | `test_phase2_else_branch` | ✅ | Good |
| Nested if | `test_phase3_nested_if` | ✅ | Good |
| `while` loop | `test_phase3_basic_while` | ✅ | Good |
| `while` exit condition | `test_phase3_while_exit_knowledge` | ✅ | Good |
| `till(n, step)` loop with `$` | `for_range`, `for_range_typed` | ⚠️ | Verify step value in `$` |
| `till` with variable bounds | none | ❌ | **Create `tests/till_variable_bound.aria`** |
| `loopback` (reverse till) | none | ❌ | **Create `tests/loopback_basic.aria`** |
| `break` | `test_phase3_5_break` | ✅ | Good |
| `continue` | none | ❌ | **Create `tests/continue_basic.aria`** |
| `fall` (allow fall-through in pick) | `fall_statement_test` | ✅ | Good |
| `pick/when` | `test_pick_*` | ✅ | Good |
| `pick` unknown | `test_pick_unknown*` | ✅ | Good |
| Nested loops + break | none | ❌ | **Create `tests/nested_loop_break.aria`** |

**Plan test files to write:**
- `tests/till_variable_bound.aria` — `till(n, 1)` where `n` is a variable
- `tests/loopback_basic.aria` — reverse iteration, verify count-down
- `tests/continue_basic.aria` — skip even elements with `continue`
- `tests/nested_loop_break.aria` — break inner loop only, outer continues

---

## Category 10 — Operators

| Feature | Existing tests | Status | Action needed |
|---------|---------------|--------|---------------|
| Arithmetic `+`, `-`, `*`, `/`, `%` | `math_utilities`, `test_precedence` | ✅ | Good |
| Overflow clamping (safe add) | `test_overflow_safety` | ✅ | Good |
| Integer division truncation | `test_divide_by_zero_safety` | ✅ | Good |
| Bitwise `~`, `&`, `\|`, `^`, `<<`, `>>` | `test_bitwise_not`, some | ⚠️ | All 6 operators with values |
| Comparison `==`, `!=`, `<`, `>`, `<=`, `>=` | `flt64_compare_all`, `literal_compare` | ✅ | Good |
| Spaceship `<=>` | `spaceship_basic/comprehensive` | ✅ | Good |
| Logical `&&`, `\|\|`, `!` | `test_bool_*` | ✅ | Good |
| Assignment `=`, `+=`, `-=`, `*=`, etc. | scattered | ⚠️ | Compound assignment explicit test |
| Pipeline `\|>` | none | ❌ | **Create `tests/pipeline_operator.aria`** |
| Address-of `@` | `address_of_test` | ✅ | Good |
| Increment/decrement `++`, `--` | not in spec as operators; use `+= 1` | — | Skip |
| Range `..`, `...` | `range_basic`, `range_expressions` | ✅ | Good |
| Type cast | `debug_typecast`, `cast/` | ⚠️ | Cross-type correctness |

**Plan test files to write:**
- `tests/bitwise_comprehensive.aria` — test all 6 bitwise ops with known values
- `tests/compound_assign.aria` — `+=`, `-=`, `*=`, `/=`, `%=`
- `tests/pipeline_operator.aria` — chain 3+ operations with `\|>`

---

## Category 11 — Pointers & Memory

| Feature | Existing tests | Status | Action needed |
|---------|---------------|--------|---------------|
| Pointer syntax `Type@:name` | `pointer_syntax_test`, `simple_pointer_test` | ✅ | Good |
| Pointer creation `@value` | `address_of_test` | ✅ | Good |
| Dereference `*ptr` | `pointer_syntax_test` | ⚠️ | Verify dereferenced value |
| Null pointer `NULL` | `test_simple_wild_global` | ⚠️ | Null-check before deref |
| Arrow `ptr->field` | `arrow_operator_full_test` | ✅ | Good |
| Borrow semantics | `borrow_basic/lifetime/pinning/wild` | ⚠️ | Runtime correctness hard to test |
| Move semantics | `move_basic_test/comprehensive_test/use_after_move` | ✅ | Good |
| Wild pointer | `borrow_wild`, `test_wild_result` | ⚠️ | Verify wild pointer use pattern |
| `Handle<T>` arena reference | none | ❌ | **Create `tests/handle_basic.aria`** |

**Plan test files to write:**
- `tests/pointer_roundtrip.aria` — create pointer, dereference, verify value
- `tests/handle_basic.aria` — allocate via Handle<T>, access field, release

---

## Category 12 — Async & Concurrency

| Feature | Existing tests | Status | Action needed |
|---------|---------------|--------|---------------|
| `async:name` function declaration | none | ❌ | **Create `tests/async_basic.aria`** |
| `await expr` | none | ❌ | See above |
| `atomic<T>` | `test_atomic_*`, `atomic_minimal` | ✅ | Good |
| `atomic<T>` compare-exchange | `test_atomic_operations` | ⚠️ | Verify CAS return |
| Thread safety (stress) | `stress_test_atomic` | ✅ | Good |

**Plan test files to write:**
- `tests/async_basic.aria` — declare async function, await it in main (if runtime supports)
- Note: async requires runtime coroutine infrastructure; test compile-only first with `-c`

---

## Category 13 — Modules & Imports

| Feature | Existing tests | Status | Action needed |
|---------|---------------|--------|---------------|
| `import` declaration | `test_module_import_fix`, `import_mem` | ⚠️ | Verify symbol resolution |
| Cross-module function call | `test_turbofish_module` | ⚠️ | Two-file compile |
| Module-scoped globals | none | ❌ | **Create `tests/modules/module_globals.aria`** |
| Stdmodule `aria.io` | `file_io_*`, `io_types_basic` | ⚠️ | Read + write confirmed output |

---

## Category 14 — Vectors & SIMD

| Feature | Existing tests | Status | Action needed |
|---------|---------------|--------|---------------|
| `vec2` | `vec2_basic/arithmetic/compare/index` | ✅ | Good |
| `vec3` | none | ❌ | **Create `tests/vec3_basic.aria`** |
| `vec9` | none | ❌ | **Create `tests/vec9_basic.aria`** |
| `simd<T,N>` | none | ❌ | **Create `tests/simd_basic.aria`** |
| `dvec2`, `fvec2`, `ivec2` | none | ❌ | **Create `tests/vec_typed.aria`** |

---

## Category 15 — Complex, Quantum, Handle

| Feature | Existing tests | Status | Action needed |
|---------|---------------|--------|---------------|
| `complex<T>` | `test_complex_numbers`, `test_complex_simple` | ✅ | Add subtraction/magnitude |
| `Q3` / `Q9` / `Q21` types | `test_q3_*`, `test_q9_*`, `test_q21_simple` | ✅ | Good |
| `Handle<T>` | none | ❌ | See Category 11 |
| `dbug` facility | `test_dbug*` | ⚠️ | Verify watch + counter output |

---

## Category 16 — FFI & External

| Feature | Existing tests | Status | Action needed |
|---------|---------------|--------|---------------|
| `extern` function declaration | `test_extern_raw_return` | ⚠️ | Call C stdlib function |
| FFI safety wrapper | `ffi_safety_test` | ⚠️ | Verify error propagation |
| GPU/PTX compilation | compile-only tests | ⚠️ | Requires NVCC; skip until GPU available |

---

## Category 17 — Standard Library

| Feature | Existing tests | Status | Action needed |
|---------|---------------|--------|---------------|
| `print(value)` | `print_comprehensive`, `test_print_simple` | ✅ | Good |
| `println(value)` | `test_println`, `int2048_println_test` | ✅ | Good |
| `filter(arr, pred)` | none standalone | ❌ | See Category 8 lambdas |
| `transform(arr, fn)` | none standalone | ❌ | See Category 8 lambdas |
| `reduce(arr, init, fn)` | none standalone | ❌ | See Category 8 lambdas |
| `hashmap` | `test_hashmap_basic/turbofish` | ⚠️ | Insert + lookup + miss |
| File I/O read | `file_io_ultra_simple` | ⚠️ | Verify content read |
| File I/O write | `file_io_basic` | ⚠️ | Verify file created |
| `sort(arr)` | none | ❌ | **Create `tests/stdlib_sort.aria`** |
| `length(arr)` / `.length` | scattered | ⚠️ | Explicit correctness |
| `toString(value)` | `test_simple_toString` | ✅ | Good |

---

## Category 18 — Const & Compile-Time

| Feature | Existing tests | Status | Action needed |
|---------|---------------|--------|---------------|
| `const:name = expr` | `const_test/arith/advanced` | ✅ | Good |
| Const arrays | `const_array_test` | ✅ | Good |
| Const in function | `const_value_test` | ✅ | Good |
| Const propagation (folds) | `const_expr_test` | ⚠️ | Verify actual fold at compile time |

---

## Execution Order (Priority Ranking)

### Phase A — Foundation Safety (do first, affects everything)
1. `tests/primitives/int_edge_values.aria` — edge value correctness
2. `tests/primitives/uint_edge_values.aria`
3. `tests/array_in_struct.aria` — arrays in structs runtime correctness
4. `tests/nested_struct.aria` — nested field access
5. `tests/struct_impl_method.aria` — UFCS `impl` method call

### Phase B — Functional Programming Core
6. `tests/lambda_variable.aria`
7. `tests/lambda_argument.aria`
8. `tests/lambda_capture.aria`
9. `tests/stdlib_filter.aria`
10. `tests/stdlib_reduce.aria`
11. `tests/generic_function.aria`
12. `tests/mutual_recursion.aria`

### Phase C — Control Flow Completeness
13. `tests/till_variable_bound.aria`
14. `tests/loopback_basic.aria`
15. `tests/continue_basic.aria`
16. `tests/nested_loop_break.aria`

### Phase D — Operators & Expressions
17. `tests/bitwise_comprehensive.aria`
18. `tests/compound_assign.aria`
19. `tests/pipeline_operator.aria`
20. `tests/pointer_roundtrip.aria`

### Phase E — Vectors & Specialised Types
21. `tests/vec3_basic.aria`
22. `tests/simd_basic.aria`
23. `tests/fix256_basic.aria`
24. `tests/frac_operations.aria`
25. `tests/tfp_basic.aria`

### Phase F — Infrastructure & Advanced
26. `tests/async_basic.aria` (compile-only first)
27. `tests/handle_basic.aria`
28. `tests/stdlib_sort.aria`
29. `tests/modules/module_globals.aria`

---

## Correctness Verification Standard

A test counts as **correct** only when it satisfies ALL of:

1. **Compiles** — `ariac file.aria -o binary` exits 0  
2. **Runs** — binary exits 0  
3. **Asserts specific values** — uses `if (actual != expected) { pass(error_code); }` style  
4. **Tests failure path** — where applicable, also tests what happens with bad input  
5. **exit code 0 = all assertions passed** — NO test that only checks "doesn't crash"

The `math_utilities.aria` test (37 assertions, all paths) is the **gold standard** for correctness tests.

---

## Running the Audit

```bash
# Compile + run a specific test and check exit code:
./build/ariac tests/FEATURE.aria -o /tmp/feat && /tmp/feat; echo "exit $?"

# Run full suite:
bash run_comprehensive_tests.sh 2>/dev/null | tail -10
```

Track progress by adding completed tests to the suite and keeping pass rate at 100%.

---

*Next step: begin Phase A tests. Each `.aria` test file should be self-contained,
have a comment at the top listing what it checks, and use explicit exit codes
(non-zero = the assertion number that failed) so failures are instantly actionable.*
