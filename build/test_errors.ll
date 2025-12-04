; ModuleID = 'aria_module'
source_filename = "aria_module"

%result_int8 = type { i8, i8 }

declare void @puts(ptr)

declare void @print(ptr)

define internal void @__aria_module_init() {
entry:
  %result4 = alloca i8, align 1
  %result2 = alloca i8, align 1
  %result1 = alloca i8, align 1
  %calltmp = call %result_int8 @safe_divide(i8 10, i8 2)
  %result_temp = alloca %result_int8, align 8
  store %result_int8 %calltmp, ptr %result_temp, align 1
  %err_ptr = getelementptr inbounds %result_int8, ptr %result_temp, i32 0, i32 0
  %err = load i8, ptr %err_ptr, align 1
  %is_success = icmp eq i8 %err, 0
  %val_ptr = getelementptr inbounds %result_int8, ptr %result_temp, i32 0, i32 1
  %val = load i8, ptr %val_ptr, align 1
  %unwrap_result = select i1 %is_success, i8 %val, i8 -1
  store i8 %unwrap_result, ptr %result1, align 1
  %calltmp1 = call %result_int8 @always_error(i8 5)
  %result_temp2 = alloca %result_int8, align 8
  store %result_int8 %calltmp1, ptr %result_temp2, align 1
  %err_ptr3 = getelementptr inbounds %result_int8, ptr %result_temp2, i32 0, i32 0
  %err4 = load i8, ptr %err_ptr3, align 1
  %is_success5 = icmp eq i8 %err4, 0
  %val_ptr6 = getelementptr inbounds %result_int8, ptr %result_temp2, i32 0, i32 1
  %val7 = load i8, ptr %val_ptr6, align 1
  %unwrap_result8 = select i1 %is_success5, i8 %val7, i8 -1
  store i8 %unwrap_result8, ptr %result2, align 1
  %calltmp9 = call %result_int8 @multiply(i8 5, i8 3)
  %result_temp10 = alloca %result_int8, align 8
  store %result_int8 %calltmp9, ptr %result_temp10, align 1
  %err_ptr11 = getelementptr inbounds %result_int8, ptr %result_temp10, i32 0, i32 0
  %err12 = load i8, ptr %err_ptr11, align 1
  %is_success13 = icmp eq i8 %err12, 0
  %val_ptr14 = getelementptr inbounds %result_int8, ptr %result_temp10, i32 0, i32 1
  %val15 = load i8, ptr %val_ptr14, align 1
  %unwrap_result16 = select i1 %is_success13, i8 %val15, i8 -1
  store i8 %unwrap_result16, ptr %result4, align 1
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
  store i8 0, ptr %err_ptr, align 1
  %val_cast = trunc i64 %divtmp to i8
  %val_ptr = getelementptr inbounds %result_int8, ptr %result, i32 0, i32 1
  store i8 %val_cast, ptr %val_ptr, align 1
  %result_val = load %result_int8, ptr %result, align 1
  ret %result_int8 %result_val
}

define internal %result_int8 @always_error(i8 %a) {
entry:
  %a1 = alloca i8, align 1
  store i8 %a, ptr %a1, align 1
  %result = alloca %result_int8, align 8
  %err_ptr = getelementptr inbounds %result_int8, ptr %result, i32 0, i32 0
  store i8 1, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int8, ptr %result, i32 0, i32 1
  store i8 0, ptr %val_ptr, align 1
  %result_val = load %result_int8, ptr %result, align 1
  ret %result_int8 %result_val
}

define internal %result_int8 @multiply(i8 %a, i8 %b) {
entry:
  %a1 = alloca i8, align 1
  store i8 %a, ptr %a1, align 1
  %b2 = alloca i8, align 1
  store i8 %b, ptr %b2, align 1
  %0 = load i64, ptr %a1, align 4
  %1 = load i64, ptr %b2, align 4
  %multmp = mul i64 %0, %1
  %auto_wrap_cast = trunc i64 %multmp to i8
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 %auto_wrap_cast, ptr %val_ptr, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val
}

define i64 @main() {
entry:
  call void @__aria_module_init()
  ret i64 0
}
