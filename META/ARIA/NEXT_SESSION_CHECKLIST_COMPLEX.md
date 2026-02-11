# Next Session Startup Checklist
**For:** Complex Numbers Trait Implementation Completion  
**Date:** February 11, 2026 (after 1-hour pause)

## Pre-Session Preparation

### 1. Context Loading (in order)
- [ ] Read: `META/ARIA/QUICK_REF_FEB11_COMPLEX.md` (1 min - overview)
- [ ] Read: `META/ARIA/HANDOFF_FEB11_2026_COMPLEX_NUMBERS.md` (5 min - full details)
- [ ] Optional: `META/ARIA/SAFETY_FEATURE_VERIFICATION_FEB11.md` (context only)

### 2. Verify Environment
```bash
cd /home/randy/Workspace/REPOS/aria

# Check compiler is built
ls -lh build/ariac

# Verify current error count (should be 9)
./build/ariac stdlib/complex.aria 2>&1 | grep "Summary"
```

Expected output: `Summary: 9 errors`

### 3. Confirm File States

**Trait files exist:**
- [ ] `stdlib/traits/numeric.aria` (89 lines)
- [ ] `stdlib/traits/numeric_impls.aria` (95 lines)

**Complex.aria updated:**
- [ ] Has trait imports at top (line 21-22)
- [ ] Struct uses `T: Numeric` constraint (line 40)
- [ ] All 11 functions use `T: Numeric` constraint

**Type checker modified:**
- [ ] `src/frontend/sema/type_checker.cpp` has generic support

## Session Tasks (10-20 minutes total)

### Task 1: Fix Initialization Errors (5-10 min)

**Use multi_replace_string_in_file for efficiency!**

File: `stdlib/complex.aria`

Replace at 5 locations (lines 164, 201, 262, 334, 376):
```aria
FROM: complex<*T>:result;
TO:   wild complex<*T>:result;
```

**Tip:** Search for `complex<*T>:result;` to find all instances quickly.

### Task 2: Verify Compilation (2 min)

```bash
cd /home/randy/Workspace/REPOS/aria
./build/ariac stdlib/complex.aria 2>&1 | tail -10
```

Expected: No errors or "Success" message

### Task 3: Test with Concrete Types (3 min)

**If complex_tests.aria exists:**
```bash
./build/ariac tests/stdlib/complex_tests.aria 2>&1
```

**If not, create simple test:**
```aria
use "stdlib/complex.aria".*;

pub func:main = int32() {
    complex<fix256>:z = complex_new(3.0, 4.0);
    complex<fix256>:w = complex_new(1.0, 2.0);
    complex<fix256>:sum = complex_add(z, w);
    pass(0);
};

pub func:failsafe = void(int32:code) {
};
```

### Task 4: Verify Success Criteria (2 min)

- [ ] Complex.aria compiles with 0 errors
- [ ] Can instantiate complex<fix256>
- [ ] Arithmetic operations work (add, mul, etc.)
- [ ] ERR propagation functions correctly

## If Something's Wrong

### Common Issues

**"Trait not found" errors:**
- Check trait imports in complex.aria (lines 21-22)
- Verify numeric.aria has main/failsafe functions

**"Still getting arithmetic errors":**
- Rebuild compiler: `make -C build -j8 ariac`
- Verify type_checker.cpp changes are present

**"Parser hangs on trait files":**
- Check trait method syntax (should be `add:Self(...)` not `func:add = ...`)

### Emergency Rollback

```bash
cd /home/randy/Workspace/REPOS/aria
git status  # See what's changed
git diff src/frontend/sema/type_checker.cpp  # Review changes
# If needed: git checkout -- <file>  # Restore original
```

## Success Indicators

✅ **Command output shows:**
```
Summary: 0 errors
```
or
```
[IR generation output]
[LLVM output]
Success
```

✅ **Can compile complex with different numeric types:**
- `complex<fix256>:z = ...` ✓
- `complex<tfp64>:z = ...` ✓
- `complex<tbb64>:z = ...` ✓

## After Successful Completion

### Next Phase Options

**Option A: Continue with Safety Documentation (2-3 hours)**
- Consolidate all safety feature docs
- Create comprehensive ARIA_SAFETY_REFERENCE.md
- Define fuzzing targets

**Option B: Take a break and plan next session**
- Complex numbers DONE
- Safety docs can be separate session
- Allows time for other priorities

### Update Status Documents

1. Mark complex numbers as COMPLETE in todo list
2. Update START_HERE_NEXT_SESSION.md
3. Add note to relevant META/ARIA/ status docs

## User Preferences to Remember

- **Fix now, not later:** Object impermanence means close loops immediately
- **Document everything:** Future-self needs trails
- **Quality over speed:** "Unlimited time (within reason)"
- **Test real systems:** No placeholder implementations
- **Avoid deep rabbit holes:** Stay focused unless nugget is discovered

## Context Expiration Warning

If this session starts and >24 hours have passed:
- Verify no other work was done on complex numbers
- Check git log for conflicting changes
- Re-verify compiler state before making changes

---

**Ready to proceed?** 
1. Load context documents
2. Verify environment  
3. Make the 5 trivial fixes
4. Test and celebrate! 🎉

**Estimated total time:** 15-20 minutes from context load to completion
