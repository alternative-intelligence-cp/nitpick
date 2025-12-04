; ModuleID = 'aria_module'
source_filename = "aria_module"

%result_int8 = type { i8, i8 }
%result_int32 = type { i8, i32 }

@0 = private unnamed_addr constant [40 x i8] c"=== Student Grade Management System ===\00", align 1
@1 = private unnamed_addr constant [31 x i8] c"Grade validation tests passed!\00", align 1
@2 = private unnamed_addr constant [38 x i8] c"Letter grade conversion tests passed!\00", align 1
@3 = private unnamed_addr constant [29 x i8] c"Class statistics calculated!\00", align 1
@4 = private unnamed_addr constant [20 x i8] c"All tests complete!\00", align 1

declare void @puts(ptr)

declare void @print(ptr)

define internal void @__aria_module_init() {
entry:
  ret void
}

define internal %result_int8 @validate_grade(i8 %grade) {
entry:
  %grade1 = alloca i8, align 1
  store i8 %grade, ptr %grade1, align 1
  %0 = load i64, ptr %grade1, align 4
  %pick_lt = icmp slt i64 %0, 0
  br i1 %pick_lt, label %case_body_0, label %case_next_0

case_body_0:                                      ; preds = %entry
  %result = alloca %result_int8, align 8
  %err_ptr = getelementptr inbounds %result_int8, ptr %result, i32 0, i32 0
  store i8 1, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int8, ptr %result, i32 0, i32 1
  store i8 0, ptr %val_ptr, align 1
  %result_val = load %result_int8, ptr %result, align 1
  ret %result_int8 %result_val

case_next_0:                                      ; preds = %entry
  %pick_gt = icmp sgt i64 %0, 100
  br i1 %pick_gt, label %case_body_1, label %case_next_1

case_body_1:                                      ; preds = %case_next_0
  %result2 = alloca %result_int8, align 8
  %err_ptr3 = getelementptr inbounds %result_int8, ptr %result2, i32 0, i32 0
  store i8 1, ptr %err_ptr3, align 1
  %val_ptr4 = getelementptr inbounds %result_int8, ptr %result2, i32 0, i32 1
  store i8 0, ptr %val_ptr4, align 1
  %result_val5 = load %result_int8, ptr %result2, align 1
  ret %result_int8 %result_val5

case_next_1:                                      ; preds = %case_next_0
  br i1 true, label %case_body_2, label %case_next_2

case_body_2:                                      ; preds = %case_next_1
  %1 = load i64, ptr %grade1, align 4
  %result6 = alloca %result_int8, align 8
  %err_ptr7 = getelementptr inbounds %result_int8, ptr %result6, i32 0, i32 0
  store i8 0, ptr %err_ptr7, align 1
  %val_cast = trunc i64 %1 to i8
  %val_ptr8 = getelementptr inbounds %result_int8, ptr %result6, i32 0, i32 1
  store i8 %val_cast, ptr %val_ptr8, align 1
  %result_val9 = load %result_int8, ptr %result6, align 1
  ret %result_int8 %result_val9

case_next_2:                                      ; preds = %case_next_1
  br label %pick_done

pick_done:                                        ; preds = %case_next_2
  %result10 = alloca %result_int8, align 8
  %err_ptr11 = getelementptr inbounds %result_int8, ptr %result10, i32 0, i32 0
  store i8 1, ptr %err_ptr11, align 1
  %val_ptr12 = getelementptr inbounds %result_int8, ptr %result10, i32 0, i32 1
  store i8 0, ptr %val_ptr12, align 1
  %result_val13 = load %result_int8, ptr %result10, align 1
  ret %result_int8 %result_val13
}

define internal %result_int8 @get_letter_grade(i8 %grade) {
entry:
  %grade1 = alloca i8, align 1
  store i8 %grade, ptr %grade1, align 1
  %0 = load i64, ptr %grade1, align 4
  %pick_ge = icmp sge i64 %0, 90
  br i1 %pick_ge, label %case_body_0, label %case_next_0

case_body_0:                                      ; preds = %entry
  %result = alloca %result_int8, align 8
  %err_ptr = getelementptr inbounds %result_int8, ptr %result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int8, ptr %result, i32 0, i32 1
  store i8 65, ptr %val_ptr, align 1
  %result_val = load %result_int8, ptr %result, align 1
  ret %result_int8 %result_val

case_next_0:                                      ; preds = %entry
  %pick_ge2 = icmp sge i64 %0, 80
  br i1 %pick_ge2, label %case_body_1, label %case_next_1

case_body_1:                                      ; preds = %case_next_0
  %result3 = alloca %result_int8, align 8
  %err_ptr4 = getelementptr inbounds %result_int8, ptr %result3, i32 0, i32 0
  store i8 0, ptr %err_ptr4, align 1
  %val_ptr5 = getelementptr inbounds %result_int8, ptr %result3, i32 0, i32 1
  store i8 66, ptr %val_ptr5, align 1
  %result_val6 = load %result_int8, ptr %result3, align 1
  ret %result_int8 %result_val6

case_next_1:                                      ; preds = %case_next_0
  %pick_ge7 = icmp sge i64 %0, 70
  br i1 %pick_ge7, label %case_body_2, label %case_next_2

case_body_2:                                      ; preds = %case_next_1
  %result8 = alloca %result_int8, align 8
  %err_ptr9 = getelementptr inbounds %result_int8, ptr %result8, i32 0, i32 0
  store i8 0, ptr %err_ptr9, align 1
  %val_ptr10 = getelementptr inbounds %result_int8, ptr %result8, i32 0, i32 1
  store i8 67, ptr %val_ptr10, align 1
  %result_val11 = load %result_int8, ptr %result8, align 1
  ret %result_int8 %result_val11

case_next_2:                                      ; preds = %case_next_1
  %pick_ge12 = icmp sge i64 %0, 60
  br i1 %pick_ge12, label %case_body_3, label %case_next_3

case_body_3:                                      ; preds = %case_next_2
  %result13 = alloca %result_int8, align 8
  %err_ptr14 = getelementptr inbounds %result_int8, ptr %result13, i32 0, i32 0
  store i8 0, ptr %err_ptr14, align 1
  %val_ptr15 = getelementptr inbounds %result_int8, ptr %result13, i32 0, i32 1
  store i8 68, ptr %val_ptr15, align 1
  %result_val16 = load %result_int8, ptr %result13, align 1
  ret %result_int8 %result_val16

case_next_3:                                      ; preds = %case_next_2
  br i1 true, label %case_body_4, label %case_next_4

case_body_4:                                      ; preds = %case_next_3
  %result17 = alloca %result_int8, align 8
  %err_ptr18 = getelementptr inbounds %result_int8, ptr %result17, i32 0, i32 0
  store i8 0, ptr %err_ptr18, align 1
  %val_ptr19 = getelementptr inbounds %result_int8, ptr %result17, i32 0, i32 1
  store i8 70, ptr %val_ptr19, align 1
  %result_val20 = load %result_int8, ptr %result17, align 1
  ret %result_int8 %result_val20

case_next_4:                                      ; preds = %case_next_3
  br label %pick_done

pick_done:                                        ; preds = %case_next_4
  %result21 = alloca %result_int8, align 8
  %err_ptr22 = getelementptr inbounds %result_int8, ptr %result21, i32 0, i32 0
  store i8 0, ptr %err_ptr22, align 1
  %val_ptr23 = getelementptr inbounds %result_int8, ptr %result21, i32 0, i32 1
  store i8 70, ptr %val_ptr23, align 1
  %result_val24 = load %result_int8, ptr %result21, align 1
  ret %result_int8 %result_val24
}

define internal %result_int32 @process_student(i8 %grade1, i8 %grade2, i8 %grade3) {
entry:
  %avg = alloca i32, align 4
  %sum = alloca i32, align 4
  %g3 = alloca i8, align 1
  %g2 = alloca i8, align 1
  %g1 = alloca i8, align 1
  %grade11 = alloca i8, align 1
  store i8 %grade1, ptr %grade11, align 1
  %grade22 = alloca i8, align 1
  store i8 %grade2, ptr %grade22, align 1
  %grade33 = alloca i8, align 1
  store i8 %grade3, ptr %grade33, align 1
  %0 = load i64, ptr %grade11, align 4
  %1 = trunc i64 %0 to i8
  %calltmp = call %result_int8 @validate_grade(i8 %1)
  %result_temp = alloca %result_int8, align 8
  store %result_int8 %calltmp, ptr %result_temp, align 1
  %err_ptr = getelementptr inbounds %result_int8, ptr %result_temp, i32 0, i32 0
  %err = load i8, ptr %err_ptr, align 1
  %is_success = icmp eq i8 %err, 0
  %val_ptr = getelementptr inbounds %result_int8, ptr %result_temp, i32 0, i32 1
  %val = load i8, ptr %val_ptr, align 1
  %unwrap_result = select i1 %is_success, i8 %val, i8 0
  store i8 %unwrap_result, ptr %g1, align 1
  %2 = load i64, ptr %grade22, align 4
  %3 = trunc i64 %2 to i8
  %calltmp4 = call %result_int8 @validate_grade(i8 %3)
  %result_temp5 = alloca %result_int8, align 8
  store %result_int8 %calltmp4, ptr %result_temp5, align 1
  %err_ptr6 = getelementptr inbounds %result_int8, ptr %result_temp5, i32 0, i32 0
  %err7 = load i8, ptr %err_ptr6, align 1
  %is_success8 = icmp eq i8 %err7, 0
  %val_ptr9 = getelementptr inbounds %result_int8, ptr %result_temp5, i32 0, i32 1
  %val10 = load i8, ptr %val_ptr9, align 1
  %unwrap_result11 = select i1 %is_success8, i8 %val10, i8 0
  store i8 %unwrap_result11, ptr %g2, align 1
  %4 = load i64, ptr %grade33, align 4
  %5 = trunc i64 %4 to i8
  %calltmp12 = call %result_int8 @validate_grade(i8 %5)
  %result_temp13 = alloca %result_int8, align 8
  store %result_int8 %calltmp12, ptr %result_temp13, align 1
  %err_ptr14 = getelementptr inbounds %result_int8, ptr %result_temp13, i32 0, i32 0
  %err15 = load i8, ptr %err_ptr14, align 1
  %is_success16 = icmp eq i8 %err15, 0
  %val_ptr17 = getelementptr inbounds %result_int8, ptr %result_temp13, i32 0, i32 1
  %val18 = load i8, ptr %val_ptr17, align 1
  %unwrap_result19 = select i1 %is_success16, i8 %val18, i8 0
  store i8 %unwrap_result19, ptr %g3, align 1
  %6 = load i8, ptr %g1, align 1
  %7 = load i8, ptr %g2, align 1
  %addtmp = add i8 %6, %7
  %8 = load i8, ptr %g3, align 1
  %addtmp20 = add i8 %addtmp, %8
  store i8 %addtmp20, ptr %sum, align 1
  %9 = load i32, ptr %sum, align 4
  %10 = sext i32 %9 to i64
  %divtmp = sdiv i64 %10, 3
  store i64 %divtmp, ptr %avg, align 4
  %11 = load i32, ptr %avg, align 4
  %result = alloca %result_int32, align 8
  %err_ptr21 = getelementptr inbounds %result_int32, ptr %result, i32 0, i32 0
  store i8 0, ptr %err_ptr21, align 1
  %val_ptr22 = getelementptr inbounds %result_int32, ptr %result, i32 0, i32 1
  store i32 %11, ptr %val_ptr22, align 4
  %result_val = load %result_int32, ptr %result, align 4
  ret %result_int32 %result_val
}

define internal %result_int32 @calculate_class_stats() {
entry:
  %class_avg = alloca i32, align 4
  %total = alloca i32, align 4
  %student4_avg = alloca i32, align 4
  %student3_avg = alloca i32, align 4
  %student2_avg = alloca i32, align 4
  %student1_avg = alloca i32, align 4
  %calltmp = call %result_int32 @process_student(i8 85, i8 92, i8 78)
  %result_temp = alloca %result_int32, align 8
  store %result_int32 %calltmp, ptr %result_temp, align 4
  %err_ptr = getelementptr inbounds %result_int32, ptr %result_temp, i32 0, i32 0
  %err = load i8, ptr %err_ptr, align 1
  %is_success = icmp eq i8 %err, 0
  %val_ptr = getelementptr inbounds %result_int32, ptr %result_temp, i32 0, i32 1
  %val = load i32, ptr %val_ptr, align 4
  %unwrap_result = select i1 %is_success, i32 %val, i32 0
  store i32 %unwrap_result, ptr %student1_avg, align 4
  %calltmp1 = call %result_int32 @process_student(i8 95, i8 88, i8 91)
  %result_temp2 = alloca %result_int32, align 8
  store %result_int32 %calltmp1, ptr %result_temp2, align 4
  %err_ptr3 = getelementptr inbounds %result_int32, ptr %result_temp2, i32 0, i32 0
  %err4 = load i8, ptr %err_ptr3, align 1
  %is_success5 = icmp eq i8 %err4, 0
  %val_ptr6 = getelementptr inbounds %result_int32, ptr %result_temp2, i32 0, i32 1
  %val7 = load i32, ptr %val_ptr6, align 4
  %unwrap_result8 = select i1 %is_success5, i32 %val7, i32 0
  store i32 %unwrap_result8, ptr %student2_avg, align 4
  %calltmp9 = call %result_int32 @process_student(i8 72, i8 68, i8 75)
  %result_temp10 = alloca %result_int32, align 8
  store %result_int32 %calltmp9, ptr %result_temp10, align 4
  %err_ptr11 = getelementptr inbounds %result_int32, ptr %result_temp10, i32 0, i32 0
  %err12 = load i8, ptr %err_ptr11, align 1
  %is_success13 = icmp eq i8 %err12, 0
  %val_ptr14 = getelementptr inbounds %result_int32, ptr %result_temp10, i32 0, i32 1
  %val15 = load i32, ptr %val_ptr14, align 4
  %unwrap_result16 = select i1 %is_success13, i32 %val15, i32 0
  store i32 %unwrap_result16, ptr %student3_avg, align 4
  %calltmp17 = call %result_int32 @process_student(i8 88, i8 86, i8 90)
  %result_temp18 = alloca %result_int32, align 8
  store %result_int32 %calltmp17, ptr %result_temp18, align 4
  %err_ptr19 = getelementptr inbounds %result_int32, ptr %result_temp18, i32 0, i32 0
  %err20 = load i8, ptr %err_ptr19, align 1
  %is_success21 = icmp eq i8 %err20, 0
  %val_ptr22 = getelementptr inbounds %result_int32, ptr %result_temp18, i32 0, i32 1
  %val23 = load i32, ptr %val_ptr22, align 4
  %unwrap_result24 = select i1 %is_success21, i32 %val23, i32 0
  store i32 %unwrap_result24, ptr %student4_avg, align 4
  %0 = load i32, ptr %student1_avg, align 4
  %1 = load i32, ptr %student2_avg, align 4
  %addtmp = add i32 %0, %1
  %2 = load i32, ptr %student3_avg, align 4
  %addtmp25 = add i32 %addtmp, %2
  %3 = load i32, ptr %student4_avg, align 4
  %addtmp26 = add i32 %addtmp25, %3
  store i32 %addtmp26, ptr %total, align 4
  %4 = load i32, ptr %total, align 4
  %5 = sext i32 %4 to i64
  %divtmp = sdiv i64 %5, 4
  store i64 %divtmp, ptr %class_avg, align 4
  %6 = load i32, ptr %class_avg, align 4
  %auto_wrap_result = alloca %result_int32, align 8
  %err_ptr27 = getelementptr inbounds %result_int32, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr27, align 1
  %val_ptr28 = getelementptr inbounds %result_int32, ptr %auto_wrap_result, i32 0, i32 1
  store i32 %6, ptr %val_ptr28, align 4
  %result_val = load %result_int32, ptr %auto_wrap_result, align 4
  ret %result_int32 %result_val
}

define internal %result_int8 @test_grade_boundaries() {
entry:
  %t5 = alloca i8, align 1
  %t4 = alloca i8, align 1
  %t3 = alloca i8, align 1
  %t2 = alloca i8, align 1
  %t1 = alloca i8, align 1
  %calltmp = call %result_int8 @validate_grade(i8 0)
  %result_temp = alloca %result_int8, align 8
  store %result_int8 %calltmp, ptr %result_temp, align 1
  %err_ptr = getelementptr inbounds %result_int8, ptr %result_temp, i32 0, i32 0
  %err = load i8, ptr %err_ptr, align 1
  %is_success = icmp eq i8 %err, 0
  %val_ptr = getelementptr inbounds %result_int8, ptr %result_temp, i32 0, i32 1
  %val = load i8, ptr %val_ptr, align 1
  %unwrap_result = select i1 %is_success, i8 %val, i8 -1
  store i8 %unwrap_result, ptr %t1, align 1
  %calltmp1 = call %result_int8 @validate_grade(i8 100)
  %result_temp2 = alloca %result_int8, align 8
  store %result_int8 %calltmp1, ptr %result_temp2, align 1
  %err_ptr3 = getelementptr inbounds %result_int8, ptr %result_temp2, i32 0, i32 0
  %err4 = load i8, ptr %err_ptr3, align 1
  %is_success5 = icmp eq i8 %err4, 0
  %val_ptr6 = getelementptr inbounds %result_int8, ptr %result_temp2, i32 0, i32 1
  %val7 = load i8, ptr %val_ptr6, align 1
  %unwrap_result8 = select i1 %is_success5, i8 %val7, i8 -1
  store i8 %unwrap_result8, ptr %t2, align 1
  %calltmp9 = call %result_int8 @validate_grade(i8 50)
  %result_temp10 = alloca %result_int8, align 8
  store %result_int8 %calltmp9, ptr %result_temp10, align 1
  %err_ptr11 = getelementptr inbounds %result_int8, ptr %result_temp10, i32 0, i32 0
  %err12 = load i8, ptr %err_ptr11, align 1
  %is_success13 = icmp eq i8 %err12, 0
  %val_ptr14 = getelementptr inbounds %result_int8, ptr %result_temp10, i32 0, i32 1
  %val15 = load i8, ptr %val_ptr14, align 1
  %unwrap_result16 = select i1 %is_success13, i8 %val15, i8 -1
  store i8 %unwrap_result16, ptr %t3, align 1
  %calltmp17 = call %result_int8 @validate_grade(i8 -10)
  %result_temp18 = alloca %result_int8, align 8
  store %result_int8 %calltmp17, ptr %result_temp18, align 1
  %err_ptr19 = getelementptr inbounds %result_int8, ptr %result_temp18, i32 0, i32 0
  %err20 = load i8, ptr %err_ptr19, align 1
  %is_success21 = icmp eq i8 %err20, 0
  %val_ptr22 = getelementptr inbounds %result_int8, ptr %result_temp18, i32 0, i32 1
  %val23 = load i8, ptr %val_ptr22, align 1
  %unwrap_result24 = select i1 %is_success21, i8 %val23, i8 -1
  store i8 %unwrap_result24, ptr %t4, align 1
  %calltmp25 = call %result_int8 @validate_grade(i8 110)
  %result_temp26 = alloca %result_int8, align 8
  store %result_int8 %calltmp25, ptr %result_temp26, align 1
  %err_ptr27 = getelementptr inbounds %result_int8, ptr %result_temp26, i32 0, i32 0
  %err28 = load i8, ptr %err_ptr27, align 1
  %is_success29 = icmp eq i8 %err28, 0
  %val_ptr30 = getelementptr inbounds %result_int8, ptr %result_temp26, i32 0, i32 1
  %val31 = load i8, ptr %val_ptr30, align 1
  %unwrap_result32 = select i1 %is_success29, i8 %val31, i8 -1
  store i8 %unwrap_result32, ptr %t5, align 1
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr33 = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr33, align 1
  %val_ptr34 = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 0, ptr %val_ptr34, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val
}

define internal %result_int8 @test_letter_grades() {
entry:
  %f_grade = alloca i8, align 1
  %d_grade = alloca i8, align 1
  %c_grade = alloca i8, align 1
  %b_grade = alloca i8, align 1
  %a_grade = alloca i8, align 1
  %calltmp = call %result_int8 @get_letter_grade(i8 95)
  %result_temp = alloca %result_int8, align 8
  store %result_int8 %calltmp, ptr %result_temp, align 1
  %err_ptr = getelementptr inbounds %result_int8, ptr %result_temp, i32 0, i32 0
  %err = load i8, ptr %err_ptr, align 1
  %is_success = icmp eq i8 %err, 0
  %val_ptr = getelementptr inbounds %result_int8, ptr %result_temp, i32 0, i32 1
  %val = load i8, ptr %val_ptr, align 1
  %unwrap_result = select i1 %is_success, i8 %val, i8 0
  store i8 %unwrap_result, ptr %a_grade, align 1
  %calltmp1 = call %result_int8 @get_letter_grade(i8 85)
  %result_temp2 = alloca %result_int8, align 8
  store %result_int8 %calltmp1, ptr %result_temp2, align 1
  %err_ptr3 = getelementptr inbounds %result_int8, ptr %result_temp2, i32 0, i32 0
  %err4 = load i8, ptr %err_ptr3, align 1
  %is_success5 = icmp eq i8 %err4, 0
  %val_ptr6 = getelementptr inbounds %result_int8, ptr %result_temp2, i32 0, i32 1
  %val7 = load i8, ptr %val_ptr6, align 1
  %unwrap_result8 = select i1 %is_success5, i8 %val7, i8 0
  store i8 %unwrap_result8, ptr %b_grade, align 1
  %calltmp9 = call %result_int8 @get_letter_grade(i8 75)
  %result_temp10 = alloca %result_int8, align 8
  store %result_int8 %calltmp9, ptr %result_temp10, align 1
  %err_ptr11 = getelementptr inbounds %result_int8, ptr %result_temp10, i32 0, i32 0
  %err12 = load i8, ptr %err_ptr11, align 1
  %is_success13 = icmp eq i8 %err12, 0
  %val_ptr14 = getelementptr inbounds %result_int8, ptr %result_temp10, i32 0, i32 1
  %val15 = load i8, ptr %val_ptr14, align 1
  %unwrap_result16 = select i1 %is_success13, i8 %val15, i8 0
  store i8 %unwrap_result16, ptr %c_grade, align 1
  %calltmp17 = call %result_int8 @get_letter_grade(i8 65)
  %result_temp18 = alloca %result_int8, align 8
  store %result_int8 %calltmp17, ptr %result_temp18, align 1
  %err_ptr19 = getelementptr inbounds %result_int8, ptr %result_temp18, i32 0, i32 0
  %err20 = load i8, ptr %err_ptr19, align 1
  %is_success21 = icmp eq i8 %err20, 0
  %val_ptr22 = getelementptr inbounds %result_int8, ptr %result_temp18, i32 0, i32 1
  %val23 = load i8, ptr %val_ptr22, align 1
  %unwrap_result24 = select i1 %is_success21, i8 %val23, i8 0
  store i8 %unwrap_result24, ptr %d_grade, align 1
  %calltmp25 = call %result_int8 @get_letter_grade(i8 45)
  %result_temp26 = alloca %result_int8, align 8
  store %result_int8 %calltmp25, ptr %result_temp26, align 1
  %err_ptr27 = getelementptr inbounds %result_int8, ptr %result_temp26, i32 0, i32 0
  %err28 = load i8, ptr %err_ptr27, align 1
  %is_success29 = icmp eq i8 %err28, 0
  %val_ptr30 = getelementptr inbounds %result_int8, ptr %result_temp26, i32 0, i32 1
  %val31 = load i8, ptr %val_ptr30, align 1
  %unwrap_result32 = select i1 %is_success29, i8 %val31, i8 0
  store i8 %unwrap_result32, ptr %f_grade, align 1
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr33 = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr33, align 1
  %val_ptr34 = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 0, ptr %val_ptr34, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val
}

define internal %result_int32 @__user_main() {
entry:
  %class_average = alloca i32, align 4
  call void @puts(ptr @0)
  %calltmp = call %result_int8 @test_grade_boundaries()
  %result_temp = alloca %result_int8, align 8
  store %result_int8 %calltmp, ptr %result_temp, align 1
  %err_ptr = getelementptr inbounds %result_int8, ptr %result_temp, i32 0, i32 0
  %err = load i8, ptr %err_ptr, align 1
  %is_success = icmp eq i8 %err, 0
  %val_ptr = getelementptr inbounds %result_int8, ptr %result_temp, i32 0, i32 1
  %val = load i8, ptr %val_ptr, align 1
  %unwrap_result = select i1 %is_success, i8 %val, i8 -1
  call void @puts(ptr @1)
  %calltmp1 = call %result_int8 @test_letter_grades()
  %result_temp2 = alloca %result_int8, align 8
  store %result_int8 %calltmp1, ptr %result_temp2, align 1
  %err_ptr3 = getelementptr inbounds %result_int8, ptr %result_temp2, i32 0, i32 0
  %err4 = load i8, ptr %err_ptr3, align 1
  %is_success5 = icmp eq i8 %err4, 0
  %val_ptr6 = getelementptr inbounds %result_int8, ptr %result_temp2, i32 0, i32 1
  %val7 = load i8, ptr %val_ptr6, align 1
  %unwrap_result8 = select i1 %is_success5, i8 %val7, i8 -1
  call void @puts(ptr @2)
  %calltmp9 = call %result_int32 @calculate_class_stats()
  %result_temp10 = alloca %result_int32, align 8
  store %result_int32 %calltmp9, ptr %result_temp10, align 4
  %err_ptr11 = getelementptr inbounds %result_int32, ptr %result_temp10, i32 0, i32 0
  %err12 = load i8, ptr %err_ptr11, align 1
  %is_success13 = icmp eq i8 %err12, 0
  %val_ptr14 = getelementptr inbounds %result_int32, ptr %result_temp10, i32 0, i32 1
  %val15 = load i32, ptr %val_ptr14, align 4
  %unwrap_result16 = select i1 %is_success13, i32 %val15, i32 0
  store i32 %unwrap_result16, ptr %class_average, align 4
  call void @puts(ptr @3)
  call void @puts(ptr @4)
  %auto_wrap_result = alloca %result_int32, align 8
  %err_ptr17 = getelementptr inbounds %result_int32, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr17, align 1
  %val_ptr18 = getelementptr inbounds %result_int32, ptr %auto_wrap_result, i32 0, i32 1
  store i32 0, ptr %val_ptr18, align 4
  %result_val = load %result_int32, ptr %auto_wrap_result, align 4
  ret %result_int32 %result_val
}

define i64 @main() {
entry:
  call void @__aria_module_init()
  %0 = call %result_int32 @__user_main()
  ret i64 0
}
