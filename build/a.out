; ModuleID = 'aria_module'
source_filename = "aria_module"

%result_int8 = type { i8, i8 }

declare void @puts(ptr)

declare void @print(ptr)

define internal void @__aria_module_init() {
entry:
  ret void
}

define internal %result_int8 @__user_main() {
entry:
  %0 = call ptr @aria_alloc_exec(i64 8)
  %buf = alloca ptr, align 8
  store ptr %0, ptr %buf, align 8
  %1 = load ptr, ptr %buf, align 8
  store i64 42, ptr %1, align 4
  ret %result_int8 zeroinitializer
}

declare ptr @aria_alloc_exec(i64)

define i64 @main() {
entry:
  call void @__aria_module_init()
  %0 = call %result_int8 @__user_main()
  ret i64 0
}
