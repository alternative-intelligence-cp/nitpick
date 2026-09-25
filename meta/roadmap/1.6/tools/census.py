#!/usr/bin/env python3
"""1.6.0 planning: the instruction census of an emitted LLVM module -- what the
analyzers of leg A and leg C must model. Opcodes, intrinsics, the attributes
that carry semantics, and the three module facts that shape ingestion (debug
info, a data layout, a triple). With --assemble, which of LLVM 14/18/20's
assemblers accepts the text as written (the gate's criterion 1, first half).

usage: census.py [--assemble] FILE.ll ...
"""
import collections, os, re, subprocess, sys

ATTRS = ("nneg", "disjoint", "nuw", "nsw", "nusw", "inbounds", "samesign", "exact",
         "nnan", "ninf", "nsz", "arcp", "contract", "afn", "reassoc", "fast", "volatile",
         "syncscope", "freeze", "poison", "undef", "zeroinitializer", "splat", "dso_local",
         "internal", "private", "unnamed_addr", "noalias", "nocapture", "captures",
         "dereferenceable", "\"split-stack\"", "nounwind", "noreturn", "inlineasm",
         "blockaddress", "indirectbr", "invoke", "landingpad", "switch", "select", "phi",
         "atomic", "fence", "cmpxchg", "atomicrmw")
OP_RE = re.compile(r'(?:%[\w.$"]+\s*=\s*)?(\w+)')
KNOWN_OPS = {"load", "store", "alloca", "call", "extractvalue", "insertvalue", "br", "icmp",
             "fcmp", "ret", "zext", "sext", "trunc", "getelementptr", "unreachable", "xor",
             "add", "sub", "mul", "and", "or", "ptrtoint", "inttoptr", "phi", "sdiv", "udiv",
             "srem", "urem", "shl", "lshr", "ashr", "select", "switch", "fadd", "fsub", "fmul",
             "fdiv", "frem", "fptosi", "fptoui", "sitofp", "uitofp", "fpext", "fptrunc",
             "bitcast", "insertelement", "extractelement", "shufflevector", "fneg", "fence",
             "cmpxchg", "atomicrmw", "indirectbr", "invoke", "landingpad", "resume",
             "define", "declare", "type", "global", "constant", "attributes", "target",
             "source_filename", "module"}


def census(path):
    txt = open(path, encoding="utf-8", errors="replace").read()
    ops, intr, attrs = collections.Counter(), collections.Counter(), collections.Counter()
    for l in txt.split("\n"):
        s = l.strip()
        if not s or s.startswith(";") or s.startswith("!"):
            continue
        m = OP_RE.match(s)
        if m and m.group(1) in KNOWN_OPS:
            ops[m.group(1)] += 1
        for name in re.findall(r'@(llvm\.[\w.]+)', s):
            intr[re.sub(r'\.i\d+$', '.iN', re.sub(r'\.v\d+i\d+$', '.vN', name))] += 1
        code = s.split(";")[0]
        for a in ATTRS:
            if re.search(r'(?<![\w."])' + re.escape(a) + r'(?![\w."])', code):
                attrs[a] += 1
    defines = len(re.findall(r'^define ', txt, re.M))
    print("== %s: %d defines, %d lines, %d bytes" % (path, defines, txt.count("\n"), len(txt.encode())))
    print("   !dbg: %d   target datalayout: %d   target triple: %d   module asm: %d"
          % (txt.count("!dbg"), len(re.findall(r'^target datalayout', txt, re.M)),
             len(re.findall(r'^target triple', txt, re.M)), len(re.findall(r'^module asm', txt, re.M))))
    print("   opcodes:", ", ".join("%s %d" % kv for kv in sorted(ops.items(), key=lambda kv: -kv[1])))
    print("   intrinsics (widths folded to iN):", ", ".join("%s %d" % kv for kv in sorted(intr.items())))
    print("   attributes/words (code only, comments stripped):",
          ", ".join("%s %d" % kv for kv in sorted(attrs.items())))
    sites = len(re.findall(r'call [^@\n]*@llvm\.[su](?:add|sub|mul)\.with\.overflow', txt))
    print("   with.overflow call sites: %d" % sites)


def assemble(path):
    for v in ("14", "18", "20"):
        exe = "/usr/lib/llvm-%s/bin/llvm-as" % v
        if not os.path.exists(exe):
            print("   llvm-as-%s: not installed" % v)
            continue
        r = subprocess.run([exe, path, "-o", "/dev/null"], capture_output=True, text=True)
        first = next((l for l in r.stderr.splitlines() if l.strip()), "")
        print("   llvm-as-%s: rc %d %s" % (v, r.returncode, first[:120]))


if __name__ == "__main__":
    args = sys.argv[1:]
    do_asm = "--assemble" in args
    for p in [a for a in args if a != "--assemble"]:
        census(p)
        if do_asm:
            assemble(p)
