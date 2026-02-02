#!/usr/bin/env python3
"""
Aria Mutation-Based Fuzzer

Takes valid Aria programs and applies controlled mutations.
Parser should handle corrupted input gracefully - no crashes, clear errors.

Mission: Ensure parser fails safely on invalid input.
"""

import random
import subprocess
import sys
import os
from pathlib import Path
from typing import List, Tuple

class MutationFuzzer:
    def __init__(self, seed=None):
        if seed:
            random.seed(seed)
    
    def mutate_bit_flip(self, data: str) -> str:
        """Flip random bits in the source code (simulate cosmic rays)"""
        if not data:
            return data
        
        data_bytes = bytearray(data.encode('utf-8'))
        # Flip 1-3 random bits
        num_flips = random.randint(1, 3)
        for _ in range(num_flips):
            if len(data_bytes) == 0:
                break
            byte_idx = random.randint(0, len(data_bytes) - 1)
            bit_idx = random.randint(0, 7)
            data_bytes[byte_idx] ^= (1 << bit_idx)
        
        try:
            return data_bytes.decode('utf-8', errors='replace')
        except:
            return data  # Return original if decoding fails
    
    def mutate_delete_char(self, data: str) -> str:
        """Delete a random character"""
        if len(data) < 2:
            return data
        idx = random.randint(0, len(data) - 1)
        return data[:idx] + data[idx+1:]
    
    def mutate_insert_char(self, data: str) -> str:
        """Insert a random character"""
        chars = 'abcdefghijklmnopqrstuvwxyz0123456789;:{}()[]<>+-*/ \n\t'
        idx = random.randint(0, len(data))
        char = random.choice(chars)
        return data[:idx] + char + data[idx:]
    
    def mutate_delete_line(self, data: str) -> str:
        """Delete a random line"""
        lines = data.split('\n')
        if len(lines) <= 1:
            return data
        del_idx = random.randint(0, len(lines) - 1)
        del lines[del_idx]
        return '\n'.join(lines)
    
    def mutate_duplicate_line(self, data: str) -> str:
        """Duplicate a random line"""
        lines = data.split('\n')
        if not lines:
            return data
        dup_idx = random.randint(0, len(lines) - 1)
        lines.insert(dup_idx + 1, lines[dup_idx])
        return '\n'.join(lines)
    
    def mutate_swap_tokens(self, data: str) -> str:
        """Swap two random tokens"""
        tokens = data.split()
        if len(tokens) < 2:
            return data
        idx1 = random.randint(0, len(tokens) - 1)
        idx2 = random.randint(0, len(tokens) - 1)
        tokens[idx1], tokens[idx2] = tokens[idx2], tokens[idx1]
        return ' '.join(tokens)
    
    def mutate_truncate(self, data: str) -> str:
        """Truncate at random position"""
        if len(data) < 10:
            return data
        cut_point = random.randint(len(data) // 2, len(data) - 1)
        return data[:cut_point]
    
    def mutate_corrupt_number(self, data: str) -> str:
        """Change a numeric literal to an extreme value"""
        import re
        numbers = list(re.finditer(r'\b\d+\b', data))
        if not numbers:
            return data
        
        match = random.choice(numbers)
        # Replace with boundary value
        replacement = random.choice(['0', '255', '32767', '2147483647', '-128', '-32768'])
        return data[:match.start()] + replacement + data[match.end():]
    
    def apply_mutation(self, data: str) -> Tuple[str, str]:
        """Apply a random mutation and return (mutated_data, mutation_name)"""
        mutations = [
            (self.mutate_bit_flip, 'bit_flip'),
            (self.mutate_delete_char, 'delete_char'),
            (self.mutate_insert_char, 'insert_char'),
            (self.mutate_delete_line, 'delete_line'),
            (self.mutate_duplicate_line, 'duplicate_line'),
            (self.mutate_swap_tokens, 'swap_tokens'),
            (self.mutate_truncate, 'truncate'),
            (self.mutate_corrupt_number, 'corrupt_number'),
        ]
        
        mutation_func, mutation_name = random.choice(mutations)
        return mutation_func(data), mutation_name

def test_mutated_program(program: str, ariac_path: str) -> Tuple[str, str]:
    """Test if parser handles corrupted input safely"""
    test_file = '/tmp/mutation_test.aria'
    with open(test_file, 'w') as f:
        f.write(program)
    
    try:
        result = subprocess.run(
            [ariac_path, test_file, '--ast-dump'],
            capture_output=True,
            text=True,
            timeout=5
        )
        
        # Check for crashes
        if result.returncode < 0:
            return 'CRASH', f'Signal {-result.returncode}'
        
        # Undefined behavior indicators
        if 'Segmentation fault' in result.stderr or 'core dumped' in result.stderr:
            return 'CRASH', 'Segmentation fault'
        
        # Good outcomes: either parses or gives clear error
        if 'AST:' in result.stdout or 'AST:' in result.stderr:
            return 'PARSE', 'Unexpectedly parsed invalid input'
        elif 'Error' in result.stderr or 'error' in result.stderr:
            return 'ERROR', 'Graceful error'
        else:
            return 'UNKNOWN', result.stderr[:100]
            
    except subprocess.TimeoutExpired:
        return 'TIMEOUT', 'Parser hung'
    except Exception as e:
        return 'EXCEPTION', str(e)

def main():
    import argparse
    parser = argparse.ArgumentParser(description='Aria Mutation-Based Fuzzer')
    parser.add_argument('--test-dir', type=str, default='./tests', help='Directory with valid test files')
    parser.add_argument('--iterations', type=int, default=1000, help='Mutations per test file')
    parser.add_argument('--seed', type=int, help='Random seed')
    parser.add_argument('--ariac', type=str, default='./build/ariac', help='Path to ariac')
    parser.add_argument('--save-crashes', action='store_true', help='Save programs that crash parser')
    args = parser.parse_args()
    
    if not os.path.exists(args.ariac):
        print(f"❌ Error: ariac not found at {args.ariac}")
        sys.exit(1)
    
    # Find all .aria test files
    test_files = list(Path(args.test_dir).glob('*.aria'))
    if not test_files:
        print(f"❌ No test files found in {args.test_dir}")
        sys.exit(1)
    
    fuzzer = MutationFuzzer(seed=args.seed)
    
    stats = {
        'total': 0,
        'CRASH': 0,
        'TIMEOUT': 0,
        'ERROR': 0,  # Good - graceful error
        'PARSE': 0,  # Weird - invalid input parsed
        'UNKNOWN': 0,
        'EXCEPTION': 0,
    }
    
    crashes = []
    
    print(f"🔍 Starting mutation fuzzer")
    print(f"   Test files: {len(test_files)}")
    print(f"   Iterations per file: {args.iterations}")
    print(f"{'='*70}")
    
    for test_file in test_files:
        # Skip diagnostic files
        if 'diagnostic' in test_file.name or 'debug_' in test_file.name:
            continue
        
        print(f"📝 Fuzzing: {test_file.name}")
        
        # Read original program
        try:
            with open(test_file, 'r') as f:
                original = f.read()
        except:
            continue
        
        for i in range(args.iterations):
            mutated, mutation_name = fuzzer.apply_mutation(original)
            status, message = test_mutated_program(mutated, args.ariac)
            
            stats['total'] += 1
            stats[status] += 1
            
            if status == 'CRASH':
                print(f"   🔥 CRASH on {mutation_name}: {message}")
                crashes.append((test_file.name, mutation_name, mutated, message))
                
                if args.save_crashes:
                    crash_file = f'/tmp/crash_{test_file.stem}_{i}.aria'
                    with open(crash_file, 'w') as f:
                        f.write(mutated)
                    print(f"      Saved to: {crash_file}")
            
            elif status == 'TIMEOUT':
                print(f"   ⏱️  TIMEOUT on {mutation_name}")
            
            if (i + 1) % 100 == 0:
                errors = stats['ERROR']
                total = stats['total']
                print(f"      {i+1}/{args.iterations} - {errors} graceful errors")
    
    print(f"\n{'='*70}")
    print(f"🎯 MUTATION FUZZING COMPLETE")
    print(f"{'='*70}")
    print(f"Total mutations: {stats['total']}")
    print(f"Graceful errors: {stats['ERROR']} ✅")
    print(f"Crashes:         {stats['CRASH']} {'🔥' if stats['CRASH'] > 0 else '✅'}")
    print(f"Timeouts:        {stats['TIMEOUT']} {'⏱️' if stats['TIMEOUT'] > 0 else '✅'}")
    print(f"Unexpected parse:{stats['PARSE']} {'⚠️' if stats['PARSE'] > 0 else '✅'}")
    
    if stats['CRASH'] > 0 or stats['TIMEOUT'] > 0:
        print(f"\n❌ CRITICAL: Parser crashed or hung on corrupted input")
        print(f"   This is a SAFETY VIOLATION - children's systems cannot crash")
        sys.exit(1)
    else:
        print(f"\n✅ Parser handled all corrupted inputs safely")
        sys.exit(0)

if __name__ == '__main__':
    main()
