; ModuleID = 'aria_module'
source_filename = "aria_module"

%result_int64 = type { i8, i64 }
%result_int8 = type { i8, i8 }

declare void @puts(ptr)

declare void @print(ptr)

define internal void @__aria_module_init() {
entry:
  ret void
}

define internal %result_int64 @fib(i64 %n) {
entry:
  %sum = alloca i64, align 8
  %r2 = alloca %result_int64, align 8
  %r1 = alloca %result_int64, align 8
  %n1 = alloca i64, align 8
  store i64 %n, ptr %n1, align 4
  %0 = load i64, ptr %n1, align 4
  %letmp = icmp sle i64 %0, 1
  br i1 %letmp, label %then, label %ifcont

then:                                             ; preds = %entry
  %1 = load i64, ptr %n1, align 4
  %auto_wrap_result = alloca %result_int64, align 8
  %err_ptr = getelementptr inbounds %result_int64, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int64, ptr %auto_wrap_result, i32 0, i32 1
  store i64 %1, ptr %val_ptr, align 4
  %result_val = load %result_int64, ptr %auto_wrap_result, align 4
  ret %result_int64 %result_val

ifcont:                                           ; preds = %entry
  %2 = load i64, ptr %n1, align 4
  %subtmp = sub i64 %2, 1
  %calltmp = call %result_int64 @fib(i64 %subtmp)
  store %result_int64 %calltmp, ptr %r1, align 4
  %3 = load i64, ptr %n1, align 4
  %subtmp2 = sub i64 %3, 2
  %calltmp3 = call %result_int64 @fib(i64 %subtmp2)
  store %result_int64 %calltmp3, ptr %r2, align 4
  %4 = load %result_int64, ptr %r1, align 4
  %temp = alloca %result_int64, align 8
  store %result_int64 %4, ptr %temp, align 4
  %val_ptr4 = getelementptr inbounds %result_int64, ptr %temp, i32 0, i32 1
  %val = load i64, ptr %val_ptr4, align 4
  %5 = load %result_int64, ptr %r2, align 4
  %temp5 = alloca %result_int64, align 8
  store %result_int64 %5, ptr %temp5, align 4
  %val_ptr6 = getelementptr inbounds %result_int64, ptr %temp5, i32 0, i32 1
  %val7 = load i64, ptr %val_ptr6, align 4
  %addtmp = add i64 %val, %val7
  store i64 %addtmp, ptr %sum, align 4
  %6 = load i64, ptr %sum, align 4
  %auto_wrap_result8 = alloca %result_int64, align 8
  %err_ptr9 = getelementptr inbounds %result_int64, ptr %auto_wrap_result8, i32 0, i32 0
  store i8 0, ptr %err_ptr9, align 1
  %val_ptr10 = getelementptr inbounds %result_int64, ptr %auto_wrap_result8, i32 0, i32 1
  store i64 %6, ptr %val_ptr10, align 4
  %result_val11 = load %result_int64, ptr %auto_wrap_result8, align 4
  ret %result_int64 %result_val11
}

define internal %result_int8 @__user_main() {
entry:
  %r = alloca %result_int64, align 8
  %calltmp = call %result_int64 @fib(i64 10)
  store %result_int64 %calltmp, ptr %r, align 4
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 0, ptr %val_ptr, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val
}

define i64 @main() {
entry:
  call void @__aria_module_init()
  %0 = call %result_int8 @__user_main()
  ret i64 0
}
