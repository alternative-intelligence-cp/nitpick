; ModuleID = 'aria_module'
source_filename = "aria_module"

%result_int8 = type { i8, i8 }

declare void @puts(ptr)

declare void @print(ptr)

define internal void @__aria_module_init() {
entry:
  ret void
}

define internal %result_int8 @is_uppercase(i8 %ch) {
entry:
  %ch1 = alloca i8, align 1
  store i8 %ch, ptr %ch1, align 1
  %0 = load i8, ptr %ch1, align 1
  %1 = sext i8 %0 to i64
  %getmp = icmp sge i64 %1, 65
  br i1 %getmp, label %then, label %ifcont3

then:                                             ; preds = %entry
  %2 = load i8, ptr %ch1, align 1
  %3 = sext i8 %2 to i64
  %letmp = icmp sle i64 %3, 90
  br i1 %letmp, label %then2, label %ifcont

then2:                                            ; preds = %then
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 1, ptr %val_ptr, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val

ifcont:                                           ; preds = %then
  br label %ifcont3

ifcont3:                                          ; preds = %ifcont, %entry
  %auto_wrap_result4 = alloca %result_int8, align 8
  %err_ptr5 = getelementptr inbounds %result_int8, ptr %auto_wrap_result4, i32 0, i32 0
  store i8 0, ptr %err_ptr5, align 1
  %val_ptr6 = getelementptr inbounds %result_int8, ptr %auto_wrap_result4, i32 0, i32 1
  store i8 0, ptr %val_ptr6, align 1
  %result_val7 = load %result_int8, ptr %auto_wrap_result4, align 1
  ret %result_int8 %result_val7
}

define internal %result_int8 @is_lowercase(i8 %ch) {
entry:
  %ch1 = alloca i8, align 1
  store i8 %ch, ptr %ch1, align 1
  %0 = load i8, ptr %ch1, align 1
  %1 = sext i8 %0 to i64
  %getmp = icmp sge i64 %1, 97
  br i1 %getmp, label %then, label %ifcont3

then:                                             ; preds = %entry
  %2 = load i8, ptr %ch1, align 1
  %3 = sext i8 %2 to i64
  %letmp = icmp sle i64 %3, 122
  br i1 %letmp, label %then2, label %ifcont

then2:                                            ; preds = %then
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 1, ptr %val_ptr, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val

ifcont:                                           ; preds = %then
  br label %ifcont3

ifcont3:                                          ; preds = %ifcont, %entry
  %auto_wrap_result4 = alloca %result_int8, align 8
  %err_ptr5 = getelementptr inbounds %result_int8, ptr %auto_wrap_result4, i32 0, i32 0
  store i8 0, ptr %err_ptr5, align 1
  %val_ptr6 = getelementptr inbounds %result_int8, ptr %auto_wrap_result4, i32 0, i32 1
  store i8 0, ptr %val_ptr6, align 1
  %result_val7 = load %result_int8, ptr %auto_wrap_result4, align 1
  ret %result_int8 %result_val7
}

define internal %result_int8 @is_alpha(i8 %ch) {
entry:
  %lower = alloca i8, align 1
  %upper = alloca i8, align 1
  %ch1 = alloca i8, align 1
  store i8 %ch, ptr %ch1, align 1
  %0 = load i8, ptr %ch1, align 1
  %calltmp = call %result_int8 @is_uppercase(i8 %0)
  %result_temp = alloca %result_int8, align 8
  store %result_int8 %calltmp, ptr %result_temp, align 1
  %err_ptr = getelementptr inbounds %result_int8, ptr %result_temp, i32 0, i32 0
  %err = load i8, ptr %err_ptr, align 1
  %is_success = icmp eq i8 %err, 0
  %val_ptr = getelementptr inbounds %result_int8, ptr %result_temp, i32 0, i32 1
  %val = load i8, ptr %val_ptr, align 1
  %unwrap_result = select i1 %is_success, i8 %val, i8 0
  store i8 %unwrap_result, ptr %upper, align 1
  %1 = load i8, ptr %upper, align 1
  %2 = sext i8 %1 to i64
  %eqtmp = icmp eq i64 %2, 1
  br i1 %eqtmp, label %then, label %ifcont

then:                                             ; preds = %entry
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr2 = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr2, align 1
  %val_ptr3 = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 1, ptr %val_ptr3, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val

ifcont:                                           ; preds = %entry
  %3 = load i8, ptr %ch1, align 1
  %calltmp4 = call %result_int8 @is_lowercase(i8 %3)
  %result_temp5 = alloca %result_int8, align 8
  store %result_int8 %calltmp4, ptr %result_temp5, align 1
  %err_ptr6 = getelementptr inbounds %result_int8, ptr %result_temp5, i32 0, i32 0
  %err7 = load i8, ptr %err_ptr6, align 1
  %is_success8 = icmp eq i8 %err7, 0
  %val_ptr9 = getelementptr inbounds %result_int8, ptr %result_temp5, i32 0, i32 1
  %val10 = load i8, ptr %val_ptr9, align 1
  %unwrap_result11 = select i1 %is_success8, i8 %val10, i8 0
  store i8 %unwrap_result11, ptr %lower, align 1
  %4 = load i8, ptr %lower, align 1
  %auto_wrap_result12 = alloca %result_int8, align 8
  %err_ptr13 = getelementptr inbounds %result_int8, ptr %auto_wrap_result12, i32 0, i32 0
  store i8 0, ptr %err_ptr13, align 1
  %val_ptr14 = getelementptr inbounds %result_int8, ptr %auto_wrap_result12, i32 0, i32 1
  store i8 %4, ptr %val_ptr14, align 1
  %result_val15 = load %result_int8, ptr %auto_wrap_result12, align 1
  ret %result_int8 %result_val15
}

define internal %result_int8 @is_digit(i8 %ch) {
entry:
  %ch1 = alloca i8, align 1
  store i8 %ch, ptr %ch1, align 1
  %0 = load i8, ptr %ch1, align 1
  %1 = sext i8 %0 to i64
  %getmp = icmp sge i64 %1, 48
  br i1 %getmp, label %then, label %ifcont3

then:                                             ; preds = %entry
  %2 = load i8, ptr %ch1, align 1
  %3 = sext i8 %2 to i64
  %letmp = icmp sle i64 %3, 57
  br i1 %letmp, label %then2, label %ifcont

then2:                                            ; preds = %then
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 1, ptr %val_ptr, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val

ifcont:                                           ; preds = %then
  br label %ifcont3

ifcont3:                                          ; preds = %ifcont, %entry
  %auto_wrap_result4 = alloca %result_int8, align 8
  %err_ptr5 = getelementptr inbounds %result_int8, ptr %auto_wrap_result4, i32 0, i32 0
  store i8 0, ptr %err_ptr5, align 1
  %val_ptr6 = getelementptr inbounds %result_int8, ptr %auto_wrap_result4, i32 0, i32 1
  store i8 0, ptr %val_ptr6, align 1
  %result_val7 = load %result_int8, ptr %auto_wrap_result4, align 1
  ret %result_int8 %result_val7
}

define internal %result_int8 @is_alnum(i8 %ch) {
entry:
  %digit = alloca i8, align 1
  %alpha = alloca i8, align 1
  %ch1 = alloca i8, align 1
  store i8 %ch, ptr %ch1, align 1
  %0 = load i8, ptr %ch1, align 1
  %calltmp = call %result_int8 @is_alpha(i8 %0)
  %result_temp = alloca %result_int8, align 8
  store %result_int8 %calltmp, ptr %result_temp, align 1
  %err_ptr = getelementptr inbounds %result_int8, ptr %result_temp, i32 0, i32 0
  %err = load i8, ptr %err_ptr, align 1
  %is_success = icmp eq i8 %err, 0
  %val_ptr = getelementptr inbounds %result_int8, ptr %result_temp, i32 0, i32 1
  %val = load i8, ptr %val_ptr, align 1
  %unwrap_result = select i1 %is_success, i8 %val, i8 0
  store i8 %unwrap_result, ptr %alpha, align 1
  %1 = load i8, ptr %alpha, align 1
  %2 = sext i8 %1 to i64
  %eqtmp = icmp eq i64 %2, 1
  br i1 %eqtmp, label %then, label %ifcont

then:                                             ; preds = %entry
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr2 = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr2, align 1
  %val_ptr3 = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 1, ptr %val_ptr3, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val

ifcont:                                           ; preds = %entry
  %3 = load i8, ptr %ch1, align 1
  %calltmp4 = call %result_int8 @is_digit(i8 %3)
  %result_temp5 = alloca %result_int8, align 8
  store %result_int8 %calltmp4, ptr %result_temp5, align 1
  %err_ptr6 = getelementptr inbounds %result_int8, ptr %result_temp5, i32 0, i32 0
  %err7 = load i8, ptr %err_ptr6, align 1
  %is_success8 = icmp eq i8 %err7, 0
  %val_ptr9 = getelementptr inbounds %result_int8, ptr %result_temp5, i32 0, i32 1
  %val10 = load i8, ptr %val_ptr9, align 1
  %unwrap_result11 = select i1 %is_success8, i8 %val10, i8 0
  store i8 %unwrap_result11, ptr %digit, align 1
  %4 = load i8, ptr %digit, align 1
  %auto_wrap_result12 = alloca %result_int8, align 8
  %err_ptr13 = getelementptr inbounds %result_int8, ptr %auto_wrap_result12, i32 0, i32 0
  store i8 0, ptr %err_ptr13, align 1
  %val_ptr14 = getelementptr inbounds %result_int8, ptr %auto_wrap_result12, i32 0, i32 1
  store i8 %4, ptr %val_ptr14, align 1
  %result_val15 = load %result_int8, ptr %auto_wrap_result12, align 1
  ret %result_int8 %result_val15
}

define internal %result_int8 @is_whitespace(i8 %ch) {
entry:
  %ch1 = alloca i8, align 1
  store i8 %ch, ptr %ch1, align 1
  %0 = load i8, ptr %ch1, align 1
  %1 = sext i8 %0 to i64
  %eqtmp = icmp eq i64 %1, 32
  br i1 %eqtmp, label %then, label %ifcont

then:                                             ; preds = %entry
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 1, ptr %val_ptr, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val

ifcont:                                           ; preds = %entry
  %2 = load i8, ptr %ch1, align 1
  %3 = sext i8 %2 to i64
  %eqtmp2 = icmp eq i64 %3, 9
  br i1 %eqtmp2, label %then3, label %ifcont8

then3:                                            ; preds = %ifcont
  %auto_wrap_result4 = alloca %result_int8, align 8
  %err_ptr5 = getelementptr inbounds %result_int8, ptr %auto_wrap_result4, i32 0, i32 0
  store i8 0, ptr %err_ptr5, align 1
  %val_ptr6 = getelementptr inbounds %result_int8, ptr %auto_wrap_result4, i32 0, i32 1
  store i8 1, ptr %val_ptr6, align 1
  %result_val7 = load %result_int8, ptr %auto_wrap_result4, align 1
  ret %result_int8 %result_val7

ifcont8:                                          ; preds = %ifcont
  %4 = load i8, ptr %ch1, align 1
  %5 = sext i8 %4 to i64
  %eqtmp9 = icmp eq i64 %5, 10
  br i1 %eqtmp9, label %then10, label %ifcont15

then10:                                           ; preds = %ifcont8
  %auto_wrap_result11 = alloca %result_int8, align 8
  %err_ptr12 = getelementptr inbounds %result_int8, ptr %auto_wrap_result11, i32 0, i32 0
  store i8 0, ptr %err_ptr12, align 1
  %val_ptr13 = getelementptr inbounds %result_int8, ptr %auto_wrap_result11, i32 0, i32 1
  store i8 1, ptr %val_ptr13, align 1
  %result_val14 = load %result_int8, ptr %auto_wrap_result11, align 1
  ret %result_int8 %result_val14

ifcont15:                                         ; preds = %ifcont8
  %6 = load i8, ptr %ch1, align 1
  %7 = sext i8 %6 to i64
  %eqtmp16 = icmp eq i64 %7, 13
  br i1 %eqtmp16, label %then17, label %ifcont22

then17:                                           ; preds = %ifcont15
  %auto_wrap_result18 = alloca %result_int8, align 8
  %err_ptr19 = getelementptr inbounds %result_int8, ptr %auto_wrap_result18, i32 0, i32 0
  store i8 0, ptr %err_ptr19, align 1
  %val_ptr20 = getelementptr inbounds %result_int8, ptr %auto_wrap_result18, i32 0, i32 1
  store i8 1, ptr %val_ptr20, align 1
  %result_val21 = load %result_int8, ptr %auto_wrap_result18, align 1
  ret %result_int8 %result_val21

ifcont22:                                         ; preds = %ifcont15
  %auto_wrap_result23 = alloca %result_int8, align 8
  %err_ptr24 = getelementptr inbounds %result_int8, ptr %auto_wrap_result23, i32 0, i32 0
  store i8 0, ptr %err_ptr24, align 1
  %val_ptr25 = getelementptr inbounds %result_int8, ptr %auto_wrap_result23, i32 0, i32 1
  store i8 0, ptr %val_ptr25, align 1
  %result_val26 = load %result_int8, ptr %auto_wrap_result23, align 1
  ret %result_int8 %result_val26
}

define internal %result_int8 @is_printable(i8 %ch) {
entry:
  %ch1 = alloca i8, align 1
  store i8 %ch, ptr %ch1, align 1
  %0 = load i8, ptr %ch1, align 1
  %1 = sext i8 %0 to i64
  %getmp = icmp sge i64 %1, 32
  br i1 %getmp, label %then, label %ifcont3

then:                                             ; preds = %entry
  %2 = load i8, ptr %ch1, align 1
  %3 = sext i8 %2 to i64
  %letmp = icmp sle i64 %3, 126
  br i1 %letmp, label %then2, label %ifcont

then2:                                            ; preds = %then
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 1, ptr %val_ptr, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val

ifcont:                                           ; preds = %then
  br label %ifcont3

ifcont3:                                          ; preds = %ifcont, %entry
  %auto_wrap_result4 = alloca %result_int8, align 8
  %err_ptr5 = getelementptr inbounds %result_int8, ptr %auto_wrap_result4, i32 0, i32 0
  store i8 0, ptr %err_ptr5, align 1
  %val_ptr6 = getelementptr inbounds %result_int8, ptr %auto_wrap_result4, i32 0, i32 1
  store i8 0, ptr %val_ptr6, align 1
  %result_val7 = load %result_int8, ptr %auto_wrap_result4, align 1
  ret %result_int8 %result_val7
}

define internal %result_int8 @to_lowercase(i8 %ch) {
entry:
  %upper = alloca i8, align 1
  %ch1 = alloca i8, align 1
  store i8 %ch, ptr %ch1, align 1
  %0 = load i8, ptr %ch1, align 1
  %calltmp = call %result_int8 @is_uppercase(i8 %0)
  %result_temp = alloca %result_int8, align 8
  store %result_int8 %calltmp, ptr %result_temp, align 1
  %err_ptr = getelementptr inbounds %result_int8, ptr %result_temp, i32 0, i32 0
  %err = load i8, ptr %err_ptr, align 1
  %is_success = icmp eq i8 %err, 0
  %val_ptr = getelementptr inbounds %result_int8, ptr %result_temp, i32 0, i32 1
  %val = load i8, ptr %val_ptr, align 1
  %unwrap_result = select i1 %is_success, i8 %val, i8 0
  store i8 %unwrap_result, ptr %upper, align 1
  %1 = load i8, ptr %upper, align 1
  %2 = sext i8 %1 to i64
  %eqtmp = icmp eq i64 %2, 1
  br i1 %eqtmp, label %then, label %ifcont

then:                                             ; preds = %entry
  %3 = load i8, ptr %ch1, align 1
  %4 = sext i8 %3 to i64
  %addtmp = add i64 %4, 32
  %auto_wrap_cast = trunc i64 %addtmp to i8
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr2 = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr2, align 1
  %val_ptr3 = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 %auto_wrap_cast, ptr %val_ptr3, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val

ifcont:                                           ; preds = %entry
  %5 = load i8, ptr %ch1, align 1
  %auto_wrap_result4 = alloca %result_int8, align 8
  %err_ptr5 = getelementptr inbounds %result_int8, ptr %auto_wrap_result4, i32 0, i32 0
  store i8 0, ptr %err_ptr5, align 1
  %val_ptr6 = getelementptr inbounds %result_int8, ptr %auto_wrap_result4, i32 0, i32 1
  store i8 %5, ptr %val_ptr6, align 1
  %result_val7 = load %result_int8, ptr %auto_wrap_result4, align 1
  ret %result_int8 %result_val7
}

define internal %result_int8 @to_uppercase(i8 %ch) {
entry:
  %lower = alloca i8, align 1
  %ch1 = alloca i8, align 1
  store i8 %ch, ptr %ch1, align 1
  %0 = load i8, ptr %ch1, align 1
  %calltmp = call %result_int8 @is_lowercase(i8 %0)
  %result_temp = alloca %result_int8, align 8
  store %result_int8 %calltmp, ptr %result_temp, align 1
  %err_ptr = getelementptr inbounds %result_int8, ptr %result_temp, i32 0, i32 0
  %err = load i8, ptr %err_ptr, align 1
  %is_success = icmp eq i8 %err, 0
  %val_ptr = getelementptr inbounds %result_int8, ptr %result_temp, i32 0, i32 1
  %val = load i8, ptr %val_ptr, align 1
  %unwrap_result = select i1 %is_success, i8 %val, i8 0
  store i8 %unwrap_result, ptr %lower, align 1
  %1 = load i8, ptr %lower, align 1
  %2 = sext i8 %1 to i64
  %eqtmp = icmp eq i64 %2, 1
  br i1 %eqtmp, label %then, label %ifcont

then:                                             ; preds = %entry
  %3 = load i8, ptr %ch1, align 1
  %4 = sext i8 %3 to i64
  %subtmp = sub i64 %4, 32
  %auto_wrap_cast = trunc i64 %subtmp to i8
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr2 = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr2, align 1
  %val_ptr3 = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 %auto_wrap_cast, ptr %val_ptr3, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val

ifcont:                                           ; preds = %entry
  %5 = load i8, ptr %ch1, align 1
  %auto_wrap_result4 = alloca %result_int8, align 8
  %err_ptr5 = getelementptr inbounds %result_int8, ptr %auto_wrap_result4, i32 0, i32 0
  store i8 0, ptr %err_ptr5, align 1
  %val_ptr6 = getelementptr inbounds %result_int8, ptr %auto_wrap_result4, i32 0, i32 1
  store i8 %5, ptr %val_ptr6, align 1
  %result_val7 = load %result_int8, ptr %auto_wrap_result4, align 1
  ret %result_int8 %result_val7
}

define internal %result_int8 @digit_to_int(i8 %ch) {
entry:
  %valid = alloca i8, align 1
  %ch1 = alloca i8, align 1
  store i8 %ch, ptr %ch1, align 1
  %0 = load i8, ptr %ch1, align 1
  %calltmp = call %result_int8 @is_digit(i8 %0)
  %result_temp = alloca %result_int8, align 8
  store %result_int8 %calltmp, ptr %result_temp, align 1
  %err_ptr = getelementptr inbounds %result_int8, ptr %result_temp, i32 0, i32 0
  %err = load i8, ptr %err_ptr, align 1
  %is_success = icmp eq i8 %err, 0
  %val_ptr = getelementptr inbounds %result_int8, ptr %result_temp, i32 0, i32 1
  %val = load i8, ptr %val_ptr, align 1
  %unwrap_result = select i1 %is_success, i8 %val, i8 0
  store i8 %unwrap_result, ptr %valid, align 1
  %1 = load i8, ptr %valid, align 1
  %2 = sext i8 %1 to i64
  %eqtmp = icmp eq i64 %2, 0
  br i1 %eqtmp, label %then, label %ifcont

then:                                             ; preds = %entry
  %result = alloca %result_int8, align 8
  %err_ptr2 = getelementptr inbounds %result_int8, ptr %result, i32 0, i32 0
  store i8 1, ptr %err_ptr2, align 1
  %val_ptr3 = getelementptr inbounds %result_int8, ptr %result, i32 0, i32 1
  store i8 0, ptr %val_ptr3, align 1
  %result_val = load %result_int8, ptr %result, align 1
  ret %result_int8 %result_val

ifcont:                                           ; preds = %entry
  %3 = load i8, ptr %ch1, align 1
  %4 = sext i8 %3 to i64
  %subtmp = sub i64 %4, 48
  %auto_wrap_cast = trunc i64 %subtmp to i8
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr4 = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr4, align 1
  %val_ptr5 = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 %auto_wrap_cast, ptr %val_ptr5, align 1
  %result_val6 = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val6
}

define internal %result_int8 @int_to_digit(i8 %n) {
entry:
  %n1 = alloca i8, align 1
  store i8 %n, ptr %n1, align 1
  %0 = load i8, ptr %n1, align 1
  %1 = sext i8 %0 to i64
  %lttmp = icmp slt i64 %1, 0
  br i1 %lttmp, label %then, label %ifcont

then:                                             ; preds = %entry
  %result = alloca %result_int8, align 8
  %err_ptr = getelementptr inbounds %result_int8, ptr %result, i32 0, i32 0
  store i8 1, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int8, ptr %result, i32 0, i32 1
  store i8 0, ptr %val_ptr, align 1
  %result_val = load %result_int8, ptr %result, align 1
  ret %result_int8 %result_val

ifcont:                                           ; preds = %entry
  %2 = load i8, ptr %n1, align 1
  %3 = sext i8 %2 to i64
  %gttmp = icmp sgt i64 %3, 9
  br i1 %gttmp, label %then2, label %ifcont7

then2:                                            ; preds = %ifcont
  %result3 = alloca %result_int8, align 8
  %err_ptr4 = getelementptr inbounds %result_int8, ptr %result3, i32 0, i32 0
  store i8 1, ptr %err_ptr4, align 1
  %val_ptr5 = getelementptr inbounds %result_int8, ptr %result3, i32 0, i32 1
  store i8 0, ptr %val_ptr5, align 1
  %result_val6 = load %result_int8, ptr %result3, align 1
  ret %result_int8 %result_val6

ifcont7:                                          ; preds = %ifcont
  %4 = load i8, ptr %n1, align 1
  %5 = sext i8 %4 to i64
  %addtmp = add i64 %5, 48
  %auto_wrap_cast = trunc i64 %addtmp to i8
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr8 = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr8, align 1
  %val_ptr9 = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 %auto_wrap_cast, ptr %val_ptr9, align 1
  %result_val10 = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val10
}

define internal %result_int8 @hex_to_int(i8 %ch) {
entry:
  %ch1 = alloca i8, align 1
  store i8 %ch, ptr %ch1, align 1
  %0 = load i8, ptr %ch1, align 1
  %1 = sext i8 %0 to i64
  %getmp = icmp sge i64 %1, 48
  br i1 %getmp, label %then, label %ifcont3

then:                                             ; preds = %entry
  %2 = load i8, ptr %ch1, align 1
  %3 = sext i8 %2 to i64
  %letmp = icmp sle i64 %3, 57
  br i1 %letmp, label %then2, label %ifcont

then2:                                            ; preds = %then
  %4 = load i8, ptr %ch1, align 1
  %5 = sext i8 %4 to i64
  %subtmp = sub i64 %5, 48
  %auto_wrap_cast = trunc i64 %subtmp to i8
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 %auto_wrap_cast, ptr %val_ptr, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val

ifcont:                                           ; preds = %then
  br label %ifcont3

ifcont3:                                          ; preds = %ifcont, %entry
  %6 = load i8, ptr %ch1, align 1
  %7 = sext i8 %6 to i64
  %getmp4 = icmp sge i64 %7, 65
  br i1 %getmp4, label %then5, label %ifcont15

then5:                                            ; preds = %ifcont3
  %8 = load i8, ptr %ch1, align 1
  %9 = sext i8 %8 to i64
  %letmp6 = icmp sle i64 %9, 70
  br i1 %letmp6, label %then7, label %ifcont14

then7:                                            ; preds = %then5
  %10 = load i8, ptr %ch1, align 1
  %11 = sext i8 %10 to i64
  %subtmp8 = sub i64 %11, 65
  %addtmp = add i64 %subtmp8, 10
  %auto_wrap_cast9 = trunc i64 %addtmp to i8
  %auto_wrap_result10 = alloca %result_int8, align 8
  %err_ptr11 = getelementptr inbounds %result_int8, ptr %auto_wrap_result10, i32 0, i32 0
  store i8 0, ptr %err_ptr11, align 1
  %val_ptr12 = getelementptr inbounds %result_int8, ptr %auto_wrap_result10, i32 0, i32 1
  store i8 %auto_wrap_cast9, ptr %val_ptr12, align 1
  %result_val13 = load %result_int8, ptr %auto_wrap_result10, align 1
  ret %result_int8 %result_val13

ifcont14:                                         ; preds = %then5
  br label %ifcont15

ifcont15:                                         ; preds = %ifcont14, %ifcont3
  %12 = load i8, ptr %ch1, align 1
  %13 = sext i8 %12 to i64
  %getmp16 = icmp sge i64 %13, 97
  br i1 %getmp16, label %then17, label %ifcont28

then17:                                           ; preds = %ifcont15
  %14 = load i8, ptr %ch1, align 1
  %15 = sext i8 %14 to i64
  %letmp18 = icmp sle i64 %15, 102
  br i1 %letmp18, label %then19, label %ifcont27

then19:                                           ; preds = %then17
  %16 = load i8, ptr %ch1, align 1
  %17 = sext i8 %16 to i64
  %subtmp20 = sub i64 %17, 97
  %addtmp21 = add i64 %subtmp20, 10
  %auto_wrap_cast22 = trunc i64 %addtmp21 to i8
  %auto_wrap_result23 = alloca %result_int8, align 8
  %err_ptr24 = getelementptr inbounds %result_int8, ptr %auto_wrap_result23, i32 0, i32 0
  store i8 0, ptr %err_ptr24, align 1
  %val_ptr25 = getelementptr inbounds %result_int8, ptr %auto_wrap_result23, i32 0, i32 1
  store i8 %auto_wrap_cast22, ptr %val_ptr25, align 1
  %result_val26 = load %result_int8, ptr %auto_wrap_result23, align 1
  ret %result_int8 %result_val26

ifcont27:                                         ; preds = %then17
  br label %ifcont28

ifcont28:                                         ; preds = %ifcont27, %ifcont15
  %result = alloca %result_int8, align 8
  %err_ptr29 = getelementptr inbounds %result_int8, ptr %result, i32 0, i32 0
  store i8 1, ptr %err_ptr29, align 1
  %val_ptr30 = getelementptr inbounds %result_int8, ptr %result, i32 0, i32 1
  store i8 0, ptr %val_ptr30, align 1
  %result_val31 = load %result_int8, ptr %result, align 1
  ret %result_int8 %result_val31
  %auto_wrap_result32 = alloca %result_int8, align 8
  %err_ptr33 = getelementptr inbounds %result_int8, ptr %auto_wrap_result32, i32 0, i32 0
  store i8 0, ptr %err_ptr33, align 1
  %val_ptr34 = getelementptr inbounds %result_int8, ptr %auto_wrap_result32, i32 0, i32 1
  store i8 0, ptr %val_ptr34, align 1
  %result_val35 = load %result_int8, ptr %auto_wrap_result32, align 1
  ret %result_int8 %result_val35
}

define internal %result_int8 @int_to_hex(i8 %n) {
entry:
  %n1 = alloca i8, align 1
  store i8 %n, ptr %n1, align 1
  %0 = load i8, ptr %n1, align 1
  %1 = sext i8 %0 to i64
  %lttmp = icmp slt i64 %1, 0
  br i1 %lttmp, label %then, label %ifcont

then:                                             ; preds = %entry
  %result = alloca %result_int8, align 8
  %err_ptr = getelementptr inbounds %result_int8, ptr %result, i32 0, i32 0
  store i8 1, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int8, ptr %result, i32 0, i32 1
  store i8 0, ptr %val_ptr, align 1
  %result_val = load %result_int8, ptr %result, align 1
  ret %result_int8 %result_val

ifcont:                                           ; preds = %entry
  %2 = load i8, ptr %n1, align 1
  %3 = sext i8 %2 to i64
  %gttmp = icmp sgt i64 %3, 15
  br i1 %gttmp, label %then2, label %ifcont7

then2:                                            ; preds = %ifcont
  %result3 = alloca %result_int8, align 8
  %err_ptr4 = getelementptr inbounds %result_int8, ptr %result3, i32 0, i32 0
  store i8 1, ptr %err_ptr4, align 1
  %val_ptr5 = getelementptr inbounds %result_int8, ptr %result3, i32 0, i32 1
  store i8 0, ptr %val_ptr5, align 1
  %result_val6 = load %result_int8, ptr %result3, align 1
  ret %result_int8 %result_val6

ifcont7:                                          ; preds = %ifcont
  %4 = load i8, ptr %n1, align 1
  %5 = sext i8 %4 to i64
  %lttmp8 = icmp slt i64 %5, 10
  br i1 %lttmp8, label %then9, label %ifcont13

then9:                                            ; preds = %ifcont7
  %6 = load i8, ptr %n1, align 1
  %7 = sext i8 %6 to i64
  %addtmp = add i64 %7, 48
  %auto_wrap_cast = trunc i64 %addtmp to i8
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr10 = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr10, align 1
  %val_ptr11 = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 %auto_wrap_cast, ptr %val_ptr11, align 1
  %result_val12 = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val12

ifcont13:                                         ; preds = %ifcont7
  %8 = load i8, ptr %n1, align 1
  %9 = sext i8 %8 to i64
  %subtmp = sub i64 %9, 10
  %addtmp14 = add i64 %subtmp, 65
  %auto_wrap_cast15 = trunc i64 %addtmp14 to i8
  %auto_wrap_result16 = alloca %result_int8, align 8
  %err_ptr17 = getelementptr inbounds %result_int8, ptr %auto_wrap_result16, i32 0, i32 0
  store i8 0, ptr %err_ptr17, align 1
  %val_ptr18 = getelementptr inbounds %result_int8, ptr %auto_wrap_result16, i32 0, i32 1
  store i8 %auto_wrap_cast15, ptr %val_ptr18, align 1
  %result_val19 = load %result_int8, ptr %auto_wrap_result16, align 1
  ret %result_int8 %result_val19
}

define internal %result_int8 @char_case_cmp(i8 %a, i8 %b) {
entry:
  %b_prop = alloca i8, align 1
  %a_prop = alloca i8, align 1
  %a1 = alloca i8, align 1
  store i8 %a, ptr %a1, align 1
  %b2 = alloca i8, align 1
  store i8 %b, ptr %b2, align 1
  %0 = load i8, ptr %a1, align 1
  %calltmp = call %result_int8 @is_uppercase(i8 %0)
  %result_temp = alloca %result_int8, align 8
  store %result_int8 %calltmp, ptr %result_temp, align 1
  %err_ptr = getelementptr inbounds %result_int8, ptr %result_temp, i32 0, i32 0
  %err = load i8, ptr %err_ptr, align 1
  %is_success = icmp eq i8 %err, 0
  %val_ptr = getelementptr inbounds %result_int8, ptr %result_temp, i32 0, i32 1
  %val = load i8, ptr %val_ptr, align 1
  %unwrap_result = select i1 %is_success, i8 %val, i8 0
  store i8 %unwrap_result, ptr %a_prop, align 1
  %1 = load i8, ptr %b2, align 1
  %calltmp3 = call %result_int8 @is_uppercase(i8 %1)
  %result_temp4 = alloca %result_int8, align 8
  store %result_int8 %calltmp3, ptr %result_temp4, align 1
  %err_ptr5 = getelementptr inbounds %result_int8, ptr %result_temp4, i32 0, i32 0
  %err6 = load i8, ptr %err_ptr5, align 1
  %is_success7 = icmp eq i8 %err6, 0
  %val_ptr8 = getelementptr inbounds %result_int8, ptr %result_temp4, i32 0, i32 1
  %val9 = load i8, ptr %val_ptr8, align 1
  %unwrap_result10 = select i1 %is_success7, i8 %val9, i8 0
  store i8 %unwrap_result10, ptr %b_prop, align 1
  %2 = load i8, ptr %a_prop, align 1
  %3 = load i8, ptr %b_prop, align 1
  %gttmp = icmp sgt i8 %2, %3
  br i1 %gttmp, label %then, label %ifcont

then:                                             ; preds = %entry
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr11 = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr11, align 1
  %val_ptr12 = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 1, ptr %val_ptr12, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val

ifcont:                                           ; preds = %entry
  %4 = load i8, ptr %a_prop, align 1
  %5 = load i8, ptr %b_prop, align 1
  %lttmp = icmp slt i8 %4, %5
  br i1 %lttmp, label %then13, label %ifcont18

then13:                                           ; preds = %ifcont
  %auto_wrap_result14 = alloca %result_int8, align 8
  %err_ptr15 = getelementptr inbounds %result_int8, ptr %auto_wrap_result14, i32 0, i32 0
  store i8 0, ptr %err_ptr15, align 1
  %val_ptr16 = getelementptr inbounds %result_int8, ptr %auto_wrap_result14, i32 0, i32 1
  store i8 -1, ptr %val_ptr16, align 1
  %result_val17 = load %result_int8, ptr %auto_wrap_result14, align 1
  ret %result_int8 %result_val17

ifcont18:                                         ; preds = %ifcont
  %auto_wrap_result19 = alloca %result_int8, align 8
  %err_ptr20 = getelementptr inbounds %result_int8, ptr %auto_wrap_result19, i32 0, i32 0
  store i8 0, ptr %err_ptr20, align 1
  %val_ptr21 = getelementptr inbounds %result_int8, ptr %auto_wrap_result19, i32 0, i32 1
  store i8 0, ptr %val_ptr21, align 1
  %result_val22 = load %result_int8, ptr %auto_wrap_result19, align 1
  ret %result_int8 %result_val22
}

define internal %result_int8 @char_alpha_cmp(i8 %a, i8 %b) {
entry:
  %b_prop = alloca i8, align 1
  %a_prop = alloca i8, align 1
  %a1 = alloca i8, align 1
  store i8 %a, ptr %a1, align 1
  %b2 = alloca i8, align 1
  store i8 %b, ptr %b2, align 1
  %0 = load i8, ptr %a1, align 1
  %calltmp = call %result_int8 @is_alpha(i8 %0)
  %result_temp = alloca %result_int8, align 8
  store %result_int8 %calltmp, ptr %result_temp, align 1
  %err_ptr = getelementptr inbounds %result_int8, ptr %result_temp, i32 0, i32 0
  %err = load i8, ptr %err_ptr, align 1
  %is_success = icmp eq i8 %err, 0
  %val_ptr = getelementptr inbounds %result_int8, ptr %result_temp, i32 0, i32 1
  %val = load i8, ptr %val_ptr, align 1
  %unwrap_result = select i1 %is_success, i8 %val, i8 0
  store i8 %unwrap_result, ptr %a_prop, align 1
  %1 = load i8, ptr %b2, align 1
  %calltmp3 = call %result_int8 @is_alpha(i8 %1)
  %result_temp4 = alloca %result_int8, align 8
  store %result_int8 %calltmp3, ptr %result_temp4, align 1
  %err_ptr5 = getelementptr inbounds %result_int8, ptr %result_temp4, i32 0, i32 0
  %err6 = load i8, ptr %err_ptr5, align 1
  %is_success7 = icmp eq i8 %err6, 0
  %val_ptr8 = getelementptr inbounds %result_int8, ptr %result_temp4, i32 0, i32 1
  %val9 = load i8, ptr %val_ptr8, align 1
  %unwrap_result10 = select i1 %is_success7, i8 %val9, i8 0
  store i8 %unwrap_result10, ptr %b_prop, align 1
  %2 = load i8, ptr %a_prop, align 1
  %3 = load i8, ptr %b_prop, align 1
  %gttmp = icmp sgt i8 %2, %3
  br i1 %gttmp, label %then, label %ifcont

then:                                             ; preds = %entry
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr11 = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr11, align 1
  %val_ptr12 = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 1, ptr %val_ptr12, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val

ifcont:                                           ; preds = %entry
  %4 = load i8, ptr %a_prop, align 1
  %5 = load i8, ptr %b_prop, align 1
  %lttmp = icmp slt i8 %4, %5
  br i1 %lttmp, label %then13, label %ifcont18

then13:                                           ; preds = %ifcont
  %auto_wrap_result14 = alloca %result_int8, align 8
  %err_ptr15 = getelementptr inbounds %result_int8, ptr %auto_wrap_result14, i32 0, i32 0
  store i8 0, ptr %err_ptr15, align 1
  %val_ptr16 = getelementptr inbounds %result_int8, ptr %auto_wrap_result14, i32 0, i32 1
  store i8 -1, ptr %val_ptr16, align 1
  %result_val17 = load %result_int8, ptr %auto_wrap_result14, align 1
  ret %result_int8 %result_val17

ifcont18:                                         ; preds = %ifcont
  %auto_wrap_result19 = alloca %result_int8, align 8
  %err_ptr20 = getelementptr inbounds %result_int8, ptr %auto_wrap_result19, i32 0, i32 0
  store i8 0, ptr %err_ptr20, align 1
  %val_ptr21 = getelementptr inbounds %result_int8, ptr %auto_wrap_result19, i32 0, i32 1
  store i8 0, ptr %val_ptr21, align 1
  %result_val22 = load %result_int8, ptr %auto_wrap_result19, align 1
  ret %result_int8 %result_val22
}

define internal %result_int8 @char_numeric_cmp(i8 %a, i8 %b) {
entry:
  %b_prop = alloca i8, align 1
  %a_prop = alloca i8, align 1
  %a1 = alloca i8, align 1
  store i8 %a, ptr %a1, align 1
  %b2 = alloca i8, align 1
  store i8 %b, ptr %b2, align 1
  %0 = load i8, ptr %a1, align 1
  %calltmp = call %result_int8 @is_digit(i8 %0)
  %result_temp = alloca %result_int8, align 8
  store %result_int8 %calltmp, ptr %result_temp, align 1
  %err_ptr = getelementptr inbounds %result_int8, ptr %result_temp, i32 0, i32 0
  %err = load i8, ptr %err_ptr, align 1
  %is_success = icmp eq i8 %err, 0
  %val_ptr = getelementptr inbounds %result_int8, ptr %result_temp, i32 0, i32 1
  %val = load i8, ptr %val_ptr, align 1
  %unwrap_result = select i1 %is_success, i8 %val, i8 0
  store i8 %unwrap_result, ptr %a_prop, align 1
  %1 = load i8, ptr %b2, align 1
  %calltmp3 = call %result_int8 @is_digit(i8 %1)
  %result_temp4 = alloca %result_int8, align 8
  store %result_int8 %calltmp3, ptr %result_temp4, align 1
  %err_ptr5 = getelementptr inbounds %result_int8, ptr %result_temp4, i32 0, i32 0
  %err6 = load i8, ptr %err_ptr5, align 1
  %is_success7 = icmp eq i8 %err6, 0
  %val_ptr8 = getelementptr inbounds %result_int8, ptr %result_temp4, i32 0, i32 1
  %val9 = load i8, ptr %val_ptr8, align 1
  %unwrap_result10 = select i1 %is_success7, i8 %val9, i8 0
  store i8 %unwrap_result10, ptr %b_prop, align 1
  %2 = load i8, ptr %a_prop, align 1
  %3 = load i8, ptr %b_prop, align 1
  %gttmp = icmp sgt i8 %2, %3
  br i1 %gttmp, label %then, label %ifcont

then:                                             ; preds = %entry
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr11 = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr11, align 1
  %val_ptr12 = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 1, ptr %val_ptr12, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val

ifcont:                                           ; preds = %entry
  %4 = load i8, ptr %a_prop, align 1
  %5 = load i8, ptr %b_prop, align 1
  %lttmp = icmp slt i8 %4, %5
  br i1 %lttmp, label %then13, label %ifcont18

then13:                                           ; preds = %ifcont
  %auto_wrap_result14 = alloca %result_int8, align 8
  %err_ptr15 = getelementptr inbounds %result_int8, ptr %auto_wrap_result14, i32 0, i32 0
  store i8 0, ptr %err_ptr15, align 1
  %val_ptr16 = getelementptr inbounds %result_int8, ptr %auto_wrap_result14, i32 0, i32 1
  store i8 -1, ptr %val_ptr16, align 1
  %result_val17 = load %result_int8, ptr %auto_wrap_result14, align 1
  ret %result_int8 %result_val17

ifcont18:                                         ; preds = %ifcont
  %auto_wrap_result19 = alloca %result_int8, align 8
  %err_ptr20 = getelementptr inbounds %result_int8, ptr %auto_wrap_result19, i32 0, i32 0
  store i8 0, ptr %err_ptr20, align 1
  %val_ptr21 = getelementptr inbounds %result_int8, ptr %auto_wrap_result19, i32 0, i32 1
  store i8 0, ptr %val_ptr21, align 1
  %result_val22 = load %result_int8, ptr %auto_wrap_result19, align 1
  ret %result_int8 %result_val22
}

define internal %result_int8 @char_min(i8 %a, i8 %b) {
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

define internal %result_int8 @char_max(i8 %a, i8 %b) {
entry:
  %a1 = alloca i8, align 1
  store i8 %a, ptr %a1, align 1
  %b2 = alloca i8, align 1
  store i8 %b, ptr %b2, align 1
  %0 = load i8, ptr %a1, align 1
  %1 = load i8, ptr %b2, align 1
  %gttmp = icmp sgt i8 %0, %1
  br i1 %gttmp, label %then, label %ifcont

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

define internal %result_int8 @char_in_range(i8 %ch, i8 %low, i8 %high) {
entry:
  %ch1 = alloca i8, align 1
  store i8 %ch, ptr %ch1, align 1
  %low2 = alloca i8, align 1
  store i8 %low, ptr %low2, align 1
  %high3 = alloca i8, align 1
  store i8 %high, ptr %high3, align 1
  %0 = load i8, ptr %ch1, align 1
  %1 = load i8, ptr %low2, align 1
  %getmp = icmp sge i8 %0, %1
  br i1 %getmp, label %then, label %ifcont5

then:                                             ; preds = %entry
  %2 = load i8, ptr %ch1, align 1
  %3 = load i8, ptr %high3, align 1
  %letmp = icmp sle i8 %2, %3
  br i1 %letmp, label %then4, label %ifcont

then4:                                            ; preds = %then
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 1, ptr %val_ptr, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val

ifcont:                                           ; preds = %then
  br label %ifcont5

ifcont5:                                          ; preds = %ifcont, %entry
  %auto_wrap_result6 = alloca %result_int8, align 8
  %err_ptr7 = getelementptr inbounds %result_int8, ptr %auto_wrap_result6, i32 0, i32 0
  store i8 0, ptr %err_ptr7, align 1
  %val_ptr8 = getelementptr inbounds %result_int8, ptr %auto_wrap_result6, i32 0, i32 1
  store i8 0, ptr %val_ptr8, align 1
  %result_val9 = load %result_int8, ptr %auto_wrap_result6, align 1
  ret %result_int8 %result_val9
}

define i64 @main() {
entry:
  call void @__aria_module_init()
  ret i64 0
}
