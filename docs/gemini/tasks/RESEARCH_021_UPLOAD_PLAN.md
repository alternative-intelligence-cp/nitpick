# Research 021 - GC System: File Upload Priority List

**Task**: Garbage Collection System Research  
**Max Additional Files**: 7 (after required context files)  
**Strategy**: Prioritize headers and core implementation files

## Required Context Files (Already Specified)
1. ✅ `docs/info/aria_specs.txt` (847 lines)
2. ✅ `docs/gemini/responses/research_001_borrow_checker.txt` (406 lines)
3. ✅ `docs/gemini/responses/research_014_composite_types_part1.txt` (261 lines)

**Subtotal**: 3 files, 1,514 lines

## Priority Runtime Files (7 slots available)

### Tier 1: Core GC Headers (CRITICAL - Upload First)
**Priority**: These define the fundamental GC architecture

1. **`src/runtime/gc/header.h`** (36 lines)
   - ObjHeader structure with bitfields
   - mark_bit, pinned_bit, forwarded_bit, is_nursery
   - TypeID enum for RTTI
   - Nursery and Fragment structures
   - **Why**: Defines memory layout for ALL GC objects

2. **`src/runtime/gc/shadow_stack.h`** (64 lines)
   - Shadow stack API for GC root tracking
   - Frame management (push/pop)
   - Root registration (add/remove)
   - **Why**: Critical for precise GC, answers "how are roots tracked?"

3. **`src/runtime/gc/gc_impl.h`** (34 lines)
   - Public GC interface
   - aria_gc_alloc, aria_gc_collect_minor/major
   - **Why**: Entry points for GC operations

**Tier 1 Subtotal**: 3 files, 134 lines

### Tier 2: Core GC Implementation (HIGH Priority)
**Priority**: Implementation details of GC algorithm

4. **`src/runtime/gc/gc_impl.cpp`** (222 lines)
   - Main GC algorithm implementation
   - Mark-sweep-compact logic
   - Nursery collection
   - **Why**: Answers algorithm questions (generational structure, collection triggers)

5. **`src/runtime/gc/shadow_stack.cpp`** (154 lines)
   - Shadow stack implementation
   - Frame tracking with std::vector
   - Root management
   - **Why**: Shows actual overhead and LLVM integration strategy

6. **`src/runtime/gc/nursery.cpp`** (115 lines)
   - Nursery allocation and management
   - Bump pointer allocation
   - Fragment management
   - **Why**: Generational GC details, promotion policy

**Tier 2 Subtotal**: 3 files, 491 lines

### Tier 3: Memory Safety Integration (MEDIUM Priority)
**Priority**: Shows pinning and wild memory interaction

7. **`src/runtime/memory/allocator.h`** (30 lines)
   - Basic allocation interface
   - aria_alloc/aria_free for wild memory
   - **Why**: Shows GC vs wild memory boundary

**Tier 3 Subtotal**: 1 file, 30 lines

## Final Upload List (10 files total)

### Required (3 files):
1. docs/info/aria_specs.txt
2. docs/gemini/responses/research_001_borrow_checker.txt
3. docs/gemini/responses/research_014_composite_types_part1.txt

### Priority Runtime Files (7 files):
4. src/runtime/gc/header.h ⭐ CRITICAL
5. src/runtime/gc/shadow_stack.h ⭐ CRITICAL
6. src/runtime/gc/gc_impl.h ⭐ CRITICAL
7. src/runtime/gc/gc_impl.cpp
8. src/runtime/gc/shadow_stack.cpp
9. src/runtime/gc/nursery.cpp
10. src/runtime/memory/allocator.h

**Total**: 10 files, ~2,169 lines

## Files NOT Uploaded (But Mentioned in Prompt)

### Why These Are Lower Priority:
- **fat_pointer.c/h** (427 lines): About wild pointer safety, not core GC
- **wildx_allocator.c/h** (240 lines): WildX memory regions, separate from GC
- **wildx_guard.c/h** (260 lines): RAII guards for WildX, not GC-specific

### Provide Summary In Prompt:
"Additional runtime files exist but aren't uploaded due to space constraints:
- fat_pointer.c/h (308+119 lines): Fat pointer bounds checking for wild memory
- wildx_allocator.c/h (189+51 lines): WildX allocation with guard pages
- wildx_guard.c/h (104+156 lines): RAII scope guards for WildX regions

These relate to wild memory safety, not GC implementation."

## Upload Order (Optimal)

1. aria_specs.txt (foundation)
2. research_001_borrow_checker.txt (dependency)
3. research_014_composite_types_part1.txt (ObjHeader reference)
4. **gc/header.h** ← Start here for runtime files
5. **gc/shadow_stack.h**
6. **gc/gc_impl.h**
7. gc/gc_impl.cpp
8. gc/shadow_stack.cpp
9. gc/nursery.cpp
10. memory/allocator.h

## Key Information for Prompt

### Include This Summary:
"The runtime/ directory contains 1,614 lines across 14 files. I'm uploading the 7 most critical GC implementation files (655 lines total):

**Core GC (134 lines)**:
- header.h: ObjHeader with mark/pinned/forwarded bits, TypeID, Nursery struct
- shadow_stack.h: Root tracking API (push/pop frames, add/remove roots)
- gc_impl.h: Public interface (aria_gc_alloc, collect_minor/major)

**Implementation (491 lines)**:
- gc_impl.cpp: Mark-sweep-compact algorithm, nursery collection
- shadow_stack.cpp: Frame/root management with std::vector
- nursery.cpp: Bump pointer allocation, fragment management

**Boundary (30 lines)**:
- allocator.h: Wild memory allocation (aria_alloc/free)

Not uploaded due to space: fat_pointer.c/h (wild pointer bounds checking), wildx_allocator/guard (WildX regions with guards)."

## Task JSON Update

Update the context_files array to be specific:

```json
"context_files": [
    "docs/info/aria_specs.txt",
    "docs/gemini/responses/research_001_borrow_checker.txt",
    "docs/gemini/responses/research_014_composite_types_part1.txt",
    "src/runtime/gc/header.h",
    "src/runtime/gc/shadow_stack.h",
    "src/runtime/gc/gc_impl.h",
    "src/runtime/gc/gc_impl.cpp",
    "src/runtime/gc/shadow_stack.cpp",
    "src/runtime/gc/nursery.cpp",
    "src/runtime/memory/allocator.h"
]
```

## Questions These Files Answer

### GC Algorithm:
- ✅ Generational structure: 2 generations (nursery + old), see header.h is_nursery bit
- ✅ Collection algorithm: Mark-sweep-compact with copying for nursery
- ✅ Triggers: Implemented in gc_impl.cpp (bump pointer overflow, threshold)
- ✅ Concurrent: Not yet (no write barriers visible)

### Pinning:
- ✅ API: # operator sets pinned_bit in ObjHeader (header.h)
- ✅ Safety: Pinned objects skip moving phase (gc_impl.cpp checks bit)
- ✅ Performance: Pinned objects force nursery retention or old-gen promotion
- ⚠️ RAII: Not visible in uploaded files (may need to infer/propose)

### Shadow Stack:
- ✅ Implementation: Separate stack with std::vector (shadow_stack.cpp)
- ✅ Registration: Manual push_frame/add_root in function prologue
- ✅ Overhead: One vector push per root, frame overhead
- ✅ LLVM integration: Direct function calls in codegen
- ⚠️ Conservative scanning: Not implemented (precise only)

### Integration:
- ✅ Borrow checker: Pinning prevents moves during borrows
- ✅ obj/dyn: Use ObjHeader, tracked by shadow stack
- ✅ Wild boundary: allocator.h shows separate aria_alloc
- ⚠️ Debugging tools: Not visible in uploaded files
- ⚠️ LOH: Not visible in uploaded files

## Summary

This file selection maximizes research coverage with the 7-file limit. The headers (134 lines) provide complete architectural understanding, while the implementation files (491 lines) show actual algorithms. The 30-line allocator.h shows the GC/wild boundary.

Total coverage: **~655 runtime lines** answering ~70% of task questions directly, with remaining 30% inferable or requiring new design.
