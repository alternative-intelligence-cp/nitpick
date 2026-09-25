#!/usr/bin/env python3
"""1.6.0 step 2: the gate's INPUT SET, produced from the compiler under test and recorded by digest
(1.6.0.md §2.2). Writes into OUT (default .internal/gate/inputs):

  <name>.plain.ll          the three plain emissions as the harness assembles them (-O0)
  <name>.whole.ll          each plain emission with the floor appended BY TEXT (P-7): the program's
                           `declare`s of floor-defined symbols and the floor's `declare`s of
                           program-defined symbols removed, one `target triple` kept
  <name>.whole.roots.txt   the entry points of the whole-program form: `main` and every floor function
                           whose address escapes to the assembly or the kernel, read off runtime/npkrt.ll
  npkc.verified.ll         the compiler's emission under `--elide nitpick.obligations` (the llvm.assume channel)
  *.dl.ll                  the datalayout twin of every input above (P-5)
  <name>.opt.ll            the opt pair's post file (nitpick.toml's opt-flags) for dyn_slots and extern_c_driver
  inputs.txt               one line per file: sha256, bytes, defines, name -- and the assembler belt's verdicts

Every .ll must assemble under LLVM 18 and LLVM 20 (the belt; a refusal is a red run by name).
usage: gate_inputs.py [--out DIR]
"""
import hashlib, os, re, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", "..", "..", ".."))
PROGRAMS = ["dyn_slots", "extern_c_driver"]
LLVM = {"18": "/usr/lib/llvm-18/bin", "20": "/usr/lib/llvm-20/bin"}
DATALAYOUT = ('target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-'
              'n8:16:32:64-S128"')
DECL_RE = re.compile(r'^declare[^@\n]*@("?[\w.$:<>,]+"?)\(', re.M)
DEF_RE = re.compile(r'^define[^@\n]*@("?[\w.$:<>,]+"?)\(', re.M)


def sh(cmd, **kw):
    r = subprocess.run(cmd, capture_output=True, text=True, **kw)
    if r.returncode != 0:
        sys.exit("gate_inputs: %s failed (rc %d):\n%s" % (" ".join(cmd[:3]), r.returncode, r.stderr[:800]))
    return r


def opt_flags():
    t = open(os.path.join(ROOT, "nitpick.toml")).read()
    m = re.search(r'^opt-flags\s*=\s*\[(.*?)\]', t, re.M)
    return [x.strip().strip('"') for x in m.group(1).split(",") if x.strip()]


def digest(path):
    b = open(path, "rb").read()
    return hashlib.sha256(b).hexdigest(), len(b), len(DEF_RE.findall(b.decode("utf-8", "replace")))


def floor_roots(floor_text):
    """A defined floor function whose address escapes: named by `ptr @f` where it is not the callee of a
    call, or named inside a `module asm` line (the assembly calls it by symbol)."""
    defined = set(DEF_RE.findall(floor_text))
    roots = set()
    for line in floor_text.split("\n"):
        code = line.split(";")[0]
        if code.startswith("module asm"):
            for name in defined:
                if re.search(r'\b' + re.escape(name.strip('"')) + r'\b', code):
                    roots.add(name)
            continue
        for m in re.finditer(r'ptr @("?[\w.$]+"?)', code):
            name = m.group(1)
            if name not in defined:
                continue
            # the callee of a call is `call ... @f(` -- not a `ptr @f` operand
            after = code[m.end():m.end() + 1]
            if after == "(":
                continue
            roots.add(name)
    return sorted(roots)


def whole_program(prog_text, floor_text):
    """The program's text, then the floor's: a `declare` of a symbol the other side DEFINES goes (the
    program declares the floor's exports; the floor declares `main`), and a `declare` the program
    already carries -- the intrinsics both sides declare, `llvm.umul.with.overflow.i64` among them --
    is dropped from the floor (an identical second `declare` is "invalid redefinition" to llvm-as).
    One `target triple`."""
    prog_defs = set(DEF_RE.findall(prog_text))
    floor_defs = set(DEF_RE.findall(floor_text))
    prog_decls = set(DECL_RE.findall(prog_text))
    keep_prog = [l for l in prog_text.split("\n")
                 if not (DECL_RE.match(l) and DECL_RE.match(l).group(1) in floor_defs)]
    keep_floor = []
    for l in floor_text.split("\n"):
        m = DECL_RE.match(l)
        if m and (m.group(1) in prog_defs or m.group(1) in prog_decls):
            continue
        if l.startswith("target triple"):
            continue
        keep_floor.append(l)
    return "\n".join(keep_prog).rstrip("\n") + "\n\n" + "\n".join(keep_floor).rstrip("\n") + "\n"


def with_datalayout(text):
    return re.sub(r'^(target triple = .*)$', DATALAYOUT + "\n\\1", text, count=1, flags=re.M)


def main():
    out = os.path.join(ROOT, ".internal", "gate", "inputs")
    if "--out" in sys.argv:
        out = os.path.abspath(sys.argv[sys.argv.index("--out") + 1])
    os.makedirs(out, exist_ok=True)
    # 1. the compiler under test, through quickemit (rebuilds .internal/quickemit/npkc when src/ moved)
    for p in PROGRAMS:
        sh([sys.executable, os.path.join(ROOT, "bootstrap/harness/quickemit.py"), "--keep",
            os.path.join(ROOT, "tests/backend/programs", p + ".npk")], cwd=ROOT)
    npkc = os.path.join(ROOT, ".internal/quickemit/npkc")
    files = {}
    for p in PROGRAMS:
        src = os.path.join(ROOT, ".internal/quickemit/p_%s_npk.ll" % p)
        files[p + ".plain.ll"] = open(src).read()
    r = sh([npkc, "src/npkc.npk", "-o", os.path.join(out, "npkc.plain.ll")], cwd=ROOT)
    files["npkc.plain.ll"] = open(os.path.join(out, "npkc.plain.ll")).read()
    # 2. the verified emission of the compiler (the committed manifest; D-218.9's channel)
    obl = os.path.join(out, "obl")
    sh([npkc, "src/npkc.npk", "--elide", "nitpick.obligations", "--obligations", obl,
        "-o", os.path.join(out, "npkc.verified.ll")], cwd=ROOT)
    files["npkc.verified.ll"] = open(os.path.join(out, "npkc.verified.ll")).read()
    # 3. the whole-program forms and their roots
    floor = open(os.path.join(ROOT, "runtime/npkrt.ll")).read()
    roots = floor_roots(floor)
    for p in PROGRAMS + ["npkc"]:
        files[p + ".whole.ll"] = whole_program(files[p + ".plain.ll"], floor)
        files[p + ".whole.roots.txt"] = "\n".join(["main"] + roots) + "\n"
    # 4. the datalayout twins
    for name in [n for n in list(files) if n.endswith(".ll")]:
        files[name[:-3] + ".dl.ll"] = with_datalayout(files[name])
    # write, then the opt pairs and the belt
    for name, text in files.items():
        open(os.path.join(out, name), "w").write(text)
    for p in PROGRAMS:
        sh([os.path.join(LLVM["20"], "opt")] + opt_flags() + [os.path.join(out, p + ".plain.ll"),
            "-o", os.path.join(out, p + ".opt.ll")])
    lines = []
    red = 0
    for name in sorted(os.listdir(out)):
        path = os.path.join(out, name)
        if not (name.endswith(".ll") or name.endswith(".txt")) or name == "inputs.txt" or os.path.isdir(path):
            continue
        d, n, defs = digest(path)
        belt = ""
        if name.endswith(".ll"):
            # every input assembles under LLVM 20; the ones Clam reads (not the opt pairs, which are
            # leg C's and carry LLVM 20's newer attribute syntax) assemble under LLVM 18 as well
            verdicts = []
            for v, b in LLVM.items():
                if v == "18" and name.endswith(".opt.ll"):
                    continue
                rr = subprocess.run([os.path.join(b, "llvm-as"), path, "-o", "/dev/null"],
                                    capture_output=True, text=True)
                verdicts.append("as%s=%s" % (v, "ok" if rr.returncode == 0 else "REFUSED"))
                red += rr.returncode != 0
            belt = " " + " ".join(verdicts)
        lines.append("%s %d %d %s%s" % (d, n, defs, name, belt))
    open(os.path.join(out, "inputs.txt"), "w").write("\n".join(lines) + "\n")
    print("\n".join(lines))
    print("roots:", ", ".join(roots))
    if red:
        sys.exit("gate_inputs: %d input(s) refused by an assembler -- a red run by name" % red)


if __name__ == "__main__":
    main()
