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

define internal %result_int32 @abs_int32(i32 %value) {
entry:
  %value1 = alloca i32, align 4
  store i32 %value, ptr %value1, align 4
  %0 = load i64, ptr %value1, align 4
  %lttmp = icmp slt i64 %0, 0
  br i1 %lttmp, label %then, label %ifcont

then:                                             ; preds = %entry
  %1 = load i64, ptr %value1, align 4
  %subtmp = sub i64 0, %1
  %auto_wrap_cast = trunc i64 %subtmp to i32
  %auto_wrap_result = alloca %result_int32, align 8
  %err_ptr = getelementptr inbounds %result_int32, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int32, ptr %auto_wrap_result, i32 0, i32 1
  store i32 %auto_wrap_cast, ptr %val_ptr, align 4
  %result_val = load %result_int32, ptr %auto_wrap_result, align 4
  ret %result_int32 %result_val

ifcont:                                           ; preds = %entry
  %2 = load i64, ptr %value1, align 4
  %auto_wrap_cast2 = trunc i64 %2 to i32
  %auto_wrap_result3 = alloca %result_int32, align 8
  %err_ptr4 = getelementptr inbounds %result_int32, ptr %auto_wrap_result3, i32 0, i32 0
  store i8 0, ptr %err_ptr4, align 1
  %val_ptr5 = getelementptr inbounds %result_int32, ptr %auto_wrap_result3, i32 0, i32 1
  store i32 %auto_wrap_cast2, ptr %val_ptr5, align 4
  %result_val6 = load %result_int32, ptr %auto_wrap_result3, align 4
  ret %result_int32 %result_val6
}

define internal %result_int32 @min_int32(i32 %a, i32 %b) {
entry:
  %a1 = alloca i32, align 4
  store i32 %a, ptr %a1, align 4
  %b2 = alloca i32, align 4
  store i32 %b, ptr %b2, align 4
  %0 = load i64, ptr %a1, align 4
  %1 = load i64, ptr %b2, align 4
  %lttmp = icmp slt i64 %0, %1
  br i1 %lttmp, label %then, label %ifcont

then:                                             ; preds = %entry
  %2 = load i64, ptr %a1, align 4
  %auto_wrap_cast = trunc i64 %2 to i32
  %auto_wrap_result = alloca %result_int32, align 8
  %err_ptr = getelementptr inbounds %result_int32, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int32, ptr %auto_wrap_result, i32 0, i32 1
  store i32 %auto_wrap_cast, ptr %val_ptr, align 4
  %result_val = load %result_int32, ptr %auto_wrap_result, align 4
  ret %result_int32 %result_val

ifcont:                                           ; preds = %entry
  %3 = load i64, ptr %b2, align 4
  %auto_wrap_cast3 = trunc i64 %3 to i32
  %auto_wrap_result4 = alloca %result_int32, align 8
  %err_ptr5 = getelementptr inbounds %result_int32, ptr %auto_wrap_result4, i32 0, i32 0
  store i8 0, ptr %err_ptr5, align 1
  %val_ptr6 = getelementptr inbounds %result_int32, ptr %auto_wrap_result4, i32 0, i32 1
  store i32 %auto_wrap_cast3, ptr %val_ptr6, align 4
  %result_val7 = load %result_int32, ptr %auto_wrap_result4, align 4
  ret %result_int32 %result_val7
}

define internal %result_int32 @max_int32(i32 %a, i32 %b) {
entry:
  %a1 = alloca i32, align 4
  store i32 %a, ptr %a1, align 4
  %b2 = alloca i32, align 4
  store i32 %b, ptr %b2, align 4
  %0 = load i64, ptr %a1, align 4
  %1 = load i64, ptr %b2, align 4
  %gttmp = icmp sgt i64 %0, %1
  br i1 %gttmp, label %then, label %ifcont

then:                                             ; preds = %entry
  %2 = load i64, ptr %a1, align 4
  %auto_wrap_cast = trunc i64 %2 to i32
  %auto_wrap_result = alloca %result_int32, align 8
  %err_ptr = getelementptr inbounds %result_int32, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int32, ptr %auto_wrap_result, i32 0, i32 1
  store i32 %auto_wrap_cast, ptr %val_ptr, align 4
  %result_val = load %result_int32, ptr %auto_wrap_result, align 4
  ret %result_int32 %result_val

ifcont:                                           ; preds = %entry
  %3 = load i64, ptr %b2, align 4
  %auto_wrap_cast3 = trunc i64 %3 to i32
  %auto_wrap_result4 = alloca %result_int32, align 8
  %err_ptr5 = getelementptr inbounds %result_int32, ptr %auto_wrap_result4, i32 0, i32 0
  store i8 0, ptr %err_ptr5, align 1
  %val_ptr6 = getelementptr inbounds %result_int32, ptr %auto_wrap_result4, i32 0, i32 1
  store i32 %auto_wrap_cast3, ptr %val_ptr6, align 4
  %result_val7 = load %result_int32, ptr %auto_wrap_result4, align 4
  ret %result_int32 %result_val7
}

define internal %result_int8 @test_abs() {
entry:
  %r3 = alloca i32, align 4
  %r2 = alloca i32, align 4
  %r1 = alloca i32, align 4
  %calltmp = call %result_int32 @abs_int32(i32 5)
  %result_temp = alloca %result_int32, align 8
  store %result_int32 %calltmp, ptr %result_temp, align 4
  %err_ptr = getelementptr inbounds %result_int32, ptr %result_temp, i32 0, i32 0
  %err = load i8, ptr %err_ptr, align 1
  %is_success = icmp eq i8 %err, 0
  %val_ptr = getelementptr inbounds %result_int32, ptr %result_temp, i32 0, i32 1
  %val = load i32, ptr %val_ptr, align 4
  %unwrap_result = select i1 %is_success, i32 %val, i32 0
  store i32 %unwrap_result, ptr %r1, align 4
  %calltmp1 = call %result_int32 @abs_int32(i32 -5)
  %result_temp2 = alloca %result_int32, align 8
  store %result_int32 %calltmp1, ptr %result_temp2, align 4
  %err_ptr3 = getelementptr inbounds %result_int32, ptr %result_temp2, i32 0, i32 0
  %err4 = load i8, ptr %err_ptr3, align 1
  %is_success5 = icmp eq i8 %err4, 0
  %val_ptr6 = getelementptr inbounds %result_int32, ptr %result_temp2, i32 0, i32 1
  %val7 = load i32, ptr %val_ptr6, align 4
  %unwrap_result8 = select i1 %is_success5, i32 %val7, i32 0
  store i32 %unwrap_result8, ptr %r2, align 4
  %calltmp9 = call %result_int32 @abs_int32(i32 0)
  %result_temp10 = alloca %result_int32, align 8
  store %result_int32 %calltmp9, ptr %result_temp10, align 4
  %err_ptr11 = getelementptr inbounds %result_int32, ptr %result_temp10, i32 0, i32 0
  %err12 = load i8, ptr %err_ptr11, align 1
  %is_success13 = icmp eq i8 %err12, 0
  %val_ptr14 = getelementptr inbounds %result_int32, ptr %result_temp10, i32 0, i32 1
  %val15 = load i32, ptr %val_ptr14, align 4
  %unwrap_result16 = select i1 %is_success13, i32 %val15, i32 0
  store i32 %unwrap_result16, ptr %r3, align 4
  %0 = load i32, ptr %r1, align 4
  %1 = sext i32 %0 to i64
  %eqtmp = icmp eq i64 %1, 5
  br i1 %eqtmp, label %then, label %ifcont24

then:                                             ; preds = %entry
  %2 = load i32, ptr %r2, align 4
  %3 = sext i32 %2 to i64
  %eqtmp17 = icmp eq i64 %3, 5
  br i1 %eqtmp17, label %then18, label %ifcont23

then18:                                           ; preds = %then
  %4 = load i32, ptr %r3, align 4
  %5 = sext i32 %4 to i64
  %eqtmp19 = icmp eq i64 %5, 0
  br i1 %eqtmp19, label %then20, label %ifcont

then20:                                           ; preds = %then18
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr21 = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr21, align 1
  %val_ptr22 = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 1, ptr %val_ptr22, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val

ifcont:                                           ; preds = %then18
  br label %ifcont23

ifcont23:                                         ; preds = %ifcont, %then
  br label %ifcont24

ifcont24:                                         ; preds = %ifcont23, %entry
  %auto_wrap_result25 = alloca %result_int8, align 8
  %err_ptr26 = getelementptr inbounds %result_int8, ptr %auto_wrap_result25, i32 0, i32 0
  store i8 0, ptr %err_ptr26, align 1
  %val_ptr27 = getelementptr inbounds %result_int8, ptr %auto_wrap_result25, i32 0, i32 1
  store i8 0, ptr %val_ptr27, align 1
  %result_val28 = load %result_int8, ptr %auto_wrap_result25, align 1
  ret %result_int8 %result_val28
}

define internal %result_int8 @test_min() {
entry:
  %r3 = alloca i32, align 4
  %r2 = alloca i32, align 4
  %r1 = alloca i32, align 4
  %calltmp = call %result_int32 @min_int32(i32 5, i32 10)
  %result_temp = alloca %result_int32, align 8
  store %result_int32 %calltmp, ptr %result_temp, align 4
  %err_ptr = getelementptr inbounds %result_int32, ptr %result_temp, i32 0, i32 0
  %err = load i8, ptr %err_ptr, align 1
  %is_success = icmp eq i8 %err, 0
  %val_ptr = getelementptr inbounds %result_int32, ptr %result_temp, i32 0, i32 1
  %val = load i32, ptr %val_ptr, align 4
  %unwrap_result = select i1 %is_success, i32 %val, i32 0
  store i32 %unwrap_result, ptr %r1, align 4
  %calltmp1 = call %result_int32 @min_int32(i32 10, i32 5)
  %result_temp2 = alloca %result_int32, align 8
  store %result_int32 %calltmp1, ptr %result_temp2, align 4
  %err_ptr3 = getelementptr inbounds %result_int32, ptr %result_temp2, i32 0, i32 0
  %err4 = load i8, ptr %err_ptr3, align 1
  %is_success5 = icmp eq i8 %err4, 0
  %val_ptr6 = getelementptr inbounds %result_int32, ptr %result_temp2, i32 0, i32 1
  %val7 = load i32, ptr %val_ptr6, align 4
  %unwrap_result8 = select i1 %is_success5, i32 %val7, i32 0
  store i32 %unwrap_result8, ptr %r2, align 4
  %calltmp9 = call %result_int32 @min_int32(i32 7, i32 7)
  %result_temp10 = alloca %result_int32, align 8
  store %result_int32 %calltmp9, ptr %result_temp10, align 4
  %err_ptr11 = getelementptr inbounds %result_int32, ptr %result_temp10, i32 0, i32 0
  %err12 = load i8, ptr %err_ptr11, align 1
  %is_success13 = icmp eq i8 %err12, 0
  %val_ptr14 = getelementptr inbounds %result_int32, ptr %result_temp10, i32 0, i32 1
  %val15 = load i32, ptr %val_ptr14, align 4
  %unwrap_result16 = select i1 %is_success13, i32 %val15, i32 0
  store i32 %unwrap_result16, ptr %r3, align 4
  %0 = load i32, ptr %r1, align 4
  %1 = sext i32 %0 to i64
  %eqtmp = icmp eq i64 %1, 5
  br i1 %eqtmp, label %then, label %ifcont24

then:                                             ; preds = %entry
  %2 = load i32, ptr %r2, align 4
  %3 = sext i32 %2 to i64
  %eqtmp17 = icmp eq i64 %3, 5
  br i1 %eqtmp17, label %then18, label %ifcont23

then18:                                           ; preds = %then
  %4 = load i32, ptr %r3, align 4
  %5 = sext i32 %4 to i64
  %eqtmp19 = icmp eq i64 %5, 7
  br i1 %eqtmp19, label %then20, label %ifcont

then20:                                           ; preds = %then18
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr21 = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr21, align 1
  %val_ptr22 = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 1, ptr %val_ptr22, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val

ifcont:                                           ; preds = %then18
  br label %ifcont23

ifcont23:                                         ; preds = %ifcont, %then
  br label %ifcont24

ifcont24:                                         ; preds = %ifcont23, %entry
  %auto_wrap_result25 = alloca %result_int8, align 8
  %err_ptr26 = getelementptr inbounds %result_int8, ptr %auto_wrap_result25, i32 0, i32 0
  store i8 0, ptr %err_ptr26, align 1
  %val_ptr27 = getelementptr inbounds %result_int8, ptr %auto_wrap_result25, i32 0, i32 1
  store i8 0, ptr %val_ptr27, align 1
  %result_val28 = load %result_int8, ptr %auto_wrap_result25, align 1
  ret %result_int8 %result_val28
}

define internal %result_int8 @test_max() {
entry:
  %r3 = alloca i32, align 4
  %r2 = alloca i32, align 4
  %r1 = alloca i32, align 4
  %calltmp = call %result_int32 @max_int32(i32 5, i32 10)
  %result_temp = alloca %result_int32, align 8
  store %result_int32 %calltmp, ptr %result_temp, align 4
  %err_ptr = getelementptr inbounds %result_int32, ptr %result_temp, i32 0, i32 0
  %err = load i8, ptr %err_ptr, align 1
  %is_success = icmp eq i8 %err, 0
  %val_ptr = getelementptr inbounds %result_int32, ptr %result_temp, i32 0, i32 1
  %val = load i32, ptr %val_ptr, align 4
  %unwrap_result = select i1 %is_success, i32 %val, i32 0
  store i32 %unwrap_result, ptr %r1, align 4
  %calltmp1 = call %result_int32 @max_int32(i32 10, i32 5)
  %result_temp2 = alloca %result_int32, align 8
  store %result_int32 %calltmp1, ptr %result_temp2, align 4
  %err_ptr3 = getelementptr inbounds %result_int32, ptr %result_temp2, i32 0, i32 0
  %err4 = load i8, ptr %err_ptr3, align 1
  %is_success5 = icmp eq i8 %err4, 0
  %val_ptr6 = getelementptr inbounds %result_int32, ptr %result_temp2, i32 0, i32 1
  %val7 = load i32, ptr %val_ptr6, align 4
  %unwrap_result8 = select i1 %is_success5, i32 %val7, i32 0
  store i32 %unwrap_result8, ptr %r2, align 4
  %calltmp9 = call %result_int32 @max_int32(i32 7, i32 7)
  %result_temp10 = alloca %result_int32, align 8
  store %result_int32 %calltmp9, ptr %result_temp10, align 4
  %err_ptr11 = getelementptr inbounds %result_int32, ptr %result_temp10, i32 0, i32 0
  %err12 = load i8, ptr %err_ptr11, align 1
  %is_success13 = icmp eq i8 %err12, 0
  %val_ptr14 = getelementptr inbounds %result_int32, ptr %result_temp10, i32 0, i32 1
  %val15 = load i32, ptr %val_ptr14, align 4
  %unwrap_result16 = select i1 %is_success13, i32 %val15, i32 0
  store i32 %unwrap_result16, ptr %r3, align 4
  %0 = load i32, ptr %r1, align 4
  %1 = sext i32 %0 to i64
  %eqtmp = icmp eq i64 %1, 10
  br i1 %eqtmp, label %then, label %ifcont24

then:                                             ; preds = %entry
  %2 = load i32, ptr %r2, align 4
  %3 = sext i32 %2 to i64
  %eqtmp17 = icmp eq i64 %3, 10
  br i1 %eqtmp17, label %then18, label %ifcont23

then18:                                           ; preds = %then
  %4 = load i32, ptr %r3, align 4
  %5 = sext i32 %4 to i64
  %eqtmp19 = icmp eq i64 %5, 7
  br i1 %eqtmp19, label %then20, label %ifcont

then20:                                           ; preds = %then18
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr21 = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr21, align 1
  %val_ptr22 = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 1, ptr %val_ptr22, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val

ifcont:                                           ; preds = %then18
  br label %ifcont23

ifcont23:                                         ; preds = %ifcont, %then
  br label %ifcont24

ifcont24:                                         ; preds = %ifcont23, %entry
  %auto_wrap_result25 = alloca %result_int8, align 8
  %err_ptr26 = getelementptr inbounds %result_int8, ptr %auto_wrap_result25, i32 0, i32 0
  store i8 0, ptr %err_ptr26, align 1
  %val_ptr27 = getelementptr inbounds %result_int8, ptr %auto_wrap_result25, i32 0, i32 1
  store i8 0, ptr %val_ptr27, align 1
  %result_val28 = load %result_int8, ptr %auto_wrap_result25, align 1
  ret %result_int8 %result_val28
}

define internal %result_int8 @__user_main() {
entry:
  %tests_passed = alloca i8, align 1
  store i64 0, ptr %tests_passed, align 4
  %calltmp = call %result_int8 @test_abs()
  %result_temp = alloca %result_int8, align 8
  store %result_int8 %calltmp, ptr %result_temp, align 1
  %err_ptr = getelementptr inbounds %result_int8, ptr %result_temp, i32 0, i32 0
  %err = load i8, ptr %err_ptr, align 1
  %is_success = icmp eq i8 %err, 0
  %val_ptr = getelementptr inbounds %result_int8, ptr %result_temp, i32 0, i32 1
  %val = load i8, ptr %val_ptr, align 1
  %unwrap_result = select i1 %is_success, i8 %val, i8 0
  %0 = sext i8 %unwrap_result to i64
  %eqtmp = icmp eq i64 %0, 1
  br i1 %eqtmp, label %then, label %ifcont

then:                                             ; preds = %entry
  %1 = load i8, ptr %tests_passed, align 1
  %2 = sext i8 %1 to i64
  %addtmp = add i64 %2, 1
  store i64 %addtmp, ptr %tests_passed, align 4
  br label %ifcont

ifcont:                                           ; preds = %then, %entry
  %calltmp1 = call %result_int8 @test_min()
  %result_temp2 = alloca %result_int8, align 8
  store %result_int8 %calltmp1, ptr %result_temp2, align 1
  %err_ptr3 = getelementptr inbounds %result_int8, ptr %result_temp2, i32 0, i32 0
  %err4 = load i8, ptr %err_ptr3, align 1
  %is_success5 = icmp eq i8 %err4, 0
  %val_ptr6 = getelementptr inbounds %result_int8, ptr %result_temp2, i32 0, i32 1
  %val7 = load i8, ptr %val_ptr6, align 1
  %unwrap_result8 = select i1 %is_success5, i8 %val7, i8 0
  %3 = sext i8 %unwrap_result8 to i64
  %eqtmp9 = icmp eq i64 %3, 1
  br i1 %eqtmp9, label %then10, label %ifcont12

then10:                                           ; preds = %ifcont
  %4 = load i8, ptr %tests_passed, align 1
  %5 = sext i8 %4 to i64
  %addtmp11 = add i64 %5, 1
  store i64 %addtmp11, ptr %tests_passed, align 4
  br label %ifcont12

ifcont12:                                         ; preds = %then10, %ifcont
  %calltmp13 = call %result_int8 @test_max()
  %result_temp14 = alloca %result_int8, align 8
  store %result_int8 %calltmp13, ptr %result_temp14, align 1
  %err_ptr15 = getelementptr inbounds %result_int8, ptr %result_temp14, i32 0, i32 0
  %err16 = load i8, ptr %err_ptr15, align 1
  %is_success17 = icmp eq i8 %err16, 0
  %val_ptr18 = getelementptr inbounds %result_int8, ptr %result_temp14, i32 0, i32 1
  %val19 = load i8, ptr %val_ptr18, align 1
  %unwrap_result20 = select i1 %is_success17, i8 %val19, i8 0
  %6 = sext i8 %unwrap_result20 to i64
  %eqtmp21 = icmp eq i64 %6, 1
  br i1 %eqtmp21, label %then22, label %ifcont24

then22:                                           ; preds = %ifcont12
  %7 = load i8, ptr %tests_passed, align 1
  %8 = sext i8 %7 to i64
  %addtmp23 = add i64 %8, 1
  store i64 %addtmp23, ptr %tests_passed, align 4
  br label %ifcont24

ifcont24:                                         ; preds = %then22, %ifcont12
  %9 = load i8, ptr %tests_passed, align 1
  %10 = sext i8 %9 to i64
  %eqtmp25 = icmp eq i64 %10, 3
  br i1 %eqtmp25, label %then26, label %ifcont29

then26:                                           ; preds = %ifcont24
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr27 = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr27, align 1
  %val_ptr28 = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 0, ptr %val_ptr28, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val

ifcont29:                                         ; preds = %ifcont24
  %auto_wrap_result30 = alloca %result_int8, align 8
  %err_ptr31 = getelementptr inbounds %result_int8, ptr %auto_wrap_result30, i32 0, i32 0
  store i8 0, ptr %err_ptr31, align 1
  %val_ptr32 = getelementptr inbounds %result_int8, ptr %auto_wrap_result30, i32 0, i32 1
  store i8 1, ptr %val_ptr32, align 1
  %result_val33 = load %result_int8, ptr %auto_wrap_result30, align 1
  ret %result_int8 %result_val33
}

define i64 @main() {
entry:
  call void @__aria_module_init()
  %0 = call %result_int8 @__user_main()
  ret i64 0
}
