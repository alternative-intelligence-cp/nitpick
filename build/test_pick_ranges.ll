; ModuleID = 'aria_module'
source_filename = "aria_module"

declare void @puts(ptr)

declare void @print(ptr)

define internal void @__aria_module_init() {
entry:
  ret void
}

define internal i8 @classify_number(i8 %x) {
entry:
  %x1 = alloca i8, align 1
  store i8 %x, ptr %x1, align 1
  %0 = load i64, ptr %x1, align 4
  %range_ge = icmp sge i64 %0, 0
  %range_le = icmp sle i64 %0, 5
  %range_match = and i1 %range_ge, %range_le
  br i1 %range_match, label %case_body_0, label %case_next_0

case_body_0:                                      ; preds = %entry
  ret i64 1

case_next_0:                                      ; preds = %entry
  %range_ge2 = icmp sge i64 %0, 6
  %range_lt = icmp slt i64 %0, 10
  %range_match3 = and i1 %range_ge2, %range_lt
  br i1 %range_match3, label %case_body_1, label %case_next_1

case_body_1:                                      ; preds = %case_next_0
  ret i64 2

case_next_1:                                      ; preds = %case_next_0
  %pick_ge = icmp sge i64 %0, 10
  br i1 %pick_ge, label %case_body_2, label %case_next_2

case_body_2:                                      ; preds = %case_next_1
  ret i64 3

case_next_2:                                      ; preds = %case_next_1
  %pick_lt = icmp slt i64 %0, 0
  br i1 %pick_lt, label %case_body_3, label %case_next_3

case_body_3:                                      ; preds = %case_next_2
  ret i64 -1

case_next_3:                                      ; preds = %case_next_2
  br label %pick_done

pick_done:                                        ; preds = %case_next_3
  ret i64 0
}

define internal i8 @test_ranges() {
entry:
  %0 = call ptr @get_current_thread_nursery()
  %1 = call ptr @aria_gc_alloc(ptr %0, i64 1)
  %r1 = alloca ptr, align 8
  store ptr %1, ptr %r1, align 8
  %calltmp = call i8 @classify_number(i64 3)
  %2 = load ptr, ptr %r1, align 8
  store i8 %calltmp, ptr %2, align 1
  %3 = call ptr @get_current_thread_nursery.1()
  %4 = call ptr @aria_gc_alloc.2(ptr %3, i64 1)
  %r2 = alloca ptr, align 8
  store ptr %4, ptr %r2, align 8
  %calltmp1 = call i8 @classify_number(i64 7)
  %5 = load ptr, ptr %r2, align 8
  store i8 %calltmp1, ptr %5, align 1
  %6 = call ptr @get_current_thread_nursery.3()
  %7 = call ptr @aria_gc_alloc.4(ptr %6, i64 1)
  %r3 = alloca ptr, align 8
  store ptr %7, ptr %r3, align 8
  %calltmp2 = call i8 @classify_number(i64 15)
  %8 = load ptr, ptr %r3, align 8
  store i8 %calltmp2, ptr %8, align 1
  %9 = call ptr @get_current_thread_nursery.5()
  %10 = call ptr @aria_gc_alloc.6(ptr %9, i64 1)
  %r4 = alloca ptr, align 8
  store ptr %10, ptr %r4, align 8
  %calltmp3 = call i8 @classify_number(i64 -5)
  %11 = load ptr, ptr %r4, align 8
  store i8 %calltmp3, ptr %11, align 1
  %12 = load i8, ptr %r1, align 1
  %13 = load i8, ptr %r2, align 1
  %addtmp = add i8 %12, %13
  %14 = load i8, ptr %r3, align 1
  %addtmp4 = add i8 %addtmp, %14
  %15 = load i8, ptr %r4, align 1
  %addtmp5 = add i8 %addtmp4, %15
  ret i8 %addtmp5
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
  %calltmp = call i8 @test_ranges()
  ret i8 %calltmp
}

define i64 @main() {
entry:
  call void @__aria_module_init()
  %0 = call i8 @__user_main()
  %1 = sext i8 %0 to i64
  ret i64 %1
}
