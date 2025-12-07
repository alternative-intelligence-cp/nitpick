; ModuleID = 'aria_module'
source_filename = "aria_module"

%result_int64 = type { i8, i64 }
%result_int8 = type { i8, i8 }

@0 = private unnamed_addr constant [35 x i8] c"Testing array parameter passing...\00", align 1
@1 = private unnamed_addr constant [33 x i8] c"SUCCESS: parameter passing works\00", align 1
@2 = private unnamed_addr constant [5 x i8] c"FAIL\00", align 1

declare void @puts(ptr)

declare void @print(ptr)

define internal void @__aria_module_init() {
entry:
  ret void
}

define internal %result_int64 @get_length(ptr %arr, i64 %len) {
entry:
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %len2 = alloca i64, align 8
  store i64 %len, ptr %len2, align 4
  %0 = load i64, ptr %len2, align 4
  %auto_wrap_result = alloca %result_int64, align 8
  %err_ptr = getelementptr inbounds %result_int64, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int64, ptr %auto_wrap_result, i32 0, i32 1
  store i64 %0, ptr %val_ptr, align 4
  %result_val = load %result_int64, ptr %auto_wrap_result, align 4
  ret %result_int64 %result_val
}

define internal %result_int8 @__user_main() {
entry:
  %length = alloca i64, align 8
  %data = alloca [5 x i8], align 1
  call void @puts(ptr @0)
  %data_ptr = getelementptr [5 x i8], ptr %data, i64 0, i64 0
  %elem_ptr = getelementptr i8, ptr %data_ptr, i64 0
  store i8 10, ptr %elem_ptr, align 1
  %data_ptr1 = getelementptr [5 x i8], ptr %data, i64 0, i64 0
  %elem_ptr2 = getelementptr i8, ptr %data_ptr1, i64 1
  store i8 20, ptr %elem_ptr2, align 1
  %data_ptr3 = getelementptr [5 x i8], ptr %data, i64 0, i64 0
  %elem_ptr4 = getelementptr i8, ptr %data_ptr3, i64 2
  store i8 30, ptr %elem_ptr4, align 1
  %data_ptr5 = getelementptr [5 x i8], ptr %data, i64 0, i64 0
  %elem_ptr6 = getelementptr i8, ptr %data_ptr5, i64 3
  store i8 40, ptr %elem_ptr6, align 1
  %data_ptr7 = getelementptr [5 x i8], ptr %data, i64 0, i64 0
  %elem_ptr8 = getelementptr i8, ptr %data_ptr7, i64 4
  store i8 50, ptr %elem_ptr8, align 1
  %data_ptr9 = getelementptr [5 x i8], ptr %data, i64 0, i64 0
  %calltmp = call %result_int64 @get_length(ptr %data_ptr9, i64 5)
  %result_temp = alloca %result_int64, align 8
  store %result_int64 %calltmp, ptr %result_temp, align 4
  %err_ptr = getelementptr inbounds %result_int64, ptr %result_temp, i32 0, i32 0
  %err = load i8, ptr %err_ptr, align 1
  %is_success = icmp eq i8 %err, 0
  %val_ptr = getelementptr inbounds %result_int64, ptr %result_temp, i32 0, i32 1
  %val = load i64, ptr %val_ptr, align 4
  %unwrap_result = select i1 %is_success, i64 %val, i64 0
  store i64 %unwrap_result, ptr %length, align 4
  %0 = load i64, ptr %length, align 4
  %eqtmp = icmp eq i64 %0, 5
  br i1 %eqtmp, label %then, label %ifcont

then:                                             ; preds = %entry
  call void @puts(ptr @1)
  %result = alloca %result_int8, align 8
  %err_ptr10 = getelementptr inbounds %result_int8, ptr %result, i32 0, i32 0
  store i8 0, ptr %err_ptr10, align 1
  %val_ptr11 = getelementptr inbounds %result_int8, ptr %result, i32 0, i32 1
  store i8 0, ptr %val_ptr11, align 1
  %result_val = load %result_int8, ptr %result, align 1
  ret %result_int8 %result_val

ifcont:                                           ; preds = %entry
  call void @puts(ptr @2)
  %result12 = alloca %result_int8, align 8
  %err_ptr13 = getelementptr inbounds %result_int8, ptr %result12, i32 0, i32 0
  store i8 0, ptr %err_ptr13, align 1
  %val_ptr14 = getelementptr inbounds %result_int8, ptr %result12, i32 0, i32 1
  store i8 1, ptr %val_ptr14, align 1
  %result_val15 = load %result_int8, ptr %result12, align 1
  ret %result_int8 %result_val15
}

define i64 @main() {
entry:
  call void @__aria_module_init()
  %0 = call %result_int8 @__user_main()
  ret i64 0
}
