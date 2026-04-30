#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
K_DIR="$ROOT_DIR/k-semantics"
DEF_DIR="$K_DIR/.build/aria-proofs-kompiled"
PROOF_DIR="$K_DIR/proofs"
REQUIRE_K=false

if [[ "${1:-}" == "--require-k" ]]; then
    REQUIRE_K=true
fi

skip_missing_k() {
    local tool="$1"
    echo "[k-proofs] SKIP: '$tool' not found in PATH."
    echo "          See k-semantics/INSTALL.md."
    if [[ "$REQUIRE_K" == true ]]; then
        exit 1
    fi
    exit 77
}

for tool in kompile kprove; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        skip_missing_k "$tool"
    fi
done

shopt -s nullglob
proof_files=("$PROOF_DIR"/*.k)
if (( ${#proof_files[@]} == 0 )); then
    echo "[k-proofs] SKIP: no proof files found in $PROOF_DIR"
    exit 77
fi

echo "[k-proofs] kompile aria.k for kprove"
rm -rf "$DEF_DIR"
kompile \
    --backend haskell \
    --main-module ARIA \
    --syntax-module ARIA-SYNTAX \
    --output-definition "$DEF_DIR" \
    "$K_DIR/aria.k"

pass=0
fail=0

for proof_file in "${proof_files[@]}"; do
    module=$(grep -E '^// proof-module: ' "$proof_file" | head -n1 | sed 's#^// proof-module: ##' || true)
    name=$(basename "$proof_file")

    if [[ -z "$module" ]]; then
        echo "[k-proofs] FAIL $name (missing // proof-module: MODULE header)"
        fail=$((fail + 1))
        continue
    fi

    if output=$(kprove "$proof_file" --definition "$DEF_DIR" --spec-module "$module" 2>&1); then
        if grep -q '#Top' <<<"$output"; then
            echo "[k-proofs] PASS $name ($module)"
            pass=$((pass + 1))
        else
            echo "[k-proofs] FAIL $name ($module)"
            echo "$output"
            fail=$((fail + 1))
        fi
    else
        echo "[k-proofs] FAIL $name ($module)"
        echo "$output"
        fail=$((fail + 1))
    fi
done

echo "[k-proofs] passed=$pass failed=$fail"

if (( fail > 0 )); then
    exit 1
fi
