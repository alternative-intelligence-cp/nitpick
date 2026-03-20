#!/usr/bin/env python3
"""
Aria Fuzzer V2 - Main Orchestrator
Comprehensive grammar-based fuzzing campaign
"""

import sys
import time
import random
import shutil
import hashlib
import subprocess
from pathlib import Path
from dataclasses import dataclass
from typing import List, Dict, Optional, Set
from collections import defaultdict

from grammar_parser import load_type_system
from program_generator import AriaGenerator, TypeExhaustiveGenerator


@dataclass
class FuzzConfig:
    """Fuzzer configuration."""
    aria_root: Path
    build_dir: Path
    compiler: Path
    corpus_dir: Path
    output_dir: Path
    duration_hours: float = 24.0
    compile_timeout: int = 10
    exec_timeout: int = 5
    parallel_workers: int = 4
    
    def __post_init__(self):
        # Create directories
        self.corpus_dir.mkdir(parents=True, exist_ok=True)
        self.output_dir.mkdir(parents=True, exist_ok=True)
        (self.output_dir / 'crashes').mkdir(exist_ok=True)
        (self.output_dir / 'valid').mkdir(exist_ok=True)
        (self.output_dir / 'invalid').mkdir(exist_ok=True)


@dataclass
class FuzzStats:
    """Fuzzing statistics."""
    total_generated: int = 0
    successfully_compiled: int = 0
    compile_errors: int = 0
    crashes: int = 0
    timeouts: int = 0
    start_time: float = 0.0
    
    def __post_init__(self):
        self.start_time = time.time()
    
    def elapsed_time(self) -> float:
        return time.time() - self.start_time
    
    def programs_per_sec(self) -> float:
        elapsed = self.elapsed_time()
        return self.total_generated / elapsed if elapsed > 0 else 0
    
    def print_report(self):
        """Print current statistics."""
        print("\n" + "=" * 70)
        print("FUZZING STATISTICS")
        print("=" * 70)
        print(f"Duration:            {self.elapsed_time() / 3600:.2f} hours")
        print(f"Total programs:      {self.total_generated:,}")
        print(f"Throughput:          {self.programs_per_sec():.2f} programs/sec")
        print(f"─" * 70)
        print(f"Successful compiles: {self.successfully_compiled:,} ({self._pct(self.successfully_compiled)}%)")
        print(f"Compile errors:      {self.compile_errors:,} ({self._pct(self.compile_errors)}%)")
        print(f"Crashes:             {self.crashes:,} ({self._pct(self.crashes)}%)")
        print(f"Timeouts:            {self.timeouts:,} ({self._pct(self.timeouts)}%)")
        print("=" * 70)
    
    def _pct(self, count: int) -> str:
        if self.total_generated == 0:
            return "  0.00"
        pct = (count / self.total_generated) * 100
        return f"{pct:5.2f}"


class AriaFuzzer:
    """Main fuzzer orchestrator."""
    
    def __init__(self, config: FuzzConfig, seed: Optional[int] = None):
        self.config = config
        self.stats = FuzzStats()
        self.generator = AriaGenerator(config.aria_root, seed)
        self.exhaustive_gen = TypeExhaustiveGenerator(config.aria_root)
        
        if seed is not None:
            random.seed(seed)
        
        # Crash deduplication
        self.crash_hashes: Set[str] = set()
    
    def run_campaign(self):
        """Run the complete fuzzing campaign."""
        print("\n🚀 Starting Aria Fuzzer V2")
        print(f"   Compiler: {self.config.compiler}")
        print(f"   Duration: {self.config.duration_hours} hours")
        print(f"   Output:   {self.config.output_dir}")
        print()
        
        end_time = time.time() + (self.config.duration_hours * 3600)
        last_report = time.time()
        
        # Phase 1: Generate exhaustive type tests
        print("Phase 1: Type-exhaustive testing...")
        self._run_exhaustive_tests()
        
        # Phase 2: Grammar-based generation
        print("\nPhase 2: Grammar-based generation...")
        
        try:
            while time.time() < end_time:
                # Generate program
                strategy = random.choice([
                    'minimal', 'type_test', 'control_flow',
                    'functions', 'edge_case',
                    'pick', 'when', 'defer', 'result', 'cast', 'pipeline',
                    'struct', 'enum', 'array', 'pointer', 'loop_range',
                    'optional_type',
                    'combo',
                    'negative',
                    'type_coercion',
                    'large_type',
                    'recursive',
                    'tbb_sticky',
                    'string_interp',
                    'multi_struct',
                    'runtime_check',
                ])
                program = self.generator.generate_program(strategy)
                
                # Test it
                self._test_program(program, f"generated_{self.stats.total_generated:06d}")
                
                # Print progress every 60 seconds
                if time.time() - last_report > 60:
                    self.stats.print_report()
                    last_report = time.time()
                
        except KeyboardInterrupt:
            print("\n\n⚠️  Fuzzing interrupted by user")
        
        # Final report
        self.stats.print_report()
        
        # Summary
        print("\n📊 FINAL SUMMARY")
        if self.stats.crashes == 0:
            print("   ✅ No crashes found!")
        else:
            print(f"   ❌ Found {self.stats.crashes} crashes")
            print(f"      Saved in: {self.config.output_dir / 'crashes'}")
        
        print(f"\n   Valid programs saved: {self.config.output_dir / 'valid'}")
        print(f"   Throughput: {self.stats.programs_per_sec():.2f} programs/sec")
        print()
    
    def _run_exhaustive_tests(self):
        """Run all type-exhaustive tests."""
        tests = self.exhaustive_gen.generate_all_type_tests()
        print(f"   Generated {len(tests)} exhaustive test cases")
        
        for i, (filename, code) in enumerate(tests):
            if i % 20 == 0:
                print(f"   Progress: {i}/{len(tests)} tests...")
            
            self._test_program(code, f"exhaustive_{filename.replace('.aria', '')}")
        
        print(f"   ✅ Completed {len(tests)} exhaustive tests")
    
    def _test_program(self, code: str, test_id: str):
        """Test a single program."""
        self.stats.total_generated += 1
        
        # Save to temporary file
        temp_file = self.config.corpus_dir / f"{test_id}.aria"
        temp_file.write_text(code)
        
        # Compile it
        result = self._compile(temp_file)
        
        if result['crashed']:
            self.stats.crashes += 1
            self._save_crash(test_id, code, result)
        elif result['timed_out']:
            self.stats.timeouts += 1
            self._save_timeout(test_id, code)
        elif result['exit_code'] == 0:
            self.stats.successfully_compiled += 1
            self._save_valid(test_id, code)
        else:
            self.stats.compile_errors += 1
            # Don't save expected compile errors (keeps corpus small)
        
        # Cleanup temp file
        temp_file.unlink()
    
    def _compile(self, source_file: Path) -> Dict:
        """Compile an Aria program."""
        result = {
            'exit_code': 0,
            'stdout': '',
            'stderr': '',
            'crashed': False,
            'timed_out': False
        }
        
        try:
            proc = subprocess.run(
                [str(self.config.compiler), str(source_file)],
                capture_output=True,
                text=True,
                timeout=self.config.compile_timeout
            )
            
            result['exit_code'] = proc.returncode
            result['stdout'] = proc.stdout
            result['stderr'] = proc.stderr
            
            # Detect crashes
            if proc.returncode < 0:  # Negative means signal
                result['crashed'] = True
            elif 'assertion failed' in proc.stderr.lower():
                result['crashed'] = True
            elif 'segmentation fault' in proc.stderr.lower():
                result['crashed'] = True
                
        except subprocess.TimeoutExpired:
            result['timed_out'] = True
        except Exception as e:
            result['crashed'] = True
            result['stderr'] = str(e)
        
        return result
    
    def _save_crash(self, test_id: str, code: str, result: Dict):
        """Save a crashing test case."""
        crash_hash = hashlib.sha256(result['stderr'].encode()).hexdigest()[:16]
        
        # Deduplicate
        if crash_hash in self.crash_hashes:
            return
        
        self.crash_hashes.add(crash_hash)
        
        crash_file = self.config.output_dir / 'crashes' / f"crash_{test_id}_{crash_hash}.aria"
        crash_file.write_text(code)
        
        # Save crash info
        info_file = crash_file.with_suffix('.txt')
        info_file.write_text(f"""Crash Report
Test ID: {test_id}
Hash: {crash_hash}
Exit code: {result['exit_code']}

STDERR:
{result['stderr']}

STDOUT:
{result['stdout']}
""")
        
        print(f"\n   ⚠️  CRASH FOUND: {crash_file.name}")
    
    def _save_timeout(self, test_id: str, code: str):
        """Save a test that timed out."""
        timeout_file = self.config.output_dir / 'crashes' / f"timeout_{test_id}.aria"
        timeout_file.write_text(code)
    
    def _save_valid(self, test_id: str, code: str):
        """Save a valid program (sample only to avoid filling disk)."""
        if random.random() < 0.01:  # Save 1% of valid programs
            valid_file = self.config.output_dir / 'valid' / f"{test_id}.aria"
            valid_file.write_text(code)


def main():
    """Main entry point."""
    import argparse
    
    parser = argparse.ArgumentParser(description='Aria Fuzzer V2 - Grammar-based fuzzing')
    parser.add_argument('--duration', type=float, default=1.0,
                       help='Fuzzing duration in hours (default: 1.0)')
    parser.add_argument('--seed', type=int, default=None,
                       help='Random seed for reproducibility')
    parser.add_argument('--quick', action='store_true',
                       help='Quick test (5 minutes)')
    
    args = parser.parse_args()
    
    # Paths
    script_dir = Path(__file__).parent
    aria_root = script_dir.parent.parent.parent
    build_dir = aria_root / 'build'
    compiler = build_dir / 'ariac'
    
    if not compiler.exists():
        print(f"❌ Compiler not found: {compiler}")
        print("   Please build Aria first: ./build.sh")
        sys.exit(1)
    
    # Configuration
    duration = 5/60 if args.quick else args.duration  # 5 minutes for quick test
    
    config = FuzzConfig(
        aria_root=aria_root,
        build_dir=build_dir,
        compiler=compiler,
        corpus_dir=script_dir / 'corpus_v2',
        output_dir=script_dir / 'output_v2',
        duration_hours=duration
    )
    
    # Run fuzzer
    fuzzer = AriaFuzzer(config, seed=args.seed)
    fuzzer.run_campaign()


if __name__ == '__main__':
    main()
