// v0.28.1 — JIT end-to-end smoke helpers.
//
// Hand-rolled support for bug264. Writes a small `int32 add(int32, int32)`
// machine-code body into a wildx page (the caller is expected to have
// already invoked `wildx_alloc` but not yet `wildx_seal`); a second
// helper invokes the resulting function through a typed function-pointer
// cast.
//
// This is the minimum surface needed to prove the JIT loop closes:
// the v0.28.2 slice will wrap both helpers (plus signature dispatch
// and ownership) behind `npklibc/jit.npk`.
//
// x86-64 only this cycle (JIT-DEC-002). ARM64 is documented as future
// work in guide/jit/ but no code or tests for it.

#include <cstdint>
#include <cstddef>
#include <cstring>

extern "C" int npk_jit_install_add_i32(void* page) {
    if (!page) return -1;
    // SysV AMD64: arg1 in EDI, arg2 in ESI, return in EAX.
    //   89 f8     mov  eax, edi
    //   01 f0    add  eax, esi
    //   c3        ret
    static const uint8_t bytes[] = { 0x89, 0xf8, 0x01, 0xf0, 0xc3 };
    std::memcpy(page, bytes, sizeof bytes);
    return 0;
}

extern "C" int npk_jit_call_i32_i32(void* page, int a, int b) {
    if (!page) return 0;
    using Fn = int (*)(int, int);
    Fn fn;
    std::memcpy(&fn, &page, sizeof fn);
    return fn(a, b);
}
