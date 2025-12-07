; ModuleID = 'aria_module'
source_filename = "aria_module"

%result_int8 = type { i8, i8 }

declare void @puts(ptr)

declare void @print(ptr)

define internal void @__aria_module_init() {
entry:
  ret void
}

define internal %result_int8 @abs_int8(i8 %value) {
entry:
  %value1 = alloca i8, align 1
  store i8 %value, ptr %value1, align 1
  %0 = load i8, ptr %value1, align 1
  %1 = sext i8 %0 to i64
  %lttmp = icmp slt i64 %1, 0
  br i1 %lttmp, label %then, label %ifcont

then:                                             ; preds = %entry
  %2 = load i8, ptr %value1, align 1
  %3 = sext i8 %2 to i64
  %subtmp = sub i64 0, %3
  %auto_wrap_cast = trunc i64 %subtmp to i8
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 %auto_wrap_cast, ptr %val_ptr, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val

ifcont:                                           ; preds = %entry
  %4 = load i8, ptr %value1, align 1
  %auto_wrap_result2 = alloca %result_int8, align 8
  %err_ptr3 = getelementptr inbounds %result_int8, ptr %auto_wrap_result2, i32 0, i32 0
  store i8 0, ptr %err_ptr3, align 1
  %val_ptr4 = getelementptr inbounds %result_int8, ptr %auto_wrap_result2, i32 0, i32 1
  store i8 %4, ptr %val_ptr4, align 1
  %result_val5 = load %result_int8, ptr %auto_wrap_result2, align 1
  ret %result_int8 %result_val5
}

define internal %result_int8 @min_int8(i8 %a, i8 %b) {
entry:
  %a1 = alloca i8, align 1
  store i8 %a, ptr %a1, align 1
  %b2 = alloca i8, align 1
  store i8 %b, ptr %b2, align 1
  %0 = load i8, ptr %a1, align 1
  %1 = load i8, ptr %b2, align 1
  %lttmp = icmp slt i8 %0, %1
  br i1 %lttmp, label %then, label %ifcont

then:                                             ; preds = %entry
  %2 = load i8, ptr %a1, align 1
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 %2, ptr %val_ptr, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val

ifcont:                                           ; preds = %entry
  %3 = load i8, ptr %b2, align 1
  %auto_wrap_result3 = alloca %result_int8, align 8
  %err_ptr4 = getelementptr inbounds %result_int8, ptr %auto_wrap_result3, i32 0, i32 0
  store i8 0, ptr %err_ptr4, align 1
  %val_ptr5 = getelementptr inbounds %result_int8, ptr %auto_wrap_result3, i32 0, i32 1
  store i8 %3, ptr %val_ptr5, align 1
  %result_val6 = load %result_int8, ptr %auto_wrap_result3, align 1
  ret %result_int8 %result_val6
}

define internal %result_int8 @__user_main() {
entry:
  %y = alloca i8, align 1
  %x = alloca i8, align 1
  %calltmp = call %result_int8 @abs_int8(i8 -5)
  store %result_int8 %calltmp, ptr %x, align 1
  %calltmp1 = call %result_int8 @min_int8(i8 3, i8 7)
  store %result_int8 %calltmp1, ptr %y, align 1
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
