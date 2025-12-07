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

define internal %result_int32 @square_int32(i32 %x) {
entry:
  %val = alloca i32, align 4
  %x1 = alloca i32, align 4
  store i32 %x, ptr %x1, align 4
  %0 = load i64, ptr %x1, align 4
  %1 = load i64, ptr %x1, align 4
  %multmp = mul i64 %0, %1
  store i64 %multmp, ptr %val, align 4
  %2 = load i32, ptr %val, align 4
  %auto_wrap_result = alloca %result_int32, align 8
  %err_ptr = getelementptr inbounds %result_int32, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int32, ptr %auto_wrap_result, i32 0, i32 1
  store i32 %2, ptr %val_ptr, align 4
  %result_val = load %result_int32, ptr %auto_wrap_result, align 4
  ret %result_int32 %result_val
}

define internal %result_int8 @__user_main() {
entry:
  %s = alloca i32, align 4
  %calltmp = call %result_int32 @square_int32(i32 5)
  %result_temp = alloca %result_int32, align 8
  store %result_int32 %calltmp, ptr %result_temp, align 4
  %err_ptr = getelementptr inbounds %result_int32, ptr %result_temp, i32 0, i32 0
  %err = load i8, ptr %err_ptr, align 1
  %is_success = icmp eq i8 %err, 0
  %val_ptr = getelementptr inbounds %result_int32, ptr %result_temp, i32 0, i32 1
  %val = load i32, ptr %val_ptr, align 4
  %unwrap_result = select i1 %is_success, i32 %val, i32 0
  store i32 %unwrap_result, ptr %s, align 4
  %0 = load i32, ptr %s, align 4
  %1 = sext i32 %0 to i64
  %eqtmp = icmp eq i64 %1, 25
  br i1 %eqtmp, label %then, label %ifcont

then:                                             ; preds = %entry
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr1 = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr1, align 1
  %val_ptr2 = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 0, ptr %val_ptr2, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val

ifcont:                                           ; preds = %entry
  %auto_wrap_result3 = alloca %result_int8, align 8
  %err_ptr4 = getelementptr inbounds %result_int8, ptr %auto_wrap_result3, i32 0, i32 0
  store i8 0, ptr %err_ptr4, align 1
  %val_ptr5 = getelementptr inbounds %result_int8, ptr %auto_wrap_result3, i32 0, i32 1
  store i8 1, ptr %val_ptr5, align 1
  %result_val6 = load %result_int8, ptr %auto_wrap_result3, align 1
  ret %result_int8 %result_val6
}

define i64 @main() {
entry:
  call void @__aria_module_init()
  %0 = call %result_int8 @__user_main()
  ret i64 0
}
