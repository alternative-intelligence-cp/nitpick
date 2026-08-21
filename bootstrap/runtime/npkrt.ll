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
; This runs before `main`, so it allocates the cstring array from the bump
; allocator and never frees it -- which is what the allocator does anyway, and
; argv outlives everything regardless.
; ---------------------------------------------------------------------------

declare i32 @main({ ptr, i64 })

define internal void @npk_start(i64 %sp) noreturn {
entry:
  %spp = inttoptr i64 %sp to ptr
  %argc = load i64, ptr %spp
  %argvp = getelementptr i8, ptr %spp, i64 8      ; &argv[0]
  %bytes = mul i64 %argc, 16                      ; sizeof(cstring) = {ptr,i64}
  %buf = call ptr @npk_alloc(i64 %bytes)
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

define internal i64 @npk_sys6(i64 %nr, i64 %a1, i64 %a2, i64 %a3,
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
;
; The route is trap -> the program's own `failsafe` -> exit with its return.
; Every program defines `failsafe` (D-013, mandatory), so @npk_failsafe always
; resolves at link. D-014 requires failsafe to return POSITIVE; until 1.3
; injects and verifies that `ensures`, the runtime refuses to report success
; after a fault: a nonpositive return exits 70, the floor's own
; runtime-violation code.
; ---------------------------------------------------------------------------

declare i32 @npk_failsafe(i32)

define void @npk_trap(i32 %code) noreturn {
  %r = call i32 @npk_failsafe(i32 %code)
  %bad = icmp sle i32 %r, 0
  %code2 = select i1 %bad, i32 70, i32 %r
  call void @npk_exit(i32 %code2)
  unreachable
}

define void @npk_exit(i32 %code) noreturn {
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

; `write_all` is the retry loop over `write` -- discipline, not a syscall, and
; in IR only because stepping a pointer is not yet expressible in the language
; (#ptr_add is a 0.9 rung). When it is, this graduates to lib/nio.npk and the
; floor loses a symbol.
define { i32 } @npk_write_all(i32 %fd, ptr %buf, i64 %len) {
entry:
  %f = sext i32 %fd to i64
  br label %wloop
wloop:
  %off = phi i64 [ 0, %entry ], [ %off2, %wnext ]
  %left = sub i64 %len, %off
  %done = icmp eq i64 %left, 0
  br i1 %done, label %ok, label %wone
wone:
  %at = getelementptr i8, ptr %buf, i64 %off
  %ati = ptrtoint ptr %at to i64
  %n = call i64 @npk_sys6(i64 1, i64 %f, i64 %ati, i64 %left, i64 0, i64 0, i64 0)
  %bad = icmp slt i64 %n, 0
  br i1 %bad, label %err, label %wnext
wnext:
  %off2 = add i64 %off, %n
  br label %wloop
ok:
  ret { i32 } zeroinitializer
err:
  %c = trunc i64 %n to i32
  %e0 = insertvalue { i32 } undef, i32 %c, 0
  ret { i32 } %e0
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
  %buf = call ptr @npk_alloc(i64 %sz)
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
  %buf0 = call ptr @npk_alloc(i64 65536)
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
  %buf2 = call ptr @npk_alloc(i64 %cap2)
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
  %buf0 = call ptr @npk_alloc(i64 65536)
  br label %loop

loop:                                     ; preds = %entry, %iter
  %buf = phi ptr [ %buf0, %entry ], [ %rbuf, %iter ]
  %cap = phi i64 [ 65536, %entry ], [ %rcap, %iter ]
  %len = phi i64 [ 0, %entry ], [ %len_n, %iter ]
  %room = sub i64 %cap, %len
  %full = icmp eq i64 %room, 0
  br i1 %full, label %grow, label %read

grow:                                     ; preds = %loop
  ; The bump allocator never frees, so growing copies into a fresh block and
  ; abandons the old one. That is the trade the allocator's own comment already
  ; accepted: this process runs once and exits.
  %cap2 = mul i64 %cap, 2
  %buf2 = call ptr @npk_alloc(i64 %cap2)
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
; Allocation: a bump allocator that never frees.
;
; This is CORRECT here, not a shortcut. The compiler is a process that runs once
; and exits, so reclamation buys nothing -- and an allocator is exactly the kind
; of subtle code that must not live in the least-audited artifact in the chain.
; dalloc is therefore a no-op, and our sources still write `defer { dalloc(p); }`
; so they stay correct when real allocation lands.
;
; EVERY BLOCK CARRIES ITS SIZE IN SIXTEEN BYTES IN FRONT OF IT (0.7.3), which is
; the whole reason ralloc can be correct. Without it, ralloc had no way to know how
; much of the old block was real and copied the NEW size out of it -- an
; out-of-bounds read that leaves the mapping entirely when the old block sits near
; the end of a chunk, and takes the process down with SIGBUS.
;
; IT WAS NOT A LATENT BUG. About 511 declarations in one file is enough to reach a
; doubling that crosses a chunk boundary, and FIVE OF THE COMPILER'S OWN SOURCES
; exceed it -- so the compiler could not parse itself, and nothing said so because
; the harness had never asked it to.
;
; The alternative was to pass the old size in at every call site, which every
; caller does know. It was rejected: that moves a memory-safety obligation onto
; twenty call sites, and the one that forgets is silent corruption rather than a
; compile error. A header costs sixteen bytes per allocation in a process that runs
; once and exits.
; ---------------------------------------------------------------------------

@npk_cur = internal global i64 0
@npk_end = internal global i64 0

define ptr @npk_alloc(i64 %n) {
entry:
  ; Round the request up to 16 bytes so every allocation is aligned, then take one
  ; more 16-byte slot in front for the size. Sixteen and not eight: the header has
  ; to keep the returned pointer 16-aligned, which is what every caller's struct
  ; array depends on.
  %a = add i64 %n, 15
  %szb = and i64 %a, -16
  %sz = add i64 %szb, 16
  %cur = load i64, ptr @npk_cur
  %new = add i64 %cur, %sz
  %end = load i64, ptr @npk_end
  %fits = icmp ule i64 %new, %end
  %live = icmp ne i64 %cur, 0
  %ok = and i1 %fits, %live
  br i1 %ok, label %bump, label %grow

bump:
  store i64 %new, ptr @npk_cur
  %hdr1 = inttoptr i64 %cur to ptr
  store i64 %szb, ptr %hdr1
  %u1 = add i64 %cur, 16
  %p1 = inttoptr i64 %u1 to ptr
  ret ptr %p1

grow:
  ; take at least 1 MiB, or the whole request rounded to a page if larger
  %need = add i64 %sz, 4095
  %needp = and i64 %need, -4096
  %big = icmp ugt i64 %needp, 1048576
  %chunk = select i1 %big, i64 %needp, i64 1048576
  ; mmap(NULL, chunk, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0)
  %m = call i64 @npk_sys6(i64 9, i64 0, i64 %chunk, i64 3, i64 34, i64 -1, i64 0)
  ; a syscall error is a small negative value returned as a large unsigned one
  %bad = icmp ugt i64 %m, -4096
  br i1 %bad, label %oom, label %fresh

fresh:
  %nend = add i64 %m, %chunk
  store i64 %nend, ptr @npk_end
  %ncur = add i64 %m, %sz
  store i64 %ncur, ptr @npk_cur
  %hdr2 = inttoptr i64 %m to ptr
  store i64 %szb, ptr %hdr2
  %u2 = add i64 %m, 16
  %p2 = inttoptr i64 %u2 to ptr
  ret ptr %p2

oom:
  ; Out of memory is unrecoverable here. The real runtime routes this through
  ; failsafe; the seed has no allocation to preallocate for, so it stops.
  call void @npk_exit(i32 70)
  unreachable
}

define ptr @npk_calloc(i64 %count, i64 %size) {
  %n = mul i64 %count, %size
  %p = call ptr @npk_alloc(i64 %n)
  call void @llvm.memset.p0.i64(ptr %p, i8 0, i64 %n, i1 false)
  ret ptr %p
}

define ptr @npk_ralloc(ptr %old, i64 %n) {
entry:
  ; Never freeing means realloc is always a fresh block plus a copy. Correct,
  ; and wasteful in a way that does not matter for a process that exits.
  ;
  ; THE COPY IS BOUNDED BY THE OLD BLOCK, not by the new size. Copying `n` bytes
  ; out of a block that is `old` bytes long reads past its end, and when the block
  ; sits near the end of a chunk that read leaves the mapping -- SIGBUS, in the
  ; least-audited artifact in the chain, on every program large enough to grow an
  ; array twice.
  %isnull = icmp eq ptr %old, null
  br i1 %isnull, label %plain, label %copy

plain:
  ; A null old block is a first allocation, and has no header to read.
  %f = call ptr @npk_alloc(i64 %n)
  ret ptr %f

copy:
  %p = call ptr @npk_alloc(i64 %n)
  %oi = ptrtoint ptr %old to i64
  %hi = sub i64 %oi, 16
  %hp = inttoptr i64 %hi to ptr
  %osz = load i64, ptr %hp
  %smaller = icmp ult i64 %osz, %n
  %cnt = select i1 %smaller, i64 %osz, i64 %n
  call void @llvm.memcpy.p0.p0.i64(ptr %p, ptr %old, i64 %cnt, i1 false)
  ret ptr %p
}

define void @npk_dalloc(ptr %p) {
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
  %r = call ptr @memcpy(ptr %dst, ptr %src, i64 %n)
  ret ptr %r
}

declare void @llvm.memcpy.p0.p0.i64(ptr, ptr, i64, i1)
declare void @llvm.memset.p0.i64(ptr, i8, i64, i1)

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
  %p = call ptr @npk_alloc(i64 %n)
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
  %buf = call ptr @npk_alloc(i64 24)
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
