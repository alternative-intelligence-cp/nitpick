; ModuleID = 'aria_module'
source_filename = "aria_module"

%result_int8 = type { i8, i8 }

@0 = private unnamed_addr constant [27 x i8] c"All unwrap tests complete!\00", align 1

declare void @puts(ptr)

declare void @print(ptr)

define internal void @__aria_module_init() {
entry:
  ret void
}

define internal %result_int8 @make_success(i8 %value) {
entry:
  %value1 = alloca i8, align 1
  store i8 %value, ptr %value1, align 1
  %0 = load i64, ptr %value1, align 4
  %result = alloca %result_int8, align 8
  %err_ptr = getelementptr inbounds %result_int8, ptr %result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_cast = trunc i64 %0 to i8
  %val_ptr = getelementptr inbounds %result_int8, ptr %result, i32 0, i32 1
  store i8 %val_cast, ptr %val_ptr, align 1
  %result_val = load %result_int8, ptr %result, align 1
  ret %result_int8 %result_val
}

define internal %result_int8 @make_error(i8 %code) {
entry:
  %code1 = alloca i8, align 1
  store i8 %code, ptr %code1, align 1
  %0 = load i64, ptr %code1, align 4
  %result = alloca %result_int8, align 8
  %err_cast = trunc i64 %0 to i8
  %err_ptr = getelementptr inbounds %result_int8, ptr %result, i32 0, i32 0
  store i8 %err_cast, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int8, ptr %result, i32 0, i32 1
  store i8 0, ptr %val_ptr, align 1
  %result_val = load %result_int8, ptr %result, align 1
  ret %result_int8 %result_val
}

define internal %result_int8 @test_unwrap_success() {
entry:
  %value = alloca i8, align 1
  %calltmp = call %result_int8 @make_success(i8 42)
  %result_temp = alloca %result_int8, align 8
  store %result_int8 %calltmp, ptr %result_temp, align 1
  %err_ptr = getelementptr inbounds %result_int8, ptr %result_temp, i32 0, i32 0
  %err = load i8, ptr %err_ptr, align 1
  %is_success = icmp eq i8 %err, 0
  %val_ptr = getelementptr inbounds %result_int8, ptr %result_temp, i32 0, i32 1
  %val = load i8, ptr %val_ptr, align 1
  %unwrap_result = select i1 %is_success, i8 %val, i8 -1
  store i8 %unwrap_result, ptr %value, align 1
  %0 = load i8, ptr %value, align 1
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr1 = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr1, align 1
  %val_ptr2 = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 %0, ptr %val_ptr2, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val
}

define internal %result_int8 @test_unwrap_error() {
entry:
  %value = alloca i8, align 1
  %calltmp = call %result_int8 @make_error(i8 1)
  %result_temp = alloca %result_int8, align 8
  store %result_int8 %calltmp, ptr %result_temp, align 1
  %err_ptr = getelementptr inbounds %result_int8, ptr %result_temp, i32 0, i32 0
  %err = load i8, ptr %err_ptr, align 1
  %is_success = icmp eq i8 %err, 0
  %val_ptr = getelementptr inbounds %result_int8, ptr %result_temp, i32 0, i32 1
  %val = load i8, ptr %val_ptr, align 1
  %unwrap_result = select i1 %is_success, i8 %val, i8 -1
  store i8 %unwrap_result, ptr %value, align 1
  %0 = load i8, ptr %value, align 1
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr1 = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr1, align 1
  %val_ptr2 = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 %0, ptr %val_ptr2, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val
}

define internal %result_int8 @test_unwrap_chain() {
entry:
  %sum = alloca i8, align 1
  %v3 = alloca i8, align 1
  %v2 = alloca i8, align 1
  %v1 = alloca i8, align 1
  %calltmp = call %result_int8 @make_success(i8 10)
  %result_temp = alloca %result_int8, align 8
  store %result_int8 %calltmp, ptr %result_temp, align 1
  %err_ptr = getelementptr inbounds %result_int8, ptr %result_temp, i32 0, i32 0
  %err = load i8, ptr %err_ptr, align 1
  %is_success = icmp eq i8 %err, 0
  %val_ptr = getelementptr inbounds %result_int8, ptr %result_temp, i32 0, i32 1
  %val = load i8, ptr %val_ptr, align 1
  %unwrap_result = select i1 %is_success, i8 %val, i8 0
  store i8 %unwrap_result, ptr %v1, align 1
  %calltmp1 = call %result_int8 @make_success(i8 20)
  %result_temp2 = alloca %result_int8, align 8
  store %result_int8 %calltmp1, ptr %result_temp2, align 1
  %err_ptr3 = getelementptr inbounds %result_int8, ptr %result_temp2, i32 0, i32 0
  %err4 = load i8, ptr %err_ptr3, align 1
  %is_success5 = icmp eq i8 %err4, 0
  %val_ptr6 = getelementptr inbounds %result_int8, ptr %result_temp2, i32 0, i32 1
  %val7 = load i8, ptr %val_ptr6, align 1
  %unwrap_result8 = select i1 %is_success5, i8 %val7, i8 0
  store i8 %unwrap_result8, ptr %v2, align 1
  %calltmp9 = call %result_int8 @make_success(i8 30)
  %result_temp10 = alloca %result_int8, align 8
  store %result_int8 %calltmp9, ptr %result_temp10, align 1
  %err_ptr11 = getelementptr inbounds %result_int8, ptr %result_temp10, i32 0, i32 0
  %err12 = load i8, ptr %err_ptr11, align 1
  %is_success13 = icmp eq i8 %err12, 0
  %val_ptr14 = getelementptr inbounds %result_int8, ptr %result_temp10, i32 0, i32 1
  %val15 = load i8, ptr %val_ptr14, align 1
  %unwrap_result16 = select i1 %is_success13, i8 %val15, i8 0
  store i8 %unwrap_result16, ptr %v3, align 1
  %0 = load i8, ptr %v1, align 1
  %1 = load i8, ptr %v2, align 1
  %addtmp = add i8 %0, %1
  %2 = load i8, ptr %v3, align 1
  %addtmp17 = add i8 %addtmp, %2
  store i8 %addtmp17, ptr %sum, align 1
  %3 = load i8, ptr %sum, align 1
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr18 = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr18, align 1
  %val_ptr19 = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 %3, ptr %val_ptr19, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val
}

define internal %result_int8 @add_auto(i8 %a, i8 %b) {
entry:
  %a1 = alloca i8, align 1
  store i8 %a, ptr %a1, align 1
  %b2 = alloca i8, align 1
  store i8 %b, ptr %b2, align 1
  %0 = load i64, ptr %a1, align 4
  %1 = load i64, ptr %b2, align 4
  %addtmp = add i64 %0, %1
  %auto_wrap_cast = trunc i64 %addtmp to i8
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 %auto_wrap_cast, ptr %val_ptr, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val
}

define internal %result_int8 @test_autowrap_unwrap() {
entry:
  %res = alloca i8, align 1
  %calltmp = call %result_int8 @add_auto(i8 5, i8 3)
  %result_temp = alloca %result_int8, align 8
  store %result_int8 %calltmp, ptr %result_temp, align 1
  %err_ptr = getelementptr inbounds %result_int8, ptr %result_temp, i32 0, i32 0
  %err = load i8, ptr %err_ptr, align 1
  %is_success = icmp eq i8 %err, 0
  %val_ptr = getelementptr inbounds %result_int8, ptr %result_temp, i32 0, i32 1
  %val = load i8, ptr %val_ptr, align 1
  %unwrap_result = select i1 %is_success, i8 %val, i8 -1
  store i8 %unwrap_result, ptr %res, align 1
  %0 = load i8, ptr %res, align 1
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr1 = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr1, align 1
  %val_ptr2 = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 %0, ptr %val_ptr2, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val
}

define internal %result_int8 @divide(i8 %a, i8 %b) {
entry:
  %a1 = alloca i8, align 1
  store i8 %a, ptr %a1, align 1
  %b2 = alloca i8, align 1
  store i8 %b, ptr %b2, align 1
  %0 = load i64, ptr %a1, align 4
  %1 = load i64, ptr %b2, align 4
  %divtmp = sdiv i64 %0, %1
  %result = alloca %result_int8, align 8
  %err_ptr = getelementptr inbounds %result_int8, ptr %result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_cast = trunc i64 %divtmp to i8
  %val_ptr = getelementptr inbounds %result_int8, ptr %result, i32 0, i32 1
  store i8 %val_cast, ptr %val_ptr, align 1
  %result_val = load %result_int8, ptr %result, align 1
  ret %result_int8 %result_val
}

define internal %result_int8 @test_divide() {
entry:
  %r1 = alloca i8, align 1
  %calltmp = call %result_int8 @divide(i8 10, i8 2)
  %result_temp = alloca %result_int8, align 8
  store %result_int8 %calltmp, ptr %result_temp, align 1
  %err_ptr = getelementptr inbounds %result_int8, ptr %result_temp, i32 0, i32 0
  %err = load i8, ptr %err_ptr, align 1
  %is_success = icmp eq i8 %err, 0
  %val_ptr = getelementptr inbounds %result_int8, ptr %result_temp, i32 0, i32 1
  %val = load i8, ptr %val_ptr, align 1
  %unwrap_result = select i1 %is_success, i8 %val, i8 -1
  store i8 %unwrap_result, ptr %r1, align 1
  %0 = load i8, ptr %r1, align 1
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr1 = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr1, align 1
  %val_ptr2 = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 %0, ptr %val_ptr2, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val
}

define internal %result_int8 @__user_main() {
entry:
  %t5 = alloca i8, align 1
  %t4 = alloca i8, align 1
  %t3 = alloca i8, align 1
  %t2 = alloca i8, align 1
  %t1 = alloca i8, align 1
  %calltmp = call %result_int8 @test_unwrap_success()
  %result_temp = alloca %result_int8, align 8
  store %result_int8 %calltmp, ptr %result_temp, align 1
  %err_ptr = getelementptr inbounds %result_int8, ptr %result_temp, i32 0, i32 0
  %err = load i8, ptr %err_ptr, align 1
  %is_success = icmp eq i8 %err, 0
  %val_ptr = getelementptr inbounds %result_int8, ptr %result_temp, i32 0, i32 1
  %val = load i8, ptr %val_ptr, align 1
  %unwrap_result = select i1 %is_success, i8 %val, i8 -1
  store i8 %unwrap_result, ptr %t1, align 1
  %calltmp1 = call %result_int8 @test_unwrap_error()
  %result_temp2 = alloca %result_int8, align 8
  store %result_int8 %calltmp1, ptr %result_temp2, align 1
  %err_ptr3 = getelementptr inbounds %result_int8, ptr %result_temp2, i32 0, i32 0
  %err4 = load i8, ptr %err_ptr3, align 1
  %is_success5 = icmp eq i8 %err4, 0
  %val_ptr6 = getelementptr inbounds %result_int8, ptr %result_temp2, i32 0, i32 1
  %val7 = load i8, ptr %val_ptr6, align 1
  %unwrap_result8 = select i1 %is_success5, i8 %val7, i8 -1
  store i8 %unwrap_result8, ptr %t2, align 1
  %calltmp9 = call %result_int8 @test_unwrap_chain()
  %result_temp10 = alloca %result_int8, align 8
  store %result_int8 %calltmp9, ptr %result_temp10, align 1
  %err_ptr11 = getelementptr inbounds %result_int8, ptr %result_temp10, i32 0, i32 0
  %err12 = load i8, ptr %err_ptr11, align 1
  %is_success13 = icmp eq i8 %err12, 0
  %val_ptr14 = getelementptr inbounds %result_int8, ptr %result_temp10, i32 0, i32 1
  %val15 = load i8, ptr %val_ptr14, align 1
  %unwrap_result16 = select i1 %is_success13, i8 %val15, i8 -1
  store i8 %unwrap_result16, ptr %t3, align 1
  %calltmp17 = call %result_int8 @test_autowrap_unwrap()
  %result_temp18 = alloca %result_int8, align 8
  store %result_int8 %calltmp17, ptr %result_temp18, align 1
  %err_ptr19 = getelementptr inbounds %result_int8, ptr %result_temp18, i32 0, i32 0
  %err20 = load i8, ptr %err_ptr19, align 1
  %is_success21 = icmp eq i8 %err20, 0
  %val_ptr22 = getelementptr inbounds %result_int8, ptr %result_temp18, i32 0, i32 1
  %val23 = load i8, ptr %val_ptr22, align 1
  %unwrap_result24 = select i1 %is_success21, i8 %val23, i8 -1
  store i8 %unwrap_result24, ptr %t4, align 1
  %calltmp25 = call %result_int8 @test_divide()
  %result_temp26 = alloca %result_int8, align 8
  store %result_int8 %calltmp25, ptr %result_temp26, align 1
  %err_ptr27 = getelementptr inbounds %result_int8, ptr %result_temp26, i32 0, i32 0
  %err28 = load i8, ptr %err_ptr27, align 1
  %is_success29 = icmp eq i8 %err28, 0
  %val_ptr30 = getelementptr inbounds %result_int8, ptr %result_temp26, i32 0, i32 1
  %val31 = load i8, ptr %val_ptr30, align 1
  %unwrap_result32 = select i1 %is_success29, i8 %val31, i8 -1
  store i8 %unwrap_result32, ptr %t5, align 1
  call void @puts(ptr @0)
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr33 = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr33, align 1
  %val_ptr34 = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 0, ptr %val_ptr34, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val
}

define i64 @main() {
entry:
  call void @__aria_module_init()
  %0 = call %result_int8 @__user_main()
  ret i64 0
}
