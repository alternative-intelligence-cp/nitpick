# Installing Aria

This document covers every way to install the Aria compiler toolchain.

---

## Quick Install (Linux)

```bash
git clone https://github.com/alternative-intelligence-cp/aria.git
cd aria
./install.sh
```

The install script detects your distro, checks prerequisites, builds all tools,
installs them to `/usr/local`, and sets up `~/.aria/env` for your shell.

**Options:**

| Flag | Description |
|------|-------------|
| `--prefix=DIR` | Install to DIR instead of /usr/local |
| `--build-only` | Build without installing |
| `--install-deps` | Automatically install missing packages (apt/dnf/pacman) |
| `--no-deps` | Skip dependency checking entirely |
| `--uninstall` | Remove a previous installation |

After install, either restart your shell or run:

```bash
source ~/.aria/env
```

**Verify:**

```bash
ariac --help
aria-pkg help
aria_make --version
```

---

## Debian / Ubuntu Package (.deb)

Pre-built `.deb` packages are available for Debian 12+, Ubuntu 22.04+, and
derivatives (Linux Mint, Pop!_OS, etc.).

### Build the .deb yourself

```bash
cd aria
./packaging/build-deb.sh
```

This produces `aria-lang_<version>_amd64.deb` in the `packaging/` directory.

### Install

```bash
sudo dpkg -i packaging/aria-lang_*_amd64.deb
```

If dependencies are missing:

```bash
sudo apt install -f
```

### Remove

```bash
sudo dpkg -r aria-lang         # remove (keep config)
sudo dpkg --purge aria-lang    # remove everything
```

### What's included

- `ariac` — compiler
- `aria-ls` — language server
- `aria-pkg` — package manager
- `aria-doc` — documentation generator
- `aria_make` — build system
- 69 stdlib modules (`/usr/lib/aria/stdlib/`)
- 30 aria-libc sources + 18 shim libraries (`/usr/lib/aria/libc/`)
- `liburing.so.2` bundled in `/usr/lib/aria/lib/`
- ldconfig entry at `/etc/ld.so.conf.d/aria.conf`

---

## Fedora / RHEL Package (.rpm)

### Build the .rpm yourself

```bash
cd aria
./packaging/build-rpm.sh
```

This produces `aria-lang-<version>.x86_64.rpm` in `~/rpmbuild/RPMS/x86_64/`.

### Install

```bash
sudo rpm -ivh ~/rpmbuild/RPMS/x86_64/aria-lang-*.x86_64.rpm
```

### Remove

```bash
sudo rpm -e aria-lang
```

The `.rpm` includes the same contents as the `.deb` (see above), with libraries
at `/usr/lib64/aria/` per RPM convention.

---

## Build from Source

### Prerequisites

| Dependency | Minimum | Check |
|-----------|---------|-------|
| LLVM | 20.1+ | `llvm-config-20 --version` |
| CMake | 3.20+ | `cmake --version` |
| C++17 compiler | GCC 10+ or Clang 14+ | `g++ --version` |
| liburing-dev | 2.0+ | `dpkg -l liburing-dev` |

**Debian/Ubuntu:**

```bash
sudo apt install -y build-essential cmake llvm-20-dev liburing-dev
```

**Fedora/RHEL:**

```bash
sudo dnf install -y gcc-c++ cmake llvm-devel liburing-devel
```

**Arch:**

```bash
sudo pacman -S base-devel cmake llvm liburing
```

### Build

```bash
git clone https://github.com/alternative-intelligence-cp/aria.git
cd aria
mkdir -p build && cd build
cmake ..
cmake --build . -j$(nproc)
```

### Build with aria-make and aria-libc (optional)

If you also want the build system and C interop library:

```bash
# Build aria-make (in sibling directory)
cd ../..
git clone https://github.com/alternative-intelligence-cp/aria-make.git
cd aria-make && mkdir -p build && cd build
cmake .. && cmake --build . -j$(nproc)

# Build aria-libc shims
cd ../../aria
git clone https://github.com/alternative-intelligence-cp/aria-libc.git ../aria-libc
# Shim libraries are pre-built .a files in aria-libc/shim/
```

### Install manually

```bash
sudo cp build/ariac build/aria-ls build/aria-pkg build/aria-doc /usr/local/bin/
sudo cp ../aria-make/build/aria_make /usr/local/bin/
sudo mkdir -p /usr/local/lib/aria/stdlib
sudo cp stdlib/*.aria /usr/local/lib/aria/stdlib/
```

---

## Package Manager (aria-pkg)

Once installed, use `aria-pkg` to fetch Aria packages from the registry:

```bash
aria-pkg update                    # sync package registry
aria-pkg list --remote             # browse all packages
aria-pkg list --remote http        # search by keyword
aria-pkg install aria-json         # install a package
aria-pkg list                      # list installed packages
aria-pkg remove aria-json          # uninstall
```

Packages install to `~/.aria/packages/installed/`.

See the [aria-pkg guide](https://github.com/alternative-intelligence-cp/aria-docs)
for full usage.

---

## Supported Platforms

| Platform | Status |
|----------|--------|
| Linux x86_64 | Fully supported |
| macOS (Apple Silicon) | Builds, lightly tested |
| WSL2 (Windows) | Builds, lightly tested |
| Native Windows | Not yet supported |

---

## Uninstall

**Install script method:**
```bash
./install.sh --uninstall
```

**Debian package:**
```bash
sudo dpkg --purge aria-lang
```

**RPM package:**
```bash
sudo rpm -e aria-lang
```

**Manual:** Remove binaries from wherever you copied them and delete
`/usr/local/lib/aria/` and `~/.aria/`.
