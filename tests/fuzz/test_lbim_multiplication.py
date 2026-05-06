#!/usr/bin/env python3
"""
LBIM Multiplication Fuzz Test
Tests int256 multiplication with 1M random test cases against GMP oracle.

Per Gemini recommendation: "Run $10^6$ iterations. This will expose edge cases
in carry propagation that human-written tests miss."
"""

import subprocess
import random
import sys
import time
from multiprocessing import Pool, cpu_count


# Check for GMP (we'll use Python's built-in bigint as oracle)
def test_multiplication(case_num, a_str, b_str):
    """Test a single multiplication case"""
    # Python's arbitrary precision as oracle
    oracle = int(a_str) * int(b_str)

    # For now, just test that multiplication compiles without crashing
    # TODO: Add print support for int256 to validate results
    aria_code = f"""
func:main = int32() {{
    int256:a = {a_str};
    int256:b = {b_str};
    int256:result = a * b;
    test_file = f"/tmp/mult_test_{case_num}.npk"
    with open(test_file, "w") as f:
        f.write(aria_code)

    try:
        # Compile
        compile_result = subprocess.run(
            ["./npkc", test_file, "-o", f"/tmp/mult_test_{case_num}"],
            cwd="/home/randy/._____RANDY_____/REPOS/aria/build",
            capture_output=True,
            timeout=5,
        )

        if compile_result.returncode != 0:
            return (
                case_num,
                "COMPILE_FAIL",
                a_str,
                b_str,
                compile_result.stderr.decode()[:200],
            )

        # Run
        run_result = subprocess.run(
            [f"/tmp/mult_test_{case_num}"], capture_output=True, timeout=5
        )

        if run_result.returncode != 0:
            return (
                case_num,
                "RUN_FAIL",
                a_str,
                b_str,
                run_result.stderr.decode()[:200],
            )

        # Parse output
        aria_result_str = run_result.stdout.decode().strip()
        try:
            aria_result = int(aria_result_str)
        except ValueError:
            return (
                case_num,
                "PARSE_FAIL",
                a_str,
                b_str,
                f"Could not parse output: {aria_result_str[:100]}",
            )

        # Compare
        if aria_result != oracle:
            return (
                case_num,
                "MISMATCH",
                a_str,
                b_str,
                f"Aria: {aria_result}, Oracle: {oracle}",
            )

        return (case_num, "PASS", "", "", "")

    except subprocess.TimeoutExpired:
        return (case_num, "TIMEOUT", a_str, b_str, "")
    finally:
        # Cleanup
        subprocess.run(
            ["rm", "-f", test_file, f"/tmp/mult_test_{case_num}"],
            stderr=subprocess.DEVNULL,
        )


def generate_test_case(case_num):
    """Generate random int256 values for testing"""
    # Int256 range: -2^255 to 2^255-1
    max_val = 2**255 - 1
    min_val = -(2**255)

    # Different test patterns
    pattern = case_num % 10

    if pattern == 0:
        # Random full range
        a = random.randint(min_val, max_val)
        b = random.randint(min_val, max_val)
    elif pattern == 1:
        # Small numbers (more likely to succeed)
        a = random.randint(-1000, 1000)
        b = random.randint(-1000, 1000)
    elif pattern == 2:
        # Edge values
        edges = [0, 1, -1, max_val, min_val, max_val // 2, min_val // 2]
        a = random.choice(edges)
        b = random.choice(edges)
    elif pattern == 3:
        # Powers of 2
        a = 2 ** random.randint(0, 250)
        b = 2 ** random.randint(0, 250)
        if random.random() > 0.5:
            a = -a
        if random.random() > 0.5:
            b = -b
    elif pattern == 4:
        # One large, one small
        a = random.randint(min_val, max_val)
        b = random.randint(-100, 100)
    elif pattern == 5:
        # Carry propagation tests (alternating bits)
        a = int("10" * 127, 2)  # 01010101...
        b = random.randint(1, 1000)
    elif pattern == 6:
        # All ones in limbs
        a = (1 << 64) - 1  # Single limb all 1s
        b = random.randint(1, max_val)
    elif pattern == 7:
        # Prime numbers (using small primes * large primes)
        small_primes = [2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31]
        a = random.choice(small_primes) * (2 ** random.randint(200, 250))
        b = random.choice(small_primes) * (2 ** random.randint(200, 250))
    elif pattern == 8:
        # Sequential values (test off-by-one)
        base = random.randint(min_val // 2, max_val // 2)
        a = base
        b = base + random.randint(-10, 10)
    else:  # pattern == 9
        # Mixed sign combinations
        a = random.randint(0, max_val)
        b = random.randint(min_val, 0)

    return (case_num, str(a), str(b))


def worker(args):
    """Worker function for multiprocessing"""
    case_num, a_str, b_str = args
    return test_multiplication(case_num, a_str, b_str)


def main():
    print("LBIM int256 Multiplication Fuzz Test")
    print("=" * 60)
    print(f"Target: 1,000,000 iterations")
    print(f"CPU cores: {cpu_count()}")
    print(f"Parallelism: {min(cpu_count(), 48)}")
    print()

    # Check compiler exists
    import os

    if not os.path.exists("/home/randy/._____RANDY_____/REPOS/aria/build/npkc"):
        print("ERROR: Compiler not found at build/npkc")
        print("Run: cd ~/._____RANDY_____/REPOS/aria && ./build.sh")
        sys.exit(1)

    total_tests = 1_000_000
    batch_size = 10000
    workers = min(cpu_count(), 48)  # Use up to 48 cores

    start_time = time.time()
    passed = 0
    failed = 0
    failures = []

    print(f"Starting fuzz campaign...")
    print(f"Progress: ", end="", flush=True)

    # Generate all test cases
    test_cases = [generate_test_case(i) for i in range(total_tests)]

    # Run in parallel batches
    with Pool(workers) as pool:
        for i, result in enumerate(
            pool.imap_unordered(worker, test_cases, chunksize=100)
        ):
            case_num, status, a, b, error = result

            if status == "PASS":
                passed += 1
            else:
                failed += 1
                failures.append((case_num, status, a, b, error))

            # Progress indicator
            if (i + 1) % 10000 == 0:
                elapsed = time.time() - start_time
                rate = (i + 1) / elapsed
                eta = (total_tests - i - 1) / rate if rate > 0 else 0
                print(
                    f"\n[{i+1}/{total_tests}] {passed} pass, {failed} fail "
                    f"({rate:.0f} tests/sec, ETA: {eta:.0f}s)",
                    end="",
                    flush=True,
                )

    elapsed = time.time() - start_time

    print("\n")
    print("=" * 60)
    print("RESULTS")
    print("=" * 60)
    print(f"Total tests:  {total_tests:,}")
    print(f"Passed:       {passed:,} ({100*passed/total_tests:.2f}%)")
    print(f"Failed:       {failed:,} ({100*failed/total_tests:.2f}%)")
    print(f"Time:         {elapsed:.1f}s ({total_tests/elapsed:.0f} tests/sec)")
    print()

    if failures:
        print("FAILURES:")
        print("-" * 60)
        for case_num, status, a, b, error in failures[:20]:  # Show first 20
            print(f"Case {case_num}: {status}")
            if a and b:
                print(f"  a = {a[:50]}...")
                print(f"  b = {b[:50]}...")
            if error:
                print(f"  {error[:100]}")
            print()

        if len(failures) > 20:
            print(f"... and {len(failures) - 20} more failures")

        sys.exit(1)
    else:
        print("✅ ALL TESTS PASSED!")
        print()
        print("LBIM multiplication is correct for 1M random test cases.")
        print("Carry propagation, sign handling, and edge cases validated.")
        sys.exit(0)


if __name__ == "__main__":
    main()
