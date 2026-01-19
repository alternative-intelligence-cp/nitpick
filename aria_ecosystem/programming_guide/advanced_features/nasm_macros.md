# NASM-Style Macros

**Category**: Advanced Features → Macros  
**Purpose**: Low-level assembly-style macros for system programming

---

## Overview

NASM-style macros provide **low-level code generation** similar to assembly preprocessor directives.

---

## Basic Macro

```aria
%macro SAVE_REGS 0
    push rax
    push rbx
    push rcx
    push rdx
%endmacro

%macro RESTORE_REGS 0
    pop rdx
    pop rcx
    pop rbx
    pop rax
%endmacro

fn low_level_operation() {
    asm {
        SAVE_REGS
        
        // Do work
        mov rax, 42
        
        RESTORE_REGS
    }
}
```

---

## Parameterized Macros

```aria
%macro LOAD_IMM 2   ; 2 parameters: register, value
    mov %1, %2
%endmacro

fn main() {
    asm {
        LOAD_IMM rax, 42
        LOAD_IMM rbx, 100
        
        // Expands to:
        // mov rax, 42
        // mov rbx, 100
    }
}
```

---

## Multi-Line Macros

```aria
%macro FUNCTION_PROLOGUE 1  ; 1 parameter: stack space
    push rbp
    mov rbp, rsp
    sub rsp, %1
%endmacro

%macro FUNCTION_EPILOGUE 0
    mov rsp, rbp
    pop rbp
    ret
%endmacro

fn asm_function() {
    asm {
        FUNCTION_PROLOGUE 32
        
        // Function body
        mov rax, 0
        
        FUNCTION_EPILOGUE
    }
}
```

---

## Conditional Assembly

```aria
%ifdef DEBUG
    %macro LOG 1
        push rax
        mov rax, %1
        call debug_print
        pop rax
    %endmacro
%else
    %macro LOG 1
        ; No-op in release
    %endmacro
%endif

fn main() {
    asm {
        LOG "Starting operation"
        // Code here
    }
}
```

---

## Common Patterns

### Register Save/Restore

```aria
%macro PUSH_ALL 0
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15
%endmacro

%macro POP_ALL 0
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
%endmacro
```

---

### System Call Wrapper

```aria
%macro SYSCALL 3  ; syscall number, arg1, arg2
    mov rax, %1
    mov rdi, %2
    mov rsi, %3
    syscall
%endmacro

fn write_to_stdout(message: *u8, length: i64) {
    asm {
        SYSCALL 1, message, length  ; sys_write(stdout, message, length)
    }
}
```

---

### Memory Operations

```aria
%macro ZERO_MEMORY 2  ; address, size
    mov rdi, %1
    mov rcx, %2
    xor rax, rax
    rep stosb
%endmacro

%macro COPY_MEMORY 3  ; dest, src, size
    mov rdi, %1
    mov rsi, %2
    mov rcx, %3
    rep movsb
%endmacro

fn initialize_buffer(buffer: *u8, size: i64) {
    asm {
        ZERO_MEMORY buffer, size
    }
}
```

---

### Stack Frame Macros

```aria
%macro ENTER_FRAME 1  ; local variables size
    push rbp
    mov rbp, rsp
    sub rsp, %1
    
    ; Align stack to 16 bytes
    and rsp, -16
%endmacro

%macro LEAVE_FRAME 0
    mov rsp, rbp
    pop rbp
%endmacro

%macro LOCAL 2  ; name, offset
    %define %1 [rbp - %2]
%endmacro

fn complex_function() {
    asm {
        ENTER_FRAME 64
        
        LOCAL temp, 8
        LOCAL counter, 16
        
        mov temp, rax
        mov counter, 0
        
        LEAVE_FRAME
        ret
    }
}
```

---

### Spinlock Implementation

```aria
%macro ACQUIRE_LOCK 1  ; lock address
.retry_%1:
    mov rax, 1
    xchg [%1], rax
    test rax, rax
    jnz .retry_%1
%endmacro

%macro RELEASE_LOCK 1  ; lock address
    mov qword [%1], 0
%endmacro

lock: u64 = 0;

fn critical_section() {
    asm {
        ACQUIRE_LOCK lock
        
        // Critical section code
        
        RELEASE_LOCK lock
    }
}
```

---

### Atomic Operations

```aria
%macro ATOMIC_ADD 2  ; address, value
    lock add [%1], %2
%endmacro

%macro ATOMIC_INC 1  ; address
    lock inc qword [%1]
%endmacro

%macro ATOMIC_DEC 1  ; address
    lock dec qword [%1]
%endmacro

%macro ATOMIC_CAS 3  ; address, expected, desired
    mov rax, %2
    lock cmpxchg [%1], %3
%endmacro

counter: u64 = 0;

fn increment_counter() {
    asm {
        ATOMIC_INC counter
    }
}
```

---

### Repeat Macros

```aria
%macro UNROLL_4 1  ; code to unroll
    %rep 4
        %1
    %endrep
%endmacro

fn unrolled_loop() {
    asm {
        UNROLL_4 {
            add rax, 1
            mov [rdi], rax
            add rdi, 8
        }
        
        // Expands to 4 copies of the code
    }
}
```

---

## Platform-Specific Macros

```aria
%ifdef X86_64
    %macro CALL_CONVENTION 4  ; fn, arg1, arg2, arg3
        mov rdi, %2
        mov rsi, %3
        mov rdx, %4
        call %1
    %endmacro
%elifdef ARM64
    %macro CALL_CONVENTION 4
        mov x0, %2
        mov x1, %3
        mov x2, %4
        bl %1
    %endmacro
%endif
```

---

## Macro Libraries

### String Macros

```aria
%macro STRLEN 2  ; dest, src
    xor rax, rax
    mov rcx, -1
    mov rdi, %2
    repne scasb
    not rcx
    dec rcx
    mov %1, rcx
%endmacro

%macro STRCMP 3  ; result, str1, str2
    mov rsi, %2
    mov rdi, %3
    call strcmp_impl
    mov %1, rax
%endmacro
```

---

### Alignment Macros

```aria
%macro ALIGN_STACK 0
    push rbp
    mov rbp, rsp
    and rsp, -16  ; 16-byte align
%endmacro

%macro ALIGN_DATA 1  ; alignment
    times (%1 - ($ - $$) % %1) % %1 nop
%endmacro
```

---

## Best Practices

### ✅ DO: Use for Low-Level Optimization

```aria
%macro FAST_MEMCPY 3  ; dest, src, size
    ; Optimized memory copy using AVX
    mov rcx, %3
    shr rcx, 5  ; Divide by 32
    mov rsi, %2
    mov rdi, %1
.loop:
    vmovdqu ymm0, [rsi]
    vmovdqu [rdi], ymm0
    add rsi, 32
    add rdi, 32
    loop .loop
%endmacro
```

### ✅ DO: Use for Platform Abstraction

```aria
%ifdef WINDOWS
    %macro OS_CALL 1
        call %1
        ; Windows calling convention
    %endmacro
%else
    %macro OS_CALL 1
        syscall
        ; Unix syscall
    %endmacro
%endif
```

### ⚠️ WARNING: Document Register Usage

```aria
%macro COMPLEX_OP 2  ; Clobbers: rax, rcx, rdx
    ; Clearly document which registers are modified
    mov rax, %1
    mov rcx, %2
    ; ... operation
%endmacro
```

### ❌ DON'T: Make Overly Complex Macros

```aria
// ❌ Too complex - hard to debug
%macro MEGA_MACRO 10
    ; 200 lines of complex assembly
%endmacro

// ✅ Better - break into smaller macros
%macro STEP1 2
    ; Simple, focused operation
%endmacro

%macro STEP2 2
    ; Another simple operation
%endmacro
```

---

## Integration with Aria

```aria
fn low_level_function() {
    x: i64 = 42;
    Result: i64;
    
    asm {
        %macro SQUARE 2  ; result, input
            mov rax, %2
            imul rax, rax
            mov %1, rax
        %endmacro
        
        SQUARE result, x
    }
    
    stdout << "Result: $result";  // Result: 1764
}
```

---

## Related

- [macros](macros.md) - High-level macros
- [extern](../modules/extern.md) - External functions
- [ffi](../modules/ffi.md) - Foreign function interface

---

**Remember**: NASM-style macros provide **low-level control** - use for performance-critical code and platform abstraction!
