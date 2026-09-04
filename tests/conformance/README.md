# Conformance suite

Every construct **inside** subset 1 (`meta/SUBSET_1.md`), exercised.

Two audiences, and both matter:

- **The seed must compile all of it.** This is the seed's acceptance test — the
  executable form of `SUBSET_1.md`. If the seed cannot compile a file here, either
  the seed is incomplete or the subset was drawn wrong.
- **The real compiler must accept all of it too**, forever. Subset 1 is a subset
  of Nitpick, not a dialect of it, so nothing here should ever stop compiling as
  the rungs are climbed.

That second property is what makes this suite worth keeping after the seed is
gone: it is a regression suite against the language accidentally narrowing.

Files are numbered by construct group rather than by dependency — `09` and `10`
are the two halves of the module test and belong together. The `c` in front
of every number is D-248's (1.5.1b step 1): a file's first declaration is
`mod:<basename>;`, a module name is an identifier, and an identifier cannot
begin with a digit — so `00_minimal.npk` became `c00_minimal.npk` and keeps
its place in the reading order.
