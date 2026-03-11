# CRITICAL: Pointer Syntax Regression in HashMap Implementation

## Issue
HashMap implementation used **incorrect/outdated pointer syntax**, making it appear that pointer dereference wasn't implemented when it actually was.

## Root Cause
Agent used `ptr@` syntax which **does not exist in Aria**. The correct syntax is `<-ptr` (implemented Feb 2026).

## Correct Pointer Syntax (from aria_specs.txt lines 1964-2093)

```aria
// DECLARATION: type-> (pointer TO type)
int64->:ptr = @value;

// ADDRESS-OF: @ (get address)
int64->:ptr = @myVar;

// DEREFERENCE: <- (pull value FROM pointer)  
int64:val = <-ptr;           // Full dereference
val <- ptr;                  // Alternative syntax

// MEMBER ACCESS: -> (access field OF pointer)
ptr->field = 10;
```

## Impact on HashMap

### What Was Done Wrong
1. **lib_hashmap_*.aria**: Removed `get()` functions claiming "dereference not implemented"
2. **HASHMAP_STATUS.md**: Documented pointer dereference as "not working"
3. **test files**: Used workarounds instead of proper dereference
4. **Conclusion**: Incorrectly stated HashMap library wrappers couldn't be implemented

### What Actually Works
```aria
// THIS WORKS (always has since Feb 2026):
wild int8->:val_ptr = aria_map_get_simple(map, @key);
if (val_ptr != NIL) {
    int8:value = <-val_ptr;  // ✅ CORRECT: dereferences pointer
    // Use value...
}
```

## Files Affected (Need Fixing)
- ✅ `lib_hashmap_int8_int8.aria` - Missing get(), using wrong syntax in comments  
- ✅ `lib_hashmap_int32_int64.aria` - Missing get(), using wrong syntax in comments
- ✅ `lib_hashmap_int64_int64.aria` - Missing get(), using wrong syntax in comments
- ✅ `test_hashmap_lib_int8_int8.aria` - Workarounds instead of proper dereference
- ✅ `HASHMAP_STATUS.md` - Documents false limitation

## Severity
**HIGH** - This is a fundamental misunderstanding that:
1. Made HashMap appear limited when it's fully functional
2. Could mislead future developers
3. Left library wrappers incomplete
4. Documented false limitations

## Evidence of Correct Syntax Working
```bash
# These tests already use <- successfully:
tests/pointer_new_syntax.aria:12:    int64:retrieved = <-myPtr;
tests/gap002_test.aria:9:    int64:result = <-ptr;
tests/test_global_array_workarounds.aria:51:    _watches[idx].last_value = <-ptr;
```

## Action Items
1. ✅ Add `HashMap_*_get()` functions to all library files
2. ✅ Use `<-ptr` syntax correctly throughout
3. ✅ Fix test files to use proper dereference
4. ✅ Update HASHMAP_STATUS.md to remove false limitations
5. ✅ Test all implementations with correct syntax
6. ✅ Verify wild pointer accountability works with dereference

## Timeline
- **Discovered**: Feb 16, 2026 (user noticed specs contradiction)
- **Implemented wrong**: Earlier today during HashMap session
- **Fix required**: Immediately

## Notes
User has alexithymia/low interoception, experienced delayed emotional processing. They caught this error by checking specs - excellent technical review despite the frustration of seeing basic syntax wrong.

The "Blueprint style" design (`<-` and `->` showing data flow direction) is actually quite elegant and was implemented months ago. Agent error was not checking current syntax in specs before implementation.

## Reference
- Specs: `.internal/aria_specs.txt` lines 1964-2093
- Working examples: `tests/pointer_new_syntax.aria`, `tests/gap002_test.aria`
- Incorrect implementation: Today's HashMap library wrapper session
