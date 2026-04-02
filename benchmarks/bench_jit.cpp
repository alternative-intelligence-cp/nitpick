/**
 * JIT Assembler Benchmark — v0.7.4
 *
 * Measures JIT compilation and execution performance.
 * Compares: native C function, JIT-generated with physical regs,
 * JIT-generated with virtual regs (register allocator + peephole + isel).
 *
 * Build: g++ -std=c++17 -O2 -I include bench_jit.cpp \
 *        src/runtime/assembler/assembler.cpp \
 *        src/runtime/allocators/wildx_alloc.cpp -o benchmarks/bench_jit
 * Run:   ./benchmarks/bench_jit
 */

#include "runtime/assembler.h"
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <chrono>

using Clock = std::chrono::high_resolution_clock;

// Native C: sum 1..N
static int64_t native_sum(int64_t n) {
    int64_t sum = 0;
    for (int64_t i = 1; i <= n; i++) sum += i;
    return sum;
}

// Build JIT sum function using physical registers (no regalloc)
static WildXGuard build_sum_phys(Assembler* a) {
    int lbl_loop = aria_asm_new_label(a);
    int lbl_done = aria_asm_new_label(a);

    // RDI = n (argument), RAX = sum, RCX = i
    aria_asm_mov_r64_imm64(a, REG_RAX, 0);       // sum = 0
    aria_asm_mov_r64_imm64(a, REG_RCX, 1);       // i = 1

    aria_asm_bind_label(a, lbl_loop);
    aria_asm_cmp_r64_r64(a, REG_RCX, REG_RDI);
    aria_asm_jg(a, lbl_done);
    aria_asm_add_r64_r64(a, REG_RAX, REG_RCX);
    aria_asm_add_r64_imm32(a, REG_RCX, 1);
    aria_asm_jmp(a, lbl_loop);

    aria_asm_bind_label(a, lbl_done);
    aria_asm_ret(a);

    return aria_asm_finalize(a);
}

// Build JIT sum function using virtual registers (regalloc + peephole + isel)
static WildXGuard build_sum_vreg(Assembler* a) {
    int lbl_loop = aria_asm_new_label(a);
    int lbl_done = aria_asm_new_label(a);

    int v_n   = aria_asm_vreg_new_gpr(a);
    int v_sum = aria_asm_vreg_new_gpr(a);
    int v_i   = aria_asm_vreg_new_gpr(a);

    // Copy argument from RDI
    aria_asm_mov_r64_r64(a, (AsmRegister)v_n, REG_RDI);
    aria_asm_mov_r64_imm64(a, (AsmRegister)v_sum, 0);    // peephole: → XOR
    aria_asm_mov_r64_imm64(a, (AsmRegister)v_i, 1);      // isel: → MOV r32

    aria_asm_bind_label(a, lbl_loop);
    aria_asm_cmp_r64_r64(a, (AsmRegister)v_i, (AsmRegister)v_n);
    aria_asm_jg(a, lbl_done);
    aria_asm_add_r64_r64(a, (AsmRegister)v_sum, (AsmRegister)v_i);
    aria_asm_add_r64_imm32(a, (AsmRegister)v_i, 1);      // isel: → INC
    aria_asm_jmp(a, lbl_loop);

    aria_asm_bind_label(a, lbl_done);
    aria_asm_mov_r64_r64(a, REG_RAX, (AsmRegister)v_sum);
    aria_asm_ret(a);

    return aria_asm_finalize(a);
}

static double bench(const char* name, int iterations, int64_t (*fn)(int64_t), int64_t arg) {
    // Warmup
    volatile int64_t sink = 0;
    for (int i = 0; i < 10; i++) sink = fn(arg);

    auto t0 = Clock::now();
    for (int i = 0; i < iterations; i++) {
        sink = fn(arg);
    }
    auto t1 = Clock::now();
    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    printf("  %-30s %8.2f ms  (%d iters, result=%ld)\n", name, ms, iterations, (long)sink);
    return ms;
}

int main() {
    const int64_t N = 10000;
    const int ITERS = 100000;

    printf("\n=== JIT Assembler Benchmark (v0.7.4) ===\n");
    printf("  Sum 1..%ld, %d iterations\n\n", (long)N, ITERS);

    // 1. Native C
    double t_native = bench("Native C (compiled -O2)", ITERS, native_sum, N);

    // 2. JIT physical registers
    Assembler* a_phys = aria_asm_create();
    WildXGuard g_phys = build_sum_phys(a_phys);

    // Cast to function pointer that takes int64 arg
    typedef int64_t (*sum_fn)(int64_t);
    sum_fn jit_phys = (sum_fn)g_phys.ptr;

    double t_phys = bench("JIT physical regs", ITERS, jit_phys, N);

    // 3. JIT virtual registers (regalloc + optimizations)
    Assembler* a_vreg = aria_asm_create();
    WildXGuard g_vreg = build_sum_vreg(a_vreg);
    sum_fn jit_vreg = (sum_fn)g_vreg.ptr;

    double t_vreg = bench("JIT vreg (regalloc+opt)", ITERS, jit_vreg, N);

    // Optimization stats
    PeepholeStats ps = aria_asm_peephole_stats(a_vreg);
    InsnSelStats is = aria_asm_insn_sel_stats(a_vreg);
    printf("\n  Peephole eliminations: %d\n", ps.total_eliminated);
    printf("  Instruction selections: %d\n", is.total_selected);
    printf("    MOV narrowed: %d, CMP→TEST: %d, INC: %d, DEC: %d, imm8: %d\n",
           is.mov_imm64_narrowed, is.cmp_zero_to_test,
           is.add_to_inc, is.sub_to_dec, is.imm32_to_imm8);

    printf("\n  Ratios:\n");
    printf("    JIT phys / Native:  %.2fx\n", t_phys / t_native);
    printf("    JIT vreg / Native:  %.2fx\n", t_vreg / t_native);
    printf("    JIT vreg / phys:    %.2fx\n", t_vreg / t_phys);

    printf("\n  Code sizes:\n");
    printf("    JIT phys: %zu bytes\n", g_phys.size);
    printf("    JIT vreg: %zu bytes\n", g_vreg.size);

    // Cleanup
    aria_free_exec(&g_phys);
    aria_free_exec(&g_vreg);
    aria_asm_destroy(a_phys);
    aria_asm_destroy(a_vreg);

    printf("\n=== Benchmark complete ===\n\n");
    return 0;
}
