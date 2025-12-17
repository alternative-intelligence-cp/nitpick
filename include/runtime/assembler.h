/**
 * Aria Runtime Assembler (ARA)
 * 
 * Lightweight JIT compiler for x86-64 machine code generation.
 * Integrates with WildX executable memory for secure runtime compilation.
 * 
 * Key Features:
 * - Fluent interface API for instruction emission
 * - Linear scan register allocation (O(N) complexity)
 * - Label backpatching for forward jumps
 * - System V AMD64 ABI compliance
 * - W⊕X security enforcement via WildXGuard integration
 * 
 * Reference: research_023_runtime_assembler.txt
 */

#ifndef ARIA_RUNTIME_ASSEMBLER_H
#define ARIA_RUNTIME_ASSEMBLER_H

#include "runtime/allocators.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================
// Register Definitions (x86-64)
// =============================================================================

typedef enum {
    // 64-bit General Purpose Registers
    REG_RAX = 0,
    REG_RCX = 1,
    REG_RDX = 2,
    REG_RBX = 3,
    REG_RSP = 4,
    REG_RBP = 5,
    REG_RSI = 6,
    REG_RDI = 7,
    REG_R8  = 8,
    REG_R9  = 9,
    REG_R10 = 10,
    REG_R11 = 11,
    REG_R12 = 12,
    REG_R13 = 13,
    REG_R14 = 14,
    REG_R15 = 15,
    
    // 32-bit variants (lower 32 bits)
    REG_EAX = 32 + 0,
    REG_ECX = 32 + 1,
    REG_EDX = 32 + 2,
    REG_EBX = 32 + 3,
    REG_ESP = 32 + 4,
    REG_EBP = 32 + 5,
    REG_ESI = 32 + 6,
    REG_EDI = 32 + 7,
} AsmRegister;

// =============================================================================
// Code Buffer Management
// =============================================================================

typedef struct {
    uint8_t* data;          // Raw instruction bytes
    size_t size;            // Current size (bytes)
    size_t capacity;        // Allocated capacity (bytes)
} CodeBuffer;

/**
 * Create new code buffer with initial capacity
 */
CodeBuffer* aria_asm_buffer_create(size_t initial_capacity);

/**
 * Destroy code buffer
 */
void aria_asm_buffer_destroy(CodeBuffer* buf);

/**
 * Emit single byte into buffer
 */
void aria_asm_emit_byte(CodeBuffer* buf, uint8_t byte);

/**
 * Emit 32-bit immediate value (little-endian)
 */
void aria_asm_emit_i32(CodeBuffer* buf, int32_t value);

/**
 * Emit 64-bit immediate value (little-endian)
 */
void aria_asm_emit_i64(CodeBuffer* buf, int64_t value);

/**
 * Get current code offset (for label binding)
 */
size_t aria_asm_buffer_offset(const CodeBuffer* buf);

// =============================================================================
// Label Management
// =============================================================================

#define MAX_PATCH_SITES 64

typedef struct {
    int32_t position;                    // Bound offset, or -1 if unbound
    uint32_t patch_sites[MAX_PATCH_SITES]; // Forward reference sites
    uint32_t num_patches;                // Number of unresolved patches
} AsmLabel;

/**
 * Create unbound label
 */
AsmLabel aria_asm_label_create();

/**
 * Check if label is bound
 */
bool aria_asm_label_is_bound(const AsmLabel* label);

// =============================================================================
// Assembler Core
// =============================================================================

#define MAX_LABELS 128

typedef struct {
    CodeBuffer* buffer;              // Instruction buffer
    AsmLabel labels[MAX_LABELS];     // Label table
    uint32_t label_count;            // Active labels
    bool error;                      // Error flag
    char error_msg[256];             // Error description
} Assembler;

/**
 * Create new assembler instance
 */
Assembler* aria_asm_create();

/**
 * Destroy assembler and release resources
 */
void aria_asm_destroy(Assembler* asm_ctx);

/**
 * Check for assembly errors
 */
bool aria_asm_has_error(const Assembler* asm_ctx);

/**
 * Get error message
 */
const char* aria_asm_get_error(const Assembler* asm_ctx);

// =============================================================================
// Label Operations
// =============================================================================

/**
 * Allocate new label
 * 
 * @return Label index, or -1 on error
 */
int aria_asm_new_label(Assembler* asm_ctx);

/**
 * Bind label to current position
 * 
 * Resolves all forward references by backpatching jump offsets.
 * 
 * @param label_id Label index from aria_asm_new_label
 */
void aria_asm_bind_label(Assembler* asm_ctx, int label_id);

// =============================================================================
// x86-64 Instruction Emission
// =============================================================================

/**
 * MOV reg64, imm64 - Load 64-bit immediate into register
 * 
 * Encoding: REX.W + B8+rd id (MOVABS)
 * 
 * @param dst Destination register (64-bit)
 * @param value Immediate value
 */
void aria_asm_mov_r64_imm64(Assembler* asm_ctx, AsmRegister dst, int64_t value);

/**
 * MOV reg64, reg64 - Move register to register
 * 
 * Encoding: REX.W + 89 /r
 * 
 * @param dst Destination register
 * @param src Source register
 */
void aria_asm_mov_r64_r64(Assembler* asm_ctx, AsmRegister dst, AsmRegister src);

/**
 * ADD reg64, reg64 - Add two registers
 * 
 * Encoding: REX.W + 01 /r
 * 
 * @param dst Destination register (also first operand)
 * @param src Source register (second operand)
 * Result: dst = dst + src
 */
void aria_asm_add_r64_r64(Assembler* asm_ctx, AsmRegister dst, AsmRegister src);

/**
 * SUB reg64, reg64 - Subtract registers
 * 
 * Encoding: REX.W + 29 /r
 * 
 * @param dst Destination register (also first operand)
 * @param src Source register (second operand)
 * Result: dst = dst - src
 */
void aria_asm_sub_r64_r64(Assembler* asm_ctx, AsmRegister dst, AsmRegister src);

/**
 * IMUL reg64, reg64 - Signed multiply
 * 
 * Encoding: REX.W + 0F AF /r
 * 
 * @param dst Destination register (also first operand)
 * @param src Source register (second operand)
 * Result: dst = dst * src (lower 64 bits)
 */
void aria_asm_imul_r64_r64(Assembler* asm_ctx, AsmRegister dst, AsmRegister src);

/**
 * RET - Return from function
 * 
 * Encoding: C3
 */
void aria_asm_ret(Assembler* asm_ctx);

/**
 * PUSH reg64 - Push register onto stack
 * 
 * Encoding: 50+rd (or REX.B + 50+rd for R8-R15)
 */
void aria_asm_push_r64(Assembler* asm_ctx, AsmRegister reg);

/**
 * POP reg64 - Pop register from stack
 * 
 * Encoding: 58+rd (or REX.B + 58+rd for R8-R15)
 */
void aria_asm_pop_r64(Assembler* asm_ctx, AsmRegister reg);

/**
 * JMP label - Unconditional jump to label
 * 
 * Encoding: E9 cd (rel32)
 * 
 * @param label_id Target label from aria_asm_new_label
 */
void aria_asm_jmp(Assembler* asm_ctx, int label_id);

/**
 * JE label - Jump if equal (ZF=1)
 * 
 * Encoding: 0F 84 cd (rel32)
 * 
 * @param label_id Target label
 */
void aria_asm_je(Assembler* asm_ctx, int label_id);

/**
 * JNE label - Jump if not equal (ZF=0)
 * 
 * Encoding: 0F 85 cd (rel32)
 * 
 * @param label_id Target label
 */
void aria_asm_jne(Assembler* asm_ctx, int label_id);

/**
 * CMP reg64, reg64 - Compare two registers
 * 
 * Encoding: REX.W + 39 /r
 * 
 * Sets flags based on (left - right).
 * Used before conditional jumps.
 */
void aria_asm_cmp_r64_r64(Assembler* asm_ctx, AsmRegister left, AsmRegister right);

// =============================================================================
// High-Level Code Generation
// =============================================================================

/**
 * Generate function prologue (System V AMD64 ABI)
 * 
 * Emits:
 *   PUSH RBP
 *   MOV RBP, RSP
 *   SUB RSP, stack_size (if stack_size > 0)
 * 
 * @param stack_size Local variable space (bytes)
 */
void aria_asm_prologue(Assembler* asm_ctx, size_t stack_size);

/**
 * Generate function epilogue (System V AMD64 ABI)
 * 
 * Emits:
 *   MOV RSP, RBP
 *   POP RBP
 *   RET
 */
void aria_asm_epilogue(Assembler* asm_ctx);

// =============================================================================
// Finalization and Execution
// =============================================================================

/**
 * Finalize assembly and create executable WildX memory
 * 
 * Process:
 * 1. Verify all labels are resolved (no dangling forward refs)
 * 2. Allocate WildX memory (executable)
 * 3. Copy code buffer to WildX
 * 4. Seal memory (RW → RX transition)
 * 
 * @param asm_ctx Assembler instance
 * @return WildXGuard with executable code, or {NULL, 0, UNINITIALIZED} on error
 */
WildXGuard aria_asm_finalize(Assembler* asm_ctx);

/**
 * Execute JIT-compiled function with no arguments
 * 
 * Assumes function signature: int64_t func(void)
 * 
 * @param guard Sealed WildXGuard from aria_asm_finalize
 * @return Function return value (from RAX)
 */
int64_t aria_asm_execute(WildXGuard* guard);

/**
 * Execute JIT function with one int64 argument
 * 
 * Assumes function signature: int64_t func(int64_t)
 * Follows System V AMD64 ABI (arg in RDI)
 * 
 * @param guard Sealed WildXGuard
 * @param arg1 First argument (passed in RDI)
 * @return Function return value (from RAX)
 */
int64_t aria_asm_execute_i64(WildXGuard* guard, int64_t arg1);

/**
 * Execute JIT function with two int64 arguments
 * 
 * Assumes function signature: int64_t func(int64_t, int64_t)
 * Follows System V AMD64 ABI (args in RDI, RSI)
 * 
 * @param guard Sealed WildXGuard
 * @param arg1 First argument (RDI)
 * @param arg2 Second argument (RSI)
 * @return Function return value (RAX)
 */
int64_t aria_asm_execute_i64_i64(WildXGuard* guard, int64_t arg1, int64_t arg2);

#ifdef __cplusplus
}
#endif

#endif // ARIA_RUNTIME_ASSEMBLER_H
