#!/usr/bin/env python3
"""1.6.0 step 3: THE RUNS (1.6.0.md §2.3). Every engine over every input under ONE fixed profile per engine and
mode, twice from two working directories, the outputs normalised and compared; per (engine, input, mode): the
ingestion verdict, functions defined / analyzed, checks by kind and status, every alarm's text, wall-clock and
peak RSS, and whether the two runs agree byte for byte. The controls (§2.4) run under the same profiles, plain
and planted. Writes .internal/gate/runs/<engine>/<input>/<mode>/{run1,run2}/... and .internal/gate/report.md.

usage: gate_run.py [--inputs DIR] [--controls DIR] [--out DIR] [--only nikos|clam|alive2] [--quick] [--jobs N] [--resume] [--redo PREFIX[,PREFIX]] [--skip PREFIX[,PREFIX]]
       (an --only run merges its rows into an existing OUT/report.json; --resume runs only the tasks the
       report lacks; a plain full run starts the report afresh)
  --quick   the two programs and the controls only (no npkc): the loop for iterating on a profile
"""
import hashlib, json, os, re, shutil, signal, sqlite3, subprocess, sys, time

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", "..", "..", ".."))
PROGRAMS = ["dyn_slots", "extern_c_driver"]
CONTROLS = ["ctl_oob_read", "ctl_oob_write", "ctl_null", "ctl_null_known", "ctl_divz", "ctl_uninit", "ctl_wrap_branch", "ctl_uaf"]
# WHERE each plant's defect sits, and which NIKOS check kind names it (the kind ids are read from the pinned
# tree's checker/kind.hpp at run time -- never a number written here). A control is FOUND when the planted
# program's checks hold a warning or error of that kind in that function; found PRECISELY when the plain
# program's do not (a plain program that alarms at the guarded site too is a precision defect, and 1.6.0.md
# §2.3 (4) counts it as a false alarm of the class, never as a miss).
CONTROL_SITES = {
    "ctl_oob_read":    ("main", ["BufferOverflow", "UnknownMemoryAccess", "InvalidPointerDereference"]),
    "ctl_oob_write":   ("main", ["BufferOverflow", "UnknownMemoryAccess", "InvalidPointerDereference"]),
    "ctl_null":        ("main", ["NullPointerDereference"]),
    "ctl_null_known":  ("main", ["NullPointerDereference"]),
    "ctl_divz":        ("main", ["DivisionByZero"]),
    "ctl_uninit":      ("npk.ctl_uninit.read_vacant", ["UninitializedVariable"]),
    "ctl_wrap_branch": ("main", ["BufferOverflow", "UnknownMemoryAccess", "InvalidPointerDereference"]),
    "ctl_uaf":         ("main", ["UseAfterFree", "InvalidPointerDereference"]),
}


def nikos_kinds(paths):
    """The CheckKind enumeration of the pinned NIKOS, name -> id, read from its header."""
    hdr = os.path.join(os.path.dirname(os.path.dirname(paths["NIKOS_BIN"].rstrip("/"))),
                       "analyzer", "include", "ikos", "analyzer", "checker", "kind.hpp")
    hdr = os.path.normpath(hdr)
    t = open(hdr).read()
    body = t[t.index("enum class CheckKind {"):]
    body = body[:body.index("};")]
    names = [l.strip().rstrip(",") for l in body.split("\n")[1:]
             if l.strip() and not l.strip().startswith("//") and not l.strip().startswith("/")]
    return {n: i for i, n in enumerate(names)}
NIKOS_ANALYSES = "boa,dbz,nullity,uva,sio,uio,shc,poa,uaf,dfa"   # the memory-safety checkers leg A names, plus the overflow/shift ones for the census
NIKOS_STATUS = {0: "ok", 1: "warning", 2: "error", 3: "unreachable"}


def engine_paths():
    r = subprocess.run(["bash", os.path.join(HERE, "engines.sh"), "paths"], capture_output=True, text=True)
    if r.returncode != 0:
        sys.exit("gate_run: engines.sh paths failed: " + r.stderr)
    return dict(l.split("=", 1) for l in r.stdout.strip().split("\n"))


def timed(cmd, cwd, env=None, log=None, timeout=None):
    """Run under /usr/bin/time in its OWN PROCESS GROUP; returns (rc, seconds, peak KB, stdout, stderr).
    At the cap the whole group is killed: `subprocess.run(timeout=)` kills only `/usr/bin/time` and leaves
    the analyzer running as an orphan (measured at step 3: six ikos-analyzer orphans reparented to
    systemd with an hour of CPU each after the first capped batch), which is how a cap becomes a load."""
    tf = os.path.join(cwd, ".time")
    full = ["/usr/bin/time", "-f", "%e %M", "-o", tf] + cmd
    proc = subprocess.Popen(full, cwd=cwd, env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                            text=True, start_new_session=True)
    try:
        out, err = proc.communicate(timeout=timeout)
    except subprocess.TimeoutExpired:
        try:
            os.killpg(os.getpgid(proc.pid), signal.SIGKILL)
        except ProcessLookupError:
            pass
        proc.communicate()
        if log:
            open(log, "w").write("$ " + " ".join(cmd) + "\n--- killed at the cap: %s s (the process group)\n" % timeout)
        return 124, float(timeout), 0, "", "TIMEOUT after %s s" % timeout
    secs, kb = 0.0, 0
    try:
        parts = open(tf).read().split()[-2:]
        secs, kb = float(parts[0]), int(parts[1])
    except Exception:
        pass
    if log:
        open(log, "w").write("$ " + " ".join(cmd) + "\n--- stdout\n" + out + "\n--- stderr\n" + err)
    return proc.returncode, secs, kb, out, err


def killed_by_machine(rc, text):
    """A run the MACHINE stopped is not a result of the engine's. Measured at step 3: earlyoom (installed
    after the 09:59 freeze) SIGTERMs the heaviest process when available memory drops below 10%, and
    Clam over the compiler's whole-program form is 40-80 GiB resident; clam.py reports a child killed by
    SIGTERM/SIGKILL as its "timeout" code 26 (`** Killed by signal N`), which read as an ordinary failure
    until the library listener read the earlyoom journal. Returns the signal number, or 0."""
    m = re.search(r"\*\* Killed by signal (\d+)", text)
    if m:
        return int(m.group(1))
    m = re.search(r"Command terminated by signal (\d+)", text)
    if m:
        return int(m.group(1))
    if rc in (-9, -15, 137, 143):
        return 9 if rc in (-9, 137) else 15
    return 0


def mem_available_gb():
    for l in open("/proc/meminfo"):
        if l.startswith("MemAvailable:"):
            return int(l.split()[1]) / 1024 / 1024
    return 0.0


def wait_for_memory(need_gb, key, log=print):
    """Before a heavy task: wait until MemAvailable is at least need_gb (poll every 30 s, up to 3 h)."""
    waited = 0
    while mem_available_gb() < need_gb and waited < 3 * 3600:
        if waited == 0:
            log("%s: waiting for %d GiB available (%.0f now)" % (key, need_gb, mem_available_gb()))
        time.sleep(30); waited += 30
    return waited


def normalise(text):
    """Strip what may legitimately differ between two runs: timings, temp paths, addresses, the run's own directory."""
    text = re.sub(r'/tmp/clam-[\w]+', '/tmp/clam-X', text)
    text = re.sub(r'/[\w./-]*/run[12]/', '/RUN/', text)
    text = re.sub(r'BRUNCH_STAT [A-Za-z]+ [0-9.]+', 'BRUNCH_STAT X', text)
    text = re.sub(r'0x[0-9a-f]{6,}', '0xADDR', text)
    text = re.sub(r'\b\d+\.\d+ ?s\b', 'T', text)
    return text


# ----------------------------------------------------------------------------------------------- NIKOS
def run_nikos(paths, ll, roots, mode, out, extra=()):
    """ikos-pp -opt=basic, then ikos-analyzer under the profile; the SQLite result read directly."""
    os.makedirs(out, exist_ok=True)
    bc = os.path.join(out, "in.bc")
    subprocess.run([os.path.join(paths["LLVM20_BIN"], "llvm-as"), ll, "-o", bc], check=True)
    ep = ",".join(roots)
    rc, s1, k1, o1, e1 = timed([os.path.join(paths["NIKOS_BIN"], "ikos-pp"), "-opt=basic", "-entry-points=" + ep,
                                bc, "-o", os.path.join(out, "pp.bc")], out, log=os.path.join(out, "pp.log"))
    if rc != 0:
        return {"ingest": "ikos-pp refused (rc %d): %s" % (rc, e1.strip().split("\n")[-1][:200]), "rc": rc}
    cmd = [os.path.join(paths["NIKOS_BIN"], "ikos-analyzer"), "-a=" + NIKOS_ANALYSES, "-d=interval",
           "--entry-points=" + ep, "--proc=" + mode, "-j", "1", "--name-values", "--display-checks=no",
           os.path.join(out, "pp.bc"), "-o", os.path.join(out, "out.db")] + list(extra)
    # intra mode does not complete on our emission within the hour even for dyn_slots's 28 functions
    # (measured at step 3, while inter mode takes 0.04 s): an hour is the row's cap, and a task that
    # reaches it is recorded as such -- the cost is the finding, six hours of it would not be.
    cap = 3600 if mode == "intra" else 6 * 3600
    rc, s2, k2, o2, e2 = timed(cmd, out, log=os.path.join(out, "analyze.log"), timeout=cap)
    res = {"cmd": " ".join(cmd), "rc": rc, "secs": round(s1 + s2, 2), "peak_kb": max(k1, k2)}
    if rc == 124:
        res["ingest"] = "did not complete within %d s (the cap)" % cap
        return res
    sig = killed_by_machine(rc, o2 + e2)
    if sig:
        res["ingest"] = "KILLED BY THE MACHINE (signal %d): no result" % sig
        res["killed"] = True
        return res
    if rc != 0:
        res["ingest"] = "ikos-analyzer refused (rc %d): %s" % (rc, e2.strip().split("\n")[-1][-300:])
        return res
    db = sqlite3.connect(os.path.join(out, "out.db"))
    res["ingest"] = "analyzed"
    res["functions_defined"] = db.execute("select count(*) from functions where definition=1").fetchone()[0]
    res["functions_with_checks"] = db.execute(
        "select count(distinct s.function_id) from checks c join statements s on c.statement_id=s.id").fetchone()[0]
    res["by_status"] = {NIKOS_STATUS.get(k, str(k)): n for k, n in
                        db.execute("select status, count(*) from checks group by status")}
    res["by_kind_status"] = [[k, NIKOS_STATUS.get(s, str(s)), n] for k, s, n in
                             db.execute("select kind, status, count(*) from checks group by kind, status order by kind, status")]
    alarms = db.execute("select f.name, c.kind, c.status, c.operands, c.info from checks c "
                        "join statements s on c.statement_id=s.id join functions f on s.function_id=f.id "
                        "where c.status in (1,2) order by f.name, c.kind, c.operands").fetchall()
    res["alarms"] = ["%s | kind %s | %s | %s | %s" % (f, k, NIKOS_STATUS[s], op, info or "") for f, k, s, op, info in alarms]
    open(os.path.join(out, "alarms.txt"), "w").write("\n".join(res["alarms"]) + "\n")
    res["site_checks"] = [[f, k, NIKOS_STATUS.get(s, str(s))] for f, k, s in db.execute(
        "select f.name, c.kind, c.status from checks c join statements s on c.statement_id=s.id "
        "join functions f on s.function_id=f.id where c.status in (1,2)")]
    return res


# ------------------------------------------------------------------------------------------------ Clam
# Clam's --crab-check takes ONE property per run: none, assert, null(-legacy), uaf(-legacy), bounds,
# is-deref -- there is no division-by-zero check in Clam (measured at step 3: "invalid choice:
# 'div-zero'"), so leg A's third named property has no Clam row; the numeric settings run with
# `assert` (no __CRAB_assert in our IR, so 0 checks: they measure that the numeric machinery reads
# and walks the module, and its cost). The memory settings need a sea-dsa heap analysis.
CLAM_SETTINGS = {}
for _chk in ("bounds", "null", "uaf", "is-deref"):
    CLAM_SETTINGS[_chk + "-inter"] = ["--crab-inter", "--crab-track=mem", "--crab-heap-analysis=cs-sea-dsa", "--crab-check=" + _chk]
    CLAM_SETTINGS[_chk + "-intra"] = ["--crab-track=mem", "--crab-heap-analysis=cs-sea-dsa", "--crab-check=" + _chk]
CLAM_SETTINGS["num-inter"] = ["--crab-inter", "--crab-track=num", "--crab-heap-analysis=none", "--crab-check=assert"]
CLAM_SETTINGS["num-intra"] = ["--crab-track=num", "--crab-heap-analysis=none", "--crab-check=assert"]


def run_clam(paths, ll, mode, out):
    os.makedirs(out, exist_ok=True)
    bc = os.path.join(out, "in18.bc")
    r = subprocess.run([os.path.join(paths["LLVM18_BIN"], "llvm-as"), ll, "-o", bc], capture_output=True, text=True)
    if r.returncode != 0:
        return {"ingest": "llvm-as-18 refused: " + r.stderr.strip()[:200], "rc": r.returncode}
    env = dict(os.environ, PATH=paths["LLVM18_BIN"] + ":" + os.environ["PATH"])
    cmd = [sys.executable, os.path.join(paths["CLAM_BIN"], "clam.py"), bc] + CLAM_SETTINGS[mode] + [
        "--crab-dom=zones", "--crab-lower-with-overflow-intrinsics", "--crab-check-verbose=1",
        "--crab-print-invariants=false", "-o", os.path.join(out, "out.bc")]
    rc, secs, kb, o, e = timed(cmd, out, env=env, log=os.path.join(out, "clam.log"), timeout=6 * 3600)
    res = {"cmd": " ".join(cmd), "rc": rc, "secs": round(secs, 2), "peak_kb": kb}
    text = o + e
    sig = killed_by_machine(rc, text)
    if sig:
        res["ingest"] = "KILLED BY THE MACHINE (signal %d): no result" % sig
        res["killed"] = True
        return res
    m = re.search(r'CLAM ERROR: (.*)', text)
    if m:
        res["ingest"] = "read, analysis aborted: " + m.group(1).strip()
        return res
    if rc != 0:
        res["ingest"] = "clam.py failed (rc %d): %s" % (rc, e.strip()[-300:])
        return res
    res["ingest"] = "analyzed"
    counts = {}
    for n, kind in re.findall(r'(\d+)\s+Number of total (\w+) checks', text):
        counts[kind] = int(n)
    res["by_status"] = counts
    res["alarms"] = [l.strip() for l in text.split("\n") if re.search(r'\b(warning|error)\b', l, re.I)
                     and "Number of total" not in l and "BRUNCH" not in l]
    open(os.path.join(out, "alarms.txt"), "w").write("\n".join(res["alarms"]) + "\n")
    return res


# ---------------------------------------------------------------------------------------------- Alive2
def run_alive2(paths, pre, post, out, rlimit=20000000):
    os.makedirs(out, exist_ok=True)
    cmd = [paths["ALIVE_TV"], "--smt-rlimit=%d" % rlimit, "--smt-random-seed=0", pre, post]
    rc, secs, kb, o, e = timed(cmd, out, log=os.path.join(out, "alive.log"), timeout=6 * 3600)
    sig = killed_by_machine(rc, o + e)
    if sig:
        return {"cmd": " ".join(cmd), "rc": rc, "secs": round(secs, 2), "peak_kb": kb, "killed": True,
                "summary": "KILLED BY THE MACHINE (signal %d): no result" % sig, "ingest": "KILLED BY THE MACHINE (signal %d): no result" % sig}
    res = {"cmd": " ".join(cmd), "rc": rc, "secs": round(secs, 2), "peak_kb": kb}
    text = o + e
    summ = {}
    for n, what in re.findall(r'(\d+) (correct transformations|incorrect transformations|failed-to-prove transformations|Alive2 errors)', text):
        summ[what.split()[0] if what != "Alive2 errors" else "errors"] = int(n)
    res["summary"] = summ
    res["errors"] = sorted(set(l.strip() for l in text.split("\n") if l.startswith("ERROR:")))
    # THE VERDICT PER FUNCTION is what determinism is measured on (the models z3 prints for a
    # counterexample are its choice and may differ between runs; the verdict may not): the
    # function name of each pair, its verdict line, and the ERROR class if any.
    verdicts, fn = [], None
    for l in text.split("\n"):
        m = re.match(r'define [^@]*@([^(]+)\(', l)
        if m:
            fn = m.group(1)
        elif l.startswith("Transformation seems to be correct") or l.startswith("Transformation doesn't verify"):
            verdicts.append("%s: %s" % (fn, l.strip()))
        elif l.startswith("ERROR:"):
            # A BUDGET-OUT IS ONE CLASS whichever z3 call the rlimit lands in: Alive2 labels it
            # `Timeout` when a check returns unknown and `SMT Error: push canceled` when the limit
            # interrupts a push (measured at step 3: one function of extern_c_driver carried each
            # label in the two runs, the same failed-to-prove verdict). The determinism row is over
            # the class; the raw labels stay in `errors`.
            cls = l.strip()
            if cls.startswith("ERROR: Timeout") or cls.startswith("ERROR: SMT Error"):
                cls = "ERROR: budget-out"
            verdicts.append("%s: %s" % (fn, re.sub(r'\d+', 'N', cls)))
    res["verdicts"] = verdicts
    return res


# ------------------------------------------------------------------------------------------------ main
def same_twice(fn, out1, out2, key="alarms"):
    a = fn(out1)
    if a.get("killed"):
        a["deterministic"] = None
        a["twin"] = "not run: the first run was killed by the machine"
        return a
    if a.get("rc") == 124:
        # a run that reached the cap is a row ("did not complete"), not a determinism candidate: the
        # twin run would cost a second cap for nothing (measured at step 3, where it did)
        a["deterministic"] = None
        a["twin"] = "not run: the first run reached the cap"
        return a
    b = fn(out2)
    skip = ("secs", "peak_kb", "cmd")
    n1 = normalise(json.dumps({k: v for k, v in a.items() if k not in skip}, sort_keys=True))
    n2 = normalise(json.dumps({k: v for k, v in b.items() if k not in skip}, sort_keys=True))
    a["deterministic"] = n1 == n2
    a["secs_run2"], a["peak_kb_run2"] = b.get("secs"), b.get("peak_kb")
    return a


def task(kind, key, paths, path, roots, mode, d, rlimit=20000000, post=None):
    """One (engine, input, mode): two runs, sequential, in the task's own directory pair."""
    if kind == "nikos":
        fn = lambda o: run_nikos(paths, path, roots, mode, o)
    elif kind == "clam":
        fn = lambda o: run_clam(paths, path, mode, o)
    else:
        fn = lambda o: run_alive2(paths, path, post, o, rlimit)
    try:
        r = same_twice(fn, os.path.join(d, "run1"), os.path.join(d, "run2"))
    except Exception as e:  # a crashed task is a row, not a lost run
        r = {"ingest": "RUNNER EXCEPTION: %r" % (e,), "rc": -1}
    return key, r


def main():
    args = sys.argv[1:]
    def opt(name, default):
        return args[args.index(name) + 1] if name in args else default
    inputs = os.path.abspath(opt("--inputs", os.path.join(ROOT, ".internal/gate/inputs")))
    controls = os.path.abspath(opt("--controls", os.path.join(ROOT, ".internal/gate/controls")))
    out = os.path.abspath(opt("--out", os.path.join(ROOT, ".internal/gate/runs")))
    only = opt("--only", None)
    jobs = int(opt("--jobs", "8"))
    quick = "--quick" in args
    paths = engine_paths()
    names = PROGRAMS + ([] if quick else ["npkc"])
    forms = []
    for n in names:
        forms.append((n + ".plain", n + ".plain.ll", ["main"]))
        roots = open(os.path.join(inputs, n + ".whole.roots.txt")).read().split()
        forms.append((n + ".whole", n + ".whole.ll", roots))
        forms.append((n + ".plain.dl", n + ".plain.dl.ll", ["main"]))
    if not quick:
        forms.append(("npkc.verified", "npkc.verified.ll", ["main"]))
    for c in CONTROLS:
        for variant in ("plain", "planted"):
            forms.append(("%s.%s" % (c, variant), os.path.join(controls, "%s.%s.ll" % (c, variant)), ["main"]))
    tasks = []
    for name, ll, roots in forms:
        path = ll if os.path.isabs(ll) else os.path.join(inputs, ll)
        if not os.path.exists(path):
            print("skip (no input):", name); continue
        if only in (None, "nikos"):
            for mode in ("inter", "intra"):
                tasks.append(("nikos", "nikos/%s/%s" % (name, mode), paths, path, roots, mode,
                              os.path.join(out, "nikos", name, mode)))
        if only in (None, "clam"):
            for mode in CLAM_SETTINGS:
                tasks.append(("clam", "clam/%s/%s" % (name, mode), paths, path, roots, mode,
                              os.path.join(out, "clam", name, mode)))
    if only in (None, "alive2"):
        for p in PROGRAMS:
            pre = os.path.join(inputs, p + ".plain.ll")
            # the opt pair, and its no-inlining twin (gate_inputs.py): a verdict the twin keeps is not the
            # inliner's, and a verdict it loses was
            for suffix, key in ((".opt.ll", p), (".opt.noinline.ll", p + ".noinline")):
                post = os.path.join(inputs, p + suffix)
                if os.path.exists(post):
                    tasks.append(("alive2", "alive2/%s" % key, paths, pre, ["main"], "tv",
                                  os.path.join(out, "alive2", key), 20000000, post))
    os.makedirs(out, exist_ok=True)
    print("gate_run: %d tasks, %d workers" % (len(tasks), jobs))
    # an --only run MERGES into the report of a full run (the full run's rows stay, its rows are replaced);
    # a --resume run keeps every row the report already holds and runs only the tasks it lacks (a run
    # stopped by hand -- the orphan finding at step 3 -- continues where it was) -- EXCEPT a row the
    # machine killed or a row whose two runs disagreed, which are rerun; --redo PREFIX[,PREFIX] reruns
    # every row whose key starts with a prefix (the rows earlyoom poisoned before kills were classified)
    report = {}
    if (only is not None or "--resume" in args) and os.path.exists(os.path.join(out, "report.json")):
        report = json.load(open(os.path.join(out, "report.json")))
    redo = tuple(x for x in opt("--redo", "").split(",") if x)
    # --skip PREFIX[,PREFIX]: a task the machine cannot run is not re-attempted -- measured at step 3: Clam's
    # memory settings over the compiler's whole-program form reach 132.7-134.6 GiB resident every time and
    # earlyoom kills them at the machine's line (157 GiB total), alone or not, seven times in three hours;
    # the recorded "KILLED BY THE MACHINE" row IS the result on this machine, and a rerun is twenty
    # minutes for the same row
    skip = tuple(x for x in opt("--skip", "").split(",") if x)
    if skip:
        before = len(tasks)
        tasks = [tk for tk in tasks if not tk[1].startswith(skip)]
        print("skip: %d task(s) under %s not attempted" % (before - len(tasks), ",".join(skip)))
    if "--resume" in args:
        before = len(tasks)
        def keep(key):
            r = report.get(key)
            if r is None or key.startswith(redo):
                return False
            if r.get("killed") or "KILLED BY THE MACHINE" in str(r.get("ingest", "")):
                return False
            if r.get("deterministic") is False:
                return False
            return True
        tasks = [tk for tk in tasks if not keep(tk[1])]
        print("resume: %d rows kept, %d of %d tasks still to run" % (len(report), len(tasks), before))
    # THE HEAVY PHASE: every task over one of the compiler's forms runs ALONE, after the light ones, behind
    # a memory guard -- Clam over `npkc.whole` is 40-80 GiB resident (measured at step 3: six such runs in
    # parallel were what froze the machine at 09:59 and what earlyoom killed six times after it)
    heavy = [tk for tk in tasks if tk[1].split("/")[1].startswith("npkc")]
    tasks = [tk for tk in tasks if tk not in heavy]
    from concurrent.futures import ProcessPoolExecutor, as_completed
    def record(key, r):
        report[key] = r
        det = r.get("deterministic")
        print(key, r.get("ingest", r.get("summary")), r.get("by_status", ""), r.get("secs"), "s",
              "deterministic" if det else ("no twin (capped or killed)" if det is None else "NOT-DETERMINISTIC"),
              flush=True)
        json.dump(report, open(os.path.join(out, "report.json"), "w"), indent=1, sort_keys=True)
    with ProcessPoolExecutor(max_workers=jobs) as ex:
        futs = [ex.submit(task, *tk) for tk in tasks]
        for f in as_completed(futs):
            key, r = f.result()
            record(key, r)
    print("heavy phase: %d task(s) over the compiler's forms, one at a time" % len(heavy), flush=True)
    for tk in heavy:
        waited = wait_for_memory(100, tk[1], log=lambda s: print(s, flush=True))
        key, r = task(*tk)
        if waited:
            r["waited_for_memory_s"] = waited
        record(key, r)
    lines = ["# gate runs -- %s" % time.strftime("%Y-%m-%d %H:%M"), "",
             "| run | ingestion | functions (defined / with checks) | by status | alarms | secs | peak MB | deterministic |",
             "|---|---|---|---|---|---|---|---|"]
    for k in sorted(report):
        r = report[k]
        lines.append("| %s | %s | %s / %s | %s | %s | %s | %s | %s |" % (
            k, r.get("ingest", r.get("summary")), r.get("functions_defined", "-"), r.get("functions_with_checks", "-"),
            r.get("by_status", "-"), len(r.get("alarms", r.get("errors", []))), r.get("secs"),
            round((r.get("peak_kb") or 0) / 1024), r.get("deterministic")))
    # THE CONTROLS' VERDICTS (§2.4, row 2 of the scorecard), per engine and mode: FOUND when the planted
    # program's result holds an alarm of the control's kind in the control's function -- precisely when
    # the plain program's does not -- and MISSED otherwise; NO RUN when the engine did not analyze it.
    kinds = nikos_kinds(paths)
    lines += ["", "## controls: is the planted defect reported at its site? (planted / plain), per engine and mode", ""]
    for c in CONTROLS:
        fn, kind_names = CONTROL_SITES[c]
        want = set(kinds[k] for k in kind_names if k in kinds)
        for key in sorted(k for k in report if "/%s.planted/" % c in k):
            plain_key = key.replace(".planted/", ".plain/")
            r, rp = report[key], report.get(plain_key, {})
            if "analyzed" not in str(r.get("ingest", "")):
                lines.append("- `%s`: NO RUN (%s)" % (key, str(r.get("ingest", ""))[:120])); continue
            if key.startswith("nikos/"):
                hit = [x for x in r.get("site_checks", []) if x[0] == fn and x[1] in want]
                hit_plain = [x for x in rp.get("site_checks", []) if x[0] == fn and x[1] in want]
            else:  # clam: a check line naming the function
                hit = [a for a in r.get("alarms", []) if fn in a]
                hit_plain = [a for a in rp.get("alarms", []) if fn in a]
            if hit and not hit_plain:
                v = "FOUND, precisely (%d at the site, none in the plain program)" % len(hit)
            elif hit:
                v = "FOUND, imprecisely (%d at the site; the plain program alarms there too: %d)" % (len(hit), len(hit_plain))
            else:
                v = "MISSED (no alarm of %s in %s; the planted program's alarms: %d)" % ("/".join(kind_names), fn, len(r.get("alarms", [])))
            lines.append("- `%s`: %s" % (key, v))
    open(os.path.join(out, "report.md"), "w").write("\n".join(lines) + "\n")
    print("\n".join(lines))


if __name__ == "__main__":
    main()
