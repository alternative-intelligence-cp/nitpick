# Research Context Index

This directory contains source compilations and existing research for Gemini tasks.

## Source Code Compilations

**13 Parts** covering the complete Aria compiler codebase, split for Gemini's upload limits:

- **Part 1a-1b**: Frontend (Tokens, Lexer, Preprocessor)
- **Part 2**: AST Definitions  
- **Part 3a-3c**: Parser (Header, Core, Expressions)
- **Part 4**: Parser Components (Declarations, Functions, Structs, Traits)
- **Part 5a-5b**: Semantic Analysis (Type Checker, Borrow Checker, Traits)
- **Part 6a-6b**: CodeGen Core (Context, Headers, Expressions)
- **Part 7a-7c**: CodeGen Components (Statements, Control Flow, Lambda, Async)
- **Part 8**: Advanced Features (TBB, Monomorphization, VTable, Runtime)

**See NAVIGATION_GUIDE.txt** for detailed file recommendations for each research task.

## Existing Research Documents

**Location:** `/home/randy/._____RANDY_____/REPOS/aria/docs/research/`

### Work Packages (5 docs)
- `aria_work_package_1_core_runtime.md` - Core runtime, GC, memory model
- `aria_work_package_2_advanced_features.md` - SIMD, wildx, struct methods
- `aria_work_package_3_stdlib.md` - Standard library functions
- `aria_work_package_4_metaprogramming.md` - Macros, generics, comptime
- `aria_work_package_5_optimization.md` - Compiler optimizations

### Architectural Reviews (3 docs)
- `aria_llvm_architecture_review.md` - LLVM integration analysis
- `aria_compiler_architecture_recommendations.md` - Architecture improvements
- `aria_compiler_cleanup_plan.md` - Codebase organization

### Feature-Specific Research (9 docs)
- `async_await_implementation.md` - Async runtime, event loop, futures
- `generics_monomorphization_implementation.md` - Generic type instantiation
- `module_system_implementation.md` - Module loading, namespaces
- `lambda_closure_implementation.md` - Lambda captures, closure conversion
- `gc_nursery_implementation.md` - Generational GC design
- `simd_vector_implementation.md` - SIMD intrinsics, vectorization
- `stack_trace_implementation.md` - Debug information, unwinding
- `fat_pointer_implementation.md` - Slice representation, bounds checking
- `trait_system_implementation.md` - Trait resolution, dispatch

### Metaprogramming Specs (4 docs)
- `metaprogramming_macro_expansion.md` - NASM-style macros
- `metaprogramming_comptime_evaluation.md` - Compile-time execution
- `metaprogramming_type_reflection.md` - Type introspection
- `metaprogramming_code_generation.md` - AST manipulation

## Usage for Research Tasks

When creating a task, reference relevant documents in the `context_files` array of the task JSON. For example:

```json
"context_files": [
  "/home/randy/._____RANDY_____/REPOS/aria/docs/info/aria_specs.txt",
  "/home/randy/._____RANDY_____/REPOS/aria/docs/research/async_await_implementation.md"
]
```

## Coverage Analysis

See `/home/randy/._____RANDY_____/WORK_SPACE/RESEARCH_COVERAGE_ANALYSIS.md` for detailed mapping of which research documents cover which features.

**Summary:**
- 16 features fully researched (ready to implement)
- 2 features partially researched
- 10 features need new research (tasks research_001 through research_010)
