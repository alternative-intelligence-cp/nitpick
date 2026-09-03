# The switch — replacing the prototype

**Status: the REPOSITORY half executed on 2026-09-02, by the user, as a
tidy-up; the DOCUMENTATION half is still owed and still gated on the compiler
being finished.** The prototype and its docs are archived on GitHub as
`nitpick-prototype` and `nitpick-prototype-docs` (locally under
`REPOS/ARCHIVE/`, with every prototype-era library and application), this
repository is `alternative-intelligence-cp/nitpick` (renamed from
`nitpick-native`), and `nitpick-docs` exists again, EMPTY, for the official
documentation — HTML, man pages and Markdown built from `meta/specs/` once the
compiler is finished. The rest of this document stands as written: what
`meta/specs/` owes before it can fill `nitpick-docs`, the version scheme, the
tag ordering, and the redirect caveat (which has now happened, and whose
safeguard — the README pointer — is in place).

This document was written when the plan was "nothing happens until Phase C is
finished", because a repo half migrated to a version that is not finished is
worse than one that is cleanly old. The user judged the tidier layout worth
more than that caution for the repository half, and the archived repositories
remain fully browsable, so nothing is lost; the documentation half keeps the
original gate.

## What the switch is

One coordinated operation across two repositories and the website:

| Before | After | Status |
|---|---|---|
| `nitpick` — the C/C++ prototype compiler | `nitpick` — **this repository's contents** | **done 2026-09-02** (the remote renamed; the local folder is renamed at the 1.4 close) |
| `nitpick-docs` — the prototype's specification | `nitpick-docs` — **the official documentation, built from `meta/specs/`** | the repo exists, empty; populated after the compiler is finished |
| — | `nitpick-prototype` — the old compiler, archived | **done 2026-09-02** |
| — | `nitpick-prototype-docs` — the old specification, archived | **done 2026-09-02** |

Every library and application targeting the prototype was archived (or
deleted on the remote) in the same operation — `REPOS/ARCHIVE/` holds them.

## Why the prototype's docs are frozen until then

`nitpick-docs` is **not stale by neglect — it is correct for what it documents.**
It describes the prototype, which is live on GitHub, linked from the website, and
in use today. Updating it to describe this version would send someone who checked
out the prototype to documentation for a compiler they cannot get.

So a contradiction between `nitpick-docs` and a settled decision here is
**expected**, and the correction is recorded in `meta/specs/PROTOTYPE_DELTA.md`
rather than fixed at the source. Write access to the prototype repos is
deliberately not held until the switch, as a guard rather than an oversight.

## The docs are replaced wholesale, not merged

The prototype carries a great deal of historical baggage — not only in the
documentation but in the structure, with several versions of the same thing in
different places, much of it conflicting because it was never updated. Sorting
through it would cost more than it returns, since most of it is no longer valid.

So `nitpick-docs` is **emptied and repopulated from `meta/specs/`**.

**The consequence is a requirement on this repository: `meta/specs/` has to be
complete on its own.** Nothing is merged in to fill gaps, so anything the old set
covers that the new one does not is simply missing afterwards.

### Owed before `nitpick-docs` is filled

A **coverage pass** against the prototype's topic list — the archive happened
first, but archived repositories stay browsable and the prototype's docs are
local under `REPOS/ARCHIVE/nitpick-prototype-docs/`, so the comparison is still
easy to make. The prototype splits its specification across ~28 topic
files; this repository has twenty consolidated references. Topics to check for an
equivalent, none of which obviously has one yet:

- `hardware_os_specs.txt`
- `streams_io_specs.txt` (vs `IO_REFERENCE.md`)
- `asm_specs.txt`
- `build_system_specs.txt` (vs `BUILD_REFERENCE.md`)
- `collections_builtins_specs.txt`
- `api_reference/API_REFERENCE.txt`

### And a decision about what ships

`meta/specs/` holds three kinds of document, and they do not all belong in public
documentation:

- **Fourteen reference manuals** — `TYPE_`, `AST_`, `OP_`, `MEMORY_`, `LEXICAL_`,
  `TRAITS_`, `MODULE_`, `CONTROL_`, `CONCURRENCY_`, `IO_`, `BUILTIN_`, `BUILD_`,
  `VERIFICATION_`, `SAFETY_ARCHITECTURE`. These are the manual and clearly ship.
- **Five transitional documents** — `PROTOTYPE_DELTA`, `PRE_PLANNING_REVIEW`,
  `GRAMMAR_ADOPTION_CONFLICTS`, `FORMAL_DRAFT_AUDIT`, `SPEC_GAPS_AND_AMBIGUITIES`.
  These reconcile carried-over specs against decisions and reference prototype
  internals throughout. They document a migration nobody outside the project needs.
- **`DECISIONS.md`** — a rationale record rather than a reference manual, and the
  one genuinely open question. For a language heading into formal verification, a
  decision log with reasoning is arguably *required* rather than optional: the
  evidence campaign's work (D-233) needs to know why a rule exists, not only that
  it does.

## Versioning

**The version restarts at `0.0`.** The prototype keeps its own history under
`nitpick-prototype`, so this is a fresh line rather than a continuation — the
prototype was a prototype, and this is the real thing.

A jump to `0.100.0` was considered first, to signal the discontinuity while the
two shared a name. The archive plan removes the need: nothing is shared, so
nothing needs signalling.

`nitpick-docs` adopts the compiler's version scheme at the same time, so the
documentation and the compiler it documents carry matching numbers instead of
drifting apart.

### The roadmap's cycle numbers are not renamed

They line up with the version rather than colliding with it, now that the version
starts at `0.0`. Renaming was considered and rejected on cost: **149 files carry a
cycle number in their header**, `DECISIONS.md` cites cycles throughout, and **49
commit subjects begin with one** — and commits cannot be rewritten, so a rename
would break the link between a commit and the file headers it touched.

Write "cycle 0.5.3" rather than a bare "0.5.3" when the plan is meant. The two
schemes now look alike.

### Tag sorting — already handled

`git tag` sorts **lexically** by default, which is wrong for version numbers and
already wrong in the prototype today: across its 434 tags it reports `v0.9.4` as
the newest when the real latest is `v0.89.0`.

This repository sets `tag.sort = v:refname` locally, before its first tag exists.
Set the same on the new repos. `dpkg` compares numerically and is unaffected;
`sort -V` is the shell equivalent.

## The archive, and the one step that cannot be undone cleanly

`nitpick` → `nitpick-prototype` and `nitpick-docs` → `nitpick-prototype-docs`,
both marked archived on GitHub. Archived repositories stay fully browsable and
clonable and keep their issues, tags and releases, so the 434 tags and the `.deb`
releases survive.

**Renaming creates a GitHub redirect from the old URL, and creating the new
`nitpick` repo at that name breaks it.** Every existing link, bookmark and README
reference to the prototype's code then lands on the *new* compiler's code instead
— which is exactly the confusion the freeze exists to prevent, arriving at the
moment the freeze ends.

Two things make it safe, and both are in the plan already:

- **The new `nitpick` README leads with a pointer to `nitpick-prototype`**, so the
  deprecation link doubles as the redirect notice. The website carries the same
  pointer, marked deprecated, for a couple of cycles.
- **Anyone with a prototype clone who runs `git pull` gets "refusing to merge
  unrelated histories"** rather than a silent wrong merge. It fails loudly, which
  is the good outcome.

**This step happened on 2026-09-02**: `nitpick` → `nitpick-prototype` (archived),
then `nitpick-native` → `nitpick`, so the prototype's old URL now lands on this
compiler. The README pointer to `nitpick-prototype` leads this repository's
README from the same day; the website's pointer is the remaining half of the
safeguard.
