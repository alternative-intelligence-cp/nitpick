/**
 * Aria Runtime Assembler Implementation
 * 
 * x86-64 instruction encoder with label backpatching and WildX integration.
 */

#include "runtime/assembler.h"
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <unistd.h>

// =============================================================================
// Forward Declarations — Register Allocator Helpers (v0.7.3)
// =============================================================================

static void set_error(Assembler* asm_ctx, const char* msg);
static RegAllocState* ensure_regalloc(Assembler* asm_ctx);
static JitIR* ir_append(Assembler* asm_ctx);
static bool needs_ir(Assembler* asm_ctx);
static void ir_queue_rr(Assembler* asm_ctx, JitOpcode op, int32_t dst, int32_t src);
static void ir_queue_ri64(Assembler* asm_ctx, JitOpcode op, int32_t dst, int64_t imm);
static void ir_queue_ri32(Assembler* asm_ctx, JitOpcode op, int32_t dst, int32_t val);
static void ir_queue_r(Assembler* asm_ctx, JitOpcode op, int32_t dst);
static void ir_queue_label(Assembler* asm_ctx, JitOpcode op, int32_t label_id);
static void ir_queue_mem(Assembler* asm_ctx, JitOpcode op, int32_t reg, int32_t base, int32_t offset);
static void ir_queue_store(Assembler* asm_ctx, JitOpcode op, int32_t base, int32_t offset, int32_t src);
static void run_regalloc_pipeline(Assembler* asm_ctx);
static int32_t resolve_reg2(Assembler* asm_ctx, int32_t reg, bool is_def);
static int32_t peephole_optimize(RegAllocState* ra);

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
    asm_ctx->regalloc = nullptr;  // Lazy init when first vreg is created
    
    return asm_ctx;
}

void aria_asm_destroy(Assembler* asm_ctx) {
    if (!asm_ctx) return;
    aria_asm_buffer_destroy(asm_ctx->buffer);
    if (asm_ctx->regalloc) {
        free(asm_ctx->regalloc->ir);
        free(asm_ctx->regalloc);
    }
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
    if (needs_ir(asm_ctx)) { ir_queue_label(asm_ctx, JIT_OP_LABEL, label_id); return; }
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
    if (needs_ir(asm_ctx)) { ir_queue_ri64(asm_ctx, JIT_OP_MOV_IMM64, (int32_t)dst, value); return; }
    int reg = (int)dst;
    
    // MOVABS: REX.W + B8+rd id
    emit_rex(asm_ctx->buffer, true, 0, reg);
    aria_asm_emit_byte(asm_ctx->buffer, 0xB8 + (reg & 0x07));
    aria_asm_emit_i64(asm_ctx->buffer, value);
}

void aria_asm_mov_r64_r64(Assembler* asm_ctx, AsmRegister dst, AsmRegister src) {
    if (needs_ir(asm_ctx)) { ir_queue_rr(asm_ctx, JIT_OP_MOV_RR, (int32_t)dst, (int32_t)src); return; }
    int dst_reg = (int)dst;
    int src_reg = (int)src;
    
    // MOV r64, r64: REX.W + 89 /r
    emit_rex(asm_ctx->buffer, true, src_reg, dst_reg);
    aria_asm_emit_byte(asm_ctx->buffer, 0x89);
    emit_modrm(asm_ctx->buffer, 0x03, src_reg, dst_reg);  // mod=11 (register direct)
}

void aria_asm_add_r64_r64(Assembler* asm_ctx, AsmRegister dst, AsmRegister src) {
    if (needs_ir(asm_ctx)) { ir_queue_rr(asm_ctx, JIT_OP_ADD_RR, (int32_t)dst, (int32_t)src); return; }
    int dst_reg = (int)dst;
    int src_reg = (int)src;
    
    // ADD r64, r64: REX.W + 01 /r
    emit_rex(asm_ctx->buffer, true, src_reg, dst_reg);
    aria_asm_emit_byte(asm_ctx->buffer, 0x01);
    emit_modrm(asm_ctx->buffer, 0x03, src_reg, dst_reg);
}

void aria_asm_sub_r64_r64(Assembler* asm_ctx, AsmRegister dst, AsmRegister src) {
    if (needs_ir(asm_ctx)) { ir_queue_rr(asm_ctx, JIT_OP_SUB_RR, (int32_t)dst, (int32_t)src); return; }
    int dst_reg = (int)dst;
    int src_reg = (int)src;
    
    // SUB r64, r64: REX.W + 29 /r
    emit_rex(asm_ctx->buffer, true, src_reg, dst_reg);
    aria_asm_emit_byte(asm_ctx->buffer, 0x29);
    emit_modrm(asm_ctx->buffer, 0x03, src_reg, dst_reg);
}

void aria_asm_imul_r64_r64(Assembler* asm_ctx, AsmRegister dst, AsmRegister src) {
    if (needs_ir(asm_ctx)) { ir_queue_rr(asm_ctx, JIT_OP_IMUL_RR, (int32_t)dst, (int32_t)src); return; }
    int dst_reg = (int)dst;
    int src_reg = (int)src;
    
    // IMUL r64, r64: REX.W + 0F AF /r
    emit_rex(asm_ctx->buffer, true, dst_reg, src_reg);
    aria_asm_emit_byte(asm_ctx->buffer, 0x0F);
    aria_asm_emit_byte(asm_ctx->buffer, 0xAF);
    emit_modrm(asm_ctx->buffer, 0x03, dst_reg, src_reg);
}

void aria_asm_ret(Assembler* asm_ctx) {
    if (needs_ir(asm_ctx)) { ir_queue_r(asm_ctx, JIT_OP_RET, 0); return; }
    // RET: C3
    aria_asm_emit_byte(asm_ctx->buffer, 0xC3);
}

void aria_asm_push_r64(Assembler* asm_ctx, AsmRegister reg) {
    if (needs_ir(asm_ctx)) { ir_queue_r(asm_ctx, JIT_OP_PUSH, (int32_t)reg); return; }
    int reg_num = (int)reg;
    
    // PUSH r64: 50+rd (or REX.B + 50+rd for R8-R15)
    if (reg_num >= 8) {
        emit_rex(asm_ctx->buffer, false, 0, reg_num);
    }
    aria_asm_emit_byte(asm_ctx->buffer, 0x50 + (reg_num & 0x07));
}

void aria_asm_pop_r64(Assembler* asm_ctx, AsmRegister reg) {
    if (needs_ir(asm_ctx)) { ir_queue_r(asm_ctx, JIT_OP_POP, (int32_t)reg); return; }
    int reg_num = (int)reg;
    
    // POP r64: 58+rd (or REX.B + 58+rd for R8-R15)
    if (reg_num >= 8) {
        emit_rex(asm_ctx->buffer, false, 0, reg_num);
    }
    aria_asm_emit_byte(asm_ctx->buffer, 0x58 + (reg_num & 0x07));
}

void aria_asm_jmp(Assembler* asm_ctx, int label_id) {
    if (needs_ir(asm_ctx)) { ir_queue_label(asm_ctx, JIT_OP_JMP, label_id); return; }
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
    if (needs_ir(asm_ctx)) { ir_queue_label(asm_ctx, JIT_OP_JE, label_id); return; }
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
    if (needs_ir(asm_ctx)) { ir_queue_label(asm_ctx, JIT_OP_JNE, label_id); return; }
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
    if (needs_ir(asm_ctx)) { ir_queue_rr(asm_ctx, JIT_OP_CMP_RR, (int32_t)left, (int32_t)right); return; }
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
    if (needs_ir(asm_ctx)) { ir_queue_ri32(asm_ctx, JIT_OP_ADD_IMM32, (int32_t)dst, value); return; }
    int reg = (int)dst;
    // ADD r64, imm32: REX.W + 81 /0 id
    emit_rex(asm_ctx->buffer, true, 0, reg);
    aria_asm_emit_byte(asm_ctx->buffer, 0x81);
    emit_modrm(asm_ctx->buffer, 0x03, 0, reg);  // /0
    aria_asm_emit_i32(asm_ctx->buffer, value);
}

void aria_asm_sub_r64_imm32(Assembler* asm_ctx, AsmRegister dst, int32_t value) {
    if (needs_ir(asm_ctx)) { ir_queue_ri32(asm_ctx, JIT_OP_SUB_IMM32, (int32_t)dst, value); return; }
    int reg = (int)dst;
    // SUB r64, imm32: REX.W + 81 /5 id
    emit_rex(asm_ctx->buffer, true, 0, reg);
    aria_asm_emit_byte(asm_ctx->buffer, 0x81);
    emit_modrm(asm_ctx->buffer, 0x03, 5, reg);  // /5
    aria_asm_emit_i32(asm_ctx->buffer, value);
}

void aria_asm_xor_r64_r64(Assembler* asm_ctx, AsmRegister dst, AsmRegister src) {
    if (needs_ir(asm_ctx)) { ir_queue_rr(asm_ctx, JIT_OP_XOR_RR, (int32_t)dst, (int32_t)src); return; }
    int dst_reg = (int)dst;
    int src_reg = (int)src;
    // XOR r64, r64: REX.W + 31 /r
    emit_rex(asm_ctx->buffer, true, src_reg, dst_reg);
    aria_asm_emit_byte(asm_ctx->buffer, 0x31);
    emit_modrm(asm_ctx->buffer, 0x03, src_reg, dst_reg);
}

void aria_asm_and_r64_r64(Assembler* asm_ctx, AsmRegister dst, AsmRegister src) {
    if (needs_ir(asm_ctx)) { ir_queue_rr(asm_ctx, JIT_OP_AND_RR, (int32_t)dst, (int32_t)src); return; }
    int dst_reg = (int)dst;
    int src_reg = (int)src;
    // AND r64, r64: REX.W + 21 /r
    emit_rex(asm_ctx->buffer, true, src_reg, dst_reg);
    aria_asm_emit_byte(asm_ctx->buffer, 0x21);
    emit_modrm(asm_ctx->buffer, 0x03, src_reg, dst_reg);
}

void aria_asm_or_r64_r64(Assembler* asm_ctx, AsmRegister dst, AsmRegister src) {
    if (needs_ir(asm_ctx)) { ir_queue_rr(asm_ctx, JIT_OP_OR_RR, (int32_t)dst, (int32_t)src); return; }
    int dst_reg = (int)dst;
    int src_reg = (int)src;
    // OR r64, r64: REX.W + 09 /r
    emit_rex(asm_ctx->buffer, true, src_reg, dst_reg);
    aria_asm_emit_byte(asm_ctx->buffer, 0x09);
    emit_modrm(asm_ctx->buffer, 0x03, src_reg, dst_reg);
}

void aria_asm_not_r64(Assembler* asm_ctx, AsmRegister reg) {
    if (needs_ir(asm_ctx)) { ir_queue_r(asm_ctx, JIT_OP_NOT, (int32_t)reg); return; }
    int r = (int)reg;
    // NOT r64: REX.W + F7 /2
    emit_rex(asm_ctx->buffer, true, 0, r);
    aria_asm_emit_byte(asm_ctx->buffer, 0xF7);
    emit_modrm(asm_ctx->buffer, 0x03, 2, r);  // /2
}

void aria_asm_neg_r64(Assembler* asm_ctx, AsmRegister reg) {
    if (needs_ir(asm_ctx)) { ir_queue_r(asm_ctx, JIT_OP_NEG, (int32_t)reg); return; }
    int r = (int)reg;
    // NEG r64: REX.W + F7 /3
    emit_rex(asm_ctx->buffer, true, 0, r);
    aria_asm_emit_byte(asm_ctx->buffer, 0xF7);
    emit_modrm(asm_ctx->buffer, 0x03, 3, r);  // /3
}

void aria_asm_shl_r64_imm8(Assembler* asm_ctx, AsmRegister reg, uint8_t count) {
    if (needs_ir(asm_ctx)) { ir_queue_ri32(asm_ctx, JIT_OP_SHL_IMM8, (int32_t)reg, count); return; }
    int r = (int)reg;
    // SHL r64, imm8: REX.W + C1 /4 ib
    emit_rex(asm_ctx->buffer, true, 0, r);
    aria_asm_emit_byte(asm_ctx->buffer, 0xC1);
    emit_modrm(asm_ctx->buffer, 0x03, 4, r);  // /4
    aria_asm_emit_byte(asm_ctx->buffer, count);
}

void aria_asm_shr_r64_imm8(Assembler* asm_ctx, AsmRegister reg, uint8_t count) {
    if (needs_ir(asm_ctx)) { ir_queue_ri32(asm_ctx, JIT_OP_SHR_IMM8, (int32_t)reg, count); return; }
    int r = (int)reg;
    // SHR r64, imm8: REX.W + C1 /5 ib
    emit_rex(asm_ctx->buffer, true, 0, r);
    aria_asm_emit_byte(asm_ctx->buffer, 0xC1);
    emit_modrm(asm_ctx->buffer, 0x03, 5, r);  // /5
    aria_asm_emit_byte(asm_ctx->buffer, count);
}

void aria_asm_sar_r64_imm8(Assembler* asm_ctx, AsmRegister reg, uint8_t count) {
    if (needs_ir(asm_ctx)) { ir_queue_ri32(asm_ctx, JIT_OP_SAR_IMM8, (int32_t)reg, count); return; }
    int r = (int)reg;
    // SAR r64, imm8: REX.W + C1 /7 ib
    emit_rex(asm_ctx->buffer, true, 0, r);
    aria_asm_emit_byte(asm_ctx->buffer, 0xC1);
    emit_modrm(asm_ctx->buffer, 0x03, 7, r);  // /7
    aria_asm_emit_byte(asm_ctx->buffer, count);
}

void aria_asm_cmp_r64_imm32(Assembler* asm_ctx, AsmRegister reg, int32_t value) {
    if (needs_ir(asm_ctx)) { ir_queue_ri32(asm_ctx, JIT_OP_CMP_IMM32, (int32_t)reg, value); return; }
    int r = (int)reg;
    // CMP r64, imm32: REX.W + 81 /7 id
    emit_rex(asm_ctx->buffer, true, 0, r);
    aria_asm_emit_byte(asm_ctx->buffer, 0x81);
    emit_modrm(asm_ctx->buffer, 0x03, 7, r);  // /7
    aria_asm_emit_i32(asm_ctx->buffer, value);
}

// =============================================================================
// v0.7.4: Instruction Selection — New Encoders
// =============================================================================

void aria_asm_test_r64_r64(Assembler* asm_ctx, AsmRegister r1, AsmRegister r2) {
    int r1_reg = (int)r1;
    int r2_reg = (int)r2;
    // TEST r64, r64: REX.W + 85 /r
    emit_rex(asm_ctx->buffer, true, r2_reg, r1_reg);
    aria_asm_emit_byte(asm_ctx->buffer, 0x85);
    emit_modrm(asm_ctx->buffer, 0x03, r2_reg, r1_reg);
}

void aria_asm_inc_r64(Assembler* asm_ctx, AsmRegister reg) {
    int r = (int)reg;
    // INC r64: REX.W + FF /0
    emit_rex(asm_ctx->buffer, true, 0, r);
    aria_asm_emit_byte(asm_ctx->buffer, 0xFF);
    emit_modrm(asm_ctx->buffer, 0x03, 0, r);
}

void aria_asm_dec_r64(Assembler* asm_ctx, AsmRegister reg) {
    int r = (int)reg;
    // DEC r64: REX.W + FF /1
    emit_rex(asm_ctx->buffer, true, 0, r);
    aria_asm_emit_byte(asm_ctx->buffer, 0xFF);
    emit_modrm(asm_ctx->buffer, 0x03, 1, r);
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

void aria_asm_jl(Assembler* asm_ctx, int label_id)  { if (needs_ir(asm_ctx)) { ir_queue_label(asm_ctx, JIT_OP_JL, label_id); return; } emit_jcc_rel32(asm_ctx, 0x8C, label_id, "JL"); }
void aria_asm_jle(Assembler* asm_ctx, int label_id) { if (needs_ir(asm_ctx)) { ir_queue_label(asm_ctx, JIT_OP_JLE, label_id); return; } emit_jcc_rel32(asm_ctx, 0x8E, label_id, "JLE"); }
void aria_asm_jg(Assembler* asm_ctx, int label_id)  { if (needs_ir(asm_ctx)) { ir_queue_label(asm_ctx, JIT_OP_JG, label_id); return; } emit_jcc_rel32(asm_ctx, 0x8F, label_id, "JG"); }
void aria_asm_jge(Assembler* asm_ctx, int label_id) { if (needs_ir(asm_ctx)) { ir_queue_label(asm_ctx, JIT_OP_JGE, label_id); return; } emit_jcc_rel32(asm_ctx, 0x8D, label_id, "JGE"); }
void aria_asm_jb(Assembler* asm_ctx, int label_id)  { if (needs_ir(asm_ctx)) { ir_queue_label(asm_ctx, JIT_OP_JB, label_id); return; } emit_jcc_rel32(asm_ctx, 0x82, label_id, "JB"); }
void aria_asm_jbe(Assembler* asm_ctx, int label_id) { if (needs_ir(asm_ctx)) { ir_queue_label(asm_ctx, JIT_OP_JBE, label_id); return; } emit_jcc_rel32(asm_ctx, 0x86, label_id, "JBE"); }
void aria_asm_ja(Assembler* asm_ctx, int label_id)  { if (needs_ir(asm_ctx)) { ir_queue_label(asm_ctx, JIT_OP_JA, label_id); return; } emit_jcc_rel32(asm_ctx, 0x87, label_id, "JA"); }
void aria_asm_jae(Assembler* asm_ctx, int label_id) { if (needs_ir(asm_ctx)) { ir_queue_label(asm_ctx, JIT_OP_JAE, label_id); return; } emit_jcc_rel32(asm_ctx, 0x83, label_id, "JAE"); }

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
    if (needs_ir(asm_ctx)) { ir_queue_mem(asm_ctx, JIT_OP_MOV_LOAD, (int32_t)dst, (int32_t)base, offset); return; }
    int dst_reg = (int)dst;
    int base_reg = (int)base;

    // MOV r64, [base + offset]: REX.W + 8B /r
    emit_rex(asm_ctx->buffer, true, dst_reg, base_reg);
    aria_asm_emit_byte(asm_ctx->buffer, 0x8B);
    emit_mem_operand(asm_ctx->buffer, dst_reg, base_reg, offset);
}

void aria_asm_mov_mem_r64(Assembler* asm_ctx, AsmRegister base, int32_t offset, AsmRegister src) {
    if (needs_ir(asm_ctx)) { ir_queue_store(asm_ctx, JIT_OP_MOV_STORE, (int32_t)base, offset, (int32_t)src); return; }
    int src_reg = (int)src;
    int base_reg = (int)base;

    // MOV [base + offset], r64: REX.W + 89 /r
    emit_rex(asm_ctx->buffer, true, src_reg, base_reg);
    aria_asm_emit_byte(asm_ctx->buffer, 0x89);
    emit_mem_operand(asm_ctx->buffer, src_reg, base_reg, offset);
}

void aria_asm_lea_r64_mem(Assembler* asm_ctx, AsmRegister dst, AsmRegister base, int32_t offset) {
    if (needs_ir(asm_ctx)) { ir_queue_mem(asm_ctx, JIT_OP_LEA, (int32_t)dst, (int32_t)base, offset); return; }
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
    if (needs_ir(asm_ctx)) {
        JitIR* insn = ir_append(asm_ctx);
        if (insn) { insn->opcode = JIT_OP_STORE_LOCAL; insn->offset = (int32_t)slot_offset; insn->src2 = (int32_t)src; }
        return;
    }
    // MOV [RBP - slot_offset], src
    aria_asm_mov_mem_r64(asm_ctx, REG_RBP, -(int32_t)slot_offset, src);
}

void aria_asm_load_local(Assembler* asm_ctx, AsmRegister dst, uint32_t slot_offset) {
    if (needs_ir(asm_ctx)) {
        JitIR* insn = ir_append(asm_ctx);
        if (insn) { insn->opcode = JIT_OP_LOAD_LOCAL; insn->dst = (int32_t)dst; insn->offset = (int32_t)slot_offset; }
        return;
    }
    // MOV dst, [RBP - slot_offset]
    aria_asm_mov_r64_mem(asm_ctx, dst, REG_RBP, -(int32_t)slot_offset);
}

// =============================================================================
// CALL Instructions (v0.7.2)
// =============================================================================

void aria_asm_call_r64(Assembler* asm_ctx, AsmRegister target) {
    if (needs_ir(asm_ctx)) { ir_queue_r(asm_ctx, JIT_OP_CALL_REG, (int32_t)target); return; }
    int reg = (int)target;

    // CALL r64: FF /2 (ModR/M mod=11, reg=010)
    if (reg >= 8) {
        emit_rex(asm_ctx->buffer, false, 0, reg);  // REX.B for extended regs
    }
    aria_asm_emit_byte(asm_ctx->buffer, 0xFF);
    emit_modrm(asm_ctx->buffer, 0x03, 2, reg);  // mod=11, /2
}

void aria_asm_call_label(Assembler* asm_ctx, int label_id) {
    if (needs_ir(asm_ctx)) { ir_queue_label(asm_ctx, JIT_OP_CALL_LABEL, label_id); return; }
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
    if (needs_ir(asm_ctx)) { ir_queue_ri64(asm_ctx, JIT_OP_CALL_ABS, 0, (int64_t)(uintptr_t)func_ptr); return; }
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
    if (needs_ir(asm_ctx)) { ir_queue_rr(asm_ctx, JIT_OP_MOVSD_RR, (int32_t)dst, (int32_t)src); return; }
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
    if (needs_ir(asm_ctx)) { ir_queue_mem(asm_ctx, JIT_OP_MOVSD_LOAD, (int32_t)dst, (int32_t)base, offset); return; }
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
    if (needs_ir(asm_ctx)) { ir_queue_store(asm_ctx, JIT_OP_MOVSD_STORE, (int32_t)base, offset, (int32_t)src); return; }
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
    if (needs_ir(asm_ctx)) { ir_queue_rr(asm_ctx, JIT_OP_ADDSD, (int32_t)dst, (int32_t)src); return; }
    emit_sse2_sd_rr(asm_ctx, 0x58, dst, src);
}

void aria_asm_subsd(Assembler* asm_ctx, AsmRegister dst, AsmRegister src) {
    if (needs_ir(asm_ctx)) { ir_queue_rr(asm_ctx, JIT_OP_SUBSD, (int32_t)dst, (int32_t)src); return; }
    emit_sse2_sd_rr(asm_ctx, 0x5C, dst, src);
}

void aria_asm_mulsd(Assembler* asm_ctx, AsmRegister dst, AsmRegister src) {
    if (needs_ir(asm_ctx)) { ir_queue_rr(asm_ctx, JIT_OP_MULSD, (int32_t)dst, (int32_t)src); return; }
    emit_sse2_sd_rr(asm_ctx, 0x59, dst, src);
}

void aria_asm_divsd(Assembler* asm_ctx, AsmRegister dst, AsmRegister src) {
    if (needs_ir(asm_ctx)) { ir_queue_rr(asm_ctx, JIT_OP_DIVSD, (int32_t)dst, (int32_t)src); return; }
    emit_sse2_sd_rr(asm_ctx, 0x5E, dst, src);
}

void aria_asm_ucomisd(Assembler* asm_ctx, AsmRegister left, AsmRegister right) {
    if (needs_ir(asm_ctx)) { ir_queue_rr(asm_ctx, JIT_OP_UCOMISD, (int32_t)left, (int32_t)right); return; }
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
    if (needs_ir(asm_ctx)) { ir_queue_rr(asm_ctx, JIT_OP_MOVAPS_RR, (int32_t)dst, (int32_t)src); return; }
    int d = xmm_num(dst);
    int s = xmm_num(src);
    // MOVAPS xmm, xmm: [REX] 0F 28 /r
    emit_rex_sse(asm_ctx->buffer, d, s);
    aria_asm_emit_byte(asm_ctx->buffer, 0x0F);
    aria_asm_emit_byte(asm_ctx->buffer, 0x28);
    emit_modrm(asm_ctx->buffer, 0x03, d, s);
}

void aria_asm_movaps_xmm_mem(Assembler* asm_ctx, AsmRegister dst, AsmRegister base, int32_t offset) {
    if (needs_ir(asm_ctx)) { ir_queue_mem(asm_ctx, JIT_OP_MOVAPS_LOAD, (int32_t)dst, (int32_t)base, offset); return; }
    int d = xmm_num(dst);
    int b = (int)base;
    // MOVAPS xmm, [mem]: [REX] 0F 28 /r mem
    emit_rex_sse_mem(asm_ctx->buffer, d, b);
    aria_asm_emit_byte(asm_ctx->buffer, 0x0F);
    aria_asm_emit_byte(asm_ctx->buffer, 0x28);
    emit_mem_operand(asm_ctx->buffer, d, b, offset);
}

void aria_asm_movaps_mem_xmm(Assembler* asm_ctx, AsmRegister base, int32_t offset, AsmRegister src) {
    if (needs_ir(asm_ctx)) { ir_queue_store(asm_ctx, JIT_OP_MOVAPS_STORE, (int32_t)base, offset, (int32_t)src); return; }
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
    if (needs_ir(asm_ctx)) { ir_queue_rr(asm_ctx, JIT_OP_ADDPS, (int32_t)dst, (int32_t)src); return; }
    emit_sse_ps_rr(asm_ctx, 0x58, dst, src);
}

void aria_asm_mulps(Assembler* asm_ctx, AsmRegister dst, AsmRegister src) {
    if (needs_ir(asm_ctx)) { ir_queue_rr(asm_ctx, JIT_OP_MULPS, (int32_t)dst, (int32_t)src); return; }
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
    if (needs_ir(asm_ctx)) { ir_queue_ri64(asm_ctx, JIT_OP_PROLOGUE, 0, (int64_t)stack_size); return; }
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
    if (needs_ir(asm_ctx)) {
        ir_queue_r(asm_ctx, JIT_OP_EPILOGUE, 0);
        ir_queue_r(asm_ctx, JIT_OP_RET, 0);  // Epilogue includes ret semantically
        return;
    }
    // MOV RSP, RBP
    aria_asm_mov_r64_r64(asm_ctx, REG_RSP, REG_RBP);
    
    // POP RBP
    aria_asm_pop_r64(asm_ctx, REG_RBP);
    
    // RET
    aria_asm_ret(asm_ctx);
}

// =============================================================================
// Register Allocator — IR Queueing (v0.7.3)
// =============================================================================

static RegAllocState* ensure_regalloc(Assembler* asm_ctx) {
    if (asm_ctx->regalloc) return asm_ctx->regalloc;

    RegAllocState* ra = (RegAllocState*)calloc(1, sizeof(RegAllocState));
    if (!ra) {
        set_error(asm_ctx, "Failed to allocate RegAllocState");
        return nullptr;
    }

    ra->ir_capacity = 256;  // Initial, grows geometrically
    ra->ir = (JitIR*)malloc(ra->ir_capacity * sizeof(JitIR));
    if (!ra->ir) {
        free(ra);
        set_error(asm_ctx, "Failed to allocate IR buffer");
        return nullptr;
    }
    ra->ir_count = 0;
    ra->next_gpr_vreg = VREG_GPR_BASE;
    ra->next_xmm_vreg = VREG_XMM_BASE;
    ra->has_vregs = false;
    ra->range_count = 0;
    ra->spill_count = 0;
    ra->extra_stack_size = 0;
    asm_ctx->regalloc = ra;
    return ra;
}

static JitIR* ir_append(Assembler* asm_ctx) {
    RegAllocState* ra = ensure_regalloc(asm_ctx);
    if (!ra) return nullptr;

    if (ra->ir_count >= ra->ir_capacity) {
        int32_t new_cap = ra->ir_capacity * 2;
        if (new_cap > MAX_IR_INSNS) {
            set_error(asm_ctx, "IR instruction limit exceeded");
            return nullptr;
        }
        JitIR* new_ir = (JitIR*)realloc(ra->ir, new_cap * sizeof(JitIR));
        if (!new_ir) {
            set_error(asm_ctx, "Failed to grow IR buffer");
            return nullptr;
        }
        ra->ir = new_ir;
        ra->ir_capacity = new_cap;
    }

    JitIR* insn = &ra->ir[ra->ir_count++];
    memset(insn, 0, sizeof(JitIR));
    return insn;
}

// Helper: check if any operand is a vreg, meaning we must use IR mode
static bool needs_ir(Assembler* asm_ctx) {
    return asm_ctx->regalloc && asm_ctx->regalloc->has_vregs;
}

// Queue a 2-operand instruction (dst, src)
static void ir_queue_rr(Assembler* asm_ctx, JitOpcode op, int32_t dst, int32_t src) {
    JitIR* insn = ir_append(asm_ctx);
    if (insn) { insn->opcode = op; insn->dst = dst; insn->src1 = src; }
}

// Queue a 1-operand + imm64
static void ir_queue_ri64(Assembler* asm_ctx, JitOpcode op, int32_t dst, int64_t imm) {
    JitIR* insn = ir_append(asm_ctx);
    if (insn) { insn->opcode = op; insn->dst = dst; insn->imm64 = imm; }
}

// Queue a 1-operand + imm32/offset
static void ir_queue_ri32(Assembler* asm_ctx, JitOpcode op, int32_t dst, int32_t val) {
    JitIR* insn = ir_append(asm_ctx);
    if (insn) { insn->opcode = op; insn->dst = dst; insn->offset = val; }
}

// Queue a 1-operand instruction (dst only)
static void ir_queue_r(Assembler* asm_ctx, JitOpcode op, int32_t dst) {
    JitIR* insn = ir_append(asm_ctx);
    if (insn) { insn->opcode = op; insn->dst = dst; }
}

// Queue a no-operand (or label-only)
static void ir_queue_label(Assembler* asm_ctx, JitOpcode op, int32_t label_id) {
    JitIR* insn = ir_append(asm_ctx);
    if (insn) { insn->opcode = op; insn->offset = label_id; }
}

// Queue memory operation: dst/src with base+offset
static void ir_queue_mem(Assembler* asm_ctx, JitOpcode op, int32_t reg, int32_t base, int32_t offset) {
    JitIR* insn = ir_append(asm_ctx);
    if (insn) { insn->opcode = op; insn->dst = reg; insn->src1 = base; insn->offset = offset; }
}

// Queue store: [base + offset] = src
static void ir_queue_store(Assembler* asm_ctx, JitOpcode op, int32_t base, int32_t offset, int32_t src) {
    JitIR* insn = ir_append(asm_ctx);
    if (insn) { insn->opcode = op; insn->dst = base; insn->offset = offset; insn->src2 = src; }
}

// =============================================================================
// Virtual Register API (v0.7.3)
// =============================================================================

int aria_asm_vreg_new_gpr(Assembler* asm_ctx) {
    RegAllocState* ra = ensure_regalloc(asm_ctx);
    if (!ra) return -1;

    if (ra->next_gpr_vreg >= VREG_XMM_BASE) {
        set_error(asm_ctx, "Too many virtual GPR registers");
        return -1;
    }

    ra->has_vregs = true;
    return ra->next_gpr_vreg++;
}

int aria_asm_vreg_new_xmm(Assembler* asm_ctx) {
    RegAllocState* ra = ensure_regalloc(asm_ctx);
    if (!ra) return -1;

    if (ra->next_xmm_vreg >= VREG_MAX) {
        set_error(asm_ctx, "Too many virtual XMM registers");
        return -1;
    }

    ra->has_vregs = true;
    return ra->next_xmm_vreg++;
}

int aria_asm_vreg_count(const Assembler* asm_ctx) {
    if (!asm_ctx->regalloc) return 0;
    return (asm_ctx->regalloc->next_gpr_vreg - VREG_GPR_BASE) +
           (asm_ctx->regalloc->next_xmm_vreg - VREG_XMM_BASE);
}

int aria_asm_spill_count(const Assembler* asm_ctx) {
    if (!asm_ctx->regalloc) return 0;
    return asm_ctx->regalloc->spill_count;
}

// =============================================================================
// Peephole Optimizer (v0.7.4)
// =============================================================================

// Check if a value is a power of 2 and return the exponent, or -1
static int32_t log2_if_power_of_2(int64_t val) {
    if (val <= 0 || (val & (val - 1)) != 0) return -1;
    int32_t exp = 0;
    while (val > 1) { val >>= 1; exp++; }
    return exp;
}

// Run peephole optimizations on the IR buffer in-place.
// Replaces suboptimal patterns with better ones; dead instructions become JIT_OP_NOP.
static int32_t peephole_optimize(RegAllocState* ra) {
    if (!ra || ra->ir_count == 0) return 0;
    int32_t eliminated = 0;
    JitIR* ir = ra->ir;
    int32_t count = ra->ir_count;

    for (int32_t i = 0; i < count; i++) {
        JitIR* insn = &ir[i];

        switch (insn->opcode) {
            // MOV r, 0 → XOR r, r  (11 bytes → 3-4 bytes)
            case JIT_OP_MOV_IMM64:
                if (insn->imm64 == 0) {
                    insn->opcode = JIT_OP_XOR_RR;
                    insn->src1 = insn->dst;
                    insn->imm64 = 0;
                    eliminated++;
                }
                break;

            // MOV r, r → NOP (identity move)
            case JIT_OP_MOV_RR:
                if (insn->dst == insn->src1) {
                    insn->opcode = JIT_OP_NOP;
                    eliminated++;
                }
                break;

            // ADD r, 0 / SUB r, 0 → NOP
            case JIT_OP_ADD_IMM32:
            case JIT_OP_SUB_IMM32:
                if (insn->offset == 0) {
                    insn->opcode = JIT_OP_NOP;
                    eliminated++;
                }
                break;

            // SHL/SHR/SAR r, 0 → NOP
            case JIT_OP_SHL_IMM8:
            case JIT_OP_SHR_IMM8:
            case JIT_OP_SAR_IMM8:
                if ((insn->offset & 0xFF) == 0) {
                    insn->opcode = JIT_OP_NOP;
                    eliminated++;
                }
                break;

            // IMUL r, imm64 where imm64 is power of 2 → SHL (strength reduction)
            // Only when next instruction is MOV_IMM64 loading the multiplier,
            // so we look for the pattern: MOV_IMM64 vreg, pow2; IMUL dst, vreg
            // This is handled at the two-instruction level below.
            case JIT_OP_IMUL_RR:
                break;

            // Dead move elimination: MOV r, X followed by MOV r, Y → NOP first MOV
            // (only when no intervening uses)
            default:
                break;
        }

        // Two-instruction peephole patterns
        if (i + 1 < count) {
            JitIR* next = &ir[i + 1];

            // MOV r, X; MOV r, Y → NOP; MOV r, Y  (dead store)
            if (insn->opcode == JIT_OP_MOV_IMM64 && next->opcode == JIT_OP_MOV_IMM64 &&
                insn->dst == next->dst) {
                insn->opcode = JIT_OP_NOP;
                eliminated++;
            }

            // MOV r, pow2_imm64; IMUL dst, r → MOV dst, pow2_imm64_as_nop; SHL dst, exp
            // Only safe if 'r' is a vreg not used afterward (conservative: only if r == src1)
            if (insn->opcode == JIT_OP_MOV_IMM64 && next->opcode == JIT_OP_IMUL_RR &&
                next->src1 == insn->dst && IS_VREG(insn->dst)) {
                int32_t exp = log2_if_power_of_2(insn->imm64);
                if (exp > 0) {
                    // Convert IMUL to SHL
                    insn->opcode = JIT_OP_NOP;
                    next->opcode = JIT_OP_SHL_IMM8;
                    next->src1 = 0;
                    next->offset = exp;  // shift amount stored in offset field
                    next->imm64 = 0;
                    eliminated++;
                }
            }

            // XOR r, r followed by ADD r, src → MOV r, src (skip the zero-init)
            if (insn->opcode == JIT_OP_XOR_RR && insn->dst == insn->src1 &&
                next->opcode == JIT_OP_ADD_RR && next->dst == insn->dst) {
                insn->opcode = JIT_OP_NOP;
                next->opcode = JIT_OP_MOV_RR;
                eliminated++;
            }
        }
    }

    // Compact: remove NOPs by shifting remaining instructions down
    int32_t write = 0;
    for (int32_t read = 0; read < count; read++) {
        if (ir[read].opcode != JIT_OP_NOP) {
            if (write != read) ir[write] = ir[read];
            write++;
        }
    }
    ra->ir_count = write;
    ra->peephole_elim = eliminated;
    return eliminated;
}

// =============================================================================
// Register Allocator — Liveness Analysis (v0.7.3)
// =============================================================================

// Allocatable physical GPRs (12 of 16, excluding RSP=4, RBP=5, R10=10, R11=11)
// R10 and R11 are reserved as scratch registers for spill/reload
// Order: caller-saved first to minimize callee-save overhead
static const int ALLOC_GPRS[] = {
    REG_RAX, REG_RCX, REG_RDX,
    REG_RSI, REG_RDI,
    REG_R8, REG_R9,
    REG_RBX, REG_R12, REG_R13, REG_R14, REG_R15
};
static const int NUM_ALLOC_GPRS = 12;

// Allocatable XMM registers (14 of 16, excluding XMM14/XMM15 as scratch)
static const int ALLOC_XMMS[] = {
    64, 65, 66, 67, 68, 69, 70, 71,
    72, 73, 74, 75, 76, 77
};
static const int NUM_ALLOC_XMMS = 14;

// Get or create a live range for a vreg
static LiveRange* find_or_create_range(RegAllocState* ra, int32_t vreg) {
    for (int32_t i = 0; i < ra->range_count; i++) {
        if (ra->ranges[i].vreg == vreg) return &ra->ranges[i];
    }
    if (ra->range_count >= MAX_LIVE_RANGES) return nullptr;

    LiveRange* lr = &ra->ranges[ra->range_count++];
    lr->vreg = vreg;
    lr->start = -1;
    lr->end = -1;
    lr->phys = -1;
    lr->spill_slot = -1;
    lr->reg_class = IS_VREG_XMM(vreg) ? REG_CLASS_XMM : REG_CLASS_GPR;
    return lr;
}

// Update range: set start (first def) and extend end (last use)
static void update_range_def(RegAllocState* ra, int32_t vreg, int32_t insn_idx) {
    if (!IS_VREG(vreg)) return;
    LiveRange* lr = find_or_create_range(ra, vreg);
    if (!lr) return;
    if (lr->start == -1 || insn_idx < lr->start) lr->start = insn_idx;
    if (insn_idx > lr->end) lr->end = insn_idx;
}

static void update_range_use(RegAllocState* ra, int32_t vreg, int32_t insn_idx) {
    if (!IS_VREG(vreg)) return;
    LiveRange* lr = find_or_create_range(ra, vreg);
    if (!lr) return;
    if (lr->start == -1) lr->start = insn_idx;  // Use before def = start here
    if (insn_idx > lr->end) lr->end = insn_idx;
}

static void compute_liveness(RegAllocState* ra) {
    ra->range_count = 0;
    ra->reserved_gprs = 0;
    ra->reserved_xmms = 0;

    // Helper lambda-like inline: mark a physical register as reserved if it's not a vreg
    #define MARK_PHYS_REG(reg) do { \
        if (!IS_VREG(reg) && (reg) >= 0) { \
            if ((reg) < 16) ra->reserved_gprs |= (1u << (reg)); \
            else if ((reg) >= 64 && (reg) < 80) ra->reserved_xmms |= (1u << ((reg) - 64)); \
        } \
    } while(0)

    for (int32_t i = 0; i < ra->ir_count; i++) {
        JitIR* insn = &ra->ir[i];

        switch (insn->opcode) {
            // Instructions that define dst only
            case JIT_OP_MOV_IMM64:
            case JIT_OP_POP:
                MARK_PHYS_REG(insn->dst);
                update_range_def(ra, insn->dst, i);
                break;

            // Instructions that define dst, use src1
            case JIT_OP_MOV_RR:
            case JIT_OP_MOVSD_RR:
            case JIT_OP_MOVAPS_RR:
                MARK_PHYS_REG(insn->dst);
                MARK_PHYS_REG(insn->src1);
                update_range_def(ra, insn->dst, i);
                update_range_use(ra, insn->src1, i);
                break;

            // Instructions that def+use dst, use src1 (dst = dst OP src)
            case JIT_OP_ADD_RR: case JIT_OP_SUB_RR: case JIT_OP_IMUL_RR:
            case JIT_OP_XOR_RR: case JIT_OP_AND_RR: case JIT_OP_OR_RR:
            case JIT_OP_ADDSD: case JIT_OP_SUBSD: case JIT_OP_MULSD: case JIT_OP_DIVSD:
            case JIT_OP_ADDPS: case JIT_OP_MULPS:
                MARK_PHYS_REG(insn->dst);
                MARK_PHYS_REG(insn->src1);
                update_range_def(ra, insn->dst, i);
                update_range_use(ra, insn->dst, i);
                update_range_use(ra, insn->src1, i);
                break;

            // Unary ops: def+use dst
            case JIT_OP_NOT: case JIT_OP_NEG:
            case JIT_OP_SHL_IMM8: case JIT_OP_SHR_IMM8: case JIT_OP_SAR_IMM8:
                MARK_PHYS_REG(insn->dst);
                update_range_def(ra, insn->dst, i);
                update_range_use(ra, insn->dst, i);
                break;

            // Immediate arithmetic: def+use dst
            case JIT_OP_ADD_IMM32: case JIT_OP_SUB_IMM32:
                MARK_PHYS_REG(insn->dst);
                update_range_def(ra, insn->dst, i);
                update_range_use(ra, insn->dst, i);
                break;

            // Compare: use both, no def
            case JIT_OP_CMP_RR: case JIT_OP_UCOMISD:
                MARK_PHYS_REG(insn->dst);
                MARK_PHYS_REG(insn->src1);
                update_range_use(ra, insn->dst, i);
                update_range_use(ra, insn->src1, i);
                break;

            case JIT_OP_CMP_IMM32:
                MARK_PHYS_REG(insn->dst);
                update_range_use(ra, insn->dst, i);
                break;

            // Memory load: def dst, use base
            case JIT_OP_MOV_LOAD: case JIT_OP_LEA:
            case JIT_OP_MOVSD_LOAD: case JIT_OP_MOVAPS_LOAD:
                MARK_PHYS_REG(insn->dst);
                MARK_PHYS_REG(insn->src1);
                update_range_def(ra, insn->dst, i);
                update_range_use(ra, insn->src1, i);
                break;

            case JIT_OP_LOAD_LOCAL:
                MARK_PHYS_REG(insn->dst);
                update_range_def(ra, insn->dst, i);
                break;

            // Memory store: use base + src
            case JIT_OP_MOV_STORE: case JIT_OP_MOVSD_STORE: case JIT_OP_MOVAPS_STORE:
                MARK_PHYS_REG(insn->dst);
                MARK_PHYS_REG(insn->src2);
                update_range_use(ra, insn->dst, i);
                update_range_use(ra, insn->src2, i);
                break;

            case JIT_OP_STORE_LOCAL:
                MARK_PHYS_REG(insn->src2);
                update_range_use(ra, insn->src2, i);
                break;

            // Push: use reg
            case JIT_OP_PUSH:
                MARK_PHYS_REG(insn->dst);
                update_range_use(ra, insn->dst, i);
                break;

            // Call: use target register
            case JIT_OP_CALL_REG:
                MARK_PHYS_REG(insn->dst);
                update_range_use(ra, insn->dst, i);
                break;

            // Everything else (RET, jumps, labels, prologue, epilogue, CALL_LABEL, CALL_ABS): no vreg ops
            default:
                break;
        }
    }

    #undef MARK_PHYS_REG
}

// =============================================================================
// Register Allocator — Linear Scan (v0.7.3)
// =============================================================================

// Sort comparison for live ranges by start position
static int compare_ranges_by_start(const void* a, const void* b) {
    const LiveRange* la = (const LiveRange*)a;
    const LiveRange* lb = (const LiveRange*)b;
    return la->start - lb->start;
}

static void linear_scan_allocate(Assembler* asm_ctx) {
    RegAllocState* ra = asm_ctx->regalloc;
    if (!ra || ra->range_count == 0) return;

    // Sort live ranges by start position
    qsort(ra->ranges, ra->range_count, sizeof(LiveRange), compare_ranges_by_start);

    // Track which physical registers are free
    // GPR availability (1 = free, 0 = in use)
    bool gpr_free[16];
    memset(gpr_free, 1, sizeof(gpr_free));
    gpr_free[REG_RSP] = false;  // Reserved: stack pointer
    gpr_free[REG_RBP] = false;  // Reserved: frame pointer
    gpr_free[REG_R10] = false;  // Reserved: spill scratch 2
    gpr_free[REG_R11] = false;  // Reserved: spill scratch 1

    // Reserve physical GPRs that are used directly in the IR
    for (int k = 0; k < 16; k++) {
        if (ra->reserved_gprs & (1u << k)) gpr_free[k] = false;
    }

    bool xmm_free[16];
    memset(xmm_free, 1, sizeof(xmm_free));
    xmm_free[14] = false;  // Reserved: XMM14 spill scratch 2
    xmm_free[15] = false;  // Reserved: XMM15 spill scratch 1

    // Reserve physical XMMs that are used directly in the IR
    for (int k = 0; k < 16; k++) {
        if (ra->reserved_xmms & (1u << k)) xmm_free[k] = false;
    }

    // Active list: indices into ranges[] of currently active (allocated) ranges
    int32_t active[MAX_LIVE_RANGES];
    int32_t active_count = 0;

    ra->spill_count = 0;

    for (int32_t i = 0; i < ra->range_count; i++) {
        LiveRange* cur = &ra->ranges[i];

        // Expire old intervals: remove from active any that end before cur starts
        int32_t new_active_count = 0;
        for (int32_t j = 0; j < active_count; j++) {
            LiveRange* act = &ra->ranges[active[j]];
            if (act->end < cur->start) {
                // Free the physical register
                if (act->phys >= 0) {
                    if (act->reg_class == REG_CLASS_GPR)
                        gpr_free[act->phys] = true;
                    else
                        xmm_free[act->phys - 64] = true;
                }
            } else {
                active[new_active_count++] = active[j];
            }
        }
        active_count = new_active_count;

        // Try to allocate a physical register
        bool allocated = false;
        if (cur->reg_class == REG_CLASS_GPR) {
            for (int k = 0; k < NUM_ALLOC_GPRS; k++) {
                int preg = ALLOC_GPRS[k];
                if (gpr_free[preg]) {
                    cur->phys = preg;
                    gpr_free[preg] = false;
                    allocated = true;
                    break;
                }
            }
        } else {
            for (int k = 0; k < NUM_ALLOC_XMMS; k++) {
                int preg_idx = k;
                if (xmm_free[preg_idx]) {
                    cur->phys = 64 + preg_idx;  // XMM register as AsmRegister value
                    xmm_free[preg_idx] = false;
                    allocated = true;
                    break;
                }
            }
        }

        if (!allocated) {
            // Spill: find the active range with the farthest end point
            int32_t spill_idx = -1;
            int32_t farthest_end = -1;
            for (int32_t j = 0; j < active_count; j++) {
                LiveRange* act = &ra->ranges[active[j]];
                if (act->reg_class == cur->reg_class && act->end > farthest_end && act->phys >= 0) {
                    farthest_end = act->end;
                    spill_idx = j;
                }
            }

            if (spill_idx >= 0 && ra->ranges[active[spill_idx]].end > cur->end) {
                // Spill the one with farthest end, give its register to cur
                LiveRange* victim = &ra->ranges[active[spill_idx]];
                cur->phys = victim->phys;
                victim->phys = -1;
                victim->spill_slot = (ra->spill_count + 1) * 8;  // 8-byte slots
                ra->spill_count++;
                // Remove victim from active, add cur
                active[spill_idx] = active[--active_count];
            } else {
                // Spill cur itself
                cur->phys = -1;
                cur->spill_slot = (ra->spill_count + 1) * 8;
                ra->spill_count++;
            }
        }

        if (cur->phys >= 0) {
            active[active_count++] = i;
        }
    }

    // Compute extra stack space needed for spills
    ra->extra_stack_size = ra->spill_count * 8;
    // Round up to 16-byte alignment
    if (ra->extra_stack_size % 16 != 0)
        ra->extra_stack_size = (ra->extra_stack_size + 15) & ~15;
}

// =============================================================================
// Register Allocator — IR Emission Pass (v0.7.3)
// =============================================================================

// Resolve a register: if vreg, return assigned physical register.
// If spilled, emit reload first and return the specified scratch register.
// Use different scratch_gpr/scratch_xmm values for different operands
// to avoid conflicts when both operands of an instruction are spilled.
static int32_t resolve_reg_ex(Assembler* asm_ctx, int32_t reg, bool is_def,
                              int32_t scratch_gpr, int32_t scratch_xmm) {
    if (!IS_VREG(reg)) return reg;  // Already physical

    RegAllocState* ra = asm_ctx->regalloc;
    for (int32_t i = 0; i < ra->range_count; i++) {
        if (ra->ranges[i].vreg == reg) {
            if (ra->ranges[i].phys >= 0) {
                return ra->ranges[i].phys;
            }
            // Spilled: use scratch register and emit reload/spill
            int32_t slot = ra->ranges[i].spill_slot;
            int32_t scratch;
            if (ra->ranges[i].reg_class == REG_CLASS_XMM) {
                scratch = scratch_xmm;
                if (!is_def) {
                    aria_asm_movsd_xmm_mem(asm_ctx, (AsmRegister)scratch, REG_RBP, -slot);
                }
            } else {
                scratch = scratch_gpr;
                if (!is_def) {
                    aria_asm_mov_r64_mem(asm_ctx, (AsmRegister)scratch, REG_RBP, -slot);
                }
            }
            return scratch;
        }
    }
    return reg;  // Not found (shouldn't happen)
}

// Default resolve: uses primary scratch registers (R11/XMM15)
static int32_t resolve_reg(Assembler* asm_ctx, int32_t reg, bool is_def) {
    return resolve_reg_ex(asm_ctx, reg, is_def, REG_R11, REG_XMM15);
}

// Alternate resolve: uses secondary scratch registers (R10/XMM14)
static int32_t resolve_reg2(Assembler* asm_ctx, int32_t reg, bool is_def) {
    return resolve_reg_ex(asm_ctx, reg, is_def, REG_R10, REG_XMM14);
}

// After a defining instruction, if the dst was spilled, store it
static void spill_if_needed(Assembler* asm_ctx, int32_t orig_vreg, int32_t resolved_reg) {
    if (!IS_VREG(orig_vreg)) return;

    RegAllocState* ra = asm_ctx->regalloc;
    for (int32_t i = 0; i < ra->range_count; i++) {
        if (ra->ranges[i].vreg == orig_vreg && ra->ranges[i].phys < 0) {
            int32_t slot = ra->ranges[i].spill_slot;
            if (ra->ranges[i].reg_class == REG_CLASS_XMM) {
                aria_asm_movsd_mem_xmm(asm_ctx, REG_RBP, -slot, (AsmRegister)resolved_reg);
            } else {
                aria_asm_mov_mem_r64(asm_ctx, REG_RBP, -slot, (AsmRegister)resolved_reg);
            }
            break;
        }
    }
}

// Emit the entire IR queue using physical registers
static void emit_ir_to_machine_code(Assembler* asm_ctx) {
    RegAllocState* ra = asm_ctx->regalloc;

    for (int32_t i = 0; i < ra->ir_count; i++) {
        JitIR* insn = &ra->ir[i];

        switch (insn->opcode) {
            case JIT_OP_NOP:
                break;  // skip (peephole artifact)
            case JIT_OP_MOV_IMM64: {
                int32_t d = resolve_reg(asm_ctx, insn->dst, true);
                // v0.7.4 instruction selection: narrow MOVABS to MOV r32, imm32
                // when value fits in unsigned 32-bit (zero-extends to 64-bit)
                if (insn->imm64 >= 0 && insn->imm64 <= (int64_t)0xFFFFFFFF) {
                    if (d >= 8) emit_rex(asm_ctx->buffer, false, 0, d);
                    aria_asm_emit_byte(asm_ctx->buffer, 0xB8 + (d & 0x07));
                    aria_asm_emit_i32(asm_ctx->buffer, (int32_t)insn->imm64);
                    ra->insn_sel.mov_imm64_narrowed++;
                    ra->insn_sel.total_selected++;
                } else {
                    aria_asm_mov_r64_imm64(asm_ctx, (AsmRegister)d, insn->imm64);
                }
                spill_if_needed(asm_ctx, insn->dst, d);
                break;
            }
            case JIT_OP_MOV_RR: {
                int32_t s = resolve_reg(asm_ctx, insn->src1, false);
                int32_t d = resolve_reg2(asm_ctx, insn->dst, true);
                aria_asm_mov_r64_r64(asm_ctx, (AsmRegister)d, (AsmRegister)s);
                spill_if_needed(asm_ctx, insn->dst, d);
                break;
            }
            case JIT_OP_ADD_RR: {
                int32_t s = resolve_reg(asm_ctx, insn->src1, false);
                int32_t d = resolve_reg2(asm_ctx, insn->dst, false);  // read-modify-write
                aria_asm_add_r64_r64(asm_ctx, (AsmRegister)d, (AsmRegister)s);
                spill_if_needed(asm_ctx, insn->dst, d);
                break;
            }
            case JIT_OP_SUB_RR: {
                int32_t s = resolve_reg(asm_ctx, insn->src1, false);
                int32_t d = resolve_reg2(asm_ctx, insn->dst, false);
                aria_asm_sub_r64_r64(asm_ctx, (AsmRegister)d, (AsmRegister)s);
                spill_if_needed(asm_ctx, insn->dst, d);
                break;
            }
            case JIT_OP_IMUL_RR: {
                int32_t s = resolve_reg(asm_ctx, insn->src1, false);
                int32_t d = resolve_reg2(asm_ctx, insn->dst, false);
                aria_asm_imul_r64_r64(asm_ctx, (AsmRegister)d, (AsmRegister)s);
                spill_if_needed(asm_ctx, insn->dst, d);
                break;
            }
            case JIT_OP_XOR_RR: {
                int32_t s = resolve_reg(asm_ctx, insn->src1, false);
                int32_t d = resolve_reg2(asm_ctx, insn->dst, false);
                aria_asm_xor_r64_r64(asm_ctx, (AsmRegister)d, (AsmRegister)s);
                spill_if_needed(asm_ctx, insn->dst, d);
                break;
            }
            case JIT_OP_AND_RR: {
                int32_t s = resolve_reg(asm_ctx, insn->src1, false);
                int32_t d = resolve_reg2(asm_ctx, insn->dst, false);
                aria_asm_and_r64_r64(asm_ctx, (AsmRegister)d, (AsmRegister)s);
                spill_if_needed(asm_ctx, insn->dst, d);
                break;
            }
            case JIT_OP_OR_RR: {
                int32_t s = resolve_reg(asm_ctx, insn->src1, false);
                int32_t d = resolve_reg2(asm_ctx, insn->dst, false);
                aria_asm_or_r64_r64(asm_ctx, (AsmRegister)d, (AsmRegister)s);
                spill_if_needed(asm_ctx, insn->dst, d);
                break;
            }
            case JIT_OP_NOT: {
                int32_t d = resolve_reg(asm_ctx, insn->dst, false);
                aria_asm_not_r64(asm_ctx, (AsmRegister)d);
                spill_if_needed(asm_ctx, insn->dst, d);
                break;
            }
            case JIT_OP_NEG: {
                int32_t d = resolve_reg(asm_ctx, insn->dst, false);
                aria_asm_neg_r64(asm_ctx, (AsmRegister)d);
                spill_if_needed(asm_ctx, insn->dst, d);
                break;
            }
            case JIT_OP_ADD_IMM32: {
                int32_t d = resolve_reg(asm_ctx, insn->dst, false);
                // v0.7.4 instruction selection: ADD r, 1 → INC; small imm → imm8 form
                if (insn->offset == 1) {
                    aria_asm_inc_r64(asm_ctx, (AsmRegister)d);
                    ra->insn_sel.add_to_inc++;
                    ra->insn_sel.total_selected++;
                } else if (insn->offset == -1) {
                    aria_asm_dec_r64(asm_ctx, (AsmRegister)d);
                    ra->insn_sel.sub_to_dec++;
                    ra->insn_sel.total_selected++;
                } else if (insn->offset >= -128 && insn->offset <= 127) {
                    // ADD r/m64, imm8: REX.W + 83 /0 ib (4 bytes vs 7)
                    emit_rex(asm_ctx->buffer, true, 0, d);
                    aria_asm_emit_byte(asm_ctx->buffer, 0x83);
                    emit_modrm(asm_ctx->buffer, 0x03, 0, d);
                    aria_asm_emit_byte(asm_ctx->buffer, (uint8_t)(insn->offset & 0xFF));
                    ra->insn_sel.imm32_to_imm8++;
                    ra->insn_sel.total_selected++;
                } else {
                    aria_asm_add_r64_imm32(asm_ctx, (AsmRegister)d, insn->offset);
                }
                spill_if_needed(asm_ctx, insn->dst, d);
                break;
            }
            case JIT_OP_SUB_IMM32: {
                int32_t d = resolve_reg(asm_ctx, insn->dst, false);
                // v0.7.4 instruction selection: SUB r, 1 → DEC; small imm → imm8 form
                if (insn->offset == 1) {
                    aria_asm_dec_r64(asm_ctx, (AsmRegister)d);
                    ra->insn_sel.sub_to_dec++;
                    ra->insn_sel.total_selected++;
                } else if (insn->offset == -1) {
                    aria_asm_inc_r64(asm_ctx, (AsmRegister)d);
                    ra->insn_sel.add_to_inc++;
                    ra->insn_sel.total_selected++;
                } else if (insn->offset >= -128 && insn->offset <= 127) {
                    // SUB r/m64, imm8: REX.W + 83 /5 ib (4 bytes vs 7)
                    emit_rex(asm_ctx->buffer, true, 0, d);
                    aria_asm_emit_byte(asm_ctx->buffer, 0x83);
                    emit_modrm(asm_ctx->buffer, 0x03, 5, d);
                    aria_asm_emit_byte(asm_ctx->buffer, (uint8_t)(insn->offset & 0xFF));
                    ra->insn_sel.imm32_to_imm8++;
                    ra->insn_sel.total_selected++;
                } else {
                    aria_asm_sub_r64_imm32(asm_ctx, (AsmRegister)d, insn->offset);
                }
                spill_if_needed(asm_ctx, insn->dst, d);
                break;
            }
            case JIT_OP_CMP_IMM32: {
                int32_t d = resolve_reg(asm_ctx, insn->dst, false);
                // v0.7.4 instruction selection: CMP r, 0 → TEST r, r
                if (insn->offset == 0) {
                    aria_asm_test_r64_r64(asm_ctx, (AsmRegister)d, (AsmRegister)d);
                    ra->insn_sel.cmp_zero_to_test++;
                    ra->insn_sel.total_selected++;
                } else {
                    aria_asm_cmp_r64_imm32(asm_ctx, (AsmRegister)d, insn->offset);
                }
                break;
            }
            case JIT_OP_SHL_IMM8: {
                int32_t d = resolve_reg(asm_ctx, insn->dst, false);
                aria_asm_shl_r64_imm8(asm_ctx, (AsmRegister)d, (uint8_t)insn->offset);
                spill_if_needed(asm_ctx, insn->dst, d);
                break;
            }
            case JIT_OP_SHR_IMM8: {
                int32_t d = resolve_reg(asm_ctx, insn->dst, false);
                aria_asm_shr_r64_imm8(asm_ctx, (AsmRegister)d, (uint8_t)insn->offset);
                spill_if_needed(asm_ctx, insn->dst, d);
                break;
            }
            case JIT_OP_SAR_IMM8: {
                int32_t d = resolve_reg(asm_ctx, insn->dst, false);
                aria_asm_sar_r64_imm8(asm_ctx, (AsmRegister)d, (uint8_t)insn->offset);
                spill_if_needed(asm_ctx, insn->dst, d);
                break;
            }
            case JIT_OP_CMP_RR: {
                int32_t l = resolve_reg(asm_ctx, insn->dst, false);
                int32_t r = resolve_reg2(asm_ctx, insn->src1, false);
                aria_asm_cmp_r64_r64(asm_ctx, (AsmRegister)l, (AsmRegister)r);
                break;
            }
            case JIT_OP_MOV_LOAD: {
                int32_t b = resolve_reg(asm_ctx, insn->src1, false);
                int32_t d = resolve_reg2(asm_ctx, insn->dst, true);
                aria_asm_mov_r64_mem(asm_ctx, (AsmRegister)d, (AsmRegister)b, insn->offset);
                spill_if_needed(asm_ctx, insn->dst, d);
                break;
            }
            case JIT_OP_MOV_STORE: {
                int32_t b = resolve_reg(asm_ctx, insn->dst, false);
                int32_t s = resolve_reg2(asm_ctx, insn->src2, false);
                aria_asm_mov_mem_r64(asm_ctx, (AsmRegister)b, insn->offset, (AsmRegister)s);
                break;
            }
            case JIT_OP_LEA: {
                int32_t b = resolve_reg(asm_ctx, insn->src1, false);
                int32_t d = resolve_reg2(asm_ctx, insn->dst, true);
                aria_asm_lea_r64_mem(asm_ctx, (AsmRegister)d, (AsmRegister)b, insn->offset);
                spill_if_needed(asm_ctx, insn->dst, d);
                break;
            }
            case JIT_OP_STORE_LOCAL: {
                int32_t s = resolve_reg(asm_ctx, insn->src2, false);
                aria_asm_store_local(asm_ctx, (uint32_t)insn->offset, (AsmRegister)s);
                break;
            }
            case JIT_OP_LOAD_LOCAL: {
                int32_t d = resolve_reg(asm_ctx, insn->dst, true);
                aria_asm_load_local(asm_ctx, (AsmRegister)d, (uint32_t)insn->offset);
                spill_if_needed(asm_ctx, insn->dst, d);
                break;
            }
            case JIT_OP_PUSH: {
                int32_t r = resolve_reg(asm_ctx, insn->dst, false);
                aria_asm_push_r64(asm_ctx, (AsmRegister)r);
                break;
            }
            case JIT_OP_POP: {
                int32_t r = resolve_reg(asm_ctx, insn->dst, true);
                aria_asm_pop_r64(asm_ctx, (AsmRegister)r);
                spill_if_needed(asm_ctx, insn->dst, r);
                break;
            }
            case JIT_OP_RET:
                if (ra->auto_frame) {
                    // Frame teardown: mov rsp, rbp; pop rbp (no ret yet)
                    aria_asm_mov_r64_r64(asm_ctx, REG_RSP, REG_RBP);
                    aria_asm_pop_r64(asm_ctx, REG_RBP);
                    // Restore callee-saved registers (reverse order)
                    for (int c = ra->csr_save_count - 1; c >= 0; c--) {
                        aria_asm_pop_r64(asm_ctx, (AsmRegister)ra->csr_save_regs[c]);
                    }
                }
                aria_asm_ret(asm_ctx);
                break;
            case JIT_OP_JMP:
                aria_asm_jmp(asm_ctx, insn->offset);
                break;
            case JIT_OP_JE:  aria_asm_je(asm_ctx, insn->offset); break;
            case JIT_OP_JNE: aria_asm_jne(asm_ctx, insn->offset); break;
            case JIT_OP_JL:  aria_asm_jl(asm_ctx, insn->offset); break;
            case JIT_OP_JLE: aria_asm_jle(asm_ctx, insn->offset); break;
            case JIT_OP_JG:  aria_asm_jg(asm_ctx, insn->offset); break;
            case JIT_OP_JGE: aria_asm_jge(asm_ctx, insn->offset); break;
            case JIT_OP_JB:  aria_asm_jb(asm_ctx, insn->offset); break;
            case JIT_OP_JBE: aria_asm_jbe(asm_ctx, insn->offset); break;
            case JIT_OP_JA:  aria_asm_ja(asm_ctx, insn->offset); break;
            case JIT_OP_JAE: aria_asm_jae(asm_ctx, insn->offset); break;
            case JIT_OP_LABEL:
                aria_asm_bind_label(asm_ctx, insn->offset);
                break;
            case JIT_OP_CALL_REG: {
                int32_t r = resolve_reg(asm_ctx, insn->dst, false);
                aria_asm_call_r64(asm_ctx, (AsmRegister)r);
                break;
            }
            case JIT_OP_CALL_LABEL:
                aria_asm_call_label(asm_ctx, insn->offset);
                break;
            case JIT_OP_CALL_ABS:
                aria_asm_call_abs(asm_ctx, (void*)(uintptr_t)insn->imm64);
                break;
            case JIT_OP_PROLOGUE: {
                // Save callee-saved regs before frame (user-provided prologue path)
                if (!ra->auto_frame) {
                    for (uint8_t c = 0; c < ra->csr_save_count; c++) {
                        aria_asm_push_r64(asm_ctx, (AsmRegister)ra->csr_save_regs[c]);
                    }
                }
                // Frame setup: push rbp; mov rbp, rsp; sub rsp, N
                size_t total_stack = (size_t)insn->imm64 + ra->extra_stack_size;
                aria_asm_prologue(asm_ctx, total_stack);
                break;
            }
            case JIT_OP_EPILOGUE:
                // Frame teardown: mov rsp, rbp; pop rbp (NO ret — that's JIT_OP_RET's job)
                aria_asm_mov_r64_r64(asm_ctx, REG_RSP, REG_RBP);
                aria_asm_pop_r64(asm_ctx, REG_RBP);
                // Restore callee-saved regs (user-provided epilogue path)
                if (!ra->auto_frame) {
                    for (int c = ra->csr_save_count - 1; c >= 0; c--) {
                        aria_asm_pop_r64(asm_ctx, (AsmRegister)ra->csr_save_regs[c]);
                    }
                }
                break;
            // SSE2 scalar double
            case JIT_OP_MOVSD_RR: {
                int32_t s = resolve_reg(asm_ctx, insn->src1, false);
                int32_t d = resolve_reg2(asm_ctx, insn->dst, true);
                aria_asm_movsd_xmm_xmm(asm_ctx, (AsmRegister)d, (AsmRegister)s);
                spill_if_needed(asm_ctx, insn->dst, d);
                break;
            }
            case JIT_OP_MOVSD_LOAD: {
                int32_t b = resolve_reg(asm_ctx, insn->src1, false);
                int32_t d = resolve_reg2(asm_ctx, insn->dst, true);
                aria_asm_movsd_xmm_mem(asm_ctx, (AsmRegister)d, (AsmRegister)b, insn->offset);
                spill_if_needed(asm_ctx, insn->dst, d);
                break;
            }
            case JIT_OP_MOVSD_STORE: {
                int32_t b = resolve_reg(asm_ctx, insn->dst, false);
                int32_t s = resolve_reg2(asm_ctx, insn->src2, false);
                aria_asm_movsd_mem_xmm(asm_ctx, (AsmRegister)b, insn->offset, (AsmRegister)s);
                break;
            }
            case JIT_OP_ADDSD: {
                int32_t s = resolve_reg(asm_ctx, insn->src1, false);
                int32_t d = resolve_reg2(asm_ctx, insn->dst, false);
                aria_asm_addsd(asm_ctx, (AsmRegister)d, (AsmRegister)s);
                spill_if_needed(asm_ctx, insn->dst, d);
                break;
            }
            case JIT_OP_SUBSD: {
                int32_t s = resolve_reg(asm_ctx, insn->src1, false);
                int32_t d = resolve_reg2(asm_ctx, insn->dst, false);
                aria_asm_subsd(asm_ctx, (AsmRegister)d, (AsmRegister)s);
                spill_if_needed(asm_ctx, insn->dst, d);
                break;
            }
            case JIT_OP_MULSD: {
                int32_t s = resolve_reg(asm_ctx, insn->src1, false);
                int32_t d = resolve_reg2(asm_ctx, insn->dst, false);
                aria_asm_mulsd(asm_ctx, (AsmRegister)d, (AsmRegister)s);
                spill_if_needed(asm_ctx, insn->dst, d);
                break;
            }
            case JIT_OP_DIVSD: {
                int32_t s = resolve_reg(asm_ctx, insn->src1, false);
                int32_t d = resolve_reg2(asm_ctx, insn->dst, false);
                aria_asm_divsd(asm_ctx, (AsmRegister)d, (AsmRegister)s);
                spill_if_needed(asm_ctx, insn->dst, d);
                break;
            }
            case JIT_OP_UCOMISD: {
                int32_t l = resolve_reg(asm_ctx, insn->dst, false);
                int32_t r = resolve_reg2(asm_ctx, insn->src1, false);
                aria_asm_ucomisd(asm_ctx, (AsmRegister)l, (AsmRegister)r);
                break;
            }
            // SSE packed
            case JIT_OP_MOVAPS_RR: {
                int32_t s = resolve_reg(asm_ctx, insn->src1, false);
                int32_t d = resolve_reg2(asm_ctx, insn->dst, true);
                aria_asm_movaps_xmm_xmm(asm_ctx, (AsmRegister)d, (AsmRegister)s);
                spill_if_needed(asm_ctx, insn->dst, d);
                break;
            }
            case JIT_OP_MOVAPS_LOAD: {
                int32_t b = resolve_reg(asm_ctx, insn->src1, false);
                int32_t d = resolve_reg2(asm_ctx, insn->dst, true);
                aria_asm_movaps_xmm_mem(asm_ctx, (AsmRegister)d, (AsmRegister)b, insn->offset);
                spill_if_needed(asm_ctx, insn->dst, d);
                break;
            }
            case JIT_OP_MOVAPS_STORE: {
                int32_t b = resolve_reg(asm_ctx, insn->dst, false);
                int32_t s = resolve_reg2(asm_ctx, insn->src2, false);
                aria_asm_movaps_mem_xmm(asm_ctx, (AsmRegister)b, insn->offset, (AsmRegister)s);
                break;
            }
            case JIT_OP_ADDPS: {
                int32_t s = resolve_reg(asm_ctx, insn->src1, false);
                int32_t d = resolve_reg2(asm_ctx, insn->dst, false);
                aria_asm_addps(asm_ctx, (AsmRegister)d, (AsmRegister)s);
                spill_if_needed(asm_ctx, insn->dst, d);
                break;
            }
            case JIT_OP_MULPS: {
                int32_t s = resolve_reg(asm_ctx, insn->src1, false);
                int32_t d = resolve_reg2(asm_ctx, insn->dst, false);
                aria_asm_mulps(asm_ctx, (AsmRegister)d, (AsmRegister)s);
                spill_if_needed(asm_ctx, insn->dst, d);
                break;
            }
        }
    }
}

// Callee-saved GPRs per SysV AMD64 ABI
static const int32_t CSR_REGS[] = {REG_RBX, REG_R12, REG_R13, REG_R14, REG_R15};
static const int CSR_COUNT = 5;

// Run the full register allocation pipeline
static void run_regalloc_pipeline(Assembler* asm_ctx) {
    RegAllocState* ra = asm_ctx->regalloc;
    if (!ra || !ra->has_vregs || ra->ir_count == 0) return;

    // Step 0 (v0.7.4): Peephole optimizer — runs before liveness analysis
    peephole_optimize(ra);

    // Step 1: Compute live ranges + detect physical reg usage
    compute_liveness(ra);

    // Step 2: Run linear scan allocation
    linear_scan_allocate(asm_ctx);

    // Step 3: Determine which callee-saved registers were assigned to vregs
    ra->csr_save_count = 0;
    for (int32_t i = 0; i < ra->range_count; i++) {
        if (ra->ranges[i].phys < 0) continue;
        for (int c = 0; c < CSR_COUNT; c++) {
            if (ra->ranges[i].phys == CSR_REGS[c]) {
                // Check not already in the list
                bool dup = false;
                for (uint8_t d = 0; d < ra->csr_save_count; d++) {
                    if (ra->csr_save_regs[d] == CSR_REGS[c]) { dup = true; break; }
                }
                if (!dup && ra->csr_save_count < 5) {
                    ra->csr_save_regs[ra->csr_save_count++] = CSR_REGS[c];
                }
            }
        }
    }

    // Step 4: Check if user provided prologue/epilogue in IR
    bool has_prologue = false;
    for (int32_t i = 0; i < ra->ir_count; i++) {
        if (ra->ir[i].opcode == JIT_OP_PROLOGUE) { has_prologue = true; break; }
    }

    // Auto-frame needed if (spills > 0 or callee-saved regs used) and no user prologue
    ra->auto_frame = !has_prologue && (ra->spill_count > 0 || ra->csr_save_count > 0);

    // Step 5: Emit machine code from IR with resolved registers
    // Temporarily disable IR mode so emission goes to buffer directly
    bool saved_has_vregs = ra->has_vregs;
    ra->has_vregs = false;

    if (ra->auto_frame) {
        // Push callee-saved registers BEFORE frame setup
        for (uint8_t c = 0; c < ra->csr_save_count; c++) {
            aria_asm_push_r64(asm_ctx, (AsmRegister)ra->csr_save_regs[c]);
        }
        // Frame setup: push rbp; mov rbp, rsp; sub rsp, extra_stack_size
        aria_asm_prologue(asm_ctx, ra->extra_stack_size);
    }

    emit_ir_to_machine_code(asm_ctx);

    ra->has_vregs = saved_has_vregs;
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

    // v0.7.3: Run register allocator if virtual registers were used
    if (asm_ctx->regalloc && asm_ctx->regalloc->has_vregs && asm_ctx->regalloc->ir_count > 0) {
        run_regalloc_pipeline(asm_ctx);
        if (asm_ctx->error) {
            return empty;
        }
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

// =============================================================================
// v0.7.4: Peephole Statistics API
// =============================================================================

PeepholeStats aria_asm_peephole_stats(const Assembler* asm_ctx) {
    PeepholeStats stats = {0, 0, 0, 0, 0, 0};
    if (!asm_ctx || !asm_ctx->regalloc) return stats;
    stats.total_eliminated = asm_ctx->regalloc->peephole_elim;
    return stats;
}

InsnSelStats aria_asm_insn_sel_stats(const Assembler* asm_ctx) {
    InsnSelStats stats = {0, 0, 0, 0, 0, 0};
    if (!asm_ctx || !asm_ctx->regalloc) return stats;
    return asm_ctx->regalloc->insn_sel;
}

// =============================================================================
// v0.7.4: Profiling — perf map integration
// =============================================================================

void aria_asm_perf_map_register(const void* code_addr, size_t code_size, const char* name) {
    if (!code_addr || !name || code_size == 0) return;

#if defined(__linux__)
    // Write to /tmp/perf-<pid>.map
    // Format: <hex_addr> <hex_size> <name>\n
    char path[64];
    snprintf(path, sizeof(path), "/tmp/perf-%d.map", (int)getpid());
    FILE* f = fopen(path, "a");
    if (!f) return;
    fprintf(f, "%lx %lx %s\n",
            (unsigned long)(uintptr_t)code_addr,
            (unsigned long)code_size,
            name);
    fclose(f);
#else
    (void)code_addr;
    (void)code_size;
    (void)name;
#endif
}

// =============================================================================
// v0.7.4: Architecture abstraction
// =============================================================================

AsmArch aria_asm_get_arch(void) {
#if defined(__x86_64__) || defined(_M_X64)
    return ASM_ARCH_X86_64;
#elif defined(__aarch64__) || defined(_M_ARM64)
    return ASM_ARCH_AARCH64;
#else
    return ASM_ARCH_X86_64;
#endif
}

bool aria_asm_arch_supported(AsmArch arch) {
    // Currently only x86-64 has a full code generator
    return arch == ASM_ARCH_X86_64;
}
