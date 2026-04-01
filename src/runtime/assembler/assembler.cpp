/**
 * Aria Runtime Assembler Implementation
 * 
 * x86-64 instruction encoder with label backpatching and WildX integration.
 */

#include "runtime/assembler.h"
#include <cstdlib>
#include <cstring>
#include <cstdio>

// =============================================================================
// Code Buffer Implementation
// =============================================================================

CodeBuffer* aria_asm_buffer_create(size_t initial_capacity) {
    CodeBuffer* buf = (CodeBuffer*)malloc(sizeof(CodeBuffer));
    if (!buf) return nullptr;
    
    buf->data = (uint8_t*)malloc(initial_capacity);
    if (!buf->data) {
        free(buf);
        return nullptr;
    }
    
    buf->size = 0;
    buf->capacity = initial_capacity;
    return buf;
}

void aria_asm_buffer_destroy(CodeBuffer* buf) {
    if (!buf) return;
    free(buf->data);
    free(buf);
}

static void buffer_ensure_capacity(CodeBuffer* buf, size_t needed) {
    if (buf->size + needed <= buf->capacity) return;
    
    // Geometric growth: double capacity
    size_t new_capacity = buf->capacity * 2;
    while (buf->size + needed > new_capacity) {
        new_capacity *= 2;
    }
    
    uint8_t* new_data = (uint8_t*)realloc(buf->data, new_capacity);
    if (!new_data) {
        fprintf(stderr, "Fatal: Code buffer realloc failed\n");
        abort();
    }
    
    buf->data = new_data;
    buf->capacity = new_capacity;
}

void aria_asm_emit_byte(CodeBuffer* buf, uint8_t byte) {
    buffer_ensure_capacity(buf, 1);
    buf->data[buf->size++] = byte;
}

void aria_asm_emit_i32(CodeBuffer* buf, int32_t value) {
    buffer_ensure_capacity(buf, 4);
    uint8_t* ptr = buf->data + buf->size;
    ptr[0] = (value >> 0) & 0xFF;
    ptr[1] = (value >> 8) & 0xFF;
    ptr[2] = (value >> 16) & 0xFF;
    ptr[3] = (value >> 24) & 0xFF;
    buf->size += 4;
}

void aria_asm_emit_i64(CodeBuffer* buf, int64_t value) {
    buffer_ensure_capacity(buf, 8);
    uint8_t* ptr = buf->data + buf->size;
    for (int i = 0; i < 8; ++i) {
        ptr[i] = (value >> (i * 8)) & 0xFF;
    }
    buf->size += 8;
}

size_t aria_asm_buffer_offset(const CodeBuffer* buf) {
    return buf->size;
}

// =============================================================================
// Label Management
// =============================================================================

AsmLabel aria_asm_label_create() {
    AsmLabel label;
    label.position = -1;  // Unbound
    label.num_patches = 0;
    return label;
}

bool aria_asm_label_is_bound(const AsmLabel* label) {
    return label->position >= 0;
}

// =============================================================================
// Assembler Core
// =============================================================================

Assembler* aria_asm_create() {
    Assembler* asm_ctx = (Assembler*)malloc(sizeof(Assembler));
    if (!asm_ctx) return nullptr;
    
    asm_ctx->buffer = aria_asm_buffer_create(4096);  // 4KB initial capacity
    if (!asm_ctx->buffer) {
        free(asm_ctx);
        return nullptr;
    }
    
    asm_ctx->label_count = 0;
    asm_ctx->error = false;
    asm_ctx->error_msg[0] = '\0';
    
    return asm_ctx;
}

void aria_asm_destroy(Assembler* asm_ctx) {
    if (!asm_ctx) return;
    aria_asm_buffer_destroy(asm_ctx->buffer);
    free(asm_ctx);
}

bool aria_asm_has_error(const Assembler* asm_ctx) {
    return asm_ctx->error;
}

const char* aria_asm_get_error(const Assembler* asm_ctx) {
    return asm_ctx->error_msg;
}

static void set_error(Assembler* asm_ctx, const char* msg) {
    asm_ctx->error = true;
    snprintf(asm_ctx->error_msg, sizeof(asm_ctx->error_msg), "%s", msg);
}

// =============================================================================
// Label Operations
// =============================================================================

int aria_asm_new_label(Assembler* asm_ctx) {
    if (asm_ctx->label_count >= MAX_LABELS) {
        set_error(asm_ctx, "Too many labels (MAX_LABELS exceeded)");
        return -1;
    }
    
    int label_id = asm_ctx->label_count++;
    asm_ctx->labels[label_id] = aria_asm_label_create();
    return label_id;
}

void aria_asm_bind_label(Assembler* asm_ctx, int label_id) {
    if (label_id < 0 || label_id >= (int)asm_ctx->label_count) {
        set_error(asm_ctx, "Invalid label ID");
        return;
    }
    
    AsmLabel* label = &asm_ctx->labels[label_id];
    if (aria_asm_label_is_bound(label)) {
        set_error(asm_ctx, "Label already bound");
        return;
    }
    
    // Bind label to current offset
    label->position = (int32_t)aria_asm_buffer_offset(asm_ctx->buffer);
    
    // Backpatch all forward references
    for (uint32_t i = 0; i < label->num_patches; ++i) {
        uint32_t patch_site = label->patch_sites[i];
        
        // Calculate relative offset: target - (site + 4)
        // The +4 accounts for the size of the rel32 field itself
        int32_t offset = label->position - (int32_t)(patch_site + 4);
        
        // Write offset into buffer (little-endian)
        uint8_t* ptr = asm_ctx->buffer->data + patch_site;
        ptr[0] = (offset >> 0) & 0xFF;
        ptr[1] = (offset >> 8) & 0xFF;
        ptr[2] = (offset >> 16) & 0xFF;
        ptr[3] = (offset >> 24) & 0xFF;
    }
    
    // Clear patch list
    label->num_patches = 0;
}

// =============================================================================
// x86-64 Instruction Helpers
// =============================================================================

/**
 * Emit REX prefix if needed for 64-bit operation or extended registers
 * 
 * REX format: 0100WRXB
 * - W (bit 3): 1 = 64-bit operand size
 * - R (bit 2): Extension of ModR/M.reg field
 * - X (bit 1): Extension of SIB.index field
 * - B (bit 0): Extension of ModR/M.rm or SIB.base field
 */
static void emit_rex(CodeBuffer* buf, bool w, int reg, int rm) {
    uint8_t rex = 0x40;  // REX base
    
    if (w) rex |= 0x08;  // REX.W (64-bit)
    if (reg >= 8) rex |= 0x04;  // REX.R (extended reg)
    if (rm >= 8) rex |= 0x01;   // REX.B (extended rm)
    
    // Only emit if non-default or 64-bit mode
    if (rex != 0x40 || w) {
        aria_asm_emit_byte(buf, rex);
    }
}

/**
 * Emit ModR/M byte
 * 
 * Format: MMRRRMMM
 * - MM (bits 7-6): Addressing mode (11 = register direct)
 * - RRR (bits 5-3): Register operand or opcode extension
 * - MMM (bits 2-0): R/M operand
 */
static void emit_modrm(CodeBuffer* buf, uint8_t mod, int reg, int rm) {
    uint8_t modrm = (mod << 6) | ((reg & 0x07) << 3) | (rm & 0x07);
    aria_asm_emit_byte(buf, modrm);
}

// =============================================================================
// x86-64 Instruction Emission
// =============================================================================

void aria_asm_mov_r64_imm64(Assembler* asm_ctx, AsmRegister dst, int64_t value) {
    int reg = (int)dst;
    
    // MOVABS: REX.W + B8+rd id
    emit_rex(asm_ctx->buffer, true, 0, reg);
    aria_asm_emit_byte(asm_ctx->buffer, 0xB8 + (reg & 0x07));
    aria_asm_emit_i64(asm_ctx->buffer, value);
}

void aria_asm_mov_r64_r64(Assembler* asm_ctx, AsmRegister dst, AsmRegister src) {
    int dst_reg = (int)dst;
    int src_reg = (int)src;
    
    // MOV r64, r64: REX.W + 89 /r
    emit_rex(asm_ctx->buffer, true, src_reg, dst_reg);
    aria_asm_emit_byte(asm_ctx->buffer, 0x89);
    emit_modrm(asm_ctx->buffer, 0x03, src_reg, dst_reg);  // mod=11 (register direct)
}

void aria_asm_add_r64_r64(Assembler* asm_ctx, AsmRegister dst, AsmRegister src) {
    int dst_reg = (int)dst;
    int src_reg = (int)src;
    
    // ADD r64, r64: REX.W + 01 /r
    emit_rex(asm_ctx->buffer, true, src_reg, dst_reg);
    aria_asm_emit_byte(asm_ctx->buffer, 0x01);
    emit_modrm(asm_ctx->buffer, 0x03, src_reg, dst_reg);
}

void aria_asm_sub_r64_r64(Assembler* asm_ctx, AsmRegister dst, AsmRegister src) {
    int dst_reg = (int)dst;
    int src_reg = (int)src;
    
    // SUB r64, r64: REX.W + 29 /r
    emit_rex(asm_ctx->buffer, true, src_reg, dst_reg);
    aria_asm_emit_byte(asm_ctx->buffer, 0x29);
    emit_modrm(asm_ctx->buffer, 0x03, src_reg, dst_reg);
}

void aria_asm_imul_r64_r64(Assembler* asm_ctx, AsmRegister dst, AsmRegister src) {
    int dst_reg = (int)dst;
    int src_reg = (int)src;
    
    // IMUL r64, r64: REX.W + 0F AF /r
    emit_rex(asm_ctx->buffer, true, dst_reg, src_reg);
    aria_asm_emit_byte(asm_ctx->buffer, 0x0F);
    aria_asm_emit_byte(asm_ctx->buffer, 0xAF);
    emit_modrm(asm_ctx->buffer, 0x03, dst_reg, src_reg);
}

void aria_asm_ret(Assembler* asm_ctx) {
    // RET: C3
    aria_asm_emit_byte(asm_ctx->buffer, 0xC3);
}

void aria_asm_push_r64(Assembler* asm_ctx, AsmRegister reg) {
    int reg_num = (int)reg;
    
    // PUSH r64: 50+rd (or REX.B + 50+rd for R8-R15)
    if (reg_num >= 8) {
        emit_rex(asm_ctx->buffer, false, 0, reg_num);
    }
    aria_asm_emit_byte(asm_ctx->buffer, 0x50 + (reg_num & 0x07));
}

void aria_asm_pop_r64(Assembler* asm_ctx, AsmRegister reg) {
    int reg_num = (int)reg;
    
    // POP r64: 58+rd (or REX.B + 58+rd for R8-R15)
    if (reg_num >= 8) {
        emit_rex(asm_ctx->buffer, false, 0, reg_num);
    }
    aria_asm_emit_byte(asm_ctx->buffer, 0x58 + (reg_num & 0x07));
}

void aria_asm_jmp(Assembler* asm_ctx, int label_id) {
    if (label_id < 0 || label_id >= (int)asm_ctx->label_count) {
        set_error(asm_ctx, "Invalid label ID for JMP");
        return;
    }
    
    AsmLabel* label = &asm_ctx->labels[label_id];
    
    // JMP rel32: E9 cd
    aria_asm_emit_byte(asm_ctx->buffer, 0xE9);
    
    if (aria_asm_label_is_bound(label)) {
        // Backward jump: calculate offset immediately
        uint32_t current = (uint32_t)aria_asm_buffer_offset(asm_ctx->buffer);
        int32_t offset = label->position - (int32_t)(current + 4);
        aria_asm_emit_i32(asm_ctx->buffer, offset);
    } else {
        // Forward jump: add patch site and emit placeholder
        if (label->num_patches >= MAX_PATCH_SITES) {
            set_error(asm_ctx, "Too many forward references to label");
            return;
        }
        label->patch_sites[label->num_patches++] = (uint32_t)aria_asm_buffer_offset(asm_ctx->buffer);
        aria_asm_emit_i32(asm_ctx->buffer, 0);  // Placeholder
    }
}

void aria_asm_je(Assembler* asm_ctx, int label_id) {
    if (label_id < 0 || label_id >= (int)asm_ctx->label_count) {
        set_error(asm_ctx, "Invalid label ID for JE");
        return;
    }
    
    AsmLabel* label = &asm_ctx->labels[label_id];
    
    // JE rel32: 0F 84 cd
    aria_asm_emit_byte(asm_ctx->buffer, 0x0F);
    aria_asm_emit_byte(asm_ctx->buffer, 0x84);
    
    if (aria_asm_label_is_bound(label)) {
        uint32_t current = (uint32_t)aria_asm_buffer_offset(asm_ctx->buffer);
        int32_t offset = label->position - (int32_t)(current + 4);
        aria_asm_emit_i32(asm_ctx->buffer, offset);
    } else {
        if (label->num_patches >= MAX_PATCH_SITES) {
            set_error(asm_ctx, "Too many forward references to label");
            return;
        }
        label->patch_sites[label->num_patches++] = (uint32_t)aria_asm_buffer_offset(asm_ctx->buffer);
        aria_asm_emit_i32(asm_ctx->buffer, 0);
    }
}

void aria_asm_jne(Assembler* asm_ctx, int label_id) {
    if (label_id < 0 || label_id >= (int)asm_ctx->label_count) {
        set_error(asm_ctx, "Invalid label ID for JNE");
        return;
    }
    
    AsmLabel* label = &asm_ctx->labels[label_id];
    
    // JNE rel32: 0F 85 cd
    aria_asm_emit_byte(asm_ctx->buffer, 0x0F);
    aria_asm_emit_byte(asm_ctx->buffer, 0x85);
    
    if (aria_asm_label_is_bound(label)) {
        uint32_t current = (uint32_t)aria_asm_buffer_offset(asm_ctx->buffer);
        int32_t offset = label->position - (int32_t)(current + 4);
        aria_asm_emit_i32(asm_ctx->buffer, offset);
    } else {
        if (label->num_patches >= MAX_PATCH_SITES) {
            set_error(asm_ctx, "Too many forward references to label");
            return;
        }
        label->patch_sites[label->num_patches++] = (uint32_t)aria_asm_buffer_offset(asm_ctx->buffer);
        aria_asm_emit_i32(asm_ctx->buffer, 0);
    }
}

void aria_asm_cmp_r64_r64(Assembler* asm_ctx, AsmRegister left, AsmRegister right) {
    int left_reg = (int)left;
    int right_reg = (int)right;
    
    // CMP r64, r64: REX.W + 39 /r
    emit_rex(asm_ctx->buffer, true, right_reg, left_reg);
    aria_asm_emit_byte(asm_ctx->buffer, 0x39);
    emit_modrm(asm_ctx->buffer, 0x03, right_reg, left_reg);
}

// =============================================================================
// Extended Integer Instructions (v0.7.2)
// =============================================================================

void aria_asm_add_r64_imm32(Assembler* asm_ctx, AsmRegister dst, int32_t value) {
    int reg = (int)dst;
    // ADD r64, imm32: REX.W + 81 /0 id
    emit_rex(asm_ctx->buffer, true, 0, reg);
    aria_asm_emit_byte(asm_ctx->buffer, 0x81);
    emit_modrm(asm_ctx->buffer, 0x03, 0, reg);  // /0
    aria_asm_emit_i32(asm_ctx->buffer, value);
}

void aria_asm_sub_r64_imm32(Assembler* asm_ctx, AsmRegister dst, int32_t value) {
    int reg = (int)dst;
    // SUB r64, imm32: REX.W + 81 /5 id
    emit_rex(asm_ctx->buffer, true, 0, reg);
    aria_asm_emit_byte(asm_ctx->buffer, 0x81);
    emit_modrm(asm_ctx->buffer, 0x03, 5, reg);  // /5
    aria_asm_emit_i32(asm_ctx->buffer, value);
}

void aria_asm_xor_r64_r64(Assembler* asm_ctx, AsmRegister dst, AsmRegister src) {
    int dst_reg = (int)dst;
    int src_reg = (int)src;
    // XOR r64, r64: REX.W + 31 /r
    emit_rex(asm_ctx->buffer, true, src_reg, dst_reg);
    aria_asm_emit_byte(asm_ctx->buffer, 0x31);
    emit_modrm(asm_ctx->buffer, 0x03, src_reg, dst_reg);
}

void aria_asm_and_r64_r64(Assembler* asm_ctx, AsmRegister dst, AsmRegister src) {
    int dst_reg = (int)dst;
    int src_reg = (int)src;
    // AND r64, r64: REX.W + 21 /r
    emit_rex(asm_ctx->buffer, true, src_reg, dst_reg);
    aria_asm_emit_byte(asm_ctx->buffer, 0x21);
    emit_modrm(asm_ctx->buffer, 0x03, src_reg, dst_reg);
}

void aria_asm_or_r64_r64(Assembler* asm_ctx, AsmRegister dst, AsmRegister src) {
    int dst_reg = (int)dst;
    int src_reg = (int)src;
    // OR r64, r64: REX.W + 09 /r
    emit_rex(asm_ctx->buffer, true, src_reg, dst_reg);
    aria_asm_emit_byte(asm_ctx->buffer, 0x09);
    emit_modrm(asm_ctx->buffer, 0x03, src_reg, dst_reg);
}

void aria_asm_not_r64(Assembler* asm_ctx, AsmRegister reg) {
    int r = (int)reg;
    // NOT r64: REX.W + F7 /2
    emit_rex(asm_ctx->buffer, true, 0, r);
    aria_asm_emit_byte(asm_ctx->buffer, 0xF7);
    emit_modrm(asm_ctx->buffer, 0x03, 2, r);  // /2
}

void aria_asm_neg_r64(Assembler* asm_ctx, AsmRegister reg) {
    int r = (int)reg;
    // NEG r64: REX.W + F7 /3
    emit_rex(asm_ctx->buffer, true, 0, r);
    aria_asm_emit_byte(asm_ctx->buffer, 0xF7);
    emit_modrm(asm_ctx->buffer, 0x03, 3, r);  // /3
}

void aria_asm_shl_r64_imm8(Assembler* asm_ctx, AsmRegister reg, uint8_t count) {
    int r = (int)reg;
    // SHL r64, imm8: REX.W + C1 /4 ib
    emit_rex(asm_ctx->buffer, true, 0, r);
    aria_asm_emit_byte(asm_ctx->buffer, 0xC1);
    emit_modrm(asm_ctx->buffer, 0x03, 4, r);  // /4
    aria_asm_emit_byte(asm_ctx->buffer, count);
}

void aria_asm_shr_r64_imm8(Assembler* asm_ctx, AsmRegister reg, uint8_t count) {
    int r = (int)reg;
    // SHR r64, imm8: REX.W + C1 /5 ib
    emit_rex(asm_ctx->buffer, true, 0, r);
    aria_asm_emit_byte(asm_ctx->buffer, 0xC1);
    emit_modrm(asm_ctx->buffer, 0x03, 5, r);  // /5
    aria_asm_emit_byte(asm_ctx->buffer, count);
}

void aria_asm_sar_r64_imm8(Assembler* asm_ctx, AsmRegister reg, uint8_t count) {
    int r = (int)reg;
    // SAR r64, imm8: REX.W + C1 /7 ib
    emit_rex(asm_ctx->buffer, true, 0, r);
    aria_asm_emit_byte(asm_ctx->buffer, 0xC1);
    emit_modrm(asm_ctx->buffer, 0x03, 7, r);  // /7
    aria_asm_emit_byte(asm_ctx->buffer, count);
}

void aria_asm_cmp_r64_imm32(Assembler* asm_ctx, AsmRegister reg, int32_t value) {
    int r = (int)reg;
    // CMP r64, imm32: REX.W + 81 /7 id
    emit_rex(asm_ctx->buffer, true, 0, r);
    aria_asm_emit_byte(asm_ctx->buffer, 0x81);
    emit_modrm(asm_ctx->buffer, 0x03, 7, r);  // /7
    aria_asm_emit_i32(asm_ctx->buffer, value);
}

// =============================================================================
// Extended Conditional Jumps (v0.7.2)
// =============================================================================

// Helper: emit a conditional jump with 0F xx opcode
static void emit_jcc_rel32(Assembler* asm_ctx, uint8_t cc_opcode, int label_id, const char* mnemonic) {
    if (label_id < 0 || label_id >= (int)asm_ctx->label_count) {
        char msg[64];
        snprintf(msg, sizeof(msg), "Invalid label ID for %s", mnemonic);
        set_error(asm_ctx, msg);
        return;
    }

    AsmLabel* label = &asm_ctx->labels[label_id];

    // Jcc rel32: 0F xx cd
    aria_asm_emit_byte(asm_ctx->buffer, 0x0F);
    aria_asm_emit_byte(asm_ctx->buffer, cc_opcode);

    if (aria_asm_label_is_bound(label)) {
        uint32_t current = (uint32_t)aria_asm_buffer_offset(asm_ctx->buffer);
        int32_t offset = label->position - (int32_t)(current + 4);
        aria_asm_emit_i32(asm_ctx->buffer, offset);
    } else {
        if (label->num_patches >= MAX_PATCH_SITES) {
            set_error(asm_ctx, "Too many forward references to label");
            return;
        }
        label->patch_sites[label->num_patches++] = (uint32_t)aria_asm_buffer_offset(asm_ctx->buffer);
        aria_asm_emit_i32(asm_ctx->buffer, 0);
    }
}

void aria_asm_jl(Assembler* asm_ctx, int label_id)  { emit_jcc_rel32(asm_ctx, 0x8C, label_id, "JL"); }
void aria_asm_jle(Assembler* asm_ctx, int label_id) { emit_jcc_rel32(asm_ctx, 0x8E, label_id, "JLE"); }
void aria_asm_jg(Assembler* asm_ctx, int label_id)  { emit_jcc_rel32(asm_ctx, 0x8F, label_id, "JG"); }
void aria_asm_jge(Assembler* asm_ctx, int label_id) { emit_jcc_rel32(asm_ctx, 0x8D, label_id, "JGE"); }
void aria_asm_jb(Assembler* asm_ctx, int label_id)  { emit_jcc_rel32(asm_ctx, 0x82, label_id, "JB"); }
void aria_asm_jbe(Assembler* asm_ctx, int label_id) { emit_jcc_rel32(asm_ctx, 0x86, label_id, "JBE"); }
void aria_asm_ja(Assembler* asm_ctx, int label_id)  { emit_jcc_rel32(asm_ctx, 0x87, label_id, "JA"); }
void aria_asm_jae(Assembler* asm_ctx, int label_id) { emit_jcc_rel32(asm_ctx, 0x83, label_id, "JAE"); }

// =============================================================================
// Memory Operations (v0.7.2)
// =============================================================================

/**
 * Emit ModR/M + optional SIB + displacement for [base + offset] addressing.
 * Handles special cases: RSP/R12 need SIB byte, RBP/R13 need explicit disp8=0.
 */
static void emit_mem_operand(CodeBuffer* buf, int reg, int base, int32_t offset) {
    int base_lo = base & 0x07;

    // Determine mod field
    uint8_t mod;
    if (offset == 0 && base_lo != 5) {  // RBP/R13 always need displacement
        mod = 0x00;
    } else if (offset >= -128 && offset <= 127) {
        mod = 0x01;  // disp8
    } else {
        mod = 0x02;  // disp32
    }

    if (base_lo == 4) {
        // RSP/R12: need SIB byte (base=RSP, index=none, scale=1)
        emit_modrm(buf, mod, reg, 4);    // r/m=100 means SIB follows
        aria_asm_emit_byte(buf, 0x24);    // SIB: scale=00, index=100(none), base=100(RSP)
    } else {
        emit_modrm(buf, mod, reg, base_lo);
    }

    // Emit displacement
    if (mod == 0x01) {
        aria_asm_emit_byte(buf, (uint8_t)(offset & 0xFF));
    } else if (mod == 0x02) {
        aria_asm_emit_i32(buf, offset);
    }
}

void aria_asm_mov_r64_mem(Assembler* asm_ctx, AsmRegister dst, AsmRegister base, int32_t offset) {
    int dst_reg = (int)dst;
    int base_reg = (int)base;

    // MOV r64, [base + offset]: REX.W + 8B /r
    emit_rex(asm_ctx->buffer, true, dst_reg, base_reg);
    aria_asm_emit_byte(asm_ctx->buffer, 0x8B);
    emit_mem_operand(asm_ctx->buffer, dst_reg, base_reg, offset);
}

void aria_asm_mov_mem_r64(Assembler* asm_ctx, AsmRegister base, int32_t offset, AsmRegister src) {
    int src_reg = (int)src;
    int base_reg = (int)base;

    // MOV [base + offset], r64: REX.W + 89 /r
    emit_rex(asm_ctx->buffer, true, src_reg, base_reg);
    aria_asm_emit_byte(asm_ctx->buffer, 0x89);
    emit_mem_operand(asm_ctx->buffer, src_reg, base_reg, offset);
}

void aria_asm_lea_r64_mem(Assembler* asm_ctx, AsmRegister dst, AsmRegister base, int32_t offset) {
    int dst_reg = (int)dst;
    int base_reg = (int)base;

    // LEA r64, [base + offset]: REX.W + 8D /r
    emit_rex(asm_ctx->buffer, true, dst_reg, base_reg);
    aria_asm_emit_byte(asm_ctx->buffer, 0x8D);
    emit_mem_operand(asm_ctx->buffer, dst_reg, base_reg, offset);
}

// =============================================================================
// Stack Frame & Local Variables (v0.7.2)
// =============================================================================

void aria_asm_store_local(Assembler* asm_ctx, uint32_t slot_offset, AsmRegister src) {
    // MOV [RBP - slot_offset], src
    aria_asm_mov_mem_r64(asm_ctx, REG_RBP, -(int32_t)slot_offset, src);
}

void aria_asm_load_local(Assembler* asm_ctx, AsmRegister dst, uint32_t slot_offset) {
    // MOV dst, [RBP - slot_offset]
    aria_asm_mov_r64_mem(asm_ctx, dst, REG_RBP, -(int32_t)slot_offset);
}

// =============================================================================
// CALL Instructions (v0.7.2)
// =============================================================================

void aria_asm_call_r64(Assembler* asm_ctx, AsmRegister target) {
    int reg = (int)target;

    // CALL r64: FF /2 (ModR/M mod=11, reg=010)
    if (reg >= 8) {
        emit_rex(asm_ctx->buffer, false, 0, reg);  // REX.B for extended regs
    }
    aria_asm_emit_byte(asm_ctx->buffer, 0xFF);
    emit_modrm(asm_ctx->buffer, 0x03, 2, reg);  // mod=11, /2
}

void aria_asm_call_label(Assembler* asm_ctx, int label_id) {
    if (label_id < 0 || label_id >= (int)asm_ctx->label_count) {
        set_error(asm_ctx, "Invalid label ID for CALL");
        return;
    }

    AsmLabel* label = &asm_ctx->labels[label_id];

    // CALL rel32: E8 cd
    aria_asm_emit_byte(asm_ctx->buffer, 0xE8);

    if (aria_asm_label_is_bound(label)) {
        uint32_t current = (uint32_t)aria_asm_buffer_offset(asm_ctx->buffer);
        int32_t offset = label->position - (int32_t)(current + 4);
        aria_asm_emit_i32(asm_ctx->buffer, offset);
    } else {
        if (label->num_patches >= MAX_PATCH_SITES) {
            set_error(asm_ctx, "Too many forward references to label");
            return;
        }
        label->patch_sites[label->num_patches++] = (uint32_t)aria_asm_buffer_offset(asm_ctx->buffer);
        aria_asm_emit_i32(asm_ctx->buffer, 0);
    }
}

void aria_asm_call_abs(Assembler* asm_ctx, void* func_ptr) {
    // MOV R11, <64-bit address>
    aria_asm_mov_r64_imm64(asm_ctx, REG_R11, (int64_t)(uintptr_t)func_ptr);
    // CALL R11
    aria_asm_call_r64(asm_ctx, REG_R11);
}

// =============================================================================
// SSE2 Floating-Point Instructions (v0.7.2)
// =============================================================================

// Helper: get XMM register number (0-15)
static int xmm_num(AsmRegister reg) {
    return (int)reg - 64;  // REG_XMM0 = 64
}

// Helper: emit REX prefix for SSE instructions (only needed for XMM8-15)
static void emit_rex_sse(CodeBuffer* buf, int xmm_reg, int xmm_rm) {
    uint8_t rex = 0x40;
    if (xmm_reg >= 8) rex |= 0x04;  // REX.R
    if (xmm_rm >= 8)  rex |= 0x01;  // REX.B
    if (rex != 0x40) {
        aria_asm_emit_byte(buf, rex);
    }
}

// Helper: emit REX for SSE memory operands (reg + base addressing)
static void emit_rex_sse_mem(CodeBuffer* buf, int xmm_reg, int base_gpr) {
    uint8_t rex = 0x40;
    if (xmm_reg >= 8) rex |= 0x04;  // REX.R
    if (base_gpr >= 8) rex |= 0x01;  // REX.B
    if (rex != 0x40) {
        aria_asm_emit_byte(buf, rex);
    }
}

void aria_asm_movsd_xmm_xmm(Assembler* asm_ctx, AsmRegister dst, AsmRegister src) {
    int d = xmm_num(dst);
    int s = xmm_num(src);
    // MOVSD xmm1, xmm2: F2 0F 10 /r
    aria_asm_emit_byte(asm_ctx->buffer, 0xF2);
    emit_rex_sse(asm_ctx->buffer, d, s);
    aria_asm_emit_byte(asm_ctx->buffer, 0x0F);
    aria_asm_emit_byte(asm_ctx->buffer, 0x10);
    emit_modrm(asm_ctx->buffer, 0x03, d, s);
}

void aria_asm_movsd_xmm_mem(Assembler* asm_ctx, AsmRegister dst, AsmRegister base, int32_t offset) {
    int d = xmm_num(dst);
    int b = (int)base;
    // MOVSD xmm, [base+offset]: F2 [REX] 0F 10 /r mem
    aria_asm_emit_byte(asm_ctx->buffer, 0xF2);
    emit_rex_sse_mem(asm_ctx->buffer, d, b);
    aria_asm_emit_byte(asm_ctx->buffer, 0x0F);
    aria_asm_emit_byte(asm_ctx->buffer, 0x10);
    emit_mem_operand(asm_ctx->buffer, d, b, offset);
}

void aria_asm_movsd_mem_xmm(Assembler* asm_ctx, AsmRegister base, int32_t offset, AsmRegister src) {
    int s = xmm_num(src);
    int b = (int)base;
    // MOVSD [base+offset], xmm: F2 [REX] 0F 11 /r mem
    aria_asm_emit_byte(asm_ctx->buffer, 0xF2);
    emit_rex_sse_mem(asm_ctx->buffer, s, b);
    aria_asm_emit_byte(asm_ctx->buffer, 0x0F);
    aria_asm_emit_byte(asm_ctx->buffer, 0x11);
    emit_mem_operand(asm_ctx->buffer, s, b, offset);
}

// Helper: emit SSE2 scalar double arithmetic (F2 0F <op> /r)
static void emit_sse2_sd_rr(Assembler* asm_ctx, uint8_t opcode, AsmRegister dst, AsmRegister src) {
    int d = xmm_num(dst);
    int s = xmm_num(src);
    aria_asm_emit_byte(asm_ctx->buffer, 0xF2);
    emit_rex_sse(asm_ctx->buffer, d, s);
    aria_asm_emit_byte(asm_ctx->buffer, 0x0F);
    aria_asm_emit_byte(asm_ctx->buffer, opcode);
    emit_modrm(asm_ctx->buffer, 0x03, d, s);
}

void aria_asm_addsd(Assembler* asm_ctx, AsmRegister dst, AsmRegister src) {
    emit_sse2_sd_rr(asm_ctx, 0x58, dst, src);
}

void aria_asm_subsd(Assembler* asm_ctx, AsmRegister dst, AsmRegister src) {
    emit_sse2_sd_rr(asm_ctx, 0x5C, dst, src);
}

void aria_asm_mulsd(Assembler* asm_ctx, AsmRegister dst, AsmRegister src) {
    emit_sse2_sd_rr(asm_ctx, 0x59, dst, src);
}

void aria_asm_divsd(Assembler* asm_ctx, AsmRegister dst, AsmRegister src) {
    emit_sse2_sd_rr(asm_ctx, 0x5E, dst, src);
}

void aria_asm_ucomisd(Assembler* asm_ctx, AsmRegister left, AsmRegister right) {
    int l = xmm_num(left);
    int r = xmm_num(right);
    // UCOMISD: 66 0F 2E /r
    aria_asm_emit_byte(asm_ctx->buffer, 0x66);
    emit_rex_sse(asm_ctx->buffer, l, r);
    aria_asm_emit_byte(asm_ctx->buffer, 0x0F);
    aria_asm_emit_byte(asm_ctx->buffer, 0x2E);
    emit_modrm(asm_ctx->buffer, 0x03, l, r);
}

// =============================================================================
// SSE Packed Float Instructions (v0.7.2)
// =============================================================================

void aria_asm_movaps_xmm_xmm(Assembler* asm_ctx, AsmRegister dst, AsmRegister src) {
    int d = xmm_num(dst);
    int s = xmm_num(src);
    // MOVAPS xmm, xmm: [REX] 0F 28 /r
    emit_rex_sse(asm_ctx->buffer, d, s);
    aria_asm_emit_byte(asm_ctx->buffer, 0x0F);
    aria_asm_emit_byte(asm_ctx->buffer, 0x28);
    emit_modrm(asm_ctx->buffer, 0x03, d, s);
}

void aria_asm_movaps_xmm_mem(Assembler* asm_ctx, AsmRegister dst, AsmRegister base, int32_t offset) {
    int d = xmm_num(dst);
    int b = (int)base;
    // MOVAPS xmm, [mem]: [REX] 0F 28 /r mem
    emit_rex_sse_mem(asm_ctx->buffer, d, b);
    aria_asm_emit_byte(asm_ctx->buffer, 0x0F);
    aria_asm_emit_byte(asm_ctx->buffer, 0x28);
    emit_mem_operand(asm_ctx->buffer, d, b, offset);
}

void aria_asm_movaps_mem_xmm(Assembler* asm_ctx, AsmRegister base, int32_t offset, AsmRegister src) {
    int s = xmm_num(src);
    int b = (int)base;
    // MOVAPS [mem], xmm: [REX] 0F 29 /r mem
    emit_rex_sse_mem(asm_ctx->buffer, s, b);
    aria_asm_emit_byte(asm_ctx->buffer, 0x0F);
    aria_asm_emit_byte(asm_ctx->buffer, 0x29);
    emit_mem_operand(asm_ctx->buffer, s, b, offset);
}

// Helper: emit packed single arithmetic ([REX] 0F <op> /r)
static void emit_sse_ps_rr(Assembler* asm_ctx, uint8_t opcode, AsmRegister dst, AsmRegister src) {
    int d = xmm_num(dst);
    int s = xmm_num(src);
    emit_rex_sse(asm_ctx->buffer, d, s);
    aria_asm_emit_byte(asm_ctx->buffer, 0x0F);
    aria_asm_emit_byte(asm_ctx->buffer, opcode);
    emit_modrm(asm_ctx->buffer, 0x03, d, s);
}

void aria_asm_addps(Assembler* asm_ctx, AsmRegister dst, AsmRegister src) {
    emit_sse_ps_rr(asm_ctx, 0x58, dst, src);
}

void aria_asm_mulps(Assembler* asm_ctx, AsmRegister dst, AsmRegister src) {
    emit_sse_ps_rr(asm_ctx, 0x59, dst, src);
}

// =============================================================================
// Floating-Point Execution Variants (v0.7.2)
// =============================================================================

double aria_asm_execute_f64(WildXGuard* guard) {
    if (!guard || !guard->ptr || guard->state != WILDX_STATE_EXECUTABLE) {
        return 0.0;
    }

    if (guard->code_hash != 0) {
        uint64_t current = aria_wildx_verify_hash(guard);
        if (current != guard->code_hash) {
            return 0.0;  // Tampered
        }
    }

    typedef double (*func_t)(void);
    func_t func = (func_t)guard->ptr;
    return func();
}

double aria_asm_execute_f64_f64(WildXGuard* guard, double arg1) {
    if (!guard || !guard->ptr || guard->state != WILDX_STATE_EXECUTABLE) {
        return 0.0;
    }

    if (guard->code_hash != 0) {
        uint64_t current = aria_wildx_verify_hash(guard);
        if (current != guard->code_hash) {
            return 0.0;  // Tampered
        }
    }

    typedef double (*func_t)(double);
    func_t func = (func_t)guard->ptr;
    return func(arg1);
}

// =============================================================================
// High-Level Code Generation
// =============================================================================

void aria_asm_prologue(Assembler* asm_ctx, size_t stack_size) {
    // PUSH RBP
    aria_asm_push_r64(asm_ctx, REG_RBP);
    
    // MOV RBP, RSP
    aria_asm_mov_r64_r64(asm_ctx, REG_RBP, REG_RSP);
    
    // SUB RSP, stack_size (if needed)
    if (stack_size > 0) {
        // For simplicity, use SUB RSP, imm32 (REX.W + 81 /5 id)
        emit_rex(asm_ctx->buffer, true, 0, REG_RSP);
        aria_asm_emit_byte(asm_ctx->buffer, 0x81);
        emit_modrm(asm_ctx->buffer, 0x03, 5, REG_RSP);  // opcode extension /5
        aria_asm_emit_i32(asm_ctx->buffer, (int32_t)stack_size);
    }
}

void aria_asm_epilogue(Assembler* asm_ctx) {
    // MOV RSP, RBP
    aria_asm_mov_r64_r64(asm_ctx, REG_RSP, REG_RBP);
    
    // POP RBP
    aria_asm_pop_r64(asm_ctx, REG_RBP);
    
    // RET
    aria_asm_ret(asm_ctx);
}

// =============================================================================
// Finalization and Execution
// =============================================================================

WildXGuard aria_asm_finalize(Assembler* asm_ctx) {
    WildXGuard empty = {nullptr, 0, 0, WILDX_STATE_UNINITIALIZED, false, 0};
    // Check for errors
    if (asm_ctx->error) {
        return empty;
    }
    
    // Verify all labels are bound
    for (uint32_t i = 0; i < asm_ctx->label_count; ++i) {
        if (!aria_asm_label_is_bound(&asm_ctx->labels[i])) {
            set_error(asm_ctx, "Unbound label detected at finalization");
            return empty;
        }
    }
    
    // Allocate WildX memory
    WildXGuard guard = aria_alloc_exec(asm_ctx->buffer->size);
    if (!guard.ptr) {
        set_error(asm_ctx, "Failed to allocate WildX memory");
        return empty;
    }
    
    // Copy code to WildX memory
    memcpy(guard.ptr, asm_ctx->buffer->data, asm_ctx->buffer->size);
    
    // Seal memory (RW → RX) — also computes code hash (v0.7.1)
    if (aria_mem_protect_exec(&guard) != 0) {
        aria_free_exec(&guard);
        set_error(asm_ctx, "Failed to seal WildX memory");
        return empty;
    }
    
    return guard;
}

int64_t aria_asm_execute(WildXGuard* guard) {
    if (!guard || !guard->ptr || guard->state != WILDX_STATE_EXECUTABLE) {
        return -1;
    }
    
    // v0.7.1: Verify code integrity before execution
    if (guard->code_hash != 0) {
        uint64_t current = aria_wildx_verify_hash(guard);
        if (current != guard->code_hash) {
            return -1; // Tampered
        }
    }
    
    // Cast to function pointer: int64_t (*)(void)
    typedef int64_t (*func_t)(void);
    func_t func = (func_t)guard->ptr;
    
    return func();
}

int64_t aria_asm_execute_i64(WildXGuard* guard, int64_t arg1) {
    if (!guard || !guard->ptr || guard->state != WILDX_STATE_EXECUTABLE) {
        return -1;
    }
    
    // v0.7.1: Verify code integrity before execution
    if (guard->code_hash != 0) {
        uint64_t current = aria_wildx_verify_hash(guard);
        if (current != guard->code_hash) {
            return -1; // Tampered
        }
    }
    
    // Cast to function pointer: int64_t (*)(int64_t)
    typedef int64_t (*func_t)(int64_t);
    func_t func = (func_t)guard->ptr;
    
    return func(arg1);
}

int64_t aria_asm_execute_i64_i64(WildXGuard* guard, int64_t arg1, int64_t arg2) {
    if (!guard || !guard->ptr || guard->state != WILDX_STATE_EXECUTABLE) {
        return -1;
    }
    
    // v0.7.1: Verify code integrity before execution
    if (guard->code_hash != 0) {
        uint64_t current = aria_wildx_verify_hash(guard);
        if (current != guard->code_hash) {
            return -1; // Tampered
        }
    }
    
    // Cast to function pointer: int64_t (*)(int64_t, int64_t)
    typedef int64_t (*func_t)(int64_t, int64_t);
    func_t func = (func_t)guard->ptr;
    
    return func(arg1, arg2);
}
