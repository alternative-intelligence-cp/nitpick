#!/usr/bin/env python3
"""
Nitpick EMI Minimizer — Phase 4

Delta-debugs a miscompile .npk file to find the minimal source that still
triggers the optimization bug.

Algorithm:
  1. Verify the input still reproduces the miscompile (opt != -O0 output).
  2. Try removing each line. Keep the removal if miscompile still reproduces.
  3. Try removing whole blocks (brace-delimited).
  4. Repeat passes until no progress.
  5. Print the minimal source.

Usage:
    python3 minimizer.py miscompile.npk --opt -O1
    python3 minimizer.py miscompile.npk --opt -O2 --compiler ../../build/npkc
    python3 minimizer.py results/miscompiles/mc_0001_O1_BUG.npk --opt -O1

The minimized source is written to <input>_min.npk.
"""

import argparse
import subprocess
import sys
import tempfile
from pathlib import Path


# ---------------------------------------------------------------------------
# Core: compile + run, return (exit_code, stdout) or (None, "") on fail
# ---------------------------------------------------------------------------

def _run(source: str, compiler: Path, opt: str,
         compile_timeout: int = 15, run_timeout: int = 5):
    """Compile source at `opt` and return (exit_code, stdout). Returns (None, "") on failure."""
    with tempfile.TemporaryDirectory(prefix="min_") as tmpdir:
        src = Path(tmpdir) / "min.npk"
        exe = Path(tmpdir) / "min_bin"
        src.write_text(source, encoding="utf-8")
        try:
            cp = subprocess.run(
                [str(compiler), str(src), opt, "-o", str(exe)],
                capture_output=True, text=True, timeout=compile_timeout)
        except subprocess.TimeoutExpired:
            return (None, "")
        if cp.returncode != 0 or not exe.exists():
            return (None, "")
        try:
            rp = subprocess.run(
                [str(exe)], capture_output=True, text=True, timeout=run_timeout)
            return (rp.returncode, rp.stdout)
        except subprocess.TimeoutExpired:
            return (None, "")


def _run_baseline(source: str, compiler: Path,
                  compile_timeout: int = 15, run_timeout: int = 5):
    """Compile source at -O0 and return (exit_code, stdout)."""
    return _run(source, compiler, "-O0", compile_timeout, run_timeout)


# ---------------------------------------------------------------------------
# Check: does `source` still reproduce the miscompile?
# ---------------------------------------------------------------------------

def still_bugs(source: str, compiler: Path, opt: str,
               ref_exit: int, ref_stdout: str) -> bool:
    """
    Returns True if source compiled at `opt` produces output different from
    (ref_exit, ref_stdout) — i.e. the miscompile is still present.
    ref_exit/ref_stdout are the -O0 output of the variant.
    """
    # First confirm -O0 result matches ref (mutation is still safe)
    o0_exit, o0_stdout = _run_baseline(source, compiler)
    if o0_exit is None:
        return False  # Doesn't compile at -O0 anymore

    # Get opt result
    opt_exit, opt_stdout = _run(source, compiler, opt)
    if opt_exit is None:
        # Compile failure at opt = still a bug (compiler crash on higher opt)
        return True

    # Bug still present if outputs differ
    return (opt_exit != o0_exit) or (opt_stdout != o0_stdout)


# ---------------------------------------------------------------------------
# Reduction passes
# ---------------------------------------------------------------------------

def _try_remove_lines(source: str, compiler: Path, opt: str,
                      ref_exit: int, ref_stdout: str) -> tuple[str, int]:
    """
    One pass: try removing each line. Return (reduced_source, n_removed).
    """
    lines = source.split('\n')
    removed = 0
    i = 0
    while i < len(lines):
        candidate = '\n'.join(lines[:i] + lines[i+1:])
        if still_bugs(candidate, compiler, opt, ref_exit, ref_stdout):
            lines = lines[:i] + lines[i+1:]
            removed += 1
        else:
            i += 1
    return '\n'.join(lines), removed


def _try_remove_blocks(source: str, compiler: Path, opt: str,
                       ref_exit: int, ref_stdout: str) -> tuple[str, int]:
    """
    One pass: try removing matched brace blocks (one at a time, innermost first).
    Finds lines that contain '{' without '}', and the matching '}' line,
    and tries removing the whole range.
    """
    lines = source.split('\n')
    removed = 0
    i = 0
    while i < len(lines):
        # Look for a line that opens a block
        stripped = lines[i].strip()
        if '{' in stripped and stripped.endswith('{'):
            # Find the matching closing brace
            depth = 0
            j = i
            for j in range(i, len(lines)):
                depth += lines[j].count('{') - lines[j].count('}')
                if depth == 0:
                    break
            # Try removing lines[i:j+1]
            if j > i:
                candidate = '\n'.join(lines[:i] + lines[j+1:])
                if still_bugs(candidate, compiler, opt, ref_exit, ref_stdout):
                    lines = lines[:i] + lines[j+1:]
                    removed += 1
                    continue
        i += 1
    return '\n'.join(lines), removed


def _try_simplify_expressions(source: str, compiler: Path, opt: str,
                               ref_exit: int, ref_stdout: str) -> tuple[str, int]:
    """
    Try replacing complex expressions in variable declarations with simple literals.
    Conservative: only simplify lines with patterns like `int32:x = COMPLEX;`
    where COMPLEX is not already a literal.
    """
    import re
    lines = source.split('\n')
    simplified = 0

    for i, line in enumerate(lines):
        # Only touch mutable int32/int64 declarations with non-literal rvalues
        m = re.match(r'^(\s*)(int32|int64):(\w+)\s*=\s*(.+);$', line)
        if not m:
            continue
        rvalue = m.group(4).strip()
        if re.fullmatch(r'\d+', rvalue):
            continue  # already a literal
        # Try replacing with 0
        candidate_line = f"{m.group(1)}{m.group(2)}:{m.group(3)} = 0;"
        candidate = '\n'.join(lines[:i] + [candidate_line] + lines[i+1:])
        if still_bugs(candidate, compiler, opt, ref_exit, ref_stdout):
            lines[i] = candidate_line
            simplified += 1

    return '\n'.join(lines), simplified


# ---------------------------------------------------------------------------
# Main reduction loop
# ---------------------------------------------------------------------------

def minimize(source: str, compiler: Path, opt: str,
             verbose: bool = True) -> str:
    """
    Run all reduction passes until convergence.
    Returns the minimized source.
    """
    # Get the -O0 reference output for the current source
    ref_exit, ref_stdout = _run_baseline(source, compiler)
    if ref_exit is None:
        print("ERROR: source doesn't compile at -O0. Cannot minimize.", file=sys.stderr)
        return source

    # Verify bug is present before starting
    if not still_bugs(source, compiler, opt, ref_exit, ref_stdout):
        print("WARNING: miscompile not reproduced on current source/compiler.",
              file=sys.stderr)
        return source

    if verbose:
        n_lines = source.count('\n')
        print(f"Starting minimization: {n_lines} lines, -O0 exit={ref_exit}")

    pass_num = 0
    while True:
        pass_num += 1
        progress = 0

        # Update reference after each change (so ref tracks the reduced program)
        ref_exit, ref_stdout = _run_baseline(source, compiler)

        source, n = _try_remove_lines(source, compiler, opt, ref_exit, ref_stdout)
        progress += n
        if verbose and n:
            print(f"  Pass {pass_num}a: removed {n} lines  ({source.count(chr(10))} remain)")

        ref_exit, ref_stdout = _run_baseline(source, compiler)
        source, n = _try_remove_blocks(source, compiler, opt, ref_exit, ref_stdout)
        progress += n
        if verbose and n:
            print(f"  Pass {pass_num}b: removed {n} blocks ({source.count(chr(10))} remain)")

        ref_exit, ref_stdout = _run_baseline(source, compiler)
        source, n = _try_simplify_expressions(source, compiler, opt, ref_exit, ref_stdout)
        progress += n
        if verbose and n:
            print(f"  Pass {pass_num}c: simplified {n} exprs ({source.count(chr(10))} remain)")

        if progress == 0:
            break  # Converged

    if verbose:
        print(f"Minimization complete: {source.count(chr(10))} lines remain.")

    return source


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(description="Nitpick EMI minimizer")
    parser.add_argument("input", help="Miscompile .npk file to minimize")
    parser.add_argument("--opt", required=True,
                        help="Optimization level that triggered the bug (-O1 or -O2)")
    parser.add_argument("--compiler", default=None)
    parser.add_argument("--out", default=None,
                        help="Output path for minimized file (default: <input>_min.npk)")
    parser.add_argument("--quiet", action="store_true")
    args = parser.parse_args()

    script_dir = Path(__file__).parent
    repo_root = script_dir.parent.parent
    compiler = Path(args.compiler) if args.compiler else repo_root / "build" / "npkc"

    if not compiler.exists():
        print(f"ERROR: compiler not found at {compiler}", file=sys.stderr)
        sys.exit(1)

    input_path = Path(args.input)
    if not input_path.exists():
        print(f"ERROR: {input_path} not found", file=sys.stderr)
        sys.exit(1)

    source = input_path.read_text(encoding="utf-8")
    out_path = Path(args.out) if args.out else input_path.with_suffix("").with_name(
        input_path.stem + "_min.npk")

    minimized = minimize(source, compiler, args.opt, verbose=not args.quiet)
    out_path.write_text(minimized, encoding="utf-8")

    if not args.quiet:
        print(f"\nMinimized source written to: {out_path}")
        print("\n--- Minimized source ---")
        print(minimized)

    sys.exit(0)


if __name__ == "__main__":
    main()
