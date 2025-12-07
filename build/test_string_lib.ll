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
  %0 = load i64, ptr %ch1, align 4
  %getmp = icmp sge i64 %0, 65
  br i1 %getmp, label %then, label %ifcont3

then:                                             ; preds = %entry
  %1 = load i64, ptr %ch1, align 4
  %letmp = icmp sle i64 %1, 90
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
  %0 = load i64, ptr %ch1, align 4
  %getmp = icmp sge i64 %0, 97
  br i1 %getmp, label %then, label %ifcont3

then:                                             ; preds = %entry
  %1 = load i64, ptr %ch1, align 4
  %letmp = icmp sle i64 %1, 122
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

define internal %result_int8 @is_digit(i8 %ch) {
entry:
  %ch1 = alloca i8, align 1
  store i8 %ch, ptr %ch1, align 1
  %0 = load i64, ptr %ch1, align 4
  %getmp = icmp sge i64 %0, 48
  br i1 %getmp, label %then, label %ifcont3

then:                                             ; preds = %entry
  %1 = load i64, ptr %ch1, align 4
  %letmp = icmp sle i64 %1, 57
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
  %0 = load i64, ptr %ch1, align 4
  %1 = trunc i64 %0 to i8
  %calltmp = call %result_int8 @is_uppercase(i8 %1)
  %result_temp = alloca %result_int8, align 8
  store %result_int8 %calltmp, ptr %result_temp, align 1
  %err_ptr = getelementptr inbounds %result_int8, ptr %result_temp, i32 0, i32 0
  %err = load i8, ptr %err_ptr, align 1
  %is_success = icmp eq i8 %err, 0
  %val_ptr = getelementptr inbounds %result_int8, ptr %result_temp, i32 0, i32 1
  %val = load i8, ptr %val_ptr, align 1
  %unwrap_result = select i1 %is_success, i8 %val, i8 0
  store i8 %unwrap_result, ptr %upper, align 1
  %2 = load i8, ptr %upper, align 1
  %3 = sext i8 %2 to i64
  %eqtmp = icmp eq i64 %3, 1
  br i1 %eqtmp, label %then, label %ifcont

then:                                             ; preds = %entry
  %4 = load i64, ptr %ch1, align 4
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
  %5 = load i64, ptr %ch1, align 4
  %auto_wrap_cast4 = trunc i64 %5 to i8
  %auto_wrap_result5 = alloca %result_int8, align 8
  %err_ptr6 = getelementptr inbounds %result_int8, ptr %auto_wrap_result5, i32 0, i32 0
  store i8 0, ptr %err_ptr6, align 1
  %val_ptr7 = getelementptr inbounds %result_int8, ptr %auto_wrap_result5, i32 0, i32 1
  store i8 %auto_wrap_cast4, ptr %val_ptr7, align 1
  %result_val8 = load %result_int8, ptr %auto_wrap_result5, align 1
  ret %result_int8 %result_val8
}

define internal %result_int8 @to_uppercase(i8 %ch) {
entry:
  %lower = alloca i8, align 1
  %ch1 = alloca i8, align 1
  store i8 %ch, ptr %ch1, align 1
  %0 = load i64, ptr %ch1, align 4
  %1 = trunc i64 %0 to i8
  %calltmp = call %result_int8 @is_lowercase(i8 %1)
  %result_temp = alloca %result_int8, align 8
  store %result_int8 %calltmp, ptr %result_temp, align 1
  %err_ptr = getelementptr inbounds %result_int8, ptr %result_temp, i32 0, i32 0
  %err = load i8, ptr %err_ptr, align 1
  %is_success = icmp eq i8 %err, 0
  %val_ptr = getelementptr inbounds %result_int8, ptr %result_temp, i32 0, i32 1
  %val = load i8, ptr %val_ptr, align 1
  %unwrap_result = select i1 %is_success, i8 %val, i8 0
  store i8 %unwrap_result, ptr %lower, align 1
  %2 = load i8, ptr %lower, align 1
  %3 = sext i8 %2 to i64
  %eqtmp = icmp eq i64 %3, 1
  br i1 %eqtmp, label %then, label %ifcont

then:                                             ; preds = %entry
  %4 = load i64, ptr %ch1, align 4
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
  %5 = load i64, ptr %ch1, align 4
  %auto_wrap_cast4 = trunc i64 %5 to i8
  %auto_wrap_result5 = alloca %result_int8, align 8
  %err_ptr6 = getelementptr inbounds %result_int8, ptr %auto_wrap_result5, i32 0, i32 0
  store i8 0, ptr %err_ptr6, align 1
  %val_ptr7 = getelementptr inbounds %result_int8, ptr %auto_wrap_result5, i32 0, i32 1
  store i8 %auto_wrap_cast4, ptr %val_ptr7, align 1
  %result_val8 = load %result_int8, ptr %auto_wrap_result5, align 1
  ret %result_int8 %result_val8
}

define internal %result_int8 @digit_to_int(i8 %ch) {
entry:
  %valid = alloca i8, align 1
  %ch1 = alloca i8, align 1
  store i8 %ch, ptr %ch1, align 1
  %0 = load i64, ptr %ch1, align 4
  %1 = trunc i64 %0 to i8
  %calltmp = call %result_int8 @is_digit(i8 %1)
  %result_temp = alloca %result_int8, align 8
  store %result_int8 %calltmp, ptr %result_temp, align 1
  %err_ptr = getelementptr inbounds %result_int8, ptr %result_temp, i32 0, i32 0
  %err = load i8, ptr %err_ptr, align 1
  %is_success = icmp eq i8 %err, 0
  %val_ptr = getelementptr inbounds %result_int8, ptr %result_temp, i32 0, i32 1
  %val = load i8, ptr %val_ptr, align 1
  %unwrap_result = select i1 %is_success, i8 %val, i8 0
  store i8 %unwrap_result, ptr %valid, align 1
  %2 = load i8, ptr %valid, align 1
  %3 = sext i8 %2 to i64
  %eqtmp = icmp eq i64 %3, 0
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
  %4 = load i64, ptr %ch1, align 4
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
  %0 = load i64, ptr %n1, align 4
  %lttmp = icmp slt i64 %0, 0
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
  %1 = load i64, ptr %n1, align 4
  %gttmp = icmp sgt i64 %1, 9
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
  %2 = load i64, ptr %n1, align 4
  %addtmp = add i64 %2, 48
  %auto_wrap_cast = trunc i64 %addtmp to i8
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr8 = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr8, align 1
  %val_ptr9 = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 %auto_wrap_cast, ptr %val_ptr9, align 1
  %result_val10 = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val10
}

define internal %result_int8 @test_char_classification() {
entry:
  %r7 = alloca i8, align 1
  %r6 = alloca i8, align 1
  %r5 = alloca i8, align 1
  %r4 = alloca i8, align 1
  %r3 = alloca i8, align 1
  %r2 = alloca i8, align 1
  %r1 = alloca i8, align 1
  %calltmp = call %result_int8 @is_uppercase(i8 65)
  %result_temp = alloca %result_int8, align 8
  store %result_int8 %calltmp, ptr %result_temp, align 1
  %err_ptr = getelementptr inbounds %result_int8, ptr %result_temp, i32 0, i32 0
  %err = load i8, ptr %err_ptr, align 1
  %is_success = icmp eq i8 %err, 0
  %val_ptr = getelementptr inbounds %result_int8, ptr %result_temp, i32 0, i32 1
  %val = load i8, ptr %val_ptr, align 1
  %unwrap_result = select i1 %is_success, i8 %val, i8 0
  store i8 %unwrap_result, ptr %r1, align 1
  %0 = load i8, ptr %r1, align 1
  %1 = sext i8 %0 to i64
  %netmp = icmp ne i64 %1, 1
  br i1 %netmp, label %then, label %ifcont

then:                                             ; preds = %entry
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr1 = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr1, align 1
  %val_ptr2 = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 1, ptr %val_ptr2, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val

ifcont:                                           ; preds = %entry
  %calltmp3 = call %result_int8 @is_uppercase(i8 97)
  %result_temp4 = alloca %result_int8, align 8
  store %result_int8 %calltmp3, ptr %result_temp4, align 1
  %err_ptr5 = getelementptr inbounds %result_int8, ptr %result_temp4, i32 0, i32 0
  %err6 = load i8, ptr %err_ptr5, align 1
  %is_success7 = icmp eq i8 %err6, 0
  %val_ptr8 = getelementptr inbounds %result_int8, ptr %result_temp4, i32 0, i32 1
  %val9 = load i8, ptr %val_ptr8, align 1
  %unwrap_result10 = select i1 %is_success7, i8 %val9, i8 0
  store i8 %unwrap_result10, ptr %r2, align 1
  %2 = load i8, ptr %r2, align 1
  %3 = sext i8 %2 to i64
  %netmp11 = icmp ne i64 %3, 0
  br i1 %netmp11, label %then12, label %ifcont17

then12:                                           ; preds = %ifcont
  %auto_wrap_result13 = alloca %result_int8, align 8
  %err_ptr14 = getelementptr inbounds %result_int8, ptr %auto_wrap_result13, i32 0, i32 0
  store i8 0, ptr %err_ptr14, align 1
  %val_ptr15 = getelementptr inbounds %result_int8, ptr %auto_wrap_result13, i32 0, i32 1
  store i8 1, ptr %val_ptr15, align 1
  %result_val16 = load %result_int8, ptr %auto_wrap_result13, align 1
  ret %result_int8 %result_val16

ifcont17:                                         ; preds = %ifcont
  %calltmp18 = call %result_int8 @is_lowercase(i8 97)
  %result_temp19 = alloca %result_int8, align 8
  store %result_int8 %calltmp18, ptr %result_temp19, align 1
  %err_ptr20 = getelementptr inbounds %result_int8, ptr %result_temp19, i32 0, i32 0
  %err21 = load i8, ptr %err_ptr20, align 1
  %is_success22 = icmp eq i8 %err21, 0
  %val_ptr23 = getelementptr inbounds %result_int8, ptr %result_temp19, i32 0, i32 1
  %val24 = load i8, ptr %val_ptr23, align 1
  %unwrap_result25 = select i1 %is_success22, i8 %val24, i8 0
  store i8 %unwrap_result25, ptr %r3, align 1
  %4 = load i8, ptr %r3, align 1
  %5 = sext i8 %4 to i64
  %netmp26 = icmp ne i64 %5, 1
  br i1 %netmp26, label %then27, label %ifcont32

then27:                                           ; preds = %ifcont17
  %auto_wrap_result28 = alloca %result_int8, align 8
  %err_ptr29 = getelementptr inbounds %result_int8, ptr %auto_wrap_result28, i32 0, i32 0
  store i8 0, ptr %err_ptr29, align 1
  %val_ptr30 = getelementptr inbounds %result_int8, ptr %auto_wrap_result28, i32 0, i32 1
  store i8 1, ptr %val_ptr30, align 1
  %result_val31 = load %result_int8, ptr %auto_wrap_result28, align 1
  ret %result_int8 %result_val31

ifcont32:                                         ; preds = %ifcont17
  %calltmp33 = call %result_int8 @is_lowercase(i8 65)
  %result_temp34 = alloca %result_int8, align 8
  store %result_int8 %calltmp33, ptr %result_temp34, align 1
  %err_ptr35 = getelementptr inbounds %result_int8, ptr %result_temp34, i32 0, i32 0
  %err36 = load i8, ptr %err_ptr35, align 1
  %is_success37 = icmp eq i8 %err36, 0
  %val_ptr38 = getelementptr inbounds %result_int8, ptr %result_temp34, i32 0, i32 1
  %val39 = load i8, ptr %val_ptr38, align 1
  %unwrap_result40 = select i1 %is_success37, i8 %val39, i8 0
  store i8 %unwrap_result40, ptr %r4, align 1
  %6 = load i8, ptr %r4, align 1
  %7 = sext i8 %6 to i64
  %netmp41 = icmp ne i64 %7, 0
  br i1 %netmp41, label %then42, label %ifcont47

then42:                                           ; preds = %ifcont32
  %auto_wrap_result43 = alloca %result_int8, align 8
  %err_ptr44 = getelementptr inbounds %result_int8, ptr %auto_wrap_result43, i32 0, i32 0
  store i8 0, ptr %err_ptr44, align 1
  %val_ptr45 = getelementptr inbounds %result_int8, ptr %auto_wrap_result43, i32 0, i32 1
  store i8 1, ptr %val_ptr45, align 1
  %result_val46 = load %result_int8, ptr %auto_wrap_result43, align 1
  ret %result_int8 %result_val46

ifcont47:                                         ; preds = %ifcont32
  %calltmp48 = call %result_int8 @is_digit(i8 48)
  %result_temp49 = alloca %result_int8, align 8
  store %result_int8 %calltmp48, ptr %result_temp49, align 1
  %err_ptr50 = getelementptr inbounds %result_int8, ptr %result_temp49, i32 0, i32 0
  %err51 = load i8, ptr %err_ptr50, align 1
  %is_success52 = icmp eq i8 %err51, 0
  %val_ptr53 = getelementptr inbounds %result_int8, ptr %result_temp49, i32 0, i32 1
  %val54 = load i8, ptr %val_ptr53, align 1
  %unwrap_result55 = select i1 %is_success52, i8 %val54, i8 0
  store i8 %unwrap_result55, ptr %r5, align 1
  %8 = load i8, ptr %r5, align 1
  %9 = sext i8 %8 to i64
  %netmp56 = icmp ne i64 %9, 1
  br i1 %netmp56, label %then57, label %ifcont62

then57:                                           ; preds = %ifcont47
  %auto_wrap_result58 = alloca %result_int8, align 8
  %err_ptr59 = getelementptr inbounds %result_int8, ptr %auto_wrap_result58, i32 0, i32 0
  store i8 0, ptr %err_ptr59, align 1
  %val_ptr60 = getelementptr inbounds %result_int8, ptr %auto_wrap_result58, i32 0, i32 1
  store i8 1, ptr %val_ptr60, align 1
  %result_val61 = load %result_int8, ptr %auto_wrap_result58, align 1
  ret %result_int8 %result_val61

ifcont62:                                         ; preds = %ifcont47
  %calltmp63 = call %result_int8 @is_digit(i8 57)
  %result_temp64 = alloca %result_int8, align 8
  store %result_int8 %calltmp63, ptr %result_temp64, align 1
  %err_ptr65 = getelementptr inbounds %result_int8, ptr %result_temp64, i32 0, i32 0
  %err66 = load i8, ptr %err_ptr65, align 1
  %is_success67 = icmp eq i8 %err66, 0
  %val_ptr68 = getelementptr inbounds %result_int8, ptr %result_temp64, i32 0, i32 1
  %val69 = load i8, ptr %val_ptr68, align 1
  %unwrap_result70 = select i1 %is_success67, i8 %val69, i8 0
  store i8 %unwrap_result70, ptr %r6, align 1
  %10 = load i8, ptr %r6, align 1
  %11 = sext i8 %10 to i64
  %netmp71 = icmp ne i64 %11, 1
  br i1 %netmp71, label %then72, label %ifcont77

then72:                                           ; preds = %ifcont62
  %auto_wrap_result73 = alloca %result_int8, align 8
  %err_ptr74 = getelementptr inbounds %result_int8, ptr %auto_wrap_result73, i32 0, i32 0
  store i8 0, ptr %err_ptr74, align 1
  %val_ptr75 = getelementptr inbounds %result_int8, ptr %auto_wrap_result73, i32 0, i32 1
  store i8 1, ptr %val_ptr75, align 1
  %result_val76 = load %result_int8, ptr %auto_wrap_result73, align 1
  ret %result_int8 %result_val76

ifcont77:                                         ; preds = %ifcont62
  %calltmp78 = call %result_int8 @is_digit(i8 65)
  %result_temp79 = alloca %result_int8, align 8
  store %result_int8 %calltmp78, ptr %result_temp79, align 1
  %err_ptr80 = getelementptr inbounds %result_int8, ptr %result_temp79, i32 0, i32 0
  %err81 = load i8, ptr %err_ptr80, align 1
  %is_success82 = icmp eq i8 %err81, 0
  %val_ptr83 = getelementptr inbounds %result_int8, ptr %result_temp79, i32 0, i32 1
  %val84 = load i8, ptr %val_ptr83, align 1
  %unwrap_result85 = select i1 %is_success82, i8 %val84, i8 0
  store i8 %unwrap_result85, ptr %r7, align 1
  %12 = load i8, ptr %r7, align 1
  %13 = sext i8 %12 to i64
  %netmp86 = icmp ne i64 %13, 0
  br i1 %netmp86, label %then87, label %ifcont92

then87:                                           ; preds = %ifcont77
  %auto_wrap_result88 = alloca %result_int8, align 8
  %err_ptr89 = getelementptr inbounds %result_int8, ptr %auto_wrap_result88, i32 0, i32 0
  store i8 0, ptr %err_ptr89, align 1
  %val_ptr90 = getelementptr inbounds %result_int8, ptr %auto_wrap_result88, i32 0, i32 1
  store i8 1, ptr %val_ptr90, align 1
  %result_val91 = load %result_int8, ptr %auto_wrap_result88, align 1
  ret %result_int8 %result_val91

ifcont92:                                         ; preds = %ifcont77
  %auto_wrap_result93 = alloca %result_int8, align 8
  %err_ptr94 = getelementptr inbounds %result_int8, ptr %auto_wrap_result93, i32 0, i32 0
  store i8 0, ptr %err_ptr94, align 1
  %val_ptr95 = getelementptr inbounds %result_int8, ptr %auto_wrap_result93, i32 0, i32 1
  store i8 0, ptr %val_ptr95, align 1
  %result_val96 = load %result_int8, ptr %auto_wrap_result93, align 1
  ret %result_int8 %result_val96
}

define internal %result_int8 @test_char_conversion() {
entry:
  %already_upper = alloca i8, align 1
  %upper_z = alloca i8, align 1
  %upper_a = alloca i8, align 1
  %already_lower = alloca i8, align 1
  %lower_z = alloca i8, align 1
  %lower_a = alloca i8, align 1
  %calltmp = call %result_int8 @to_lowercase(i8 65)
  %result_temp = alloca %result_int8, align 8
  store %result_int8 %calltmp, ptr %result_temp, align 1
  %err_ptr = getelementptr inbounds %result_int8, ptr %result_temp, i32 0, i32 0
  %err = load i8, ptr %err_ptr, align 1
  %is_success = icmp eq i8 %err, 0
  %val_ptr = getelementptr inbounds %result_int8, ptr %result_temp, i32 0, i32 1
  %val = load i8, ptr %val_ptr, align 1
  %unwrap_result = select i1 %is_success, i8 %val, i8 0
  store i8 %unwrap_result, ptr %lower_a, align 1
  %0 = load i8, ptr %lower_a, align 1
  %1 = sext i8 %0 to i64
  %netmp = icmp ne i64 %1, 97
  br i1 %netmp, label %then, label %ifcont

then:                                             ; preds = %entry
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr1 = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr1, align 1
  %val_ptr2 = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 1, ptr %val_ptr2, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val

ifcont:                                           ; preds = %entry
  %calltmp3 = call %result_int8 @to_lowercase(i8 90)
  %result_temp4 = alloca %result_int8, align 8
  store %result_int8 %calltmp3, ptr %result_temp4, align 1
  %err_ptr5 = getelementptr inbounds %result_int8, ptr %result_temp4, i32 0, i32 0
  %err6 = load i8, ptr %err_ptr5, align 1
  %is_success7 = icmp eq i8 %err6, 0
  %val_ptr8 = getelementptr inbounds %result_int8, ptr %result_temp4, i32 0, i32 1
  %val9 = load i8, ptr %val_ptr8, align 1
  %unwrap_result10 = select i1 %is_success7, i8 %val9, i8 0
  store i8 %unwrap_result10, ptr %lower_z, align 1
  %2 = load i8, ptr %lower_z, align 1
  %3 = sext i8 %2 to i64
  %netmp11 = icmp ne i64 %3, 122
  br i1 %netmp11, label %then12, label %ifcont17

then12:                                           ; preds = %ifcont
  %auto_wrap_result13 = alloca %result_int8, align 8
  %err_ptr14 = getelementptr inbounds %result_int8, ptr %auto_wrap_result13, i32 0, i32 0
  store i8 0, ptr %err_ptr14, align 1
  %val_ptr15 = getelementptr inbounds %result_int8, ptr %auto_wrap_result13, i32 0, i32 1
  store i8 1, ptr %val_ptr15, align 1
  %result_val16 = load %result_int8, ptr %auto_wrap_result13, align 1
  ret %result_int8 %result_val16

ifcont17:                                         ; preds = %ifcont
  %calltmp18 = call %result_int8 @to_lowercase(i8 97)
  %result_temp19 = alloca %result_int8, align 8
  store %result_int8 %calltmp18, ptr %result_temp19, align 1
  %err_ptr20 = getelementptr inbounds %result_int8, ptr %result_temp19, i32 0, i32 0
  %err21 = load i8, ptr %err_ptr20, align 1
  %is_success22 = icmp eq i8 %err21, 0
  %val_ptr23 = getelementptr inbounds %result_int8, ptr %result_temp19, i32 0, i32 1
  %val24 = load i8, ptr %val_ptr23, align 1
  %unwrap_result25 = select i1 %is_success22, i8 %val24, i8 0
  store i8 %unwrap_result25, ptr %already_lower, align 1
  %4 = load i8, ptr %already_lower, align 1
  %5 = sext i8 %4 to i64
  %netmp26 = icmp ne i64 %5, 97
  br i1 %netmp26, label %then27, label %ifcont32

then27:                                           ; preds = %ifcont17
  %auto_wrap_result28 = alloca %result_int8, align 8
  %err_ptr29 = getelementptr inbounds %result_int8, ptr %auto_wrap_result28, i32 0, i32 0
  store i8 0, ptr %err_ptr29, align 1
  %val_ptr30 = getelementptr inbounds %result_int8, ptr %auto_wrap_result28, i32 0, i32 1
  store i8 1, ptr %val_ptr30, align 1
  %result_val31 = load %result_int8, ptr %auto_wrap_result28, align 1
  ret %result_int8 %result_val31

ifcont32:                                         ; preds = %ifcont17
  %calltmp33 = call %result_int8 @to_uppercase(i8 97)
  %result_temp34 = alloca %result_int8, align 8
  store %result_int8 %calltmp33, ptr %result_temp34, align 1
  %err_ptr35 = getelementptr inbounds %result_int8, ptr %result_temp34, i32 0, i32 0
  %err36 = load i8, ptr %err_ptr35, align 1
  %is_success37 = icmp eq i8 %err36, 0
  %val_ptr38 = getelementptr inbounds %result_int8, ptr %result_temp34, i32 0, i32 1
  %val39 = load i8, ptr %val_ptr38, align 1
  %unwrap_result40 = select i1 %is_success37, i8 %val39, i8 0
  store i8 %unwrap_result40, ptr %upper_a, align 1
  %6 = load i8, ptr %upper_a, align 1
  %7 = sext i8 %6 to i64
  %netmp41 = icmp ne i64 %7, 65
  br i1 %netmp41, label %then42, label %ifcont47

then42:                                           ; preds = %ifcont32
  %auto_wrap_result43 = alloca %result_int8, align 8
  %err_ptr44 = getelementptr inbounds %result_int8, ptr %auto_wrap_result43, i32 0, i32 0
  store i8 0, ptr %err_ptr44, align 1
  %val_ptr45 = getelementptr inbounds %result_int8, ptr %auto_wrap_result43, i32 0, i32 1
  store i8 1, ptr %val_ptr45, align 1
  %result_val46 = load %result_int8, ptr %auto_wrap_result43, align 1
  ret %result_int8 %result_val46

ifcont47:                                         ; preds = %ifcont32
  %calltmp48 = call %result_int8 @to_uppercase(i8 122)
  %result_temp49 = alloca %result_int8, align 8
  store %result_int8 %calltmp48, ptr %result_temp49, align 1
  %err_ptr50 = getelementptr inbounds %result_int8, ptr %result_temp49, i32 0, i32 0
  %err51 = load i8, ptr %err_ptr50, align 1
  %is_success52 = icmp eq i8 %err51, 0
  %val_ptr53 = getelementptr inbounds %result_int8, ptr %result_temp49, i32 0, i32 1
  %val54 = load i8, ptr %val_ptr53, align 1
  %unwrap_result55 = select i1 %is_success52, i8 %val54, i8 0
  store i8 %unwrap_result55, ptr %upper_z, align 1
  %8 = load i8, ptr %upper_z, align 1
  %9 = sext i8 %8 to i64
  %netmp56 = icmp ne i64 %9, 90
  br i1 %netmp56, label %then57, label %ifcont62

then57:                                           ; preds = %ifcont47
  %auto_wrap_result58 = alloca %result_int8, align 8
  %err_ptr59 = getelementptr inbounds %result_int8, ptr %auto_wrap_result58, i32 0, i32 0
  store i8 0, ptr %err_ptr59, align 1
  %val_ptr60 = getelementptr inbounds %result_int8, ptr %auto_wrap_result58, i32 0, i32 1
  store i8 1, ptr %val_ptr60, align 1
  %result_val61 = load %result_int8, ptr %auto_wrap_result58, align 1
  ret %result_int8 %result_val61

ifcont62:                                         ; preds = %ifcont47
  %calltmp63 = call %result_int8 @to_uppercase(i8 65)
  %result_temp64 = alloca %result_int8, align 8
  store %result_int8 %calltmp63, ptr %result_temp64, align 1
  %err_ptr65 = getelementptr inbounds %result_int8, ptr %result_temp64, i32 0, i32 0
  %err66 = load i8, ptr %err_ptr65, align 1
  %is_success67 = icmp eq i8 %err66, 0
  %val_ptr68 = getelementptr inbounds %result_int8, ptr %result_temp64, i32 0, i32 1
  %val69 = load i8, ptr %val_ptr68, align 1
  %unwrap_result70 = select i1 %is_success67, i8 %val69, i8 0
  store i8 %unwrap_result70, ptr %already_upper, align 1
  %10 = load i8, ptr %already_upper, align 1
  %11 = sext i8 %10 to i64
  %netmp71 = icmp ne i64 %11, 65
  br i1 %netmp71, label %then72, label %ifcont77

then72:                                           ; preds = %ifcont62
  %auto_wrap_result73 = alloca %result_int8, align 8
  %err_ptr74 = getelementptr inbounds %result_int8, ptr %auto_wrap_result73, i32 0, i32 0
  store i8 0, ptr %err_ptr74, align 1
  %val_ptr75 = getelementptr inbounds %result_int8, ptr %auto_wrap_result73, i32 0, i32 1
  store i8 1, ptr %val_ptr75, align 1
  %result_val76 = load %result_int8, ptr %auto_wrap_result73, align 1
  ret %result_int8 %result_val76

ifcont77:                                         ; preds = %ifcont62
  %auto_wrap_result78 = alloca %result_int8, align 8
  %err_ptr79 = getelementptr inbounds %result_int8, ptr %auto_wrap_result78, i32 0, i32 0
  store i8 0, ptr %err_ptr79, align 1
  %val_ptr80 = getelementptr inbounds %result_int8, ptr %auto_wrap_result78, i32 0, i32 1
  store i8 0, ptr %val_ptr80, align 1
  %result_val81 = load %result_int8, ptr %auto_wrap_result78, align 1
  ret %result_int8 %result_val81
}

define internal %result_int8 @test_digit_conversion() {
entry:
  %c9 = alloca i8, align 1
  %c5 = alloca i8, align 1
  %c0 = alloca i8, align 1
  %d9 = alloca i8, align 1
  %d5 = alloca i8, align 1
  %d0 = alloca i8, align 1
  %calltmp = call %result_int8 @digit_to_int(i8 48)
  %result_temp = alloca %result_int8, align 8
  store %result_int8 %calltmp, ptr %result_temp, align 1
  %err_ptr = getelementptr inbounds %result_int8, ptr %result_temp, i32 0, i32 0
  %err = load i8, ptr %err_ptr, align 1
  %is_success = icmp eq i8 %err, 0
  %val_ptr = getelementptr inbounds %result_int8, ptr %result_temp, i32 0, i32 1
  %val = load i8, ptr %val_ptr, align 1
  %unwrap_result = select i1 %is_success, i8 %val, i8 -1
  store i8 %unwrap_result, ptr %d0, align 1
  %0 = load i8, ptr %d0, align 1
  %1 = sext i8 %0 to i64
  %netmp = icmp ne i64 %1, 0
  br i1 %netmp, label %then, label %ifcont

then:                                             ; preds = %entry
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr1 = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr1, align 1
  %val_ptr2 = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 1, ptr %val_ptr2, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val

ifcont:                                           ; preds = %entry
  %calltmp3 = call %result_int8 @digit_to_int(i8 53)
  %result_temp4 = alloca %result_int8, align 8
  store %result_int8 %calltmp3, ptr %result_temp4, align 1
  %err_ptr5 = getelementptr inbounds %result_int8, ptr %result_temp4, i32 0, i32 0
  %err6 = load i8, ptr %err_ptr5, align 1
  %is_success7 = icmp eq i8 %err6, 0
  %val_ptr8 = getelementptr inbounds %result_int8, ptr %result_temp4, i32 0, i32 1
  %val9 = load i8, ptr %val_ptr8, align 1
  %unwrap_result10 = select i1 %is_success7, i8 %val9, i8 -1
  store i8 %unwrap_result10, ptr %d5, align 1
  %2 = load i8, ptr %d5, align 1
  %3 = sext i8 %2 to i64
  %netmp11 = icmp ne i64 %3, 5
  br i1 %netmp11, label %then12, label %ifcont17

then12:                                           ; preds = %ifcont
  %auto_wrap_result13 = alloca %result_int8, align 8
  %err_ptr14 = getelementptr inbounds %result_int8, ptr %auto_wrap_result13, i32 0, i32 0
  store i8 0, ptr %err_ptr14, align 1
  %val_ptr15 = getelementptr inbounds %result_int8, ptr %auto_wrap_result13, i32 0, i32 1
  store i8 1, ptr %val_ptr15, align 1
  %result_val16 = load %result_int8, ptr %auto_wrap_result13, align 1
  ret %result_int8 %result_val16

ifcont17:                                         ; preds = %ifcont
  %calltmp18 = call %result_int8 @digit_to_int(i8 57)
  %result_temp19 = alloca %result_int8, align 8
  store %result_int8 %calltmp18, ptr %result_temp19, align 1
  %err_ptr20 = getelementptr inbounds %result_int8, ptr %result_temp19, i32 0, i32 0
  %err21 = load i8, ptr %err_ptr20, align 1
  %is_success22 = icmp eq i8 %err21, 0
  %val_ptr23 = getelementptr inbounds %result_int8, ptr %result_temp19, i32 0, i32 1
  %val24 = load i8, ptr %val_ptr23, align 1
  %unwrap_result25 = select i1 %is_success22, i8 %val24, i8 -1
  store i8 %unwrap_result25, ptr %d9, align 1
  %4 = load i8, ptr %d9, align 1
  %5 = sext i8 %4 to i64
  %netmp26 = icmp ne i64 %5, 9
  br i1 %netmp26, label %then27, label %ifcont32

then27:                                           ; preds = %ifcont17
  %auto_wrap_result28 = alloca %result_int8, align 8
  %err_ptr29 = getelementptr inbounds %result_int8, ptr %auto_wrap_result28, i32 0, i32 0
  store i8 0, ptr %err_ptr29, align 1
  %val_ptr30 = getelementptr inbounds %result_int8, ptr %auto_wrap_result28, i32 0, i32 1
  store i8 1, ptr %val_ptr30, align 1
  %result_val31 = load %result_int8, ptr %auto_wrap_result28, align 1
  ret %result_int8 %result_val31

ifcont32:                                         ; preds = %ifcont17
  %calltmp33 = call %result_int8 @int_to_digit(i8 0)
  %result_temp34 = alloca %result_int8, align 8
  store %result_int8 %calltmp33, ptr %result_temp34, align 1
  %err_ptr35 = getelementptr inbounds %result_int8, ptr %result_temp34, i32 0, i32 0
  %err36 = load i8, ptr %err_ptr35, align 1
  %is_success37 = icmp eq i8 %err36, 0
  %val_ptr38 = getelementptr inbounds %result_int8, ptr %result_temp34, i32 0, i32 1
  %val39 = load i8, ptr %val_ptr38, align 1
  %unwrap_result40 = select i1 %is_success37, i8 %val39, i8 0
  store i8 %unwrap_result40, ptr %c0, align 1
  %6 = load i8, ptr %c0, align 1
  %7 = sext i8 %6 to i64
  %netmp41 = icmp ne i64 %7, 48
  br i1 %netmp41, label %then42, label %ifcont47

then42:                                           ; preds = %ifcont32
  %auto_wrap_result43 = alloca %result_int8, align 8
  %err_ptr44 = getelementptr inbounds %result_int8, ptr %auto_wrap_result43, i32 0, i32 0
  store i8 0, ptr %err_ptr44, align 1
  %val_ptr45 = getelementptr inbounds %result_int8, ptr %auto_wrap_result43, i32 0, i32 1
  store i8 1, ptr %val_ptr45, align 1
  %result_val46 = load %result_int8, ptr %auto_wrap_result43, align 1
  ret %result_int8 %result_val46

ifcont47:                                         ; preds = %ifcont32
  %calltmp48 = call %result_int8 @int_to_digit(i8 5)
  %result_temp49 = alloca %result_int8, align 8
  store %result_int8 %calltmp48, ptr %result_temp49, align 1
  %err_ptr50 = getelementptr inbounds %result_int8, ptr %result_temp49, i32 0, i32 0
  %err51 = load i8, ptr %err_ptr50, align 1
  %is_success52 = icmp eq i8 %err51, 0
  %val_ptr53 = getelementptr inbounds %result_int8, ptr %result_temp49, i32 0, i32 1
  %val54 = load i8, ptr %val_ptr53, align 1
  %unwrap_result55 = select i1 %is_success52, i8 %val54, i8 0
  store i8 %unwrap_result55, ptr %c5, align 1
  %8 = load i8, ptr %c5, align 1
  %9 = sext i8 %8 to i64
  %netmp56 = icmp ne i64 %9, 53
  br i1 %netmp56, label %then57, label %ifcont62

then57:                                           ; preds = %ifcont47
  %auto_wrap_result58 = alloca %result_int8, align 8
  %err_ptr59 = getelementptr inbounds %result_int8, ptr %auto_wrap_result58, i32 0, i32 0
  store i8 0, ptr %err_ptr59, align 1
  %val_ptr60 = getelementptr inbounds %result_int8, ptr %auto_wrap_result58, i32 0, i32 1
  store i8 1, ptr %val_ptr60, align 1
  %result_val61 = load %result_int8, ptr %auto_wrap_result58, align 1
  ret %result_int8 %result_val61

ifcont62:                                         ; preds = %ifcont47
  %calltmp63 = call %result_int8 @int_to_digit(i8 9)
  %result_temp64 = alloca %result_int8, align 8
  store %result_int8 %calltmp63, ptr %result_temp64, align 1
  %err_ptr65 = getelementptr inbounds %result_int8, ptr %result_temp64, i32 0, i32 0
  %err66 = load i8, ptr %err_ptr65, align 1
  %is_success67 = icmp eq i8 %err66, 0
  %val_ptr68 = getelementptr inbounds %result_int8, ptr %result_temp64, i32 0, i32 1
  %val69 = load i8, ptr %val_ptr68, align 1
  %unwrap_result70 = select i1 %is_success67, i8 %val69, i8 0
  store i8 %unwrap_result70, ptr %c9, align 1
  %10 = load i8, ptr %c9, align 1
  %11 = sext i8 %10 to i64
  %netmp71 = icmp ne i64 %11, 57
  br i1 %netmp71, label %then72, label %ifcont77

then72:                                           ; preds = %ifcont62
  %auto_wrap_result73 = alloca %result_int8, align 8
  %err_ptr74 = getelementptr inbounds %result_int8, ptr %auto_wrap_result73, i32 0, i32 0
  store i8 0, ptr %err_ptr74, align 1
  %val_ptr75 = getelementptr inbounds %result_int8, ptr %auto_wrap_result73, i32 0, i32 1
  store i8 1, ptr %val_ptr75, align 1
  %result_val76 = load %result_int8, ptr %auto_wrap_result73, align 1
  ret %result_int8 %result_val76

ifcont77:                                         ; preds = %ifcont62
  %auto_wrap_result78 = alloca %result_int8, align 8
  %err_ptr79 = getelementptr inbounds %result_int8, ptr %auto_wrap_result78, i32 0, i32 0
  store i8 0, ptr %err_ptr79, align 1
  %val_ptr80 = getelementptr inbounds %result_int8, ptr %auto_wrap_result78, i32 0, i32 1
  store i8 0, ptr %val_ptr80, align 1
  %result_val81 = load %result_int8, ptr %auto_wrap_result78, align 1
  ret %result_int8 %result_val81
}

define internal %result_int8 @__user_main() {
entry:
  %result_val = alloca i8, align 1
  store i64 0, ptr %result_val, align 4
  %calltmp = call %result_int8 @test_char_classification()
  %result_temp = alloca %result_int8, align 8
  store %result_int8 %calltmp, ptr %result_temp, align 1
  %err_ptr = getelementptr inbounds %result_int8, ptr %result_temp, i32 0, i32 0
  %err = load i8, ptr %err_ptr, align 1
  %is_success = icmp eq i8 %err, 0
  %val_ptr = getelementptr inbounds %result_int8, ptr %result_temp, i32 0, i32 1
  %val = load i8, ptr %val_ptr, align 1
  %unwrap_result = select i1 %is_success, i8 %val, i8 1
  store i8 %unwrap_result, ptr %result_val, align 1
  %0 = load i8, ptr %result_val, align 1
  %1 = sext i8 %0 to i64
  %netmp = icmp ne i64 %1, 0
  br i1 %netmp, label %then, label %ifcont

then:                                             ; preds = %entry
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr1 = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr1, align 1
  %val_ptr2 = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 1, ptr %val_ptr2, align 1
  %result_val3 = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val3

ifcont:                                           ; preds = %entry
  %calltmp4 = call %result_int8 @test_char_conversion()
  %result_temp5 = alloca %result_int8, align 8
  store %result_int8 %calltmp4, ptr %result_temp5, align 1
  %err_ptr6 = getelementptr inbounds %result_int8, ptr %result_temp5, i32 0, i32 0
  %err7 = load i8, ptr %err_ptr6, align 1
  %is_success8 = icmp eq i8 %err7, 0
  %val_ptr9 = getelementptr inbounds %result_int8, ptr %result_temp5, i32 0, i32 1
  %val10 = load i8, ptr %val_ptr9, align 1
  %unwrap_result11 = select i1 %is_success8, i8 %val10, i8 1
  store i8 %unwrap_result11, ptr %result_val, align 1
  %2 = load i8, ptr %result_val, align 1
  %3 = sext i8 %2 to i64
  %netmp12 = icmp ne i64 %3, 0
  br i1 %netmp12, label %then13, label %ifcont18

then13:                                           ; preds = %ifcont
  %auto_wrap_result14 = alloca %result_int8, align 8
  %err_ptr15 = getelementptr inbounds %result_int8, ptr %auto_wrap_result14, i32 0, i32 0
  store i8 0, ptr %err_ptr15, align 1
  %val_ptr16 = getelementptr inbounds %result_int8, ptr %auto_wrap_result14, i32 0, i32 1
  store i8 1, ptr %val_ptr16, align 1
  %result_val17 = load %result_int8, ptr %auto_wrap_result14, align 1
  ret %result_int8 %result_val17

ifcont18:                                         ; preds = %ifcont
  %calltmp19 = call %result_int8 @test_digit_conversion()
  %result_temp20 = alloca %result_int8, align 8
  store %result_int8 %calltmp19, ptr %result_temp20, align 1
  %err_ptr21 = getelementptr inbounds %result_int8, ptr %result_temp20, i32 0, i32 0
  %err22 = load i8, ptr %err_ptr21, align 1
  %is_success23 = icmp eq i8 %err22, 0
  %val_ptr24 = getelementptr inbounds %result_int8, ptr %result_temp20, i32 0, i32 1
  %val25 = load i8, ptr %val_ptr24, align 1
  %unwrap_result26 = select i1 %is_success23, i8 %val25, i8 1
  store i8 %unwrap_result26, ptr %result_val, align 1
  %4 = load i8, ptr %result_val, align 1
  %5 = sext i8 %4 to i64
  %netmp27 = icmp ne i64 %5, 0
  br i1 %netmp27, label %then28, label %ifcont33

then28:                                           ; preds = %ifcont18
  %auto_wrap_result29 = alloca %result_int8, align 8
  %err_ptr30 = getelementptr inbounds %result_int8, ptr %auto_wrap_result29, i32 0, i32 0
  store i8 0, ptr %err_ptr30, align 1
  %val_ptr31 = getelementptr inbounds %result_int8, ptr %auto_wrap_result29, i32 0, i32 1
  store i8 1, ptr %val_ptr31, align 1
  %result_val32 = load %result_int8, ptr %auto_wrap_result29, align 1
  ret %result_int8 %result_val32

ifcont33:                                         ; preds = %ifcont18
  %auto_wrap_result34 = alloca %result_int8, align 8
  %err_ptr35 = getelementptr inbounds %result_int8, ptr %auto_wrap_result34, i32 0, i32 0
  store i8 0, ptr %err_ptr35, align 1
  %val_ptr36 = getelementptr inbounds %result_int8, ptr %auto_wrap_result34, i32 0, i32 1
  store i8 0, ptr %val_ptr36, align 1
  %result_val37 = load %result_int8, ptr %auto_wrap_result34, align 1
  ret %result_int8 %result_val37
}

define i64 @main() {
entry:
  call void @__aria_module_init()
  %0 = call %result_int8 @__user_main()
  ret i64 0
}
