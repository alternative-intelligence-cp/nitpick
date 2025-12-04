; ModuleID = 'aria_module'
source_filename = "aria_module"

%result_int8 = type { ptr, i8 }
%result_int32 = type { ptr, i32 }
%result_int64 = type { ptr, i64 }

declare void @puts(ptr)

declare void @print(ptr)

define internal void @__aria_module_init() {
entry:
  ret void
}

define internal %result_int8 @add8(i8 %a, i8 %b) {
entry:
  %a1 = alloca i8, align 1
  store i8 %a, ptr %a1, align 1
  %b2 = alloca i8, align 1
  store i8 %b, ptr %b2, align 1
  %0 = load i64, ptr %a1, align 4
  %1 = load i64, ptr %b2, align 4
  %addtmp = add i64 %0, %1
  %auto_wrap_cast = trunc i64 %addtmp to i8
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store ptr null, ptr %err_ptr, align 8
  %val_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 %auto_wrap_cast, ptr %val_ptr, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 8
  ret %result_int8 %result_val
}

define internal %result_int32 @add32(i32 %a, i32 %b) {
entry:
  %a1 = alloca i32, align 4
  store i32 %a, ptr %a1, align 4
  %b2 = alloca i32, align 4
  store i32 %b, ptr %b2, align 4
  %0 = load i64, ptr %a1, align 4
  %1 = load i64, ptr %b2, align 4
  %addtmp = add i64 %0, %1
  %auto_wrap_cast = trunc i64 %addtmp to i32
  %auto_wrap_result = alloca %result_int32, align 8
  %err_ptr = getelementptr inbounds %result_int32, ptr %auto_wrap_result, i32 0, i32 0
  store ptr null, ptr %err_ptr, align 8
  %val_ptr = getelementptr inbounds %result_int32, ptr %auto_wrap_result, i32 0, i32 1
  store i32 %auto_wrap_cast, ptr %val_ptr, align 4
  %result_val = load %result_int32, ptr %auto_wrap_result, align 8
  ret %result_int32 %result_val
}

define internal %result_int64 @add64(i64 %a, i64 %b) {
entry:
  %a1 = alloca i64, align 8
  store i64 %a, ptr %a1, align 4
  %b2 = alloca i64, align 8
  store i64 %b, ptr %b2, align 4
  %0 = load i64, ptr %a1, align 4
  %1 = load i64, ptr %b2, align 4
  %addtmp = add i64 %0, %1
  %auto_wrap_result = alloca %result_int64, align 8
  %err_ptr = getelementptr inbounds %result_int64, ptr %auto_wrap_result, i32 0, i32 0
  store ptr null, ptr %err_ptr, align 8
  %val_ptr = getelementptr inbounds %result_int64, ptr %auto_wrap_result, i32 0, i32 1
  store i64 %addtmp, ptr %val_ptr, align 4
  %result_val = load %result_int64, ptr %auto_wrap_result, align 8
  ret %result_int64 %result_val
}

define internal %result_int8 @__user_main() {
entry:
  %r3 = alloca i64, align 8
  %r2 = alloca i32, align 4
  %r1 = alloca i8, align 1
  %calltmp = call %result_int8 @add8(i8 10, i8 20)
  %result_temp = alloca %result_int8, align 8
  store %result_int8 %calltmp, ptr %result_temp, align 8
  %err_ptr = getelementptr inbounds %result_int8, ptr %result_temp, i32 0, i32 0
  %err = load ptr, ptr %err_ptr, align 8
  %is_null = icmp eq ptr %err, null
  %val_ptr = getelementptr inbounds %result_int8, ptr %result_temp, i32 0, i32 1
  %val = load i8, ptr %val_ptr, align 1
  %unwrap_result = select i1 %is_null, i8 %val, i8 0
  store i8 %unwrap_result, ptr %r1, align 1
  %calltmp1 = call %result_int32 @add32(i32 100, i32 200)
  %result_temp2 = alloca %result_int32, align 8
  store %result_int32 %calltmp1, ptr %result_temp2, align 8
  %err_ptr3 = getelementptr inbounds %result_int32, ptr %result_temp2, i32 0, i32 0
  %err4 = load ptr, ptr %err_ptr3, align 8
  %is_null5 = icmp eq ptr %err4, null
  %val_ptr6 = getelementptr inbounds %result_int32, ptr %result_temp2, i32 0, i32 1
  %val7 = load i32, ptr %val_ptr6, align 4
  %unwrap_result8 = select i1 %is_null5, i32 %val7, i32 0
  store i32 %unwrap_result8, ptr %r2, align 4
  %calltmp9 = call %result_int64 @add64(i64 1000, i64 2000)
  %result_temp10 = alloca %result_int64, align 8
  store %result_int64 %calltmp9, ptr %result_temp10, align 8
  %err_ptr11 = getelementptr inbounds %result_int64, ptr %result_temp10, i32 0, i32 0
  %err12 = load ptr, ptr %err_ptr11, align 8
  %is_null13 = icmp eq ptr %err12, null
  %val_ptr14 = getelementptr inbounds %result_int64, ptr %result_temp10, i32 0, i32 1
  %val15 = load i64, ptr %val_ptr14, align 4
  %unwrap_result16 = select i1 %is_null13, i64 %val15, i64 0
  store i64 %unwrap_result16, ptr %r3, align 4
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr17 = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store ptr null, ptr %err_ptr17, align 8
  %val_ptr18 = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 0, ptr %val_ptr18, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 8
  ret %result_int8 %result_val
}

define i64 @main() {
entry:
  call void @__aria_module_init()
  %0 = call %result_int8 @__user_main()
  ret i64 0
}
