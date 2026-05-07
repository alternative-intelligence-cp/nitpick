#!/usr/bin/env python3
"""
Nitpick EMI Mass Fuzzer — Phase 4

Generates 100,000+ variants from the full corpus and compiles each at
-O0, -O1, and -O2, cross-comparing outputs to find miscompilations.

Classification:
  - O1_BUG   : variant(-O1) output differs from variant(-O0) output
  - O2_BUG   : variant(-O2) output differs from variant(-O0) output
  - UNSAFE   : variant(-O0) differs from seed(-O0) — mutator produced an
                accidentally non-equivalent variant (logged, not a compiler bug)

Usage:
    python3 mass_fuzzer.py [options]
    python3 mass_fuzzer.py --seed-list priority_seeds.txt --variants 160 --workers 48
"""

import argparse
import json
import os
import random
import sys
import time
from concurrent.futures import ProcessPoolExecutor, as_completed
from pathlib import Path
from typing import List, Optional

sys.path.insert(0, str(Path(__file__).parent))


# ---------------------------------------------------------------------------
# Per-seed multi-opt worker (runs in subprocess pool)
# ---------------------------------------------------------------------------

def _process_seed_multiopt(args_tuple) -> dict:
    """
    For one seed:
      1. Compile seed at -O0 (canonical baseline)
      2. Generate N variants
      3. For each variant:
           a. Compile at -O0 — must match seed-O0 (mutation-safety check)
           b. Compile at -O1 — compare to variant-O0
           c. Compile at -O2 — compare to variant-O0
    """
    (seed_path, compiler_path, n_variants, rng_seed,
     compile_timeout, run_timeout) = args_tuple

    from mutator import generate_variants
    from harness import compile_and_run

    compiler = Path(compiler_path)
    seed = Path(seed_path)
    rng = random.Random(rng_seed)
    source = seed.read_text(encoding="utf-8")

    # Seed baseline at -O0
    seed_o0 = compile_and_run(source, compiler,
                               compile_timeout=compile_timeout,
                               run_timeout=run_timeout,
                               extra_flags=["-O0"])

    if not seed_o0.compiled or seed_o0.timed_out:
        return {"status": "skipped", "seed": seed_path,
                "miscompiles": [], "unsafe": 0,
                "compiled": 0, "timeouts": 0, "variants": 0}

    variants = generate_variants(source, seed_path, n_variants, rng)

    miscompiles = []
    compiled_count = 0
    timeout_count = 0
    unsafe_count = 0

    for idx, variant in enumerate(variants):
        # Step (a): compile at -O0, verify mutation safety
        r0 = compile_and_run(variant.source, compiler,
                              compile_timeout=compile_timeout,
                              run_timeout=run_timeout,
                              extra_flags=["-O0"])

        if r0.timed_out:
            timeout_count += 1
            continue
        if not r0.compiled:
            # Variant doesn't compile at all — not a miscompile, skip
            continue

        compiled_count += 1

        if not seed_o0.matches(r0):
            # Mutation accidentally changed semantics — log as unsafe, skip opt checks
            unsafe_count += 1
            continue

        # Steps (b) and (c): check each optimization level against -O0
        for opt_flag, opt_label in [("-O1", "O1_BUG"), ("-O2", "O2_BUG")]:
            ropt = compile_and_run(variant.source, compiler,
                                   compile_timeout=compile_timeout,
                                   run_timeout=run_timeout,
                                   extra_flags=[opt_flag])

            if ropt.timed_out:
                timeout_count += 1
                continue
            if not ropt.compiled:
                # Failed to compile at higher opt — treat as miscompile (compiler crash)
                miscompiles.append({
                    "kind": opt_label + "_COMPILE_FAIL",
                    "opt": opt_flag,
                    "seed_path": seed_path,
                    "variant_index": idx,
                    "mutations": [{"kind": m.kind.name, "description": m.description}
                                   for m in variant.mutations],
                    "baseline_exit": seed_o0.exit_code,
                    "baseline_stdout": seed_o0.stdout,
                    "o0_exit": r0.exit_code,
                    "o0_stdout": r0.stdout,
                    "opt_exit": None,
                    "opt_stdout": "",
                    "opt_stderr": ropt.compile_stderr[:500],
                    "variant_source": variant.source,
                })
                continue

            if not r0.matches(ropt):
                miscompiles.append({
                    "kind": opt_label,
                    "opt": opt_flag,
                    "seed_path": seed_path,
                    "variant_index": idx,
                    "mutations": [{"kind": m.kind.name, "description": m.description}
                                   for m in variant.mutations],
                    "baseline_exit": seed_o0.exit_code,
                    "baseline_stdout": seed_o0.stdout,
                    "o0_exit": r0.exit_code,
                    "o0_stdout": r0.stdout,
                    "opt_exit": ropt.exit_code,
                    "opt_stdout": ropt.stdout,
                    "opt_stderr": "",
                    "variant_source": variant.source,
                })

    return {
        "status": "ok",
        "seed": seed_path,
        "variants": len(variants),
        "compiled": compiled_count,
        "timeouts": timeout_count,
        "unsafe": unsafe_count,
        "miscompiles": miscompiles,
    }


# ---------------------------------------------------------------------------
# Stats
# ---------------------------------------------------------------------------

class Stats:
    def __init__(self):
        self.seeds_tried = 0
        self.seeds_skipped = 0
        self.variants_generated = 0
        self.variants_compiled = 0
        self.timeouts = 0
        self.unsafe = 0
        self.o1_bugs = 0
        self.o2_bugs = 0
        self.start = time.time()

    def total_bugs(self):
        return self.o1_bugs + self.o2_bugs

    def elapsed(self):
        s = int(time.time() - self.start)
        return f"{s // 3600}h {(s % 3600) // 60}m {s % 60}s"

    def rate(self):
        elapsed = time.time() - self.start
        if elapsed < 1:
            return "—"
        return f"{self.variants_compiled / elapsed:.1f} variants/s"


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(
        description="Nitpick EMI Mass Fuzzer — 100K+ variants at -O0/-O1/-O2")
    parser.add_argument("--compiler", default=None)
    parser.add_argument("--seeds", default=None,
                        help="Seed directory (default: tools/emi/corpus/)")
    parser.add_argument("--seed-list", default=None,
                        help="Priority seed list file (from coverage_runner.py)")
    parser.add_argument("--variants", type=int, default=160,
                        help="Variants per seed (default: 160 → 696×160 = 111,360)")
    parser.add_argument("--workers", type=int,
                        default=max(1, os.cpu_count() // 2))
    parser.add_argument("--out", default=None)
    parser.add_argument("--seed-limit", type=int, default=0,
                        help="Only process first N seeds (0=all)")
    parser.add_argument("--hours", type=float, default=0,
                        help="Wall-clock time limit in hours (0=unlimited)")
    parser.add_argument("--compile-timeout", type=int, default=15)
    parser.add_argument("--run-timeout", type=int, default=5)
    parser.add_argument("--rng-seed", type=int, default=1337)
    args = parser.parse_args()

    script_dir = Path(__file__).parent
    repo_root = script_dir.parent.parent

    compiler = Path(args.compiler) if args.compiler else repo_root / "build" / "npkc"
    if not compiler.exists():
        print(f"ERROR: compiler not found at {compiler}", file=sys.stderr)
        sys.exit(1)

    out_dir = Path(args.out) if args.out else script_dir / "results"
    mc_dir = out_dir / "miscompiles"
    mc_dir.mkdir(parents=True, exist_ok=True)

    rng = random.Random(args.rng_seed)

    # Collect seeds
    if args.seed_list:
        seed_list_path = Path(args.seed_list)
        seeds = [
            Path(line.strip())
            for line in seed_list_path.read_text().splitlines()
            if line.strip() and Path(line.strip()).exists()
        ]
        seed_source = str(seed_list_path)
    else:
        seeds_dir = Path(args.seeds) if args.seeds else script_dir / "corpus"
        seeds = sorted(seeds_dir.glob("*.npk")) + sorted(seeds_dir.glob("*.aria"))
        rng.shuffle(seeds)
        seed_source = str(seeds_dir)

    if args.seed_limit:
        seeds = seeds[:args.seed_limit]

    total_expected = len(seeds) * args.variants
    print(f"Nitpick EMI Mass Fuzzer v0.18.1")
    print(f"Compiler  : {compiler}")
    print(f"Seeds     : {len(seeds)} files from {seed_source}")
    print(f"Variants  : {args.variants} per seed  ({total_expected:,} total)")
    print(f"Opt levels: -O0 (baseline), -O1, -O2")
    print(f"Workers   : {args.workers}")
    print(f"Out       : {out_dir}")
    print(f"RNG seed  : {args.rng_seed}")
    print("-" * 60)

    stats = Stats()
    deadline = time.time() + args.hours * 3600 if args.hours > 0 else float("inf")

    work_items = [
        (str(s), str(compiler), args.variants,
         rng.randint(0, 2**32),
         args.compile_timeout, args.run_timeout)
        for s in seeds
    ]

    all_miscompiles = []

    with ProcessPoolExecutor(max_workers=args.workers) as pool:
        futures = {pool.submit(_process_seed_multiopt, item): item
                   for item in work_items}

        for future in as_completed(futures):
            if time.time() > deadline:
                pool.shutdown(wait=False, cancel_futures=True)
                print("\n[time limit reached]")
                break

            try:
                result = future.result()
            except Exception as exc:
                print(f"  [worker error] {exc}")
                continue

            stats.seeds_tried += 1

            if result["status"] == "skipped":
                stats.seeds_skipped += 1
                continue

            stats.variants_generated += result["variants"]
            stats.variants_compiled  += result["compiled"]
            stats.timeouts           += result["timeouts"]
            stats.unsafe             += result["unsafe"]

            for mc in result["miscompiles"]:
                kind = mc["kind"]
                if kind.startswith("O1"):
                    stats.o1_bugs += 1
                elif kind.startswith("O2"):
                    stats.o2_bugs += 1

                mc_num = stats.total_bugs()
                all_miscompiles.append(mc)

                # Save variant source for reproduction + minimization
                src_file = mc_dir / f"mc_{mc_num:04d}_{kind}.npk"
                src_file.write_text(mc["variant_source"], encoding="utf-8")

                report = {k: v for k, v in mc.items() if k != "variant_source"}
                report["source_file"] = str(src_file)
                (mc_dir / f"mc_{mc_num:04d}_{kind}.json").write_text(
                    json.dumps(report, indent=2), encoding="utf-8")

                print(f"\n*** BUG #{mc_num} — {kind} ({mc['opt']}) ***")
                print(f"  Seed      : {mc['seed_path']}")
                print(f"  Mutations : {[m['kind'] for m in mc['mutations']]}")
                print(f"  -O0  exit={mc['o0_exit']}  "
                      f"stdout={repr(mc['o0_stdout'][:80])}")
                print(f"  {mc['opt']} exit={mc['opt_exit']}  "
                      f"stdout={repr(mc['opt_stdout'][:80])}")
                print(f"  Saved : {src_file}")

            if stats.seeds_tried % 10 == 0 or result["miscompiles"]:
                print(
                    f"  [{stats.elapsed()}] "
                    f"seeds={stats.seeds_tried}/{len(seeds)} "
                    f"variants={stats.variants_compiled:,} "
                    f"bugs=O1:{stats.o1_bugs}/O2:{stats.o2_bugs} "
                    f"unsafe={stats.unsafe} "
                    f"rate={stats.rate()}"
                )

    # Final summary
    print("\n" + "=" * 60)
    print("MASS FUZZING COMPLETE")
    print("=" * 60)
    print(f"Seeds tried        : {stats.seeds_tried}")
    print(f"Seeds skipped      : {stats.seeds_skipped}  (didn't compile at -O0)")
    print(f"Variants generated : {stats.variants_generated:,}")
    print(f"Variants compiled  : {stats.variants_compiled:,}")
    print(f"Timeouts           : {stats.timeouts}")
    print(f"Unsafe mutations   : {stats.unsafe}  (mutation accidentally changed semantics)")
    print(f"BUGS FOUND — -O1   : {stats.o1_bugs}")
    print(f"BUGS FOUND — -O2   : {stats.o2_bugs}")
    print(f"TOTAL BUGS         : {stats.total_bugs()}")
    print(f"Elapsed            : {stats.elapsed()}")

    summary = {
        "compiler": str(compiler),
        "seeds_tried": stats.seeds_tried,
        "seeds_skipped": stats.seeds_skipped,
        "variants_generated": stats.variants_generated,
        "variants_compiled": stats.variants_compiled,
        "timeouts": stats.timeouts,
        "unsafe_mutations": stats.unsafe,
        "o1_bugs": stats.o1_bugs,
        "o2_bugs": stats.o2_bugs,
        "total_bugs": stats.total_bugs(),
        "elapsed_seconds": int(time.time() - stats.start),
        "opt_levels": ["-O0", "-O1", "-O2"],
    }
    summary_path = out_dir / "mass_fuzzer_summary.json"
    summary_path.write_text(json.dumps(summary, indent=2), encoding="utf-8")
    print(f"Summary            : {summary_path}")

    if stats.total_bugs() > 0:
        print(f"\n!!! {stats.total_bugs()} BUG(S) FOUND — see {mc_dir}")
        print(f"    Run minimizer.py on each .npk to get a minimal reproducer.")
        sys.exit(1)
    else:
        print("\nAll variants produced identical results across -O0/-O1/-O2.")
        sys.exit(0)


if __name__ == "__main__":
    main()
