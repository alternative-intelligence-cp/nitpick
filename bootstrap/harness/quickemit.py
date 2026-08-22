"""Build npkc once, then compile and RUN one program through the real backend.

The backend counterpart to `quickcheck.py`, and it exists for the same reason:
the middle of a backend subcycle asks "does this one construct emit, link and
run", and a full harness invocation costs about twenty minutes to answer it.

    python3 bootstrap/harness/quickemit.py tests/backend/programs/family_impl.npk

It prints the exit code, and with `--ir` the emitted IR instead — which is the
thing you actually want when a program links but does the wrong thing.

IT IS NOT A SUBSTITUTE FOR THE HARNESS and skips every whole-suite check: the
selfhost fixpoint, the zero-dependency scan over every program, node-kind
reachability, and the seven whole-tree checks. Nothing is committed on the
strength of a `quickemit` run; do a full one first.

Rebuild is automatic when `src/` is newer than the cached binary, because a
stale compiler answering an old question is the failure mode to expect.
"""

import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(ROOT, "bootstrap", "generator"))

import harness  # noqa: E402

OUT = os.path.join(ROOT, ".internal", "quickemit")


def newest_source():
    newest = 0.0
    for base in ("src", "lib", "bootstrap/generator", "bootstrap/runtime"):
        for dirpath, _, names in os.walk(os.path.join(ROOT, base)):
            for n in names:
                if n.endswith((".npk", ".py", ".ll")):
                    newest = max(newest, os.path.getmtime(os.path.join(dirpath, n)))
    return newest


def build():
    os.makedirs(OUT, exist_ok=True)
    rt = os.path.join(OUT, "npkrt.o")
    npkc = os.path.join(OUT, "npkc")
    if os.path.exists(npkc) and os.path.getmtime(npkc) > newest_source():
        return npkc
    if not os.path.exists(rt):
        r = subprocess.run(["llc", "-O0", "-filetype=obj",
                            "-relocation-model=static",
                            harness.RUNTIME_LL, "-o", rt],
                           capture_output=True, text=True)
        if r.returncode != 0:
            return "runtime: %s" % r.stderr.strip()[:400]
    return harness.build_tool(OUT, True, harness.EMIT_CHECK, "npkc")


def emit(npkc, path):
    r = subprocess.run([npkc, path], capture_output=True, timeout=120)
    if r.returncode != 0:
        return None, r.stderr.decode("utf-8", "replace").strip()
    return r.stdout, None


def main():
    args = [a for a in sys.argv[1:] if a != "--ir"]
    want_ir = "--ir" in sys.argv[1:]

    npkc = build()
    if not os.path.exists(npkc):
        print(npkc)
        return 1
    print(npkc)

    rc = 0
    for path in args:
        ir, err = emit(npkc, path)
        if ir is None:
            print("\n%s: REFUSED\n%s" % (path, err))
            rc = 1
            continue
        if want_ir:
            print("\n%s:\n%s" % (path, ir.decode("utf-8", "replace")))
            continue
        base = os.path.join(OUT, "p_" + os.path.basename(path).replace(".", "_"))
        with open(base + ".ll", "wb") as fh:
            fh.write(ir)
        r = subprocess.run(["llc", "-O0", "-filetype=obj",
                            "-relocation-model=static",
                            base + ".ll", "-o", base + ".o"],
                           capture_output=True, text=True)
        if r.returncode != 0:
            first = next((l for l in r.stderr.splitlines() if "error" in l),
                         r.stderr)
            print("\n%s: llc REJECTED the IR: %s" % (path, first.strip()[:400]))
            rc = 1
            continue
        r = subprocess.run(["ld.lld", "-static", "-o", base, base + ".o",
                            os.path.join(OUT, "npkrt.o")],
                           capture_output=True, text=True)
        if r.returncode != 0:
            print("\n%s: LINK failed: %s" % (path, r.stderr.strip()[:400]))
            rc = 1
            continue
        try:
            got = subprocess.run([base], capture_output=True, timeout=10).returncode
        except subprocess.TimeoutExpired:
            print("\n%s: timed out" % path)
            rc = 1
            continue
        print("\n%s: exit %d" % (path, got))
    print("\n(quickemit skips every whole-suite check -- iterate here, "
          "conclude with the harness)")
    return rc


if __name__ == "__main__":
    sys.exit(main())
