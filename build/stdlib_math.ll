; ModuleID = 'aria_module'
source_filename = "aria_module"

%result_int64 = type { i8, i64 }
%result_int8 = type { i8, i8 }

@0 = private unnamed_addr constant [28 x i8] c"Testing basic operations...\00", align 1
@1 = private unnamed_addr constant [25 x i8] c"Basic operations passed!\00", align 1
@2 = private unnamed_addr constant [33 x i8] c"Testing comparison operations...\00", align 1
@3 = private unnamed_addr constant [30 x i8] c"Comparison operations passed!\00", align 1
@4 = private unnamed_addr constant [25 x i8] c"Testing advanced math...\00", align 1
@5 = private unnamed_addr constant [22 x i8] c"Advanced math passed!\00", align 1
@6 = private unnamed_addr constant [47 x i8] c"=== Aria Standard Library - Math Utilities ===\00", align 1
@7 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@8 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@9 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@10 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@11 = private unnamed_addr constant [31 x i8] c"=== All Math Tests Passed! ===\00", align 1

declare void @puts(ptr)

declare void @print(ptr)

define internal void @__aria_module_init() {
entry:
  ret void
}

define internal %result_int64 @safe_add(i64 %a, i64 %b) {
entry:
  %sum = alloca i64, align 8
  %a1 = alloca i64, align 8
  store i64 %a, ptr %a1, align 4
  %b2 = alloca i64, align 8
  store i64 %b, ptr %b2, align 4
  %0 = load i64, ptr %a1, align 4
  %1 = load i64, ptr %b2, align 4
  %addtmp = add i64 %0, %1
  store i64 %addtmp, ptr %sum, align 4
  %2 = load i64, ptr %sum, align 4
  %result = alloca %result_int64, align 8
  %err_ptr = getelementptr inbounds %result_int64, ptr %result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int64, ptr %result, i32 0, i32 1
  store i64 %2, ptr %val_ptr, align 4
  %result_val = load %result_int64, ptr %result, align 4
  ret %result_int64 %result_val
}

define internal %result_int64 @safe_subtract(i64 %a, i64 %b) {
entry:
  %diff = alloca i64, align 8
  %a1 = alloca i64, align 8
  store i64 %a, ptr %a1, align 4
  %b2 = alloca i64, align 8
  store i64 %b, ptr %b2, align 4
  %0 = load i64, ptr %a1, align 4
  %1 = load i64, ptr %b2, align 4
  %subtmp = sub i64 %0, %1
  store i64 %subtmp, ptr %diff, align 4
  %2 = load i64, ptr %diff, align 4
  %result = alloca %result_int64, align 8
  %err_ptr = getelementptr inbounds %result_int64, ptr %result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int64, ptr %result, i32 0, i32 1
  store i64 %2, ptr %val_ptr, align 4
  %result_val = load %result_int64, ptr %result, align 4
  ret %result_int64 %result_val
}

define internal %result_int64 @safe_multiply(i64 %a, i64 %b) {
entry:
  %product = alloca i64, align 8
  %a1 = alloca i64, align 8
  store i64 %a, ptr %a1, align 4
  %b2 = alloca i64, align 8
  store i64 %b, ptr %b2, align 4
  %0 = load i64, ptr %a1, align 4
  %1 = load i64, ptr %b2, align 4
  %multmp = mul i64 %0, %1
  store i64 %multmp, ptr %product, align 4
  %2 = load i64, ptr %product, align 4
  %result = alloca %result_int64, align 8
  %err_ptr = getelementptr inbounds %result_int64, ptr %result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int64, ptr %result, i32 0, i32 1
  store i64 %2, ptr %val_ptr, align 4
  %result_val = load %result_int64, ptr %result, align 4
  ret %result_int64 %result_val
}

define internal %result_int64 @safe_divide(i64 %a, i64 %b) {
entry:
  %quotient = alloca i64, align 8
  %a1 = alloca i64, align 8
  store i64 %a, ptr %a1, align 4
  %b2 = alloca i64, align 8
  store i64 %b, ptr %b2, align 4
  %0 = load i64, ptr %b2, align 4
  br label %pick_label_division_by_zero

pick_label_division_by_zero:                      ; preds = %entry
  %result = alloca %result_int64, align 8
  %err_ptr = getelementptr inbounds %result_int64, ptr %result, i32 0, i32 0
  store i8 1, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int64, ptr %result, i32 0, i32 1
  store i64 0, ptr %val_ptr, align 4
  %result_val = load %result_int64, ptr %result, align 4
  ret %result_int64 %result_val

pick_label_valid_divisor:                         ; preds = %case_next_0
  %1 = load i64, ptr %a1, align 4
  %2 = load i64, ptr %b2, align 4
  %divtmp = sdiv i64 %1, %2
  store i64 %divtmp, ptr %quotient, align 4
  %3 = load i64, ptr %quotient, align 4
  %result3 = alloca %result_int64, align 8
  %err_ptr4 = getelementptr inbounds %result_int64, ptr %result3, i32 0, i32 0
  store i8 0, ptr %err_ptr4, align 1
  %val_ptr5 = getelementptr inbounds %result_int64, ptr %result3, i32 0, i32 1
  store i64 %3, ptr %val_ptr5, align 4
  %result_val6 = load %result_int64, ptr %result3, align 4
  ret %result_int64 %result_val6

case_next_0:                                      ; No predecessors!
  br label %pick_label_valid_divisor

case_next_1:                                      ; No predecessors!
  br label %pick_done

pick_done:                                        ; preds = %case_next_1
  ret %result_int64 zeroinitializer
}

define internal %result_int64 @modulo(i64 %a, i64 %b) {
entry:
  %remainder = alloca i64, align 8
  %a1 = alloca i64, align 8
  store i64 %a, ptr %a1, align 4
  %b2 = alloca i64, align 8
  store i64 %b, ptr %b2, align 4
  %0 = load i64, ptr %b2, align 4
  br label %pick_label_division_by_zero

pick_label_division_by_zero:                      ; preds = %entry
  %result = alloca %result_int64, align 8
  %err_ptr = getelementptr inbounds %result_int64, ptr %result, i32 0, i32 0
  store i8 1, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int64, ptr %result, i32 0, i32 1
  store i64 0, ptr %val_ptr, align 4
  %result_val = load %result_int64, ptr %result, align 4
  ret %result_int64 %result_val

pick_label_valid_divisor:                         ; preds = %case_next_0
  %1 = load i64, ptr %a1, align 4
  %2 = load i64, ptr %b2, align 4
  %modtmp = srem i64 %1, %2
  store i64 %modtmp, ptr %remainder, align 4
  %3 = load i64, ptr %remainder, align 4
  %result3 = alloca %result_int64, align 8
  %err_ptr4 = getelementptr inbounds %result_int64, ptr %result3, i32 0, i32 0
  store i8 0, ptr %err_ptr4, align 1
  %val_ptr5 = getelementptr inbounds %result_int64, ptr %result3, i32 0, i32 1
  store i64 %3, ptr %val_ptr5, align 4
  %result_val6 = load %result_int64, ptr %result3, align 4
  ret %result_int64 %result_val6

case_next_0:                                      ; No predecessors!
  br label %pick_label_valid_divisor

case_next_1:                                      ; No predecessors!
  br label %pick_done

pick_done:                                        ; preds = %case_next_1
  ret %result_int64 zeroinitializer
}

define internal %result_int64 @abs(i64 %n) {
entry:
  %pos = alloca i64, align 8
  %n1 = alloca i64, align 8
  store i64 %n, ptr %n1, align 4
  %0 = load i64, ptr %n1, align 4
  br label %pick_label_negative

pick_label_negative:                              ; preds = %entry
  %1 = load i64, ptr %n1, align 4
  %subtmp = sub i64 0, %1
  store i64 %subtmp, ptr %pos, align 4
  %2 = load i64, ptr %pos, align 4
  %result = alloca %result_int64, align 8
  %err_ptr = getelementptr inbounds %result_int64, ptr %result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int64, ptr %result, i32 0, i32 1
  store i64 %2, ptr %val_ptr, align 4
  %result_val = load %result_int64, ptr %result, align 4
  ret %result_int64 %result_val

pick_label_non_negative:                          ; preds = %case_next_0
  %3 = load i64, ptr %n1, align 4
  %result2 = alloca %result_int64, align 8
  %err_ptr3 = getelementptr inbounds %result_int64, ptr %result2, i32 0, i32 0
  store i8 0, ptr %err_ptr3, align 1
  %val_ptr4 = getelementptr inbounds %result_int64, ptr %result2, i32 0, i32 1
  store i64 %3, ptr %val_ptr4, align 4
  %result_val5 = load %result_int64, ptr %result2, align 4
  ret %result_int64 %result_val5

case_next_0:                                      ; No predecessors!
  br label %pick_label_non_negative

case_next_1:                                      ; No predecessors!
  br label %pick_done

pick_done:                                        ; preds = %case_next_1
  ret %result_int64 zeroinitializer
}

define internal %result_int8 @sign(i64 %n) {
entry:
  %n1 = alloca i64, align 8
  store i64 %n, ptr %n1, align 4
  %0 = load i64, ptr %n1, align 4
  br label %pick_label_negative

pick_label_negative:                              ; preds = %entry
  %result = alloca %result_int8, align 8
  %err_ptr = getelementptr inbounds %result_int8, ptr %result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int8, ptr %result, i32 0, i32 1
  store i8 -1, ptr %val_ptr, align 1
  %result_val = load %result_int8, ptr %result, align 1
  ret %result_int8 %result_val

pick_label_positive:                              ; preds = %case_next_0
  %result2 = alloca %result_int8, align 8
  %err_ptr3 = getelementptr inbounds %result_int8, ptr %result2, i32 0, i32 0
  store i8 0, ptr %err_ptr3, align 1
  %val_ptr4 = getelementptr inbounds %result_int8, ptr %result2, i32 0, i32 1
  store i8 1, ptr %val_ptr4, align 1
  %result_val5 = load %result_int8, ptr %result2, align 1
  ret %result_int8 %result_val5

pick_label_zero:                                  ; preds = %case_next_1
  %result6 = alloca %result_int8, align 8
  %err_ptr7 = getelementptr inbounds %result_int8, ptr %result6, i32 0, i32 0
  store i8 0, ptr %err_ptr7, align 1
  %val_ptr8 = getelementptr inbounds %result_int8, ptr %result6, i32 0, i32 1
  store i8 0, ptr %val_ptr8, align 1
  %result_val9 = load %result_int8, ptr %result6, align 1
  ret %result_int8 %result_val9

case_next_0:                                      ; No predecessors!
  br label %pick_label_positive

case_next_1:                                      ; No predecessors!
  br label %pick_label_zero

case_next_2:                                      ; No predecessors!
  br label %pick_done

pick_done:                                        ; preds = %case_next_2
  ret %result_int8 zeroinitializer
}

define internal %result_int64 @min(i64 %a, i64 %b) {
entry:
  %a1 = alloca i64, align 8
  store i64 %a, ptr %a1, align 4
  %b2 = alloca i64, align 8
  store i64 %b, ptr %b2, align 4
  %0 = load i64, ptr %a1, align 4
  br label %pick_label_a_is_smaller

pick_label_a_is_smaller:                          ; preds = %entry
  %1 = load i64, ptr %a1, align 4
  %result = alloca %result_int64, align 8
  %err_ptr = getelementptr inbounds %result_int64, ptr %result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int64, ptr %result, i32 0, i32 1
  store i64 %1, ptr %val_ptr, align 4
  %result_val = load %result_int64, ptr %result, align 4
  ret %result_int64 %result_val

pick_label_b_is_smaller_or_equal:                 ; preds = %case_next_0
  %2 = load i64, ptr %b2, align 4
  %result3 = alloca %result_int64, align 8
  %err_ptr4 = getelementptr inbounds %result_int64, ptr %result3, i32 0, i32 0
  store i8 0, ptr %err_ptr4, align 1
  %val_ptr5 = getelementptr inbounds %result_int64, ptr %result3, i32 0, i32 1
  store i64 %2, ptr %val_ptr5, align 4
  %result_val6 = load %result_int64, ptr %result3, align 4
  ret %result_int64 %result_val6

case_next_0:                                      ; No predecessors!
  br label %pick_label_b_is_smaller_or_equal

case_next_1:                                      ; No predecessors!
  br label %pick_done

pick_done:                                        ; preds = %case_next_1
  ret %result_int64 zeroinitializer
}

define internal %result_int64 @max(i64 %a, i64 %b) {
entry:
  %a1 = alloca i64, align 8
  store i64 %a, ptr %a1, align 4
  %b2 = alloca i64, align 8
  store i64 %b, ptr %b2, align 4
  %0 = load i64, ptr %a1, align 4
  br label %pick_label_a_is_larger

pick_label_a_is_larger:                           ; preds = %entry
  %1 = load i64, ptr %a1, align 4
  %result = alloca %result_int64, align 8
  %err_ptr = getelementptr inbounds %result_int64, ptr %result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int64, ptr %result, i32 0, i32 1
  store i64 %1, ptr %val_ptr, align 4
  %result_val = load %result_int64, ptr %result, align 4
  ret %result_int64 %result_val

pick_label_b_is_larger_or_equal:                  ; preds = %case_next_0
  %2 = load i64, ptr %b2, align 4
  %result3 = alloca %result_int64, align 8
  %err_ptr4 = getelementptr inbounds %result_int64, ptr %result3, i32 0, i32 0
  store i8 0, ptr %err_ptr4, align 1
  %val_ptr5 = getelementptr inbounds %result_int64, ptr %result3, i32 0, i32 1
  store i64 %2, ptr %val_ptr5, align 4
  %result_val6 = load %result_int64, ptr %result3, align 4
  ret %result_int64 %result_val6

case_next_0:                                      ; No predecessors!
  br label %pick_label_b_is_larger_or_equal

case_next_1:                                      ; No predecessors!
  br label %pick_done

pick_done:                                        ; preds = %case_next_1
  ret %result_int64 zeroinitializer
}

define internal %result_int64 @clamp(i64 %value, i64 %min_val, i64 %max_val) {
entry:
  %clamped = alloca i64, align 8
  %value1 = alloca i64, align 8
  store i64 %value, ptr %value1, align 4
  %min_val2 = alloca i64, align 8
  store i64 %min_val, ptr %min_val2, align 4
  %max_val3 = alloca i64, align 8
  store i64 %max_val, ptr %max_val3, align 4
  %0 = load i64, ptr %max_val3, align 4
  %1 = load i64, ptr %min_val2, align 4
  %2 = load i64, ptr %value1, align 4
  %calltmp = call %result_int64 @max(i64 %1, i64 %2)
  %3 = load i64, ptr %value1, align 4
  %result_temp = alloca %result_int64, align 8
  store %result_int64 %calltmp, ptr %result_temp, align 4
  %err_ptr = getelementptr inbounds %result_int64, ptr %result_temp, i32 0, i32 0
  %err = load i8, ptr %err_ptr, align 1
  %is_success = icmp eq i8 %err, 0
  %val_ptr = getelementptr inbounds %result_int64, ptr %result_temp, i32 0, i32 1
  %val = load i64, ptr %val_ptr, align 4
  %unwrap_result = select i1 %is_success, i64 %val, i64 %3
  %calltmp4 = call %result_int64 @min(i64 %0, i64 %unwrap_result)
  %4 = load i64, ptr %value1, align 4
  %result_temp5 = alloca %result_int64, align 8
  store %result_int64 %calltmp4, ptr %result_temp5, align 4
  %err_ptr6 = getelementptr inbounds %result_int64, ptr %result_temp5, i32 0, i32 0
  %err7 = load i8, ptr %err_ptr6, align 1
  %is_success8 = icmp eq i8 %err7, 0
  %val_ptr9 = getelementptr inbounds %result_int64, ptr %result_temp5, i32 0, i32 1
  %val10 = load i64, ptr %val_ptr9, align 4
  %unwrap_result11 = select i1 %is_success8, i64 %val10, i64 %4
  store i64 %unwrap_result11, ptr %clamped, align 4
  %5 = load i64, ptr %clamped, align 4
  %result = alloca %result_int64, align 8
  %err_ptr12 = getelementptr inbounds %result_int64, ptr %result, i32 0, i32 0
  store i8 0, ptr %err_ptr12, align 1
  %val_ptr13 = getelementptr inbounds %result_int64, ptr %result, i32 0, i32 1
  store i64 %5, ptr %val_ptr13, align 4
  %result_val = load %result_int64, ptr %result, align 4
  ret %result_int64 %result_val
}

define internal %result_int64 @pow(i64 %base, i64 %exponent) {
entry:
  %count = alloca i64, align 8
  %res = alloca i64, align 8
  %base1 = alloca i64, align 8
  store i64 %base, ptr %base1, align 4
  %exponent2 = alloca i64, align 8
  store i64 %exponent, ptr %exponent2, align 4
  %0 = load i64, ptr %exponent2, align 4
  br label %pick_label_zero_exponent

pick_label_zero_exponent:                         ; preds = %entry
  %result = alloca %result_int64, align 8
  %err_ptr = getelementptr inbounds %result_int64, ptr %result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int64, ptr %result, i32 0, i32 1
  store i64 1, ptr %val_ptr, align 4
  %result_val = load %result_int64, ptr %result, align 4
  ret %result_int64 %result_val

pick_label_one_exponent:                          ; preds = %case_next_0
  %1 = load i64, ptr %base1, align 4
  %result3 = alloca %result_int64, align 8
  %err_ptr4 = getelementptr inbounds %result_int64, ptr %result3, i32 0, i32 0
  store i8 0, ptr %err_ptr4, align 1
  %val_ptr5 = getelementptr inbounds %result_int64, ptr %result3, i32 0, i32 1
  store i64 %1, ptr %val_ptr5, align 4
  %result_val6 = load %result_int64, ptr %result3, align 4
  ret %result_int64 %result_val6

pick_label_negative_exponent:                     ; preds = %case_next_1
  %result7 = alloca %result_int64, align 8
  %err_ptr8 = getelementptr inbounds %result_int64, ptr %result7, i32 0, i32 0
  store i8 4, ptr %err_ptr8, align 1
  %val_ptr9 = getelementptr inbounds %result_int64, ptr %result7, i32 0, i32 1
  store i64 0, ptr %val_ptr9, align 4
  %result_val10 = load %result_int64, ptr %result7, align 4
  ret %result_int64 %result_val10

pick_label_positive_exponent:                     ; preds = %case_next_2
  store i64 1, ptr %res, align 4
  store i64 0, ptr %count, align 4
  br label %while_cond

case_next_0:                                      ; No predecessors!
  br label %pick_label_one_exponent

case_next_1:                                      ; No predecessors!
  br label %pick_label_negative_exponent

case_next_2:                                      ; No predecessors!
  br label %pick_label_positive_exponent

while_cond:                                       ; preds = %while_body, %pick_label_positive_exponent
  %2 = load i64, ptr %count, align 4
  %3 = load i64, ptr %exponent2, align 4
  %lttmp = icmp slt i64 %2, %3
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %4 = load i64, ptr %res, align 4
  %5 = load i64, ptr %base1, align 4
  %multmp = mul i64 %4, %5
  store i64 %multmp, ptr %res, align 4
  %6 = load i64, ptr %count, align 4
  %addtmp = add i64 %6, 1
  store i64 %addtmp, ptr %count, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %7 = load i64, ptr %res, align 4
  %result11 = alloca %result_int64, align 8
  %err_ptr12 = getelementptr inbounds %result_int64, ptr %result11, i32 0, i32 0
  store i8 0, ptr %err_ptr12, align 1
  %val_ptr13 = getelementptr inbounds %result_int64, ptr %result11, i32 0, i32 1
  store i64 %7, ptr %val_ptr13, align 4
  %result_val14 = load %result_int64, ptr %result11, align 4
  ret %result_int64 %result_val14

case_next_3:                                      ; No predecessors!
  br label %pick_done

pick_done:                                        ; preds = %case_next_3
  ret %result_int64 zeroinitializer
}

define internal %result_int64 @gcd(i64 %a, i64 %b) {
entry:
  %temp = alloca i64, align 8
  %abs_b = alloca i64, align 8
  %abs_a = alloca i64, align 8
  %a1 = alloca i64, align 8
  store i64 %a, ptr %a1, align 4
  %b2 = alloca i64, align 8
  store i64 %b, ptr %b2, align 4
  %0 = load i64, ptr %a1, align 4
  %calltmp = call %result_int64 @abs(i64 %0)
  %result_temp = alloca %result_int64, align 8
  store %result_int64 %calltmp, ptr %result_temp, align 4
  %err_ptr = getelementptr inbounds %result_int64, ptr %result_temp, i32 0, i32 0
  %err = load i8, ptr %err_ptr, align 1
  %is_success = icmp eq i8 %err, 0
  %val_ptr = getelementptr inbounds %result_int64, ptr %result_temp, i32 0, i32 1
  %val = load i64, ptr %val_ptr, align 4
  %unwrap_result = select i1 %is_success, i64 %val, i64 0
  store i64 %unwrap_result, ptr %abs_a, align 4
  %1 = load i64, ptr %b2, align 4
  %calltmp3 = call %result_int64 @abs(i64 %1)
  %result_temp4 = alloca %result_int64, align 8
  store %result_int64 %calltmp3, ptr %result_temp4, align 4
  %err_ptr5 = getelementptr inbounds %result_int64, ptr %result_temp4, i32 0, i32 0
  %err6 = load i8, ptr %err_ptr5, align 1
  %is_success7 = icmp eq i8 %err6, 0
  %val_ptr8 = getelementptr inbounds %result_int64, ptr %result_temp4, i32 0, i32 1
  %val9 = load i64, ptr %val_ptr8, align 4
  %unwrap_result10 = select i1 %is_success7, i64 %val9, i64 0
  store i64 %unwrap_result10, ptr %abs_b, align 4
  br label %while_cond

while_cond:                                       ; preds = %while_body, %entry
  %2 = load i64, ptr %abs_b, align 4
  %gttmp = icmp sgt i64 %2, 0
  br i1 %gttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %3 = load i64, ptr %abs_b, align 4
  store i64 %3, ptr %temp, align 4
  %4 = load i64, ptr %abs_a, align 4
  %5 = load i64, ptr %abs_b, align 4
  %calltmp11 = call %result_int64 @modulo(i64 %4, i64 %5)
  %result_temp12 = alloca %result_int64, align 8
  store %result_int64 %calltmp11, ptr %result_temp12, align 4
  %err_ptr13 = getelementptr inbounds %result_int64, ptr %result_temp12, i32 0, i32 0
  %err14 = load i8, ptr %err_ptr13, align 1
  %is_success15 = icmp eq i8 %err14, 0
  %val_ptr16 = getelementptr inbounds %result_int64, ptr %result_temp12, i32 0, i32 1
  %val17 = load i64, ptr %val_ptr16, align 4
  %unwrap_result18 = select i1 %is_success15, i64 %val17, i64 0
  store i64 %unwrap_result18, ptr %abs_b, align 4
  %6 = load i64, ptr %temp, align 4
  store i64 %6, ptr %abs_a, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %7 = load i64, ptr %abs_a, align 4
  %result = alloca %result_int64, align 8
  %err_ptr19 = getelementptr inbounds %result_int64, ptr %result, i32 0, i32 0
  store i8 0, ptr %err_ptr19, align 1
  %val_ptr20 = getelementptr inbounds %result_int64, ptr %result, i32 0, i32 1
  store i64 %7, ptr %val_ptr20, align 4
  %result_val = load %result_int64, ptr %result, align 4
  ret %result_int64 %result_val
}

define internal %result_int64 @lcm(i64 %a, i64 %b) {
entry:
  %lcm_val = alloca i64, align 8
  %product = alloca i64, align 8
  %abs_b = alloca i64, align 8
  %abs_a = alloca i64, align 8
  %gcd_val = alloca i64, align 8
  %a1 = alloca i64, align 8
  store i64 %a, ptr %a1, align 4
  %b2 = alloca i64, align 8
  store i64 %b, ptr %b2, align 4
  %0 = load i64, ptr %a1, align 4
  %1 = load i64, ptr %b2, align 4
  %calltmp = call %result_int64 @gcd(i64 %0, i64 %1)
  %result_temp = alloca %result_int64, align 8
  store %result_int64 %calltmp, ptr %result_temp, align 4
  %err_ptr = getelementptr inbounds %result_int64, ptr %result_temp, i32 0, i32 0
  %err = load i8, ptr %err_ptr, align 1
  %is_success = icmp eq i8 %err, 0
  %val_ptr = getelementptr inbounds %result_int64, ptr %result_temp, i32 0, i32 1
  %val = load i64, ptr %val_ptr, align 4
  %unwrap_result = select i1 %is_success, i64 %val, i64 1
  store i64 %unwrap_result, ptr %gcd_val, align 4
  %2 = load i64, ptr %gcd_val, align 4
  br label %pick_label_both_zero

pick_label_both_zero:                             ; preds = %entry
  %result = alloca %result_int64, align 8
  %err_ptr3 = getelementptr inbounds %result_int64, ptr %result, i32 0, i32 0
  store i8 0, ptr %err_ptr3, align 1
  %val_ptr4 = getelementptr inbounds %result_int64, ptr %result, i32 0, i32 1
  store i64 0, ptr %val_ptr4, align 4
  %result_val = load %result_int64, ptr %result, align 4
  ret %result_int64 %result_val

pick_label_compute_lcm:                           ; preds = %case_next_0
  %3 = load i64, ptr %a1, align 4
  %calltmp5 = call %result_int64 @abs(i64 %3)
  %result_temp6 = alloca %result_int64, align 8
  store %result_int64 %calltmp5, ptr %result_temp6, align 4
  %err_ptr7 = getelementptr inbounds %result_int64, ptr %result_temp6, i32 0, i32 0
  %err8 = load i8, ptr %err_ptr7, align 1
  %is_success9 = icmp eq i8 %err8, 0
  %val_ptr10 = getelementptr inbounds %result_int64, ptr %result_temp6, i32 0, i32 1
  %val11 = load i64, ptr %val_ptr10, align 4
  %unwrap_result12 = select i1 %is_success9, i64 %val11, i64 0
  store i64 %unwrap_result12, ptr %abs_a, align 4
  %4 = load i64, ptr %b2, align 4
  %calltmp13 = call %result_int64 @abs(i64 %4)
  %result_temp14 = alloca %result_int64, align 8
  store %result_int64 %calltmp13, ptr %result_temp14, align 4
  %err_ptr15 = getelementptr inbounds %result_int64, ptr %result_temp14, i32 0, i32 0
  %err16 = load i8, ptr %err_ptr15, align 1
  %is_success17 = icmp eq i8 %err16, 0
  %val_ptr18 = getelementptr inbounds %result_int64, ptr %result_temp14, i32 0, i32 1
  %val19 = load i64, ptr %val_ptr18, align 4
  %unwrap_result20 = select i1 %is_success17, i64 %val19, i64 0
  store i64 %unwrap_result20, ptr %abs_b, align 4
  %5 = load i64, ptr %abs_a, align 4
  %6 = load i64, ptr %abs_b, align 4
  %multmp = mul i64 %5, %6
  store i64 %multmp, ptr %product, align 4
  %7 = load i64, ptr %product, align 4
  %8 = load i64, ptr %gcd_val, align 4
  %calltmp21 = call %result_int64 @safe_divide(i64 %7, i64 %8)
  %result_temp22 = alloca %result_int64, align 8
  store %result_int64 %calltmp21, ptr %result_temp22, align 4
  %err_ptr23 = getelementptr inbounds %result_int64, ptr %result_temp22, i32 0, i32 0
  %err24 = load i8, ptr %err_ptr23, align 1
  %is_success25 = icmp eq i8 %err24, 0
  %val_ptr26 = getelementptr inbounds %result_int64, ptr %result_temp22, i32 0, i32 1
  %val27 = load i64, ptr %val_ptr26, align 4
  %unwrap_result28 = select i1 %is_success25, i64 %val27, i64 0
  store i64 %unwrap_result28, ptr %lcm_val, align 4
  %9 = load i64, ptr %lcm_val, align 4
  %result29 = alloca %result_int64, align 8
  %err_ptr30 = getelementptr inbounds %result_int64, ptr %result29, i32 0, i32 0
  store i8 0, ptr %err_ptr30, align 1
  %val_ptr31 = getelementptr inbounds %result_int64, ptr %result29, i32 0, i32 1
  store i64 %9, ptr %val_ptr31, align 4
  %result_val32 = load %result_int64, ptr %result29, align 4
  ret %result_int64 %result_val32

case_next_0:                                      ; No predecessors!
  br label %pick_label_compute_lcm

case_next_1:                                      ; No predecessors!
  br label %pick_done

pick_done:                                        ; preds = %case_next_1
  ret %result_int64 zeroinitializer
}

define internal %result_int8 @is_even(i64 %n) {
entry:
  %remainder = alloca i64, align 8
  %n1 = alloca i64, align 8
  store i64 %n, ptr %n1, align 4
  %0 = load i64, ptr %n1, align 4
  %calltmp = call %result_int64 @modulo(i64 %0, i64 2)
  %result_temp = alloca %result_int64, align 8
  store %result_int64 %calltmp, ptr %result_temp, align 4
  %err_ptr = getelementptr inbounds %result_int64, ptr %result_temp, i32 0, i32 0
  %err = load i8, ptr %err_ptr, align 1
  %is_success = icmp eq i8 %err, 0
  %val_ptr = getelementptr inbounds %result_int64, ptr %result_temp, i32 0, i32 1
  %val = load i64, ptr %val_ptr, align 4
  %unwrap_result = select i1 %is_success, i64 %val, i64 1
  store i64 %unwrap_result, ptr %remainder, align 4
  %1 = load i64, ptr %remainder, align 4
  br label %pick_label_even

pick_label_even:                                  ; preds = %entry
  %result = alloca %result_int8, align 8
  %err_ptr2 = getelementptr inbounds %result_int8, ptr %result, i32 0, i32 0
  store i8 0, ptr %err_ptr2, align 1
  %val_ptr3 = getelementptr inbounds %result_int8, ptr %result, i32 0, i32 1
  store i8 1, ptr %val_ptr3, align 1
  %result_val = load %result_int8, ptr %result, align 1
  ret %result_int8 %result_val

pick_label_odd:                                   ; preds = %case_next_0
  %result4 = alloca %result_int8, align 8
  %err_ptr5 = getelementptr inbounds %result_int8, ptr %result4, i32 0, i32 0
  store i8 0, ptr %err_ptr5, align 1
  %val_ptr6 = getelementptr inbounds %result_int8, ptr %result4, i32 0, i32 1
  store i8 0, ptr %val_ptr6, align 1
  %result_val7 = load %result_int8, ptr %result4, align 1
  ret %result_int8 %result_val7

case_next_0:                                      ; No predecessors!
  br label %pick_label_odd

case_next_1:                                      ; No predecessors!
  br label %pick_done

pick_done:                                        ; preds = %case_next_1
  ret %result_int8 zeroinitializer
}

define internal %result_int8 @is_odd(i64 %n) {
entry:
  %even = alloca i8, align 1
  %n1 = alloca i64, align 8
  store i64 %n, ptr %n1, align 4
  %0 = load i64, ptr %n1, align 4
  %calltmp = call %result_int8 @is_even(i64 %0)
  %result_temp = alloca %result_int8, align 8
  store %result_int8 %calltmp, ptr %result_temp, align 1
  %err_ptr = getelementptr inbounds %result_int8, ptr %result_temp, i32 0, i32 0
  %err = load i8, ptr %err_ptr, align 1
  %is_success = icmp eq i8 %err, 0
  %val_ptr = getelementptr inbounds %result_int8, ptr %result_temp, i32 0, i32 1
  %val = load i8, ptr %val_ptr, align 1
  %unwrap_result = select i1 %is_success, i8 %val, i8 0
  store i8 %unwrap_result, ptr %even, align 1
  %1 = load i8, ptr %even, align 1
  br label %pick_label_not_even

pick_label_not_even:                              ; preds = %entry
  %result = alloca %result_int8, align 8
  %err_ptr2 = getelementptr inbounds %result_int8, ptr %result, i32 0, i32 0
  store i8 0, ptr %err_ptr2, align 1
  %val_ptr3 = getelementptr inbounds %result_int8, ptr %result, i32 0, i32 1
  store i8 1, ptr %val_ptr3, align 1
  %result_val = load %result_int8, ptr %result, align 1
  ret %result_int8 %result_val

pick_label_is_even:                               ; preds = %case_next_0
  %result4 = alloca %result_int8, align 8
  %err_ptr5 = getelementptr inbounds %result_int8, ptr %result4, i32 0, i32 0
  store i8 0, ptr %err_ptr5, align 1
  %val_ptr6 = getelementptr inbounds %result_int8, ptr %result4, i32 0, i32 1
  store i8 0, ptr %val_ptr6, align 1
  %result_val7 = load %result_int8, ptr %result4, align 1
  ret %result_int8 %result_val7

case_next_0:                                      ; No predecessors!
  br label %pick_label_is_even

case_next_1:                                      ; No predecessors!
  br label %pick_done

pick_done:                                        ; preds = %case_next_1
  ret %result_int8 zeroinitializer
}

define internal %result_int64 @factorial(i64 %n) {
entry:
  %i = alloca i64, align 8
  %res = alloca i64, align 8
  %n1 = alloca i64, align 8
  store i64 %n, ptr %n1, align 4
  %0 = load i64, ptr %n1, align 4
  br label %pick_label_negative

pick_label_negative:                              ; preds = %entry
  %result = alloca %result_int64, align 8
  %err_ptr = getelementptr inbounds %result_int64, ptr %result, i32 0, i32 0
  store i8 4, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int64, ptr %result, i32 0, i32 1
  store i64 0, ptr %val_ptr, align 4
  %result_val = load %result_int64, ptr %result, align 4
  ret %result_int64 %result_val

pick_label_zero:                                  ; preds = %case_next_0
  %result2 = alloca %result_int64, align 8
  %err_ptr3 = getelementptr inbounds %result_int64, ptr %result2, i32 0, i32 0
  store i8 0, ptr %err_ptr3, align 1
  %val_ptr4 = getelementptr inbounds %result_int64, ptr %result2, i32 0, i32 1
  store i64 1, ptr %val_ptr4, align 4
  %result_val5 = load %result_int64, ptr %result2, align 4
  ret %result_int64 %result_val5

pick_label_one:                                   ; preds = %case_next_1
  %result6 = alloca %result_int64, align 8
  %err_ptr7 = getelementptr inbounds %result_int64, ptr %result6, i32 0, i32 0
  store i8 0, ptr %err_ptr7, align 1
  %val_ptr8 = getelementptr inbounds %result_int64, ptr %result6, i32 0, i32 1
  store i64 1, ptr %val_ptr8, align 4
  %result_val9 = load %result_int64, ptr %result6, align 4
  ret %result_int64 %result_val9

pick_label_compute:                               ; preds = %case_next_2
  store i64 1, ptr %res, align 4
  store i64 2, ptr %i, align 4
  br label %while_cond

case_next_0:                                      ; No predecessors!
  br label %pick_label_zero

case_next_1:                                      ; No predecessors!
  br label %pick_label_one

case_next_2:                                      ; No predecessors!
  br label %pick_label_compute

while_cond:                                       ; preds = %while_body, %pick_label_compute
  %1 = load i64, ptr %i, align 4
  %2 = load i64, ptr %n1, align 4
  %letmp = icmp sle i64 %1, %2
  br i1 %letmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %3 = load i64, ptr %res, align 4
  %4 = load i64, ptr %i, align 4
  %multmp = mul i64 %3, %4
  store i64 %multmp, ptr %res, align 4
  %5 = load i64, ptr %i, align 4
  %addtmp = add i64 %5, 1
  store i64 %addtmp, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %6 = load i64, ptr %res, align 4
  %result10 = alloca %result_int64, align 8
  %err_ptr11 = getelementptr inbounds %result_int64, ptr %result10, i32 0, i32 0
  store i8 0, ptr %err_ptr11, align 1
  %val_ptr12 = getelementptr inbounds %result_int64, ptr %result10, i32 0, i32 1
  store i64 %6, ptr %val_ptr12, align 4
  %result_val13 = load %result_int64, ptr %result10, align 4
  ret %result_int64 %result_val13

case_next_3:                                      ; No predecessors!
  br label %pick_done

pick_done:                                        ; preds = %case_next_3
  ret %result_int64 zeroinitializer
}

define internal %result_int64 @fibonacci(i64 %n) {
entry:
  %temp = alloca i64, align 8
  %count = alloca i64, align 8
  %b = alloca i64, align 8
  %a = alloca i64, align 8
  %n1 = alloca i64, align 8
  store i64 %n, ptr %n1, align 4
  %0 = load i64, ptr %n1, align 4
  br label %pick_label_negative

pick_label_negative:                              ; preds = %entry
  %result = alloca %result_int64, align 8
  %err_ptr = getelementptr inbounds %result_int64, ptr %result, i32 0, i32 0
  store i8 4, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int64, ptr %result, i32 0, i32 1
  store i64 0, ptr %val_ptr, align 4
  %result_val = load %result_int64, ptr %result, align 4
  ret %result_int64 %result_val

pick_label_zero:                                  ; preds = %case_next_0
  %result2 = alloca %result_int64, align 8
  %err_ptr3 = getelementptr inbounds %result_int64, ptr %result2, i32 0, i32 0
  store i8 0, ptr %err_ptr3, align 1
  %val_ptr4 = getelementptr inbounds %result_int64, ptr %result2, i32 0, i32 1
  store i64 0, ptr %val_ptr4, align 4
  %result_val5 = load %result_int64, ptr %result2, align 4
  ret %result_int64 %result_val5

pick_label_one:                                   ; preds = %case_next_1
  %result6 = alloca %result_int64, align 8
  %err_ptr7 = getelementptr inbounds %result_int64, ptr %result6, i32 0, i32 0
  store i8 0, ptr %err_ptr7, align 1
  %val_ptr8 = getelementptr inbounds %result_int64, ptr %result6, i32 0, i32 1
  store i64 1, ptr %val_ptr8, align 4
  %result_val9 = load %result_int64, ptr %result6, align 4
  ret %result_int64 %result_val9

pick_label_compute:                               ; preds = %case_next_2
  store i64 0, ptr %a, align 4
  store i64 1, ptr %b, align 4
  store i64 2, ptr %count, align 4
  br label %while_cond

case_next_0:                                      ; No predecessors!
  br label %pick_label_zero

case_next_1:                                      ; No predecessors!
  br label %pick_label_one

case_next_2:                                      ; No predecessors!
  br label %pick_label_compute

while_cond:                                       ; preds = %while_body, %pick_label_compute
  %1 = load i64, ptr %count, align 4
  %2 = load i64, ptr %n1, align 4
  %letmp = icmp sle i64 %1, %2
  br i1 %letmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %3 = load i64, ptr %a, align 4
  %4 = load i64, ptr %b, align 4
  %addtmp = add i64 %3, %4
  store i64 %addtmp, ptr %temp, align 4
  %5 = load i64, ptr %b, align 4
  store i64 %5, ptr %a, align 4
  %6 = load i64, ptr %temp, align 4
  store i64 %6, ptr %b, align 4
  %7 = load i64, ptr %count, align 4
  %addtmp10 = add i64 %7, 1
  store i64 %addtmp10, ptr %count, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %8 = load i64, ptr %b, align 4
  %result11 = alloca %result_int64, align 8
  %err_ptr12 = getelementptr inbounds %result_int64, ptr %result11, i32 0, i32 0
  store i8 0, ptr %err_ptr12, align 1
  %val_ptr13 = getelementptr inbounds %result_int64, ptr %result11, i32 0, i32 1
  store i64 %8, ptr %val_ptr13, align 4
  %result_val14 = load %result_int64, ptr %result11, align 4
  ret %result_int64 %result_val14

case_next_3:                                      ; No predecessors!
  br label %pick_done

pick_done:                                        ; preds = %case_next_3
  ret %result_int64 zeroinitializer
}

define internal %result_int64 @isqrt(i64 %n) {
entry:
  %y = alloca i64, align 8
  %x = alloca i64, align 8
  %n1 = alloca i64, align 8
  store i64 %n, ptr %n1, align 4
  %0 = load i64, ptr %n1, align 4
  br label %pick_label_negative

pick_label_negative:                              ; preds = %entry
  %result = alloca %result_int64, align 8
  %err_ptr = getelementptr inbounds %result_int64, ptr %result, i32 0, i32 0
  store i8 2, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int64, ptr %result, i32 0, i32 1
  store i64 0, ptr %val_ptr, align 4
  %result_val = load %result_int64, ptr %result, align 4
  ret %result_int64 %result_val

pick_label_zero:                                  ; preds = %case_next_0
  %result2 = alloca %result_int64, align 8
  %err_ptr3 = getelementptr inbounds %result_int64, ptr %result2, i32 0, i32 0
  store i8 0, ptr %err_ptr3, align 1
  %val_ptr4 = getelementptr inbounds %result_int64, ptr %result2, i32 0, i32 1
  store i64 0, ptr %val_ptr4, align 4
  %result_val5 = load %result_int64, ptr %result2, align 4
  ret %result_int64 %result_val5

pick_label_one:                                   ; preds = %case_next_1
  %result6 = alloca %result_int64, align 8
  %err_ptr7 = getelementptr inbounds %result_int64, ptr %result6, i32 0, i32 0
  store i8 0, ptr %err_ptr7, align 1
  %val_ptr8 = getelementptr inbounds %result_int64, ptr %result6, i32 0, i32 1
  store i64 1, ptr %val_ptr8, align 4
  %result_val9 = load %result_int64, ptr %result6, align 4
  ret %result_int64 %result_val9

pick_label_compute:                               ; preds = %case_next_2
  %1 = load i64, ptr %n1, align 4
  store i64 %1, ptr %x, align 4
  %2 = load i64, ptr %n1, align 4
  %divtmp = sdiv i64 %2, 2
  %addtmp = add i64 %divtmp, 1
  store i64 %addtmp, ptr %y, align 4
  br label %while_cond

case_next_0:                                      ; No predecessors!
  br label %pick_label_zero

case_next_1:                                      ; No predecessors!
  br label %pick_label_one

case_next_2:                                      ; No predecessors!
  br label %pick_label_compute

while_cond:                                       ; preds = %while_body, %pick_label_compute
  %3 = load i64, ptr %y, align 4
  %4 = load i64, ptr %x, align 4
  %lttmp = icmp slt i64 %3, %4
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %5 = load i64, ptr %y, align 4
  store i64 %5, ptr %x, align 4
  %6 = load i64, ptr %x, align 4
  %7 = load i64, ptr %n1, align 4
  %8 = load i64, ptr %x, align 4
  %divtmp10 = sdiv i64 %7, %8
  %addtmp11 = add i64 %6, %divtmp10
  %divtmp12 = sdiv i64 %addtmp11, 2
  store i64 %divtmp12, ptr %y, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %9 = load i64, ptr %x, align 4
  %result13 = alloca %result_int64, align 8
  %err_ptr14 = getelementptr inbounds %result_int64, ptr %result13, i32 0, i32 0
  store i8 0, ptr %err_ptr14, align 1
  %val_ptr15 = getelementptr inbounds %result_int64, ptr %result13, i32 0, i32 1
  store i64 %9, ptr %val_ptr15, align 4
  %result_val16 = load %result_int64, ptr %result13, align 4
  ret %result_int64 %result_val16

case_next_3:                                      ; No predecessors!
  br label %pick_done

pick_done:                                        ; preds = %case_next_3
  ret %result_int64 zeroinitializer
}

define internal %result_int8 @test_basic_ops() {
entry:
  %rem = alloca i64, align 8
  %quot = alloca i64, align 8
  %prod = alloca i64, align 8
  %diff = alloca i64, align 8
  %sum = alloca i64, align 8
  call void @puts(ptr @0)
  %calltmp = call %result_int64 @safe_add(i64 100, i64 50)
  %result_temp = alloca %result_int64, align 8
  store %result_int64 %calltmp, ptr %result_temp, align 4
  %err_ptr = getelementptr inbounds %result_int64, ptr %result_temp, i32 0, i32 0
  %err = load i8, ptr %err_ptr, align 1
  %is_success = icmp eq i8 %err, 0
  %val_ptr = getelementptr inbounds %result_int64, ptr %result_temp, i32 0, i32 1
  %val = load i64, ptr %val_ptr, align 4
  %unwrap_result = select i1 %is_success, i64 %val, i64 0
  store i64 %unwrap_result, ptr %sum, align 4
  %calltmp1 = call %result_int64 @safe_subtract(i64 100, i64 30)
  %result_temp2 = alloca %result_int64, align 8
  store %result_int64 %calltmp1, ptr %result_temp2, align 4
  %err_ptr3 = getelementptr inbounds %result_int64, ptr %result_temp2, i32 0, i32 0
  %err4 = load i8, ptr %err_ptr3, align 1
  %is_success5 = icmp eq i8 %err4, 0
  %val_ptr6 = getelementptr inbounds %result_int64, ptr %result_temp2, i32 0, i32 1
  %val7 = load i64, ptr %val_ptr6, align 4
  %unwrap_result8 = select i1 %is_success5, i64 %val7, i64 0
  store i64 %unwrap_result8, ptr %diff, align 4
  %calltmp9 = call %result_int64 @safe_multiply(i64 12, i64 8)
  %result_temp10 = alloca %result_int64, align 8
  store %result_int64 %calltmp9, ptr %result_temp10, align 4
  %err_ptr11 = getelementptr inbounds %result_int64, ptr %result_temp10, i32 0, i32 0
  %err12 = load i8, ptr %err_ptr11, align 1
  %is_success13 = icmp eq i8 %err12, 0
  %val_ptr14 = getelementptr inbounds %result_int64, ptr %result_temp10, i32 0, i32 1
  %val15 = load i64, ptr %val_ptr14, align 4
  %unwrap_result16 = select i1 %is_success13, i64 %val15, i64 0
  store i64 %unwrap_result16, ptr %prod, align 4
  %calltmp17 = call %result_int64 @safe_divide(i64 100, i64 5)
  %result_temp18 = alloca %result_int64, align 8
  store %result_int64 %calltmp17, ptr %result_temp18, align 4
  %err_ptr19 = getelementptr inbounds %result_int64, ptr %result_temp18, i32 0, i32 0
  %err20 = load i8, ptr %err_ptr19, align 1
  %is_success21 = icmp eq i8 %err20, 0
  %val_ptr22 = getelementptr inbounds %result_int64, ptr %result_temp18, i32 0, i32 1
  %val23 = load i64, ptr %val_ptr22, align 4
  %unwrap_result24 = select i1 %is_success21, i64 %val23, i64 0
  store i64 %unwrap_result24, ptr %quot, align 4
  %calltmp25 = call %result_int64 @modulo(i64 17, i64 5)
  %result_temp26 = alloca %result_int64, align 8
  store %result_int64 %calltmp25, ptr %result_temp26, align 4
  %err_ptr27 = getelementptr inbounds %result_int64, ptr %result_temp26, i32 0, i32 0
  %err28 = load i8, ptr %err_ptr27, align 1
  %is_success29 = icmp eq i8 %err28, 0
  %val_ptr30 = getelementptr inbounds %result_int64, ptr %result_temp26, i32 0, i32 1
  %val31 = load i64, ptr %val_ptr30, align 4
  %unwrap_result32 = select i1 %is_success29, i64 %val31, i64 0
  store i64 %unwrap_result32, ptr %rem, align 4
  call void @puts(ptr @1)
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr33 = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr33, align 1
  %val_ptr34 = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 0, ptr %val_ptr34, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val
}

define internal %result_int8 @test_comparison_ops() {
entry:
  %sign_val = alloca i8, align 1
  %abs_val = alloca i64, align 8
  %max_val = alloca i64, align 8
  %min_val = alloca i64, align 8
  call void @puts(ptr @2)
  %calltmp = call %result_int64 @min(i64 10, i64 20)
  %result_temp = alloca %result_int64, align 8
  store %result_int64 %calltmp, ptr %result_temp, align 4
  %err_ptr = getelementptr inbounds %result_int64, ptr %result_temp, i32 0, i32 0
  %err = load i8, ptr %err_ptr, align 1
  %is_success = icmp eq i8 %err, 0
  %val_ptr = getelementptr inbounds %result_int64, ptr %result_temp, i32 0, i32 1
  %val = load i64, ptr %val_ptr, align 4
  %unwrap_result = select i1 %is_success, i64 %val, i64 0
  store i64 %unwrap_result, ptr %min_val, align 4
  %calltmp1 = call %result_int64 @max(i64 10, i64 20)
  %result_temp2 = alloca %result_int64, align 8
  store %result_int64 %calltmp1, ptr %result_temp2, align 4
  %err_ptr3 = getelementptr inbounds %result_int64, ptr %result_temp2, i32 0, i32 0
  %err4 = load i8, ptr %err_ptr3, align 1
  %is_success5 = icmp eq i8 %err4, 0
  %val_ptr6 = getelementptr inbounds %result_int64, ptr %result_temp2, i32 0, i32 1
  %val7 = load i64, ptr %val_ptr6, align 4
  %unwrap_result8 = select i1 %is_success5, i64 %val7, i64 0
  store i64 %unwrap_result8, ptr %max_val, align 4
  %calltmp9 = call %result_int64 @abs(i64 -42)
  %result_temp10 = alloca %result_int64, align 8
  store %result_int64 %calltmp9, ptr %result_temp10, align 4
  %err_ptr11 = getelementptr inbounds %result_int64, ptr %result_temp10, i32 0, i32 0
  %err12 = load i8, ptr %err_ptr11, align 1
  %is_success13 = icmp eq i8 %err12, 0
  %val_ptr14 = getelementptr inbounds %result_int64, ptr %result_temp10, i32 0, i32 1
  %val15 = load i64, ptr %val_ptr14, align 4
  %unwrap_result16 = select i1 %is_success13, i64 %val15, i64 0
  store i64 %unwrap_result16, ptr %abs_val, align 4
  %calltmp17 = call %result_int8 @sign(i64 -15)
  %result_temp18 = alloca %result_int8, align 8
  store %result_int8 %calltmp17, ptr %result_temp18, align 1
  %err_ptr19 = getelementptr inbounds %result_int8, ptr %result_temp18, i32 0, i32 0
  %err20 = load i8, ptr %err_ptr19, align 1
  %is_success21 = icmp eq i8 %err20, 0
  %val_ptr22 = getelementptr inbounds %result_int8, ptr %result_temp18, i32 0, i32 1
  %val23 = load i8, ptr %val_ptr22, align 1
  %unwrap_result24 = select i1 %is_success21, i8 %val23, i8 0
  store i8 %unwrap_result24, ptr %sign_val, align 1
  call void @puts(ptr @3)
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr25 = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr25, align 1
  %val_ptr26 = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 0, ptr %val_ptr26, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val
}

define internal %result_int8 @test_advanced_math() {
entry:
  %sqrt_val = alloca i64, align 8
  %lcm_val = alloca i64, align 8
  %gcd_val = alloca i64, align 8
  %fib_val = alloca i64, align 8
  %fact_val = alloca i64, align 8
  %pow_val = alloca i64, align 8
  call void @puts(ptr @4)
  %calltmp = call %result_int64 @pow(i64 2, i64 10)
  %result_temp = alloca %result_int64, align 8
  store %result_int64 %calltmp, ptr %result_temp, align 4
  %err_ptr = getelementptr inbounds %result_int64, ptr %result_temp, i32 0, i32 0
  %err = load i8, ptr %err_ptr, align 1
  %is_success = icmp eq i8 %err, 0
  %val_ptr = getelementptr inbounds %result_int64, ptr %result_temp, i32 0, i32 1
  %val = load i64, ptr %val_ptr, align 4
  %unwrap_result = select i1 %is_success, i64 %val, i64 0
  store i64 %unwrap_result, ptr %pow_val, align 4
  %calltmp1 = call %result_int64 @factorial(i64 5)
  %result_temp2 = alloca %result_int64, align 8
  store %result_int64 %calltmp1, ptr %result_temp2, align 4
  %err_ptr3 = getelementptr inbounds %result_int64, ptr %result_temp2, i32 0, i32 0
  %err4 = load i8, ptr %err_ptr3, align 1
  %is_success5 = icmp eq i8 %err4, 0
  %val_ptr6 = getelementptr inbounds %result_int64, ptr %result_temp2, i32 0, i32 1
  %val7 = load i64, ptr %val_ptr6, align 4
  %unwrap_result8 = select i1 %is_success5, i64 %val7, i64 0
  store i64 %unwrap_result8, ptr %fact_val, align 4
  %calltmp9 = call %result_int64 @fibonacci(i64 10)
  %result_temp10 = alloca %result_int64, align 8
  store %result_int64 %calltmp9, ptr %result_temp10, align 4
  %err_ptr11 = getelementptr inbounds %result_int64, ptr %result_temp10, i32 0, i32 0
  %err12 = load i8, ptr %err_ptr11, align 1
  %is_success13 = icmp eq i8 %err12, 0
  %val_ptr14 = getelementptr inbounds %result_int64, ptr %result_temp10, i32 0, i32 1
  %val15 = load i64, ptr %val_ptr14, align 4
  %unwrap_result16 = select i1 %is_success13, i64 %val15, i64 0
  store i64 %unwrap_result16, ptr %fib_val, align 4
  %calltmp17 = call %result_int64 @gcd(i64 48, i64 18)
  %result_temp18 = alloca %result_int64, align 8
  store %result_int64 %calltmp17, ptr %result_temp18, align 4
  %err_ptr19 = getelementptr inbounds %result_int64, ptr %result_temp18, i32 0, i32 0
  %err20 = load i8, ptr %err_ptr19, align 1
  %is_success21 = icmp eq i8 %err20, 0
  %val_ptr22 = getelementptr inbounds %result_int64, ptr %result_temp18, i32 0, i32 1
  %val23 = load i64, ptr %val_ptr22, align 4
  %unwrap_result24 = select i1 %is_success21, i64 %val23, i64 0
  store i64 %unwrap_result24, ptr %gcd_val, align 4
  %calltmp25 = call %result_int64 @lcm(i64 12, i64 18)
  %result_temp26 = alloca %result_int64, align 8
  store %result_int64 %calltmp25, ptr %result_temp26, align 4
  %err_ptr27 = getelementptr inbounds %result_int64, ptr %result_temp26, i32 0, i32 0
  %err28 = load i8, ptr %err_ptr27, align 1
  %is_success29 = icmp eq i8 %err28, 0
  %val_ptr30 = getelementptr inbounds %result_int64, ptr %result_temp26, i32 0, i32 1
  %val31 = load i64, ptr %val_ptr30, align 4
  %unwrap_result32 = select i1 %is_success29, i64 %val31, i64 0
  store i64 %unwrap_result32, ptr %lcm_val, align 4
  %calltmp33 = call %result_int64 @isqrt(i64 144)
  %result_temp34 = alloca %result_int64, align 8
  store %result_int64 %calltmp33, ptr %result_temp34, align 4
  %err_ptr35 = getelementptr inbounds %result_int64, ptr %result_temp34, i32 0, i32 0
  %err36 = load i8, ptr %err_ptr35, align 1
  %is_success37 = icmp eq i8 %err36, 0
  %val_ptr38 = getelementptr inbounds %result_int64, ptr %result_temp34, i32 0, i32 1
  %val39 = load i64, ptr %val_ptr38, align 4
  %unwrap_result40 = select i1 %is_success37, i64 %val39, i64 0
  store i64 %unwrap_result40, ptr %sqrt_val, align 4
  call void @puts(ptr @5)
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr41 = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr41, align 1
  %val_ptr42 = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 0, ptr %val_ptr42, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val
}

define internal %result_int8 @__user_main() {
entry:
  call void @puts(ptr @6)
  call void @puts(ptr @7)
  %calltmp = call %result_int8 @test_basic_ops()
  %result_temp = alloca %result_int8, align 8
  store %result_int8 %calltmp, ptr %result_temp, align 1
  %err_ptr = getelementptr inbounds %result_int8, ptr %result_temp, i32 0, i32 0
  %err = load i8, ptr %err_ptr, align 1
  %is_success = icmp eq i8 %err, 0
  %val_ptr = getelementptr inbounds %result_int8, ptr %result_temp, i32 0, i32 1
  %val = load i8, ptr %val_ptr, align 1
  %unwrap_result = select i1 %is_success, i8 %val, i8 -1
  call void @puts(ptr @8)
  %calltmp1 = call %result_int8 @test_comparison_ops()
  %result_temp2 = alloca %result_int8, align 8
  store %result_int8 %calltmp1, ptr %result_temp2, align 1
  %err_ptr3 = getelementptr inbounds %result_int8, ptr %result_temp2, i32 0, i32 0
  %err4 = load i8, ptr %err_ptr3, align 1
  %is_success5 = icmp eq i8 %err4, 0
  %val_ptr6 = getelementptr inbounds %result_int8, ptr %result_temp2, i32 0, i32 1
  %val7 = load i8, ptr %val_ptr6, align 1
  %unwrap_result8 = select i1 %is_success5, i8 %val7, i8 -1
  call void @puts(ptr @9)
  %calltmp9 = call %result_int8 @test_advanced_math()
  %result_temp10 = alloca %result_int8, align 8
  store %result_int8 %calltmp9, ptr %result_temp10, align 1
  %err_ptr11 = getelementptr inbounds %result_int8, ptr %result_temp10, i32 0, i32 0
  %err12 = load i8, ptr %err_ptr11, align 1
  %is_success13 = icmp eq i8 %err12, 0
  %val_ptr14 = getelementptr inbounds %result_int8, ptr %result_temp10, i32 0, i32 1
  %val15 = load i8, ptr %val_ptr14, align 1
  %unwrap_result16 = select i1 %is_success13, i8 %val15, i8 -1
  call void @puts(ptr @10)
  call void @puts(ptr @11)
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr17 = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr17, align 1
  %val_ptr18 = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 0, ptr %val_ptr18, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val
}

define i64 @main() {
entry:
  call void @__aria_module_init()
  %0 = call %result_int8 @__user_main()
  ret i64 0
}
