; ModuleID = 'aria_module'
source_filename = "aria_module"

%result_int64 = type { i8, i64 }

declare void @puts(ptr)

declare void @print(ptr)

define internal void @__aria_module_init() {
entry:
  ret void
}

define internal %result_int64 @test() {
entry:
  %result = alloca %result_int64, align 8
  %err_ptr = getelementptr inbounds %result_int64, ptr %result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int64, ptr %result, i32 0, i32 1
  store i64 42, ptr %val_ptr, align 4
  %result_val = load %result_int64, ptr %result, align 4
  ret %result_int64 %result_val
}

define internal %result_int64 @__user_main() {
entry:
  %x = alloca i64, align 8
  %calltmp = call %result_int64 @test()
  store %result_int64 %calltmp, ptr %x, align 4
  ret i64 0
}

define i64 @main() {
entry:
  call void @__aria_module_init()
  %0 = call %result_int64 @__user_main()
  ret i64 0
}
