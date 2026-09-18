#!/usr/bin/env python3
"""PROTOTYPE (1.5.7 planning; outside every gate -- kept in the tree by D-303 as the IR shim's behavioural
reference, see README.md; SUPERSEDED as a transformer by `npkg/explore.npk`, the one transformer both runners
hold to a literal): the floor with a scheduling point before every synchronization step. Text in, text out.

  - every `@npk_sys6(` CALL site calls `@npkx_sys6(` instead (same signature; the shim does the point and
    virtualizes the blocking numbers)
  - every atomic step line (`atomicrmw`, `cmpxchg`, `fence`, `load atomic`, `store atomic`) is preceded by
    `call void @npkx_point(i32 SITE)`
  - thread lifecycle: `@npkx_prespawn()` before and `@npkx_spawned(i64 tid)` after the clone call;
    `@npkx_begin()` first in `@npk_thread_entry`; `@npkx_end()` before its `ret`
  - `@npkx_trap()` first in `@npk_trap`: from there on the shim passes everything through

usage: transform.py IN.ll OUT.ll [SITES.txt]
"""
import re
import sys

src, dst = sys.argv[1], sys.argv[2]
sites_path = sys.argv[3] if len(sys.argv) > 3 else None
STEP = re.compile(r'\b(atomicrmw|cmpxchg|fence)\b|load atomic |store atomic ')


def code_part(line):
    out, in_str = [], False
    for ch in line:
        if ch == '"':
            in_str = not in_str
        if ch == ';' and not in_str:
            break
        out.append(ch)
    return "".join(out)


lines = open(src).read().split("\n")
out = []
sites = []
fn = None
pending_spawn = None      # the register the clone call defines, while its statement is still open
i = 0
while i < len(lines):
    line = lines[i]
    code = code_part(line)
    m = re.match(r'^define\b.*?(@[\w.$-]+)\s*\(', code)
    if m:
        fn = m.group(1)
        out.append(line)
        i += 1
        continue
    if fn is None or code.startswith("module asm"):
        out.append(line)
        i += 1
        continue
    if code.strip() == "}":
        fn = None
        out.append(line)
        i += 1
        continue
    # entry hooks
    if code.strip() == "entry:" and fn == "@npk_thread_entry":
        out.append(line)
        out.append("  call void @npkx_begin()")
        i += 1
        continue
    if code.strip() == "entry:" and fn == "@npk_small_free":
        out.append(line)
        out.append("  call void @npkx_chk_small_free(i64 %ip, ptr @npk_cls_part)")
        i += 1
        continue
    if code.strip() == "entry:" and fn == "@npk_rq_push":
        out.append(line)
        out.append("  call void @npkx_chk_rq_push(ptr %f)")
        i += 1
        continue
    if code.strip() == "entry:" and fn == "@npk_trap":
        out.append(line)
        out.append("  call void @npkx_trap()")
        i += 1
        continue
    if fn == "@npk_thread_entry" and re.match(r'^\s*ret\b', code):
        out.append("  call void @npkx_end()")
        out.append(line)
        i += 1
        continue
    if fn == "@npk_sys6":
        out.append(line)
        i += 1
        continue
    if "@npk_clone_raw(" in code and "call" in code:
        reg = re.match(r'^\s*(%[\w.]+)\s*=', code).group(1)
        out.append("  call void @npkx_prespawn()")
        # the statement may continue on following lines: copy through the line that closes it
        while True:
            out.append(lines[i])
            if code_part(lines[i]).rstrip().endswith(")"):
                break
            i += 1
        out.append("  call void @npkx_spawned(i64 %s)" % reg)
        i += 1
        continue
    if "@npk_sys6(" in code and "call" in code:
        sites.append((len(sites), fn, "sys6", code.strip()[:80]))
        out.append(line.replace("@npk_sys6(", "@npkx_sys6(", 1))
        i += 1
        continue
    if STEP.search(code):
        n = len(sites)
        sites.append((n, fn, "atomic", code.strip()[:80]))
        out.append("  call void @npkx_point(i32 %d)" % n)
        out.append(line)
        i += 1
        continue
    out.append(line)
    i += 1

out.append("")
out.append("; --- the explorer's shim (PROTOTYPE) ---")
for d in ("declare void @npkx_point(i32)", "declare i64 @npkx_sys6(i64, i64, i64, i64, i64, i64, i64)",
          "declare void @npkx_prespawn()", "declare void @npkx_spawned(i64)", "declare void @npkx_begin()",
          "declare void @npkx_end()", "declare void @npkx_trap()", "declare void @npkx_chk_small_free(i64, ptr)",
          "declare void @npkx_chk_rq_push(ptr)"):
    out.append(d)
open(dst, "w").write("\n".join(out) + "\n")
if sites_path:
    open(sites_path, "w").write("".join("%d\t%s\t%s\t%s\n" % s for s in sites))
print("sites: %d (%d atomic, %d sys6)" % (len(sites), sum(1 for s in sites if s[2] == "atomic"), sum(1 for s in sites if s[2] == "sys6")))
