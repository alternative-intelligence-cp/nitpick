#!/bin/bash
# hashcmp.sh ROOT OUT SEEDS -- D-303's MEASUREMENT (1.5.7 step 1): every `// stress:` program without argv/fixture,
# built against the explored floor with the C reference shim (`npkx.c`, clang) and with the IR shim
# (`runtime/explore/npkx.ll`, the pinned llc); per seed the two (exit, steps, hash, verdict) must agree. Run by
# hand, outside every gate; OUT must already hold `npkrt.explore.o` (the transformer's output assembled),
# `npkx_c.o` (the C shim) and `npkx.o` (the IR shim). Programs that need virtual signals (step 2) or spawn a
# real child (`explore: no`) differ by design until then. Measured at step 1 under a twelve-process CPU load:
# 30 of 30 signal-free programs agree on all 20 seeds.
set -u
ROOT=$1; OUT=$2; SEEDS=$3
cd "$ROOT" || exit 2
LLC=$(python3 -c 'import sys; sys.path.insert(0,"bootstrap/harness"); import harness; print(" ".join(harness.LLC_FLAGS))')
LLD=$(python3 -c 'import sys; sys.path.insert(0,"bootstrap/harness"); import harness; print(" ".join(harness.LLD_FLAGS))')
ulimit -n 1024
agree=0; disagree=0; skipped=0
for f in $(grep -l '^// stress:' tests/backend/programs/*.npk | sort); do
  name=$(basename "$f" .npk)
  if grep -q '^// argv:\|^// fixture' "$f"; then skipped=$((skipped+1)); continue; fi
  build/npkc "$f" > "$OUT/$name.ll" 2> "$OUT/$name.err" || { echo "$name SKIP(compile)"; continue; }
  llc $LLC "$OUT/$name.ll" -o "$OUT/$name.o" 2>> "$OUT/$name.err" || { echo "$name SKIP(llc)"; continue; }
  ld.lld $LLD -o "$OUT/$name.c" "$OUT/$name.o" "$OUT/npkrt.explore.o" "$OUT/npkx_c.o" 2>> "$OUT/$name.err" || { echo "$name SKIP(link c)"; continue; }
  ld.lld $LLD -o "$OUT/$name.ir" "$OUT/$name.o" "$OUT/npkrt.explore.o" "$OUT/npkx.o" 2>> "$OUT/$name.err" || { echo "$name SKIP(link ir)"; continue; }
  same=0; diff=0; first=""
  for s in $(seq 1 "$SEEDS"); do
    NPKX_SEED=$s NPKX_TRACE=1 timeout 30 "$OUT/$name.c" < /dev/null > /dev/null 2> "$OUT/$name.tc"; ec=$?
    NPKX_SEED=$s NPKX_TRACE=1 timeout 30 "$OUT/$name.ir" < /dev/null > /dev/null 2> "$OUT/$name.ti"; ei=$?
    hc=$(grep -o 'steps=[0-9]* hash=[0-9]*' "$OUT/$name.tc" | head -1); hi=$(grep -o 'steps=[0-9]* hash=[0-9]*' "$OUT/$name.ti" | head -1)
    vc=$(grep -oE 'npkx: (DEADLOCK|STEP BUDGET|LOST[A-Z -]*|ASSUMPTION)' "$OUT/$name.tc" | head -1); vi=$(grep -oE 'npkx: (DEADLOCK|STEP BUDGET|LOST[A-Z -]*|ASSUMPTION)' "$OUT/$name.ti" | head -1)
    if [ "$ec:$hc:$vc" = "$ei:$hi:$vi" ]; then same=$((same+1)); else diff=$((diff+1)); [ -z "$first" ] && first="seed $s: c=$ec:$hc:$vc ir=$ei:$hi:$vi"; fi
  done
  if [ "$diff" = 0 ]; then agree=$((agree+1)); else disagree=$((disagree+1)); fi
  printf "%-28s same=%-4s diff=%-3s %s\n" "$name" "$same" "$diff" "$first"
done
echo "programs agreeing on every seed: $agree; disagreeing: $disagree; skipped (argv/fixture): $skipped"
