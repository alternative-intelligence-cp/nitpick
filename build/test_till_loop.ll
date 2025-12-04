; ModuleID = 'aria_module'
source_filename = "aria_module"

%result_int8 = type { i8, i8 }

declare void @puts(ptr)

declare void @print(ptr)

define internal void @__aria_module_init() {
entry:
  ret void
}

define internal %result_int8 @test_till_up() {
entry:
  %sum = alloca i8, align 1
  store i64 0, ptr %sum, align 4
  br label %loop_body

loop_body:                                        ; preds = %loop_body, %entry
  %"$" = phi i64 [ 0, %entry ], [ %next_val, %loop_body ]
  %0 = load i8, ptr %sum, align 1
  %1 = sext i8 %0 to i64
  %addtmp = add i64 %1, %"$"
  store i64 %addtmp, ptr %sum, align 4
  %next_val = add i64 %"$", 1
  %cond_pos = icmp slt i64 %next_val, 10
  %cond_neg = icmp sge i64 %next_val, 0
  %loop_cond = select i1 false, i1 %cond_neg, i1 %cond_pos
  br i1 %loop_cond, label %loop_body, label %loop_exit

loop_exit:                                        ; preds = %loop_body
  %2 = load i8, ptr %sum, align 1
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 %2, ptr %val_ptr, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val
}

define internal %result_int8 @test_till_down() {
entry:
  %sum = alloca i8, align 1
  store i64 0, ptr %sum, align 4
  br label %loop_body

loop_body:                                        ; preds = %loop_body, %entry
  %"$" = phi i64 [ 10, %entry ], [ %next_val, %loop_body ]
  %0 = load i8, ptr %sum, align 1
  %1 = sext i8 %0 to i64
  %addtmp = add i64 %1, %"$"
  store i64 %addtmp, ptr %sum, align 4
  %next_val = add i64 %"$", -1
  %cond_pos = icmp slt i64 %next_val, 10
  %cond_neg = icmp sge i64 %next_val, 0
  %loop_cond = select i1 true, i1 %cond_neg, i1 %cond_pos
  br i1 %loop_cond, label %loop_body, label %loop_exit

loop_exit:                                        ; preds = %loop_body
  %2 = load i8, ptr %sum, align 1
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 %2, ptr %val_ptr, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val
}

define internal %result_int8 @__user_main() {
entry:
  %down = alloca i8, align 1
  %up = alloca i8, align 1
  %calltmp = call %result_int8 @test_till_up()
  store %result_int8 %calltmp, ptr %up, align 1
  %calltmp1 = call %result_int8 @test_till_down()
  store %result_int8 %calltmp1, ptr %down, align 1
  %0 = load i8, ptr %up, align 1
  %1 = load i8, ptr %down, align 1
  %addtmp = add i8 %0, %1
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 %addtmp, ptr %val_ptr, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val
}

define i64 @main() {
entry:
  call void @__aria_module_init()
  %0 = call %result_int8 @__user_main()
  ret i64 0
}
