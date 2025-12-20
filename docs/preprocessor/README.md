# Aria Preprocessor

## Overview

The Aria preprocessor is a NASM-style macro system with automatic hygiene that runs before lexical analysis. It provides powerful metaprogramming capabilities while preventing variable collision bugs through automatic hygienic renaming.

## Features

### 1. Macro Definitions (`%macro`/`%endmacro`)

Define reusable code templates with parameters:

```aria
%macro GEN_ABS 1
func abs_%1(int32:x) -> int32 {
    int32:temp = x
    if temp < 0 {
        temp = -temp
    }
    result temp
}
%endmacro

// Generate specializations
GEN_ABS(int8)
GEN_ABS(int32)
```

### 2. Macro Hygiene (Automatic)

**Problem**: Traditional macros can have variable collisions when invoked multiple times:

```aria
%macro SWAP 2
int32:temp = %1
%1 = %2
%2 = temp
%endmacro

// Without hygiene: temp variable collides!
SWAP(a, b)  // temp
SWAP(c, d)  // temp - COLLISION!
```

**Solution**: Aria's preprocessor automatically renames variables with unique IDs:

```aria
SWAP(a, b)  // Generates: temp_h0
SWAP(c, d)  // Generates: temp_h1
```

This is based on the **gensym pattern** from Lisp/Scheme using an atomic counter for thread-safe ID generation.

### 3. Constants (`%define`/`%undef`)

Define compile-time constants:

```aria
%define VERSION 1
%define MAX_SIZE 1024
%define PI 3.14159

%undef VERSION  // Remove definition
```

### 4. Conditional Compilation

Control what code gets compiled:

```aria
%ifdef DEBUG
func log(string:msg) {
    print("DEBUG: " + msg)
}
%endif

%ifndef PRODUCTION
%define ENABLE_ASSERTS 1
%endif

%if VERSION > 1
// Version 2+ only code
%elif VERSION == 1
// Version 1 specific code
%else
// Legacy code
%endif
```

### 5. Repeat Blocks (`%rep`/`%endrep`)

Generate repetitive code:

```aria
%rep 10
func handler_%i() {
    // Handler code
}
%endrep
```

### 6. Context Stack (`%push`/`%pop`/`%repl`)

Manage local label scopes:

```aria
%push loop_context
    // %$label is local to this context
    %$start:
    %$end:
%pop
```

### 7. Include System (`%include`)

Include other source files:

```aria
%include "stdlib.aria"
%include <system/io.aria>
```

Search paths:
- Relative includes (`"..."`) search from current file directory first
- System includes (`<...>`) search from standard library paths

### 8. Expression Evaluation (`%assign`)

Evaluate compile-time expressions:

```aria
%assign BUFFER_SIZE 1024 * 4
%assign MAX_THREADS 8
%assign TOTAL MAX_THREADS * 2
```

## Implementation Details

### Hygiene Algorithm

1. **Unique ID Generation**: Thread-safe atomic counter (`std::atomic<uint64_t>`)
2. **Type:Name Detection**: Regex pattern matching for variable declarations
3. **Reserved Keywords**: Protected keywords are never renamed
4. **Parameter Preservation**: Macro parameters (`%1`, `%2`) are left unchanged
5. **Hygienic Suffix**: Variables renamed to `varname_hNNN` where NNN is unique ID

### Processing Pipeline

```
Source Code
    ↓
Phase 0: Preprocessing
    ├── Macro expansion
    ├── Constant substitution
    ├── Conditional evaluation
    ├── Include resolution
    └── Hygienic renaming
    ↓
Preprocessed Source
    ↓
Phase 1: Lexical Analysis
    ↓
...
```

### Debug Flag

Use `-E` to see preprocessed output:

```bash
ariac program.aria -E
```

This outputs the preprocessed source after all macro expansion and hygiene transformations.

## Examples

### Example 1: Generic Function Generation

```aria
%macro GEN_MIN 1
func min_%1(%1:a, %1:b) -> %1 {
    if a < b {
        result a
    } else {
        result b
    }
}
%endmacro

GEN_MIN(int32)
GEN_MIN(flt64)
```

### Example 2: Assertion Macros

```aria
%ifdef DEBUG
%macro ASSERT 1
if !(%1) {
    print("Assertion failed: %1")
    fail
}
%endmacro
%else
%macro ASSERT 1
// No-op in release builds
%endmacro
%endif
```

### Example 3: Metaprogramming

```aria
%assign NUM_HANDLERS 8

%rep NUM_HANDLERS
func register_handler_%i() {
    // Registration code
}
%endrep

func init_system() {
%rep NUM_HANDLERS
    register_handler_%i()
%endrep
}
```

## Research References

- **gem3_06.txt**: Gemini's comprehensive analysis of macro hygiene bugs
- **Lisp/Scheme gensym**: Original inspiration for hygienic renaming
- **NASM preprocessor**: Syntax and directive design reference

## Technical Specifications

- **File**: `src/frontend/preprocessor/preprocessor.cpp` (1,400 lines)
- **Header**: `include/frontend/preprocessor/preprocessor.h`
- **Max Recursion Depth**: 1000 levels (prevents infinite expansion)
- **Hygiene Counter**: 64-bit atomic (supports 18 quintillion unique IDs)
- **Thread Safety**: All preprocessing is thread-safe via atomic operations

## Version History

- **v0.0.17 (Dec 14, 2024)**: Initial implementation archived during refactor
- **v0.1.0 (Current)**: Re-implemented with modular architecture + hygiene system added

## Status

✅ **FULLY IMPLEMENTED** - Non-negotiable feature for v0.1.0 release
