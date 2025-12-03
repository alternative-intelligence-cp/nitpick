; ModuleID = 'aria_module'
source_filename = "aria_module"

@0 = private unnamed_addr constant [20 x i8] c"Loop tests complete\00", align 1

declare void @puts(ptr)

declare void @print(ptr)

define internal void @__aria_module_init() {
entry:
  ret void
}

define internal i8 @test_while() {
entry:
  %0 = call ptr @get_current_thread_nursery()
  %1 = call ptr @aria_gc_alloc(ptr %0, i64 1)
  %count = alloca ptr, align 8
  store ptr %1, ptr %count, align 8
  %2 = load ptr, ptr %count, align 8
  store i64 0, ptr %2, align 4
  br label %while_cond

while_cond:                                       ; preds = %while_body, %entry
  %3 = load i8, ptr %count, align 1
  %4 = sext i8 %3 to i64
  %lttmp = icmp slt i64 %4, 5
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %5 = load i8, ptr %count, align 1
  %6 = load i8, ptr %count, align 1
  %7 = sext i8 %6 to i64
  %addtmp = add i64 %7, 1
  %8 = sext i8 %5 to i64
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %9 = load i8, ptr %count, align 1
  ret i8 %9
}

declare ptr @get_current_thread_nursery()

declare ptr @aria_gc_alloc(ptr, i64)

define internal i8 @test_while_nested() {
entry:
  %0 = call ptr @get_current_thread_nursery.1()
  %1 = call ptr @aria_gc_alloc.2(ptr %0, i64 1)
  %sum = alloca ptr, align 8
  store ptr %1, ptr %sum, align 8
  %2 = load ptr, ptr %sum, align 8
  store i64 0, ptr %2, align 4
  %3 = call ptr @get_current_thread_nursery.3()
  %4 = call ptr @aria_gc_alloc.4(ptr %3, i64 1)
  %i = alloca ptr, align 8
  store ptr %4, ptr %i, align 8
  %5 = load ptr, ptr %i, align 8
  store i64 0, ptr %5, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont, %entry
  %6 = load i8, ptr %i, align 1
  %7 = sext i8 %6 to i64
  %lttmp = icmp slt i64 %7, 10
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %8 = load i8, ptr %i, align 1
  %9 = load i8, ptr %i, align 1
  %10 = sext i8 %9 to i64
  %addtmp = add i64 %10, 1
  %11 = sext i8 %8 to i64
  %12 = load i8, ptr %i, align 1
  %13 = sext i8 %12 to i64
  %lttmp1 = icmp slt i64 %13, 5
  br i1 %lttmp1, label %then, label %ifcont

then:                                             ; preds = %while_body
  %14 = load i8, ptr %sum, align 1
  %15 = load i8, ptr %sum, align 1
  %16 = load i8, ptr %i, align 1
  %addtmp2 = add i8 %15, %16
  br label %ifcont

ifcont:                                           ; preds = %then, %while_body
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %17 = load i8, ptr %sum, align 1
  ret i8 %17
}

declare ptr @get_current_thread_nursery.1()

declare ptr @aria_gc_alloc.2(ptr, i64)

declare ptr @get_current_thread_nursery.3()

declare ptr @aria_gc_alloc.4(ptr, i64)

define internal i8 @test_nested() {
entry:
  %0 = call ptr @get_current_thread_nursery.5()
  %1 = call ptr @aria_gc_alloc.6(ptr %0, i64 1)
  %total = alloca ptr, align 8
  store ptr %1, ptr %total, align 8
  %2 = load ptr, ptr %total, align 8
  store i64 0, ptr %2, align 4
  %3 = call ptr @get_current_thread_nursery.7()
  %4 = call ptr @aria_gc_alloc.8(ptr %3, i64 1)
  %outer = alloca ptr, align 8
  store ptr %4, ptr %outer, align 8
  %5 = load ptr, ptr %outer, align 8
  store i64 0, ptr %5, align 4
  br label %while_cond

while_cond:                                       ; preds = %while_exit, %entry
  %6 = load i8, ptr %outer, align 1
  %7 = sext i8 %6 to i64
  %lttmp = icmp slt i64 %7, 3
  br i1 %lttmp, label %while_body, label %while_exit6

while_body:                                       ; preds = %while_cond
  %8 = call ptr @get_current_thread_nursery.9()
  %9 = call ptr @aria_gc_alloc.10(ptr %8, i64 1)
  %inner = alloca ptr, align 8
  store ptr %9, ptr %inner, align 8
  %10 = load ptr, ptr %inner, align 8
  store i64 0, ptr %10, align 4
  br label %while_cond1

while_cond1:                                      ; preds = %while_body2, %while_body
  %11 = load i8, ptr %inner, align 1
  %12 = sext i8 %11 to i64
  %lttmp3 = icmp slt i64 %12, 3
  br i1 %lttmp3, label %while_body2, label %while_exit

while_body2:                                      ; preds = %while_cond1
  %13 = load i8, ptr %total, align 1
  %14 = load i8, ptr %total, align 1
  %15 = sext i8 %14 to i64
  %addtmp = add i64 %15, 1
  %16 = sext i8 %13 to i64
  %17 = load i8, ptr %inner, align 1
  %18 = load i8, ptr %inner, align 1
  %19 = sext i8 %18 to i64
  %addtmp4 = add i64 %19, 1
  %20 = sext i8 %17 to i64
  br label %while_cond1

while_exit:                                       ; preds = %while_cond1
  %21 = load i8, ptr %outer, align 1
  %22 = load i8, ptr %outer, align 1
  %23 = sext i8 %22 to i64
  %addtmp5 = add i64 %23, 1
  %24 = sext i8 %21 to i64
  br label %while_cond

while_exit6:                                      ; preds = %while_cond
  %25 = load i8, ptr %total, align 1
  ret i8 %25
}

declare ptr @get_current_thread_nursery.5()

declare ptr @aria_gc_alloc.6(ptr, i64)

declare ptr @get_current_thread_nursery.7()

declare ptr @aria_gc_alloc.8(ptr, i64)

declare ptr @get_current_thread_nursery.9()

declare ptr @aria_gc_alloc.10(ptr, i64)

define internal i8 @__user_main() {
entry:
  %0 = call ptr @get_current_thread_nursery.11()
  %1 = call ptr @aria_gc_alloc.12(ptr %0, i64 1)
  %r1 = alloca ptr, align 8
  store ptr %1, ptr %r1, align 8
  %calltmp = call i8 @test_while()
  %2 = load ptr, ptr %r1, align 8
  store i8 %calltmp, ptr %2, align 1
  %3 = call ptr @get_current_thread_nursery.13()
  %4 = call ptr @aria_gc_alloc.14(ptr %3, i64 1)
  %r2 = alloca ptr, align 8
  store ptr %4, ptr %r2, align 8
  %calltmp1 = call i8 @test_while_nested()
  %5 = load ptr, ptr %r2, align 8
  store i8 %calltmp1, ptr %5, align 1
  %6 = call ptr @get_current_thread_nursery.15()
  %7 = call ptr @aria_gc_alloc.16(ptr %6, i64 1)
  %r3 = alloca ptr, align 8
  store ptr %7, ptr %r3, align 8
  %calltmp2 = call i8 @test_nested()
  %8 = load ptr, ptr %r3, align 8
  store i8 %calltmp2, ptr %8, align 1
  call void @puts(ptr @0)
  ret i64 0
}

declare ptr @get_current_thread_nursery.11()

declare ptr @aria_gc_alloc.12(ptr, i64)

declare ptr @get_current_thread_nursery.13()

declare ptr @aria_gc_alloc.14(ptr, i64)

declare ptr @get_current_thread_nursery.15()

declare ptr @aria_gc_alloc.16(ptr, i64)

define i64 @main() {
entry:
  call void @__aria_module_init()
  %0 = call i8 @__user_main()
  %1 = sext i8 %0 to i64
  ret i64 %1
}
