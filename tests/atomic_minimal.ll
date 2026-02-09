; ModuleID = 'tests/atomic_minimal.aria'
source_filename = "tests/atomic_minimal.aria"

declare internal ptr @aria_atomic_int32_create(i32)

declare internal void @aria_atomic_int32_destroy(ptr)

declare internal void @aria_atomic_int32_store(ptr, i32, i32)

declare internal i32 @aria_atomic_int32_load(ptr, i32)

define i32 @main() {
entry:
  call void @aria_gc_init(i64 0, i64 0)
  %ptr = alloca ptr, align 8
  %calltmp = call ptr @aria_atomic_int32_create(i32 0)
  store ptr %calltmp, ptr %ptr, align 8
  %value = alloca i32, align 4
  %ptr1 = load ptr, ptr %ptr, align 8
  %calltmp2 = call i32 @aria_atomic_int32_load(ptr %ptr1, i32 4)
  store i32 %calltmp2, ptr %value, align 4
  %value3 = load i32, ptr %value, align 4
  %cmp_cast = sext i32 %value3 to i64
  %eqtmp = icmp eq i64 %cmp_cast, 42
  %ifcond = icmp ne i1 %eqtmp, false
  br i1 %ifcond, label %then, label %else

then:                                             ; preds = %entry
  ret i32 0

else:                                             ; preds = %entry
  ret i32 1

ifcont:                                           ; No predecessors!
  ret i32 0
}

declare void @aria_gc_init(i64, i64)

define i32 @__atomic_minimal_init() {
entry:
  ret i32 0
}
