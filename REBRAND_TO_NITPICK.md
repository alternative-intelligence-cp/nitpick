# Aria → Nitpick Rebrand Plan

**Status:** Started after the `v0.18.0` K Framework executable semantics tag.
**Last Aria-named milestone:** `v0.18.0` — K semantics seed complete.
**New language name:** Nitpick.

Aria is being renamed to **Nitpick** to avoid confusion with other projects using
the Aria name and to make the language easier to find, discuss, package, and
install. The name Nitpick fits the language: it already has `nit`-family
concepts, a `pick` control construct, and a design philosophy that favors
explicit correctness over convenience theater.

## Naming Direction

| Current | Planned / Proposed |
|---------|--------------------|
| Aria | Nitpick |
| `ariac` | `npkc` — Nitpick compiler |
| `.aria` | `.npk` |
| `aria_make` / `aria-make` | `npkb` or `npkbld` — final spelling TBD |
| `aria-pkg` | Nitpick package manager name TBD |

Compatibility aliases should remain during the transition wherever practical.
The goal is to make the new name obvious without breaking existing users in one
giant pain-ball. The pain-ball is real; we are simply slicing it politely.

## Logo Assets

The first Nitpick logo is a raccoon holding a magnifying glass, leaning into the
“pickiness as a feature” identity.

- Master/source image: `aria-docs/pics/nitpik_logo_compressed.png`
- README-friendly repo-local copy: `assets/nitpick_logo.png`
- README display convention: centered HTML image block with descriptive alt text
    and a fixed width (`220`–`280`) so GitHub rendering stays tidy.

Keep README references local to each repository. Do not point satellite READMEs
at another repo's raw image URL; local copies avoid broken images after repo
renames, archive moves, or branch changes.

## Repository Strategy

Preferred path: **rename existing repositories** rather than creating fresh repos
and copying files. Renaming preserves commit history, tags, releases, issues,
pull requests, and GitHub redirect behavior from old URLs.

Proposed mapping:

| Current repo | Proposed repo | Notes |
|--------------|---------------|-------|
| `aria` | `nitpick` | Main compiler/runtime/toolchain history should be preserved. |
| `aria-docs` | `nitpick-docs` | Docs will carry transition notices while content is migrated. |
| `aria-packages` | `nitpick-packages` | Package names can migrate gradually with aliases. |
| `aria-libc` | `nitpick-libc` | Rename after current in-flight libc work is settled. |
| `aria-tools` | `nitpick-tools` | Rename after generated VS Code/tool artifacts are clean. |
| `aria-make` | `nitpick-build` or `nitpick-make` | Depends on final `npkb` / `npkbld` decision. |
| `aria-lang` | `nitpick-lang` | Hub repo / coordination point. |
| `aria-packages-apt` | `nitpick-packages-apt` | APT paths require careful compatibility planning. |
| `aria-specialist` | `nitpick-specialist` | Training corpora and model names can migrate later. |
| `aria_community` | `nitpick-community` | Community/governance rename. |
| `ariax` | TBD | AriaX distro branding needs a separate naming decision. |

## Phases

### Phase 0 — Freeze the Aria milestone

- [x] Finish and tag `v0.18.0` as the final Aria-named K semantics milestone.
- [x] Add an audit file documenting what is complete and what remains future work.

### Phase 1 — Public transition notices

- [x] Add a prominent rebrand notice to the main `aria` README.
- [x] Add prominent rebrand notices to all satellite repo READMEs.
- [x] Add local README logo assets using `assets/nitpick_logo.png`.
- [ ] Add website/front-page notices.
- [ ] Add package-manager notices once package naming is decided.

### Phase 2 — Repository names

- [ ] Confirm final repo mapping.
- [ ] Rename GitHub repositories in place where possible.
- [ ] Verify GitHub redirects and update local remotes.
- [ ] Archive or freeze any legacy-only repos that should not continue under Nitpick.

### Phase 3 — Tooling aliases

- [ ] Add `npkc` while keeping `ariac` as a compatibility alias.
- [ ] Add `.npk` file-extension support while keeping `.aria` accepted during migration.
- [ ] Decide and implement the build tool name (`npkb` vs `npkbld`).
- [ ] Add package-manager naming/alias strategy.

### Phase 4 — Source and documentation migration

- [ ] Rename user-facing docs from Aria to Nitpick.
- [ ] Rename examples and file extensions.
- [ ] Rename package prefixes where appropriate.
- [ ] Rename source-level identifiers only after compatibility and docs are planned.

## Do Not Do Yet

- Do not mass-rename source code blindly.
- Do not break `.aria`, `ariac`, or package compatibility before aliases exist.
- Do not rename dirty repositories until in-flight unrelated work is committed,
  parked, or explicitly preserved.

## First-pass README Notice

Recommended wording for repos during the transition:

> **Rebrand in progress:** Aria is becoming **Nitpick**. This repository still
> uses Aria names while the migration is underway. Existing history, tags, and
> compatibility paths are being preserved; source-level renames will happen in
> later coordinated slices.