#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
K_DIR="$ROOT_DIR/k-semantics"
DEF_DIR="$K_DIR/.build/aria-kompiled"
TEST_DIR="$K_DIR/tests/core"

require_k=false
if [[ "${1:-}" == "--require-k" ]]; then
    require_k=true
fi

if ! command -v kompile >/dev/null 2>&1 || ! command -v krun >/dev/null 2>&1; then
    echo "SKIP: K Framework tools not found (need kompile and krun)."
    echo "      See k-semantics/INSTALL.md."
    if [[ "$require_k" == true ]]; then
        exit 1
    fi
    exit 77
fi

mkdir -p "$K_DIR/.build"

echo "[k-semantics] kompile aria.k"
kompile \
    --main-module ARIA \
    --syntax-module ARIA-SYNTAX \
    --output-definition "$DEF_DIR" \
    "$K_DIR/aria.k"

pass=0
fail=0

for test_file in "$TEST_DIR"/*.aria; do
    name="$(basename "$test_file")"
    expected="$(sed -n 's|^// expect-exit:[[:space:]]*||p' "$test_file" | head -1)"
    expected_stdout="$(sed -n 's|^// expect-stdout:[[:space:]]*||p' "$test_file" | head -1)"
    if [[ -z "$expected" ]]; then
        echo "FAIL $name: missing // expect-exit: N header"
        fail=$((fail + 1))
        continue
    fi

    output="$(krun "$test_file" --definition "$DEF_DIR" 2>&1 || true)"
    actual="$(awk '
        /<exit-code>/ {
            if ($0 ~ /<exit-code>[[:space:]]*-?[0-9]+[[:space:]]*<\/exit-code>/) {
                gsub(/.*<exit-code>[[:space:]]*|[[:space:]]*<\/exit-code>.*/, "")
                print
                exit
            }
            getline
            gsub(/[[:space:]]/, "")
            print
            exit
        }
    ' <<< "$output")"
    actual_stdout=""
    if [[ -n "$expected_stdout" ]]; then
        actual_stdout="$(awk '
            /<stdout>/ {
                if ($0 ~ /<stdout>[[:space:]]*.*[[:space:]]*<\/stdout>/) {
                    gsub(/.*<stdout>[[:space:]]*|[[:space:]]*<\/stdout>.*/, "")
                    print
                    exit
                }
                getline
                gsub(/^[[:space:]]*|[[:space:]]*$/, "")
                print
                exit
            }
        ' <<< "$output")"
    fi

    if [[ "$actual" == "$expected" && ( -z "$expected_stdout" || "$actual_stdout" == "$expected_stdout" ) ]]; then
        echo "PASS $name"
        pass=$((pass + 1))
    else
        echo "FAIL $name: expected exit-code $expected, got ${actual:-<missing>}"
        if [[ -n "$expected_stdout" ]]; then
            echo "           expected stdout $expected_stdout, got ${actual_stdout:-<missing>}"
        fi
        echo "$output" | sed -n '1,80p'
        fail=$((fail + 1))
    fi
done

echo "[k-semantics] passed=$pass failed=$fail"
if [[ "$fail" -ne 0 ]]; then
    exit 1
fi