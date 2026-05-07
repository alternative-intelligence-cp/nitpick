#!/usr/bin/env python3
"""
Phase 4.2: Aria Compiler Adversarial Fuzzer

A mutation-based fuzzer targeting the Aria compiler to find:
1. Crashes (segfaults, aborts, assertions)
2. Hangs (infinite loops, excessive memory)
3. Undefined behavior (ASAN/UBSAN violations)
4. ICE (Internal Compiler Errors)

Fuzzing Strategies:
- Mutation: Modify valid programs
- Generation: Create random programs from grammar
- Combination: Mix valid programs together
- Adversarial: Target known weak points
"""

import os
import sys
import time
import random
import string
import signal
import hashlib
import subprocess
import json
import threading
import multiprocessing
from pathlib import Path
from dataclasses import dataclass, field
from typing import List, Dict, Set, Optional, Tuple
from enum import Enum, auto
from collections import defaultdict
from concurrent.futures import ProcessPoolExecutor, as_completed

# Configuration
SCRIPT_DIR = Path(__file__).parent
ARIA_ROOT = SCRIPT_DIR.parent.parent
BUILD_DIR = ARIA_ROOT / "build"
NPKC = BUILD_DIR / "npkc"
CORPUS_DIR = SCRIPT_DIR / "corpus"
CRASHES_DIR = SCRIPT_DIR / "crashes"
COVERAGE_DIR = SCRIPT_DIR / "coverage"

# Timeouts
COMPILE_TIMEOUT = 10  # seconds
EXECUTION_TIMEOUT = 5  # seconds

class CrashType(Enum):
    NONE = auto()
    SEGFAULT = auto()
    ABORT = auto()
    TIMEOUT = auto()
    ASSERTION = auto()
    ICE = auto()  # Internal Compiler Error
    ASAN = auto()
    UBSAN = auto()
    OOM = auto()
    UNKNOWN = auto()

@dataclass
class FuzzResult:
    """Result of a single fuzz iteration."""
    input_hash: str
    crash_type: CrashType
    exit_code: int
    stderr: str
    stdout: str
    duration_ms: float
    input_size: int

@dataclass
class FuzzStats:
    """Statistics for the fuzzing campaign."""
    total_runs: int = 0
    crashes: int = 0
    timeouts: int = 0
    unique_crashes: int = 0
    coverage_edges: int = 0
    start_time: float = field(default_factory=time.time)
    crash_types: Dict[CrashType, int] = field(default_factory=lambda: defaultdict(int))

    def executions_per_sec(self) -> float:
        elapsed = time.time() - self.start_time
        return self.total_runs / elapsed if elapsed > 0 else 0

class AriaMutator:
    """Mutates Aria source code to find bugs."""

    # Token types that can be mutated
    KEYWORDS = [
        'func', 'return', 'if', 'else', 'while', 'for', 'in', 'defer',
        'wild', 'arena', 'pool', 'slab', 'const', 'mut', 'move',
        'struct', 'enum', 'trait', 'impl', 'type', 'async', 'await',
        'true', 'false', 'nil', 'err'
    ]

    TYPES = [
        'int8', 'int16', 'int32', 'int64', 'int128', 'int256', 'int512',
        'uint8', 'uint16', 'uint32', 'uint64', 'uint128',
        'flt32', 'flt64', 'flt128', 'flt256', 'flt512',
        'bool', 'void', 'string', 'tbb8', 'tbb16', 'tbb32', 'tbb64'
    ]

    OPERATORS = [
        '+', '-', '*', '/', '%', '==', '!=', '<', '>', '<=', '>=',
        '&&', '||', '!', '&', '|', '^', '~', '<<', '>>', '?:', '??'
    ]

    DELIMITERS = ['(', ')', '{', '}', '[', ']', ';', ':', ',', '.', '@', '$', '#']

    def __init__(self, seed: Optional[int] = None):
        if seed is not None:
            random.seed(seed)

    def mutate(self, source: str) -> str:
        """Apply a random mutation to the source."""
        mutations = [
            self._mutate_delete_char,
            self._mutate_insert_char,
            self._mutate_replace_char,
            self._mutate_swap_chars,
            self._mutate_duplicate_line,
            self._mutate_delete_line,
            self._mutate_swap_lines,
            self._mutate_replace_keyword,
            self._mutate_replace_type,
            self._mutate_insert_delimiter,
            self._mutate_change_number,
            self._mutate_insert_unicode,
            self._mutate_deep_nesting,
            self._mutate_long_identifier,
        ]

        mutation = random.choice(mutations)
        try:
            return mutation(source)
        except Exception:
            return source

    def _mutate_delete_char(self, source: str) -> str:
        if len(source) < 2:
            return source
        pos = random.randint(0, len(source) - 1)
        return source[:pos] + source[pos + 1:]

    def _mutate_insert_char(self, source: str) -> str:
        pos = random.randint(0, len(source))
        char = random.choice(string.printable)
        return source[:pos] + char + source[pos:]

    def _mutate_replace_char(self, source: str) -> str:
        if not source:
            return source
        pos = random.randint(0, len(source) - 1)
        char = random.choice(string.printable)
        return source[:pos] + char + source[pos + 1:]

    def _mutate_swap_chars(self, source: str) -> str:
        if len(source) < 2:
            return source
        pos = random.randint(0, len(source) - 2)
        return source[:pos] + source[pos + 1] + source[pos] + source[pos + 2:]

    def _mutate_duplicate_line(self, source: str) -> str:
        lines = source.split('\n')
        if not lines:
            return source
        idx = random.randint(0, len(lines) - 1)
        lines.insert(idx, lines[idx])
        return '\n'.join(lines)

    def _mutate_delete_line(self, source: str) -> str:
        lines = source.split('\n')
        if len(lines) < 2:
            return source
        idx = random.randint(0, len(lines) - 1)
        del lines[idx]
        return '\n'.join(lines)

    def _mutate_swap_lines(self, source: str) -> str:
        lines = source.split('\n')
        if len(lines) < 2:
            return source
        i, j = random.sample(range(len(lines)), 2)
        lines[i], lines[j] = lines[j], lines[i]
        return '\n'.join(lines)

    def _mutate_replace_keyword(self, source: str) -> str:
        old_kw = random.choice(self.KEYWORDS)
        new_kw = random.choice(self.KEYWORDS)
        return source.replace(old_kw, new_kw, 1)

    def _mutate_replace_type(self, source: str) -> str:
        old_type = random.choice(self.TYPES)
        new_type = random.choice(self.TYPES)
        return source.replace(old_type, new_type, 1)

    def _mutate_insert_delimiter(self, source: str) -> str:
        if not source:
            return source
        pos = random.randint(0, len(source))
        delim = random.choice(self.DELIMITERS)
        return source[:pos] + delim + source[pos:]

    def _mutate_change_number(self, source: str) -> str:
        # Find numbers and change them
        import re
        numbers = list(re.finditer(r'\d+', source))
        if not numbers:
            return source
        match = random.choice(numbers)

        # Choose new number
        choices = [0, 1, -1, 127, 128, 255, 256, 65535, 65536,
                   2**31 - 1, 2**31, 2**63 - 1, 2**63]
        new_num = str(random.choice(choices))

        return source[:match.start()] + new_num + source[match.end():]

    def _mutate_insert_unicode(self, source: str) -> str:
        if not source:
            return source
        pos = random.randint(0, len(source))
        # Interesting Unicode characters
        unicode_chars = [
            '\x00', '\x01', '\x7f',           # Control chars (valid UTF-8)
            '\u200b', '\u200c', '\u200d',     # Zero-width
            '\ufeff',                         # BOM
            '\U0001f4a9',                     # Emoji
            '\u0000', '\u0001',               # Null and SOH
            '\u00e9', '\u00f1',               # Accented chars
        ]
        char = random.choice(unicode_chars)
        return source[:pos] + char + source[pos:]

    def _mutate_deep_nesting(self, source: str) -> str:
        # Insert deeply nested blocks
        depth = random.randint(10, 100)
        nested = "if (true) { " * depth + "return 0; " + "} " * depth
        pos = random.randint(0, len(source))
        return source[:pos] + nested + source[pos:]

    def _mutate_long_identifier(self, source: str) -> str:
        # Insert very long identifier
        length = random.randint(1000, 10000)
        long_id = 'x' * length
        return source.replace('main', long_id, 1)


class AriaGenerator:
    """Generates random Aria programs."""

    FAILSAFE = "func:failsafe = int32(tbb32:err) { exit 1; };\n"

    def __init__(self, seed: Optional[int] = None):
        if seed is not None:
            random.seed(seed)

    def generate(self) -> str:
        """Generate a random valid-ish Aria program."""
        generators = [
            self._gen_minimal,
            self._gen_with_vars,
            self._gen_with_loops,
            self._gen_with_alloc,
            self._gen_with_functions,
            self._gen_adversarial,
            self._gen_drop_raw_shorthand,
        ]
        return random.choice(generators)()

    def _gen_minimal(self) -> str:
        return self.FAILSAFE + "func:main = int32() { exit 0; };"

    def _gen_with_vars(self) -> str:
        types = ['int32', 'int64', 'bool', 'flt64']
        t = random.choice(types)
        val = random.randint(-1000, 1000)
        return self.FAILSAFE + f"""func:main = int32() {{
    {t}:x = {val};
    print(x);
    exit 0;
}};"""

    def _gen_with_loops(self) -> str:
        count = random.randint(1, 100)
        return self.FAILSAFE + f"""func:main = int32() {{
    int64:i = 0;
    int64:count = {count};
    while (i < count) {{
        print(i);
        i = (i + 1);
    }}
    exit 0;
}};"""

    def _gen_with_alloc(self) -> str:
        size = random.choice([1, 16, 64, 256, 1024, 4096, 65536, 1048576])
        return self.FAILSAFE + f"""func:main = int32() {{
    int64:size = {size};
    wild int8@:ptr = alloc(size);
    free(ptr);
    exit 0;
}};"""

    def _gen_with_functions(self) -> str:
        return self.FAILSAFE + """func:helper = int64(int64:x) {
    int64:result = (x * 2);
    pass result;
};

func:main = int32() {
    int64:val = 21;
    int64:result = helper(val);
    print(result);
    exit 0;
};"""

    def _gen_adversarial(self) -> str:
        """Generate adversarial edge cases."""
        fs = self.FAILSAFE
        cases = [
            # Empty function
            fs + "func:main = int32() { };",
            # Missing exit
            fs + "func:main = int32() { int32:x = 1; };",
            # Recursive (could cause stack overflow)
            fs + "func:main = int32() { return main(); };",
            # Very long line
            fs + "func:main = int32() { " + "int32:x = 1; " * 1000 + "exit 0; };",
            # Many parameters
            fs + "func:main = int32(" + ", ".join(f"int32:p{i}" for i in range(100)) + ") { exit 0; };",
            # Unicode identifier
            fs + "func:main = int32() { int32:変量 = 42; exit 0; };",
            # Null byte in source
            fs + "func:main = int32() { int32:x\x00y = 1; exit 0; };",
        ]
        return random.choice(cases)

    def _gen_drop_raw_shorthand(self) -> str:
        """Generate programs using _? and _! shorthand operators."""
        fs = self.FAILSAFE
        cases = [
            # _? basic — drop println result
            fs + 'func:main = int32() { _?println("hello"); exit 0; };',
            # _! basic — raw extract from known-good result
            fs + 'func:safe_add = Result<int64>(int64:a, int64:b) { pass a + b; };\n'
               + 'func:main = int32() { int64:x = _!safe_add(1i64, 2i64); exit 0; };',
            # Mixed verbose and shorthand
            fs + 'func:main = int32() { drop println("a"); _?println("b"); exit 0; };',
            # _? with side-effect expression
            fs + 'func:do_work = Result<int64>() { pass 42i64; };\n'
               + 'func:main = int32() { _?do_work(); exit 0; };',
            # _! chained with arithmetic
            fs + 'func:safe_val = Result<int64>() { pass 10i64; };\n'
               + 'func:main = int32() { int64:x = _!safe_val() + 5i64; exit 0; };',
        ]
        return random.choice(cases)


class AriaFuzzer:
    """Main fuzzer orchestrating the campaign."""

    def __init__(self,
                 corpus_dir: Path = CORPUS_DIR,
                 crashes_dir: Path = CRASHES_DIR,
                 verbose: bool = False):
        self.corpus_dir = corpus_dir
        self.crashes_dir = crashes_dir
        self.verbose = verbose
        self.mutator = AriaMutator()
        self.generator = AriaGenerator()
        self.stats = FuzzStats()
        self.seen_crashes: Set[str] = set()
        self.corpus: List[str] = []

        # Ensure directories exist
        self.corpus_dir.mkdir(parents=True, exist_ok=True)
        self.crashes_dir.mkdir(parents=True, exist_ok=True)

        # Load existing corpus
        self._load_corpus()

    def _load_corpus(self):
        """Load seed corpus from directory."""
        for f in self.corpus_dir.glob("*.npk"):
            try:
                self.corpus.append(f.read_text())
            except Exception:
                pass

        if not self.corpus:
            # Add minimal seeds
            self.corpus = [
                "func:failsafe = int32(tbb32:err) { exit 1; };\nfunc:main = int32() { exit 0; };",
                "func:failsafe = int32(tbb32:err) { exit 1; };\nfunc:main = int32() { int32:x = 42; print(x); exit 0; };",
            ]

    def _hash_input(self, source: str) -> str:
        """Generate hash for deduplication."""
        # Use 'surrogatepass' to handle any problematic characters
        return hashlib.sha256(source.encode('utf-8', errors='surrogatepass')).hexdigest()[:16]

    def _classify_crash(self, exit_code: int, stderr: str, stdout: str) -> CrashType:
        """Classify the type of crash.

        Important: We distinguish between:
        - NONE: Compilation succeeded or failed with normal error (exit 1)
        - REAL CRASHES: SEGFAULT, ABORT, ASAN, etc. (bugs to fix)
        """
        stderr_lower = stderr.lower()

        if exit_code == 0:
            return CrashType.NONE

        # Normal compile error (exit 1) is not a crash - expected for mutated code
        if exit_code == 1:
            return CrashType.NONE

        # Check for sanitizer issues first (highest priority)
        if 'addresssanitizer' in stderr_lower or 'asan' in stderr_lower:
            return CrashType.ASAN

        if 'undefined' in stderr_lower and 'sanitizer' in stderr_lower:
            return CrashType.UBSAN

        # Real crashes (signals)
        if exit_code == -signal.SIGSEGV or exit_code == 139:
            return CrashType.SEGFAULT

        if exit_code == -signal.SIGABRT or exit_code == 134:
            # This is a real crash (uncaught exception, assertion, etc.)
            if 'assertion' in stderr_lower or 'assert' in stderr_lower:
                return CrashType.ASSERTION
            return CrashType.ABORT

        if exit_code == -signal.SIGKILL or exit_code == 137:
            return CrashType.OOM

        if 'timeout' in stderr_lower:
            return CrashType.TIMEOUT

        if 'internal compiler error' in stderr_lower or 'ice' in stderr_lower:
            return CrashType.ICE

        # Other non-zero/non-1 exit codes might be interesting
        if exit_code != 0:
            return CrashType.UNKNOWN

        return CrashType.NONE

    def _run_compiler(self, source: str) -> FuzzResult:
        """Run the compiler on the given source."""
        input_hash = self._hash_input(source)

        # Write to temp file
        tmp_file = Path(f"/tmp/fuzz_{input_hash}.npk")
        tmp_output = Path(f"/tmp/fuzz_{input_hash}")

        try:
            # Use surrogatepass to handle any encoding issues
            tmp_file.write_text(source, encoding='utf-8', errors='surrogatepass')

            start = time.perf_counter()
            try:
                result = subprocess.run(
                    [str(NPKC), str(tmp_file), "-o", str(tmp_output)],
                    capture_output=True,
                    timeout=COMPILE_TIMEOUT
                )
                exit_code = result.returncode
                # Decode with errors='replace' to handle non-UTF-8 output
                stderr = result.stderr.decode('utf-8', errors='replace')
                stdout = result.stdout.decode('utf-8', errors='replace')
            except subprocess.TimeoutExpired:
                exit_code = -1
                stderr = "TIMEOUT"
                stdout = ""

            duration_ms = (time.perf_counter() - start) * 1000

            crash_type = self._classify_crash(exit_code, stderr, stdout)

            return FuzzResult(
                input_hash=input_hash,
                crash_type=crash_type,
                exit_code=exit_code,
                stderr=stderr,
                stdout=stdout,
                duration_ms=duration_ms,
                input_size=len(source)
            )

        finally:
            # Cleanup
            try:
                tmp_file.unlink()
                tmp_output.unlink()
            except Exception:
                pass

    def _save_crash(self, source: str, result: FuzzResult):
        """Save a crash-inducing input."""
        crash_hash = result.input_hash

        if crash_hash in self.seen_crashes:
            return

        self.seen_crashes.add(crash_hash)
        self.stats.unique_crashes += 1

        # Create crash directory
        crash_dir = self.crashes_dir / result.crash_type.name.lower()
        crash_dir.mkdir(exist_ok=True)

        # Save source
        (crash_dir / f"{crash_hash}.npk").write_text(source)

        # Save metadata
        meta = {
            "hash": crash_hash,
            "type": result.crash_type.name,
            "exit_code": result.exit_code,
            "stderr": result.stderr[:1000],
            "duration_ms": result.duration_ms,
            "input_size": result.input_size,
            "timestamp": time.time()
        }
        (crash_dir / f"{crash_hash}.json").write_text(json.dumps(meta, indent=2))

        if self.verbose:
            print(f"  [!] NEW CRASH: {result.crash_type.name} -> {crash_hash}")

    def fuzz_one(self) -> FuzzResult:
        """Execute a single fuzz iteration."""
        # Choose strategy
        if random.random() < 0.2:
            # Generate new program
            source = self.generator.generate()
        else:
            # Mutate existing
            base = random.choice(self.corpus)
            source = self.mutator.mutate(base)

        result = self._run_compiler(source)

        self.stats.total_runs += 1
        self.stats.crash_types[result.crash_type] += 1

        if result.crash_type != CrashType.NONE:
            self.stats.crashes += 1
            self._save_crash(source, result)

        if result.crash_type == CrashType.TIMEOUT:
            self.stats.timeouts += 1

        return result

    def run(self, duration_hours: float = 72, workers: int = 1):
        """Run the fuzzing campaign."""
        end_time = time.time() + (duration_hours * 3600)

        print(f"Starting adversarial fuzzing campaign")
        print(f"  Duration: {duration_hours} hours")
        print(f"  Workers: {workers}")
        print(f"  Corpus size: {len(self.corpus)}")
        print("-" * 60)

        last_report = time.time()
        report_interval = 60  # seconds

        try:
            while time.time() < end_time:
                result = self.fuzz_one()

                # Periodic status report
                if time.time() - last_report >= report_interval:
                    self._print_status()
                    last_report = time.time()

        except KeyboardInterrupt:
            print("\n[!] Interrupted by user")

        self._print_final_report()

    def _print_status(self):
        """Print current fuzzing status."""
        elapsed = time.time() - self.stats.start_time
        hours = elapsed / 3600

        print(f"[{hours:.1f}h] "
              f"runs={self.stats.total_runs} "
              f"crashes={self.stats.unique_crashes} "
              f"exec/s={self.stats.executions_per_sec():.1f}")

    def _print_final_report(self):
        """Print final fuzzing report."""
        elapsed = time.time() - self.stats.start_time

        print("\n" + "=" * 60)
        print("ADVERSARIAL FUZZING CAMPAIGN COMPLETE")
        print("=" * 60)
        print(f"Duration: {elapsed/3600:.2f} hours")
        print(f"Total runs: {self.stats.total_runs}")
        print(f"Executions/sec: {self.stats.executions_per_sec():.1f}")
        print(f"Total crashes: {self.stats.crashes}")
        print(f"Unique crashes: {self.stats.unique_crashes}")
        print(f"Timeouts: {self.stats.timeouts}")
        print()
        print("Crash breakdown:")
        for crash_type, count in sorted(self.stats.crash_types.items(),
                                          key=lambda x: -x[1]):
            if count > 0 and crash_type != CrashType.NONE:
                print(f"  {crash_type.name}: {count}")
        print("=" * 60)

        # Save final stats
        stats_file = self.crashes_dir / "campaign_stats.json"
        stats = {
            "duration_seconds": elapsed,
            "total_runs": self.stats.total_runs,
            "executions_per_sec": self.stats.executions_per_sec(),
            "total_crashes": self.stats.crashes,
            "unique_crashes": self.stats.unique_crashes,
            "timeouts": self.stats.timeouts,
            "crash_breakdown": {k.name: v for k, v in self.stats.crash_types.items() if v > 0}
        }
        stats_file.write_text(json.dumps(stats, indent=2))
        print(f"\nStats saved to {stats_file}")


def main():
    import argparse

    parser = argparse.ArgumentParser(description="Aria Compiler Adversarial Fuzzer")
    parser.add_argument("--duration", type=float, default=72,
                        help="Duration in hours (default: 72)")
    parser.add_argument("--quick", action="store_true",
                        help="Quick test run (5 minutes)")
    parser.add_argument("--workers", type=int, default=1,
                        help="Number of parallel workers")
    parser.add_argument("--verbose", "-v", action="store_true",
                        help="Verbose output")
    parser.add_argument("--seed", type=int,
                        help="Random seed for reproducibility")
    args = parser.parse_args()

    if not NPKC.exists():
        print(f"ERROR: Compiler not found at {NPKC}")
        print("Please build the compiler first")
        return 1

    duration = 5/60 if args.quick else args.duration  # 5 minutes for quick test

    fuzzer = AriaFuzzer(verbose=args.verbose)
    if args.seed:
        random.seed(args.seed)

    fuzzer.run(duration_hours=duration, workers=args.workers)

    return 0 if fuzzer.stats.unique_crashes == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
