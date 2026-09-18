#!/bin/bash
# PROTOTYPE (1.5.7 planning; kept by D-303 as the IR shim's behavioural reference, outside every gate -- see
# README.md): every `// stress:` program under the explorer, SEEDS seeds each.
# usage: sweep.sh ROOT OUTDIR SEEDS
set -u
ROOT=$1; OUT=$2; SEEDS=$3
HERE=$(dirname "$(readlink -f "$0")")
mkdir -p "$OUT"
python3 "$HERE/transform.py" "$ROOT/runtime/npkrt.ll" "$OUT/npkrt.explore.ll" "$OUT/sites.txt" > /dev/null
llc -O0 -filetype=obj -relocation-model=static "$OUT/npkrt.explore.ll" -o "$OUT/npkrt.explore.o"
clang -O1 -ffreestanding -fno-builtin -fno-stack-protector -fno-pic -c "$HERE/npkx.c" -o "$OUT/npkx.o"
ulimit -n 1024
for f in $(grep -l '^// stress:' "$ROOT"/tests/backend/programs/*.npk | sort); do
  name=$(basename "$f" .npk)
  want=$(grep -m1 '^// expect-exit:' "$f" | awk '{print $3}'); want=${want:-0}
  if grep -q '^// argv:\|^// fixture' "$f"; then echo "$name SKIP(fixture/argv)"; continue; fi
  if ! ( cd "$ROOT" && build/npkc "tests/backend/programs/$name.npk" > "$OUT/$name.ll" 2> "$OUT/$name.err" ); then echo "$name SKIP(compile)"; continue; fi
  llc -O0 -filetype=obj -relocation-model=static "$OUT/$name.ll" -o "$OUT/$name.o" 2>> "$OUT/$name.err" || { echo "$name SKIP(llc)"; continue; }
  ld.lld -static -o "$OUT/$name.x" "$OUT/$name.o" "$OUT/npkrt.explore.o" "$OUT/npkx.o" 2>> "$OUT/$name.err" || { echo "$name SKIP(link)"; continue; }
  ok=0; bad=0; firstbad=""; codes=""
  s0=$(date +%s.%N)
  for s in $(seq 1 "$SEEDS"); do
    NPKX_SEED=$s NPKX_TRACE=1 timeout 20 "$OUT/$name.x" > /dev/null 2> "$OUT/$name.trace"
    e=$?
    if [ "$e" = "$want" ]; then ok=$((ok+1)); else bad=$((bad+1)); [ -z "$firstbad" ] && { firstbad="$s:$e"; cp "$OUT/$name.trace" "$OUT/$name.firstbad"; }; fi
  done
  s1=$(date +%s.%N)
  steps=$(grep -o 'steps=[0-9]*' "$OUT/$name.trace" | head -1)
  printf "%-28s want=%-3s ok=%-3s bad=%-3s first=%-8s %s %.1fs\n" "$name" "$want" "$ok" "$bad" "${firstbad:--}" "${steps:-steps=?}" "$(echo "$s1 - $s0" | bc)"
done
