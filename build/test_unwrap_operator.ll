; ModuleID = 'aria_module'
source_filename = "aria_module"

%Result = type { ptr, i64 }
%Result.0 = type { ptr, i64 }

@0 = private unnamed_addr constant [17 x i8] c"division by zero\00", align 1

declare void @puts(ptr)

declare void @print(ptr)

define internal void @__aria_module_init() {
entry:
  ret void
}

define internal ptr @safe_divide(i8 %a, i8 %b) {
entry:
  %a1 = alloca i8, align 1
  store i8 %a, ptr %a1, align 1
  %b2 = alloca i8, align 1
  store i8 %b, ptr %b2, align 1
  %0 = load i64, ptr %b2, align 4
  %eqtmp = icmp eq i64 %0, 0
  br i1 %eqtmp, label %then, label %ifcont

then:                                             ; preds = %entry
  %result = alloca %Result, align 8
  %err_ptr = getelementptr inbounds %Result, ptr %result, i32 0, i32 0
  store ptr @0, ptr %err_ptr, align 8
  %val_ptr = getelementptr inbounds %Result, ptr %result, i32 0, i32 1
  store i64 0, ptr %val_ptr, align 4
  %result_val = load %Result, ptr %result, align 8
  ret %Result %result_val

ifcont:                                           ; preds = %entry
  %1 = load i64, ptr %a1, align 4
  %2 = load i64, ptr %b2, align 4
  %divtmp = sdiv i64 %1, %2
  %result3 = alloca %Result.0, align 8
  %val_ptr4 = getelementptr inbounds %Result.0, ptr %result3, i32 0, i32 1
  store i64 %divtmp, ptr %val_ptr4, align 4
  %result_val5 = load %Result.0, ptr %result3, align 8
  ret %Result.0 %result_val5
}

define internal i8 @test_unwrap_with_default() {
entry:
  %0 = call ptr @get_current_thread_nursery()
  %1 = call ptr @aria_gc_alloc(ptr %0, i64 8)
  %r1 = alloca ptr, align 8
  store ptr %1, ptr %r1, align 8
  %calltmp = call ptr @safe_divide(i64 10, i64 2)
  %2 = load ptr, ptr %r1, align 8
  store ptr %calltmp, ptr %2, align 8
  %3 = call ptr @get_current_thread_nursery.1()
  %4 = call ptr @aria_gc_alloc.2(ptr %3, i64 8)
  %r2 = alloca ptr, align 8
  store ptr %4, ptr %r2, align 8
  %calltmp1 = call ptr @safe_divide(i64 10, i64 0)
  %5 = load ptr, ptr %r2, align 8
  store ptr %calltmp1, ptr %5, align 8
  %6 = call ptr @get_current_thread_nursery.3()
  %7 = call ptr @aria_gc_alloc.4(ptr %6, i64 1)
  %v1 = alloca ptr, align 8
  store ptr %7, ptr %v1, align 8
  %8 = load ptr, ptr %r1, align 8
  %9 = call ptr @get_current_thread_nursery.5()
  %10 = call ptr @aria_gc_alloc.6(ptr %9, i64 1)
  %v2 = alloca ptr, align 8
  store ptr %10, ptr %v2, align 8
  %11 = load ptr, ptr %r2, align 8
  %12 = load i8, ptr %v1, align 1
  %13 = load i8, ptr %v2, align 1
  %addtmp = add i8 %12, %13
  ret i8 %addtmp
}

declare ptr @get_current_thread_nursery()

declare ptr @aria_gc_alloc(ptr, i64)

declare ptr @get_current_thread_nursery.1()

declare ptr @aria_gc_alloc.2(ptr, i64)

declare ptr @get_current_thread_nursery.3()

declare ptr @aria_gc_alloc.4(ptr, i64)

declare ptr @get_current_thread_nursery.5()

declare ptr @aria_gc_alloc.6(ptr, i64)

define internal i8 @__user_main() {
entry:
  %calltmp = call i8 @test_unwrap_with_default()
  ret i8 %calltmp
}

define i64 @main() {
entry:
  call void @__aria_module_init()
  %0 = call i8 @__user_main()
  %1 = sext i8 %0 to i64
  ret i64 %1
}
