# Aria Pointer Syntax - Current Status

## Date: January 6, 2026

## Randy's Design Philosophy

### Why `@` for Addresses?
**Visual pattern matching + mental model:**
- `@` is universally associated with addresses (email: `user@domain.com`)
- Reads naturally: `int64@:myPtr = @myInt` 
  - "int64 address named myPtr at the address of myInt"
- `*` doesn't convey meaning visually - it's arbitrary
- `@` makes the concept of "address" immediately recognizable

### Why `->` for Pointer Member Access?
**Arrow literally points:**
- `->` is a visual arrow pointing at something
- Natural meaning: "follow the pointer to the member"
- Keeps `.` for regular struct/class member access (consistency with conventions)

### Why `.` for Direct Member Access?
**Don't reinvent working conventions:**
- `.` for member access is ubiquitous across languages
- Works for both structs and classes
- Familiar, logical, no need to change

## Current Implementation Status

### ✅ WORKING:

**1. Address-of operator**
```aria
int64:myInt = 345;
int64@:myPtr = @myInt;  // Get address
```
- Syntax: `@variable`
- Status: ✅ **Fully implemented and tested**
- Randy's design: @ already means "at address" (email: user@domain.com)

**2. Pointer type declaration**  
```aria
int64@:ptr;    // Pointer to int64
int8@:str;     // Pointer to int8 (like char*)
Point@:ptr;    // Pointer to struct
```
- Syntax: `type@:name`
- Status: ✅ **Fully implemented**

**3. Array indexing** (workaround for dereference)
```aria
int64@:ptr = @value;
int64:retrieved = ptr[0];  // Get value through pointer
```
- Syntax: `ptr[index]`
- Status: ✅ **Works perfectly, used throughout all 26 working stdlib modules**

**4. Direct member access (structs/classes)**
```aria
Point:p = Point{ x: 10, y: 20 };
int32:val = p.x;  // Direct member access
p.y = 100;        // Modify member
```
- Syntax: `var.member`
- Status: ✅ **Fully implemented**
- Randy's design: Don't reinvent wheels, `.` is universal

### ❌ NOT YET IMPLEMENTED:

**1. Standalone pointer dereference**
```aria
int64@:ptr = @value;
int64:retrieved = ptr@;  // ❌ NOT IMPLEMENTED
```
- Desired syntax: `ptr@` (consistent with `@` theme)
- Status: ❌ "syntax TBD in type system design" (aria_specs.txt line 432)
- Workaround: Use `ptr[0]` for now

**2. Arrow operator for pointer member access**
```aria
Point@:ptr = @p;
int32:x = ptr->x;   // ❌ NOT IMPLEMENTED IN CODEGEN
ptr->y = 100;       // ❌ NOT IMPLEMENTED IN CODEGEN
```
- Desired syntax: `ptr->member`
- Randy's design: Arrow literally points at the member
- Status: ❌ **Parsed but not implemented in codegen**
- Token exists: `TOKEN_ARROW` in lexer and parser
- Missing: Backend IR generation for arrow operator
- Workaround: Use `ptr[0].member` for now (dereference then access)

**Alternative syntaxes tested (all fail):**
- `@ptr` - Wrong, this gets address OF the pointer
- `*ptr` - Wrong, `*` only valid in extern blocks for FFI
- `ptr@` - Not implemented yet

## Implementation Priority

### Current State:
The workaround using `ptr[0]` is functional and used throughout the entire stdlib. All 26/27 working modules use this pattern successfully.

### When to Implement `ptr@`:
**Low priority** because:
1. Array indexing workaround is clean and works
2. All stdlib modules compile with it
3. Semantic is clear: `ptr[0]` means "first element at address"
4. `->` operator handles the most common case (struct member access)

**Benefits of implementing `ptr@` later:**
1. Consistency with `@` address-of operator
2. Clearer intent: `ptr@` vs `ptr[0]`
3. Type safety: `ptr@` implies single value, `ptr[0]` implies array

### Recommended Implementation:
When pointer dereference is implemented, use `ptr@`:
- Consistent with `@variable` for address-of
- Reads as "the value at pointer"
- Completes the symmetry: `@var` gets address, `ptr@` gets value

## Summary

Randy's pointer design is **logically consistent and visually intuitive**:

**What works now:**
- ✅ `@variable` - address-of
- ✅ `type@:ptr` - pointer declaration
- ✅ `ptr[0]` - dereference workaround
- ✅ `var.member` - direct member access

**What's NOT implemented yet:**
- ⏳ `ptr@` - standalone dereference (low priority, workaround exists)
- ⏳ `ptr->member` - pointer member access (parsed, needs codegen implementation)

The design philosophy makes sense:
- `@` = addresses (email mental model - user@domain.com)
- `->` = pointing arrow (visual meaning - arrow points at target)
- `.` = member access (don't reinvent wheels - universal convention)
- `*` = wildcard/generic (file patterns *.txt, already mental model for "any")

**26 out of 27 stdlib modules** work with current implementation using workarounds.

## Design Philosophy Notes

Randy's choices are **optimized for neurodivergent pattern matching**:

1. **`@` for addresses**: Reuses existing mental model (email addresses)
2. **`*` for generics**: Matches wildcard pattern (*.txt, regex *)  
3. **`->` for pointer navigation**: Arrow literally points
4. **`.` for members**: Works everywhere, no need to change

These aren't arbitrary - they leverage visual patterns and existing associations, making the syntax more intuitive for those who think in patterns rather than arbitrary conventions.

**This is a feature, not a bug.** Designing for neurodivergent cognition often produces MORE logical systems, not less.
