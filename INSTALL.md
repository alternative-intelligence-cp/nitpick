# Installing Nitpick

This builds the compiler from source on a clean Linux machine and compiles a
first program with it. It takes about a minute once the tools are installed.
Everything here was run, in this order, on a fresh clone.

## 1. What you need

- A 64-bit Linux on x86-64. Nitpick programs are static binaries with no libc:
  the compiler's output targets `x86_64-unknown-linux-gnu` and links against
  its own runtime, nothing else.
- **LLVM 20** — the three tools `llc`, `opt` and `ld.lld`. **Prefer your
  distribution's own packages where it ships LLVM 20** — Ubuntu 26.04 and the
  Debian releases that carry `llvm-20` do (about 235 MB installed):

  ```sh
  sudo apt install llvm-20 lld-20
  ```

  Where the distribution has no `llvm-20`, or an older one than 20.1.8 (Ubuntu
  24.04 "noble" ships 20.1.2, which §6 refuses), use the LLVM project's own
  repository (about 880 MB for the 26 packages it installs):

  ```sh
  wget https://apt.llvm.org/llvm.sh
  chmod +x llvm.sh
  sudo ./llvm.sh 20
  sudo apt install lld-20
  ```

  `llvm.sh` does not check that a suite exists for your release: on one it does
  not serve LLVM 20 for (Ubuntu 26.04 today — it serves only 21 and later there,
  and the fetch answers 404, "does not have a Release file") it exits 100 after
  half a minute and leaves a dead source and key behind, so every later
  `apt-get update` fails. The recovery: delete the file it wrote under
  `/etc/apt/sources.list.d/` (named for `llvm-toolchain-<release>-20`) and the
  key it added under `/etc/apt/trusted.gpg.d/`, then `sudo apt-get update`, and
  take the distribution route above. Do not pass `--no-install-recommends` to
  either route: `llc` and `opt` arrive through the `llvm-20` package, which the
  recommends pull in.

  On a machine that also holds the distribution's 32-bit LLVM library
  (`libllvm20:i386`, pulled in by the 32-bit Mesa drivers that Steam and Wine
  need), `apt` refuses the repository's packages ("libllvm20 … but 1:20.1.2…
  is to be installed"): apt.llvm.org ships no i386 build, and a multi-arch
  library must stay at one version across its architectures. Do not remove the
  32-bit drivers. Install the release as a PREFIX instead, from the same signed
  packages, with no sudo beyond the repository line `llvm.sh` already added:

  ```sh
  mkdir -p ~/.local/llvm-20.1.8/debs && cd ~/.local/llvm-20.1.8/debs
  apt-get download libllvm20 llvm-20 llvm-20-runtime llvm-20-linker-tools llvm-20-dev lld-20 \
                   clang-20 libclang-cpp20 libclang1-20 libclang-common-20-dev
  for d in *.deb; do dpkg-deb -x "$d" ~/.local/llvm-20.1.8; done
  ```

  `apt-get download` checks each package against the repository's signed index.
  The tools then live in `~/.local/llvm-20.1.8/usr/lib/llvm-20/bin`, their
  library beside them (found through the binaries' own `$ORIGIN/../lib`;
  `llvm-20-dev` carries the `libLLVM.so.20.1` link that makes this work, so do
  not leave it out), and the links below point there instead of
  `/usr/lib/llvm-20/bin`. The distribution's 20.1.2 stays where it is for
  everything else on the machine. (This is how the project's own machine moved
  to 20.1.8 on 2026-10-08.)

  Both apt routes install versioned names (`llc-20`, `opt-20`, `ld.lld-20`); the
  prefix route puts the plain names under its own `bin`. The commands below use
  the plain names; either put links on your `PATH` (for the prefix route,
  `~/.local/llvm-20.1.8/usr/lib/llvm-20/bin` in place of `/usr/lib/llvm-20/bin`):

  ```sh
  mkdir -p ~/.local/bin
  for t in llc opt ld.lld; do ln -sf /usr/lib/llvm-20/bin/$t ~/.local/bin/$t; done
  export PATH="$HOME/.local/bin:$PATH"     # add this line to ~/.bashrc (bash), or ~/.profile for a login shell
  ```

  or spell the versioned names yourself (they give byte-identical output).
  Check with `llc --version`: it must report an LLVM 20.1 release.
- `git`.
- Nothing else. Python is used only by the compiler's own development test
  harness, not to build or use the compiler. There is no C compiler in the
  build: the runtime is hand-written LLVM IR.

> The compiler's own build and test driver (`npkg`, §6) holds the toolchain to
> the exact release recorded in `nitpick.toml` — `20.1.8`, the LLVM 20.1 release
> the distributions serve (D-349 in `meta/specs/DECISIONS.md`, 2026-10-08; the pin
> was `20.1.2` from the day it was made until then) — and refuses another, because
> a patch release can change instruction selection and the project's test results
> are recorded against one. Building and using the compiler by hand, as below,
> works with any LLVM 20.1 release; §§2–5 need no pin. Both routes above install
> 20.1.8 today (apt.llvm.org's noble suite and Ubuntu 26.04's archive alike);
> Ubuntu 24.04's own archive stops at 20.1.2, so on it §6 needs the apt.llvm.org
> route.

**What it was tested at.** This procedure was run, in this order, on Linux
Mint 22.3 (an Ubuntu 24.04 base), x86-64, LLVM 20.1.2, and on a fresh Ubuntu
Server 26.04.1 LTS virtual machine (16 vCPUs, 15 GiB; the distribution's LLVM
20.1.8; §§2–5 and the optimised build, 2026-10-08). On 2026-10-08 the Mint machine moved to LLVM 20.1.8 by §1's prefix route (its `apt` route was blocked by the 32-bit Mesa drivers' `libllvm20:i386`), and the project's full test suite ran green under it (D-349's landing). The build runs ONE process
at a time — it uses no parallelism, so the number of cores does not matter —
and its peak memory is 345 MB, in `llc` assembling the compiler's 31 MB of IR
(measured with `/usr/bin/time -v` on both machines); 1 GB of free memory is
ample. It takes about fifty seconds of CPU on a 2024 desktop processor (52 s on
the VM). Disk: the clone is about 100 MB, the build adds about 80 MB, the
installed pair of §4 is 11 MB, and LLVM itself is the figure given above for
the route you took. Compiling a small program afterwards takes a quarter of a
second and about 75 MB.

## 2. Get the source

```sh
git clone https://github.com/alternative-intelligence-cp/nitpick.git
cd nitpick
```

## 3. Build the compiler

The compiler is written in Nitpick and builds itself from a committed snapshot
of its own output (`bootstrap/seed/stage1.ll`, LLVM IR). Three steps, from the
repository's root:

```sh
mkdir -p build

# 1. the runtime floor: hand-written LLVM IR, linked into every program
llc -O0 -filetype=obj -relocation-model=static runtime/npkrt.ll -o build/npkrt.o

# 2. the builder: the snapshot of the compiler, assembled and linked
llc -O0 -filetype=obj -relocation-model=static bootstrap/seed/stage1.ll -o build/builder.o
ld.lld -static -o build/builder build/builder.o build/npkrt.o

# 3. the compiler from its current source, compiled by the builder
./build/builder src/npkc.npk -o build/npkc.ll
llc -O0 -filetype=obj -relocation-model=static build/npkc.ll -o build/npkc.o
ld.lld -static -o build/npkc build/npkc.o build/npkrt.o
```

Step 2 takes about fifteen seconds, step 3 about thirty. The result is
`build/npkc`, the compiler, and `build/npkrt.o`, the runtime object every
program links against. Keep both.

## 4. Install

There is no install target; the compiler is one static binary and the runtime
is one object file. Put them where you want them:

```sh
install -D -m 755 build/npkc    ~/.local/bin/npkc
install -D -m 644 build/npkrt.o ~/.local/lib/nitpick/npkrt.o
```

The rest of this file assumes those two paths.

## 5. A first program

Save this as `hello.npk`, in any directory; the paths in this section are
relative to it. The file's `mod:` name must match its file name.

```
mod:hello;

func:main = int32(cstring[]:_~argv) {
    string:greeting = "hello from Nitpick\n";
    discard(write(1i32 =>! fd, greeting.ptr =>! wild int8->, greeting.len) ?! E9);
    exit 0i32;
};

error:E9;

func:failsafe = int32(Error:e) {
    pick (e) {
        (E9)             { exit 9i32; },
        (StackExhausted) { exit 97i32; },
        (MachineFault)   { exit 98i32; },
        (Unreachable)    { exit 95i32; },
        (HeapOom)        { exit 92i32; },
        (HeapBadRequest) { exit 91i32; },
        (WildLeak)       { exit 96i32; },
        (*)              { exit 99i32; }
    }
    exit 99i32;
};
```

What is in it, since none of it is optional:

- `main` takes the command line and returns an `int32`. `exit` is legal only
  in `main` and `failsafe`: everywhere else a function reports failure with a
  value, never by stopping the program.
- `write` is one kernel write to file descriptor 1. Every function that can
  fail returns a `Result`; `?! E9` takes the value or stops the program with
  the error `E9`, declared on the line `error:E9;`. A value is never dropped
  in silence, so the byte count `write` returns is `discard`ed on purpose.
- `failsafe` is where every uncaught error arrives, and it must name every
  error that can reach it. The compiler lists the missing ones by name
  (`NITPICK-REACH-002 ... failsafe does not name HeapOom`), so write the two
  lines first, compile, and add the arms it asks for. `(*)` is required and
  counts for nothing: a new failure mode is always acknowledged by name.

Compile, link and run:

```sh
npkc hello.npk -o hello.ll
llc -O0 -filetype=obj -relocation-model=static hello.ll -o hello.o
ld.lld -static -o hello hello.o ~/.local/lib/nitpick/npkrt.o
./hello
```

For an optimised build, run the IR through `opt` first:

```sh
opt -O2 -S hello.ll -o hello.opt.ll
llc -O2 -filetype=obj -relocation-model=static hello.opt.ll -o hello.o
ld.lld -static -o hello hello.o ~/.local/lib/nitpick/npkrt.o
```

Both forms must behave identically; the project's own tests run every program
both ways. A three-line script saves the typing:

```sh
#!/bin/sh
# npkbuild NAME -- compile NAME.npk to the binary NAME
set -e
npkc "$1.npk" -o "$1.ll"
llc -O0 -filetype=obj -relocation-model=static "$1.ll" -o "$1.o"
ld.lld -static -o "$1" "$1.o" ~/.local/lib/nitpick/npkrt.o
```

## 6. Checking the build (optional)

`npkg` is the project's build and test driver, itself a Nitpick program. Build
it with the compiler you just made and let it rebuild the compiler through the
same ladder, printing a digest per intermediate:

```sh
./build/npkc npkg/main.npk -o build/npkg.ll
llc -O0 -filetype=obj -relocation-model=static build/npkg.ll -o build/npkg.o
ld.lld -static -o build/npkg build/npkg.o build/npkrt.o
./build/npkg build
```

It prints one `sha256` line per intermediate. The line for `build/npkc.ll` is
the one that must match across machines on the same LLVM release: the emitted
IR is the project's cross-machine claim (D-265), and the notice each landing
sends to the library repositories carries the project's digest for it; the
object and the binary belong to your toolchain build. `npkg build` refuses an
LLVM release other than the one `nitpick.toml` records (20.1.8; §1's note). `npkg
test` runs the whole suite and takes about three hours.

## 7. Where to go next

- `meta/specs/` is the language specification: one `*_REFERENCE.md` per
  area (`LEXICAL`, `TYPE`, `OP`, `CONTROL`, `MEMORY`, `MODULE`, `TRAITS`,
  `MACRO`, `CONCURRENCY`, `IO`, `BUILTIN`, `BUILD`, `VERIFICATION`), and
  `meta/specs/DECISIONS.md` records every design decision with its
  reasoning — start there when something looks unusual, because it says why.
- `tests/backend/programs/` holds some four hundred complete programs, each
  run by the test suite at both optimisation levels: the quickest way to see a
  construct used. Five of them (`fd_io`, `file_io`, `read_big_once`,
  `read_big_many`, `unwrap_forms`) write into `.internal/`, a gitignored
  directory a fresh clone lacks: `mkdir -p .internal` at the root first.
- The libraries live in their own repositories under the same organisation
  (`nitpick-libs`); each builds against this compiler and says how.

## 8. If something goes wrong

| What you see | What it means |
|---|---|
| `llc: command not found` | the versioned names are installed and the links of §1 are not on your `PATH` |
| `ld.lld: error: unable to find library` or an undefined `npk_...` symbol | the link line is missing `npkrt.o` (§3 step 1, §4) |
| `NITPICK-RESOLVE-012 file 'x.npk' declares 'mod:y;' first` | the file's `mod:` name does not match its file name |
| `NITPICK-RESOLVE-005` | a `mod:` or `use` names a file that does not exist |
| `NITPICK-REACH-002 failsafe does not name X` | add the arm `(X) { exit N; }` to `failsafe` |
| `NITPICK-TYPE-039 the error is handled but the VALUE is discarded` | wrap the statement in `discard(...)` |
| `NITPICK-TYPE-010 exit is legal only in main and failsafe` | return a `Result` instead; `fail E9;` reports an error |

Every diagnostic carries a code; the specification's reference files
(`meta/specs/*_REFERENCE.md`) describe each one.
