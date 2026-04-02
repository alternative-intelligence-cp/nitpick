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
    
    // SSE2 XMM Registers (128-bit)
    REG_XMM0  = 64,
    REG_XMM1  = 65,
    REG_XMM2  = 66,
    REG_XMM3  = 67,
    REG_XMM4  = 68,
    REG_XMM5  = 69,
    REG_XMM6  = 70,
    REG_XMM7  = 71,
    REG_XMM8  = 72,
    REG_XMM9  = 73,
    REG_XMM10 = 74,
    REG_XMM11 = 75,
    REG_XMM12 = 76,
    REG_XMM13 = 77,
    REG_XMM14 = 78,
    REG_XMM15 = 79,
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

// =============================================================================
// Register Allocator IR (v0.7.3)
// =============================================================================

/** Register classes for virtual register allocation */
typedef enum {
    REG_CLASS_GPR = 0,   // General-purpose (RAX-R15, excluding RSP/RBP)
    REG_CLASS_XMM = 1,   // SSE/floating-point (XMM0-XMM15)
} RegClass;

/** Virtual register ID ranges:
 *  0-79: physical registers (AsmRegister enum values)
 *  256-511: virtual GPRs
 *  512-767: virtual XMMs
 */
#define VREG_GPR_BASE  256
#define VREG_XMM_BASE  512
#define VREG_MAX       768
#define MAX_VREGS      256   // per class
#define MAX_IR_INSNS   4096

/** Check if a register ID is virtual */
#define IS_VREG(r) ((r) >= VREG_GPR_BASE)
#define IS_VREG_GPR(r) ((r) >= VREG_GPR_BASE && (r) < VREG_XMM_BASE)
#define IS_VREG_XMM(r) ((r) >= VREG_XMM_BASE && (r) < VREG_MAX)

/** JIT IR opcodes — internal representation of all assembler instructions */
typedef enum {
    // Integer register-register
    JIT_OP_MOV_IMM64,     // dst = imm64
    JIT_OP_MOV_RR,        // dst = src
    JIT_OP_ADD_RR,        // dst += src
    JIT_OP_SUB_RR,        // dst -= src
    JIT_OP_IMUL_RR,       // dst *= src
    JIT_OP_XOR_RR,        // dst ^= src
    JIT_OP_AND_RR,        // dst &= src
    JIT_OP_OR_RR,         // dst |= src

    // Integer unary
    JIT_OP_NOT,           // dst = ~dst
    JIT_OP_NEG,           // dst = -dst

    // Integer immediate
    JIT_OP_ADD_IMM32,     // dst += imm32
    JIT_OP_SUB_IMM32,     // dst -= imm32
    JIT_OP_CMP_IMM32,     // cmp dst, imm32
    JIT_OP_SHL_IMM8,      // dst <<= imm8
    JIT_OP_SHR_IMM8,      // dst >>= imm8 (logical)
    JIT_OP_SAR_IMM8,      // dst >>= imm8 (arithmetic)

    // Compare
    JIT_OP_CMP_RR,        // cmp left, right

    // Memory
    JIT_OP_MOV_LOAD,      // dst = [base + offset]
    JIT_OP_MOV_STORE,     // [base + offset] = src
    JIT_OP_LEA,           // dst = base + offset
    JIT_OP_STORE_LOCAL,   // [RBP - slot] = src
    JIT_OP_LOAD_LOCAL,    // dst = [RBP - slot]

    // Stack
    JIT_OP_PUSH,          // push reg
    JIT_OP_POP,           // pop reg

    // Control flow
    JIT_OP_RET,
    JIT_OP_JMP,           // jmp label
    JIT_OP_JE,            // je label
    JIT_OP_JNE,           // jne label
    JIT_OP_JL,
    JIT_OP_JLE,
    JIT_OP_JG,
    JIT_OP_JGE,
    JIT_OP_JB,
    JIT_OP_JBE,
    JIT_OP_JA,
    JIT_OP_JAE,
    JIT_OP_LABEL,         // pseudo: bind label

    // Call
    JIT_OP_CALL_REG,      // call reg
    JIT_OP_CALL_LABEL,    // call label
    JIT_OP_CALL_ABS,      // call absolute address

    // Prologue/Epilogue
    JIT_OP_PROLOGUE,      // stack_size in imm64
    JIT_OP_EPILOGUE,

    // SSE2 scalar double
    JIT_OP_MOVSD_RR,      // xmm = xmm
    JIT_OP_MOVSD_LOAD,    // xmm = [base + offset]
    JIT_OP_MOVSD_STORE,   // [base + offset] = xmm
    JIT_OP_ADDSD,
    JIT_OP_SUBSD,
    JIT_OP_MULSD,
    JIT_OP_DIVSD,
    JIT_OP_UCOMISD,

    // SSE packed single
    JIT_OP_MOVAPS_RR,
    JIT_OP_MOVAPS_LOAD,
    JIT_OP_MOVAPS_STORE,
    JIT_OP_ADDPS,
    JIT_OP_MULPS,

    // Pseudo-ops
    JIT_OP_NOP,           // eliminated instruction (peephole)
} JitOpcode;

/** Single IR instruction */
typedef struct {
    JitOpcode opcode;
    int32_t dst;        // destination register (phys or vreg)
    int32_t src1;       // source 1 / base register
    int32_t src2;       // source 2 (for store ops)
    int32_t offset;     // memory offset or label_id or imm8
    int64_t imm64;      // 64-bit immediate or absolute address
} JitIR;

/** Live range for a virtual register */
typedef struct {
    int32_t vreg;       // virtual register ID
    int32_t start;      // first instruction index (def)
    int32_t end;        // last instruction index (use)
    int32_t phys;       // assigned physical register (-1 if spilled)
    int32_t spill_slot; // stack spill offset (-1 if not spilled)
    RegClass reg_class; // GPR or XMM
} LiveRange;

#define MAX_LIVE_RANGES 512
#define MAX_SPILL_SLOTS 64

/** Statistics from instruction selection during IR emission (v0.7.4) */
typedef struct {
    int32_t mov_imm64_narrowed;    // MOVABS → MOV r32, imm32 (10→5-6 bytes)
    int32_t cmp_zero_to_test;      // CMP r, 0 → TEST r, r
    int32_t add_to_inc;            // ADD r, 1 → INC r
    int32_t sub_to_dec;            // SUB r, 1 → DEC r
    int32_t imm32_to_imm8;        // ADD/SUB imm32 → imm8 form
    int32_t total_selected;
} InsnSelStats;

/** Register allocator state (attached to Assembler when vregs are used) */
typedef struct {
    JitIR* ir;                          // instruction queue
    int32_t ir_count;                   // number of queued instructions
    int32_t ir_capacity;                // allocated capacity

    int32_t next_gpr_vreg;             // next GPR vreg ID to allocate
    int32_t next_xmm_vreg;            // next XMM vreg ID to allocate
    bool has_vregs;                     // true if any vreg has been allocated

    LiveRange ranges[MAX_LIVE_RANGES]; // live ranges
    int32_t range_count;

    int32_t spill_count;               // number of spill slots used
    int32_t extra_stack_size;          // additional stack bytes for spills

    uint16_t reserved_gprs;            // bitmask: physical GPRs used directly in IR
    uint16_t reserved_xmms;            // bitmask: physical XMMs used directly in IR
    bool auto_frame;                   // true if auto-prologue/epilogue needed
    uint8_t csr_save_count;           // number of callee-saved regs to save
    int32_t csr_save_regs[5];         // callee-saved regs to push/pop (RBX,R12-R15)
    int32_t peephole_elim;            // instructions eliminated by peephole (v0.7.4)
    InsnSelStats insn_sel;            // instruction selection statistics (v0.7.4)
} RegAllocState;

typedef struct {
    CodeBuffer* buffer;              // Instruction buffer
    AsmLabel labels[MAX_LABELS];     // Label table
    uint32_t label_count;            // Active labels
    bool error;                      // Error flag
    char error_msg[256];             // Error description
    RegAllocState* regalloc;         // Register allocator (NULL if not used)
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
// Extended Integer Instructions (v0.7.2)
// =============================================================================

/**
 * ADD reg64, imm32 - Add sign-extended 32-bit immediate to register
 * Encoding: REX.W + 81 /0 id
 */
void aria_asm_add_r64_imm32(Assembler* asm_ctx, AsmRegister dst, int32_t value);

/**
 * SUB reg64, imm32 - Subtract sign-extended 32-bit immediate from register
 * Encoding: REX.W + 81 /5 id
 */
void aria_asm_sub_r64_imm32(Assembler* asm_ctx, AsmRegister dst, int32_t value);

/**
 * XOR reg64, reg64 - Bitwise exclusive OR
 * Encoding: REX.W + 31 /r
 */
void aria_asm_xor_r64_r64(Assembler* asm_ctx, AsmRegister dst, AsmRegister src);

/**
 * AND reg64, reg64 - Bitwise AND
 * Encoding: REX.W + 21 /r
 */
void aria_asm_and_r64_r64(Assembler* asm_ctx, AsmRegister dst, AsmRegister src);

/**
 * OR reg64, reg64 - Bitwise OR
 * Encoding: REX.W + 09 /r
 */
void aria_asm_or_r64_r64(Assembler* asm_ctx, AsmRegister dst, AsmRegister src);

/**
 * NOT reg64 - Bitwise complement
 * Encoding: REX.W + F7 /2
 */
void aria_asm_not_r64(Assembler* asm_ctx, AsmRegister reg);

/**
 * NEG reg64 - Two's complement negation
 * Encoding: REX.W + F7 /3
 */
void aria_asm_neg_r64(Assembler* asm_ctx, AsmRegister reg);

/**
 * SHL reg64, imm8 - Shift left by immediate
 * Encoding: REX.W + C1 /4 ib
 */
void aria_asm_shl_r64_imm8(Assembler* asm_ctx, AsmRegister reg, uint8_t count);

/**
 * SHR reg64, imm8 - Logical shift right by immediate
 * Encoding: REX.W + C1 /5 ib
 */
void aria_asm_shr_r64_imm8(Assembler* asm_ctx, AsmRegister reg, uint8_t count);

/**
 * SAR reg64, imm8 - Arithmetic shift right by immediate
 * Encoding: REX.W + C1 /7 ib
 */
void aria_asm_sar_r64_imm8(Assembler* asm_ctx, AsmRegister reg, uint8_t count);

/**
 * CMP reg64, imm32 - Compare register with sign-extended 32-bit immediate
 * Encoding: REX.W + 81 /7 id
 */
void aria_asm_cmp_r64_imm32(Assembler* asm_ctx, AsmRegister reg, int32_t value);

// Extended conditional jumps (v0.7.2)

/** JL label - Jump if less (SF≠OF). Encoding: 0F 8C cd */
void aria_asm_jl(Assembler* asm_ctx, int label_id);

/** JLE label - Jump if less or equal (ZF=1 or SF≠OF). Encoding: 0F 8E cd */
void aria_asm_jle(Assembler* asm_ctx, int label_id);

/** JG label - Jump if greater (ZF=0 and SF=OF). Encoding: 0F 8F cd */
void aria_asm_jg(Assembler* asm_ctx, int label_id);

/** JGE label - Jump if greater or equal (SF=OF). Encoding: 0F 8D cd */
void aria_asm_jge(Assembler* asm_ctx, int label_id);

/** JB label - Jump if below (unsigned, CF=1). Encoding: 0F 82 cd */
void aria_asm_jb(Assembler* asm_ctx, int label_id);

/** JBE label - Jump if below or equal (unsigned, CF=1 or ZF=1). Encoding: 0F 86 cd */
void aria_asm_jbe(Assembler* asm_ctx, int label_id);

/** JA label - Jump if above (unsigned, CF=0 and ZF=0). Encoding: 0F 87 cd */
void aria_asm_ja(Assembler* asm_ctx, int label_id);

/** JAE label - Jump if above or equal (unsigned, CF=0). Encoding: 0F 83 cd */
void aria_asm_jae(Assembler* asm_ctx, int label_id);

// =============================================================================
// Memory Operations (v0.7.2)
// =============================================================================

/**
 * MOV reg64, [base + offset] - Load 64-bit value from memory
 * Encoding: REX.W + 8B /r with ModR/M + optional SIB
 * offset=0: mod=00, nonzero: mod=01 (disp8) or mod=10 (disp32)
 */
void aria_asm_mov_r64_mem(Assembler* asm_ctx, AsmRegister dst, AsmRegister base, int32_t offset);

/**
 * MOV [base + offset], reg64 - Store 64-bit value to memory
 * Encoding: REX.W + 89 /r with ModR/M + optional SIB
 */
void aria_asm_mov_mem_r64(Assembler* asm_ctx, AsmRegister base, int32_t offset, AsmRegister src);

/**
 * LEA reg64, [base + offset] - Load effective address
 * Encoding: REX.W + 8D /r with ModR/M + optional SIB
 */
void aria_asm_lea_r64_mem(Assembler* asm_ctx, AsmRegister dst, AsmRegister base, int32_t offset);

// =============================================================================
// Stack Frame & Local Variables (v0.7.2)
// =============================================================================

/**
 * Store register to local variable slot on stack.
 * Emits MOV [RBP - slot_offset], reg
 * slot_offset should be positive (e.g., 8 for first local at [RBP-8])
 */
void aria_asm_store_local(Assembler* asm_ctx, uint32_t slot_offset, AsmRegister src);

/**
 * Load register from local variable slot on stack.
 * Emits MOV reg, [RBP - slot_offset]
 */
void aria_asm_load_local(Assembler* asm_ctx, AsmRegister dst, uint32_t slot_offset);

// =============================================================================
// CALL Instructions (v0.7.2)
// =============================================================================

/**
 * CALL indirect - Call function at address in register
 * Encoding: FF /2 with ModR/M mod=11
 * For R8-R15: REX.B prefix
 */
void aria_asm_call_r64(Assembler* asm_ctx, AsmRegister target);

/**
 * CALL label - Call function at label (rel32)
 * Encoding: E8 cd
 */
void aria_asm_call_label(Assembler* asm_ctx, int label_id);

/**
 * Load absolute 64-bit address into register then CALL it.
 * Convenience: MOV reg, addr; CALL reg
 * Uses R11 (caller-saved scratch) as target register.
 */
void aria_asm_call_abs(Assembler* asm_ctx, void* func_ptr);

// =============================================================================
// SSE2 Floating-Point Instructions (v0.7.2)
// =============================================================================

/**
 * MOVSD xmm, xmm - Move scalar double
 * Encoding: F2 0F 10 /r (reg-to-reg)
 */
void aria_asm_movsd_xmm_xmm(Assembler* asm_ctx, AsmRegister dst, AsmRegister src);

/**
 * MOVSD xmm, [base + offset] - Load scalar double from memory
 * Encoding: F2 0F 10 /r with ModR/M
 */
void aria_asm_movsd_xmm_mem(Assembler* asm_ctx, AsmRegister dst, AsmRegister base, int32_t offset);

/**
 * MOVSD [base + offset], xmm - Store scalar double to memory
 * Encoding: F2 0F 11 /r with ModR/M
 */
void aria_asm_movsd_mem_xmm(Assembler* asm_ctx, AsmRegister base, int32_t offset, AsmRegister src);

/**
 * ADDSD xmm, xmm - Add scalar double
 * Encoding: F2 0F 58 /r
 */
void aria_asm_addsd(Assembler* asm_ctx, AsmRegister dst, AsmRegister src);

/**
 * SUBSD xmm, xmm - Subtract scalar double
 * Encoding: F2 0F 5C /r
 */
void aria_asm_subsd(Assembler* asm_ctx, AsmRegister dst, AsmRegister src);

/**
 * MULSD xmm, xmm - Multiply scalar double
 * Encoding: F2 0F 59 /r
 */
void aria_asm_mulsd(Assembler* asm_ctx, AsmRegister dst, AsmRegister src);

/**
 * DIVSD xmm, xmm - Divide scalar double
 * Encoding: F2 0F 5E /r
 */
void aria_asm_divsd(Assembler* asm_ctx, AsmRegister dst, AsmRegister src);

/**
 * UCOMISD xmm, xmm - Unordered compare scalar double (sets EFLAGS)
 * Encoding: 66 0F 2E /r
 * Use JA/JAE/JB/JBE for unsigned comparisons after this.
 */
void aria_asm_ucomisd(Assembler* asm_ctx, AsmRegister left, AsmRegister right);

// =============================================================================
// SSE Packed Float Instructions (v0.7.2)
// =============================================================================

/**
 * MOVAPS xmm, xmm - Move aligned packed single (128-bit)
 * Encoding: 0F 28 /r
 */
void aria_asm_movaps_xmm_xmm(Assembler* asm_ctx, AsmRegister dst, AsmRegister src);

/**
 * MOVAPS xmm, [base + offset] - Load aligned packed single from memory
 * Encoding: 0F 28 /r with ModR/M
 * Memory address MUST be 16-byte aligned.
 */
void aria_asm_movaps_xmm_mem(Assembler* asm_ctx, AsmRegister dst, AsmRegister base, int32_t offset);

/**
 * MOVAPS [base + offset], xmm - Store aligned packed single to memory
 * Encoding: 0F 29 /r with ModR/M
 * Memory address MUST be 16-byte aligned.
 */
void aria_asm_movaps_mem_xmm(Assembler* asm_ctx, AsmRegister base, int32_t offset, AsmRegister src);

/**
 * ADDPS xmm, xmm - Add packed single (4x float32)
 * Encoding: 0F 58 /r
 */
void aria_asm_addps(Assembler* asm_ctx, AsmRegister dst, AsmRegister src);

/**
 * MULPS xmm, xmm - Multiply packed single (4x float32)
 * Encoding: 0F 59 /r
 */
void aria_asm_mulps(Assembler* asm_ctx, AsmRegister dst, AsmRegister src);

/**
 * Execution variant: JIT function returning double via XMM0
 * Signature: double func(void)
 */
double aria_asm_execute_f64(WildXGuard* guard);

/**
 * Execution variant: JIT function taking double arg (XMM0), returning double
 * Signature: double func(double)
 */
double aria_asm_execute_f64_f64(WildXGuard* guard, double arg1);
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

// =============================================================================
// Virtual Register API (v0.7.3 — Register Allocator)
// =============================================================================

/**
 * Allocate a new virtual GPR register.
 * Returns vreg ID >= VREG_GPR_BASE. Pass this to any instruction
 * that takes AsmRegister — the allocator resolves it at finalize time.
 *
 * Enables IR-buffered mode: subsequent instructions are queued
 * instead of directly emitted, and register allocation runs at finalize.
 *
 * @return Virtual register ID, or -1 on error
 */
int aria_asm_vreg_new_gpr(Assembler* asm_ctx);

/**
 * Allocate a new virtual XMM register (for SSE2/SIMD).
 * Returns vreg ID >= VREG_XMM_BASE.
 *
 * @return Virtual register ID, or -1 on error
 */
int aria_asm_vreg_new_xmm(Assembler* asm_ctx);

/**
 * Query how many virtual registers have been allocated.
 */
int aria_asm_vreg_count(const Assembler* asm_ctx);

/**
 * Query how many spill slots were needed (available after finalize).
 */
int aria_asm_spill_count(const Assembler* asm_ctx);

// =============================================================================
// v0.7.4: Instruction Selection — New Encoders
// =============================================================================

/**
 * TEST r64, r64 — Sets flags based on bitwise AND without storing result.
 * Used by instruction selection: CMP r, 0 → TEST r, r (shorter encoding).
 */
void aria_asm_test_r64_r64(Assembler* asm_ctx, AsmRegister r1, AsmRegister r2);

/**
 * INC r64 — Increment register by 1.
 * Used by instruction selection: ADD r, 1 → INC r (3 bytes vs 7 bytes).
 */
void aria_asm_inc_r64(Assembler* asm_ctx, AsmRegister reg);

/**
 * DEC r64 — Decrement register by 1.
 * Used by instruction selection: SUB r, 1 → DEC r (3 bytes vs 7 bytes).
 */
void aria_asm_dec_r64(Assembler* asm_ctx, AsmRegister reg);

// =============================================================================
// v0.7.4: Peephole Optimizer Statistics
// =============================================================================

/** Optimization statistics from the peephole pass */
typedef struct {
    int32_t mov_zero_to_xor;       // MOV r, 0 → XOR r, r
    int32_t identity_mov_elim;     // MOV r, r → NOP
    int32_t dead_store_elim;       // store immediately overwritten
    int32_t strength_reduced;      // e.g., IMUL by power-of-2 → SHL
    int32_t constant_folded;       // e.g., ADD_IMM 0 → NOP
    int32_t total_eliminated;
} PeepholeStats;

/**
 * Query peephole optimization statistics (available after finalize).
 * Returns zero-initialized stats if no vregs were used or not yet finalized.
 */
PeepholeStats aria_asm_peephole_stats(const Assembler* asm_ctx);

/**
 * Query instruction selection statistics (available after finalize).
 * Returns zero-initialized stats if no vregs were used or not yet finalized.
 */
InsnSelStats aria_asm_insn_sel_stats(const Assembler* asm_ctx);

// =============================================================================
// v0.7.4: Profiling — perf map integration
// =============================================================================

/**
 * Register a JIT code region with the Linux perf profiler.
 * Writes to /tmp/perf-<pid>.map for `perf record` integration.
 *
 * @param code_addr Start address of JIT code
 * @param code_size Size in bytes
 * @param name      Human-readable name for the function
 */
void aria_asm_perf_map_register(const void* code_addr, size_t code_size, const char* name);

// =============================================================================
// v0.7.4: Architecture abstraction
// =============================================================================

/** Supported target architectures */
typedef enum {
    ASM_ARCH_X86_64 = 0,
    ASM_ARCH_AARCH64 = 1,
} AsmArch;

/**
 * Query which architecture the assembler targets.
 * Currently always returns ASM_ARCH_X86_64.
 */
AsmArch aria_asm_get_arch(void);

/**
 * Query if a given architecture is supported for code generation.
 */
bool aria_asm_arch_supported(AsmArch arch);

#ifdef __cplusplus
}
#endif

#endif // ARIA_RUNTIME_ASSEMBLER_H
