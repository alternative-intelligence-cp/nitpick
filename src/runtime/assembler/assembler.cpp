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
    // Check for errors
    if (asm_ctx->error) {
        return {nullptr, 0, WILDX_STATE_UNINITIALIZED, false};
    }
    
    // Verify all labels are bound
    for (uint32_t i = 0; i < asm_ctx->label_count; ++i) {
        if (!aria_asm_label_is_bound(&asm_ctx->labels[i])) {
            set_error(asm_ctx, "Unbound label detected at finalization");
            return {nullptr, 0, WILDX_STATE_UNINITIALIZED, false};
        }
    }
    
    // Allocate WildX memory
    WildXGuard guard = aria_alloc_exec(asm_ctx->buffer->size);
    if (!guard.ptr) {
        set_error(asm_ctx, "Failed to allocate WildX memory");
        return {nullptr, 0, WILDX_STATE_UNINITIALIZED, false};
    }
    
    // Copy code to WildX memory
    memcpy(guard.ptr, asm_ctx->buffer->data, asm_ctx->buffer->size);
    
    // Seal memory (RW → RX)
    if (aria_mem_protect_exec(&guard) != 0) {
        aria_free_exec(&guard);
        set_error(asm_ctx, "Failed to seal WildX memory");
        return {nullptr, 0, WILDX_STATE_UNINITIALIZED, false};
    }
    
    return guard;
}

int64_t aria_asm_execute(WildXGuard* guard) {
    if (!guard || !guard->ptr || guard->state != WILDX_STATE_EXECUTABLE) {
        return -1;
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
    
    // Cast to function pointer: int64_t (*)(int64_t)
    typedef int64_t (*func_t)(int64_t);
    func_t func = (func_t)guard->ptr;
    
    return func(arg1);
}

int64_t aria_asm_execute_i64_i64(WildXGuard* guard, int64_t arg1, int64_t arg2) {
    if (!guard || !guard->ptr || guard->state != WILDX_STATE_EXECUTABLE) {
        return -1;
    }
    
    // Cast to function pointer: int64_t (*)(int64_t, int64_t)
    typedef int64_t (*func_t)(int64_t, int64_t);
    func_t func = (func_t)guard->ptr;
    
    return func(arg1, arg2);
}
