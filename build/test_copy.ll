; ModuleID = 'aria_module'
source_filename = "aria_module"

%result_int8 = type { i8, i8 }

@0 = private unnamed_addr constant [15 x i8] c"COPY compiled!\00", align 1

declare void @puts(ptr)

declare void @print(ptr)

define internal void @__aria_module_init() {
entry:
  ret void
}

define internal %result_int8 @copy_int32(ptr %dest, ptr %src, i64 %length) {
entry:
  %i = alloca i64, align 8
  %dest1 = alloca ptr, align 8
  store ptr %dest, ptr %dest1, align 8
  %src2 = alloca ptr, align 8
  store ptr %src, ptr %src2, align 8
  %length3 = alloca i64, align 8
  store i64 %length, ptr %length3, align 4
  store i64 0, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %while_body, %entry
  %0 = load i64, ptr %i, align 4
  %1 = load i64, ptr %length3, align 4
  %lttmp = icmp slt i64 %0, %1
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %dest_ptr = getelementptr ptr, ptr %dest1, i64 0, i64 0
  %2 = load i64, ptr %i, align 4
  %src_ptr = load ptr, ptr %src2, align 8
  %3 = load i64, ptr %i, align 4
  %elem_ptr = getelementptr i32, ptr %src_ptr, i64 %3
  %elem = load i32, ptr %elem_ptr, align 4
  %elem_ptr4 = getelementptr i32, ptr %dest_ptr, i64 %2
  store i32 %elem, ptr %elem_ptr4, align 4
  %4 = load i64, ptr %i, align 4
  %addtmp = add i64 %4, 1
  store i64 %addtmp, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 0, ptr %val_ptr, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val
}

define internal %result_int8 @__user_main() {
entry:
  call void @puts(ptr @0)
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
