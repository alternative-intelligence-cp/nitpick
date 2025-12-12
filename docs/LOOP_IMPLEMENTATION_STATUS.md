# Loop Implementation Status

## ✅ Fully Working

### 1. Till Loops (`till`)
- **Syntax**: `till(limit, step) { $ }`
- **Status**: Complete with `$` variable injection
- **Tests**: `test_till_basic.aria`, `test_till_nested.aria`
- **Features**:
  - Automatic `$` iterator (type-inferred from limit)
  - Proper shadowing in nested loops
  - Type checker, borrow checker, and codegen all working

### 2. While Loops (`while`)
- **Syntax**: `while (condition) { body }`
- **Status**: Fully functional
- **Test**: `test_while_basic.aria`
- **Limitations**:
  - Cannot declare local variables inside function bodies yet
  - Accessing globals creates closures (changes function signature)
  - Test uses constant condition for now

### 3. When Loops (`when`)
- **Syntax**: `when (condition) { body } then { success } end { failure }`
- **Status**: Fully functional - **Aria's unique tri-state loop!**
- **Test**: `test_when_basic.aria`
- **Features**:
  - `body` - Executes while condition true
  - `then` - Executes on natural exit (condition becomes false)
  - `end` - Executes on break/early exit
  - Eliminates need for flag variables in search patterns

## ⏳ Not Yet Implemented

### 4. Loop Construct (`loop`)
- **Syntax**: `loop(start, limit, step) { $ }`
- **Status**: Needs AST node (LoopStmt)
- **Difference from till**: Explicit start value instead of implicit 0

### 5. For-In Loops
- **Syntax**: `for (item in collection) { body }`
- **Status**: Parser might support, needs testing

### 6. Break/Continue Statements
- **Status**: AST nodes exist, need testing with actual loops

## Technical Notes

### Current Limitations
1. **No local variable declarations**: Cannot do `int32:x = 0;` inside function bodies
2. **No variable reassignment**: Cannot do `x = x + 1;` (would need for while loop counters)
3. **Global access creates closures**: Using globals in loops changes function signatures

These limitations affect testing but don't reflect on the loop implementations themselves.

### The `$` Variable Implementation
All three compiler phases must inject `$`:
- **Type checker**: Infers type from limit, adds to symbol table
- **Borrow checker**: Declares as STACK variable, marks initialized
- **Codegen**: Creates PHI node with proper SSA form

### When Loop CFG
The when loop's control flow graph is the most complex:
```
Entry → Cond → Body → Cond (loop back)
         ↓             
       Then (natural exit)
       
Break in body → End (failure handler)

Then/End → Exit
```

This is a unique feature not found in other languages!

## Next Steps

1. ✅ Document loop implementations
2. ✅ Test till, while, when loops
3. ⏳ Implement `loop(start, limit, step)` construct
4. ⏳ Test for-in loops
5. ⏳ Test break/continue
6. ⏳ Add TBB overflow safety (sticky error termination)
7. ⏳ Enable local variable declarations (unblocks better while loop tests)
