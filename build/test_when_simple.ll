; ModuleID = 'aria_module'
source_filename = "aria_module"

declare void @puts(ptr)

declare void @print(ptr)

define internal void @__aria_module_init() {
entry:
  ret void
}

define internal i8 @test1() {
entry:
  %0 = call ptr @get_current_thread_nursery()
  %1 = call ptr @aria_gc_alloc(ptr %0, i64 1)
  %x = alloca ptr, align 8
  store ptr %1, ptr %x, align 8
  %2 = load ptr, ptr %x, align 8
  store i64 0, ptr %2, align 4
  br label %when_cond

when_cond:                                        ; preds = %when_body, %entry
  %3 = load i8, ptr %x, align 1
  %4 = sext i8 %3 to i64
  %eqtmp = icmp eq i64 %4, 0
  br i1 %eqtmp, label %when_body, label %when_then

when_body:                                        ; preds = %when_cond
  %5 = load i8, ptr %x, align 1
  %6 = sext i8 %5 to i64
  br label %when_cond

when_then:                                        ; preds = %when_cond
  ret i64 10

when_end:                                         ; No predecessors!
  ret i64 20

when_exit:                                        ; No predecessors!
  ret i64 30
}

declare ptr @get_current_thread_nursery()

declare ptr @aria_gc_alloc(ptr, i64)

define internal i8 @__user_main() {
entry:
  %calltmp = call i8 @test1()
  ret i8 %calltmp
}

define i64 @main() {
entry:
  call void @__aria_module_init()
  %0 = call i8 @__user_main()
  %1 = sext i8 %0 to i64
  ret i64 %1
}
