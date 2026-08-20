# Cycle 0.10 — The memory allocator

**Phase B, new cycle.** (On the numbering: see "The cycle-numbering convention,
relaxed at 0.10" in `../ROADMAP.md` — the map's ordering is authoritative over
the folder name's lexical sort.)

The only allocator that exists at 0.8-close is the throwaway bump allocator in the
runtime floor, whose `dalloc` is a no-op. That is correct for a compiler that runs
once and exits (D-015), but it means **the real memory allocator — the load-bearing
runtime TCB component — is entirely unbuilt**, and the roadmap scheduled it nowhere.
This cycle builds it, and in doing so makes real three guarantees that are currently
theoretical.

## Why this cycle exists, and why here

The dependency chain the audit drew (total_audit §B-1):

```
1.1 async ──needs──▶ per-thread executor frame allocator (D-034)
                              │
arenas load-bearing (D-003) ──┤ arena<T> / Handle<T>
                              ▼
                    a real heap with real free()
                              │
                              ▼
                    the exit-time <wild-live> leak registry
                    (the K-semantics guarantee, currently vacuous)
```

Every arrow is a hard prerequisite. **1.1 cannot build its executor without this
cycle** (D-034: "each thread's executor owns an arena from which it allocates task
frames"). And the exit-time leak guarantee — the thing that lets arenas replace a
collector without losing leak detection (D-003, CONTROL_REFERENCE §4.6) — is
**presently vacuous**: with `dalloc` a no-op and no live-set tracked, nothing is in
`<wild-live>` and nothing is checked. This cycle is where that guarantee stops being
a promise about code that does not exist.

It comes **after 0.9** because arenas and `Handle<T>` need the general type lowering
0.9 provides, and **before 1.0/1.1** because 1.1 depends on it and 1.0 does not (so
0.10 could in principle swap with 1.0, but not with 1.1).

## Decisions in

Three spec contradictions the audit found (total_audit C-1…C-3 of the allocator
dive) are corrected as this cycle's code lands — they are cheap and they currently
mislead:

- **The "8-byte CRC32 header" claim** (MEMORY_REFERENCE §111, BUILTIN_REFERENCE §28)
  is wrong three ways (CRC32 is 4 bytes; no allocator uses a CRC; double-free is
  already a compile-time error via D-119). Replace with the real header shape
  (§0.10.0) as it is built.
- **The stale `free`/`realloc` "legacy aliases" line** (MEMORY_REFERENCE §3) — D-119
  already struck it in BUILTIN_REFERENCE; propagate.
- **The OOM→failsafe contract** must state the allocator's own allocation-free
  obligation (§0.10.1).

## The subcycles

| # | Topic |
|---|---|
| 0.10.0 | **The heap** — slab + large-block allocator, real `dalloc`, the header canary |
| 0.10.1 | **The `<wild-live>` registry** and the exit-time leak check; OOM→`failsafe` |
| 0.10.2 | **`arena<T>` + `Handle<T>`** — single-threaded, generation-counted |
| 0.10.3 | **The executor frame allocator** — distinct from `arena<T>`; 1.1's dependency |
| 0.10.4 | **`shared_arena<T>`** — chunked, non-moving, atomic-bump |
| 0.10.5 | **`wildx` W^X state machine** — the JIT lifecycle as verifiable obligations |

## Watch for

- **This is TCB code, all of it.** Every routine gets the D-015 hand-written-IR (or
  audited-Nitpick) discipline, the three-way signature diff where it crosses the
  runtime boundary, and the D-011 undefined-symbol scan. An allocator is exactly the
  subtle code the zero-dependency rule exists to keep verifiable.
- **The seed still builds `src/` (C-13).** As in 0.9, new allocator features are
  exercised by test programs, not adopted in the compiler's own source until the
  builder switches at 1.2 — the bump floor keeps building the compiler through this
  cycle.
- **`arena<T>` is not the frame allocator.** The concurrency audit's sharpest catch:
  the surface `arena<T>` is a fixed-slot, `Handle`-returning allocator that *cannot*
  size variable coroutine frames. 0.10.3 builds the distinct frame allocator D-034
  actually needs. Do not try to make one type serve both — that conflation is a
  planned-for mistake this cycle exists partly to avoid.
