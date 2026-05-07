#!/usr/bin/env python3
"""
EMI Compile/Run Harness for Nitpick (npkc)

Compiles a Nitpick source file, runs the resulting binary, and returns the
(exit_code, stdout, stderr) triple. Used by the EMI engine to compare
seed output against variant output.
"""

import os
import subprocess
import tempfile
from dataclasses import dataclass
from pathlib import Path
from typing import Optional, Tuple


@dataclass
class RunResult:
    compiled: bool
    exit_code: Optional[int]   # None if compilation failed or timed out
    stdout: str
    stderr: str
    compile_stderr: str
    timed_out: bool = False

    def matches(self, other: "RunResult") -> bool:
        """
        Two results are EMI-equivalent if:
          - Both fail to compile (different compile errors are still 'same')
          - Both produce the same exit code AND stdout
        We intentionally ignore stderr from the binary (only stdout is observable).
        """
        if not self.compiled and not other.compiled:
            return True
        if self.compiled != other.compiled:
            return False
        if self.timed_out or other.timed_out:
            # Timeout on either side is inconclusive, not a bug
            return True
        return self.exit_code == other.exit_code and self.stdout == other.stdout


def compile_and_run(
    source: str,
    compiler: Path,
    compile_timeout: int = 15,
    run_timeout: int = 5,
    extra_flags: Optional[list] = None,
) -> RunResult:
    """
    Write `source` to a temp file, compile with npkc, run, return result.
    Cleans up all temp files regardless of outcome.
    """
    extra_flags = extra_flags or []

    with tempfile.TemporaryDirectory(prefix="emi_") as tmpdir:
        src_path = Path(tmpdir) / "variant.npk"
        bin_path = Path(tmpdir) / "variant_bin"

        src_path.write_text(source, encoding="utf-8")

        # --- Compilation ---
        try:
            compile_proc = subprocess.run(
                [str(compiler), str(src_path), "-o", str(bin_path)] + extra_flags,
                capture_output=True,
                text=True,
                timeout=compile_timeout,
            )
        except subprocess.TimeoutExpired:
            return RunResult(
                compiled=False, exit_code=None,
                stdout="", stderr="", compile_stderr="[compile timeout]",
                timed_out=True
            )

        if compile_proc.returncode != 0 or not bin_path.exists():
            return RunResult(
                compiled=False, exit_code=None,
                stdout="", stderr="",
                compile_stderr=compile_proc.stderr,
            )

        # --- Execution ---
        try:
            run_proc = subprocess.run(
                [str(bin_path)],
                capture_output=True,
                text=True,
                timeout=run_timeout,
            )
            return RunResult(
                compiled=True,
                exit_code=run_proc.returncode,
                stdout=run_proc.stdout,
                stderr=run_proc.stderr,
                compile_stderr="",
            )
        except subprocess.TimeoutExpired:
            return RunResult(
                compiled=True, exit_code=None,
                stdout="", stderr="", compile_stderr="",
                timed_out=True
            )


def baseline(source_path: Path, compiler: Path,
             compile_timeout: int = 15, run_timeout: int = 5) -> RunResult:
    """Run the baseline seed from disk."""
    return compile_and_run(
        source_path.read_text(encoding="utf-8"),
        compiler,
        compile_timeout=compile_timeout,
        run_timeout=run_timeout,
    )
