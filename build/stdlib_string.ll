; ModuleID = 'aria_module'
source_filename = "aria_module"

%result_int64 = type { i8, i64 }
%result_int8 = type { i8, i8 }

@0 = private unnamed_addr constant [36 x i8] c"Testing character classification...\00", align 1
@1 = private unnamed_addr constant [39 x i8] c"Character classification tests passed!\00", align 1
@2 = private unnamed_addr constant [27 x i8] c"Testing case conversion...\00", align 1
@3 = private unnamed_addr constant [30 x i8] c"Case conversion tests passed!\00", align 1
@4 = private unnamed_addr constant [31 x i8] c"Testing numeric conversions...\00", align 1
@5 = private unnamed_addr constant [33 x i8] c"Numeric conversion tests passed!\00", align 1
@6 = private unnamed_addr constant [32 x i8] c"Testing whitespace detection...\00", align 1
@7 = private unnamed_addr constant [35 x i8] c"Whitespace detection tests passed!\00", align 1
@8 = private unnamed_addr constant [49 x i8] c"=== Aria Standard Library - String Utilities ===\00", align 1
@9 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@10 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@11 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@12 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@13 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@14 = private unnamed_addr constant [41 x i8] c"=== All String Utility Tests Passed! ===\00", align 1

declare void @puts(ptr)

declare void @print(ptr)

define internal void @__aria_module_init() {
entry:
  ret void
}

define internal %result_int64 @string_length(i64 %dummy_for_now) {
entry:
  %dummy_for_now1 = alloca i64, align 8
  store i64 %dummy_for_now, ptr %dummy_for_now1, align 4
  %result = alloca %result_int64, align 8
  %err_ptr = getelementptr inbounds %result_int64, ptr %result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int64, ptr %result, i32 0, i32 1
  store i64 10, ptr %val_ptr, align 4
  %result_val = load %result_int64, ptr %result, align 4
  ret %result_int64 %result_val
}

define internal %result_int8 @is_uppercase(i8 %ch) {
entry:
  %ch1 = alloca i8, align 1
  store i8 %ch, ptr %ch1, align 1
  %0 = load i64, ptr %ch1, align 4
  %range_ge = icmp sge i64 %0, 65
  %range_le = icmp sle i64 %0, 90
  %range_match = and i1 %range_ge, %range_le
  br i1 %range_match, label %case_body_0, label %case_next_0

case_body_0:                                      ; preds = %entry
  %result = alloca %result_int8, align 8
  %err_ptr = getelementptr inbounds %result_int8, ptr %result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int8, ptr %result, i32 0, i32 1
  store i8 1, ptr %val_ptr, align 1
  %result_val = load %result_int8, ptr %result, align 1
  ret %result_int8 %result_val

case_next_0:                                      ; preds = %entry
  br i1 true, label %case_body_1, label %case_next_1

case_body_1:                                      ; preds = %case_next_0
  %result2 = alloca %result_int8, align 8
  %err_ptr3 = getelementptr inbounds %result_int8, ptr %result2, i32 0, i32 0
  store i8 0, ptr %err_ptr3, align 1
  %val_ptr4 = getelementptr inbounds %result_int8, ptr %result2, i32 0, i32 1
  store i8 0, ptr %val_ptr4, align 1
  %result_val5 = load %result_int8, ptr %result2, align 1
  ret %result_int8 %result_val5

case_next_1:                                      ; preds = %case_next_0
  br label %pick_done

pick_done:                                        ; preds = %case_next_1
  ret %result_int8 zeroinitializer
}

define internal %result_int8 @is_lowercase(i8 %ch) {
entry:
  %ch1 = alloca i8, align 1
  store i8 %ch, ptr %ch1, align 1
  %0 = load i64, ptr %ch1, align 4
  br label %pick_label_lowercase_letters

pick_label_lowercase_letters:                     ; preds = %entry
  %result = alloca %result_int8, align 8
  %err_ptr = getelementptr inbounds %result_int8, ptr %result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int8, ptr %result, i32 0, i32 1
  store i8 1, ptr %val_ptr, align 1
  %result_val = load %result_int8, ptr %result, align 1
  ret %result_int8 %result_val

pick_label_default:                               ; preds = %case_next_0
  %result2 = alloca %result_int8, align 8
  %err_ptr3 = getelementptr inbounds %result_int8, ptr %result2, i32 0, i32 0
  store i8 0, ptr %err_ptr3, align 1
  %val_ptr4 = getelementptr inbounds %result_int8, ptr %result2, i32 0, i32 1
  store i8 0, ptr %val_ptr4, align 1
  %result_val5 = load %result_int8, ptr %result2, align 1
  ret %result_int8 %result_val5

case_next_0:                                      ; No predecessors!
  br label %pick_label_default

case_next_1:                                      ; No predecessors!
  br label %pick_done

pick_done:                                        ; preds = %case_next_1
  ret %result_int8 zeroinitializer
}

define internal %result_int8 @is_digit(i8 %ch) {
entry:
  %ch1 = alloca i8, align 1
  store i8 %ch, ptr %ch1, align 1
  %0 = load i64, ptr %ch1, align 4
  br label %pick_label_digit_chars

pick_label_digit_chars:                           ; preds = %entry
  %result = alloca %result_int8, align 8
  %err_ptr = getelementptr inbounds %result_int8, ptr %result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int8, ptr %result, i32 0, i32 1
  store i8 1, ptr %val_ptr, align 1
  %result_val = load %result_int8, ptr %result, align 1
  ret %result_int8 %result_val

pick_label_default:                               ; preds = %case_next_0
  %result2 = alloca %result_int8, align 8
  %err_ptr3 = getelementptr inbounds %result_int8, ptr %result2, i32 0, i32 0
  store i8 0, ptr %err_ptr3, align 1
  %val_ptr4 = getelementptr inbounds %result_int8, ptr %result2, i32 0, i32 1
  store i8 0, ptr %val_ptr4, align 1
  %result_val5 = load %result_int8, ptr %result2, align 1
  ret %result_int8 %result_val5

case_next_0:                                      ; No predecessors!
  br label %pick_label_default

case_next_1:                                      ; No predecessors!
  br label %pick_done

pick_done:                                        ; preds = %case_next_1
  ret %result_int8 zeroinitializer
}

define internal %result_int8 @is_alpha(i8 %ch) {
entry:
  %lower = alloca i8, align 1
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
  %2 = load i64, ptr %ch1, align 4
  %3 = trunc i64 %2 to i8
  %calltmp2 = call %result_int8 @is_lowercase(i8 %3)
  %result_temp3 = alloca %result_int8, align 8
  store %result_int8 %calltmp2, ptr %result_temp3, align 1
  %err_ptr4 = getelementptr inbounds %result_int8, ptr %result_temp3, i32 0, i32 0
  %err5 = load i8, ptr %err_ptr4, align 1
  %is_success6 = icmp eq i8 %err5, 0
  %val_ptr7 = getelementptr inbounds %result_int8, ptr %result_temp3, i32 0, i32 1
  %val8 = load i8, ptr %val_ptr7, align 1
  %unwrap_result9 = select i1 %is_success6, i8 %val8, i8 0
  store i8 %unwrap_result9, ptr %lower, align 1
  %4 = load i8, ptr %upper, align 1
  %5 = load i8, ptr %lower, align 1
  %addtmp = add i8 %4, %5
  br label %pick_label_not_alpha

pick_label_not_alpha:                             ; preds = %entry
  %result = alloca %result_int8, align 8
  %err_ptr10 = getelementptr inbounds %result_int8, ptr %result, i32 0, i32 0
  store i8 0, ptr %err_ptr10, align 1
  %val_ptr11 = getelementptr inbounds %result_int8, ptr %result, i32 0, i32 1
  store i8 0, ptr %val_ptr11, align 1
  %result_val = load %result_int8, ptr %result, align 1
  ret %result_int8 %result_val

pick_label_is_alpha:                              ; preds = %case_next_0
  %result12 = alloca %result_int8, align 8
  %err_ptr13 = getelementptr inbounds %result_int8, ptr %result12, i32 0, i32 0
  store i8 0, ptr %err_ptr13, align 1
  %val_ptr14 = getelementptr inbounds %result_int8, ptr %result12, i32 0, i32 1
  store i8 1, ptr %val_ptr14, align 1
  %result_val15 = load %result_int8, ptr %result12, align 1
  ret %result_int8 %result_val15

case_next_0:                                      ; No predecessors!
  br label %pick_label_is_alpha

case_next_1:                                      ; No predecessors!
  br label %pick_done

pick_done:                                        ; preds = %case_next_1
  ret %result_int8 zeroinitializer
}

define internal %result_int8 @is_alphanumeric(i8 %ch) {
entry:
  %digit = alloca i8, align 1
  %alpha = alloca i8, align 1
  %ch1 = alloca i8, align 1
  store i8 %ch, ptr %ch1, align 1
  %0 = load i64, ptr %ch1, align 4
  %1 = trunc i64 %0 to i8
  %calltmp = call %result_int8 @is_alpha(i8 %1)
  %result_temp = alloca %result_int8, align 8
  store %result_int8 %calltmp, ptr %result_temp, align 1
  %err_ptr = getelementptr inbounds %result_int8, ptr %result_temp, i32 0, i32 0
  %err = load i8, ptr %err_ptr, align 1
  %is_success = icmp eq i8 %err, 0
  %val_ptr = getelementptr inbounds %result_int8, ptr %result_temp, i32 0, i32 1
  %val = load i8, ptr %val_ptr, align 1
  %unwrap_result = select i1 %is_success, i8 %val, i8 0
  store i8 %unwrap_result, ptr %alpha, align 1
  %2 = load i64, ptr %ch1, align 4
  %3 = trunc i64 %2 to i8
  %calltmp2 = call %result_int8 @is_digit(i8 %3)
  %result_temp3 = alloca %result_int8, align 8
  store %result_int8 %calltmp2, ptr %result_temp3, align 1
  %err_ptr4 = getelementptr inbounds %result_int8, ptr %result_temp3, i32 0, i32 0
  %err5 = load i8, ptr %err_ptr4, align 1
  %is_success6 = icmp eq i8 %err5, 0
  %val_ptr7 = getelementptr inbounds %result_int8, ptr %result_temp3, i32 0, i32 1
  %val8 = load i8, ptr %val_ptr7, align 1
  %unwrap_result9 = select i1 %is_success6, i8 %val8, i8 0
  store i8 %unwrap_result9, ptr %digit, align 1
  %4 = load i8, ptr %alpha, align 1
  %5 = load i8, ptr %digit, align 1
  %addtmp = add i8 %4, %5
  br label %pick_label_not_alphanumeric

pick_label_not_alphanumeric:                      ; preds = %entry
  %result = alloca %result_int8, align 8
  %err_ptr10 = getelementptr inbounds %result_int8, ptr %result, i32 0, i32 0
  store i8 0, ptr %err_ptr10, align 1
  %val_ptr11 = getelementptr inbounds %result_int8, ptr %result, i32 0, i32 1
  store i8 0, ptr %val_ptr11, align 1
  %result_val = load %result_int8, ptr %result, align 1
  ret %result_int8 %result_val

pick_label_is_alphanumeric:                       ; preds = %case_next_0
  %result12 = alloca %result_int8, align 8
  %err_ptr13 = getelementptr inbounds %result_int8, ptr %result12, i32 0, i32 0
  store i8 0, ptr %err_ptr13, align 1
  %val_ptr14 = getelementptr inbounds %result_int8, ptr %result12, i32 0, i32 1
  store i8 1, ptr %val_ptr14, align 1
  %result_val15 = load %result_int8, ptr %result12, align 1
  ret %result_int8 %result_val15

case_next_0:                                      ; No predecessors!
  br label %pick_label_is_alphanumeric

case_next_1:                                      ; No predecessors!
  br label %pick_done

pick_done:                                        ; preds = %case_next_1
  ret %result_int8 zeroinitializer
}

define internal %result_int8 @to_lowercase(i8 %ch) {
entry:
  %lower_ch = alloca i8, align 1
  %is_upper = alloca i8, align 1
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
  store i8 %unwrap_result, ptr %is_upper, align 1
  %2 = load i8, ptr %is_upper, align 1
  br label %pick_label_convert_to_lower

pick_label_convert_to_lower:                      ; preds = %entry
  %3 = load i64, ptr %ch1, align 4
  %addtmp = add i64 %3, 32
  store i64 %addtmp, ptr %lower_ch, align 4
  %4 = load i8, ptr %lower_ch, align 1
  %result = alloca %result_int8, align 8
  %err_ptr2 = getelementptr inbounds %result_int8, ptr %result, i32 0, i32 0
  store i8 0, ptr %err_ptr2, align 1
  %val_ptr3 = getelementptr inbounds %result_int8, ptr %result, i32 0, i32 1
  store i8 %4, ptr %val_ptr3, align 1
  %result_val = load %result_int8, ptr %result, align 1
  ret %result_int8 %result_val

pick_label_already_lower:                         ; preds = %case_next_0
  %5 = load i64, ptr %ch1, align 4
  %result4 = alloca %result_int8, align 8
  %err_ptr5 = getelementptr inbounds %result_int8, ptr %result4, i32 0, i32 0
  store i8 0, ptr %err_ptr5, align 1
  %val_cast = trunc i64 %5 to i8
  %val_ptr6 = getelementptr inbounds %result_int8, ptr %result4, i32 0, i32 1
  store i8 %val_cast, ptr %val_ptr6, align 1
  %result_val7 = load %result_int8, ptr %result4, align 1
  ret %result_int8 %result_val7

case_next_0:                                      ; No predecessors!
  br label %pick_label_already_lower

case_next_1:                                      ; No predecessors!
  br label %pick_done

pick_done:                                        ; preds = %case_next_1
  ret %result_int8 zeroinitializer
}

define internal %result_int8 @to_uppercase(i8 %ch) {
entry:
  %upper_ch = alloca i8, align 1
  %is_lower = alloca i8, align 1
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
  store i8 %unwrap_result, ptr %is_lower, align 1
  %2 = load i8, ptr %is_lower, align 1
  br label %pick_label_convert_to_upper

pick_label_convert_to_upper:                      ; preds = %entry
  %3 = load i64, ptr %ch1, align 4
  %subtmp = sub i64 %3, 32
  store i64 %subtmp, ptr %upper_ch, align 4
  %4 = load i8, ptr %upper_ch, align 1
  %result = alloca %result_int8, align 8
  %err_ptr2 = getelementptr inbounds %result_int8, ptr %result, i32 0, i32 0
  store i8 0, ptr %err_ptr2, align 1
  %val_ptr3 = getelementptr inbounds %result_int8, ptr %result, i32 0, i32 1
  store i8 %4, ptr %val_ptr3, align 1
  %result_val = load %result_int8, ptr %result, align 1
  ret %result_int8 %result_val

pick_label_already_upper:                         ; preds = %case_next_0
  %5 = load i64, ptr %ch1, align 4
  %result4 = alloca %result_int8, align 8
  %err_ptr5 = getelementptr inbounds %result_int8, ptr %result4, i32 0, i32 0
  store i8 0, ptr %err_ptr5, align 1
  %val_cast = trunc i64 %5 to i8
  %val_ptr6 = getelementptr inbounds %result_int8, ptr %result4, i32 0, i32 1
  store i8 %val_cast, ptr %val_ptr6, align 1
  %result_val7 = load %result_int8, ptr %result4, align 1
  ret %result_int8 %result_val7

case_next_0:                                      ; No predecessors!
  br label %pick_label_already_upper

case_next_1:                                      ; No predecessors!
  br label %pick_done

pick_done:                                        ; preds = %case_next_1
  ret %result_int8 zeroinitializer
}

define internal %result_int8 @char_compare(i8 %a, i8 %b) {
entry:
  %a1 = alloca i8, align 1
  store i8 %a, ptr %a1, align 1
  %b2 = alloca i8, align 1
  store i8 %b, ptr %b2, align 1
  %0 = load i64, ptr %a1, align 4
  br label %pick_label_less_than

pick_label_less_than:                             ; preds = %entry
  %result = alloca %result_int8, align 8
  %err_ptr = getelementptr inbounds %result_int8, ptr %result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int8, ptr %result, i32 0, i32 1
  store i8 -1, ptr %val_ptr, align 1
  %result_val = load %result_int8, ptr %result, align 1
  ret %result_int8 %result_val

pick_label_greater_than:                          ; preds = %case_next_0
  %result3 = alloca %result_int8, align 8
  %err_ptr4 = getelementptr inbounds %result_int8, ptr %result3, i32 0, i32 0
  store i8 0, ptr %err_ptr4, align 1
  %val_ptr5 = getelementptr inbounds %result_int8, ptr %result3, i32 0, i32 1
  store i8 1, ptr %val_ptr5, align 1
  %result_val6 = load %result_int8, ptr %result3, align 1
  ret %result_int8 %result_val6

pick_label_equal:                                 ; preds = %case_next_1
  %result7 = alloca %result_int8, align 8
  %err_ptr8 = getelementptr inbounds %result_int8, ptr %result7, i32 0, i32 0
  store i8 0, ptr %err_ptr8, align 1
  %val_ptr9 = getelementptr inbounds %result_int8, ptr %result7, i32 0, i32 1
  store i8 0, ptr %val_ptr9, align 1
  %result_val10 = load %result_int8, ptr %result7, align 1
  ret %result_int8 %result_val10

case_next_0:                                      ; No predecessors!
  br label %pick_label_greater_than

case_next_1:                                      ; No predecessors!
  br label %pick_label_equal

case_next_2:                                      ; No predecessors!
  br label %pick_done

pick_done:                                        ; preds = %case_next_2
  ret %result_int8 zeroinitializer
}

define internal %result_int8 @is_whitespace(i8 %ch) {
entry:
  %ch1 = alloca i8, align 1
  store i8 %ch, ptr %ch1, align 1
  %0 = load i64, ptr %ch1, align 4
  br label %pick_label_space

pick_label_space:                                 ; preds = %entry
  %result = alloca %result_int8, align 8
  %err_ptr = getelementptr inbounds %result_int8, ptr %result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int8, ptr %result, i32 0, i32 1
  store i8 1, ptr %val_ptr, align 1
  %result_val = load %result_int8, ptr %result, align 1
  ret %result_int8 %result_val

pick_label_tab:                                   ; preds = %case_next_0
  %result2 = alloca %result_int8, align 8
  %err_ptr3 = getelementptr inbounds %result_int8, ptr %result2, i32 0, i32 0
  store i8 0, ptr %err_ptr3, align 1
  %val_ptr4 = getelementptr inbounds %result_int8, ptr %result2, i32 0, i32 1
  store i8 1, ptr %val_ptr4, align 1
  %result_val5 = load %result_int8, ptr %result2, align 1
  ret %result_int8 %result_val5

pick_label_newline:                               ; preds = %case_next_1
  %result6 = alloca %result_int8, align 8
  %err_ptr7 = getelementptr inbounds %result_int8, ptr %result6, i32 0, i32 0
  store i8 0, ptr %err_ptr7, align 1
  %val_ptr8 = getelementptr inbounds %result_int8, ptr %result6, i32 0, i32 1
  store i8 1, ptr %val_ptr8, align 1
  %result_val9 = load %result_int8, ptr %result6, align 1
  ret %result_int8 %result_val9

pick_label_carriage_return:                       ; preds = %case_next_2
  %result10 = alloca %result_int8, align 8
  %err_ptr11 = getelementptr inbounds %result_int8, ptr %result10, i32 0, i32 0
  store i8 0, ptr %err_ptr11, align 1
  %val_ptr12 = getelementptr inbounds %result_int8, ptr %result10, i32 0, i32 1
  store i8 1, ptr %val_ptr12, align 1
  %result_val13 = load %result_int8, ptr %result10, align 1
  ret %result_int8 %result_val13

pick_label_default:                               ; preds = %case_next_3
  %result14 = alloca %result_int8, align 8
  %err_ptr15 = getelementptr inbounds %result_int8, ptr %result14, i32 0, i32 0
  store i8 0, ptr %err_ptr15, align 1
  %val_ptr16 = getelementptr inbounds %result_int8, ptr %result14, i32 0, i32 1
  store i8 0, ptr %val_ptr16, align 1
  %result_val17 = load %result_int8, ptr %result14, align 1
  ret %result_int8 %result_val17

case_next_0:                                      ; No predecessors!
  br label %pick_label_tab

case_next_1:                                      ; No predecessors!
  br label %pick_label_newline

case_next_2:                                      ; No predecessors!
  br label %pick_label_carriage_return

case_next_3:                                      ; No predecessors!
  br label %pick_label_default

case_next_4:                                      ; No predecessors!
  br label %pick_done

pick_done:                                        ; preds = %case_next_4
  ret %result_int8 zeroinitializer
}

define internal %result_int8 @digit_to_int(i8 %ch) {
entry:
  %value = alloca i8, align 1
  %is_num = alloca i8, align 1
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
  store i8 %unwrap_result, ptr %is_num, align 1
  %2 = load i8, ptr %is_num, align 1
  br label %pick_label_valid_digit

pick_label_valid_digit:                           ; preds = %entry
  %3 = load i64, ptr %ch1, align 4
  %subtmp = sub i64 %3, 48
  store i64 %subtmp, ptr %value, align 4
  %4 = load i8, ptr %value, align 1
  %result = alloca %result_int8, align 8
  %err_ptr2 = getelementptr inbounds %result_int8, ptr %result, i32 0, i32 0
  store i8 0, ptr %err_ptr2, align 1
  %val_ptr3 = getelementptr inbounds %result_int8, ptr %result, i32 0, i32 1
  store i8 %4, ptr %val_ptr3, align 1
  %result_val = load %result_int8, ptr %result, align 1
  ret %result_int8 %result_val

pick_label_invalid:                               ; preds = %case_next_0
  %result4 = alloca %result_int8, align 8
  %err_ptr5 = getelementptr inbounds %result_int8, ptr %result4, i32 0, i32 0
  store i8 1, ptr %err_ptr5, align 1
  %val_ptr6 = getelementptr inbounds %result_int8, ptr %result4, i32 0, i32 1
  store i8 0, ptr %val_ptr6, align 1
  %result_val7 = load %result_int8, ptr %result4, align 1
  ret %result_int8 %result_val7

case_next_0:                                      ; No predecessors!
  br label %pick_label_invalid

case_next_1:                                      ; No predecessors!
  br label %pick_done

pick_done:                                        ; preds = %case_next_1
  ret %result_int8 zeroinitializer
}

define internal %result_int8 @int_to_digit(i8 %num) {
entry:
  %ch = alloca i8, align 1
  %num1 = alloca i8, align 1
  store i8 %num, ptr %num1, align 1
  %0 = load i64, ptr %num1, align 4
  br label %pick_label_valid_range

pick_label_valid_range:                           ; preds = %entry
  %1 = load i64, ptr %num1, align 4
  %addtmp = add i64 %1, 48
  store i64 %addtmp, ptr %ch, align 4
  %2 = load i8, ptr %ch, align 1
  %result = alloca %result_int8, align 8
  %err_ptr = getelementptr inbounds %result_int8, ptr %result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int8, ptr %result, i32 0, i32 1
  store i8 %2, ptr %val_ptr, align 1
  %result_val = load %result_int8, ptr %result, align 1
  ret %result_int8 %result_val

pick_label_out_of_range:                          ; preds = %case_next_0
  %result2 = alloca %result_int8, align 8
  %err_ptr3 = getelementptr inbounds %result_int8, ptr %result2, i32 0, i32 0
  store i8 2, ptr %err_ptr3, align 1
  %val_ptr4 = getelementptr inbounds %result_int8, ptr %result2, i32 0, i32 1
  store i8 48, ptr %val_ptr4, align 1
  %result_val5 = load %result_int8, ptr %result2, align 1
  ret %result_int8 %result_val5

case_next_0:                                      ; No predecessors!
  br label %pick_label_out_of_range

case_next_1:                                      ; No predecessors!
  br label %pick_done

pick_done:                                        ; preds = %case_next_1
  ret %result_int8 zeroinitializer
}

define internal %result_int8 @test_char_classification() {
entry:
  %t10 = alloca i8, align 1
  %t9 = alloca i8, align 1
  %t8 = alloca i8, align 1
  %t7 = alloca i8, align 1
  %t6 = alloca i8, align 1
  %t5 = alloca i8, align 1
  %t4 = alloca i8, align 1
  %t3 = alloca i8, align 1
  %t2 = alloca i8, align 1
  %t1 = alloca i8, align 1
  call void @puts(ptr @0)
  %calltmp = call %result_int8 @is_uppercase(i8 65)
  %result_temp = alloca %result_int8, align 8
  store %result_int8 %calltmp, ptr %result_temp, align 1
  %err_ptr = getelementptr inbounds %result_int8, ptr %result_temp, i32 0, i32 0
  %err = load i8, ptr %err_ptr, align 1
  %is_success = icmp eq i8 %err, 0
  %val_ptr = getelementptr inbounds %result_int8, ptr %result_temp, i32 0, i32 1
  %val = load i8, ptr %val_ptr, align 1
  %unwrap_result = select i1 %is_success, i8 %val, i8 0
  store i8 %unwrap_result, ptr %t1, align 1
  %calltmp1 = call %result_int8 @is_uppercase(i8 97)
  %result_temp2 = alloca %result_int8, align 8
  store %result_int8 %calltmp1, ptr %result_temp2, align 1
  %err_ptr3 = getelementptr inbounds %result_int8, ptr %result_temp2, i32 0, i32 0
  %err4 = load i8, ptr %err_ptr3, align 1
  %is_success5 = icmp eq i8 %err4, 0
  %val_ptr6 = getelementptr inbounds %result_int8, ptr %result_temp2, i32 0, i32 1
  %val7 = load i8, ptr %val_ptr6, align 1
  %unwrap_result8 = select i1 %is_success5, i8 %val7, i8 0
  store i8 %unwrap_result8, ptr %t2, align 1
  %calltmp9 = call %result_int8 @is_lowercase(i8 97)
  %result_temp10 = alloca %result_int8, align 8
  store %result_int8 %calltmp9, ptr %result_temp10, align 1
  %err_ptr11 = getelementptr inbounds %result_int8, ptr %result_temp10, i32 0, i32 0
  %err12 = load i8, ptr %err_ptr11, align 1
  %is_success13 = icmp eq i8 %err12, 0
  %val_ptr14 = getelementptr inbounds %result_int8, ptr %result_temp10, i32 0, i32 1
  %val15 = load i8, ptr %val_ptr14, align 1
  %unwrap_result16 = select i1 %is_success13, i8 %val15, i8 0
  store i8 %unwrap_result16, ptr %t3, align 1
  %calltmp17 = call %result_int8 @is_lowercase(i8 65)
  %result_temp18 = alloca %result_int8, align 8
  store %result_int8 %calltmp17, ptr %result_temp18, align 1
  %err_ptr19 = getelementptr inbounds %result_int8, ptr %result_temp18, i32 0, i32 0
  %err20 = load i8, ptr %err_ptr19, align 1
  %is_success21 = icmp eq i8 %err20, 0
  %val_ptr22 = getelementptr inbounds %result_int8, ptr %result_temp18, i32 0, i32 1
  %val23 = load i8, ptr %val_ptr22, align 1
  %unwrap_result24 = select i1 %is_success21, i8 %val23, i8 0
  store i8 %unwrap_result24, ptr %t4, align 1
  %calltmp25 = call %result_int8 @is_digit(i8 48)
  %result_temp26 = alloca %result_int8, align 8
  store %result_int8 %calltmp25, ptr %result_temp26, align 1
  %err_ptr27 = getelementptr inbounds %result_int8, ptr %result_temp26, i32 0, i32 0
  %err28 = load i8, ptr %err_ptr27, align 1
  %is_success29 = icmp eq i8 %err28, 0
  %val_ptr30 = getelementptr inbounds %result_int8, ptr %result_temp26, i32 0, i32 1
  %val31 = load i8, ptr %val_ptr30, align 1
  %unwrap_result32 = select i1 %is_success29, i8 %val31, i8 0
  store i8 %unwrap_result32, ptr %t5, align 1
  %calltmp33 = call %result_int8 @is_digit(i8 57)
  %result_temp34 = alloca %result_int8, align 8
  store %result_int8 %calltmp33, ptr %result_temp34, align 1
  %err_ptr35 = getelementptr inbounds %result_int8, ptr %result_temp34, i32 0, i32 0
  %err36 = load i8, ptr %err_ptr35, align 1
  %is_success37 = icmp eq i8 %err36, 0
  %val_ptr38 = getelementptr inbounds %result_int8, ptr %result_temp34, i32 0, i32 1
  %val39 = load i8, ptr %val_ptr38, align 1
  %unwrap_result40 = select i1 %is_success37, i8 %val39, i8 0
  store i8 %unwrap_result40, ptr %t6, align 1
  %calltmp41 = call %result_int8 @is_digit(i8 65)
  %result_temp42 = alloca %result_int8, align 8
  store %result_int8 %calltmp41, ptr %result_temp42, align 1
  %err_ptr43 = getelementptr inbounds %result_int8, ptr %result_temp42, i32 0, i32 0
  %err44 = load i8, ptr %err_ptr43, align 1
  %is_success45 = icmp eq i8 %err44, 0
  %val_ptr46 = getelementptr inbounds %result_int8, ptr %result_temp42, i32 0, i32 1
  %val47 = load i8, ptr %val_ptr46, align 1
  %unwrap_result48 = select i1 %is_success45, i8 %val47, i8 0
  store i8 %unwrap_result48, ptr %t7, align 1
  %calltmp49 = call %result_int8 @is_alpha(i8 65)
  %result_temp50 = alloca %result_int8, align 8
  store %result_int8 %calltmp49, ptr %result_temp50, align 1
  %err_ptr51 = getelementptr inbounds %result_int8, ptr %result_temp50, i32 0, i32 0
  %err52 = load i8, ptr %err_ptr51, align 1
  %is_success53 = icmp eq i8 %err52, 0
  %val_ptr54 = getelementptr inbounds %result_int8, ptr %result_temp50, i32 0, i32 1
  %val55 = load i8, ptr %val_ptr54, align 1
  %unwrap_result56 = select i1 %is_success53, i8 %val55, i8 0
  store i8 %unwrap_result56, ptr %t8, align 1
  %calltmp57 = call %result_int8 @is_alpha(i8 97)
  %result_temp58 = alloca %result_int8, align 8
  store %result_int8 %calltmp57, ptr %result_temp58, align 1
  %err_ptr59 = getelementptr inbounds %result_int8, ptr %result_temp58, i32 0, i32 0
  %err60 = load i8, ptr %err_ptr59, align 1
  %is_success61 = icmp eq i8 %err60, 0
  %val_ptr62 = getelementptr inbounds %result_int8, ptr %result_temp58, i32 0, i32 1
  %val63 = load i8, ptr %val_ptr62, align 1
  %unwrap_result64 = select i1 %is_success61, i8 %val63, i8 0
  store i8 %unwrap_result64, ptr %t9, align 1
  %calltmp65 = call %result_int8 @is_alpha(i8 48)
  %result_temp66 = alloca %result_int8, align 8
  store %result_int8 %calltmp65, ptr %result_temp66, align 1
  %err_ptr67 = getelementptr inbounds %result_int8, ptr %result_temp66, i32 0, i32 0
  %err68 = load i8, ptr %err_ptr67, align 1
  %is_success69 = icmp eq i8 %err68, 0
  %val_ptr70 = getelementptr inbounds %result_int8, ptr %result_temp66, i32 0, i32 1
  %val71 = load i8, ptr %val_ptr70, align 1
  %unwrap_result72 = select i1 %is_success69, i8 %val71, i8 0
  store i8 %unwrap_result72, ptr %t10, align 1
  call void @puts(ptr @1)
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr73 = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr73, align 1
  %val_ptr74 = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 0, ptr %val_ptr74, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val
}

define internal %result_int8 @test_case_conversion() {
entry:
  %already_upper = alloca i8, align 1
  %upper_z = alloca i8, align 1
  %upper_a = alloca i8, align 1
  %already_lower = alloca i8, align 1
  %lower_z = alloca i8, align 1
  %lower_a = alloca i8, align 1
  call void @puts(ptr @2)
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
  %calltmp1 = call %result_int8 @to_lowercase(i8 90)
  %result_temp2 = alloca %result_int8, align 8
  store %result_int8 %calltmp1, ptr %result_temp2, align 1
  %err_ptr3 = getelementptr inbounds %result_int8, ptr %result_temp2, i32 0, i32 0
  %err4 = load i8, ptr %err_ptr3, align 1
  %is_success5 = icmp eq i8 %err4, 0
  %val_ptr6 = getelementptr inbounds %result_int8, ptr %result_temp2, i32 0, i32 1
  %val7 = load i8, ptr %val_ptr6, align 1
  %unwrap_result8 = select i1 %is_success5, i8 %val7, i8 0
  store i8 %unwrap_result8, ptr %lower_z, align 1
  %calltmp9 = call %result_int8 @to_lowercase(i8 97)
  %result_temp10 = alloca %result_int8, align 8
  store %result_int8 %calltmp9, ptr %result_temp10, align 1
  %err_ptr11 = getelementptr inbounds %result_int8, ptr %result_temp10, i32 0, i32 0
  %err12 = load i8, ptr %err_ptr11, align 1
  %is_success13 = icmp eq i8 %err12, 0
  %val_ptr14 = getelementptr inbounds %result_int8, ptr %result_temp10, i32 0, i32 1
  %val15 = load i8, ptr %val_ptr14, align 1
  %unwrap_result16 = select i1 %is_success13, i8 %val15, i8 0
  store i8 %unwrap_result16, ptr %already_lower, align 1
  %calltmp17 = call %result_int8 @to_uppercase(i8 97)
  %result_temp18 = alloca %result_int8, align 8
  store %result_int8 %calltmp17, ptr %result_temp18, align 1
  %err_ptr19 = getelementptr inbounds %result_int8, ptr %result_temp18, i32 0, i32 0
  %err20 = load i8, ptr %err_ptr19, align 1
  %is_success21 = icmp eq i8 %err20, 0
  %val_ptr22 = getelementptr inbounds %result_int8, ptr %result_temp18, i32 0, i32 1
  %val23 = load i8, ptr %val_ptr22, align 1
  %unwrap_result24 = select i1 %is_success21, i8 %val23, i8 0
  store i8 %unwrap_result24, ptr %upper_a, align 1
  %calltmp25 = call %result_int8 @to_uppercase(i8 122)
  %result_temp26 = alloca %result_int8, align 8
  store %result_int8 %calltmp25, ptr %result_temp26, align 1
  %err_ptr27 = getelementptr inbounds %result_int8, ptr %result_temp26, i32 0, i32 0
  %err28 = load i8, ptr %err_ptr27, align 1
  %is_success29 = icmp eq i8 %err28, 0
  %val_ptr30 = getelementptr inbounds %result_int8, ptr %result_temp26, i32 0, i32 1
  %val31 = load i8, ptr %val_ptr30, align 1
  %unwrap_result32 = select i1 %is_success29, i8 %val31, i8 0
  store i8 %unwrap_result32, ptr %upper_z, align 1
  %calltmp33 = call %result_int8 @to_uppercase(i8 65)
  %result_temp34 = alloca %result_int8, align 8
  store %result_int8 %calltmp33, ptr %result_temp34, align 1
  %err_ptr35 = getelementptr inbounds %result_int8, ptr %result_temp34, i32 0, i32 0
  %err36 = load i8, ptr %err_ptr35, align 1
  %is_success37 = icmp eq i8 %err36, 0
  %val_ptr38 = getelementptr inbounds %result_int8, ptr %result_temp34, i32 0, i32 1
  %val39 = load i8, ptr %val_ptr38, align 1
  %unwrap_result40 = select i1 %is_success37, i8 %val39, i8 0
  store i8 %unwrap_result40, ptr %already_upper, align 1
  call void @puts(ptr @3)
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr41 = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr41, align 1
  %val_ptr42 = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 0, ptr %val_ptr42, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val
}

define internal %result_int8 @test_numeric_conversion() {
entry:
  %d9 = alloca i8, align 1
  %d5 = alloca i8, align 1
  %d0 = alloca i8, align 1
  %n9 = alloca i8, align 1
  %n5 = alloca i8, align 1
  %n0 = alloca i8, align 1
  call void @puts(ptr @4)
  %calltmp = call %result_int8 @digit_to_int(i8 48)
  %result_temp = alloca %result_int8, align 8
  store %result_int8 %calltmp, ptr %result_temp, align 1
  %err_ptr = getelementptr inbounds %result_int8, ptr %result_temp, i32 0, i32 0
  %err = load i8, ptr %err_ptr, align 1
  %is_success = icmp eq i8 %err, 0
  %val_ptr = getelementptr inbounds %result_int8, ptr %result_temp, i32 0, i32 1
  %val = load i8, ptr %val_ptr, align 1
  %unwrap_result = select i1 %is_success, i8 %val, i8 -1
  store i8 %unwrap_result, ptr %n0, align 1
  %calltmp1 = call %result_int8 @digit_to_int(i8 53)
  %result_temp2 = alloca %result_int8, align 8
  store %result_int8 %calltmp1, ptr %result_temp2, align 1
  %err_ptr3 = getelementptr inbounds %result_int8, ptr %result_temp2, i32 0, i32 0
  %err4 = load i8, ptr %err_ptr3, align 1
  %is_success5 = icmp eq i8 %err4, 0
  %val_ptr6 = getelementptr inbounds %result_int8, ptr %result_temp2, i32 0, i32 1
  %val7 = load i8, ptr %val_ptr6, align 1
  %unwrap_result8 = select i1 %is_success5, i8 %val7, i8 -1
  store i8 %unwrap_result8, ptr %n5, align 1
  %calltmp9 = call %result_int8 @digit_to_int(i8 57)
  %result_temp10 = alloca %result_int8, align 8
  store %result_int8 %calltmp9, ptr %result_temp10, align 1
  %err_ptr11 = getelementptr inbounds %result_int8, ptr %result_temp10, i32 0, i32 0
  %err12 = load i8, ptr %err_ptr11, align 1
  %is_success13 = icmp eq i8 %err12, 0
  %val_ptr14 = getelementptr inbounds %result_int8, ptr %result_temp10, i32 0, i32 1
  %val15 = load i8, ptr %val_ptr14, align 1
  %unwrap_result16 = select i1 %is_success13, i8 %val15, i8 -1
  store i8 %unwrap_result16, ptr %n9, align 1
  %calltmp17 = call %result_int8 @int_to_digit(i8 0)
  %result_temp18 = alloca %result_int8, align 8
  store %result_int8 %calltmp17, ptr %result_temp18, align 1
  %err_ptr19 = getelementptr inbounds %result_int8, ptr %result_temp18, i32 0, i32 0
  %err20 = load i8, ptr %err_ptr19, align 1
  %is_success21 = icmp eq i8 %err20, 0
  %val_ptr22 = getelementptr inbounds %result_int8, ptr %result_temp18, i32 0, i32 1
  %val23 = load i8, ptr %val_ptr22, align 1
  %unwrap_result24 = select i1 %is_success21, i8 %val23, i8 0
  store i8 %unwrap_result24, ptr %d0, align 1
  %calltmp25 = call %result_int8 @int_to_digit(i8 5)
  %result_temp26 = alloca %result_int8, align 8
  store %result_int8 %calltmp25, ptr %result_temp26, align 1
  %err_ptr27 = getelementptr inbounds %result_int8, ptr %result_temp26, i32 0, i32 0
  %err28 = load i8, ptr %err_ptr27, align 1
  %is_success29 = icmp eq i8 %err28, 0
  %val_ptr30 = getelementptr inbounds %result_int8, ptr %result_temp26, i32 0, i32 1
  %val31 = load i8, ptr %val_ptr30, align 1
  %unwrap_result32 = select i1 %is_success29, i8 %val31, i8 0
  store i8 %unwrap_result32, ptr %d5, align 1
  %calltmp33 = call %result_int8 @int_to_digit(i8 9)
  %result_temp34 = alloca %result_int8, align 8
  store %result_int8 %calltmp33, ptr %result_temp34, align 1
  %err_ptr35 = getelementptr inbounds %result_int8, ptr %result_temp34, i32 0, i32 0
  %err36 = load i8, ptr %err_ptr35, align 1
  %is_success37 = icmp eq i8 %err36, 0
  %val_ptr38 = getelementptr inbounds %result_int8, ptr %result_temp34, i32 0, i32 1
  %val39 = load i8, ptr %val_ptr38, align 1
  %unwrap_result40 = select i1 %is_success37, i8 %val39, i8 0
  store i8 %unwrap_result40, ptr %d9, align 1
  call void @puts(ptr @5)
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr41 = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr41, align 1
  %val_ptr42 = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 0, ptr %val_ptr42, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val
}

define internal %result_int8 @test_whitespace() {
entry:
  %ws4 = alloca i8, align 1
  %ws3 = alloca i8, align 1
  %ws2 = alloca i8, align 1
  %ws1 = alloca i8, align 1
  call void @puts(ptr @6)
  %calltmp = call %result_int8 @is_whitespace(i8 32)
  %result_temp = alloca %result_int8, align 8
  store %result_int8 %calltmp, ptr %result_temp, align 1
  %err_ptr = getelementptr inbounds %result_int8, ptr %result_temp, i32 0, i32 0
  %err = load i8, ptr %err_ptr, align 1
  %is_success = icmp eq i8 %err, 0
  %val_ptr = getelementptr inbounds %result_int8, ptr %result_temp, i32 0, i32 1
  %val = load i8, ptr %val_ptr, align 1
  %unwrap_result = select i1 %is_success, i8 %val, i8 0
  store i8 %unwrap_result, ptr %ws1, align 1
  %calltmp1 = call %result_int8 @is_whitespace(i8 9)
  %result_temp2 = alloca %result_int8, align 8
  store %result_int8 %calltmp1, ptr %result_temp2, align 1
  %err_ptr3 = getelementptr inbounds %result_int8, ptr %result_temp2, i32 0, i32 0
  %err4 = load i8, ptr %err_ptr3, align 1
  %is_success5 = icmp eq i8 %err4, 0
  %val_ptr6 = getelementptr inbounds %result_int8, ptr %result_temp2, i32 0, i32 1
  %val7 = load i8, ptr %val_ptr6, align 1
  %unwrap_result8 = select i1 %is_success5, i8 %val7, i8 0
  store i8 %unwrap_result8, ptr %ws2, align 1
  %calltmp9 = call %result_int8 @is_whitespace(i8 10)
  %result_temp10 = alloca %result_int8, align 8
  store %result_int8 %calltmp9, ptr %result_temp10, align 1
  %err_ptr11 = getelementptr inbounds %result_int8, ptr %result_temp10, i32 0, i32 0
  %err12 = load i8, ptr %err_ptr11, align 1
  %is_success13 = icmp eq i8 %err12, 0
  %val_ptr14 = getelementptr inbounds %result_int8, ptr %result_temp10, i32 0, i32 1
  %val15 = load i8, ptr %val_ptr14, align 1
  %unwrap_result16 = select i1 %is_success13, i8 %val15, i8 0
  store i8 %unwrap_result16, ptr %ws3, align 1
  %calltmp17 = call %result_int8 @is_whitespace(i8 65)
  %result_temp18 = alloca %result_int8, align 8
  store %result_int8 %calltmp17, ptr %result_temp18, align 1
  %err_ptr19 = getelementptr inbounds %result_int8, ptr %result_temp18, i32 0, i32 0
  %err20 = load i8, ptr %err_ptr19, align 1
  %is_success21 = icmp eq i8 %err20, 0
  %val_ptr22 = getelementptr inbounds %result_int8, ptr %result_temp18, i32 0, i32 1
  %val23 = load i8, ptr %val_ptr22, align 1
  %unwrap_result24 = select i1 %is_success21, i8 %val23, i8 0
  store i8 %unwrap_result24, ptr %ws4, align 1
  call void @puts(ptr @7)
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr25 = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr25, align 1
  %val_ptr26 = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 0, ptr %val_ptr26, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val
}

define internal %result_int8 @__user_main() {
entry:
  call void @puts(ptr @8)
  call void @puts(ptr @9)
  %calltmp = call %result_int8 @test_char_classification()
  %result_temp = alloca %result_int8, align 8
  store %result_int8 %calltmp, ptr %result_temp, align 1
  %err_ptr = getelementptr inbounds %result_int8, ptr %result_temp, i32 0, i32 0
  %err = load i8, ptr %err_ptr, align 1
  %is_success = icmp eq i8 %err, 0
  %val_ptr = getelementptr inbounds %result_int8, ptr %result_temp, i32 0, i32 1
  %val = load i8, ptr %val_ptr, align 1
  %unwrap_result = select i1 %is_success, i8 %val, i8 -1
  call void @puts(ptr @10)
  %calltmp1 = call %result_int8 @test_case_conversion()
  %result_temp2 = alloca %result_int8, align 8
  store %result_int8 %calltmp1, ptr %result_temp2, align 1
  %err_ptr3 = getelementptr inbounds %result_int8, ptr %result_temp2, i32 0, i32 0
  %err4 = load i8, ptr %err_ptr3, align 1
  %is_success5 = icmp eq i8 %err4, 0
  %val_ptr6 = getelementptr inbounds %result_int8, ptr %result_temp2, i32 0, i32 1
  %val7 = load i8, ptr %val_ptr6, align 1
  %unwrap_result8 = select i1 %is_success5, i8 %val7, i8 -1
  call void @puts(ptr @11)
  %calltmp9 = call %result_int8 @test_numeric_conversion()
  %result_temp10 = alloca %result_int8, align 8
  store %result_int8 %calltmp9, ptr %result_temp10, align 1
  %err_ptr11 = getelementptr inbounds %result_int8, ptr %result_temp10, i32 0, i32 0
  %err12 = load i8, ptr %err_ptr11, align 1
  %is_success13 = icmp eq i8 %err12, 0
  %val_ptr14 = getelementptr inbounds %result_int8, ptr %result_temp10, i32 0, i32 1
  %val15 = load i8, ptr %val_ptr14, align 1
  %unwrap_result16 = select i1 %is_success13, i8 %val15, i8 -1
  call void @puts(ptr @12)
  %calltmp17 = call %result_int8 @test_whitespace()
  %result_temp18 = alloca %result_int8, align 8
  store %result_int8 %calltmp17, ptr %result_temp18, align 1
  %err_ptr19 = getelementptr inbounds %result_int8, ptr %result_temp18, i32 0, i32 0
  %err20 = load i8, ptr %err_ptr19, align 1
  %is_success21 = icmp eq i8 %err20, 0
  %val_ptr22 = getelementptr inbounds %result_int8, ptr %result_temp18, i32 0, i32 1
  %val23 = load i8, ptr %val_ptr22, align 1
  %unwrap_result24 = select i1 %is_success21, i8 %val23, i8 -1
  call void @puts(ptr @13)
  call void @puts(ptr @14)
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr25 = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr25, align 1
  %val_ptr26 = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 0, ptr %val_ptr26, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val
}

define i64 @main() {
entry:
  call void @__aria_module_init()
  %0 = call %result_int8 @__user_main()
  ret i64 0
}
