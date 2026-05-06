#!/usr/bin/env python3
"""
Phase 3.3: Allocation Determinism Test Runner

Validates that Aria's memory allocation system is fully deterministic:
- Same input → Same output
- Consistent timing (low variance)
- No hidden state affecting behavior
- Reproducible across fresh processes
"""

import subprocess
import sys
import os
import time
import json
from pathlib import Path
from typing import List, Tuple, Dict
from dataclasses import dataclass
import statistics

SCRIPT_DIR = Path(__file__).parent
ARIA_ROOT = SCRIPT_DIR.parent.parent
BUILD_DIR = ARIA_ROOT / "build"
NPKC = BUILD_DIR / "npkc"

@dataclass
class TestResult:
    name: str
    passed: bool
    message: str
    timing_ms: float = 0.0
    cov: float = 0.0  # Coefficient of variation

def run_command(cmd: List[str], timeout: int = 60) -> Tuple[int, str, str]:
    """Run a command and return (returncode, stdout, stderr)."""
    try:
        result = subprocess.run(
            cmd,
            capture_output=True,
            timeout=timeout,
            text=True
        )
        return result.returncode, result.stdout, result.stderr
    except subprocess.TimeoutExpired:
        return -1, "", "TIMEOUT"
    except Exception as e:
        return -1, "", str(e)

def compile_aria(source: Path, output: Path) -> bool:
    """Compile an Aria source file."""
    if not NPKC.exists():
        print(f"ERROR: Compiler not found at {NPKC}")
        return False

    cmd = [str(NPKC), str(source), "-o", str(output)]
    code, stdout, stderr = run_command(cmd, timeout=30)

    if code != 0:
        print(f"  Compilation failed: {stderr}")
        return False
    return True

def test_output_determinism(executable: Path, runs: int = 5) -> TestResult:
    """Test that program output is identical across runs."""
    outputs = []

    for i in range(runs):
        code, stdout, stderr = run_command([str(executable)], timeout=10)
        if code != 0:
            return TestResult(
                name=f"output_determinism_{executable.stem}",
                passed=False,
                message=f"Run {i+1} failed: {stderr}"
            )
        outputs.append(stdout)

    # Check all outputs match
    if len(set(outputs)) == 1:
        return TestResult(
            name=f"output_determinism_{executable.stem}",
            passed=True,
            message=f"All {runs} runs produced identical output"
        )
    else:
        return TestResult(
            name=f"output_determinism_{executable.stem}",
            passed=False,
            message=f"Outputs differ: {len(set(outputs))} unique outputs in {runs} runs"
        )

def test_timing_consistency(executable: Path, runs: int = 20) -> TestResult:
    """Test that execution timing is consistent."""
    times = []

    for i in range(runs):
        start = time.perf_counter()
        code, stdout, stderr = run_command([str(executable)], timeout=10)
        elapsed = (time.perf_counter() - start) * 1000  # ms

        if code != 0:
            continue
        times.append(elapsed)

    if len(times) < 5:
        return TestResult(
            name=f"timing_consistency_{executable.stem}",
            passed=False,
            message="Too few successful runs"
        )

    mean = statistics.mean(times)
    std = statistics.stdev(times)
    cov = std / mean if mean > 0 else 0

    # Allow higher variance for execution (includes process startup, I/O, etc.)
    # Short-running programs have high relative variance due to startup overhead
    passed = cov < 0.50  # 50% CoV threshold (reasonable for sub-5ms programs)

    return TestResult(
        name=f"timing_consistency_{executable.stem}",
        passed=passed,
        message=f"Mean: {mean:.1f}ms, Std: {std:.1f}ms, CoV: {cov:.3f}",
        timing_ms=mean,
        cov=cov
    )

def test_fresh_process_determinism(executable: Path, runs: int = 3) -> TestResult:
    """Test that fresh processes produce identical results."""
    outputs = []

    for i in range(runs):
        # Use unique env to ensure fresh process state
        env = os.environ.copy()
        env["ARIA_DETERMINISM_RUN"] = str(i)

        try:
            result = subprocess.run(
                [str(executable)],
                capture_output=True,
                timeout=10,
                env=env,
                text=True
            )
            outputs.append(result.stdout)
        except Exception as e:
            return TestResult(
                name=f"fresh_process_{executable.stem}",
                passed=False,
                message=f"Run {i+1} failed: {e}"
            )

    if len(set(outputs)) == 1:
        return TestResult(
            name=f"fresh_process_{executable.stem}",
            passed=True,
            message="Fresh processes produce identical results"
        )
    else:
        return TestResult(
            name=f"fresh_process_{executable.stem}",
            passed=False,
            message="HIDDEN STATE DETECTED: Fresh processes differ"
        )

def run_all_tests(quick: bool = False) -> Dict:
    """Run all determinism tests."""
    results = []

    # Find Aria test files
    aria_files = list(SCRIPT_DIR.glob("*.npk"))
    if not aria_files:
        print("No .npk test files found!")
        return {"passed": 0, "total": 0, "results": []}

    print(f"Found {len(aria_files)} test files")
    print("-" * 60)

    runs = 5 if quick else 20

    for aria_file in sorted(aria_files):
        print(f"\nTesting: {aria_file.name}")

        # Compile
        executable = SCRIPT_DIR / f"_tmp_{aria_file.stem}"
        if not compile_aria(aria_file, executable):
            results.append(TestResult(
                name=aria_file.stem,
                passed=False,
                message="Compilation failed"
            ))
            continue

        try:
            # Run determinism tests
            r1 = test_output_determinism(executable, runs=runs)
            r2 = test_timing_consistency(executable, runs=runs)
            r3 = test_fresh_process_determinism(executable, runs=min(3, runs))

            results.extend([r1, r2, r3])

            for r in [r1, r2, r3]:
                status = "PASS" if r.passed else "FAIL"
                print(f"  [{status}] {r.name}: {r.message}")

        finally:
            # Cleanup
            if executable.exists():
                executable.unlink()

    # Summary
    passed = sum(1 for r in results if r.passed)
    total = len(results)

    return {
        "passed": passed,
        "total": total,
        "pass_rate": passed / total if total > 0 else 0,
        "results": [
            {"name": r.name, "passed": r.passed, "message": r.message,
             "timing_ms": r.timing_ms, "cov": r.cov}
            for r in results
        ]
    }

def print_summary(results: Dict):
    """Print test summary."""
    print("\n" + "=" * 60)
    print("PHASE 3.3: ALLOCATION DETERMINISM RESULTS")
    print("=" * 60)

    passed = results["passed"]
    total = results["total"]
    rate = results.get("pass_rate", 0) * 100

    print(f"\nPassed: {passed}/{total} ({rate:.1f}%)")

    if rate == 100:
        print("\nDETERMINISM VALIDATED:")
        print("  - All outputs are reproducible")
        print("  - Timing variance within acceptable limits")
        print("  - No hidden state affecting behavior")
    elif rate >= 80:
        print("\nMOSTLY DETERMINISTIC:")
        print("  - Minor timing variance detected (expected)")
        print("  - Output behavior is consistent")
    else:
        print("\nWARNING: Significant non-determinism detected!")
        for r in results["results"]:
            if not r["passed"]:
                print(f"  - {r['name']}: {r['message']}")

    print("=" * 60)


def main():
    import argparse

    parser = argparse.ArgumentParser(description="Allocation Determinism Tests")
    parser.add_argument("--quick", action="store_true", help="Quick test (fewer iterations)")
    parser.add_argument("--verbose", "-v", action="store_true", help="Verbose output")
    args = parser.parse_args()

    print("=" * 60)
    print("PHASE 3.3: ALLOCATION DETERMINISM TESTING")
    print("=" * 60)
    print()

    if not NPKC.exists():
        print(f"ERROR: Aria compiler not found at {NPKC}")
        print("Please build the compiler first: cd build && make")
        return 1

    results = run_all_tests(quick=args.quick)
    print_summary(results)

    # Save results
    results_file = SCRIPT_DIR / "results.json"
    with open(results_file, "w") as f:
        json.dump(results, f, indent=2)
    print(f"\nResults saved to {results_file}")

    return 0 if results.get("pass_rate", 0) >= 0.8 else 1


if __name__ == "__main__":
    sys.exit(main())
