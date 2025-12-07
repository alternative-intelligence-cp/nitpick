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

define internal %result_int32 @pow_int32(i32 %base, i32 %exponent) {
entry:
  %i = alloca i32, align 4
  %result_val3 = alloca i32, align 4
  %base1 = alloca i32, align 4
  store i32 %base, ptr %base1, align 4
  %exponent2 = alloca i32, align 4
  store i32 %exponent, ptr %exponent2, align 4
  %0 = load i64, ptr %exponent2, align 4
  %eqtmp = icmp eq i64 %0, 0
  br i1 %eqtmp, label %then, label %ifcont

then:                                             ; preds = %entry
  %auto_wrap_result = alloca %result_int32, align 8
  %err_ptr = getelementptr inbounds %result_int32, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int32, ptr %auto_wrap_result, i32 0, i32 1
  store i32 1, ptr %val_ptr, align 4
  %result_val = load %result_int32, ptr %auto_wrap_result, align 4
  ret %result_int32 %result_val

ifcont:                                           ; preds = %entry
  store i64 1, ptr %result_val3, align 4
  store i64 0, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %while_body, %ifcont
  %1 = load i32, ptr %i, align 4
  %2 = load i64, ptr %exponent2, align 4
  %3 = sext i32 %1 to i64
  %lttmp = icmp slt i64 %3, %2
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %4 = load i32, ptr %result_val3, align 4
  %5 = load i64, ptr %base1, align 4
  %6 = sext i32 %4 to i64
  %multmp = mul i64 %6, %5
  store i64 %multmp, ptr %result_val3, align 4
  %7 = load i32, ptr %i, align 4
  %8 = sext i32 %7 to i64
  %addtmp = add i64 %8, 1
  store i64 %addtmp, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %9 = load i32, ptr %result_val3, align 4
  %auto_wrap_result4 = alloca %result_int32, align 8
  %err_ptr5 = getelementptr inbounds %result_int32, ptr %auto_wrap_result4, i32 0, i32 0
  store i8 0, ptr %err_ptr5, align 1
  %val_ptr6 = getelementptr inbounds %result_int32, ptr %auto_wrap_result4, i32 0, i32 1
  store i32 %9, ptr %val_ptr6, align 4
  %result_val7 = load %result_int32, ptr %auto_wrap_result4, align 4
  ret %result_int32 %result_val7
}

define internal %result_int8 @__user_main() {
entry:
  %r = alloca i32, align 4
  %calltmp = call %result_int32 @pow_int32(i32 2, i32 3)
  %result_temp = alloca %result_int32, align 8
  store %result_int32 %calltmp, ptr %result_temp, align 4
  %err_ptr = getelementptr inbounds %result_int32, ptr %result_temp, i32 0, i32 0
  %err = load i8, ptr %err_ptr, align 1
  %is_success = icmp eq i8 %err, 0
  %val_ptr = getelementptr inbounds %result_int32, ptr %result_temp, i32 0, i32 1
  %val = load i32, ptr %val_ptr, align 4
  %unwrap_result = select i1 %is_success, i32 %val, i32 0
  store i32 %unwrap_result, ptr %r, align 4
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr1 = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr1, align 1
  %val_ptr2 = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 0, ptr %val_ptr2, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val
}

define i64 @main() {
entry:
  call void @__aria_module_init()
  %0 = call %result_int8 @__user_main()
  ret i64 0
}
