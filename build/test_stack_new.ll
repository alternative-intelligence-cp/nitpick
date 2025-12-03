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
  %x = alloca i64, align 8
  store i64 42, ptr %x, align 4
  %0 = load i64, ptr %x, align 4
  ret i64 %0
}

define i64 @main() {
entry:
  call void @__aria_module_init()
  ret i64 0
}
