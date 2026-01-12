#!/usr/bin/env python3
"""
Quick LBIM Multiplication Test
Tests compilation and execution of int256 multiplication (no output validation yet).
"""

import subprocess
import random
import sys

def test_mult(a, b):
    """Test one multiplication"""
    aria_code = f"""
func:main = int32() {{
    int256:x = {a};
    int256:y = {b};
    int256:z = x * y;
    pass(0);
}};
"""
    
    with open('/tmp/test_mult.aria', 'w') as f:
        f.write(aria_code)
    
    # Compile
    result = subprocess.run(
        ['./build/ariac', '/tmp/test_mult.aria', '-o', '/tmp/test_mult'],
        capture_output=True,
        timeout=5
    )
    
    if result.returncode != 0:
        print(f"COMPILE FAIL: {a} * {b}")
        print(result.stderr.decode()[:500])
        return False
    
    # Run
    result = subprocess.run(['/tmp/test_mult'], capture_output=True, timeout=5)
    
    if result.returncode != 0:
        print(f"RUN FAIL: {a} * {b}")
        print(result.stderr.decode()[:500])
        return False
    
    return True

# Test cases
tests = [
    (0, 0),
    (1, 1),
    (42, 100),
    (-1, 1),
    (-1, -1),
    (2**64, 2**64),  # Limb boundary
    (2**128, 2),      # Multi-limb
    ((2**64)-1, (2**64)-1),  # Max limb values
]

print("Testing int256 multiplication...")
passed = 0
failed = 0

for a, b in tests:
    if test_mult(a, b):
        passed += 1
        print(".", end="", flush=True)
    else:
        failed += 1
        print("F", end="", flush=True)

print(f"\n\n{passed} passed, {failed} failed")
sys.exit(0 if failed == 0 else 1)
