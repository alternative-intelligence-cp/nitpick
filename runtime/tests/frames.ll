; expect-exit: 0
;
; THE EXECUTOR FRAME ALLOCATOR, DRIVEN DIRECTLY (0.10.3, D-153). No surface
; syntax reaches this family until 1.1's coroutine lowering, so the test is
; hand-written IR against the fixed interface -- the same five calls C-7
; will emit. It proves: exact-size LIFO reuse, out-of-order frees, the
; oversize path, chunk growth under pressure, drain resetting the bump into
; memory the executor already owns, and destroy returning every byte -- the
; wild-live count is zero afterwards, and the exit-time check (D-151) holds
; the same fact at exit 0.

declare ptr @npk_frame_exec_new()
declare ptr @npk_frame_alloc(ptr, i64, i64)
declare void @npk_frame_free(ptr, ptr)
declare void @npk_frame_drain(ptr)
declare void @npk_frame_exec_destroy(ptr)
declare i64 @npk_wild_live_count()

define i32 @main({ ptr, i64 } %argv) {
entry:
  %fe = call ptr @npk_frame_exec_new()

  ; --- exact-size LIFO reuse, frees out of order --------------------------
  %a = call ptr @npk_frame_alloc(ptr %fe, i64 64, i64 16)
  %b = call ptr @npk_frame_alloc(ptr %fe, i64 128, i64 8)
  %c = call ptr @npk_frame_alloc(ptr %fe, i64 64, i64 16)
  store i64 42, ptr %a
  %av = load i64, ptr %a
  %aok = icmp eq i64 %av, 42
  br i1 %aok, label %frees, label %f10
f10:
  ret i32 10
frees:
  call void @npk_frame_free(ptr %fe, ptr %a)
  call void @npk_frame_free(ptr %fe, ptr %c)
  %d = call ptr @npk_frame_alloc(ptr %fe, i64 64, i64 16)
  %dc = icmp eq ptr %d, %c
  br i1 %dc, label %r2, label %f11
f11:
  ret i32 11
r2:
  %e = call ptr @npk_frame_alloc(ptr %fe, i64 64, i64 16)
  %ea = icmp eq ptr %e, %a
  br i1 %ea, label %sizes, label %f12
f12:
  ret i32 12
sizes:
  ; a 128-frame free does not satisfy a 64 request or vice versa
  call void @npk_frame_free(ptr %fe, ptr %b)
  %g = call ptr @npk_frame_alloc(ptr %fe, i64 128, i64 8)
  %gb = icmp eq ptr %g, %b
  br i1 %gb, label %drain1, label %f13
f13:
  ret i32 13

drain1:
  ; --- drain resets the bump into the FIRST chunk -------------------------
  call void @npk_frame_drain(ptr %fe)
  %h = call ptr @npk_frame_alloc(ptr %fe, i64 64, i64 16)
  %ha = icmp eq ptr %h, %a
  br i1 %ha, label %oversize, label %f14
f14:
  ret i32 14

oversize:
  ; --- a frame larger than a chunk takes a dedicated heap block -----------
  %big = call ptr @npk_frame_alloc(ptr %fe, i64 100000, i64 16)
  store i8 7, ptr %big
  %tail = getelementptr i8, ptr %big, i64 99999
  store i8 9, ptr %tail
  call void @npk_frame_free(ptr %fe, ptr %big)
  br label %growth

growth:
  ; --- 200 frames of 1024 bytes force chunk growth ------------------------
  br label %ghead
ghead:
  %i = phi i64 [ 0, %growth ], [ %i2, %gbody ]
  %more = icmp ult i64 %i, 200
  br i1 %more, label %gbody, label %gdone
gbody:
  %p = call ptr @npk_frame_alloc(ptr %fe, i64 1024, i64 16)
  store i64 %i, ptr %p
  %i2 = add i64 %i, 1
  br label %ghead
gdone:

  ; --- destroy returns every byte -----------------------------------------
  call void @npk_frame_exec_destroy(ptr %fe)
  %live = call i64 @npk_wild_live_count()
  %clean = icmp eq i64 %live, 0
  br i1 %clean, label %ok, label %f15
f15:
  ret i32 15
ok:
  ret i32 0
}

define i32 @npk_failsafe(i32 %code) {
  ; any trap during this test is a failure with a recognizable code
  ret i32 9
}
