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
module asm "  andq $-16, %rsp"
module asm "  callq main"
module asm "  movl %eax, %edi"
module asm "  movl $60, %eax"          ; SYS_exit
module asm "  syscall"
module asm "  hlt"

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

define void @npk_exit(i32 %code) noreturn {
  %c = sext i32 %code to i64
  %r = call i64 @npk_sys6(i64 60, i64 %c, i64 0, i64 0, i64 0, i64 0, i64 0)
  unreachable
}

define i64 @npk_write(i32 %fd, ptr %buf, i64 %len) {
  %f = sext i32 %fd to i64
  %p = ptrtoint ptr %buf to i64
  %r = call i64 @npk_sys6(i64 1, i64 %f, i64 %p, i64 %len, i64 0, i64 0, i64 0)
  ret i64 %r
}

; ---------------------------------------------------------------------------
; Allocation: a bump allocator that never frees.
;
; This is CORRECT here, not a shortcut. The compiler is a process that runs once
; and exits, so reclamation buys nothing -- and an allocator is exactly the kind
; of subtle code that must not live in the least-audited artifact in the chain.
; dalloc is therefore a no-op, and our sources still write `defer { dalloc(p); }`
; so they stay correct when real allocation lands.
; ---------------------------------------------------------------------------

@npk_cur = internal global i64 0
@npk_end = internal global i64 0

define ptr @npk_alloc(i64 %n) {
entry:
  ; round the request up to 16 bytes so every allocation is aligned
  %a = add i64 %n, 15
  %sz = and i64 %a, -16
  %cur = load i64, ptr @npk_cur
  %new = add i64 %cur, %sz
  %end = load i64, ptr @npk_end
  %fits = icmp ule i64 %new, %end
  %live = icmp ne i64 %cur, 0
  %ok = and i1 %fits, %live
  br i1 %ok, label %bump, label %grow

bump:
  store i64 %new, ptr @npk_cur
  %p1 = inttoptr i64 %cur to ptr
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
  %p2 = inttoptr i64 %m to ptr
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
  ; Never freeing means realloc is always a fresh block plus a copy. Correct,
  ; and wasteful in a way that does not matter for a process that exits.
  %p = call ptr @npk_alloc(i64 %n)
  call void @llvm.memcpy.p0.p0.i64(ptr %p, ptr %old, i64 %n, i1 false)
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
