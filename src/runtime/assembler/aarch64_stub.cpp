/**
 * AArch64 Backend Stub (v0.7.4)
 *
 * Placeholder for AArch64 JIT code generation. Currently provides stub
 * functions that report the architecture as unsupported. The actual
 * AArch64 encoder will be implemented in a future release.
 *
 * AArch64 instruction encoding uses fixed-width 32-bit instructions:
 *   - Data processing (immediate): ADD, SUB, MOV (wide/shifted), logical
 *   - Data processing (register): ADD, SUB, AND, ORR, EOR, shifts
 *   - Branches: B, B.cond, BL, BR, BLR, RET
 *   - Loads/Stores: LDR, STR with multiple addressing modes
 *   - SIMD/FP: FADD, FMUL, etc. via NEON/SVE
 *
 * Register model:
 *   X0-X30  (64-bit GPR), W0-W30 (32-bit views)
 *   X29 = FP, X30 = LR, SP = stack pointer (not a GPR)
 *   V0-V31  (128-bit SIMD/FP), D0-D31 (64-bit), S0-S31 (32-bit)
 */

#include "runtime/assembler.h"
#include <cstdio>

// =============================================================================
// AArch64 Register Definitions (for future use)
// =============================================================================

// AArch64 GPRs: X0-X30 (64-bit), W0-W30 (32-bit)
// X29 = Frame Pointer, X30 = Link Register, SP = Stack Pointer
enum AArch64GPR : int {
    AARCH64_X0  = 0,  AARCH64_X1  = 1,  AARCH64_X2  = 2,  AARCH64_X3  = 3,
    AARCH64_X4  = 4,  AARCH64_X5  = 5,  AARCH64_X6  = 6,  AARCH64_X7  = 7,
    AARCH64_X8  = 8,  AARCH64_X9  = 9,  AARCH64_X10 = 10, AARCH64_X11 = 11,
    AARCH64_X12 = 12, AARCH64_X13 = 13, AARCH64_X14 = 14, AARCH64_X15 = 15,
    AARCH64_X16 = 16, AARCH64_X17 = 17, AARCH64_X18 = 18, AARCH64_X19 = 19,
    AARCH64_X20 = 20, AARCH64_X21 = 21, AARCH64_X22 = 22, AARCH64_X23 = 23,
    AARCH64_X24 = 24, AARCH64_X25 = 25, AARCH64_X26 = 26, AARCH64_X27 = 27,
    AARCH64_X28 = 28,
    AARCH64_FP  = 29, // Frame pointer
    AARCH64_LR  = 30, // Link register
    // SP is encoded as register 31 in some contexts
};

// AArch64 SIMD/FP registers: V0-V31 (128-bit), D0-D31 (64-bit), S0-S31 (32-bit)
enum AArch64SIMD : int {
    AARCH64_V0  = 0,  AARCH64_V1  = 1,  AARCH64_V2  = 2,  AARCH64_V3  = 3,
    AARCH64_V4  = 4,  AARCH64_V5  = 5,  AARCH64_V6  = 6,  AARCH64_V7  = 7,
    AARCH64_V8  = 8,  AARCH64_V9  = 9,  AARCH64_V10 = 10, AARCH64_V11 = 11,
    AARCH64_V12 = 12, AARCH64_V13 = 13, AARCH64_V14 = 14, AARCH64_V15 = 15,
    AARCH64_V16 = 16, AARCH64_V17 = 17, AARCH64_V18 = 18, AARCH64_V19 = 19,
    AARCH64_V20 = 20, AARCH64_V21 = 21, AARCH64_V22 = 22, AARCH64_V23 = 23,
    AARCH64_V24 = 24, AARCH64_V25 = 25, AARCH64_V26 = 26, AARCH64_V27 = 27,
    AARCH64_V28 = 28, AARCH64_V29 = 29, AARCH64_V30 = 30, AARCH64_V31 = 31,
};

// =============================================================================
// AArch64 Instruction Encoding Helpers (stubs)
// =============================================================================

/**
 * AArch64 uses fixed-width 32-bit instructions.
 * All encodings follow the pattern:
 *   bits [31:25] = opcode category
 *   bits [24:0]  = operands, immediates, conditions
 *
 * Example encodings (for future implementation):
 *   ADD  Xd, Xn, Xm      : sf=1, op=0, S=0, shift=00, Rm, imm6=0, Rn, Rd
 *   SUB  Xd, Xn, Xm      : sf=1, op=1, S=0, shift=00, Rm, imm6=0, Rn, Rd
 *   MOV  Xd, #imm16       : sf=1, opc=10, hw=0, imm16, Rd  (MOVZ)
 *   B    label             : 000101 imm26
 *   B.cond label           : 01010100 imm19 0 cond
 *   RET  {Xn}             : 1101011 0010 11111 0000 00 Rn 00000
 */

// Emit a 32-bit AArch64 instruction (little-endian)
static void aarch64_emit_insn(CodeBuffer* buf, uint32_t insn) {
    if (!buf) return;
    npk_asm_emit_byte(buf, (insn >>  0) & 0xFF);
    npk_asm_emit_byte(buf, (insn >>  8) & 0xFF);
    npk_asm_emit_byte(buf, (insn >> 16) & 0xFF);
    npk_asm_emit_byte(buf, (insn >> 24) & 0xFF);
}

// =============================================================================
// AArch64 Calling Convention (AAPCS64)
// =============================================================================

// Arguments: X0-X7 (integer), V0-V7 (FP/SIMD)
// Return:    X0 (integer), V0 (FP/SIMD)
// Callee-saved: X19-X28, D8-D15, FP(X29), LR(X30)
// Caller-saved: X0-X18, V0-V7, V16-V31

// =============================================================================
// Stub: AArch64 is not yet implemented
// =============================================================================

// This file is compiled but the functions are only called if
// npk_asm_get_arch() returns ASM_ARCH_AARCH64, which currently
// never happens on x86-64 builds. When AArch64 support is added,
// these stubs will be replaced with real encoders.

// Placeholder: encode AArch64 NOP (0xD503201F)
static const uint32_t AARCH64_NOP = 0xD503201F;

// Placeholder: encode AArch64 RET (return via X30/LR)
static const uint32_t AARCH64_RET = 0xD65F03C0;

// Placeholder: encode AArch64 BRK #0 (debug breakpoint)
static const uint32_t AARCH64_BRK = 0xD4200000;

// Suppress unused variable warnings for the stub constants
__attribute__((unused)) static void aarch64_stub_use_constants(void) {
    (void)AARCH64_NOP;
    (void)AARCH64_RET;
    (void)AARCH64_BRK;
    (void)aarch64_emit_insn;
}
