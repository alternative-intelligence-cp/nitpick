#!/usr/bin/env python3
"""
Aria Property Testing Runner
Phase 2.6: Property Testing Framework

Main entry point for running 100,000+ property tests.

Usage:
    python3 run_property_tests.py --quick      # Quick test (100 programs)
    python3 run_property_tests.py --standard   # Standard test (10,000 programs)
    python3 run_property_tests.py --full       # Full test (100,000 programs)
    python3 run_property_tests.py --marathon   # Marathon test (1,000,000 programs)
"""

import argparse
import os
import sys
import time
import json
from pathlib import Path
from datetime import datetime

# Add current directory to path
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from aria_generator import AriaGenerator, AdversarialGenerator
from property_tester import PropertyTester, TestStatistics


def find_compiler() -> str:
    """Find the Aria compiler"""
    # Try common locations
    candidates = [
        os.path.expanduser("~/._____RANDY_____/REPOS/aria/build/ariac"),
        os.path.expanduser("~/aria/build/ariac"),
        "./build/ariac",
        "../build/ariac",
        "../../build/ariac",
    ]

    for path in candidates:
        if os.path.isfile(path):
            return os.path.abspath(path)

    # Try to find via which
    import subprocess
    try:
        result = subprocess.run(["which", "ariac"], capture_output=True, text=True)
        if result.returncode == 0:
            return result.stdout.strip()
    except:
        pass

    return None


def run_quick_test(tester: PropertyTester) -> TestStatistics:
    """Quick test: 100 programs in ~30 seconds"""
    gen = AriaGenerator(complexity=2)
    adv = AdversarialGenerator()

    return tester.run_full_suite(
        generator=gen,
        adversarial_generator=adv,
        valid_count=50,
        adversarial_count=30,
        determinism_count=20
    )


def run_standard_test(tester: PropertyTester) -> TestStatistics:
    """Standard test: 10,000 programs in ~5 minutes"""
    gen = AriaGenerator(complexity=3)
    adv = AdversarialGenerator()

    return tester.run_full_suite(
        generator=gen,
        adversarial_generator=adv,
        valid_count=8000,
        adversarial_count=1500,
        determinism_count=500
    )


def run_full_test(tester: PropertyTester) -> TestStatistics:
    """Full test: 100,000 programs in ~1 hour"""
    gen = AriaGenerator(complexity=4)
    adv = AdversarialGenerator()

    return tester.run_full_suite(
        generator=gen,
        adversarial_generator=adv,
        valid_count=85000,
        adversarial_count=10000,
        determinism_count=5000
    )


def run_marathon_test(tester: PropertyTester) -> TestStatistics:
    """Marathon test: 1,000,000 programs in ~10 hours"""
    gen = AriaGenerator(complexity=5)
    adv = AdversarialGenerator()

    return tester.run_full_suite(
        generator=gen,
        adversarial_generator=adv,
        valid_count=900000,
        adversarial_count=80000,
        determinism_count=20000
    )


def run_borrow_checker_focus(tester: PropertyTester, count: int) -> TestStatistics:
    """Focus on borrow checker testing"""
    print("\n=== BORROW CHECKER FOCUSED TESTING ===\n")

    gen = AriaGenerator(
        complexity=4,
        test_borrow_checker=True,
        test_wild_memory=True,
        test_tbb=False,
        test_control_flow=True
    )
    adv = AdversarialGenerator()

    return tester.run_full_suite(
        generator=gen,
        adversarial_generator=adv,
        valid_count=int(count * 0.7),
        adversarial_count=int(count * 0.25),
        determinism_count=int(count * 0.05)
    )


def run_tbb_focus(tester: PropertyTester, count: int) -> TestStatistics:
    """Focus on TBB sentinel testing"""
    print("\n=== TBB SENTINEL FOCUSED TESTING ===\n")

    gen = AriaGenerator(
        complexity=3,
        test_borrow_checker=False,
        test_wild_memory=False,
        test_tbb=True,
        test_control_flow=True
    )
    adv = AdversarialGenerator()

    return tester.run_full_suite(
        generator=gen,
        adversarial_generator=adv,
        valid_count=int(count * 0.8),
        adversarial_count=int(count * 0.15),
        determinism_count=int(count * 0.05)
    )


def run_wild_memory_focus(tester: PropertyTester, count: int) -> TestStatistics:
    """Focus on wild memory testing"""
    print("\n=== WILD MEMORY FOCUSED TESTING ===\n")

    gen = AriaGenerator(
        complexity=4,
        test_borrow_checker=False,
        test_wild_memory=True,
        test_tbb=False,
        test_control_flow=True
    )
    adv = AdversarialGenerator()

    return tester.run_full_suite(
        generator=gen,
        adversarial_generator=adv,
        valid_count=int(count * 0.6),
        adversarial_count=int(count * 0.35),
        determinism_count=int(count * 0.05)
    )


def print_banner():
    """Print the property testing banner"""
    print("""
    ╔══════════════════════════════════════════════════════════════╗
    ║           ARIA PROPERTY TESTING FRAMEWORK                    ║
    ║                   Phase 2.6 Validation                       ║
    ╠══════════════════════════════════════════════════════════════╣
    ║  Testing Properties:                                         ║
    ║    1. SOUNDNESS: Unsafe programs are REJECTED                ║
    ║    2. COMPLETENESS: Safe programs are ACCEPTED               ║
    ║    3. DETERMINISM: Same input → Same output                  ║
    ║    4. NO CRASH: Compiler never crashes on valid syntax       ║
    ╚══════════════════════════════════════════════════════════════╝
    """)


def main():
    parser = argparse.ArgumentParser(
        description="Aria Property Testing Framework - Phase 2.6"
    )

    # Test level selection
    level_group = parser.add_mutually_exclusive_group()
    level_group.add_argument("--quick", action="store_true",
                            help="Quick test: 100 programs (~30 seconds)")
    level_group.add_argument("--standard", action="store_true",
                            help="Standard test: 10,000 programs (~5 minutes)")
    level_group.add_argument("--full", action="store_true",
                            help="Full test: 100,000 programs (~1 hour)")
    level_group.add_argument("--marathon", action="store_true",
                            help="Marathon: 1,000,000 programs (~10 hours)")
    level_group.add_argument("--count", type=int, default=0,
                            help="Custom program count")

    # Focus areas
    parser.add_argument("--focus", choices=["borrow", "tbb", "wild", "all"],
                       default="all", help="Focus testing on specific area")

    # Options
    parser.add_argument("--compiler", type=str, default=None,
                       help="Path to ariac compiler")
    parser.add_argument("--output", type=str, default=None,
                       help="Output file for results (JSON)")
    parser.add_argument("--verbose", "-v", action="store_true",
                       help="Verbose output")
    parser.add_argument("--keep-artifacts", action="store_true",
                       help="Keep generated .aria files")
    parser.add_argument("--timeout", type=float, default=10.0,
                       help="Timeout per compilation (seconds)")
    parser.add_argument("--seed", type=int, default=None,
                       help="Random seed for reproducibility")

    args = parser.parse_args()

    print_banner()

    # Find compiler
    compiler = args.compiler or find_compiler()
    if not compiler or not os.path.isfile(compiler):
        print("ERROR: Cannot find Aria compiler (ariac)")
        print("Please specify path with --compiler or build the compiler first.")
        sys.exit(1)

    print(f"Compiler: {compiler}")
    print(f"Timeout:  {args.timeout}s per test")
    print(f"Verbose:  {args.verbose}")
    if args.seed is not None:
        print(f"Seed:     {args.seed}")
    print()

    # Create tester
    tester = PropertyTester(
        compiler_path=compiler,
        timeout_seconds=args.timeout,
        keep_artifacts=args.keep_artifacts,
        verbose=args.verbose
    )

    # Run tests
    start_time = time.time()

    if args.count > 0:
        count = args.count
    elif args.quick:
        count = 100
    elif args.standard:
        count = 10000
    elif args.full:
        count = 100000
    elif args.marathon:
        count = 1000000
    else:
        # Default to quick
        count = 100

    print(f"Target: {count:,} programs\n")

    if args.focus == "borrow":
        stats = run_borrow_checker_focus(tester, count)
    elif args.focus == "tbb":
        stats = run_tbb_focus(tester, count)
    elif args.focus == "wild":
        stats = run_wild_memory_focus(tester, count)
    else:
        # Run by level
        if count <= 100:
            stats = run_quick_test(tester)
        elif count <= 10000:
            stats = run_standard_test(tester)
        elif count <= 100000:
            stats = run_full_test(tester)
        else:
            stats = run_marathon_test(tester)

    # Save results
    if args.output:
        tester.save_results(args.output)
    else:
        # Auto-generate output filename
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        output_file = f"property_test_results_{timestamp}.json"
        output_path = os.path.join(
            os.path.dirname(os.path.abspath(__file__)),
            output_file
        )
        tester.save_results(output_path)

    # Final summary
    total_time = time.time() - start_time
    print(f"\nTotal time: {total_time:.1f} seconds")

    # Exit code based on soundness
    if stats.soundness_violations > 0:
        print(f"\n!!! {stats.soundness_violations} SOUNDNESS VIOLATIONS FOUND !!!")
        print("These are CRITICAL BUGS that must be fixed before v0.1.0 release.")
        sys.exit(1)
    elif stats.failed > 0:
        print(f"\n{stats.failed} failures (non-critical)")
        sys.exit(0)  # Non-critical failures don't fail the test
    else:
        print("\nAll tests passed!")
        sys.exit(0)


if __name__ == "__main__":
    main()
