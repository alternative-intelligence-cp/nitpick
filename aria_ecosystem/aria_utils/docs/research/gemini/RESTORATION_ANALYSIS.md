# Utility Restoration Analysis

**Date**: December 24, 2025
**Status**: Analysis Complete
**Affected Files**: netstat, parallel, regex, and others

---

## 1. Executive Summary

Multiple Aria utility files generated from Gemini research responses have been corrupted during an automated conversion process. The corruption follows consistent patterns that can be fixed systematically.

**Affected Files**:
- `sysUtils_12_netstat_CORRECTED.aria` - Major struct corruption
- `sysUtils_17_parallel_CORRECTED.aria` - Comment/field merging
- `sysUtils_20_regex_CORRECTED.aria` - Wild pointer syntax issues
- `sysUtils_15_git_CORRECTED.aria` - Similar patterns
- Others with `_CORRECTED.aria` suffix

---

## 2. Corruption Patterns Identified

### Pattern 1: Comment-Field Merging

**Problem**: Comments are merged into field definitions, breaking the type:name syntax.

**Corrupted Example**:
```aria
struct:Connection = {
    array<byte>  // Optional binary payload:payload;
    int32   // Timezone offset in minutes:offset;
};
```

**Correct Syntax**:
```aria
struct:Connection = {
    array<byte>:payload;  // Optional binary payload
    int32:offset;         // Timezone offset in minutes
};
```

**Regex to Detect**:
```regex
(\w+(?:<[^>]+>)?)\s+//[^:]+:(\w+);
```

**Fix Rule**: Move comment to end of line, restore `type:field_name;` format.

---

### Pattern 2: Truncated Struct Definitions

**Problem**: Struct field definitions are cut off mid-line.

**Corrupted Example** (netstat):
```aria
struct:Connection = {
    protocol;      // Protocol ID (6=TCP:u8;
    tx_queue;:u64;
};
```

**Correct Syntax**:
```aria
struct:Connection = {
    u8:protocol;      // Protocol ID (6=TCP)
    u64:tx_queue;
};
```

**Root Cause**: The conversion script appears to have truncated lines at field comments.

---

### Pattern 3: Wild Pointer Syntax

**Problem**: `wild` keyword usage is inconsistent.

**Observed Usages**:
```aria
wild void:data;           // Generic pointer
wild ASTNode:target;      // Typed pointer
wild byte:name;           // C string pointer
```

**Analysis**: The `wild` keyword appears to be Aria's equivalent of C's raw pointer. This may be intentional syntax but should be verified against the Aria language spec.

**Recommendation**: If `wild` is not valid Aria syntax, replace with:
- `ptr:data` for generic pointers
- `ptr<ASTNode>:target` for typed pointers

---

### Pattern 4: Const Declaration Syntax

**Problem**: Constant declarations have incorrect ordering.

**Corrupted Example**:
```aria
const APEP_MAGIC:u32 = 0x41504152;
```

**Expected Aria Syntax** (verify with spec):
```aria
const u32:APEP_MAGIC = 0x41504152;
// OR
u32:APEP_MAGIC = 0x41504152;  // Static constant
```

---

### Pattern 5: Semicolons After Block Ends

**Problem**: Some files have inconsistent semicolon usage after `}`.

**Corrupted**:
```aria
if (condition) {
    do_something();
};   // <- Semicolon after if block
```

**Correct** (per Aria spec):
```aria
if (condition) {
    do_something();
}    // No semicolon after control flow
```

**Note**: Semicolons ARE required after struct/enum/func definitions:
```aria
struct:Foo = { int64:x; };   // Semicolon required
func:bar = void() { };       // Semicolon required
```

---

### Pattern 6: Return Statement Syntax

**Problem**: Use of `return` instead of `pass()`/`fail()`.

**Corrupted**:
```aria
return value;
```

**Correct Aria Syntax**:
```aria
pass(value);   // For successful returns
fail(error);   // For error returns
```

---

### Pattern 7: Character Literals

**Problem**: Use of character literals instead of byte values.

**Corrupted**:
```aria
byte:newline = '\n';
```

**Correct Aria Syntax**:
```aria
byte:newline = 10;    // ASCII value for newline
// OR
byte:newline = 0x0A;  // Hex format
```

---

### Pattern 8: ANSI Escape Sequences

**Problem**: Use of `\x` hex escapes instead of octal.

**Corrupted**:
```aria
string:red = "\x1b[31m";
```

**Correct Aria Syntax**:
```aria
string:red = "\033[31m";  // Octal escape
```

---

## 3. File-Specific Analysis

### 3.1 netstat (sysUtils_12_netstat_CORRECTED.aria)

**Error Count**: ~108 (estimated)

**Critical Issues**:
1. Lines 35-38: Completely malformed struct definition
2. Multiple comment-field merging issues
3. Missing type declarations

**Restoration Priority**: HIGH

**Sample Fix** (lines 35-38):
```aria
// BEFORE (corrupted):
struct:Connection = {
    protocol;      // Protocol ID (6=TCP:u8;
    tx_queue;:u64;
};

// AFTER (corrected):
struct:Connection = {
    u8:protocol;     // Protocol ID (6=TCP, 17=UDP)
    u64:tx_queue;    // Transmit queue bytes
    u64:rx_queue;    // Receive queue bytes
    // ... additional fields from original spec
};
```

---

### 3.2 parallel (sysUtils_17_parallel_CORRECTED.aria)

**Error Count**: ~158 (estimated)

**Critical Issues**:
1. Line 18: Const syntax incorrect
2. Lines 35, 47: Comment-field merging in array declarations
3. Multiple struct definitions affected

**Restoration Priority**: MEDIUM

**Sample Fix** (line 35):
```aria
// BEFORE:
array<byte>  // Optional binary payload:payload;

// AFTER:
array<byte>:payload;  // Optional binary payload
```

---

### 3.3 regex (sysUtils_20_regex_CORRECTED.aria)

**Error Count**: Variable (wild/end keyword issues)

**Critical Issues**:
1. `wild` keyword usage throughout
2. `END` enum value (may conflict with reserved word?)
3. Comment-field merging

**Restoration Priority**: MEDIUM

**Wild Pointer Analysis**:
```aria
// Current:
wild void:data;
wild ASTNode:target;

// If 'wild' is not valid, replace with:
ptr:data;
ptr<ASTNode>:target;
```

---

## 4. Restoration Strategy

### Step 1: Automated Comment Fix

Create a script to fix comment-field merging:

```python
import re

def fix_comment_field(line):
    # Pattern: type  // comment:field;
    pattern = r'^(\s*)(\w+(?:<[^>]+>)?)\s+//([^:]+):(\w+);'
    match = re.match(pattern, line)
    if match:
        indent, type_name, comment, field = match.groups()
        return f"{indent}{type_name}:{field};  //{comment}"
    return line
```

### Step 2: Manual Struct Reconstruction

For severely corrupted structs (like netstat), manually reconstruct from the original `.txt` specification files.

### Step 3: Syntax Verification

After fixes, verify against Aria compiler:
```bash
ariac --check <file>.aria
```

### Step 4: Keyword Replacement

Replace non-standard keywords:
- `return` -> `pass()` or `fail()`
- `'\x'` escapes -> `'\0'` octal escapes
- Character literals -> byte values

---

## 5. Verification Checklist

For each restored file:

- [ ] All struct definitions parse correctly
- [ ] All func definitions have proper signatures
- [ ] No `return` statements (use pass/fail)
- [ ] No character literals (use byte values)
- [ ] No `\x` hex escapes (use octal)
- [ ] Semicolons only after definitions, not control flow
- [ ] Comments don't interfere with syntax
- [ ] File compiles with `ariac --check`

---

## 6. Original Source References

For complete reconstruction, reference the original Gemini responses:

| Utility | Original Source | Lines |
|---------|-----------------|-------|
| netstat | sysUtils_12_netstat.txt | 700+ |
| parallel | sysUtils_17_parallel.txt | 800+ |
| regex | sysUtils_20_regex.txt | 600+ |

These contain the complete architectural specifications and code snippets that can be used to manually reconstruct corrupted sections.

---

## 7. Recommended Next Steps

1. **Priority 1**: Fix netstat (most critical, many errors)
   - Manually reconstruct Connection struct from spec
   - Apply comment-field fix pattern
   - Test compilation

2. **Priority 2**: Fix parallel
   - Apply automated comment-field fix
   - Fix const syntax
   - Test compilation

3. **Priority 3**: Fix regex
   - Clarify `wild` keyword with Aria spec
   - Apply other pattern fixes
   - Test compilation

4. **Future**: Create a robust conversion script that properly handles:
   - Gemini markdown code blocks
   - Inline comments
   - Aria-specific syntax requirements

---

*This analysis provides the foundation for systematic restoration of corrupted utility files.*
