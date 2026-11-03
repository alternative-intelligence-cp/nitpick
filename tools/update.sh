#!/bin/sh
# update.sh -- bring a Nitpick checkout and its installed compiler up to date.
#
#   update.sh             fetch; if there's a newer commit, fast-forward, build, test, install
#   update.sh --check     fetch and say whether there's a newer commit; change nothing
#   update.sh --rebuild   build and install what's checked out, uncommitted changes included; no git
#
#   --repo DIR     the checkout (default: the one this script is in, else the current directory)
#   --prefix DIR   where to install (default: ~/.local, as INSTALL.md section 4)
#
# It runs INSTALL.md's commands, sections 3 and 4. It never merges, rebases, stashes or discards: if
# your tree has changes, or your branch has gone its own way, it says so and stops. The installed
# compiler is replaced only after the new one has built INSTALL.md's first program and that program
# has run. The pair it replaces is kept in PREFIX/lib/nitpick/previous/.
#
# Exit: 0 done, or up to date; 1 a step failed; 2 refused, nothing changed; 3 --check found a newer commit.
set -eu

say()  { printf 'update: %s\n' "$*"; }
stop() { printf 'update: %s\n' "$*" >&2; exit 2; }

mode=update
repo=
prefix=${HOME:?HOME is not set}/.local
while [ $# -gt 0 ]; do
    case $1 in
        --check)   mode=check ;;
        --rebuild) mode=rebuild ;;
        --repo)    [ $# -ge 2 ] || stop "--repo needs a directory"; repo=$2; shift ;;
        --prefix)  [ $# -ge 2 ] || stop "--prefix needs a directory"; prefix=$2; shift ;;
        -h|--help) sed -n '2,/^set -eu/p' "$0" | sed -e '/^set -eu/d' -e 's/^# \{0,1\}//'; exit 0 ;;
        *)         stop "unknown argument: $1 (--help lists them)" ;;
    esac
    shift
done

# --- the checkout -------------------------------------------------------------------------------
is_nitpick() { [ -f "$1/src/npkc.npk" ] && [ -f "$1/bootstrap/seed/stage1.ll" ]; }
if [ -z "$repo" ]; then
    repo=$(git -C "$(dirname "$0")" rev-parse --show-toplevel 2>/dev/null) || repo=
    if [ -z "$repo" ] || ! is_nitpick "$repo"; then
        repo=$(git rev-parse --show-toplevel 2>/dev/null) || repo=
    fi
fi
if [ -z "$repo" ] || ! is_nitpick "$repo"; then
    stop "no Nitpick checkout here: run this inside one, or name it with --repo DIR"
fi
cd "$repo"
root=$(pwd -P)

# --- the tools: plain names, or the versioned ones INSTALL.md section 1 says give the same bytes ---
find_tool() {
    command -v "$1" 2>/dev/null || command -v "$1-20" 2>/dev/null \
        || stop "$1 isn't on your PATH, nor $1-20: see INSTALL.md section 1"
}
LLC=$(find_tool llc)
LLD=$(find_tool ld.lld)
if ! "$LLC" --version | grep -q 'LLVM version 20\.1\.'; then
    stop "$LLC isn't an LLVM 20.1 release, which the build needs: see INSTALL.md section 1"
fi

# --- what's installed ----------------------------------------------------------------------------
bin=$prefix/bin
lib=$prefix/lib/nitpick
record=$lib/installed-commit
installed=
[ -f "$record" ] && installed=$(cat "$record")
short() {
    case $1 in
        *-dirty) printf '%s-dirty' "$(printf '%s' "$1" | cut -c1-8)" ;;
        *)       printf '%s' "$1" | cut -c1-8 ;;
    esac
}

# --- on any failure after this point, say what state things are in --------------------------------
phase=
moved_from=
installing=
kept=
tmp=
on_exit() {
    rc=$?
    [ -n "$tmp" ] && rm -rf "$tmp"
    if [ "$rc" -eq 1 ]; then
        printf 'update: FAILED while %s.\n' "$phase" >&2
        if [ -n "$installing" ]; then
            printf 'update: the install may be incomplete.\n' >&2
            if [ -n "$kept" ]; then
                printf 'update: the pair it was replacing is in %s/previous.\n' "$lib" >&2
            fi
        else
            printf 'update: your installed compiler is untouched.\n' >&2
        fi
        if [ -n "$moved_from" ]; then
            printf 'update: the checkout did move: git reset --keep %s puts it back.\n' "$(short "$moved_from")" >&2
        fi
    fi
    exit "$rc"
}
trap on_exit EXIT

# --- git ------------------------------------------------------------------------------------------
if [ "$mode" != rebuild ]; then
    branch=$(git symbolic-ref --quiet --short HEAD) \
        || stop "not on a branch (a detached HEAD): check one out, or use --rebuild"
    git rev-parse --quiet --verify '@{upstream}' >/dev/null \
        || stop "branch $branch follows no remote branch: set one, or use --rebuild"
    phase="fetching from the remote"
    git fetch --quiet || exit 1
    phase=
    head=$(git rev-parse HEAD)
    behind=$(git rev-list --count 'HEAD..@{upstream}')
    ahead=$(git rev-list --count '@{upstream}..HEAD')
    if [ "$behind" -gt 0 ]; then
        say "$behind newer commit(s) on $(git rev-parse --abbrev-ref '@{upstream}'):"
        git log --max-count=20 --format='  %h %s' 'HEAD..@{upstream}' | cut -c1-100
        [ "$behind" -le 20 ] || say "  ... and $((behind - 20)) more"
    fi
    if [ "$mode" = check ]; then
        if [ "$behind" -gt 0 ]; then
            exit 3
        fi
        say "the checkout is up to date at $(short "$head")"
        if [ "$installed" != "$head" ]; then
            say "the installed compiler is from ${installed:-an unrecorded build}, so update.sh would rebuild it"
        fi
        exit 0
    fi
    dirty=$(git status --porcelain --untracked-files=no)
    if [ "$ahead" -gt 0 ] && [ "$behind" -gt 0 ]; then
        stop "your branch and the remote have each moved ($ahead of yours, $behind of theirs): merge or rebase it yourself, then run this again; nothing changed"
    fi
    if [ -n "$dirty" ]; then
        if [ "$behind" -gt 0 ] || [ "$installed" != "$head" ]; then
            stop "you have uncommitted changes to tracked files: commit or stash them and run this again, or use --rebuild to build them as they are; nothing changed"
        fi
    fi
    if [ "$behind" -eq 0 ] && [ "$installed" = "$head" ]; then
        say "up to date: the checkout and the installed compiler are both at $(short "$head")"
        exit 0
    fi
    if [ "$behind" -gt 0 ]; then
        phase="fast-forwarding the checkout"
        git merge --quiet --ff-only '@{upstream}' || exit 1
        moved_from=$head
    elif [ -z "$installed" ]; then
        say "the checkout is at $(short "$head"), and there's no record of which commit is installed, so building once"
    else
        say "the checkout is at $(short "$head"), and the installed compiler is from $(short "$installed")"
    fi
fi
commit=$(git rev-parse HEAD)
if [ -n "$(git status --porcelain --untracked-files=no)" ]; then
    built=$commit-dirty
else
    built=$commit
fi

# --- build: INSTALL.md section 3, command for command ---------------------------------------------
say "building $(short "$built") (about a minute)"
phase="creating build/"
mkdir -p build || exit 1
phase="building the runtime"
"$LLC" -O0 -filetype=obj -relocation-model=static runtime/npkrt.ll -o build/npkrt.o || exit 1
phase="building the builder from the seed"
"$LLC" -O0 -filetype=obj -relocation-model=static bootstrap/seed/stage1.ll -o build/builder.o || exit 1
"$LLD" -static -o build/builder build/builder.o build/npkrt.o || exit 1
phase="compiling the compiler"
./build/builder src/npkc.npk -o build/npkc.ll || exit 1
"$LLC" -O0 -filetype=obj -relocation-model=static build/npkc.ll -o build/npkc.o || exit 1
"$LLD" -static -o build/npkc build/npkc.o build/npkrt.o || exit 1

# --- test: the new compiler builds INSTALL.md's first program, and that program runs ---------------
phase="testing the new compiler on INSTALL.md's first program"
tmp=$(mktemp -d) || exit 1
awk '/^## 5\./ { s = 1 } s && /^```/ { if (++n == 2) exit; next } n == 1' INSTALL.md > "$tmp/hello.npk" || exit 1
[ -s "$tmp/hello.npk" ] || { printf 'update: no first program in INSTALL.md section 5\n' >&2; exit 1; }
(
    cd "$tmp" \
    && "$root/build/npkc" hello.npk -o hello.ll \
    && "$LLC" -O0 -filetype=obj -relocation-model=static hello.ll -o hello.o \
    && "$LLD" -static -o hello hello.o "$root/build/npkrt.o" \
    && ./hello > hello.out
) || exit 1
[ -s "$tmp/hello.out" ] || { printf 'update: the first program printed nothing\n' >&2; exit 1; }
say "tested: the first program printed: $(head -n 1 "$tmp/hello.out")"

# --- install: INSTALL.md section 4, keeping the pair it replaces -----------------------------------
phase="keeping the pair it replaces"
if [ -f "$bin/npkc" ]; then
    mkdir -p "$lib/previous" || exit 1
    cp -f "$bin/npkc" "$lib/previous/npkc" || exit 1
    if [ -f "$lib/npkrt.o" ]; then cp -f "$lib/npkrt.o" "$lib/previous/npkrt.o" || exit 1; fi
    printf '%s\n' "${installed:-unrecorded}" > "$lib/previous/installed-commit" || exit 1
    kept=yes
fi
phase="installing"
installing=yes
install -D -m 755 build/npkc "$bin/npkc" || exit 1
install -D -m 644 build/npkrt.o "$lib/npkrt.o" || exit 1
printf '%s\n' "$built" > "$record" || exit 1

if [ -n "$installed" ]; then was=$(short "$installed"); else was="an unrecorded build"; fi
if [ -n "$kept" ]; then
    say "installed $(short "$built") to $bin/npkc (it was $was, kept in $lib/previous/)"
else
    say "installed $(short "$built") to $bin/npkc"
fi
found=$(command -v npkc 2>/dev/null) || found=
if [ -z "$found" ]; then
    say "note: npkc isn't on your PATH: INSTALL.md section 1 shows the line to add"
elif [ "$found" != "$bin/npkc" ]; then
    say "note: your PATH finds another npkc first, at $found"
fi
if [ -f build/npkg ]; then
    say "note: build/npkg is still the old one: INSTALL.md section 6 rebuilds it"
fi
exit 0
