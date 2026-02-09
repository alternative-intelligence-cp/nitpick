# trit/nit Implementation Roadmap

**Created**: February 5, 2026  
**Status**: Ready to Begin  
**Timeline**: 11-14 working days  
**Success Criteria**: 363/363 tests passing (batch02 moves from tests/future/)

---

## 📚 Key Documentation References

### Primary Specifications
- **[TRIT_NIT_IMPLEMENTATION_GUIDE.md](TRIT_NIT_IMPLEMENTATION_GUIDE.md)**: Complete implementation guide with critical failure modes
- **[TRIT_NIT_RESEARCH_SUMMARY.md](TRIT_NIT_RESEARCH_SUMMARY.md)**: Full research extraction (~97KB comprehensive spec)
- **[ENGINEERING_PHILOSOPHY.md](ENGINEERING_PHILOSOPHY.md)**: Blueprint thinking principles

### Research Library
- **[/META/RESEARCH/RESEARCH_INDEX.md](/home/randy/Workspace/META/RESEARCH/RESEARCH_INDEX.md)**: Searchable index of 671 research files
  - 659 files on balanced ternary/nonary (trit/nit)
  - 372 files on LLVM backend integration
  - 355 files on type system design
  - 350 files on testing strategies

### Key Audit Documents
- **[Safety Audit: Exotic Types Conversions.md](/home/randy/Workspace/META/RESEARCH/general/Safety Audit_ Exotic Types Conversions.md)**: Critical failures from previous implementation
- **[Balanced Ternary and Nonary Intrinsics.md](/home/randy/Workspace/META/RESEARCH/aria-lang/Balanced Ternary and Nonary Intrinsics.md)**: Complete C++ intrinsic signatures

---

## 🚨 CRITICAL GOTCHAS - READ FIRST

### ⚠️ GOTCHA #1: Backend Bypass Bug
**What Went Wrong Before**:
```cpp
// WRONG - caused True AND False = True
if (exoticType == "tryte") { funcPrefix = "aria_tryte_"; }
else if (exoticType == "nyte") { funcPrefix = "aria_nyte_"; }
else {
    // trit and nit use regular int8 operations (no special runtime needed)
    return nullptr;  // ❌ CATASTROPHIC - bypasses runtime
}
```

**Why It Breaks**:
- LLVM i8 bitwise AND: `0x01 & 0xFF = 0x01` → True AND False = True ❌
- Correct Kleene logic: `min(1, -1) = -1` → True AND False = False ✅

**THE FIX**: **ALWAYS** call runtime functions for ALL exotic types (trit, nit, tryte, nyte)

---

### ⚠️ GOTCHA #2: Sentinel Healing
**What Went Wrong Before**:
```cpp
var x: nit = ERR;  // -128
var y: nit = 1;
var z = x + y;     // Uses LLVM add: -128 + 1 = -127
// z is now garbage, NOT ERR - error state disappeared
```

**Why It Breaks**:
- ERR sentinel (-128) + 1 = -127 (just an invalid scalar)
- Subsequent `z == ERR` checks fail silently
- Program proceeds with corrupted data

**THE FIX**: Sticky ERR propagation MUST be in runtime functions with explicit checks

---

### ⚠️ GOTCHA #3: Error Swallowing
**What Went Wrong Before**:
```cpp
// WRONG - silently masks errors
if (a < NIT_MIN || a > NIT_MAX) {
    return 0;  // ❌ Converts error to "Unknown" (valid value)
}
```

**Why It Breaks**:
- Out-of-bounds input becomes valid 0
- Error detection completely broken

**THE FIX**: **ALWAYS** return ERR sentinel on invalid input, never 0 or neutral value

---

## 📋 Implementation Phases

### Phase 1: Type System Integration ⏱️ 2 days

**Goal**: Add trit/nit/tryte/nyte to Aria's type system and AST

#### Tasks:

- [ ] **1.1**: Add types to Type enum
  - File: `include/ast/type.h`
  - Add: `Trit, Nit, Tryte, Nyte` to enum
  - Verify: Enum values don't conflict with existing types

- [ ] **1.2**: Create shared constants header
  - File: `include/runtime/exotic_constants.h` (NEW)
  - Define: `TRIT_ERR`, `NIT_ERR`, `TRYTE_ERR`, `NYTE_ERR`
  - Define: `TRIT_MIN`, `TRIT_MAX`, `NIT_MIN`, `NIT_MAX`
  - Define: `TRYTE_MIN`, `TRYTE_MAX`, `NYTE_MIN`, `NYTE_MAX`
  - Values: See TRIT_NIT_IMPLEMENTATION_GUIDE.md section "Type Mapping"

- [ ] **1.3**: Update Type class methods
  - File: `src/ast/type.cpp`
  - Update `getLLVMType()`: i8 for trit/nit, i16 for tryte/nyte
  - Update `toString()`: Return "trit", "nit", "tryte", "nyte"
  - Update `isPrimitive()`: Return true for all exotic types
  - Add `isTrit()`, `isNit()`, `isTryte()`, `isNyte()` helper methods

- [ ] **1.4**: Add type checking rules
  - File: `src/semantic/type_checker.cpp`
  - Add: Type compatibility checks for exotic types
  - Add: Explicit conversion requirements (no implicit bin→trit)
  - Reference: ATPM_DESIGN_RATIONALE.md for why conversions must be explicit

- [ ] **1.5**: Add literal parsing
  - File: `src/parser/parser.cpp`
  - Add: Token recognition for `1T`, `-1T`, `0T` (trit literals)
  - Add: Token recognition for `4N`, `-4N`, `0N` (nit literals)
  - Add: Range validation during parsing
  - Error: If trit literal not in {-1, 0, 1}
  - Error: If nit literal not in {-4..4}

- [ ] **1.6**: Test type system integration
  - Create: `tests/type_system_exotic.aria` (minimal test)
  - Test: Variable declarations (`var x: trit = 1T;`)
  - Test: Type checking catches invalid assignments
  - Run: `./build.sh && ./aria_test tests/type_system_exotic.aria`

**Completion Criteria**:
- ✅ All exotic types compile without "unknown type" errors
- ✅ Type checker enforces correct usage
- ✅ Literals parse correctly with range validation

---

### Phase 2: Runtime Library Implementation ⏱️ 3-4 days

**Goal**: Implement all 40+ intrinsic functions with correct semantics

#### Tasks:

- [ ] **2.1**: Create runtime directory structure
  - Create: `lib/runtime/` directory
  - Create: `lib/runtime/exotic_types.c` (main implementation)
  - Create: `lib/runtime/exotic_types.h` (function declarations)
  - Create: `lib/runtime/CMakeLists.txt` (build configuration)

- [ ] **2.2**: Initialize LUT tables
  - In: `lib/runtime/exotic_types.c`
  - Implement: `Trybble` struct (5 trits per trybble)
  - Implement: `LUT_UNPACK[256]` (byte → 5 trits)
  - Implement: `LUT_PACK[243]` (5 trits → byte)
  - Implement: `aria_init_lut()` function
  - Verify: LUT size ~1.28 KB (L1 cache resident)
  - Reference: TRIT_NIT_IMPLEMENTATION_GUIDE.md section 2.1

- [ ] **2.3**: Implement trit operations (Kleene logic)
  - Implement: `aria_trit_and(a, b)` using `min(a, b)` ⚠️ NOT bitwise
  - Implement: `aria_trit_or(a, b)` using `max(a, b)` ⚠️ NOT bitwise
  - Implement: `aria_trit_not(a)` using `-a`
  - Implement: Sticky ERR checks in ALL functions
  - Reference: TRIT_NIT_IMPLEMENTATION_GUIDE.md section 2.2

- [ ] **2.4**: Implement trit arithmetic
  - Implement: `aria_trit_add(a, b)` with overflow → ERR
  - Implement: `aria_trit_add_carry(a, b, cin)` returning {result, carry}
  - Implement: Truth table logic (A + B + Cin = 3×Cout + Result)
  - Implement: `aria_trit_sub(a, b)` 
  - Implement: `aria_trit_mul(a, b)`
  - Implement: `aria_trit_neg(a)`
  - Add: Overflow detection (result outside {-1, 0, 1} → ERR)

- [ ] **2.5**: Implement nit operations (9-valued Kleene)
  - Implement: `aria_nit_and(a, b)` using `min(a, b)` for {-4..4}
  - Implement: `aria_nit_or(a, b)` using `max(a, b)`
  - Implement: `aria_nit_not(a)` using `-a`
  - Implement: Bounds checking (a < -4 || a > 4 → ERR) ⚠️ NOT 0
  - Reference: GOTCHA #3 above

- [ ] **2.6**: Implement nit arithmetic with lookup table
  - Create: `NIT_ADD_TABLE[9][9]` (81 entries)
  - Implement: `aria_nit_add(a, b)` using table lookup
  - Add: Bounds checking before table access
  - Implement: `aria_nit_sub(a, b)`
  - Implement: `aria_nit_mul(a, b)`
  - Implement: `aria_nit_neg(a)`

- [ ] **2.7**: Implement tryte operations (Split-Byte packing)
  - Implement: `aria_tryte_add(a, b)` with ripple-carry
  - Steps: Unpack → Trit-wise add → Check carry → Repack
  - Implement: `aria_tryte_sub(a, b)`
  - Implement: `aria_tryte_mul(a, b)`
  - Implement: `aria_tryte_div(a, b)` with div-by-zero → ERR
  - Implement: `aria_tryte_mod(a, b)`
  - Implement: `aria_tryte_neg(a)`
  - Reference: TRIT_NIT_IMPLEMENTATION_GUIDE.md section 2.3

- [ ] **2.8**: Implement nyte operations (Biased radix)
  - Implement: `aria_nyte_add(a, b)` with unbias → add → rebias
  - Implement: Overflow detection (result outside ±29,524 → ERR)
  - Implement: `aria_nyte_sub(a, b)`
  - Implement: `aria_nyte_mul(a, b)`
  - Implement: `aria_nyte_div(a, b)`
  - Implement: `aria_nyte_mod(a, b)`
  - Implement: `aria_nyte_neg(a)`

- [ ] **2.9**: Implement comparison operations
  - Implement: `aria_trit_cmp(a, b)` returning {-1, 0, 1}
  - Implement: `aria_tryte_cmp(a, b)`
  - Implement: `aria_nit_cmp(a, b)`
  - Implement: `aria_nyte_cmp(a, b)`

- [ ] **2.10**: Implement conversion functions
  - Implement: `aria_bin_to_tryte(int32_t val)` with range check
  - Implement: `aria_tryte_to_bin(uint16_t val)` with unbias
  - Implement: `aria_tryte_to_nyte(uint16_t val)` (no-op cast, isomorphic)
  - Implement: `aria_nyte_to_tryte(uint16_t val)` (no-op cast)
  - Verify: Isomorphism (same bias, same range)

- [ ] **2.11**: Unit test each runtime function
  - Create: `lib/runtime/tests/test_exotic.c`
  - Test: Each function individually
  - Test: Kleene logic axioms (commutativity, idempotence, absorption)
  - Test: Sticky ERR propagation
  - Test: Overflow detection
  - Test: Boundary cases (MIN/MAX values)
  - Run: `make test_runtime`

**Completion Criteria**:
- ✅ All 40+ intrinsic functions implemented
- ✅ LUT initialized correctly (verified by unit tests)
- ✅ Zero logic inversions (True AND False = False)
- ✅ Zero sentinel healing (ERR + x = ERR)
- ✅ Zero error swallowing (invalid → ERR, not 0)

---

**Completion Criteria**:
- ✅ Literal syntax implemented (0t and 0n prefixes)
- ✅ Lexer tokenizes balanced literals correctly
- ✅ Parser converts to int64 values
- ✅ Test suite validates correct conversion
- ✅ 0t1T0 = 6, 0n2A = 17 both working

---

### Phase 3: Compiler Integration ⏱️ 3 days

**Goal**: Generate correct LLVM IR that calls runtime functions

#### Tasks:

- [ ] **3.1**: Declare runtime functions in codegen context
  - File: `src/backend/codegen/codegen_context.h`
  - Add: Function declarations for all intrinsics
  - Add: `llvm::Function*` members for caching function refs
  - Example: `llvm::Function* aria_trit_add_func;`

- [ ] **3.2**: Implement runtime function getter
  - File: `src/backend/codegen/codegen_context.cpp`
  - Implement: `getOrCreateExoticFunction(name, type)`
  - Create: LLVM function signatures matching runtime
  - Mark: External linkage for runtime functions
  - Cache: Function pointers to avoid repeated lookups

- [ ] **3.3**: Update binary operation codegen ⚠️ CRITICAL
  - File: `src/backend/codegen/codegen_expr.cpp`
  - Modify: `visitBinaryOp()` to detect exotic types
  - Add: Call to `generateExoticBinaryOp()` for ALL exotic types
  - **CRITICAL**: NO bypass for atomic types (trit/nit)
  - Reference: GOTCHA #1 above

- [ ] **3.4**: Implement generateExoticBinaryOp()
  - File: `src/backend/codegen/codegen_expr.cpp`
  - Map: Type + Op → Runtime function name
  - Examples:
    - `Trit + Plus → "aria_trit_add"`
    - `Trit + And → "aria_trit_and"` ⚠️ NOT bitwise!
    - `Tryte + Star → "aria_tryte_mul"`
  - Generate: LLVM call instruction
  - Return: Result value

- [ ] **3.5**: Handle comparison operators
  - Add: Special case for comparison ops (==, !=, <, >, <=, >=)
  - Call: `aria_*_cmp()` functions
  - Convert: Result {-1, 0, 1} to boolean (i1) if needed

- [ ] **3.6**: Handle unary operations
  - Add: Negation operator (`-x`)
  - Call: `aria_trit_neg()`, `aria_tryte_neg()`, etc.
  - Add: Logical NOT (`!x`)
  - Call: `aria_trit_not()` (already implemented)

- [ ] **3.7**: Update module linking
  - File: `src/backend/codegen/codegen_module.cpp`
  - Add: Link to runtime library (`libaria_runtime.a`)
  - Update: CMakeLists.txt to build runtime
  - Verify: Runtime symbols resolve at link time

- [ ] **3.8**: Test IR generation
  - Create: Simple test program with exotic arithmetic
  - Build: With `-emit-llvm` flag
  - Dump: LLVM IR to file
  - Verify: Calls to `aria_trit_add`, NOT `add i8`
  - Verify: NO direct bitwise operations on exotic types

- [ ] **3.9**: Optional: Intrinsic lowering for hot paths
  - Implement: Inline LLVM IR for simple ops (future optimization)
  - Use: `icmp` + `select` for min/max logic
  - Benchmark: Compare function call vs inline performance
  - Reference: TRIT_NIT_IMPLEMENTATION_GUIDE.md section 3.2

**Completion Criteria**:
- ✅ All exotic binary ops generate runtime calls
- ✅ NO backend bypass (verified by IR inspection)
- ✅ Runtime library links successfully
- ✅ Simple test programs compile and run

---

### Phase 4: Testing & Validation ⏱️ 2 days

**Goal**: Achieve 363/363 tests passing with comprehensive coverage

#### Tasks:

- [ ] **4.1**: Move batch02 from tests/future/
  - Command: `mv tests/future/batch02_gemini_audit_fixes.aria tests/`
  - Verify: File contains 31 test cases
  - Review: Test coverage (arithmetic, logic, boundaries, conversions)

- [ ] **4.2**: Run full test suite
  - Command: `./build.sh && ./run_tests.sh`
  - Expected: 363/363 tests passing
  - Check: Zero failures, zero skipped
  - Debug: Any failures immediately (don't accumulate debt)

- [ ] **4.3**: Validate Kleene logic tests
  - Test: `1T && (-1T) == -1T` (True AND False = False)
  - Test: `1T || (-1T) == 1T` (True OR False = True)
  - Test: `!0T == 0T` (NOT Unknown = Unknown)
  - Test: `!1T == -1T` (NOT True = False)
  - Reference: Property-based testing in GUIDE

- [ ] **4.4**: Validate sticky ERR propagation
  - Test: `ERR + 1T == ERR`
  - Test: `TRYTE_ERR + 1 == TRYTE_ERR`
  - Test: `aria_trit_and(ERR, 1T) == ERR`
  - Test: No sentinel healing (ERR never becomes valid value)
  - Reference: GOTCHA #2 above

- [ ] **4.5**: Validate overflow detection
  - Test: `1T + 1T == ERR` (trit overflow)
  - Test: `TRYTE_MAX + 1 == TRYTE_ERR`
  - Test: `4N + 1N == ERR` (nit overflow)
  - Test: `NYTE_MAX + 1 == NYTE_ERR`

- [ ] **4.6**: Validate conversions
  - Test: `tryte_to_nyte(x)` isomorphism
  - Test: `bin_to_tryte(59049)` out-of-range → ERR
  - Test: `tryte_to_bin(TRYTE_MAX) == 29524`
  - Test: Round-trip conversions preserve value

- [ ] **4.7**: Add fuzzer corpus
  - Create: `tests/fuzzing/trit_corpus/` directory
  - Add: Random trit combinations
  - Add: Boundary values (MIN, MAX, ERR)
  - Add: Known edge cases from research

- [ ] **4.8**: Property-based testing
  - Implement: Kleene logic axioms
    - Commutativity: `a ∧ b = b ∧ a`
    - Idempotence: `a ∧ a = a`
    - Absorption: `a ∧ ERR = ERR`
  - Test: All trit/nit combinations ({-1,0,1} × {-1,0,1} = 9 cases)
  - Verify: No violations

- [ ] **4.9**: Stress testing
  - Test: Large tryte arithmetic chains (no accumulating errors)
  - Test: Nested logic operations
  - Test: Type conversion chains
  - Profile: Performance (LUT cache efficiency)

- [ ] **4.10**: Update TEST_STATUS.md
  - Update: Test count (363/363 implemented features)
  - Document: Any edge cases discovered
  - Mark: batch02 as no longer in future/
  - Success: 100% coverage on implemented features

**Completion Criteria**:
- ✅ 363/363 tests passing (100%)
- ✅ Zero logic inversions detected
- ✅ Zero sentinel healing detected
- ✅ Zero error swallowing detected
- ✅ Fuzzer finds no crashes
- ✅ Property-based tests verify Kleene axioms

---

### Phase 5: Documentation ⏱️ 1 day

**Goal**: Complete documentation for community use

#### Tasks:

- [ ] **5.1**: Update aria_specs.txt
  - Add: Section on exotic types
  - List: All runtime function signatures (40+ functions)
  - Document: Kleene logic semantics
  - Document: ERR sentinel propagation rules
  - Document: Packing strategies (Split-Byte vs Biased Radix)

- [ ] **5.2**: Update programming guide
  - File: `docs/PROGRAMMING_GUIDE.md`
  - Add: Section on trit/nit usage
  - Examples: Basic arithmetic
  - Examples: Logical operations
  - Examples: Type conversions
  - Warnings: Common pitfalls (no implicit conversions)

- [ ] **5.3**: Document implementation notes
  - File: `docs/IMPLEMENTATION_NOTES.md`
  - Document: LUT design choices
  - Document: Cache efficiency analysis
  - Document: Why Split-Byte for tryte vs Biased for nyte
  - Document: Any gotchas discovered during implementation

- [ ] **5.4**: Update CHANGELOG.md
  - Add: Entry for v0.X.0 (exotic types feature)
  - List: New types (trit, nit, tryte, nyte)
  - List: 40+ new runtime functions
  - Note: Breaking change (new reserved keywords)

- [ ] **5.5**: Update README.md
  - Update: Test count (363/363 implemented features → 100%)
  - Update: Language features section
  - Remove: "(NOT YET IMPLEMENTED)" from trit/nit examples
  - Add: Link to exotic types documentation

- [ ] **5.6**: Cross-reference documentation
  - Verify: All doc links work
  - Verify: Code examples compile
  - Verify: No contradictions between docs
  - Create: Index of exotic type documentation

**Completion Criteria**:
- ✅ All documentation updated
- ✅ Code examples verified
- ✅ No broken links
- ✅ README accurately reflects features

---

## 📊 Progress Tracking

### Overall Status: SIGNIFICANT INFRASTRUCTURE ALREADY COMPLETE!

**Discovery (Feb 5, 2026)**: Previous implementation already has:
✅ Type system registration (trit/nit/tryte/nyte in TypeSystem)
✅ Runtime library (666 lines in ternary_ops.cpp)
✅ Complete header file (ternary_ops.h with all 40+ intrinsics)
✅ Compiler codegen integration (ternary_codegen.cpp exists)
✅ 363 test files exist

**Remaining Work**: Fix integration issues, validate correctness, ensure no GOTCHAS

**Phase 1**: Type System ✅✅✅✅✅✅ (6/6 tasks - ALREADY DONE)  
**Phase 2**: Runtime Library ✅✅✅✅✅✅✅✅✅✅✅ (11/11 tasks - ALREADY DONE)  
**Phase 3**: Compiler Integration ✅✅✅✅⬜⬜⬜⬜⬜ (5/9 tasks - MOSTLY DONE)  
**Phase 4**: Testing & Validation ⬜⬜⬜⬜⬜⬜⬜⬜⬜⬜ (0/10 tasks - NEEDS WORK)  
**Phase 5**: Documentation ⬜⬜⬜⬜⬜⬜ (0/6 tasks)  

**Actual Progress**: 22/42 infrastructure tasks complete (52%)  
**Remaining**: Validate correctness, fix GOTCHAS, test comprehensively

---

## 🎯 Daily Goals

### Day 1 (Phase 1 - Part 1)
- [ ] Tasks 1.1-1.3: Type enum, constants, Type class updates
- [ ] Test compilation with new types

### Day 2 (Phase 1 - Part 2)
- [ ] Tasks 1.4-1.6: Type checking, literal parsing, initial tests
- [ ] Verify type system integration complete

### Day 3 (Phase 2 - Part 1)
- [ ] Tasks 2.1-2.4: Runtime structure, LUT, trit operations
- [ ] Unit test trit functions

### Day 4 (Phase 2 - Part 2)
- [ ] Tasks 2.5-2.6: Nit operations and lookup tables
- [ ] Unit test nit functions

### Day 5 (Phase 2 - Part 3)
- [ ] Tasks 2.7-2.8: Tryte and nyte operations
- [ ] Unit test composite types

### Day 6 (Phase 2 - Part 4)
- [ ] Tasks 2.9-2.11: Conversions, comparisons, full testing
- [ ] Runtime library complete

### Day 7 (Phase 3 - Part 1)
- [ ] Tasks 3.1-3.4: Codegen context, runtime functions, binary ops
- [ ] Test IR generation

### Day 8 (Phase 3 - Part 2)
- [ ] Tasks 3.5-3.7: Comparisons, unary ops, module linking
- [ ] Verify compilation end-to-end

### Day 9 (Phase 3 - Part 3)
- [ ] Tasks 3.8-3.9: IR verification, optional optimizations
- [ ] Compiler integration complete

### Day 10 (Phase 4 - Part 1)
- [ ] Tasks 4.1-4.5: Move tests, full suite, basic validation
- [ ] Fix any immediate failures

### Day 11 (Phase 4 - Part 2)
- [ ] Tasks 4.6-4.10: Advanced validation, fuzzing, stress tests
- [ ] 363/363 tests passing

### Day 12 (Phase 5)
- [ ] Tasks 5.1-5.6: All documentation updates
- [ ] Final review and cleanup

### Day 13-14 (Buffer)
- [ ] Address any issues discovered
- [ ] Final verification
- [ ] Celebrate success 🎉

---

## 🔍 Quality Gates

Each phase must pass these gates before proceeding:

**Phase 1 Gates**:
- ✅ New types compile without errors
- ✅ Type checker enforces correct usage
- ✅ Literals parse with range validation

**Phase 2 Gates**:
- ✅ All 40+ functions pass unit tests
- ✅ Zero crashes on LUT access
- ✅ Kleene logic verified (no inversions)
- ✅ Sticky ERR verified (no healing)

**Phase 3 Gates**:
- ✅ IR contains runtime calls, NOT direct ops
- ✅ Runtime library links successfully
- ✅ Simple programs execute correctly

**Phase 4 Gates**:
- ✅ 363/363 tests passing (100%)
- ✅ Fuzzer finds no crashes
- ✅ Property tests verify axioms

**Phase 5 Gates**:
- ✅ All documentation accurate
- ✅ Code examples compile and run
- ✅ No broken cross-references

---

## 📝 Notes & Discoveries

**Critical Insights from Research**:
1. **Radix Economy**: Base-3 is 5% more efficient than base-2 (closest to e=2.718)
2. **Isomorphism**: tryte (10 trits) ≡ nyte (5 nits) in range and capacity
3. **Cache Optimization**: Split-Byte LUT (1.28 KB) vs Full LUT (640 KB) = 500× difference
4. **Deterministic Latency**: L1 cache residency critical for physics simulations
5. **Blueprint Philosophy**: Same symbol = same meaning everywhere (door analogy)

**Lessons from Previous Failures**:
1. Never bypass runtime for "simple" types - leads to logic inversions
2. Never use LLVM arithmetic directly on exotic types - breaks semantics
3. Never return 0 on error - breaks error detection entirely
4. Always propagate ERR sticky, never "heal" it to valid values

**Implementation Strategy**:
- **Measure Twice, Cut Once**: Verify each phase before proceeding
- **Test Continuously**: Unit test each function as implemented
- **Document Gotchas**: Record any surprises for future reference
- **No Shortcuts**: Blueprint thinking demands thoroughness

---

## ✅ Success Criteria Summary

**Functional Requirements**:
- [x] Research complete (670 searchable .md files)
- [ ] Type system supports trit/nit/tryte/nyte
- [ ] 40+ runtime functions implemented correctly
- [ ] Compiler generates runtime calls (no bypass)
- [ ] 363/363 tests passing (100% coverage)

**Safety Requirements**:
- [ ] Zero logic inversions (True AND False = False)
- [ ] Zero sentinel healing (ERR + x = ERR always)
- [ ] Zero error swallowing (invalid → ERR, not 0)
- [ ] Overflow detection working (boundaries enforced)
- [ ] Sticky ERR propagation in all operations

**Performance Requirements**:
- [ ] LUT fits in L1 cache (~1.28 KB)
- [ ] Deterministic latency (no cache misses)
- [ ] Competitive with hand-rolled ternary arithmetic

**Documentation Requirements**:
- [ ] aria_specs.txt updated with intrinsics
- [ ] Programming guide has exotic type examples
- [ ] Implementation notes capture design choices
- [ ] README reflects actual feature status

---

**Ready to begin Phase 1 when you are.** 🏗️

**Philosophy**: "We will get there and when we get there we will be certain we have done the best we could. That's all that matters."
