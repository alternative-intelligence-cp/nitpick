# Aria Language Examples

This directory contains example programs demonstrating the Aria programming language features.

## Directory Structure

```
examples/
├── basic/          # Basic language features
├── features/       # Advanced language features
└── advanced/       # Complete applications
```

## Basic Examples

These examples cover fundamental language concepts:

### 01_hello_world.aria
- Basic program structure
- Main function
- Print statement
- Return values

**Run:** `ariac examples/basic/01_hello_world.aria`

### 02_variables.aria
- Variable declarations
- Type annotations
- Type inference (`:=` operator)
- Integer types (i8, i16, i32, i64, u8, u16, u32, u64)
- Float types (f32, f64)
- Boolean and string types
- Mutable variables (`mut`)

**Run:** `ariac examples/basic/02_variables.aria`

### 03_functions.aria
- Function definitions
- Parameters and return types
- Function calls
- Multiple parameters
- Void return type

**Run:** `ariac examples/basic/03_functions.aria`

### 04_control_flow.aria
- If/else statements
- If-else if-else chains
- While loops
- For loops with ranges
- Break and continue
- Nested loops
- Comparison operators

**Run:** `ariac examples/basic/04_control_flow.aria`

### 05_memory.aria
- Ownership semantics
- Borrowing with `&` (immutable)
- Mutable borrowing with `&mut`
- Move semantics
- Reference parameters
- Memory safety guarantees

**Run:** `ariac examples/basic/05_memory.aria`

## Feature Examples

These examples demonstrate advanced language features:

### 06_modules.aria
- Module declarations
- Import statements
- Public/private visibility
- Module hierarchies
- Module aliases
- Standard library imports

**Run:** `ariac examples/features/06_modules.aria`

### 07_generics.aria
- Generic function definitions
- Type parameters
- Multiple type parameters
- Generic constraints
- Type inference
- Generic containers (structs)
- Monomorphization

**Run:** `ariac examples/features/07_generics.aria`

### 08_stdlib.aria
- Standard library overview
- I/O operations (std.io)
- String manipulation (std.string)
- Math functions (std.math)
- Collections (std.collections)
- File operations (std.fs)
- Error handling patterns

**Run:** `ariac examples/features/08_stdlib.aria`

### 09_tbb_arithmetic.aria
- Ternary Balanced Base (TBB) literals
- TBB arithmetic operations
- TBB to decimal conversion
- Decimal to TBB conversion
- TBB comparison operators
- Practical TBB applications

**Run:** `ariac examples/features/09_tbb_arithmetic.aria`

**TBB Overview:**
- Uses digits: `-1`, `0`, `+1` (represented as `-`, `0`, `+`)
- Syntax: `T[+-0+]` for TBB literals
- Base-3 system centered around zero
- No rounding errors for certain fractions
- Useful for quantum computing and certain algorithms

## Advanced Examples

Complete applications demonstrating integration of multiple features:

### 10_complete_app.aria
A complete task manager application featuring:
- Data structures (Task, TaskManager)
- Module organization
- Generic functions
- Memory management patterns
- TBB arithmetic for priority calculation
- Standard library usage
- Control flow patterns
- Error handling

**Run:** `ariac examples/advanced/10_complete_app.aria`

## Learning Path

**Recommended order for learning:**

1. **Start with basics** (01-05)
   - Understand core syntax
   - Learn variables and functions
   - Master control flow
   - Grasp memory management

2. **Explore features** (06-09)
   - Module system
   - Generics
   - Standard library
   - TBB arithmetic

3. **Study complete application** (10)
   - Integration of all concepts
   - Real-world patterns
   - Best practices

## Running Examples

### Compile and Run
```bash
ariac examples/basic/01_hello_world.aria -o hello
./hello
```

### Compile to LLVM IR
```bash
ariac examples/basic/01_hello_world.aria --emit-llvm -o hello.ll
```

### Compile to Assembly
```bash
ariac examples/basic/01_hello_world.aria --emit-asm -o hello.s
```

### Run with Debugging
```bash
ariac examples/basic/01_hello_world.aria -g -o hello
gdb ./hello
```

## Integration Tests

These examples also serve as integration tests. The test runner in `tests/integration/runner.sh` will:
1. Compile each `.aria` file
2. Run the executable
3. Verify output
4. Check exit codes

Run integration tests:
```bash
cd tests/integration
./runner.sh
```

## Contributing Examples

When adding new examples:

1. **File naming:** Use `NN_descriptive_name.aria` format
2. **Comments:** Include header comment explaining what's demonstrated
3. **Documentation:** Update this README.md
4. **Testing:** Ensure example compiles and runs correctly
5. **Incremental:** Build on previous examples

## Common Patterns

### Print and Debug
```aria
print("Debug value: ");
print(my_variable);
```

### Type Inference
```aria
let x := 42;           // Inferred as i32
let y := 3.14;         // Inferred as f64
let flag := true;      // Inferred as bool
```

### Borrowing Pattern
```aria
fn process(data: &MyType) -> void {
    // Borrow immutably
}

fn modify(data: &mut MyType) -> void {
    // Borrow mutably
}
```

### Generic Function
```aria
fn identity<T>(value: T) -> T {
    return value;
}
```

### Module Pattern
```aria
module my_module {
    pub fn public_function() -> void { }
    fn private_function() -> void { }
}
```

## Style Guide

These examples follow Aria style conventions:
- 4 spaces for indentation
- Snake_case for variables and functions
- PascalCase for types and structs
- Clear, descriptive names
- Comments for complex logic
- Blank lines between logical sections

## Resources

- **Language Spec:** `docs/aria_specs.txt`
- **Grammar:** `docs/GRAMMAR.md`
- **Standard Library:** `lib/std/`
- **Compiler Guide:** `docs/COMPILER_GUIDE.md`

## Feedback

Found an issue with an example? Have suggestions?
Open an issue at: https://github.com/your-org/aria/issues
