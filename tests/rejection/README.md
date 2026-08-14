# Rejection suite

Programs that are **valid Nitpick** but use constructs outside subset 1.

Every one of them must produce **`NITPICK-RUNG-001`** — *construct not supported
at this backend rung* — and **must not produce a parse error.**

That distinction is the entire point of this directory:

> **The parser never restricts. The backend does.** (D-085)

The frontend accepts the whole grammar from day one. A construct the current rung
cannot lower is rejected during **lowering**, with a diagnostic that names the
rung — never by the parser refusing to read it.

This is the standing guard against the failure that ended `nitpick-bootstrap`:
a partial grammar, re-widened stage by stage, and a parser rewritten each time.
If any test here starts failing with a *syntax* error, the grammar has been made
partial and that is a defect regardless of how reasonable it looked.

These tests are written **before there is a parser to break**, so the requirement
is fixed rather than discovered.

As each rung lands, the construct it enables moves from here to
`tests/conformance/` — this directory shrinking is how subset 1 disappearing is
measured.
