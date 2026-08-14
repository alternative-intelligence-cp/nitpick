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
are the two halves of the module test and belong together.
