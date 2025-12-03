; ModuleID = 'aria_module'
source_filename = "aria_module"

declare void @puts(ptr)

declare void @print(ptr)

define internal void @__aria_module_init() {
entry:
  ret void
}

define internal i64 @test_local() {
entry:
  %0 = call ptr @get_current_thread_nursery()
  %1 = call ptr @aria_gc_alloc(ptr %0, i64 8)
  %x = alloca ptr, align 8
  store ptr %1, ptr %x, align 8
  %2 = load ptr, ptr %x, align 8
  store i64 42, ptr %2, align 4
  %3 = load i64, ptr %x, align 4
  ret i64 %3
}

declare ptr @get_current_thread_nursery()

declare ptr @aria_gc_alloc(ptr, i64)

define i64 @main() {
entry:
  call void @__aria_module_init()
  ret i64 0
}
