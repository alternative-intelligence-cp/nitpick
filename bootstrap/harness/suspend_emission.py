#!/usr/bin/env python3
"""Does the emission keep every value a resume reads, and is every back edge over a park known?

Run from the repository root:  python3 bootstrap/harness/suspend_emission.py NPKC [FILE...]
(the harness names the compiler in NPK_SELFCHECK_COMPILER and takes the default files)

The other half of suspend_walk.py: that one reads the walk's source, this one reads
what it produced. A coroutine's body is emitted as `npk.resume.*`, entered afresh
at each resume, so an alloca there is empty on every entry but the first segment's.

  READS WITH NO WRITE. Per alloca, a worklist over the CFG from the entry switch's
  targets other than `seg0` and `coro.bad`. A write is a `store` to it or a GEP of
  it, or a mem* destination. A read is a `load`, or any other call handed it; a
  call is read AND written, so it only errs toward more reports, and the runtime's
  own out-parameters (OUT_PARAM) are listed as writes. An alloca the entry block
  itself stores to is defined on every entry, so it can't be read dead: a
  temporary's drop flag, zeroed there. And so a temporary's slot, dropped only
  under that flag (`tmp.drop*`), was written in this activation. A hit is a value
  the walk failed to frame: read off the IR, no stack contents involved, so it
  doesn't depend on what a frame's neighbours happened to write (DEF-494's clause
  cells, merged, read right at run time; this finds each of them).

  BACK EDGES OVER A PARK. A branch to an earlier label with a park (`ret i8 1`)
  between is a loop the walk owes a `loop_push`, or a runtime retry whose operand
  is framed. Counted by label stem, each of KNOWN accounted for. A new stem fails
  until someone says who pays for it: `pk2.arm`, a backward `fall`, was how DEF-487
  got here.

ITS REACH: the function's own reads. A pointer held elsewhere, a callee's read of
this frame, and a slot read through a chain of calls are not seen. Nor is a flag
set before a park and read after it: the entry block has zeroed it, so what's lost
is the drop, a leak with no wrong read (DEF-497's `b5`). Programs that don't compile
are the suites' to report, not this one's. compiler_69's cross494.py and
backedge494.py, at DEF-494.
"""

import concurrent.futures as cf
import glob
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
LABEL = re.compile(r"^([A-Za-z0-9_.$-]+):")
TGT = re.compile(r"label %([A-Za-z0-9_.$-]+)")
ALLOCA = re.compile(r"^\s+(%[A-Za-z0-9_.]+) = alloca (.*)$")
GEP = re.compile(r"^\s+(%[A-Za-z0-9_.]+) = getelementptr [^,]*(?:\{.*?\})?.*?, ptr (%[A-Za-z0-9_.]+)")
LOAD = re.compile(r"= load [^%]*?(?:\{.*\} )?.*?, ptr (%[A-Za-z0-9_.]+)")
STORE = re.compile(r"^\s+store .*, ptr (%[A-Za-z0-9_.]+)(?:, align \d+)?\s*$")
MEM = re.compile(r"call void @llvm\.mem(?:cpy|set|move)[^(]*\(ptr (%[A-Za-z0-9_.]+)")
CALLPTR = re.compile(r"ptr (%[A-Za-z0-9_.]+)")

# A runtime call whose `ptr` argument is only written: the channel's drain.
OUT_PARAM = re.compile(r"call i32 @npk_ch_try_recv\(i64 %[A-Za-z0-9_.]+, ptr (%[A-Za-z0-9_.]+)\)")

# Each stem that repeats across a park, and who pays for it.
KNOWN = {
    "wh.head": "`while`: its arm's loop_push",
    "wn.head": "`when`: its arm's loop_push",
    "fr.head": "`for` over a range: its arm's loop_push",
    "fe.head": "`for` over an array or slice: its arm's loop_push",
    "ct.head": "`loop` and `till`: suspend_counted's loop_push",
    "ch.try": "a channel's retry: its handle and deadline are frame slots, computed once before it",
    "mx.try": "a lock's retry: its receiver's address, the address rule",
    "cv.step": "a condvar's steps: its receiver's address, the address rule",
    "ba.poll": "a barrier's poll: its receiver's address, the address rule",
}


def resume_bodies(ll):
    lines = ll.split("\n")
    i = 0
    while i < len(lines):
        line = lines[i]
        if not (line.startswith("define") and '"npk.resume.' in line):
            i += 1
            continue
        fn = re.search(r'"npk\.resume\.([^"]+)"', line).group(1)
        body = []
        i += 1
        while not lines[i].startswith("}"):
            body.append(lines[i])
            i += 1
        i += 1
        yield fn, body


def back_edges(fn, body):
    pos, parks = {}, []
    for j, line in enumerate(body):
        m = LABEL.match(line)
        if m:
            pos[m.group(1)] = j
        if line.strip() == "ret i8 1":
            parks.append(j)
    stems = set()
    for j, line in enumerate(body):
        t = line.strip()
        if t.startswith("br ") or t.startswith("switch "):
            for x in TGT.findall(line):
                if x in pos and pos[x] < j and any(pos[x] < p < j for p in parks):
                    stems.add(re.sub(r"\d+$", "", x))
    return stems


def stale_reads(fn, body):
    blocks, cur, order = {"entry": []}, "entry", ["entry"]
    for line in body:
        m = LABEL.match(line)
        if m:
            cur = m.group(1)
            blocks[cur] = []
            order.append(cur)
        else:
            blocks[cur].append(line)
    allocas, alias = {}, {}
    for b in order:
        for x in blocks[b]:
            m = ALLOCA.match(x)
            if m:
                allocas[m.group(1)] = m.group(2)
                alias[m.group(1)] = m.group(1)
    for b in order:
        for x in blocks[b]:
            m = GEP.match(x)
            if m and m.group(2) in alias:
                alias[m.group(1)] = alias[m.group(2)]
    defined = set()
    for x in blocks["entry"]:
        m = STORE.match(x)
        if m and m.group(1) in alias:
            defined.add(alias[m.group(1)])
    succ, acc = {}, {}
    for b in order:
        ev, s = [], []
        guarded = b.startswith("tmp.drop")
        for x in blocks[b]:
            t = x.strip()
            if t.startswith(("br ", "switch ")) or t.startswith("[") or re.match(r"i32 \d+, label", t):
                s += TGT.findall(x)
            m = STORE.match(x)
            if m:
                if m.group(1) in alias:
                    ev.append(("w", alias[m.group(1)]))
                continue
            m = LOAD.search(x)
            if m:
                if m.group(1) in alias:
                    ev.append(("r", alias[m.group(1)]))
                continue
            m = MEM.search(x) or OUT_PARAM.search(x)
            if m:
                if m.group(1) in alias:
                    ev.append(("w", alias[m.group(1)]))
                continue
            if " call " in (" " + x) and not (guarded and '@"npk.drop.' in x):
                for p in CALLPTR.findall(x):
                    if p in alias:
                        ev.append(("c", alias[p]))
        succ[b], acc[b] = s, ev
    out = []
    roots = [t for t in succ["entry"] if t not in ("seg0", "coro.bad")]
    for a in (set(allocas) - defined) if roots else ():
        seen, work, hit = set(roots), list(roots), None
        while work and not hit:
            b = work.pop()
            live = True
            for k, v in acc.get(b, ()):
                if v != a:
                    continue
                if k == "w":
                    live = False
                    break
                hit = (k, b)
                break
            if hit or not live:
                continue
            for n in succ.get(b, ()):
                if n not in seen:
                    seen.add(n)
                    work.append(n)
        if hit:
            out.append((fn, a, allocas[a][:50], hit[0], hit[1]))
    return out


def one(npkc, path):
    try:
        r = subprocess.run(["bash", "-c", 'ulimit -n 1024; ulimit -v 24000000; exec "$0" "$1"', npkc, os.path.basename(path)],
                           cwd=os.path.dirname(path), capture_output=True, timeout=300)
    except subprocess.TimeoutExpired:
        return path, None, None
    if r.returncode:
        return path, None, None
    ll = r.stdout.decode("utf-8", "replace")
    if '"npk.resume.' not in ll:
        return path, [], set()
    stale, stems = [], set()
    for fn, body in resume_bodies(ll):
        stale += stale_reads(fn, body)
        stems |= back_edges(fn, body)
    return path, stale, stems


def main():
    args = sys.argv[1:]
    npkc = args.pop(0) if args else os.environ.get("NPK_SELFCHECK_COMPILER")
    if not npkc or not os.path.exists(npkc):
        print("suspend-emission: no compiler named: argument 1, or NPK_SELFCHECK_COMPILER")
        return 2
    npkc = os.path.abspath(npkc)
    paths = [os.path.abspath(p) for p in args] or sorted(glob.glob(os.path.join(ROOT, "tests", "backend", "programs", "*.npk")))
    bad = stale_total = compiled = 0
    seen = set()
    with cf.ThreadPoolExecutor(int(os.environ.get("JOBS", "8"))) as ex:
        for path, stale, stems in ex.map(lambda p: one(npkc, p), paths):
            if stale is None:
                continue
            compiled += 1
            for row in stale:
                stale_total += 1
                bad += 1
                print("suspend-emission: %s: %s reads %s (%s) by %s at %s with no write since a resume entry"
                      % (os.path.basename(path), row[0], row[1], row[2], "a call" if row[3] == "c" else "a load", row[4]))
            for s in sorted(stems - set(KNOWN)):
                bad += 1
                print("suspend-emission: %s: a back edge over a park at `%s`, a stem nobody has accounted for" % (os.path.basename(path), s))
            seen |= stems
    # A table row with no program left is a program lost, or a label renamed.
    for s in sorted(set(KNOWN) - seen):
        if not args:
            bad += 1
            print("suspend-emission: `%s` is in the table and no program's emission holds it" % s)
    if compiled == 0:
        bad += 1
        print("suspend-emission: no program compiled")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
