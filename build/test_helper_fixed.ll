; ModuleID = 'aria_module'
source_filename = "aria_module"

%result_int32 = type { i8, i32 }
%result_int8 = type { i8, i8 }

declare void @puts(ptr)

declare void @print(ptr)

define internal void @__aria_module_init() {
entry:
  ret void
}

define internal %result_int32 @helper(i32 %x) {
entry:
  %value = alloca i32, align 4
  %x1 = alloca i32, align 4
  store i32 %x, ptr %x1, align 4
  %0 = load i32, ptr %x1, align 4
  %1 = sext i32 %0 to i64
  %addtmp = add i64 %1, 1
  %2 = trunc i64 %addtmp to i32
  store i32 %2, ptr %value, align 4
  %3 = load i32, ptr %value, align 4
  %auto_wrap_result = alloca %result_int32, align 8
  %err_ptr = getelementptr inbounds %result_int32, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int32, ptr %auto_wrap_result, i32 0, i32 1
  store i32 %3, ptr %val_ptr, align 4
  %result_val = load %result_int32, ptr %auto_wrap_result, align 4
  ret %result_int32 %result_val
}

define internal %result_int8 @__user_main() {
entry:
  %val = alloca i32, align 4
  %calltmp = call %result_int32 @helper(i32 5)
  store %result_int32 %calltmp, ptr %val, align 4
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 0, ptr %val_ptr, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val
}

define i64 @main() {
entry:
  call void @__aria_module_init()
  %0 = call %result_int8 @__user_main()
  ret i64 0
}
