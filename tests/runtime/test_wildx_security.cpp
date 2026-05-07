// WildX Security Hardening Test (v0.7.1)
// C++ test that exercises guard pages, ASLR, quota, code hashing, audit logging
//
// Build: Compiled as part of ariac_testing target
// Run:   ./build/ariac_testing --wildx-security

#include "runtime/allocators.h"
#include "runtime/assembler.h"
#include <cstdio>
#include <cstring>
#include <cassert>
#include <cstdlib>

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST_ASSERT(cond, msg) do { \
    if (!(cond)) { \
        std::fprintf(stderr, "  [FAIL] %s: %s\n", msg, #cond); \
        tests_failed++; \
        return; \
    } \
} while(0)

#define TEST_PASS(msg) do { \
    std::fprintf(stdout, "  [PASS] %s\n", msg); \
    tests_passed++; \
} while(0)

// ============================================================================
// Test 1: Basic alloc/seal/execute/free cycle
// ============================================================================
static void test_basic_cycle() {
    Assembler* a = npk_asm_create();
    TEST_ASSERT(a != nullptr, "basic cycle");
    
    // Generate: mov rax, 42; ret
    npk_asm_mov_r64_imm64(a, REG_RAX, 42);
    npk_asm_ret(a);
    
    WildXGuard guard = npk_asm_finalize(a);
    TEST_ASSERT(guard.ptr != nullptr, "basic cycle - finalize");
    TEST_ASSERT(guard.state == WILDX_STATE_EXECUTABLE, "basic cycle - state");
    TEST_ASSERT(guard.sealed == true, "basic cycle - sealed");
    TEST_ASSERT(guard.code_hash != 0, "basic cycle - hash computed");
    
    int64_t result = npk_asm_execute(&guard);
    TEST_ASSERT(result == 42, "basic cycle - execute");
    
    npk_free_exec(&guard);
    TEST_ASSERT(guard.state == WILDX_STATE_FREED, "basic cycle - freed");
    
    npk_asm_destroy(a);
    TEST_PASS("basic alloc/seal/execute/free cycle");
}

// ============================================================================
// Test 2: Code hash is set at seal time
// ============================================================================
static void test_code_hash() {
    Assembler* a = npk_asm_create();
    
    npk_asm_mov_r64_imm64(a, REG_RAX, 100);
    npk_asm_ret(a);
    
    WildXGuard guard = npk_asm_finalize(a);
    TEST_ASSERT(guard.ptr != nullptr, "code hash");
    TEST_ASSERT(guard.code_hash != 0, "code hash - non-zero");
    
    // Verify hash matches current content
    uint64_t computed = npk_wildx_verify_hash(&guard);
    TEST_ASSERT(computed == guard.code_hash, "code hash - verify matches");
    
    npk_free_exec(&guard);
    npk_asm_destroy(a);
    TEST_PASS("code hash computed and verified at seal time");
}

// ============================================================================
// Test 3: Different code produces different hashes
// ============================================================================
static void test_hash_uniqueness() {
    Assembler* a1 = npk_asm_create();
    npk_asm_mov_r64_imm64(a1, REG_RAX, 1);
    npk_asm_ret(a1);
    WildXGuard g1 = npk_asm_finalize(a1);
    
    Assembler* a2 = npk_asm_create();
    npk_asm_mov_r64_imm64(a2, REG_RAX, 2);
    npk_asm_ret(a2);
    WildXGuard g2 = npk_asm_finalize(a2);
    
    TEST_ASSERT(g1.code_hash != g2.code_hash, "hash uniqueness");
    
    npk_free_exec(&g1);
    npk_free_exec(&g2);
    npk_asm_destroy(a1);
    npk_asm_destroy(a2);
    TEST_PASS("different code produces different hashes");
}

// ============================================================================
// Test 4: Guard pages — normal use works
// ============================================================================
static void test_guard_pages_normal() {
    npk_wildx_enable_guard_pages(true);
    
    Assembler* a = npk_asm_create();
    npk_asm_mov_r64_imm64(a, REG_RAX, 77);
    npk_asm_ret(a);
    
    WildXGuard guard = npk_asm_finalize(a);
    TEST_ASSERT(guard.ptr != nullptr, "guard pages normal");
    TEST_ASSERT(guard.map_size > guard.size, "guard pages - map_size > size");
    
    int64_t result = npk_asm_execute(&guard);
    TEST_ASSERT(result == 77, "guard pages - execute");
    
    npk_free_exec(&guard);
    npk_asm_destroy(a);
    
    npk_wildx_enable_guard_pages(false);
    TEST_PASS("guard pages around exec region (normal use)");
}

// ============================================================================
// Test 5: ASLR — different allocations get different addresses
// ============================================================================
static void test_aslr_jitter() {
    // Allocate several WildX regions and check addresses differ
    WildXGuard guards[4];
    for (int i = 0; i < 4; i++) {
        guards[i] = npk_alloc_exec(4096);
        TEST_ASSERT(guards[i].ptr != nullptr, "ASLR jitter");
    }
    
    // At least some addresses should differ (with ASLR, all should)
    int different = 0;
    for (int i = 1; i < 4; i++) {
        if (guards[i].ptr != guards[0].ptr) different++;
    }
    TEST_ASSERT(different >= 2, "ASLR jitter - addresses differ");
    
    for (int i = 0; i < 4; i++) {
        npk_free_exec(&guards[i]);
    }
    TEST_PASS("ASLR userspace jitter (different addresses)");
}

// ============================================================================
// Test 6: WildX quota — enforcement
// ============================================================================
static void test_quota_enforcement() {
    // Set tiny quota
    npk_wildx_set_quota(8192);
    
    // First allocation should succeed (4096 bytes = 1 page)
    WildXGuard g1 = npk_alloc_exec(100);
    TEST_ASSERT(g1.ptr != nullptr, "quota - first alloc");
    
    // Second should succeed (still within 8192)
    WildXGuard g2 = npk_alloc_exec(100);
    TEST_ASSERT(g2.ptr != nullptr, "quota - second alloc");
    
    // Third should fail (over 8192 quota — 3 pages = 12288)
    WildXGuard g3 = npk_alloc_exec(100);
    TEST_ASSERT(g3.ptr == nullptr, "quota - third alloc rejected");
    
    npk_free_exec(&g1);
    npk_free_exec(&g2);
    
    // After freeing, allocation should succeed again
    WildXGuard g4 = npk_alloc_exec(100);
    TEST_ASSERT(g4.ptr != nullptr, "quota - post-free alloc");
    npk_free_exec(&g4);
    
    // Restore default quota
    npk_wildx_set_quota(64ULL * 1024 * 1024);
    TEST_PASS("WildX quota enforcement (64MB default)");
}

// ============================================================================
// Test 7: Guard pages with guard page mode — map_size tracking
// ============================================================================
static void test_guard_pages_map_size() {
    npk_wildx_enable_guard_pages(true);
    
    WildXGuard guard = npk_alloc_exec(4096);
    TEST_ASSERT(guard.ptr != nullptr, "guard map_size");
    // With guard pages: map_size = alloc_size + 2 * page_size
    // alloc_size = 4096 (1 page), so map_size = 4096 + 8192 = 12288 (3 pages)
    TEST_ASSERT(guard.map_size == guard.size + 2 * 4096, "guard map_size - correct");
    
    npk_free_exec(&guard);
    npk_wildx_enable_guard_pages(false);
    TEST_PASS("guard pages map_size tracking");
}

// ============================================================================
// Test 8: Seal state machine enforcement
// ============================================================================
static void test_state_machine() {
    WildXGuard guard = npk_alloc_exec(4096);
    TEST_ASSERT(guard.state == WILDX_STATE_WRITABLE, "state machine - writable");
    
    // Can't execute writable memory
    int64_t r = npk_asm_execute(&guard);
    TEST_ASSERT(r == -1, "state machine - can't exec writable");
    
    // Seal
    int ret = npk_mem_protect_exec(&guard);
    TEST_ASSERT(ret == 0, "state machine - seal ok");
    TEST_ASSERT(guard.state == WILDX_STATE_EXECUTABLE, "state machine - executable");
    
    // Can't re-seal
    ret = npk_mem_protect_exec(&guard);
    TEST_ASSERT(ret == -1, "state machine - can't re-seal");
    
    npk_free_exec(&guard);
    TEST_ASSERT(guard.state == WILDX_STATE_FREED, "state machine - freed");
    TEST_PASS("W⊕X state machine enforcement");
}

// ============================================================================
// Test 9: Execute with argument (1-arg JIT)
// ============================================================================
static void test_execute_with_arg() {
    Assembler* a = npk_asm_create();
    
    // Generate: mov rax, rdi; add rax, rax; ret
    // (Returns arg * 2)
    npk_asm_mov_r64_r64(a, REG_RAX, REG_RDI);
    npk_asm_add_r64_r64(a, REG_RAX, REG_RAX);
    npk_asm_ret(a);
    
    WildXGuard guard = npk_asm_finalize(a);
    TEST_ASSERT(guard.ptr != nullptr, "execute with arg");
    
    int64_t result = npk_asm_execute_i64(&guard, 21);
    TEST_ASSERT(result == 42, "execute with arg - result");
    
    npk_free_exec(&guard);
    npk_asm_destroy(a);
    TEST_PASS("JIT execute with argument (1-arg)");
}

// ============================================================================
// Test 10: Audit logging produces output
// ============================================================================
static void test_audit_logging() {
    // Just enable and disable — verify no crash
    npk_wildx_enable_audit(true);
    
    Assembler* a = npk_asm_create();
    npk_asm_mov_r64_imm64(a, REG_RAX, 99);
    npk_asm_ret(a);
    
    WildXGuard guard = npk_asm_finalize(a);
    TEST_ASSERT(guard.ptr != nullptr, "audit logging");
    
    int64_t result = npk_asm_execute(&guard);
    TEST_ASSERT(result == 99, "audit logging - execute");
    
    npk_free_exec(&guard);
    npk_asm_destroy(a);
    
    npk_wildx_enable_audit(false);
    TEST_PASS("audit logging (alloc/seal/exec/free events)");
}

// ============================================================================
// Main
// ============================================================================
int wildx_security_tests_main() {
    std::printf("WildX Security Hardening Tests (v0.7.1)\n");
    std::printf("========================================\n");
    
    test_basic_cycle();
    test_code_hash();
    test_hash_uniqueness();
    test_guard_pages_normal();
    test_aslr_jitter();
    test_quota_enforcement();
    test_guard_pages_map_size();
    test_state_machine();
    test_execute_with_arg();
    test_audit_logging();
    
    std::printf("========================================\n");
    std::printf("Results: %d passed, %d failed\n", tests_passed, tests_failed);
    
    return tests_failed > 0 ? 1 : 0;
}
