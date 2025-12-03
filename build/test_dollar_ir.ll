; ModuleID = 'aria_module'
source_filename = "aria_module"

declare void @puts(ptr)

declare void @print(ptr)

define internal void @__aria_module_init() {
entry:
  ret void
}

define internal i8 @test_till_basic() {
entry:
  %0 = call ptr @get_current_thread_nursery()
  %1 = call ptr @aria_gc_alloc(ptr %0, i64 1)
  %sum = alloca ptr, align 8
  store ptr %1, ptr %sum, align 8
  %2 = load ptr, ptr %sum, align 8
  store i64 0, ptr %2, align 4
  br label %loop_body

loop_body:                                        ; preds = %loop_body, %entry
  %"$" = phi i64 [ 0, %entry ], [ %next_val, %loop_body ]
  %3 = load i8, ptr %sum, align 1
  %4 = sext i8 %3 to i64
  %addtmp = add i64 %4, %"$"
  store i64 %addtmp, ptr %sum, align 4
  %next_val = add i64 %"$", 1
  %cond_pos = icmp slt i64 %next_val, 5
  %cond_neg = icmp sge i64 %next_val, 0
  %loop_cond = select i1 false, i1 %cond_neg, i1 %cond_pos
  br i1 %loop_cond, label %loop_body, label %loop_exit

loop_exit:                                        ; preds = %loop_body
  %5 = load i8, ptr %sum, align 1
  ret i8 %5
}

declare ptr @get_current_thread_nursery()

declare ptr @aria_gc_alloc(ptr, i64)

define internal i8 @test_till_negative_step() {
entry:
  %0 = call ptr @get_current_thread_nursery.1()
  %1 = call ptr @aria_gc_alloc.2(ptr %0, i64 1)
  %sum = alloca ptr, align 8
  store ptr %1, ptr %sum, align 8
  %2 = load ptr, ptr %sum, align 8
  store i64 0, ptr %2, align 4
  br label %loop_body

loop_body:                                        ; preds = %loop_body, %entry
  %"$" = phi i64 [ 5, %entry ], [ %next_val, %loop_body ]
  %3 = load i8, ptr %sum, align 1
  %4 = sext i8 %3 to i64
  %addtmp = add i64 %4, %"$"
  store i64 %addtmp, ptr %sum, align 4
  %next_val = add i64 %"$", -1
  %cond_pos = icmp slt i64 %next_val, 5
  %cond_neg = icmp sge i64 %next_val, 0
  %loop_cond = select i1 true, i1 %cond_neg, i1 %cond_pos
  br i1 %loop_cond, label %loop_body, label %loop_exit

loop_exit:                                        ; preds = %loop_body
  %5 = load i8, ptr %sum, align 1
  ret i8 %5
}

declare ptr @get_current_thread_nursery.1()

declare ptr @aria_gc_alloc.2(ptr, i64)

define internal i8 @test_till_larger_step() {
entry:
  %0 = call ptr @get_current_thread_nursery.3()
  %1 = call ptr @aria_gc_alloc.4(ptr %0, i64 1)
  %sum = alloca ptr, align 8
  store ptr %1, ptr %sum, align 8
  %2 = load ptr, ptr %sum, align 8
  store i64 0, ptr %2, align 4
  br label %loop_body

loop_body:                                        ; preds = %loop_body, %entry
  %"$" = phi i64 [ 0, %entry ], [ %next_val, %loop_body ]
  %3 = load i8, ptr %sum, align 1
  %4 = sext i8 %3 to i64
  %addtmp = add i64 %4, %"$"
  store i64 %addtmp, ptr %sum, align 4
  %next_val = add i64 %"$", 2
  %cond_pos = icmp slt i64 %next_val, 10
  %cond_neg = icmp sge i64 %next_val, 0
  %loop_cond = select i1 false, i1 %cond_neg, i1 %cond_pos
  br i1 %loop_cond, label %loop_body, label %loop_exit

loop_exit:                                        ; preds = %loop_body
  %5 = load i8, ptr %sum, align 1
  ret i8 %5
}

declare ptr @get_current_thread_nursery.3()

declare ptr @aria_gc_alloc.4(ptr, i64)

define internal i8 @__user_main() {
entry:
  %0 = call ptr @get_current_thread_nursery.5()
  %1 = call ptr @aria_gc_alloc.6(ptr %0, i64 1)
  %r1 = alloca ptr, align 8
  store ptr %1, ptr %r1, align 8
  %calltmp = call i8 @test_till_basic()
  %2 = load ptr, ptr %r1, align 8
  store i8 %calltmp, ptr %2, align 1
  %3 = call ptr @get_current_thread_nursery.7()
  %4 = call ptr @aria_gc_alloc.8(ptr %3, i64 1)
  %r2 = alloca ptr, align 8
  store ptr %4, ptr %r2, align 8
  %calltmp1 = call i8 @test_till_negative_step()
  %5 = load ptr, ptr %r2, align 8
  store i8 %calltmp1, ptr %5, align 1
  %6 = call ptr @get_current_thread_nursery.9()
  %7 = call ptr @aria_gc_alloc.10(ptr %6, i64 1)
  %r3 = alloca ptr, align 8
  store ptr %7, ptr %r3, align 8
  %calltmp2 = call i8 @test_till_larger_step()
  %8 = load ptr, ptr %r3, align 8
  store i8 %calltmp2, ptr %8, align 1
  ret i64 0
}

declare ptr @get_current_thread_nursery.5()

declare ptr @aria_gc_alloc.6(ptr, i64)

declare ptr @get_current_thread_nursery.7()

declare ptr @aria_gc_alloc.8(ptr, i64)

declare ptr @get_current_thread_nursery.9()

declare ptr @aria_gc_alloc.10(ptr, i64)

define i64 @main() {
entry:
  call void @__aria_module_init()
  %0 = call i8 @__user_main()
  %1 = sext i8 %0 to i64
  ret i64 %1
}
