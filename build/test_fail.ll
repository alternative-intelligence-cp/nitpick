; ModuleID = 'aria_module'
source_filename = "aria_module"

%result_int8 = type { i8, i8 }

declare void @puts(ptr)

declare void @print(ptr)

define internal void @__aria_module_init() {
entry:
  ret void
}

define internal %result_int8 @test(i8 %n) {
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
  %auto_wrap_cast = trunc i64 %1 to i8
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr2 = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr2, align 1
  %val_ptr3 = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 %auto_wrap_cast, ptr %val_ptr3, align 1
  %result_val4 = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val4
}

define internal %result_int8 @__user_main() {
entry:
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
