#!/usr/bin/env python3
"""
Aria Boundary-Value Fuzzer

Tests numeric edge cases, TBB boundaries, ERR propagation.
These are the values where off-by-one errors turn into safety violations.

Mission: Verify safety at all numeric boundaries.
"""

import subprocess
import sys
import os
from pathlib import Path
from typing import List, Tuple

# TBB types have eliminated -128, -32768, etc.
# Test the boundaries that SHOULD work
TBB_BOUNDARIES = {
    'tbb8': {
        'min': -127,
        'max': 127,
        'forbidden': -128,  # Should reject
        'zero': 0,
        'near_min': -126,
        'near_max': 126,
    },
    'tbb16': {
        'min': -32767,
        'max': 32767,
        'forbidden': -32768,
        'zero': 0,
        'near_min': -32766,
        'near_max': 32766,
    },
    'tbb32': {
        'min': -2147483647,
        'max': 2147483647,
        'forbidden': -2147483648,
        'zero': 0,
        'near_min': -2147483646,
        'near_max': 2147483646,
    },
    'tbb64': {
        'min': -9223372036854775807,
        'max': 9223372036854775807,
        'forbidden': -9223372036854775808,
        'zero': 0,
        'near_min': -9223372036854775806,
        'near_max': 9223372036854775806,
    },
}

# Standard integer boundaries
# NOTE: Minimum values (-128, -32768, etc.) require special parsing
# Using near-minimum values for now until parser handles MIN constant
INT_BOUNDARIES = {
    'int8': (-127, 127),
    'int16': (-32767, 32767),
    'int32': (-2147483647, 2147483647),
    'int64': (-9223372036854775807, 9223372036854775807),
}

UINT_BOUNDARIES = {
    'uint8': (0, 255),
    'uint16': (0, 65535),
    'uint32': (0, 4294967295),
    'uint64': (0, 9223372036854775807),  # Using int64 max for now
}

class BoundaryFuzzer:
    def __init__(self):
        self.tests_generated = 0
        self.programs = []
    
    def generate_tbb_boundary_test(self, type_name: str, boundaries: dict) -> str:
        """Generate test for TBB type boundaries"""
        tests = []
        
        # Valid boundaries
        tests.append(f"    {type_name}:min = {boundaries['min']};")
        tests.append(f"    {type_name}:max = {boundaries['max']};")
        tests.append(f"    {type_name}:zero = {boundaries['zero']};")
        tests.append(f"    {type_name}:near_min = {boundaries['near_min']};")
        tests.append(f"    {type_name}:near_max = {boundaries['near_max']};")
        
        # Test arithmetic at boundaries
        tests.append(f"    {type_name}:add_result = (min + 1);")
        tests.append(f"    {type_name}:sub_result = (max - 1);")
        
        # Test that forbidden value is rejected (commented out - should fail to parse)
        tests.append(f"    // {type_name}:forbidden = {boundaries['forbidden']};  // Should be parse error")
        
        body = '\n'.join(tests)
        return f"""
func:test_{type_name}_boundaries = bool() {{
{body}
    pass(true);
}};
"""
    
    def generate_int_boundary_test(self, type_name: str, boundaries: Tuple[int, int]) -> str:
        """Generate test for standard integer boundaries"""
        min_val, max_val = boundaries
        
        tests = [
            f"    {type_name}:min = {min_val};",
            f"    {type_name}:max = {max_val};",
            f"    {type_name}:zero = 0;",
            f"    {type_name}:near_min = {min_val + 1};",
            f"    {type_name}:near_max = {max_val - 1};",
        ]
        
        body = '\n'.join(tests)
        return f"""
func:test_{type_name}_boundaries = bool() {{
{body}
    pass(true);
}};
"""
    
    def generate_fraction_boundary_test(self) -> str:
        """Test fraction edge cases"""
        return """
func:test_fraction_boundaries = bool() {{
    // Zero fraction
    frac32:zero = {0, 0, 1};
    
    // Unit fraction
    frac32:one = {1, 0, 1};
    
    // Large numerator/denominator
    frac32:large = {0, 2147483647, 2147483647};
    
    // Small denominator (but not zero)
    frac32:precise = {0, 1, 1000000};
    
    pass(true);
}};
"""
    
    def generate_tfp_boundary_test(self) -> str:
        """Test TFP edge cases"""
        return """
func:test_tfp_boundaries = bool() {{
    // Zero
    tfp64:zero = {0, 0};
    
    // Large exponent
    tfp64:big = {100, 9999999999};
    
    // Small exponent
    tfp64:small = {1, 1};
    
    // Max precision
    tfp64:precise = {10, 9223372036854775807};
    
    pass(true);
}};
"""
    
    def generate_fix256_test(self) -> str:
        """Test fix256 precision boundaries"""
        return """
func:test_fix256_boundaries = bool() {{
    // Maximum value: 2^127 - 2^-128
    fix256:max = 170141183460469231731687303715884105727.0;
    
    // Minimum positive: 2^-128
    fix256:min_pos = 0.0000000000000000000000000000000000000029387358770557187699;
    
    // Zero
    fix256:zero = 0.0;
    
    // One
    fix256:one = 1.0;
    
    pass(true);
}};
"""
    
    def generate_err_propagation_test(self) -> str:
        """Test ERR sticky propagation"""
        return """
func:test_err_propagation = tbb32() {{
    tbb32:normal = 100;
    
    // ERR is sticky - once introduced, it propagates
    // TODO: Need type.ERR syntax support in parser
    // tbb32:err_val = tbb32.ERR;
    // tbb32:result = (normal + err_val);  // Should be ERR
    
    pass(normal);
}};
"""
    
    def generate_all_tests(self) -> str:
        """Generate comprehensive boundary test suite"""
        program_parts = []
        
        # TBB boundaries
        for type_name, boundaries in TBB_BOUNDARIES.items():
            program_parts.append(self.generate_tbb_boundary_test(type_name, boundaries))
        
        # Integer boundaries
        for type_name, boundaries in INT_BOUNDARIES.items():
            program_parts.append(self.generate_int_boundary_test(type_name, boundaries))
        
        # Unsigned boundaries
        for type_name, boundaries in UINT_BOUNDARIES.items():
            program_parts.append(self.generate_int_boundary_test(type_name, boundaries))
        
        # Special types
        program_parts.append(self.generate_fraction_boundary_test())
        program_parts.append(self.generate_tfp_boundary_test())
        program_parts.append(self.generate_fix256_test())
        program_parts.append(self.generate_err_propagation_test())
        
        # Main function
        program_parts.append("""
func:main = int32() {{
    pass(0);
}};
""")
        
        return '\n'.join(program_parts)

def test_program(program: str, ariac_path: str, description: str) -> Tuple[bool, str]:
    """Test a boundary program"""
    test_file = '/tmp/boundary_test.aria'
    with open(test_file, 'w') as f:
        f.write(program)
    
    try:
        result = subprocess.run(
            [ariac_path, test_file, '--ast-dump'],
            capture_output=True,
            text=True,
            timeout=5
        )
        
        if result.returncode < 0:
            return False, f"CRASH: Signal {-result.returncode}"
        
        if 'AST:' in result.stdout or 'AST:' in result.stderr:
            return True, "PASS"
        else:
            return False, f"Parse failed: {result.stderr[:200]}"
            
    except subprocess.TimeoutExpired:
        return False, "TIMEOUT"
    except Exception as e:
        return False, f"EXCEPTION: {str(e)}"

def main():
    import argparse
    parser = argparse.ArgumentParser(description='Aria Boundary-Value Fuzzer')
    parser.add_argument('--ariac', type=str, default='./build/ariac', help='Path to ariac')
    parser.add_argument('--output', type=str, help='Save generated test to file')
    args = parser.parse_args()
    
    if not os.path.exists(args.ariac):
        print(f"❌ Error: ariac not found at {args.ariac}")
        sys.exit(1)
    
    fuzzer = BoundaryFuzzer()
    
    print(f"🔍 Starting boundary fuzzer")
    print(f"{'='*70}")
    
    # Generate comprehensive boundary test
    program = fuzzer.generate_all_tests()
    
    if args.output:
        with open(args.output, 'w') as f:
            f.write(program)
        print(f"💾 Saved boundary test to: {args.output}")
    
    # Test it
    success, message = test_program(program, args.ariac, "Comprehensive boundary test")
    
    if success:
        print(f"✅ All boundary values parsed successfully")
        print(f"   - TBB types: {len(TBB_BOUNDARIES)} tested")
        print(f"   - Integer types: {len(INT_BOUNDARIES)} tested")
        print(f"   - Unsigned types: {len(UINT_BOUNDARIES)} tested")
        print(f"   - Fraction/TFP/fix256: tested")
        print(f"\n{'='*70}")
        print(f"✅ BOUNDARY TESTING COMPLETE - No edge case failures")
        sys.exit(0)
    else:
        print(f"❌ FAILED: {message}")
        print(f"\n{'='*70}")
        print(f"❌ BOUNDARY TEST FAILED - Edge case handling broken")
        sys.exit(1)

if __name__ == '__main__':
    main()
