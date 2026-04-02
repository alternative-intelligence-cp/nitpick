/**
 * JIT Optimization & Multi-Arch Tests — v0.7.4
 *
 * Tests peephole optimizer, instruction selection, architecture APIs,
 * profiling hooks, and combined optimization passes.
 */

#include "runtime/assembler.h"
#include <cstdio>
#include <cstring>
#include <cmath>

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;
static int assertions = 0;

#define TEST(name) \
    static void test_##name(); \
    static void run_##name() { \
        tests_run++; \
        printf("  [%2d] %-45s ", tests_run, #name); \
        test_##name(); \
    } \
    static void test_##name()

#define ASSERT_EQ(a, b) do { \
    assertions++; \
    auto _a = (a); auto _b = (b); \
    if (_a == _b) { printf("PASS\n"); tests_passed++; } \
    else { printf("FAIL (got %ld, expected %ld)\n", (long)_a, (long)_b); tests_failed++; } \
} while(0)

#define ASSERT_TRUE(cond) do { \
    assertions++; \
    if (cond) { printf("PASS\n"); tests_passed++; } \
    else { printf("FAIL (condition false)\n"); tests_failed++; } \
} while(0)

#define ASSERT_NEAR(a, b, eps) do { \
    assertions++; \
    double _a = (a); double _b = (b); \
    if (fabs(_a - _b) < (eps)) { printf("PASS\n"); tests_passed++; } \
    else { printf("FAIL (got %.6f, expected %.6f)\n", _a, _b); tests_failed++; } \
} while(0)

// ==========================================================================
// Peephole Optimizer Tests
// ==========================================================================

TEST(peephole_mov_zero_to_xor) {
    // MOV v0, 0 should be converted to XOR v0, v0 by peephole.
    // Verify the result is still 0.
    Assembler* a = aria_asm_create();
    int v0 = aria_asm_vreg_new_gpr(a);
    aria_asm_mov_r64_imm64(a, (AsmRegister)v0, 0);
    aria_asm_mov_r64_r64(a, REG_RAX, (AsmRegister)v0);
    aria_asm_ret(a);
    WildXGuard g = aria_asm_finalize(a);
    ASSERT_EQ(aria_asm_execute(&g), 0);
    aria_free_exec(&g);
    aria_asm_destroy(a);
}

TEST(peephole_identity_mov) {
    // MOV v0, v0 should be eliminated. v0 = 77, result should be 77.
    Assembler* a = aria_asm_create();
    int v0 = aria_asm_vreg_new_gpr(a);
    aria_asm_mov_r64_imm64(a, (AsmRegister)v0, 77);
    aria_asm_mov_r64_r64(a, (AsmRegister)v0, (AsmRegister)v0); // identity → NOP
    aria_asm_mov_r64_r64(a, REG_RAX, (AsmRegister)v0);
    aria_asm_ret(a);
    WildXGuard g = aria_asm_finalize(a);
    ASSERT_EQ(aria_asm_execute(&g), 77);
    aria_free_exec(&g);
    aria_asm_destroy(a);
}

TEST(peephole_add_zero_elim) {
    // ADD v0, 0 should be eliminated. v0 = 42, result should be 42.
    Assembler* a = aria_asm_create();
    int v0 = aria_asm_vreg_new_gpr(a);
    aria_asm_mov_r64_imm64(a, (AsmRegister)v0, 42);
    aria_asm_add_r64_imm32(a, (AsmRegister)v0, 0); // → NOP
    aria_asm_mov_r64_r64(a, REG_RAX, (AsmRegister)v0);
    aria_asm_ret(a);
    WildXGuard g = aria_asm_finalize(a);
    ASSERT_EQ(aria_asm_execute(&g), 42);
    aria_free_exec(&g);
    aria_asm_destroy(a);
}

TEST(peephole_shift_zero_elim) {
    // SHL v0, 0 should be eliminated. v0 = 99, result should be 99.
    Assembler* a = aria_asm_create();
    int v0 = aria_asm_vreg_new_gpr(a);
    aria_asm_mov_r64_imm64(a, (AsmRegister)v0, 99);
    aria_asm_shl_r64_imm8(a, (AsmRegister)v0, 0); // → NOP
    aria_asm_mov_r64_r64(a, REG_RAX, (AsmRegister)v0);
    aria_asm_ret(a);
    WildXGuard g = aria_asm_finalize(a);
    ASSERT_EQ(aria_asm_execute(&g), 99);
    aria_free_exec(&g);
    aria_asm_destroy(a);
}

TEST(peephole_dead_store_elim) {
    // MOV v0, 10; MOV v0, 20 → NOP; MOV v0, 20. Result should be 20.
    Assembler* a = aria_asm_create();
    int v0 = aria_asm_vreg_new_gpr(a);
    aria_asm_mov_r64_imm64(a, (AsmRegister)v0, 10);
    aria_asm_mov_r64_imm64(a, (AsmRegister)v0, 20); // kills first MOV
    aria_asm_mov_r64_r64(a, REG_RAX, (AsmRegister)v0);
    aria_asm_ret(a);
    WildXGuard g = aria_asm_finalize(a);
    ASSERT_EQ(aria_asm_execute(&g), 20);
    aria_free_exec(&g);
    aria_asm_destroy(a);
}

TEST(peephole_strength_reduction) {
    // MOV v1, 8; IMUL v0, v1 → SHL v0, 3 (since 8 = 2^3)
    // v0 = 5, v1 = 8; v0 * v1 = 40
    Assembler* a = aria_asm_create();
    int v0 = aria_asm_vreg_new_gpr(a);
    int v1 = aria_asm_vreg_new_gpr(a);
    aria_asm_mov_r64_imm64(a, (AsmRegister)v0, 5);
    aria_asm_mov_r64_imm64(a, (AsmRegister)v1, 8);
    aria_asm_imul_r64_r64(a, (AsmRegister)v0, (AsmRegister)v1);
    aria_asm_mov_r64_r64(a, REG_RAX, (AsmRegister)v0);
    aria_asm_ret(a);
    WildXGuard g = aria_asm_finalize(a);
    ASSERT_EQ(aria_asm_execute(&g), 40);
    aria_free_exec(&g);
    aria_asm_destroy(a);
}

TEST(peephole_stats_available) {
    // Verify peephole stats are populated after finalize.
    Assembler* a = aria_asm_create();
    int v0 = aria_asm_vreg_new_gpr(a);
    aria_asm_mov_r64_imm64(a, (AsmRegister)v0, 0);  // → XOR (1 elimination)
    aria_asm_add_r64_imm32(a, (AsmRegister)v0, 0);   // → NOP (1 elimination)
    aria_asm_mov_r64_r64(a, REG_RAX, (AsmRegister)v0);
    aria_asm_ret(a);
    WildXGuard g = aria_asm_finalize(a);
    PeepholeStats ps = aria_asm_peephole_stats(a);
    ASSERT_TRUE(ps.total_eliminated >= 2);
    aria_free_exec(&g);
    aria_asm_destroy(a);
}

// ==========================================================================
// Instruction Selection Tests
// ==========================================================================

TEST(isel_mov_imm64_narrow) {
    // MOV v0, 42 should use MOV r32, imm32 (5-6 bytes) instead of MOVABS (10).
    // Verify result is correct.
    Assembler* a = aria_asm_create();
    int v0 = aria_asm_vreg_new_gpr(a);
    aria_asm_mov_r64_imm64(a, (AsmRegister)v0, 42);
    aria_asm_mov_r64_r64(a, REG_RAX, (AsmRegister)v0);
    aria_asm_ret(a);
    WildXGuard g = aria_asm_finalize(a);
    ASSERT_EQ(aria_asm_execute(&g), 42);
    aria_free_exec(&g);
    // Check narrowing happened
    InsnSelStats is = aria_asm_insn_sel_stats(a);
    ASSERT_TRUE(is.mov_imm64_narrowed >= 1);
    aria_asm_destroy(a);
}

TEST(isel_mov_large_imm64) {
    // MOV v0, 0x100000000 should NOT be narrowed (doesn't fit in u32).
    Assembler* a = aria_asm_create();
    int v0 = aria_asm_vreg_new_gpr(a);
    aria_asm_mov_r64_imm64(a, (AsmRegister)v0, 0x100000000LL);
    aria_asm_mov_r64_r64(a, REG_RAX, (AsmRegister)v0);
    aria_asm_ret(a);
    WildXGuard g = aria_asm_finalize(a);
    ASSERT_EQ(aria_asm_execute(&g), 0x100000000LL);
    aria_free_exec(&g);
    aria_asm_destroy(a);
}

TEST(isel_mov_negative_imm64) {
    // MOV v0, -1 should NOT be narrowed (negative values need full 64-bit).
    // -1 as int64 = 0xFFFFFFFFFFFFFFFF, which exceeds u32 range.
    Assembler* a = aria_asm_create();
    int v0 = aria_asm_vreg_new_gpr(a);
    aria_asm_mov_r64_imm64(a, (AsmRegister)v0, -1);
    aria_asm_mov_r64_r64(a, REG_RAX, (AsmRegister)v0);
    aria_asm_ret(a);
    WildXGuard g = aria_asm_finalize(a);
    // -1 as unsigned int64 = 0xFFFFFFFFFFFFFFFF
    int64_t result = aria_asm_execute(&g);
    ASSERT_EQ(result, -1);
    aria_free_exec(&g);
    aria_asm_destroy(a);
}

TEST(isel_add_one_inc) {
    // ADD v0, 1 should become INC. v0 = 99, result should be 100.
    Assembler* a = aria_asm_create();
    int v0 = aria_asm_vreg_new_gpr(a);
    aria_asm_mov_r64_imm64(a, (AsmRegister)v0, 99);
    aria_asm_add_r64_imm32(a, (AsmRegister)v0, 1); // → INC
    aria_asm_mov_r64_r64(a, REG_RAX, (AsmRegister)v0);
    aria_asm_ret(a);
    WildXGuard g = aria_asm_finalize(a);
    ASSERT_EQ(aria_asm_execute(&g), 100);
    aria_free_exec(&g);
    aria_asm_destroy(a);
}

TEST(isel_sub_one_dec) {
    // SUB v0, 1 should become DEC. v0 = 100, result should be 99.
    Assembler* a = aria_asm_create();
    int v0 = aria_asm_vreg_new_gpr(a);
    aria_asm_mov_r64_imm64(a, (AsmRegister)v0, 100);
    aria_asm_sub_r64_imm32(a, (AsmRegister)v0, 1); // → DEC
    aria_asm_mov_r64_r64(a, REG_RAX, (AsmRegister)v0);
    aria_asm_ret(a);
    WildXGuard g = aria_asm_finalize(a);
    ASSERT_EQ(aria_asm_execute(&g), 99);
    aria_free_exec(&g);
    aria_asm_destroy(a);
}

TEST(isel_add_imm8_form) {
    // ADD v0, 42 should use imm8 encoding (42 fits in signed byte).
    Assembler* a = aria_asm_create();
    int v0 = aria_asm_vreg_new_gpr(a);
    aria_asm_mov_r64_imm64(a, (AsmRegister)v0, 100);
    aria_asm_add_r64_imm32(a, (AsmRegister)v0, 42); // → imm8 form
    aria_asm_mov_r64_r64(a, REG_RAX, (AsmRegister)v0);
    aria_asm_ret(a);
    WildXGuard g = aria_asm_finalize(a);
    ASSERT_EQ(aria_asm_execute(&g), 142);
    aria_free_exec(&g);
    InsnSelStats is = aria_asm_insn_sel_stats(a);
    ASSERT_TRUE(is.imm32_to_imm8 >= 1);
    aria_asm_destroy(a);
}

TEST(isel_cmp_zero_test) {
    // CMP v0, 0 followed by JE should use TEST r, r.
    // v0 = 0, branch taken → return 1; else return 0.
    Assembler* a = aria_asm_create();
    int lbl_zero = aria_asm_new_label(a);
    int lbl_end  = aria_asm_new_label(a);
    int v0 = aria_asm_vreg_new_gpr(a);

    aria_asm_mov_r64_imm64(a, (AsmRegister)v0, 0);
    aria_asm_cmp_r64_imm32(a, (AsmRegister)v0, 0); // → TEST v0, v0
    aria_asm_je(a, lbl_zero);
    // Not zero path
    aria_asm_mov_r64_imm64(a, REG_RAX, 0);
    aria_asm_jmp(a, lbl_end);
    // Zero path
    aria_asm_bind_label(a, lbl_zero);
    aria_asm_mov_r64_imm64(a, REG_RAX, 1);
    aria_asm_bind_label(a, lbl_end);
    aria_asm_ret(a);

    WildXGuard g = aria_asm_finalize(a);
    ASSERT_EQ(aria_asm_execute(&g), 1);
    aria_free_exec(&g);
    aria_asm_destroy(a);
}

TEST(isel_sub_imm8_form) {
    // SUB v0, 50 should use imm8 encoding.
    Assembler* a = aria_asm_create();
    int v0 = aria_asm_vreg_new_gpr(a);
    aria_asm_mov_r64_imm64(a, (AsmRegister)v0, 200);
    aria_asm_sub_r64_imm32(a, (AsmRegister)v0, 50); // → imm8 form
    aria_asm_mov_r64_r64(a, REG_RAX, (AsmRegister)v0);
    aria_asm_ret(a);
    WildXGuard g = aria_asm_finalize(a);
    ASSERT_EQ(aria_asm_execute(&g), 150);
    aria_free_exec(&g);
    aria_asm_destroy(a);
}

TEST(isel_stats_tracking) {
    // Verify InsnSelStats tracks selections correctly.
    Assembler* a = aria_asm_create();
    int v0 = aria_asm_vreg_new_gpr(a);
    aria_asm_mov_r64_imm64(a, (AsmRegister)v0, 10);  // narrowed
    aria_asm_add_r64_imm32(a, (AsmRegister)v0, 1);    // → INC
    aria_asm_sub_r64_imm32(a, (AsmRegister)v0, 1);    // → DEC
    aria_asm_add_r64_imm32(a, (AsmRegister)v0, 42);   // → imm8
    aria_asm_cmp_r64_imm32(a, (AsmRegister)v0, 0);    // → TEST
    aria_asm_mov_r64_r64(a, REG_RAX, (AsmRegister)v0);
    aria_asm_ret(a);
    WildXGuard g = aria_asm_finalize(a);
    InsnSelStats is = aria_asm_insn_sel_stats(a);
    ASSERT_TRUE(is.total_selected >= 4);  // at least narrowed + INC + DEC + imm8
    aria_free_exec(&g);
    aria_asm_destroy(a);
}

// ==========================================================================
// Architecture API Tests
// ==========================================================================

TEST(arch_get_current) {
    // On x86-64, should return ASM_ARCH_X86_64
    ASSERT_EQ((int)aria_asm_get_arch(), (int)ASM_ARCH_X86_64);
}

TEST(arch_x86_64_supported) {
    ASSERT_TRUE(aria_asm_arch_supported(ASM_ARCH_X86_64));
}

TEST(arch_aarch64_not_supported) {
    // AArch64 is not yet supported on this x86-64 build
    ASSERT_TRUE(!aria_asm_arch_supported(ASM_ARCH_AARCH64));
}

// ==========================================================================
// Combined Optimization Tests
// ==========================================================================

TEST(combined_peephole_isel_loop) {
    // Loop: v0 = 0, v1 = 10; while (v0 < v1) v0++;  return v0.
    // Tests: MOV 0 → XOR (peephole), CMP 0 → TEST (isel),
    //        ADD 1 → INC (isel), MOV imm narrow (isel)
    Assembler* a = aria_asm_create();
    int lbl_top  = aria_asm_new_label(a);
    int lbl_done = aria_asm_new_label(a);
    int v0 = aria_asm_vreg_new_gpr(a);
    int v1 = aria_asm_vreg_new_gpr(a);

    aria_asm_mov_r64_imm64(a, (AsmRegister)v0, 0);   // → XOR (peephole)
    aria_asm_mov_r64_imm64(a, (AsmRegister)v1, 10);   // → MOV r32, 10 (isel)
    aria_asm_bind_label(a, lbl_top);
    aria_asm_cmp_r64_r64(a, (AsmRegister)v0, (AsmRegister)v1);
    aria_asm_jge(a, lbl_done);
    aria_asm_add_r64_imm32(a, (AsmRegister)v0, 1);    // → INC (isel)
    aria_asm_jmp(a, lbl_top);
    aria_asm_bind_label(a, lbl_done);
    aria_asm_mov_r64_r64(a, REG_RAX, (AsmRegister)v0);
    aria_asm_ret(a);

    WildXGuard g = aria_asm_finalize(a);
    ASSERT_EQ(aria_asm_execute(&g), 10);
    aria_free_exec(&g);
    aria_asm_destroy(a);
}

TEST(combined_strength_reduce_add) {
    // v0 = 7, v1 = 16; result = v0 * v1 = 112
    // Peephole: IMUL by 16 (2^4) → SHL by 4
    Assembler* a = aria_asm_create();
    int v0 = aria_asm_vreg_new_gpr(a);
    int v1 = aria_asm_vreg_new_gpr(a);
    aria_asm_mov_r64_imm64(a, (AsmRegister)v0, 7);
    aria_asm_mov_r64_imm64(a, (AsmRegister)v1, 16);
    aria_asm_imul_r64_r64(a, (AsmRegister)v0, (AsmRegister)v1);
    aria_asm_mov_r64_r64(a, REG_RAX, (AsmRegister)v0);
    aria_asm_ret(a);
    WildXGuard g = aria_asm_finalize(a);
    ASSERT_EQ(aria_asm_execute(&g), 112);
    aria_free_exec(&g);
    aria_asm_destroy(a);
}

TEST(combined_maxval_u32) {
    // MOV v0, 0xFFFFFFFF (max u32). Should be narrowed to MOV r32, imm32.
    Assembler* a = aria_asm_create();
    int v0 = aria_asm_vreg_new_gpr(a);
    aria_asm_mov_r64_imm64(a, (AsmRegister)v0, 0xFFFFFFFF);
    aria_asm_mov_r64_r64(a, REG_RAX, (AsmRegister)v0);
    aria_asm_ret(a);
    WildXGuard g = aria_asm_finalize(a);
    ASSERT_EQ(aria_asm_execute(&g), 0xFFFFFFFF);
    aria_free_exec(&g);
    aria_asm_destroy(a);
}

// ==========================================================================
// Main
// ==========================================================================

int main() {
    printf("\n=== JIT Optimization & Multi-Arch Tests (v0.7.4) ===\n\n");

    // Peephole tests
    run_peephole_mov_zero_to_xor();
    run_peephole_identity_mov();
    run_peephole_add_zero_elim();
    run_peephole_shift_zero_elim();
    run_peephole_dead_store_elim();
    run_peephole_strength_reduction();
    run_peephole_stats_available();

    // Instruction selection tests
    run_isel_mov_imm64_narrow();
    run_isel_mov_large_imm64();
    run_isel_mov_negative_imm64();
    run_isel_add_one_inc();
    run_isel_sub_one_dec();
    run_isel_add_imm8_form();
    run_isel_cmp_zero_test();
    run_isel_sub_imm8_form();
    run_isel_stats_tracking();

    // Architecture API tests
    run_arch_get_current();
    run_arch_x86_64_supported();
    run_arch_aarch64_not_supported();

    // Combined optimization tests
    run_combined_peephole_isel_loop();
    run_combined_strength_reduce_add();
    run_combined_maxval_u32();

    printf("\n=== Results: %d/%d passed (%d assertions) ===\n",
           tests_passed, tests_run, assertions);
    return tests_failed > 0 ? 1 : 0;
}
