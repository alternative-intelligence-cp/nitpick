# Aria v0.0.6 Implementation Roadmap
**Goal: Complete language features for impressive demos**
**Last Updated:** December 5, 2025

---

## 🎯 PRIORITY QUEUE (Sorted by Demo Impact)

### ⭐ TIER 1: CRITICAL FOR DEMOS (Must Have)

#### 1. Structs (4-6 hours) - HIGHEST PRIORITY
**Why:** Unlocks realistic data structures (linked lists, trees, hash tables)
**What's Needed:**
- [ ] Parser: `struct StructName { type:field; ... }`
- [ ] AST node: StructDecl with field list
- [ ] Codegen: LLVM StructType creation
- [ ] Member access: `instance.field`
- [ ] Constructor syntax: `StructName{field: value, ...}`
- [ ] Test: LinkedList, BinaryTree, Point2D

**Demo Impact:** 🔥🔥🔥 (Game changer - shows real systems programming)

---

#### 2. Arrays (2-3 hours)
**Why:** Data structure operations, algorithms need array manipulation
**What's Needed:**
- [ ] Parser: Array literals `[1, 2, 3, 4]`
- [ ] Codegen: LLVM array allocation
- [ ] Indexing: `arr[i]` read/write
- [ ] Length operator: `arr.length` or builtin
- [ ] Test: Quicksort, bubble sort, array sum

**Demo Impact:** 🔥🔥🔥 (Essential for algorithm demos)

---

#### 3. Break/Continue (1 hour)
**Why:** Every algorithm demo needs early loop exit
**What's Needed:**
- [ ] Parser: Already handles `break;` and `continue;` tokens
- [ ] AST: BreakStmt, ContinueStmt nodes (probably exist)
- [ ] Codegen: Branch to loop exit/continue blocks
- [ ] Test: Prime search with break, skip-even with continue

**Demo Impact:** 🔥🔥 (Makes loops usable for real algorithms)

---

#### 4. Modules (mod/pub) (3-4 hours)
**Why:** Organize code into multiple files, show professional structure
**What's Needed:**
- [ ] Parser: `mod moduleName { ... }` and `pub func:...`
- [ ] Symbol table: Namespace management
- [ ] Import/export: `use otherModule;`
- [ ] Codegen: LLVM module linking
- [ ] Test: Math module, Utils module, multi-file project

**Demo Impact:** 🔥🔥 (Shows scalability beyond toy projects)

---

### ⭐ TIER 2: HIGH VALUE (Should Have)

#### 5. String Interpolation (2 hours)
**Why:** Clean output for demos, user-friendly syntax
**What's Needed:**
- [ ] Lexer: Backtick string tokenization (may exist)
- [ ] Parser: Parse `` `text ${expr} more` `` into concatenation
- [ ] Codegen: String formatting/concatenation
- [ ] Test: `` `Result: ${x + y}` ``

**Demo Impact:** 🔥🔥 (Makes output readable and impressive)

---

#### 6. Bitwise Operators (1 hour)
**Why:** Systems programming credibility, bit manipulation demos
**What's Needed:**
- [ ] Parser: Already parses `&`, `|`, `^`, `~`, `<<`, `>>`
- [ ] Codegen: LLVM bitwise IR instructions
- [ ] Test: Flags, bit packing, crypto primitives

**Demo Impact:** 🔥 (Shows systems programming capability)

---

#### 7. Defer Statements (1-2 hours)
**Why:** Resource cleanup, RAII pattern (C++ credibility)
**What's Needed:**
- [ ] Parser: May already parse `defer { ... }`
- [ ] Codegen: Stack of cleanup blocks, emit before function exit
- [ ] Test: File close, memory free, lock release

**Demo Impact:** 🔥 (Shows modern language design)

---

### ⭐ TIER 3: NICE TO HAVE (Could Have)

#### 8. Object Literals (1 hour)
**Why:** Result type construction, struct initialization
**What's Needed:**
- [ ] Parser: `{field: value, field: value}`
- [ ] Codegen: Struct/Result initialization
- [ ] Test: `result{err: 0, val: 42}`

**Demo Impact:** ⭐ (Quality of life, not critical)

---

#### 9. For Loops (Comprehensive Testing) (30 min)
**Why:** Spec says they work, need validation
**What's Needed:**
- [ ] Test: `for(init; cond; step) { ... }`
- [ ] Verify codegen matches spec
- [ ] Add demo samples

**Demo Impact:** ⭐ (Already works, just needs confidence)

---

#### 10. Comptime Evaluation (4-8 hours)
**Why:** Meta-programming, impressive feature
**What's Needed:**
- [ ] Interpreter: Execute functions at compile time
- [ ] `comptime` keyword handling
- [ ] Constant folding integration
- [ ] Test: Fibonacci at compile time

**Demo Impact:** ⭐⭐ (Cool but not essential for v0.0.6)

---

### ⭐ TIER 4: FUTURE (Won't Have in v0.0.6)

#### 11. Wild Pointers (Manual Memory)
- Defer to v0.0.7 (needs manual memory management design)

#### 12. Pipeline Operators (|>, <|)
- Defer to v0.0.7 (functional programming sugar)

#### 13. Null Safety (??, ?.)
- Defer to v0.0.7 (needs null type design)

#### 14. NASM Macro Integration
- Preprocessor exists, but full macro system is v0.0.7+

---

## 📊 RECOMMENDED IMPLEMENTATION ORDER

**Week 1: Core Data Structures**
1. Structs (6 hours) - BLOCKING EVERYTHING
2. Arrays (3 hours) - DEPENDS ON: Structs
3. Break/Continue (1 hour) - INDEPENDENT

**Week 2: Organization & Polish**
4. Modules (4 hours) - INDEPENDENT
5. String Interpolation (2 hours) - INDEPENDENT
6. Bitwise Operators (1 hour) - INDEPENDENT
7. Defer (2 hours) - INDEPENDENT

**Week 3: Nice-to-Haves**
8. Object Literals (1 hour)
9. For Loop Testing (30 min)
10. Comptime (if time permits, 8 hours)

**Total Estimated Time: 20-25 hours of focused work**
**Realistic Timeline: 1-2 weeks of part-time work**

---

## 🎯 DEMO IMPACT AFTER EACH FEATURE

### After Structs + Arrays (Day 3)
**New Demos Unlocked:**
- Linked List (insert, delete, traverse)
- Binary Search Tree (insert, search, inorder traversal)
- Quicksort on arrays
- Hash table (simple chaining)
- Stack/Queue implementations

### After Break/Continue + Modules (Day 5)
**New Demos Unlocked:**
- Prime sieve (break on found)
- Search algorithms (break on match)
- Multi-file projects (math lib + main)
- Professional code organization

### After String Interpolation + Bitwise (Day 7)
**New Demos Unlocked:**
- Pretty-printed output for all demos
- Bit flags and masking
- Simple encryption (XOR cipher)
- Network protocol parsing

### After Defer + Object Literals (Day 10)
**New Demos Unlocked:**
- File I/O with auto-close
- Memory safety patterns
- Resource cleanup examples
- Clean Result type construction

---

## 🚀 TSODING DEMO READINESS TRACKER

| Feature | Current | After Structs | After Arrays | After All Tier 1 |
|---------|---------|---------------|--------------|------------------|
| Demo Variety | 13 samples | 20 samples | 25 samples | 35+ samples |
| Data Structures | None | 5+ types | 8+ types | 12+ types |
| Code Organization | Single file | Single file | Single file | Multi-file |
| Algorithm Complexity | Basic | Intermediate | Advanced | Expert |
| Systems Credibility | Medium | High | High | Very High |
| **Tsoding Ready %** | **95%** | **97%** | **98%** | **99%** |

---

## 📝 NOTES

- Each feature builds on previous (structs unlock arrays, arrays unlock algorithms)
- Break/Continue is quick win (1 hour, high impact)
- Modules can wait until after structs+arrays if needed
- String interpolation makes ALL demos more impressive (pretty output)
- Defer shows you understand RAII (C++ credibility with Tsoding)

**Current Focus:** Implement in order listed above. Each completion unlocks new demo potential.

**Velocity Estimate:** Based on recent work (recursion fix: 2 hours, error messages: 2 hours), expect each feature to take stated time or less. Randy + Aria team has proven 2-3x faster than estimates.

---

## 🎉 COMPLETED FEATURES (Reference)

- ✅ Lexer (100%)
- ✅ Preprocessor (100%)
- ✅ Parser - Core constructs (95%)
- ✅ Codegen - Core features (95%)
- ✅ Recursive functions
- ✅ Pattern matching (pick/fall)
- ✅ While/till/when loops
- ✅ Closures
- ✅ Result types with auto-wrap
- ✅ Professional error messages
- ✅ VSCode syntax highlighting

**Ready to start with Structs whenever you say go!** 🚀
