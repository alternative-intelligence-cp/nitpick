# Aria Runtime Assembler (JIT) Guide

## Overview

The Aria Runtime Assembler provides safe, efficient Just-In-Time (JIT) compilation capabilities. It allows Aria programs to generate and execute x86-64 machine code at runtime, enabling dynamic optimizations, DSL implementations, and high-performance code generation.

**Key Features:**
- **W⊕X Security**: Write-XOR-Execute enforcement prevents code injection attacks
- **Type-Safe API**: Compile-time protection against invalid instruction combinations  
- **Label Management**: Forward and backward jumps with automatic backpatching
- **System V ABI**: Compatible with C calling conventions
- **Cross-Platform**: Works on Linux, macOS, and Windows

## Quick Start

### Example 1: Return a Constant

Generate a function that returns `42`:

```aria
import jit;

func:main = int64() {
    // Create assembler
    Assembler:asm = asm_new();
    
    // Generate: int64_t func() { return 42; }
    asm_mov(asm, REG_RAX, 42);  // MOV RAX, 42
    asm_return(asm);            // RET
    
    // Finalize to executable memory
    wildx void@:code = asm_finalize(asm);
    
    // Execute
    int64:result = asm_call(code);
    
    // Cleanup
    asm_free(code);
    asm_destroy(asm);
    
    pass(result);  // Returns 42
};
```

### Example 2: Add Two Numbers

Generate a function that adds two int64 arguments:

```aria
import jit;

func:generate_add_function = wildx void@() {
    Assembler:asm = asm_new();
    
    // Generate: int64_t add(int64_t a, int64_t b) { return a + b; }
    // System V ABI: arg1 in RDI, arg2 in RSI, return in RAX
    
    asm_mov_reg(asm, REG_RAX, REG_RDI);  // MOV RAX, RDI (copy first arg)
    asm_add(asm, REG_RAX, REG_RSI);      // ADD RAX, RSI (add second arg)
    asm_return(asm);                      // RET
    
    wildx void@:code = asm_finalize(asm);
    asm_destroy(asm);
    
    pass(code);
};

func:main = int64() {
    wildx void@:add_func = generate_add_function();
    
    int64:result = asm_call_i64_i64(add_func, 10, 32);
    asm_free(add_func);
    
    pass(result);  // Returns 42
};
```

### Example 3: Conditional Logic with Labels

Generate a function with branching:

```aria
import jit;

func:generate_abs_function = wildx void@() {
    Assembler:asm = asm_new();
    
    // Generate: int64_t abs(int64_t x) { return x < 0 ? -x : x; }
    // arg in RDI, return in RAX
    
    int32:positive_label = asm_label(asm);
    
    // MOV RAX, RDI (copy arg to return register)
    asm_mov_reg(asm, REG_RAX, REG_RDI);
    
    // TEST RDI, RDI (check sign)
    aria_asm_test_reg_reg(asm.ctx, REG_RDI, REG_RDI);
    
    // JNS positive (jump if not sign - if >= 0)
    aria_asm_jns(asm.ctx, positive_label);
    
    // NEG RAX (negate if negative)
    aria_asm_neg_reg(asm.ctx, REG_RAX);
    
    // positive:
    asm_bind(asm, positive_label);
    
    // RET
    asm_return(asm);
    
    wildx void@:code = asm_finalize(asm);
    asm_destroy(asm);
    pass(code);
};

func:main = int64() {
    wildx void@:abs_func = generate_abs_function();
    
    int64:r1 = asm_call_i64(abs_func, -42);   // Returns 42
    int64:r2 = asm_call_i64(abs_func, 100);   // Returns 100
    
    asm_free(abs_func);
    
    pass(r1 + r2);  // Returns 142
};
```

## API Reference

### Assembler Lifecycle

#### `asm_new() -> Assembler`
Creates a new assembler instance with an empty code buffer.

#### `asm_destroy(Assembler)`
Destroys the assembler and frees associated resources. Does NOT free finalized code.

#### `asm_finalize(Assembler) -> wildx void@`
Finalizes the code buffer and transitions to executable memory. Returns a WildX guard.

**Security Note**: After finalization, the memory transitions from WRITABLE to EXECUTABLE. This enforces W⊕X security.

### Instruction Emission

#### `asm_mov(Assembler, dst: int32, value: int64)`
Emits: `MOV dst, value` - Load immediate value into register.

```aria
asm_mov(asm, REG_RAX, 42);  // MOV RAX, 42
```

#### `asm_mov_reg(Assembler, dst: int32, src: int32)`
Emits: `MOV dst, src` - Copy register to register.

```aria
asm_mov_reg(asm, REG_RAX, REG_RDI);  // MOV RAX, RDI
```

#### `asm_add(Assembler, dst: int32, src: int32)`
Emits: `ADD dst, src` - Add registers.

```aria
asm_add(asm, REG_RAX, REG_RBX);  // ADD RAX, RBX
```

#### `asm_sub(Assembler, dst: int32, src: int32)`
Emits: `SUB dst, src` - Subtract registers.

```aria
asm_sub(asm, REG_RAX, REG_RBX);  // SUB RAX, RBX
```

#### `asm_return(Assembler)`
Emits: `RET` - Return from function.

### Label Management

#### `asm_label(Assembler) -> int32`
Creates a new label for jumps. Returns label ID.

#### `asm_bind(Assembler, label_id: int32)`
Binds a label to the current code position.

#### `asm_jump(Assembler, label_id: int32)`
Emits: `JMP label` - Unconditional jump.

```aria
int32:skip = asm_label(asm);
asm_jump(asm, skip);
// ... code to skip ...
asm_bind(asm, skip);
```

### Code Execution

#### `asm_call(wildx void@) -> int64`
Executes JIT function with no arguments. Returns int64.

#### `asm_call_i64(wildx void@, arg: int64) -> int64`
Executes JIT function with one int64 argument.

#### `asm_call_i64_i64(wildx void@, a: int64, b: int64) -> int64`
Executes JIT function with two int64 arguments.

### Memory Management

#### `asm_free(wildx void@)`
Frees executable memory allocated by `asm_finalize()`.

**Important**: Always free JIT-compiled code when done to avoid memory leaks.

## Register Constants

x86-64 general-purpose registers (64-bit):

| Constant | Register | System V ABI Use |
|----------|----------|------------------|
| `REG_RAX` | RAX | Return value, accumulator |
| `REG_RCX` | RCX | 4th argument, scratch |
| `REG_RDX` | RDX | 3rd argument, scratch |
| `REG_RBX` | RBX | Callee-saved |
| `REG_RSP` | RSP | Stack pointer |
| `REG_RBP` | RBP | Frame pointer (callee-saved) |
| `REG_RSI` | RSI | 2nd argument |
| `REG_RDI` | RDI | 1st argument |
| `REG_R8` | R8 | 5th argument |
| `REG_R9` | R9 | 6th argument |
| `REG_R10` | R10 | Scratch |
| `REG_R11` | R11 | Scratch |
| `REG_R12` | R12 | Callee-saved |
| `REG_R13` | R13 | Callee-saved |
| `REG_R14` | R14 | Callee-saved |
| `REG_R15` | R15 | Callee-saved |

## System V AMD64 ABI (Linux/macOS)

Function arguments are passed in registers:
1. 1st arg: `RDI`
2. 2nd arg: `RSI`
3. 3rd arg: `RDX`
4. 4th arg: `RCX`
5. 5th arg: `R8`
6. 6th arg: `R9`
7. Additional args: stack

Return value: `RAX`

**Caller-saved registers** (caller must preserve): RAX, RCX, RDX, RSI, RDI, R8-R11  
**Callee-saved registers** (callee must preserve): RBX, RBP, R12-R15

## Security Model

### W⊕X Enforcement

The assembler enforces Write-XOR-Execute security through a state machine:

```
UNINITIALIZED → WRITABLE → EXECUTABLE → FREED
```

- **WRITABLE**: Memory is read/write, NOT executable. Code generation happens here.
- **EXECUTABLE**: Memory is read/execute, NOT writable. Code execution happens here.
- **FREED**: Memory is released back to OS.

**You cannot:**
- Execute code in WRITABLE state
- Modify code in EXECUTABLE state
- Transition from EXECUTABLE back to WRITABLE

This prevents entire classes of security vulnerabilities including:
- JIT spraying attacks
- Return-oriented programming (ROP) via code modification
- Code injection exploits

### Platform Support

| Platform | Memory API | Notes |
|----------|------------|-------|
| Linux | `mmap()` with `PROT_EXEC` | SELinux may require policy |
| macOS | `mmap()` with `PROT_EXEC` | Code signing not required for JIT |
| Windows | `VirtualAlloc()` with `PAGE_EXECUTE_READ` | Works on all versions |

## Advanced Examples

### Example 4: Loop Compilation

Generate code for: `sum = 0; for (i = 0; i < n; i++) sum += i;`

```aria
import jit;

func:generate_sum_n = wildx void@() {
    Assembler:asm = asm_new();
    
    // int64_t sum_n(int64_t n) {
    //     int64_t sum = 0;
    //     for (int64_t i = 0; i < n; i++) sum += i;
    //     return sum;
    // }
    
    // n in RDI
    int32:loop_start = asm_label(asm);
    int32:loop_end = asm_label(asm);
    
    // XOR RAX, RAX (sum = 0)
    aria_asm_xor_reg_reg(asm.ctx, REG_RAX, REG_RAX);
    
    // XOR RCX, RCX (i = 0)
    aria_asm_xor_reg_reg(asm.ctx, REG_RCX, REG_RCX);
    
    // loop_start:
    asm_bind(asm, loop_start);
    
    // CMP RCX, RDI (compare i < n)
    aria_asm_cmp_reg_reg(asm.ctx, REG_RCX, REG_RDI);
    
    // JGE loop_end (jump if i >= n)
    aria_asm_jge(asm.ctx, loop_end);
    
    // ADD RAX, RCX (sum += i)
    asm_add(asm, REG_RAX, REG_RCX);
    
    // INC RCX (i++)
    aria_asm_inc_reg(asm.ctx, REG_RCX);
    
    // JMP loop_start
    asm_jump(asm, loop_start);
    
    // loop_end:
    asm_bind(asm, loop_end);
    
    asm_return(asm);
    
    wildx void@:code = asm_finalize(asm);
    asm_destroy(asm);
    pass(code);
};

func:main = int64() {
    wildx void@:sum_func = generate_sum_n();
    
    int64:result = asm_call_i64(sum_func, 10);  // 0+1+2+...+9 = 45
    asm_free(sum_func);
    
    pass(result);
};
```

## Performance Considerations

### When to Use JIT

✅ **Good use cases:**
- Expression evaluators (math, regex)
- Domain-specific languages
- Hot path specialization
- Dynamic FFI bridges

❌ **Poor use cases:**
- Code that runs once
- Already-fast static code
- Safety-critical paths

### Optimization Tips

1. **Minimize State Transitions**: Creating/destroying assemblers has overhead. Reuse when possible.
2. **Batch Code Generation**: Generate multiple functions, then finalize once.
3. **Register Allocation**: Use caller-saved registers (RAX, RCX, RDX) for temporaries.
4. **Avoid Branching**: Straight-line code is faster than jumps when possible.

## Error Handling

Always check for errors after operations:

```aria
Assembler:asm = asm_new();

asm_mov(asm, REG_RAX, 42);
asm_return(asm);

if (asm_check_error(asm)) {
    string@:err_msg = asm_get_error_msg(asm);
    // Handle error
    pass(1);
}

wildx void@:code = asm_finalize(asm);
// ...
```

Common errors:
- **"Too many labels"**: Exceeded MAX_LABELS (default 256)
- **"Label not bound"**: Jumped to undefined label
- **"Invalid register"**: Used register ID outside valid range
- **"Buffer overflow"**: Code exceeded maximum size (rare)

## Testing

The assembler includes comprehensive tests. Run them:

```bash
cd build
./test_runner --filter=assembler
```

Tests cover:
- WildX state machine
- Instruction encoding
- Label backpatching
- System V ABI compliance
- Error conditions

## References

- Research: `docs/gemini/responses/research_023_runtime_assembler.txt`
- Implementation: `src/runtime/assembler/`
- Tests: `tests/test_assembler.cpp`
- Examples: `tests/wildx_*.aria`

## See Also

- [WildX Memory Management](wildx.md)
- [Foreign Function Interface](ffi.md)
- [Performance Guide](performance.md)
