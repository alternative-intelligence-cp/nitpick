# Nitpick

A safety-critical systems language and its self-hosted compiler.

**Status: pre-implementation.** The specification set is complete and the build-out
has just begun. There is no working compiler in this repository yet.

## What it is

Nitpick is built for software where a failure is a physical event rather than a
debugging inconvenience. Three things matter, in this order: **safety,
correctness, performance.**

What that buys, concretely:

- **Every function returns `Result<T>`** except `main` and `failsafe`. There are
  no exceptions and no unhandled errors — a caller never has to ask whether *this
  particular* function can fail.
- **A trap is a controlled shutdown.** Anything uncaught routes through a
  mandatory `failsafe` handler, so a program with actuators live stops in a state
  someone chose.
- **No external dependencies.** No C, no C++, no third-party runtime. Everything
  in the trusted computing base is verifiable, and nothing crosses an FFI barrier
  where a fault could not be intercepted.
- **A large amount of compile-time checking, and the necessary amount at
  runtime.** That is the product, not the tax.

The language is deliberately demanding. It is not optimised for familiarity or
for brevity.

## Layout

See [`meta/LAYOUT.md`](meta/LAYOUT.md) for the tree and the reasoning behind it.

| Path | Contents |
|---|---|
| `src/` | the compiler — Nitpick source only |
| `bootstrap/` | the throwaway bootstrap seed and its generator |
| `tests/` | conformance, rejection, and per-cycle suites |
| `meta/specs/` | the language specification |
| `meta/roadmap/` | the plan, in numbered cycles |

## Specification

`meta/specs/` is the authority on language semantics.
[`DECISIONS.md`](meta/specs/DECISIONS.md) records every settled design decision
with its reasoning — start there when something looks unusual, because it is
recorded why.

## Licence

Apache 2.0. See [`LICENSE`](LICENSE).
