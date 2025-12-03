; ModuleID = 'aria_module'
source_filename = "aria_module"

@0 = private unnamed_addr constant [19 x i8] c"Pick test complete\00", align 1

declare void @puts(ptr)

declare void @print(ptr)

define internal void @__aria_module_init() {
entry:
  ret void
}

define internal i8 @test_pick(i8 %n) {
entry:
  %n1 = alloca i8, align 1
  store i8 %n, ptr %n1, align 1
  %0 = load i64, ptr %n1, align 4
  %pick_eq = icmp eq i64 %0, 0
  br i1 %pick_eq, label %case_body_0, label %case_next_0

case_body_0:                                      ; preds = %entry
  ret i64 10

case_next_0:                                      ; preds = %entry
  %pick_eq2 = icmp eq i64 %0, 1
  br i1 %pick_eq2, label %case_body_1, label %case_next_1

case_body_1:                                      ; preds = %case_next_0
  ret i64 20

case_next_1:                                      ; preds = %case_next_0
  br i1 true, label %case_body_2, label %case_next_2

case_body_2:                                      ; preds = %case_next_1
  ret i64 -1

case_next_2:                                      ; preds = %case_next_1
  br label %pick_done

pick_done:                                        ; preds = %case_next_2
  ret i64 0
}

define internal i8 @__user_main() {
entry:
  %0 = call ptr @get_current_thread_nursery()
  %1 = call ptr @aria_gc_alloc(ptr %0, i64 1)
  %r = alloca ptr, align 8
  store ptr %1, ptr %r, align 8
  %calltmp = call i8 @test_pick(i64 1)
  %2 = load ptr, ptr %r, align 8
  store i8 %calltmp, ptr %2, align 1
  call void @puts(ptr @0)
  ret i64 0
}

declare ptr @get_current_thread_nursery()

declare ptr @aria_gc_alloc(ptr, i64)

define i64 @main() {
entry:
  call void @__aria_module_init()
  %0 = call i8 @__user_main()
  %1 = sext i8 %0 to i64
  ret i64 %1
}
