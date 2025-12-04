; ModuleID = 'aria_module'
source_filename = "aria_module"

%result_int8 = type { ptr, i8 }

declare void @puts(ptr)

declare void @print(ptr)

define internal void @__aria_module_init() {
entry:
  ret void
}

define internal %result_int8 @safe_divide(i8 %a, i8 %b) {
entry:
  %a1 = alloca i8, align 1
  store i8 %a, ptr %a1, align 1
  %b2 = alloca i8, align 1
  store i8 %b, ptr %b2, align 1
  %0 = load i64, ptr %a1, align 4
  %1 = load i64, ptr %b2, align 4
  %divtmp = sdiv i64 %0, %1
  %result = alloca %result_int8, align 8
  %err_ptr = getelementptr inbounds %result_int8, ptr %result, i32 0, i32 0
  store ptr null, ptr %err_ptr, align 8
  %val_cast = trunc i64 %divtmp to i8
  %val_ptr = getelementptr inbounds %result_int8, ptr %result, i32 0, i32 1
  store i8 %val_cast, ptr %val_ptr, align 1
  %result_val = load %result_int8, ptr %result, align 8
  ret %result_int8 %result_val
}

define internal %result_int8 @__user_main() {
entry:
  %result2 = alloca i8, align 1
  %result1 = alloca i8, align 1
  %calltmp = call %result_int8 @safe_divide(i8 10, i8 2)
  %result_temp = alloca %result_int8, align 8
  store %result_int8 %calltmp, ptr %result_temp, align 8
  %err_ptr = getelementptr inbounds %result_int8, ptr %result_temp, i32 0, i32 0
  %err = load ptr, ptr %err_ptr, align 8
  %is_null = icmp eq ptr %err, null
  %val_ptr = getelementptr inbounds %result_int8, ptr %result_temp, i32 0, i32 1
  %val = load i8, ptr %val_ptr, align 1
  %unwrap_result = select i1 %is_null, i8 %val, i8 -1
  store i8 %unwrap_result, ptr %result1, align 1
  %calltmp1 = call %result_int8 @safe_divide(i8 10, i8 0)
  %result_temp2 = alloca %result_int8, align 8
  store %result_int8 %calltmp1, ptr %result_temp2, align 8
  %err_ptr3 = getelementptr inbounds %result_int8, ptr %result_temp2, i32 0, i32 0
  %err4 = load ptr, ptr %err_ptr3, align 8
  %is_null5 = icmp eq ptr %err4, null
  %val_ptr6 = getelementptr inbounds %result_int8, ptr %result_temp2, i32 0, i32 1
  %val7 = load i8, ptr %val_ptr6, align 1
  %unwrap_result8 = select i1 %is_null5, i8 %val7, i8 -1
  store i8 %unwrap_result8, ptr %result2, align 1
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr9 = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store ptr null, ptr %err_ptr9, align 8
  %val_ptr10 = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 0, ptr %val_ptr10, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 8
  ret %result_int8 %result_val
}

define i64 @main() {
entry:
  call void @__aria_module_init()
  %0 = call %result_int8 @__user_main()
  ret i64 0
}
