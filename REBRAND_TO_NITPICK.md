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

| Current | Decided |
|---------|---------|
| Aria | Nitpick |
| `ariac` | `npkc` — Nitpick compiler (follows gcc/rustc/etc. `c`-suffix convention) |
| `.aria` | `.npk` |
| `aria_make` / `aria-make` | `npkbld` — Nitpick build tool |
| `aria-pkg` / package manager | `npkpkg` — Nitpick package manager |
| AriaX (distro) | **Nitpicker** — raccoon mascot carries over; separate branding workstream |

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

### Phase 0 — Freeze the Aria milestone ✅
- [x] Finish and tag `v0.18.0` as the final Aria-named K semantics milestone.
- [x] Add an audit file documenting what is complete and what remains future work.

### Phase 1 — Public transition notices (mostly done)
- [x] Add a prominent rebrand notice to the main `aria` README.
- [x] Add prominent rebrand notices to all satellite repo READMEs.
- [x] Add local README logo assets using `assets/nitpick_logo.png`.
- [ ] Add website front-page notice (ailp-website).
- [ ] Clean up dirty worktrees in `aria-libc`, `aria-tools`, `ariax`
      (README/logo commits were made but unrelated changes are still parked).

### Phase 2 — Repository renames on GitHub ✅
All 11 repos renamed in place on GitHub. History, issues, tags, releases, and
GitHub redirects preserved. Local remotes updated in each `.git/config`.
Local worktree folder names left unchanged (renaming them would break build paths).

| Old | New |
|-----|-----|
| `aria` | `nitpick` |
| `aria-docs` | `nitpick-docs` |
| `aria-packages` | `nitpick-packages` |
| `aria-libc` | `nitpick-libc` |
| `aria-tools` | `nitpick-tools` |
| `aria-make` | `nitpick-build` |
| `aria-lang` | `nitpick-lang` |
| `aria-packages-apt` | `nitpick-packages-apt` |
| `aria-specialist` | `nitpick-specialist` |
| `aria_community` | `nitpick-community` |
| `ariax` | `nitpicker` |

### Phase 3 — Tooling aliases (compiler / build / pkg) ✅
All changes additive — no compat aliases removed yet.

- [x] 3a: `npkc` is now the primary binary (`OUTPUT_NAME "npkc"`); `ariac` symlink installed via CMake install rule.
- [x] 3b: Compiler accepts `.npk` source files alongside `.aria` (`module_resolver`, `module_table`, `module_resolver_shim`, `doc` tool all updated).
- [x] 3c: `npkbld` is now the primary `aria-make` binary (`OUTPUT_NAME "npkbld"`).
- [x] 3d: `npkpkg` is now the primary `aria-pkg` binary (`OUTPUT_NAME "npkpkg"`); `aria-pkg` compat symlink via CMake install rule.
- [x] 3e: CMake `project()` name changed from `aria` to `nitpick`.
- [x] 3f: `--version` output: "Nitpick Compiler (npkc)"; help text uses `npkc` and `.npk` examples; `.aria` transition note included.

Commits: `293b070` (nitpick/dev-0.18.x), `56327b9` (nitpick-build/main)
CTest: 8/8 passed after changes.

### Phase 4 — Docs and examples migration
Risk: low — no binary impact.

- [ ] 4a: Guide chapters in `nitpick-docs` renamed Aria → Nitpick.
- [ ] 4b: Example source files renamed from `.aria` to `.npk`.
- [ ] 4c: Test suite `.aria` files renamed to `.npk`; CMakeLists updated.
- [ ] 4d: README walkthroughs updated to use `npkc`, `npkbld`, `npkpkg`.

### Phase 5 — Source identifier migration
Risk: medium — per-module, not a mass rename.

- [ ] 5a: Internal C++ diagnostic messages and strings updated (Aria → Nitpick).
- [ ] 5b: `AriaString` aliased to `NpkString`; `AriaString` kept as compat typedef.
- [ ] 5c: Non-ABI-visible internal identifiers (class names, namespaces) renamed.

### Phase 6 — aria-libc C API rename (highest-risk phase)
Generated compiler output and user-facing headers both call `aria_*` symbols.
Must use a shim strategy — not an atomic rename.

- [ ] 6a: Add `npk_*` aliases for all `aria_*` public symbols in a shim header.
- [ ] 6b: Update compiler codegen to emit `npk_*` calls.
- [ ] 6c: Update libc source to use `npk_*` as primary names.
- [ ] 6d: Deprecation window: keep `aria_*` shims for at least one minor release.
- [ ] 6e: Remove `aria_*` shims after bake time.

### Phase 7 — Drop legacy compat aliases
Only after sufficient bake time per alias.

- [ ] Remove `.aria` extension acceptance from compiler.
- [ ] Remove `ariac` symlink/alias.
- [ ] Remove `aria_*` libc shims.
- [ ] Final cleanup of remaining `aria`-prefixed identifiers.

---

## Nitpicker OS (ariax) — Parallel Workstream

Does not block the compiler/language rebrand. Key tasks:

- [ ] Rename `ariax` repo to `nitpicker` on GitHub.
- [ ] Update OS build scripts, branding configs, and skel files to Nitpicker name.
- [ ] Export a Nitpicker-specific logo variant from `aria-docs/pics/nitpik_logo.xcf`
      if needed (transparent background, boot splash, etc.).
- [ ] Update installer/welcome text and desktop defaults.
- [ ] Decide if Nitpicker gets its own website section or sub-domain.

## Do Not Do Yet

- Do not mass-rename source code or C API symbols before the Phase 6 shim strategy is in place.
- Do not break `.aria`, `ariac`, or `aria_*` compatibility before aliases exist.
- Do not rename dirty repositories until in-flight work is committed or parked.
- Do not start Phase 6 (libc C API rename) until Phases 3–5 are stable.

## First-pass README Notice

Recommended wording for repos during the transition:

> **Rebrand in progress:** Aria is becoming **Nitpick**. This repository still
> uses Aria names while the migration is underway. Existing history, tags, and
> compatibility paths are being preserved; source-level renames will happen in
> later coordinated slices.