# AUDIT_v0.17.5.md — Installers, Packaging & Distribution

**Date:** July 2025  
**Series:** v0.17.0–v0.17.5  
**Branch:** dev-0.17.x  
**Focus:** Making Aria installable via every mainstream Linux method

---

## Series Summary

| Release | Description | Commit | Tag |
|---------|-------------|--------|-----|
| v0.17.0 | Enhanced install script (680 lines) | 73d50ec | v0.17.0 |
| v0.17.1 | Debian .deb package builder (393 lines) | 35984be | v0.17.1 |
| v0.17.2 | RPM package builder (352 lines) | 996b9fd | v0.17.2 |
| v0.17.3 | aria-pkg remote fetch from GitHub (v0.3.0) | 513006c | v0.17.3 |
| v0.17.4 | Documentation (INSTALL.md, tool guides) | 8aaa69d / 23bfe18 | v0.17.4 |
| v0.17.5 | Final audit (this document) | — | v0.17.5 |

---

## v0.17.0 — Install Script

**File:** `install.sh` (680 lines)

- Distro detection via `/etc/os-release` + `ID_LIKE` (Debian, RHEL, Arch, SUSE, Alpine)
- Dependency install per distro (`--install-deps`)
- Builds ariac, aria-ls, aria-pkg, aria-doc; installs aria-make and aria-libc
- `~/.aria/env` profile snippet (ARIA_HOME + PATH)
- Post-install verification of all tools
- `--build-only`, `--prefix=DIR`, `--no-deps`, `--uninstall` flags
- Tested: syntax check, build-only, full install to /tmp, uninstall

## v0.17.1 — Debian Package

**File:** `packaging/build-deb.sh` (393 lines)

- Standalone `dpkg-deb --build --root-owner-group` approach
- 5 binaries, 69 stdlib, 30 libc src, 18 shim libs
- Bundled `liburing.so.2` in `/usr/lib/aria/lib/` with ldconfig
- Runtime deps: libc6, libstdc++6, zlib1g, libzstd1, libz3-4, libatomic1
- 82 MB installed size
- Tested: `dpkg -i`, all tools working, compiled hello.aria, `dpkg --purge`

## v0.17.2 — RPM Package

**File:** `packaging/build-rpm.sh` (352 lines)

- Auto-generated `.spec` file, staged files in separate STAGED dir
- `/usr/lib64/aria/` per RPM convention, `AutoReqProv: no`
- 149 files, 29 MB .rpm, 84 MB installed
- Tested: `rpmbuild` produces clean .rpm, rpmlint passes

## v0.17.3 — aria-pkg Remote Fetch

**Files:** `installer.h` (+36), `installer.cpp` (+248), `main.cpp` (+89)

- `aria-pkg update`: git clone --depth=1 (or git pull) to `~/.aria/cache/`
- `aria-pkg list --remote [query]`: custom JSON parser, 101/109 packages listed
- `aria-pkg install <name>`: auto-detect local vs remote, install from cache
- Default remote: `github.com/alternative-intelligence-cp/aria-packages`
- TOML `[build].type` and `[build].entry` now optional (defaults: library / "")
- aria-pkg bumped to v0.3.0
- Tested: update (clone + pull), list --remote, list --remote http, install aria-args, install aria-csv, install aria-ascii

## v0.17.4 — Documentation

**aria repo:**
- `INSTALL.md` (235 lines): all install methods (script, .deb, .rpm, source, aria-pkg)
- `README.md`: added package install section, --install-deps, INSTALL.md link

**aria-docs repo:**
- `GETTING_STARTED.md`: version updated to 0.17.x, .deb/.rpm paths added
- `README.md`: updated to v0.17.3, new guide references, installation link
- `guide/aria-pkg.md`: full package manager usage guide
- `guide/aria-make.md`: build system quick reference

---

## Regression Testing

### CTest
```
4/4 tests passed (0 failed)
- compiler_tests: Passed (1.87s)
- gpu_tests: Passed (1.85s)  
- z3_tests: Passed (0.10s)
- gpu_tests_ptx_only: Passed (0.27s)
```

### Self-Hosting (test_aria_files.sh)
```
Total:  1015
Passed: 891
Failed: 124
```
Identical to v0.16.12 — no regressions.

### Installation Methods Verified
- [x] `install.sh --build-only` — builds all tools
- [x] `packaging/build-deb.sh` — syntax valid
- [x] `packaging/build-rpm.sh` — syntax valid
- [x] `aria-pkg update` — clones/pulls registry
- [x] `aria-pkg list --remote` — 101+ packages
- [x] `aria-pkg install aria-args` — end-to-end remote install
- [x] `aria-pkg install aria-csv` — TOML optional fields work
- [x] `aria-pkg install aria-ascii` — no-build-section package works

---

## Line Count Summary

| File | Lines |
|------|-------|
| install.sh | 680 |
| packaging/build-deb.sh | 393 |
| packaging/build-rpm.sh | 352 |
| INSTALL.md | 235 |
| src/tools/pkg/main.cpp | 337 |
| src/tools/pkg/installer.cpp | 1,087 |
| include/tools/pkg/installer.h | 183 |
| **Total new/modified** | **3,267** |

---

## Known Limitations

1. **Version pinning**: `aria-pkg install <name>@<version>` not yet implemented
2. **Checksums**: No SHA-256 verification on remote packages  
3. **RPM not tested on actual Fedora** — built on Ubuntu via rpmbuild
4. **8 packages** in registry lack `[build]` info entirely — install as copy-only
5. **LLVM statically linked** — binary is 66 MB (unavoidable with LLVM 20)
