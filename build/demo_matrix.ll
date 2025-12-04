; ModuleID = 'aria_module'
source_filename = "aria_module"

%result_int64 = type { i8, i64 }

@0 = private unnamed_addr constant [42 x i8] c"=== Matrix and Vector Operations Demo ===\00", align 1
@1 = private unnamed_addr constant [40 x i8] c"Identity matrix diagonal sum calculated\00", align 1
@2 = private unnamed_addr constant [31 x i8] c"Matrix multiplication complete\00", align 1
@3 = private unnamed_addr constant [23 x i8] c"Dot product calculated\00", align 1
@4 = private unnamed_addr constant [28 x i8] c"Nested computation complete\00", align 1
@5 = private unnamed_addr constant [32 x i8] c"Fibonacci calculations complete\00", align 1
@6 = private unnamed_addr constant [32 x i8] c"Factorial calculations complete\00", align 1
@7 = private unnamed_addr constant [29 x i8] c"Complex calculation complete\00", align 1
@8 = private unnamed_addr constant [25 x i8] c"All operations complete!\00", align 1

declare void @puts(ptr)

declare void @print(ptr)

define internal void @__aria_module_init() {
entry:
  ret void
}

define internal %result_int64 @create_identity_matrix() {
entry:
  %diagonal_sum = alloca i64, align 8
  %0 = call ptr @aria_alloc(i64 8)
  %m00 = alloca ptr, align 8
  store ptr %0, ptr %m00, align 8
  %1 = load ptr, ptr %m00, align 8
  store i64 1, ptr %1, align 4
  %2 = call ptr @aria_alloc.1(i64 8)
  %m01 = alloca ptr, align 8
  store ptr %2, ptr %m01, align 8
  %3 = load ptr, ptr %m01, align 8
  store i64 0, ptr %3, align 4
  %4 = call ptr @aria_alloc.2(i64 8)
  %m02 = alloca ptr, align 8
  store ptr %4, ptr %m02, align 8
  %5 = load ptr, ptr %m02, align 8
  store i64 0, ptr %5, align 4
  %6 = call ptr @aria_alloc.3(i64 8)
  %m10 = alloca ptr, align 8
  store ptr %6, ptr %m10, align 8
  %7 = load ptr, ptr %m10, align 8
  store i64 0, ptr %7, align 4
  %8 = call ptr @aria_alloc.4(i64 8)
  %m11 = alloca ptr, align 8
  store ptr %8, ptr %m11, align 8
  %9 = load ptr, ptr %m11, align 8
  store i64 1, ptr %9, align 4
  %10 = call ptr @aria_alloc.5(i64 8)
  %m12 = alloca ptr, align 8
  store ptr %10, ptr %m12, align 8
  %11 = load ptr, ptr %m12, align 8
  store i64 0, ptr %11, align 4
  %12 = call ptr @aria_alloc.6(i64 8)
  %m20 = alloca ptr, align 8
  store ptr %12, ptr %m20, align 8
  %13 = load ptr, ptr %m20, align 8
  store i64 0, ptr %13, align 4
  %14 = call ptr @aria_alloc.7(i64 8)
  %m21 = alloca ptr, align 8
  store ptr %14, ptr %m21, align 8
  %15 = load ptr, ptr %m21, align 8
  store i64 0, ptr %15, align 4
  %16 = call ptr @aria_alloc.8(i64 8)
  %m22 = alloca ptr, align 8
  store ptr %16, ptr %m22, align 8
  %17 = load ptr, ptr %m22, align 8
  store i64 1, ptr %17, align 4
  %18 = load ptr, ptr %m00, align 8
  %19 = load i64, ptr %18, align 4
  %20 = load ptr, ptr %m11, align 8
  %21 = load i64, ptr %20, align 4
  %addtmp = add i64 %19, %21
  %22 = load ptr, ptr %m22, align 8
  %23 = load i64, ptr %22, align 4
  %addtmp1 = add i64 %addtmp, %23
  store i64 %addtmp1, ptr %diagonal_sum, align 4
  %24 = load i64, ptr %diagonal_sum, align 4
  %result = alloca %result_int64, align 8
  %err_ptr = getelementptr inbounds %result_int64, ptr %result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int64, ptr %result, i32 0, i32 1
  store i64 %24, ptr %val_ptr, align 4
  %result_val = load %result_int64, ptr %result, align 4
  ret %result_int64 %result_val
}

declare ptr @aria_alloc(i64)

declare ptr @aria_alloc.1(i64)

declare ptr @aria_alloc.2(i64)

declare ptr @aria_alloc.3(i64)

declare ptr @aria_alloc.4(i64)

declare ptr @aria_alloc.5(i64)

declare ptr @aria_alloc.6(i64)

declare ptr @aria_alloc.7(i64)

declare ptr @aria_alloc.8(i64)

define internal %result_int64 @matrix_multiply_2x2(i64 %a00, i64 %a01, i64 %a10, i64 %a11, i64 %b00, i64 %b01, i64 %b10, i64 %b11) {
entry:
  %sum = alloca i64, align 8
  %c11 = alloca i64, align 8
  %c10 = alloca i64, align 8
  %c01 = alloca i64, align 8
  %c00 = alloca i64, align 8
  %a001 = alloca i64, align 8
  store i64 %a00, ptr %a001, align 4
  %a012 = alloca i64, align 8
  store i64 %a01, ptr %a012, align 4
  %a103 = alloca i64, align 8
  store i64 %a10, ptr %a103, align 4
  %a114 = alloca i64, align 8
  store i64 %a11, ptr %a114, align 4
  %b005 = alloca i64, align 8
  store i64 %b00, ptr %b005, align 4
  %b016 = alloca i64, align 8
  store i64 %b01, ptr %b016, align 4
  %b107 = alloca i64, align 8
  store i64 %b10, ptr %b107, align 4
  %b118 = alloca i64, align 8
  store i64 %b11, ptr %b118, align 4
  %0 = load i64, ptr %a001, align 4
  %1 = load i64, ptr %b005, align 4
  %multmp = mul i64 %0, %1
  %2 = load i64, ptr %a012, align 4
  %3 = load i64, ptr %b107, align 4
  %multmp9 = mul i64 %2, %3
  %addtmp = add i64 %multmp, %multmp9
  store i64 %addtmp, ptr %c00, align 4
  %4 = load i64, ptr %a001, align 4
  %5 = load i64, ptr %b016, align 4
  %multmp10 = mul i64 %4, %5
  %6 = load i64, ptr %a012, align 4
  %7 = load i64, ptr %b118, align 4
  %multmp11 = mul i64 %6, %7
  %addtmp12 = add i64 %multmp10, %multmp11
  store i64 %addtmp12, ptr %c01, align 4
  %8 = load i64, ptr %a103, align 4
  %9 = load i64, ptr %b005, align 4
  %multmp13 = mul i64 %8, %9
  %10 = load i64, ptr %a114, align 4
  %11 = load i64, ptr %b107, align 4
  %multmp14 = mul i64 %10, %11
  %addtmp15 = add i64 %multmp13, %multmp14
  store i64 %addtmp15, ptr %c10, align 4
  %12 = load i64, ptr %a103, align 4
  %13 = load i64, ptr %b016, align 4
  %multmp16 = mul i64 %12, %13
  %14 = load i64, ptr %a114, align 4
  %15 = load i64, ptr %b118, align 4
  %multmp17 = mul i64 %14, %15
  %addtmp18 = add i64 %multmp16, %multmp17
  store i64 %addtmp18, ptr %c11, align 4
  %16 = load i64, ptr %c00, align 4
  %17 = load i64, ptr %c01, align 4
  %addtmp19 = add i64 %16, %17
  %18 = load i64, ptr %c10, align 4
  %addtmp20 = add i64 %addtmp19, %18
  %19 = load i64, ptr %c11, align 4
  %addtmp21 = add i64 %addtmp20, %19
  store i64 %addtmp21, ptr %sum, align 4
  %20 = load i64, ptr %sum, align 4
  %result = alloca %result_int64, align 8
  %err_ptr = getelementptr inbounds %result_int64, ptr %result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int64, ptr %result, i32 0, i32 1
  store i64 %20, ptr %val_ptr, align 4
  %result_val = load %result_int64, ptr %result, align 4
  ret %result_int64 %result_val
}

define internal %result_int64 @vector_dot_product(i64 %x1, i64 %y1, i64 %z1, i64 %x2, i64 %y2, i64 %z2) {
entry:
  %dot_res = alloca i64, align 8
  %x11 = alloca i64, align 8
  store i64 %x1, ptr %x11, align 4
  %y12 = alloca i64, align 8
  store i64 %y1, ptr %y12, align 4
  %z13 = alloca i64, align 8
  store i64 %z1, ptr %z13, align 4
  %x24 = alloca i64, align 8
  store i64 %x2, ptr %x24, align 4
  %y25 = alloca i64, align 8
  store i64 %y2, ptr %y25, align 4
  %z26 = alloca i64, align 8
  store i64 %z2, ptr %z26, align 4
  %0 = load i64, ptr %x11, align 4
  %1 = load i64, ptr %x24, align 4
  %multmp = mul i64 %0, %1
  %2 = load i64, ptr %y12, align 4
  %3 = load i64, ptr %y25, align 4
  %multmp7 = mul i64 %2, %3
  %addtmp = add i64 %multmp, %multmp7
  %4 = load i64, ptr %z13, align 4
  %5 = load i64, ptr %z26, align 4
  %multmp8 = mul i64 %4, %5
  %addtmp9 = add i64 %addtmp, %multmp8
  store i64 %addtmp9, ptr %dot_res, align 4
  %6 = load i64, ptr %dot_res, align 4
  %result = alloca %result_int64, align 8
  %err_ptr = getelementptr inbounds %result_int64, ptr %result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int64, ptr %result, i32 0, i32 1
  store i64 %6, ptr %val_ptr, align 4
  %result_val = load %result_int64, ptr %result, align 4
  ret %result_int64 %result_val
}

define internal %result_int64 @nested_computation() {
entry:
  %product = alloca i64, align 8
  %j = alloca i64, align 8
  %i = alloca i64, align 8
  %total = alloca i64, align 8
  store i64 0, ptr %total, align 4
  store i64 0, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %while_exit, %entry
  %0 = load i64, ptr %i, align 4
  %lttmp = icmp slt i64 %0, 5
  br i1 %lttmp, label %while_body, label %while_exit6

while_body:                                       ; preds = %while_cond
  store i64 0, ptr %j, align 4
  br label %while_cond1

while_cond1:                                      ; preds = %while_body2, %while_body
  %1 = load i64, ptr %j, align 4
  %lttmp3 = icmp slt i64 %1, 5
  br i1 %lttmp3, label %while_body2, label %while_exit

while_body2:                                      ; preds = %while_cond1
  %2 = load i64, ptr %i, align 4
  %3 = load i64, ptr %j, align 4
  %multmp = mul i64 %2, %3
  store i64 %multmp, ptr %product, align 4
  %4 = load i64, ptr %total, align 4
  %5 = load i64, ptr %product, align 4
  %addtmp = add i64 %4, %5
  store i64 %addtmp, ptr %total, align 4
  %6 = load i64, ptr %j, align 4
  %addtmp4 = add i64 %6, 1
  store i64 %addtmp4, ptr %j, align 4
  br label %while_cond1

while_exit:                                       ; preds = %while_cond1
  %7 = load i64, ptr %i, align 4
  %addtmp5 = add i64 %7, 1
  store i64 %addtmp5, ptr %i, align 4
  br label %while_cond

while_exit6:                                      ; preds = %while_cond
  %8 = load i64, ptr %total, align 4
  %auto_wrap_result = alloca %result_int64, align 8
  %err_ptr = getelementptr inbounds %result_int64, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int64, ptr %auto_wrap_result, i32 0, i32 1
  store i64 %8, ptr %val_ptr, align 4
  %result_val = load %result_int64, ptr %auto_wrap_result, align 4
  ret %result_int64 %result_val
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
  %pick_eq = icmp eq i64 %0, 0
  br i1 %pick_eq, label %case_body_0, label %case_next_0

case_body_0:                                      ; preds = %entry
  %result = alloca %result_int64, align 8
  %err_ptr = getelementptr inbounds %result_int64, ptr %result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int64, ptr %result, i32 0, i32 1
  store i64 0, ptr %val_ptr, align 4
  %result_val = load %result_int64, ptr %result, align 4
  ret %result_int64 %result_val

case_next_0:                                      ; preds = %entry
  %pick_eq2 = icmp eq i64 %0, 1
  br i1 %pick_eq2, label %case_body_1, label %case_next_1

case_body_1:                                      ; preds = %case_next_0
  %result3 = alloca %result_int64, align 8
  %err_ptr4 = getelementptr inbounds %result_int64, ptr %result3, i32 0, i32 0
  store i8 0, ptr %err_ptr4, align 1
  %val_ptr5 = getelementptr inbounds %result_int64, ptr %result3, i32 0, i32 1
  store i64 1, ptr %val_ptr5, align 4
  %result_val6 = load %result_int64, ptr %result3, align 4
  ret %result_int64 %result_val6

case_next_1:                                      ; preds = %case_next_0
  br i1 true, label %case_body_2, label %case_next_2

case_body_2:                                      ; preds = %case_next_1
  store i64 0, ptr %a, align 4
  store i64 1, ptr %b, align 4
  store i64 2, ptr %count, align 4
  br label %while_cond

while_cond:                                       ; preds = %while_body, %case_body_2
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
  %addtmp7 = add i64 %7, 1
  store i64 %addtmp7, ptr %count, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %8 = load i64, ptr %b, align 4
  %result8 = alloca %result_int64, align 8
  %err_ptr9 = getelementptr inbounds %result_int64, ptr %result8, i32 0, i32 0
  store i8 0, ptr %err_ptr9, align 1
  %val_ptr10 = getelementptr inbounds %result_int64, ptr %result8, i32 0, i32 1
  store i64 %8, ptr %val_ptr10, align 4
  %result_val11 = load %result_int64, ptr %result8, align 4
  ret %result_int64 %result_val11

case_next_2:                                      ; preds = %case_next_1
  br label %pick_done

pick_done:                                        ; preds = %case_next_2
  %result12 = alloca %result_int64, align 8
  %err_ptr13 = getelementptr inbounds %result_int64, ptr %result12, i32 0, i32 0
  store i8 0, ptr %err_ptr13, align 1
  %val_ptr14 = getelementptr inbounds %result_int64, ptr %result12, i32 0, i32 1
  store i64 0, ptr %val_ptr14, align 4
  %result_val15 = load %result_int64, ptr %result12, align 4
  ret %result_int64 %result_val15
}

define internal %result_int64 @factorial(i64 %n) {
entry:
  %i = alloca i64, align 8
  %res = alloca i64, align 8
  %n1 = alloca i64, align 8
  store i64 %n, ptr %n1, align 4
  %0 = load i64, ptr %n1, align 4
  %pick_eq = icmp eq i64 %0, 0
  br i1 %pick_eq, label %case_body_0, label %case_next_0

case_body_0:                                      ; preds = %entry
  %result = alloca %result_int64, align 8
  %err_ptr = getelementptr inbounds %result_int64, ptr %result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int64, ptr %result, i32 0, i32 1
  store i64 1, ptr %val_ptr, align 4
  %result_val = load %result_int64, ptr %result, align 4
  ret %result_int64 %result_val

case_next_0:                                      ; preds = %entry
  %pick_eq2 = icmp eq i64 %0, 1
  br i1 %pick_eq2, label %case_body_1, label %case_next_1

case_body_1:                                      ; preds = %case_next_0
  %result3 = alloca %result_int64, align 8
  %err_ptr4 = getelementptr inbounds %result_int64, ptr %result3, i32 0, i32 0
  store i8 0, ptr %err_ptr4, align 1
  %val_ptr5 = getelementptr inbounds %result_int64, ptr %result3, i32 0, i32 1
  store i64 1, ptr %val_ptr5, align 4
  %result_val6 = load %result_int64, ptr %result3, align 4
  ret %result_int64 %result_val6

case_next_1:                                      ; preds = %case_next_0
  br i1 true, label %case_body_2, label %case_next_2

case_body_2:                                      ; preds = %case_next_1
  store i64 1, ptr %res, align 4
  store i64 2, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %while_body, %case_body_2
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
  %result7 = alloca %result_int64, align 8
  %err_ptr8 = getelementptr inbounds %result_int64, ptr %result7, i32 0, i32 0
  store i8 0, ptr %err_ptr8, align 1
  %val_ptr9 = getelementptr inbounds %result_int64, ptr %result7, i32 0, i32 1
  store i64 %6, ptr %val_ptr9, align 4
  %result_val10 = load %result_int64, ptr %result7, align 4
  ret %result_int64 %result_val10

case_next_2:                                      ; preds = %case_next_1
  br label %pick_done

pick_done:                                        ; preds = %case_next_2
  %result11 = alloca %result_int64, align 8
  %err_ptr12 = getelementptr inbounds %result_int64, ptr %result11, i32 0, i32 0
  store i8 0, ptr %err_ptr12, align 1
  %val_ptr13 = getelementptr inbounds %result_int64, ptr %result11, i32 0, i32 1
  store i64 1, ptr %val_ptr13, align 4
  %result_val14 = load %result_int64, ptr %result11, align 4
  ret %result_int64 %result_val14
}

define internal %result_int64 @complex_calculation(i64 %a, i64 %b, i64 %c) {
entry:
  %dot = alloca i64, align 8
  %fact_b = alloca i64, align 8
  %fib_a = alloca i64, align 8
  %a1 = alloca i64, align 8
  store i64 %a, ptr %a1, align 4
  %b2 = alloca i64, align 8
  store i64 %b, ptr %b2, align 4
  %c3 = alloca i64, align 8
  store i64 %c, ptr %c3, align 4
  %0 = load i64, ptr %a1, align 4
  %calltmp = call %result_int64 @fibonacci(i64 %0)
  %result_temp = alloca %result_int64, align 8
  store %result_int64 %calltmp, ptr %result_temp, align 4
  %err_ptr = getelementptr inbounds %result_int64, ptr %result_temp, i32 0, i32 0
  %err = load i8, ptr %err_ptr, align 1
  %is_success = icmp eq i8 %err, 0
  %val_ptr = getelementptr inbounds %result_int64, ptr %result_temp, i32 0, i32 1
  %val = load i64, ptr %val_ptr, align 4
  %unwrap_result = select i1 %is_success, i64 %val, i64 0
  store i64 %unwrap_result, ptr %fib_a, align 4
  %1 = load i64, ptr %b2, align 4
  %calltmp4 = call %result_int64 @factorial(i64 %1)
  %result_temp5 = alloca %result_int64, align 8
  store %result_int64 %calltmp4, ptr %result_temp5, align 4
  %err_ptr6 = getelementptr inbounds %result_int64, ptr %result_temp5, i32 0, i32 0
  %err7 = load i8, ptr %err_ptr6, align 1
  %is_success8 = icmp eq i8 %err7, 0
  %val_ptr9 = getelementptr inbounds %result_int64, ptr %result_temp5, i32 0, i32 1
  %val10 = load i64, ptr %val_ptr9, align 4
  %unwrap_result11 = select i1 %is_success8, i64 %val10, i64 1
  store i64 %unwrap_result11, ptr %fact_b, align 4
  %2 = load i64, ptr %fib_a, align 4
  %3 = load i64, ptr %fact_b, align 4
  %4 = load i64, ptr %c3, align 4
  %calltmp12 = call %result_int64 @vector_dot_product(i64 %2, i64 %3, i64 %4, i64 1, i64 1, i64 1)
  %result_temp13 = alloca %result_int64, align 8
  store %result_int64 %calltmp12, ptr %result_temp13, align 4
  %err_ptr14 = getelementptr inbounds %result_int64, ptr %result_temp13, i32 0, i32 0
  %err15 = load i8, ptr %err_ptr14, align 1
  %is_success16 = icmp eq i8 %err15, 0
  %val_ptr17 = getelementptr inbounds %result_int64, ptr %result_temp13, i32 0, i32 1
  %val18 = load i64, ptr %val_ptr17, align 4
  %unwrap_result19 = select i1 %is_success16, i64 %val18, i64 0
  store i64 %unwrap_result19, ptr %dot, align 4
  %5 = load i64, ptr %dot, align 4
  %result = alloca %result_int64, align 8
  %err_ptr20 = getelementptr inbounds %result_int64, ptr %result, i32 0, i32 0
  store i8 0, ptr %err_ptr20, align 1
  %val_ptr21 = getelementptr inbounds %result_int64, ptr %result, i32 0, i32 1
  store i64 %5, ptr %val_ptr21, align 4
  %result_val = load %result_int64, ptr %result, align 4
  ret %result_int64 %result_val
}

define internal %result_int64 @__user_main() {
entry:
  %complex = alloca i64, align 8
  %fact6 = alloca i64, align 8
  %fact5 = alloca i64, align 8
  %fib10 = alloca i64, align 8
  %fib5 = alloca i64, align 8
  %nested_result = alloca i64, align 8
  %dot_result = alloca i64, align 8
  %mat_result = alloca i64, align 8
  %identity_sum = alloca i64, align 8
  call void @puts(ptr @0)
  %calltmp = call %result_int64 @create_identity_matrix()
  %result_temp = alloca %result_int64, align 8
  store %result_int64 %calltmp, ptr %result_temp, align 4
  %err_ptr = getelementptr inbounds %result_int64, ptr %result_temp, i32 0, i32 0
  %err = load i8, ptr %err_ptr, align 1
  %is_success = icmp eq i8 %err, 0
  %val_ptr = getelementptr inbounds %result_int64, ptr %result_temp, i32 0, i32 1
  %val = load i64, ptr %val_ptr, align 4
  %unwrap_result = select i1 %is_success, i64 %val, i64 0
  store i64 %unwrap_result, ptr %identity_sum, align 4
  call void @puts(ptr @1)
  %calltmp1 = call %result_int64 @matrix_multiply_2x2(i64 1, i64 2, i64 3, i64 4, i64 5, i64 6, i64 7, i64 8)
  %result_temp2 = alloca %result_int64, align 8
  store %result_int64 %calltmp1, ptr %result_temp2, align 4
  %err_ptr3 = getelementptr inbounds %result_int64, ptr %result_temp2, i32 0, i32 0
  %err4 = load i8, ptr %err_ptr3, align 1
  %is_success5 = icmp eq i8 %err4, 0
  %val_ptr6 = getelementptr inbounds %result_int64, ptr %result_temp2, i32 0, i32 1
  %val7 = load i64, ptr %val_ptr6, align 4
  %unwrap_result8 = select i1 %is_success5, i64 %val7, i64 0
  store i64 %unwrap_result8, ptr %mat_result, align 4
  call void @puts(ptr @2)
  %calltmp9 = call %result_int64 @vector_dot_product(i64 1, i64 2, i64 3, i64 4, i64 5, i64 6)
  %result_temp10 = alloca %result_int64, align 8
  store %result_int64 %calltmp9, ptr %result_temp10, align 4
  %err_ptr11 = getelementptr inbounds %result_int64, ptr %result_temp10, i32 0, i32 0
  %err12 = load i8, ptr %err_ptr11, align 1
  %is_success13 = icmp eq i8 %err12, 0
  %val_ptr14 = getelementptr inbounds %result_int64, ptr %result_temp10, i32 0, i32 1
  %val15 = load i64, ptr %val_ptr14, align 4
  %unwrap_result16 = select i1 %is_success13, i64 %val15, i64 0
  store i64 %unwrap_result16, ptr %dot_result, align 4
  call void @puts(ptr @3)
  %calltmp17 = call %result_int64 @nested_computation()
  %result_temp18 = alloca %result_int64, align 8
  store %result_int64 %calltmp17, ptr %result_temp18, align 4
  %err_ptr19 = getelementptr inbounds %result_int64, ptr %result_temp18, i32 0, i32 0
  %err20 = load i8, ptr %err_ptr19, align 1
  %is_success21 = icmp eq i8 %err20, 0
  %val_ptr22 = getelementptr inbounds %result_int64, ptr %result_temp18, i32 0, i32 1
  %val23 = load i64, ptr %val_ptr22, align 4
  %unwrap_result24 = select i1 %is_success21, i64 %val23, i64 0
  store i64 %unwrap_result24, ptr %nested_result, align 4
  call void @puts(ptr @4)
  %calltmp25 = call %result_int64 @fibonacci(i64 5)
  %result_temp26 = alloca %result_int64, align 8
  store %result_int64 %calltmp25, ptr %result_temp26, align 4
  %err_ptr27 = getelementptr inbounds %result_int64, ptr %result_temp26, i32 0, i32 0
  %err28 = load i8, ptr %err_ptr27, align 1
  %is_success29 = icmp eq i8 %err28, 0
  %val_ptr30 = getelementptr inbounds %result_int64, ptr %result_temp26, i32 0, i32 1
  %val31 = load i64, ptr %val_ptr30, align 4
  %unwrap_result32 = select i1 %is_success29, i64 %val31, i64 0
  store i64 %unwrap_result32, ptr %fib5, align 4
  %calltmp33 = call %result_int64 @fibonacci(i64 10)
  %result_temp34 = alloca %result_int64, align 8
  store %result_int64 %calltmp33, ptr %result_temp34, align 4
  %err_ptr35 = getelementptr inbounds %result_int64, ptr %result_temp34, i32 0, i32 0
  %err36 = load i8, ptr %err_ptr35, align 1
  %is_success37 = icmp eq i8 %err36, 0
  %val_ptr38 = getelementptr inbounds %result_int64, ptr %result_temp34, i32 0, i32 1
  %val39 = load i64, ptr %val_ptr38, align 4
  %unwrap_result40 = select i1 %is_success37, i64 %val39, i64 0
  store i64 %unwrap_result40, ptr %fib10, align 4
  call void @puts(ptr @5)
  %calltmp41 = call %result_int64 @factorial(i64 5)
  %result_temp42 = alloca %result_int64, align 8
  store %result_int64 %calltmp41, ptr %result_temp42, align 4
  %err_ptr43 = getelementptr inbounds %result_int64, ptr %result_temp42, i32 0, i32 0
  %err44 = load i8, ptr %err_ptr43, align 1
  %is_success45 = icmp eq i8 %err44, 0
  %val_ptr46 = getelementptr inbounds %result_int64, ptr %result_temp42, i32 0, i32 1
  %val47 = load i64, ptr %val_ptr46, align 4
  %unwrap_result48 = select i1 %is_success45, i64 %val47, i64 1
  store i64 %unwrap_result48, ptr %fact5, align 4
  %calltmp49 = call %result_int64 @factorial(i64 6)
  %result_temp50 = alloca %result_int64, align 8
  store %result_int64 %calltmp49, ptr %result_temp50, align 4
  %err_ptr51 = getelementptr inbounds %result_int64, ptr %result_temp50, i32 0, i32 0
  %err52 = load i8, ptr %err_ptr51, align 1
  %is_success53 = icmp eq i8 %err52, 0
  %val_ptr54 = getelementptr inbounds %result_int64, ptr %result_temp50, i32 0, i32 1
  %val55 = load i64, ptr %val_ptr54, align 4
  %unwrap_result56 = select i1 %is_success53, i64 %val55, i64 1
  store i64 %unwrap_result56, ptr %fact6, align 4
  call void @puts(ptr @6)
  %calltmp57 = call %result_int64 @complex_calculation(i64 5, i64 4, i64 10)
  %result_temp58 = alloca %result_int64, align 8
  store %result_int64 %calltmp57, ptr %result_temp58, align 4
  %err_ptr59 = getelementptr inbounds %result_int64, ptr %result_temp58, i32 0, i32 0
  %err60 = load i8, ptr %err_ptr59, align 1
  %is_success61 = icmp eq i8 %err60, 0
  %val_ptr62 = getelementptr inbounds %result_int64, ptr %result_temp58, i32 0, i32 1
  %val63 = load i64, ptr %val_ptr62, align 4
  %unwrap_result64 = select i1 %is_success61, i64 %val63, i64 0
  store i64 %unwrap_result64, ptr %complex, align 4
  call void @puts(ptr @7)
  call void @puts(ptr @8)
  %auto_wrap_result = alloca %result_int64, align 8
  %err_ptr65 = getelementptr inbounds %result_int64, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr65, align 1
  %val_ptr66 = getelementptr inbounds %result_int64, ptr %auto_wrap_result, i32 0, i32 1
  store i64 0, ptr %val_ptr66, align 4
  %result_val = load %result_int64, ptr %auto_wrap_result, align 4
  ret %result_int64 %result_val
}

define i64 @main() {
entry:
  call void @__aria_module_init()
  %0 = call %result_int64 @__user_main()
  ret i64 0
}
