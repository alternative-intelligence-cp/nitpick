#!/usr/bin/env python3
"""
Aria Grammar-Based Fuzzer

Generates valid Aria programs by composing grammar rules randomly.
Every generated program should parse successfully.

Mission: Ensure parser handles all valid syntax combinations.
"""

import random
import subprocess
import sys
import os
from pathlib import Path
from typing import List, Tuple

# Aria type keywords
NUMERIC_TYPES = [
    'int8', 'int16', 'int32', 'int64', 'int128', 'int256', 'int512', 'int1024',
    'uint8', 'uint16', 'uint32', 'uint64', 'uint128', 'uint256', 'uint512', 'uint1024',
    'tbb8', 'tbb16', 'tbb32', 'tbb64',
    'frac8', 'frac16', 'frac32', 'frac64',
    'tfp32', 'tfp64',
    'fix256',
    'flt32', 'flt64',
]

SIMPLE_TYPES = NUMERIC_TYPES + ['bool', 'string']

class AriaGrammarFuzzer:
    def __init__(self, seed=None):
        if seed:
            random.seed(seed)
        self.function_counter = 0
        self.var_counter = 0
        
    def generate_identifier(self, prefix='var') -> str:
        """Generate a unique identifier"""
        self.var_counter += 1
        return f"{prefix}_{self.var_counter}"
    
    def generate_literal(self, type_name: str) -> str:
        """Generate a literal value for a type"""
        if type_name == 'bool':
            return random.choice(['true', 'false'])
        elif type_name == 'string':
            return '"fuzzed_string"'
        elif type_name.startswith('int') or type_name.startswith('uint') or type_name.startswith('tbb'):
            # Safe small integers for testing
            return str(random.randint(0, 100))
        elif type_name.startswith('frac'):
            # Fraction literal {whole, num, denom}
            return f"{{{random.randint(0, 10)}, {random.randint(1, 10)}, {random.randint(1, 10)}}}"
        elif type_name.startswith('tfp'):
            # TFP literal {exponent, mantissa}
            return f"{{{random.randint(0, 5)}, {random.randint(1000, 9999)}}}"
        elif type_name.startswith('flt'):
            return f"{random.uniform(0.0, 100.0):.2f}"
        else:
            return "0"
    
    def generate_expression(self, type_name: str, depth: int = 0) -> str:
        """Generate an expression of the given type"""
        if depth > 3:  # Prevent infinite recursion
            return self.generate_literal(type_name)
        
        choices = ['literal', 'variable', 'binary_op']
        if depth < 2:
            choices.append('type_cast')
        
        choice = random.choice(choices)
        
        if choice == 'literal':
            return self.generate_literal(type_name)
        elif choice == 'variable':
            var_name = self.generate_identifier('x')
            return var_name
        elif choice == 'binary_op' and type_name in NUMERIC_TYPES:
            op = random.choice(['+', '-', '*', '/'])
            left = self.generate_expression(type_name, depth + 1)
            right = self.generate_expression(type_name, depth + 1)
            return f"({left} {op} {right})"
        elif choice == 'type_cast':
            # Type cast syntax: (expr:type)
            inner_type = random.choice(SIMPLE_TYPES)
            expr = self.generate_expression(inner_type, depth + 1)
            return f"({expr}:{type_name})"
        else:
            return self.generate_literal(type_name)
    
    def generate_statement(self, depth: int = 0) -> str:
        """Generate a random statement"""
        if depth > 5:
            return self.generate_simple_statement()
        
        choices = ['var_decl', 'if_stmt', 'while_stmt', 'pass_stmt']
        choice = random.choice(choices)
        
        if choice == 'var_decl':
            return self.generate_var_decl()
        elif choice == 'if_stmt':
            return self.generate_if_stmt(depth)
        elif choice == 'while_stmt':
            return self.generate_while_stmt(depth)
        elif choice == 'pass_stmt':
            type_name = random.choice(SIMPLE_TYPES)
            expr = self.generate_expression(type_name, 0)
            return f"    pass({expr});"
        else:
            return self.generate_simple_statement()
    
    def generate_simple_statement(self) -> str:
        """Generate a simple statement (no nesting)"""
        return self.generate_var_decl()
    
    def generate_var_decl(self) -> str:
        """Generate a variable declaration"""
        type_name = random.choice(SIMPLE_TYPES)
        var_name = self.generate_identifier('v')
        expr = self.generate_expression(type_name, 0)
        return f"    {type_name}:{var_name} = {expr};"
    
    def generate_if_stmt(self, depth: int) -> str:
        """Generate an if statement"""
        condition = self.generate_expression('bool', 0)
        body = '\n'.join([self.generate_statement(depth + 1) for _ in range(random.randint(1, 3))])
        
        if random.choice([True, False]):
            else_body = '\n'.join([self.generate_statement(depth + 1) for _ in range(random.randint(1, 3))])
            return f"    if ({condition}) {{\n{body}\n    }} else {{\n{else_body}\n    }}"
        else:
            return f"    if ({condition}) {{\n{body}\n    }}"
    
    def generate_while_stmt(self, depth: int) -> str:
        """Generate a while statement"""
        condition = self.generate_expression('bool', 0)
        body = '\n'.join([self.generate_statement(depth + 1) for _ in range(random.randint(1, 2))])
        return f"    while ({condition}) {{\n{body}\n    }}"
    
    def generate_function(self) -> str:
        """Generate a complete function"""
        self.function_counter += 1
        func_name = f"fuzz_func_{self.function_counter}"
        return_type = random.choice(SIMPLE_TYPES)
        
        # Generate 1-5 statements
        num_statements = random.randint(1, 5)
        statements = [self.generate_statement(0) for _ in range(num_statements)]
        
        # Add a return statement
        return_expr = self.generate_expression(return_type, 0)
        statements.append(f"    pass({return_expr});")
        
        body = '\n'.join(statements)
        
        return f"func:{func_name} = {return_type}() {{\n{body}\n}};"
    
    def generate_program(self, num_functions: int = 3) -> str:
        """Generate a complete Aria program"""
        functions = [self.generate_function() for _ in range(num_functions)]
        return '\n\n'.join(functions)

def test_generated_program(program: str, ariac_path: str) -> Tuple[bool, str]:
    """Test if a generated program parses successfully"""
    # Write to temporary file
    test_file = '/tmp/fuzz_test.aria'
    with open(test_file, 'w') as f:
        f.write(program)
    
    # Run parser
    try:
        result = subprocess.run(
            [ariac_path, test_file, '--ast-dump'],
            capture_output=True,
            text=True,
            timeout=5
        )
        
        # Success if AST is generated
        if 'AST:' in result.stdout or 'AST:' in result.stderr:
            return True, "PASS"
        else:
            return False, result.stderr
    except subprocess.TimeoutExpired:
        return False, "TIMEOUT"
    except Exception as e:
        return False, f"EXCEPTION: {str(e)}"

def main():
    import argparse
    parser = argparse.ArgumentParser(description='Aria Grammar-Based Fuzzer')
    parser.add_argument('--iterations', type=int, default=1000, help='Number of programs to generate')
    parser.add_argument('--seed', type=int, help='Random seed for reproducibility')
    parser.add_argument('--ariac', type=str, default='./build/ariac', help='Path to ariac compiler')
    parser.add_argument('--save-failures', action='store_true', help='Save failed programs')
    args = parser.parse_args()
    
    # Check if ariac exists
    if not os.path.exists(args.ariac):
        print(f"❌ Error: ariac not found at {args.ariac}")
        print("   Run ./build.sh first")
        sys.exit(1)
    
    fuzzer = AriaGrammarFuzzer(seed=args.seed)
    
    passed = 0
    failed = 0
    failures = []
    
    print(f"🔍 Starting grammar fuzzer: {args.iterations} iterations")
    print(f"{'='*70}")
    
    for i in range(args.iterations):
        program = fuzzer.generate_program(num_functions=random.randint(1, 5))
        success, message = test_generated_program(program, args.ariac)
        
        if success:
            passed += 1
            if (i + 1) % 100 == 0:
                print(f"✅ {i+1}/{args.iterations} - {passed} passed, {failed} failed")
        else:
            failed += 1
            failures.append((i, program, message))
            print(f"❌ Iteration {i+1} FAILED: {message[:50]}")
            
            if args.save_failures:
                failure_file = f'/tmp/fuzz_failure_{i}.aria'
                with open(failure_file, 'w') as f:
                    f.write(program)
                print(f"   Saved to: {failure_file}")
    
    print(f"\n{'='*70}")
    print(f"🎯 FUZZING COMPLETE")
    print(f"{'='*70}")
    print(f"Total:  {args.iterations}")
    print(f"Passed: {passed} ({100*passed/args.iterations:.1f}%)")
    print(f"Failed: {failed} ({100*failed/args.iterations:.1f}%)")
    
    if failed > 0:
        print(f"\n⚠️  {failed} FAILURES DETECTED")
        print("   Valid programs that failed to parse - PARSER BUG LIKELY")
        sys.exit(1)
    else:
        print(f"\n✅ ALL TESTS PASSED - Parser handled all valid syntax")
        sys.exit(0)

if __name__ == '__main__':
    main()
