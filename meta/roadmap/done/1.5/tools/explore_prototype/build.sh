#!/bin/bash
# PROTOTYPE (1.5.7 planning; kept by D-303 as the IR shim's behavioural reference, outside every gate -- see
# README.md): build one program against the transformed floor + the shim, with the system C compiler.
# usage: build.sh ROOT PROGRAM.npk OUTDIR [FLOOR.ll]     (FLOOR.ll defaults to ROOT/runtime/npkrt.ll)
set -eu
ROOT=$1; PROG=$2; OUT=$3; FLOOR=${4:-$ROOT/runtime/npkrt.ll}
HERE=$(dirname "$(readlink -f "$0")")
mkdir -p "$OUT"
NAME=$(basename "$PROG" .npk)
python3 "$HERE/transform.py" "$FLOOR" "$OUT/npkrt.explore.ll" "$OUT/sites.txt"
llc -O0 -filetype=obj -relocation-model=static "$OUT/npkrt.explore.ll" -o "$OUT/npkrt.explore.o"
clang -O1 -ffreestanding -fno-builtin -fno-stack-protector -fno-pic -c "$HERE/npkx.c" -o "$OUT/npkx.o"
( cd "$ROOT" && build/npkc "$PROG" > "$OUT/$NAME.ll" )
llc -O0 -filetype=obj -relocation-model=static "$OUT/$NAME.ll" -o "$OUT/$NAME.o"
ld.lld -static -o "$OUT/$NAME.x" "$OUT/$NAME.o" "$OUT/npkrt.explore.o" "$OUT/npkx.o"
# the plain build beside it, for the expected exit
llc -O0 -filetype=obj -relocation-model=static "$FLOOR" -o "$OUT/npkrt.plain.o"
ld.lld -static -o "$OUT/$NAME.plain" "$OUT/$NAME.o" "$OUT/npkrt.plain.o"
echo "built $OUT/$NAME.x"
