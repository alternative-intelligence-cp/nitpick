# Claude Session 2 Tasks
**Date**: December 25, 2025  
**Session Reset**: ~50 minutes  
**Context**: Unit test fixes 95% complete, variable init verified working

---

## 🔧 IMMEDIATE: Finish Unit Test Compilation

### Task 1: Fix test_lambda_execution.cpp Compilation Error
**Priority**: HIGH  
**Estimated Time**: 10-15 minutes

**Issue**: LambdaExpr uses `std::string returnType` but test may have FuncDeclStmt issues embedded

**Steps**:
1. Check `/home/randy/._____RANDY_____/REPOS/aria/tests/backend/test_lambda_execution.cpp`
2. Search for any FuncDeclStmt or Param eterNode constructor calls
3. Ensure all use `std::make_shared<SimpleType>("type")` pattern
4. Note: LambdaExpr CORRECTLY uses string (not changed in refactoring)
5. Rebuild: `cd build && make test_runner`

**Success Criteria**: test_runner executable builds successfully

---

## ✅ RUN TESTS

### Task 2: Execute Unit Test Suite
**Priority**: HIGH  
**Estimated Time**: 5 minutes

**Steps**:
```bash
cd /home/randy/._____RANDY_____/REPOS/aria/build
./tests/test_runner
# OR
make test
```

**Report**:
- Total tests
- Passed/Failed counts
- Any critical failures (not related to ArrayType stub)

---

## 🎯 METHOD CALL SYSTEM - Phase 1 (UNBLOCKS ALL 9 UTILITIES!)

### Task 3: Implement Namespace-Qualified Static Calls
**Priority**: CRITICAL (Unblocks ecosystem!)  
**Estimated Time**: 4-6 hours (detailed implementation guide provided)  
**Reference**: `/home/randy/._____RANDY_____/REPOS/aria/docs/gemini/responses/Aria Method Call System Design.txt`

**Problem**: All 9 system utilities use OOP syntax (`string.from_char()`, `array.push()`) but parser rejects it.

**Solution**: Implement compile-time resolution of `Type.method()` → `Type_method()` with zero runtime overhead.

**Implementation Guide (from Gemini Research)**:

**Part 1: Parser Changes** (`src/frontend/parser/parser.cpp`)

1. **Augment `parsePrimary()`** to accept type keywords when followed by `.`:
```cpp
ASTNodePtr Parser::parsePrimary() {
    // NEW: Handle Type Keywords as Namespaces
    if (isTypeKeyword(current.type)) {
        if (peekNext().type == TOKEN_DOT) {
            Token typeToken = advance();
            std::string typeName = typeToken.value;
            return std::make_shared<TypeReferenceExpr>(typeName, typeToken.line, typeToken.column);
        }
    }
    // ... existing cases
}
```

2. **Update `parseDot()`** to handle TypeReferenceExpr:
```cpp
ASTNodePtr Parser::parseDot(ASTNodePtr lhs) {
    consume(TOKEN_DOT, "Expected '.'");
    Token methodToken = consume(TOKEN_IDENTIFIER, "Expected method name");
    
    if (auto typeRef = std::dynamic_pointer_cast<TypeReferenceExpr>(lhs)) {
        // Static call: string.from_char
        return std::make_shared<StaticMethodCallExpr>(
            typeRef->typeName, methodToken.value, typeRef->line, typeRef->column
        );
    } else {
        // Instance access: obj.field
        return std::make_shared<MemberAccessExpr>(lhs, methodToken.value);
    }
}
```

**Part 2: AST Nodes** (`include/frontend/ast/expr.h`)

Add two new node types:
```cpp
class TypeReferenceExpr : public ASTNode {
public:
    std::string typeName;
    TypeReferenceExpr(const std::string& name, int line = 0, int column = 0);
};

class StaticMethodCallExpr : public ASTNode {
public:
    std::string typeName;
    std::string methodName;
    StaticMethodCallExpr(const std::string& type, const std::string& method, 
                         int line = 0, int column = 0);
};
```

**Part 3: Semantic Analysis** (`src/frontend/sema/type_checker.cpp`)

Resolve `Type.method()` to mangled function name:
```cpp
// In checkCallExpr():
if (auto staticCall = dynamic_cast<StaticMethodCallExpr*>(callee)) {
    // Construct mangled name: {typeName}_{methodName}
    std::string mangledName = staticCall->typeName + "_" + staticCall->methodName;
    
    // Look up in symbol table
    if (!symbolTable.hasFunction(mangledName)) {
        error("Type '" + staticCall->typeName + "' has no static method '" + 
              staticCall->methodName + "'");
    }
    
    // Rewrite AST to direct function call
    return std::make_shared<FuncCallExpr>(mangledName, args);
}
```

**Part 4: Generic Handling** (for `array<T>.new()`)

When TypeReferenceExpr contains generics:
- Extract base type: `array<string>` → `array`
- Trigger monomorphization with type parameter
- Use specialized mangled name: `_Aria_M_array_new_string`

**Testing**:
```bash
# Create test file
cat > tests/method_calls/static_basic.aria << 'EOF'
func:main = int32() {
    string:s = string.from_char(65);  // Should resolve to string_from_char(65)
    print(s);  // Should print "A"
    pass(0);
}
EOF

# Compile
./build/ariac tests/method_calls/static_basic.aria -o test_static
./test_static  # Should output: A
```

**Success Criteria**:
- ✅ `string.from_char(65)` compiles without parser error
- ✅ Resolves to `string_from_char` builtin
- ✅ All 9 utilities compile (aeon, systemctl, watch, df, netstat, parallel, regex, git, abuild)
- ✅ Error message for invalid method: "Type 'string' has no static method 'unknown'"

**Why This Matters**:
- **Unblocks ecosystem**: All utilities can compile
- **Zero runtime cost**: Pure compile-time transformation
- **Foundation for Phase 2**: Instance methods (`obj.method()`) use same infrastructure
- **Modern ergonomics**: Developers get expected OOP syntax

**Estimated Breakdown**:
- Parser changes: 1-2 hours
- AST nodes: 30 minutes
- Semantic resolution: 2-3 hours
- Testing & debugging: 1-2 hours
- **Total**: 4-6 hours

**Next Session Priority**: This should be tackled **after** getting test_runner building but could be **instead of** TBB if time is limited. The payoff is immediate - compile all utilities vs implementing one primitive type.

---

## 📊 TBB TYPE SYSTEM (Priority for Utilities)

### Task 4: Implement TBB64 Type and Builtins
**Priority**: HIGH (blocks systemctl, watch, parallel utilities)  
**Estimated Time**: 45-60 minutes

**Background**: TBB (Tagged Balanced Base) is Aria's error handling primitive. Similar to Result<T> but using balanced ternary encoding for efficient error propagation.

**Required APIs**:
```aria
// Type: tbb64 (64-bit tagged balanced base)
tbb64:x = tbb64_from_int(42);      // Create TBB from int
bool:is_err = tbb64_is_err(x);      // Check if error state  
int64:val = tbb64_to_int(x);        // Extract value (unsafe)

// Also need: tbb32, tbb16, tbb8 variants
```

**Implementation Layers**:

**1. Runtime (C++)** - `src/runtime/tbb/tbb.cpp`:
```cpp
extern "C" {
    // TBB64 is encoded as int64_t with error flag in sign/magnitude
    int64_t aria_tbb64_from_int(int64_t value);
    bool aria_tbb64_is_err(int64_t tbb_value);
    int64_t aria_tbb64_to_int(int64_t tbb_value);
}
```

**2. Type Checker** - `src/frontend/sema/type_checker.cpp`:
Register builtins in `initializeBuiltins()`:
```cpp
builtins["tbb64_from_int"] = ...;  // int64 -> tbb64
builtins["tbb64_is_err"] = ...;    // tbb64 -> bool
builtins["tbb64_to_int"] = ...;    // tbb64 -> int64
```

**3. IR Codegen** - `src/backend/ir/codegen_expr.cpp`:
Add cases in `codegenCallExpr()` for TBB builtins

**Reference**: See existing TBB codegen in `src/backend/ir/tbb_codegen.cpp` (arithmetic operations already implemented)

**Test**:
```aria
func:test_tbb = int32() {
    tbb64:x = tbb64_from_int(42);
    if (!tbb64_is_err(x)) {
        print(tbb64_to_int(x));  // Should print 42
    }
    pass(0);
}
```

---

## 🗂️ COLLECTION CONSTRUCTORS

### Task 5: Implement Array Constructor APIs
**Priority**: MEDIUM  
**Estimated Time**: 30-45 minutes

**Required APIs**:
```aria
array<T>:arr = array_new<int32>();      // Create empty array
array_push(arr, 42);                     // Add element
int64:len = array_length(arr);           // Get length
int32:val = array_get(arr, 0);           // Access element
array_set(arr, 0, 99);                   // Update element
```

**Implementation**:
1. Runtime: `src/runtime/collections/array.cpp`
2. Generic builtin registration (template functions)
3. IR codegen for generic array operations

**Note**: May need monomorphization support for `array<T>` - check `src/frontend/sema/generic_resolver.cpp`

---

## 🔗 EXTENDED STRING APIs

### Task 6: Additional String Functions
**Priority**: LOW (nice-to-have)  
**Estimated Time**: 20-30 minutes

**APIs to Add**:
```aria
string:num = string_from_int(12345);         // Int to string
int64:val = string_to_int("12345");          // Parse int
string:hex = string_to_hex(0xFF);            // Format hex
string:padded = string_pad_right("Hi", 5);   // Right-pad
string:pi = string_format_float(3.14159, 2); // Float format
```

**Implementation**: Same pattern as existing string functions in:
- Runtime: `src/runtime/string/strings.cpp`
- Type checker: `src/frontend/sema/type_checker.cpp`
- Codegen: `src/backend/ir/codegen_expr.cpp`

---

## 🌐 EXTERN/FFI SYNTAX

### Task 7: Opaque Struct Declarations
**Priority**: MEDIUM (blocks df utility)  
**Estimated Time**: 30-45 minutes

**Required Syntax**:
```aria
extern "libc" {
    opaque struct:FILE;                          // Forward declaration
    func:fopen = FILE*(int8*:path, int8*:mode);  // Returns opaque ptr
    func:fclose = int32(FILE*:stream);
}
```

**Implementation**:
1. **Lexer**: Add `opaque` keyword
2. **Parser**: Recognize `opaque struct:Name;` in extern blocks  
3. **AST**: Add `OpaqueStructDecl` node type
4. **IR Generator**: Generate opaque LLVM struct types

**Test**:
```aria
extern "libc" {
    opaque struct:FILE;
    func:fopen = FILE*(int8*:path, int8*:mode);
}

func:main = int32() {
    FILE*:f = fopen("test.txt", "r");
    pass(0);
}
```

---

## 📝 UTILITY COMPILATION TESTING

### Task 8: Compile Real Utilities
**Priority**: MEDIUM  
**Estimated Time**: 15-20 minutes per utility

**Utilities to Test**:
1. **aeon** (ACL parser) - Should work now with string APIs ✅
2. **systemctl** - Needs TBB types
3. **watch** - Needs TBB types
4. **df** - Needs extern/opaque syntax
5. **netstat** - Needs corruption fixes
6. **parallel** - Needs corruption fixes

**Process for Each**:
```bash
cd /home/randy/._____RANDY_____/REPOS/aria
./build/ariac aria_utils/bin/aeon.aria -o test_aeon
./test_aeon  # Verify it runs
```

**Report**: List which compile, which have errors, what blockers remain

---

## 📚 DOCUMENTATION

### Task 9: Update STATUS Files
**Priority**: LOW  
**Estimated Time**: 10 minutes

**Files to Update**:
1. `/home/randy/._____RANDY_____/____FILES/aria/STATE` - Current session achievements
2. `/home/randy/._____RANDY_____/REPOS/aria/IMPLEMENTATION_STATUS.md` - Feature status
3. Create `/home/randy/._____RANDY_____/REPOS/aria/UNIT_TEST_FIX_SUMMARY.md` documenting the 76-file AST constructor migration

**Include**:
- What was broken (AST refactoring: string → ASTNodePtr)
- How it was fixed (systematic sed/perl transformations)
- Files processed (76 test files)
- Remaining issues (test_lambda_execution, test_const_generics_arrays stub)

---

## 🎯 SUCCESS METRICS

**Minimum Goals** (achievable in 50 min):
- ✅ Unit tests building (Task 1)
- ✅ Test suite runs (Task 2)  
- ✅ Method call system Phase 1 (Task 3) **← GAME CHANGER**
- ✅ Status docs updated (Task 9)

**High-Value Goals** (if time allows):
- TBB types implemented (Task 4)
- Collection constructors (Task 5)

**Stretch Goals**:
- Extended string APIs (Task 6)
- Extern/FFI syntax (Task 7)
- Utilities compiling (Task 8)

---

## 📋 HANDOFF NOTES FOR NEXT SESSION

**When Claude's Session Ends**:

Document in `/home/randy/._____RANDY_____/____FILES/aria/CHAT`:
1. Which tasks completed
2. Any blockers encountered
3. Test results (pass/fail counts)
4. Utility compilation status
5. Recommendations for next priorities

**Critical for Aria (next Claude session)**:
- Unit test suite status
- TBB implementation complete/incomplete
- Utility readiness assessment
- Any compiler bugs discovered

---

## 🔍 DEBUGGING TIPS

**If Unit Tests Fail**:
1. Check constructor signatures (FuncDeclStmt, ParameterNode)
2. Verify `#include "frontend/ast/type.h"` present
3. Look for missing `->toString()` on type comparisons
4. Check for multi-line constructor calls (need -p0 perl mode)

**If TBB Tests Fail**:
1. Verify runtime functions in `src/runtime/tbb/tbb.cpp`
2. Check builtin registration in type_checker
3. Verify IR codegen matches sret pattern (like string functions)

**If Utilities Won't Compile**:
1. Check STDLIB_API_REQUIREMENTS.md for missing APIs
2. Use compiler errors to identify specific blockers
3. Prioritize fixing APIs over fixing corrupted utility files

---

**START WITH TASK 1** - Get tests building, then assess what's achievable in remaining time!
