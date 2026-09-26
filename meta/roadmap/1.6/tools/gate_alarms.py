#!/usr/bin/env python3
"""1.6.0 step 3: THE ALARM SITES of a NIKOS run (1.6.0.md §2.3 row 4). NIKOS's inter-procedural mode
records one check per (statement, call context), so a run's raw alarm count is context-multiplied: one
floor function with four alarm statements reached from 740 contexts is 4,440 rows of `checks` (measured
at step 3 on dyn_slots.whole: 15,360 rows, 455 sites). What is READ is the SITE -- (function, statement,
check, status) with the message ikos-report renders and the number of contexts that reach it -- so the
record's row-4 numbers are sites, and the raw count is quoted beside them.

usage: gate_alarms.py [--shapes] [--sample PREFIX[,PREFIX] --random N --seed S] RUN_DIR...
                                                   each RUN_DIR holds run1/out.db (gate_run.py's layout)
--sample writes RUN_DIR/sample.txt: every site of the functions whose name starts with a PREFIX (§2.3
row 4's compiler sample: `npk.lexer.` and `npk.ir_rules.`) plus N sites drawn at random under seed S
from the rest -- the compiler's alarms are read by sample, the two programs' in full.
Writes RUN_DIR/sites.txt (one line per site: function | check | status | contexts | message) and prints,
per run, the sites by (check, status) and the functions with the most sites; with --shapes, also the
sites by MESSAGE SHAPE (the quoted operand expressions replaced by `…`), which is how the floor's sites
in a whole-program form are classed for reading -- one representative per shape is read, not every site.
"""
import collections, csv, io, os, random, re, subprocess, sys

csv.field_size_limit(sys.maxsize)  # a whole-program site lists hundreds of contexts in one field

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import gate_run  # engine_paths()


def sites_of(db, report_bin):
    r = subprocess.run([report_bin, "-f", "csv", "-v", "4", db], capture_output=True, text=True)
    if r.returncode != 0:
        sys.exit("gate_alarms: ikos-report failed on %s: %s" % (db, r.stderr[-300:]))
    rows = list(csv.DictReader(io.StringIO(r.stdout)))
    out = collections.OrderedDict()
    for row in rows:
        if row["status"] not in ("warning", "error"):
            continue
        key = (row["function"], row["statement_id"], row["check"], row["status"], row["message"])
        ctx = row["contexts"].strip()
        n = 1 if ctx in ("", ".") else len([c for c in ctx.split("|") if c.strip()])
        out[key] = out.get(key, 0) + n
    return out


def shape(msg):
    s = re.sub(r"'[^']*'", "…", msg)
    s = re.sub(r"of \d+ elements", "of N elements", s)
    return re.sub(r"\d+", "N", s)


def main():
    paths = gate_run.engine_paths()
    report_bin = os.path.join(paths["NIKOS_BIN"], "ikos-report")
    args = sys.argv[1:]
    def opt(name, default):
        return args[args.index(name) + 1] if name in args else default
    shapes = "--shapes" in args
    sample = opt("--sample", None)
    n_random, seed = int(opt("--random", "100")), int(opt("--seed", "0"))
    skip = set()
    for name in ("--sample", "--random", "--seed"):
        if name in args:
            skip.add(args.index(name)); skip.add(args.index(name) + 1)
    dirs = [a for i, a in enumerate(args) if a != "--shapes" and i not in skip]
    for d in dirs:
        db = os.path.join(d, "run1", "out.db")
        if not os.path.exists(db):
            print("%s: no run1/out.db (not analyzed)" % d); continue
        sites = sites_of(db, report_bin)
        by_cs, by_fn, by_shape = collections.Counter(), collections.Counter(), collections.Counter()
        ctx_cs, shape_fns = collections.Counter(), collections.defaultdict(set)
        lines = []
        for (fn, sid, chk, st, msg), n in sites.items():
            by_cs[(chk, st)] += 1; ctx_cs[(chk, st)] += n; by_fn[fn] += 1
            by_shape[(chk, st, shape(msg))] += 1; shape_fns[(chk, st, shape(msg))].add(fn)
            lines.append("%s | %s | %s | contexts=%d | %s" % (fn, chk, st, n, msg.replace("\n", " ")))
        open(os.path.join(d, "sites.txt"), "w").write("\n".join(lines) + "\n")
        print("== %s: %d sites, %d context-rows" % (d, len(sites), sum(sites.values())))
        for (chk, st), n in sorted(by_cs.items(), key=lambda kv: -kv[1]):
            print("   %-24s %-8s sites %5d  context-rows %6d" % (chk, st, n, ctx_cs[(chk, st)]))
        print("   functions with the most sites:", ", ".join("%s %d" % kv for kv in by_fn.most_common(12)))
        if sample:
            prefixes = tuple(sample.split(","))
            named = [l for l in lines if l.split(" | ")[0].startswith(prefixes)]
            rest = [l for l in lines if not l.split(" | ")[0].startswith(prefixes)]
            rng = random.Random(seed)
            drawn = rng.sample(rest, min(n_random, len(rest)))
            open(os.path.join(d, "sample.txt"), "w").write(
                "# %d sites of %s, then %d drawn at random (seed %d) from the other %d\n"
                % (len(named), sample, len(drawn), seed, len(rest)) + "\n".join(named + drawn) + "\n")
            print("   sample: %d sites under %s + %d random (seed %d) of %d -> sample.txt"
                  % (len(named), sample, len(drawn), seed, len(rest)))
        if shapes:
            print("   by shape (sites, functions):")
            for (chk, st, sh), n in sorted(by_shape.items(), key=lambda kv: -kv[1]):
                print("     %4d  %-22s %-8s %s  [%d fns]" % (n, chk, st, sh[:150], len(shape_fns[(chk, st, sh)])))


if __name__ == "__main__":
    main()
