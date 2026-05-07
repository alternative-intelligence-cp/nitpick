#!/usr/bin/env python3
"""
Corpus builder for EMI testing.

Scans tests/ for .npk/.aria files, compiles each with npkc, runs the binary,
and copies any that (a) compile cleanly and (b) exit 0 into tools/emi/corpus/.

Usage:
    python3 tools/emi/build_corpus.py [--jobs N] [--timeout-compile N] [--timeout-run N]

Output:
    tools/emi/corpus/          — seed .npk files (renamed for uniqueness)
    tools/emi/corpus_build.json — full report (pass/fail/skip per source file)
"""

import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
from concurrent.futures import ProcessPoolExecutor, as_completed
from pathlib import Path


SKIP_DIRS   = {"adversarial", "fuzz", "v2", "__pycache__", "emi"}
SKIP_NAMES  = {"fuzz_", "crash_", "failsafe_only"}
EXTENSIONS  = {".npk", ".aria"}


def _slug(src: Path, root: Path) -> str:
    """Turn tests/stdlib/test_foo.aria → stdlib__test_foo"""
    rel = src.relative_to(root)
    parts = list(rel.parts)
    stem = parts[-1].rsplit(".", 1)[0]
    prefix = "__".join(parts[:-1]) if len(parts) > 1 else ""
    slug = f"{prefix}__{stem}" if prefix else stem
    # sanitize
    slug = re.sub(r"[^a-zA-Z0-9_]", "_", slug)
    return slug


def _try_seed(args_tuple):
    src_path, compiler_path, compile_timeout, run_timeout = args_tuple
    compiler = Path(compiler_path)
    src = Path(src_path)

    with tempfile.TemporaryDirectory(prefix="corpus_") as tmp:
        bin_path = Path(tmp) / "out"
        try:
            cp = subprocess.run(
                [str(compiler), str(src), "-o", str(bin_path)],
                capture_output=True, text=True, timeout=compile_timeout,
            )
        except subprocess.TimeoutExpired:
            return {"path": src_path, "status": "compile_timeout"}

        if cp.returncode != 0 or not bin_path.exists():
            return {"path": src_path, "status": "compile_fail",
                    "stderr": cp.stderr[:300]}

        try:
            rp = subprocess.run(
                [str(bin_path)],
                capture_output=True, text=True, timeout=run_timeout,
            )
        except subprocess.TimeoutExpired:
            return {"path": src_path, "status": "run_timeout"}

        if rp.returncode != 0:
            return {"path": src_path, "status": "nonzero_exit",
                    "exit_code": rp.returncode}

        return {"path": src_path, "status": "ok",
                "stdout": rp.stdout[:200]}


def main():
    parser = argparse.ArgumentParser(description="Build EMI seed corpus")
    parser.add_argument("--jobs", type=int, default=max(1, os.cpu_count() // 2))
    parser.add_argument("--timeout-compile", type=int, default=15)
    parser.add_argument("--timeout-run",     type=int, default=5)
    parser.add_argument("--compiler", default=None)
    parser.add_argument("--tests-dir", default=None)
    parser.add_argument("--corpus-dir", default=None)
    args = parser.parse_args()

    script_dir = Path(__file__).parent
    repo_root  = script_dir.parent.parent

    compiler = Path(args.compiler) if args.compiler else repo_root / "build" / "npkc"
    if not compiler.exists():
        compiler = repo_root / "build" / "ariac"
    if not compiler.exists():
        print(f"ERROR: compiler not found under {repo_root}/build/", file=sys.stderr)
        sys.exit(1)

    tests_dir  = Path(args.tests_dir)  if args.tests_dir  else repo_root / "tests"
    corpus_dir = Path(args.corpus_dir) if args.corpus_dir else script_dir / "corpus"
    corpus_dir.mkdir(parents=True, exist_ok=True)

    # Collect candidates
    candidates = []
    for p in sorted(tests_dir.rglob("*")):
        if p.suffix not in EXTENSIONS:
            continue
        if any(part in SKIP_DIRS for part in p.parts):
            continue
        if any(p.name.startswith(s) for s in SKIP_NAMES):
            continue
        candidates.append(p)

    print(f"Compiler : {compiler}")
    print(f"Tests    : {tests_dir}  ({len(candidates)} candidates)")
    print(f"Corpus   : {corpus_dir}")
    print(f"Jobs     : {args.jobs}")
    print("-" * 60)

    work = [(str(p), str(compiler), args.timeout_compile, args.timeout_run)
            for p in candidates]

    results = []
    ok = fail_compile = fail_run = timeouts = nonzero = 0

    with ProcessPoolExecutor(max_workers=args.jobs) as pool:
        futs = {pool.submit(_try_seed, item): item for item in work}
        for i, fut in enumerate(as_completed(futs), 1):
            try:
                r = fut.result()
            except Exception as exc:
                r = {"path": str(futs[fut][0]), "status": "exception", "err": str(exc)}
            results.append(r)

            s = r["status"]
            if s == "ok":
                ok += 1
                src = Path(r["path"])
                slug = _slug(src, tests_dir)
                dest = corpus_dir / f"{slug}.npk"
                # avoid clobbering if slug collision
                if dest.exists():
                    dest = corpus_dir / f"{slug}_{ok}.npk"
                shutil.copy2(src, dest)
            elif s in ("compile_timeout", "run_timeout"):
                timeouts += 1
            elif s == "compile_fail":
                fail_compile += 1
            elif s == "nonzero_exit":
                nonzero += 1
            else:
                fail_run += 1

            if i % 50 == 0 or i == len(candidates):
                pct = 100 * i // len(candidates)
                print(f"  [{pct:3d}%] {i}/{len(candidates)}  "
                      f"seeds={ok}  compile_fail={fail_compile}  "
                      f"nonzero={nonzero}  timeout={timeouts}")

    # Save report
    report_path = script_dir / "corpus_build.json"
    report_path.write_text(json.dumps({
        "compiler": str(compiler),
        "total": len(candidates),
        "seeds_added": ok,
        "compile_fail": fail_compile,
        "nonzero_exit": nonzero,
        "timeouts": timeouts,
        "other": fail_run,
        "results": results,
    }, indent=2), encoding="utf-8")

    print()
    print("=" * 60)
    print(f"DONE  —  {ok} seeds added to {corpus_dir}")
    print(f"  compile fail : {fail_compile}")
    print(f"  nonzero exit : {nonzero}")
    print(f"  timeouts     : {timeouts}")
    print(f"  other        : {fail_run}")
    print(f"  report       : {report_path}")


if __name__ == "__main__":
    main()
