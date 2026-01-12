#!/usr/bin/env python3
"""
Aria Property Tester
Phase 2.6: Property Testing Framework

Tests generated programs against the compiler and validates properties.
"""

import os
import subprocess
import tempfile
import time
import json
from dataclasses import dataclass, field
from typing import List, Optional, Tuple, Dict, Any
from enum import Enum, auto
from pathlib import Path
import hashlib

class TestResult(Enum):
    """Result of a property test"""
    PASSED = auto()           # Test behaved as expected
    FAILED = auto()           # Test violated expected behavior
    COMPILE_ERROR = auto()    # Compiler rejected (may be expected)
    RUNTIME_ERROR = auto()    # Runtime error (may be expected)
    TIMEOUT = auto()          # Compilation or execution timed out
    INTERNAL_ERROR = auto()   # Test framework error

@dataclass
class PropertyTestResult:
    """Result of testing a single property"""
    property_name: str
    result: TestResult
    expected_compile: bool  # True if program should compile
    actual_compiled: bool   # True if program did compile
    compiler_output: str = ""
    runtime_output: str = ""
    execution_time_ms: float = 0.0
    seed: int = 0
    program_hash: str = ""
    program_source: str = ""

    def is_soundness_violation(self) -> bool:
        """
        Check if this result represents a SOUNDNESS VIOLATION.

        A soundness violation means the compiler ACCEPTED a program
        that should have been REJECTED (unsafe code compiled).
        """
        if self.expected_compile:
            # Expected to compile - if it didn't, that's a false negative
            # (overly strict), not a soundness issue
            return False
        else:
            # Expected to NOT compile - if it did, SOUNDNESS BUG!
            return self.actual_compiled

    def is_false_negative(self) -> bool:
        """
        Check if this is a false negative (over-rejection).

        The compiler rejected a valid program.
        """
        return self.expected_compile and not self.actual_compiled


@dataclass
class TestStatistics:
    """Statistics for a property testing run"""
    total_tests: int = 0
    passed: int = 0
    failed: int = 0
    compile_errors: int = 0
    runtime_errors: int = 0
    timeouts: int = 0
    soundness_violations: int = 0
    false_negatives: int = 0
    start_time: float = 0.0
    end_time: float = 0.0
    bugs_found: List[PropertyTestResult] = field(default_factory=list)

    def duration_seconds(self) -> float:
        return self.end_time - self.start_time

    def tests_per_second(self) -> float:
        duration = self.duration_seconds()
        if duration <= 0:
            return 0.0
        return self.total_tests / duration

    def to_dict(self) -> Dict[str, Any]:
        return {
            "total_tests": self.total_tests,
            "passed": self.passed,
            "failed": self.failed,
            "compile_errors": self.compile_errors,
            "runtime_errors": self.runtime_errors,
            "timeouts": self.timeouts,
            "soundness_violations": self.soundness_violations,
            "false_negatives": self.false_negatives,
            "duration_seconds": self.duration_seconds(),
            "tests_per_second": self.tests_per_second(),
            "bugs_found": len(self.bugs_found)
        }


class PropertyTester:
    """
    Tests Aria programs for property violations.

    Properties tested:
    1. SOUNDNESS: Unsafe programs are REJECTED by compiler
    2. COMPLETENESS: Safe programs are ACCEPTED by compiler
    3. DETERMINISM: Same input produces same output
    4. NO_CRASH: Compiler never crashes on valid syntax
    """

    def __init__(self, compiler_path: str,
                 build_dir: Optional[str] = None,
                 timeout_seconds: float = 10.0,
                 keep_artifacts: bool = False,
                 verbose: bool = False):
        """
        Initialize property tester.

        Args:
            compiler_path: Path to ariac compiler
            build_dir: Build directory for artifacts
            timeout_seconds: Max time for compilation/execution
            keep_artifacts: Keep generated .aria files
            verbose: Print detailed output
        """
        self.compiler_path = compiler_path
        self.build_dir = build_dir or tempfile.mkdtemp(prefix="aria_proptest_")
        self.timeout = timeout_seconds
        self.keep_artifacts = keep_artifacts
        self.verbose = verbose
        self.stats = TestStatistics()

        # Ensure build dir exists
        Path(self.build_dir).mkdir(parents=True, exist_ok=True)

        # Check compiler exists
        if not os.path.isfile(compiler_path):
            raise FileNotFoundError(f"Compiler not found: {compiler_path}")

    def _log(self, msg: str):
        """Log message if verbose"""
        if self.verbose:
            print(f"[PropTest] {msg}")

    def _hash_program(self, source: str) -> str:
        """Generate hash for program source"""
        return hashlib.sha256(source.encode()).hexdigest()[:16]

    def _compile_program(self, source: str, seed: int) -> Tuple[bool, str, float]:
        """
        Compile an Aria program.

        Returns:
            (success, output, time_ms)
        """
        prog_hash = self._hash_program(source)
        source_file = os.path.join(self.build_dir, f"test_{seed}_{prog_hash}.aria")
        output_file = os.path.join(self.build_dir, f"test_{seed}_{prog_hash}")

        # Write source
        with open(source_file, 'w') as f:
            f.write(source)

        start = time.perf_counter()

        try:
            result = subprocess.run(
                [self.compiler_path, source_file, "-o", output_file],
                capture_output=True,
                text=True,
                timeout=self.timeout
            )
            elapsed = (time.perf_counter() - start) * 1000

            success = result.returncode == 0
            output = result.stdout + result.stderr

            # Cleanup if not keeping artifacts
            if not self.keep_artifacts:
                try:
                    os.remove(source_file)
                    if os.path.exists(output_file):
                        os.remove(output_file)
                except:
                    pass

            return success, output, elapsed

        except subprocess.TimeoutExpired:
            return False, "TIMEOUT", self.timeout * 1000

        except Exception as e:
            return False, f"ERROR: {e}", 0.0

    def _run_executable(self, seed: int, prog_hash: str) -> Tuple[bool, str]:
        """Run compiled executable"""
        output_file = os.path.join(self.build_dir, f"test_{seed}_{prog_hash}")

        if not os.path.exists(output_file):
            return False, "Executable not found"

        try:
            result = subprocess.run(
                [output_file],
                capture_output=True,
                text=True,
                timeout=self.timeout
            )
            success = result.returncode == 0
            output = result.stdout + result.stderr
            return success, output

        except subprocess.TimeoutExpired:
            return False, "RUNTIME TIMEOUT"

        except Exception as e:
            return False, f"RUNTIME ERROR: {e}"

    def test_program(self, source: str, expected_compile: bool,
                     property_name: str, seed: int = 0) -> PropertyTestResult:
        """
        Test a single program.

        Args:
            source: Aria source code
            expected_compile: True if program should compile successfully
            property_name: Name of property being tested
            seed: Random seed used to generate program

        Returns:
            PropertyTestResult with test outcome
        """
        prog_hash = self._hash_program(source)

        # Compile
        compiled, compiler_output, compile_time = self._compile_program(source, seed)

        # Determine result
        if "TIMEOUT" in compiler_output:
            result = TestResult.TIMEOUT
        elif compiled:
            if expected_compile:
                result = TestResult.PASSED
            else:
                # SOUNDNESS BUG: unsafe code compiled!
                result = TestResult.FAILED
        else:
            if expected_compile:
                # False negative: safe code rejected
                result = TestResult.FAILED
            else:
                # Correctly rejected
                result = TestResult.PASSED

        test_result = PropertyTestResult(
            property_name=property_name,
            result=result,
            expected_compile=expected_compile,
            actual_compiled=compiled,
            compiler_output=compiler_output,
            execution_time_ms=compile_time,
            seed=seed,
            program_hash=prog_hash,
            program_source=source
        )

        # Update stats
        self.stats.total_tests += 1

        if result == TestResult.PASSED:
            self.stats.passed += 1
        elif result == TestResult.FAILED:
            self.stats.failed += 1
            if test_result.is_soundness_violation():
                self.stats.soundness_violations += 1
                self.stats.bugs_found.append(test_result)
                self._log(f"SOUNDNESS BUG FOUND! Seed: {seed}")
            elif test_result.is_false_negative():
                self.stats.false_negatives += 1
        elif result == TestResult.TIMEOUT:
            self.stats.timeouts += 1
        elif result == TestResult.COMPILE_ERROR:
            self.stats.compile_errors += 1

        return test_result

    def test_valid_programs(self, generator, num_programs: int,
                           base_seed: int = 0) -> List[PropertyTestResult]:
        """
        Test that generated valid programs compile successfully.

        Property: COMPLETENESS - valid programs should be accepted.
        """
        results = []
        self._log(f"Testing {num_programs} valid programs...")

        for i in range(num_programs):
            seed = base_seed + i
            generator.__init__(seed=seed)

            source = generator.generate_program(num_statements=10)
            result = self.test_program(
                source=source,
                expected_compile=True,
                property_name="valid_program_compiles",
                seed=seed
            )
            results.append(result)

            if self.verbose and (i + 1) % 100 == 0:
                self._log(f"  Tested {i + 1}/{num_programs}")

        return results

    def test_adversarial_programs(self, generator,
                                  num_programs: int) -> List[PropertyTestResult]:
        """
        Test that adversarial programs are REJECTED.

        Property: SOUNDNESS - unsafe programs must be rejected.
        """
        results = []
        self._log(f"Testing {num_programs} adversarial programs...")

        adversarial_generators = [
            ("use_after_free", generator.generate_use_after_free),
            ("double_free", generator.generate_double_free),
            ("aliasing_violation", generator.generate_aliasing_violation),
            ("borrow_mutation", generator.generate_borrow_mutation),
            ("loop_carried_uaf", generator.generate_loop_carried_uaf),
            ("conditional_leak", generator.generate_conditional_leak),
        ]

        tests_per_pattern = num_programs // len(adversarial_generators)

        for i, (name, gen_func) in enumerate(adversarial_generators):
            for j in range(tests_per_pattern):
                seed = i * 1000 + j
                source = gen_func()
                result = self.test_program(
                    source=source,
                    expected_compile=False,  # Should be REJECTED
                    property_name=f"adversarial_{name}",
                    seed=seed
                )
                results.append(result)

        return results

    def test_determinism(self, generator, num_programs: int,
                        base_seed: int = 0) -> List[PropertyTestResult]:
        """
        Test that compilation is deterministic.

        Property: DETERMINISM - same input always produces same output.
        """
        results = []
        self._log(f"Testing determinism with {num_programs} programs...")

        for i in range(num_programs):
            seed = base_seed + i
            generator.__init__(seed=seed)
            source = generator.generate_program(num_statements=8)

            # Compile twice
            compiled1, output1, _ = self._compile_program(source, seed)
            compiled2, output2, _ = self._compile_program(source, seed + 1000000)

            # Results should match
            if compiled1 == compiled2:
                result = TestResult.PASSED
            else:
                result = TestResult.FAILED

            test_result = PropertyTestResult(
                property_name="determinism",
                result=result,
                expected_compile=True,
                actual_compiled=compiled1,
                compiler_output=f"Run1: {compiled1}, Run2: {compiled2}",
                seed=seed,
                program_hash=self._hash_program(source),
                program_source=source
            )
            results.append(test_result)
            self.stats.total_tests += 1

            if result == TestResult.PASSED:
                self.stats.passed += 1
            else:
                self.stats.failed += 1

        return results

    def run_full_suite(self, generator, adversarial_generator,
                       valid_count: int = 1000,
                       adversarial_count: int = 100,
                       determinism_count: int = 50) -> TestStatistics:
        """
        Run the complete property testing suite.

        Args:
            generator: AriaGenerator for valid programs
            adversarial_generator: AdversarialGenerator for unsafe programs
            valid_count: Number of valid programs to test
            adversarial_count: Number of adversarial programs
            determinism_count: Number of determinism checks
        """
        self.stats = TestStatistics()
        self.stats.start_time = time.time()

        print(f"\n{'='*60}")
        print("Aria Property Testing Suite")
        print(f"{'='*60}")
        print(f"Valid programs:      {valid_count:,}")
        print(f"Adversarial tests:   {adversarial_count:,}")
        print(f"Determinism tests:   {determinism_count:,}")
        print(f"{'='*60}\n")

        # Test valid programs
        print("[1/3] Testing valid program compilation...")
        self.test_valid_programs(generator, valid_count)

        # Test adversarial programs
        print("[2/3] Testing adversarial program rejection...")
        self.test_adversarial_programs(adversarial_generator, adversarial_count)

        # Test determinism
        print("[3/3] Testing compilation determinism...")
        self.test_determinism(generator, determinism_count)

        self.stats.end_time = time.time()

        # Print summary
        print(f"\n{'='*60}")
        print("RESULTS SUMMARY")
        print(f"{'='*60}")
        print(f"Total tests:         {self.stats.total_tests:,}")
        print(f"Passed:              {self.stats.passed:,}")
        print(f"Failed:              {self.stats.failed:,}")
        print(f"Timeouts:            {self.stats.timeouts:,}")
        print(f"Duration:            {self.stats.duration_seconds():.2f}s")
        print(f"Tests/second:        {self.stats.tests_per_second():.1f}")
        print(f"{'='*60}")

        if self.stats.soundness_violations > 0:
            print(f"\n!!! SOUNDNESS VIOLATIONS: {self.stats.soundness_violations} !!!")
            print("These represent CRITICAL BUGS where unsafe code compiled.\n")

            for bug in self.stats.bugs_found[:5]:  # Show first 5
                print(f"  Bug: {bug.property_name}, Seed: {bug.seed}")
                print(f"  Hash: {bug.program_hash}")
                print()

        if self.stats.false_negatives > 0:
            print(f"\nFalse negatives: {self.stats.false_negatives}")
            print("These are valid programs incorrectly rejected.\n")

        return self.stats

    def save_results(self, filepath: str):
        """Save test results to JSON"""
        data = self.stats.to_dict()
        data["bugs"] = []

        for bug in self.stats.bugs_found:
            data["bugs"].append({
                "property": bug.property_name,
                "seed": bug.seed,
                "hash": bug.program_hash,
                "source": bug.program_source,
                "compiler_output": bug.compiler_output
            })

        with open(filepath, 'w') as f:
            json.dump(data, f, indent=2)

        print(f"Results saved to {filepath}")


if __name__ == "__main__":
    # Quick test
    from aria_generator import AriaGenerator, AdversarialGenerator

    # Find compiler
    compiler = os.path.expanduser("~/._____RANDY_____/REPOS/aria/build/ariac")

    if not os.path.exists(compiler):
        print(f"Compiler not found at {compiler}")
        print("Please build the compiler first.")
        exit(1)

    tester = PropertyTester(
        compiler_path=compiler,
        verbose=True
    )

    gen = AriaGenerator(seed=42)
    adv = AdversarialGenerator()

    # Small test run
    stats = tester.run_full_suite(
        generator=gen,
        adversarial_generator=adv,
        valid_count=10,
        adversarial_count=6,
        determinism_count=5
    )

    print(f"\nQuick test complete: {stats.total_tests} tests")
