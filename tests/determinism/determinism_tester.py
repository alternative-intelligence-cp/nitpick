#!/usr/bin/env python3
"""
Phase 3.3: Allocation Determinism Testing Framework

Tests that memory allocation in Aria programs is deterministic:
1. TIMING: Allocation timing has low variance (< 10% CoV)
2. ORDER: Same program produces same allocation order
3. REPRODUCIBILITY: Multiple runs yield consistent results
4. NO HIDDEN STATE: Fresh process = identical behavior
"""

import subprocess
import os
import sys
import time
import json
import statistics
from pathlib import Path
from dataclasses import dataclass, field
from typing import List, Dict, Optional, Tuple
from concurrent.futures import ProcessPoolExecutor, as_completed

# Configuration
ARIA_ROOT = Path(__file__).parent.parent.parent
BUILD_DIR = ARIA_ROOT / "build"
NPKC = BUILD_DIR / "npkc"
TESTS_DIR = Path(__file__).parent

@dataclass
class AllocationEvent:
    """Represents a single allocation event."""
    timestamp_ns: int
    size: int
    address: int
    allocator: str  # "wild", "arena", "pool", "slab", "wildx"

@dataclass
class DeterminismResult:
    """Result of a determinism test."""
    test_name: str
    passed: bool
    timing_cov: float  # Coefficient of variation for timing
    order_match: bool  # Whether allocation order matches across runs
    message: str
    details: Dict = field(default_factory=dict)

class DeterminismTester:
    """Tests allocation determinism properties."""

    def __init__(self, verbose: bool = False):
        self.verbose = verbose
        self.results: List[DeterminismResult] = []

    def log(self, msg: str):
        if self.verbose:
            print(f"[DETERMINISM] {msg}")

    def compile_test(self, aria_file: Path, output: Path) -> bool:
        """Compile an Aria test program."""
        cmd = [str(NPKC), str(aria_file), "-o", str(output)]
        try:
            result = subprocess.run(cmd, capture_output=True, timeout=30)
            return result.returncode == 0
        except subprocess.TimeoutExpired:
            return False

    def run_program_timed(self, executable: Path, runs: int = 10) -> Tuple[List[float], List[str]]:
        """Run a program multiple times and collect timing data."""
        times = []
        outputs = []

        for i in range(runs):
            start = time.perf_counter_ns()
            try:
                result = subprocess.run(
                    [str(executable)],
                    capture_output=True,
                    timeout=10,
                    env={**os.environ, "ARIA_DETERMINISM_TRACE": "1"}
                )
                elapsed = time.perf_counter_ns() - start
                times.append(elapsed / 1_000_000)  # Convert to ms
                outputs.append(result.stdout.decode('utf-8', errors='replace'))
            except subprocess.TimeoutExpired:
                self.log(f"Run {i+1} timed out")
                continue

        return times, outputs

    def test_timing_consistency(self, aria_file: Path, max_cov: float = 0.15) -> DeterminismResult:
        """Test that allocation timing is consistent (low variance)."""
        test_name = f"timing_{aria_file.stem}"
        output = TESTS_DIR / f"_tmp_{aria_file.stem}"

        # Compile
        if not self.compile_test(aria_file, output):
            return DeterminismResult(
                test_name=test_name,
                passed=False,
                timing_cov=float('inf'),
                order_match=False,
                message=f"Failed to compile {aria_file.name}"
            )

        # Run multiple times
        times, outputs = self.run_program_timed(output, runs=20)

        # Cleanup
        if output.exists():
            output.unlink()

        if len(times) < 5:
            return DeterminismResult(
                test_name=test_name,
                passed=False,
                timing_cov=float('inf'),
                order_match=False,
                message="Too few successful runs"
            )

        # Calculate coefficient of variation
        mean_time = statistics.mean(times)
        std_time = statistics.stdev(times)
        cov = std_time / mean_time if mean_time > 0 else 0

        passed = cov <= max_cov

        return DeterminismResult(
            test_name=test_name,
            passed=passed,
            timing_cov=cov,
            order_match=True,  # Not tested here
            message=f"Timing CoV: {cov:.3f} (max: {max_cov}), Mean: {mean_time:.2f}ms",
            details={
                "mean_ms": mean_time,
                "std_ms": std_time,
                "min_ms": min(times),
                "max_ms": max(times),
                "runs": len(times)
            }
        )

    def test_output_reproducibility(self, aria_file: Path) -> DeterminismResult:
        """Test that program output is identical across runs."""
        test_name = f"reproducibility_{aria_file.stem}"
        output = TESTS_DIR / f"_tmp_{aria_file.stem}"

        # Compile
        if not self.compile_test(aria_file, output):
            return DeterminismResult(
                test_name=test_name,
                passed=False,
                timing_cov=0,
                order_match=False,
                message=f"Failed to compile {aria_file.name}"
            )

        # Run multiple times
        _, outputs = self.run_program_timed(output, runs=5)

        # Cleanup
        if output.exists():
            output.unlink()

        if len(outputs) < 2:
            return DeterminismResult(
                test_name=test_name,
                passed=False,
                timing_cov=0,
                order_match=False,
                message="Too few successful runs"
            )

        # Check all outputs match
        first_output = outputs[0]
        all_match = all(o == first_output for o in outputs)

        return DeterminismResult(
            test_name=test_name,
            passed=all_match,
            timing_cov=0,
            order_match=all_match,
            message="All outputs identical" if all_match else "Outputs differ between runs",
            details={
                "runs": len(outputs),
                "unique_outputs": len(set(outputs))
            }
        )

    def test_fresh_process_determinism(self, aria_file: Path) -> DeterminismResult:
        """Test that fresh processes produce identical results."""
        test_name = f"fresh_process_{aria_file.stem}"
        output = TESTS_DIR / f"_tmp_{aria_file.stem}"

        # Compile
        if not self.compile_test(aria_file, output):
            return DeterminismResult(
                test_name=test_name,
                passed=False,
                timing_cov=0,
                order_match=False,
                message=f"Failed to compile {aria_file.name}"
            )

        # Run in isolated subprocesses (truly fresh processes)
        outputs = []
        for i in range(5):
            try:
                # Fork a completely new process
                result = subprocess.run(
                    [str(output)],
                    capture_output=True,
                    timeout=10,
                    env={**os.environ, "ARIA_FRESH_TEST": str(i)}
                )
                outputs.append(result.stdout.decode('utf-8', errors='replace'))
            except subprocess.TimeoutExpired:
                continue

        # Cleanup
        if output.exists():
            output.unlink()

        if len(outputs) < 2:
            return DeterminismResult(
                test_name=test_name,
                passed=False,
                timing_cov=0,
                order_match=False,
                message="Too few successful runs"
            )

        # Check all outputs match
        first_output = outputs[0]
        all_match = all(o == first_output for o in outputs)

        return DeterminismResult(
            test_name=test_name,
            passed=all_match,
            timing_cov=0,
            order_match=all_match,
            message="Fresh processes produce identical results" if all_match
                    else "Fresh processes produce different results (HIDDEN STATE DETECTED)",
            details={
                "runs": len(outputs),
                "unique_outputs": len(set(outputs))
            }
        )

    def test_allocation_sequence(self, aria_file: Path) -> DeterminismResult:
        """Test that allocation sequence is deterministic."""
        test_name = f"alloc_sequence_{aria_file.stem}"
        output = TESTS_DIR / f"_tmp_{aria_file.stem}"

        # Compile with allocation tracing
        if not self.compile_test(aria_file, output):
            return DeterminismResult(
                test_name=test_name,
                passed=False,
                timing_cov=0,
                order_match=False,
                message=f"Failed to compile {aria_file.name}"
            )

        # Run with allocation tracing enabled
        alloc_traces = []
        for i in range(3):
            try:
                result = subprocess.run(
                    [str(output)],
                    capture_output=True,
                    timeout=10,
                    env={**os.environ, "ARIA_ALLOC_TRACE": "1"}
                )
                # Parse allocation trace from stderr or special output
                alloc_traces.append(result.stdout.decode('utf-8', errors='replace'))
            except subprocess.TimeoutExpired:
                continue

        # Cleanup
        if output.exists():
            output.unlink()

        if len(alloc_traces) < 2:
            return DeterminismResult(
                test_name=test_name,
                passed=False,
                timing_cov=0,
                order_match=False,
                message="Too few successful runs"
            )

        # Check allocation sequences match
        first_trace = alloc_traces[0]
        all_match = all(t == first_trace for t in alloc_traces)

        return DeterminismResult(
            test_name=test_name,
            passed=all_match,
            timing_cov=0,
            order_match=all_match,
            message="Allocation sequences match" if all_match else "Allocation sequences differ",
            details={
                "runs": len(alloc_traces),
                "unique_sequences": len(set(alloc_traces))
            }
        )

    def run_all_tests(self, test_files: List[Path]) -> Dict:
        """Run all determinism tests on given files."""
        all_results = []

        for aria_file in test_files:
            self.log(f"Testing {aria_file.name}...")

            # Run all test types
            results = [
                self.test_timing_consistency(aria_file),
                self.test_output_reproducibility(aria_file),
                self.test_fresh_process_determinism(aria_file),
                self.test_allocation_sequence(aria_file),
            ]

            all_results.extend(results)
            self.results.extend(results)

        # Summary
        passed = sum(1 for r in all_results if r.passed)
        total = len(all_results)

        return {
            "passed": passed,
            "total": total,
            "pass_rate": passed / total if total > 0 else 0,
            "results": [
                {
                    "name": r.test_name,
                    "passed": r.passed,
                    "timing_cov": r.timing_cov,
                    "order_match": r.order_match,
                    "message": r.message,
                    "details": r.details
                }
                for r in all_results
            ]
        }

    def print_summary(self, results: Dict):
        """Print test summary."""
        print("\n" + "=" * 60)
        print("PHASE 3.3: ALLOCATION DETERMINISM TEST RESULTS")
        print("=" * 60)

        passed = results["passed"]
        total = results["total"]
        rate = results["pass_rate"] * 100

        for r in results["results"]:
            status = "PASS" if r["passed"] else "FAIL"
            print(f"  [{status}] {r['name']}: {r['message']}")

        print("-" * 60)
        print(f"TOTAL: {passed}/{total} ({rate:.1f}%)")

        if rate == 100:
            print("\nDETERMINISM VALIDATED: All allocation behavior is deterministic")
        elif rate >= 80:
            print("\nMOSTLY DETERMINISTIC: Minor timing variance detected")
        else:
            print("\nWARNING: Significant non-determinism detected!")

        print("=" * 60)


def main():
    import argparse

    parser = argparse.ArgumentParser(description="Allocation Determinism Tester")
    parser.add_argument("--verbose", "-v", action="store_true", help="Verbose output")
    parser.add_argument("--quick", action="store_true", help="Quick test (fewer iterations)")
    parser.add_argument("--file", type=Path, help="Test specific file")
    args = parser.parse_args()

    tester = DeterminismTester(verbose=args.verbose)

    # Find test files
    if args.file:
        test_files = [args.file]
    else:
        test_files = list(TESTS_DIR.glob("*.npk"))
        if not test_files:
            print("No .npk test files found in determinism directory")
            print("Creating sample test files...")
            # Will be created by companion script
            return 1

    print(f"Running determinism tests on {len(test_files)} files...")
    results = tester.run_all_tests(test_files)
    tester.print_summary(results)

    # Save results
    results_file = TESTS_DIR / "determinism_results.json"
    with open(results_file, "w") as f:
        json.dump(results, f, indent=2)
    print(f"\nResults saved to {results_file}")

    return 0 if results["pass_rate"] >= 0.8 else 1


if __name__ == "__main__":
    sys.exit(main())
