# Future Feature Tests

**DO NOT RUN THESE TESTS** - They document requirements for unimplemented features.

## Status: Specification, Not Validation

These tests define the behavior of features not yet built.
They will fail until implementation is complete.

## Blocked Features

### batch02_gemini_audit_fixes.aria
**Requires**: trit/nit type implementation
- `trit`: Balanced ternary digit (-1, 0, 1)
- `nit`: Balanced nonary digit (-4..4)
- Runtime functions: `trit_and()`, `trit_or()`, `nit_and()`, `nit_or()`
- **Priority**: HIGH - Core Nikola substrate requirement (like `int` for C)

### safety_critical_suite.aria  
**Requires**: Multiple subsystems
- Full IO module (`io.read()`, `io.write()`)
- Complete TBB runtime (`tbb_widen()`, conversion functions)
- Additional stdlib functions (`int1024_pow()`, string operations)
- **Priority**: MEDIUM - Extension of existing systems

## Implementation Order

1. **trit/nit types** (blocking Nikola substrate)
2. **IO module completion** (safety-critical validation)
3. **Extended stdlib** (coverage expansion)

## When To Move Back

Tests return to main suite when:
- ✅ Feature fully implemented in compiler
- ✅ Runtime functions available
- ✅ Test passes without errors
- ✅ No false positives (real validation, not aspirational)

---

**Blueprint Rule**: Never claim a feature works until you can prove it under load.

