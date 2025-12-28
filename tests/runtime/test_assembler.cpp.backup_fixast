/**
 * Tests for Aria Runtime Assembler (ARA)
 * 
 * Tests x86-64 code generation, label backpatching, and JIT execution.
 */

#include "../test_helpers.h"
#include "runtime/assembler.h"

// =============================================================================
// Buffer Management Tests
// =============================================================================

TEST_CASE(asm_buffer_create_destroy) {
    CodeBuffer* buf = aria_asm_buffer_create(1024);
    ASSERT(buf != nullptr, "Buffer creation should succeed");
    ASSERT(buf->capacity >= 1024, "Buffer should have requested capacity");
    ASSERT(buf->size == 0, "Initial size should be zero");
    
    aria_asm_buffer_destroy(buf);
}

TEST_CASE(asm_buffer_emit_byte) {
    CodeBuffer* buf = aria_asm_buffer_create(16);
    ASSERT(buf != nullptr, "Buffer creation should succeed");
    
    aria_asm_emit_byte(buf, 0x90);  // NOP
    ASSERT(buf->size == 1, "Size should be 1");
    ASSERT(buf->data[0] == 0x90, "Byte should be emitted");
    
    aria_asm_buffer_destroy(buf);
}

TEST_CASE(asm_buffer_emit_i32) {
    CodeBuffer* buf = aria_asm_buffer_create(16);
    ASSERT(buf != nullptr, "Buffer creation should succeed");
    
    aria_asm_emit_i32(buf, 0x12345678);
    ASSERT(buf->size == 4, "Size should be 4");
    ASSERT(buf->data[0] == 0x78, "Byte 0 should be LSB (little-endian)");
    ASSERT(buf->data[1] == 0x56, "Byte 1 should match");
    ASSERT(buf->data[2] == 0x34, "Byte 2 should match");
    ASSERT(buf->data[3] == 0x12, "Byte 3 should be MSB");
    
    aria_asm_buffer_destroy(buf);
}

TEST_CASE(asm_buffer_growth) {
    CodeBuffer* buf = aria_asm_buffer_create(4);  // Small initial capacity
    ASSERT(buf != nullptr, "Buffer creation should succeed");
    
    // Fill beyond initial capacity
    for (int i = 0; i < 100; ++i) {
        aria_asm_emit_byte(buf, (uint8_t)i);
    }
    
    ASSERT(buf->size == 100, "All bytes should be stored");
    ASSERT(buf->capacity >= 100, "Buffer should have grown");
    
    // Verify data integrity
    for (int i = 0; i < 100; ++i) {
        ASSERT(buf->data[i] == (uint8_t)i, "Data should be preserved");
    }
    
    aria_asm_buffer_destroy(buf);
}

// =============================================================================
// Label Management Tests
// =============================================================================

TEST_CASE(asm_label_create_unbound) {
    AsmLabel label = aria_asm_label_create();
    ASSERT(!aria_asm_label_is_bound(&label), "New label should be unbound");
    ASSERT(label.num_patches == 0, "No patches initially");
}

TEST_CASE(asm_new_label) {
    Assembler* asm_ctx = aria_asm_create();
    ASSERT(asm_ctx != nullptr, "Assembler creation should succeed");
    
    int label1 = aria_asm_new_label(asm_ctx);
    int label2 = aria_asm_new_label(asm_ctx);
    
    ASSERT(label1 == 0, "First label should be 0");
    ASSERT(label2 == 1, "Second label should be 1");
    ASSERT(asm_ctx->label_count == 2, "Label count should be 2");
    
    aria_asm_destroy(asm_ctx);
}

TEST_CASE(asm_bind_label) {
    Assembler* asm_ctx = aria_asm_create();
    ASSERT(asm_ctx != nullptr, "Assembler creation should succeed");
    
    int label = aria_asm_new_label(asm_ctx);
    
    // Emit some bytes
    aria_asm_emit_byte(asm_ctx->buffer, 0x90);
    aria_asm_emit_byte(asm_ctx->buffer, 0x90);
    
    aria_asm_bind_label(asm_ctx, label);
    
    ASSERT(aria_asm_label_is_bound(&asm_ctx->labels[label]), "Label should be bound");
    ASSERT(asm_ctx->labels[label].position == 2, "Label position should be 2");
    
    aria_asm_destroy(asm_ctx);
}

// =============================================================================
// Instruction Emission Tests
// =============================================================================

TEST_CASE(asm_mov_r64_imm64) {
    Assembler* asm_ctx = aria_asm_create();
    ASSERT(asm_ctx != nullptr, "Assembler creation should succeed");
    
    // MOV RAX, 42
    aria_asm_mov_r64_imm64(asm_ctx, REG_RAX, 42);
    
    // Expected: REX.W + B8+rd id
    // REX.W = 0x48 (64-bit), B8 = MOVABS, 42 as 8 bytes
    ASSERT(asm_ctx->buffer->size == 10, "MOV r64, imm64 should be 10 bytes");
    ASSERT(asm_ctx->buffer->data[0] == 0x48, "REX.W prefix");
    ASSERT(asm_ctx->buffer->data[1] == 0xB8, "MOVABS opcode for RAX");
    ASSERT(asm_ctx->buffer->data[2] == 42, "Immediate value LSB");
    
    aria_asm_destroy(asm_ctx);
}

TEST_CASE(asm_mov_r64_r64) {
    Assembler* asm_ctx = aria_asm_create();
    ASSERT(asm_ctx != nullptr, "Assembler creation should succeed");
    
    // MOV RAX, RBX
    aria_asm_mov_r64_r64(asm_ctx, REG_RAX, REG_RBX);
    
    // Expected: REX.W + 89 /r
    ASSERT(asm_ctx->buffer->size == 3, "MOV r64, r64 should be 3 bytes");
    ASSERT(asm_ctx->buffer->data[0] == 0x48, "REX.W prefix");
    ASSERT(asm_ctx->buffer->data[1] == 0x89, "MOV opcode");
    ASSERT(asm_ctx->buffer->data[2] == 0xD8, "ModR/M: mod=11, reg=RBX(3), rm=RAX(0)");
    
    aria_asm_destroy(asm_ctx);
}

TEST_CASE(asm_add_r64_r64) {
    Assembler* asm_ctx = aria_asm_create();
    ASSERT(asm_ctx != nullptr, "Assembler creation should succeed");
    
    // ADD RAX, RBX
    aria_asm_add_r64_r64(asm_ctx, REG_RAX, REG_RBX);
    
    // Expected: REX.W + 01 /r
    ASSERT(asm_ctx->buffer->size == 3, "ADD r64, r64 should be 3 bytes");
    ASSERT(asm_ctx->buffer->data[0] == 0x48, "REX.W prefix");
    ASSERT(asm_ctx->buffer->data[1] == 0x01, "ADD opcode");
    
    aria_asm_destroy(asm_ctx);
}

TEST_CASE(asm_sub_r64_r64) {
    Assembler* asm_ctx = aria_asm_create();
    ASSERT(asm_ctx != nullptr, "Assembler creation should succeed");
    
    // SUB RAX, RBX
    aria_asm_sub_r64_r64(asm_ctx, REG_RAX, REG_RBX);
    
    // Expected: REX.W + 29 /r
    ASSERT(asm_ctx->buffer->size == 3, "SUB r64, r64 should be 3 bytes");
    ASSERT(asm_ctx->buffer->data[0] == 0x48, "REX.W prefix");
    ASSERT(asm_ctx->buffer->data[1] == 0x29, "SUB opcode");
    
    aria_asm_destroy(asm_ctx);
}

TEST_CASE(asm_ret) {
    Assembler* asm_ctx = aria_asm_create();
    ASSERT(asm_ctx != nullptr, "Assembler creation should succeed");
    
    aria_asm_ret(asm_ctx);
    
    ASSERT(asm_ctx->buffer->size == 1, "RET should be 1 byte");
    ASSERT(asm_ctx->buffer->data[0] == 0xC3, "RET opcode");
    
    aria_asm_destroy(asm_ctx);
}

TEST_CASE(asm_push_pop) {
    Assembler* asm_ctx = aria_asm_create();
    ASSERT(asm_ctx != nullptr, "Assembler creation should succeed");
    
    aria_asm_push_r64(asm_ctx, REG_RBP);
    aria_asm_pop_r64(asm_ctx, REG_RBP);
    
    ASSERT(asm_ctx->buffer->size == 2, "PUSH + POP should be 2 bytes");
    ASSERT(asm_ctx->buffer->data[0] == 0x55, "PUSH RBP opcode");
    ASSERT(asm_ctx->buffer->data[1] == 0x5D, "POP RBP opcode");
    
    aria_asm_destroy(asm_ctx);
}

// =============================================================================
// Jump and Branch Tests
// =============================================================================

TEST_CASE(asm_jmp_backward) {
    Assembler* asm_ctx = aria_asm_create();
    ASSERT(asm_ctx != nullptr, "Assembler creation should succeed");
    
    int label = aria_asm_new_label(asm_ctx);
    aria_asm_bind_label(asm_ctx, label);  // Bind at offset 0
    
    aria_asm_emit_byte(asm_ctx->buffer, 0x90);  // NOP (offset 0)
    aria_asm_jmp(asm_ctx, label);  // Jump back to offset 0
    
    // JMP rel32 = E9 cd (5 bytes total)
    ASSERT(asm_ctx->buffer->size == 6, "NOP + JMP should be 6 bytes");
    ASSERT(asm_ctx->buffer->data[1] == 0xE9, "JMP opcode");
    
    // Offset should be: 0 - (1 + 5) = -6 (0xFFFFFFFA in two's complement)
    int32_t offset = (int32_t)(
        (asm_ctx->buffer->data[2] << 0) |
        (asm_ctx->buffer->data[3] << 8) |
        (asm_ctx->buffer->data[4] << 16) |
        (asm_ctx->buffer->data[5] << 24)
    );
    ASSERT(offset == -6, "Jump offset should be -6");
    
    aria_asm_destroy(asm_ctx);
}

TEST_CASE(asm_jmp_forward_patching) {
    Assembler* asm_ctx = aria_asm_create();
    ASSERT(asm_ctx != nullptr, "Assembler creation should succeed");
    
    int label = aria_asm_new_label(asm_ctx);
    
    aria_asm_jmp(asm_ctx, label);  // Forward jump (unresolved)
    
    // Should emit placeholder (0x00000000)
    ASSERT(asm_ctx->buffer->data[1] == 0x00, "Placeholder byte 0");
    ASSERT(asm_ctx->buffer->data[2] == 0x00, "Placeholder byte 1");
    
    aria_asm_emit_byte(asm_ctx->buffer, 0x90);  // NOP
    aria_asm_bind_label(asm_ctx, label);  // Bind at offset 6
    
    // After binding, offset should be patched to: 6 - (0 + 5) = 1
    int32_t offset = (int32_t)(
        (asm_ctx->buffer->data[1] << 0) |
        (asm_ctx->buffer->data[2] << 8) |
        (asm_ctx->buffer->data[3] << 16) |
        (asm_ctx->buffer->data[4] << 24)
    );
    ASSERT(offset == 1, "Forward jump should be patched to offset 1");
    
    aria_asm_destroy(asm_ctx);
}

TEST_CASE(asm_conditional_jumps) {
    Assembler* asm_ctx = aria_asm_create();
    ASSERT(asm_ctx != nullptr, "Assembler creation should succeed");
    
    int label1 = aria_asm_new_label(asm_ctx);
    int label2 = aria_asm_new_label(asm_ctx);
    
    aria_asm_je(asm_ctx, label1);
    aria_asm_jne(asm_ctx, label2);
    
    // JE = 0F 84 cd (6 bytes), JNE = 0F 85 cd (6 bytes)
    ASSERT(asm_ctx->buffer->size == 12, "Two conditional jumps should be 12 bytes");
    ASSERT(asm_ctx->buffer->data[0] == 0x0F, "JE prefix");
    ASSERT(asm_ctx->buffer->data[1] == 0x84, "JE opcode");
    ASSERT(asm_ctx->buffer->data[6] == 0x0F, "JNE prefix");
    ASSERT(asm_ctx->buffer->data[7] == 0x85, "JNE opcode");
    
    aria_asm_destroy(asm_ctx);
}

// =============================================================================
// JIT Execution Tests
// =============================================================================

TEST_CASE(jit_return_constant) {
    // Generate function: int64_t func() { return 42; }
    Assembler* asm_ctx = aria_asm_create();
    ASSERT(asm_ctx != nullptr, "Assembler creation should succeed");
    
    // MOV RAX, 42
    aria_asm_mov_r64_imm64(asm_ctx, REG_RAX, 42);
    
    // RET
    aria_asm_ret(asm_ctx);
    
    // Finalize and execute
    WildXGuard guard = aria_asm_finalize(asm_ctx);
    ASSERT(guard.ptr != nullptr, "Finalization should succeed");
    ASSERT(guard.state == WILDX_STATE_EXECUTABLE, "Memory should be executable");
    
    int64_t result = aria_asm_execute(&guard);
    ASSERT(result == 42, "Function should return 42");
    
    aria_free_exec(&guard);
    aria_asm_destroy(asm_ctx);
}

TEST_CASE(jit_identity_function) {
    // Generate function: int64_t func(int64_t x) { return x; }
    // x is passed in RDI (System V ABI), return value in RAX
    Assembler* asm_ctx = aria_asm_create();
    ASSERT(asm_ctx != nullptr, "Assembler creation should succeed");
    
    // MOV RAX, RDI (copy argument to return register)
    aria_asm_mov_r64_r64(asm_ctx, REG_RAX, REG_RDI);
    
    // RET
    aria_asm_ret(asm_ctx);
    
    WildXGuard guard = aria_asm_finalize(asm_ctx);
    ASSERT(guard.ptr != nullptr, "Finalization should succeed");
    
    int64_t result = aria_asm_execute_i64(&guard, 100);
    ASSERT(result == 100, "Identity function should return input");
    
    aria_free_exec(&guard);
    aria_asm_destroy(asm_ctx);
}

TEST_CASE(jit_add_two_numbers) {
    // Generate function: int64_t add(int64_t a, int64_t b) { return a + b; }
    // a in RDI, b in RSI (System V ABI)
    Assembler* asm_ctx = aria_asm_create();
    ASSERT(asm_ctx != nullptr, "Assembler creation should succeed");
    
    // MOV RAX, RDI (copy a to RAX)
    aria_asm_mov_r64_r64(asm_ctx, REG_RAX, REG_RDI);
    
    // ADD RAX, RSI (add b)
    aria_asm_add_r64_r64(asm_ctx, REG_RAX, REG_RSI);
    
    // RET
    aria_asm_ret(asm_ctx);
    
    WildXGuard guard = aria_asm_finalize(asm_ctx);
    ASSERT(guard.ptr != nullptr, "Finalization should succeed");
    
    int64_t result = aria_asm_execute_i64_i64(&guard, 10, 32);
    ASSERT(result == 42, "add(10, 32) should return 42");
    
    aria_free_exec(&guard);
    aria_asm_destroy(asm_ctx);
}

TEST_CASE(jit_subtract_numbers) {
    // Generate function: int64_t sub(int64_t a, int64_t b) { return a - b; }
    Assembler* asm_ctx = aria_asm_create();
    ASSERT(asm_ctx != nullptr, "Assembler creation should succeed");
    
    aria_asm_mov_r64_r64(asm_ctx, REG_RAX, REG_RDI);
    aria_asm_sub_r64_r64(asm_ctx, REG_RAX, REG_RSI);
    aria_asm_ret(asm_ctx);
    
    WildXGuard guard = aria_asm_finalize(asm_ctx);
    ASSERT(guard.ptr != nullptr, "Finalization should succeed");
    
    int64_t result = aria_asm_execute_i64_i64(&guard, 100, 58);
    ASSERT(result == 42, "sub(100, 58) should return 42");
    
    aria_free_exec(&guard);
    aria_asm_destroy(asm_ctx);
}

TEST_CASE(jit_multiply_numbers) {
    // Generate function: int64_t mul(int64_t a, int64_t b) { return a * b; }
    Assembler* asm_ctx = aria_asm_create();
    ASSERT(asm_ctx != nullptr, "Assembler creation should succeed");
    
    aria_asm_mov_r64_r64(asm_ctx, REG_RAX, REG_RDI);
    aria_asm_imul_r64_r64(asm_ctx, REG_RAX, REG_RSI);
    aria_asm_ret(asm_ctx);
    
    WildXGuard guard = aria_asm_finalize(asm_ctx);
    ASSERT(guard.ptr != nullptr, "Finalization should succeed");
    
    int64_t result = aria_asm_execute_i64_i64(&guard, 6, 7);
    ASSERT(result == 42, "mul(6, 7) should return 42");
    
    aria_free_exec(&guard);
    aria_asm_destroy(asm_ctx);
}

TEST_CASE(jit_conditional_branch) {
    // Generate function: int64_t select(int64_t a, int64_t b) 
    // Returns a if a == b, otherwise returns b
    Assembler* asm_ctx = aria_asm_create();
    ASSERT(asm_ctx != nullptr, "Assembler creation should succeed");
    
    int label_return_a = aria_asm_new_label(asm_ctx);
    
    // CMP RDI, RSI (compare a and b)
    aria_asm_cmp_r64_r64(asm_ctx, REG_RDI, REG_RSI);
    
    // JE label_return_a (if a == b, return a)
    aria_asm_je(asm_ctx, label_return_a);
    
    // Return b (RSI) - not equal case
    aria_asm_mov_r64_r64(asm_ctx, REG_RAX, REG_RSI);
    aria_asm_ret(asm_ctx);
    
    // Return a (RDI) - equal case
    aria_asm_bind_label(asm_ctx, label_return_a);
    aria_asm_mov_r64_r64(asm_ctx, REG_RAX, REG_RDI);
    aria_asm_ret(asm_ctx);
    
    WildXGuard guard = aria_asm_finalize(asm_ctx);
    ASSERT(guard.ptr != nullptr, "Finalization should succeed");
    
    // Test equal case: should return first argument
    int64_t result1 = aria_asm_execute_i64_i64(&guard, 42, 42);
    ASSERT(result1 == 42, "select(42, 42) should return 42");
    
    // Test not equal case: should return second argument
    int64_t result2 = aria_asm_execute_i64_i64(&guard, 30, 70);
    ASSERT(result2 == 70, "select(30, 70) should return 70");
    
    aria_free_exec(&guard);
    aria_asm_destroy(asm_ctx);
}

TEST_CASE(jit_prologue_epilogue) {
    // Generate function with standard prologue/epilogue
    Assembler* asm_ctx = aria_asm_create();
    ASSERT(asm_ctx != nullptr, "Assembler creation should succeed");
    
    aria_asm_prologue(asm_ctx, 0);  // No stack variables
    aria_asm_mov_r64_imm64(asm_ctx, REG_RAX, 42);
    aria_asm_epilogue(asm_ctx);
    
    WildXGuard guard = aria_asm_finalize(asm_ctx);
    ASSERT(guard.ptr != nullptr, "Finalization should succeed");
    
    int64_t result = aria_asm_execute(&guard);
    ASSERT(result == 42, "Function should return 42");
    
    aria_free_exec(&guard);
    aria_asm_destroy(asm_ctx);
}

TEST_CASE(jit_error_unbound_label) {
    // Try to finalize with unbound label
    Assembler* asm_ctx = aria_asm_create();
    ASSERT(asm_ctx != nullptr, "Assembler creation should succeed");
    
    int label = aria_asm_new_label(asm_ctx);
    aria_asm_jmp(asm_ctx, label);  // Forward jump, never bound
    aria_asm_ret(asm_ctx);
    
    WildXGuard guard = aria_asm_finalize(asm_ctx);
    ASSERT(guard.ptr == nullptr, "Finalization should fail");
    ASSERT(aria_asm_has_error(asm_ctx), "Should have error");
    
    aria_asm_destroy(asm_ctx);
}
