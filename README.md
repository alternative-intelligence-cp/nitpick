# Nitpick

A safety-critical systems language and its self-hosted compiler.

> **Looking for the C/C++ prototype that used to live at this address?** It is
> archived, fully browsable, as
> [`nitpick-prototype`](https://github.com/alternative-intelligence-cp/nitpick-prototype),
> with its documentation at
> [`nitpick-prototype-docs`](https://github.com/alternative-intelligence-cp/nitpick-prototype-docs).
> This repository is its successor — a fresh line that starts at version `0.0`,
> written in Nitpick and compiled by itself. A prototype clone that pulls from
> here will refuse to merge unrelated histories, which is the intended outcome.

**Status: self-hosting declared — cycle 1.4 of the plan (Phase C) is complete;
verification (cycle 1.5) is next.** The specification set
is complete and every design decision is recorded with its reasoning. The
compiler is Nitpick source, built by a committed snapshot of its own emission
(`bootstrap/seed/`) and rebuilding itself byte-identically on every full test
run; the runtime floor is hand-written LLVM IR and nothing else is linked. The
build and test driver `npkg` is Nitpick too. What comes next is verification
(cycle 1.5) — the reason the language exists in this form.

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
| `bootstrap/` | the committed bootstrap snapshot (`seed/`), the retired generator, and the Python harness that runs until `npkg` parity is proven |
| `tests/` | every suite `nitpick.toml` declares — conformance, the rejection suites by stage, the real-backend programs, acceptance |
| `meta/specs/` | the language specification |
| `meta/roadmap/` | the plan, in numbered cycles |
| `npkg/` | the build and test driver, in Nitpick |
| `runtime/` | `npkrt.ll`, the runtime floor — hand-written LLVM IR |

## Specification

`meta/specs/` is the authority on language semantics.
[`DECISIONS.md`](meta/specs/DECISIONS.md) records every settled design decision
with its reasoning — start there when something looks unusual, because it is
recorded why.

## Licence

Apache 2.0. See [`LICENSE`](LICENSE).
