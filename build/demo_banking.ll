; ModuleID = 'aria_module'
source_filename = "aria_module"

%result_int64 = type { i8, i64 }
%result_int8 = type { i8, i8 }

@0 = private unnamed_addr constant [32 x i8] c"Processing transaction batch...\00", align 1
@1 = private unnamed_addr constant [27 x i8] c"Testing error scenarios...\00", align 1
@2 = private unnamed_addr constant [33 x i8] c"Testing interest calculations...\00", align 1
@3 = private unnamed_addr constant [29 x i8] c"Testing loan calculations...\00", align 1
@4 = private unnamed_addr constant [42 x i8] c"=== Banking System Comprehensive Demo ===\00", align 1
@5 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@6 = private unnamed_addr constant [27 x i8] c"Transaction batch complete\00", align 1
@7 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@8 = private unnamed_addr constant [30 x i8] c"Error scenario tests complete\00", align 1
@9 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@10 = private unnamed_addr constant [36 x i8] c"Interest calculation tests complete\00", align 1
@11 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@12 = private unnamed_addr constant [32 x i8] c"Loan calculation tests complete\00", align 1
@13 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@14 = private unnamed_addr constant [38 x i8] c"Monthly statement processing complete\00", align 1
@15 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@16 = private unnamed_addr constant [31 x i8] c"Account status checks complete\00", align 1
@17 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@18 = private unnamed_addr constant [34 x i8] c"=== All Banking Tests Passed! ===\00", align 1

declare void @puts(ptr)

declare void @print(ptr)

define internal void @__aria_module_init() {
entry:
  ret void
}

define internal %result_int64 @create_account(i64 %initial_balance) {
entry:
  %initial_balance1 = alloca i64, align 8
  store i64 %initial_balance, ptr %initial_balance1, align 4
  %0 = load i64, ptr %initial_balance1, align 4
  %pick_lt = icmp slt i64 %0, 0
  br i1 %pick_lt, label %case_body_0, label %case_next_0

case_body_0:                                      ; preds = %entry
  %result = alloca %result_int64, align 8
  %err_ptr = getelementptr inbounds %result_int64, ptr %result, i32 0, i32 0
  store i8 3, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int64, ptr %result, i32 0, i32 1
  store i64 0, ptr %val_ptr, align 4
  %result_val = load %result_int64, ptr %result, align 4
  ret %result_int64 %result_val

case_next_0:                                      ; preds = %entry
  br i1 true, label %case_body_1, label %case_next_1

case_body_1:                                      ; preds = %case_next_0
  %1 = load i64, ptr %initial_balance1, align 4
  %result2 = alloca %result_int64, align 8
  %err_ptr3 = getelementptr inbounds %result_int64, ptr %result2, i32 0, i32 0
  store i8 0, ptr %err_ptr3, align 1
  %val_ptr4 = getelementptr inbounds %result_int64, ptr %result2, i32 0, i32 1
  store i64 %1, ptr %val_ptr4, align 4
  %result_val5 = load %result_int64, ptr %result2, align 4
  ret %result_int64 %result_val5

case_next_1:                                      ; preds = %case_next_0
  br label %pick_done

pick_done:                                        ; preds = %case_next_1
  %result6 = alloca %result_int64, align 8
  %err_ptr7 = getelementptr inbounds %result_int64, ptr %result6, i32 0, i32 0
  store i8 0, ptr %err_ptr7, align 1
  %val_ptr8 = getelementptr inbounds %result_int64, ptr %result6, i32 0, i32 1
  store i64 0, ptr %val_ptr8, align 4
  %result_val9 = load %result_int64, ptr %result6, align 4
  ret %result_int64 %result_val9
}

define internal %result_int64 @validate_amount(i64 %amount) {
entry:
  %amount1 = alloca i64, align 8
  store i64 %amount, ptr %amount1, align 4
  %0 = load i64, ptr %amount1, align 4
  %pick_le = icmp sle i64 %0, 0
  br i1 %pick_le, label %case_body_0, label %case_next_0

case_body_0:                                      ; preds = %entry
  %result = alloca %result_int64, align 8
  %err_ptr = getelementptr inbounds %result_int64, ptr %result, i32 0, i32 0
  store i8 3, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int64, ptr %result, i32 0, i32 1
  store i64 0, ptr %val_ptr, align 4
  %result_val = load %result_int64, ptr %result, align 4
  ret %result_int64 %result_val

case_next_0:                                      ; preds = %entry
  %pick_gt = icmp sgt i64 %0, 10000
  br i1 %pick_gt, label %case_body_1, label %case_next_1

case_body_1:                                      ; preds = %case_next_0
  %result2 = alloca %result_int64, align 8
  %err_ptr3 = getelementptr inbounds %result_int64, ptr %result2, i32 0, i32 0
  store i8 4, ptr %err_ptr3, align 1
  %val_ptr4 = getelementptr inbounds %result_int64, ptr %result2, i32 0, i32 1
  store i64 0, ptr %val_ptr4, align 4
  %result_val5 = load %result_int64, ptr %result2, align 4
  ret %result_int64 %result_val5

case_next_1:                                      ; preds = %case_next_0
  br i1 true, label %case_body_2, label %case_next_2

case_body_2:                                      ; preds = %case_next_1
  %1 = load i64, ptr %amount1, align 4
  %result6 = alloca %result_int64, align 8
  %err_ptr7 = getelementptr inbounds %result_int64, ptr %result6, i32 0, i32 0
  store i8 0, ptr %err_ptr7, align 1
  %val_ptr8 = getelementptr inbounds %result_int64, ptr %result6, i32 0, i32 1
  store i64 %1, ptr %val_ptr8, align 4
  %result_val9 = load %result_int64, ptr %result6, align 4
  ret %result_int64 %result_val9

case_next_2:                                      ; preds = %case_next_1
  br label %pick_done

pick_done:                                        ; preds = %case_next_2
  %result10 = alloca %result_int64, align 8
  %err_ptr11 = getelementptr inbounds %result_int64, ptr %result10, i32 0, i32 0
  store i8 3, ptr %err_ptr11, align 1
  %val_ptr12 = getelementptr inbounds %result_int64, ptr %result10, i32 0, i32 1
  store i64 0, ptr %val_ptr12, align 4
  %result_val13 = load %result_int64, ptr %result10, align 4
  ret %result_int64 %result_val13
}

define internal %result_int64 @deposit(i64 %current_balance, i64 %amount) {
entry:
  %new_balance = alloca i64, align 8
  %valid_amount = alloca i64, align 8
  %current_balance1 = alloca i64, align 8
  store i64 %current_balance, ptr %current_balance1, align 4
  %amount2 = alloca i64, align 8
  store i64 %amount, ptr %amount2, align 4
  %0 = load i64, ptr %amount2, align 4
  %calltmp = call %result_int64 @validate_amount(i64 %0)
  %result_temp = alloca %result_int64, align 8
  store %result_int64 %calltmp, ptr %result_temp, align 4
  %err_ptr = getelementptr inbounds %result_int64, ptr %result_temp, i32 0, i32 0
  %err = load i8, ptr %err_ptr, align 1
  %is_success = icmp eq i8 %err, 0
  %val_ptr = getelementptr inbounds %result_int64, ptr %result_temp, i32 0, i32 1
  %val = load i64, ptr %val_ptr, align 4
  %unwrap_result = select i1 %is_success, i64 %val, i64 0
  store i64 %unwrap_result, ptr %valid_amount, align 4
  %1 = load i64, ptr %valid_amount, align 4
  %pick_eq = icmp eq i64 %1, 0
  br i1 %pick_eq, label %case_body_0, label %case_next_0

case_body_0:                                      ; preds = %entry
  %2 = load i64, ptr %current_balance1, align 4
  %result = alloca %result_int64, align 8
  %err_ptr3 = getelementptr inbounds %result_int64, ptr %result, i32 0, i32 0
  store i8 3, ptr %err_ptr3, align 1
  %val_ptr4 = getelementptr inbounds %result_int64, ptr %result, i32 0, i32 1
  store i64 %2, ptr %val_ptr4, align 4
  %result_val = load %result_int64, ptr %result, align 4
  ret %result_int64 %result_val

case_next_0:                                      ; preds = %entry
  br i1 true, label %case_body_1, label %case_next_1

case_body_1:                                      ; preds = %case_next_0
  %3 = load i64, ptr %current_balance1, align 4
  %4 = load i64, ptr %valid_amount, align 4
  %addtmp = add i64 %3, %4
  store i64 %addtmp, ptr %new_balance, align 4
  %5 = load i64, ptr %new_balance, align 4
  %result5 = alloca %result_int64, align 8
  %err_ptr6 = getelementptr inbounds %result_int64, ptr %result5, i32 0, i32 0
  store i8 0, ptr %err_ptr6, align 1
  %val_ptr7 = getelementptr inbounds %result_int64, ptr %result5, i32 0, i32 1
  store i64 %5, ptr %val_ptr7, align 4
  %result_val8 = load %result_int64, ptr %result5, align 4
  ret %result_int64 %result_val8

case_next_1:                                      ; preds = %case_next_0
  br label %pick_done

pick_done:                                        ; preds = %case_next_1
  %6 = load i64, ptr %current_balance1, align 4
  %result9 = alloca %result_int64, align 8
  %err_ptr10 = getelementptr inbounds %result_int64, ptr %result9, i32 0, i32 0
  store i8 0, ptr %err_ptr10, align 1
  %val_ptr11 = getelementptr inbounds %result_int64, ptr %result9, i32 0, i32 1
  store i64 %6, ptr %val_ptr11, align 4
  %result_val12 = load %result_int64, ptr %result9, align 4
  ret %result_int64 %result_val12
}

define internal %result_int64 @withdraw(i64 %current_balance, i64 %amount) {
entry:
  %new_balance = alloca i64, align 8
  %valid_amount = alloca i64, align 8
  %current_balance1 = alloca i64, align 8
  store i64 %current_balance, ptr %current_balance1, align 4
  %amount2 = alloca i64, align 8
  store i64 %amount, ptr %amount2, align 4
  %0 = load i64, ptr %amount2, align 4
  %calltmp = call %result_int64 @validate_amount(i64 %0)
  %result_temp = alloca %result_int64, align 8
  store %result_int64 %calltmp, ptr %result_temp, align 4
  %err_ptr = getelementptr inbounds %result_int64, ptr %result_temp, i32 0, i32 0
  %err = load i8, ptr %err_ptr, align 1
  %is_success = icmp eq i8 %err, 0
  %val_ptr = getelementptr inbounds %result_int64, ptr %result_temp, i32 0, i32 1
  %val = load i64, ptr %val_ptr, align 4
  %unwrap_result = select i1 %is_success, i64 %val, i64 0
  store i64 %unwrap_result, ptr %valid_amount, align 4
  %1 = load i64, ptr %valid_amount, align 4
  %pick_eq = icmp eq i64 %1, 0
  br i1 %pick_eq, label %case_body_0, label %case_next_0

case_body_0:                                      ; preds = %entry
  %2 = load i64, ptr %current_balance1, align 4
  %result = alloca %result_int64, align 8
  %err_ptr3 = getelementptr inbounds %result_int64, ptr %result, i32 0, i32 0
  store i8 3, ptr %err_ptr3, align 1
  %val_ptr4 = getelementptr inbounds %result_int64, ptr %result, i32 0, i32 1
  store i64 %2, ptr %val_ptr4, align 4
  %result_val = load %result_int64, ptr %result, align 4
  ret %result_int64 %result_val

case_next_0:                                      ; preds = %entry
  br i1 true, label %case_body_1, label %case_next_120

case_body_1:                                      ; preds = %case_next_0
  %3 = load i64, ptr %current_balance1, align 4
  %4 = load i64, ptr %amount2, align 4
  %pick_lt = icmp slt i64 %3, %4
  br i1 %pick_lt, label %case_body_05, label %case_next_010

case_body_05:                                     ; preds = %case_body_1
  %5 = load i64, ptr %current_balance1, align 4
  %result6 = alloca %result_int64, align 8
  %err_ptr7 = getelementptr inbounds %result_int64, ptr %result6, i32 0, i32 0
  store i8 1, ptr %err_ptr7, align 1
  %val_ptr8 = getelementptr inbounds %result_int64, ptr %result6, i32 0, i32 1
  store i64 %5, ptr %val_ptr8, align 4
  %result_val9 = load %result_int64, ptr %result6, align 4
  ret %result_int64 %result_val9

case_next_010:                                    ; preds = %case_body_1
  br i1 true, label %case_body_111, label %case_next_1

case_body_111:                                    ; preds = %case_next_010
  %6 = load i64, ptr %current_balance1, align 4
  %7 = load i64, ptr %valid_amount, align 4
  %subtmp = sub i64 %6, %7
  store i64 %subtmp, ptr %new_balance, align 4
  %8 = load i64, ptr %new_balance, align 4
  %result12 = alloca %result_int64, align 8
  %err_ptr13 = getelementptr inbounds %result_int64, ptr %result12, i32 0, i32 0
  store i8 0, ptr %err_ptr13, align 1
  %val_ptr14 = getelementptr inbounds %result_int64, ptr %result12, i32 0, i32 1
  store i64 %8, ptr %val_ptr14, align 4
  %result_val15 = load %result_int64, ptr %result12, align 4
  ret %result_int64 %result_val15

case_next_1:                                      ; preds = %case_next_010
  br label %pick_done

pick_done:                                        ; preds = %case_next_1
  %9 = load i64, ptr %current_balance1, align 4
  %result16 = alloca %result_int64, align 8
  %err_ptr17 = getelementptr inbounds %result_int64, ptr %result16, i32 0, i32 0
  store i8 1, ptr %err_ptr17, align 1
  %val_ptr18 = getelementptr inbounds %result_int64, ptr %result16, i32 0, i32 1
  store i64 %9, ptr %val_ptr18, align 4
  %result_val19 = load %result_int64, ptr %result16, align 4
  ret %result_int64 %result_val19

case_next_120:                                    ; preds = %case_next_0
  br label %pick_done21

pick_done21:                                      ; preds = %case_next_120
  %10 = load i64, ptr %current_balance1, align 4
  %result22 = alloca %result_int64, align 8
  %err_ptr23 = getelementptr inbounds %result_int64, ptr %result22, i32 0, i32 0
  store i8 0, ptr %err_ptr23, align 1
  %val_ptr24 = getelementptr inbounds %result_int64, ptr %result22, i32 0, i32 1
  store i64 %10, ptr %val_ptr24, align 4
  %result_val25 = load %result_int64, ptr %result22, align 4
  ret %result_int64 %result_val25
}

define internal %result_int64 @transfer(i64 %sender_balance, i64 %receiver_balance, i64 %amount) {
entry:
  %new_sender = alloca i64, align 8
  %sender_balance1 = alloca i64, align 8
  store i64 %sender_balance, ptr %sender_balance1, align 4
  %receiver_balance2 = alloca i64, align 8
  store i64 %receiver_balance, ptr %receiver_balance2, align 4
  %amount3 = alloca i64, align 8
  store i64 %amount, ptr %amount3, align 4
  %0 = load i64, ptr %sender_balance1, align 4
  %1 = load i64, ptr %amount3, align 4
  %calltmp = call %result_int64 @withdraw(i64 %0, i64 %1)
  %2 = load i64, ptr %sender_balance1, align 4
  %result_temp = alloca %result_int64, align 8
  store %result_int64 %calltmp, ptr %result_temp, align 4
  %err_ptr = getelementptr inbounds %result_int64, ptr %result_temp, i32 0, i32 0
  %err = load i8, ptr %err_ptr, align 1
  %is_success = icmp eq i8 %err, 0
  %val_ptr = getelementptr inbounds %result_int64, ptr %result_temp, i32 0, i32 1
  %val = load i64, ptr %val_ptr, align 4
  %unwrap_result = select i1 %is_success, i64 %val, i64 %2
  store i64 %unwrap_result, ptr %new_sender, align 4
  %3 = load i64, ptr %new_sender, align 4
  %4 = load i64, ptr %sender_balance1, align 4
  %pick_eq = icmp eq i64 %3, %4
  br i1 %pick_eq, label %case_body_0, label %case_next_0

case_body_0:                                      ; preds = %entry
  %5 = load i64, ptr %sender_balance1, align 4
  %result = alloca %result_int64, align 8
  %err_ptr4 = getelementptr inbounds %result_int64, ptr %result, i32 0, i32 0
  store i8 1, ptr %err_ptr4, align 1
  %val_ptr5 = getelementptr inbounds %result_int64, ptr %result, i32 0, i32 1
  store i64 %5, ptr %val_ptr5, align 4
  %result_val = load %result_int64, ptr %result, align 4
  ret %result_int64 %result_val

case_next_0:                                      ; preds = %entry
  br i1 true, label %case_body_1, label %case_next_1

case_body_1:                                      ; preds = %case_next_0
  %6 = load i64, ptr %new_sender, align 4
  %result6 = alloca %result_int64, align 8
  %err_ptr7 = getelementptr inbounds %result_int64, ptr %result6, i32 0, i32 0
  store i8 0, ptr %err_ptr7, align 1
  %val_ptr8 = getelementptr inbounds %result_int64, ptr %result6, i32 0, i32 1
  store i64 %6, ptr %val_ptr8, align 4
  %result_val9 = load %result_int64, ptr %result6, align 4
  ret %result_int64 %result_val9

case_next_1:                                      ; preds = %case_next_0
  br label %pick_done

pick_done:                                        ; preds = %case_next_1
  %7 = load i64, ptr %sender_balance1, align 4
  %result10 = alloca %result_int64, align 8
  %err_ptr11 = getelementptr inbounds %result_int64, ptr %result10, i32 0, i32 0
  store i8 1, ptr %err_ptr11, align 1
  %val_ptr12 = getelementptr inbounds %result_int64, ptr %result10, i32 0, i32 1
  store i64 %7, ptr %val_ptr12, align 4
  %result_val13 = load %result_int64, ptr %result10, align 4
  ret %result_int64 %result_val13
}

define internal %result_int64 @calculate_interest(i64 %balance, i64 %rate_percent) {
entry:
  %new_balance = alloca i64, align 8
  %interest = alloca i64, align 8
  %balance1 = alloca i64, align 8
  store i64 %balance, ptr %balance1, align 4
  %rate_percent2 = alloca i64, align 8
  store i64 %rate_percent, ptr %rate_percent2, align 4
  %0 = load i64, ptr %balance1, align 4
  %1 = load i64, ptr %rate_percent2, align 4
  %multmp = mul i64 %0, %1
  %divtmp = sdiv i64 %multmp, 100
  store i64 %divtmp, ptr %interest, align 4
  %2 = load i64, ptr %balance1, align 4
  %3 = load i64, ptr %interest, align 4
  %addtmp = add i64 %2, %3
  store i64 %addtmp, ptr %new_balance, align 4
  %4 = load i64, ptr %new_balance, align 4
  %result = alloca %result_int64, align 8
  %err_ptr = getelementptr inbounds %result_int64, ptr %result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int64, ptr %result, i32 0, i32 1
  store i64 %4, ptr %val_ptr, align 4
  %result_val = load %result_int64, ptr %result, align 4
  ret %result_int64 %result_val
}

define internal %result_int64 @process_monthly_statement(i64 %starting_balance, i64 %num_transactions) {
entry:
  %count = alloca i64, align 8
  %current = alloca i64, align 8
  %starting_balance1 = alloca i64, align 8
  store i64 %starting_balance, ptr %starting_balance1, align 4
  %num_transactions2 = alloca i64, align 8
  store i64 %num_transactions, ptr %num_transactions2, align 4
  %0 = load i64, ptr %starting_balance1, align 4
  store i64 %0, ptr %current, align 4
  store i64 0, ptr %count, align 4
  br label %while_cond

while_cond:                                       ; preds = %while_body, %entry
  %1 = load i64, ptr %count, align 4
  %2 = load i64, ptr %num_transactions2, align 4
  %lttmp = icmp slt i64 %1, %2
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %3 = load i64, ptr %current, align 4
  %calltmp = call %result_int64 @calculate_interest(i64 %3, i64 2)
  %4 = load i64, ptr %current, align 4
  %result_temp = alloca %result_int64, align 8
  store %result_int64 %calltmp, ptr %result_temp, align 4
  %err_ptr = getelementptr inbounds %result_int64, ptr %result_temp, i32 0, i32 0
  %err = load i8, ptr %err_ptr, align 1
  %is_success = icmp eq i8 %err, 0
  %val_ptr = getelementptr inbounds %result_int64, ptr %result_temp, i32 0, i32 1
  %val = load i64, ptr %val_ptr, align 4
  %unwrap_result = select i1 %is_success, i64 %val, i64 %4
  store i64 %unwrap_result, ptr %current, align 4
  %5 = load i64, ptr %count, align 4
  %addtmp = add i64 %5, 1
  store i64 %addtmp, ptr %count, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %6 = load i64, ptr %current, align 4
  %result = alloca %result_int64, align 8
  %err_ptr3 = getelementptr inbounds %result_int64, ptr %result, i32 0, i32 0
  store i8 0, ptr %err_ptr3, align 1
  %val_ptr4 = getelementptr inbounds %result_int64, ptr %result, i32 0, i32 1
  store i64 %6, ptr %val_ptr4, align 4
  %result_val = load %result_int64, ptr %result, align 4
  ret %result_int64 %result_val
}

define internal %result_int64 @calculate_loan_payment(i64 %principal, i64 %months, i64 %interest_rate) {
entry:
  %monthly_payment = alloca i64, align 8
  %total_with_interest = alloca i64, align 8
  %principal1 = alloca i64, align 8
  store i64 %principal, ptr %principal1, align 4
  %months2 = alloca i64, align 8
  store i64 %months, ptr %months2, align 4
  %interest_rate3 = alloca i64, align 8
  store i64 %interest_rate, ptr %interest_rate3, align 4
  %0 = load i64, ptr %principal1, align 4
  %1 = load i64, ptr %interest_rate3, align 4
  %addtmp = add i64 100, %1
  %multmp = mul i64 %0, %addtmp
  %divtmp = sdiv i64 %multmp, 100
  store i64 %divtmp, ptr %total_with_interest, align 4
  %2 = load i64, ptr %total_with_interest, align 4
  %3 = load i64, ptr %months2, align 4
  %divtmp4 = sdiv i64 %2, %3
  store i64 %divtmp4, ptr %monthly_payment, align 4
  %4 = load i64, ptr %monthly_payment, align 4
  %result = alloca %result_int64, align 8
  %err_ptr = getelementptr inbounds %result_int64, ptr %result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int64, ptr %result, i32 0, i32 1
  store i64 %4, ptr %val_ptr, align 4
  %result_val = load %result_int64, ptr %result, align 4
  ret %result_int64 %result_val
}

define internal %result_int8 @check_account_status(i64 %balance, i64 %overdraft_limit) {
entry:
  %effective_limit = alloca i64, align 8
  %balance1 = alloca i64, align 8
  store i64 %balance, ptr %balance1, align 4
  %overdraft_limit2 = alloca i64, align 8
  store i64 %overdraft_limit, ptr %overdraft_limit2, align 4
  %0 = load i64, ptr %overdraft_limit2, align 4
  %subtmp = sub i64 0, %0
  store i64 %subtmp, ptr %effective_limit, align 4
  %1 = load i64, ptr %balance1, align 4
  %2 = load i64, ptr %effective_limit, align 4
  %pick_lt = icmp slt i64 %1, %2
  br i1 %pick_lt, label %case_body_0, label %case_next_0

case_body_0:                                      ; preds = %entry
  %result = alloca %result_int8, align 8
  %err_ptr = getelementptr inbounds %result_int8, ptr %result, i32 0, i32 0
  store i8 5, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int8, ptr %result, i32 0, i32 1
  store i8 0, ptr %val_ptr, align 1
  %result_val = load %result_int8, ptr %result, align 1
  ret %result_int8 %result_val

case_next_0:                                      ; preds = %entry
  %pick_lt3 = icmp slt i64 %1, 0
  br i1 %pick_lt3, label %case_body_1, label %case_next_1

case_body_1:                                      ; preds = %case_next_0
  %result4 = alloca %result_int8, align 8
  %err_ptr5 = getelementptr inbounds %result_int8, ptr %result4, i32 0, i32 0
  store i8 0, ptr %err_ptr5, align 1
  %val_ptr6 = getelementptr inbounds %result_int8, ptr %result4, i32 0, i32 1
  store i8 1, ptr %val_ptr6, align 1
  %result_val7 = load %result_int8, ptr %result4, align 1
  ret %result_int8 %result_val7

case_next_1:                                      ; preds = %case_next_0
  br i1 true, label %case_body_2, label %case_next_2

case_body_2:                                      ; preds = %case_next_1
  %result8 = alloca %result_int8, align 8
  %err_ptr9 = getelementptr inbounds %result_int8, ptr %result8, i32 0, i32 0
  store i8 0, ptr %err_ptr9, align 1
  %val_ptr10 = getelementptr inbounds %result_int8, ptr %result8, i32 0, i32 1
  store i8 2, ptr %val_ptr10, align 1
  %result_val11 = load %result_int8, ptr %result8, align 1
  ret %result_int8 %result_val11

case_next_2:                                      ; preds = %case_next_1
  br label %pick_done

pick_done:                                        ; preds = %case_next_2
  %result12 = alloca %result_int8, align 8
  %err_ptr13 = getelementptr inbounds %result_int8, ptr %result12, i32 0, i32 0
  store i8 0, ptr %err_ptr13, align 1
  %val_ptr14 = getelementptr inbounds %result_int8, ptr %result12, i32 0, i32 1
  store i8 0, ptr %val_ptr14, align 1
  %result_val15 = load %result_int8, ptr %result12, align 1
  ret %result_int8 %result_val15
}

define internal %result_int64 @run_transaction_batch() {
entry:
  %total = alloca i64, align 8
  %acc2 = alloca i64, align 8
  %acc1 = alloca i64, align 8
  call void @puts(ptr @0)
  %calltmp = call %result_int64 @create_account(i64 1000)
  %result_temp = alloca %result_int64, align 8
  store %result_int64 %calltmp, ptr %result_temp, align 4
  %err_ptr = getelementptr inbounds %result_int64, ptr %result_temp, i32 0, i32 0
  %err = load i8, ptr %err_ptr, align 1
  %is_success = icmp eq i8 %err, 0
  %val_ptr = getelementptr inbounds %result_int64, ptr %result_temp, i32 0, i32 1
  %val = load i64, ptr %val_ptr, align 4
  %unwrap_result = select i1 %is_success, i64 %val, i64 0
  store i64 %unwrap_result, ptr %acc1, align 4
  %calltmp1 = call %result_int64 @create_account(i64 500)
  %result_temp2 = alloca %result_int64, align 8
  store %result_int64 %calltmp1, ptr %result_temp2, align 4
  %err_ptr3 = getelementptr inbounds %result_int64, ptr %result_temp2, i32 0, i32 0
  %err4 = load i8, ptr %err_ptr3, align 1
  %is_success5 = icmp eq i8 %err4, 0
  %val_ptr6 = getelementptr inbounds %result_int64, ptr %result_temp2, i32 0, i32 1
  %val7 = load i64, ptr %val_ptr6, align 4
  %unwrap_result8 = select i1 %is_success5, i64 %val7, i64 0
  store i64 %unwrap_result8, ptr %acc2, align 4
  %0 = load i64, ptr %acc1, align 4
  %calltmp9 = call %result_int64 @deposit(i64 %0, i64 200)
  %1 = load i64, ptr %acc1, align 4
  %result_temp10 = alloca %result_int64, align 8
  store %result_int64 %calltmp9, ptr %result_temp10, align 4
  %err_ptr11 = getelementptr inbounds %result_int64, ptr %result_temp10, i32 0, i32 0
  %err12 = load i8, ptr %err_ptr11, align 1
  %is_success13 = icmp eq i8 %err12, 0
  %val_ptr14 = getelementptr inbounds %result_int64, ptr %result_temp10, i32 0, i32 1
  %val15 = load i64, ptr %val_ptr14, align 4
  %unwrap_result16 = select i1 %is_success13, i64 %val15, i64 %1
  store i64 %unwrap_result16, ptr %acc1, align 4
  %2 = load i64, ptr %acc1, align 4
  %calltmp17 = call %result_int64 @deposit(i64 %2, i64 150)
  %3 = load i64, ptr %acc1, align 4
  %result_temp18 = alloca %result_int64, align 8
  store %result_int64 %calltmp17, ptr %result_temp18, align 4
  %err_ptr19 = getelementptr inbounds %result_int64, ptr %result_temp18, i32 0, i32 0
  %err20 = load i8, ptr %err_ptr19, align 1
  %is_success21 = icmp eq i8 %err20, 0
  %val_ptr22 = getelementptr inbounds %result_int64, ptr %result_temp18, i32 0, i32 1
  %val23 = load i64, ptr %val_ptr22, align 4
  %unwrap_result24 = select i1 %is_success21, i64 %val23, i64 %3
  store i64 %unwrap_result24, ptr %acc1, align 4
  %4 = load i64, ptr %acc1, align 4
  %calltmp25 = call %result_int64 @withdraw(i64 %4, i64 100)
  %5 = load i64, ptr %acc1, align 4
  %result_temp26 = alloca %result_int64, align 8
  store %result_int64 %calltmp25, ptr %result_temp26, align 4
  %err_ptr27 = getelementptr inbounds %result_int64, ptr %result_temp26, i32 0, i32 0
  %err28 = load i8, ptr %err_ptr27, align 1
  %is_success29 = icmp eq i8 %err28, 0
  %val_ptr30 = getelementptr inbounds %result_int64, ptr %result_temp26, i32 0, i32 1
  %val31 = load i64, ptr %val_ptr30, align 4
  %unwrap_result32 = select i1 %is_success29, i64 %val31, i64 %5
  store i64 %unwrap_result32, ptr %acc1, align 4
  %6 = load i64, ptr %acc2, align 4
  %calltmp33 = call %result_int64 @withdraw(i64 %6, i64 50)
  %7 = load i64, ptr %acc2, align 4
  %result_temp34 = alloca %result_int64, align 8
  store %result_int64 %calltmp33, ptr %result_temp34, align 4
  %err_ptr35 = getelementptr inbounds %result_int64, ptr %result_temp34, i32 0, i32 0
  %err36 = load i8, ptr %err_ptr35, align 1
  %is_success37 = icmp eq i8 %err36, 0
  %val_ptr38 = getelementptr inbounds %result_int64, ptr %result_temp34, i32 0, i32 1
  %val39 = load i64, ptr %val_ptr38, align 4
  %unwrap_result40 = select i1 %is_success37, i64 %val39, i64 %7
  store i64 %unwrap_result40, ptr %acc2, align 4
  %8 = load i64, ptr %acc1, align 4
  %9 = load i64, ptr %acc2, align 4
  %calltmp41 = call %result_int64 @transfer(i64 %8, i64 %9, i64 300)
  %10 = load i64, ptr %acc1, align 4
  %result_temp42 = alloca %result_int64, align 8
  store %result_int64 %calltmp41, ptr %result_temp42, align 4
  %err_ptr43 = getelementptr inbounds %result_int64, ptr %result_temp42, i32 0, i32 0
  %err44 = load i8, ptr %err_ptr43, align 1
  %is_success45 = icmp eq i8 %err44, 0
  %val_ptr46 = getelementptr inbounds %result_int64, ptr %result_temp42, i32 0, i32 1
  %val47 = load i64, ptr %val_ptr46, align 4
  %unwrap_result48 = select i1 %is_success45, i64 %val47, i64 %10
  store i64 %unwrap_result48, ptr %acc1, align 4
  %11 = load i64, ptr %acc1, align 4
  %12 = load i64, ptr %acc2, align 4
  %addtmp = add i64 %11, %12
  store i64 %addtmp, ptr %total, align 4
  %13 = load i64, ptr %total, align 4
  %auto_wrap_result = alloca %result_int64, align 8
  %err_ptr49 = getelementptr inbounds %result_int64, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr49, align 1
  %val_ptr50 = getelementptr inbounds %result_int64, ptr %auto_wrap_result, i32 0, i32 1
  store i64 %13, ptr %val_ptr50, align 4
  %result_val = load %result_int64, ptr %auto_wrap_result, align 4
  ret %result_int64 %result_val
}

define internal %result_int8 @test_error_scenarios() {
entry:
  %bad_acc = alloca i64, align 8
  %bal3 = alloca i64, align 8
  %bal2 = alloca i64, align 8
  %bal1 = alloca i64, align 8
  call void @puts(ptr @1)
  store i64 1000, ptr %bal1, align 4
  %0 = load i64, ptr %bal1, align 4
  %calltmp = call %result_int64 @deposit(i64 %0, i64 -50)
  %1 = load i64, ptr %bal1, align 4
  %result_temp = alloca %result_int64, align 8
  store %result_int64 %calltmp, ptr %result_temp, align 4
  %err_ptr = getelementptr inbounds %result_int64, ptr %result_temp, i32 0, i32 0
  %err = load i8, ptr %err_ptr, align 1
  %is_success = icmp eq i8 %err, 0
  %val_ptr = getelementptr inbounds %result_int64, ptr %result_temp, i32 0, i32 1
  %val = load i64, ptr %val_ptr, align 4
  %unwrap_result = select i1 %is_success, i64 %val, i64 %1
  store i64 %unwrap_result, ptr %bal1, align 4
  %2 = load i64, ptr %bal1, align 4
  %calltmp1 = call %result_int64 @withdraw(i64 %2, i64 0)
  %3 = load i64, ptr %bal1, align 4
  %result_temp2 = alloca %result_int64, align 8
  store %result_int64 %calltmp1, ptr %result_temp2, align 4
  %err_ptr3 = getelementptr inbounds %result_int64, ptr %result_temp2, i32 0, i32 0
  %err4 = load i8, ptr %err_ptr3, align 1
  %is_success5 = icmp eq i8 %err4, 0
  %val_ptr6 = getelementptr inbounds %result_int64, ptr %result_temp2, i32 0, i32 1
  %val7 = load i64, ptr %val_ptr6, align 4
  %unwrap_result8 = select i1 %is_success5, i64 %val7, i64 %3
  store i64 %unwrap_result8, ptr %bal1, align 4
  store i64 100, ptr %bal2, align 4
  %4 = load i64, ptr %bal2, align 4
  %calltmp9 = call %result_int64 @withdraw(i64 %4, i64 500)
  %5 = load i64, ptr %bal2, align 4
  %result_temp10 = alloca %result_int64, align 8
  store %result_int64 %calltmp9, ptr %result_temp10, align 4
  %err_ptr11 = getelementptr inbounds %result_int64, ptr %result_temp10, i32 0, i32 0
  %err12 = load i8, ptr %err_ptr11, align 1
  %is_success13 = icmp eq i8 %err12, 0
  %val_ptr14 = getelementptr inbounds %result_int64, ptr %result_temp10, i32 0, i32 1
  %val15 = load i64, ptr %val_ptr14, align 4
  %unwrap_result16 = select i1 %is_success13, i64 %val15, i64 %5
  store i64 %unwrap_result16, ptr %bal2, align 4
  store i64 50000, ptr %bal3, align 4
  %6 = load i64, ptr %bal3, align 4
  %calltmp17 = call %result_int64 @withdraw(i64 %6, i64 15000)
  %7 = load i64, ptr %bal3, align 4
  %result_temp18 = alloca %result_int64, align 8
  store %result_int64 %calltmp17, ptr %result_temp18, align 4
  %err_ptr19 = getelementptr inbounds %result_int64, ptr %result_temp18, i32 0, i32 0
  %err20 = load i8, ptr %err_ptr19, align 1
  %is_success21 = icmp eq i8 %err20, 0
  %val_ptr22 = getelementptr inbounds %result_int64, ptr %result_temp18, i32 0, i32 1
  %val23 = load i64, ptr %val_ptr22, align 4
  %unwrap_result24 = select i1 %is_success21, i64 %val23, i64 %7
  store i64 %unwrap_result24, ptr %bal3, align 4
  %calltmp25 = call %result_int64 @create_account(i64 -100)
  %result_temp26 = alloca %result_int64, align 8
  store %result_int64 %calltmp25, ptr %result_temp26, align 4
  %err_ptr27 = getelementptr inbounds %result_int64, ptr %result_temp26, i32 0, i32 0
  %err28 = load i8, ptr %err_ptr27, align 1
  %is_success29 = icmp eq i8 %err28, 0
  %val_ptr30 = getelementptr inbounds %result_int64, ptr %result_temp26, i32 0, i32 1
  %val31 = load i64, ptr %val_ptr30, align 4
  %unwrap_result32 = select i1 %is_success29, i64 %val31, i64 0
  store i64 %unwrap_result32, ptr %bad_acc, align 4
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr33 = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr33, align 1
  %val_ptr34 = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 0, ptr %val_ptr34, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val
}

define internal %result_int64 @test_interest() {
entry:
  %bal = alloca i64, align 8
  call void @puts(ptr @2)
  store i64 1000, ptr %bal, align 4
  %0 = load i64, ptr %bal, align 4
  %calltmp = call %result_int64 @calculate_interest(i64 %0, i64 5)
  %1 = load i64, ptr %bal, align 4
  %result_temp = alloca %result_int64, align 8
  store %result_int64 %calltmp, ptr %result_temp, align 4
  %err_ptr = getelementptr inbounds %result_int64, ptr %result_temp, i32 0, i32 0
  %err = load i8, ptr %err_ptr, align 1
  %is_success = icmp eq i8 %err, 0
  %val_ptr = getelementptr inbounds %result_int64, ptr %result_temp, i32 0, i32 1
  %val = load i64, ptr %val_ptr, align 4
  %unwrap_result = select i1 %is_success, i64 %val, i64 %1
  store i64 %unwrap_result, ptr %bal, align 4
  %2 = load i64, ptr %bal, align 4
  %calltmp1 = call %result_int64 @calculate_interest(i64 %2, i64 10)
  %3 = load i64, ptr %bal, align 4
  %result_temp2 = alloca %result_int64, align 8
  store %result_int64 %calltmp1, ptr %result_temp2, align 4
  %err_ptr3 = getelementptr inbounds %result_int64, ptr %result_temp2, i32 0, i32 0
  %err4 = load i8, ptr %err_ptr3, align 1
  %is_success5 = icmp eq i8 %err4, 0
  %val_ptr6 = getelementptr inbounds %result_int64, ptr %result_temp2, i32 0, i32 1
  %val7 = load i64, ptr %val_ptr6, align 4
  %unwrap_result8 = select i1 %is_success5, i64 %val7, i64 %3
  store i64 %unwrap_result8, ptr %bal, align 4
  %4 = load i64, ptr %bal, align 4
  %auto_wrap_result = alloca %result_int64, align 8
  %err_ptr9 = getelementptr inbounds %result_int64, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr9, align 1
  %val_ptr10 = getelementptr inbounds %result_int64, ptr %auto_wrap_result, i32 0, i32 1
  store i64 %4, ptr %val_ptr10, align 4
  %result_val = load %result_int64, ptr %auto_wrap_result, align 4
  ret %result_int64 %result_val
}

define internal %result_int64 @test_loans() {
entry:
  %payment2 = alloca i64, align 8
  %payment1 = alloca i64, align 8
  call void @puts(ptr @3)
  %calltmp = call %result_int64 @calculate_loan_payment(i64 10000, i64 12, i64 5)
  %result_temp = alloca %result_int64, align 8
  store %result_int64 %calltmp, ptr %result_temp, align 4
  %err_ptr = getelementptr inbounds %result_int64, ptr %result_temp, i32 0, i32 0
  %err = load i8, ptr %err_ptr, align 1
  %is_success = icmp eq i8 %err, 0
  %val_ptr = getelementptr inbounds %result_int64, ptr %result_temp, i32 0, i32 1
  %val = load i64, ptr %val_ptr, align 4
  %unwrap_result = select i1 %is_success, i64 %val, i64 0
  store i64 %unwrap_result, ptr %payment1, align 4
  %calltmp1 = call %result_int64 @calculate_loan_payment(i64 5000, i64 24, i64 3)
  %result_temp2 = alloca %result_int64, align 8
  store %result_int64 %calltmp1, ptr %result_temp2, align 4
  %err_ptr3 = getelementptr inbounds %result_int64, ptr %result_temp2, i32 0, i32 0
  %err4 = load i8, ptr %err_ptr3, align 1
  %is_success5 = icmp eq i8 %err4, 0
  %val_ptr6 = getelementptr inbounds %result_int64, ptr %result_temp2, i32 0, i32 1
  %val7 = load i64, ptr %val_ptr6, align 4
  %unwrap_result8 = select i1 %is_success5, i64 %val7, i64 0
  store i64 %unwrap_result8, ptr %payment2, align 4
  %0 = load i64, ptr %payment1, align 4
  %1 = load i64, ptr %payment2, align 4
  %addtmp = add i64 %0, %1
  %auto_wrap_result = alloca %result_int64, align 8
  %err_ptr9 = getelementptr inbounds %result_int64, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr9, align 1
  %val_ptr10 = getelementptr inbounds %result_int64, ptr %auto_wrap_result, i32 0, i32 1
  store i64 %addtmp, ptr %val_ptr10, align 4
  %result_val = load %result_int64, ptr %auto_wrap_result, align 4
  ret %result_int64 %result_val
}

define internal %result_int64 @__user_main() {
entry:
  %status3 = alloca i8, align 1
  %status2 = alloca i8, align 1
  %status1 = alloca i8, align 1
  %final_balance = alloca i64, align 8
  %loan_result = alloca i64, align 8
  %interest_result = alloca i64, align 8
  %batch_total = alloca i64, align 8
  call void @puts(ptr @4)
  call void @puts(ptr @5)
  %calltmp = call %result_int64 @run_transaction_batch()
  %result_temp = alloca %result_int64, align 8
  store %result_int64 %calltmp, ptr %result_temp, align 4
  %err_ptr = getelementptr inbounds %result_int64, ptr %result_temp, i32 0, i32 0
  %err = load i8, ptr %err_ptr, align 1
  %is_success = icmp eq i8 %err, 0
  %val_ptr = getelementptr inbounds %result_int64, ptr %result_temp, i32 0, i32 1
  %val = load i64, ptr %val_ptr, align 4
  %unwrap_result = select i1 %is_success, i64 %val, i64 0
  store i64 %unwrap_result, ptr %batch_total, align 4
  call void @puts(ptr @6)
  call void @puts(ptr @7)
  %calltmp1 = call %result_int8 @test_error_scenarios()
  %result_temp2 = alloca %result_int8, align 8
  store %result_int8 %calltmp1, ptr %result_temp2, align 1
  %err_ptr3 = getelementptr inbounds %result_int8, ptr %result_temp2, i32 0, i32 0
  %err4 = load i8, ptr %err_ptr3, align 1
  %is_success5 = icmp eq i8 %err4, 0
  %val_ptr6 = getelementptr inbounds %result_int8, ptr %result_temp2, i32 0, i32 1
  %val7 = load i8, ptr %val_ptr6, align 1
  %unwrap_result8 = select i1 %is_success5, i8 %val7, i8 -1
  call void @puts(ptr @8)
  call void @puts(ptr @9)
  %calltmp9 = call %result_int64 @test_interest()
  %result_temp10 = alloca %result_int64, align 8
  store %result_int64 %calltmp9, ptr %result_temp10, align 4
  %err_ptr11 = getelementptr inbounds %result_int64, ptr %result_temp10, i32 0, i32 0
  %err12 = load i8, ptr %err_ptr11, align 1
  %is_success13 = icmp eq i8 %err12, 0
  %val_ptr14 = getelementptr inbounds %result_int64, ptr %result_temp10, i32 0, i32 1
  %val15 = load i64, ptr %val_ptr14, align 4
  %unwrap_result16 = select i1 %is_success13, i64 %val15, i64 0
  store i64 %unwrap_result16, ptr %interest_result, align 4
  call void @puts(ptr @10)
  call void @puts(ptr @11)
  %calltmp17 = call %result_int64 @test_loans()
  %result_temp18 = alloca %result_int64, align 8
  store %result_int64 %calltmp17, ptr %result_temp18, align 4
  %err_ptr19 = getelementptr inbounds %result_int64, ptr %result_temp18, i32 0, i32 0
  %err20 = load i8, ptr %err_ptr19, align 1
  %is_success21 = icmp eq i8 %err20, 0
  %val_ptr22 = getelementptr inbounds %result_int64, ptr %result_temp18, i32 0, i32 1
  %val23 = load i64, ptr %val_ptr22, align 4
  %unwrap_result24 = select i1 %is_success21, i64 %val23, i64 0
  store i64 %unwrap_result24, ptr %loan_result, align 4
  call void @puts(ptr @12)
  call void @puts(ptr @13)
  %calltmp25 = call %result_int64 @process_monthly_statement(i64 1000, i64 5)
  %result_temp26 = alloca %result_int64, align 8
  store %result_int64 %calltmp25, ptr %result_temp26, align 4
  %err_ptr27 = getelementptr inbounds %result_int64, ptr %result_temp26, i32 0, i32 0
  %err28 = load i8, ptr %err_ptr27, align 1
  %is_success29 = icmp eq i8 %err28, 0
  %val_ptr30 = getelementptr inbounds %result_int64, ptr %result_temp26, i32 0, i32 1
  %val31 = load i64, ptr %val_ptr30, align 4
  %unwrap_result32 = select i1 %is_success29, i64 %val31, i64 0
  store i64 %unwrap_result32, ptr %final_balance, align 4
  call void @puts(ptr @14)
  call void @puts(ptr @15)
  %calltmp33 = call %result_int8 @check_account_status(i64 1000, i64 500)
  %result_temp34 = alloca %result_int8, align 8
  store %result_int8 %calltmp33, ptr %result_temp34, align 1
  %err_ptr35 = getelementptr inbounds %result_int8, ptr %result_temp34, i32 0, i32 0
  %err36 = load i8, ptr %err_ptr35, align 1
  %is_success37 = icmp eq i8 %err36, 0
  %val_ptr38 = getelementptr inbounds %result_int8, ptr %result_temp34, i32 0, i32 1
  %val39 = load i8, ptr %val_ptr38, align 1
  %unwrap_result40 = select i1 %is_success37, i8 %val39, i8 0
  store i8 %unwrap_result40, ptr %status1, align 1
  %calltmp41 = call %result_int8 @check_account_status(i64 -200, i64 500)
  %result_temp42 = alloca %result_int8, align 8
  store %result_int8 %calltmp41, ptr %result_temp42, align 1
  %err_ptr43 = getelementptr inbounds %result_int8, ptr %result_temp42, i32 0, i32 0
  %err44 = load i8, ptr %err_ptr43, align 1
  %is_success45 = icmp eq i8 %err44, 0
  %val_ptr46 = getelementptr inbounds %result_int8, ptr %result_temp42, i32 0, i32 1
  %val47 = load i8, ptr %val_ptr46, align 1
  %unwrap_result48 = select i1 %is_success45, i8 %val47, i8 0
  store i8 %unwrap_result48, ptr %status2, align 1
  %calltmp49 = call %result_int8 @check_account_status(i64 -600, i64 500)
  %result_temp50 = alloca %result_int8, align 8
  store %result_int8 %calltmp49, ptr %result_temp50, align 1
  %err_ptr51 = getelementptr inbounds %result_int8, ptr %result_temp50, i32 0, i32 0
  %err52 = load i8, ptr %err_ptr51, align 1
  %is_success53 = icmp eq i8 %err52, 0
  %val_ptr54 = getelementptr inbounds %result_int8, ptr %result_temp50, i32 0, i32 1
  %val55 = load i8, ptr %val_ptr54, align 1
  %unwrap_result56 = select i1 %is_success53, i8 %val55, i8 0
  store i8 %unwrap_result56, ptr %status3, align 1
  call void @puts(ptr @16)
  call void @puts(ptr @17)
  call void @puts(ptr @18)
  %auto_wrap_result = alloca %result_int64, align 8
  %err_ptr57 = getelementptr inbounds %result_int64, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr57, align 1
  %val_ptr58 = getelementptr inbounds %result_int64, ptr %auto_wrap_result, i32 0, i32 1
  store i64 0, ptr %val_ptr58, align 4
  %result_val = load %result_int64, ptr %auto_wrap_result, align 4
  ret %result_int64 %result_val
}

define i64 @main() {
entry:
  call void @__aria_module_init()
  %0 = call %result_int64 @__user_main()
  ret i64 0
}
