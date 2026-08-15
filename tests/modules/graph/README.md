# `tests/modules/` — fixtures, not tests

These `.npk` files are **inputs to a test**, never run on their own. The harness
does not compile them: `tests/frontend/module_graph.npk` loads them through the
real module loader and asserts what came out.

A module system cannot be tested from a single file, and until cycle 0.3 every
suite here was single-file. This is the first fixture directory.

The graph is deliberately awkward:

```
    root.npk  ──▶ alpha.npk ──▶ beta.npk
        │             ▲            │
        │             └────────────┘      a genuine CYCLE (D-086: legal)
        └───────▶ nested/mod.npk          reached by `mod:nested;`
```

`alpha` and `beta` import each other. That is legal, it is the case D-086 was
settled for, and it loads because collection is per-module and needs nothing from
anywhere else.
