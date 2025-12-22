# Allocator Research Findings from Educational Repository

**Date**: December 21, 2025  
**Purpose**: Survey of existing allocator implementations and research  
**Context**: Preparing for Aria Phase 4.2 Extended Memory Features

---

## Executive Summary

The educational repository contains **extensive allocator research** spanning 3 complete implementations with detailed documentation. While none implement pure arena/bump allocators, they provide valuable insights into:

1. **Size-class segregation** (15 classes in SegFit)
2. **Performance optimization techniques** (1.6-45ns allocation in Stabilized)
3. **Memory reclamation strategies** (doubly-linked free lists in SegFit)
4. **Large block handling** (mremap optimization in SegFit)
5. **Production-ready architecture** (ApexAlloc educational roadmap)

**Key Finding**: We have deep expertise in segregated-fit and bump allocators, but **no dedicated arena allocator research**. Stage 1.2 arena research is still valuable and not duplicative.

---

## Implemented Allocators Overview

### 1. Stabilized Allocator (`educational/allocators/stabilized/`)

**Type**: Pure Bump Allocator  
**Status**: Phase 2 Complete  
**Performance**: 1.6-45ns allocation (10x faster than malloc)

#### Architecture
```
Pool Memory Layout:
┌─────────────────────────────────────┐
│ [Used Memory]     [Free Space]      │
│     ↑                               │
│   current                           │
└─────────────────────────────────────┘
     bump pointer moves →
```

#### Key Characteristics
- **Allocation**: O(1) pointer bump
- **Deallocation**: ❌ None (no individual free)
- **Reclamation**: Pool reset only (all-or-nothing)
- **Use Case**: Batch processing, educational baseline

#### Relevance to Arena Design
- ✅ **Directly applicable**: This IS a bump allocator (arena without chaining)
- ✅ **Performance baseline**: 1.6-45ns allocation target
- ✅ **Simple architecture**: Good starting point for arena implementation
- ❌ **Missing features**: No block chaining, no reset with memory reuse

**Code Inspection Recommendation**: Read `sm_consciousness_stabilized.c` for bump allocation implementation.

---

### 2. SegFit Advanced Allocator (`educational/allocators/segfit_advanced/`)

**Type**: Segregated Fit with Free Lists  
**Status**: Phase 3-4 Complete  
**Performance**: 56ns allocation, 12ns same-class reallocation

#### Architecture
```
15 Size Classes (O(1) mapping):
┌──────────┬──────────┬──────────┬────────┬─────────┐
│ 8B-32B   │ 48B-128B │ 192B-512B│ 768B+  │ Large   │
│ Class 0-3│ Class 4-7│ Class 8-11│Class12-│ >65KB   │
│ ┌──┬──┐  │ ┌──┬──┐  │ ┌──┬──┐  │ ┌────┐ │ ┌─────┐ │
│ │  │  │  │ │  │  │  │ │  │  │  │ │    │ │ │mremap│ │
│ └──┴──┘  │ └──┴──┘  │ └──┴──┘  │ └────┘ │ └─────┘ │
└──────────┴──────────┴──────────┴────────┴─────────┘

Each Size Class: Doubly-linked free list
┌─────────────────────────────────────────────────┐
│ Free List Head → Block ↔ Block ↔ Block → NULL  │
│                    ↓        ↓        ↓         │
│                  Data     Data     Data        │
└─────────────────────────────────────────────────┘
```

#### Key Characteristics
- **Allocation**: O(1) from size-class free list
- **Deallocation**: O(1) return to free list
- **Reclamation**: Full support via doubly-linked lists
- **Large Blocks**: mremap optimization for >4KB reallocations

#### Size Class Design
```c
#define SEGFIT_SIZE_CLASSES 15
Classes: 8, 16, 24, 32, 48, 64, 96, 128, 192, 256, 384, 512, 768, 1024, 65536 bytes
Large Block Threshold: >65KB
```

#### mremap Optimization
```c
#define SEGFIT_MREMAP_THRESHOLD 4096  // PAGE_SIZE
#define SEGFIT_ENABLE_MREMAP 1

// Revolutionary feature: O(1) reallocation for large blocks
// Uses Linux mremap() syscall to avoid copying data
// Result: 12ns reallocation vs 100-2000ns for malloc
```

#### Free Block Structure
```c
typedef struct segfit_free_block {
    struct segfit_free_block *next;   // Doubly-linked list
    struct segfit_free_block *prev;
    size_t size;                      // For verification
    uint32_t size_class;              // Fast deallocation mapping
    uint32_t magic;                   // Corruption detection
} segfit_free_block_t;
```

#### Relevance to Arena Design
- ❌ **Not arena**: This is segregated-fit, not bump/arena
- ✅ **Block chaining insight**: Shows how to manage multiple memory blocks
- ✅ **Size class math**: Power-of-2 size class mapping algorithms
- ✅ **Free list management**: Doubly-linked list patterns
- ✅ **Large block handling**: Dedicated >64KB block tracking
- ✅ **Performance target**: 56ns allocation is competitive baseline

**Code Inspection Recommendation**: Read `segfit_consciousness_allocator.h` (lines 1-273) for:
- Size class architecture
- Free list management
- Large block tracking structures
- Performance statistics

---

### 3. ApexAlloc Educational Allocator (`educational/allocators/apexalloc/`)

**Type**: Multi-level Educational Framework (mimalloc/snmalloc/jemalloc inspired)  
**Status**: In Progress (Tutorial Phase)  
**Goal**: Comprehensive learning resource for modern allocator design

#### Architecture (Planned)
```
Level 1: Superblock (4MB Segment)
  ├─ NUMA-aware allocation
  ├─ Transparent Huge Pages (THP)
  └─ Aligned to 2MB boundaries

Level 2: Block (64KB Page)
  ├─ Thread-local ownership
  ├─ Segregated by size class
  └─ Local + remote free lists

Level 3: Chunk (User Object)
  ├─ Cryptographic checksums
  └─ Safe-linking pointer obfuscation
```

#### Key Features (Planned)
- **Performance**: Lock-free fast path, message-passing remote free
- **Security**: CRC32 checksums, safe-linking, double-free detection
- **Observability**: Introspection API, heap visualization
- **Educational**: 10-phase tutorial with exercises

#### Implementation Phases
| Phase | Topic | Status |
|-------|-------|--------|
| I | Build System | ⏸️ Not started |
| II | Memory Hierarchy | ⏸️ Not started |
| III | Global State | ⏸️ Not started |
| IV | Superblock Management | ⏸️ Not started |
| V | Fast Path (Thread-Local) | ⏸️ Not started |
| VI | Concurrency (Remote Free) | ⏸️ Not started |
| VII | Security Hardening | ⏸️ Not started |
| VIII | Memory Reclamation | ⏸️ Not started |
| IX | Introspection | ⏸️ Not started |
| X | Testing | ⏸️ Not started |

#### Relevance to Arena Design
- ❌ **Not arena**: This is a full-featured general-purpose allocator
- ❌ **Too complex**: 10 phases, thread-safety, security features beyond scope
- ✅ **Thread-local insight**: Shows how to avoid locks with TLS arenas
- ✅ **Superblock concept**: Large OS-allocated regions (similar to arena blocks)
- ✅ **Educational structure**: Good example of phased implementation

**Code Inspection Recommendation**: Read `README.md` for architecture overview, but implementation not yet complete.

---

### 4. Randy's Chunked Allocator (`educational/allocator_inc/allocator_randy_chunked.h`)

**Type**: Size-class chunked allocator (header only)  
**Status**: Interface defined, implementation unknown

#### Size Classes
```c
#define SM_CHUNK_TINY_SIZE    64    // Tiny allocations
#define SM_CHUNK_SMALL_SIZE   256   // Small allocations  
#define SM_CHUNK_MEDIUM_SIZE  1024  // Medium allocations
#define SM_CHUNK_LARGE_SIZE   4096  // Large allocations
```

#### Key Features
```c
#define RANDY_CHUNKING_ENABLED 1
#define RANDY_CHUNK_STATS_ENABLED 1
#define RANDY_DYNAMIC_RESIZE_ENABLED 1  // Auto-create new sizes
```

#### Relevance to Arena Design
- ✅ **Size classes**: Similar concept to arena block sizes
- ❌ **No implementation**: Interface only, can't extract insights
- ❌ **Different pattern**: Chunking ≠ arena allocation

---

## Comparative Analysis

### Allocator Types Present

| Type | Example | Deallocation | Use Case |
|------|---------|--------------|----------|
| **Bump** | Stabilized | ❌ None | Batch processing |
| **Segregated Fit** | SegFit | ✅ Individual | General purpose |
| **Multi-level** | ApexAlloc | ✅ Full | Enterprise (planned) |
| **Arena** | ❌ None | Bulk only | ❌ **MISSING** |

### Performance Comparison

| Allocator | Allocation | Deallocation | Memory Reclaim |
|-----------|------------|--------------|----------------|
| Stabilized | 1.6-45ns ✅ | ❌ N/A | ❌ Pool reset only |
| SegFit | 56ns ✅ | 245ns | ✅ Free lists |
| ApexAlloc | ⏸️ Planned | ⏸️ Planned | ⏸️ Planned |
| **Arena (target)** | **~2ns** | **❌ N/A** | **✅ Bulk destroy** |

---

## Relevant Findings for Arena Design

### 1. Bump Allocation Pattern (from Stabilized)

**File**: `educational/allocators/stabilized/sm_consciousness_stabilized.c`

**Core pattern** (likely similar to this):
```c
void* allocate(size_t size) {
    // Align size
    size_t aligned_size = ALIGN(size, alignment);
    
    // Check if fits in current pool
    if (pool->current + aligned_size > pool->end) {
        // Out of memory or allocate new pool
        return NULL;
    }
    
    // Bump pointer
    void* ptr = pool->current;
    pool->current += aligned_size;
    
    return ptr;
}
```

**Lesson**: Arena allocation is just bump + block chaining when pool full.

---

### 2. Block Chaining Strategy (from SegFit)

**File**: `educational/allocators/segfit_advanced/segfit_consciousness_allocator.h`

**Size class architecture**:
```c
typedef struct {
    size_t block_size;                    // Size of blocks in this class
    size_t blocks_per_chunk;              // Blocks per 64KB chunk
    segfit_free_block_t *free_list_head;  // Doubly-linked free list
    uint32_t free_blocks;                 // Stats
    uint32_t total_blocks;
} segfit_size_class_t;
```

**Lesson**: Track multiple memory chunks per size class, use linked lists for chaining.

---

### 3. Large Block Handling (from SegFit)

**Large block tracking**:
```c
typedef struct {
    void *ptr;                            // Block pointer
    size_t size;                          // Block size
    uint64_t allocation_time_ns;          // Timestamp
    uint32_t magic;                       // Validation
} segfit_large_block_t;

#define SEGFIT_LARGE_THRESHOLD 65536      // >64KB
#define SEGFIT_LARGE_BLOCK_LIMIT 1000     // Max tracked
```

**Lesson**: Track large blocks separately, use array for simple management.

---

### 4. mremap Optimization (from SegFit)

**Zero-copy reallocation**:
```c
#define SEGFIT_MREMAP_THRESHOLD 4096  // PAGE_SIZE
#define SEGFIT_ENABLE_MREMAP 1

// For blocks >4KB, use mremap() instead of malloc + memcpy
// Achieves O(1) reallocation without copying data
// Performance: 12ns vs 100-2000ns
```

**Lesson**: For large arena blocks, mremap can enable O(1) growth.

---

### 5. Alignment Handling (from SegFit)

**Cache line optimization**:
```c
#define SEGFIT_CACHE_LINE_SIZE 64
#define SEGFIT_ALIGN_TO_CACHE_LINE(x) \
    (((x) + SEGFIT_CACHE_LINE_SIZE - 1) & ~(SEGFIT_CACHE_LINE_SIZE - 1))

// Fast alignment formula
static inline size_t align_forward(size_t offset, size_t alignment) {
    return (offset + alignment - 1) & ~(alignment - 1);
}
```

**Lesson**: Use bitwise AND for O(1) alignment (already in our arena design).

---

### 6. Statistics and Validation (from SegFit)

**Performance tracking**:
```c
typedef struct {
    uint64_t total_allocations;
    uint64_t total_bytes_allocated;
    uint64_t fragmentation_events;
    uint64_t cache_hits;
    uint64_t cache_misses;
    uint32_t allocator_magic;             // Corruption detection
    uint8_t corruption_detected;
} segfit_allocator_t;

#define SEGFIT_ALLOCATOR_MAGIC 0x53454746  // "SEGF"
```

**Lesson**: Magic numbers for validation, comprehensive stats for debugging.

---

## Gaps in Existing Research

### What We DON'T Have

1. ❌ **Arena allocator with block chaining**
   - Stabilized is pure bump (no chaining)
   - SegFit is segregated-fit (not arena)

2. ❌ **Arena reset() with memory reuse**
   - Stabilized pools are single-use
   - Need multi-block reset strategy

3. ❌ **Type-safe arena API design**
   - Existing allocators are C void* based
   - Aria needs `Arena.alloc<T>()` wrapper

4. ❌ **Arena alignment for mixed types**
   - SegFit has alignment, but for segregated-fit
   - Need alignment within continuous arena

5. ❌ **Arena overflow handling**
   - What happens when allocation > block_size?
   - Oversized block strategy not documented

### What We DO Have

1. ✅ **Bump allocation pattern** (Stabilized)
2. ✅ **Block chaining concept** (SegFit size classes)
3. ✅ **Large block tracking** (SegFit large blocks)
4. ✅ **Alignment algorithms** (SegFit cache line alignment)
5. ✅ **Performance baselines** (1.6-45ns bump, 56ns segregated)
6. ✅ **Statistics patterns** (SegFit comprehensive stats)
7. ✅ **Magic number validation** (SegFit corruption detection)

---

## Recommendations for Arena Implementation

### 1. Start with Stabilized's Bump Pattern

**Recommendation**: Read `sm_consciousness_stabilized.c` for basic bump allocation.

**Expected code pattern**:
```c
// Phase 1: Simple bump (no chaining)
void* bump_alloc(pool_t* pool, size_t size) {
    void* ptr = pool->current;
    pool->current += size;
    return ptr;
}
```

**Then extend** with block chaining from SegFit concepts.

---

### 2. Adopt SegFit's Block Chaining

**Recommendation**: Use SegFit's linked list pattern for arena blocks.

**Adapt for arena**:
```c
typedef struct arena_block {
    void* memory;                  // From SegFit large_block_t
    size_t size;
    struct arena_block* next;      // From SegFit free_block_t
} arena_block_t;

typedef struct {
    arena_block_t* first_block;
    arena_block_t* current_block;
    size_t current_offset;
    size_t block_size;             // Fixed size (from SegFit size_class_t)
} arena_t;
```

---

### 3. Use SegFit's Alignment Algorithm

**Recommendation**: Copy `align_forward()` directly.

```c
static inline size_t align_forward(size_t offset, size_t alignment) {
    return (offset + alignment - 1) & ~(alignment - 1);
}
```

**Already specified** in our `arena_allocator_design.md`.

---

### 4. Add SegFit's Statistics Pattern

**Recommendation**: Track similar stats for debugging.

```c
typedef struct {
    arena_block_t* blocks;
    size_t total_allocated;        // From SegFit
    size_t total_reserved;         // From SegFit
    uint32_t block_count;
    uint32_t allocator_magic;      // From SegFit
} aria_arena;

#define ARIA_ARENA_MAGIC 0x4152454E  // "AREN"
```

**Already specified** in our design document.

---

### 5. Consider mremap for Large Arenas

**Recommendation**: If arena block > 4KB, use mremap for growth.

**From SegFit**:
```c
#define ARENA_MREMAP_THRESHOLD 4096

void* arena_grow_block(arena_block_t* block, size_t new_size) {
    if (block->size >= ARENA_MREMAP_THRESHOLD) {
        // Try mremap first (O(1))
        void* new_mem = mremap(block->memory, block->size, new_size, MREMAP_MAYMOVE);
        if (new_mem != MAP_FAILED) {
            block->memory = new_mem;
            block->size = new_size;
            return new_mem;
        }
    }
    
    // Fallback: allocate new block
    return NULL;  // Signal to create new block
}
```

**Future enhancement** (Phase 4.3+).

---

## Code Files to Inspect

### High Priority (Directly Applicable)

1. **`educational/allocators/stabilized/sm_consciousness_stabilized.c`**
   - Pure bump allocation implementation
   - 1.6-45ns performance baseline
   - Simple pool management

2. **`educational/allocators/segfit_advanced/segfit_consciousness_allocator.h`** (lines 1-273)
   - Block chaining structures
   - Size class management
   - Large block tracking
   - Statistics patterns

3. **`educational/allocators/segfit_advanced/segfit_consciousness_allocator.c`**
   - Actual allocation algorithms
   - Free list management
   - mremap optimization code

### Medium Priority (Conceptual Insights)

4. **`educational/allocators/apexalloc/README.md`**
   - Multi-level architecture concepts
   - Thread-local patterns
   - Educational structure

5. **`educational/allocators/README.md`**
   - Performance comparison table
   - Use case analysis
   - Quick start patterns

### Low Priority (Reference Only)

6. **`educational/allocator_inc/allocator_randy_chunked.h`**
   - Size class definitions
   - Interface patterns (no implementation)

---

## Impact on Phase 4.2 Research

### Stage 1.2 (Arena Allocator Research) - STILL VALUABLE ✅

**Original plan**: Research Rust bumpalo, Zig ArenaAllocator, C++ patterns

**Impact of findings**:
- ✅ **Still needed**: We have bump allocator but NOT arena with chaining
- ✅ **Complementary**: Rust/Zig research adds multi-block patterns
- ✅ **Type safety**: Our research focuses on Aria integration, not present in C code
- ✅ **Defer integration**: Rust borrow checker lessons for Aria's defer

**Recommendation**: **PROCEED** with Stage 1.2 as planned. Our educational allocators provide implementation baseline, but Rust/Zig provide arena-specific patterns.

---

### Stage 1.3 (Custom Allocator Patterns) - POTENTIALLY REDUNDANT ⚠️

**Original plan**: Research pool allocator, slab allocator, allocator trait

**Impact of findings**:
- ⚠️ **Partial overlap**: SegFit is essentially a pool allocator (size classes)
- ⚠️ **Not slab**: SegFit is segregated-fit, not kernel-style slab
- ❌ **No trait system**: Our C implementations don't have allocator traits

**Recommendation**: **SHORTEN** Stage 1.3. Focus on:
- Pool allocator: Reference SegFit, don't re-research
- Slab allocator: Research kernel-style caching (different from SegFit)
- Allocator trait: Research Rust/Zig allocator interfaces (new)

**Estimated time reduction**: 45 min → 30 min

---

## Performance Targets Informed by Findings

### Allocation Speed

| Allocator Type | Existing Performance | Aria Target |
|----------------|----------------------|-------------|
| Bump (Stabilized) | 1.6-45ns | Arena: ~2ns |
| Segregated (SegFit) | 56ns | Custom: ~50ns |
| Wild (TBB) | ~100ns | Already implemented |

### Memory Overhead

| Allocator | Per-allocation Overhead |
|-----------|------------------------|
| malloc | ~24 bytes (header + footer) |
| SegFit | ~24 bytes (free_block_t) |
| Stabilized | 0 bytes (pure bump) |
| **Arena (target)** | **0-15 bytes (alignment only)** ✅ |

---

## Summary

### What We Learned

1. ✅ **Bump allocation pattern** - Stabilized provides 1.6-45ns baseline
2. ✅ **Block chaining structures** - SegFit shows how to manage multiple blocks
3. ✅ **Large block handling** - SegFit tracks >64KB separately
4. ✅ **Alignment algorithms** - SegFit provides O(1) bitwise alignment
5. ✅ **Statistics patterns** - SegFit comprehensive tracking for debugging
6. ✅ **Performance baselines** - Real numbers from production-ready code

### What We Still Need

1. ⏳ **Arena-specific block chaining** - Rust bumpalo, Zig ArenaAllocator patterns
2. ⏳ **Arena reset with reuse** - Not present in Stabilized
3. ⏳ **Type-safe API design** - Aria `Arena.alloc<T>()` wrapper patterns
4. ⏳ **Slab allocator research** - Kernel-style caching (different from SegFit)
5. ⏳ **Allocator trait design** - Rust/Zig allocator interfaces

### Recommendation

**PROCEED** with Phase 4.2 Stage 1.2 (Arena Allocator Research) as planned.

Our educational repository provides:
- ✅ Implementation baseline (bump allocation)
- ✅ Performance targets (1.6-45ns)
- ✅ Block management patterns (SegFit)
- ✅ Alignment algorithms (bitwise AND)

But we still need:
- ⏳ Arena-specific patterns (multi-block chaining)
- ⏳ Language-specific integration (Rust borrow checker, Zig interfaces)
- ⏳ Type-safe wrapper design (Aria generics)

**Time savings**: Reduce Stage 1.3 from 45 min to 30 min (leverage SegFit findings).

**Net impact**: ~15 minutes saved, research quality improved with concrete baselines.

---

**Next Action**: Continue with Stage 1.2 Arena Allocator Research when quota resets, armed with:
- Stabilized bump pattern as baseline
- SegFit block chaining as reference
- 1.6-45ns performance target
- Concrete C implementations to compare against Rust/Zig

