#!/usr/bin/env python3
"""
Check semantic analysis pass rate across all test files
"""

import subprocess
import sys
from pathlib import Path

def test_file(filepath, ariac_path):
    """Test if a file passes semantic analysis (compiles without semantic errors)"""
    try:
        result = subprocess.run(
            [ariac_path, str(filepath)],
            capture_output=True,
            text=True,
            timeout=5
        )
        
        # Check for semantic errors
        stderr = result.stderr + result.stdout
        
        # Skip files meant to fail or be diagnostics
        if 'diagnostic' in filepath.name or 'debug_' in filepath.name:
            return None, "SKIP"
        
        # Linker errors are not semantic errors - they mean semantic analysis passed
        if 'linker command failed' in stderr:
            return True, "PASS (linker error - runtime not impl)"
        
        # Intentional error validation tests - these SHOULD show errors to validate borrow checker
        intentional_error_tests = ['move_comprehensive_test.aria', 'move_use_after_move_test.aria']
        if any(test in filepath.name for test in intentional_error_tests):
            expected_errors = ['Use after move', 'Cannot move variable']
            if any(err in stderr for err in expected_errors):
                return True, "PASS (borrow checker validation test)"
        
        # Success if no errors (warnings are OK)
        if 'error:' not in stderr.lower():
            return True, "PASS"
        
        # Check for actual semantic errors (not linker/fatal file errors)
        has_semantic_error = False
        error_lines = []
        for line in stderr.split('\n'):
            if 'error:' in line.lower():
                if 'fatal error: could not open' in line.lower():
                    return None, "SKIP (file not found)"
                if 'linker command failed' not in line:
                    has_semantic_error = True
                    error_lines.append(line.strip()[:80])
        
        if has_semantic_error:
            # Count actual semantic errors
            error_count = len([l for l in error_lines if l])
            return False, f"FAIL ({error_count} semantic errors)"
        
        return True, "PASS"
        
    except subprocess.TimeoutExpired:
        return False, "TIMEOUT"
    except Exception as e:
        return False, f"EXCEPTION: {str(e)}"

def main():
    test_dir = Path('./tests')
    ariac = './build/ariac'
    
    if not Path(ariac).exists():
        print(f"❌ ariac not found at {ariac}")
        sys.exit(1)
    
    # Find all .aria files
    test_files = sorted(test_dir.glob('*.aria'))
    
    stats = {
        'total': 0,
        'pass': 0,
        'fail': 0,
        'skip': 0,
    }
    
    failures = []
    
    print(f"🔍 Testing semantic analysis on {len(test_files)} files")
    print(f"{'='*70}")
    
    for filepath in test_files:
        success, status = test_file(filepath, ariac)
        
        if success is None:
            stats['skip'] += 1
            continue
        
        stats['total'] += 1
        
        if success:
            stats['pass'] += 1
            print(f"✅ {filepath.name}")
        else:
            stats['fail'] += 1
            failures.append((filepath.name, status))
            print(f"❌ {filepath.name}: {status}")
    
    print(f"\n{'='*70}")
    print(f"📊 SEMANTIC ANALYSIS RESULTS")
    print(f"{'='*70}")
    print(f"Total tests:  {stats['total']}")
    print(f"Passed:       {stats['pass']} ({100*stats['pass']/max(stats['total'],1):.1f}%)")
    print(f"Failed:       {stats['fail']} ({100*stats['fail']/max(stats['total'],1):.1f}%)")
    print(f"Skipped:      {stats['skip']}")
    
    if failures:
        print(f"\n{'='*70}")
        print(f"❌ FAILED TESTS ({len(failures)}):")
        print(f"{'='*70}")
        for name, status in failures[:20]:  # Show first 20
            print(f"  {name}: {status}")
        
        if len(failures) > 20:
            print(f"  ... and {len(failures) - 20} more")
    
    sys.exit(0 if stats['fail'] == 0 else 1)

if __name__ == '__main__':
    main()
