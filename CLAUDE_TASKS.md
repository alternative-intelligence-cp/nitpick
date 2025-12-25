# Claude's Parallel Work Queue
**Created**: 2024-12-24  
**Subscription Expires**: In a few days  
**Goal**: Maximize productivity before subscription ends

---

## ✅ COMPLETED
- [x] GlobEngine implementation (aria_utils/src/aglob/)
- [x] Syntax fixes across utility files (char literals, ANSI escapes, semicolons)
- [x] **C-1: Variable Initialization Bug** - ALREADY WORKING (calls are generated correctly)
- [x] **C-2: String Manipulation APIs** - Fixed string_concat, string_length
- [x] **C-3: aria_make ConfigParser** - ALREADY IMPLEMENTED in src/tools/project_config.cpp
- [x] **C-4: TBB Type System** - Implemented tbb64/32/16/8 builtins (from_int, is_err, to_int)
- [x] **C-5: Collection Constructors** - Implemented array_new, array_push, array_length, array_get, array_set, array_pop
- [x] **C-6: File Restoration** - Fixed netstat, parallel, regex corruption
- [x] **Print AriaString fix** - print() now handles strings from functions without literals

---

## 🔥 CRITICAL PRIORITY (Do These First)

### TASK C-1: Fix Variable Initialization Bug
**Status**: ✅ VERIFIED WORKING (December 24, 2025)
**Location**: aria compiler (`src/backend/ir/`)

**Investigation Result**:
The reported bug does NOT exist as described. Function call initializers in variable declarations DO generate correct IR.

**Test Results**:
```aria
func:main = int32() {
    string:x = string_from_char(65);  // ✅ Call IS generated
    print(x);  // ✅ Prints "A"
    pass(0);
};
```

**Generated IR** (correct):
```llvm
call void @aria_string_from_char(ptr %char_result_ptr, i8 65)
%char_str = extractvalue { ptr, ptr, i1 } %char_result, 0
store ptr %char_str, ptr %x, align 8
```

**Related Fix Applied**: `print()` builtin wasn't creating AriaString type when no literals existed. Fixed in `ir_generator.cpp:1923-1936` to create the type on-demand.

**Conclusion**: No blocking issue for stdlib development.

---

### TASK C-2: Implement String Manipulation APIs
**Status**: ✅ COMPLETE (December 24, 2025)
**Location**: aria compiler (`src/backend/ir/codegen_expr.cpp`)

**All APIs Now Working**:
| API | Status | Test Result |
|-----|--------|-------------|
| `string_from_char(byte)` | ✅ WORKS | Creates "H", "i", etc. |
| `string_concat(string, string)` | ✅ FIXED | Creates "Hi", "Hello" |
| `string_length(string)` | ✅ FIXED | Returns 2 for "Hi", 5 for "Hello" |

**Fixes Applied**:
1. `string_concat`: Added sret (struct return) pattern matching `string_from_char`, removed extra indirection
2. `string_length`: Fixed to use direct pointer from `codegenExpressionNode` (was double-loading)
3. Both fixes in `src/backend/ir/codegen_expr.cpp`

**Test Verified**:
```aria
func:main = int32() {
    string:h = string_from_char(72);  // H
    string:i = string_from_char(105); // i
    string:hi = string_concat(h, i);
    print(hi);                        // Prints: Hi
    print(string_length(hi));         // Prints: 2
    pass(0);
};
```

**Unblocked**: aeon utility (ACL config parser) can now use string APIs

**Implementation Steps**:

**Part 1: Runtime (C++)**
```cpp
// File: src/runtime/string/string.cpp
extern "C" AriaString* aria_string_from_char(uint8_t ch) {
    char buf[2] = {(char)ch, '\0'};
    return aria_string_create(buf, 1);
}

extern "C" AriaString* aria_string_concat_byte(AriaString* str, uint8_t ch) {
    // Allocate new string with length+1
    // Copy original + append byte
    // Return new string
}
```

**Part 2: Builtin Registration**
```cpp
// File: src/frontend/sema/type_checker.cpp
// In TypeChecker::registerBuiltins():
builtins["string_from_char"] = BuiltinFunction{
    .returnType = "string",
    .params = {{"ch", "byte"}},
    .cSymbol = "aria_string_from_char"
};
```

**Part 3: IR Codegen**
```cpp
// File: src/backend/ir/codegen_expr.cpp
// In codegenCallExpr() for builtins:
if (funcName == "string_from_char") {
    llvm::Function* fn = getOrDeclareFunction("aria_string_from_char", ...);
    return builder.CreateCall(fn, args);
}
```

**Files to Modify**:
- `src/runtime/string/string.cpp` (+30 lines)
- `include/runtime/string/string.h` (+3 declarations)
- `src/frontend/sema/type_checker.cpp` (+20 lines)
- `src/backend/ir/codegen_expr.cpp` (+40 lines)

**Test**:
```aria
func:main = int32() {
    string:greeting = string_from_char(72);  // 'H'
    greeting = string_concat_byte(greeting, 105);  // 'i'
    print(greeting);  // Should print "Hi"
    pass(0);
};
```

**Success Criteria**:
- Functions registered and callable
- Test program compiles and runs
- Output is correct
- **Unblocks**: aeon utility (ACL config parser needs this)

---

## 🟡 HIGH PRIORITY (After Critical)

### TASK C-3: Complete aria_make ConfigParser
**Status**: ✅ ALREADY IMPLEMENTED (discovered December 25, 2025)
**Location**: `src/tools/project_config.cpp`, `include/tools/project_config.h`

**Discovery**: A fully functional TOML-based config parser already exists in the codebase!

**Existing Implementation Features**:
- Parses `aria.toml` files using toml11 library
- Handles all sections: [package], [dependencies], [build], [features], [lib], [[bin]], [workspace]
- Validates package names (alphanumeric, 1-64 chars)
- Validates semantic version format
- Supports glob patterns in sources
- Supports multiple dependency formats (version string, table with path/git/features)
- Comprehensive error handling with descriptive messages

**Key Functions**:
- `ProjectConfigParser::parse_file(path)` - Parses aria.toml from file
- `ProjectConfigParser::parse_string(content)` - Parses TOML from string
- `ProjectConfigParser::validate()` - Validates configuration

**Note**: Task spec mentioned JSON-derivative syntax, but the actual implementation uses TOML (following Cargo.toml pattern per `docs/aria_toml_spec.txt`)

---

### TASK C-4: Implement TBB Type System
**Status**: 🟡 HIGH - Core language feature  
**Location**: aria compiler (`src/runtime/types/`, `src/frontend/sema/`, `src/backend/ir/`)  
**Estimated Time**: 2-3 days  
**Depends On**: C-1 (variable initialization fix)

**Background**: TBB (Tagged Balanced Base) types are NON-NEGOTIABLE per spec
- tbb64: 63-bit value + 1 ERR bit
- tbb32: 31-bit value + 1 ERR bit  
- tbb16: 15-bit value + 1 ERR bit

**Required APIs per Type**:
```aria
tbb64.from_int(int64) -> tbb64      // Constructor
tbb64.is_err(tbb64) -> bool         // Error check
tbb64.to_int(tbb64) -> int64        // Extract value
tbb64::ERR                          // Error constant
```

**Implementation Steps**:

**Part 1: Runtime Implementation**
```cpp
// File: include/runtime/types/tbb.h
typedef struct {
    int64_t value;  // Bit 63 is ERR flag, bits 0-62 are value
} AriaTBB64;

#define ARIA_TBB64_ERR_BIT (1ULL << 63)
#define ARIA_TBB64_ERR ((AriaTBB64){.value = ARIA_TBB64_ERR_BIT})

extern "C" AriaTBB64 aria_tbb64_from_int(int64_t val);
extern "C" bool aria_tbb64_is_err(AriaTBB64 tbb);
extern "C" int64_t aria_tbb64_to_int(AriaTBB64 tbb);

// Similar for tbb32, tbb16
```

**Part 2: Builtin Registration**
```cpp
// In type_checker.cpp
// Register 3 types × 4 operations = 12 builtins
builtins["tbb64_from_int"] = ...;
builtins["tbb64_is_err"] = ...;
// etc.
```

**Part 3: IR Codegen**
- Generate LLVM i64/i32/i16 types
- Implement operations with bit masking
- Add lowering pass in backend

**Files to Create**:
- `include/runtime/types/tbb.h` (150 lines)
- `src/runtime/types/tbb.cpp` (300 lines)
- `tests/types/test_tbb64.aria` (test)
- `tests/types/test_tbb32.aria` (test)
- `tests/types/test_tbb16.aria` (test)

**Files to Modify**:
- `src/frontend/sema/type_checker.cpp` (+150 lines for 12 builtins)
- `src/backend/ir/codegen_expr.cpp` (+200 lines)
- `src/runtime/CMakeLists.txt` (add tbb.cpp)

**Why Important**: 
- Blocks systemctl, watch, parallel utilities
- Core language feature (NON-NEGOTIABLE)

---

### TASK C-5: Implement Collection Constructors
**Status**: ✅ COMPLETE (December 25, 2025) - Array builtins implemented
**Location**: aria compiler (`src/runtime/collections/`, `src/backend/ir/codegen_expr.cpp`, `src/frontend/sema/type_checker.cpp`)

**Implemented Array APIs**:
| API | Status | Description |
|-----|--------|-------------|
| `array_new(element_size)` | ✅ WORKS | Creates new empty array |
| `array_push(arr, value)` | ✅ WORKS | Appends element to array |
| `array_length(arr)` | ✅ WORKS | Returns array length |
| `array_get(arr, index)` | ✅ WORKS | Returns pointer to element |
| `array_set(arr, index, value)` | ✅ WORKS | Sets element at index |
| `array_pop(arr)` | ✅ WORKS | Removes and returns last element |

**Files Modified**:
- `src/runtime/collections/collections.cpp` - Added simple wrappers
- `include/runtime/collections.h` - Added wrapper declarations
- `src/frontend/sema/type_checker.cpp` - Added array builtins type checking
- `src/backend/ir/codegen_expr.cpp` - Added array builtins codegen
- `src/backend/ir/ir_generator.cpp` - Added array builtins delegation

**Test Verified**:
```aria
func:main = int32() {
    int8@:arr = array_new(8);
    array_push(arr, 42);
    array_push(arr, 100);
    array_push(arr, 255);
    print(array_length(arr));  // Prints: 3
    pass(0);
};
```

**Map APIs**: Not yet implemented (can be added later if needed)

**Unblocks**: Basic array operations for utilities

---

## 🟢 MEDIUM PRIORITY (When time permits)

### TASK C-6: File Restoration and Syntax Cleanup
**Status**: 🟢 CLEANUP  
**Location**: Various utility files  
**Estimated Time**: 2-3 hours

**Corrupted Files** (from transformation scripts):
1. `sysUtils_12_netstat_CORRECTED.aria` (108 errors)
2. `sysUtils_17_parallel_CORRECTED.aria` (158 errors)
3. `sysUtils_20_regex_CORRECTED.aria` (wild/end issues)

**Tasks**:
- Review error messages
- Apply syntax fixes (char literals, escapes, etc.)
- Verify compilation
- Document any remaining blockers

---

### TASK C-7: Documentation and Examples
**Status**: 🟢 NICE TO HAVE  
**Estimated Time**: 1-2 hours

**Tasks**:
1. Document GlobEngine API (README already exists, expand examples)
2. Create example build.aria files for aria_make
3. Write cookbook for common patterns
4. Document TBB usage patterns (once C-4 done)

**Files**:
- `aria_utils/src/aglob/EXAMPLES.md`
- `aria_utils/src/amake/COOKBOOK.md`
- `aria/docs/TBB_GUIDE.md`

---

## 📊 TASK PRIORITY SUMMARY

**Must Do (Before Subscription Expires)**:
1. C-1: Variable initialization fix (🔴 CRITICAL - 2-4 hours)
2. C-2: String APIs (🟠 HIGH - 1 day, needs C-1)
3. C-3: ConfigParser (🟡 HIGH - 4-6 hours)

**Should Do (If Time Allows)**:
4. C-4: TBB types (🟡 HIGH - 2-3 days, needs C-1)
5. C-5: Collections (🟡 MEDIUM - 2 days, needs C-1)

**Nice to Have**:
6. C-6: File restoration (🟢 CLEANUP - 2-3 hours)
7. C-7: Documentation (🟢 NICE - 1-2 hours)

---

## 🎯 RECOMMENDED SEQUENCE

**Day 1**: 
- Morning: C-1 (Variable init fix) - UNBLOCKS EVERYTHING
- Afternoon: C-2 (String APIs) - Quick win, unblocks utilities

**Day 2**:
- Morning: C-3 (ConfigParser) - Complete aria_make
- Afternoon: Start C-4 (TBB types part 1)

**Day 3** (if subscription still active):
- Continue C-4 (TBB types part 2)
- Start C-5 (Collections) if time allows

**Day 4+** (if still active):
- Finish C-5 (Collections)
- C-6 and C-7 as time permits

---

## 📝 NOTES FOR CLAUDE

**Context**:
- You've been doing excellent work on GlobEngine and syntax fixes
- The variable initialization bug is BLOCKING all stdlib testing
- Your subscription expires soon, so we want to maximize impact
- Focus on unblocking features over perfection

**Communication**:
- Document any blockers you encounter
- If stuck >30 min, document and move to next task
- Commit frequently (every working milestone)
- Write clear commit messages referencing task IDs

**Testing**:
- Every feature needs at least one test case
- Compile and run tests before committing
- Document any test failures

**Questions**:
- Randy and Aria are available for clarification
- Check CLAUDE_TASKS.md for updates
- See aria/TODO for broader context

---

**Last Updated**: 2024-12-24  
**Next Review**: Daily or as tasks complete
