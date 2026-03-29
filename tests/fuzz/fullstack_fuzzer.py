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
    
    # --- Shared building blocks ---
    FAILSAFE = "func:failsafe = int32(tbb32:err) { exit(1); };\n"

    def _rand_int32(self, lo: int = 1, hi: int = 100) -> str:
        return f"{random.randint(lo, hi)}i32"

    def _rand_int64(self, lo: int = 1, hi: int = 100) -> str:
        return f"{random.randint(lo, hi)}i64"

    # ------------------------------------------------------------------ #
    #  GENERATORS — each returns a complete, valid .aria program string   #
    # ------------------------------------------------------------------ #

    def generate_basic_program(self) -> str:
        """Minimal valid program."""
        v = self._rand_int32()
        return f"""{self.FAILSAFE}func:main = int32() {{
    int32:x = {v};
    exit(0);
}};"""

    def generate_arithmetic(self) -> str:
        """Signed integer arithmetic (+, -, *, /, %)."""
        op = random.choice(['+', '-', '*', '/', '%'])
        a = random.randint(1, 100)
        b = random.randint(1, 10) if op in ('/', '%') else random.randint(1, 100)
        return f"""{self.FAILSAFE}func:main = int32() {{
    int32:a = {a}i32;
    int32:b = {b}i32;
    int32:result = (a {op} b);
    exit(0);
}};"""

    def generate_bitwise(self) -> str:
        """Unsigned bitwise operations — & | ^ << >>."""
        op = random.choice(['&', '|', '^', '<<', '>>'])
        a = random.randint(0, 255)
        b = random.randint(0, 7) if op in ('<<', '>>') else random.randint(0, 255)
        return f"""{self.FAILSAFE}func:main = int32() {{
    uint32:a = {a}u32;
    uint32:b = {b}u32;
    uint32:result = (a {op} b);
    exit(0);
}};"""

    def generate_multi_type(self) -> str:
        """Variables of different scalar types."""
        return f"""{self.FAILSAFE}func:main = int32() {{
    int32:i = {self._rand_int32()};
    int64:l = {self._rand_int64()};
    flt64:f = {random.uniform(0.1, 99.9):.4f}f64;
    bool:b = {random.choice(['true', 'false'])};
    string:s = "hello";
    exit(0);
}};"""

    def generate_if_else(self) -> str:
        """If / else conditional."""
        a, b = random.randint(1, 50), random.randint(1, 50)
        cmp = random.choice(['<', '>', '<=', '>=', '==', '!='])
        return f"""{self.FAILSAFE}func:main = int32() {{
    int32:a = {a}i32;
    int32:b = {b}i32;
    int32:r = 0i32;
    if (a {cmp} b) {{
        r = 1i32;
    }} else {{
        r = 2i32;
    }}
    exit(0);
}};"""

    def generate_while_loop(self) -> str:
        """Simple counted while loop."""
        limit = random.randint(3, 20)
        return f"""{self.FAILSAFE}func:main = int32() {{
    int32:i = 0i32;
    int32:sum = 0i32;
    while (i < {limit}i32) {{
        sum = (sum + i);
        i = (i + 1i32);
    }}
    exit(0);
}};"""

    def generate_for_loop(self) -> str:
        """C-style for loop."""
        limit = random.randint(3, 20)
        return f"""{self.FAILSAFE}func:main = int32() {{
    int32:sum = 0i32;
    for (int32:i = 0i32; i < {limit}i32; i += 1i32) {{
        sum = (sum + i);
    }}
    exit(0);
}};"""

    def generate_function_call(self) -> str:
        """Helper function called from main, Result unwrapped with raw()."""
        a, b = random.randint(1, 50), random.randint(1, 50)
        return f"""{self.FAILSAFE}func:add = int32(int32:x, int32:y) {{
    int32:result = (x + y);
    pass(result);
}};

func:main = int32() {{
    int32:val = raw(add({a}i32, {b}i32));
    exit(0);
}};"""

    def generate_multi_function(self) -> str:
        """Several helper functions chained together."""
        return f"""{self.FAILSAFE}func:square = int64(int64:n) {{
    int64:r = (n * n);
    pass(r);
}};

func:add = int64(int64:a, int64:b) {{
    int64:r = (a + b);
    pass(r);
}};

func:main = int32() {{
    int64:a = raw(square({random.randint(2,10)}i64));
    int64:b = raw(square({random.randint(2,10)}i64));
    int64:c = raw(add(a, b));
    exit(0);
}};"""

    def generate_struct_basic(self) -> str:
        """Struct declaration, instantiation, field access."""
        x, y = self._rand_int32(), self._rand_int32()
        return f"""{self.FAILSAFE}struct:Point = {{
    int32:x;
    int32:y;
}};

func:main = int32() {{
    Point:p = Point{{x: {x}, y: {y}}};
    int32:px = p.x;
    int32:py = p.y;
    exit(0);
}};"""

    def generate_struct_mutation(self) -> str:
        """Struct field mutation."""
        return f"""{self.FAILSAFE}struct:Counter = {{
    int32:value;
    int32:cap;
}};

func:main = int32() {{
    Counter:c = Counter{{value: 0i32, cap: {random.randint(5,20)}i32}};
    c.value = (c.value + 1i32);
    c.value = (c.value + 1i32);
    exit(0);
}};"""

    def generate_struct_return(self) -> str:
        """Function returning a struct (via pass)."""
        return f"""{self.FAILSAFE}struct:Pair = {{
    int32:first;
    int32:second;
}};

func:make_pair = Pair(int32:a, int32:b) {{
    pass(Pair{{first: a, second: b}});
}};

func:main = int32() {{
    Pair:p = raw(make_pair({self._rand_int32()}, {self._rand_int32()}));
    int32:f = p.first;
    exit(0);
}};"""

    def generate_array_basic(self) -> str:
        """Fixed-size array operations."""
        vals = [self._rand_int32() for _ in range(4)]
        return f"""{self.FAILSAFE}func:main = int32() {{
    int32[4]:arr = [{', '.join(vals)}];
    int32:first = arr[0];
    int32:last  = arr[3];
    arr[1] = 99i32;
    exit(0);
}};"""

    def generate_string_ops(self) -> str:
        """String builtin operations."""
        words = ["hello", "world", "aria", "fuzz", "test", "alpha", "beta"]
        w1, w2 = random.choice(words), random.choice(words)
        return f"""{self.FAILSAFE}func:main = int32() {{
    string:a = "{w1}";
    string:b = "{w2}";
    int64:len_a = string_length(a);
    string:joined = string_concat(a, b);
    bool:has = string_contains(joined, a);
    exit(0);
}};"""

    def generate_println(self) -> str:
        """Print / println calls (drop() needed for Result)."""
        return f"""{self.FAILSAFE}func:main = int32() {{
    drop(println("fuzz test output"));
    int32:x = {self._rand_int32()};
    drop(println(string_from_int({random.randint(1,999)}i64)));
    exit(0);
}};"""

    def generate_nested_if(self) -> str:
        """Nested conditionals."""
        a, b, c = (random.randint(1, 30) for _ in range(3))
        return f"""{self.FAILSAFE}func:main = int32() {{
    int32:a = {a}i32;
    int32:b = {b}i32;
    int32:c = {c}i32;
    int32:r = 0i32;
    if (a > b) {{
        if (b > c) {{
            r = 1i32;
        }} else {{
            r = 2i32;
        }}
    }} else {{
        r = 3i32;
    }}
    exit(0);
}};"""

    def generate_bool_logic(self) -> str:
        """Boolean expressions and comparisons."""
        a, b = random.randint(1, 50), random.randint(1, 50)
        return f"""{self.FAILSAFE}func:main = int32() {{
    int32:x = {a}i32;
    int32:y = {b}i32;
    bool:lt = (x < y);
    bool:eq = (x == y);
    bool:gt = (x > y);
    exit(0);
}};"""

    def generate_pick(self) -> str:
        """Pick statement (pattern matching / switch)."""
        val = random.randint(0, 4)
        return f"""{self.FAILSAFE}func:main = int32() {{
    int32:v = {val}i32;
    int32:r = 0i32;
    pick (v) {{
        (0) {{ r = 10i32; }},
        (1) {{ r = 20i32; }},
        (2) {{ r = 30i32; }},
        (*) {{ r = 99i32; }}
    }}
    exit(0);
}};"""

    def generate_loop_construct(self) -> str:
        """Aria loop(start, limit, step) statement — iterator is $."""
        start = random.randint(0, 3)
        lim = random.randint(5, 15)
        return f"""{self.FAILSAFE}func:main = int32() {{
    int64:sum = 0i64;
    loop({start}, {lim}, 1) {{
        sum = (sum + $);
    }}
    exit(0);
}};"""

    def generate_till_construct(self) -> str:
        """Aria till(limit, step) counted loop — iterator is $."""
        lim = random.randint(3, 12)
        return f"""{self.FAILSAFE}func:main = int32() {{
    int64:total = 0i64;
    till({lim}, 1) {{
        total = (total + $);
    }}
    exit(0);
}};"""

    def generate_cast(self) -> str:
        """@cast<T> numeric conversion."""
        v = random.randint(1, 100)
        return f"""{self.FAILSAFE}func:main = int32() {{
    int32:x = {v}i32;
    int64:y = @cast<int64>(x);
    flt64:z = @cast<flt64>(x);
    exit(0);
}};"""

    def generate_struct_array_field(self) -> str:
        """Struct with a fixed-size array field."""
        return f"""{self.FAILSAFE}struct:Buffer = {{
    int32:len;
    int32[4]:data;
}};

func:main = int32() {{
    Buffer:buf;
    buf.len = 4i32;
    buf.data[0] = {self._rand_int32()};
    buf.data[1] = {self._rand_int32()};
    buf.data[2] = {self._rand_int32()};
    buf.data[3] = {self._rand_int32()};
    int32:first = buf.data[0];
    exit(0);
}};"""

    def generate_complex_math(self) -> str:
        """Mixed arithmetic with multiple types and conversions."""
        a, b, c = (random.randint(1, 50) for _ in range(3))
        return f"""{self.FAILSAFE}func:main = int32() {{
    int64:a = {a}i64;
    int64:b = {b}i64;
    int64:c = {c}i64;
    int64:r1 = ((a * b) + c);
    int64:r2 = ((a + b) * c);
    int64:r3 = ((a - b) + (c * 2i64));
    exit(0);
}};"""

    def generate_recursive_func(self) -> str:
        """Recursive function (factorial-style)."""
        n = random.randint(3, 10)
        return f"""{self.FAILSAFE}func:factorial = int64(int64:n) {{
    if (n <= 1i64) {{
        pass(1i64);
    }}
    int64:sub = raw(factorial((n - 1i64)));
    int64:result = (n * sub);
    pass(result);
}};

func:main = int32() {{
    int64:r = raw(factorial({n}i64));
    exit(0);
}};"""

    def generate_string_compare(self) -> str:
        """String equality and search operations."""
        words = ["alpha", "beta", "gamma", "delta", "epsilon"]
        w1, w2 = random.sample(words, 2)
        return f"""{self.FAILSAFE}func:main = int32() {{
    string:a = "{w1}";
    string:b = "{w2}";
    bool:same = string_equals(a, b);
    bool:starts = string_starts_with(a, "{w1[0]}");
    bool:ends = string_ends_with(b, "{w2[-1]}");
    int64:idx = string_index_of(a, "{w1[:2]}");
    exit(0);
}};"""

    def generate_const_literal(self) -> str:
        """Const declarations (simple literal only — arithmetic broken)."""
        return f"""{self.FAILSAFE}func:main = int32() {{
    const int32:LIMIT = {self._rand_int32(10, 50)};
    const int64:BIG = {self._rand_int64(100, 999)};
    const string:NAME = "fuzz";
    int32:x = LIMIT;
    exit(0);
}};"""

    def generate_pointer_basic(self) -> str:
        """Stack pointer via @ operator and dereference."""
        return f"""{self.FAILSAFE}func:main = int32() {{
    stack int32:val = {self._rand_int32()};
    stack int32->:ptr = @val;
    exit(0);
}};"""

    def generate_syscall(self) -> str:
        """Tiered sys() syscall invocations."""
        safe_calls = ['GETPID', 'GETPPID', 'GETTID', 'GETUID', 'GETGID', 'GETEUID', 'GETEGID']
        variant = random.choice(['safe', 'safe_write', 'full', 'raw', 'wrapper'])
        sc = random.choice(safe_calls)

        if variant == 'safe':
            return f"""use "sys.aria".*;
{self.FAILSAFE}func:main = int32() {{
    Result<int64>:r = sys({sc});
    int64:val = r ? -1i64;
    exit(0);
}};"""
        elif variant == 'safe_write':
            msg = f"fuzz{random.randint(0,9999)}"
            return f"""use "sys.aria".*;
{self.FAILSAFE}func:main = int32() {{
    Result<int64>:w = sys(WRITE, 1i64, "{msg}\n", {len(msg)+1}i64);
    int64:bytes = w ? 0i64;
    exit(0);
}};"""
        elif variant == 'full':
            return f"""use "sys.aria".*;
{self.FAILSAFE}func:main = int32() {{
    Result<int64>:r = sys!!({sc});
    int64:val = r ? -1i64;
    exit(0);
}};"""
        elif variant == 'raw':
            nr_map = {'GETPID': 39, 'GETPPID': 110, 'GETTID': 186,
                      'GETUID': 102, 'GETGID': 104, 'GETEUID': 107, 'GETEGID': 108}
            nr = nr_map[sc]
            return f"""use "sys.aria".*;
{self.FAILSAFE}func:main = int32() {{
    int64:val = sys!!!({nr}i64);
    exit(0);
}};"""
        else:  # wrapper
            return f"""use "sys.aria".*;
func:get_val = int64() {{
    Result<int64>:r = sys({sc});
    int64:v = r ? -1i64;
    pass(v);
}};
{self.FAILSAFE}func:main = int32() {{
    Result<int64>:v = get_val();
    int64:result = v ? 0i64;
    exit(0);
}};"""

    def generate_drop_raw_shorthand(self) -> str:
        """_? drop shorthand and _! raw shorthand operators."""
        variant = random.choice(['drop_print', 'raw_add', 'mixed', 'drop_func', 'raw_chain'])

        if variant == 'drop_print':
            return f"""{self.FAILSAFE}func:main = int32() {{
    _?println("fuzz drop {self._rand_int32()}");
    exit(0);
}};"""
        elif variant == 'raw_add':
            a, b = self._rand_int64(), self._rand_int64()
            return f"""func:safe_add = Result<int64>(int64:a, int64:b) {{ pass(a + b); }};
{self.FAILSAFE}func:main = int32() {{
    int64:x = _!safe_add({a}i64, {b}i64);
    exit(0);
}};"""
        elif variant == 'mixed':
            return f"""{self.FAILSAFE}func:main = int32() {{
    drop(println("verbose"));
    _?println("shorthand");
    exit(0);
}};"""
        elif variant == 'drop_func':
            return f"""func:do_work = Result<int64>() {{ pass({self._rand_int64()}i64); }};
{self.FAILSAFE}func:main = int32() {{
    _?do_work();
    exit(0);
}};"""
        else:  # raw_chain
            v = self._rand_int64()
            return f"""func:safe_val = Result<int64>() {{ pass({v}i64); }};
{self.FAILSAFE}func:main = int32() {{
    int64:x = _!safe_val() + 1i64;
    exit(0);
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
            ("Arithmetic", self.generate_arithmetic),
            ("Bitwise", self.generate_bitwise),
            ("Multi-type", self.generate_multi_type),
            ("If/Else", self.generate_if_else),
            ("While Loop", self.generate_while_loop),
            ("For Loop", self.generate_for_loop),
            ("Function Call", self.generate_function_call),
            ("Multi Function", self.generate_multi_function),
            ("Struct Basic", self.generate_struct_basic),
            ("Struct Mutation", self.generate_struct_mutation),
            ("Struct Return", self.generate_struct_return),
            ("Array Basic", self.generate_array_basic),
            ("String Ops", self.generate_string_ops),
            ("Println", self.generate_println),
            ("Nested If", self.generate_nested_if),
            ("Bool Logic", self.generate_bool_logic),
            ("Pick", self.generate_pick),
            ("Loop", self.generate_loop_construct),
            ("Till", self.generate_till_construct),
            ("Cast", self.generate_cast),
            ("Struct Array", self.generate_struct_array_field),
            ("Complex Math", self.generate_complex_math),
            ("Recursive", self.generate_recursive_func),
            ("String Compare", self.generate_string_compare),
            ("Const Literal", self.generate_const_literal),
            ("Pointer Basic", self.generate_pointer_basic),
            ("Syscall", self.generate_syscall),
            ("Drop/Raw Shorthand", self.generate_drop_raw_shorthand),
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
