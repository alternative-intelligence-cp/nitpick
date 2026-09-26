#!/usr/bin/env python3
"""1.6.0 step 2: THE CONTROLS (1.6.0.md §2.4, P-9). Each control under controls/ is a program the compiler under
test accepts, with ONE defect the language's guards make unreachable in source; its .plant file removes the
guard from the EMISSION by exact-line substitution (plant.py). Both binaries -- plain and planted -- are built
exactly as the harness builds a program (llvm-as, llc with nitpick.toml's llc-flags, ld.lld with the floor)
and RUN under the runners' descriptor limit, N times each; a control counts only when the plain binary answers
its `expect-plain` every time and the planted one its `expect-planted` every time. A control whose planted
binary answers like the plain one is BLIND and is reported as such -- its engine verdicts are worth nothing.

Writes OUT/<name>.plain.ll, OUT/<name>.planted.ll (the analyzers' inputs) and OUT/controls.txt.
usage: gate_controls.py [--out DIR] [--runs N]
"""
import os, re, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", "..", "..", ".."))
CONTROLS = ["ctl_oob_read", "ctl_oob_write", "ctl_null", "ctl_null_known", "ctl_divz", "ctl_uninit", "ctl_wrap_branch", "ctl_uaf"]
LLVM20 = "/usr/lib/llvm-20/bin"


def flags(key):
    t = open(os.path.join(ROOT, "nitpick.toml")).read()
    m = re.search(r'^%s\s*=\s*\[(.*?)\]' % re.escape(key), t, re.M)
    return [x.strip().strip('"') for x in m.group(1).split(",") if x.strip()]


def nofile():
    t = open(os.path.join(ROOT, "nitpick.toml")).read()
    m = re.search(r'^nofile\s*=\s*(\d+)', t, re.M)
    return int(m.group(1)) if m else 1024


def build(ll, out_bin, tmp):
    bc, obj = out_bin + ".bc", out_bin + ".o"
    for cmd in ([os.path.join(LLVM20, "llvm-as"), ll, "-o", bc],
                [os.path.join(LLVM20, "llc")] + flags("llc-flags") + [bc, "-o", obj],
                [os.path.join(LLVM20, "ld.lld")] + flags("lld-flags") + ["-o", out_bin, obj,
                 os.path.join(tmp, "npkrt.o")]):
        r = subprocess.run(cmd, capture_output=True, text=True)
        if r.returncode != 0:
            sys.exit("gate_controls: %s failed: %s" % (cmd[0], r.stderr[:300]))


def run_exit(binary, limit):
    r = subprocess.run(["bash", "-c", "ulimit -n %d; exec %s" % (limit, binary)],
                       capture_output=True, timeout=60)
    return r.returncode


def main():
    args = sys.argv[1:]
    out = os.path.abspath(args[args.index("--out") + 1]) if "--out" in args else os.path.join(ROOT, ".internal/gate/controls")
    runs = int(args[args.index("--runs") + 1]) if "--runs" in args else 5
    os.makedirs(out, exist_ok=True)
    qe = os.path.join(ROOT, ".internal/quickemit")
    limit = nofile()
    lines, blind = [], 0
    for c in CONTROLS:
        src = os.path.join(HERE, "controls", c + ".npk")
        r = subprocess.run([sys.executable, os.path.join(ROOT, "bootstrap/harness/quickemit.py"), "--keep", src],
                           cwd=ROOT, capture_output=True, text=True)
        plain_ll = os.path.join(qe, "p_%s_npk.ll" % c)
        if r.returncode != 0 or not os.path.exists(plain_ll):
            sys.exit("gate_controls: quickemit refused %s: %s" % (c, (r.stdout + r.stderr)[-400:]))
        plain_out = os.path.join(out, c + ".plain.ll")
        planted_out = os.path.join(out, c + ".planted.ll")
        open(plain_out, "w").write(open(plain_ll).read())
        p = subprocess.run([sys.executable, os.path.join(HERE, "plant.py"), os.path.join(HERE, "controls", c + ".plant"),
                            plain_ll, planted_out], capture_output=True, text=True)
        if p.returncode != 0:
            sys.exit("gate_controls: " + p.stdout + p.stderr)
        exp_plain, exp_planted = re.match(r'plain (\d+) planted (\d+)', p.stdout).groups()
        build(plain_out, os.path.join(out, c + ".plain"), qe)
        build(planted_out, os.path.join(out, c + ".planted"), qe)
        got_plain = [run_exit(os.path.join(out, c + ".plain"), limit) for _ in range(runs)]
        got_planted = [run_exit(os.path.join(out, c + ".planted"), limit) for _ in range(runs)]
        ok_plain = all(g == int(exp_plain) for g in got_plain)
        ok_planted = all(g == int(exp_planted) for g in got_planted)
        verdict = "exhibits" if ok_plain and ok_planted else "BLIND"
        blind += verdict == "BLIND"
        lines.append("%s plain %s (expected %s) planted %s (expected %s) x%d: %s"
                     % (c, sorted(set(got_plain)), exp_plain, sorted(set(got_planted)), exp_planted, runs, verdict))
        print(lines[-1])
    open(os.path.join(out, "controls.txt"), "w").write("\n".join(lines) + "\n")
    if blind:
        sys.exit("gate_controls: %d blind control(s)" % blind)


if __name__ == "__main__":
    main()
