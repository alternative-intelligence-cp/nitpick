; The Nitpick bootstrap runtime floor.
;
; THROWAWAY, alongside the seed (D-085). Hand-written LLVM IR, which is what
; D-015 specifies for the first rung: runtime symbols start as hand-written IR
; and are replaced at a later rung. Real allocation and real I/O arrive with
; nlibc in cycle 0.8.
;
; Freestanding: no libc, no crt. `_start` is provided here, and every kernel
; call is a raw syscall.

target triple = "x86_64-unknown-linux-gnu"

; ---------------------------------------------------------------------------
; Entry point.
;
; Written as module-level assembly rather than as an LLVM function on purpose:
; at _start the stack is NOT laid out the way it is at a normal call -- there is
; no return address -- so a compiler-generated prologue can leave %rsp
; misaligned and any later SSE spill faults. Aligning by hand sidesteps the
; whole class.
; ---------------------------------------------------------------------------

module asm ".globl _start"
module asm "_start:"
module asm "  xorq %rbp, %rbp"
module asm "  movq %rsp, %rdi"         ; the ORIGINAL rsp, before alignment
module asm "  andq $-16, %rsp"
module asm "  callq npk_start"
module asm "  hlt"

; ---------------------------------------------------------------------------
; The process boundary.
;
; `main` takes `cstring[]:argv` and nothing else (D-089): a slice carries its
; length (D-070), so there is no argc to pass and no second copy of a fact to
; disagree with the first.
;
; At _start the stack holds argc, then argv[0..argc-1], then a NULL, then envp.
; That layout is only reachable through the ORIGINAL %rsp, which is why the
; prologue above captures it into %rdi BEFORE the alignment that destroys it.
;
; This runs before `main`, so it allocates the cstring array from the heap
; (0.10.0) and never frees it -- argv outlives everything regardless, and the
; first allocation is also what draws the heap secret.
; ---------------------------------------------------------------------------

declare i32 @main({ ptr, i64 })

define internal void @npk_start(i64 %sp) noreturn {
entry:
  %spp = inttoptr i64 %sp to ptr
  %argc = load i64, ptr %spp
  %argvp = getelementptr i8, ptr %spp, i64 8      ; &argv[0]
  %bytes = mul i64 %argc, 16                      ; sizeof(cstring) = {ptr,i64}
  %buf = call ptr @npk_alloc_internal(i64 %bytes)
  br label %loop

loop:                                             ; preds = %entry, %next
  %i = phi i64 [ 0, %entry ], [ %i1, %next ]
  %done = icmp uge i64 %i, %argc
  br i1 %done, label %ready, label %body

body:                                             ; preds = %loop
  %slotp = getelementptr ptr, ptr %argvp, i64 %i
  %s = load ptr, ptr %slotp
  br label %slen

; A cstring carries its length (D-049), and the kernel hands over a bare
; NUL-terminated pointer -- so the length is measured exactly once, here, at the
; boundary. Nothing downstream scans for a NUL again.
slen:                                             ; preds = %body, %slen
  %k = phi i64 [ 0, %body ], [ %k1, %slen ]
  %cp = getelementptr i8, ptr %s, i64 %k
  %ch = load i8, ptr %cp
  %k1 = add i64 %k, 1
  %atnul = icmp eq i8 %ch, 0
  br i1 %atnul, label %store, label %slen

store:                                            ; preds = %slen
  %pf = getelementptr { ptr, i64 }, ptr %buf, i64 %i, i32 0
  store ptr %s, ptr %pf
  %lf = getelementptr { ptr, i64 }, ptr %buf, i64 %i, i32 1
  store i64 %k, ptr %lf
  br label %next

next:                                             ; preds = %store
  %i1 = add i64 %i, 1
  br label %loop

ready:                                            ; preds = %loop
  %sl0 = insertvalue { ptr, i64 } undef, ptr %buf, 0
  %sl1 = insertvalue { ptr, i64 } %sl0, i64 %argc, 1
  %rc = call i32 @main({ ptr, i64 } %sl1)
  call void @npk_exit(i32 %rc)
  unreachable
}

; ---------------------------------------------------------------------------
; Raw syscall
; ---------------------------------------------------------------------------

; PUBLIC since 0.9.7: the `sys` builtin calls straight through it.
define i64 @npk_sys6(i64 %nr, i64 %a1, i64 %a2, i64 %a3,
                              i64 %a4, i64 %a5, i64 %a6) {
  %r = call i64 asm sideeffect "syscall",
       "={ax},{ax},{di},{si},{dx},{r10},{r8},{r9},~{rcx},~{r11},~{memory},~{dirflag},~{fpsr},~{flags}"
       (i64 %nr, i64 %a1, i64 %a2, i64 %a3, i64 %a4, i64 %a5, i64 %a6)
  ret i64 %r
}

; ---------------------------------------------------------------------------
; The trap route (D-142, cycle 0.9.0): how a RUNTIME FAULT becomes a controlled
; shutdown. Emitted guards (division by zero, INT_MIN/-1 — and every guard a
; later rung adds) call @npk_trap with a code from the D-141 space's runtime
; region below E_EOF:
;
;   -4097  DIV_BY_ZERO       integer / or % with a zero divisor (D-007)
;   -4098  INT_MIN_OVERFLOW  INT_MIN / -1 or INT_MIN % -1 (no defined value;
;                            D-008 refused inventing one, so it traps)
;   -4099  OUT_OF_BOUNDS     a slice/array index past the end, or a range view
;                            that does not fit its source (D-070; 0.9.2)
;   -4100  TBB_ERR           an ERR value reached a bare comparison or a
;                            checked cast out of tbb (D-008 SS5, D-144; 0.9.5)
;   -4101  BAD_STEP          a counted loop's step, not a literal, evaluated
;                            to zero or negative at run time (D-022; 0.9.7)
;   -4102  HEAP_INTEGRITY    double-free, foreign/misaligned/null pointer to
;                            dalloc/ralloc, corrupted header or torn guard,
;                            or a UAF caught by a freed slot's magic (0.10.0)
;   -4103  HEAP_OOM          mmap failed; the trap path allocates nothing,
;                            which is the allocator's own C-3 obligation
;   -4104  HEAP_BAD_REQUEST  negative size, calloc count*size overflow,
;                            ralloc(p, 0), or a non-power-of-two alignment
;   -4106  STALE_HANDLE      NOT A TRAP -- the code an arena get/put/free
;                            RETURNS in Result.error when the handle's
;                            generation no longer matches (D-152); staleness
;                            is a condition the program handles, not a
;                            defect the runtime ends
;   -4105  HEAP_LEAK         exit reached with live `wild` memory -- the
;                            K-semantics rule (CONTROL_REFERENCE 4.6) made
;                            real at 0.10.1; failsafe may clean up with
;                            wild_release_all() and exit positive
;
; The route is trap -> the program's own `failsafe` -> exit with its return.
; A trap RAISED WHILE FAILSAFE IS RUNNING -- failsafe itself double-freeing,
; say -- exits 70 directly rather than recursing into failsafe forever; the
; same flag is what lets failsafe's own exit skip the leak check (the check
; runs once, at the program's exit, never at failsafe's).
; Every program defines `failsafe` (D-013, mandatory), so @npk_failsafe always
; resolves at link. D-014 requires failsafe to return POSITIVE; until 1.3
; injects and verifies that `ensures`, the runtime refuses to report success
; after a fault: a nonpositive return exits 70, the floor's own
; runtime-violation code.
; ---------------------------------------------------------------------------

declare i32 @npk_failsafe(i32)

@npk_in_failsafe = internal global i32 0

define void @npk_trap(i32 %code) noreturn {
  %in = load i32, ptr @npk_in_failsafe
  %re = icmp ne i32 %in, 0
  br i1 %re, label %hard, label %run
hard:
  ; re-entry: failsafe trapped. There is no second handler to hand the
  ; situation to, so this is the one uncatchable stop: exit 70 directly.
  %x = call i64 @npk_sys6(i64 60, i64 70, i64 0, i64 0, i64 0, i64 0, i64 0)
  unreachable
run:
  store i32 1, ptr @npk_in_failsafe
  %r = call i32 @npk_failsafe(i32 %code)
  %bad = icmp sle i32 %r, 0
  %code2 = select i1 %bad, i32 70, i32 %r
  call void @npk_exit(i32 %code2)
  unreachable
}

define void @npk_exit(i32 %code) noreturn {
  ; THE K-SEMANTICS EXIT RULE (0.10.1, D-151): a SUCCESSFUL exit -- code 0,
  ; exactly as CONTROL_REFERENCE 4.6 scopes it -- requires the <wild-live>
  ; set empty. Non-empty routes to failsafe (-4105), which may clean up
  ; (wild_release_all) and exit -- and THAT exit passes, because the
  ; in-failsafe flag is set. A FAILURE exit is already a report of
  ; abnormality and keeps its code: hijacking it with a leak trap would
  ; destroy the error it was raising, and error paths carry no cleanup
  ; obligation (the same reasoning as defer-does-not-run-on-trap, D-014).
  ; The count walks preallocated state only.
  %in = load i32, ptr @npk_in_failsafe
  %skip = icmp ne i32 %in, 0
  %fail = icmp ne i32 %code, 0
  %pass = or i1 %skip, %fail
  br i1 %pass, label %leave, label %check
check:
  %live = call i64 @npk_wild_live_count()
  %leaks = icmp ne i64 %live, 0
  br i1 %leaks, label %trap, label %leave
trap:
  call void @npk_trap(i32 -4105)
  unreachable
leave:
  %c = sext i32 %code to i64
  %r = call i64 @npk_sys6(i64 60, i64 %c, i64 0, i64 0, i64 0, i64 0, i64 0)
  unreachable
}

; ---------------------------------------------------------------------------
; fmod / fmodf (0.9.4, D-143): `frem` on flt64/flt32 lowers to these libcalls,
; which libm normally provides -- the floor provides them as hand-written IR.
;
; THE RESULT IS EXACT, which is what makes a short implementation correct:
; align |b| up to |a| by exact power-of-two doublings, subtract (Sterbenz: the
; subtrahend is within [|a|/2, |a|], so the subtraction is exact), repeat. No
; rounding ever occurs, so this IS IEEE fmod, not an approximation of it.
; Specials per IEEE: NaN in, |a| infinite, or b zero -> NaN; |b| infinite ->
; a unchanged; the result carries a's sign (llvm.copysign; the fabs/copysign
; intrinsics lower inline -- no symbol survives to the object).
; ---------------------------------------------------------------------------

declare double @llvm.fabs.f64(double)
declare float @llvm.fabs.f32(float)
declare double @llvm.copysign.f64(double, double)
declare float @llvm.copysign.f32(float, float)

define double @fmod(double %a, double %b) {
entry:
  %ab0 = call double @llvm.fabs.f64(double %a)
  %bb = call double @llvm.fabs.f64(double %b)
  %anan = fcmp uno double %a, %a
  %bnan = fcmp uno double %b, %b
  %bzero = fcmp oeq double %bb, 0.0
  %ainf = fcmp oeq double %ab0, 0x7FF0000000000000
  %n1 = or i1 %anan, %bnan
  %n2 = or i1 %n1, %bzero
  %bad = or i1 %n2, %ainf
  br i1 %bad, label %retnan, label %chk
retnan:
  ret double 0x7FF8000000000000
chk:
  %binf = fcmp oeq double %bb, 0x7FF0000000000000
  br i1 %binf, label %reta, label %outer
reta:
  ret double %a
outer:
  %ab = phi double [ %ab0, %chk ], [ %abn, %step ]
  %small = fcmp olt double %ab, %bb
  br i1 %small, label %fin, label %scale
scale:
  %c = phi double [ %bb, %outer ], [ %c2, %grow ]
  %c2 = fmul double %c, 2.0
  %fits = fcmp ole double %c2, %ab
  br i1 %fits, label %grow, label %step
grow:
  br label %scale
step:
  %abn = fsub double %ab, %c
  br label %outer
fin:
  %r = call double @llvm.copysign.f64(double %ab, double %a)
  ret double %r
}

define float @fmodf(float %a, float %b) {
entry:
  %ab0 = call float @llvm.fabs.f32(float %a)
  %bb = call float @llvm.fabs.f32(float %b)
  %anan = fcmp uno float %a, %a
  %bnan = fcmp uno float %b, %b
  %bzero = fcmp oeq float %bb, 0.0
  %ainf = fcmp oeq float %ab0, 0x7FF0000000000000
  %n1 = or i1 %anan, %bnan
  %n2 = or i1 %n1, %bzero
  %bad = or i1 %n2, %ainf
  br i1 %bad, label %retnan, label %chk
retnan:
  ret float 0x7FF8000000000000
chk:
  %binf = fcmp oeq float %bb, 0x7FF0000000000000
  br i1 %binf, label %reta, label %outer
reta:
  ret float %a
outer:
  %ab = phi float [ %ab0, %chk ], [ %abn, %step ]
  %small = fcmp olt float %ab, %bb
  br i1 %small, label %fin, label %scale
scale:
  %c = phi float [ %bb, %outer ], [ %c2, %grow ]
  %c2 = fmul float %c, 2.0
  %fits = fcmp ole float %c2, %ab
  br i1 %fits, label %grow, label %step
grow:
  br label %scale
step:
  %abn = fsub float %ab, %c
  br label %outer
fin:
  %r = call float @llvm.copysign.f32(float %ab, float %a)
  ret float %r
}

; ---------------------------------------------------------------------------
; The i128 division family (0.9.3, D-011 SS2): the ONE libcall class llc emits
; for wide integers -- sdiv/udiv/srem/urem at exactly 128 bits call these four
; symbols (measured on this toolchain; division at 256 bits and beyond expands
; inline, all the way to an executed udiv i4096 probe). compiler-rt provides
; them in C; the floor provides them as hand-written IR under the same TCB
; discipline as everything else here. Shift-subtract long division, 128
; iterations, dependency-free.
;
; A zero divisor never reaches these from emitted code -- the D-142 guard traps
; first -- but the symbols stay TOTAL: b == 0 yields an all-ones quotient and
; r == a from the loop's own arithmetic, a defined result, never UB. INT128_MIN
; negates to itself under two's complement, and its unsigned reading 2^127 is
; exactly what the unsigned core needs, so the sign wrappers are total too.
; ---------------------------------------------------------------------------

define internal { i128, i128 } @npk_udivmod128(i128 %a, i128 %b) {
entry:
  br label %loop
loop:
  %i = phi i32 [ 127, %entry ], [ %inext, %next ]
  %q = phi i128 [ 0, %entry ], [ %q2, %next ]
  %r = phi i128 [ 0, %entry ], [ %r2, %next ]
  %r1 = shl i128 %r, 1
  %iw = zext i32 %i to i128
  %ab = lshr i128 %a, %iw
  %ab1 = and i128 %ab, 1
  %rb = or i128 %r1, %ab1
  %ge = icmp uge i128 %rb, %b
  %rs = sub i128 %rb, %b
  %r2 = select i1 %ge, i128 %rs, i128 %rb
  %qb = shl i128 1, %iw
  %qs = or i128 %q, %qb
  %q2 = select i1 %ge, i128 %qs, i128 %q
  %done = icmp eq i32 %i, 0
  %inext = sub i32 %i, 1
  br i1 %done, label %out, label %next
next:
  br label %loop
out:
  %p0 = insertvalue { i128, i128 } undef, i128 %q2, 0
  %p1 = insertvalue { i128, i128 } %p0, i128 %r2, 1
  ret { i128, i128 } %p1
}

define i128 @__udivti3(i128 %a, i128 %b) {
  %qr = call { i128, i128 } @npk_udivmod128(i128 %a, i128 %b)
  %q = extractvalue { i128, i128 } %qr, 0
  ret i128 %q
}

define i128 @__umodti3(i128 %a, i128 %b) {
  %qr = call { i128, i128 } @npk_udivmod128(i128 %a, i128 %b)
  %r = extractvalue { i128, i128 } %qr, 1
  ret i128 %r
}

define i128 @__divti3(i128 %a, i128 %b) {
  %an = icmp slt i128 %a, 0
  %na = sub i128 0, %a
  %aa = select i1 %an, i128 %na, i128 %a
  %bn = icmp slt i128 %b, 0
  %nb = sub i128 0, %b
  %ba = select i1 %bn, i128 %nb, i128 %b
  %qr = call { i128, i128 } @npk_udivmod128(i128 %aa, i128 %ba)
  %q = extractvalue { i128, i128 } %qr, 0
  %sx = xor i1 %an, %bn
  %nq = sub i128 0, %q
  %qq = select i1 %sx, i128 %nq, i128 %q
  ret i128 %qq
}

; C truncated-division semantics, which is what srem lowers against: the
; remainder carries the DIVIDEND's sign.
define i128 @__modti3(i128 %a, i128 %b) {
  %an = icmp slt i128 %a, 0
  %na = sub i128 0, %a
  %aa = select i1 %an, i128 %na, i128 %a
  %bn = icmp slt i128 %b, 0
  %nb = sub i128 0, %b
  %ba = select i1 %bn, i128 %nb, i128 %b
  %qr = call { i128, i128 } @npk_udivmod128(i128 %aa, i128 %ba)
  %r = extractvalue { i128, i128 } %qr, 1
  %nr = sub i128 0, %r
  %rr = select i1 %an, i128 %nr, i128 %r
  ret i128 %rr
}

; ---------------------------------------------------------------------------
; The fd quartet (D-141): open / close / read / write, each ONE syscall,
; faithfully -- the floor is the syscall surface (D-051), so a short write is
; returned, not retried. Discipline lives above it in ordinary Nitpick
; (lib/nio.npk): write-all loops, line endings (D-050), buffering (D-076).
;
; An errored Result carries a zeroed value slot. For `open` that zero is i32 0,
; which reads as stdin -- inert, because reading a tainted `Result.value` is
; refused at compile time; the zero is defence in depth, not the contract.
; ---------------------------------------------------------------------------

define { i32, i32 } @npk_open({ ptr, i64 } %path, i64 %flags, i64 %mode) {
  %pp = extractvalue { ptr, i64 } %path, 0
  %ppi = ptrtoint ptr %pp to i64
  ; openat(AT_FDCWD = -100, path, flags, mode)
  %r = call i64 @npk_sys6(i64 257, i64 -100, i64 %ppi, i64 %flags, i64 %mode, i64 0, i64 0)
  %bad = icmp slt i64 %r, 0
  br i1 %bad, label %err, label %ok
ok:
  %f = trunc i64 %r to i32
  %o0 = insertvalue { i32, i32 } undef, i32 %f, 0
  %o1 = insertvalue { i32, i32 } %o0, i32 0, 1
  ret { i32, i32 } %o1
err:
  %c = trunc i64 %r to i32
  %e0 = insertvalue { i32, i32 } undef, i32 0, 0
  %e1 = insertvalue { i32, i32 } %e0, i32 %c, 1
  ret { i32, i32 } %e1
}

define { i32 } @npk_close(i32 %fd) {
  %f = sext i32 %fd to i64
  %r = call i64 @npk_sys6(i64 3, i64 %f, i64 0, i64 0, i64 0, i64 0, i64 0)
  %bad = icmp slt i64 %r, 0
  br i1 %bad, label %err, label %ok
ok:
  ret { i32 } zeroinitializer
err:
  %c = trunc i64 %r to i32
  %e0 = insertvalue { i32 } undef, i32 %c, 0
  ret { i32 } %e0
}

define { i64, i32 } @npk_read(i32 %fd, ptr %buf, i64 %cap) {
entry:
  ; Zero asked is zero delivered, NOT end-of-input: E_EOF must mean the stream
  ; ended, never that the caller handed over an empty buffer.
  %none = icmp eq i64 %cap, 0
  br i1 %none, label %zero, label %go
zero:
  ret { i64, i32 } zeroinitializer
go:
  %f = sext i32 %fd to i64
  %p = ptrtoint ptr %buf to i64
  %r = call i64 @npk_sys6(i64 0, i64 %f, i64 %p, i64 %cap, i64 0, i64 0, i64 0)
  %bad = icmp slt i64 %r, 0
  br i1 %bad, label %err, label %sift
sift:
  %eof = icmp eq i64 %r, 0
  br i1 %eof, label %ateof, label %ok
ateof:
  ; E_EOF = -4096 (D-141): end-of-input is an error code, never a sentinel in
  ; the value channel (D-075) -- at the floor exactly as it will be in the
  ; Stream trait above it.
  %z0 = insertvalue { i64, i32 } undef, i64 0, 0
  %z1 = insertvalue { i64, i32 } %z0, i32 -4096, 1
  ret { i64, i32 } %z1
ok:
  %k0 = insertvalue { i64, i32 } undef, i64 %r, 0
  %k1 = insertvalue { i64, i32 } %k0, i32 0, 1
  ret { i64, i32 } %k1
err:
  %c = trunc i64 %r to i32
  %e0 = insertvalue { i64, i32 } undef, i64 0, 0
  %e1 = insertvalue { i64, i32 } %e0, i32 %c, 1
  ret { i64, i32 } %e1
}

define { i64, i32 } @npk_write(i32 %fd, ptr %buf, i64 %len) {
  %f = sext i32 %fd to i64
  %p = ptrtoint ptr %buf to i64
  %r = call i64 @npk_sys6(i64 1, i64 %f, i64 %p, i64 %len, i64 0, i64 0, i64 0)
  %bad = icmp slt i64 %r, 0
  br i1 %bad, label %err, label %ok
ok:
  %k0 = insertvalue { i64, i32 } undef, i64 %r, 0
  %k1 = insertvalue { i64, i32 } %k0, i32 0, 1
  ret { i64, i32 } %k1
err:
  %c = trunc i64 %r to i32
  %e0 = insertvalue { i64, i32 } undef, i64 0, 0
  %e1 = insertvalue { i64, i32 } %e0, i32 %c, 1
  ret { i64, i32 } %e1
}

; ---------------------------------------------------------------------------
; string -> cstring, and reading a file by path.
;
; `to_cstring` FAILS ON AN INTERIOR NUL, and that is the entire point of D-049.
; A Nitpick `string` is {ptr, len, cap} and length-carrying, so an embedded 0u8
; is just a byte. Converting such a string by copying up to the first NUL
; silently truncates it, and the caller has no indication -- so a path that was
; validated and a path that was opened become DIFFERENT PATHS.
;
;     attacker supplies:   "avatar.png\0.sh"
;     validator sees:      len 14, suffix ".sh"
;     kernel sees:         "avatar.png"
;
; That is the poison-NUL class, and it is why `read_file` takes a cstring rather
; than accepting a string and terminating it itself. The conversion is explicit,
; fallible, and the caller handles the failure like any other.
;
; Error codes (D-141): the error slot carries NEGATIVE system codes -- the
; kernel's own return, exactly as the syscall delivered it, so a caller can
; tell ENOENT (-2) from EACCES (-13) without a second mechanism. Conditions
; the floor detects itself reuse the kernel's vocabulary (-22 EINVAL for the
; interior NUL here, -34 ERANGE for a slice out of range); the one condition
; errno has no word for, end-of-input, is E_EOF = -4096 -- the first code past
; the kernel's own error space (errno stops at 4095), so it can never collide.
; Positive codes are the program's (`fail`), and 0 is ok.
; ---------------------------------------------------------------------------

define { { ptr, i64 }, i32 } @npk_to_cstring({ ptr, i64, i64 } %s) {
entry:
  %p = extractvalue { ptr, i64, i64 } %s, 0
  %n = extractvalue { ptr, i64, i64 } %s, 1
  br label %scan

scan:                                     ; preds = %entry, %step
  %i = phi i64 [ 0, %entry ], [ %i1, %step ]
  %done = icmp uge i64 %i, %n
  br i1 %done, label %copy, label %check

check:                                    ; preds = %scan
  %cp = getelementptr i8, ptr %p, i64 %i
  %ch = load i8, ptr %cp
  %isnul = icmp eq i8 %ch, 0
  br i1 %isnul, label %interior, label %step

step:                                     ; preds = %check
  %i1 = add i64 %i, 1
  br label %scan

copy:                                     ; preds = %scan
  ; One byte more than the length, for the terminator the kernel needs.
  %sz = add i64 %n, 1
  %buf = call ptr @npk_alloc_internal(i64 %sz)
  call ptr @memcpy(ptr %buf, ptr %p, i64 %n)
  %end = getelementptr i8, ptr %buf, i64 %n
  store i8 0, ptr %end
  %c0 = insertvalue { ptr, i64 } undef, ptr %buf, 0
  %c1 = insertvalue { ptr, i64 } %c0, i64 %n, 1
  %r0 = insertvalue { { ptr, i64 }, i32 } undef, { ptr, i64 } %c1, 0
  %r1 = insertvalue { { ptr, i64 }, i32 } %r0, i32 0, 1
  ret { { ptr, i64 }, i32 } %r1

interior:                                 ; preds = %check
  %e0 = insertvalue { ptr, i64 } undef, ptr null, 0
  %e1 = insertvalue { ptr, i64 } %e0, i64 0, 1
  %q0 = insertvalue { { ptr, i64 }, i32 } undef, { ptr, i64 } %e1, 0
  %q1 = insertvalue { { ptr, i64 }, i32 } %q0, i32 -22, 1
  ret { { ptr, i64 }, i32 } %q1
}

; Can this path be opened for reading?
;
; Ambiguity-is-an-error (BUILD_REFERENCE section 3) means the resolver must probe
; EVERY dependency root before deciding, not stop at the first hit -- a resolver
; that returns early cannot report the second candidate. Probing by full read
; would be O(file size) per root, and most probes miss.
;
; It tests READABILITY rather than existence, which is the question actually
; being asked: a file that exists and cannot be opened is not a candidate.
define i8 @npk_path_exists({ ptr, i64 } %path) {
entry:
  %pp = extractvalue { ptr, i64 } %path, 0
  %ppi = ptrtoint ptr %pp to i64
  %fd = call i64 @npk_sys6(i64 257, i64 -100, i64 %ppi, i64 0, i64 0, i64 0, i64 0)
  %bad = icmp slt i64 %fd, 0
  br i1 %bad, label %no, label %yes

yes:
  %cr = call i64 @npk_sys6(i64 3, i64 %fd, i64 0, i64 0, i64 0, i64 0, i64 0)
  ret i8 1

no:
  ret i8 0
}

; Write a whole buffer to a path, replacing what was there -- read_file's
; mirror, and the routine `npkc -o` stands on (0.8.3). openat/write/close live
; INSIDE it as one auditable body rather than as three exposed symbols; the
; fd-granular primitives arrive with D-050's line discipline, where a held-open
; descriptor is the point.
;
; A SHORT WRITE IS NOT FAILURE. write(2) may take fewer bytes than offered --
; signals, pipes, quotas -- so the loop advances by what was ACCEPTED and only a
; negative return is an error. Treating a short write as success is the classic
; way to truncate a file and report victory; the error code travels out as the
; Result's tbb32.
; Byte equality over two strings -- the primitive the comptime folder mirrors
; (fold_string_builtin) and the one string predicate worth a runtime symbol:
; everything nlibc builds (contains, starts_with, index_of) is ordinary Nitpick
; over byte access, but equality is called from generated code paths where a
; call beats an inlined loop for auditability.
define i8 @npk_string_equals({ ptr, i64, i64 } %a, { ptr, i64, i64 } %b) {
entry:
  %al = extractvalue { ptr, i64, i64 } %a, 1
  %bl = extractvalue { ptr, i64, i64 } %b, 1
  %same = icmp eq i64 %al, %bl
  br i1 %same, label %scan, label %no

scan:
  %ap = extractvalue { ptr, i64, i64 } %a, 0
  %bp = extractvalue { ptr, i64, i64 } %b, 0
  br label %loop

loop:
  %i = phi i64 [ 0, %scan ], [ %i2, %next ]
  %done = icmp eq i64 %i, %al
  br i1 %done, label %yes, label %cmp

cmp:
  %pa = getelementptr i8, ptr %ap, i64 %i
  %pb = getelementptr i8, ptr %bp, i64 %i
  %ca = load i8, ptr %pa
  %cb = load i8, ptr %pb
  %eq = icmp eq i8 %ca, %cb
  br i1 %eq, label %next, label %no

next:
  %i2 = add i64 %i, 1
  br label %loop

yes:
  ret i8 1

no:
  ret i8 0
}

define { i32 } @npk_write_file({ ptr, i64 } %path, { ptr, i64, i64 } %data) {
entry:
  %pp = extractvalue { ptr, i64 } %path, 0
  %ppi = ptrtoint ptr %pp to i64
  ; openat(AT_FDCWD, path, O_WRONLY|O_CREAT|O_TRUNC, 0644). 577 = 1|64|512.
  %fd = call i64 @npk_sys6(i64 257, i64 -100, i64 %ppi, i64 577, i64 420, i64 0, i64 0)
  %obad = icmp slt i64 %fd, 0
  br i1 %obad, label %openfail, label %wstart

wstart:
  %base = extractvalue { ptr, i64, i64 } %data, 0
  %total = extractvalue { ptr, i64, i64 } %data, 1
  br label %wloop

wloop:
  %off = phi i64 [ 0, %wstart ], [ %off2, %wnext ]
  %left = sub i64 %total, %off
  %done = icmp eq i64 %left, 0
  br i1 %done, label %wclose, label %wone

wone:
  %at = getelementptr i8, ptr %base, i64 %off
  %ati = ptrtoint ptr %at to i64
  %n = call i64 @npk_sys6(i64 1, i64 %fd, i64 %ati, i64 %left, i64 0, i64 0, i64 0)
  %wbad = icmp slt i64 %n, 0
  br i1 %wbad, label %writefail, label %wnext

wnext:
  %off2 = add i64 %off, %n
  br label %wloop

wclose:
  %c = call i64 @npk_sys6(i64 3, i64 %fd, i64 0, i64 0, i64 0, i64 0, i64 0)
  %cbad = icmp slt i64 %c, 0
  br i1 %cbad, label %closefail, label %ok

ok:
  ret { i32 } zeroinitializer

openfail:
  %oec = trunc i64 %fd to i32
  %or0 = insertvalue { i32 } undef, i32 %oec, 0
  ret { i32 } %or0

writefail:
  ; close best-effort: the write's errno is the story, not the close's.
  %ce = call i64 @npk_sys6(i64 3, i64 %fd, i64 0, i64 0, i64 0, i64 0, i64 0)
  %wec = trunc i64 %n to i32
  %wr0 = insertvalue { i32 } undef, i32 %wec, 0
  ret { i32 } %wr0

closefail:
  ; A FAILED CLOSE IS A FAILED WRITE. Buffered-at-the-kernel errors surface
  ; here, and reporting success past one is reporting bytes that may not exist.
  %lec = trunc i64 %c to i32
  %lr0 = insertvalue { i32 } undef, i32 %lec, 0
  ret { i32 } %lr0
}

define { { ptr, i64, i64 }, i32 } @npk_read_file({ ptr, i64 } %path) {
entry:
  %pp = extractvalue { ptr, i64 } %path, 0
  %ppi = ptrtoint ptr %pp to i64
  ; openat(AT_FDCWD, path, O_RDONLY, 0). AT_FDCWD is -100.
  %fd = call i64 @npk_sys6(i64 257, i64 -100, i64 %ppi, i64 0, i64 0, i64 0, i64 0)
  %bad = icmp slt i64 %fd, 0
  br i1 %bad, label %openfail, label %start

start:                                    ; preds = %entry
  %buf0 = call ptr @npk_alloc_internal(i64 65536)
  br label %loop

loop:                                     ; preds = %start, %iter
  %buf = phi ptr [ %buf0, %start ], [ %rbuf, %iter ]
  %cap = phi i64 [ 65536, %start ], [ %rcap, %iter ]
  %len = phi i64 [ 0, %start ], [ %len_n, %iter ]
  %room = sub i64 %cap, %len
  %full = icmp eq i64 %room, 0
  br i1 %full, label %grow, label %read

grow:                                     ; preds = %loop
  %cap2 = mul i64 %cap, 2
  %buf2 = call ptr @npk_alloc_internal(i64 %cap2)
  call ptr @memcpy(ptr %buf2, ptr %buf, i64 %len)
  br label %read

read:                                     ; preds = %loop, %grow
  %rbuf = phi ptr [ %buf, %loop ], [ %buf2, %grow ]
  %rcap = phi i64 [ %cap, %loop ], [ %cap2, %grow ]
  %rroom = sub i64 %rcap, %len
  %dst = getelementptr i8, ptr %rbuf, i64 %len
  %dsti = ptrtoint ptr %dst to i64
  %n = call i64 @npk_sys6(i64 0, i64 %fd, i64 %dsti, i64 %rroom, i64 0, i64 0, i64 0)
  %rbad = icmp slt i64 %n, 0
  br i1 %rbad, label %readfail, label %check

check:                                    ; preds = %read
  ; A SHORT READ IS NOT END OF FILE. Treating it as one is the classic way to
  ; truncate a file silently, and here it would mean parsing a prefix of a module
  ; and reporting success.
  %eof = icmp eq i64 %n, 0
  br i1 %eof, label %done, label %iter

iter:                                     ; preds = %check
  %len_n = add i64 %len, %n
  br label %loop

done:                                     ; preds = %check
  %cr = call i64 @npk_sys6(i64 3, i64 %fd, i64 0, i64 0, i64 0, i64 0, i64 0)
  %s0 = insertvalue { ptr, i64, i64 } undef, ptr %rbuf, 0
  %s1 = insertvalue { ptr, i64, i64 } %s0, i64 %len, 1
  %s2 = insertvalue { ptr, i64, i64 } %s1, i64 %rcap, 2
  %o0 = insertvalue { { ptr, i64, i64 }, i32 } undef, { ptr, i64, i64 } %s2, 0
  %o1 = insertvalue { { ptr, i64, i64 }, i32 } %o0, i32 0, 1
  ret { { ptr, i64, i64 }, i32 } %o1

readfail:                                 ; preds = %read
  ; Close before returning. The process is about to report an error, not exit,
  ; and a driver that reports many missing files must not leak a descriptor each
  ; time it succeeds partway.
  %cr2 = call i64 @npk_sys6(i64 3, i64 %fd, i64 0, i64 0, i64 0, i64 0, i64 0)
  %rerr32 = trunc i64 %n to i32
  br label %fail

openfail:                                 ; preds = %entry
  %oerr32 = trunc i64 %fd to i32
  br label %fail

fail:                                     ; preds = %readfail, %openfail
  %code = phi i32 [ %rerr32, %readfail ], [ %oerr32, %openfail ]
  %f0 = insertvalue { ptr, i64, i64 } undef, ptr null, 0
  %f1 = insertvalue { ptr, i64, i64 } %f0, i64 0, 1
  %f2 = insertvalue { ptr, i64, i64 } %f1, i64 0, 2
  %g0 = insertvalue { { ptr, i64, i64 }, i32 } undef, { ptr, i64, i64 } %f2, 0
  %g1 = insertvalue { { ptr, i64, i64 }, i32 } %g0, i32 %code, 1
  ret { { ptr, i64, i64 }, i32 } %g1
}

; ---------------------------------------------------------------------------
; Reading standard input, whole.
;
; The frontend needs to run on REAL FILES for the rejection suite to mean what
; D-085 says it means -- every file there must PARSE and be refused later, and
; only the real parser can demonstrate that. Reading fd 0 rather than opening a
; path is deliberate: it needs no argv (which `_start` does not pass, and which
; would raise the separate question of what `main`'s signature is), and it does
; not pre-commit the file-opening API that cycle 0.3's module loader will want.
;
; SUBSET_1 section 1.6 has listed open/read/close in the runtime floor since 0.0.4
; while none of them existed, because nothing had needed one yet. This is the
; first, and the entry there now says which.
;
; Grows by doubling and reads until read(2) returns 0. A short read is NOT
; end-of-file -- treating it as one is the classic way to silently truncate a
; file, and here it would mean parsing a prefix of a test and reporting success.
; ---------------------------------------------------------------------------

define { { ptr, i64, i64 }, i32 } @npk_read_stdin() {
entry:
  %buf0 = call ptr @npk_alloc_internal(i64 65536)
  br label %loop

loop:                                     ; preds = %entry, %iter
  %buf = phi ptr [ %buf0, %entry ], [ %rbuf, %iter ]
  %cap = phi i64 [ 65536, %entry ], [ %rcap, %iter ]
  %len = phi i64 [ 0, %entry ], [ %len_n, %iter ]
  %room = sub i64 %cap, %len
  %full = icmp eq i64 %room, 0
  br i1 %full, label %grow, label %read

grow:                                     ; preds = %loop
  ; Growing copies into a fresh block; since 0.10.0 the abandoned one is
  ; ralloc's to free, not a permanent loss.
  %cap2 = mul i64 %cap, 2
  %buf2 = call ptr @npk_alloc_internal(i64 %cap2)
  call ptr @memcpy(ptr %buf2, ptr %buf, i64 %len)
  br label %read

read:                                     ; preds = %loop, %grow
  %rbuf = phi ptr [ %buf, %loop ], [ %buf2, %grow ]
  %rcap = phi i64 [ %cap, %loop ], [ %cap2, %grow ]
  %rroom = sub i64 %rcap, %len
  %dst = getelementptr i8, ptr %rbuf, i64 %len
  %dsti = ptrtoint ptr %dst to i64
  %n = call i64 @npk_sys6(i64 0, i64 0, i64 %dsti, i64 %rroom, i64 0, i64 0, i64 0)
  %failed = icmp slt i64 %n, 0
  br i1 %failed, label %err, label %check

check:                                    ; preds = %read
  %eof = icmp eq i64 %n, 0
  br i1 %eof, label %done, label %iter

iter:                                     ; preds = %check
  %len_n = add i64 %len, %n
  br label %loop

done:                                     ; preds = %check
  %s0 = insertvalue { ptr, i64, i64 } undef, ptr %rbuf, 0
  %s1 = insertvalue { ptr, i64, i64 } %s0, i64 %len, 1
  %s2 = insertvalue { ptr, i64, i64 } %s1, i64 %rcap, 2
  %r0 = insertvalue { { ptr, i64, i64 }, i32 } undef, { ptr, i64, i64 } %s2, 0
  %r1 = insertvalue { { ptr, i64, i64 }, i32 } %r0, i32 0, 1
  ret { { ptr, i64, i64 }, i32 } %r1

err:
  ; An errored Result carries a zeroed value, so a caller that unwraps without
  ; checking gets an empty string rather than a pointer into nothing.
  %e0 = insertvalue { ptr, i64, i64 } undef, ptr null, 0
  %e1 = insertvalue { ptr, i64, i64 } %e0, i64 0, 1
  %e2 = insertvalue { ptr, i64, i64 } %e1, i64 0, 2
  %q0 = insertvalue { { ptr, i64, i64 }, i32 } undef, { ptr, i64, i64 } %e2, 0
  %qc = trunc i64 %n to i32
  %q1 = insertvalue { { ptr, i64, i64 }, i32 } %q0, i32 %qc, 1
  ret { { ptr, i64, i64 }, i32 } %q1
}

; ---------------------------------------------------------------------------
; THE HEAP (0.10.0, D-150). Size-classed slab chunks + a large-block mmap path,
; with a real dalloc. This replaces the seed-era bump floor; the seed still
; BUILDS the compiler (C-13), but everything it builds now links a runtime that
; actually frees.
;
; SAFETY SHAPE, stated once and obeyed everywhere below:
;
;   1. OUT-OF-BAND METADATA. The allocator's control state -- chunk bitmaps,
;      the chunk table, the large table -- never lives inside user-reachable
;      payload. The prototype's free-list-in-payload design was consulted and
;      REJECTED: a use-after-free there corrupts the allocator's own control
;      data, and detection is probabilistic. Here a UAF can only touch payload
;      and canaries; the bitmap says what is free, deterministically.
;   2. VALIDATE BEFORE DEREFERENCE. dalloc/ralloc prove a pointer lies inside
;      memory THIS allocator mapped (sorted chunk table / large table, binary
;      search) before reading a single byte through it. A garbage pointer is a
;      trap, never a wild load -- the allocator itself must not be the thing
;      that segfaults.
;   3. EVERY FAILURE IS A TRAP, NEVER UB. Double-free, foreign/misaligned
;      pointer, corrupted header, torn guard: npk_trap(-4102) -> failsafe.
;      OOM: npk_trap(-4103). Bad request (negative size, calloc overflow,
;      ralloc(p,0), bad alignment): npk_trap(-4104). The trap path allocates
;      nothing (the C-3 obligation): npk_trap -> failsafe runs on preallocated
;      state, because OOM handling runs when allocation just failed.
;
; BLOCK SHAPE. Every block: [ size i64 | magic i64 | payload... ]. The 16-byte
; header keeps payloads 16-aligned (the floor's 0.7.3 discipline, still what
; bounds ralloc's copy). Magic words are secret-keyed and ADDRESS-keyed:
;   live block (runtime-internal / managed-regime storage: string bodies,
;                argv, file buffers -- OUTSIDE the <wild-live> set, their RAII
;                lands with the managed lowering):
;                 secret ^ blockaddr ^ K_LIVE
;   live block (WILD regime -- the alloc/calloc/ralloc/aalloc builtins; what
;                the exit-time leak check counts, D-151):
;                 secret ^ blockaddr ^ K_LIVEW
;   freed slot:   secret ^ blockaddr ^ K_FREED
;   large block:  secret ^ blockaddr ^ K_LARGE
;   chunk header: secret ^ chunkaddr ^ K_CHUNK
;   guards:       secret ^ guardaddr ^ K_GUARD  (two words per guard)
; The secret comes from getrandom(2) at first allocation, so a forged or
; replayed magic does not survive across runs. In a slab, the NEXT block's
; header is the overrun canary for its neighbour; the last slot is closed by a
; 16-byte tail guard, checked on every free in the chunk. Large blocks carry
; their own 16-byte footer guard.
;
; DOUBLE-FREE is deterministic, not probabilistic: the chunk bitmap holds one
; bit per slot (set = free), so the second dalloc of a slot finds its bit
; already set and traps. This is defense in depth -- D-119 already refuses a
; double-free of a TRACKED binding at compile time; this catches pointers the
; static analysis cannot follow (#wild_ptr fabrications, stored wild aliases).
; A chunk WATERMARK (highest slot ever handed out) additionally traps a free
; of a never-allocated slot, and a recycled slot's header must still carry its
; FREED magic when popped -- a UAF that scribbled a freed block's header traps
; on the next allocation from that slot.
;
; SIZE CLASSES (chunk = 64 KiB, 64 KiB-aligned so block->chunk is one AND):
;   class  = payload bytes; stride = class + 16 header
;   layout = [64B chunk header][bitmap W x i64][pad to 16][slots][16B guard]
; The tables below are compiled-in constants; the generator formula is
;   slots = max S with align16(64 + 8*ceil(S/64)) + S*(class+16) + 16 <= 65536
; and each row was computed exactly (see meta/roadmap/0.10/0.10.0.md).
;
; CHUNK HEADER (64 bytes): +0 magic  +8 class-index  +16 slot-count
;   +24 free-count  +32 scan-hint(word)  +40 next  +48 prev  +56 watermark.
; Chunks sit on per-class PARTIAL/FULL doubly-linked lists (O(1) transitions);
; the sorted chunk table answers "is this address one of my chunks" in
; O(log chunks) with no dereference.
;
; SINGLE-THREADED AT THIS RUNG, deliberately: programs have one thread until
; 1.1, and the lock discipline lands with 1.1's executor work (stated in
; MEMORY_REFERENCE; the shared_arena subcycle 0.10.4 is the concurrent piece).
; ---------------------------------------------------------------------------

@npk_hsec = internal global i64 0
@npk_cls_size = internal constant [14 x i64] [i64 16, i64 32, i64 48, i64 64, i64 96, i64 128, i64 192, i64 256, i64 384, i64 512, i64 768, i64 1024, i64 1536, i64 2048]
@npk_cls_slots = internal constant [14 x i64] [i64 2037, i64 1360, i64 1020, i64 816, i64 583, i64 454, i64 314, i64 240, i64 163, i64 123, i64 83, i64 62, i64 42, i64 31]
@npk_cls_bmw = internal constant [14 x i64] [i64 32, i64 22, i64 16, i64 13, i64 10, i64 8, i64 5, i64 4, i64 3, i64 2, i64 2, i64 1, i64 1, i64 1]
@npk_cls_data = internal constant [14 x i64] [i64 320, i64 240, i64 192, i64 176, i64 144, i64 128, i64 112, i64 96, i64 96, i64 80, i64 80, i64 80, i64 80, i64 80]
@npk_cls_guard = internal constant [14 x i64] [i64 65504, i64 65520, i64 65472, i64 65456, i64 65440, i64 65504, i64 65424, i64 65376, i64 65296, i64 65024, i64 65152, i64 64560, i64 65264, i64 64064]
@npk_cls_part = internal global [14 x i64] zeroinitializer
@npk_cls_full = internal global [14 x i64] zeroinitializer
@npk_chtab = internal global i64 0
@npk_chtab_cap = internal global i64 0
@npk_chtab_len = internal global i64 0
@npk_lgtab = internal global i64 0
@npk_lgtab_cap = internal global i64 0
@npk_lgtab_len = internal global i64 0

; --- the three traps (D-141 region: -4102 integrity, -4103 oom, -4104 request)

define internal void @npk_heap_bad() noreturn {
  call void @npk_trap(i32 -4102)
  unreachable
}

define internal void @npk_heap_oom() noreturn {
  call void @npk_trap(i32 -4103)
  unreachable
}

define internal void @npk_heap_badreq() noreturn {
  call void @npk_trap(i32 -4104)
  unreachable
}

; --- magic formulas, one definition per role so no site can drift -----------

define internal i64 @npk_m_chunk(i64 %a) {
  %s = load i64, ptr @npk_hsec
  %x = xor i64 %s, %a
  %m = xor i64 %x, -4059465807380287133
  ret i64 %m
}

define internal i64 @npk_m_live(i64 %a) {
  %s = load i64, ptr @npk_hsec
  %x = xor i64 %s, %a
  %m = xor i64 %x, 5885026092677834074
  ret i64 %m
}

define internal i64 @npk_m_freed(i64 %a) {
  %s = load i64, ptr @npk_hsec
  %x = xor i64 %s, %a
  %m = xor i64 %x, -581298601873003291
  ret i64 %m
}

define internal i64 @npk_m_large(i64 %a) {
  %s = load i64, ptr @npk_hsec
  %x = xor i64 %s, %a
  %m = xor i64 %x, 1884440546999092433
  ret i64 %m
}

define internal i64 @npk_m_livew(i64 %a) {
  %s = load i64, ptr @npk_hsec
  %x = xor i64 %s, %a
  %m = xor i64 %x, 8639445676566075373
  ret i64 %m
}

define internal i64 @npk_m_largew(i64 %a) {
  %s = load i64, ptr @npk_hsec
  %x = xor i64 %s, %a
  %m = xor i64 %x, 4436545153374750491
  ret i64 %m
}

define internal i64 @npk_m_guard(i64 %a) {
  %s = load i64, ptr @npk_hsec
  %x = xor i64 %s, %a
  %m = xor i64 %x, 7651035258233467253
  ret i64 %m
}

; --- mmap-or-trap. All allocator memory arrives through here. ---------------

define internal i64 @npk_hmap(i64 %len) {
  %m = call i64 @npk_sys6(i64 9, i64 0, i64 %len, i64 3, i64 34, i64 -1, i64 0)
  %bad = icmp ugt i64 %m, -4096
  br i1 %bad, label %oom, label %ok
oom:
  call void @npk_heap_oom()
  unreachable
ok:
  ret i64 %m
}

define internal void @npk_hunmap(i64 %a, i64 %len) {
  %r = call i64 @npk_sys6(i64 11, i64 %a, i64 %len, i64 0, i64 0, i64 0, i64 0)
  %bad = icmp ne i64 %r, 0
  br i1 %bad, label %trap, label %ok
trap:
  ; munmap failing means the table and the kernel disagree about what is
  ; mapped -- an integrity failure, not an OOM.
  call void @npk_heap_bad()
  unreachable
ok:
  ret void
}

; --- init: the secret, then the two tables. Idempotent, called lazily. ------

define internal void @npk_heap_init() {
  %s0 = load i64, ptr @npk_hsec
  %live = icmp ne i64 %s0, 0
  br i1 %live, label %done, label %seed
seed:
  ; getrandom(&npk_hsec, 8, 0)
  %addr = ptrtoint ptr @npk_hsec to i64
  %r = call i64 @npk_sys6(i64 318, i64 %addr, i64 8, i64 0, i64 0, i64 0, i64 0)
  %short = icmp ne i64 %r, 8
  br i1 %short, label %trap, label %nz
trap:
  call void @npk_heap_bad()
  unreachable
nz:
  ; zero is the uninitialized sentinel, so a zero draw takes a fixed odd
  ; constant instead -- weaker keying for one run in 2^64, never a stall.
  %s1 = load i64, ptr @npk_hsec
  %z = icmp eq i64 %s1, 0
  br i1 %z, label %fix, label %tabs
fix:
  store i64 -7046029254386353131, ptr @npk_hsec
  br label %tabs
tabs:
  %ct = call i64 @npk_hmap(i64 4096)
  store i64 %ct, ptr @npk_chtab
  store i64 512, ptr @npk_chtab_cap
  store i64 0, ptr @npk_chtab_len
  %lt = call i64 @npk_hmap(i64 4096)
  store i64 %lt, ptr @npk_lgtab
  store i64 128, ptr @npk_lgtab_cap
  store i64 0, ptr @npk_lgtab_len
  br label %done
done:
  ret void
}

; --- the sorted chunk table: membership before any dereference --------------

define internal i64 @npk_chtab_find(i64 %a) {
entry:
  %tab = load i64, ptr @npk_chtab
  %len = load i64, ptr @npk_chtab_len
  br label %head
head:
  %lo = phi i64 [ 0, %entry ], [ %lo2, %step ]
  %hi = phi i64 [ %len, %entry ], [ %hi2, %step ]
  %more = icmp ult i64 %lo, %hi
  br i1 %more, label %probe, label %miss
probe:
  %sum = add i64 %lo, %hi
  %mid = lshr i64 %sum, 1
  %ea = add i64 %tab, %mid
  %eoff = shl i64 %mid, 3
  %ep = add i64 %tab, %eoff
  %epp = inttoptr i64 %ep to ptr
  %v = load i64, ptr %epp
  %eq = icmp eq i64 %v, %a
  br i1 %eq, label %hit, label %cmp
cmp:
  %lt = icmp ult i64 %v, %a
  %lo3 = add i64 %mid, 1
  br label %step
step:
  %lo2 = select i1 %lt, i64 %lo3, i64 %lo
  %hi2 = select i1 %lt, i64 %hi, i64 %mid
  br label %head
hit:
  ret i64 %mid
miss:
  ret i64 -1
}

define internal void @npk_chtab_insert(i64 %a) {
entry:
  %len = load i64, ptr @npk_chtab_len
  %cap = load i64, ptr @npk_chtab_cap
  %full = icmp eq i64 %len, %cap
  br i1 %full, label %grow, label %place
grow:
  %tab0 = load i64, ptr @npk_chtab
  %ncap = shl i64 %cap, 1
  %nbytes = shl i64 %ncap, 3
  %nt = call i64 @npk_hmap(i64 %nbytes)
  %obytes = shl i64 %cap, 3
  %ntp = inttoptr i64 %nt to ptr
  %otp = inttoptr i64 %tab0 to ptr
  %cbytes = shl i64 %len, 3
  call void @llvm.memcpy.p0.p0.i64(ptr %ntp, ptr %otp, i64 %cbytes, i1 false)
  call void @npk_hunmap(i64 %tab0, i64 %obytes)
  store i64 %nt, ptr @npk_chtab
  store i64 %ncap, ptr @npk_chtab_cap
  br label %place
place:
  %tab = load i64, ptr @npk_chtab
  br label %shift
shift:
  ; walk down from the end, moving greater entries up one slot
  %pos = phi i64 [ %len, %place ], [ %pos2, %move ]
  %atz = icmp eq i64 %pos, 0
  br i1 %atz, label %store, label %look
look:
  %pm1 = add i64 %pos, -1
  %poff = shl i64 %pm1, 3
  %pp = add i64 %tab, %poff
  %ppp = inttoptr i64 %pp to ptr
  %pv = load i64, ptr %ppp
  %gt = icmp ugt i64 %pv, %a
  br i1 %gt, label %move, label %store
move:
  %doff = shl i64 %pos, 3
  %dp = add i64 %tab, %doff
  %dpp = inttoptr i64 %dp to ptr
  store i64 %pv, ptr %dpp
  %pos2 = add i64 %pos, -1
  br label %shift
store:
  %soff = shl i64 %pos, 3
  %sp = add i64 %tab, %soff
  %spp = inttoptr i64 %sp to ptr
  store i64 %a, ptr %spp
  %nlen = add i64 %len, 1
  store i64 %nlen, ptr @npk_chtab_len
  ret void
}

; --- the large table: 32-byte entries { ptr, base, mapsize, size }, sorted --

define internal i64 @npk_lg_entry(i64 %idx) {
  %tab = load i64, ptr @npk_lgtab
  %off = shl i64 %idx, 5
  %e = add i64 %tab, %off
  ret i64 %e
}

define internal i64 @npk_lg_find(i64 %p) {
entry:
  %len = load i64, ptr @npk_lgtab_len
  br label %head
head:
  %lo = phi i64 [ 0, %entry ], [ %lo2, %step ]
  %hi = phi i64 [ %len, %entry ], [ %hi2, %step ]
  %more = icmp ult i64 %lo, %hi
  br i1 %more, label %probe, label %miss
probe:
  %sum = add i64 %lo, %hi
  %mid = lshr i64 %sum, 1
  %e = call i64 @npk_lg_entry(i64 %mid)
  %ep = inttoptr i64 %e to ptr
  %v = load i64, ptr %ep
  %eq = icmp eq i64 %v, %p
  br i1 %eq, label %hit, label %cmp
cmp:
  %lt = icmp ult i64 %v, %p
  %lo3 = add i64 %mid, 1
  br label %step
step:
  %lo2 = select i1 %lt, i64 %lo3, i64 %lo
  %hi2 = select i1 %lt, i64 %hi, i64 %mid
  br label %head
hit:
  ret i64 %mid
miss:
  ret i64 -1
}

define internal void @npk_lg_insert(i64 %p, i64 %base, i64 %msz, i64 %sz) {
entry:
  %len = load i64, ptr @npk_lgtab_len
  %cap = load i64, ptr @npk_lgtab_cap
  %full = icmp eq i64 %len, %cap
  br i1 %full, label %grow, label %place
grow:
  %tab0 = load i64, ptr @npk_lgtab
  %ncap = shl i64 %cap, 1
  %nbytes = shl i64 %ncap, 5
  %nt = call i64 @npk_hmap(i64 %nbytes)
  %obytes = shl i64 %cap, 5
  %cbytes = shl i64 %len, 5
  %ntp = inttoptr i64 %nt to ptr
  %otp = inttoptr i64 %tab0 to ptr
  call void @llvm.memcpy.p0.p0.i64(ptr %ntp, ptr %otp, i64 %cbytes, i1 false)
  call void @npk_hunmap(i64 %tab0, i64 %obytes)
  store i64 %nt, ptr @npk_lgtab
  store i64 %ncap, ptr @npk_lgtab_cap
  br label %place
place:
  br label %shift
shift:
  %pos = phi i64 [ %len, %place ], [ %pos2, %move ]
  %atz = icmp eq i64 %pos, 0
  br i1 %atz, label %write, label %look
look:
  %pm1 = add i64 %pos, -1
  %se = call i64 @npk_lg_entry(i64 %pm1)
  %sep = inttoptr i64 %se to ptr
  %sv = load i64, ptr %sep
  %gt = icmp ugt i64 %sv, %p
  br i1 %gt, label %move, label %write
move:
  %de = call i64 @npk_lg_entry(i64 %pos)
  %dep = inttoptr i64 %de to ptr
  %sp1 = inttoptr i64 %se to ptr
  call void @llvm.memcpy.p0.p0.i64(ptr %dep, ptr %sp1, i64 32, i1 false)
  %pos2 = add i64 %pos, -1
  br label %shift
write:
  %e = call i64 @npk_lg_entry(i64 %pos)
  %f0 = inttoptr i64 %e to ptr
  store i64 %p, ptr %f0
  %e1 = add i64 %e, 8
  %f1 = inttoptr i64 %e1 to ptr
  store i64 %base, ptr %f1
  %e2 = add i64 %e, 16
  %f2 = inttoptr i64 %e2 to ptr
  store i64 %msz, ptr %f2
  %e3 = add i64 %e, 24
  %f3 = inttoptr i64 %e3 to ptr
  store i64 %sz, ptr %f3
  %nlen = add i64 %len, 1
  store i64 %nlen, ptr @npk_lgtab_len
  ret void
}

define internal void @npk_lg_remove(i64 %idx) {
entry:
  %len = load i64, ptr @npk_lgtab_len
  br label %head
head:
  %i = phi i64 [ %idx, %entry ], [ %i2, %body ]
  %i1 = add i64 %i, 1
  %more = icmp ult i64 %i1, %len
  br i1 %more, label %body, label %done
body:
  %de = call i64 @npk_lg_entry(i64 %i)
  %se = call i64 @npk_lg_entry(i64 %i1)
  %dep = inttoptr i64 %de to ptr
  %sep = inttoptr i64 %se to ptr
  call void @llvm.memcpy.p0.p0.i64(ptr %dep, ptr %sep, i64 32, i1 false)
  %i2 = add i64 %i1, 0
  br label %head
done:
  %nlen = add i64 %len, -1
  store i64 %nlen, ptr @npk_lgtab_len
  ret void
}

; --- chunk lists ------------------------------------------------------------

define internal void @npk_ch_push(ptr %hp, i64 %ch) {
  %h = load i64, ptr %hp
  %nx = add i64 %ch, 40
  %nxp = inttoptr i64 %nx to ptr
  store i64 %h, ptr %nxp
  %pv = add i64 %ch, 48
  %pvp = inttoptr i64 %pv to ptr
  store i64 0, ptr %pvp
  %some = icmp ne i64 %h, 0
  br i1 %some, label %link, label %sethead
link:
  %hpv = add i64 %h, 48
  %hpvp = inttoptr i64 %hpv to ptr
  store i64 %ch, ptr %hpvp
  br label %sethead
sethead:
  store i64 %ch, ptr %hp
  ret void
}

define internal void @npk_ch_unlink(ptr %hp, i64 %ch) {
  %nxa = add i64 %ch, 40
  %nxap = inttoptr i64 %nxa to ptr
  %nx = load i64, ptr %nxap
  %pva = add i64 %ch, 48
  %pvap = inttoptr i64 %pva to ptr
  %pv = load i64, ptr %pvap
  %isfirst = icmp eq i64 %pv, 0
  br i1 %isfirst, label %head, label %mid
head:
  store i64 %nx, ptr %hp
  br label %fixnext
mid:
  %pnx = add i64 %pv, 40
  %pnxp = inttoptr i64 %pnx to ptr
  store i64 %nx, ptr %pnxp
  br label %fixnext
fixnext:
  %some = icmp ne i64 %nx, 0
  br i1 %some, label %fix, label %clear
fix:
  %npv = add i64 %nx, 48
  %npvp = inttoptr i64 %npv to ptr
  store i64 %pv, ptr %npvp
  br label %clear
clear:
  store i64 0, ptr %nxap
  store i64 0, ptr %pvap
  ret void
}

; --- a fresh chunk: 64 KiB, 64 KiB-aligned by over-map-and-trim -------------

define internal i64 @npk_chunk_new(i64 %ci) {
entry:
  %raw = call i64 @npk_hmap(i64 131072)
  %r1 = add i64 %raw, 65535
  %al = and i64 %r1, -65536
  %pre = sub i64 %al, %raw
  %haspre = icmp ne i64 %pre, 0
  br i1 %haspre, label %trimpre, label %posttrim
trimpre:
  call void @npk_hunmap(i64 %raw, i64 %pre)
  br label %posttrim
posttrim:
  %post = sub i64 65536, %pre
  %haspost = icmp ne i64 %post, 0
  br i1 %haspost, label %trimpost, label %init
trimpost:
  %pend = add i64 %al, 65536
  call void @npk_hunmap(i64 %pend, i64 %post)
  br label %init
init:
  %m = call i64 @npk_m_chunk(i64 %al)
  %mp = inttoptr i64 %al to ptr
  store i64 %m, ptr %mp
  %cia = add i64 %al, 8
  %cip = inttoptr i64 %cia to ptr
  store i64 %ci, ptr %cip
  %sgep = getelementptr [14 x i64], ptr @npk_cls_slots, i64 0, i64 %ci
  %S = load i64, ptr %sgep
  %sca = add i64 %al, 16
  %scp = inttoptr i64 %sca to ptr
  store i64 %S, ptr %scp
  %fca = add i64 %al, 24
  %fcp = inttoptr i64 %fca to ptr
  store i64 %S, ptr %fcp
  %ha = add i64 %al, 32
  %hp = inttoptr i64 %ha to ptr
  store i64 0, ptr %hp
  %nxa = add i64 %al, 40
  %nxp = inttoptr i64 %nxa to ptr
  store i64 0, ptr %nxp
  %pva = add i64 %al, 48
  %pvp = inttoptr i64 %pva to ptr
  store i64 0, ptr %pvp
  %wma = add i64 %al, 56
  %wmp = inttoptr i64 %wma to ptr
  store i64 0, ptr %wmp
  %wgep = getelementptr [14 x i64], ptr @npk_cls_bmw, i64 0, i64 %ci
  %W = load i64, ptr %wgep
  br label %bmhead
bmhead:
  ; word w holds a set bit per free slot: full words of -1, then the remainder
  %w = phi i64 [ 0, %init ], [ %w2, %bmstore ]
  %morew = icmp ult i64 %w, %W
  br i1 %morew, label %bmbody, label %guard
bmbody:
  %before = shl i64 %w, 6
  %left = sub i64 %S, %before
  %ge64 = icmp uge i64 %left, 64
  br i1 %ge64, label %fullw, label %remw
fullw:
  br label %bmstore
remw:
  %one = shl i64 1, %left
  %rem = add i64 %one, -1
  br label %bmstore
bmstore:
  %val = phi i64 [ -1, %fullw ], [ %rem, %remw ]
  %woff = shl i64 %w, 3
  %wa0 = add i64 %al, 64
  %wa = add i64 %wa0, %woff
  %wp = inttoptr i64 %wa to ptr
  store i64 %val, ptr %wp
  %w2 = add i64 %w, 1
  br label %bmhead
guard:
  %ggep = getelementptr [14 x i64], ptr @npk_cls_guard, i64 0, i64 %ci
  %go = load i64, ptr %ggep
  %ga = add i64 %al, %go
  %g0 = call i64 @npk_m_guard(i64 %ga)
  %gp0 = inttoptr i64 %ga to ptr
  store i64 %g0, ptr %gp0
  %ga1 = add i64 %ga, 8
  %g1 = call i64 @npk_m_guard(i64 %ga1)
  %gp1 = inttoptr i64 %ga1 to ptr
  store i64 %g1, ptr %gp1
  call void @npk_chtab_insert(i64 %al)
  ret i64 %al
}

; --- guard and header checks, shared ----------------------------------------

define internal void @npk_chunk_guard_check(i64 %ch, i64 %ci) {
  %ggep = getelementptr [14 x i64], ptr @npk_cls_guard, i64 0, i64 %ci
  %go = load i64, ptr %ggep
  %ga = add i64 %ch, %go
  %w0 = call i64 @npk_m_guard(i64 %ga)
  %gp0 = inttoptr i64 %ga to ptr
  %v0 = load i64, ptr %gp0
  %ok0 = icmp eq i64 %v0, %w0
  br i1 %ok0, label %second, label %trap
second:
  %ga1 = add i64 %ga, 8
  %w1 = call i64 @npk_m_guard(i64 %ga1)
  %gp1 = inttoptr i64 %ga1 to ptr
  %v1 = load i64, ptr %gp1
  %ok1 = icmp eq i64 %v1, %w1
  br i1 %ok1, label %done, label %trap
trap:
  call void @npk_heap_bad()
  unreachable
done:
  ret void
}

; Validate a small-block pointer end to end WITHOUT mutating: proves p is a
; live slot of one of our chunks and returns the chunk address. Traps on any
; violation. Callers re-derive slot arithmetic; validation lives only here.
define internal i64 @npk_small_check(i64 %ip) {
entry:
  %ch = and i64 %ip, -65536
  %idx = call i64 @npk_chtab_find(i64 %ch)
  %miss = icmp slt i64 %idx, 0
  br i1 %miss, label %trap, label %magic
magic:
  %want = call i64 @npk_m_chunk(i64 %ch)
  %mp = inttoptr i64 %ch to ptr
  %have = load i64, ptr %mp
  %mok = icmp eq i64 %have, %want
  br i1 %mok, label %klass, label %trap
klass:
  %cia = add i64 %ch, 8
  %cip = inttoptr i64 %cia to ptr
  %ci = load i64, ptr %cip
  %cibad = icmp uge i64 %ci, 14
  br i1 %cibad, label %trap, label %geom
geom:
  %dgep = getelementptr [14 x i64], ptr @npk_cls_data, i64 0, i64 %ci
  %data = load i64, ptr %dgep
  %zgep = getelementptr [14 x i64], ptr @npk_cls_size, i64 0, i64 %ci
  %cls = load i64, ptr %zgep
  %stride = add i64 %cls, 16
  %sgep = getelementptr [14 x i64], ptr @npk_cls_slots, i64 0, i64 %ci
  %S = load i64, ptr %sgep
  %b = add i64 %ip, -16
  %dstart = add i64 %ch, %data
  %off = sub i64 %b, %dstart
  %neg = icmp slt i64 %off, 0
  br i1 %neg, label %trap, label %shape
shape:
  %rem = urem i64 %off, %stride
  %misfit = icmp ne i64 %rem, 0
  br i1 %misfit, label %trap, label %range
range:
  %slot = udiv i64 %off, %stride
  %oob = icmp uge i64 %slot, %S
  br i1 %oob, label %trap, label %wmk
wmk:
  %wma = add i64 %ch, 56
  %wmp = inttoptr i64 %wma to ptr
  %wm = load i64, ptr %wmp
  %virgin = icmp uge i64 %slot, %wm
  br i1 %virgin, label %trap, label %bmap
bmap:
  %w = lshr i64 %slot, 6
  %bit = and i64 %slot, 63
  %mask = shl i64 1, %bit
  %woff = shl i64 %w, 3
  %wa0 = add i64 %ch, 64
  %wa = add i64 %wa0, %woff
  %wp = inttoptr i64 %wa to ptr
  %word = load i64, ptr %wp
  %freebit = and i64 %word, %mask
  %isfree = icmp ne i64 %freebit, 0
  br i1 %isfree, label %trap, label %hdr
hdr:
  %hmi = call i64 @npk_m_live(i64 %b)
  %hmw = call i64 @npk_m_livew(i64 %b)
  %hma = add i64 %b, 8
  %hmp = inttoptr i64 %hma to ptr
  %hv = load i64, ptr %hmp
  %oki = icmp eq i64 %hv, %hmi
  %okw = icmp eq i64 %hv, %hmw
  %hok = or i1 %oki, %okw
  br i1 %hok, label %tail, label %trap
tail:
  call void @npk_chunk_guard_check(i64 %ch, i64 %ci)
  ret i64 %ch
trap:
  call void @npk_heap_bad()
  unreachable
}

; --- the small path ---------------------------------------------------------

define internal ptr @npk_small_alloc(i64 %n, i64 %ci, i64 %wild) {
entry:
  %hp = getelementptr [14 x i64], ptr @npk_cls_part, i64 0, i64 %ci
  %h0 = load i64, ptr %hp
  %none = icmp eq i64 %h0, 0
  br i1 %none, label %fresh, label %have
fresh:
  %nc = call i64 @npk_chunk_new(i64 %ci)
  call void @npk_ch_push(ptr %hp, i64 %nc)
  br label %have
have:
  %ch = load i64, ptr %hp
  %wgep = getelementptr [14 x i64], ptr @npk_cls_bmw, i64 0, i64 %ci
  %W = load i64, ptr %wgep
  %ha = add i64 %ch, 32
  %hip = inttoptr i64 %ha to ptr
  %hint = load i64, ptr %hip
  br label %scan
scan:
  %w = phi i64 [ %hint, %have ], [ %w2, %next ]
  %inw = icmp ult i64 %w, %W
  br i1 %inw, label %look, label %broken
look:
  %woff = shl i64 %w, 3
  %wa0 = add i64 %ch, 64
  %wa = add i64 %wa0, %woff
  %wp = inttoptr i64 %wa to ptr
  %word = load i64, ptr %wp
  %nz = icmp ne i64 %word, 0
  br i1 %nz, label %take, label %next
next:
  %w2 = add i64 %w, 1
  br label %scan
broken:
  ; a partial chunk with no set bit is a bookkeeping violation
  call void @npk_heap_bad()
  unreachable
take:
  %bit = call i64 @llvm.cttz.i64(i64 %word, i1 true)
  %shifted = shl i64 %w, 6
  %slot = add i64 %shifted, %bit
  %wm1 = add i64 %word, -1
  %nword = and i64 %word, %wm1
  store i64 %nword, ptr %wp
  ; hint: this word if it still has bits, else the next
  %emptied = icmp eq i64 %nword, 0
  %w3 = add i64 %w, 1
  %nhint = select i1 %emptied, i64 %w3, i64 %w
  store i64 %nhint, ptr %hip
  %fca = add i64 %ch, 24
  %fcp = inttoptr i64 %fca to ptr
  %free = load i64, ptr %fcp
  %nf = add i64 %free, -1
  store i64 %nf, ptr %fcp
  %isfull = icmp eq i64 %nf, 0
  br i1 %isfull, label %tofull, label %geom
tofull:
  %fhp = getelementptr [14 x i64], ptr @npk_cls_full, i64 0, i64 %ci
  call void @npk_ch_unlink(ptr %hp, i64 %ch)
  call void @npk_ch_push(ptr %fhp, i64 %ch)
  br label %geom
geom:
  %dgep = getelementptr [14 x i64], ptr @npk_cls_data, i64 0, i64 %ci
  %data = load i64, ptr %dgep
  %zgep = getelementptr [14 x i64], ptr @npk_cls_size, i64 0, i64 %ci
  %cls = load i64, ptr %zgep
  %stride = add i64 %cls, 16
  %soff = mul i64 %slot, %stride
  %d0 = add i64 %ch, %data
  %b = add i64 %d0, %soff
  ; watermark: below it the slot was used before and must still carry its
  ; FREED magic -- a UAF that scribbled a freed block traps here, on reuse
  %wma = add i64 %ch, 56
  %wmp = inttoptr i64 %wma to ptr
  %wm = load i64, ptr %wmp
  %virgin = icmp uge i64 %slot, %wm
  br i1 %virgin, label %mark, label %verify
mark:
  %nwm = add i64 %slot, 1
  store i64 %nwm, ptr %wmp
  br label %stamp
verify:
  %fm = call i64 @npk_m_freed(i64 %b)
  %fma = add i64 %b, 8
  %fmp = inttoptr i64 %fma to ptr
  %fv = load i64, ptr %fmp
  %fok = icmp eq i64 %fv, %fm
  br i1 %fok, label %stamp, label %poisoned
poisoned:
  call void @npk_heap_bad()
  unreachable
stamp:
  %szp = inttoptr i64 %b to ptr
  store i64 %n, ptr %szp
  %lmi = call i64 @npk_m_live(i64 %b)
  %lmw = call i64 @npk_m_livew(i64 %b)
  %iswild = icmp ne i64 %wild, 0
  %lm = select i1 %iswild, i64 %lmw, i64 %lmi
  %lma = add i64 %b, 8
  %lmp = inttoptr i64 %lma to ptr
  store i64 %lm, ptr %lmp
  %u = add i64 %b, 16
  %p = inttoptr i64 %u to ptr
  ret ptr %p
}

define internal void @npk_small_free(i64 %ip) {
entry:
  %ch = call i64 @npk_small_check(i64 %ip)
  %cia = add i64 %ch, 8
  %cip = inttoptr i64 %cia to ptr
  %ci = load i64, ptr %cip
  %dgep = getelementptr [14 x i64], ptr @npk_cls_data, i64 0, i64 %ci
  %data = load i64, ptr %dgep
  %zgep = getelementptr [14 x i64], ptr @npk_cls_size, i64 0, i64 %ci
  %cls = load i64, ptr %zgep
  %stride = add i64 %cls, 16
  %b = add i64 %ip, -16
  %dstart = add i64 %ch, %data
  %off = sub i64 %b, %dstart
  %slot = udiv i64 %off, %stride
  ; stamp FREED, then flip the bit
  %fm = call i64 @npk_m_freed(i64 %b)
  %fma = add i64 %b, 8
  %fmp = inttoptr i64 %fma to ptr
  store i64 %fm, ptr %fmp
  %w = lshr i64 %slot, 6
  %bit = and i64 %slot, 63
  %mask = shl i64 1, %bit
  %woff = shl i64 %w, 3
  %wa0 = add i64 %ch, 64
  %wa = add i64 %wa0, %woff
  %wp = inttoptr i64 %wa to ptr
  %word = load i64, ptr %wp
  %nword = or i64 %word, %mask
  store i64 %nword, ptr %wp
  %fca = add i64 %ch, 24
  %fcp = inttoptr i64 %fca to ptr
  %free = load i64, ptr %fcp
  %nf = add i64 %free, 1
  store i64 %nf, ptr %fcp
  %ha = add i64 %ch, 32
  %hip = inttoptr i64 %ha to ptr
  %hint = load i64, ptr %hip
  %lower = icmp ult i64 %w, %hint
  %nhint = select i1 %lower, i64 %w, i64 %hint
  store i64 %nhint, ptr %hip
  %wasfull = icmp eq i64 %free, 0
  br i1 %wasfull, label %topart, label %done
topart:
  %fhp = getelementptr [14 x i64], ptr @npk_cls_full, i64 0, i64 %ci
  %php = getelementptr [14 x i64], ptr @npk_cls_part, i64 0, i64 %ci
  call void @npk_ch_unlink(ptr %fhp, i64 %ch)
  call void @npk_ch_push(ptr %php, i64 %ch)
  br label %done
done:
  ret void
}

; --- the large path ---------------------------------------------------------

define internal ptr @npk_large_new(i64 %n, i64 %align, i64 %wild) {
entry:
  %r15 = add i64 %n, 15
  %rn = and i64 %r15, -16
  %wide = icmp ugt i64 %align, 16
  %extra = select i1 %wide, i64 %align, i64 0
  %n32 = add i64 %rn, 32
  %need = add i64 %n32, %extra
  %n4095 = add i64 %need, 4095
  %msz = and i64 %n4095, -4096
  %base = call i64 @npk_hmap(i64 %msz)
  %p0 = add i64 %base, 16
  %am1 = add i64 %align, -1
  %pa0 = add i64 %p0, %am1
  %nega = sub i64 0, %align
  %pa = and i64 %pa0, %nega
  ; the contract, asserted where it is produced: the payload meets the
  ; requested alignment and the block fits its mapping
  %alcheck = and i64 %pa, %am1
  %misaligned = icmp ne i64 %alcheck, 0
  %endp = add i64 %pa, %rn
  %endg = add i64 %endp, 16
  %mapend = add i64 %base, %msz
  %overrun = icmp ugt i64 %endg, %mapend
  %broken = or i1 %misaligned, %overrun
  br i1 %broken, label %selftrap, label %fit
selftrap:
  call void @npk_heap_bad()
  unreachable
fit:
  %b = add i64 %pa, -16
  %szp = inttoptr i64 %b to ptr
  store i64 %n, ptr %szp
  %lmi = call i64 @npk_m_large(i64 %b)
  %lmw = call i64 @npk_m_largew(i64 %b)
  %iswild = icmp ne i64 %wild, 0
  %lm = select i1 %iswild, i64 %lmw, i64 %lmi
  %lma = add i64 %b, 8
  %lmp = inttoptr i64 %lma to ptr
  store i64 %lm, ptr %lmp
  %fa = add i64 %pa, %rn
  %g0 = call i64 @npk_m_guard(i64 %fa)
  %gp0 = inttoptr i64 %fa to ptr
  store i64 %g0, ptr %gp0
  %fa1 = add i64 %fa, 8
  %g1 = call i64 @npk_m_guard(i64 %fa1)
  %gp1 = inttoptr i64 %fa1 to ptr
  store i64 %g1, ptr %gp1
  call void @npk_lg_insert(i64 %pa, i64 %base, i64 %msz, i64 %n)
  %p = inttoptr i64 %pa to ptr
  ret ptr %p
}

; Validate a large block's header and footer against its table entry. Traps on
; violation. %e is the entry address.
define internal void @npk_large_check(i64 %ip, i64 %e) {
entry:
  %b = add i64 %ip, -16
  %wanti = call i64 @npk_m_large(i64 %b)
  %wantw = call i64 @npk_m_largew(i64 %b)
  %hma = add i64 %b, 8
  %hmp = inttoptr i64 %hma to ptr
  %have = load i64, ptr %hmp
  %oki = icmp eq i64 %have, %wanti
  %okw = icmp eq i64 %have, %wantw
  %hok = or i1 %oki, %okw
  br i1 %hok, label %foot, label %trap
foot:
  %sza = add i64 %e, 24
  %szp = inttoptr i64 %sza to ptr
  %sz = load i64, ptr %szp
  %r15 = add i64 %sz, 15
  %rn = and i64 %r15, -16
  %fa = add i64 %ip, %rn
  %w0 = call i64 @npk_m_guard(i64 %fa)
  %fp0 = inttoptr i64 %fa to ptr
  %v0 = load i64, ptr %fp0
  %ok0 = icmp eq i64 %v0, %w0
  br i1 %ok0, label %foot2, label %trap
foot2:
  %fa1 = add i64 %fa, 8
  %w1 = call i64 @npk_m_guard(i64 %fa1)
  %fp1 = inttoptr i64 %fa1 to ptr
  %v1 = load i64, ptr %fp1
  %ok1 = icmp eq i64 %v1, %w1
  br i1 %ok1, label %done, label %trap
trap:
  call void @npk_heap_bad()
  unreachable
done:
  ret void
}

; --- the four builtins, plus aalloc -----------------------------------------

define internal ptr @npk_alloc_impl(i64 %n, i64 %wild) {
entry:
  %sec = load i64, ptr @npk_hsec
  %uninit = icmp eq i64 %sec, 0
  br i1 %uninit, label %init, label %sized
init:
  call void @npk_heap_init()
  br label %sized
sized:
  %neg = icmp slt i64 %n, 0
  br i1 %neg, label %badreq, label %norm
badreq:
  call void @npk_heap_badreq()
  unreachable
norm:
  ; alloc(0) is a real, unique, freeable 16-byte block (D-150) -- a trap here
  ; would make every `alloc(count * elem)` with a legal zero count a landmine
  %z = icmp eq i64 %n, 0
  %n1 = select i1 %z, i64 16, i64 %n
  %r15 = add i64 %n1, 15
  %rn = and i64 %r15, -16
  br label %pick
pick:
  %ci = phi i64 [ 0, %norm ], [ %ci2, %bigger ]
  %inrange = icmp ult i64 %ci, 14
  br i1 %inrange, label %try, label %large
try:
  %zgep = getelementptr [14 x i64], ptr @npk_cls_size, i64 0, i64 %ci
  %cls = load i64, ptr %zgep
  %fits = icmp ule i64 %rn, %cls
  br i1 %fits, label %small, label %bigger
bigger:
  %ci2 = add i64 %ci, 1
  br label %pick
small:
  %sp = call ptr @npk_small_alloc(i64 %n1, i64 %ci, i64 %wild)
  ret ptr %sp
large:
  %lp = call ptr @npk_large_new(i64 %n1, i64 16, i64 %wild)
  ret ptr %lp
}

; The WILD entry -- the alloc builtin. What this hands out is in the
; <wild-live> set until dalloc'd, and the exit check counts it (D-151).
define ptr @npk_alloc(i64 %n) {
  %p = call ptr @npk_alloc_impl(i64 %n, i64 1)
  ret ptr %p
}

; The INTERNAL entry -- runtime-owned storage (string bodies, argv, file
; buffers). Managed-regime: not in <wild-live>; its RAII lands with the
; managed lowering, and wild_release_all still reclaims it wholesale.
define internal ptr @npk_alloc_internal(i64 %n) {
  %p = call ptr @npk_alloc_impl(i64 %n, i64 0)
  ret ptr %p
}

define ptr @npk_calloc(i64 %count, i64 %size) {
entry:
  %negc = icmp slt i64 %count, 0
  %negs = icmp slt i64 %size, 0
  %neg = or i1 %negc, %negs
  br i1 %neg, label %badreq, label %mul
mul:
  ; the multiply is CHECKED (0.10.0): a wrap here is an undersized allocation
  ; wearing a plausible size
  %wo = call { i64, i1 } @llvm.umul.with.overflow.i64(i64 %count, i64 %size)
  %prod = extractvalue { i64, i1 } %wo, 0
  %ovf = extractvalue { i64, i1 } %wo, 1
  br i1 %ovf, label %badreq, label %ok
badreq:
  call void @npk_heap_badreq()
  unreachable
ok:
  %p = call ptr @npk_alloc(i64 %prod)
  call void @llvm.memset.p0.i64(ptr %p, i8 0, i64 %prod, i1 false)
  ret ptr %p
}

define ptr @npk_ralloc(ptr %old, i64 %n) {
entry:
  %isnull = icmp eq ptr %old, null
  br i1 %isnull, label %plain, label %sized
plain:
  %f = call ptr @npk_alloc(i64 %n)
  ret ptr %f
sized:
  ; ralloc(p, 0) is REFUSED (D-150): freeing is spelled dalloc, and C's
  ; realloc(p, 0) is the exact implementation-defined footgun this is not
  %neg = icmp slt i64 %n, 1
  br i1 %neg, label %badreq, label %checks
badreq:
  call void @npk_heap_badreq()
  unreachable
checks:
  %ip = ptrtoint ptr %old to i64
  %misal = and i64 %ip, 15
  %crooked = icmp ne i64 %misal, 0
  br i1 %crooked, label %bad, label %virginheap
bad:
  call void @npk_heap_bad()
  unreachable
virginheap:
  %sec = load i64, ptr @npk_hsec
  %uninit = icmp eq i64 %sec, 0
  br i1 %uninit, label %bad, label %route
route:
  %lgidx = call i64 @npk_lg_find(i64 %ip)
  %islarge = icmp sge i64 %lgidx, 0
  br i1 %islarge, label %lg, label %sm
lg:
  %e = call i64 @npk_lg_entry(i64 %lgidx)
  call void @npk_large_check(i64 %ip, i64 %e)
  %basea = add i64 %e, 8
  %basep = inttoptr i64 %basea to ptr
  %base = load i64, ptr %basep
  %msza = add i64 %e, 16
  %mszp = inttoptr i64 %msza to ptr
  %msz = load i64, ptr %mszp
  %osza = add i64 %e, 24
  %oszp = inttoptr i64 %osza to ptr
  %osz = load i64, ptr %oszp
  %r15 = add i64 %n, 15
  %rn = and i64 %r15, -16
  %inblock = sub i64 %ip, %base
  %needend = add i64 %inblock, %rn
  %needall = add i64 %needend, 16
  %fitsin = icmp ule i64 %needall, %msz
  br i1 %fitsin, label %inplace, label %move
inplace:
  %b = add i64 %ip, -16
  %szslot = inttoptr i64 %b to ptr
  store i64 %n, ptr %szslot
  store i64 %n, ptr %oszp
  %fa = add i64 %ip, %rn
  %g0 = call i64 @npk_m_guard(i64 %fa)
  %gp0 = inttoptr i64 %fa to ptr
  store i64 %g0, ptr %gp0
  %fa1 = add i64 %fa, 8
  %g1 = call i64 @npk_m_guard(i64 %fa1)
  %gp1 = inttoptr i64 %fa1 to ptr
  store i64 %g1, ptr %gp1
  ret ptr %old
move:
  ; the regime travels with the data: a wild block moves to a wild block
  %mb = add i64 %ip, -16
  %mw = call i64 @npk_m_largew(i64 %mb)
  %mha = add i64 %mb, 8
  %mhp = inttoptr i64 %mha to ptr
  %mhv = load i64, ptr %mhp
  %mwild = icmp eq i64 %mhv, %mw
  %mrole = select i1 %mwild, i64 1, i64 0
  %np = call ptr @npk_alloc_impl(i64 %n, i64 %mrole)
  %smaller = icmp ult i64 %osz, %n
  %cnt = select i1 %smaller, i64 %osz, i64 %n
  call void @llvm.memcpy.p0.p0.i64(ptr %np, ptr %old, i64 %cnt, i1 false)
  call void @npk_dalloc(ptr %old)
  ret ptr %np
sm:
  %ch = call i64 @npk_small_check(i64 %ip)
  %cia = add i64 %ch, 8
  %cip = inttoptr i64 %cia to ptr
  %ci = load i64, ptr %cip
  %zgep = getelementptr [14 x i64], ptr @npk_cls_size, i64 0, i64 %ci
  %cls = load i64, ptr %zgep
  %b2 = add i64 %ip, -16
  %oszp2 = inttoptr i64 %b2 to ptr
  %osz2 = load i64, ptr %oszp2
  %r15b = add i64 %n, 15
  %rnb = and i64 %r15b, -16
  %fitscls = icmp ule i64 %rnb, %cls
  br i1 %fitscls, label %inplace2, label %move2
inplace2:
  store i64 %n, ptr %oszp2
  ret ptr %old
move2:
  %mb2 = add i64 %ip, -16
  %mw2 = call i64 @npk_m_livew(i64 %mb2)
  %mha2 = add i64 %mb2, 8
  %mhp2 = inttoptr i64 %mha2 to ptr
  %mhv2 = load i64, ptr %mhp2
  %mwild2 = icmp eq i64 %mhv2, %mw2
  %mrole2 = select i1 %mwild2, i64 1, i64 0
  %np2 = call ptr @npk_alloc_impl(i64 %n, i64 %mrole2)
  %smaller2 = icmp ult i64 %osz2, %n
  %cnt2 = select i1 %smaller2, i64 %osz2, i64 %n
  call void @llvm.memcpy.p0.p0.i64(ptr %np2, ptr %old, i64 %cnt2, i1 false)
  call void @npk_dalloc(ptr %old)
  ret ptr %np2
}

define void @npk_dalloc(ptr %p) {
entry:
  ; dalloc(NULL) is a TRAP (D-150): alloc never returns null, so a null here
  ; is a program in a state its author did not intend -- explicit over
  ; implicit, and unlike C there is no cleanup idiom that needs free(NULL)
  %ip = ptrtoint ptr %p to i64
  %isz = icmp eq i64 %ip, 0
  br i1 %isz, label %bad, label %aligned
bad:
  call void @npk_heap_bad()
  unreachable
aligned:
  %misal = and i64 %ip, 15
  %crooked = icmp ne i64 %misal, 0
  br i1 %crooked, label %bad, label %virginheap
virginheap:
  ; a free before the first allocation cannot name our memory
  %sec = load i64, ptr @npk_hsec
  %uninit = icmp eq i64 %sec, 0
  br i1 %uninit, label %bad, label %route
route:
  %lgidx = call i64 @npk_lg_find(i64 %ip)
  %islarge = icmp sge i64 %lgidx, 0
  br i1 %islarge, label %lg, label %sm
lg:
  %e = call i64 @npk_lg_entry(i64 %lgidx)
  call void @npk_large_check(i64 %ip, i64 %e)
  %basea = add i64 %e, 8
  %basep = inttoptr i64 %basea to ptr
  %base = load i64, ptr %basep
  %msza = add i64 %e, 16
  %mszp = inttoptr i64 %msza to ptr
  %msz = load i64, ptr %mszp
  call void @npk_lg_remove(i64 %lgidx)
  call void @npk_hunmap(i64 %base, i64 %msz)
  ret void
sm:
  call void @npk_small_free(i64 %ip)
  ret void
}

define ptr @npk_aalloc(i64 %n, i64 %align) {
entry:
  %sec = load i64, ptr @npk_hsec
  %uninit = icmp eq i64 %sec, 0
  br i1 %uninit, label %init, label %checks
init:
  call void @npk_heap_init()
  br label %checks
checks:
  %negn = icmp slt i64 %n, 0
  br i1 %negn, label %badreq, label %alignck
alignck:
  %zeroa = icmp sle i64 %align, 0
  br i1 %zeroa, label %badreq, label %pow2
pow2:
  %am1 = add i64 %align, -1
  %mix = and i64 %align, %am1
  %notpow = icmp ne i64 %mix, 0
  br i1 %notpow, label %badreq, label %norm
badreq:
  call void @npk_heap_badreq()
  unreachable
norm:
  %z = icmp eq i64 %n, 0
  %n1 = select i1 %z, i64 16, i64 %n
  %smalla = icmp ule i64 %align, 16
  br i1 %smalla, label %plain, label %wide
plain:
  ; every ordinary block is 16-aligned already
  %p = call ptr @npk_alloc(i64 %n1)
  ret ptr %p
wide:
  %q = call ptr @npk_large_new(i64 %n1, i64 %align, i64 1)
  ret ptr %q
}

; --- the <wild-live> registry view (0.10.1, D-151) --------------------------
;
; The chunk bitmaps and the large table ARE the live-set; these two walk it.
; Both are allocation-free and walk only preallocated state, so they are safe
; from failsafe in a degraded process -- the C-3 discipline.

define i64 @npk_wild_live_count() {
entry:
  %tab = load i64, ptr @npk_chtab
  %len = load i64, ptr @npk_chtab_len
  br label %chead
chead:
  %i = phi i64 [ 0, %entry ], [ %i2, %cnext ]
  %acc = phi i64 [ 0, %entry ], [ %acc2, %cnext ]
  %more = icmp ult i64 %i, %len
  br i1 %more, label %cbody, label %larges
cbody:
  %eoff = shl i64 %i, 3
  %ep = add i64 %tab, %eoff
  %epp = inttoptr i64 %ep to ptr
  %ch = load i64, ptr %epp
  %cia = add i64 %ch, 8
  %cip = inttoptr i64 %cia to ptr
  %ci = load i64, ptr %cip
  %dgep = getelementptr [14 x i64], ptr @npk_cls_data, i64 0, i64 %ci
  %data = load i64, ptr %dgep
  %zgep = getelementptr [14 x i64], ptr @npk_cls_size, i64 0, i64 %ci
  %cls = load i64, ptr %zgep
  %stride = add i64 %cls, 16
  %wma = add i64 %ch, 56
  %wmp = inttoptr i64 %wma to ptr
  %wm = load i64, ptr %wmp
  br label %shead
shead:
  ; every slot below the watermark: live iff its free bit is CLEAR, wild iff
  ; its header carries the wild role
  %slot = phi i64 [ 0, %cbody ], [ %slot2, %snext ]
  %sacc = phi i64 [ %acc, %cbody ], [ %sacc2, %snext ]
  %smore = icmp ult i64 %slot, %wm
  br i1 %smore, label %sbody, label %cdone
sbody:
  %w = lshr i64 %slot, 6
  %bit = and i64 %slot, 63
  %mask = shl i64 1, %bit
  %woff = shl i64 %w, 3
  %wa0 = add i64 %ch, 64
  %wa = add i64 %wa0, %woff
  %wp = inttoptr i64 %wa to ptr
  %word = load i64, ptr %wp
  %fb = and i64 %word, %mask
  %isfree = icmp ne i64 %fb, 0
  br i1 %isfree, label %snext0, label %livecheck
livecheck:
  %soff = mul i64 %slot, %stride
  %d0 = add i64 %ch, %data
  %b = add i64 %d0, %soff
  %wantw = call i64 @npk_m_livew(i64 %b)
  %hma = add i64 %b, 8
  %hmp = inttoptr i64 %hma to ptr
  %hv = load i64, ptr %hmp
  %isw = icmp eq i64 %hv, %wantw
  %inc = select i1 %isw, i64 1, i64 0
  br label %scount
snext0:
  br label %scount
scount:
  %add = phi i64 [ %inc, %livecheck ], [ 0, %snext0 ]
  br label %snext
snext:
  %sacc2 = add i64 %sacc, %add
  %slot2 = add i64 %slot, 1
  br label %shead
cdone:
  br label %cnext
cnext:
  %acc2 = add i64 %sacc, 0
  %i2 = add i64 %i, 1
  br label %chead
larges:
  %ltab = load i64, ptr @npk_lgtab
  %llen = load i64, ptr @npk_lgtab_len
  br label %lhead
lhead:
  %j = phi i64 [ 0, %larges ], [ %j2, %lnext ]
  %lacc = phi i64 [ %acc, %larges ], [ %lacc2, %lnext ]
  %lmore = icmp ult i64 %j, %llen
  br i1 %lmore, label %lbody, label %out
lbody:
  %leoff = shl i64 %j, 5
  %lep = add i64 %ltab, %leoff
  %lepp = inttoptr i64 %lep to ptr
  %p = load i64, ptr %lepp
  %lb = add i64 %p, -16
  %lwant = call i64 @npk_m_largew(i64 %lb)
  %lhma = add i64 %lb, 8
  %lhmp = inttoptr i64 %lhma to ptr
  %lhv = load i64, ptr %lhmp
  %lisw = icmp eq i64 %lhv, %lwant
  %linc = select i1 %lisw, i64 1, i64 0
  br label %lnext
lnext:
  %lacc2 = add i64 %lacc, %linc
  %j2 = add i64 %j, 1
  br label %lhead
out:
  ret i64 %lacc
}

; Releases the WHOLE heap -- every chunk and every large mapping, both
; regimes -- and leaves the allocator usable. This is failsafe's controlled
; cleanup: after it, only exit; anything still pointing into the heap points
; at unmapped pages.
define void @npk_wild_release_all() {
entry:
  %tab = load i64, ptr @npk_chtab
  %len = load i64, ptr @npk_chtab_len
  br label %chead
chead:
  %i = phi i64 [ 0, %entry ], [ %i2, %cbody ]
  %more = icmp ult i64 %i, %len
  br i1 %more, label %cbody, label %clists
cbody:
  %eoff = shl i64 %i, 3
  %ep = add i64 %tab, %eoff
  %epp = inttoptr i64 %ep to ptr
  %ch = load i64, ptr %epp
  call void @npk_hunmap(i64 %ch, i64 65536)
  %i2 = add i64 %i, 1
  br label %chead
clists:
  store i64 0, ptr @npk_chtab_len
  br label %zhead
zhead:
  %k = phi i64 [ 0, %clists ], [ %k2, %zbody ]
  %zmore = icmp ult i64 %k, 14
  br i1 %zmore, label %zbody, label %larges
zbody:
  %pp = getelementptr [14 x i64], ptr @npk_cls_part, i64 0, i64 %k
  store i64 0, ptr %pp
  %fp = getelementptr [14 x i64], ptr @npk_cls_full, i64 0, i64 %k
  store i64 0, ptr %fp
  %k2 = add i64 %k, 1
  br label %zhead
larges:
  %ltab = load i64, ptr @npk_lgtab
  %llen = load i64, ptr @npk_lgtab_len
  br label %lhead
lhead:
  %j = phi i64 [ 0, %larges ], [ %j2, %lbody ]
  %lmore = icmp ult i64 %j, %llen
  br i1 %lmore, label %lbody, label %done
lbody:
  %leoff = shl i64 %j, 5
  %lep = add i64 %ltab, %leoff
  %lepp = inttoptr i64 %lep to ptr
  %basea = add i64 %lep, 8
  %basep = inttoptr i64 %basea to ptr
  %base = load i64, ptr %basep
  %msza = add i64 %lep, 16
  %mszp = inttoptr i64 %msza to ptr
  %msz = load i64, ptr %mszp
  call void @npk_hunmap(i64 %base, i64 %msz)
  %j2 = add i64 %j, 1
  br label %lhead
done:
  store i64 0, ptr @npk_lgtab_len
  ret void
}

; ---------------------------------------------------------------------------
; THE ARENA (0.10.2, D-152). arena<T> = { slab ptr, gens ptr, cap, top,
; free_head }, stride-erased: the compiler computes the element stride
; (size rounded to alignment, floored at 8 so a free slot holds the freelist
; link) and passes it to every call. Generations are i32 with a PARITY
; DISCIPLINE: live slots hold EVEN generations, freed slots ODD -- a handle
; only ever carries the even generation `alloc` issued, so a forged or stale
; handle can never name a freed slot: `at` demands exact equality, and the
; freed slot's generation is odd. Reuse bumps odd back to even (+1), so every
; retired handle to the slot mismatches by at least 2. A slot whose
; generation reaches 0xFFFFFFFE is RETIRED rather than reused -- the counter
; never wraps. The slab and generation array are WILD-role heap blocks: an
; un-destroyed arena is a countable leak (D-151), which is the D-003 story
; told mechanically -- drop the arena wholesale, or the exit check names it.
; Single-threaded by contract (D-017); shared_arena is 0.10.4's.
; ---------------------------------------------------------------------------

define { ptr, ptr, i64, i64, i64 } @npk_arena_make(i64 %stride, i64 %cap0) {
entry:
  %neg = icmp slt i64 %cap0, 0
  br i1 %neg, label %badreq, label %floor
badreq:
  call void @npk_heap_badreq()
  unreachable
floor:
  %small = icmp slt i64 %cap0, 8
  %cap = select i1 %small, i64 8, i64 %cap0
  %bo = call { i64, i1 } @llvm.umul.with.overflow.i64(i64 %cap, i64 %stride)
  %bytes = extractvalue { i64, i1 } %bo, 0
  %ovf = extractvalue { i64, i1 } %bo, 1
  br i1 %ovf, label %badreq, label %mkslab
mkslab:
  %slab = call ptr @npk_alloc_impl(i64 %bytes, i64 1)
  %gb = shl i64 %cap, 2
  %gens = call ptr @npk_alloc_impl(i64 %gb, i64 1)
  call void @llvm.memset.p0.i64(ptr %gens, i8 0, i64 %gb, i1 false)
  %r0 = insertvalue { ptr, ptr, i64, i64, i64 } undef, ptr %slab, 0
  %r1 = insertvalue { ptr, ptr, i64, i64, i64 } %r0, ptr %gens, 1
  %r2 = insertvalue { ptr, ptr, i64, i64, i64 } %r1, i64 %cap, 2
  %r3 = insertvalue { ptr, ptr, i64, i64, i64 } %r2, i64 0, 3
  %r4 = insertvalue { ptr, ptr, i64, i64, i64 } %r3, i64 -1, 4
  ret { ptr, ptr, i64, i64, i64 } %r4
}

define { i64, i32 } @npk_arena_alloc(ptr %a, i64 %stride) {
entry:
  %ai = ptrtoint ptr %a to i64
  %fha = add i64 %ai, 32
  %fhp = inttoptr i64 %fha to ptr
  %fh = load i64, ptr %fhp
  %none = icmp eq i64 %fh, -1
  br i1 %none, label %bump, label %pop
pop:
  ; the freed slot's first word is the next-free link
  %slaba = inttoptr i64 %ai to ptr
  %slab0 = load ptr, ptr %slaba
  %si = ptrtoint ptr %slab0 to i64
  %soff = mul i64 %fh, %stride
  %sa = add i64 %si, %soff
  %sp = inttoptr i64 %sa to ptr
  %nxt = load i64, ptr %sp
  store i64 %nxt, ptr %fhp
  br label %issue
bump:
  %topa = add i64 %ai, 24
  %topp = inttoptr i64 %topa to ptr
  %top = load i64, ptr %topp
  %capa = add i64 %ai, 16
  %capp = inttoptr i64 %capa to ptr
  %cap = load i64, ptr %capp
  %room = icmp ult i64 %top, %cap
  br i1 %room, label %take, label %grow
grow:
  ; double both stores; ralloc validates, preserves the wild role, and frees
  ; the old blocks -- the single-threaded contract is what makes the
  ; relocation safe (D-017)
  %ncap = shl i64 %cap, 1
  %nbo = call { i64, i1 } @llvm.umul.with.overflow.i64(i64 %ncap, i64 %stride)
  %nbytes = extractvalue { i64, i1 } %nbo, 0
  %novf = extractvalue { i64, i1 } %nbo, 1
  br i1 %novf, label %badreq, label %doslab
badreq:
  call void @npk_heap_badreq()
  unreachable
doslab:
  %slabaa = inttoptr i64 %ai to ptr
  %oslab = load ptr, ptr %slabaa
  %nslab = call ptr @npk_ralloc(ptr %oslab, i64 %nbytes)
  store ptr %nslab, ptr %slabaa
  %gensa = add i64 %ai, 8
  %gensp = inttoptr i64 %gensa to ptr
  %ogens = load ptr, ptr %gensp
  %ngb = shl i64 %ncap, 2
  %ngens = call ptr @npk_ralloc(ptr %ogens, i64 %ngb)
  store ptr %ngens, ptr %gensp
  ; the new half of the generation array must start at zero
  %gi = ptrtoint ptr %ngens to i64
  %oldgb = shl i64 %cap, 2
  %za = add i64 %gi, %oldgb
  %zp = inttoptr i64 %za to ptr
  %zn = sub i64 %ngb, %oldgb
  call void @llvm.memset.p0.i64(ptr %zp, i8 0, i64 %zn, i1 false)
  store i64 %ncap, ptr %capp
  br label %take
take:
  %top2 = load i64, ptr %topp
  %ntop = add i64 %top2, 1
  store i64 %ntop, ptr %topp
  br label %issue2
issue2:
  br label %issue
issue:
  %idx = phi i64 [ %fh, %pop ], [ %top2, %issue2 ]
  ; a reused or reset slot holds an ODD generation; bump it live (even)
  %gensa2 = add i64 %ai, 8
  %gensp2 = inttoptr i64 %gensa2 to ptr
  %gens2 = load ptr, ptr %gensp2
  %gi2 = ptrtoint ptr %gens2 to i64
  %goff = shl i64 %idx, 2
  %ga = add i64 %gi2, %goff
  %gp = inttoptr i64 %ga to ptr
  %g = load i32, ptr %gp
  %odd = and i32 %g, 1
  %isodd = icmp ne i32 %odd, 0
  %g2 = add i32 %g, 1
  %gv = select i1 %isodd, i32 %g2, i32 %g
  store i32 %gv, ptr %gp
  %h0 = insertvalue { i64, i32 } undef, i64 %idx, 0
  %h1 = insertvalue { i64, i32 } %h0, i32 %gv, 1
  ret { i64, i32 } %h1
}

define ptr @npk_arena_at(ptr %a, i64 %stride, i64 %idx, i32 %gen) {
entry:
  %ai = ptrtoint ptr %a to i64
  %topa = add i64 %ai, 24
  %topp = inttoptr i64 %topa to ptr
  %top = load i64, ptr %topp
  %oob = icmp uge i64 %idx, %top
  br i1 %oob, label %stale, label %genck
genck:
  %gensa = add i64 %ai, 8
  %gensp = inttoptr i64 %gensa to ptr
  %gens = load ptr, ptr %gensp
  %gi = ptrtoint ptr %gens to i64
  %goff = shl i64 %idx, 2
  %ga = add i64 %gi, %goff
  %gp = inttoptr i64 %ga to ptr
  %g = load i32, ptr %gp
  %match = icmp eq i32 %g, %gen
  br i1 %match, label %live, label %stale
live:
  %slaba = inttoptr i64 %ai to ptr
  %slab = load ptr, ptr %slaba
  %si = ptrtoint ptr %slab to i64
  %soff = mul i64 %idx, %stride
  %sa = add i64 %si, %soff
  %sp = inttoptr i64 %sa to ptr
  ret ptr %sp
stale:
  ret ptr null
}

define i32 @npk_arena_free(ptr %a, i64 %stride, i64 %idx, i32 %gen) {
entry:
  %p = call ptr @npk_arena_at(ptr %a, i64 %stride, i64 %idx, i32 %gen)
  %bad = icmp eq ptr %p, null
  br i1 %bad, label %stale, label %retire
retire:
  %ai = ptrtoint ptr %a to i64
  %gensa = add i64 %ai, 8
  %gensp = inttoptr i64 %gensa to ptr
  %gens = load ptr, ptr %gensp
  %gi = ptrtoint ptr %gens to i64
  %goff = shl i64 %idx, 2
  %ga = add i64 %gi, %goff
  %gp = inttoptr i64 %ga to ptr
  %g = load i32, ptr %gp
  %g2 = add i32 %g, 1
  store i32 %g2, ptr %gp
  ; retire at the cap: a slot at 0xFFFFFFFE never re-enters the freelist
  %cap = icmp ult i32 %g2, -2
  br i1 %cap, label %push, label %done
push:
  %fha = add i64 %ai, 32
  %fhp = inttoptr i64 %fha to ptr
  %fh = load i64, ptr %fhp
  %sp2 = ptrtoint ptr %p to i64
  %spp = inttoptr i64 %sp2 to ptr
  store i64 %fh, ptr %spp
  store i64 %idx, ptr %fhp
  br label %done
done:
  ret i32 0
stale:
  ret i32 1
}

define void @npk_arena_reset(ptr %a, i64 %stride) {
entry:
  %ai = ptrtoint ptr %a to i64
  %topa = add i64 %ai, 24
  %topp = inttoptr i64 %topa to ptr
  %top = load i64, ptr %topp
  %gensa = add i64 %ai, 8
  %gensp = inttoptr i64 %gensa to ptr
  %gens = load ptr, ptr %gensp
  %gi = ptrtoint ptr %gens to i64
  br label %head
head:
  %i = phi i64 [ 0, %entry ], [ %i2, %next ]
  %more = icmp ult i64 %i, %top
  br i1 %more, label %body, label %clear
body:
  %goff = shl i64 %i, 2
  %ga = add i64 %gi, %goff
  %gp = inttoptr i64 %ga to ptr
  %g = load i32, ptr %gp
  %odd = and i32 %g, 1
  %isodd = icmp ne i32 %odd, 0
  br i1 %isodd, label %next, label %bump
bump:
  %g2 = add i32 %g, 1
  store i32 %g2, ptr %gp
  br label %next
next:
  %i2 = add i64 %i, 1
  br label %head
clear:
  store i64 0, ptr %topp
  %fha = add i64 %ai, 32
  %fhp = inttoptr i64 %fha to ptr
  store i64 -1, ptr %fhp
  ret void
}

; ---------------------------------------------------------------------------
; THE EXECUTOR FRAME ALLOCATOR (0.10.3, D-153). NOT arena<T>, deliberately:
; the surface arena is a fixed-slot allocator handing out generation-checked
; INDICES, and a coroutine frame is a per-function, variably-sized block that
; @llvm.coro.begin needs as a RAW POINTER. Conflating them is the mistake the
; concurrency audit caught (total_audit B-1); this family is the allocator
; D-034 actually means, built where the heap lives and consumed by 1.1's
; coroutine lowering, which is its ONLY intended caller -- no surface type,
; no keyword, no builtin.
;
; SHAPE. One allocator per executor; tasks are pinned (D-032), so the whole
; path is single-threaded and ZERO-ATOMIC -- that is D-034's rationale for
; pinning. Chunks of 64 KiB bump-allocate frames; a completed task's frame
; returns to a free list BUCKETED BY EXACT SIZE, which fits coroutines
; precisely: a program has one frame size per async function, recurring many
; times. Frames larger than a chunk take a dedicated heap block (flag bit in
; the header). `drain` retires every frame at once by resetting the bump and
; the buckets, KEEPING the chunks -- the executor's steady state allocates
; from memory it already owns. Frame headers carry the size and a
; secret-keyed state magic: freeing a frame twice, or freeing a pointer that
; is not a live frame, traps -4102 like every other heap-integrity failure.
;
; The executor struct and its chunks are WILD-role heap blocks: an
; un-destroyed executor is a countable leak the D-151 exit check names.
;
; struct FrameExec (64 bytes, a wild heap block):
;   +0  chunk_head   first chunk (chunk: [ next i64 | pad | frames... ])
;   +8  cur_chunk    the chunk being bumped
;   +16 cur_off      bump offset within cur_chunk (starts at 16)
;   +24 bsizes       ptr to i64[cap]  bucket sizes (exact rounded frame size)
;   +32 bheads       ptr to i64[cap]  bucket free-list heads (frame addrs)
;   +40 bcount
;   +48 bcap
;   +56 reserved
; frame block: [ size i64 | state i64 ] then payload, 16-aligned; state is
; secret^addr^K_LIVE or ^K_FREE, with bit 0 of the SIZE word marking a
; dedicated (oversize) block.
; ---------------------------------------------------------------------------

define internal i64 @npk_m_flive(i64 %a) {
  %s = load i64, ptr @npk_hsec
  %x = xor i64 %s, %a
  %m = xor i64 %x, -602391508442037685
  ret i64 %m
}

define internal i64 @npk_m_ffree(i64 %a) {
  %s = load i64, ptr @npk_hsec
  %x = xor i64 %s, %a
  %m = xor i64 %x, 999621991244018563
  ret i64 %m
}

define ptr @npk_frame_exec_new() {
entry:
  %fe = call ptr @npk_alloc_impl(i64 64, i64 1)
  %fi = ptrtoint ptr %fe to i64
  ; one 64 KiB chunk up front; the steady state never maps again
  %ch = call ptr @npk_alloc_impl(i64 65536, i64 1)
  %ci = ptrtoint ptr %ch to i64
  %np = inttoptr i64 %ci to ptr
  store i64 0, ptr %np
  %hp = inttoptr i64 %fi to ptr
  store i64 %ci, ptr %hp
  %cca = add i64 %fi, 8
  %ccp = inttoptr i64 %cca to ptr
  store i64 %ci, ptr %ccp
  %coa = add i64 %fi, 16
  %cop = inttoptr i64 %coa to ptr
  store i64 16, ptr %cop
  ; eight buckets to start; ralloc grows the pair in step
  %bs = call ptr @npk_alloc_impl(i64 64, i64 1)
  %bh = call ptr @npk_alloc_impl(i64 64, i64 1)
  %bsa = add i64 %fi, 24
  %bsp = inttoptr i64 %bsa to ptr
  store ptr %bs, ptr %bsp
  %bha = add i64 %fi, 32
  %bhp = inttoptr i64 %bha to ptr
  store ptr %bh, ptr %bhp
  %bca = add i64 %fi, 40
  %bcp = inttoptr i64 %bca to ptr
  store i64 0, ptr %bcp
  %bpa = add i64 %fi, 48
  %bpp = inttoptr i64 %bpa to ptr
  store i64 8, ptr %bpp
  ret ptr %fe
}

; find the bucket for an exact rounded size; -1 when absent
define internal i64 @npk_frame_bucket(i64 %fi, i64 %rn) {
entry:
  %bca = add i64 %fi, 40
  %bcp = inttoptr i64 %bca to ptr
  %bc = load i64, ptr %bcp
  %bsa = add i64 %fi, 24
  %bsp = inttoptr i64 %bsa to ptr
  %bs = load ptr, ptr %bsp
  %bi = ptrtoint ptr %bs to i64
  br label %head
head:
  %i = phi i64 [ 0, %entry ], [ %i2, %next ]
  %more = icmp ult i64 %i, %bc
  br i1 %more, label %look, label %miss
look:
  %ea = shl i64 %i, 3
  %ep = add i64 %bi, %ea
  %epp = inttoptr i64 %ep to ptr
  %v = load i64, ptr %epp
  %hit = icmp eq i64 %v, %rn
  br i1 %hit, label %found, label %next
next:
  %i2 = add i64 %i, 1
  br label %head
found:
  ret i64 %i
miss:
  ret i64 -1
}

define ptr @npk_frame_alloc(ptr %fe, i64 %size, i64 %align) {
entry:
  %fi = ptrtoint ptr %fe to i64
  %negsz = icmp slt i64 %size, 0
  br i1 %negsz, label %badreq, label %alck
alck:
  ; coroutine frames align at or below the heap's own sixteen
  %zeroa = icmp sle i64 %align, 0
  %bigal = icmp sgt i64 %align, 16
  %bad = or i1 %zeroa, %bigal
  br i1 %bad, label %badreq, label %norm
badreq:
  call void @npk_heap_badreq()
  unreachable
norm:
  %z = icmp eq i64 %size, 0
  %n1 = select i1 %z, i64 16, i64 %size
  %r15 = add i64 %n1, 15
  %rn = and i64 %r15, -16
  ; exact-size bucket first: the common steady state
  %bkt = call i64 @npk_frame_bucket(i64 %fi, i64 %rn)
  %none = icmp slt i64 %bkt, 0
  br i1 %none, label %fresh, label %check
check:
  %bha = add i64 %fi, 32
  %bhp = inttoptr i64 %bha to ptr
  %bh = load ptr, ptr %bhp
  %hi = ptrtoint ptr %bh to i64
  %ha = shl i64 %bkt, 3
  %hp = add i64 %hi, %ha
  %hpp = inttoptr i64 %hp to ptr
  %head = load i64, ptr %hpp
  %empty = icmp eq i64 %head, 0
  br i1 %empty, label %fresh, label %pop
pop:
  ; the freed frame's payload first word is the next link
  %pa = add i64 %head, 16
  %pp = inttoptr i64 %pa to ptr
  %nxt = load i64, ptr %pp
  store i64 %nxt, ptr %hpp
  ; verify the freed magic, then stamp live
  %sma = add i64 %head, 8
  %smp = inttoptr i64 %sma to ptr
  %sv = load i64, ptr %smp
  %want = call i64 @npk_m_ffree(i64 %head)
  %ok = icmp eq i64 %sv, %want
  br i1 %ok, label %stamp, label %corrupt
corrupt:
  call void @npk_heap_bad()
  unreachable
stamp:
  %lm = call i64 @npk_m_flive(i64 %head)
  store i64 %lm, ptr %smp
  %u = add i64 %head, 16
  %uptr = inttoptr i64 %u to ptr
  ret ptr %uptr
fresh:
  ; oversize takes a dedicated heap block, flagged in the size word
  %need = add i64 %rn, 16
  %big = icmp ugt i64 %need, 65520
  br i1 %big, label %dedicated, label %bump
dedicated:
  %db = call ptr @npk_alloc_impl(i64 %need, i64 1)
  %di = ptrtoint ptr %db to i64
  %dsz = or i64 %rn, 1
  %dsp = inttoptr i64 %di to ptr
  store i64 %dsz, ptr %dsp
  %dma = add i64 %di, 8
  %dmp = inttoptr i64 %dma to ptr
  %dm = call i64 @npk_m_flive(i64 %di)
  store i64 %dm, ptr %dmp
  %du = add i64 %di, 16
  %dup = inttoptr i64 %du to ptr
  ret ptr %dup
bump:
  %coa = add i64 %fi, 16
  %cop = inttoptr i64 %coa to ptr
  %off = load i64, ptr %cop
  %end = add i64 %off, %need
  %fits = icmp ule i64 %end, 65536
  br i1 %fits, label %take, label %newchunk
newchunk:
  %nc = call ptr @npk_alloc_impl(i64 65536, i64 1)
  %nci = ptrtoint ptr %nc to i64
  ; push onto the chunk list, make it current
  %hpx = inttoptr i64 %fi to ptr
  %old = load i64, ptr %hpx
  %ncp = inttoptr i64 %nci to ptr
  store i64 %old, ptr %ncp
  store i64 %nci, ptr %hpx
  %cca = add i64 %fi, 8
  %ccp = inttoptr i64 %cca to ptr
  store i64 %nci, ptr %ccp
  store i64 16, ptr %cop
  br label %take
take:
  %cca2 = add i64 %fi, 8
  %ccp2 = inttoptr i64 %cca2 to ptr
  %cur = load i64, ptr %ccp2
  %off2 = load i64, ptr %cop
  %b = add i64 %cur, %off2
  %end2 = add i64 %off2, %need
  store i64 %end2, ptr %cop
  %bsp2 = inttoptr i64 %b to ptr
  store i64 %rn, ptr %bsp2
  %bma = add i64 %b, 8
  %bmp = inttoptr i64 %bma to ptr
  %bm = call i64 @npk_m_flive(i64 %b)
  store i64 %bm, ptr %bmp
  %bu = add i64 %b, 16
  %bup = inttoptr i64 %bu to ptr
  ret ptr %bup
}

define void @npk_frame_free(ptr %fe, ptr %frame) {
entry:
  %fi = ptrtoint ptr %fe to i64
  %pi = ptrtoint ptr %frame to i64
  %isz = icmp eq i64 %pi, 0
  br i1 %isz, label %bad, label %aligned
bad:
  call void @npk_heap_bad()
  unreachable
aligned:
  %mis = and i64 %pi, 15
  %crooked = icmp ne i64 %mis, 0
  br i1 %crooked, label %bad, label %hdr
hdr:
  %b = add i64 %pi, -16
  %sma = add i64 %b, 8
  %smp = inttoptr i64 %sma to ptr
  %sv = load i64, ptr %smp
  %want = call i64 @npk_m_flive(i64 %b)
  %ok = icmp eq i64 %sv, %want
  br i1 %ok, label %route, label %bad
route:
  %szp = inttoptr i64 %b to ptr
  %szw = load i64, ptr %szp
  %ded = and i64 %szw, 1
  %isded = icmp ne i64 %ded, 0
  br i1 %isded, label %dedic, label %bucket
dedic:
  ; a dedicated block goes back to the heap whole
  %dbp = inttoptr i64 %b to ptr
  call void @npk_dalloc(ptr %dbp)
  ret void
bucket:
  %fm = call i64 @npk_m_ffree(i64 %b)
  store i64 %fm, ptr %smp
  %bkt = call i64 @npk_frame_bucket(i64 %fi, i64 %szw)
  %none = icmp slt i64 %bkt, 0
  br i1 %none, label %newbucket, label %push
newbucket:
  ; grow the parallel arrays when full, then append the size
  %bca = add i64 %fi, 40
  %bcp = inttoptr i64 %bca to ptr
  %bc = load i64, ptr %bcp
  %bpa = add i64 %fi, 48
  %bpp = inttoptr i64 %bpa to ptr
  %bcap = load i64, ptr %bpp
  %full = icmp eq i64 %bc, %bcap
  br i1 %full, label %grow, label %append
grow:
  %ncap = shl i64 %bcap, 1
  %nbytes = shl i64 %ncap, 3
  %bsa = add i64 %fi, 24
  %bsp = inttoptr i64 %bsa to ptr
  %obs = load ptr, ptr %bsp
  %nbs = call ptr @npk_ralloc(ptr %obs, i64 %nbytes)
  store ptr %nbs, ptr %bsp
  %bha = add i64 %fi, 32
  %bhp = inttoptr i64 %bha to ptr
  %obh = load ptr, ptr %bhp
  %nbh = call ptr @npk_ralloc(ptr %obh, i64 %nbytes)
  store ptr %nbh, ptr %bhp
  store i64 %ncap, ptr %bpp
  br label %append
append:
  %bc2 = load i64, ptr %bcp
  %bsa2 = add i64 %fi, 24
  %bsp2 = inttoptr i64 %bsa2 to ptr
  %bs2 = load ptr, ptr %bsp2
  %si = ptrtoint ptr %bs2 to i64
  %sa = shl i64 %bc2, 3
  %sp = add i64 %si, %sa
  %spp = inttoptr i64 %sp to ptr
  store i64 %szw, ptr %spp
  %bha2 = add i64 %fi, 32
  %bhp2 = inttoptr i64 %bha2 to ptr
  %bh2 = load ptr, ptr %bhp2
  %hi2 = ptrtoint ptr %bh2 to i64
  %ha2 = add i64 %hi2, %sa
  %hpp2 = inttoptr i64 %ha2 to ptr
  store i64 0, ptr %hpp2
  %bc3 = add i64 %bc2, 1
  store i64 %bc3, ptr %bcp
  %bkt2 = add i64 %bc2, 0
  br label %push2
push:
  br label %push2
push2:
  %slot = phi i64 [ %bkt, %push ], [ %bkt2, %append ]
  %bha3 = add i64 %fi, 32
  %bhp3 = inttoptr i64 %bha3 to ptr
  %bh3 = load ptr, ptr %bhp3
  %hi3 = ptrtoint ptr %bh3 to i64
  %ha3 = shl i64 %slot, 3
  %hp3 = add i64 %hi3, %ha3
  %hpp3 = inttoptr i64 %hp3 to ptr
  %old = load i64, ptr %hpp3
  %la = add i64 %b, 16
  %lp = inttoptr i64 %la to ptr
  store i64 %old, ptr %lp
  store i64 %b, ptr %hpp3
  ret void
}

define void @npk_frame_drain(ptr %fe) {
entry:
  ; every frame dies at once: reset the bump to the FIRST chunk and empty
  ; the buckets; the chunks stay -- the steady state owns its memory
  %fi = ptrtoint ptr %fe to i64
  %hp = inttoptr i64 %fi to ptr
  %head = load i64, ptr %hp
  %cca = add i64 %fi, 8
  %ccp = inttoptr i64 %cca to ptr
  store i64 %head, ptr %ccp
  %coa = add i64 %fi, 16
  %cop = inttoptr i64 %coa to ptr
  store i64 16, ptr %cop
  %bca = add i64 %fi, 40
  %bcp = inttoptr i64 %bca to ptr
  store i64 0, ptr %bcp
  ret void
}

define void @npk_frame_exec_destroy(ptr %fe) {
entry:
  %fi = ptrtoint ptr %fe to i64
  %hp = inttoptr i64 %fi to ptr
  %head = load i64, ptr %hp
  br label %walk
walk:
  %c = phi i64 [ %head, %entry ], [ %nxt, %freec ]
  %done = icmp eq i64 %c, 0
  br i1 %done, label %arrays, label %freec
freec:
  %np = inttoptr i64 %c to ptr
  %nxt = load i64, ptr %np
  %cp = inttoptr i64 %c to ptr
  call void @npk_dalloc(ptr %cp)
  br label %walk
arrays:
  %bsa = add i64 %fi, 24
  %bsp = inttoptr i64 %bsa to ptr
  %bs = load ptr, ptr %bsp
  call void @npk_dalloc(ptr %bs)
  %bha = add i64 %fi, 32
  %bhp = inttoptr i64 %bha to ptr
  %bh = load ptr, ptr %bhp
  call void @npk_dalloc(ptr %bh)
  call void @npk_dalloc(ptr %fe)
  ret void
}

define void @npk_arena_destroy(ptr %a) {
entry:
  %ai = ptrtoint ptr %a to i64
  %slaba = inttoptr i64 %ai to ptr
  %slab = load ptr, ptr %slaba
  %has = icmp ne ptr %slab, null
  br i1 %has, label %freeslab, label %gens
freeslab:
  call void @npk_dalloc(ptr %slab)
  br label %gens
gens:
  %gensa = add i64 %ai, 8
  %gensp = inttoptr i64 %gensa to ptr
  %garr = load ptr, ptr %gensp
  %hasg = icmp ne ptr %garr, null
  br i1 %hasg, label %freegens, label %zero
freegens:
  call void @npk_dalloc(ptr %garr)
  br label %zero
zero:
  store ptr null, ptr %slaba
  store ptr null, ptr %gensp
  %capa = add i64 %ai, 16
  %capp = inttoptr i64 %capa to ptr
  store i64 0, ptr %capp
  %topa = add i64 %ai, 24
  %topp = inttoptr i64 %topa to ptr
  store i64 0, ptr %topp
  %fha = add i64 %ai, 32
  %fhp = inttoptr i64 %fha to ptr
  store i64 -1, ptr %fhp
  ret void
}

; ---------------------------------------------------------------------------
; The symbols LLVM emits calls to. With no libc these must exist.
; ---------------------------------------------------------------------------

define ptr @memcpy(ptr %dst, ptr %src, i64 %n) {
entry:
  br label %head
head:
  %i = phi i64 [ 0, %entry ], [ %i2, %body ]
  %more = icmp ult i64 %i, %n
  br i1 %more, label %body, label %done
body:
  %sp = getelementptr i8, ptr %src, i64 %i
  %dp = getelementptr i8, ptr %dst, i64 %i
  %b = load i8, ptr %sp
  store i8 %b, ptr %dp
  %i2 = add i64 %i, 1
  br label %head
done:
  ret ptr %dst
}

define ptr @memset(ptr %dst, i32 %c, i64 %n) {
entry:
  %b = trunc i32 %c to i8
  br label %head
head:
  %i = phi i64 [ 0, %entry ], [ %i2, %body ]
  %more = icmp ult i64 %i, %n
  br i1 %more, label %body, label %done
body:
  %dp = getelementptr i8, ptr %dst, i64 %i
  store i8 %b, ptr %dp
  %i2 = add i64 %i, 1
  br label %head
done:
  ret ptr %dst
}

define ptr @memmove(ptr %dst, ptr %src, i64 %n) {
entry:
  ; A real move, not a memcpy alias (0.10.0): when dst overlaps src from
  ; above, a forward copy reads bytes it has already overwritten.
  %di = ptrtoint ptr %dst to i64
  %si = ptrtoint ptr %src to i64
  %down = icmp ult i64 %di, %si
  br i1 %down, label %fwd, label %bwd
fwd:
  %r = call ptr @memcpy(ptr %dst, ptr %src, i64 %n)
  ret ptr %r
bwd:
  br label %bhead
bhead:
  %i = phi i64 [ %n, %bwd ], [ %i2, %bbody ]
  %more = icmp ugt i64 %i, 0
  br i1 %more, label %bbody, label %bdone
bbody:
  %i2 = add i64 %i, -1
  %sp = getelementptr i8, ptr %src, i64 %i2
  %dp = getelementptr i8, ptr %dst, i64 %i2
  %b = load i8, ptr %sp
  store i8 %b, ptr %dp
  br label %bhead
bdone:
  ret ptr %dst
}

declare void @llvm.memcpy.p0.p0.i64(ptr, ptr, i64, i1)
declare void @llvm.memset.p0.i64(ptr, i8, i64, i1)
declare i64 @llvm.cttz.i64(i64, i1)
declare { i64, i1 } @llvm.umul.with.overflow.i64(i64, i64)

; ---------------------------------------------------------------------------
; Strings.
;
; string is {ptr, i64 len, i64 cap} and these return Result<string>, which is
; { {ptr,i64,i64}, i32 } -- the error field is tbb32, zero meaning success
; (D-069). The seed's checker types them exactly this way; the two must agree or
; llc rejects the caller, which is how the mismatch was caught the first time.
; ---------------------------------------------------------------------------

define { { ptr, i64, i64 }, i32 } @npk_string_concat({ ptr, i64, i64 } %a,
                                                     { ptr, i64, i64 } %b) {
entry:
  %ap = extractvalue { ptr, i64, i64 } %a, 0
  %al = extractvalue { ptr, i64, i64 } %a, 1
  %bp = extractvalue { ptr, i64, i64 } %b, 0
  %bl = extractvalue { ptr, i64, i64 } %b, 1
  %n = add i64 %al, %bl
  %p = call ptr @npk_alloc_internal(i64 %n)
  call ptr @memcpy(ptr %p, ptr %ap, i64 %al)
  %tail = getelementptr i8, ptr %p, i64 %al
  call ptr @memcpy(ptr %tail, ptr %bp, i64 %bl)
  %s0 = insertvalue { ptr, i64, i64 } undef, ptr %p, 0
  %s1 = insertvalue { ptr, i64, i64 } %s0, i64 %n, 1
  %s2 = insertvalue { ptr, i64, i64 } %s1, i64 %n, 2
  %r0 = insertvalue { { ptr, i64, i64 }, i32 } undef, { ptr, i64, i64 } %s2, 0
  %r1 = insertvalue { { ptr, i64, i64 }, i32 } %r0, i32 0, 1
  ret { { ptr, i64, i64 }, i32 } %r1
}

define { { ptr, i64, i64 }, i32 } @npk_int_to_string(i64 %v) {
entry:
  ; Fill a 24-byte buffer from the END, then return a view of the filled tail.
  ; No reversal step, and 24 bytes is enough for -9223372036854775808.
  %buf = call ptr @npk_alloc_internal(i64 24)
  %isneg = icmp slt i64 %v, 0
  %neg = sub i64 0, %v
  %u0 = select i1 %isneg, i64 %neg, i64 %v
  %zero = icmp eq i64 %u0, 0
  br i1 %zero, label %just_zero, label %digits

just_zero:
  %z = getelementptr i8, ptr %buf, i64 23
  store i8 48, ptr %z
  br label %sign

digits:
  %i = phi i64 [ 24, %entry ], [ %i2, %digits ]
  %u = phi i64 [ %u0, %entry ], [ %u2, %digits ]
  %i2 = sub i64 %i, 1
  %q = udiv i64 %u, 10
  %m = urem i64 %u, 10
  %d = trunc i64 %m to i8
  %ch = add i8 %d, 48
  %dp = getelementptr i8, ptr %buf, i64 %i2
  store i8 %ch, ptr %dp
  %u2 = udiv i64 %u, 10
  %more = icmp ugt i64 %q, 0
  br i1 %more, label %digits, label %after

after:
  br label %sign

sign:
  %start0 = phi i64 [ 23, %just_zero ], [ %i2, %after ]
  br i1 %isneg, label %put_sign, label %build

put_sign:
  %sp = sub i64 %start0, 1
  %spp = getelementptr i8, ptr %buf, i64 %sp
  store i8 45, ptr %spp
  br label %build

build:
  %start = phi i64 [ %start0, %sign ], [ %sp, %put_sign ]
  %len = sub i64 24, %start
  %base = getelementptr i8, ptr %buf, i64 %start
  %s0 = insertvalue { ptr, i64, i64 } undef, ptr %base, 0
  %s1 = insertvalue { ptr, i64, i64 } %s0, i64 %len, 1
  %s2 = insertvalue { ptr, i64, i64 } %s1, i64 %len, 2
  %r0 = insertvalue { { ptr, i64, i64 }, i32 } undef, { ptr, i64, i64 } %s2, 0
  %r1 = insertvalue { { ptr, i64, i64 }, i32 } %r0, i32 0, 1
  ret { { ptr, i64, i64 }, i32 } %r1
}

define { { ptr, i64, i64 }, i32 } @npk_string_slice({ ptr, i64, i64 } %s,
                                                    i64 %start, i64 %end) {
entry:
  %p = extractvalue { ptr, i64, i64 } %s, 0
  %l = extractvalue { ptr, i64, i64 } %s, 1
  ; Bounds are CHECKED, not assumed. A slice is a view, and a view past the end
  ; of its backing store is the defect the whole type exists to prevent (D-070).
  %b1 = icmp ult i64 %end, %start
  %b2 = icmp ugt i64 %end, %l
  %bad = or i1 %b1, %b2
  br i1 %bad, label %err, label %ok

err:
  %e0 = insertvalue { { ptr, i64, i64 }, i32 } undef,
                    { ptr, i64, i64 } zeroinitializer, 0
  %e1 = insertvalue { { ptr, i64, i64 }, i32 } %e0, i32 -34, 1
  ret { { ptr, i64, i64 }, i32 } %e1

ok:
  %np = getelementptr i8, ptr %p, i64 %start
  %n = sub i64 %end, %start
  %s0 = insertvalue { ptr, i64, i64 } undef, ptr %np, 0
  %s1 = insertvalue { ptr, i64, i64 } %s0, i64 %n, 1
  %s2 = insertvalue { ptr, i64, i64 } %s1, i64 %n, 2
  %r0 = insertvalue { { ptr, i64, i64 }, i32 } undef, { ptr, i64, i64 } %s2, 0
  %r1 = insertvalue { { ptr, i64, i64 }, i32 } %r0, i32 0, 1
  ret { { ptr, i64, i64 }, i32 } %r1
}

; The dual of .ptr / .len: wrap a buffer the caller already owns. Used by the
; lexer to build a decoded string literal, where the decoded bytes are not a
; slice of the source.
define { ptr, i64, i64 } @npk_string_from_bytes(ptr %p, i64 %n) {
  %s0 = insertvalue { ptr, i64, i64 } undef, ptr %p, 0
  %s1 = insertvalue { ptr, i64, i64 } %s0, i64 %n, 1
  %s2 = insertvalue { ptr, i64, i64 } %s1, i64 %n, 2
  ret { ptr, i64, i64 } %s2
}
