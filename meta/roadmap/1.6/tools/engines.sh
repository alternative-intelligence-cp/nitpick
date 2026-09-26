#!/bin/bash
# engines.sh -- the 1.6.0 gate's engines, built at their PINNED commits on the workbench (1.6.0.md §2.1; D-233:
# pinned by commit hash, built locally, auditable). Nothing here enters the artifact or the tree beyond
# `pins.txt`, which records the sha256 of every binary and library the gate runs (P-4: an evidence tool's
# output is a verdict, so its binary is pinned by digest beside its commit -- D-265's asymmetry, applied).
#
#   engines.sh build [ENGINE...]
#                        clone each repository at its commit under ~/.local/src/1.6/<name>-<sha7>/, build Release,
#                        install, apply each RECORDED PATCH (D-321 (3): a patch is part of the pin, lives beside this
#                        script, and is applied by this script and by nothing else), and write pins.txt beside this
#                        script. With ENGINE names (z3 alive2 nikos clam) only those are rebuilt; pins.txt is rewritten
#                        from every install
#   engines.sh check     recompute every digest pins.txt records and report each line that differs (a C++ build is
#                        not byte-reproducible by contract: the pin is the COMMIT, the digest says what RAN)
#   engines.sh paths     print the tool paths the gate scripts use (one per line, NAME=PATH)
#
# The pins (1.6.0.md §2.1; the record says which commit each was measured at):
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="${ENGINES_ROOT:-$HOME/.local/src/1.6}"
JOBS="${ENGINES_JOBS:-$(( $(nproc) < 40 ? $(nproc) : 40 ))}"
PINS="$HERE/pins.txt"
ALIVE2_PATCH="$HERE/alive2-rlimit.patch"        # D-321 (1): Z3's rlimit as Alive2's budget -- the ONE recorded patch
PATCH="$ALIVE2_PATCH"                            # (1.6.0 step 4 read Clam's cell-mapping abort for the S-101 demotion and found no
                                                 #  sound one: the abort is the pinned code's sound behaviour, so no second patch exists)

NIKOS_REPO=https://github.com/alternative-intelligence-cp/nikos.git
NIKOS_SHA=db47f9da0308ef44d8440178e7daca206f3e5401          # the `nitpick-port` branch's head (D-324; 1.6.1 step 0): v2.4.0 (94b54c2) + the three commits of the step-4 reading -- the inttoptr constraint added, i128 sampled into the AR data layout, the coroutine intrinsics in every switch
CLAM_REPO=https://github.com/seahorn/clam.git
CLAM_SHA=e80f974582904365123076f40ee1da569d829f18           # master, 2026-09-10 (targets LLVM 18.1)
CRAB_REPO=https://github.com/seahorn/crab.git
CRAB_SHA=49773d0a5740c1fdb37afb15f6f9cc6cfba7b5ae           # dev, 2026-09-22
SEADSA_REPO=https://github.com/seahorn/sea-dsa.git
SEADSA_SHA=b43a9006092225be2569071904d9ded10cf66599         # dev18, 2026-08-19
LLVMSEA_REPO=https://github.com/seahorn/llvm-seahorn.git
LLVMSEA_SHA=6cfacb810374711b4f7252f412edb2dd23e0f22e        # dev18, 2026-07-14
ALIVE2_REPO=https://github.com/AliveToolkit/alive2.git
ALIVE2_SHA=02ec3af82163bdd29297c033361bd34aa8ba953a           # 2025-01-17, the last commit before 9ff342f2 (captures(none)); re-read from the clone by build_alive2
ALIVE2_SHORT=02ec3af8                                       # 2025-01-17, the last commit before 9ff342f2 (captures(none))
Z3_REPO=https://github.com/Z3Prover/z3.git
Z3_SHA=ddb49568d3520e99799e364fb22f35fc67d887b1             # tag z3-4.16.0, the tree's [verify] solver commit

LLVM20=/usr/lib/llvm-20
LLVM18=/usr/lib/llvm-18

short() { echo "${1:0:7}"; }
say()   { echo "engines.sh: $*"; }
die()   { echo "engines.sh: $*" >&2; exit 2; }

# clone_at REPO SHA DIR [PATCH] -- a fresh clone checked out at exactly SHA (a branch name is never a pin); a
# modified tree is tolerated only when its modification IS the recorded patch (D-321 (3))
clone_at() {
  local repo=$1 sha=$2 dir=$3 patch=${4:-}
  if [ -d "$dir/.git" ]; then
    say "$dir exists; verifying its commit"
  else
    git clone -q "$repo" "$dir"
  fi
  git -C "$dir" checkout -q "$sha" 2>/dev/null || { git -C "$dir" fetch -q origin; git -C "$dir" checkout -q "$sha"; }
  local head; head=$(git -C "$dir" rev-parse HEAD)
  case "$head" in "$sha"*) ;; *) die "$dir is at $head, not $sha";; esac
  if git -C "$dir" status --short --untracked-files=no | grep -q .; then   # build/ and install/ are untracked by design
    if [ -n "$patch" ] && [ -f "$patch" ] && git -C "$dir" apply --check --reverse "$patch" 2>/dev/null; then
      say "$dir carries the recorded patch $(basename "$patch")"
    else
      die "$dir has local modifications that are not a recorded patch"
    fi
  fi
}

# apply_recorded_patch DIR PATCH LABEL -- apply PATCH at DIR unless it is already applied; a patch that neither
# applies nor reverses cleanly is a pin that does not match its commit, which stops the build
apply_recorded_patch() {
  local dir=$1 patch=$2 label=$3
  [ -f "$patch" ] || return 0
  if git -C "$dir" apply --check --reverse "$patch" 2>/dev/null; then say "$label: $(basename "$patch") already applied"; return 0; fi
  git -C "$dir" apply --check "$patch" || die "$label: the recorded patch $(basename "$patch") does not apply at $(git -C "$dir" rev-parse --short HEAD)"
  git -C "$dir" apply "$patch"
  say "$label: applied $(basename "$patch")"
}

need() { for t in "$@"; do command -v "$t" >/dev/null || die "missing tool: $t"; done; }

build_z3() {
  local dir="$ROOT/z3-$(short $Z3_SHA)"
  clone_at "$Z3_REPO" "$Z3_SHA" "$dir"
  mkdir -p "$dir/build-lib"
  # SHARED, not static: z3's internal `smt::context` symbols collide with Alive2's own `smt::context` in a
  # static link (measured at step 1: "multiple definition of smt::context::init()"); the shared library exports
  # the C API alone, and Alive2 resolves it by rpath to THIS prefix, never to the system's libz3 (checked below).
  ( cd "$dir/build-lib" && cmake -GNinja -DCMAKE_BUILD_TYPE=Release -DZ3_BUILD_LIBZ3_SHARED=ON \
      -DZ3_BUILD_EXECUTABLE=OFF -DZ3_BUILD_TEST_EXECUTABLES=OFF -DZ3_ENABLE_EXAMPLE_TARGETS=OFF \
      -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_C_COMPILER=clang -DCMAKE_INSTALL_PREFIX="$dir/install" .. \
      && ninja -j"$JOBS" && ninja install ) > "$ROOT/logs/z3.log" 2>&1 || die "z3 library build failed (see $ROOT/logs/z3.log)"
  [ -f "$dir/install/lib/libz3.so" ] || die "z3: no libz3.so"
  Z3_PREFIX="$dir/install"
}

build_alive2() {
  local dir="$ROOT/alive2-$ALIVE2_SHORT"
  if [ ! -d "$dir/.git" ]; then git clone -q "$ALIVE2_REPO" "$dir"; fi
  git -C "$dir" checkout -q -- . 2>/dev/null || true
  git -C "$dir" checkout -q "$ALIVE2_SHORT"
  ALIVE2_SHA=$(git -C "$dir" rev-parse HEAD)
  [ -f "$ALIVE2_PATCH" ] || die "missing $ALIVE2_PATCH (D-321)"
  apply_recorded_patch "$dir" "$ALIVE2_PATCH" alive2
  rm -rf "$dir/build"; mkdir -p "$dir/build"
  ( cd "$dir/build" && cmake -GNinja -DCMAKE_BUILD_TYPE=Release -DLLVM_DIR="$LLVM20/lib/cmake/llvm" -DBUILD_TV=1 \
      -DZ3_INCLUDE_DIR="$Z3_PREFIX/include" -DZ3_LIBRARIES="$Z3_PREFIX/lib/libz3.so" \
      -DCMAKE_EXE_LINKER_FLAGS="-Wl,-rpath,$Z3_PREFIX/lib" -DCMAKE_SHARED_LINKER_FLAGS="-Wl,-rpath,$Z3_PREFIX/lib" .. \
      && ninja -j"$JOBS" ) > "$ROOT/logs/alive2.log" 2>&1 || die "alive2 build failed (see $ROOT/logs/alive2.log)"
  "$dir/build/alive-tv" --version >/dev/null 2>&1 || die "alive2: alive-tv does not run"
  local z3so; z3so=$(ldd "$dir/build/alive-tv" | awk '/libz3/ {print $3}')
  [ "$(dirname "$z3so")" = "$Z3_PREFIX/lib" ] || die "alive2: alive-tv resolves libz3 to '$z3so', not into the pinned $Z3_PREFIX/lib"
  [ "$(readlink -f "$z3so")" = "$(readlink -f "$Z3_PREFIX/lib/libz3.so")" ] || die "alive2: '$z3so' is not the pinned libz3.so"
  ALIVE2_DIR="$dir"
}

build_nikos() {
  local dir="$ROOT/nikos-$(short $NIKOS_SHA)"
  clone_at "$NIKOS_REPO" "$NIKOS_SHA" "$dir"
  mkdir -p "$dir/build"
  ( cd "$dir/build" && cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$dir/install" \
      -DLLVM_CONFIG_EXECUTABLE="$LLVM20/bin/llvm-config" .. && make -j"$JOBS" && make install ) \
      > "$ROOT/logs/nikos.log" 2>&1 || die "nikos build failed (see $ROOT/logs/nikos.log)"
  for t in ikos-analyzer ikos-pp ikos-import; do
    "$dir/install/bin/$t" --version >/dev/null 2>&1 || die "nikos: $t does not run"
  done
  NIKOS_DIR="$dir"
}

build_clam() {
  local dir="$ROOT/clam-$(short $CLAM_SHA)"
  clone_at "$CLAM_REPO" "$CLAM_SHA" "$dir"
  clone_at "$CRAB_REPO" "$CRAB_SHA" "$ROOT/crab-$(short $CRAB_SHA)"
  clone_at "$SEADSA_REPO" "$SEADSA_SHA" "$ROOT/sea-dsa-$(short $SEADSA_SHA)"
  clone_at "$LLVMSEA_REPO" "$LLVMSEA_SHA" "$ROOT/llvm-seahorn-$(short $LLVMSEA_SHA)"
  # Clam's CMake finds llvm-seahorn only inside its own source tree (1.6.0.md §2.1)
  ln -sfn "$ROOT/llvm-seahorn-$(short $LLVMSEA_SHA)" "$dir/llvm-seahorn"
  mkdir -p "$dir/build"
  ( cd "$dir/build" && cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$dir/install" \
      -DLLVM_DIR="$LLVM18/lib/cmake/llvm" -DCRAB_ROOT="$ROOT/crab-$(short $CRAB_SHA)" \
      -DSEADSA_ROOT="$ROOT/sea-dsa-$(short $SEADSA_SHA)" -DCLAM_INCLUDE_TESTS=OFF .. \
      && cmake --build . -j"$JOBS" && cmake --build . --target install ) \
      > "$ROOT/logs/clam.log" 2>&1 || die "clam build failed (see $ROOT/logs/clam.log)"
  "$dir/install/bin/clam" --help >/dev/null 2>&1 || die "clam: the clam binary does not run"
  "$dir/install/bin/clam-pp" --help >/dev/null 2>&1 || die "clam: clam-pp does not run"
  CLAM_DIR="$dir"
}

pin_line() { # NAME PATH -> "NAME sha256 bytes path-relative-to-ROOT"
  local name=$1 path=$2
  [ -f "$path" ] || die "pins: $path is missing"
  printf '%s %s %s %s\n' "$name" "$(sha256sum "$path" | cut -c1-64)" "$(stat -c %s "$path")" "${path#$ROOT/}"
}

write_pins() {
  {
    echo "# pins.txt -- written by engines.sh build on $(date -u +%Y-%m-%dT%H:%M:%SZ); the commit is the pin, the digest is what ran (1.6.0.md P-4)"
    echo "# llc $($LLVM20/bin/llc --version | grep -o 'LLVM version [0-9.]*')  |  llvm-as-18 $($LLVM18/bin/llvm-as --version | grep -o 'LLVM version [0-9.]*')"
    echo "commit nikos $NIKOS_SHA"
    echo "commit clam $CLAM_SHA"
    echo "commit crab $CRAB_SHA"
    echo "commit sea-dsa $SEADSA_SHA"
    echo "commit llvm-seahorn $LLVMSEA_SHA"
    echo "commit alive2 $ALIVE2_SHA"
    echo "commit z3 $Z3_SHA"
    for pf in "$ALIVE2_PATCH"; do
      echo "patch $(basename "$pf") $(sha256sum "$pf" | cut -c1-64) $(stat -c %s "$pf")"
    done
    pin_line z3/libz3.so "$(readlink -f "$Z3_PREFIX/lib/libz3.so")"   # the real file behind the soname links
    pin_line alive2/alive-tv "$ALIVE2_DIR/build/alive-tv"
    pin_line alive2/alive-exec "$ALIVE2_DIR/build/alive-exec"
    pin_line alive2/tv.so "$ALIVE2_DIR/build/tv/tv.so"
    for t in ikos-analyzer ikos-pp ikos-import; do pin_line "nikos/$t" "$NIKOS_DIR/install/bin/$t"; done
    for t in clam clam-pp seadsa seaopt; do pin_line "clam/$t" "$CLAM_DIR/install/bin/$t"; done
    pin_line clam/clam.py "$CLAM_DIR/install/bin/clam.py"
  } > "$PINS"
  say "wrote $PINS"
}

cmd=${1:-}
case "$cmd" in
  build)
    need git cmake ninja make clang clang++ sha256sum
    [ -d "$LLVM20/lib/cmake/llvm" ] || die "no LLVM 20 cmake package at $LLVM20 (llvm-20-dev)"
    [ -d "$LLVM18/lib/cmake/llvm" ] || die "no LLVM 18 cmake package at $LLVM18 (llvm-18-dev)"
    mkdir -p "$ROOT/logs"
    shift; which_engines="${*:-z3 alive2 nikos clam}"
    # the dirs every pin line reads, whether or not this invocation rebuilds the engine (a partial build re-pins ALL)
    Z3_PREFIX="$ROOT/z3-$(short $Z3_SHA)/install"; ALIVE2_DIR="$ROOT/alive2-$ALIVE2_SHORT"
    NIKOS_DIR="$ROOT/nikos-$(short $NIKOS_SHA)"; CLAM_DIR="$ROOT/clam-$(short $CLAM_SHA)"
    for e in $which_engines; do
      case "$e" in
        z3)     build_z3;     say "z3 library: $(readlink -f "$Z3_PREFIX/lib/libz3.so")";;
        alive2) build_alive2; say "alive2: $ALIVE2_DIR/build/alive-tv ($ALIVE2_SHA)";;
        nikos)  build_nikos;  say "nikos: $NIKOS_DIR/install/bin";;
        clam)   build_clam;   say "clam: $CLAM_DIR/install/bin";;
        *) die "unknown engine '$e' (z3 alive2 nikos clam)";;
      esac
    done
    write_pins
    ;;
  check)
    [ -f "$PINS" ] || die "no $PINS"
    rc=0
    while read -r name sha bytes rel; do
      case "$name" in \#*|commit|patch|"") continue;; esac
      if [ "$name" = patch ]; then continue; fi
      f="$ROOT/$rel"
      if [ ! -f "$f" ]; then echo "MISSING  $name ($rel)"; rc=1; continue; fi
      got=$(sha256sum "$f" | cut -c1-64)
      if [ "$got" != "$sha" ]; then echo "DIFFERS  $name: pinned $sha, found $got"; rc=1; else echo "ok       $name"; fi
    done < "$PINS"
    # every recorded patch: the digest pins.txt carries against the file beside this script, and a patch file
    # present here but absent from pins.txt is a patch outside the pin (D-321 (3))
    while read -r _ pname psha _; do
      f="$HERE/$pname"
      if [ ! -f "$f" ]; then echo "MISSING  patch $pname"; rc=1; continue; fi
      [ "$(sha256sum "$f" | cut -c1-64)" = "$psha" ] && echo "ok       patch $pname" || { echo "DIFFERS  patch $pname"; rc=1; }
    done < <(grep '^patch ' "$PINS")
    for pf in "$HERE"/*.patch; do
      [ -f "$pf" ] && ! grep -q "^patch $(basename "$pf") " "$PINS" && { echo "UNPINNED $(basename "$pf") (a patch outside the pin)"; rc=1; }
    done
    exit $rc
    ;;
  paths)
    echo "Z3_LIB=$ROOT/z3-$(short $Z3_SHA)/install/lib/libz3.so"
    echo "ALIVE_TV=$ROOT/alive2-$ALIVE2_SHORT/build/alive-tv"
    echo "NIKOS_BIN=$ROOT/nikos-$(short $NIKOS_SHA)/install/bin"
    echo "CLAM_BIN=$ROOT/clam-$(short $CLAM_SHA)/install/bin"
    echo "LLVM18_BIN=$LLVM18/bin"
    echo "LLVM20_BIN=$LLVM20/bin"
    ;;
  *) echo "usage: engines.sh build [z3|alive2|nikos|clam ...]|check|paths" >&2; exit 2;;
esac
