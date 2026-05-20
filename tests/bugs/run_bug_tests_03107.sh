#!/usr/bin/env bash
# v0.31.0.7 Phase 1 async — D-9: user-facing async file I/O surface.
#
# Three fixtures verify the npk_*_async file API exposed by the runtime,
# now callable from .npk via `extern "libc"` bindings, using the new
# npk_future_is_error() accessor added this slice to disambiguate
# successful completion from error completion (both leave is_ready==true).
#
#   bug354 — async write: submit, spin-poll, observe success + on-disk effect.
#   bug355 — async read of missing file: error must surface via is_error.
#   bug356 — full roundtrip: write → read-back → byte-compare → delete.
#
# Polling here is a busy-spin (bounded by ~100k iterations); no real
# language-level await over runtime futures exists yet (the LLVM
# coroutine lowering only awaits other compiled async fns). Bridging
# user-level `await` to AriaFutureHandle is deferred to a later slice.

set -u

NPKC="${NPKC:-./build/npkc}"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

if [[ ! -x "$NPKC" ]]; then
    echo "SKIP: $NPKC not built"
    exit 77
fi

PASSED=0
FAILED=0

expect_run_lines_in_order() {
    local label="$1" src="$2"; shift 2
    local bin="$TMP/$(basename "$src" .npk)"
    local cerr crc out rrc
    cerr="$("$NPKC" "$src" -o "$bin" 2>&1)"; crc=$?
    if [[ $crc -ne 0 ]]; then
        echo "  FAIL: $label — compile rc=$crc"
        echo "----- compiler output -----"; echo "$cerr"; echo "---------------------------"
        FAILED=$((FAILED+1)); return
    fi
    out="$("$bin" 2>&1)"; rrc=$?
    if [[ $rrc -ne 0 ]]; then
        echo "  FAIL: $label — run rc=$rrc"
        echo "----- run output -----"; echo "$out"; echo "----------------------"
        FAILED=$((FAILED+1)); return
    fi
    local last_idx=0
    for needle in "$@"; do
        local idx
        idx=$(awk -v n="$needle" -v after="$last_idx" '
            NR > after && index($0, n) > 0 { print NR; exit }
        ' <<<"$out")
        if [[ -z "$idx" ]]; then
            echo "  FAIL: $label — expected line containing '$needle' after index $last_idx, not found"
            echo "----- run output -----"; echo "$out"; echo "----------------------"
            FAILED=$((FAILED+1)); return
        fi
        last_idx=$idx
    done
    echo "  PASS: $label"
    PASSED=$((PASSED+1))
}

echo "== v0.31.0.7 Phase 1 async D-9 (async file I/O surface) =="

expect_run_lines_in_order "bug354 async write completes successfully" \
    tests/bugs/bug354_async_file_write_success_pass.npk \
    "WRITE-READY" "WRITE-OK" "EXISTS-OK" "END"

expect_run_lines_in_order "bug355 async read of missing file surfaces error" \
    tests/bugs/bug355_async_file_read_missing_pass.npk \
    "READ-READY" "READ-ERROR" "END"

expect_run_lines_in_order "bug356 async write/read roundtrip + delete" \
    tests/bugs/bug356_async_file_roundtrip_pass.npk \
    "WRITE-OK" "READ-OK" "CONTENT-MATCH" "DELETE-OK" "END"

echo "== Results: $PASSED/$((PASSED+FAILED)) passed, $FAILED failed =="

[[ $FAILED -eq 0 ]]
