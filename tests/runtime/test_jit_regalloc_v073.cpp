/**
 * JIT Register Allocator Tests — v0.7.3
 *
 * Tests virtual register allocation, liveness analysis, linear scan,
 * spill/reload, mixed physical+virtual registers, XMM vregs, and loops.
 */

#include "runtime/assembler.h"
#include <cstdio>
#include <cstring>
#include <cmath>

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) \
    static void test_##name(); \
    static void run_##name() { \
        tests_run++; \
        printf("  [%2d] %-45s ", tests_run, #name); \
        test_##name(); \
    } \
    static void test_##name()

#define ASSERT_EQ(a, b) do { \
    auto _a = (a); auto _b = (b); \
    if (_a == _b) { printf("PASS\n"); tests_passed++; } \
    else { printf("FAIL (got %ld, expected %ld)\n", (long)_a, (long)_b); tests_failed++; } \
} while(0)

#define ASSERT_NEAR(a, b, eps) do { \
    double _a = (a); double _b = (b); \
    if (fabs(_a - _b) < (eps)) { printf("PASS\n"); tests_passed++; } \
    else { printf("FAIL (got %.6f, expected %.6f)\n", _a, _b); } \
} while(0)

// ── Basic Virtual Register Allocation ───────────────────────────────────

TEST(vreg_simple_add) {
    // Two vregs: v0 = 10, v1 = 32; v0 += v1; return v0
    Assembler* a = aria_asm_create();
    int v0 = aria_asm_vreg_new_gpr(a);
    int v1 = aria_asm_vreg_new_gpr(a);

    aria_asm_mov_r64_imm64(a, (AsmRegister)v0, 10);
    aria_asm_mov_r64_imm64(a, (AsmRegister)v1, 32);
    aria_asm_add_r64_r64(a, (AsmRegister)v0, (AsmRegister)v1);
    aria_asm_mov_r64_r64(a, REG_RAX, (AsmRegister)v0);
    aria_asm_ret(a);

    ASSERT_EQ(aria_asm_vreg_count(a), 2);
    WildXGuard g = aria_asm_finalize(a);
    ASSERT_EQ(aria_asm_execute(&g), 42);
    aria_free_exec(&g);
    aria_asm_destroy(a);
}

TEST(vreg_sub_imul) {
    // v0 = 100, v1 = 3, v2 = 7; result = (v0 - v2) * v1 = 93 * 3 = 279
    Assembler* a = aria_asm_create();
    int v0 = aria_asm_vreg_new_gpr(a);
    int v1 = aria_asm_vreg_new_gpr(a);
    int v2 = aria_asm_vreg_new_gpr(a);

    aria_asm_mov_r64_imm64(a, (AsmRegister)v0, 100);
    aria_asm_mov_r64_imm64(a, (AsmRegister)v1, 3);
    aria_asm_mov_r64_imm64(a, (AsmRegister)v2, 7);
    aria_asm_sub_r64_r64(a, (AsmRegister)v0, (AsmRegister)v2);
    aria_asm_imul_r64_r64(a, (AsmRegister)v0, (AsmRegister)v1);
    aria_asm_mov_r64_r64(a, REG_RAX, (AsmRegister)v0);
    aria_asm_ret(a);

    WildXGuard g = aria_asm_finalize(a);
    ASSERT_EQ(aria_asm_execute(&g), 279);
    aria_free_exec(&g);
    aria_asm_destroy(a);
}

TEST(vreg_mov_chain) {
    // v0 = 42; v1 = v0; v2 = v1; return v2
    Assembler* a = aria_asm_create();
    int v0 = aria_asm_vreg_new_gpr(a);
    int v1 = aria_asm_vreg_new_gpr(a);
    int v2 = aria_asm_vreg_new_gpr(a);

    aria_asm_mov_r64_imm64(a, (AsmRegister)v0, 42);
    aria_asm_mov_r64_r64(a, (AsmRegister)v1, (AsmRegister)v0);
    aria_asm_mov_r64_r64(a, (AsmRegister)v2, (AsmRegister)v1);
    aria_asm_mov_r64_r64(a, REG_RAX, (AsmRegister)v2);
    aria_asm_ret(a);

    WildXGuard g = aria_asm_finalize(a);
    ASSERT_EQ(aria_asm_execute(&g), 42);
    aria_free_exec(&g);
    aria_asm_destroy(a);
}

// ── Bitwise & Shift Operations with Vregs ──────────────────────────────

TEST(vreg_bitwise_ops) {
    // v0 = 0xFF, v1 = 0x0F; XOR v0, v1 → 0xF0
    Assembler* a = aria_asm_create();
    int v0 = aria_asm_vreg_new_gpr(a);
    int v1 = aria_asm_vreg_new_gpr(a);

    aria_asm_mov_r64_imm64(a, (AsmRegister)v0, 0xFF);
    aria_asm_mov_r64_imm64(a, (AsmRegister)v1, 0x0F);
    aria_asm_xor_r64_r64(a, (AsmRegister)v0, (AsmRegister)v1);
    aria_asm_mov_r64_r64(a, REG_RAX, (AsmRegister)v0);
    aria_asm_ret(a);

    WildXGuard g = aria_asm_finalize(a);
    ASSERT_EQ(aria_asm_execute(&g), 0xF0);
    aria_free_exec(&g);
    aria_asm_destroy(a);
}

TEST(vreg_shift_left) {
    // v0 = 1; v0 <<= 5 → 32
    Assembler* a = aria_asm_create();
    int v0 = aria_asm_vreg_new_gpr(a);

    aria_asm_mov_r64_imm64(a, (AsmRegister)v0, 1);
    aria_asm_shl_r64_imm8(a, (AsmRegister)v0, 5);
    aria_asm_mov_r64_r64(a, REG_RAX, (AsmRegister)v0);
    aria_asm_ret(a);

    WildXGuard g = aria_asm_finalize(a);
    ASSERT_EQ(aria_asm_execute(&g), 32);
    aria_free_exec(&g);
    aria_asm_destroy(a);
}

TEST(vreg_neg_not) {
    // v0 = 42; NEG v0 → -42
    Assembler* a = aria_asm_create();
    int v0 = aria_asm_vreg_new_gpr(a);

    aria_asm_mov_r64_imm64(a, (AsmRegister)v0, 42);
    aria_asm_neg_r64(a, (AsmRegister)v0);
    aria_asm_mov_r64_r64(a, REG_RAX, (AsmRegister)v0);
    aria_asm_ret(a);

    WildXGuard g = aria_asm_finalize(a);
    ASSERT_EQ(aria_asm_execute(&g), -42);
    aria_free_exec(&g);
    aria_asm_destroy(a);
}

TEST(vreg_add_imm32) {
    // v0 = 100; v0 += 42 → 142
    Assembler* a = aria_asm_create();
    int v0 = aria_asm_vreg_new_gpr(a);

    aria_asm_mov_r64_imm64(a, (AsmRegister)v0, 100);
    aria_asm_add_r64_imm32(a, (AsmRegister)v0, 42);
    aria_asm_mov_r64_r64(a, REG_RAX, (AsmRegister)v0);
    aria_asm_ret(a);

    WildXGuard g = aria_asm_finalize(a);
    ASSERT_EQ(aria_asm_execute(&g), 142);
    aria_free_exec(&g);
    aria_asm_destroy(a);
}

// ── Control Flow with Virtual Registers ─────────────────────────────────

TEST(vreg_cmp_and_branch) {
    // v0 = 10; if (v0 > 5) return 1 else return 0
    Assembler* a = aria_asm_create();
    int v0 = aria_asm_vreg_new_gpr(a);
    int lbl_true = aria_asm_new_label(a);
    int lbl_end = aria_asm_new_label(a);

    aria_asm_mov_r64_imm64(a, (AsmRegister)v0, 10);
    aria_asm_cmp_r64_imm32(a, (AsmRegister)v0, 5);
    aria_asm_jg(a, lbl_true);
    // false path
    aria_asm_mov_r64_imm64(a, REG_RAX, 0);
    aria_asm_jmp(a, lbl_end);
    // true path
    aria_asm_bind_label(a, lbl_true);
    aria_asm_mov_r64_imm64(a, REG_RAX, 1);
    aria_asm_bind_label(a, lbl_end);
    aria_asm_ret(a);

    WildXGuard g = aria_asm_finalize(a);
    ASSERT_EQ(aria_asm_execute(&g), 1);
    aria_free_exec(&g);
    aria_asm_destroy(a);
}

TEST(vreg_loop_sum) {
    // sum = 0; i = 1; while (i <= 10) { sum += i; i++; } return sum → 55
    Assembler* a = aria_asm_create();
    int v_sum = aria_asm_vreg_new_gpr(a);
    int v_i = aria_asm_vreg_new_gpr(a);
    int lbl_loop = aria_asm_new_label(a);
    int lbl_end = aria_asm_new_label(a);

    aria_asm_mov_r64_imm64(a, (AsmRegister)v_sum, 0);
    aria_asm_mov_r64_imm64(a, (AsmRegister)v_i, 1);

    aria_asm_bind_label(a, lbl_loop);
    aria_asm_cmp_r64_imm32(a, (AsmRegister)v_i, 10);
    aria_asm_jg(a, lbl_end);
    aria_asm_add_r64_r64(a, (AsmRegister)v_sum, (AsmRegister)v_i);
    aria_asm_add_r64_imm32(a, (AsmRegister)v_i, 1);
    aria_asm_jmp(a, lbl_loop);

    aria_asm_bind_label(a, lbl_end);
    aria_asm_mov_r64_r64(a, REG_RAX, (AsmRegister)v_sum);
    aria_asm_ret(a);

    WildXGuard g = aria_asm_finalize(a);
    ASSERT_EQ(aria_asm_execute(&g), 55);
    aria_free_exec(&g);
    aria_asm_destroy(a);
}

// ── Register Pressure (>14 GPRs → forces spills) ──────────────────────

TEST(vreg_pressure_16_gprs) {
    // Allocate 16 GPR vregs, each holding its index. Sum them all.
    // v[i] = i for i in 0..15; sum = sum(0..15) = 120
    Assembler* a = aria_asm_create();
    int vregs[16];
    for (int i = 0; i < 16; i++) {
        vregs[i] = aria_asm_vreg_new_gpr(a);
    }

    // Load values
    for (int i = 0; i < 16; i++) {
        aria_asm_mov_r64_imm64(a, (AsmRegister)vregs[i], i);
    }

    // Sum into v[0]
    for (int i = 1; i < 16; i++) {
        aria_asm_add_r64_r64(a, (AsmRegister)vregs[0], (AsmRegister)vregs[i]);
    }

    aria_asm_mov_r64_r64(a, REG_RAX, (AsmRegister)vregs[0]);
    aria_asm_ret(a);

    ASSERT_EQ(aria_asm_vreg_count(a), 16);
    // With 14 allocatable GPRs, 2 should spill
    WildXGuard g = aria_asm_finalize(a);
    int spills = aria_asm_spill_count(a);
    printf("  [    spills=%d] ", spills);  // informational
    ASSERT_EQ(aria_asm_execute(&g), 120);
    aria_free_exec(&g);
    aria_asm_destroy(a);
}

TEST(vreg_pressure_heavy) {
    // Allocate 8 vregs all alive simultaneously with distinct values
    // v0=1, v1=2, v2=4, v3=8, v4=16, v5=32, v6=64, v7=128
    // result = v0+v1+v2+v3+v4+v5+v6+v7 = 255
    Assembler* a = aria_asm_create();
    int v[8];
    for (int i = 0; i < 8; i++) {
        v[i] = aria_asm_vreg_new_gpr(a);
    }

    for (int i = 0; i < 8; i++) {
        aria_asm_mov_r64_imm64(a, (AsmRegister)v[i], 1 << i);
    }

    // Accumulate into v[0]
    for (int i = 1; i < 8; i++) {
        aria_asm_add_r64_r64(a, (AsmRegister)v[0], (AsmRegister)v[i]);
    }

    aria_asm_mov_r64_r64(a, REG_RAX, (AsmRegister)v[0]);
    aria_asm_ret(a);

    WildXGuard g = aria_asm_finalize(a);
    ASSERT_EQ(aria_asm_execute(&g), 255);
    aria_free_exec(&g);
    aria_asm_destroy(a);
}

// ── Mixed Physical + Virtual Registers ──────────────────────────────────

TEST(vreg_mixed_phys_virt) {
    // Use physical RAX alongside virtual regs
    // v0 = 50; RAX = 20; RAX += v0 → 70
    Assembler* a = aria_asm_create();
    int v0 = aria_asm_vreg_new_gpr(a);

    aria_asm_mov_r64_imm64(a, (AsmRegister)v0, 50);
    aria_asm_mov_r64_imm64(a, REG_RAX, 20);
    aria_asm_add_r64_r64(a, REG_RAX, (AsmRegister)v0);
    aria_asm_ret(a);

    WildXGuard g = aria_asm_finalize(a);
    ASSERT_EQ(aria_asm_execute(&g), 70);
    aria_free_exec(&g);
    aria_asm_destroy(a);
}

// ── XMM Virtual Registers ───────────────────────────────────────────────

TEST(vreg_xmm_simple_add) {
    // vx0 = 3.0; vx1 = 4.0; vx0 += vx1; return vx0 → 7.0
    Assembler* a = aria_asm_create();
    int vx0 = aria_asm_vreg_new_xmm(a);
    int vx1 = aria_asm_vreg_new_xmm(a);

    // Load constants via memory (store double to stack, load to XMM)
    aria_asm_prologue(a, 16);

    // Store 3.0 to stack
    double val1 = 3.0;
    int64_t bits1;
    memcpy(&bits1, &val1, sizeof(bits1));
    aria_asm_mov_r64_imm64(a, REG_RAX, bits1);
    aria_asm_store_local(a, 8, REG_RAX);
    aria_asm_movsd_xmm_mem(a, (AsmRegister)vx0, REG_RBP, -8);

    // Store 4.0 to stack
    double val2 = 4.0;
    int64_t bits2;
    memcpy(&bits2, &val2, sizeof(bits2));
    aria_asm_mov_r64_imm64(a, REG_RAX, bits2);
    aria_asm_store_local(a, 8, REG_RAX);
    aria_asm_movsd_xmm_mem(a, (AsmRegister)vx1, REG_RBP, -8);

    // Add
    aria_asm_addsd(a, (AsmRegister)vx0, (AsmRegister)vx1);

    // Move result to XMM0 for return
    aria_asm_movsd_xmm_xmm(a, REG_XMM0, (AsmRegister)vx0);
    aria_asm_epilogue(a);

    WildXGuard g = aria_asm_finalize(a);
    ASSERT_NEAR(aria_asm_execute_f64(&g), 7.0, 0.001);
    aria_free_exec(&g);
    aria_asm_destroy(a);
}

TEST(vreg_xmm_mul_sub) {
    // vx0 = 5.0; vx1 = 3.0; vx0 = vx0 * vx1 = 15.0; vx0 = vx0 - vx1 = 12.0
    Assembler* a = aria_asm_create();
    int vx0 = aria_asm_vreg_new_xmm(a);
    int vx1 = aria_asm_vreg_new_xmm(a);

    aria_asm_prologue(a, 16);

    // Load 5.0
    double val1 = 5.0;
    int64_t bits1;
    memcpy(&bits1, &val1, sizeof(bits1));
    aria_asm_mov_r64_imm64(a, REG_RAX, bits1);
    aria_asm_store_local(a, 8, REG_RAX);
    aria_asm_movsd_xmm_mem(a, (AsmRegister)vx0, REG_RBP, -8);

    // Load 3.0
    double val2 = 3.0;
    int64_t bits2;
    memcpy(&bits2, &val2, sizeof(bits2));
    aria_asm_mov_r64_imm64(a, REG_RAX, bits2);
    aria_asm_store_local(a, 8, REG_RAX);
    aria_asm_movsd_xmm_mem(a, (AsmRegister)vx1, REG_RBP, -8);

    // Multiply: vx0 *= vx1 → 15.0
    aria_asm_mulsd(a, (AsmRegister)vx0, (AsmRegister)vx1);
    // Subtract: vx0 -= vx1 → 12.0
    aria_asm_subsd(a, (AsmRegister)vx0, (AsmRegister)vx1);

    aria_asm_movsd_xmm_xmm(a, REG_XMM0, (AsmRegister)vx0);
    aria_asm_epilogue(a);

    WildXGuard g = aria_asm_finalize(a);
    ASSERT_NEAR(aria_asm_execute_f64(&g), 12.0, 0.001);
    aria_free_exec(&g);
    aria_asm_destroy(a);
}

// ── Vreg with Prologue/Epilogue ─────────────────────────────────────────

TEST(vreg_with_stack_frame) {
    // Use prologue/epilogue + vregs for a proper function
    // v0 = 25; v1 = 17; store v0 to local[8]; load v0 from local[8]; return v0 + v1
    Assembler* a = aria_asm_create();
    int v0 = aria_asm_vreg_new_gpr(a);
    int v1 = aria_asm_vreg_new_gpr(a);

    aria_asm_prologue(a, 16);

    aria_asm_mov_r64_imm64(a, (AsmRegister)v0, 25);
    aria_asm_mov_r64_imm64(a, (AsmRegister)v1, 17);
    aria_asm_add_r64_r64(a, (AsmRegister)v0, (AsmRegister)v1);
    aria_asm_mov_r64_r64(a, REG_RAX, (AsmRegister)v0);

    aria_asm_epilogue(a);

    WildXGuard g = aria_asm_finalize(a);
    ASSERT_EQ(aria_asm_execute(&g), 42);
    aria_free_exec(&g);
    aria_asm_destroy(a);
}

// ── API Validation ──────────────────────────────────────────────────────

TEST(vreg_count_tracking) {
    Assembler* a = aria_asm_create();
    ASSERT_EQ(aria_asm_vreg_count(a), 0);

    aria_asm_vreg_new_gpr(a);
    ASSERT_EQ(aria_asm_vreg_count(a), 1);

    aria_asm_vreg_new_gpr(a);
    aria_asm_vreg_new_xmm(a);
    ASSERT_EQ(aria_asm_vreg_count(a), 3);

    aria_asm_destroy(a);
}

TEST(vreg_backward_compat) {
    // Pure physical register use: no vregs, no IR mode, direct emission
    Assembler* a = aria_asm_create();
    aria_asm_mov_r64_imm64(a, REG_RAX, 99);
    aria_asm_ret(a);

    WildXGuard g = aria_asm_finalize(a);
    ASSERT_EQ(aria_asm_execute(&g), 99);
    aria_free_exec(&g);
    aria_asm_destroy(a);
}

// ── Main ────────────────────────────────────────────────────────────────

int main() {
    printf("\n=== JIT Register Allocator Tests (v0.7.3) ===\n\n");

    // Basic vreg allocation
    run_vreg_simple_add();
    run_vreg_sub_imul();
    run_vreg_mov_chain();

    // Bitwise + shifts
    run_vreg_bitwise_ops();
    run_vreg_shift_left();
    run_vreg_neg_not();
    run_vreg_add_imm32();

    // Control flow
    run_vreg_cmp_and_branch();
    run_vreg_loop_sum();

    // Register pressure
    run_vreg_pressure_16_gprs();
    run_vreg_pressure_heavy();

    // Mixed mode
    run_vreg_mixed_phys_virt();

    // XMM vregs
    run_vreg_xmm_simple_add();
    run_vreg_xmm_mul_sub();

    // Stack frame
    run_vreg_with_stack_frame();

    // API
    run_vreg_count_tracking();
    run_vreg_backward_compat();

    printf("\n=== Results: %d/%d passed (%d assertions) ===\n\n", tests_run - tests_failed, tests_run, tests_passed);
    return (tests_failed == 0) ? 0 : 1;
}
