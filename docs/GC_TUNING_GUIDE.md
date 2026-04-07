# Garbage Collection Tuning Guide

> v0.16.7 — Aria Compiler Documentation

## Overview

Aria uses a **hybrid generational garbage collector** with two regions:

- **Nursery (Young Generation)**: A copying (Cheney-style) semi-space for short-lived allocations.
- **Old Generation**: A mark-sweep region for long-lived objects promoted from the nursery.

## GC Modes

### Stop-the-World (STW)
All mutator threads are paused during collection. Default for minor (nursery) collections.

### Concurrent
Major collections use **Snapshot-At-The-Beginning (SATB)** marking with incremental sweeping. Mutator threads continue running during the mark phase.

## Safepoints

Safepoints are injected automatically at:
- Loop back-edges (every iteration boundary)
- Function call sites

At safepoints, the runtime checks if a GC is pending and yields if needed.

## Shadow Stack Root Scanning

The GC uses a **shadow stack** to track root references. Every function that allocates GC-managed objects pushes roots onto the shadow stack and pops them on exit.

## Coroutine Integration (v0.8.4)

Suspended coroutine frames contain GC roots that must be scanned during collection. The `GCCoroAllocator::scan_frames()` method iterates all suspended frames and marks their roots during both minor and major GC cycles.

## JIT Root Tracking (v0.8.4)

JIT-compiled code can register GC roots via:
- `aria_gc_register_jit_root(void** root_addr)` — Register a root location
- `aria_gc_unregister_jit_root(void** root_addr)` — Unregister when no longer needed

JIT roots are scanned during all GC phases — minor evacuation, STW major marking, and concurrent major marking.

## Object Pinning (Wild Interop)

Objects passed to `wild` (unsafe FFI) blocks are **pinned** in place so external C code retains valid pointers. Pinned objects are:
- Skipped during nursery evacuation (not moved)
- Scanned in-place during old-gen marking

## Write Barriers

A **card table** tracks cross-generational references. When an old-gen object is updated to point to a nursery object, the corresponding card is dirtied. During minor GC, dirty cards are scanned for additional roots.

## Tuning Parameters

| Parameter | Default | Description |
|-----------|---------|-------------|
| Nursery size | 4 MB | Semi-space size for young generation |
| Promotion threshold | 2 | Survive N minor GCs before promotion |
| Major GC trigger | 75% | Old-gen occupancy threshold |
| Concurrent mode | Auto | STW for small heaps, concurrent for large |
