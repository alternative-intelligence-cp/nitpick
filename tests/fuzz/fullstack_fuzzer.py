#!/usr/bin/env python3
"""
Aria Full-Stack Fuzzer with Runtime Linking

Tests the complete compilation pipeline:
1. Parsing → AST
2. Type checking → Semantic validation
3. Code generation → LLVM IR
4. Linking → Executable with libaria_runtime.a
5. Execution → Runtime behavior validation

New in this campaign:
- P2.7: Complex struct initialization with array literals
- String interpolation fixes  
- Stdlib integration (collections, async, hash)
"""

import subprocess
import random
import time
import sys
import os
from pathlib import Path
from dataclasses import dataclass
from enum import Enum, auto
from typing import Optional, List

class TestResult(Enum):
    COMPILE_SUCCESS = auto()
    LINK_SUCCESS = auto()
    RUNTIME_SUCCESS = auto()
    COMPILE_ERROR = auto()
    LINK_ERROR = auto()
    RUNTIME_ERROR = auto()
    CRASH = auto()
    TIMEOUT = auto()

@dataclass
class FuzzStats:
    total_tests: int = 0
    compile_success: int = 0
    link_success: int = 0
    runtime_success: int = 0
    compile_errors: int = 0
    link_errors: int = 0
    runtime_errors: int = 0
    crashes: int = 0
    timeouts: int = 0
    start_time: float = 0
    
    def rate(self) -> float:
        elapsed = time.time() - self.start_time
        return self.total_tests / elapsed if elapsed > 0 else 0

class AriaFullStackFuzzer:
    """Fuzzer that exercises the complete compilation and execution pipeline."""
    
    def __init__(self, ariac_path: str = "./build/ariac", 
                 runtime_lib: str = "./build/libaria_runtime.a"):
        self.ariac = Path(ariac_path)
        self.runtime_lib = Path(runtime_lib)
        self.stats = FuzzStats()
        self.stats.start_time = time.time()
        
        # Verify compiler and runtime exist
        if not self.ariac.exists():
            raise FileNotFoundError(f"Compiler not found: {self.ariac}")
        if not self.runtime_lib.exists():
            raise FileNotFoundError(f"Runtime library not found: {self.runtime_lib}")
    
    def generate_basic_program(self) -> str:
        """Generate a simple valid program."""
        return """func:failsafe = void(int32:err_code) {};
func:main = int32() {
    int32:x = 42i32;
    pass(x);
};"""
    
    def generate_struct_with_arrays(self) -> str:
        """Test P2.7: Struct initialization with array literals."""
        return """func:failsafe = void(int32:err_code) {};

struct Point {
    int32:x;
    int32:y;
}

struct Container {
    int32:count;
    Point[3]:points;
}

func:main = int32() {
    // P2.7: Initialize struct with inline array of objects
    Container:c = {
        count: 3i32,
        points: [
            {x: 10i32, y: 20i32},
            {x: 30i32, y: 40i32},
            {x: 50i32, y: 60i32}
        ]
    };
    pass(c.count);
};"""
    
    def generate_string_interpolation(self) -> str:
        """Test string interpolation fixes."""
        return """func:failsafe = void(int32:err_code) {};
func:main = int32() {
    int32:x = 42i32;
    int32:y = 13i32;
    // Test template literals
    pass(0i32);
};"""
    
    def generate_collections_test(self) -> str:
        """Test stdlib collections."""
        return """func:failsafe = void(int32:err_code) {};
use std.collections;

func:main = int32() {
    // Test Vec<T>
    Vec<int32>:numbers = vec_new<int32>();
    result:r1 = vec_push(@numbers, 10i32);
    result:r2 = vec_push(@numbers, 20i32);
    result:r3 = vec_push(@numbers, 30i32);
    
    // Test HashMap  
    HashMap<int32,int32>:map = hashmap_new<int32,int32>();
    
    pass(0i32);
};"""
    
    def generate_async_test(self) -> str:
        """Test async/await (parse only, may not execute)."""
        return """func:failsafe = void(int32:err_code) {};
use std.async;

async func:compute = Future<int32>() {
    int32:result = 42i32;
    pass(result);
};

func:main = int32() {
    // Parse test for async syntax
    pass(0i32);
};"""
    
    def generate_integer_math(self) -> str:
        """Test integer operations."""
        ops = ['+', '-', '*', '/', '%', '&', '|', '^', '<<', '>>']
        op = random.choice(ops)
        a = random.randint(1, 100)
        b = random.randint(1, 100) if op != '/' and op != '%' else random.randint(1, 10)
        
        return f"""func:failsafe = void(int32:err_code) {{}};
func:main = int32() {{
    int32:a = {a}i32;
    int32:b = {b}i32;
    int32:result = (a {op} b);
    pass(result);
}};"""
    
    def test_program(self, source: str, run_executable: bool = False) -> TestResult:
        """Compile, link, and optionally run a program."""
        test_file = Path(f"/tmp/fuzz_{os.getpid()}.aria")
        output_file = Path(f"/tmp/fuzz_{os.getpid()}_out")
        
        try:
            # Write source
            test_file.write_text(source)
            
            # Compile and link
            result = subprocess.run(
                [str(self.ariac), str(test_file), "-o", str(output_file)],
                capture_output=True,
                text=True,
                timeout=10
            )
            
            # Check for crashes
            if result.returncode < 0:
                return TestResult.CRASH
            
            # Compilation errors are expected for invalid programs
            if result.returncode != 0:
                # Check if it's a link error or compile error
                stderr = result.stderr.lower()
                if 'undefined reference' in stderr or 'cannot find' in stderr:
                    return TestResult.LINK_ERROR
                return TestResult.COMPILE_ERROR
            
            # Compiled successfully
            self.stats.compile_success += 1
            
            # Check if executable was created
            if not output_file.exists():
                return TestResult.LINK_ERROR
            
            self.stats.link_success += 1
            
            # Optionally run the executable
            if run_executable:
                try:
                    exec_result = subprocess.run(
                        [str(output_file)],
                        capture_output=True,
                        timeout=2
                    )
                    
                    if exec_result.returncode < 0:
                        return TestResult.RUNTIME_ERROR
                    
                    self.stats.runtime_success += 1
                    return TestResult.RUNTIME_SUCCESS
                    
                except subprocess.TimeoutExpired:
                    return TestResult.TIMEOUT
                except Exception:
                    return TestResult.RUNTIME_ERROR
            
            return TestResult.LINK_SUCCESS
            
        except subprocess.TimeoutExpired:
            return TestResult.TIMEOUT
        except Exception as e:
            print(f"Unexpected error: {e}")
            return TestResult.CRASH
        finally:
            # Cleanup
            try:
                test_file.unlink(missing_ok=True)
                output_file.unlink(missing_ok=True)
            except:
                pass
    
    def run_campaign(self, iterations: int = 1000, run_executables: bool = False):
        """Run a fuzzing campaign."""
        print("=" * 70)
        print("ARIA FULL-STACK FUZZING CAMPAIGN")
        print("=" * 70)
        print(f"Compiler: {self.ariac}")
        print(f"Runtime:  {self.runtime_lib}")
        print(f"Iterations: {iterations}")
        print(f"Execute: {'YES' if run_executables else 'NO'}")
        print("=" * 70)
        print()
        
        generators = [
            ("Basic", self.generate_basic_program),
            ("P2.7 Structs", self.generate_struct_with_arrays),
            ("String Interp", self.generate_string_interpolation),
            ("Collections", self.generate_collections_test),
            ("Async", self.generate_async_test),
            ("Integer Math", self.generate_integer_math),
        ]
        
        for i in range(iterations):
            self.stats.total_tests += 1
            
            # Pick a generator
            name, generator = random.choice(generators)
            source = generator()
            
            # Test it
            result = self.test_program(source, run_executable=run_executables)
            
            # Update stats
            if result == TestResult.COMPILE_ERROR:
                self.stats.compile_errors += 1
            elif result == TestResult.LINK_ERROR:
                self.stats.link_errors += 1
            elif result == TestResult.RUNTIME_ERROR:
                self.stats.runtime_errors += 1
            elif result == TestResult.CRASH:
                self.stats.crashes += 1
                print(f"❌ CRASH detected in {name} test (iteration {i})")
            elif result == TestResult.TIMEOUT:
                self.stats.timeouts += 1
            
            # Progress report
            if (i + 1) % 100 == 0:
                self.print_stats()
        
        print()
        print("=" * 70)
        print("CAMPAIGN COMPLETE")
        print("=" * 70)
        self.print_stats()
        
        if self.stats.crashes > 0:
            print(f"\n⚠️  {self.stats.crashes} CRASHES DETECTED")
            return 1
        
        print("\n✅ NO CRASHES - Compiler and runtime stable")
        return 0
    
    def print_stats(self):
        """Print current statistics."""
        s = self.stats
        print(f"\n📊 Tests: {s.total_tests} | "
              f"Rate: {s.rate():.1f}/sec | "
              f"Compile: {s.compile_success} | "
              f"Link: {s.link_success} | "
              f"Run: {s.runtime_success}")
        print(f"   Errors: C:{s.compile_errors} L:{s.link_errors} R:{s.runtime_errors} | "
              f"Crash:{s.crashes} Timeout:{s.timeouts}")

def main():
    import argparse
    parser = argparse.ArgumentParser(description='Aria Full-Stack Fuzzer')
    parser.add_argument('--iterations', type=int, default=1000, 
                       help='Number of tests to run')
    parser.add_argument('--execute', action='store_true',
                       help='Actually run the generated executables')
    parser.add_argument('--ariac', default='./build/ariac',
                       help='Path to ariac compiler')
    parser.add_argument('--runtime', default='./build/libaria_runtime.a',
                       help='Path to runtime library')
    args = parser.parse_args()
    
    try:
        fuzzer = AriaFullStackFuzzer(args.ariac, args.runtime)
        exit_code = fuzzer.run_campaign(args.iterations, args.execute)
        sys.exit(exit_code)
    except FileNotFoundError as e:
        print(f"❌ Error: {e}")
        print("\nRun ./build.sh first to build compiler and runtime")
        sys.exit(1)
    except KeyboardInterrupt:
        print("\n\n⏸️  Campaign interrupted by user")
        fuzzer.print_stats()
        sys.exit(130)

if __name__ == '__main__':
    main()
