; ModuleID = 'aria_module'
source_filename = "aria_module"

declare void @puts(ptr)

declare void @print(ptr)

define internal void @__aria_module_init() {
entry:
  ret void
}

define internal i8 @test_while_basic() {
entry:
  %0 = call ptr @get_current_thread_nursery()
  %1 = call ptr @aria_gc_alloc(ptr %0, i64 1)
  %sum = alloca ptr, align 8
  store ptr %1, ptr %sum, align 8
  %2 = load ptr, ptr %sum, align 8
  store i64 0, ptr %2, align 4
  %3 = call ptr @get_current_thread_nursery.1()
  %4 = call ptr @aria_gc_alloc.2(ptr %3, i64 1)
  %i = alloca ptr, align 8
  store ptr %4, ptr %i, align 8
  %5 = load ptr, ptr %i, align 8
  store i64 0, ptr %5, align 4
  br label %while_cond

while_cond:                                       ; preds = %while_body, %entry
  %6 = load i8, ptr %i, align 1
  %7 = sext i8 %6 to i64
  %lttmp = icmp slt i64 %7, 5
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %8 = load i8, ptr %sum, align 1
  %9 = load i8, ptr %i, align 1
  %addtmp = add i8 %8, %9
  store i8 %addtmp, ptr %sum, align 1
  %10 = load i8, ptr %i, align 1
  %11 = sext i8 %10 to i64
  %addtmp1 = add i64 %11, 1
  store i64 %addtmp1, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %12 = load i8, ptr %sum, align 1
  ret i8 %12
}

declare ptr @get_current_thread_nursery()

declare ptr @aria_gc_alloc(ptr, i64)

declare ptr @get_current_thread_nursery.1()

declare ptr @aria_gc_alloc.2(ptr, i64)

define internal i8 @test_while_break() {
entry:
  %0 = call ptr @get_current_thread_nursery.3()
  %1 = call ptr @aria_gc_alloc.4(ptr %0, i64 1)
  %sum = alloca ptr, align 8
  store ptr %1, ptr %sum, align 8
  %2 = load ptr, ptr %sum, align 8
  store i64 0, ptr %2, align 4
  %3 = call ptr @get_current_thread_nursery.5()
  %4 = call ptr @aria_gc_alloc.6(ptr %3, i64 1)
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
  %8 = load i8, ptr %sum, align 1
  %9 = load i8, ptr %i, align 1
  %addtmp = add i8 %8, %9
  store i8 %addtmp, ptr %sum, align 1
  %10 = load i8, ptr %i, align 1
  %11 = sext i8 %10 to i64
  %addtmp1 = add i64 %11, 1
  store i64 %addtmp1, ptr %i, align 4
  %12 = load i8, ptr %i, align 1
  %13 = sext i8 %12 to i64
  %eqtmp = icmp eq i64 %13, 5
  br i1 %eqtmp, label %then, label %ifcont

then:                                             ; preds = %while_body
  br label %while_exit

ifcont:                                           ; preds = %while_body
  br label %while_cond

while_exit:                                       ; preds = %then, %while_cond
  %14 = load i8, ptr %sum, align 1
  ret i8 %14
}

declare ptr @get_current_thread_nursery.3()

declare ptr @aria_gc_alloc.4(ptr, i64)

declare ptr @get_current_thread_nursery.5()

declare ptr @aria_gc_alloc.6(ptr, i64)

define internal i8 @test_while_continue() {
entry:
  %0 = call ptr @get_current_thread_nursery.7()
  %1 = call ptr @aria_gc_alloc.8(ptr %0, i64 1)
  %sum = alloca ptr, align 8
  store ptr %1, ptr %sum, align 8
  %2 = load ptr, ptr %sum, align 8
  store i64 0, ptr %2, align 4
  %3 = call ptr @get_current_thread_nursery.9()
  %4 = call ptr @aria_gc_alloc.10(ptr %3, i64 1)
  %i = alloca ptr, align 8
  store ptr %4, ptr %i, align 8
  %5 = load ptr, ptr %i, align 8
  store i64 0, ptr %5, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont, %then, %entry
  %6 = load i8, ptr %i, align 1
  %7 = sext i8 %6 to i64
  %lttmp = icmp slt i64 %7, 6
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %8 = load i8, ptr %i, align 1
  %9 = sext i8 %8 to i64
  %addtmp = add i64 %9, 1
  store i64 %addtmp, ptr %i, align 4
  %10 = load i8, ptr %i, align 1
  %11 = sext i8 %10 to i64
  %eqtmp = icmp eq i64 %11, 3
  br i1 %eqtmp, label %then, label %ifcont

then:                                             ; preds = %while_body
  br label %while_cond

ifcont:                                           ; preds = %while_body
  %12 = load i8, ptr %sum, align 1
  %13 = load i8, ptr %i, align 1
  %addtmp1 = add i8 %12, %13
  store i8 %addtmp1, ptr %sum, align 1
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %14 = load i8, ptr %sum, align 1
  ret i8 %14
}

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
  %calltmp = call i8 @test_while_basic()
  %2 = load ptr, ptr %r1, align 8
  store i8 %calltmp, ptr %2, align 1
  %3 = call ptr @get_current_thread_nursery.13()
  %4 = call ptr @aria_gc_alloc.14(ptr %3, i64 1)
  %r2 = alloca ptr, align 8
  store ptr %4, ptr %r2, align 8
  %calltmp1 = call i8 @test_while_break()
  %5 = load ptr, ptr %r2, align 8
  store i8 %calltmp1, ptr %5, align 1
  %6 = call ptr @get_current_thread_nursery.15()
  %7 = call ptr @aria_gc_alloc.16(ptr %6, i64 1)
  %r3 = alloca ptr, align 8
  store ptr %7, ptr %r3, align 8
  %calltmp2 = call i8 @test_while_continue()
  %8 = load ptr, ptr %r3, align 8
  store i8 %calltmp2, ptr %8, align 1
  %9 = load i8, ptr %r1, align 1
  %10 = load i8, ptr %r2, align 1
  %addtmp = add i8 %9, %10
  %11 = load i8, ptr %r3, align 1
  %addtmp3 = add i8 %addtmp, %11
  ret i8 %addtmp3
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
