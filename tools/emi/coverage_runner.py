#!/usr/bin/env python3
"""
Coverage-Guided Seed Prioritization for EMI v0.18.1

Runs each corpus seed through a Clang source-based coverage build of npkc
and measures which new compiler source lines each seed exercises.
Seeds are ranked by "new coverage contribution" (greedy set-cover order)
so the EMI campaign focuses mutations on programs that drive the compiler
into under-exercised paths.

Coverage pipeline (per seed):
    LLVM_PROFILE_FILE=seed.profraw  npkc seed.npk -o /dev/null
    llvm-profdata-18 merge -sparse seed.profraw -o seed.profdata
    llvm-cov-18 export npkc -instr-profile=seed.profdata  -> JSON segments

Usage:
    python3 coverage_runner.py [options]

Options:
    --compiler PATH     Path to source-based coverage npkc  [default: ../../build-sbcov/npkc]
    --seeds DIR         Corpus directory                    [default: corpus/]
    --out FILE          Output JSON report path             [default: coverage_report.json]
    --priority-out FILE Output ranked seed list             [default: priority_seeds.txt]
    --verbose           Print per-seed progress

Outputs:
    coverage_report.json   -- per-seed stats + global summary
    priority_seeds.txt     -- newline-separated seed paths, best-coverage-first

Build the instrumented compiler with:
    mkdir build-sbcov && cd build-sbcov
    CXX=clang++ cmake .. -DCMAKE_BUILD_TYPE=Debug \\
        -DCMAKE_CXX_FLAGS="-O0 -fprofile-instr-generate -fcoverage-mapping" \\
        -DCMAKE_EXE_LINKER_FLAGS="-fprofile-instr-generate" -DUSE_COVERAGE=OFF
    make -j$(nproc) npkc
"""

import argparse
import json
import os
import subprocess
import sys
import tempfile
import time
from pathlib import Path
from typing import Dict, FrozenSet, List, Set, Tuple

# Ensure output is not block-buffered when piped through tee
sys.stdout.reconfigure(line_buffering=True)  # type: ignore[union-attr]


# ---------------------------------------------------------------------------
# Source-based coverage helpers
# ---------------------------------------------------------------------------

def _compile_with_coverage(
    seed_path: Path,
    compiler: Path,
    profraw_path: Path,
    timeout: int = 20,
) -> bool:
    """
    Compile seed through the instrumented npkc, writing profile data to
    profraw_path.  Returns True if compilation succeeded (exit 0 + binary
    created).  We discard the output binary immediately to save disk space.
    """
    env = dict(os.environ)
    env["LLVM_PROFILE_FILE"] = str(profraw_path)
    with tempfile.TemporaryDirectory(prefix="cov_seed_") as tmpdir:
        bin_path = Path(tmpdir) / "out"
        try:
            proc = subprocess.run(
                [str(compiler), str(seed_path), "-o", str(bin_path)],
                capture_output=True,
                env=env,
                timeout=timeout,
            )
            return proc.returncode == 0 and bin_path.exists()
        except subprocess.TimeoutExpired:
            return False


def _merge_profile(profraw: Path, profdata: Path) -> bool:
    """Merge .profraw -> .profdata using llvm-profdata-18."""
    try:
        proc = subprocess.run(
            ["llvm-profdata-18", "merge", "-sparse", str(profraw), "-o", str(profdata)],
            capture_output=True,
            timeout=15,
        )
        return proc.returncode == 0 and profdata.exists()
    except (subprocess.TimeoutExpired, FileNotFoundError):
        return False


def _extract_hit_lines(
    compiler: Path,
    profdata: Path,
    src_filter: str,
) -> FrozenSet[Tuple[str, int]]:
    """
    Run llvm-cov-18 export and return frozenset of (abs_path, line_no)
    pairs that had count > 0.

    JSON segments format per file:
      segments: [[line, col, count, has_count, is_region_entry, is_gap], ...]
    We track line_no if count > 0 and has_count == True.
    Only includes .cpp files whose path contains src_filter.
    """
    try:
        proc = subprocess.run(
            [
                "llvm-cov-18", "export",
                str(compiler),
                f"-instr-profile={profdata}",
                "-format=text",
                f"-sources={src_filter}",
            ],
            capture_output=True,
            text=True,
            timeout=30,
        )
        if proc.returncode != 0 or not proc.stdout.strip():
            return frozenset()

        data = json.loads(proc.stdout)
    except (subprocess.TimeoutExpired, FileNotFoundError, json.JSONDecodeError):
        return frozenset()

    hit: Set[Tuple[str, int]] = set()
    for run_data in data.get("data", []):
        for file_entry in run_data.get("files", []):
            fname = file_entry.get("filename", "")
            # Only .cpp files in src/
            if not fname.endswith(".cpp"):
                continue
            if src_filter not in fname:
                continue
            for seg in file_entry.get("segments", []):
                # seg = [line, col, count, has_count, is_region_entry, is_gap_region]
                if len(seg) >= 4 and seg[3] and seg[2] > 0:
                    hit.add((fname, seg[0]))

    return frozenset(hit)


# ---------------------------------------------------------------------------
# Main analysis
# ---------------------------------------------------------------------------

def run_coverage_analysis(
    compiler: Path,
    seeds_dir: Path,
    src_filter: str,
    out_path: Path,
    priority_out_path: Path,
    verbose: bool = False,
) -> None:

    seed_files = sorted(seeds_dir.glob("*.npk"))
    if not seed_files:
        print(f"No .npk seeds found in {seeds_dir}", file=sys.stderr)
        sys.exit(1)

    print(f"Coverage analysis -- source-based (Clang/llvm-cov-18)")
    print(f"  Seeds     : {len(seed_files)}")
    print(f"  Compiler  : {compiler}")
    print(f"  Src filter: {src_filter}")
    print()

    global_covered: Set[Tuple[str, int]] = set()
    per_seed_stats: List[Dict] = []

    t_start = time.monotonic()

    with tempfile.TemporaryDirectory(prefix="cov_runner_") as tmpdir:
        profraw = Path(tmpdir) / "seed.profraw"
        profdata = Path(tmpdir) / "seed.profdata"

        for i, seed_path in enumerate(seed_files):
            # 1. Compile through instrumented npkc
            ok = _compile_with_coverage(seed_path, compiler, profraw)
            if not ok:
                if verbose:
                    print(f"  [{i+1:4d}/{len(seed_files)}] COMPILE_FAIL  {seed_path.name}")
                per_seed_stats.append({
                    "seed": str(seed_path),
                    "name": seed_path.name,
                    "compiled": False,
                    "total_lines_hit": 0,
                    "new_lines": 0,
                    "cumulative_coverage": len(global_covered),
                })
                continue

            # 2. Merge profile data
            if not _merge_profile(profraw, profdata):
                if verbose:
                    print(f"  [{i+1:4d}/{len(seed_files)}] MERGE_FAIL    {seed_path.name}")
                per_seed_stats.append({
                    "seed": str(seed_path),
                    "name": seed_path.name,
                    "compiled": True,
                    "total_lines_hit": 0,
                    "new_lines": 0,
                    "cumulative_coverage": len(global_covered),
                })
                continue

            # 3. Extract hit lines
            seed_cov = _extract_hit_lines(compiler, profdata, src_filter)
            new_lines = seed_cov - global_covered
            global_covered.update(seed_cov)

            if verbose:
                elapsed = time.monotonic() - t_start
                print(
                    f"  [{i+1:4d}/{len(seed_files)}] "
                    f"hit={len(seed_cov):6d}  new={len(new_lines):5d}  "
                    f"total={len(global_covered):6d}  {elapsed:6.1f}s  {seed_path.name}"
                )
            elif (i + 1) % 50 == 0 or i == 0:
                elapsed = time.monotonic() - t_start
                eta = elapsed / (i + 1) * (len(seed_files) - i - 1)
                print(
                    f"  [{i+1:4d}/{len(seed_files)}]  "
                    f"covered={len(global_covered):,}  "
                    f"elapsed={elapsed:.0f}s  eta={eta:.0f}s"
                )

            per_seed_stats.append({
                "seed": str(seed_path),
                "name": seed_path.name,
                "compiled": True,
                "total_lines_hit": len(seed_cov),
                "new_lines": len(new_lines),
                "cumulative_coverage": len(global_covered),
            })

    elapsed = time.monotonic() - t_start

    # --- Rank by new_lines (descending) ---
    ranked = sorted(
        [s for s in per_seed_stats if s["compiled"]],
        key=lambda s: s["new_lines"],
        reverse=True,
    )

    compiled_count = sum(1 for s in per_seed_stats if s["compiled"])
    summary = {
        "seeds_total": len(seed_files),
        "seeds_compiled": compiled_count,
        "seeds_compile_fail": len(seed_files) - compiled_count,
        "total_lines_covered": len(global_covered),
        "elapsed_sec": round(elapsed, 1),
        "ranked_seeds": ranked,
        "all_seeds": per_seed_stats,
    }

    out_path.write_text(json.dumps(summary, indent=2), encoding="utf-8")

    print()
    print(f"Coverage summary:")
    print(f"  Seeds analysed : {compiled_count}/{len(seed_files)}")
    print(f"  Total lines hit: {len(global_covered):,}")
    print(f"  Elapsed        : {elapsed:.1f}s")
    print(f"  Report         : {out_path}")

    # --- Write priority seed list ---
    with priority_out_path.open("w", encoding="utf-8") as f:
        for entry in ranked:
            f.write(entry["seed"] + "\n")
        for entry in per_seed_stats:
            if not entry["compiled"]:
                f.write(entry["seed"] + "\n")

    print(f"  Priority list  : {priority_out_path}")

    # --- Top 20 ---
    print()
    print("Top 20 seeds by new compiler paths covered:")
    print(f"  {'new_lines':>9}  {'total_hit':>9}  name")
    print(f"  {'-'*9}  {'-'*9}  {'-'*44}")
    for entry in ranked[:20]:
        print(f"  {entry['new_lines']:>9}  {entry['total_lines_hit']:>9}  {entry['name']}")


def main() -> None:
    here = Path(__file__).parent
    repo_root = (here / ".." / "..").resolve()

    parser = argparse.ArgumentParser(
        description="Coverage-guided EMI seed prioritization (Clang source-based)"
    )
    parser.add_argument(
        "--compiler",
        type=Path,
        default=repo_root / "build-sbcov" / "npkc",
        help="Path to source-based coverage npkc binary",
    )
    parser.add_argument(
        "--seeds",
        type=Path,
        default=here / "corpus",
        help="Corpus seed directory",
    )
    parser.add_argument(
        "--src-filter",
        default=str(repo_root / "src"),
        help="Only include coverage from files whose path contains this string",
    )
    parser.add_argument(
        "--out",
        type=Path,
        default=here / "coverage_report.json",
        help="Output JSON report path",
    )
    parser.add_argument(
        "--priority-out",
        type=Path,
        default=here / "priority_seeds.txt",
        help="Output ranked seed list",
    )
    parser.add_argument(
        "--verbose", "-v",
        action="store_true",
        help="Print every seed",
    )

    args = parser.parse_args()

    if not args.compiler.exists():
        print(
            f"ERROR: compiler not found: {args.compiler}\n"
            f"Build it:\n"
            f"  mkdir {repo_root}/build-sbcov && cd {repo_root}/build-sbcov\n"
            f"  CXX=clang++ cmake .. -DCMAKE_BUILD_TYPE=Debug \\\n"
            f"      -DCMAKE_CXX_FLAGS='-O0 -fprofile-instr-generate -fcoverage-mapping' \\\n"
            f"      -DCMAKE_EXE_LINKER_FLAGS='-fprofile-instr-generate' -DUSE_COVERAGE=OFF\n"
            f"  make -j$(nproc) npkc",
            file=sys.stderr,
        )
        sys.exit(1)

    if not args.seeds.is_dir():
        print(f"ERROR: seeds directory not found: {args.seeds}", file=sys.stderr)
        sys.exit(1)

    run_coverage_analysis(
        compiler=args.compiler,
        seeds_dir=args.seeds,
        src_filter=args.src_filter,
        out_path=args.out,
        priority_out_path=args.priority_out,
        verbose=args.verbose,
    )


if __name__ == "__main__":
    main()
