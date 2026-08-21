; expect-exit: 0
;
; THE NON-MOVING PROOF (0.10.4, D-154) -- the property that decides D-017's
; whole design, testable only with a raw pointer: take the slot address of
; the FIRST allocation, force growth through many chunks, and read the
; original pointer again. For arena<T> this exact sequence dangles (the slab
; reallocates); for shared_arena the chunk that holds slot 0 never moves.
; Also drives the reservation-based growth path hard from one thread: 10000
; allocations across geometrically larger chunks, every value read back.

declare ptr @npk_sarena_make(i64, i64)
declare i64 @npk_sarena_bump(ptr, i64)
declare ptr @npk_sarena_slot(ptr, i64, i64)
declare void @npk_sarena_destroy(ptr)
declare i64 @npk_wild_live_count()

define i32 @main({ ptr, i64 } %argv) {
entry:
  %sa = call ptr @npk_sarena_make(i64 8, i64 4)
  ; slot 0: write, keep the RAW POINTER
  %i0 = call i64 @npk_sarena_bump(ptr %sa, i64 8)
  %p0 = call ptr @npk_sarena_slot(ptr %sa, i64 8, i64 %i0)
  store i64 424242, ptr %p0
  br label %fill
fill:
  %i = phi i64 [ 1, %entry ], [ %i2, %fbody ]
  %more = icmp ult i64 %i, 10000
  br i1 %more, label %fbody, label %check
fbody:
  %ix = call i64 @npk_sarena_bump(ptr %sa, i64 8)
  %px = call ptr @npk_sarena_slot(ptr %sa, i64 8, i64 %ix)
  store i64 %ix, ptr %px
  %i2 = add i64 %i, 1
  br label %fill
check:
  ; the ORIGINAL pointer, after ~10 chunk installs: still the same slot
  %v0 = load i64, ptr %p0
  %ok0 = icmp eq i64 %v0, 424242
  br i1 %ok0, label %sweep, label %f10
f10:
  ret i32 10
sweep:
  ; read every slot back through the walk
  br label %shead
shead:
  %j = phi i64 [ 1, %sweep ], [ %j2, %snext ]
  %smore = icmp ult i64 %j, 10000
  br i1 %smore, label %sbody, label %forged
sbody:
  %pj = call ptr @npk_sarena_slot(ptr %sa, i64 8, i64 %j)
  %vj = load i64, ptr %pj
  %jok = icmp eq i64 %vj, %j
  br i1 %jok, label %snext, label %f11
f11:
  ret i32 11
snext:
  %j2 = add i64 %j, 1
  br label %shead
forged:
  ; an index past the published count is null, never a wild read
  %pf = call ptr @npk_sarena_slot(ptr %sa, i64 8, i64 999999)
  %fnull = icmp eq ptr %pf, null
  br i1 %fnull, label %teardown, label %f12
f12:
  ret i32 12
teardown:
  call void @npk_sarena_destroy(ptr %sa)
  %live = call i64 @npk_wild_live_count()
  %clean = icmp eq i64 %live, 0
  br i1 %clean, label %ok, label %f13
f13:
  ret i32 13
ok:
  ret i32 0
}

define i32 @npk_failsafe(i32 %code) {
  ret i32 9
}
