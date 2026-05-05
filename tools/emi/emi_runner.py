#!/usr/bin/env python3
"""
EMI Campaign Runner for Nitpick v0.18.1

Usage:
    python3 emi_runner.py [--compiler PATH] [--seeds DIR] [--variants N]
                          [--hours H] [--workers N] [--out DIR]

Finds and saves any miscompiles in --out/miscompiles/.
Prints a live summary as it runs.
"""

import argparse
import json
import os
import random
import sys
import time
from concurrent.futures import ProcessPoolExecutor, as_completed
from dataclasses import dataclass, asdict
from pathlib import Path
from typing import List, Optional

# Add tools/emi to path for sibling imports
sys.path.insert(0, str(Path(__file__).parent))
from mutator import generate_variants, Variant
from harness import compile_and_run, RunResult


# ---------------------------------------------------------------------------
# Data structures
# ---------------------------------------------------------------------------

@dataclass
class Miscompile:
    seed_path: str
    variant_index: int
    mutations: List[dict]
    seed_exit: Optional[int]
    seed_stdout: str
    variant_exit: Optional[int]
    variant_stdout: str
    variant_source: str


@dataclass
class Stats:
    seeds_tried: int = 0
    seeds_skipped: int = 0   # baseline didn't compile
    variants_generated: int = 0
    variants_compiled: int = 0
    variants_timeout: int = 0
    miscompiles: int = 0
    start_time: float = 0.0

    def elapsed(self) -> str:
        s = int(time.time() - self.start_time)
        return f"{s // 3600}h {(s % 3600) // 60}m {s % 60}s"

    def rate(self) -> str:
        elapsed = time.time() - self.start_time
        if elapsed < 1:
            return "—"
        return f"{self.variants_generated / elapsed:.1f} variants/s"


# ---------------------------------------------------------------------------
# Per-seed worker (runs in subprocess via ProcessPoolExecutor)
# ---------------------------------------------------------------------------

def _process_seed(args_tuple) -> dict:
    """Process one seed: compute baseline, generate variants, compare."""
    seed_path, compiler_path, n_variants, rng_seed, compile_timeout, run_timeout = args_tuple

    from mutator import generate_variants
    from harness import compile_and_run, RunResult

    compiler = Path(compiler_path)
    seed = Path(seed_path)
    rng = random.Random(rng_seed)

    source = seed.read_text(encoding="utf-8")

    # Baseline
    baseline_result = compile_and_run(
        source, compiler,
        compile_timeout=compile_timeout,
        run_timeout=run_timeout,
    )

    if not baseline_result.compiled or baseline_result.timed_out:
        return {"status": "skipped", "seed": seed_path, "miscompiles": []}

    # Generate variants
    variants = generate_variants(source, seed_path, n_variants, rng)

    miscompiles = []
    compiled_count = 0
    timeout_count = 0

    for idx, variant in enumerate(variants):
        result = compile_and_run(
            variant.source, compiler,
            compile_timeout=compile_timeout,
            run_timeout=run_timeout,
        )

        if result.compiled:
            compiled_count += 1
        if result.timed_out:
            timeout_count += 1

        if not baseline_result.matches(result):
            mc = {
                "seed_path": seed_path,
                "variant_index": idx,
                "mutations": [{"kind": m.kind.name, "description": m.description}
                               for m in variant.mutations],
                "seed_exit": baseline_result.exit_code,
                "seed_stdout": baseline_result.stdout,
                "variant_exit": result.exit_code,
                "variant_stdout": result.stdout,
                "variant_source": variant.source,
            }
            miscompiles.append(mc)

    return {
        "status": "ok",
        "seed": seed_path,
        "variants": len(variants),
        "compiled": compiled_count,
        "timeouts": timeout_count,
        "miscompiles": miscompiles,
    }


# ---------------------------------------------------------------------------
# Seed discovery
# ---------------------------------------------------------------------------

def _collect_seeds(seeds_dir: Path, limit: Optional[int] = None) -> List[Path]:
    """
    Collect .npk (and legacy .aria) test files from seeds_dir recursively.
    Excludes adversarial/, fuzz/, and files known to be negative tests.
    """
    SKIP_DIRS = {"adversarial", "fuzz", "v2", "__pycache__"}
    SKIP_NAME_PATTERNS = ["failsafe_only", "crash", "fuzz_"]

    seeds = []
    for p in sorted(seeds_dir.rglob("*.npk")) + sorted(seeds_dir.rglob("*.aria")):
        if any(part in SKIP_DIRS for part in p.parts):
            continue
        if any(pat in p.name for pat in SKIP_NAME_PATTERNS):
            continue
        seeds.append(p)

    if limit:
        seeds = seeds[:limit]
    return seeds


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(description="Nitpick EMI miscompile finder")
    parser.add_argument("--compiler", default=None,
                        help="Path to npkc compiler binary (auto-detected if omitted)")
    parser.add_argument("--seeds", default=None,
                        help="Directory to scan for seed programs (default: tests/)")
    parser.add_argument("--variants", type=int, default=20,
                        help="Variants to generate per seed (default: 20)")
    parser.add_argument("--hours", type=float, default=0,
                        help="Stop after N hours (0 = run until all seeds exhausted)")
    parser.add_argument("--workers", type=int, default=max(1, os.cpu_count() // 2),
                        help="Parallel workers (default: half of CPU cores)")
    parser.add_argument("--out", default=None,
                        help="Output directory for miscompile reports")
    parser.add_argument("--seed-limit", type=int, default=0,
                        help="Max seeds to process (0 = all)")
    parser.add_argument("--compile-timeout", type=int, default=15)
    parser.add_argument("--run-timeout", type=int, default=5)
    parser.add_argument("--rng-seed", type=int, default=42,
                        help="Master RNG seed for reproducibility")
    args = parser.parse_args()

    # --- Resolve paths ---
    script_dir = Path(__file__).parent
    repo_root = script_dir.parent.parent

    compiler = Path(args.compiler) if args.compiler else repo_root / "build" / "npkc"
    if not compiler.exists():
        # fallback
        compiler = repo_root / "build" / "ariac"
    if not compiler.exists():
        print(f"ERROR: compiler not found. Build first: cd {repo_root} && "
              f"cd build && cmake .. && make -j$(nproc)", file=sys.stderr)
        sys.exit(1)

    seeds_dir = Path(args.seeds) if args.seeds else repo_root / "tests"
    out_dir = Path(args.out) if args.out else script_dir / "results"
    miscompile_dir = out_dir / "miscompiles"
    miscompile_dir.mkdir(parents=True, exist_ok=True)

    seeds = _collect_seeds(seeds_dir, limit=args.seed_limit or None)
    if not seeds:
        print(f"ERROR: no seed files found under {seeds_dir}", file=sys.stderr)
        sys.exit(1)

    # Shuffle deterministically
    rng = random.Random(args.rng_seed)
    rng.shuffle(seeds)

    stats = Stats(start_time=time.time())
    deadline = time.time() + args.hours * 3600 if args.hours > 0 else float("inf")

    print(f"Nitpick EMI v0.18.1")
    print(f"Compiler : {compiler}")
    print(f"Seeds    : {len(seeds)} files under {seeds_dir}")
    print(f"Variants : {args.variants} per seed")
    print(f"Workers  : {args.workers}")
    print(f"Out      : {out_dir}")
    print(f"RNG seed : {args.rng_seed}")
    print("-" * 60)

    all_miscompiles = []
    work_items = [
        (str(s), str(compiler), args.variants,
         rng.randint(0, 2**32), args.compile_timeout, args.run_timeout)
        for s in seeds
    ]

    with ProcessPoolExecutor(max_workers=args.workers) as pool:
        futures = {pool.submit(_process_seed, item): item for item in work_items}

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
            stats.variants_timeout   += result["timeouts"]

            for mc in result["miscompiles"]:
                stats.miscompiles += 1
                all_miscompiles.append(mc)

                # Save variant source for reproduction
                mc_file = miscompile_dir / f"mc_{stats.miscompiles:04d}.npk"
                mc_file.write_text(mc["variant_source"], encoding="utf-8")

                report_file = miscompile_dir / f"mc_{stats.miscompiles:04d}.json"
                mc_copy = dict(mc)
                mc_copy.pop("variant_source")  # source saved separately
                report_file.write_text(json.dumps(mc_copy, indent=2), encoding="utf-8")

                print(f"\n*** MISCOMPILE #{stats.miscompiles} ***")
                print(f"  Seed    : {mc['seed_path']}")
                print(f"  Mutations: {[m['kind'] for m in mc['mutations']]}")
                print(f"  Seed exit={mc['seed_exit']}  stdout={repr(mc['seed_stdout'][:80])}")
                print(f"  Var  exit={mc['variant_exit']}  stdout={repr(mc['variant_stdout'][:80])}")
                print(f"  Saved : {mc_file}")

            # Progress line
            if stats.seeds_tried % 10 == 0 or result["miscompiles"]:
                print(
                    f"  [{stats.elapsed()}] seeds={stats.seeds_tried}/{len(seeds)} "
                    f"variants={stats.variants_generated} "
                    f"compiled={stats.variants_compiled} "
                    f"miscompiles={stats.miscompiles} "
                    f"rate={stats.rate()}"
                )

    # --- Final report ---
    print("\n" + "=" * 60)
    print("EMI CAMPAIGN COMPLETE")
    print("=" * 60)
    print(f"Seeds tried       : {stats.seeds_tried}")
    print(f"Seeds skipped     : {stats.seeds_skipped} (baseline didn't compile)")
    print(f"Variants generated: {stats.variants_generated}")
    print(f"Variants compiled : {stats.variants_compiled}")
    print(f"Timeouts          : {stats.variants_timeout}")
    print(f"MISCOMPILES FOUND : {stats.miscompiles}")
    print(f"Elapsed           : {stats.elapsed()}")
    print(f"Results           : {out_dir}")

    summary_path = out_dir / "emi_summary.json"
    summary = {
        "compiler": str(compiler),
        "seeds_tried": stats.seeds_tried,
        "seeds_skipped": stats.seeds_skipped,
        "variants_generated": stats.variants_generated,
        "variants_compiled": stats.variants_compiled,
        "variants_timeout": stats.variants_timeout,
        "miscompiles": stats.miscompiles,
        "elapsed_seconds": int(time.time() - stats.start_time),
    }
    summary_path.write_text(json.dumps(summary, indent=2), encoding="utf-8")

    if stats.miscompiles > 0:
        print(f"\n!!! {stats.miscompiles} MISCOMPILE(S) FOUND — see {miscompile_dir}")
        sys.exit(1)
    else:
        print("\nAll variants produced identical results — no miscompiles found.")
        sys.exit(0)


if __name__ == "__main__":
    main()
