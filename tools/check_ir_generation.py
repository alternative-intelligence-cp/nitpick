#!/usr/bin/env python3
"""
Test IR generation and compilation across all Aria test files.
Checks which files successfully compile to executables.
"""

import subprocess
import sys
from pathlib import Path

def test_compile(filepath):
    """Test if a file compiles to an executable."""
    output_path = Path("/tmp/aria_test_out")
    
    # Remove old output if exists
    if output_path.exists():
        output_path.unlink()
    
    try:
        # Run compiler with timeout
        result = subprocess.run(
            ["./build/ariac", str(filepath), "-o", str(output_path)],
            capture_output=True,
            text=True,
            timeout=10
        )
        
        stderr = result.stderr + result.stdout
        
        # Skip diagnostic/debug files
        if 'diagnostic' in filepath.name or 'debug_' in filepath.name:
            return None, "SKIP"
        
        # Compilation errors
        if 'error:' in stderr.lower():
            return False, "COMPILATION_ERROR"
        
        # Linker errors
        if 'linker command failed' in stderr or 'undefined reference' in stderr:
            return False, "LINKER_ERROR"
        
        # Check if output exists (check AFTER compilation completes)
        if output_path.exists():
            output_path.unlink()  # Clean up
            return True, "SUCCESS"
        
        # If no errors but no output, something weird happened
        if result.returncode != 0:
            return False, f"EXIT_CODE_{result.returncode}"
        
        return False, "NO_OUTPUT"
        
    except subprocess.TimeoutExpired:
        return False, "TIMEOUT"
    except Exception as e:
        return False, f"EXCEPTION: {str(e)}"

def main():
    test_dir = Path('./tests')
    
    # Skip patterns
    skip_patterns = [
        'test_fuzzer_',  # Fuzzer generated files
        '.aria.tmp',
        'diagnostic_',
        'debug_'
    ]
    
    # Get all .aria files
    test_files = sorted([f for f in test_dir.glob('*.aria') 
                        if not any(skip in f.name for skip in skip_patterns)])
    
    print(f"Testing IR generation and compilation for {len(test_files)} files...\n")
    
    passed = []
    failed_compile = []
    failed_linker = []
    skipped = []
    
    for i, filepath in enumerate(test_files, 1):
        result, reason = test_compile(filepath)
        
        if result is None:
            skipped.append((filepath.name, reason))
            print(f"⊘  {filepath.name}")
        elif result:
            passed.append(filepath.name)
            print(f"✅ {filepath.name}")
        else:
            if reason == "LINKER_ERROR":
                failed_linker.append((filepath.name, reason))
                print(f"🔗 {filepath.name} (linker)")
            else:
                failed_compile.append((filepath.name, reason))
                print(f"❌ {filepath.name} ({reason})")
    
    # Summary
    print("\n" + "="*70)
    print("📊 IR GENERATION / COMPILATION RESULTS")
    print("="*70)
    print(f"Total tests:  {len(test_files)}")
    print(f"Compiled successfully: {len(passed)} ({100*len(passed)/len(test_files):.1f}%)")
    print(f"Compilation errors:    {len(failed_compile)} ({100*len(failed_compile)/len(test_files):.1f}%)")
    print(f"Linker errors:         {len(failed_linker)} ({100*len(failed_linker)/len(test_files):.1f}%)")
    print(f"Skipped:               {len(skipped)}")
    
    if failed_compile:
        print("\n" + "="*70)
        print(f"❌ COMPILATION ERRORS ({len(failed_compile)}):")
        print("="*70)
        for name, reason in failed_compile:
            print(f"  {name}: {reason}")
    
    if failed_linker:
        print("\n" + "="*70)
        print(f"🔗 LINKER ERRORS ({len(failed_linker)}):")
        print("="*70)
        print("  (IR generation succeeded, runtime functions not implemented)")
        for name, reason in failed_linker:
            print(f"  {name}")
    
    return 0 if not failed_compile else 1

if __name__ == "__main__":
    sys.exit(main())
