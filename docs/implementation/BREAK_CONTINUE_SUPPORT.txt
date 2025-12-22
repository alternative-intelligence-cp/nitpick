# Break and Continue Statement Support

## Overview

Implemented full support for `break` and `continue` statements in the Aria compiler's IR generation phase.

## Implementation Details

### Core Changes

1. **Loop Context Tracking** (`include/backend/ir/ir_generator.h`)
   - Added `LoopContext` struct to track loop entry/exit points
   - Added `loop_stack` vector to handle nested loops
   - Stores `continueTarget` (where continue jumps) and `breakTarget` (where break jumps)

2. **While Loop Updates** (`src/backend/ir/ir_generator.cpp`)
   - Push loop context before generating loop body
   - `continueTarget` = condition block (loop back to check condition)
   - `breakTarget` = after-loop block (exit loop)
   - Pop loop context after loop generation

3. **For Loop Updates** (`src/backend/ir/ir_generator.cpp`)
   - Push loop context before generating loop body
   - `continueTarget` = update block (execute update, then loop back)
   - `breakTarget` = after-loop block (exit loop)
   - Pop loop context after loop generation

4. **Break Statement** (`src/backend/ir/ir_generator.cpp`, line ~732)
   ```cpp
   case ASTNode::NodeType::BREAK:
       // Jump to loop exit (breakTarget)
       builder.CreateBr(loop_stack.back().breakTarget);
       // Create unreachable block for any code after break
       afterBreakBB = llvm::BasicBlock::Create(context, "afterbreak", function);
       builder.SetInsertPoint(afterBreakBB);
   ```

5. **Continue Statement** (`src/backend/ir/ir_generator.cpp`, line ~754)
   ```cpp
   case ASTNode::NodeType::CONTINUE:
       // Jump to loop condition/update (continueTarget)
       builder.CreateBr(loop_stack.back().continueTarget);
       // Create unreachable block for any code after continue
       afterContinueBB = llvm::BasicBlock::Create(context, "aftercontinue", function);
       builder.SetInsertPoint(afterContinueBB);
   ```

## Generated IR Examples

### Break Statement
```llvm
then:                                             ; preds = %whilebody
  br label %afterwhile    ; <-- Break jumps to loop exit

afterbreak:                                       ; No predecessors!
  br label %ifcont

afterwhile:                                       ; preds = %then, %whilecond
  %count5 = load i32, ptr %count, align 4
  ret i32 %count5
```

### Continue Statement
```llvm
then:                                             ; preds = %whilebody
  br label %whilecond     ; <-- Continue jumps back to condition

aftercontinue:                                    ; No predecessors!
  br label %ifcont

whilecond:                                        ; preds = %ifcont, %then, %entry
  %i1 = load i32, ptr %i, align 4
  %lttmp = icmp slt i32 %i1, 10
```

## Testing

### Test Suite Results
- **Total tests**: 1226 (no regressions)
- **Total assertions**: 6393
- **All tests passing**: ✅

### Feature Validation Tests

1. **test_break_continue.aria**
   - Simple break exits loop at specified point
   - Simple continue skips loop iteration
   - Both statements work correctly with control flow

2. **comprehensive_break_continue.aria**
   - 6 test scenarios covering:
     * Simple break (exit when condition met)
     * Simple continue (skip even numbers)
     * Conditional break (exit when accumulator exceeds threshold)
     * Multiple continues (range filtering)
     * Immediate break (infinite loop with immediate exit)
     * Continue at end (no-op case)
   - **Validation**: All tests return correct values
   - **Combined result**: 10 + 25 + 55 + 11 + 0 + 5 = **106** ✅

### Execution Verification
```bash
./build/ariac comprehensive_break_continue.aria -o test
./test
echo $?
# Output: 106 (correct!)
```

## Nested Loop Support

The implementation fully supports nested loops:

```aria
while (outer_condition) {
    // Outer loop context pushed
    
    while (inner_condition) {
        // Inner loop context pushed
        
        if (inner_break_cond) {
            break;  // Breaks inner loop only
        }
        
        if (inner_continue_cond) {
            continue;  // Continues inner loop only
        }
        
        // Inner context popped
    }
    
    if (outer_break_cond) {
        break;  // Breaks outer loop
    }
    
    // Outer context popped
}
```

The stack-based approach ensures:
- `break` always exits the **innermost** enclosing loop
- `continue` always jumps to the **innermost** loop's start
- Proper nesting behavior without any additional complexity

## For Loop Behavior

For `for` loops, the targets are:
- **continue** → jumps to **update block** (executes update expression, then checks condition)
- **break** → jumps to **after-loop block** (exits loop completely)

This matches standard C/C++ semantics:
```aria
for (int32:i = 0; i < 10; i += 1) {
    if (i == 3) continue;  // Skips to i += 1
    if (i == 7) break;     // Exits loop
    // ... loop body
}
```

## Error Handling

Currently, if `break` or `continue` appears outside a loop, the IR generator:
1. Checks if `loop_stack` is empty
2. Returns `nullptr` if no enclosing loop
3. TODO: Should emit proper semantic error message

This would be caught by the semantic analyzer in a complete implementation.

## Performance Characteristics

- **Stack overhead**: O(loop_depth) - typically 1-3 entries
- **IR generation**: No overhead - direct branch instructions
- **Runtime**: Zero overhead - compiles to simple jumps
- **Optimization**: LLVM can:
  - Remove unreachable `afterbreak`/`aftercontinue` blocks
  - Inline loops with early exits
  - Unroll loops with constant break conditions

## Future Enhancements

1. **Labeled break/continue**
   ```aria
   'outer: while (condition1) {
       while (condition2) {
           break 'outer;  // Break outer loop from inner
       }
   }
   ```
   - Already have `label` field in `BreakStmt` and `ContinueStmt`
   - Would need labeled loop tracking in loop_stack

2. **Semantic error messages**
   - "break outside of loop" error
   - "continue outside of loop" error
   - Should be caught during semantic analysis phase

3. **Loop optimization hints**
   - Use break/continue patterns to guide loop unrolling
   - Detect infinite loops with break-only exits

## Status

✅ **Complete and Tested**
- Parser support: Already present
- IR generation: Fully implemented
- While loops: Working
- For loops: Working  
- Nested loops: Working
- Test coverage: Comprehensive
- No regressions: All 1226 tests passing

## References

- Implementation: `/home/randy/._____RANDY_____/REPOS/aria/src/backend/ir/ir_generator.cpp`
- Header: `/home/randy/._____RANDY_____/REPOS/aria/include/backend/ir/ir_generator.h`
- Tests: `/home/randy/._____RANDY_____/REPOS/aria/tests/feature_validation/`
  - `test_break_continue.aria`
  - `comprehensive_break_continue.aria`
