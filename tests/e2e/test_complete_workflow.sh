#!/bin/bash

#
# test_complete_workflow.sh
# 
# Phase 7.7.4: End-to-end workflow test
# Tests the complete development cycle: create project → write code → compile → run
#

set -e  # Exit on error

echo "========================================="
echo "Phase 7.7.4: End-to-End Workflow Test"
echo "========================================="
echo

# Get build directory
BUILD_DIR="${BUILD_DIR:-$(pwd)/build}"
NPKC="$BUILD_DIR/npkc"
ARIA_DOC="$BUILD_DIR/aria-doc"
ARIA_LSP="$BUILD_DIR/aria-lsp"

# Check tools exist
echo "Checking Aria toolchain..."
if [ ! -f "$NPKC" ]; then
    echo "❌ npkc not found at $NPKC"
    exit 1
fi
echo "✓ npkc found"

if [ ! -f "$ARIA_DOC" ]; then
    echo "⚠ aria-doc not found (non-critical)"
else
    echo "✓ aria-doc found"
fi

if [ ! -f "$ARIA_LSP" ]; then
    echo "⚠ aria-lsp not found (non-critical)"
else
    echo "✓ aria-lsp found"
fi

echo

# ============================================================================
# Test 1: Simple Hello World Project
# ============================================================================

echo "Test 1: Simple Hello World"
echo "-----------------------------------"

TEMP_DIR=$(mktemp -d)
cd "$TEMP_DIR"

# Create hello world
cat > hello.npk << 'EOF'
func:main = int32() {
    pass(42);
};
EOF

echo "✓ Created hello.npk"

# Compile
echo "Compiling..."
"$NPKC" hello.npk -o hello
echo "✓ Compiled successfully"

# Run
echo "Running..."
./hello
EXIT_CODE=$?
if [ $EXIT_CODE -eq 42 ]; then
    echo "✓ Program returned correct exit code: 42"
else
    echo "❌ Expected exit code 42, got $EXIT_CODE"
    cd /
    rm -rf "$TEMP_DIR"
    exit 1
fi

cd /
rm -rf "$TEMP_DIR"
echo

# ============================================================================
# Test 2: Multi-file Project with Dependencies
# ============================================================================

echo "Test 2: Multi-file Project"
echo "-----------------------------------"

TEMP_DIR=$(mktemp -d)
cd "$TEMP_DIR"

# Create project structure
mkdir -p src lib

# Create library module
cat > lib/math.npk << 'EOF'
/// Mathematical utilities
pub fn add(a: int32, b: int32) -> int32 {
    return a + b;
}

pub fn multiply(a: int32, b: int32) -> int32 {
    return a * b;
}

pub fn square(x: int32) -> int32 {
    return multiply(x, x);
}
EOF

echo "✓ Created lib/math.npk"

# Create helper module
cat > lib/utils.npk << 'EOF'
use math;

pub fn sum_of_squares(a: int32, b: int32) -> int32 {
    return math::add(math::square(a), math::square(b));
}
EOF

echo "✓ Created lib/utils.npk"

# Create main program
cat > src/main.npk << 'EOF'
use utils;

fn main() -> int32 {
    // Calculate 3^2 + 4^2 = 9 + 16 = 25
    return utils::sum_of_squares(3, 4);
}
EOF

echo "✓ Created src/main.npk"

# Compile
echo "Compiling..."
"$NPKC" src/main.npk lib/utils.npk lib/math.npk -o calculator
echo "✓ Compiled successfully"

# Run
echo "Running..."
./calculator
EXIT_CODE=$?
if [ $EXIT_CODE -eq 25 ]; then
    echo "✓ Program returned correct result: 25"
else
    echo "❌ Expected exit code 25, got $EXIT_CODE"
    cd /
    rm -rf "$TEMP_DIR"
    exit 1
fi

cd /
rm -rf "$TEMP_DIR"
echo

# ============================================================================
# Test 3: Error Handling Workflow
# ============================================================================

echo "Test 3: Error Handling"
echo "-----------------------------------"

TEMP_DIR=$(mktemp -d)
cd "$TEMP_DIR"

# Create file with error
cat > error_test.npk << 'EOF'
fn main() -> int32 {
    let x = undefined_variable;
    return x;
}
EOF

echo "✓ Created error_test.npk"

# Compile (should fail)
echo "Compiling (expecting error)..."
if "$NPKC" error_test.npk -o error_test 2>error_output.txt; then
    echo "❌ Compilation should have failed"
    cd /
    rm -rf "$TEMP_DIR"
    exit 1
else
    echo "✓ Compilation failed as expected"
fi

# Check error message
if grep -i "error" error_output.txt > /dev/null; then
    echo "✓ Error message contains 'error'"
else
    echo "❌ Error message should contain 'error'"
    cat error_output.txt
    cd /
    rm -rf "$TEMP_DIR"
    exit 1
fi

if grep -i "undefined" error_output.txt > /dev/null || \
   grep -i "undeclared" error_output.txt > /dev/null || \
   grep -i "not found" error_output.txt > /dev/null; then
    echo "✓ Error message mentions undefined variable"
else
    echo "⚠ Error message should mention undefined variable"
fi

cd /
rm -rf "$TEMP_DIR"
echo

# ============================================================================
# Test 4: Optimization Levels
# ============================================================================

echo "Test 4: Optimization Levels"
echo "-----------------------------------"

TEMP_DIR=$(mktemp -d)
cd "$TEMP_DIR"

# Create test program
cat > optimize.npk << 'EOF'
fn compute() -> int32 {
    let a = 10;
    let b = 20;
    let c = a + b;
    let d = c * 2;
    return d;
}

fn main() -> int32 {
    return compute();
}
EOF

echo "✓ Created optimize.npk"

# Compile with -O0
echo "Compiling with -O0..."
"$NPKC" optimize.npk -O0 -o optimize_O0
echo "✓ Compiled with -O0"

./optimize_O0
EXIT_CODE=$?
if [ $EXIT_CODE -eq 60 ]; then
    echo "✓ -O0 program returned 60"
else
    echo "❌ Expected 60, got $EXIT_CODE"
    cd /
    rm -rf "$TEMP_DIR"
    exit 1
fi

# Compile with -O2
echo "Compiling with -O2..."
"$NPKC" optimize.npk -O2 -o optimize_O2
echo "✓ Compiled with -O2"

./optimize_O2
EXIT_CODE=$?
if [ $EXIT_CODE -eq 60 ]; then
    echo "✓ -O2 program returned 60"
else
    echo "❌ Expected 60, got $EXIT_CODE"
    cd /
    rm -rf "$TEMP_DIR"
    exit 1
fi

cd /
rm -rf "$TEMP_DIR"
echo

# ============================================================================
# Test 5: Output Formats (IR and Assembly)
# ============================================================================

echo "Test 5: Output Formats"
echo "-----------------------------------"

TEMP_DIR=$(mktemp -d)
cd "$TEMP_DIR"

cat > simple.npk << 'EOF'
fn main() -> int32 {
    return 123;
}
EOF

echo "✓ Created simple.npk"

# Generate LLVM IR
echo "Generating LLVM IR..."
"$NPKC" simple.npk --emit-llvm -o simple.ll
if [ -f simple.ll ]; then
    echo "✓ LLVM IR generated"
else
    echo "❌ LLVM IR not generated"
    cd /
    rm -rf "$TEMP_DIR"
    exit 1
fi

# Generate assembly
echo "Generating assembly..."
"$NPKC" simple.npk -S -o simple.s
if [ -f simple.s ]; then
    echo "✓ Assembly generated"
else
    echo "❌ Assembly not generated"
    cd /
    rm -rf "$TEMP_DIR"
    exit 1
fi

cd /
rm -rf "$TEMP_DIR"
echo

# ============================================================================
# Test 6: Documentation Generation (if aria-doc available)
# ============================================================================

if [ -f "$ARIA_DOC" ]; then
    echo "Test 6: Documentation Generation"
    echo "-----------------------------------"
    
    TEMP_DIR=$(mktemp -d)
    cd "$TEMP_DIR"
    
    cat > documented.npk << 'EOF'
/// A utility function that adds two numbers
/// @param a The first number
/// @param b The second number
/// @return The sum of a and b
pub fn add(a: int32, b: int32) -> int32 {
    return a + b;
}
EOF
    
    echo "✓ Created documented.npk"
    
    # Generate docs
    echo "Generating documentation..."
    "$ARIA_DOC" documented.npk -o docs/
    if [ -f docs/index.html ]; then
        echo "✓ Documentation generated"
    else
        echo "❌ Documentation not generated"
        cd /
        rm -rf "$TEMP_DIR"
        exit 1
    fi
    
    cd /
    rm -rf "$TEMP_DIR"
    echo
fi

# ============================================================================
# Test 7: Complete Calculator Project
# ============================================================================

echo "Test 7: Complete Calculator Project"
echo "-----------------------------------"

TEMP_DIR=$(mktemp -d)
cd "$TEMP_DIR"

mkdir -p calculator/{src,lib}
cd calculator

# Create operations library
cat > lib/operations.npk << 'EOF'
pub fn add(a: int32, b: int32) -> int32 {
    return a + b;
}

pub fn subtract(a: int32, b: int32) -> int32 {
    return a - b;
}

pub fn multiply(a: int32, b: int32) -> int32 {
    return a * b;
}

pub fn divide(a: int32, b: int32) -> int32 {
    if b == 0 {
        return 0;
    }
    return a / b;
}
EOF

echo "✓ Created lib/operations.npk"

# Create main calculator
cat > src/calculator.npk << 'EOF'
use operations;

fn main() -> int32 {
    // Test: ((10 + 5) * 2) - 10 = 20
    let sum = operations::add(10, 5);       // 15
    let product = operations::multiply(sum, 2);  // 30
    let result = operations::subtract(product, 10);  // 20
    return result;
}
EOF

echo "✓ Created src/calculator.npk"

# Compile
echo "Compiling calculator..."
"$NPKC" src/calculator.npk lib/operations.npk -o calculator
echo "✓ Compiled successfully"

# Run
echo "Running calculator..."
./calculator
EXIT_CODE=$?
if [ $EXIT_CODE -eq 20 ]; then
    echo "✓ Calculator returned correct result: 20"
else
    echo "❌ Expected 20, got $EXIT_CODE"
    cd /
    rm -rf "$TEMP_DIR"
    exit 1
fi

cd /
rm -rf "$TEMP_DIR"
echo

# ============================================================================
# Summary
# ============================================================================

echo "========================================="
echo "✅ All end-to-end workflow tests passed!"
echo "========================================="
echo
echo "Summary:"
echo "  ✓ Simple compilation and execution"
echo "  ✓ Multi-file projects with dependencies"
echo "  ✓ Error detection and reporting"
echo "  ✓ Optimization levels (-O0, -O2)"
echo "  ✓ Multiple output formats (executable, IR, assembly)"
if [ -f "$ARIA_DOC" ]; then
    echo "  ✓ Documentation generation"
fi
echo "  ✓ Complete calculator project"
echo

exit 0
