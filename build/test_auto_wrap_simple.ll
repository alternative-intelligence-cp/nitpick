; ModuleID = 'aria_module'
source_filename = "aria_module"

%result_int8 = type { ptr, i8 }

declare void @puts(ptr)

declare void @print(ptr)

define internal void @__aria_module_init() {
entry:
  ret void
}

define internal %result_int8 @test() {
entry:
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store ptr null, ptr %err_ptr, align 8
  %val_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 5, ptr %val_ptr, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 8
  ret %result_int8 %result_val
}

define i64 @main() {
entry:
  call void @__aria_module_init()
  ret i64 0
}
