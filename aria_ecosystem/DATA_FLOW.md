# Aria Ecosystem - Data Flow Analysis

**Document Version**: 1.0  
**Last Updated**: December 22, 2025  
**Purpose**: Trace data movement through the ecosystem in real-world scenarios

---

## Table of Contents

1. [Compilation Flow](#1-compilation-flow)
2. [Program Execution Flow](#2-program-execution-flow)
3. [Build System Flow](#3-build-system-flow)
4. [Package Installation Flow](#4-package-installation-flow)
5. [Development Workflow](#5-development-workflow)
6. [6-Stream I/O Data Flow](#6-6-stream-io-data-flow)
7. [Error Propagation Flow](#7-error-propagation-flow)

---

## 1. Compilation Flow

### 1.1 Source Code → Executable

```
┌─────────────────────────────────────────────────────────────────┐
│ COMPILATION PIPELINE                                             │
└─────────────────────────────────────────────────────────────────┘

INPUT: hello.aria
───────────────────────────────────────────────────────────────────
func:main = int64() {
    io.stdout.write("Hello, World!\n");
    pass(0);
}
───────────────────────────────────────────────────────────────────

                    ↓

┌────────────────────────────────────────────────────────────┐
│ STAGE 1: Lexical Analysis (Lexer)                         │
│ Input: Source text                                         │
│ Output: Token stream                                       │
└────────────────────────────────────────────────────────────┘

Tokens:
  KEYWORD(func), COLON, IDENTIFIER(main), EQUALS,
  TYPE(int64), LPAREN, RPAREN, LBRACE,
  IDENTIFIER(io), DOT, IDENTIFIER(stdout), DOT,
  IDENTIFIER(write), LPAREN, STRING("Hello, World!\n"), RPAREN, SEMICOLON,
  KEYWORD(pass), LPAREN, NUMBER(0), RPAREN, SEMICOLON,
  RBRACE

                    ↓

┌────────────────────────────────────────────────────────────┐
│ STAGE 2: Syntax Analysis (Parser)                         │
│ Input: Token stream                                        │
│ Output: Abstract Syntax Tree (AST)                        │
└────────────────────────────────────────────────────────────┘

AST:
  FunctionDecl {
    name: "main",
    return_type: int64,
    params: [],
    body: Block {
      statements: [
        ExprStmt {
          expr: MethodCall {
            object: FieldAccess {
              object: Identifier("io"),
              field: "stdout"
            },
            method: "write",
            args: [StringLiteral("Hello, World!\n")]
          }
        },
        PassStmt {
          value: IntLiteral(0)
        }
      ]
    }
  }

                    ↓

┌────────────────────────────────────────────────────────────┐
│ STAGE 3: Semantic Analysis                                │
│ Input: AST                                                 │
│ Output: Typed AST + Symbol Table                          │
└────────────────────────────────────────────────────────────┘

Symbol Table:
  main: func() -> int64
  io: module(std.io)
  io.stdout: TextStream
  io.stdout.write: func(str: string) -> result<void>

Type Checking:
  ✓ "Hello, World!\n" : string
  ✓ io.stdout.write(string) : result<void>
  ✓ 0 : int64
  ✓ main returns int64

                    ↓

┌────────────────────────────────────────────────────────────┐
│ STAGE 4: IR Generation (LLVM IR)                          │
│ Input: Typed AST                                           │
│ Output: LLVM Intermediate Representation                  │
└────────────────────────────────────────────────────────────┘

LLVM IR:
───────────────────────────────────────────────────────────────────
define i64 @main() {
entry:
  ; Call runtime function aria_stdout_write
  %str = getelementptr inbounds [15 x i8], [15 x i8]* @.str, i32 0, i32 0
  %result = call %AriaResult* @aria_stdout_write(i8* %str, i64 14)
  
  ; Check if result is error (should handle, but simplified here)
  ; ...
  
  ; Return 0
  ret i64 0
}

@.str = private unnamed_addr constant [15 x i8] c"Hello, World!\0A\00"

declare %AriaResult* @aria_stdout_write(i8*, i64)
───────────────────────────────────────────────────────────────────

                    ↓

┌────────────────────────────────────────────────────────────┐
│ STAGE 5: LLVM Optimization                                │
│ Input: LLVM IR                                             │
│ Output: Optimized LLVM IR                                  │
└────────────────────────────────────────────────────────────┘

Optimization Passes:
  - Dead code elimination
  - Constant propagation
  - Inlining (small functions)
  - Register allocation hints

                    ↓

┌────────────────────────────────────────────────────────────┐
│ STAGE 6: Object Code Emission                             │
│ Input: Optimized LLVM IR                                   │
│ Output: Object file (.o)                                   │
└────────────────────────────────────────────────────────────┘

hello.o:
  ELF 64-bit LSB relocatable, x86-64
  Contains:
    - Compiled machine code for main()
    - Relocation table (undefined symbols: aria_stdout_write)
    - Symbol table (defined: main)
    - String constants (.rodata section)

                    ↓

┌────────────────────────────────────────────────────────────┐
│ STAGE 7: Linking                                           │
│ Input: Object file + libaria_runtime.a                    │
│ Output: Executable                                         │
└────────────────────────────────────────────────────────────┘

Link Command:
  clang hello.o /usr/local/lib/libaria_runtime.a -o hello

Symbol Resolution:
  hello.o: needs aria_stdout_write
  libaria_runtime.a: provides aria_stdout_write ✓
  
  All symbols resolved ✓

OUTPUT: hello (executable)
  ELF 64-bit LSB executable, x86-64
  Contains:
    - main() with inlined runtime calls
    - Runtime library code (I/O, allocators, etc.)
    - Linked system libraries (libc)
```

---

### 1.2 Data Transformations at Each Stage

| Stage | Input Format | Output Format | Data Structure |
|-------|--------------|---------------|----------------|
| Lexer | UTF-8 text | Token array | `vector<Token>` |
| Parser | Token array | AST nodes | `unique_ptr<ASTNode>` |
| Semantic | AST nodes | Typed AST + symbols | `SymbolTable` map |
| IR Gen | Typed AST | LLVM IR Module | `llvm::Module*` |
| Optimize | LLVM IR | Optimized IR | In-place transform |
| Emit | LLVM IR | Machine code | Binary bytes |
| Link | Object files | Executable | ELF/PE binary |

---

## 2. Program Execution Flow

### 2.1 Hello World Execution Trace

```
┌─────────────────────────────────────────────────────────────────┐
│ EXECUTION TRACE: ./hello                                         │
└─────────────────────────────────────────────────────────────────┘

TIME: t0 - Process Start
─────────────────────────────────────────────────────────────────
Kernel: execve("/path/to/hello", [], [env])
  ↓
Kernel: Map executable into memory
  ↓
Kernel: Set up stack, heap
  ↓
Kernel: Initialize FDs:
  - FD 0 (stdin): Terminal input
  - FD 1 (stdout): Terminal output
  - FD 2 (stderr): Terminal error
  - FD 3-5: Not yet initialized (program-specific)
  ↓
Kernel: Jump to entry point (_start)

─────────────────────────────────────────────────────────────────
TIME: t1 - C Runtime Initialization
─────────────────────────────────────────────────────────────────
_start:
  ↓
Call __libc_start_main
  ↓
C Runtime: Initialize standard library
  ↓
C Runtime: Run static constructors (__attribute__((constructor)))
  ↓
Aria Runtime: aria_runtime_init() [static constructor]
  ↓
  Initialize stream handles:
    io.stdin_fd = 0;
    io.stdout_fd = 1;
    io.stderr_fd = 2;
    io.stddbg_fd = 3;  // May open /dev/null if not provided
    io.stddati_fd = 4; // May open /dev/null if not provided
    io.stddato_fd = 5; // May open /dev/null if not provided
  ↓
  Initialize allocators (set up global state)
  ↓
  aria_runtime_init() returns

─────────────────────────────────────────────────────────────────
TIME: t2 - main() Entry
─────────────────────────────────────────────────────────────────
Call main()
  ↓
  [Stack Frame: main]
  ↓
  Instruction: Call @aria_stdout_write
    ↓
    [Stack Frame: aria_stdout_write]
    ↓
    Function: aria_stdout_write(str="Hello, World!\n", len=14)
      ↓
      Internal: Get stdout handle (FD 1)
      ↓
      System Call: write(1, "Hello, World!\n", 14)
        ↓
        Kernel: Write to terminal output buffer
        ↓
        Terminal: Display "Hello, World!\n"
      ↓
      System Call returns: 14 (bytes written)
      ↓
      Create result: aria_result_ok(NULL, 0)
      ↓
      Return result pointer
    ↓
    [Back to main]
  ↓
  Instruction: Check result (simplified - actual code should check)
  ↓
  Instruction: Return 0
  ↓
  [Exit main()]

─────────────────────────────────────────────────────────────────
TIME: t3 - Process Cleanup
─────────────────────────────────────────────────────────────────
main() returned 0
  ↓
C Runtime: Run static destructors
  ↓
Aria Runtime: aria_runtime_shutdown() [static destructor]
  ↓
  Flush all stream buffers
  ↓
  Release allocated memory
  ↓
  Close file descriptors (if owned)
  ↓
  aria_runtime_shutdown() returns
  ↓
C Runtime: Call exit(0)
  ↓
Kernel: Terminate process
  ↓
Kernel: Close all FDs
  ↓
Kernel: Release memory
  ↓
Kernel: Signal parent (SIGCHLD if applicable)
  ↓
Process terminated with exit code 0

─────────────────────────────────────────────────────────────────
TERMINAL OUTPUT:
Hello, World!
```

---

### 2.2 Memory Flow During Execution

```
HEAP ALLOCATIONS:

aria_stdout_write() calls aria_result_ok():
  ┌──────────────────────────────────────┐
  │ malloc(sizeof(AriaResult))           │
  │   ↓                                  │
  │ Address: 0x7f8a3c001020             │
  │ Size: 24 bytes                      │
  │ Content:                            │
  │   err: NULL (8 bytes)               │
  │   val: NULL (8 bytes)               │
  │   val_size: 0 (8 bytes)             │
  └──────────────────────────────────────┘
          ↓
  Returned to main()
          ↓
  [Should be freed with aria_result_free(), but
   simplified example doesn't show cleanup]
          ↓
  [Memory leak in this example - real code must free!]

Total heap usage: 24 bytes (leaked in this simplified example)
```

---

## 3. Build System Flow

### 3.1 Multi-File Project Build

```
┌─────────────────────────────────────────────────────────────────┐
│ PROJECT STRUCTURE                                                │
└─────────────────────────────────────────────────────────────────┘

my_project/
├── build.abc
├── src/
│   ├── main.aria
│   ├── parser.aria
│   └── utils.aria
└── lib/
    └── helpers.aria

─────────────────────────────────────────────────────────────────
build.abc:
{
  "project": {
    "name": "my_app",
    "version": "1.0.0"
  },
  "targets": {
    "binary": {
      "type": "executable",
      "output": "bin/my_app",
      "sources": [
        "src/main.aria",
        "src/parser.aria",
        "src/utils.aria",
        "lib/helpers.aria"
      ]
    }
  }
}
─────────────────────────────────────────────────────────────────

┌─────────────────────────────────────────────────────────────────┐
│ BUILD FLOW                                                       │
└─────────────────────────────────────────────────────────────────┘

USER: $ ariab build

  ↓

┌──────────────────────────────────────────────────────────────┐
│ STEP 1: Parse Build Configuration                           │
└──────────────────────────────────────────────────────────────┘
Read build.abc
  ↓
Parse JSON
  ↓
Extract:
  - Sources: [main.aria, parser.aria, utils.aria, helpers.aria]
  - Output: bin/my_app
  - Type: executable

  ↓

┌──────────────────────────────────────────────────────────────┐
│ STEP 2: Dependency Graph Construction                       │
└──────────────────────────────────────────────────────────────┘
Parse imports in each source file:

main.aria:
  use parser;
  use utils;
  → depends on: parser.aria, utils.aria

parser.aria:
  use utils;
  use helpers;
  → depends on: utils.aria, helpers.aria

utils.aria:
  use helpers;
  → depends on: helpers.aria

helpers.aria:
  (no imports)
  → depends on: (none)

Dependency Graph:
  helpers.aria  (no deps)
     ↑
     ├── utils.aria  (depends on helpers)
     │     ↑
     │     ├── parser.aria  (depends on utils, helpers)
     │     │     ↑
     │     │     └── main.aria
     │     └── main.aria
     └── (already covered)

Topological Sort (build order):
  1. helpers.aria
  2. utils.aria
  3. parser.aria
  4. main.aria

  ↓

┌──────────────────────────────────────────────────────────────┐
│ STEP 3: Incremental Build Check                             │
└──────────────────────────────────────────────────────────────┘
For each source file, check if object file is up-to-date:

helpers.aria: (modified yesterday)
  Object: lib/helpers.o (exists, timestamp: yesterday)
  → UP TO DATE, skip compilation

utils.aria: (modified today)
  Object: src/utils.o (exists, timestamp: yesterday)
  → OUT OF DATE, needs recompilation

parser.aria: (modified yesterday)
  Object: src/parser.o (exists, timestamp: yesterday)
  → UP TO DATE, but depends on utils.aria which changed
  → NEEDS RECOMPILATION (dependency changed)

main.aria: (modified today)
  Object: src/main.o (exists, timestamp: yesterday)
  → OUT OF DATE, needs recompilation

Compilation Queue:
  - utils.aria (source modified)
  - parser.aria (dependency modified)
  - main.aria (source modified)

  ↓

┌──────────────────────────────────────────────────────────────┐
│ STEP 4: Parallel Compilation                                │
└──────────────────────────────────────────────────────────────┘
Detect CPU cores: 8 cores available
Spawn 3 compiler instances (parallelizable tasks):

[Thread 1] ariac src/utils.aria -o src/utils.o
[Thread 2] ariac src/parser.aria -o src/parser.o  (waits for utils.o)
[Thread 3] ariac src/main.aria -o src/main.o      (waits for parser.o, utils.o)

Timeline:
  t=0s:  Thread 1 starts (utils.aria)
  t=0s:  Thread 2 waits (parser depends on utils)
  t=0s:  Thread 3 waits (main depends on parser, utils)
  
  t=2s:  Thread 1 completes (utils.o ready)
  t=2s:  Thread 2 starts (parser.aria)
  t=2s:  Thread 3 still waits
  
  t=3s:  Thread 2 completes (parser.o ready)
  t=3s:  Thread 3 starts (main.aria)
  
  t=4s:  Thread 3 completes (main.o ready)

All object files ready:
  - lib/helpers.o (cached)
  - src/utils.o (recompiled)
  - src/parser.o (recompiled)
  - src/main.o (recompiled)

  ↓

┌──────────────────────────────────────────────────────────────┐
│ STEP 5: Linking                                              │
└──────────────────────────────────────────────────────────────┘
Collect object files:
  lib/helpers.o
  src/utils.o
  src/parser.o
  src/main.o

Locate runtime library:
  /usr/local/lib/libaria_runtime.a

Link command:
  clang lib/helpers.o src/utils.o src/parser.o src/main.o \
        /usr/local/lib/libaria_runtime.a \
        -o bin/my_app

Linker resolves symbols:
  main.o: needs parser_parse() → found in parser.o ✓
  main.o: needs utils_format() → found in utils.o ✓
  utils.o: needs helpers_trim() → found in helpers.o ✓
  main.o: needs aria_stdout_write() → found in libaria_runtime.a ✓
  ... (all symbols resolved)

Output: bin/my_app (executable)

  ↓

┌──────────────────────────────────────────────────────────────┐
│ BUILD COMPLETE                                                │
└──────────────────────────────────────────────────────────────┘
Total time: 4.2 seconds
Files compiled: 3 (1 cached)
Output: bin/my_app
```

---

## 4. Package Installation Flow

```
┌─────────────────────────────────────────────────────────────────┐
│ PACKAGE INSTALLATION: ariax install std.collections             │
└─────────────────────────────────────────────────────────────────┘

USER: $ ariax install std.collections

  ↓

┌──────────────────────────────────────────────────────────────┐
│ STEP 1: Fetch Package Index                                  │
└──────────────────────────────────────────────────────────────┘
HTTP GET: https://ariax.ai-liberation-platform.org/index.json
  ↓
Response (JSON):
{
  "packages": [
    {
      "name": "std.collections",
      "version": "1.0.0",
      "dependencies": ["std.mem"],
      "tarball_url": "https://.../std.collections-1.0.0.tar.gz",
      "signature_url": "https://.../std.collections-1.0.0.tar.gz.sig",
      "checksum_sha256": "abc123..."
    },
    {
      "name": "std.mem",
      "version": "1.0.0",
      "dependencies": [],
      "tarball_url": "https://.../std.mem-1.0.0.tar.gz",
      ...
    }
  ]
}
  ↓
Parse: std.collections found ✓

  ↓

┌──────────────────────────────────────────────────────────────┐
│ STEP 2: Dependency Resolution                                │
└──────────────────────────────────────────────────────────────┘
std.collections depends on:
  - std.mem

std.mem depends on:
  - (none)

Dependency tree:
  std.mem (no deps)
    ↑
    └── std.collections

Install order:
  1. std.mem
  2. std.collections

  ↓

┌──────────────────────────────────────────────────────────────┐
│ STEP 3: Download Packages                                    │
└──────────────────────────────────────────────────────────────┘
Download std.mem:
  HTTP GET: https://.../std.mem-1.0.0.tar.gz
    ↓
  Save to: /tmp/ariax/std.mem-1.0.0.tar.gz (1.2 MB)
    ↓
  HTTP GET: https://.../std.mem-1.0.0.tar.gz.sig
    ↓
  Save to: /tmp/ariax/std.mem-1.0.0.tar.gz.sig (512 bytes)

Download std.collections:
  HTTP GET: https://.../std.collections-1.0.0.tar.gz
    ↓
  Save to: /tmp/ariax/std.collections-1.0.0.tar.gz (2.5 MB)
    ↓
  HTTP GET: https://.../std.collections-1.0.0.tar.gz.sig
    ↓
  Save to: /tmp/ariax/std.collections-1.0.0.tar.gz.sig (512 bytes)

  ↓

┌──────────────────────────────────────────────────────────────┐
│ STEP 4: Verify Signatures                                    │
└──────────────────────────────────────────────────────────────┘
Verify std.mem:
  GPG verify: std.mem-1.0.0.tar.gz using std.mem-1.0.0.tar.gz.sig
    ↓
  Result: Signature valid, signed by Aria Team <team@aria.org> ✓
    ↓
  SHA-256: Compute hash of tarball
    ↓
  Hash: abc123...
    ↓
  Compare with index.json: abc123... == abc123... ✓

Verify std.collections:
  GPG verify: std.collections-1.0.0.tar.gz
    ↓
  Result: Signature valid ✓
    ↓
  SHA-256: def456...
    ↓
  Compare: def456... == def456... ✓

  ↓

┌──────────────────────────────────────────────────────────────┐
│ STEP 5: Extract and Install                                  │
└──────────────────────────────────────────────────────────────┘
Extract std.mem:
  tar -xzf /tmp/ariax/std.mem-1.0.0.tar.gz -C /usr/local/aria/packages/
    ↓
  Created: /usr/local/aria/packages/std.mem-1.0.0/
    ├── src/
    │   ├── allocator.aria
    │   └── arena.aria
    ├── lib/
    │   └── libstd_mem.a (precompiled)
    └── package.json

Extract std.collections:
  tar -xzf /tmp/ariax/std.collections-1.0.0.tar.gz -C /usr/local/aria/packages/
    ↓
  Created: /usr/local/aria/packages/std.collections-1.0.0/
    ├── src/
    │   ├── vector.aria
    │   ├── hashmap.aria
    │   └── list.aria
    ├── lib/
    │   └── libstd_collections.a
    └── package.json

  ↓

┌──────────────────────────────────────────────────────────────┐
│ STEP 6: Update System Configuration                          │
└──────────────────────────────────────────────────────────────┘
Update: /usr/local/aria/package_registry.json
{
  "installed": [
    {
      "name": "std.mem",
      "version": "1.0.0",
      "path": "/usr/local/aria/packages/std.mem-1.0.0"
    },
    {
      "name": "std.collections",
      "version": "1.0.0",
      "path": "/usr/local/aria/packages/std.collections-1.0.0"
    }
  ]
}

Create symlinks:
  /usr/local/aria/packages/std.mem → std.mem-1.0.0
  /usr/local/aria/packages/std.collections → std.collections-1.0.0

  ↓

┌──────────────────────────────────────────────────────────────┐
│ INSTALLATION COMPLETE                                         │
└──────────────────────────────────────────────────────────────┘
Installed:
  - std.mem-1.0.0
  - std.collections-1.0.0

Total download: 3.7 MB
Time: 2.3 seconds
```

---

## 5. Development Workflow

### 5.1 Edit-Compile-Debug Cycle with VSCode

```
┌─────────────────────────────────────────────────────────────────┐
│ DEVELOPER WORKFLOW                                               │
└─────────────────────────────────────────────────────────────────┘

TIME: t0 - Developer opens VSCode
─────────────────────────────────────────────────────────────────
VSCode starts
  ↓
VSCode Extension: Detects .aria files
  ↓
VSCode Extension: Launches aria-lsp
  ↓
aria-lsp: Starts in background
  ↓
aria-lsp: Sends "initialize" response (capabilities)
  ↓
VSCode: Displays ready status

TIME: t1 - Developer opens file
─────────────────────────────────────────────────────────────────
User: Opens src/main.aria
  ↓
VSCode: Sends textDocument/didOpen notification
  ↓
aria-lsp: Receives notification
  ↓
aria-lsp: Parses file (Lexer → Parser → AST)
  ↓
aria-lsp: Runs semantic analysis
  ↓
aria-lsp: Detects error: "undefined variable 'x'"
  ↓
aria-lsp: Sends textDocument/publishDiagnostics
  ↓
VSCode: Displays red squiggly under error

TIME: t2 - Developer types
─────────────────────────────────────────────────────────────────
User: Types "int64:" before "x"
  ↓
VSCode: Sends textDocument/didChange (incremental)
  ↓
aria-lsp: Receives change
  ↓
aria-lsp: Incrementally reparses affected region
  ↓
aria-lsp: Semantic check: "x" now declared ✓
  ↓
aria-lsp: Sends updated diagnostics (no errors)
  ↓
VSCode: Removes red squiggly

User: Types "x." (dot)
  ↓
VSCode: Requests textDocument/completion
  ↓
aria-lsp: Queries type of "x" → int64
  ↓
aria-lsp: Returns completion items: (no methods for primitives)
  ↓
VSCode: Shows empty completion menu

TIME: t3 - Developer builds
─────────────────────────────────────────────────────────────────
User: Presses Ctrl+Shift+B (Build)
  ↓
VSCode: Runs task: "ariab build"
  ↓
AriaBuild: Compiles modified files
  ↓
AriaBuild: Outputs to terminal:
  "Compiling src/main.aria... Done"
  "Linking bin/my_app... Done"
  ↓
VSCode: Parses output, highlights any errors
  ↓
(No errors)
  ↓
VSCode: Shows "Build succeeded"

TIME: t4 - Developer debugs
─────────────────────────────────────────────────────────────────
User: Sets breakpoint on line 10
  ↓
VSCode: Marks breakpoint in gutter
  ↓
User: Presses F5 (Start Debugging)
  ↓
VSCode: Launches aria-dap
  ↓
aria-dap: Sends "initialize" response
  ↓
VSCode: Sends "setBreakpoints" request (line 10)
  ↓
aria-dap: Invokes LLDB, sets breakpoint
  ↓
VSCode: Sends "launch" request (executable: bin/my_app)
  ↓
aria-dap: LLDB runs executable
  ↓
aria-dap: Breakpoint hit, sends "stopped" event
  ↓
VSCode: Highlights line 10, pauses execution
  ↓
User: Hovers over variable "result"
  ↓
VSCode: Sends "variables" request (scope: local)
  ↓
aria-dap: Queries LLDB for "result" value
  ↓
aria-dap: Uses custom formatter for result<T> type
  ↓
aria-dap: Returns: "Ok(42)"
  ↓
VSCode: Shows tooltip: "result = Ok(42)"
  ↓
User: Presses F10 (Step Over)
  ↓
VSCode: Sends "next" request
  ↓
aria-dap: LLDB steps to next line
  ↓
aria-dap: Sends "stopped" event
  ↓
VSCode: Highlights next line
  ↓
User: Presses F5 (Continue)
  ↓
aria-dap: LLDB continues execution
  ↓
Program terminates
  ↓
aria-dap: Sends "terminated" event
  ↓
VSCode: Shows "Debugging stopped"
```

---

## 6. 6-Stream I/O Data Flow

### 6.1 Shell Spawns Process with 6 Streams

```
┌─────────────────────────────────────────────────────────────────┐
│ SHELL SPAWNING PROCESS WITH 6-STREAM TOPOLOGY                   │
└─────────────────────────────────────────────────────────────────┘

USER: $ aria_shell
aria_shell> ./data_processor < input.bin

  ↓

┌──────────────────────────────────────────────────────────────┐
│ SHELL: Setup Phase                                            │
└──────────────────────────────────────────────────────────────┘
aria_shell parses command: "./data_processor < input.bin"
  ↓
Create 6 pipes (12 file descriptors total):
  
  pipe2(stdin_pipe, O_CLOEXEC)     → [3, 4]
  pipe2(stdout_pipe, O_CLOEXEC)    → [5, 6]
  pipe2(stderr_pipe, O_CLOEXEC)    → [7, 8]
  pipe2(stddbg_pipe, O_CLOEXEC)    → [9, 10]
  pipe2(stddati_pipe, O_CLOEXEC)   → [11, 12]
  pipe2(stddato_pipe, O_CLOEXEC)   → [13, 14]

Pipe Layout:
  stdin:   [3 read]  ← shell writes to [4 write]
  stdout:  [5 read]  → shell reads from [6 write]
  stderr:  [7 read]  → shell reads from [8 write]
  stddbg:  [9 read]  → shell reads from [10 write]
  stddati: [11 read] ← shell writes to [12 write]
  stddato: [13 read] → shell reads from [14 write]

  ↓

┌──────────────────────────────────────────────────────────────┐
│ SHELL: Fork Process                                           │
└──────────────────────────────────────────────────────────────┘
pid = fork()
  ↓
  ├── Parent Process (shell)
  │     ↓
  │   Close child ends:
  │     close(3)  // stdin read
  │     close(6)  // stdout write
  │     close(8)  // stderr write
  │     close(10) // stddbg write
  │     close(11) // stddati read
  │     close(14) // stddato write
  │     ↓
  │   Retain parent ends:
  │     stdin_write = 4
  │     stdout_read = 5
  │     stderr_read = 7
  │     stddbg_read = 9
  │     stddati_write = 12
  │     stddato_read = 13
  │     ↓
  │   Start drain workers (threads):
  │     Thread 1: drain stdout (FD 5) → terminal
  │     Thread 2: drain stderr (FD 7) → terminal (red)
  │     Thread 3: drain stddbg (FD 9) → ring buffer
  │     Thread 4: drain stddato (FD 13) → wild buffer
  │     ↓
  │   Open input file:
  │     input_fd = open("input.bin", O_RDONLY)
  │     ↓
  │   Pump data to child's stdin via FD 4:
  │     while (read(input_fd, buf, SIZE) > 0) {
  │       write(4, buf, SIZE);  // Writes to child's stdin
  │     }
  │     close(4);  // Signal EOF to child
  │
  └── Child Process (data_processor)
        ↓
      Close parent ends:
        close(4)  // stdin write
        close(5)  // stdout read
        close(7)  // stderr read
        close(9)  // stddbg read
        close(12) // stddati write
        close(13) // stddato read
        ↓
      Remap child ends to FDs 0-5:
        dup2(3, 0)   // stdin  ← pipe read
        dup2(6, 1)   // stdout → pipe write
        dup2(8, 2)   // stderr → pipe write
        dup2(10, 3)  // stddbg → pipe write
        dup2(11, 4)  // stddati ← pipe read
        dup2(14, 5)  // stddato → pipe write
        ↓
      Close original descriptors (now duplicated):
        close(3, 6, 8, 10, 11, 14)
        ↓
      Execute program:
        execve("./data_processor", ...)

  ↓

┌──────────────────────────────────────────────────────────────┐
│ CHILD PROCESS: data_processor Execution                      │
└──────────────────────────────────────────────────────────────┘
data_processor starts:
  ↓
Aria Runtime initializes:
  io.stdin_fd = 0   (mapped to shell's FD 4 write)
  io.stdout_fd = 1  (mapped to shell's FD 6 write)
  io.stderr_fd = 2  (mapped to shell's FD 8 write)
  io.stddbg_fd = 3  (mapped to shell's FD 10 write)
  io.stddati_fd = 4 (mapped to shell's FD 11 read)
  io.stddato_fd = 5 (mapped to shell's FD 14 write)
  ↓
main() executes:
  ↓
  Read binary data from stdin:
    AriaResult* r = aria_stdin_read(buffer, 1024);
      ↓
      System call: read(0, buffer, 1024)
        ↓
        Kernel: Read from pipe connected to shell's FD 4
        ↓
        Returns 1024 bytes (from input.bin)
      ↓
      Returns result with data
    ↓
  Process data:
    uint8_t* processed = process(buffer);
    ↓
  Write processed data to stddato:
    aria_stddato_write(processed, 1024);
      ↓
      System call: write(5, processed, 1024)
        ↓
        Kernel: Write to pipe connected to shell's FD 14
        ↓
        Returns 1024 (bytes written)
    ↓
  Write debug log to stddbg:
    aria_stddbg_write("Processed 1024 bytes\n");
      ↓
      System call: write(3, "Processed 1024 bytes\n", 21)
        ↓
        Kernel: Write to pipe connected to shell's FD 10
        ↓
        Returns 21
    ↓
  Write status to stdout:
    aria_stdout_write("OK\n");
      ↓
      System call: write(1, "OK\n", 3)
        ↓
        Kernel: Write to pipe connected to shell's FD 6
        ↓
        Returns 3
    ↓
  Loop until stdin EOF
    ↓
  Return 0

  ↓

┌──────────────────────────────────────────────────────────────┐
│ SHELL: Drain Workers Receive Data                            │
└──────────────────────────────────────────────────────────────┘
Thread 1 (stdout drain):
  read(5, buffer, 4096) → "OK\n" (3 bytes)
    ↓
  terminal.write("OK\n") → display to user

Thread 2 (stderr drain):
  read(7, buffer, 4096) → 0 (EOF, no errors)

Thread 3 (stddbg drain):
  read(9, buffer, 4096) → "Processed 1024 bytes\n" (21 bytes)
    ↓
  ring_buffer.push("Processed 1024 bytes\n")
    ↓
  (Stored for later inspection, not displayed)

Thread 4 (stddato drain):
  read(13, buffer, 4096) → processed binary data (1024 bytes)
    ↓
  wild_buffer.append(data, 1024)
    ↓
  (Accumulated in memory for final output)

  ↓

┌──────────────────────────────────────────────────────────────┐
│ PROCESS TERMINATION                                           │
└──────────────────────────────────────────────────────────────┘
Child process exits (return 0)
  ↓
Kernel: Close all FDs (0-5)
  ↓
Kernel: Signal EOF on pipes (shell's FDs 5, 7, 9, 13)
  ↓
Shell drain threads:
  read() returns 0 (EOF) → threads terminate
  ↓
Shell: waitpid(child_pid) → exit code 0
  ↓
Shell: Close remaining FDs (4, 12)
  ↓
Shell: Display final output:
  ┌────────────────────────────────────────┐
  │ Terminal Output (stdout):              │
  │ OK                                     │
  └────────────────────────────────────────┘
  
  ┌────────────────────────────────────────┐
  │ Debug Log (stddbg, optional view):    │
  │ Processed 1024 bytes                   │
  └────────────────────────────────────────┘
  
  ┌────────────────────────────────────────┐
  │ Binary Output (stddato):               │
  │ [1024 bytes of processed data]         │
  │ (Saved to file or piped to next cmd)  │
  └────────────────────────────────────────┘
```

---

## 7. Error Propagation Flow

### 7.1 Result<T> Error Handling

```
┌─────────────────────────────────────────────────────────────────┐
│ ERROR PROPAGATION: File Read Failure                            │
└─────────────────────────────────────────────────────────────────┘

ARIA CODE:
───────────────────────────────────────────────────────────────────
func:main = int64() {
    result<string>:content = io.read_file("missing.txt");
    if (content.is_err()) {
        io.stderr.write("Error: " + content.err() + "\n");
        pass(1);
    }
    io.stdout.write(content.ok());
    pass(0);
}
───────────────────────────────────────────────────────────────────

EXECUTION TRACE:
───────────────────────────────────────────────────────────────────
Call: io.read_file("missing.txt")
  ↓
Runtime Function: aria_read_file(path="missing.txt")
  ↓
  System Call: open("missing.txt", O_RDONLY)
    ↓
    Kernel: File not found
    ↓
    Returns: -1 (error)
    ↓
    errno: ENOENT (No such file or directory)
  ↓
  Check return value:
    if (fd < 0) {
      // Error occurred
      char* err_msg = strerror(errno);  // "No such file or directory"
      return aria_result_err(err_msg);
    }
  ↓
  Create error result:
    AriaResult* result = malloc(sizeof(AriaResult));
    result->err = strdup("No such file or directory");
    result->val = NULL;
    result->val_size = 0;
  ↓
  Return error result ← Propagates to caller
  ↓
[Back to main()]
  ↓
Check result:
  content.is_err() → true (content->err != NULL)
  ↓
Enter error handler:
  io.stderr.write("Error: " + content.err() + "\n");
    ↓
    Constructs string: "Error: No such file or directory\n"
    ↓
    aria_stderr_write(str, len)
      ↓
      write(2, "Error: No such file or directory\n", 34)
        ↓
        Kernel: Write to stderr (FD 2, terminal)
        ↓
        TERMINAL DISPLAYS: "Error: No such file or directory"
  ↓
  pass(1) → return 1
  ↓
[Exit main() with code 1]
  ↓
Shell: waitpid() returns exit code 1
  ↓
Shell: Displays exit status to user
```

---

### 7.2 TBB Sticky Error Propagation

```
┌─────────────────────────────────────────────────────────────────┐
│ TBB STICKY ERROR: Overflow Propagation                          │
└─────────────────────────────────────────────────────────────────┘

ARIA CODE:
───────────────────────────────────────────────────────────────────
func:main = int64() {
    tbb8:size = get_buffer_size();  // Returns 120
    tbb8:extra = 10;
    tbb8:total = size + extra;      // Overflow! 120 + 10 = 130 > 127
    
    if (total < 0) {
        io.stderr.write("Buffer overflow detected!\n");
        pass(1);
    }
    
    wild byte*:buffer = alloc(total);
    pass(0);
}
───────────────────────────────────────────────────────────────────

EXECUTION TRACE:
───────────────────────────────────────────────────────────────────
Call: get_buffer_size()
  ↓
  Returns: tbb8(120)
  ↓
size = 120
extra = 10

Compute: total = size + extra
  ↓
  Runtime Function: tbb8_add(120, 10)
    ↓
    Compute: 120 + 10 = 130
    ↓
    Check range: 130 > 127 (max for tbb8)
    ↓
    Overflow detected!
    ↓
    Return ERR sentinel: -128
  ↓
total = -128 (ERR)

Check: if (total < 0)
  ↓
  Evaluate: -128 < 0 → true
  ↓
Enter error handler:
  io.stderr.write("Buffer overflow detected!\n");
    ↓
    TERMINAL DISPLAYS: "Buffer overflow detected!"
  ↓
  pass(1) → return 1

──────────────────────────────────────────────────────────────────
ALTERNATIVE SCENARIO: If check was missing:

total = -128 (ERR)
  ↓
Call: alloc(total)
  ↓
Runtime Function: aria_alloc(-128)
  ↓
  Interpret as size_t: (size_t)(-128) = 18446744073709551488
    ↓
    HUGE allocation request!
    ↓
    malloc() fails → returns NULL
  ↓
  Runtime detects ERR sentinel:
    if (size == (size_t)(-128)) {
      // TBB ERR propagation
      return NULL;  // Fail allocation safely
    }

[Would crash if not caught, but TBB ERR detection prevents UB]
──────────────────────────────────────────────────────────────────
```

---

**Document Status**: v1.0 - Comprehensive data flow analysis complete  
**Next**: Can create component-specific docs or integration guides
