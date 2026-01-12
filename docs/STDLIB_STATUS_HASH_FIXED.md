# Hash Module Status - Pointer Fixes Complete

## Date: January 6, 2026

## Summary
✅ **All pointer dereference syntax errors fixed**
⏳ **Semantic errors remain** (uint32 vs int32, bitwise operators)

## Changes Made

### Pointer Arithmetic → Array Indexing
Converted all functions from pointer arithmetic + dereference to direct array indexing:

**Pattern Applied:**
```aria
// Before (broken - dereference not implemented):
int64:offset = i;
int8@:ptr = (data + offset);
int8:value = ptr@;  // ❌ Syntax error

// After (works - array indexing):
int8:value = data[i];  // ✅ Compiles
```

### Functions Fixed (5 total):

1. **hash_string_djb2** (line ~29)
   - Changed: `char_ptr = str + offset; c = char_ptr@`
   - To: `c = str[i]`

2. **hash_string_sdbm** (line ~45)
   - Changed: Function signature `int8*:str` → `int8@:str`
   - Changed: `char_ptr = str + offset; c = char_ptr@`
   - To: `c = str[i]`

3. **checksum_simple** (line ~79)
   - Changed: `byte_ptr = data + offset; b = byte_ptr@`
   - To: `b = data[i]`

4. **checksum_xor** (line ~96)
   - Changed: `byte_ptr = data + offset; b = byte_ptr@`
   - To: `b = data[i]`

5. **checksum_fletcher16** (line ~113)
   - Changed: `byte_ptr = data + offset; b = byte_ptr@`
   - To: `b = data[i]`

## Remaining Issues (23 semantic errors)

### 1. Integer Overflow
**Line 6:** `2654435761` exceeds int32 max (2147483647)
- **Options:**
  - Use uint32 for hash values
  - Use int64
  - Use smaller constant

### 2. Bitwise Operators Require Unsigned Types
**Multiple locations:** `<<`, `>>`, `|`, `^` operators need uint32
- **Affected functions:**
  - hash_combine (shifts and XOR)
  - hash_string_djb2 (left shift)
  - hash_string_sdbm (left shifts)
  - checksum_fletcher16 (bitwise AND)

- **Solution:** Convert hash functions to use uint32 throughout

### 3. While Loop Condition Type
**Lines 26, 50:** While condition must be explicit bool
- **Current:** `while (cont)` where `cont` is int32
- **Required:** `while (cont != 0)` or change `cont` to bool type

### 4. Undefined Identifiers
Some variables showing as undefined - may be related to previous edit inconsistencies
- Need to verify all variable declarations intact

## Next Steps

**Option A: Convert to uint32** (Recommended)
- Hash functions naturally use unsigned arithmetic
- Matches common hash implementations
- Allows full int32 range for hash values

**Option B: Use int64**
- Avoids overflow
- Less idiomatic for hash functions
- Still needs unsigned types for bitwise ops

**Option C: Fix incrementally**
- Address overflow with cast
- Add uint32 casts for bitwise ops
- Keep mixing signed/unsigned (not ideal)

## Conclusion

✅ **Pointer syntax barrier removed**
- All `@` dereference attempts eliminated
- Array indexing pattern successfully applied
- Module now parses correctly

⏳ **Type system decisions needed**
- uint32 vs int32 for hash return types
- Consistent unsigned arithmetic throughout
- This is design work, not syntax fixing

**Recommendation:** Pause hash.aria work, move to simpler modules (string/path/time type casts), return to hash design later.

