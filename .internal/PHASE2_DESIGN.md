# Phase 2: Control Flow Analysis Design

## Goal
Track Result<T> checks through control flow (if/else branches, early returns) instead of just same-scope tracking.

## Current Limitation (Phase 1)
```aria
Result<int64>:r = get_number();

if (r.is_error == false) {
    int64:x = r.value;  // ERROR - Phase 1 doesn't know we checked in the condition
}
```

Phase 1 only tracks `.is_error` access as member access, not in comparison expressions.

## Design Options

### Option A: Full Control Flow Graph (CFG)
**Pros:**
- Handles all flow (loops, goto, complex nesting)
- Industry standard approach
- Extensible to other analyses

**Cons:**
- Complex to implement
- Overkill for Phase 2
- Requires separate CFG construction phase

### Option B: AST Walk with State Tracking (CHOSEN)
**Pros:**
- Simpler implementation
- Incremental (can add features step by step)
- Reuses existing AST traversal
- Easier to debug

**Cons:**
- Harder to extend to loops (Phase 3)
- Less formal

**Decision:** Start with Option B, migrate to Option A if needed for Phase 3.

## Data Structure

```cpp
// Result check state during analysis
struct ResultCheckState {
    enum class State {
        UNCHECKED,       // Haven't checked .is_error yet
        CHECKED,         // Checked .is_error, but don't know the value
        KNOWN_ERROR,     // Know .is_error == true
        KNOWN_SUCCESS    // Know .is_error == false
    };
    
    // Map variable name -> state
    std::unordered_map<std::string, State> states;
    
    // Operations
    void markChecked(const std::string& varName);
    void markKnownError(const std::string& varName);
    void markKnownSuccess(const std::string& varName);
    bool isChecked(const std::string& varName) const;
    State getState(const std::string& varName) const;
    
    // Merge states from two branches (conservative join)
    // If branches disagree, result is CHECKED (known checked, value unknown)
    static ResultCheckState merge(const ResultCheckState& then_state, 
                                   const ResultCheckState& else_state);
};
```

## Analysis Algorithm

### 1. Track State Through Statements
Replace simple set tracking with state object:
- Old: `checkedResults` (set of checked variable names)
- New: `ResultCheckState` (maps names to detailed state)

### 2. Analyze Conditions
When we see `if (condition)`:
1. Analyze condition to extract Result checks
2. If condition is `r.is_error == true`: then branch knows ERROR
3. If condition is `r.is_error == false`: then branch knows SUCCESS
4. If condition is `!r.is_error`: then branch knows SUCCESS (if Aria supports this)

### 3. Branch State Propagation
```cpp
void TypeChecker::checkIfStmt(IfStmt* stmt) {
    // Analyze condition
    Type* condType = inferType(stmt->condition.get());
    
    // Extract Result knowledge from condition
    ResultCheckState thenState = currentState;  // Copy current
    ResultCheckState elseState = currentState;  // Copy current
    
    analyzeConditionForResultChecks(stmt->condition.get(), thenState, elseState);
    
    // Check then branch with then state
    ResultCheckState savedState = currentState;
    currentState = thenState;
    checkStmt(stmt->thenBranch.get());
    ResultCheckState afterThen = currentState;
    
    // Check else branch with else state (if exists)
    ResultCheckState afterElse;
    if (stmt->elseBranch) {
        currentState = elseState;
        checkStmt(stmt->elseBranch.get());
        afterElse = currentState;
    } else {
        afterElse = elseState;  // No else = same as entry
    }
    
    // Merge states after if/else
    currentState = ResultCheckState::merge(afterThen, afterElse);
}
```

### 4. Condition Analysis
```cpp
void TypeChecker::analyzeConditionForResultChecks(
    ASTNode* condition,
    ResultCheckState& thenState,
    ResultCheckState& elseState
) {
    // Check for comparison: r.is_error == true/false
    if (condition->type == ASTNode::NodeType::BINARY_OP) {
        BinaryExpr* binExpr = static_cast<BinaryExpr*>(condition);
        
        if (binExpr->op.type == TokenType::TOKEN_EQUAL_EQUAL ||
            binExpr->op.type == TokenType::TOKEN_BANG_EQUAL) {
            
            // Check if one side is .is_error member access
            MemberAccessExpr* memberAccess = nullptr;
            LiteralExpr* literal = nullptr;
            
            if (binExpr->left->type == ASTNode::NodeType::MEMBER_ACCESS) {
                memberAccess = static_cast<MemberAccessExpr*>(binExpr->left.get());
                if (binExpr->right->type == ASTNode::NodeType::LITERAL) {
                    literal = static_cast<LiteralExpr*>(binExpr->right.get());
                }
            } else if (binExpr->right->type == ASTNode::NodeType::MEMBER_ACCESS) {
                memberAccess = static_cast<MemberAccessExpr*>(binExpr->right.get());
                if (binExpr->left->type == ASTNode::NodeType::LITERAL) {
                    literal = static_cast<LiteralExpr*>(binExpr->left.get());
                }
            }
            
            if (memberAccess && literal && memberAccess->member == "is_error") {
                // Get the Result variable name
                std::string varName = getVariableName(memberAccess->object.get());
                
                // Get the boolean value
                if (std::holds_alternative<bool>(literal->value)) {
                    bool errorValue = std::get<bool>(literal->value);
                    bool comparing_equal = (binExpr->op.type == TokenType::TOKEN_EQUAL_EQUAL);
                    
                    // Determine what each branch knows
                    if (comparing_equal) {
                        // r.is_error == true
                        if (errorValue) {
                            thenState.markKnownError(varName);
                            elseState.markKnownSuccess(varName);
                        } else {
                            // r.is_error == false
                            thenState.markKnownSuccess(varName);
                            elseState.markKnownError(varName);
                        }
                    } else {
                        // r.is_error != true (inverse)
                        if (errorValue) {
                            thenState.markKnownSuccess(varName);
                            elseState.markKnownError(varName);
                        } else {
                            thenState.markKnownError(varName);
                            elseState.markKnownSuccess(varName);
                        }
                    }
                }
            }
        }
    }
}
```

### 5. State Merging
```cpp
ResultCheckState ResultCheckState::merge(
    const ResultCheckState& state1,
    const ResultCheckState& state2
) {
    ResultCheckState result;
    
    // Merge all variables from both states
    std::set<std::string> allVars;
    for (const auto& [name, _] : state1.states) allVars.insert(name);
    for (const auto& [name, _] : state2.states) allVars.insert(name);
    
    for (const std::string& varName : allVars) {
        State s1 = state1.getState(varName);
        State s2 = state2.getState(varName);
        
        // Conservative join
        if (s1 == s2) {
            result.states[varName] = s1;  // Both agree
        } else if (s1 == State::UNCHECKED || s2 == State::UNCHECKED) {
            result.states[varName] = State::UNCHECKED;  // One path didn't check
        } else {
            result.states[varName] = State::CHECKED;  // Both checked, different values
        }
    }
    
    return result;
}
```

### 6. Early Return Handling
```cpp
// After: if (r.is_error) { return; }
// We know: r.is_error == false

void TypeChecker::checkIfStmt(IfStmt* stmt) {
    // ... existing code ...
    
    // Check if then branch always returns/exits
    if (branchAlwaysReturns(stmt->thenBranch.get())) {
        // Then branch exits, so code after if has else state
        currentState = elseState;
        return;
    }
    
    // Check if else branch always returns/exits
    if (stmt->elseBranch && branchAlwaysReturns(stmt->elseBranch.get())) {
        // Else branch exits, so code after if has then state
        currentState = thenState;
        return;
    }
    
    // Normal case: merge both branches
    currentState = ResultCheckState::merge(thenState, elseState);
}

bool TypeChecker::branchAlwaysReturns(ASTNode* branch) {
    // Check if all code paths in branch return/exit
    // For now: simple check for return statement at end
    // Phase 3: Full control flow analysis
    
    if (branch->type == ASTNode::NodeType::BLOCK) {
        BlockStmt* block = static_cast<BlockStmt*>(branch);
        if (!block->statements.empty()) {
            ASTNode* last = block->statements.back().get();
            return last->type == ASTNode::NodeType::RETURN ||
                   last->type == ASTNode::NodeType::PASS ||
                   last->type == ASTNode::NodeType::FAIL;
        }
    }
    
    return false;
}
```

## Implementation Plan

### Step 1: Add ResultCheckState Class
- Define in type_checker.h
- Implement in type_checker.cpp
- Add to TypeChecker as member (replace checkedResults set)

### Step 2: Update Existing Enforcement
- Change `checkedResults.find()` to `currentState.getState()`
- Change `checkedResults.insert()` to `currentState.markChecked()`

### Step 3: Add Condition Analysis
- Implement `analyzeConditionForResultChecks()`
- Detect `r.is_error == true/false` patterns

### Step 4: Update If Statement Checking
- Branch states before checking if/else
- Merge states after
- Handle early returns

### Step 5: Test
- Create test for if-check-then-use
- Create test for if-check-return-else-use
- Create test for nested conditionals

## Test Cases

### Test 1: Basic If Check
```aria
func:test = int32() {
    Result<int64>:r = get_number();
    
    if (r.is_error == false) {
        int64:x = r.value;  // Should PASS - we know is_error == false
    }
    
    pass(0i32);
}
```

### Test 2: Early Return
```aria
func:test = int32() {
    Result<int64>:r = get_number();
    
    if (r.is_error) {
        fail("Error occurred");
    }
    
    // After early return, we know r.is_error == false
    int64:x = r.value;  // Should PASS
    
    pass(0i32);
}
```

### Test 3: Nested Conditionals
```aria
func:test = int32() {
    Result<int64>:r1 = get_number();
    Result<int64>:r2 = get_number();
    
    if (r1.is_error == false) {
        if (r2.is_error == false) {
            int64:sum = r1.value + r2.value;  // Should PASS - both checked
        }
    }
    
    pass(0i32);
}
```

### Test 4: Both Branches Check (Conservative Merge)
```aria
func:test = int32() {
    Result<int64>:r = get_number();
    
    if (some_condition) {
        if (r.is_error == false) {
            // In this branch, we know is_error == false
        }
    } else {
        if (r.is_error == false) {
            // In this branch, we also know is_error == false
        }
    }
    
    // After both branches check the same way, we can use it
    int64:x = r.value;  // Should PASS if both branches checked
}
```

### Test 5: Divergent Branches (Should Fail)
```aria
func:test = int32() {
    Result<int64>:r = get_number();
    
    if (some_condition) {
        if (r.is_error == false) {
            int64:x = r.value;  // OK in then branch
        }
    } else {
        // Else branch doesn't check
    }
    
    int64:y = r.value;  // Should ERROR - else branch didn't check
    
    pass(0i32);
}
```

## Success Criteria

Phase 2 is complete when:
1. ✅ ResultCheckState class implemented
2. ✅ If statement branches propagate knowledge
3. ✅ Early returns handled correctly
4. ✅ State merging works conservatively
5. ✅ All 5 test cases pass
6. ✅ No regressions in Phase 1 tests

## Future Work (Phase 3)

- Loop handling (conservative at loop headers)
- Nested functions and closures
- Full CFG construction for complex control flow
- Goto statement handling
- Switch/pick statement analysis
