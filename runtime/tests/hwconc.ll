; expect-exit: 0
;
; DEF-52: `npk_hardware_concurrency` must answer the popcount of the mask THE
; KERNEL WROTE, whatever the stack held before it ran. No surface syntax reaches
; the symbol, so the test is hand-written IR (the `frames.ll` shape).
;
; The test dirties the stack below itself with one-bits, then asks the floor,
; then computes the truth itself from a ZEROED mask and the raw syscall. A floor
; that popcounts stack garbage answers more than the truth.
;
;   exit 0  the floor's answer equals the truth
;   exit 1  the floor's answer differs                (the defect)
;   exit 2  the truth itself is not in 1..1024        (the test is wrong)
;   exit 9  a trap

declare i64 @npk_hardware_concurrency()
declare i64 @npk_sys6(i64, i64, i64, i64, i64, i64, i64)
declare ptr @memset(ptr, i32, i64)
declare i64 @llvm.ctpop.i64(i64)

; Fill 8 KiB of stack with 0xFF and hand the pointer to an opaque call, so no
; tool can decide the fill is dead. getpid (39) reads none of its arguments.
define void @dirty() noinline {
entry:
  %buf = alloca [8192 x i8], align 16
  %p = call ptr @memset(ptr %buf, i32 255, i64 8192)
  %pi = ptrtoint ptr %buf to i64
  %r = call i64 @npk_sys6(i64 39, i64 %pi, i64 0, i64 0, i64 0, i64 0, i64 0)
  ret void
}

; The truth: a zeroed 128-byte mask, the raw syscall, the popcount of all of it.
define i64 @truth() noinline {
entry:
  %mask = alloca [16 x i64], align 16
  %z0 = call ptr @memset(ptr %mask, i32 0, i64 128)
  %mi = ptrtoint ptr %mask to i64
  %z = call i64 @npk_sys6(i64 204, i64 0, i64 128, i64 %mi, i64 0, i64 0, i64 0)
  br label %loop
loop:
  %i = phi i64 [ 0, %entry ], [ %ni, %step ]
  %acc = phi i64 [ 0, %entry ], [ %nacc, %step ]
  %end = icmp uge i64 %i, 16
  br i1 %end, label %fin, label %step
step:
  %wp = getelementptr [16 x i64], ptr %mask, i64 0, i64 %i
  %w = load i64, ptr %wp
  %pc = call i64 @llvm.ctpop.i64(i64 %w)
  %nacc = add i64 %acc, %pc
  %ni = add i64 %i, 1
  br label %loop
fin:
  ret i64 %acc
}

define i32 @main({ ptr, i64 } %argv) {
entry:
  %t = call i64 @truth()
  %lo = icmp sge i64 %t, 1
  %hi = icmp sle i64 %t, 1024
  %sane = and i1 %lo, %hi
  br i1 %sane, label %ask, label %f2
f2:
  ret i32 2
ask:
  call void @dirty()
  %n = call i64 @npk_hardware_concurrency()
  %same = icmp eq i64 %n, %t
  br i1 %same, label %ok, label %f1
f1:
  ret i32 1
ok:
  ret i32 0
}

define i32 @npk_failsafe(i32 %code) {
  ret i32 9
}
