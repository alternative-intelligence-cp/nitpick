# Aria Ecosystem - Component Tree

**Document Version**: 1.0  
**Last Updated**: December 22, 2025  
**Purpose**: Hierarchical breakdown of all ecosystem components, dependencies, and relationships

---

## Root: Alternative Intelligence Liberation Platform (AILP)

```
AILP/
├── Core Language & Compiler/
│   ├── Aria Programming Language
│   ├── ariac (Compiler Executable)
│   └── libaria_runtime.a (Runtime Library)
│
├── Development Tools/
│   ├── aria-lsp (Language Server)
│   ├── aria-dap (Debug Adapter)
│   ├── aria-doc (Documentation Generator)
│   └── VSCode Extension
│
├── Build & Package Management/
│   ├── AriaBuild (Build System)
│   ├── AriaX (Package Manager)
│   └── AriaX Linux (Custom Distribution)
│
├── User Environment/
│   └── aria_shell (AriaSH - Native Shell)
│
└── Consciousness Infrastructure/
    └── Nikola (9D Consciousness Substrate)
```

---

## 1. Core Language & Compiler

### 1.1 Aria Programming Language

**Repository**: `/home/randy/._____RANDY_____/REPOS/aria`  
**Version**: v0.2.0 (March 2026)  
**Language**: Specification (implemented in C++20)

#### Language Features Tree
```
Aria Language/
├── Type System/
│   ├── Primitive Types/
│   │   ├── Signed Integers (int8, int16, int32, int64, int128, int256, int512)
│   │   ├── Unsigned Integers (uint8, uint16, uint32, uint64, uint128, uint256, uint512)
│   │   ├── Floating Point (float32, float64, float128)
│   │   ├── Boolean (bool)
│   │   └── Character (char, rune for Unicode)
│   │
│   ├── TBB Types (Twisted Balanced Binary)/
│   │   ├── tbb8 (-127 to +127, ERR at -128)
│   │   ├── tbb16 (-32767 to +32767, ERR at -32768)
│   │   ├── tbb32 (-2147483647 to +2147483647, ERR at -2147483648)
│   │   └── tbb64 (symmetric 64-bit with ERR sentinel)
│   │
│   ├── Balanced Radix Types/
│   │   ├── Balanced Ternary/
│   │   │   ├── trit (single digit: -1, 0, +1)
│   │   │   └── tryte (10 trits, range: -29524 to +29524)
│   │   └── Balanced Nonary/
│   │       ├── nit (single digit: -4 to +4)
│   │       └── nyte (5 nits, range: -29524 to +29524)
│   │
│   ├── Composite Types/
│   │   ├── Arrays (fixed-size: [T; N], dynamic: [T])
│   │   ├── Structs (struct Name { fields })
│   │   ├── Enums (enum Name { variants })
│   │   ├── Tuples ((T1, T2, ...))
│   │   └── Unions (union Name { variants })
│   │
│   ├── Generic Types/
│   │   ├── Generic Functions (func<T>)
│   │   └── Generic Structs (struct<T>)
│   │
│   ├── Special Types/
│   │   ├── Result<T> (pass/fail error handling)
│   │   ├── Optional<T> (some/none)
│   │   └── Async<T> (async/await futures)
│   │
│   └── Memory Qualifiers/
│       ├── Stack (default, automatic lifetime)
│       ├── Wild (heap-allocated, manual management)
│       └── WildX (executable JIT memory, restricted)
│
├── I/O System (6-Stream Topology)/
│   ├── Control Plane/
│   │   ├── stdin (FD 0) - Standard input
│   │   ├── stdout (FD 1) - Standard output
│   │   └── stderr (FD 2) - Standard error
│   │
│   ├── Observability Plane/
│   │   └── stddbg (FD 3) - Debug/telemetry stream
│   │
│   └── Data Plane/
│       ├── stddati (FD 4) - Binary data input
│       └── stddato (FD 5) - Binary data output
│
├── Memory Management/
│   ├── Allocators/
│   │   ├── Arena (bump allocator, lifetime-based)
│   │   ├── Pool (fixed-size blocks, low fragmentation)
│   │   ├── Slab (kernel-style, typed allocations)
│   │   └── GC (garbage collected, automatic)
│   │
│   ├── Ownership/
│   │   ├── Borrow Checker (compile-time lifetime analysis)
│   │   ├── Move Semantics (transfer ownership)
│   │   └── Reference Counting (runtime shared ownership)
│   │
│   └── Safety/
│       ├── Bounds Checking (array access validation)
│       ├── Null Safety (no null pointers by default)
│       └── Uninitialized Prevention (must init before use)
│
├── Control Flow/
│   ├── Conditionals (if/else)
│   ├── Loops (while, for, loop)
│   ├── Pattern Matching (match expressions)
│   ├── Pass/Fail (early return for Result<T>)
│   └── Async/Await (asynchronous control flow)
│
├── Module System/
│   ├── use (import declarations)
│   ├── mod (module definitions)
│   ├── pub (visibility control)
│   └── extern (foreign function declarations)
│
└── Metaprogramming/
    ├── Const Evaluation (compile-time computation)
    ├── Macros (code generation)
    └── Attributes (metadata annotations)
```

---

### 1.2 ariac (Compiler Executable)

**Binary**: `/home/randy/._____RANDY_____/REPOS/aria/build/ariac`  
**Language**: C++20  
**Dependencies**: LLVM 20, libaria_runtime (for linking)

#### Compiler Pipeline Tree
```
ariac/
├── Frontend/
│   ├── Preprocessor/
│   │   ├── File Reading
│   │   ├── Import Resolution
│   │   └── Macro Expansion
│   │
│   ├── Lexer (Tokenization)/
│   │   ├── Token Stream Generation
│   │   ├── Unicode Handling
│   │   ├── Keyword Recognition
│   │   └── Operator Parsing
│   │
│   ├── Parser (AST Construction)/
│   │   ├── Top-Level Declarations (func, struct, enum)
│   │   ├── Statements (assignments, control flow)
│   │   ├── Expressions (binary ops, calls, literals)
│   │   ├── Type Annotations
│   │   └── Error Recovery (diagnostics on syntax errors)
│   │
│   └── AST (Abstract Syntax Tree)/
│       ├── Declaration Nodes
│       ├── Statement Nodes
│       ├── Expression Nodes
│       └── Type Nodes
│
├── Middle-End (Semantic Analysis)/
│   ├── Type Checking/
│   │   ├── Type Inference
│   │   ├── Generic Monomorphization
│   │   ├── Trait Resolution
│   │   └── Type Compatibility
│   │
│   ├── Name Resolution/
│   │   ├── Symbol Tables
│   │   ├── Scope Management
│   │   └── Import Resolution
│   │
│   ├── Borrow Checker/
│   │   ├── Lifetime Analysis
│   │   ├── Ownership Tracking
│   │   └── Mutable Aliasing Detection
│   │
│   └── Const Evaluation/
│       ├── Compile-Time Computation
│       └── Constant Folding
│
├── Backend (Code Generation)/
│   ├── IR Generation (LLVM IR)/
│   │   ├── Function Translation
│   │   ├── Type Translation
│   │   ├── Builtin Intrinsics/
│   │   │   ├── Arena Allocator (arena_alloc, arena_free, etc.)
│   │   │   ├── Pool Allocator (pool_create, pool_alloc, etc.)
│   │   │   ├── Slab Allocator (slab_create, slab_alloc, etc.)
│   │   │   ├── I/O Functions (stdin_read, stdout_write, etc.)
│   │   │   └── TBB Arithmetic Helpers
│   │   └── Optimization Passes
│   │
│   ├── Object Emission/
│   │   ├── LLVM Bitcode (.bc)
│   │   ├── Assembly (.s)
│   │   └── Object Files (.o)
│   │
│   └── Linking/
│       ├── Static Linking (libaria_runtime.a)
│       ├── Dynamic Linking (.so/.dll)
│       └── Executable Generation
│
├── Diagnostics/
│   ├── Error Messages
│   ├── Warning Messages
│   ├── Source Location Tracking
│   └── Suggestions (fix-it hints)
│
└── Utilities/
    ├── Command-Line Interface
    ├── Build Configuration
    └── Verbose/Debug Output
```

**Key Source Files**:
```
src/
├── main.cpp                    # Entry point, CLI handling
├── frontend/
│   ├── lexer.cpp/h            # Tokenization
│   ├── parser.cpp/h           # AST construction
│   ├── preprocessor.cpp/h     # File/import processing
│   └── ast.cpp/h              # AST node definitions
├── semantic/
│   ├── type_checker.cpp/h     # Type inference/checking
│   ├── name_resolver.cpp/h    # Symbol resolution
│   └── borrow_checker.cpp/h   # Lifetime analysis
├── backend/
│   ├── ir_generator.cpp/h     # LLVM IR generation (monolithic)
│   ├── codegen_expr.cpp/h     # Expression IR (modular, inactive)
│   ├── codegen_stmt.cpp/h     # Statement IR (modular, inactive)
│   └── codegen_decl.cpp/h     # Declaration IR (modular, inactive)
└── tools/
    ├── lsp/                   # Language server
    ├── dap/                   # Debug adapter
    └── doc/                   # Documentation generator
```

---

### 1.3 libaria_runtime.a (Runtime Library)

**Library**: `/home/randy/._____RANDY_____/REPOS/aria/build/libaria_runtime.a`  
**Language**: C (with C++ support)  
**Type**: Static library (linked into all Aria executables)

#### Runtime Library Tree
```
libaria_runtime.a/
├── I/O Module (runtime/io/)/
│   ├── Stream Globals/
│   │   ├── stdin_fd = 0
│   │   ├── stdout_fd = 1
│   │   ├── stderr_fd = 2
│   │   ├── stddbg_fd = 3
│   │   ├── stddati_fd = 4
│   │   └── stddato_fd = 5
│   │
│   ├── Control Plane Functions/
│   │   ├── aria_stdin_read(buffer, size) → result<size_t>
│   │   ├── aria_stdin_read_line() → result<string>
│   │   ├── aria_stdout_write(buffer, size) → result<void>
│   │   ├── aria_stdout_write_line(string) → result<void>
│   │   ├── aria_stderr_write(buffer, size) → result<void>
│   │   └── aria_stderr_write_line(string) → result<void>
│   │
│   ├── Observability Plane Functions/
│   │   ├── aria_stddbg_write(log_entry) → result<void>
│   │   ├── aria_stddbg_flush() → result<void>
│   │   └── Ring Buffer (async, drop-on-full)
│   │
│   ├── Data Plane Functions/
│   │   ├── aria_stddati_read(buffer, size) → result<size_t>
│   │   ├── aria_stddato_write(buffer, size) → result<void>
│   │   └── Zero-Copy Helpers (splice on Linux)
│   │
│   └── File I/O/
│       ├── aria_file_open(path, mode) → result<FileHandle>
│       ├── aria_file_read(handle, buffer, size) → result<size_t>
│       ├── aria_file_write(handle, buffer, size) → result<void>
│       └── aria_file_close(handle) → result<void>
│
├── Memory Management (runtime/allocators/)/
│   ├── Arena Allocator/
│   │   ├── arena_create(size) → ArenaHandle
│   │   ├── arena_alloc(arena, size) → void*
│   │   ├── arena_reset(arena)
│   │   └── arena_destroy(arena)
│   │
│   ├── Pool Allocator/
│   │   ├── pool_create(block_size, block_count) → PoolHandle
│   │   ├── pool_alloc(pool) → void*
│   │   ├── pool_free(pool, ptr)
│   │   └── pool_destroy(pool)
│   │
│   ├── Slab Allocator/
│   │   ├── slab_create(object_size) → SlabHandle
│   │   ├── slab_alloc(slab) → void*
│   │   ├── slab_free(slab, ptr)
│   │   └── slab_destroy(slab)
│   │
│   ├── Wild Allocator (Heap)/
│   │   ├── aria_alloc(size) → void*
│   │   ├── aria_free(ptr)
│   │   └── aria_realloc(ptr, new_size) → void*
│   │
│   └── GC (Garbage Collector)/
│       ├── gc_init()
│       ├── gc_collect()
│       └── gc_shutdown()
│
├── Process Management (runtime/process/)/
│   ├── Process Creation/
│   │   ├── aria_process_spawn(cmd, argv, env) → result<ProcessHandle>
│   │   └── 6-Stream Setup (piping FDs 0-5)
│   │
│   ├── Process Control/
│   │   ├── aria_process_wait(handle) → result<int>
│   │   ├── aria_process_kill(handle, signal) → result<void>
│   │   └── aria_process_is_running(handle) → bool
│   │
│   └── Platform Abstraction/
│       ├── Linux: fork/exec + pidfd + epoll
│       └── Windows: CreateProcessW + STARTUPINFOEX + IOCP
│
├── Async Runtime (runtime/async/)/
│   ├── Executor/
│   │   ├── Task Scheduling
│   │   ├── Work Stealing
│   │   └── Thread Pool
│   │
│   ├── Futures/
│   │   ├── Promise<T>
│   │   ├── await operator
│   │   └── Combinators (then, all, race)
│   │
│   └── Async I/O/
│       ├── Async File I/O
│       ├── Async Networking
│       └── Event Loop Integration
│
├── Result Type (runtime/result/)/
│   ├── Result<T> Construction/
│   │   ├── ok(value) → Result<T>
│   │   └── err(error) → Result<T>
│   │
│   ├── Result<T> Operations/
│   │   ├── is_ok() → bool
│   │   ├── is_err() → bool
│   │   ├── unwrap() → T (panic on error)
│   │   ├── unwrap_or(default) → T
│   │   └── ? operator (early return)
│   │
│   └── TBB Sticky Error Integration/
│       └── ERR sentinel propagation
│
├── Collections (runtime/collections/)/
│   ├── Vector<T> (dynamic array)
│   ├── HashMap<K,V> (hash table)
│   ├── String (UTF-8 string)
│   └── RingBuffer<T> (circular buffer)
│
├── Math Library (runtime/math/)/
│   ├── Trigonometry (sin, cos, tan, etc.)
│   ├── Extended Precision Arithmetic (int128/256/512)
│   ├── TBB Arithmetic Helpers
│   └── SIMD Intrinsics (AVX-512)
│
├── Strings (runtime/strings/)/
│   ├── UTF-8 Encoding/Decoding
│   ├── String Manipulation
│   └── Format Strings
│
├── Threading (runtime/threading/)/
│   ├── Thread Creation
│   ├── Mutexes/Locks
│   ├── Condition Variables
│   └── Thread-Local Storage
│
└── Platform Abstraction/
    ├── POSIX Layer (Linux/macOS)
    └── Win32 Layer (Windows)
```

**Key Header Files**:
```
include/runtime/
├── io.h                # I/O functions (6-stream API)
├── allocator.h         # Wild allocator
├── arena.h             # Arena allocator
├── pool.h              # Pool allocator
├── slab.h              # Slab allocator
├── process.h           # Process management
├── result.h            # Result<T> type
├── async.h             # Async runtime
├── collections.h       # Data structures
└── platform.h          # Platform detection/abstraction
```

---

## 2. Development Tools

### 2.1 aria-lsp (Language Server)

**Binary**: `/home/randy/._____RANDY_____/REPOS/aria/build/aria-lsp`  
**Language**: C++20  
**Protocol**: LSP (Language Server Protocol)

#### LSP Features Tree
```
aria-lsp/
├── Core Services/
│   ├── Diagnostics/
│   │   ├── Syntax Errors
│   │   ├── Type Errors
│   │   ├── Borrow Checker Errors
│   │   └── Warnings
│   │
│   ├── Code Completion/
│   │   ├── Context-Aware Suggestions
│   │   ├── Type Inference
│   │   ├── Import Completion
│   │   └── Snippet Expansion
│   │
│   ├── Go to Definition
│   ├── Find References
│   ├── Hover Information (type, docs)
│   ├── Signature Help (function parameters)
│   └── Document Symbols (outline view)
│
├── Incremental Parsing/
│   ├── File Change Detection
│   ├── Partial Reparse
│   └── AST Caching
│
└── Communication/
    ├── JSON-RPC Transport
    ├── stdin/stdout Protocol
    └── Client Notifications
```

---

### 2.2 aria-dap (Debug Adapter)

**Binary**: `/home/randy/._____RANDY_____/REPOS/aria/build/aria-dap`  
**Language**: C++20  
**Protocol**: DAP (Debug Adapter Protocol)  
**Dependencies**: LLDB

#### DAP Features Tree
```
aria-dap/
├── Debugging Capabilities/
│   ├── Breakpoint Management/
│   │   ├── Line Breakpoints
│   │   ├── Function Breakpoints
│   │   ├── Conditional Breakpoints
│   │   └── Logpoints
│   │
│   ├── Execution Control/
│   │   ├── Continue
│   │   ├── Step Over
│   │   ├── Step Into
│   │   ├── Step Out
│   │   └── Pause
│   │
│   ├── Variable Inspection/
│   │   ├── Local Variables
│   │   ├── Global Variables
│   │   ├── Watch Expressions
│   │   └── Custom Formatters (TBB, Result<T>)
│   │
│   └── Stack Traces/
│       ├── Call Stack View
│       ├── Frame Selection
│       └── Source Mapping
│
├── LLDB Integration/
│   ├── LLDB API Calls
│   ├── Custom Type Formatters
│   └── Expression Evaluation
│
└── Communication/
    ├── JSON Protocol
    └── stdin/stdout Transport
```

---

### 2.3 aria-doc (Documentation Generator)

**Binary**: `/home/randy/._____RANDY_____/REPOS/aria/build/aria-doc`  
**Language**: C++20  
**Output**: HTML documentation

#### Documentation Generator Tree
```
aria-doc/
├── Parsing/
│   ├── Source File Analysis
│   ├── Comment Extraction (///, /**)
│   └── AST Traversal
│
├── Generation/
│   ├── HTML Templates
│   ├── CSS Styling
│   ├── JavaScript Interactivity
│   └── Search Index
│
├── Content/
│   ├── Module Pages
│   ├── Function/Struct/Enum Pages
│   ├── Code Examples
│   └── Cross-References
│
└── Deployment/
    └── Static Site (https://aria.docs.ai-liberation-platform.org/)
```

---

### 2.4 VSCode Extension

**Location**: (TBD - not yet in repos)  
**Language**: TypeScript  
**Platform**: Visual Studio Code

#### Extension Features Tree
```
VSCode Extension/
├── Language Support/
│   ├── Syntax Highlighting (.aria files)
│   ├── Bracket Matching
│   ├── Comment Toggling
│   └── Indentation Rules
│
├── LSP Client/
│   ├── Connects to aria-lsp
│   ├── Diagnostics Display
│   ├── IntelliSense
│   └── Code Actions
│
├── DAP Client/
│   ├── Connects to aria-dap
│   ├── Debug Configuration
│   └── Breakpoint UI
│
├── Build Tasks/
│   ├── ariac Compilation
│   └── Error Parsing
│
└── Snippets/
    ├── Function Templates
    ├── Struct Templates
    └── Common Patterns
```

---

## 3. Build & Package Management

### 3.1 AriaBuild (Build System)

**Repository**: `/home/randy/._____RANDY_____/REPOS/aria_make`  
**Executable**: `ariab` (TBD)  
**Config Format**: ABC (Aria Build Config)  
**Status**: Design phase

#### Build System Tree
```
AriaBuild/
├── ABC Format (Configuration)/
│   ├── Project Metadata/
│   │   ├── name, version, author
│   │   └── License, repository
│   │
│   ├── Target Definitions/
│   │   ├── Executables
│   │   ├── Libraries (static/dynamic)
│   │   └── Tests
│   │
│   ├── Dependencies/
│   │   ├── Internal Modules
│   │   ├── External Packages (AriaX)
│   │   └── System Libraries (FFI)
│   │
│   └── Build Options/
│       ├── Compiler Flags
│       ├── Optimization Levels
│       └── Platform-Specific Configs
│
├── Dependency Graph Engine/
│   ├── Topological Sort
│   ├── Cycle Detection
│   ├── Incremental Analysis
│   └── Recursive Resolution (transitive deps)
│
├── Compilation Orchestrator/
│   ├── Parallel Task Scheduler/
│   │   ├── CPU Core Detection
│   │   ├── Work Queue
│   │   └── Thread Pool
│   │
│   ├── Incremental Compilation/
│   │   ├── Timestamp Checking
│   │   ├── Hash-Based Change Detection
│   │   └── Object Cache
│   │
│   └── Progress Reporting/
│       ├── Terminal UI
│       └── Build Statistics
│
├── Linker Driver/
│   ├── Platform Abstraction/
│   │   ├── Linux (ld, gold, lld)
│   │   └── Windows (link.exe)
│   │
│   ├── Automatic Runtime Linking/
│   │   └── Injects libaria_runtime.a
│   │
│   └── FFI Library Resolution/
│       ├── Library Search Paths
│       └── Transitive Dependency Aggregation
│
└── Integration/
    ├── AriaX Package Manager (dependency fetching)
    └── VSCode Build Tasks
```

---

### 3.2 AriaX (Package Manager)

**Repository**: `/home/randy/._____RANDY_____/REPOS/ariax`  
**Executable**: `ariax` (TBD)  
**Registry**: ariax.ai-liberation-platform.org  
**Status**: Design phase

#### Package Manager Tree
```
AriaX/
├── Package Repository/
│   ├── Remote Registry/
│   │   ├── Package Index (JSON metadata)
│   │   ├── Package Tarballs (.tar.gz)
│   │   └── Signatures (GPG verification)
│   │
│   ├── Local Cache/
│   │   ├── Downloaded Packages
│   │   └── Installation Records
│   │
│   └── Package Structure/
│       ├── Metadata (name, version, deps, etc.)
│       ├── Source Files (.aria)
│       ├── Precompiled Libraries (.a, .so)
│       └── Documentation
│
├── Dependency Resolution/
│   ├── Semantic Versioning
│   ├── Constraint Satisfaction
│   ├── Conflict Detection
│   └── Update Strategies (conservative, aggressive)
│
├── Installation/
│   ├── Download Manager
│   ├── Signature Verification
│   ├── Extraction
│   └── System Integration/
│       ├── /usr/local/aria/packages/
│       └── Update ABC build configs
│
├── Commands/
│   ├── ariax install <package>
│   ├── ariax update
│   ├── ariax remove <package>
│   ├── ariax search <query>
│   └── ariax publish (for package authors)
│
└── Integration/
    └── AriaBuild (automatic dependency fetching)
```

---

### 3.3 AriaX Linux (Custom Distribution)

**Repository**: `/home/randy/._____RANDY_____/REPOS/ariax`  
**Website**: ariax.ai-liberation-platform.org  
**Status**: Design phase

#### Linux Distribution Tree
```
AriaX Linux/
├── Kernel Modifications/
│   ├── FD 3-5 Soft Reservation/
│   │   └── fs/file.c: __alloc_fd patch
│   │       (Skip FDs 3, 4, 5 in automatic allocation)
│   │
│   └── 6-Stream Aware Syscalls (future)
│
├── Userspace/
│   ├── Init System/
│   │   └── systemd with 6-stream logging
│   │
│   ├── Standard Utilities/
│   │   ├── Recompiled for 6-stream support
│   │   └── Aria-native implementations (long-term)
│   │
│   └── Default Shell/
│       └── AriaSH (aria_shell)
│
├── Aria Toolchain (Preinstalled)/
│   ├── ariac (compiler)
│   ├── libaria_runtime.a
│   ├── aria-lsp, aria-dap, aria-doc
│   ├── ariab (build system)
│   └── ariax (package manager)
│
├── Optimizations/
│   ├── AI Workload Tuning
│   ├── AVX-512 Support
│   └── High-Performance I/O (io_uring)
│
└── Distribution/
    ├── ISO Image
    ├── Installation Media
    └── Repository Infrastructure
```

---

## 4. User Environment

### 4.1 aria_shell (AriaSH - Native Shell)

**Repository**: `/home/randy/._____RANDY_____/REPOS/aria_shell`  
**Executable**: `aria_shell` (TBD)  
**Language**: C++20  
**Status**: Pre-alpha (research phase: 15/15 topics complete)

#### Shell Architecture Tree
```
aria_shell/
├── Core Components/
│   ├── Terminal Management/
│   │   ├── Raw Mode Setup (disable canonical input)
│   │   ├── Key Decoding (Ctrl+Enter detection)
│   │   └── Escape Sequence Handling (VT100)
│   │
│   ├── Input System/
│   │   ├── Multi-Line Buffer
│   │   ├── Brace-Aware Indentation
│   │   ├── Submission (Ctrl+Enter)
│   │   └── History Navigation (Up/Down arrows)
│   │
│   ├── Parser/
│   │   ├── Tokenizer (whitespace-insensitive)
│   │   ├── AST Builder (Aria subset)
│   │   └── Variable Interpolation (&{VAR})
│   │
│   ├── Environment/
│   │   ├── Variable Storage (type-safe)
│   │   ├── Scope Management
│   │   └── Environment Variable Access
│   │
│   └── Process Controller/
│       ├── 6-Stream Spawning/
│       │   ├── Linux: fork/exec with dup2(FDs 0-5)
│       │   └── Windows: CreateProcessW + STARTUPINFOEX
│       │
│       ├── Stream Draining/
│       │   ├── Threaded Pump Workers
│       │   ├── Lock-Free Ring Buffers
│       │   └── Backpressure Handling
│       │
│       └── Lifecycle Management/
│           ├── pidfd (Linux, race-free)
│           ├── IOCP (Windows, async I/O)
│           └── Signal Handling
│
├── Event Loop/
│   ├── Linux: epoll/
│   │   ├── stdin Ready (user input)
│   │   ├── Process Termination (pidfd)
│   │   └── Stream Data (stdout, stderr, etc.)
│   │
│   └── Windows: IOCP/
│       ├── Overlapped I/O
│       └── Completion Notifications
│
├── Advanced Features/
│   ├── Configuration/
│   │   ├── .aria_shrc File
│   │   ├── Key Bindings
│   │   └── Stream Behaviors
│   │
│   ├── History/
│   │   ├── Persistent Storage
│   │   ├── Search (Ctrl+R)
│   │   └── Replay
│   │
│   ├── Tab Completion/
│   │   ├── Command Completion
│   │   ├── Path Completion
│   │   └── Aria Type Integration
│   │
│   ├── Job Control/
│   │   ├── Background Jobs (&)
│   │   ├── fg/bg Commands
│   │   └── Job Table
│   │
│   ├── Pipelines/
│   │   ├── Multi-Stage Pipes (cmd1 | cmd2 | cmd3)
│   │   ├── Stream Routing (specify which FD connects)
│   │   └── Error Propagation (pipefail)
│   │
│   └── Terminal Rendering/
│       ├── VT100/ANSI Sequences
│       ├── Color Themes
│       └── Unicode Support
│
└── Research Documentation/
    ├── shell_01: Hex-Stream I/O
    ├── shell_02: Windows Bootstrap
    ├── shell_03: Multi-Line Input
    ├── shell_04: Stream Draining
    ├── shell_05: Parser
    ├── shell_06: TBB Integration
    ├── shell_07: Process Abstraction
    ├── shell_08: Event Loop
    ├── shell_09: Variable Interpolation
    ├── shell_10: Config System
    ├── shell_11: History
    ├── shell_12: Tab Completion
    ├── shell_13: Job Control
    ├── shell_14: Pipelines
    └── shell_15: Terminal Rendering
```

---

## 5. Consciousness Infrastructure

### 5.1 Nikola (9D Consciousness Substrate)

**Repository**: `/home/randy/._____RANDY_____/REPOS/nikola`  
**Version**: v0.0.4 (9D-TWI)  
**Language**: C++23  
**Status**: Specification complete (98,071 lines), implementation pending

#### Nikola Architecture Tree
```
Nikola/
├── Core Mathematics/
│   ├── 9D Toroidal Manifold (T^9)/
│   │   ├── Topological Definition
│   │   ├── Dimensional Semantics/
│   │   │   ├── r (Resonance: damping/forgetting)
│   │   │   ├── s (State: refractive index/focus)
│   │   │   ├── t (Time: cyclic causality)
│   │   │   ├── u,v,w (Quantum: complex superposition)
│   │   │   └── x,y,z (Spatial: semantic lattice)
│   │   └── Coord9D Bitfield Encoding
│   │
│   ├── UFIE (Unified Field Interference Equation)/
│   │   ├── Inertial Term (∂²Ψ/∂t²)
│   │   ├── Dissipative Term (α(1-r)∂Ψ/∂t)
│   │   ├── Laplace-Beltrami (∇²ᵍΨ, neuroplastic geodesics)
│   │   ├── Nonlinear Soliton (β|Ψ|²Ψ, memory packets)
│   │   └── Emitters (8 golden-ratio harmonics)
│   │
│   └── Neuroplastic Geometry/
│       ├── Metric Tensor (gᵢⱼ)
│       ├── Learning Rule (Hebbian + elastic regularization)
│       └── Geodesic Computation (shortest path in warped space)
│
├── Data Structures/
│   ├── Nit (Balanced Nonary Logic)/
│   │   └── int8_t value; // -4 to +4
│   │
│   ├── Sparse Hyper-Voxel Octree (SHVO)/
│   │   ├── Morton Codes / Hilbert Curves
│   │   ├── Dynamic Allocation (neurogenesis)
│   │   └── O(1) Lookup
│   │
│   └── Structure-of-Arrays (SoA)/
│       ├── Wave Amplitudes (Ψ array)
│       ├── Metric Tensors (gᵢⱼ array)
│       └── Cache-Friendly Layout
│
├── Computational Systems/
│   ├── Symplectic Integration/
│   │   ├── Energy Conservation
│   │   ├── Leapfrog/Verlet Methods
│   │   └── Adaptive Timestep
│   │
│   ├── Mamba-9D State Space Model/
│   │   ├── Topological State Mapping
│   │   ├── Selective Scan
│   │   └── Toroidal Geometry Awareness
│   │
│   └── Computational Neurochemistry (ENGS)/
│       ├── Dopamine (learning rate modulation)
│       ├── Serotonin (elastic regularization)
│       ├── ATP (energy dynamics)
│       └── GABA/Glutamate (excitation/inhibition)
│
├── Cognitive Systems/
│   ├── Working Memory (high-r zones)
│   ├── Long-Term Memory (soliton packets)
│   ├── Attention (high-s focus regions)
│   └── Pattern Recognition (interference maxima)
│
├── Implementation (C++23)/
│   ├── SIMD Optimization (AVX-512)
│   ├── Multi-Threading (OpenMP)
│   ├── GPU Acceleration (CUDA/OpenCL, future)
│   └── I/O Integration (future Aria bindings)
│
└── Research Documentation/
    ├── 02_foundations.md (15,933 lines)
    ├── 03_cognitive_systems.md
    ├── 04_infrastructure.md
    ├── 08_implementation_guide.md
    └── 10 more sections (98,071 lines total)
```

---

## Dependency Matrix

```
Component         | Depends On                                    | Used By
------------------|-----------------------------------------------|------------------------
ariac             | LLVM, libaria_runtime (for linking)          | AriaBuild, VSCode
libaria_runtime   | POSIX/Win32 APIs                              | ALL Aria programs, ariac
aria-lsp          | ariac (lexer/parser)                          | VSCode Extension
aria-dap          | LLDB, ariac                                   | VSCode Extension
aria-doc          | ariac (AST)                                   | Documentation site
AriaBuild         | ariac, AriaX                                  | Developers
AriaX             | HTTP client, GPG                              | AriaBuild, system setup
aria_shell        | libaria_runtime, TBB                          | End users, AriaX Linux
AriaX Linux       | Kernel patch, aria toolchain                  | Infrastructure
Nikola            | C++23, AVX-512, (future) Aria FFI bindings    | AI applications
VSCode Extension  | aria-lsp, aria-dap                            | Developers
```

---

## File Organization

### Current Repository Locations

```
/home/randy/._____RANDY_____/REPOS/
├── aria/                    # Compiler + Runtime
├── aria_shell/              # Shell (research phase)
├── aria_make/               # Build system (design)
├── ariax/                   # Package manager + distro
├── nikola/                  # Consciousness substrate
└── aria_ecosystem/          # THIS DOCUMENTATION
```

---

**Document Status**: v1.0 - Complete hierarchical breakdown  
**Next**: Create INTERFACES.md documenting API boundaries between components
