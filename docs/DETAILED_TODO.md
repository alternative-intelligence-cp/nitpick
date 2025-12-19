# Aria v0.1.0 Detailed Implementation TODO
# WITH EXACT FILE REFERENCES, LINE NUMBERS, AND RESEARCH CITATIONS

**Purpose**: Prevent context drift by providing exact references for every task.
**Philosophy**: No deferrals, no shortcuts, test everything, honest documentation.

---

## HOW TO USE THIS TODO

### Before EVERY Work Session:

```bash
# 1. Core philosophy and status
cat /home/randy/._____RANDY_____/REPOS/aria/PROJECT_INFO.txt

# 2. Language specification (source of truth for syntax)
cat /home/randy/._____RANDY_____/REPOS/aria/docs/info/aria_specs.txt

# 3. Workflow to prevent mistakes
cat /home/randy/._____RANDY_____/REPOS/aria/docs/WORKFLOW.md

# 4. What went wrong before
cat /home/randy/._____RANDY_____/REPOS/aria/docs/POSTMORTEM_V0.1.0.md

# 5. Current implementation status
cat /home/randy/._____RANDY_____/REPOS/aria/docs/FEATURE_AUDIT.md
```

### Before EVERY Task:

1. Read task's research file COMPLETELY
2. Read relevant aria_specs.txt sections
3. Check FEATURE_AUDIT.md for dependencies
4. Write test FIRST (must fail initially)
5. Implement feature COMPLETELY
6. Compile and run test
7. Update FEATURE_AUDIT.md with ✅
8. Only then move to next task

---

## PHASE 1: PARSER ENHANCEMENTS

### Task 1.1: Generic Types Parsing ❌

**Research File**: `docs/gemini/responses/research_027_generics_templates.txt`  
**Spec Reference**: `docs/info/aria_specs.txt` (search for "generic", "template", "<T>")  
**Parser Code**: `src/parser/parser.cpp` (review current state)  
**Test File**: `tests/feature_validation/generics_basic.aria` (CREATE THIS FIRST)

**Why This Is Critical**:
- Blocks all collections (Vec, HashMap, HashSet)
- Blocks reusable data structures
- Core feature of modern language
- Cannot be deferred or simplified

**Pre-Task Reading Order**:
1. PROJECT_INFO.txt (remember: no deferrals)
2. aria_specs.txt lines 1-100 (type system overview)
3. research_027_generics_templates.txt (COMPLETE file)
4. Current parser.cpp generic handling (if any)

**Test Must Include**:
```aria
// Basic generic function
func<T>:identity = T(item:T) { pass(item); };

// Generic with constraints
func<T>:print_it = result(item:T) where T: Show {
    show(item);
    pass();
};

// Generic type usage
Vec<int32>:numbers = vec_new();
vec_push(numbers, 42);

// Nested generics
Vec<Vec<int32>>:matrix = vec_new();

// Multiple type parameters
HashMap<string, int64>:ages = hashmap_new();
```

**Acceptance Criteria (ALL Required)**:
- [ ] `func<T>` syntax recognized by parser
- [ ] `Vec<int32>` type syntax parsed correctly
- [ ] `where T: Trait` constraints parsed
- [ ] Multiple parameters `<T, U, V>` work
- [ ] Nested generics `Vec<Vec<T>>` work
- [ ] Test file compiles without syntax errors
- [ ] Test file runs correctly
- [ ] FEATURE_AUDIT.md updated with ✅

**Implementation Steps**:
1. Write test file (above template)
2. Try to compile - verify it fails
3. Add generic syntax tokens to lexer
4. Add generic parsing rules to parser
5. Update AST nodes for generics
6. Recompile test - fix errors iteratively
7. Test must compile and run
8. Try test with stdlib modules
9. Update FEATURE_AUDIT.md

**DO NOT**:
- Skip constraints ("add later")
- Limit to single type parameter
- Move on if test doesn't fully work
- Defer nested generics
- Simplify specification

---

### Task 1.2: Module System Keywords ❌

**Research File**: `docs/gemini/responses/research_028_module_system.txt`  
**Spec Reference**: `docs/info/aria_specs.txt` (search "use", "mod", "pub")  
**Lexer Code**: `src/lexer/lexer.cpp` (add keywords)  
**Parser Code**: `src/parser/parser.cpp` (add rules)  
**Test File**: `tests/feature_validation/modules_basic.aria` (CREATE THIS FIRST)

**Why This Is Critical**:
- Cannot organize code without modules
- Cannot import stdlib
- Cannot separate concerns
- Fundamental to language usability

**Pre-Task Reading**:
1. PROJECT_INFO.txt
2. aria_specs.txt module system section
3. research_028_module_system.txt (COMPLETE)
4. Current keyword list in lexer

**Test Must Include**:
```aria
// Basic import
use std.math;

// Nested module
use std.collections.vec;

// Selective import
use std.string { strlen, strcat };

// Module declaration
mod my_module {
    pub func:public_func = result() { pass(); };
    func:private_func = result() { pass(); };
}

// Using imported functions
int64:result = sqrt(16);
Vec<int32>:nums = vec_new();
```

**Acceptance Criteria**:
- [ ] `use` keyword in lexer
- [ ] `mod` keyword in lexer
- [ ] `pub` keyword in lexer
- [ ] Import statements parsed
- [ ] Module declarations parsed
- [ ] Symbol resolution works
- [ ] Can import and use std.math
- [ ] Test compiles
- [ ] Test runs
- [ ] FEATURE_AUDIT.md updated

**DO NOT**:
- Defer symbol resolution
- Skip pub/private
- Hardcode module paths
- Move on if imports don't work

---

### Task 1.3: Struct Type Variables ❌

**Research File**: `docs/gemini/responses/research_014_composite_types_part1.txt`  
**Spec Reference**: `docs/info/aria_specs.txt` (search "struct", "Type:var")  
**Parser Code**: `src/parser/parser.cpp`  
**Current Problem**: `Duration:d` fails with "Expected ';' after expression"  
**Test File**: `tests/feature_validation/struct_instantiation.aria` (CREATE)

**Why This Is Critical**:
- lib/std/time.aria cannot compile without this
- Cannot use custom types in variables
- Blocks all struct-based APIs
- Core type system feature

**Pre-Task Reading**:
1. PROJECT_INFO.txt
2. aria_specs.txt struct section
3. research_014_composite_types_part1.txt
4. lib/std/time.aria (see what's blocked)

**Test Must Include**:
```aria
// Struct definition
struct Duration {
    int64:nanos;
};

// Constructor function
func:duration_from_secs = Duration(int32:secs) {
    Duration:d;
    d.nanos = secs * 1000000000;
    pass(d);
};

// Variable declaration with struct type
Duration:five_sec = duration_from_secs(5);

// Member access
int64:nanos = five_sec.nanos;

// Passing to function
func:print_duration = void(Duration:dur) {
    println("Duration: {} nanos", dur.nanos);
};

print_duration(five_sec);
```

**Acceptance Criteria**:
- [ ] `Duration:d` declaration works
- [ ] `Duration:d = func()` initialization works
- [ ] `d.nanos` member access works
- [ ] Function parameters `func:f = void(Duration:dur)` work
- [ ] Test compiles
- [ ] Test runs
- [ ] lib/std/time.aria now compiles
- [ ] FEATURE_AUDIT.md updated

**DO NOT**:
- Work around with pointers
- Skip member access
- Simplify struct functionality
- Move on until time.aria compiles

---

(Continue with remaining tasks in same format...)

See `/home/randy/._____RANDY_____/REPOS/aria/docs/FEATURE_AUDIT.md` for complete feature list.

Each task follows same pattern:
- Research file reference (EXACT path)
- Spec reference (EXACT search term)
- Code location (EXACT file)
- Test file (CREATE FIRST)
- Pre-task reading (IN ORDER)
- Test template (EXACT code)
- Acceptance criteria (ALL required)
- Implementation steps (SPECIFIC)
- DO NOT list (PREVENT shortcuts)

NO DEFERRALS. NO SHORTCUTS. TEST EVERYTHING.
