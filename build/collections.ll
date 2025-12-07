; ModuleID = 'aria_module'
source_filename = "aria_module"

%result_int8 = type { i8, i8 }
%result_int64 = type { i8, i64 }
%result_int16 = type { i8, i16 }
%result_int32 = type { i8, i32 }
%result_uint8 = type { i8, i8 }
%result_uint16 = type { i8, i16 }
%result_uint32 = type { i8, i32 }
%result_uint64 = type { i8, i64 }
%result_flt32 = type { i8, float }
%result_flt64 = type { i8, double }

@0 = private unnamed_addr constant [63 x i8] c"\F0\9F\8E\89 Collections Library: 130 functions compiled successfully!\00", align 1

declare void @puts(ptr)

declare void @print(ptr)

define internal void @__aria_module_init() {
entry:
  ret void
}

define internal %result_int8 @contains_int8(ptr %arr, i64 %length, i8 %value) {
entry:
  %i = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %value3 = alloca i8, align 1
  store i8 %value, ptr %value3, align 1
  store i64 0, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont, %entry
  %0 = load i64, ptr %i, align 4
  %1 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %0, %1
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr = load ptr, ptr %arr1, align 8
  %2 = load i64, ptr %i, align 4
  %elem_ptr = getelementptr i8, ptr %arr_ptr, i64 %2
  %elem = load i8, ptr %elem_ptr, align 1
  %3 = load i8, ptr %value3, align 1
  %eqtmp = icmp eq i8 %elem, %3
  br i1 %eqtmp, label %then, label %ifcont

then:                                             ; preds = %while_body
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 1, ptr %val_ptr, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val

ifcont:                                           ; preds = %while_body
  %4 = load i64, ptr %i, align 4
  %addtmp = add i64 %4, 1
  store i64 %addtmp, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %auto_wrap_result4 = alloca %result_int8, align 8
  %err_ptr5 = getelementptr inbounds %result_int8, ptr %auto_wrap_result4, i32 0, i32 0
  store i8 0, ptr %err_ptr5, align 1
  %val_ptr6 = getelementptr inbounds %result_int8, ptr %auto_wrap_result4, i32 0, i32 1
  store i8 0, ptr %val_ptr6, align 1
  %result_val7 = load %result_int8, ptr %auto_wrap_result4, align 1
  ret %result_int8 %result_val7
}

define internal %result_int8 @contains_int16(ptr %arr, i64 %length, i16 %value) {
entry:
  %i = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %value3 = alloca i16, align 2
  store i16 %value, ptr %value3, align 2
  store i64 0, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont, %entry
  %0 = load i64, ptr %i, align 4
  %1 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %0, %1
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr = load ptr, ptr %arr1, align 8
  %2 = load i64, ptr %i, align 4
  %elem_ptr = getelementptr i16, ptr %arr_ptr, i64 %2
  %elem = load i16, ptr %elem_ptr, align 2
  %3 = load i16, ptr %value3, align 2
  %eqtmp = icmp eq i16 %elem, %3
  br i1 %eqtmp, label %then, label %ifcont

then:                                             ; preds = %while_body
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 1, ptr %val_ptr, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val

ifcont:                                           ; preds = %while_body
  %4 = load i64, ptr %i, align 4
  %addtmp = add i64 %4, 1
  store i64 %addtmp, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %auto_wrap_result4 = alloca %result_int8, align 8
  %err_ptr5 = getelementptr inbounds %result_int8, ptr %auto_wrap_result4, i32 0, i32 0
  store i8 0, ptr %err_ptr5, align 1
  %val_ptr6 = getelementptr inbounds %result_int8, ptr %auto_wrap_result4, i32 0, i32 1
  store i8 0, ptr %val_ptr6, align 1
  %result_val7 = load %result_int8, ptr %auto_wrap_result4, align 1
  ret %result_int8 %result_val7
}

define internal %result_int8 @contains_int32(ptr %arr, i64 %length, i32 %value) {
entry:
  %i = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %value3 = alloca i32, align 4
  store i32 %value, ptr %value3, align 4
  store i64 0, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont, %entry
  %0 = load i64, ptr %i, align 4
  %1 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %0, %1
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr = load ptr, ptr %arr1, align 8
  %2 = load i64, ptr %i, align 4
  %elem_ptr = getelementptr i32, ptr %arr_ptr, i64 %2
  %elem = load i32, ptr %elem_ptr, align 4
  %3 = load i32, ptr %value3, align 4
  %eqtmp = icmp eq i32 %elem, %3
  br i1 %eqtmp, label %then, label %ifcont

then:                                             ; preds = %while_body
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 1, ptr %val_ptr, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val

ifcont:                                           ; preds = %while_body
  %4 = load i64, ptr %i, align 4
  %addtmp = add i64 %4, 1
  store i64 %addtmp, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %auto_wrap_result4 = alloca %result_int8, align 8
  %err_ptr5 = getelementptr inbounds %result_int8, ptr %auto_wrap_result4, i32 0, i32 0
  store i8 0, ptr %err_ptr5, align 1
  %val_ptr6 = getelementptr inbounds %result_int8, ptr %auto_wrap_result4, i32 0, i32 1
  store i8 0, ptr %val_ptr6, align 1
  %result_val7 = load %result_int8, ptr %auto_wrap_result4, align 1
  ret %result_int8 %result_val7
}

define internal %result_int8 @contains_int64(ptr %arr, i64 %length, i64 %value) {
entry:
  %i = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %value3 = alloca i64, align 8
  store i64 %value, ptr %value3, align 4
  store i64 0, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont, %entry
  %0 = load i64, ptr %i, align 4
  %1 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %0, %1
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr = load ptr, ptr %arr1, align 8
  %2 = load i64, ptr %i, align 4
  %elem_ptr = getelementptr i64, ptr %arr_ptr, i64 %2
  %elem = load i64, ptr %elem_ptr, align 4
  %3 = load i64, ptr %value3, align 4
  %eqtmp = icmp eq i64 %elem, %3
  br i1 %eqtmp, label %then, label %ifcont

then:                                             ; preds = %while_body
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 1, ptr %val_ptr, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val

ifcont:                                           ; preds = %while_body
  %4 = load i64, ptr %i, align 4
  %addtmp = add i64 %4, 1
  store i64 %addtmp, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %auto_wrap_result4 = alloca %result_int8, align 8
  %err_ptr5 = getelementptr inbounds %result_int8, ptr %auto_wrap_result4, i32 0, i32 0
  store i8 0, ptr %err_ptr5, align 1
  %val_ptr6 = getelementptr inbounds %result_int8, ptr %auto_wrap_result4, i32 0, i32 1
  store i8 0, ptr %val_ptr6, align 1
  %result_val7 = load %result_int8, ptr %auto_wrap_result4, align 1
  ret %result_int8 %result_val7
}

define internal %result_int8 @contains_uint8(ptr %arr, i64 %length, i8 %value) {
entry:
  %i = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %value3 = alloca i8, align 1
  store i8 %value, ptr %value3, align 1
  store i64 0, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont, %entry
  %0 = load i64, ptr %i, align 4
  %1 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %0, %1
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr = load ptr, ptr %arr1, align 8
  %2 = load i64, ptr %i, align 4
  %elem_ptr = getelementptr i8, ptr %arr_ptr, i64 %2
  %elem = load i8, ptr %elem_ptr, align 1
  %3 = load i8, ptr %value3, align 1
  %eqtmp = icmp eq i8 %elem, %3
  br i1 %eqtmp, label %then, label %ifcont

then:                                             ; preds = %while_body
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 1, ptr %val_ptr, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val

ifcont:                                           ; preds = %while_body
  %4 = load i64, ptr %i, align 4
  %addtmp = add i64 %4, 1
  store i64 %addtmp, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %auto_wrap_result4 = alloca %result_int8, align 8
  %err_ptr5 = getelementptr inbounds %result_int8, ptr %auto_wrap_result4, i32 0, i32 0
  store i8 0, ptr %err_ptr5, align 1
  %val_ptr6 = getelementptr inbounds %result_int8, ptr %auto_wrap_result4, i32 0, i32 1
  store i8 0, ptr %val_ptr6, align 1
  %result_val7 = load %result_int8, ptr %auto_wrap_result4, align 1
  ret %result_int8 %result_val7
}

define internal %result_int8 @contains_uint16(ptr %arr, i64 %length, i16 %value) {
entry:
  %i = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %value3 = alloca i16, align 2
  store i16 %value, ptr %value3, align 2
  store i64 0, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont, %entry
  %0 = load i64, ptr %i, align 4
  %1 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %0, %1
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr = load ptr, ptr %arr1, align 8
  %2 = load i64, ptr %i, align 4
  %elem_ptr = getelementptr i16, ptr %arr_ptr, i64 %2
  %elem = load i16, ptr %elem_ptr, align 2
  %3 = load i16, ptr %value3, align 2
  %eqtmp = icmp eq i16 %elem, %3
  br i1 %eqtmp, label %then, label %ifcont

then:                                             ; preds = %while_body
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 1, ptr %val_ptr, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val

ifcont:                                           ; preds = %while_body
  %4 = load i64, ptr %i, align 4
  %addtmp = add i64 %4, 1
  store i64 %addtmp, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %auto_wrap_result4 = alloca %result_int8, align 8
  %err_ptr5 = getelementptr inbounds %result_int8, ptr %auto_wrap_result4, i32 0, i32 0
  store i8 0, ptr %err_ptr5, align 1
  %val_ptr6 = getelementptr inbounds %result_int8, ptr %auto_wrap_result4, i32 0, i32 1
  store i8 0, ptr %val_ptr6, align 1
  %result_val7 = load %result_int8, ptr %auto_wrap_result4, align 1
  ret %result_int8 %result_val7
}

define internal %result_int8 @contains_uint32(ptr %arr, i64 %length, i32 %value) {
entry:
  %i = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %value3 = alloca i32, align 4
  store i32 %value, ptr %value3, align 4
  store i64 0, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont, %entry
  %0 = load i64, ptr %i, align 4
  %1 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %0, %1
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr = load ptr, ptr %arr1, align 8
  %2 = load i64, ptr %i, align 4
  %elem_ptr = getelementptr i32, ptr %arr_ptr, i64 %2
  %elem = load i32, ptr %elem_ptr, align 4
  %3 = load i32, ptr %value3, align 4
  %eqtmp = icmp eq i32 %elem, %3
  br i1 %eqtmp, label %then, label %ifcont

then:                                             ; preds = %while_body
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 1, ptr %val_ptr, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val

ifcont:                                           ; preds = %while_body
  %4 = load i64, ptr %i, align 4
  %addtmp = add i64 %4, 1
  store i64 %addtmp, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %auto_wrap_result4 = alloca %result_int8, align 8
  %err_ptr5 = getelementptr inbounds %result_int8, ptr %auto_wrap_result4, i32 0, i32 0
  store i8 0, ptr %err_ptr5, align 1
  %val_ptr6 = getelementptr inbounds %result_int8, ptr %auto_wrap_result4, i32 0, i32 1
  store i8 0, ptr %val_ptr6, align 1
  %result_val7 = load %result_int8, ptr %auto_wrap_result4, align 1
  ret %result_int8 %result_val7
}

define internal %result_int8 @contains_uint64(ptr %arr, i64 %length, i64 %value) {
entry:
  %i = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %value3 = alloca i64, align 8
  store i64 %value, ptr %value3, align 4
  store i64 0, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont, %entry
  %0 = load i64, ptr %i, align 4
  %1 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %0, %1
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr = load ptr, ptr %arr1, align 8
  %2 = load i64, ptr %i, align 4
  %elem_ptr = getelementptr i64, ptr %arr_ptr, i64 %2
  %elem = load i64, ptr %elem_ptr, align 4
  %3 = load i64, ptr %value3, align 4
  %eqtmp = icmp eq i64 %elem, %3
  br i1 %eqtmp, label %then, label %ifcont

then:                                             ; preds = %while_body
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 1, ptr %val_ptr, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val

ifcont:                                           ; preds = %while_body
  %4 = load i64, ptr %i, align 4
  %addtmp = add i64 %4, 1
  store i64 %addtmp, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %auto_wrap_result4 = alloca %result_int8, align 8
  %err_ptr5 = getelementptr inbounds %result_int8, ptr %auto_wrap_result4, i32 0, i32 0
  store i8 0, ptr %err_ptr5, align 1
  %val_ptr6 = getelementptr inbounds %result_int8, ptr %auto_wrap_result4, i32 0, i32 1
  store i8 0, ptr %val_ptr6, align 1
  %result_val7 = load %result_int8, ptr %auto_wrap_result4, align 1
  ret %result_int8 %result_val7
}

define internal %result_int8 @contains_flt32(ptr %arr, i64 %length, float %value) {
entry:
  %i = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %value3 = alloca float, align 4
  store float %value, ptr %value3, align 4
  store i64 0, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont, %entry
  %0 = load i64, ptr %i, align 4
  %1 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %0, %1
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr = load ptr, ptr %arr1, align 8
  %2 = load i64, ptr %i, align 4
  %elem_ptr = getelementptr float, ptr %arr_ptr, i64 %2
  %elem = load float, ptr %elem_ptr, align 4
  %3 = load float, ptr %value3, align 4
  %eqtmp = fcmp oeq float %elem, %3
  br i1 %eqtmp, label %then, label %ifcont

then:                                             ; preds = %while_body
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 1, ptr %val_ptr, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val

ifcont:                                           ; preds = %while_body
  %4 = load i64, ptr %i, align 4
  %addtmp = add i64 %4, 1
  store i64 %addtmp, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %auto_wrap_result4 = alloca %result_int8, align 8
  %err_ptr5 = getelementptr inbounds %result_int8, ptr %auto_wrap_result4, i32 0, i32 0
  store i8 0, ptr %err_ptr5, align 1
  %val_ptr6 = getelementptr inbounds %result_int8, ptr %auto_wrap_result4, i32 0, i32 1
  store i8 0, ptr %val_ptr6, align 1
  %result_val7 = load %result_int8, ptr %auto_wrap_result4, align 1
  ret %result_int8 %result_val7
}

define internal %result_int8 @contains_flt64(ptr %arr, i64 %length, double %value) {
entry:
  %i = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %value3 = alloca double, align 8
  store double %value, ptr %value3, align 8
  store i64 0, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont, %entry
  %0 = load i64, ptr %i, align 4
  %1 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %0, %1
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr = load ptr, ptr %arr1, align 8
  %2 = load i64, ptr %i, align 4
  %elem_ptr = getelementptr double, ptr %arr_ptr, i64 %2
  %elem = load double, ptr %elem_ptr, align 8
  %3 = load double, ptr %value3, align 8
  %eqtmp = fcmp oeq double %elem, %3
  br i1 %eqtmp, label %then, label %ifcont

then:                                             ; preds = %while_body
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 1, ptr %val_ptr, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val

ifcont:                                           ; preds = %while_body
  %4 = load i64, ptr %i, align 4
  %addtmp = add i64 %4, 1
  store i64 %addtmp, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %auto_wrap_result4 = alloca %result_int8, align 8
  %err_ptr5 = getelementptr inbounds %result_int8, ptr %auto_wrap_result4, i32 0, i32 0
  store i8 0, ptr %err_ptr5, align 1
  %val_ptr6 = getelementptr inbounds %result_int8, ptr %auto_wrap_result4, i32 0, i32 1
  store i8 0, ptr %val_ptr6, align 1
  %result_val7 = load %result_int8, ptr %auto_wrap_result4, align 1
  ret %result_int8 %result_val7
}

define internal %result_int64 @indexOf_int8(ptr %arr, i64 %length, i8 %value) {
entry:
  %i = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %value3 = alloca i8, align 1
  store i8 %value, ptr %value3, align 1
  store i64 0, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont, %entry
  %0 = load i64, ptr %i, align 4
  %1 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %0, %1
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr = load ptr, ptr %arr1, align 8
  %2 = load i64, ptr %i, align 4
  %elem_ptr = getelementptr i8, ptr %arr_ptr, i64 %2
  %elem = load i8, ptr %elem_ptr, align 1
  %3 = load i8, ptr %value3, align 1
  %eqtmp = icmp eq i8 %elem, %3
  br i1 %eqtmp, label %then, label %ifcont

then:                                             ; preds = %while_body
  %4 = load i64, ptr %i, align 4
  %auto_wrap_result = alloca %result_int64, align 8
  %err_ptr = getelementptr inbounds %result_int64, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int64, ptr %auto_wrap_result, i32 0, i32 1
  store i64 %4, ptr %val_ptr, align 4
  %result_val = load %result_int64, ptr %auto_wrap_result, align 4
  ret %result_int64 %result_val

ifcont:                                           ; preds = %while_body
  %5 = load i64, ptr %i, align 4
  %addtmp = add i64 %5, 1
  store i64 %addtmp, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %auto_wrap_result4 = alloca %result_int64, align 8
  %err_ptr5 = getelementptr inbounds %result_int64, ptr %auto_wrap_result4, i32 0, i32 0
  store i8 0, ptr %err_ptr5, align 1
  %val_ptr6 = getelementptr inbounds %result_int64, ptr %auto_wrap_result4, i32 0, i32 1
  store i64 -1, ptr %val_ptr6, align 4
  %result_val7 = load %result_int64, ptr %auto_wrap_result4, align 4
  ret %result_int64 %result_val7
}

define internal %result_int64 @indexOf_int16(ptr %arr, i64 %length, i16 %value) {
entry:
  %i = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %value3 = alloca i16, align 2
  store i16 %value, ptr %value3, align 2
  store i64 0, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont, %entry
  %0 = load i64, ptr %i, align 4
  %1 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %0, %1
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr = load ptr, ptr %arr1, align 8
  %2 = load i64, ptr %i, align 4
  %elem_ptr = getelementptr i16, ptr %arr_ptr, i64 %2
  %elem = load i16, ptr %elem_ptr, align 2
  %3 = load i16, ptr %value3, align 2
  %eqtmp = icmp eq i16 %elem, %3
  br i1 %eqtmp, label %then, label %ifcont

then:                                             ; preds = %while_body
  %4 = load i64, ptr %i, align 4
  %auto_wrap_result = alloca %result_int64, align 8
  %err_ptr = getelementptr inbounds %result_int64, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int64, ptr %auto_wrap_result, i32 0, i32 1
  store i64 %4, ptr %val_ptr, align 4
  %result_val = load %result_int64, ptr %auto_wrap_result, align 4
  ret %result_int64 %result_val

ifcont:                                           ; preds = %while_body
  %5 = load i64, ptr %i, align 4
  %addtmp = add i64 %5, 1
  store i64 %addtmp, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %auto_wrap_result4 = alloca %result_int64, align 8
  %err_ptr5 = getelementptr inbounds %result_int64, ptr %auto_wrap_result4, i32 0, i32 0
  store i8 0, ptr %err_ptr5, align 1
  %val_ptr6 = getelementptr inbounds %result_int64, ptr %auto_wrap_result4, i32 0, i32 1
  store i64 -1, ptr %val_ptr6, align 4
  %result_val7 = load %result_int64, ptr %auto_wrap_result4, align 4
  ret %result_int64 %result_val7
}

define internal %result_int64 @indexOf_int32(ptr %arr, i64 %length, i32 %value) {
entry:
  %i = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %value3 = alloca i32, align 4
  store i32 %value, ptr %value3, align 4
  store i64 0, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont, %entry
  %0 = load i64, ptr %i, align 4
  %1 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %0, %1
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr = load ptr, ptr %arr1, align 8
  %2 = load i64, ptr %i, align 4
  %elem_ptr = getelementptr i32, ptr %arr_ptr, i64 %2
  %elem = load i32, ptr %elem_ptr, align 4
  %3 = load i32, ptr %value3, align 4
  %eqtmp = icmp eq i32 %elem, %3
  br i1 %eqtmp, label %then, label %ifcont

then:                                             ; preds = %while_body
  %4 = load i64, ptr %i, align 4
  %auto_wrap_result = alloca %result_int64, align 8
  %err_ptr = getelementptr inbounds %result_int64, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int64, ptr %auto_wrap_result, i32 0, i32 1
  store i64 %4, ptr %val_ptr, align 4
  %result_val = load %result_int64, ptr %auto_wrap_result, align 4
  ret %result_int64 %result_val

ifcont:                                           ; preds = %while_body
  %5 = load i64, ptr %i, align 4
  %addtmp = add i64 %5, 1
  store i64 %addtmp, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %auto_wrap_result4 = alloca %result_int64, align 8
  %err_ptr5 = getelementptr inbounds %result_int64, ptr %auto_wrap_result4, i32 0, i32 0
  store i8 0, ptr %err_ptr5, align 1
  %val_ptr6 = getelementptr inbounds %result_int64, ptr %auto_wrap_result4, i32 0, i32 1
  store i64 -1, ptr %val_ptr6, align 4
  %result_val7 = load %result_int64, ptr %auto_wrap_result4, align 4
  ret %result_int64 %result_val7
}

define internal %result_int64 @indexOf_int64(ptr %arr, i64 %length, i64 %value) {
entry:
  %i = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %value3 = alloca i64, align 8
  store i64 %value, ptr %value3, align 4
  store i64 0, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont, %entry
  %0 = load i64, ptr %i, align 4
  %1 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %0, %1
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr = load ptr, ptr %arr1, align 8
  %2 = load i64, ptr %i, align 4
  %elem_ptr = getelementptr i64, ptr %arr_ptr, i64 %2
  %elem = load i64, ptr %elem_ptr, align 4
  %3 = load i64, ptr %value3, align 4
  %eqtmp = icmp eq i64 %elem, %3
  br i1 %eqtmp, label %then, label %ifcont

then:                                             ; preds = %while_body
  %4 = load i64, ptr %i, align 4
  %auto_wrap_result = alloca %result_int64, align 8
  %err_ptr = getelementptr inbounds %result_int64, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int64, ptr %auto_wrap_result, i32 0, i32 1
  store i64 %4, ptr %val_ptr, align 4
  %result_val = load %result_int64, ptr %auto_wrap_result, align 4
  ret %result_int64 %result_val

ifcont:                                           ; preds = %while_body
  %5 = load i64, ptr %i, align 4
  %addtmp = add i64 %5, 1
  store i64 %addtmp, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %auto_wrap_result4 = alloca %result_int64, align 8
  %err_ptr5 = getelementptr inbounds %result_int64, ptr %auto_wrap_result4, i32 0, i32 0
  store i8 0, ptr %err_ptr5, align 1
  %val_ptr6 = getelementptr inbounds %result_int64, ptr %auto_wrap_result4, i32 0, i32 1
  store i64 -1, ptr %val_ptr6, align 4
  %result_val7 = load %result_int64, ptr %auto_wrap_result4, align 4
  ret %result_int64 %result_val7
}

define internal %result_int64 @indexOf_uint8(ptr %arr, i64 %length, i8 %value) {
entry:
  %i = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %value3 = alloca i8, align 1
  store i8 %value, ptr %value3, align 1
  store i64 0, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont, %entry
  %0 = load i64, ptr %i, align 4
  %1 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %0, %1
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr = load ptr, ptr %arr1, align 8
  %2 = load i64, ptr %i, align 4
  %elem_ptr = getelementptr i8, ptr %arr_ptr, i64 %2
  %elem = load i8, ptr %elem_ptr, align 1
  %3 = load i8, ptr %value3, align 1
  %eqtmp = icmp eq i8 %elem, %3
  br i1 %eqtmp, label %then, label %ifcont

then:                                             ; preds = %while_body
  %4 = load i64, ptr %i, align 4
  %auto_wrap_result = alloca %result_int64, align 8
  %err_ptr = getelementptr inbounds %result_int64, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int64, ptr %auto_wrap_result, i32 0, i32 1
  store i64 %4, ptr %val_ptr, align 4
  %result_val = load %result_int64, ptr %auto_wrap_result, align 4
  ret %result_int64 %result_val

ifcont:                                           ; preds = %while_body
  %5 = load i64, ptr %i, align 4
  %addtmp = add i64 %5, 1
  store i64 %addtmp, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %auto_wrap_result4 = alloca %result_int64, align 8
  %err_ptr5 = getelementptr inbounds %result_int64, ptr %auto_wrap_result4, i32 0, i32 0
  store i8 0, ptr %err_ptr5, align 1
  %val_ptr6 = getelementptr inbounds %result_int64, ptr %auto_wrap_result4, i32 0, i32 1
  store i64 -1, ptr %val_ptr6, align 4
  %result_val7 = load %result_int64, ptr %auto_wrap_result4, align 4
  ret %result_int64 %result_val7
}

define internal %result_int64 @indexOf_uint16(ptr %arr, i64 %length, i16 %value) {
entry:
  %i = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %value3 = alloca i16, align 2
  store i16 %value, ptr %value3, align 2
  store i64 0, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont, %entry
  %0 = load i64, ptr %i, align 4
  %1 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %0, %1
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr = load ptr, ptr %arr1, align 8
  %2 = load i64, ptr %i, align 4
  %elem_ptr = getelementptr i16, ptr %arr_ptr, i64 %2
  %elem = load i16, ptr %elem_ptr, align 2
  %3 = load i16, ptr %value3, align 2
  %eqtmp = icmp eq i16 %elem, %3
  br i1 %eqtmp, label %then, label %ifcont

then:                                             ; preds = %while_body
  %4 = load i64, ptr %i, align 4
  %auto_wrap_result = alloca %result_int64, align 8
  %err_ptr = getelementptr inbounds %result_int64, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int64, ptr %auto_wrap_result, i32 0, i32 1
  store i64 %4, ptr %val_ptr, align 4
  %result_val = load %result_int64, ptr %auto_wrap_result, align 4
  ret %result_int64 %result_val

ifcont:                                           ; preds = %while_body
  %5 = load i64, ptr %i, align 4
  %addtmp = add i64 %5, 1
  store i64 %addtmp, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %auto_wrap_result4 = alloca %result_int64, align 8
  %err_ptr5 = getelementptr inbounds %result_int64, ptr %auto_wrap_result4, i32 0, i32 0
  store i8 0, ptr %err_ptr5, align 1
  %val_ptr6 = getelementptr inbounds %result_int64, ptr %auto_wrap_result4, i32 0, i32 1
  store i64 -1, ptr %val_ptr6, align 4
  %result_val7 = load %result_int64, ptr %auto_wrap_result4, align 4
  ret %result_int64 %result_val7
}

define internal %result_int64 @indexOf_uint32(ptr %arr, i64 %length, i32 %value) {
entry:
  %i = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %value3 = alloca i32, align 4
  store i32 %value, ptr %value3, align 4
  store i64 0, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont, %entry
  %0 = load i64, ptr %i, align 4
  %1 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %0, %1
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr = load ptr, ptr %arr1, align 8
  %2 = load i64, ptr %i, align 4
  %elem_ptr = getelementptr i32, ptr %arr_ptr, i64 %2
  %elem = load i32, ptr %elem_ptr, align 4
  %3 = load i32, ptr %value3, align 4
  %eqtmp = icmp eq i32 %elem, %3
  br i1 %eqtmp, label %then, label %ifcont

then:                                             ; preds = %while_body
  %4 = load i64, ptr %i, align 4
  %auto_wrap_result = alloca %result_int64, align 8
  %err_ptr = getelementptr inbounds %result_int64, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int64, ptr %auto_wrap_result, i32 0, i32 1
  store i64 %4, ptr %val_ptr, align 4
  %result_val = load %result_int64, ptr %auto_wrap_result, align 4
  ret %result_int64 %result_val

ifcont:                                           ; preds = %while_body
  %5 = load i64, ptr %i, align 4
  %addtmp = add i64 %5, 1
  store i64 %addtmp, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %auto_wrap_result4 = alloca %result_int64, align 8
  %err_ptr5 = getelementptr inbounds %result_int64, ptr %auto_wrap_result4, i32 0, i32 0
  store i8 0, ptr %err_ptr5, align 1
  %val_ptr6 = getelementptr inbounds %result_int64, ptr %auto_wrap_result4, i32 0, i32 1
  store i64 -1, ptr %val_ptr6, align 4
  %result_val7 = load %result_int64, ptr %auto_wrap_result4, align 4
  ret %result_int64 %result_val7
}

define internal %result_int64 @indexOf_uint64(ptr %arr, i64 %length, i64 %value) {
entry:
  %i = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %value3 = alloca i64, align 8
  store i64 %value, ptr %value3, align 4
  store i64 0, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont, %entry
  %0 = load i64, ptr %i, align 4
  %1 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %0, %1
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr = load ptr, ptr %arr1, align 8
  %2 = load i64, ptr %i, align 4
  %elem_ptr = getelementptr i64, ptr %arr_ptr, i64 %2
  %elem = load i64, ptr %elem_ptr, align 4
  %3 = load i64, ptr %value3, align 4
  %eqtmp = icmp eq i64 %elem, %3
  br i1 %eqtmp, label %then, label %ifcont

then:                                             ; preds = %while_body
  %4 = load i64, ptr %i, align 4
  %auto_wrap_result = alloca %result_int64, align 8
  %err_ptr = getelementptr inbounds %result_int64, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int64, ptr %auto_wrap_result, i32 0, i32 1
  store i64 %4, ptr %val_ptr, align 4
  %result_val = load %result_int64, ptr %auto_wrap_result, align 4
  ret %result_int64 %result_val

ifcont:                                           ; preds = %while_body
  %5 = load i64, ptr %i, align 4
  %addtmp = add i64 %5, 1
  store i64 %addtmp, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %auto_wrap_result4 = alloca %result_int64, align 8
  %err_ptr5 = getelementptr inbounds %result_int64, ptr %auto_wrap_result4, i32 0, i32 0
  store i8 0, ptr %err_ptr5, align 1
  %val_ptr6 = getelementptr inbounds %result_int64, ptr %auto_wrap_result4, i32 0, i32 1
  store i64 -1, ptr %val_ptr6, align 4
  %result_val7 = load %result_int64, ptr %auto_wrap_result4, align 4
  ret %result_int64 %result_val7
}

define internal %result_int64 @indexOf_flt32(ptr %arr, i64 %length, float %value) {
entry:
  %i = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %value3 = alloca float, align 4
  store float %value, ptr %value3, align 4
  store i64 0, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont, %entry
  %0 = load i64, ptr %i, align 4
  %1 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %0, %1
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr = load ptr, ptr %arr1, align 8
  %2 = load i64, ptr %i, align 4
  %elem_ptr = getelementptr float, ptr %arr_ptr, i64 %2
  %elem = load float, ptr %elem_ptr, align 4
  %3 = load float, ptr %value3, align 4
  %eqtmp = fcmp oeq float %elem, %3
  br i1 %eqtmp, label %then, label %ifcont

then:                                             ; preds = %while_body
  %4 = load i64, ptr %i, align 4
  %auto_wrap_result = alloca %result_int64, align 8
  %err_ptr = getelementptr inbounds %result_int64, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int64, ptr %auto_wrap_result, i32 0, i32 1
  store i64 %4, ptr %val_ptr, align 4
  %result_val = load %result_int64, ptr %auto_wrap_result, align 4
  ret %result_int64 %result_val

ifcont:                                           ; preds = %while_body
  %5 = load i64, ptr %i, align 4
  %addtmp = add i64 %5, 1
  store i64 %addtmp, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %auto_wrap_result4 = alloca %result_int64, align 8
  %err_ptr5 = getelementptr inbounds %result_int64, ptr %auto_wrap_result4, i32 0, i32 0
  store i8 0, ptr %err_ptr5, align 1
  %val_ptr6 = getelementptr inbounds %result_int64, ptr %auto_wrap_result4, i32 0, i32 1
  store i64 -1, ptr %val_ptr6, align 4
  %result_val7 = load %result_int64, ptr %auto_wrap_result4, align 4
  ret %result_int64 %result_val7
}

define internal %result_int64 @indexOf_flt64(ptr %arr, i64 %length, double %value) {
entry:
  %i = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %value3 = alloca double, align 8
  store double %value, ptr %value3, align 8
  store i64 0, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont, %entry
  %0 = load i64, ptr %i, align 4
  %1 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %0, %1
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr = load ptr, ptr %arr1, align 8
  %2 = load i64, ptr %i, align 4
  %elem_ptr = getelementptr double, ptr %arr_ptr, i64 %2
  %elem = load double, ptr %elem_ptr, align 8
  %3 = load double, ptr %value3, align 8
  %eqtmp = fcmp oeq double %elem, %3
  br i1 %eqtmp, label %then, label %ifcont

then:                                             ; preds = %while_body
  %4 = load i64, ptr %i, align 4
  %auto_wrap_result = alloca %result_int64, align 8
  %err_ptr = getelementptr inbounds %result_int64, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int64, ptr %auto_wrap_result, i32 0, i32 1
  store i64 %4, ptr %val_ptr, align 4
  %result_val = load %result_int64, ptr %auto_wrap_result, align 4
  ret %result_int64 %result_val

ifcont:                                           ; preds = %while_body
  %5 = load i64, ptr %i, align 4
  %addtmp = add i64 %5, 1
  store i64 %addtmp, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %auto_wrap_result4 = alloca %result_int64, align 8
  %err_ptr5 = getelementptr inbounds %result_int64, ptr %auto_wrap_result4, i32 0, i32 0
  store i8 0, ptr %err_ptr5, align 1
  %val_ptr6 = getelementptr inbounds %result_int64, ptr %auto_wrap_result4, i32 0, i32 1
  store i64 -1, ptr %val_ptr6, align 4
  %result_val7 = load %result_int64, ptr %auto_wrap_result4, align 4
  ret %result_int64 %result_val7
}

define internal %result_int64 @count_int8(ptr %arr, i64 %length, i8 %value) {
entry:
  %i = alloca i64, align 8
  %count = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %value3 = alloca i8, align 1
  store i8 %value, ptr %value3, align 1
  store i64 0, ptr %count, align 4
  store i64 0, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont, %entry
  %0 = load i64, ptr %i, align 4
  %1 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %0, %1
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr = load ptr, ptr %arr1, align 8
  %2 = load i64, ptr %i, align 4
  %elem_ptr = getelementptr i8, ptr %arr_ptr, i64 %2
  %elem = load i8, ptr %elem_ptr, align 1
  %3 = load i8, ptr %value3, align 1
  %eqtmp = icmp eq i8 %elem, %3
  br i1 %eqtmp, label %then, label %ifcont

then:                                             ; preds = %while_body
  %4 = load i64, ptr %count, align 4
  %addtmp = add i64 %4, 1
  store i64 %addtmp, ptr %count, align 4
  br label %ifcont

ifcont:                                           ; preds = %then, %while_body
  %5 = load i64, ptr %i, align 4
  %addtmp4 = add i64 %5, 1
  store i64 %addtmp4, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %6 = load i64, ptr %count, align 4
  %auto_wrap_result = alloca %result_int64, align 8
  %err_ptr = getelementptr inbounds %result_int64, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int64, ptr %auto_wrap_result, i32 0, i32 1
  store i64 %6, ptr %val_ptr, align 4
  %result_val = load %result_int64, ptr %auto_wrap_result, align 4
  ret %result_int64 %result_val
}

define internal %result_int64 @count_int16(ptr %arr, i64 %length, i16 %value) {
entry:
  %i = alloca i64, align 8
  %count = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %value3 = alloca i16, align 2
  store i16 %value, ptr %value3, align 2
  store i64 0, ptr %count, align 4
  store i64 0, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont, %entry
  %0 = load i64, ptr %i, align 4
  %1 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %0, %1
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr = load ptr, ptr %arr1, align 8
  %2 = load i64, ptr %i, align 4
  %elem_ptr = getelementptr i16, ptr %arr_ptr, i64 %2
  %elem = load i16, ptr %elem_ptr, align 2
  %3 = load i16, ptr %value3, align 2
  %eqtmp = icmp eq i16 %elem, %3
  br i1 %eqtmp, label %then, label %ifcont

then:                                             ; preds = %while_body
  %4 = load i64, ptr %count, align 4
  %addtmp = add i64 %4, 1
  store i64 %addtmp, ptr %count, align 4
  br label %ifcont

ifcont:                                           ; preds = %then, %while_body
  %5 = load i64, ptr %i, align 4
  %addtmp4 = add i64 %5, 1
  store i64 %addtmp4, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %6 = load i64, ptr %count, align 4
  %auto_wrap_result = alloca %result_int64, align 8
  %err_ptr = getelementptr inbounds %result_int64, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int64, ptr %auto_wrap_result, i32 0, i32 1
  store i64 %6, ptr %val_ptr, align 4
  %result_val = load %result_int64, ptr %auto_wrap_result, align 4
  ret %result_int64 %result_val
}

define internal %result_int64 @count_int32(ptr %arr, i64 %length, i32 %value) {
entry:
  %i = alloca i64, align 8
  %count = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %value3 = alloca i32, align 4
  store i32 %value, ptr %value3, align 4
  store i64 0, ptr %count, align 4
  store i64 0, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont, %entry
  %0 = load i64, ptr %i, align 4
  %1 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %0, %1
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr = load ptr, ptr %arr1, align 8
  %2 = load i64, ptr %i, align 4
  %elem_ptr = getelementptr i32, ptr %arr_ptr, i64 %2
  %elem = load i32, ptr %elem_ptr, align 4
  %3 = load i32, ptr %value3, align 4
  %eqtmp = icmp eq i32 %elem, %3
  br i1 %eqtmp, label %then, label %ifcont

then:                                             ; preds = %while_body
  %4 = load i64, ptr %count, align 4
  %addtmp = add i64 %4, 1
  store i64 %addtmp, ptr %count, align 4
  br label %ifcont

ifcont:                                           ; preds = %then, %while_body
  %5 = load i64, ptr %i, align 4
  %addtmp4 = add i64 %5, 1
  store i64 %addtmp4, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %6 = load i64, ptr %count, align 4
  %auto_wrap_result = alloca %result_int64, align 8
  %err_ptr = getelementptr inbounds %result_int64, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int64, ptr %auto_wrap_result, i32 0, i32 1
  store i64 %6, ptr %val_ptr, align 4
  %result_val = load %result_int64, ptr %auto_wrap_result, align 4
  ret %result_int64 %result_val
}

define internal %result_int64 @count_int64(ptr %arr, i64 %length, i64 %value) {
entry:
  %i = alloca i64, align 8
  %count = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %value3 = alloca i64, align 8
  store i64 %value, ptr %value3, align 4
  store i64 0, ptr %count, align 4
  store i64 0, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont, %entry
  %0 = load i64, ptr %i, align 4
  %1 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %0, %1
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr = load ptr, ptr %arr1, align 8
  %2 = load i64, ptr %i, align 4
  %elem_ptr = getelementptr i64, ptr %arr_ptr, i64 %2
  %elem = load i64, ptr %elem_ptr, align 4
  %3 = load i64, ptr %value3, align 4
  %eqtmp = icmp eq i64 %elem, %3
  br i1 %eqtmp, label %then, label %ifcont

then:                                             ; preds = %while_body
  %4 = load i64, ptr %count, align 4
  %addtmp = add i64 %4, 1
  store i64 %addtmp, ptr %count, align 4
  br label %ifcont

ifcont:                                           ; preds = %then, %while_body
  %5 = load i64, ptr %i, align 4
  %addtmp4 = add i64 %5, 1
  store i64 %addtmp4, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %6 = load i64, ptr %count, align 4
  %auto_wrap_result = alloca %result_int64, align 8
  %err_ptr = getelementptr inbounds %result_int64, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int64, ptr %auto_wrap_result, i32 0, i32 1
  store i64 %6, ptr %val_ptr, align 4
  %result_val = load %result_int64, ptr %auto_wrap_result, align 4
  ret %result_int64 %result_val
}

define internal %result_int64 @count_uint8(ptr %arr, i64 %length, i8 %value) {
entry:
  %i = alloca i64, align 8
  %count = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %value3 = alloca i8, align 1
  store i8 %value, ptr %value3, align 1
  store i64 0, ptr %count, align 4
  store i64 0, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont, %entry
  %0 = load i64, ptr %i, align 4
  %1 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %0, %1
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr = load ptr, ptr %arr1, align 8
  %2 = load i64, ptr %i, align 4
  %elem_ptr = getelementptr i8, ptr %arr_ptr, i64 %2
  %elem = load i8, ptr %elem_ptr, align 1
  %3 = load i8, ptr %value3, align 1
  %eqtmp = icmp eq i8 %elem, %3
  br i1 %eqtmp, label %then, label %ifcont

then:                                             ; preds = %while_body
  %4 = load i64, ptr %count, align 4
  %addtmp = add i64 %4, 1
  store i64 %addtmp, ptr %count, align 4
  br label %ifcont

ifcont:                                           ; preds = %then, %while_body
  %5 = load i64, ptr %i, align 4
  %addtmp4 = add i64 %5, 1
  store i64 %addtmp4, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %6 = load i64, ptr %count, align 4
  %auto_wrap_result = alloca %result_int64, align 8
  %err_ptr = getelementptr inbounds %result_int64, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int64, ptr %auto_wrap_result, i32 0, i32 1
  store i64 %6, ptr %val_ptr, align 4
  %result_val = load %result_int64, ptr %auto_wrap_result, align 4
  ret %result_int64 %result_val
}

define internal %result_int64 @count_uint16(ptr %arr, i64 %length, i16 %value) {
entry:
  %i = alloca i64, align 8
  %count = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %value3 = alloca i16, align 2
  store i16 %value, ptr %value3, align 2
  store i64 0, ptr %count, align 4
  store i64 0, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont, %entry
  %0 = load i64, ptr %i, align 4
  %1 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %0, %1
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr = load ptr, ptr %arr1, align 8
  %2 = load i64, ptr %i, align 4
  %elem_ptr = getelementptr i16, ptr %arr_ptr, i64 %2
  %elem = load i16, ptr %elem_ptr, align 2
  %3 = load i16, ptr %value3, align 2
  %eqtmp = icmp eq i16 %elem, %3
  br i1 %eqtmp, label %then, label %ifcont

then:                                             ; preds = %while_body
  %4 = load i64, ptr %count, align 4
  %addtmp = add i64 %4, 1
  store i64 %addtmp, ptr %count, align 4
  br label %ifcont

ifcont:                                           ; preds = %then, %while_body
  %5 = load i64, ptr %i, align 4
  %addtmp4 = add i64 %5, 1
  store i64 %addtmp4, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %6 = load i64, ptr %count, align 4
  %auto_wrap_result = alloca %result_int64, align 8
  %err_ptr = getelementptr inbounds %result_int64, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int64, ptr %auto_wrap_result, i32 0, i32 1
  store i64 %6, ptr %val_ptr, align 4
  %result_val = load %result_int64, ptr %auto_wrap_result, align 4
  ret %result_int64 %result_val
}

define internal %result_int64 @count_uint32(ptr %arr, i64 %length, i32 %value) {
entry:
  %i = alloca i64, align 8
  %count = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %value3 = alloca i32, align 4
  store i32 %value, ptr %value3, align 4
  store i64 0, ptr %count, align 4
  store i64 0, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont, %entry
  %0 = load i64, ptr %i, align 4
  %1 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %0, %1
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr = load ptr, ptr %arr1, align 8
  %2 = load i64, ptr %i, align 4
  %elem_ptr = getelementptr i32, ptr %arr_ptr, i64 %2
  %elem = load i32, ptr %elem_ptr, align 4
  %3 = load i32, ptr %value3, align 4
  %eqtmp = icmp eq i32 %elem, %3
  br i1 %eqtmp, label %then, label %ifcont

then:                                             ; preds = %while_body
  %4 = load i64, ptr %count, align 4
  %addtmp = add i64 %4, 1
  store i64 %addtmp, ptr %count, align 4
  br label %ifcont

ifcont:                                           ; preds = %then, %while_body
  %5 = load i64, ptr %i, align 4
  %addtmp4 = add i64 %5, 1
  store i64 %addtmp4, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %6 = load i64, ptr %count, align 4
  %auto_wrap_result = alloca %result_int64, align 8
  %err_ptr = getelementptr inbounds %result_int64, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int64, ptr %auto_wrap_result, i32 0, i32 1
  store i64 %6, ptr %val_ptr, align 4
  %result_val = load %result_int64, ptr %auto_wrap_result, align 4
  ret %result_int64 %result_val
}

define internal %result_int64 @count_uint64(ptr %arr, i64 %length, i64 %value) {
entry:
  %i = alloca i64, align 8
  %count = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %value3 = alloca i64, align 8
  store i64 %value, ptr %value3, align 4
  store i64 0, ptr %count, align 4
  store i64 0, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont, %entry
  %0 = load i64, ptr %i, align 4
  %1 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %0, %1
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr = load ptr, ptr %arr1, align 8
  %2 = load i64, ptr %i, align 4
  %elem_ptr = getelementptr i64, ptr %arr_ptr, i64 %2
  %elem = load i64, ptr %elem_ptr, align 4
  %3 = load i64, ptr %value3, align 4
  %eqtmp = icmp eq i64 %elem, %3
  br i1 %eqtmp, label %then, label %ifcont

then:                                             ; preds = %while_body
  %4 = load i64, ptr %count, align 4
  %addtmp = add i64 %4, 1
  store i64 %addtmp, ptr %count, align 4
  br label %ifcont

ifcont:                                           ; preds = %then, %while_body
  %5 = load i64, ptr %i, align 4
  %addtmp4 = add i64 %5, 1
  store i64 %addtmp4, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %6 = load i64, ptr %count, align 4
  %auto_wrap_result = alloca %result_int64, align 8
  %err_ptr = getelementptr inbounds %result_int64, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int64, ptr %auto_wrap_result, i32 0, i32 1
  store i64 %6, ptr %val_ptr, align 4
  %result_val = load %result_int64, ptr %auto_wrap_result, align 4
  ret %result_int64 %result_val
}

define internal %result_int64 @count_flt32(ptr %arr, i64 %length, float %value) {
entry:
  %i = alloca i64, align 8
  %count = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %value3 = alloca float, align 4
  store float %value, ptr %value3, align 4
  store i64 0, ptr %count, align 4
  store i64 0, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont, %entry
  %0 = load i64, ptr %i, align 4
  %1 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %0, %1
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr = load ptr, ptr %arr1, align 8
  %2 = load i64, ptr %i, align 4
  %elem_ptr = getelementptr float, ptr %arr_ptr, i64 %2
  %elem = load float, ptr %elem_ptr, align 4
  %3 = load float, ptr %value3, align 4
  %eqtmp = fcmp oeq float %elem, %3
  br i1 %eqtmp, label %then, label %ifcont

then:                                             ; preds = %while_body
  %4 = load i64, ptr %count, align 4
  %addtmp = add i64 %4, 1
  store i64 %addtmp, ptr %count, align 4
  br label %ifcont

ifcont:                                           ; preds = %then, %while_body
  %5 = load i64, ptr %i, align 4
  %addtmp4 = add i64 %5, 1
  store i64 %addtmp4, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %6 = load i64, ptr %count, align 4
  %auto_wrap_result = alloca %result_int64, align 8
  %err_ptr = getelementptr inbounds %result_int64, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int64, ptr %auto_wrap_result, i32 0, i32 1
  store i64 %6, ptr %val_ptr, align 4
  %result_val = load %result_int64, ptr %auto_wrap_result, align 4
  ret %result_int64 %result_val
}

define internal %result_int64 @count_flt64(ptr %arr, i64 %length, double %value) {
entry:
  %i = alloca i64, align 8
  %count = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %value3 = alloca double, align 8
  store double %value, ptr %value3, align 8
  store i64 0, ptr %count, align 4
  store i64 0, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont, %entry
  %0 = load i64, ptr %i, align 4
  %1 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %0, %1
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr = load ptr, ptr %arr1, align 8
  %2 = load i64, ptr %i, align 4
  %elem_ptr = getelementptr double, ptr %arr_ptr, i64 %2
  %elem = load double, ptr %elem_ptr, align 8
  %3 = load double, ptr %value3, align 8
  %eqtmp = fcmp oeq double %elem, %3
  br i1 %eqtmp, label %then, label %ifcont

then:                                             ; preds = %while_body
  %4 = load i64, ptr %count, align 4
  %addtmp = add i64 %4, 1
  store i64 %addtmp, ptr %count, align 4
  br label %ifcont

ifcont:                                           ; preds = %then, %while_body
  %5 = load i64, ptr %i, align 4
  %addtmp4 = add i64 %5, 1
  store i64 %addtmp4, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %6 = load i64, ptr %count, align 4
  %auto_wrap_result = alloca %result_int64, align 8
  %err_ptr = getelementptr inbounds %result_int64, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int64, ptr %auto_wrap_result, i32 0, i32 1
  store i64 %6, ptr %val_ptr, align 4
  %result_val = load %result_int64, ptr %auto_wrap_result, align 4
  ret %result_int64 %result_val
}

define internal %result_int8 @reverse_int8(ptr %arr, i64 %length) {
entry:
  %tmp = alloca i8, align 1
  %one = alloca i64, align 8
  %last = alloca i64, align 8
  %start = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  store i64 0, ptr %start, align 4
  store i64 0, ptr %last, align 4
  store i64 1, ptr %one, align 4
  %0 = load i64, ptr %length2, align 4
  store i64 %0, ptr %last, align 4
  %1 = load i64, ptr %last, align 4
  %2 = load i64, ptr %one, align 4
  %subtmp = sub i64 %1, %2
  store i64 %subtmp, ptr %last, align 4
  br label %while_cond

while_cond:                                       ; preds = %while_body, %entry
  %3 = load i64, ptr %start, align 4
  %4 = load i64, ptr %last, align 4
  %lttmp = icmp slt i64 %3, %4
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr = load ptr, ptr %arr1, align 8
  %5 = load i64, ptr %start, align 4
  %elem_ptr = getelementptr i8, ptr %arr_ptr, i64 %5
  %elem = load i8, ptr %elem_ptr, align 1
  store i8 %elem, ptr %tmp, align 1
  %arr_ptr3 = load ptr, ptr %arr1, align 8
  %6 = load i64, ptr %start, align 4
  %arr_ptr4 = load ptr, ptr %arr1, align 8
  %7 = load i64, ptr %last, align 4
  %elem_ptr5 = getelementptr i8, ptr %arr_ptr4, i64 %7
  %elem6 = load i8, ptr %elem_ptr5, align 1
  %elem_ptr7 = getelementptr i8, ptr %arr_ptr3, i64 %6
  store i8 %elem6, ptr %elem_ptr7, align 1
  %arr_ptr8 = load ptr, ptr %arr1, align 8
  %8 = load i64, ptr %last, align 4
  %9 = load i8, ptr %tmp, align 1
  %elem_ptr9 = getelementptr i8, ptr %arr_ptr8, i64 %8
  store i8 %9, ptr %elem_ptr9, align 1
  %10 = load i64, ptr %start, align 4
  %11 = load i64, ptr %one, align 4
  %addtmp = add i64 %10, %11
  store i64 %addtmp, ptr %start, align 4
  %12 = load i64, ptr %last, align 4
  %13 = load i64, ptr %one, align 4
  %subtmp10 = sub i64 %12, %13
  store i64 %subtmp10, ptr %last, align 4
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

define internal %result_int8 @reverse_int16(ptr %arr, i64 %length) {
entry:
  %tmp = alloca i16, align 2
  %one = alloca i64, align 8
  %last = alloca i64, align 8
  %start = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  store i64 0, ptr %start, align 4
  store i64 0, ptr %last, align 4
  store i64 1, ptr %one, align 4
  %0 = load i64, ptr %length2, align 4
  store i64 %0, ptr %last, align 4
  %1 = load i64, ptr %last, align 4
  %2 = load i64, ptr %one, align 4
  %subtmp = sub i64 %1, %2
  store i64 %subtmp, ptr %last, align 4
  br label %while_cond

while_cond:                                       ; preds = %while_body, %entry
  %3 = load i64, ptr %start, align 4
  %4 = load i64, ptr %last, align 4
  %lttmp = icmp slt i64 %3, %4
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr = load ptr, ptr %arr1, align 8
  %5 = load i64, ptr %start, align 4
  %elem_ptr = getelementptr i16, ptr %arr_ptr, i64 %5
  %elem = load i16, ptr %elem_ptr, align 2
  store i16 %elem, ptr %tmp, align 2
  %arr_ptr3 = load ptr, ptr %arr1, align 8
  %6 = load i64, ptr %start, align 4
  %arr_ptr4 = load ptr, ptr %arr1, align 8
  %7 = load i64, ptr %last, align 4
  %elem_ptr5 = getelementptr i16, ptr %arr_ptr4, i64 %7
  %elem6 = load i16, ptr %elem_ptr5, align 2
  %elem_ptr7 = getelementptr i16, ptr %arr_ptr3, i64 %6
  store i16 %elem6, ptr %elem_ptr7, align 2
  %arr_ptr8 = load ptr, ptr %arr1, align 8
  %8 = load i64, ptr %last, align 4
  %9 = load i16, ptr %tmp, align 2
  %elem_ptr9 = getelementptr i16, ptr %arr_ptr8, i64 %8
  store i16 %9, ptr %elem_ptr9, align 2
  %10 = load i64, ptr %start, align 4
  %11 = load i64, ptr %one, align 4
  %addtmp = add i64 %10, %11
  store i64 %addtmp, ptr %start, align 4
  %12 = load i64, ptr %last, align 4
  %13 = load i64, ptr %one, align 4
  %subtmp10 = sub i64 %12, %13
  store i64 %subtmp10, ptr %last, align 4
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

define internal %result_int8 @reverse_int32(ptr %arr, i64 %length) {
entry:
  %tmp = alloca i32, align 4
  %one = alloca i64, align 8
  %last = alloca i64, align 8
  %start = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  store i64 0, ptr %start, align 4
  store i64 0, ptr %last, align 4
  store i64 1, ptr %one, align 4
  %0 = load i64, ptr %length2, align 4
  store i64 %0, ptr %last, align 4
  %1 = load i64, ptr %last, align 4
  %2 = load i64, ptr %one, align 4
  %subtmp = sub i64 %1, %2
  store i64 %subtmp, ptr %last, align 4
  br label %while_cond

while_cond:                                       ; preds = %while_body, %entry
  %3 = load i64, ptr %start, align 4
  %4 = load i64, ptr %last, align 4
  %lttmp = icmp slt i64 %3, %4
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr = load ptr, ptr %arr1, align 8
  %5 = load i64, ptr %start, align 4
  %elem_ptr = getelementptr i32, ptr %arr_ptr, i64 %5
  %elem = load i32, ptr %elem_ptr, align 4
  store i32 %elem, ptr %tmp, align 4
  %arr_ptr3 = load ptr, ptr %arr1, align 8
  %6 = load i64, ptr %start, align 4
  %arr_ptr4 = load ptr, ptr %arr1, align 8
  %7 = load i64, ptr %last, align 4
  %elem_ptr5 = getelementptr i32, ptr %arr_ptr4, i64 %7
  %elem6 = load i32, ptr %elem_ptr5, align 4
  %elem_ptr7 = getelementptr i32, ptr %arr_ptr3, i64 %6
  store i32 %elem6, ptr %elem_ptr7, align 4
  %arr_ptr8 = load ptr, ptr %arr1, align 8
  %8 = load i64, ptr %last, align 4
  %9 = load i32, ptr %tmp, align 4
  %elem_ptr9 = getelementptr i32, ptr %arr_ptr8, i64 %8
  store i32 %9, ptr %elem_ptr9, align 4
  %10 = load i64, ptr %start, align 4
  %11 = load i64, ptr %one, align 4
  %addtmp = add i64 %10, %11
  store i64 %addtmp, ptr %start, align 4
  %12 = load i64, ptr %last, align 4
  %13 = load i64, ptr %one, align 4
  %subtmp10 = sub i64 %12, %13
  store i64 %subtmp10, ptr %last, align 4
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

define internal %result_int8 @reverse_int64(ptr %arr, i64 %length) {
entry:
  %tmp = alloca i64, align 8
  %one = alloca i64, align 8
  %last = alloca i64, align 8
  %start = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  store i64 0, ptr %start, align 4
  store i64 0, ptr %last, align 4
  store i64 1, ptr %one, align 4
  %0 = load i64, ptr %length2, align 4
  store i64 %0, ptr %last, align 4
  %1 = load i64, ptr %last, align 4
  %2 = load i64, ptr %one, align 4
  %subtmp = sub i64 %1, %2
  store i64 %subtmp, ptr %last, align 4
  br label %while_cond

while_cond:                                       ; preds = %while_body, %entry
  %3 = load i64, ptr %start, align 4
  %4 = load i64, ptr %last, align 4
  %lttmp = icmp slt i64 %3, %4
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr = load ptr, ptr %arr1, align 8
  %5 = load i64, ptr %start, align 4
  %elem_ptr = getelementptr i64, ptr %arr_ptr, i64 %5
  %elem = load i64, ptr %elem_ptr, align 4
  store i64 %elem, ptr %tmp, align 4
  %arr_ptr3 = load ptr, ptr %arr1, align 8
  %6 = load i64, ptr %start, align 4
  %arr_ptr4 = load ptr, ptr %arr1, align 8
  %7 = load i64, ptr %last, align 4
  %elem_ptr5 = getelementptr i64, ptr %arr_ptr4, i64 %7
  %elem6 = load i64, ptr %elem_ptr5, align 4
  %elem_ptr7 = getelementptr i64, ptr %arr_ptr3, i64 %6
  store i64 %elem6, ptr %elem_ptr7, align 4
  %arr_ptr8 = load ptr, ptr %arr1, align 8
  %8 = load i64, ptr %last, align 4
  %9 = load i64, ptr %tmp, align 4
  %elem_ptr9 = getelementptr i64, ptr %arr_ptr8, i64 %8
  store i64 %9, ptr %elem_ptr9, align 4
  %10 = load i64, ptr %start, align 4
  %11 = load i64, ptr %one, align 4
  %addtmp = add i64 %10, %11
  store i64 %addtmp, ptr %start, align 4
  %12 = load i64, ptr %last, align 4
  %13 = load i64, ptr %one, align 4
  %subtmp10 = sub i64 %12, %13
  store i64 %subtmp10, ptr %last, align 4
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

define internal %result_int8 @reverse_uint8(ptr %arr, i64 %length) {
entry:
  %tmp = alloca i8, align 1
  %one = alloca i64, align 8
  %last = alloca i64, align 8
  %start = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  store i64 0, ptr %start, align 4
  store i64 0, ptr %last, align 4
  store i64 1, ptr %one, align 4
  %0 = load i64, ptr %length2, align 4
  store i64 %0, ptr %last, align 4
  %1 = load i64, ptr %last, align 4
  %2 = load i64, ptr %one, align 4
  %subtmp = sub i64 %1, %2
  store i64 %subtmp, ptr %last, align 4
  br label %while_cond

while_cond:                                       ; preds = %while_body, %entry
  %3 = load i64, ptr %start, align 4
  %4 = load i64, ptr %last, align 4
  %lttmp = icmp slt i64 %3, %4
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr = load ptr, ptr %arr1, align 8
  %5 = load i64, ptr %start, align 4
  %elem_ptr = getelementptr i8, ptr %arr_ptr, i64 %5
  %elem = load i8, ptr %elem_ptr, align 1
  store i8 %elem, ptr %tmp, align 1
  %arr_ptr3 = load ptr, ptr %arr1, align 8
  %6 = load i64, ptr %start, align 4
  %arr_ptr4 = load ptr, ptr %arr1, align 8
  %7 = load i64, ptr %last, align 4
  %elem_ptr5 = getelementptr i8, ptr %arr_ptr4, i64 %7
  %elem6 = load i8, ptr %elem_ptr5, align 1
  %elem_ptr7 = getelementptr i8, ptr %arr_ptr3, i64 %6
  store i8 %elem6, ptr %elem_ptr7, align 1
  %arr_ptr8 = load ptr, ptr %arr1, align 8
  %8 = load i64, ptr %last, align 4
  %9 = load i8, ptr %tmp, align 1
  %elem_ptr9 = getelementptr i8, ptr %arr_ptr8, i64 %8
  store i8 %9, ptr %elem_ptr9, align 1
  %10 = load i64, ptr %start, align 4
  %11 = load i64, ptr %one, align 4
  %addtmp = add i64 %10, %11
  store i64 %addtmp, ptr %start, align 4
  %12 = load i64, ptr %last, align 4
  %13 = load i64, ptr %one, align 4
  %subtmp10 = sub i64 %12, %13
  store i64 %subtmp10, ptr %last, align 4
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

define internal %result_int8 @reverse_uint16(ptr %arr, i64 %length) {
entry:
  %tmp = alloca i16, align 2
  %one = alloca i64, align 8
  %last = alloca i64, align 8
  %start = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  store i64 0, ptr %start, align 4
  store i64 0, ptr %last, align 4
  store i64 1, ptr %one, align 4
  %0 = load i64, ptr %length2, align 4
  store i64 %0, ptr %last, align 4
  %1 = load i64, ptr %last, align 4
  %2 = load i64, ptr %one, align 4
  %subtmp = sub i64 %1, %2
  store i64 %subtmp, ptr %last, align 4
  br label %while_cond

while_cond:                                       ; preds = %while_body, %entry
  %3 = load i64, ptr %start, align 4
  %4 = load i64, ptr %last, align 4
  %lttmp = icmp slt i64 %3, %4
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr = load ptr, ptr %arr1, align 8
  %5 = load i64, ptr %start, align 4
  %elem_ptr = getelementptr i16, ptr %arr_ptr, i64 %5
  %elem = load i16, ptr %elem_ptr, align 2
  store i16 %elem, ptr %tmp, align 2
  %arr_ptr3 = load ptr, ptr %arr1, align 8
  %6 = load i64, ptr %start, align 4
  %arr_ptr4 = load ptr, ptr %arr1, align 8
  %7 = load i64, ptr %last, align 4
  %elem_ptr5 = getelementptr i16, ptr %arr_ptr4, i64 %7
  %elem6 = load i16, ptr %elem_ptr5, align 2
  %elem_ptr7 = getelementptr i16, ptr %arr_ptr3, i64 %6
  store i16 %elem6, ptr %elem_ptr7, align 2
  %arr_ptr8 = load ptr, ptr %arr1, align 8
  %8 = load i64, ptr %last, align 4
  %9 = load i16, ptr %tmp, align 2
  %elem_ptr9 = getelementptr i16, ptr %arr_ptr8, i64 %8
  store i16 %9, ptr %elem_ptr9, align 2
  %10 = load i64, ptr %start, align 4
  %11 = load i64, ptr %one, align 4
  %addtmp = add i64 %10, %11
  store i64 %addtmp, ptr %start, align 4
  %12 = load i64, ptr %last, align 4
  %13 = load i64, ptr %one, align 4
  %subtmp10 = sub i64 %12, %13
  store i64 %subtmp10, ptr %last, align 4
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

define internal %result_int8 @reverse_uint32(ptr %arr, i64 %length) {
entry:
  %tmp = alloca i32, align 4
  %one = alloca i64, align 8
  %last = alloca i64, align 8
  %start = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  store i64 0, ptr %start, align 4
  store i64 0, ptr %last, align 4
  store i64 1, ptr %one, align 4
  %0 = load i64, ptr %length2, align 4
  store i64 %0, ptr %last, align 4
  %1 = load i64, ptr %last, align 4
  %2 = load i64, ptr %one, align 4
  %subtmp = sub i64 %1, %2
  store i64 %subtmp, ptr %last, align 4
  br label %while_cond

while_cond:                                       ; preds = %while_body, %entry
  %3 = load i64, ptr %start, align 4
  %4 = load i64, ptr %last, align 4
  %lttmp = icmp slt i64 %3, %4
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr = load ptr, ptr %arr1, align 8
  %5 = load i64, ptr %start, align 4
  %elem_ptr = getelementptr i32, ptr %arr_ptr, i64 %5
  %elem = load i32, ptr %elem_ptr, align 4
  store i32 %elem, ptr %tmp, align 4
  %arr_ptr3 = load ptr, ptr %arr1, align 8
  %6 = load i64, ptr %start, align 4
  %arr_ptr4 = load ptr, ptr %arr1, align 8
  %7 = load i64, ptr %last, align 4
  %elem_ptr5 = getelementptr i32, ptr %arr_ptr4, i64 %7
  %elem6 = load i32, ptr %elem_ptr5, align 4
  %elem_ptr7 = getelementptr i32, ptr %arr_ptr3, i64 %6
  store i32 %elem6, ptr %elem_ptr7, align 4
  %arr_ptr8 = load ptr, ptr %arr1, align 8
  %8 = load i64, ptr %last, align 4
  %9 = load i32, ptr %tmp, align 4
  %elem_ptr9 = getelementptr i32, ptr %arr_ptr8, i64 %8
  store i32 %9, ptr %elem_ptr9, align 4
  %10 = load i64, ptr %start, align 4
  %11 = load i64, ptr %one, align 4
  %addtmp = add i64 %10, %11
  store i64 %addtmp, ptr %start, align 4
  %12 = load i64, ptr %last, align 4
  %13 = load i64, ptr %one, align 4
  %subtmp10 = sub i64 %12, %13
  store i64 %subtmp10, ptr %last, align 4
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

define internal %result_int8 @reverse_uint64(ptr %arr, i64 %length) {
entry:
  %tmp = alloca i64, align 8
  %one = alloca i64, align 8
  %last = alloca i64, align 8
  %start = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  store i64 0, ptr %start, align 4
  store i64 0, ptr %last, align 4
  store i64 1, ptr %one, align 4
  %0 = load i64, ptr %length2, align 4
  store i64 %0, ptr %last, align 4
  %1 = load i64, ptr %last, align 4
  %2 = load i64, ptr %one, align 4
  %subtmp = sub i64 %1, %2
  store i64 %subtmp, ptr %last, align 4
  br label %while_cond

while_cond:                                       ; preds = %while_body, %entry
  %3 = load i64, ptr %start, align 4
  %4 = load i64, ptr %last, align 4
  %lttmp = icmp slt i64 %3, %4
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr = load ptr, ptr %arr1, align 8
  %5 = load i64, ptr %start, align 4
  %elem_ptr = getelementptr i64, ptr %arr_ptr, i64 %5
  %elem = load i64, ptr %elem_ptr, align 4
  store i64 %elem, ptr %tmp, align 4
  %arr_ptr3 = load ptr, ptr %arr1, align 8
  %6 = load i64, ptr %start, align 4
  %arr_ptr4 = load ptr, ptr %arr1, align 8
  %7 = load i64, ptr %last, align 4
  %elem_ptr5 = getelementptr i64, ptr %arr_ptr4, i64 %7
  %elem6 = load i64, ptr %elem_ptr5, align 4
  %elem_ptr7 = getelementptr i64, ptr %arr_ptr3, i64 %6
  store i64 %elem6, ptr %elem_ptr7, align 4
  %arr_ptr8 = load ptr, ptr %arr1, align 8
  %8 = load i64, ptr %last, align 4
  %9 = load i64, ptr %tmp, align 4
  %elem_ptr9 = getelementptr i64, ptr %arr_ptr8, i64 %8
  store i64 %9, ptr %elem_ptr9, align 4
  %10 = load i64, ptr %start, align 4
  %11 = load i64, ptr %one, align 4
  %addtmp = add i64 %10, %11
  store i64 %addtmp, ptr %start, align 4
  %12 = load i64, ptr %last, align 4
  %13 = load i64, ptr %one, align 4
  %subtmp10 = sub i64 %12, %13
  store i64 %subtmp10, ptr %last, align 4
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

define internal %result_int8 @reverse_flt32(ptr %arr, i64 %length) {
entry:
  %tmp = alloca float, align 4
  %one = alloca i64, align 8
  %last = alloca i64, align 8
  %start = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  store i64 0, ptr %start, align 4
  store i64 0, ptr %last, align 4
  store i64 1, ptr %one, align 4
  %0 = load i64, ptr %length2, align 4
  store i64 %0, ptr %last, align 4
  %1 = load i64, ptr %last, align 4
  %2 = load i64, ptr %one, align 4
  %subtmp = sub i64 %1, %2
  store i64 %subtmp, ptr %last, align 4
  br label %while_cond

while_cond:                                       ; preds = %while_body, %entry
  %3 = load i64, ptr %start, align 4
  %4 = load i64, ptr %last, align 4
  %lttmp = icmp slt i64 %3, %4
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr = load ptr, ptr %arr1, align 8
  %5 = load i64, ptr %start, align 4
  %elem_ptr = getelementptr float, ptr %arr_ptr, i64 %5
  %elem = load float, ptr %elem_ptr, align 4
  store float %elem, ptr %tmp, align 4
  %arr_ptr3 = load ptr, ptr %arr1, align 8
  %6 = load i64, ptr %start, align 4
  %arr_ptr4 = load ptr, ptr %arr1, align 8
  %7 = load i64, ptr %last, align 4
  %elem_ptr5 = getelementptr float, ptr %arr_ptr4, i64 %7
  %elem6 = load float, ptr %elem_ptr5, align 4
  %elem_ptr7 = getelementptr float, ptr %arr_ptr3, i64 %6
  store float %elem6, ptr %elem_ptr7, align 4
  %arr_ptr8 = load ptr, ptr %arr1, align 8
  %8 = load i64, ptr %last, align 4
  %9 = load float, ptr %tmp, align 4
  %elem_ptr9 = getelementptr float, ptr %arr_ptr8, i64 %8
  store float %9, ptr %elem_ptr9, align 4
  %10 = load i64, ptr %start, align 4
  %11 = load i64, ptr %one, align 4
  %addtmp = add i64 %10, %11
  store i64 %addtmp, ptr %start, align 4
  %12 = load i64, ptr %last, align 4
  %13 = load i64, ptr %one, align 4
  %subtmp10 = sub i64 %12, %13
  store i64 %subtmp10, ptr %last, align 4
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

define internal %result_int8 @reverse_flt64(ptr %arr, i64 %length) {
entry:
  %tmp = alloca double, align 8
  %one = alloca i64, align 8
  %last = alloca i64, align 8
  %start = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  store i64 0, ptr %start, align 4
  store i64 0, ptr %last, align 4
  store i64 1, ptr %one, align 4
  %0 = load i64, ptr %length2, align 4
  store i64 %0, ptr %last, align 4
  %1 = load i64, ptr %last, align 4
  %2 = load i64, ptr %one, align 4
  %subtmp = sub i64 %1, %2
  store i64 %subtmp, ptr %last, align 4
  br label %while_cond

while_cond:                                       ; preds = %while_body, %entry
  %3 = load i64, ptr %start, align 4
  %4 = load i64, ptr %last, align 4
  %lttmp = icmp slt i64 %3, %4
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr = load ptr, ptr %arr1, align 8
  %5 = load i64, ptr %start, align 4
  %elem_ptr = getelementptr double, ptr %arr_ptr, i64 %5
  %elem = load double, ptr %elem_ptr, align 8
  store double %elem, ptr %tmp, align 8
  %arr_ptr3 = load ptr, ptr %arr1, align 8
  %6 = load i64, ptr %start, align 4
  %arr_ptr4 = load ptr, ptr %arr1, align 8
  %7 = load i64, ptr %last, align 4
  %elem_ptr5 = getelementptr double, ptr %arr_ptr4, i64 %7
  %elem6 = load double, ptr %elem_ptr5, align 8
  %elem_ptr7 = getelementptr double, ptr %arr_ptr3, i64 %6
  store double %elem6, ptr %elem_ptr7, align 8
  %arr_ptr8 = load ptr, ptr %arr1, align 8
  %8 = load i64, ptr %last, align 4
  %9 = load double, ptr %tmp, align 8
  %elem_ptr9 = getelementptr double, ptr %arr_ptr8, i64 %8
  store double %9, ptr %elem_ptr9, align 8
  %10 = load i64, ptr %start, align 4
  %11 = load i64, ptr %one, align 4
  %addtmp = add i64 %10, %11
  store i64 %addtmp, ptr %start, align 4
  %12 = load i64, ptr %last, align 4
  %13 = load i64, ptr %one, align 4
  %subtmp10 = sub i64 %12, %13
  store i64 %subtmp10, ptr %last, align 4
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

define internal %result_int8 @fill_int8(ptr %arr, i64 %length, i8 %value) {
entry:
  %i = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %value3 = alloca i8, align 1
  store i8 %value, ptr %value3, align 1
  store i64 0, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %while_body, %entry
  %0 = load i64, ptr %i, align 4
  %1 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %0, %1
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr = load ptr, ptr %arr1, align 8
  %2 = load i64, ptr %i, align 4
  %3 = load i8, ptr %value3, align 1
  %elem_ptr = getelementptr i8, ptr %arr_ptr, i64 %2
  store i8 %3, ptr %elem_ptr, align 1
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

define internal %result_int8 @fill_int16(ptr %arr, i64 %length, i16 %value) {
entry:
  %i = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %value3 = alloca i16, align 2
  store i16 %value, ptr %value3, align 2
  store i64 0, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %while_body, %entry
  %0 = load i64, ptr %i, align 4
  %1 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %0, %1
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr = load ptr, ptr %arr1, align 8
  %2 = load i64, ptr %i, align 4
  %3 = load i16, ptr %value3, align 2
  %elem_ptr = getelementptr i16, ptr %arr_ptr, i64 %2
  store i16 %3, ptr %elem_ptr, align 2
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

define internal %result_int8 @fill_int32(ptr %arr, i64 %length, i32 %value) {
entry:
  %i = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %value3 = alloca i32, align 4
  store i32 %value, ptr %value3, align 4
  store i64 0, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %while_body, %entry
  %0 = load i64, ptr %i, align 4
  %1 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %0, %1
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr = load ptr, ptr %arr1, align 8
  %2 = load i64, ptr %i, align 4
  %3 = load i32, ptr %value3, align 4
  %elem_ptr = getelementptr i32, ptr %arr_ptr, i64 %2
  store i32 %3, ptr %elem_ptr, align 4
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

define internal %result_int8 @fill_int64(ptr %arr, i64 %length, i64 %value) {
entry:
  %i = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %value3 = alloca i64, align 8
  store i64 %value, ptr %value3, align 4
  store i64 0, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %while_body, %entry
  %0 = load i64, ptr %i, align 4
  %1 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %0, %1
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr = load ptr, ptr %arr1, align 8
  %2 = load i64, ptr %i, align 4
  %3 = load i64, ptr %value3, align 4
  %elem_ptr = getelementptr i64, ptr %arr_ptr, i64 %2
  store i64 %3, ptr %elem_ptr, align 4
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

define internal %result_int8 @fill_uint8(ptr %arr, i64 %length, i8 %value) {
entry:
  %i = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %value3 = alloca i8, align 1
  store i8 %value, ptr %value3, align 1
  store i64 0, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %while_body, %entry
  %0 = load i64, ptr %i, align 4
  %1 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %0, %1
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr = load ptr, ptr %arr1, align 8
  %2 = load i64, ptr %i, align 4
  %3 = load i8, ptr %value3, align 1
  %elem_ptr = getelementptr i8, ptr %arr_ptr, i64 %2
  store i8 %3, ptr %elem_ptr, align 1
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

define internal %result_int8 @fill_uint16(ptr %arr, i64 %length, i16 %value) {
entry:
  %i = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %value3 = alloca i16, align 2
  store i16 %value, ptr %value3, align 2
  store i64 0, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %while_body, %entry
  %0 = load i64, ptr %i, align 4
  %1 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %0, %1
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr = load ptr, ptr %arr1, align 8
  %2 = load i64, ptr %i, align 4
  %3 = load i16, ptr %value3, align 2
  %elem_ptr = getelementptr i16, ptr %arr_ptr, i64 %2
  store i16 %3, ptr %elem_ptr, align 2
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

define internal %result_int8 @fill_uint32(ptr %arr, i64 %length, i32 %value) {
entry:
  %i = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %value3 = alloca i32, align 4
  store i32 %value, ptr %value3, align 4
  store i64 0, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %while_body, %entry
  %0 = load i64, ptr %i, align 4
  %1 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %0, %1
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr = load ptr, ptr %arr1, align 8
  %2 = load i64, ptr %i, align 4
  %3 = load i32, ptr %value3, align 4
  %elem_ptr = getelementptr i32, ptr %arr_ptr, i64 %2
  store i32 %3, ptr %elem_ptr, align 4
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

define internal %result_int8 @fill_uint64(ptr %arr, i64 %length, i64 %value) {
entry:
  %i = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %value3 = alloca i64, align 8
  store i64 %value, ptr %value3, align 4
  store i64 0, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %while_body, %entry
  %0 = load i64, ptr %i, align 4
  %1 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %0, %1
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr = load ptr, ptr %arr1, align 8
  %2 = load i64, ptr %i, align 4
  %3 = load i64, ptr %value3, align 4
  %elem_ptr = getelementptr i64, ptr %arr_ptr, i64 %2
  store i64 %3, ptr %elem_ptr, align 4
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

define internal %result_int8 @fill_flt32(ptr %arr, i64 %length, float %value) {
entry:
  %i = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %value3 = alloca float, align 4
  store float %value, ptr %value3, align 4
  store i64 0, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %while_body, %entry
  %0 = load i64, ptr %i, align 4
  %1 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %0, %1
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr = load ptr, ptr %arr1, align 8
  %2 = load i64, ptr %i, align 4
  %3 = load float, ptr %value3, align 4
  %elem_ptr = getelementptr float, ptr %arr_ptr, i64 %2
  store float %3, ptr %elem_ptr, align 4
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

define internal %result_int8 @fill_flt64(ptr %arr, i64 %length, double %value) {
entry:
  %i = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %value3 = alloca double, align 8
  store double %value, ptr %value3, align 8
  store i64 0, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %while_body, %entry
  %0 = load i64, ptr %i, align 4
  %1 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %0, %1
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr = load ptr, ptr %arr1, align 8
  %2 = load i64, ptr %i, align 4
  %3 = load double, ptr %value3, align 8
  %elem_ptr = getelementptr double, ptr %arr_ptr, i64 %2
  store double %3, ptr %elem_ptr, align 8
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

define internal %result_int8 @copy_int8(ptr %dest, ptr %src, i64 %length) {
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
  %dest_ptr = load ptr, ptr %dest1, align 8
  %2 = load i64, ptr %i, align 4
  %src_ptr = load ptr, ptr %src2, align 8
  %3 = load i64, ptr %i, align 4
  %elem_ptr = getelementptr i8, ptr %src_ptr, i64 %3
  %elem = load i8, ptr %elem_ptr, align 1
  %elem_ptr4 = getelementptr i8, ptr %dest_ptr, i64 %2
  store i8 %elem, ptr %elem_ptr4, align 1
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

define internal %result_int8 @copy_int16(ptr %dest, ptr %src, i64 %length) {
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
  %dest_ptr = load ptr, ptr %dest1, align 8
  %2 = load i64, ptr %i, align 4
  %src_ptr = load ptr, ptr %src2, align 8
  %3 = load i64, ptr %i, align 4
  %elem_ptr = getelementptr i16, ptr %src_ptr, i64 %3
  %elem = load i16, ptr %elem_ptr, align 2
  %elem_ptr4 = getelementptr i16, ptr %dest_ptr, i64 %2
  store i16 %elem, ptr %elem_ptr4, align 2
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
  %dest_ptr = load ptr, ptr %dest1, align 8
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

define internal %result_int8 @copy_int64(ptr %dest, ptr %src, i64 %length) {
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
  %dest_ptr = load ptr, ptr %dest1, align 8
  %2 = load i64, ptr %i, align 4
  %src_ptr = load ptr, ptr %src2, align 8
  %3 = load i64, ptr %i, align 4
  %elem_ptr = getelementptr i64, ptr %src_ptr, i64 %3
  %elem = load i64, ptr %elem_ptr, align 4
  %elem_ptr4 = getelementptr i64, ptr %dest_ptr, i64 %2
  store i64 %elem, ptr %elem_ptr4, align 4
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

define internal %result_int8 @copy_uint8(ptr %dest, ptr %src, i64 %length) {
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
  %dest_ptr = load ptr, ptr %dest1, align 8
  %2 = load i64, ptr %i, align 4
  %src_ptr = load ptr, ptr %src2, align 8
  %3 = load i64, ptr %i, align 4
  %elem_ptr = getelementptr i8, ptr %src_ptr, i64 %3
  %elem = load i8, ptr %elem_ptr, align 1
  %elem_ptr4 = getelementptr i8, ptr %dest_ptr, i64 %2
  store i8 %elem, ptr %elem_ptr4, align 1
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

define internal %result_int8 @copy_uint16(ptr %dest, ptr %src, i64 %length) {
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
  %dest_ptr = load ptr, ptr %dest1, align 8
  %2 = load i64, ptr %i, align 4
  %src_ptr = load ptr, ptr %src2, align 8
  %3 = load i64, ptr %i, align 4
  %elem_ptr = getelementptr i16, ptr %src_ptr, i64 %3
  %elem = load i16, ptr %elem_ptr, align 2
  %elem_ptr4 = getelementptr i16, ptr %dest_ptr, i64 %2
  store i16 %elem, ptr %elem_ptr4, align 2
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

define internal %result_int8 @copy_uint32(ptr %dest, ptr %src, i64 %length) {
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
  %dest_ptr = load ptr, ptr %dest1, align 8
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

define internal %result_int8 @copy_uint64(ptr %dest, ptr %src, i64 %length) {
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
  %dest_ptr = load ptr, ptr %dest1, align 8
  %2 = load i64, ptr %i, align 4
  %src_ptr = load ptr, ptr %src2, align 8
  %3 = load i64, ptr %i, align 4
  %elem_ptr = getelementptr i64, ptr %src_ptr, i64 %3
  %elem = load i64, ptr %elem_ptr, align 4
  %elem_ptr4 = getelementptr i64, ptr %dest_ptr, i64 %2
  store i64 %elem, ptr %elem_ptr4, align 4
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

define internal %result_int8 @copy_flt32(ptr %dest, ptr %src, i64 %length) {
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
  %dest_ptr = load ptr, ptr %dest1, align 8
  %2 = load i64, ptr %i, align 4
  %src_ptr = load ptr, ptr %src2, align 8
  %3 = load i64, ptr %i, align 4
  %elem_ptr = getelementptr float, ptr %src_ptr, i64 %3
  %elem = load float, ptr %elem_ptr, align 4
  %elem_ptr4 = getelementptr float, ptr %dest_ptr, i64 %2
  store float %elem, ptr %elem_ptr4, align 4
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

define internal %result_int8 @copy_flt64(ptr %dest, ptr %src, i64 %length) {
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
  %dest_ptr = load ptr, ptr %dest1, align 8
  %2 = load i64, ptr %i, align 4
  %src_ptr = load ptr, ptr %src2, align 8
  %3 = load i64, ptr %i, align 4
  %elem_ptr = getelementptr double, ptr %src_ptr, i64 %3
  %elem = load double, ptr %elem_ptr, align 8
  %elem_ptr4 = getelementptr double, ptr %dest_ptr, i64 %2
  store double %elem, ptr %elem_ptr4, align 8
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

define internal %result_int8 @min_int8(ptr %arr, i64 %length) {
entry:
  %i = alloca i64, align 8
  %minVal = alloca i8, align 1
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %arr_ptr = load ptr, ptr %arr1, align 8
  %elem_ptr = getelementptr i8, ptr %arr_ptr, i64 0
  %elem = load i8, ptr %elem_ptr, align 1
  store i8 %elem, ptr %minVal, align 1
  store i64 1, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont, %entry
  %0 = load i64, ptr %i, align 4
  %1 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %0, %1
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr3 = load ptr, ptr %arr1, align 8
  %2 = load i64, ptr %i, align 4
  %elem_ptr4 = getelementptr i8, ptr %arr_ptr3, i64 %2
  %elem5 = load i8, ptr %elem_ptr4, align 1
  %3 = load i8, ptr %minVal, align 1
  %lttmp6 = icmp slt i8 %elem5, %3
  br i1 %lttmp6, label %then, label %ifcont

then:                                             ; preds = %while_body
  %arr_ptr7 = load ptr, ptr %arr1, align 8
  %4 = load i64, ptr %i, align 4
  %elem_ptr8 = getelementptr i8, ptr %arr_ptr7, i64 %4
  %elem9 = load i8, ptr %elem_ptr8, align 1
  store i8 %elem9, ptr %minVal, align 1
  br label %ifcont

ifcont:                                           ; preds = %then, %while_body
  %5 = load i64, ptr %i, align 4
  %addtmp = add i64 %5, 1
  store i64 %addtmp, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %6 = load i8, ptr %minVal, align 1
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 %6, ptr %val_ptr, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val
}

define internal %result_int16 @min_int16(ptr %arr, i64 %length) {
entry:
  %i = alloca i64, align 8
  %minVal = alloca i16, align 2
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %arr_ptr = load ptr, ptr %arr1, align 8
  %elem_ptr = getelementptr i16, ptr %arr_ptr, i64 0
  %elem = load i16, ptr %elem_ptr, align 2
  store i16 %elem, ptr %minVal, align 2
  store i64 1, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont, %entry
  %0 = load i64, ptr %i, align 4
  %1 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %0, %1
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr3 = load ptr, ptr %arr1, align 8
  %2 = load i64, ptr %i, align 4
  %elem_ptr4 = getelementptr i16, ptr %arr_ptr3, i64 %2
  %elem5 = load i16, ptr %elem_ptr4, align 2
  %3 = load i16, ptr %minVal, align 2
  %lttmp6 = icmp slt i16 %elem5, %3
  br i1 %lttmp6, label %then, label %ifcont

then:                                             ; preds = %while_body
  %arr_ptr7 = load ptr, ptr %arr1, align 8
  %4 = load i64, ptr %i, align 4
  %elem_ptr8 = getelementptr i16, ptr %arr_ptr7, i64 %4
  %elem9 = load i16, ptr %elem_ptr8, align 2
  store i16 %elem9, ptr %minVal, align 2
  br label %ifcont

ifcont:                                           ; preds = %then, %while_body
  %5 = load i64, ptr %i, align 4
  %addtmp = add i64 %5, 1
  store i64 %addtmp, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %6 = load i16, ptr %minVal, align 2
  %auto_wrap_result = alloca %result_int16, align 8
  %err_ptr = getelementptr inbounds %result_int16, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int16, ptr %auto_wrap_result, i32 0, i32 1
  store i16 %6, ptr %val_ptr, align 2
  %result_val = load %result_int16, ptr %auto_wrap_result, align 2
  ret %result_int16 %result_val
}

define internal %result_int32 @min_int32(ptr %arr, i64 %length) {
entry:
  %i = alloca i64, align 8
  %minVal = alloca i32, align 4
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %arr_ptr = load ptr, ptr %arr1, align 8
  %elem_ptr = getelementptr i32, ptr %arr_ptr, i64 0
  %elem = load i32, ptr %elem_ptr, align 4
  store i32 %elem, ptr %minVal, align 4
  store i64 1, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont, %entry
  %0 = load i64, ptr %i, align 4
  %1 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %0, %1
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr3 = load ptr, ptr %arr1, align 8
  %2 = load i64, ptr %i, align 4
  %elem_ptr4 = getelementptr i32, ptr %arr_ptr3, i64 %2
  %elem5 = load i32, ptr %elem_ptr4, align 4
  %3 = load i32, ptr %minVal, align 4
  %lttmp6 = icmp slt i32 %elem5, %3
  br i1 %lttmp6, label %then, label %ifcont

then:                                             ; preds = %while_body
  %arr_ptr7 = load ptr, ptr %arr1, align 8
  %4 = load i64, ptr %i, align 4
  %elem_ptr8 = getelementptr i32, ptr %arr_ptr7, i64 %4
  %elem9 = load i32, ptr %elem_ptr8, align 4
  store i32 %elem9, ptr %minVal, align 4
  br label %ifcont

ifcont:                                           ; preds = %then, %while_body
  %5 = load i64, ptr %i, align 4
  %addtmp = add i64 %5, 1
  store i64 %addtmp, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %6 = load i32, ptr %minVal, align 4
  %auto_wrap_result = alloca %result_int32, align 8
  %err_ptr = getelementptr inbounds %result_int32, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int32, ptr %auto_wrap_result, i32 0, i32 1
  store i32 %6, ptr %val_ptr, align 4
  %result_val = load %result_int32, ptr %auto_wrap_result, align 4
  ret %result_int32 %result_val
}

define internal %result_int64 @min_int64(ptr %arr, i64 %length) {
entry:
  %i = alloca i64, align 8
  %minVal = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %arr_ptr = load ptr, ptr %arr1, align 8
  %elem_ptr = getelementptr i64, ptr %arr_ptr, i64 0
  %elem = load i64, ptr %elem_ptr, align 4
  store i64 %elem, ptr %minVal, align 4
  store i64 1, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont, %entry
  %0 = load i64, ptr %i, align 4
  %1 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %0, %1
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr3 = load ptr, ptr %arr1, align 8
  %2 = load i64, ptr %i, align 4
  %elem_ptr4 = getelementptr i64, ptr %arr_ptr3, i64 %2
  %elem5 = load i64, ptr %elem_ptr4, align 4
  %3 = load i64, ptr %minVal, align 4
  %lttmp6 = icmp slt i64 %elem5, %3
  br i1 %lttmp6, label %then, label %ifcont

then:                                             ; preds = %while_body
  %arr_ptr7 = load ptr, ptr %arr1, align 8
  %4 = load i64, ptr %i, align 4
  %elem_ptr8 = getelementptr i64, ptr %arr_ptr7, i64 %4
  %elem9 = load i64, ptr %elem_ptr8, align 4
  store i64 %elem9, ptr %minVal, align 4
  br label %ifcont

ifcont:                                           ; preds = %then, %while_body
  %5 = load i64, ptr %i, align 4
  %addtmp = add i64 %5, 1
  store i64 %addtmp, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %6 = load i64, ptr %minVal, align 4
  %auto_wrap_result = alloca %result_int64, align 8
  %err_ptr = getelementptr inbounds %result_int64, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int64, ptr %auto_wrap_result, i32 0, i32 1
  store i64 %6, ptr %val_ptr, align 4
  %result_val = load %result_int64, ptr %auto_wrap_result, align 4
  ret %result_int64 %result_val
}

define internal %result_uint8 @min_uint8(ptr %arr, i64 %length) {
entry:
  %i = alloca i64, align 8
  %minVal = alloca i8, align 1
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %arr_ptr = load ptr, ptr %arr1, align 8
  %elem_ptr = getelementptr i8, ptr %arr_ptr, i64 0
  %elem = load i8, ptr %elem_ptr, align 1
  store i8 %elem, ptr %minVal, align 1
  store i64 1, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont, %entry
  %0 = load i64, ptr %i, align 4
  %1 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %0, %1
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr3 = load ptr, ptr %arr1, align 8
  %2 = load i64, ptr %i, align 4
  %elem_ptr4 = getelementptr i8, ptr %arr_ptr3, i64 %2
  %elem5 = load i8, ptr %elem_ptr4, align 1
  %3 = load i8, ptr %minVal, align 1
  %lttmp6 = icmp slt i8 %elem5, %3
  br i1 %lttmp6, label %then, label %ifcont

then:                                             ; preds = %while_body
  %arr_ptr7 = load ptr, ptr %arr1, align 8
  %4 = load i64, ptr %i, align 4
  %elem_ptr8 = getelementptr i8, ptr %arr_ptr7, i64 %4
  %elem9 = load i8, ptr %elem_ptr8, align 1
  store i8 %elem9, ptr %minVal, align 1
  br label %ifcont

ifcont:                                           ; preds = %then, %while_body
  %5 = load i64, ptr %i, align 4
  %addtmp = add i64 %5, 1
  store i64 %addtmp, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %6 = load i8, ptr %minVal, align 1
  %auto_wrap_result = alloca %result_uint8, align 8
  %err_ptr = getelementptr inbounds %result_uint8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_uint8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 %6, ptr %val_ptr, align 1
  %result_val = load %result_uint8, ptr %auto_wrap_result, align 1
  ret %result_uint8 %result_val
}

define internal %result_uint16 @min_uint16(ptr %arr, i64 %length) {
entry:
  %i = alloca i64, align 8
  %minVal = alloca i16, align 2
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %arr_ptr = load ptr, ptr %arr1, align 8
  %elem_ptr = getelementptr i16, ptr %arr_ptr, i64 0
  %elem = load i16, ptr %elem_ptr, align 2
  store i16 %elem, ptr %minVal, align 2
  store i64 1, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont, %entry
  %0 = load i64, ptr %i, align 4
  %1 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %0, %1
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr3 = load ptr, ptr %arr1, align 8
  %2 = load i64, ptr %i, align 4
  %elem_ptr4 = getelementptr i16, ptr %arr_ptr3, i64 %2
  %elem5 = load i16, ptr %elem_ptr4, align 2
  %3 = load i16, ptr %minVal, align 2
  %lttmp6 = icmp slt i16 %elem5, %3
  br i1 %lttmp6, label %then, label %ifcont

then:                                             ; preds = %while_body
  %arr_ptr7 = load ptr, ptr %arr1, align 8
  %4 = load i64, ptr %i, align 4
  %elem_ptr8 = getelementptr i16, ptr %arr_ptr7, i64 %4
  %elem9 = load i16, ptr %elem_ptr8, align 2
  store i16 %elem9, ptr %minVal, align 2
  br label %ifcont

ifcont:                                           ; preds = %then, %while_body
  %5 = load i64, ptr %i, align 4
  %addtmp = add i64 %5, 1
  store i64 %addtmp, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %6 = load i16, ptr %minVal, align 2
  %auto_wrap_result = alloca %result_uint16, align 8
  %err_ptr = getelementptr inbounds %result_uint16, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_uint16, ptr %auto_wrap_result, i32 0, i32 1
  store i16 %6, ptr %val_ptr, align 2
  %result_val = load %result_uint16, ptr %auto_wrap_result, align 2
  ret %result_uint16 %result_val
}

define internal %result_uint32 @min_uint32(ptr %arr, i64 %length) {
entry:
  %i = alloca i64, align 8
  %minVal = alloca i32, align 4
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %arr_ptr = load ptr, ptr %arr1, align 8
  %elem_ptr = getelementptr i32, ptr %arr_ptr, i64 0
  %elem = load i32, ptr %elem_ptr, align 4
  store i32 %elem, ptr %minVal, align 4
  store i64 1, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont, %entry
  %0 = load i64, ptr %i, align 4
  %1 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %0, %1
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr3 = load ptr, ptr %arr1, align 8
  %2 = load i64, ptr %i, align 4
  %elem_ptr4 = getelementptr i32, ptr %arr_ptr3, i64 %2
  %elem5 = load i32, ptr %elem_ptr4, align 4
  %3 = load i32, ptr %minVal, align 4
  %lttmp6 = icmp slt i32 %elem5, %3
  br i1 %lttmp6, label %then, label %ifcont

then:                                             ; preds = %while_body
  %arr_ptr7 = load ptr, ptr %arr1, align 8
  %4 = load i64, ptr %i, align 4
  %elem_ptr8 = getelementptr i32, ptr %arr_ptr7, i64 %4
  %elem9 = load i32, ptr %elem_ptr8, align 4
  store i32 %elem9, ptr %minVal, align 4
  br label %ifcont

ifcont:                                           ; preds = %then, %while_body
  %5 = load i64, ptr %i, align 4
  %addtmp = add i64 %5, 1
  store i64 %addtmp, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %6 = load i32, ptr %minVal, align 4
  %auto_wrap_result = alloca %result_uint32, align 8
  %err_ptr = getelementptr inbounds %result_uint32, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_uint32, ptr %auto_wrap_result, i32 0, i32 1
  store i32 %6, ptr %val_ptr, align 4
  %result_val = load %result_uint32, ptr %auto_wrap_result, align 4
  ret %result_uint32 %result_val
}

define internal %result_uint64 @min_uint64(ptr %arr, i64 %length) {
entry:
  %i = alloca i64, align 8
  %minVal = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %arr_ptr = load ptr, ptr %arr1, align 8
  %elem_ptr = getelementptr i64, ptr %arr_ptr, i64 0
  %elem = load i64, ptr %elem_ptr, align 4
  store i64 %elem, ptr %minVal, align 4
  store i64 1, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont, %entry
  %0 = load i64, ptr %i, align 4
  %1 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %0, %1
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr3 = load ptr, ptr %arr1, align 8
  %2 = load i64, ptr %i, align 4
  %elem_ptr4 = getelementptr i64, ptr %arr_ptr3, i64 %2
  %elem5 = load i64, ptr %elem_ptr4, align 4
  %3 = load i64, ptr %minVal, align 4
  %lttmp6 = icmp slt i64 %elem5, %3
  br i1 %lttmp6, label %then, label %ifcont

then:                                             ; preds = %while_body
  %arr_ptr7 = load ptr, ptr %arr1, align 8
  %4 = load i64, ptr %i, align 4
  %elem_ptr8 = getelementptr i64, ptr %arr_ptr7, i64 %4
  %elem9 = load i64, ptr %elem_ptr8, align 4
  store i64 %elem9, ptr %minVal, align 4
  br label %ifcont

ifcont:                                           ; preds = %then, %while_body
  %5 = load i64, ptr %i, align 4
  %addtmp = add i64 %5, 1
  store i64 %addtmp, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %6 = load i64, ptr %minVal, align 4
  %auto_wrap_result = alloca %result_uint64, align 8
  %err_ptr = getelementptr inbounds %result_uint64, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_uint64, ptr %auto_wrap_result, i32 0, i32 1
  store i64 %6, ptr %val_ptr, align 4
  %result_val = load %result_uint64, ptr %auto_wrap_result, align 4
  ret %result_uint64 %result_val
}

define internal %result_flt32 @min_flt32(ptr %arr, i64 %length) {
entry:
  %i = alloca i64, align 8
  %minVal = alloca float, align 4
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %arr_ptr = load ptr, ptr %arr1, align 8
  %elem_ptr = getelementptr float, ptr %arr_ptr, i64 0
  %elem = load float, ptr %elem_ptr, align 4
  store float %elem, ptr %minVal, align 4
  store i64 1, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont, %entry
  %0 = load i64, ptr %i, align 4
  %1 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %0, %1
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr3 = load ptr, ptr %arr1, align 8
  %2 = load i64, ptr %i, align 4
  %elem_ptr4 = getelementptr float, ptr %arr_ptr3, i64 %2
  %elem5 = load float, ptr %elem_ptr4, align 4
  %3 = load float, ptr %minVal, align 4
  %lttmp6 = fcmp olt float %elem5, %3
  br i1 %lttmp6, label %then, label %ifcont

then:                                             ; preds = %while_body
  %arr_ptr7 = load ptr, ptr %arr1, align 8
  %4 = load i64, ptr %i, align 4
  %elem_ptr8 = getelementptr float, ptr %arr_ptr7, i64 %4
  %elem9 = load float, ptr %elem_ptr8, align 4
  store float %elem9, ptr %minVal, align 4
  br label %ifcont

ifcont:                                           ; preds = %then, %while_body
  %5 = load i64, ptr %i, align 4
  %addtmp = add i64 %5, 1
  store i64 %addtmp, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %6 = load float, ptr %minVal, align 4
  %auto_wrap_result = alloca %result_flt32, align 8
  %err_ptr = getelementptr inbounds %result_flt32, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_flt32, ptr %auto_wrap_result, i32 0, i32 1
  store float %6, ptr %val_ptr, align 4
  %result_val = load %result_flt32, ptr %auto_wrap_result, align 4
  ret %result_flt32 %result_val
}

define internal %result_flt64 @min_flt64(ptr %arr, i64 %length) {
entry:
  %i = alloca i64, align 8
  %minVal = alloca double, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %arr_ptr = load ptr, ptr %arr1, align 8
  %elem_ptr = getelementptr double, ptr %arr_ptr, i64 0
  %elem = load double, ptr %elem_ptr, align 8
  store double %elem, ptr %minVal, align 8
  store i64 1, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont, %entry
  %0 = load i64, ptr %i, align 4
  %1 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %0, %1
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr3 = load ptr, ptr %arr1, align 8
  %2 = load i64, ptr %i, align 4
  %elem_ptr4 = getelementptr double, ptr %arr_ptr3, i64 %2
  %elem5 = load double, ptr %elem_ptr4, align 8
  %3 = load double, ptr %minVal, align 8
  %lttmp6 = fcmp olt double %elem5, %3
  br i1 %lttmp6, label %then, label %ifcont

then:                                             ; preds = %while_body
  %arr_ptr7 = load ptr, ptr %arr1, align 8
  %4 = load i64, ptr %i, align 4
  %elem_ptr8 = getelementptr double, ptr %arr_ptr7, i64 %4
  %elem9 = load double, ptr %elem_ptr8, align 8
  store double %elem9, ptr %minVal, align 8
  br label %ifcont

ifcont:                                           ; preds = %then, %while_body
  %5 = load i64, ptr %i, align 4
  %addtmp = add i64 %5, 1
  store i64 %addtmp, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %6 = load double, ptr %minVal, align 8
  %auto_wrap_result = alloca %result_flt64, align 8
  %err_ptr = getelementptr inbounds %result_flt64, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_flt64, ptr %auto_wrap_result, i32 0, i32 1
  store double %6, ptr %val_ptr, align 8
  %result_val = load %result_flt64, ptr %auto_wrap_result, align 8
  ret %result_flt64 %result_val
}

define internal %result_int8 @max_int8(ptr %arr, i64 %length) {
entry:
  %i = alloca i64, align 8
  %maxVal = alloca i8, align 1
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %arr_ptr = load ptr, ptr %arr1, align 8
  %elem_ptr = getelementptr i8, ptr %arr_ptr, i64 0
  %elem = load i8, ptr %elem_ptr, align 1
  store i8 %elem, ptr %maxVal, align 1
  store i64 1, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont, %entry
  %0 = load i64, ptr %i, align 4
  %1 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %0, %1
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr3 = load ptr, ptr %arr1, align 8
  %2 = load i64, ptr %i, align 4
  %elem_ptr4 = getelementptr i8, ptr %arr_ptr3, i64 %2
  %elem5 = load i8, ptr %elem_ptr4, align 1
  %3 = load i8, ptr %maxVal, align 1
  %gttmp = icmp sgt i8 %elem5, %3
  br i1 %gttmp, label %then, label %ifcont

then:                                             ; preds = %while_body
  %arr_ptr6 = load ptr, ptr %arr1, align 8
  %4 = load i64, ptr %i, align 4
  %elem_ptr7 = getelementptr i8, ptr %arr_ptr6, i64 %4
  %elem8 = load i8, ptr %elem_ptr7, align 1
  store i8 %elem8, ptr %maxVal, align 1
  br label %ifcont

ifcont:                                           ; preds = %then, %while_body
  %5 = load i64, ptr %i, align 4
  %addtmp = add i64 %5, 1
  store i64 %addtmp, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %6 = load i8, ptr %maxVal, align 1
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 %6, ptr %val_ptr, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val
}

define internal %result_int16 @max_int16(ptr %arr, i64 %length) {
entry:
  %i = alloca i64, align 8
  %maxVal = alloca i16, align 2
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %arr_ptr = load ptr, ptr %arr1, align 8
  %elem_ptr = getelementptr i16, ptr %arr_ptr, i64 0
  %elem = load i16, ptr %elem_ptr, align 2
  store i16 %elem, ptr %maxVal, align 2
  store i64 1, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont, %entry
  %0 = load i64, ptr %i, align 4
  %1 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %0, %1
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr3 = load ptr, ptr %arr1, align 8
  %2 = load i64, ptr %i, align 4
  %elem_ptr4 = getelementptr i16, ptr %arr_ptr3, i64 %2
  %elem5 = load i16, ptr %elem_ptr4, align 2
  %3 = load i16, ptr %maxVal, align 2
  %gttmp = icmp sgt i16 %elem5, %3
  br i1 %gttmp, label %then, label %ifcont

then:                                             ; preds = %while_body
  %arr_ptr6 = load ptr, ptr %arr1, align 8
  %4 = load i64, ptr %i, align 4
  %elem_ptr7 = getelementptr i16, ptr %arr_ptr6, i64 %4
  %elem8 = load i16, ptr %elem_ptr7, align 2
  store i16 %elem8, ptr %maxVal, align 2
  br label %ifcont

ifcont:                                           ; preds = %then, %while_body
  %5 = load i64, ptr %i, align 4
  %addtmp = add i64 %5, 1
  store i64 %addtmp, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %6 = load i16, ptr %maxVal, align 2
  %auto_wrap_result = alloca %result_int16, align 8
  %err_ptr = getelementptr inbounds %result_int16, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int16, ptr %auto_wrap_result, i32 0, i32 1
  store i16 %6, ptr %val_ptr, align 2
  %result_val = load %result_int16, ptr %auto_wrap_result, align 2
  ret %result_int16 %result_val
}

define internal %result_int32 @max_int32(ptr %arr, i64 %length) {
entry:
  %i = alloca i64, align 8
  %maxVal = alloca i32, align 4
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %arr_ptr = load ptr, ptr %arr1, align 8
  %elem_ptr = getelementptr i32, ptr %arr_ptr, i64 0
  %elem = load i32, ptr %elem_ptr, align 4
  store i32 %elem, ptr %maxVal, align 4
  store i64 1, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont, %entry
  %0 = load i64, ptr %i, align 4
  %1 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %0, %1
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr3 = load ptr, ptr %arr1, align 8
  %2 = load i64, ptr %i, align 4
  %elem_ptr4 = getelementptr i32, ptr %arr_ptr3, i64 %2
  %elem5 = load i32, ptr %elem_ptr4, align 4
  %3 = load i32, ptr %maxVal, align 4
  %gttmp = icmp sgt i32 %elem5, %3
  br i1 %gttmp, label %then, label %ifcont

then:                                             ; preds = %while_body
  %arr_ptr6 = load ptr, ptr %arr1, align 8
  %4 = load i64, ptr %i, align 4
  %elem_ptr7 = getelementptr i32, ptr %arr_ptr6, i64 %4
  %elem8 = load i32, ptr %elem_ptr7, align 4
  store i32 %elem8, ptr %maxVal, align 4
  br label %ifcont

ifcont:                                           ; preds = %then, %while_body
  %5 = load i64, ptr %i, align 4
  %addtmp = add i64 %5, 1
  store i64 %addtmp, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %6 = load i32, ptr %maxVal, align 4
  %auto_wrap_result = alloca %result_int32, align 8
  %err_ptr = getelementptr inbounds %result_int32, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int32, ptr %auto_wrap_result, i32 0, i32 1
  store i32 %6, ptr %val_ptr, align 4
  %result_val = load %result_int32, ptr %auto_wrap_result, align 4
  ret %result_int32 %result_val
}

define internal %result_int64 @max_int64(ptr %arr, i64 %length) {
entry:
  %i = alloca i64, align 8
  %maxVal = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %arr_ptr = load ptr, ptr %arr1, align 8
  %elem_ptr = getelementptr i64, ptr %arr_ptr, i64 0
  %elem = load i64, ptr %elem_ptr, align 4
  store i64 %elem, ptr %maxVal, align 4
  store i64 1, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont, %entry
  %0 = load i64, ptr %i, align 4
  %1 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %0, %1
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr3 = load ptr, ptr %arr1, align 8
  %2 = load i64, ptr %i, align 4
  %elem_ptr4 = getelementptr i64, ptr %arr_ptr3, i64 %2
  %elem5 = load i64, ptr %elem_ptr4, align 4
  %3 = load i64, ptr %maxVal, align 4
  %gttmp = icmp sgt i64 %elem5, %3
  br i1 %gttmp, label %then, label %ifcont

then:                                             ; preds = %while_body
  %arr_ptr6 = load ptr, ptr %arr1, align 8
  %4 = load i64, ptr %i, align 4
  %elem_ptr7 = getelementptr i64, ptr %arr_ptr6, i64 %4
  %elem8 = load i64, ptr %elem_ptr7, align 4
  store i64 %elem8, ptr %maxVal, align 4
  br label %ifcont

ifcont:                                           ; preds = %then, %while_body
  %5 = load i64, ptr %i, align 4
  %addtmp = add i64 %5, 1
  store i64 %addtmp, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %6 = load i64, ptr %maxVal, align 4
  %auto_wrap_result = alloca %result_int64, align 8
  %err_ptr = getelementptr inbounds %result_int64, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int64, ptr %auto_wrap_result, i32 0, i32 1
  store i64 %6, ptr %val_ptr, align 4
  %result_val = load %result_int64, ptr %auto_wrap_result, align 4
  ret %result_int64 %result_val
}

define internal %result_uint8 @max_uint8(ptr %arr, i64 %length) {
entry:
  %i = alloca i64, align 8
  %maxVal = alloca i8, align 1
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %arr_ptr = load ptr, ptr %arr1, align 8
  %elem_ptr = getelementptr i8, ptr %arr_ptr, i64 0
  %elem = load i8, ptr %elem_ptr, align 1
  store i8 %elem, ptr %maxVal, align 1
  store i64 1, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont, %entry
  %0 = load i64, ptr %i, align 4
  %1 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %0, %1
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr3 = load ptr, ptr %arr1, align 8
  %2 = load i64, ptr %i, align 4
  %elem_ptr4 = getelementptr i8, ptr %arr_ptr3, i64 %2
  %elem5 = load i8, ptr %elem_ptr4, align 1
  %3 = load i8, ptr %maxVal, align 1
  %gttmp = icmp sgt i8 %elem5, %3
  br i1 %gttmp, label %then, label %ifcont

then:                                             ; preds = %while_body
  %arr_ptr6 = load ptr, ptr %arr1, align 8
  %4 = load i64, ptr %i, align 4
  %elem_ptr7 = getelementptr i8, ptr %arr_ptr6, i64 %4
  %elem8 = load i8, ptr %elem_ptr7, align 1
  store i8 %elem8, ptr %maxVal, align 1
  br label %ifcont

ifcont:                                           ; preds = %then, %while_body
  %5 = load i64, ptr %i, align 4
  %addtmp = add i64 %5, 1
  store i64 %addtmp, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %6 = load i8, ptr %maxVal, align 1
  %auto_wrap_result = alloca %result_uint8, align 8
  %err_ptr = getelementptr inbounds %result_uint8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_uint8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 %6, ptr %val_ptr, align 1
  %result_val = load %result_uint8, ptr %auto_wrap_result, align 1
  ret %result_uint8 %result_val
}

define internal %result_uint16 @max_uint16(ptr %arr, i64 %length) {
entry:
  %i = alloca i64, align 8
  %maxVal = alloca i16, align 2
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %arr_ptr = load ptr, ptr %arr1, align 8
  %elem_ptr = getelementptr i16, ptr %arr_ptr, i64 0
  %elem = load i16, ptr %elem_ptr, align 2
  store i16 %elem, ptr %maxVal, align 2
  store i64 1, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont, %entry
  %0 = load i64, ptr %i, align 4
  %1 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %0, %1
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr3 = load ptr, ptr %arr1, align 8
  %2 = load i64, ptr %i, align 4
  %elem_ptr4 = getelementptr i16, ptr %arr_ptr3, i64 %2
  %elem5 = load i16, ptr %elem_ptr4, align 2
  %3 = load i16, ptr %maxVal, align 2
  %gttmp = icmp sgt i16 %elem5, %3
  br i1 %gttmp, label %then, label %ifcont

then:                                             ; preds = %while_body
  %arr_ptr6 = load ptr, ptr %arr1, align 8
  %4 = load i64, ptr %i, align 4
  %elem_ptr7 = getelementptr i16, ptr %arr_ptr6, i64 %4
  %elem8 = load i16, ptr %elem_ptr7, align 2
  store i16 %elem8, ptr %maxVal, align 2
  br label %ifcont

ifcont:                                           ; preds = %then, %while_body
  %5 = load i64, ptr %i, align 4
  %addtmp = add i64 %5, 1
  store i64 %addtmp, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %6 = load i16, ptr %maxVal, align 2
  %auto_wrap_result = alloca %result_uint16, align 8
  %err_ptr = getelementptr inbounds %result_uint16, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_uint16, ptr %auto_wrap_result, i32 0, i32 1
  store i16 %6, ptr %val_ptr, align 2
  %result_val = load %result_uint16, ptr %auto_wrap_result, align 2
  ret %result_uint16 %result_val
}

define internal %result_uint32 @max_uint32(ptr %arr, i64 %length) {
entry:
  %i = alloca i64, align 8
  %maxVal = alloca i32, align 4
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %arr_ptr = load ptr, ptr %arr1, align 8
  %elem_ptr = getelementptr i32, ptr %arr_ptr, i64 0
  %elem = load i32, ptr %elem_ptr, align 4
  store i32 %elem, ptr %maxVal, align 4
  store i64 1, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont, %entry
  %0 = load i64, ptr %i, align 4
  %1 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %0, %1
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr3 = load ptr, ptr %arr1, align 8
  %2 = load i64, ptr %i, align 4
  %elem_ptr4 = getelementptr i32, ptr %arr_ptr3, i64 %2
  %elem5 = load i32, ptr %elem_ptr4, align 4
  %3 = load i32, ptr %maxVal, align 4
  %gttmp = icmp sgt i32 %elem5, %3
  br i1 %gttmp, label %then, label %ifcont

then:                                             ; preds = %while_body
  %arr_ptr6 = load ptr, ptr %arr1, align 8
  %4 = load i64, ptr %i, align 4
  %elem_ptr7 = getelementptr i32, ptr %arr_ptr6, i64 %4
  %elem8 = load i32, ptr %elem_ptr7, align 4
  store i32 %elem8, ptr %maxVal, align 4
  br label %ifcont

ifcont:                                           ; preds = %then, %while_body
  %5 = load i64, ptr %i, align 4
  %addtmp = add i64 %5, 1
  store i64 %addtmp, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %6 = load i32, ptr %maxVal, align 4
  %auto_wrap_result = alloca %result_uint32, align 8
  %err_ptr = getelementptr inbounds %result_uint32, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_uint32, ptr %auto_wrap_result, i32 0, i32 1
  store i32 %6, ptr %val_ptr, align 4
  %result_val = load %result_uint32, ptr %auto_wrap_result, align 4
  ret %result_uint32 %result_val
}

define internal %result_uint64 @max_uint64(ptr %arr, i64 %length) {
entry:
  %i = alloca i64, align 8
  %maxVal = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %arr_ptr = load ptr, ptr %arr1, align 8
  %elem_ptr = getelementptr i64, ptr %arr_ptr, i64 0
  %elem = load i64, ptr %elem_ptr, align 4
  store i64 %elem, ptr %maxVal, align 4
  store i64 1, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont, %entry
  %0 = load i64, ptr %i, align 4
  %1 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %0, %1
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr3 = load ptr, ptr %arr1, align 8
  %2 = load i64, ptr %i, align 4
  %elem_ptr4 = getelementptr i64, ptr %arr_ptr3, i64 %2
  %elem5 = load i64, ptr %elem_ptr4, align 4
  %3 = load i64, ptr %maxVal, align 4
  %gttmp = icmp sgt i64 %elem5, %3
  br i1 %gttmp, label %then, label %ifcont

then:                                             ; preds = %while_body
  %arr_ptr6 = load ptr, ptr %arr1, align 8
  %4 = load i64, ptr %i, align 4
  %elem_ptr7 = getelementptr i64, ptr %arr_ptr6, i64 %4
  %elem8 = load i64, ptr %elem_ptr7, align 4
  store i64 %elem8, ptr %maxVal, align 4
  br label %ifcont

ifcont:                                           ; preds = %then, %while_body
  %5 = load i64, ptr %i, align 4
  %addtmp = add i64 %5, 1
  store i64 %addtmp, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %6 = load i64, ptr %maxVal, align 4
  %auto_wrap_result = alloca %result_uint64, align 8
  %err_ptr = getelementptr inbounds %result_uint64, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_uint64, ptr %auto_wrap_result, i32 0, i32 1
  store i64 %6, ptr %val_ptr, align 4
  %result_val = load %result_uint64, ptr %auto_wrap_result, align 4
  ret %result_uint64 %result_val
}

define internal %result_flt32 @max_flt32(ptr %arr, i64 %length) {
entry:
  %i = alloca i64, align 8
  %maxVal = alloca float, align 4
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %arr_ptr = load ptr, ptr %arr1, align 8
  %elem_ptr = getelementptr float, ptr %arr_ptr, i64 0
  %elem = load float, ptr %elem_ptr, align 4
  store float %elem, ptr %maxVal, align 4
  store i64 1, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont, %entry
  %0 = load i64, ptr %i, align 4
  %1 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %0, %1
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr3 = load ptr, ptr %arr1, align 8
  %2 = load i64, ptr %i, align 4
  %elem_ptr4 = getelementptr float, ptr %arr_ptr3, i64 %2
  %elem5 = load float, ptr %elem_ptr4, align 4
  %3 = load float, ptr %maxVal, align 4
  %gttmp = fcmp ogt float %elem5, %3
  br i1 %gttmp, label %then, label %ifcont

then:                                             ; preds = %while_body
  %arr_ptr6 = load ptr, ptr %arr1, align 8
  %4 = load i64, ptr %i, align 4
  %elem_ptr7 = getelementptr float, ptr %arr_ptr6, i64 %4
  %elem8 = load float, ptr %elem_ptr7, align 4
  store float %elem8, ptr %maxVal, align 4
  br label %ifcont

ifcont:                                           ; preds = %then, %while_body
  %5 = load i64, ptr %i, align 4
  %addtmp = add i64 %5, 1
  store i64 %addtmp, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %6 = load float, ptr %maxVal, align 4
  %auto_wrap_result = alloca %result_flt32, align 8
  %err_ptr = getelementptr inbounds %result_flt32, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_flt32, ptr %auto_wrap_result, i32 0, i32 1
  store float %6, ptr %val_ptr, align 4
  %result_val = load %result_flt32, ptr %auto_wrap_result, align 4
  ret %result_flt32 %result_val
}

define internal %result_flt64 @max_flt64(ptr %arr, i64 %length) {
entry:
  %i = alloca i64, align 8
  %maxVal = alloca double, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %arr_ptr = load ptr, ptr %arr1, align 8
  %elem_ptr = getelementptr double, ptr %arr_ptr, i64 0
  %elem = load double, ptr %elem_ptr, align 8
  store double %elem, ptr %maxVal, align 8
  store i64 1, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont, %entry
  %0 = load i64, ptr %i, align 4
  %1 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %0, %1
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr3 = load ptr, ptr %arr1, align 8
  %2 = load i64, ptr %i, align 4
  %elem_ptr4 = getelementptr double, ptr %arr_ptr3, i64 %2
  %elem5 = load double, ptr %elem_ptr4, align 8
  %3 = load double, ptr %maxVal, align 8
  %gttmp = fcmp ogt double %elem5, %3
  br i1 %gttmp, label %then, label %ifcont

then:                                             ; preds = %while_body
  %arr_ptr6 = load ptr, ptr %arr1, align 8
  %4 = load i64, ptr %i, align 4
  %elem_ptr7 = getelementptr double, ptr %arr_ptr6, i64 %4
  %elem8 = load double, ptr %elem_ptr7, align 8
  store double %elem8, ptr %maxVal, align 8
  br label %ifcont

ifcont:                                           ; preds = %then, %while_body
  %5 = load i64, ptr %i, align 4
  %addtmp = add i64 %5, 1
  store i64 %addtmp, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %6 = load double, ptr %maxVal, align 8
  %auto_wrap_result = alloca %result_flt64, align 8
  %err_ptr = getelementptr inbounds %result_flt64, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_flt64, ptr %auto_wrap_result, i32 0, i32 1
  store double %6, ptr %val_ptr, align 8
  %result_val = load %result_flt64, ptr %auto_wrap_result, align 8
  ret %result_flt64 %result_val
}

define internal %result_int8 @sum_int8(ptr %arr, i64 %length) {
entry:
  %i = alloca i64, align 8
  %total = alloca i8, align 1
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %arr_ptr = load ptr, ptr %arr1, align 8
  %elem_ptr = getelementptr i8, ptr %arr_ptr, i64 0
  %elem = load i8, ptr %elem_ptr, align 1
  store i8 %elem, ptr %total, align 1
  store i64 1, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %while_body, %entry
  %0 = load i64, ptr %i, align 4
  %1 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %0, %1
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %2 = load i8, ptr %total, align 1
  %arr_ptr3 = load ptr, ptr %arr1, align 8
  %3 = load i64, ptr %i, align 4
  %elem_ptr4 = getelementptr i8, ptr %arr_ptr3, i64 %3
  %elem5 = load i8, ptr %elem_ptr4, align 1
  %addtmp = add i8 %2, %elem5
  store i8 %addtmp, ptr %total, align 1
  %4 = load i64, ptr %i, align 4
  %addtmp6 = add i64 %4, 1
  store i64 %addtmp6, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %5 = load i8, ptr %total, align 1
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 %5, ptr %val_ptr, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val
}

define internal %result_int16 @sum_int16(ptr %arr, i64 %length) {
entry:
  %i = alloca i64, align 8
  %total = alloca i16, align 2
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %arr_ptr = load ptr, ptr %arr1, align 8
  %elem_ptr = getelementptr i16, ptr %arr_ptr, i64 0
  %elem = load i16, ptr %elem_ptr, align 2
  store i16 %elem, ptr %total, align 2
  store i64 1, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %while_body, %entry
  %0 = load i64, ptr %i, align 4
  %1 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %0, %1
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %2 = load i16, ptr %total, align 2
  %arr_ptr3 = load ptr, ptr %arr1, align 8
  %3 = load i64, ptr %i, align 4
  %elem_ptr4 = getelementptr i16, ptr %arr_ptr3, i64 %3
  %elem5 = load i16, ptr %elem_ptr4, align 2
  %addtmp = add i16 %2, %elem5
  store i16 %addtmp, ptr %total, align 2
  %4 = load i64, ptr %i, align 4
  %addtmp6 = add i64 %4, 1
  store i64 %addtmp6, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %5 = load i16, ptr %total, align 2
  %auto_wrap_result = alloca %result_int16, align 8
  %err_ptr = getelementptr inbounds %result_int16, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int16, ptr %auto_wrap_result, i32 0, i32 1
  store i16 %5, ptr %val_ptr, align 2
  %result_val = load %result_int16, ptr %auto_wrap_result, align 2
  ret %result_int16 %result_val
}

define internal %result_int32 @sum_int32(ptr %arr, i64 %length) {
entry:
  %i = alloca i64, align 8
  %total = alloca i32, align 4
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %arr_ptr = load ptr, ptr %arr1, align 8
  %elem_ptr = getelementptr i32, ptr %arr_ptr, i64 0
  %elem = load i32, ptr %elem_ptr, align 4
  store i32 %elem, ptr %total, align 4
  store i64 1, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %while_body, %entry
  %0 = load i64, ptr %i, align 4
  %1 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %0, %1
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %2 = load i32, ptr %total, align 4
  %arr_ptr3 = load ptr, ptr %arr1, align 8
  %3 = load i64, ptr %i, align 4
  %elem_ptr4 = getelementptr i32, ptr %arr_ptr3, i64 %3
  %elem5 = load i32, ptr %elem_ptr4, align 4
  %addtmp = add i32 %2, %elem5
  store i32 %addtmp, ptr %total, align 4
  %4 = load i64, ptr %i, align 4
  %addtmp6 = add i64 %4, 1
  store i64 %addtmp6, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %5 = load i32, ptr %total, align 4
  %auto_wrap_result = alloca %result_int32, align 8
  %err_ptr = getelementptr inbounds %result_int32, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int32, ptr %auto_wrap_result, i32 0, i32 1
  store i32 %5, ptr %val_ptr, align 4
  %result_val = load %result_int32, ptr %auto_wrap_result, align 4
  ret %result_int32 %result_val
}

define internal %result_int64 @sum_int64(ptr %arr, i64 %length) {
entry:
  %i = alloca i64, align 8
  %total = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %arr_ptr = load ptr, ptr %arr1, align 8
  %elem_ptr = getelementptr i64, ptr %arr_ptr, i64 0
  %elem = load i64, ptr %elem_ptr, align 4
  store i64 %elem, ptr %total, align 4
  store i64 1, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %while_body, %entry
  %0 = load i64, ptr %i, align 4
  %1 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %0, %1
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %2 = load i64, ptr %total, align 4
  %arr_ptr3 = load ptr, ptr %arr1, align 8
  %3 = load i64, ptr %i, align 4
  %elem_ptr4 = getelementptr i64, ptr %arr_ptr3, i64 %3
  %elem5 = load i64, ptr %elem_ptr4, align 4
  %addtmp = add i64 %2, %elem5
  store i64 %addtmp, ptr %total, align 4
  %4 = load i64, ptr %i, align 4
  %addtmp6 = add i64 %4, 1
  store i64 %addtmp6, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %5 = load i64, ptr %total, align 4
  %auto_wrap_result = alloca %result_int64, align 8
  %err_ptr = getelementptr inbounds %result_int64, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int64, ptr %auto_wrap_result, i32 0, i32 1
  store i64 %5, ptr %val_ptr, align 4
  %result_val = load %result_int64, ptr %auto_wrap_result, align 4
  ret %result_int64 %result_val
}

define internal %result_uint8 @sum_uint8(ptr %arr, i64 %length) {
entry:
  %i = alloca i64, align 8
  %total = alloca i8, align 1
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %arr_ptr = load ptr, ptr %arr1, align 8
  %elem_ptr = getelementptr i8, ptr %arr_ptr, i64 0
  %elem = load i8, ptr %elem_ptr, align 1
  store i8 %elem, ptr %total, align 1
  store i64 1, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %while_body, %entry
  %0 = load i64, ptr %i, align 4
  %1 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %0, %1
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %2 = load i8, ptr %total, align 1
  %arr_ptr3 = load ptr, ptr %arr1, align 8
  %3 = load i64, ptr %i, align 4
  %elem_ptr4 = getelementptr i8, ptr %arr_ptr3, i64 %3
  %elem5 = load i8, ptr %elem_ptr4, align 1
  %addtmp = add i8 %2, %elem5
  store i8 %addtmp, ptr %total, align 1
  %4 = load i64, ptr %i, align 4
  %addtmp6 = add i64 %4, 1
  store i64 %addtmp6, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %5 = load i8, ptr %total, align 1
  %auto_wrap_result = alloca %result_uint8, align 8
  %err_ptr = getelementptr inbounds %result_uint8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_uint8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 %5, ptr %val_ptr, align 1
  %result_val = load %result_uint8, ptr %auto_wrap_result, align 1
  ret %result_uint8 %result_val
}

define internal %result_uint16 @sum_uint16(ptr %arr, i64 %length) {
entry:
  %i = alloca i64, align 8
  %total = alloca i16, align 2
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %arr_ptr = load ptr, ptr %arr1, align 8
  %elem_ptr = getelementptr i16, ptr %arr_ptr, i64 0
  %elem = load i16, ptr %elem_ptr, align 2
  store i16 %elem, ptr %total, align 2
  store i64 1, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %while_body, %entry
  %0 = load i64, ptr %i, align 4
  %1 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %0, %1
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %2 = load i16, ptr %total, align 2
  %arr_ptr3 = load ptr, ptr %arr1, align 8
  %3 = load i64, ptr %i, align 4
  %elem_ptr4 = getelementptr i16, ptr %arr_ptr3, i64 %3
  %elem5 = load i16, ptr %elem_ptr4, align 2
  %addtmp = add i16 %2, %elem5
  store i16 %addtmp, ptr %total, align 2
  %4 = load i64, ptr %i, align 4
  %addtmp6 = add i64 %4, 1
  store i64 %addtmp6, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %5 = load i16, ptr %total, align 2
  %auto_wrap_result = alloca %result_uint16, align 8
  %err_ptr = getelementptr inbounds %result_uint16, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_uint16, ptr %auto_wrap_result, i32 0, i32 1
  store i16 %5, ptr %val_ptr, align 2
  %result_val = load %result_uint16, ptr %auto_wrap_result, align 2
  ret %result_uint16 %result_val
}

define internal %result_uint32 @sum_uint32(ptr %arr, i64 %length) {
entry:
  %i = alloca i64, align 8
  %total = alloca i32, align 4
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %arr_ptr = load ptr, ptr %arr1, align 8
  %elem_ptr = getelementptr i32, ptr %arr_ptr, i64 0
  %elem = load i32, ptr %elem_ptr, align 4
  store i32 %elem, ptr %total, align 4
  store i64 1, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %while_body, %entry
  %0 = load i64, ptr %i, align 4
  %1 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %0, %1
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %2 = load i32, ptr %total, align 4
  %arr_ptr3 = load ptr, ptr %arr1, align 8
  %3 = load i64, ptr %i, align 4
  %elem_ptr4 = getelementptr i32, ptr %arr_ptr3, i64 %3
  %elem5 = load i32, ptr %elem_ptr4, align 4
  %addtmp = add i32 %2, %elem5
  store i32 %addtmp, ptr %total, align 4
  %4 = load i64, ptr %i, align 4
  %addtmp6 = add i64 %4, 1
  store i64 %addtmp6, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %5 = load i32, ptr %total, align 4
  %auto_wrap_result = alloca %result_uint32, align 8
  %err_ptr = getelementptr inbounds %result_uint32, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_uint32, ptr %auto_wrap_result, i32 0, i32 1
  store i32 %5, ptr %val_ptr, align 4
  %result_val = load %result_uint32, ptr %auto_wrap_result, align 4
  ret %result_uint32 %result_val
}

define internal %result_uint64 @sum_uint64(ptr %arr, i64 %length) {
entry:
  %i = alloca i64, align 8
  %total = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %arr_ptr = load ptr, ptr %arr1, align 8
  %elem_ptr = getelementptr i64, ptr %arr_ptr, i64 0
  %elem = load i64, ptr %elem_ptr, align 4
  store i64 %elem, ptr %total, align 4
  store i64 1, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %while_body, %entry
  %0 = load i64, ptr %i, align 4
  %1 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %0, %1
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %2 = load i64, ptr %total, align 4
  %arr_ptr3 = load ptr, ptr %arr1, align 8
  %3 = load i64, ptr %i, align 4
  %elem_ptr4 = getelementptr i64, ptr %arr_ptr3, i64 %3
  %elem5 = load i64, ptr %elem_ptr4, align 4
  %addtmp = add i64 %2, %elem5
  store i64 %addtmp, ptr %total, align 4
  %4 = load i64, ptr %i, align 4
  %addtmp6 = add i64 %4, 1
  store i64 %addtmp6, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %5 = load i64, ptr %total, align 4
  %auto_wrap_result = alloca %result_uint64, align 8
  %err_ptr = getelementptr inbounds %result_uint64, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_uint64, ptr %auto_wrap_result, i32 0, i32 1
  store i64 %5, ptr %val_ptr, align 4
  %result_val = load %result_uint64, ptr %auto_wrap_result, align 4
  ret %result_uint64 %result_val
}

define internal %result_flt32 @sum_flt32(ptr %arr, i64 %length) {
entry:
  %i = alloca i64, align 8
  %total = alloca float, align 4
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %arr_ptr = load ptr, ptr %arr1, align 8
  %elem_ptr = getelementptr float, ptr %arr_ptr, i64 0
  %elem = load float, ptr %elem_ptr, align 4
  store float %elem, ptr %total, align 4
  store i64 1, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %while_body, %entry
  %0 = load i64, ptr %i, align 4
  %1 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %0, %1
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %2 = load float, ptr %total, align 4
  %arr_ptr3 = load ptr, ptr %arr1, align 8
  %3 = load i64, ptr %i, align 4
  %elem_ptr4 = getelementptr float, ptr %arr_ptr3, i64 %3
  %elem5 = load float, ptr %elem_ptr4, align 4
  %addtmp = fadd float %2, %elem5
  store float %addtmp, ptr %total, align 4
  %4 = load i64, ptr %i, align 4
  %addtmp6 = add i64 %4, 1
  store i64 %addtmp6, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %5 = load float, ptr %total, align 4
  %auto_wrap_result = alloca %result_flt32, align 8
  %err_ptr = getelementptr inbounds %result_flt32, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_flt32, ptr %auto_wrap_result, i32 0, i32 1
  store float %5, ptr %val_ptr, align 4
  %result_val = load %result_flt32, ptr %auto_wrap_result, align 4
  ret %result_flt32 %result_val
}

define internal %result_flt64 @sum_flt64(ptr %arr, i64 %length) {
entry:
  %i = alloca i64, align 8
  %total = alloca double, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %arr_ptr = load ptr, ptr %arr1, align 8
  %elem_ptr = getelementptr double, ptr %arr_ptr, i64 0
  %elem = load double, ptr %elem_ptr, align 8
  store double %elem, ptr %total, align 8
  store i64 1, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %while_body, %entry
  %0 = load i64, ptr %i, align 4
  %1 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %0, %1
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %2 = load double, ptr %total, align 8
  %arr_ptr3 = load ptr, ptr %arr1, align 8
  %3 = load i64, ptr %i, align 4
  %elem_ptr4 = getelementptr double, ptr %arr_ptr3, i64 %3
  %elem5 = load double, ptr %elem_ptr4, align 8
  %addtmp = fadd double %2, %elem5
  store double %addtmp, ptr %total, align 8
  %4 = load i64, ptr %i, align 4
  %addtmp6 = add i64 %4, 1
  store i64 %addtmp6, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %5 = load double, ptr %total, align 8
  %auto_wrap_result = alloca %result_flt64, align 8
  %err_ptr = getelementptr inbounds %result_flt64, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_flt64, ptr %auto_wrap_result, i32 0, i32 1
  store double %5, ptr %val_ptr, align 8
  %result_val = load %result_flt64, ptr %auto_wrap_result, align 8
  ret %result_flt64 %result_val
}

define internal %result_int8 @allEqual_int8(ptr %arr, i64 %length, i8 %value) {
entry:
  %i = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %value3 = alloca i8, align 1
  store i8 %value, ptr %value3, align 1
  store i64 0, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont, %entry
  %0 = load i64, ptr %i, align 4
  %1 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %0, %1
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr = load ptr, ptr %arr1, align 8
  %2 = load i64, ptr %i, align 4
  %elem_ptr = getelementptr i8, ptr %arr_ptr, i64 %2
  %elem = load i8, ptr %elem_ptr, align 1
  %3 = load i8, ptr %value3, align 1
  %netmp = icmp ne i8 %elem, %3
  br i1 %netmp, label %then, label %ifcont

then:                                             ; preds = %while_body
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 0, ptr %val_ptr, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val

ifcont:                                           ; preds = %while_body
  %4 = load i64, ptr %i, align 4
  %addtmp = add i64 %4, 1
  store i64 %addtmp, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %auto_wrap_result4 = alloca %result_int8, align 8
  %err_ptr5 = getelementptr inbounds %result_int8, ptr %auto_wrap_result4, i32 0, i32 0
  store i8 0, ptr %err_ptr5, align 1
  %val_ptr6 = getelementptr inbounds %result_int8, ptr %auto_wrap_result4, i32 0, i32 1
  store i8 1, ptr %val_ptr6, align 1
  %result_val7 = load %result_int8, ptr %auto_wrap_result4, align 1
  ret %result_int8 %result_val7
}

define internal %result_int8 @allEqual_int16(ptr %arr, i64 %length, i16 %value) {
entry:
  %i = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %value3 = alloca i16, align 2
  store i16 %value, ptr %value3, align 2
  store i64 0, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont, %entry
  %0 = load i64, ptr %i, align 4
  %1 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %0, %1
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr = load ptr, ptr %arr1, align 8
  %2 = load i64, ptr %i, align 4
  %elem_ptr = getelementptr i16, ptr %arr_ptr, i64 %2
  %elem = load i16, ptr %elem_ptr, align 2
  %3 = load i16, ptr %value3, align 2
  %netmp = icmp ne i16 %elem, %3
  br i1 %netmp, label %then, label %ifcont

then:                                             ; preds = %while_body
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 0, ptr %val_ptr, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val

ifcont:                                           ; preds = %while_body
  %4 = load i64, ptr %i, align 4
  %addtmp = add i64 %4, 1
  store i64 %addtmp, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %auto_wrap_result4 = alloca %result_int8, align 8
  %err_ptr5 = getelementptr inbounds %result_int8, ptr %auto_wrap_result4, i32 0, i32 0
  store i8 0, ptr %err_ptr5, align 1
  %val_ptr6 = getelementptr inbounds %result_int8, ptr %auto_wrap_result4, i32 0, i32 1
  store i8 1, ptr %val_ptr6, align 1
  %result_val7 = load %result_int8, ptr %auto_wrap_result4, align 1
  ret %result_int8 %result_val7
}

define internal %result_int8 @allEqual_int32(ptr %arr, i64 %length, i32 %value) {
entry:
  %i = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %value3 = alloca i32, align 4
  store i32 %value, ptr %value3, align 4
  store i64 0, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont, %entry
  %0 = load i64, ptr %i, align 4
  %1 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %0, %1
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr = load ptr, ptr %arr1, align 8
  %2 = load i64, ptr %i, align 4
  %elem_ptr = getelementptr i32, ptr %arr_ptr, i64 %2
  %elem = load i32, ptr %elem_ptr, align 4
  %3 = load i32, ptr %value3, align 4
  %netmp = icmp ne i32 %elem, %3
  br i1 %netmp, label %then, label %ifcont

then:                                             ; preds = %while_body
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 0, ptr %val_ptr, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val

ifcont:                                           ; preds = %while_body
  %4 = load i64, ptr %i, align 4
  %addtmp = add i64 %4, 1
  store i64 %addtmp, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %auto_wrap_result4 = alloca %result_int8, align 8
  %err_ptr5 = getelementptr inbounds %result_int8, ptr %auto_wrap_result4, i32 0, i32 0
  store i8 0, ptr %err_ptr5, align 1
  %val_ptr6 = getelementptr inbounds %result_int8, ptr %auto_wrap_result4, i32 0, i32 1
  store i8 1, ptr %val_ptr6, align 1
  %result_val7 = load %result_int8, ptr %auto_wrap_result4, align 1
  ret %result_int8 %result_val7
}

define internal %result_int8 @allEqual_int64(ptr %arr, i64 %length, i64 %value) {
entry:
  %i = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %value3 = alloca i64, align 8
  store i64 %value, ptr %value3, align 4
  store i64 0, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont, %entry
  %0 = load i64, ptr %i, align 4
  %1 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %0, %1
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr = load ptr, ptr %arr1, align 8
  %2 = load i64, ptr %i, align 4
  %elem_ptr = getelementptr i64, ptr %arr_ptr, i64 %2
  %elem = load i64, ptr %elem_ptr, align 4
  %3 = load i64, ptr %value3, align 4
  %netmp = icmp ne i64 %elem, %3
  br i1 %netmp, label %then, label %ifcont

then:                                             ; preds = %while_body
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 0, ptr %val_ptr, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val

ifcont:                                           ; preds = %while_body
  %4 = load i64, ptr %i, align 4
  %addtmp = add i64 %4, 1
  store i64 %addtmp, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %auto_wrap_result4 = alloca %result_int8, align 8
  %err_ptr5 = getelementptr inbounds %result_int8, ptr %auto_wrap_result4, i32 0, i32 0
  store i8 0, ptr %err_ptr5, align 1
  %val_ptr6 = getelementptr inbounds %result_int8, ptr %auto_wrap_result4, i32 0, i32 1
  store i8 1, ptr %val_ptr6, align 1
  %result_val7 = load %result_int8, ptr %auto_wrap_result4, align 1
  ret %result_int8 %result_val7
}

define internal %result_int8 @allEqual_uint8(ptr %arr, i64 %length, i8 %value) {
entry:
  %i = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %value3 = alloca i8, align 1
  store i8 %value, ptr %value3, align 1
  store i64 0, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont, %entry
  %0 = load i64, ptr %i, align 4
  %1 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %0, %1
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr = load ptr, ptr %arr1, align 8
  %2 = load i64, ptr %i, align 4
  %elem_ptr = getelementptr i8, ptr %arr_ptr, i64 %2
  %elem = load i8, ptr %elem_ptr, align 1
  %3 = load i8, ptr %value3, align 1
  %netmp = icmp ne i8 %elem, %3
  br i1 %netmp, label %then, label %ifcont

then:                                             ; preds = %while_body
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 0, ptr %val_ptr, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val

ifcont:                                           ; preds = %while_body
  %4 = load i64, ptr %i, align 4
  %addtmp = add i64 %4, 1
  store i64 %addtmp, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %auto_wrap_result4 = alloca %result_int8, align 8
  %err_ptr5 = getelementptr inbounds %result_int8, ptr %auto_wrap_result4, i32 0, i32 0
  store i8 0, ptr %err_ptr5, align 1
  %val_ptr6 = getelementptr inbounds %result_int8, ptr %auto_wrap_result4, i32 0, i32 1
  store i8 1, ptr %val_ptr6, align 1
  %result_val7 = load %result_int8, ptr %auto_wrap_result4, align 1
  ret %result_int8 %result_val7
}

define internal %result_int8 @allEqual_uint16(ptr %arr, i64 %length, i16 %value) {
entry:
  %i = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %value3 = alloca i16, align 2
  store i16 %value, ptr %value3, align 2
  store i64 0, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont, %entry
  %0 = load i64, ptr %i, align 4
  %1 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %0, %1
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr = load ptr, ptr %arr1, align 8
  %2 = load i64, ptr %i, align 4
  %elem_ptr = getelementptr i16, ptr %arr_ptr, i64 %2
  %elem = load i16, ptr %elem_ptr, align 2
  %3 = load i16, ptr %value3, align 2
  %netmp = icmp ne i16 %elem, %3
  br i1 %netmp, label %then, label %ifcont

then:                                             ; preds = %while_body
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 0, ptr %val_ptr, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val

ifcont:                                           ; preds = %while_body
  %4 = load i64, ptr %i, align 4
  %addtmp = add i64 %4, 1
  store i64 %addtmp, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %auto_wrap_result4 = alloca %result_int8, align 8
  %err_ptr5 = getelementptr inbounds %result_int8, ptr %auto_wrap_result4, i32 0, i32 0
  store i8 0, ptr %err_ptr5, align 1
  %val_ptr6 = getelementptr inbounds %result_int8, ptr %auto_wrap_result4, i32 0, i32 1
  store i8 1, ptr %val_ptr6, align 1
  %result_val7 = load %result_int8, ptr %auto_wrap_result4, align 1
  ret %result_int8 %result_val7
}

define internal %result_int8 @allEqual_uint32(ptr %arr, i64 %length, i32 %value) {
entry:
  %i = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %value3 = alloca i32, align 4
  store i32 %value, ptr %value3, align 4
  store i64 0, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont, %entry
  %0 = load i64, ptr %i, align 4
  %1 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %0, %1
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr = load ptr, ptr %arr1, align 8
  %2 = load i64, ptr %i, align 4
  %elem_ptr = getelementptr i32, ptr %arr_ptr, i64 %2
  %elem = load i32, ptr %elem_ptr, align 4
  %3 = load i32, ptr %value3, align 4
  %netmp = icmp ne i32 %elem, %3
  br i1 %netmp, label %then, label %ifcont

then:                                             ; preds = %while_body
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 0, ptr %val_ptr, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val

ifcont:                                           ; preds = %while_body
  %4 = load i64, ptr %i, align 4
  %addtmp = add i64 %4, 1
  store i64 %addtmp, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %auto_wrap_result4 = alloca %result_int8, align 8
  %err_ptr5 = getelementptr inbounds %result_int8, ptr %auto_wrap_result4, i32 0, i32 0
  store i8 0, ptr %err_ptr5, align 1
  %val_ptr6 = getelementptr inbounds %result_int8, ptr %auto_wrap_result4, i32 0, i32 1
  store i8 1, ptr %val_ptr6, align 1
  %result_val7 = load %result_int8, ptr %auto_wrap_result4, align 1
  ret %result_int8 %result_val7
}

define internal %result_int8 @allEqual_uint64(ptr %arr, i64 %length, i64 %value) {
entry:
  %i = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %value3 = alloca i64, align 8
  store i64 %value, ptr %value3, align 4
  store i64 0, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont, %entry
  %0 = load i64, ptr %i, align 4
  %1 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %0, %1
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr = load ptr, ptr %arr1, align 8
  %2 = load i64, ptr %i, align 4
  %elem_ptr = getelementptr i64, ptr %arr_ptr, i64 %2
  %elem = load i64, ptr %elem_ptr, align 4
  %3 = load i64, ptr %value3, align 4
  %netmp = icmp ne i64 %elem, %3
  br i1 %netmp, label %then, label %ifcont

then:                                             ; preds = %while_body
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 0, ptr %val_ptr, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val

ifcont:                                           ; preds = %while_body
  %4 = load i64, ptr %i, align 4
  %addtmp = add i64 %4, 1
  store i64 %addtmp, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %auto_wrap_result4 = alloca %result_int8, align 8
  %err_ptr5 = getelementptr inbounds %result_int8, ptr %auto_wrap_result4, i32 0, i32 0
  store i8 0, ptr %err_ptr5, align 1
  %val_ptr6 = getelementptr inbounds %result_int8, ptr %auto_wrap_result4, i32 0, i32 1
  store i8 1, ptr %val_ptr6, align 1
  %result_val7 = load %result_int8, ptr %auto_wrap_result4, align 1
  ret %result_int8 %result_val7
}

define internal %result_int8 @allEqual_flt32(ptr %arr, i64 %length, float %value) {
entry:
  %i = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %value3 = alloca float, align 4
  store float %value, ptr %value3, align 4
  store i64 0, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont, %entry
  %0 = load i64, ptr %i, align 4
  %1 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %0, %1
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr = load ptr, ptr %arr1, align 8
  %2 = load i64, ptr %i, align 4
  %elem_ptr = getelementptr float, ptr %arr_ptr, i64 %2
  %elem = load float, ptr %elem_ptr, align 4
  %3 = load float, ptr %value3, align 4
  %netmp = fcmp one float %elem, %3
  br i1 %netmp, label %then, label %ifcont

then:                                             ; preds = %while_body
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 0, ptr %val_ptr, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val

ifcont:                                           ; preds = %while_body
  %4 = load i64, ptr %i, align 4
  %addtmp = add i64 %4, 1
  store i64 %addtmp, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %auto_wrap_result4 = alloca %result_int8, align 8
  %err_ptr5 = getelementptr inbounds %result_int8, ptr %auto_wrap_result4, i32 0, i32 0
  store i8 0, ptr %err_ptr5, align 1
  %val_ptr6 = getelementptr inbounds %result_int8, ptr %auto_wrap_result4, i32 0, i32 1
  store i8 1, ptr %val_ptr6, align 1
  %result_val7 = load %result_int8, ptr %auto_wrap_result4, align 1
  ret %result_int8 %result_val7
}

define internal %result_int8 @allEqual_flt64(ptr %arr, i64 %length, double %value) {
entry:
  %i = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %value3 = alloca double, align 8
  store double %value, ptr %value3, align 8
  store i64 0, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont, %entry
  %0 = load i64, ptr %i, align 4
  %1 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %0, %1
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr = load ptr, ptr %arr1, align 8
  %2 = load i64, ptr %i, align 4
  %elem_ptr = getelementptr double, ptr %arr_ptr, i64 %2
  %elem = load double, ptr %elem_ptr, align 8
  %3 = load double, ptr %value3, align 8
  %netmp = fcmp one double %elem, %3
  br i1 %netmp, label %then, label %ifcont

then:                                             ; preds = %while_body
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 0, ptr %val_ptr, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val

ifcont:                                           ; preds = %while_body
  %4 = load i64, ptr %i, align 4
  %addtmp = add i64 %4, 1
  store i64 %addtmp, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %auto_wrap_result4 = alloca %result_int8, align 8
  %err_ptr5 = getelementptr inbounds %result_int8, ptr %auto_wrap_result4, i32 0, i32 0
  store i8 0, ptr %err_ptr5, align 1
  %val_ptr6 = getelementptr inbounds %result_int8, ptr %auto_wrap_result4, i32 0, i32 1
  store i8 1, ptr %val_ptr6, align 1
  %result_val7 = load %result_int8, ptr %auto_wrap_result4, align 1
  ret %result_int8 %result_val7
}

define internal %result_int8 @anyEqual_int8(ptr %arr, i64 %length, i8 %value) {
entry:
  %i = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %value3 = alloca i8, align 1
  store i8 %value, ptr %value3, align 1
  store i64 0, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont, %entry
  %0 = load i64, ptr %i, align 4
  %1 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %0, %1
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr = load ptr, ptr %arr1, align 8
  %2 = load i64, ptr %i, align 4
  %elem_ptr = getelementptr i8, ptr %arr_ptr, i64 %2
  %elem = load i8, ptr %elem_ptr, align 1
  %3 = load i8, ptr %value3, align 1
  %eqtmp = icmp eq i8 %elem, %3
  br i1 %eqtmp, label %then, label %ifcont

then:                                             ; preds = %while_body
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 1, ptr %val_ptr, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val

ifcont:                                           ; preds = %while_body
  %4 = load i64, ptr %i, align 4
  %addtmp = add i64 %4, 1
  store i64 %addtmp, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %auto_wrap_result4 = alloca %result_int8, align 8
  %err_ptr5 = getelementptr inbounds %result_int8, ptr %auto_wrap_result4, i32 0, i32 0
  store i8 0, ptr %err_ptr5, align 1
  %val_ptr6 = getelementptr inbounds %result_int8, ptr %auto_wrap_result4, i32 0, i32 1
  store i8 0, ptr %val_ptr6, align 1
  %result_val7 = load %result_int8, ptr %auto_wrap_result4, align 1
  ret %result_int8 %result_val7
}

define internal %result_int8 @anyEqual_int16(ptr %arr, i64 %length, i16 %value) {
entry:
  %i = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %value3 = alloca i16, align 2
  store i16 %value, ptr %value3, align 2
  store i64 0, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont, %entry
  %0 = load i64, ptr %i, align 4
  %1 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %0, %1
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr = load ptr, ptr %arr1, align 8
  %2 = load i64, ptr %i, align 4
  %elem_ptr = getelementptr i16, ptr %arr_ptr, i64 %2
  %elem = load i16, ptr %elem_ptr, align 2
  %3 = load i16, ptr %value3, align 2
  %eqtmp = icmp eq i16 %elem, %3
  br i1 %eqtmp, label %then, label %ifcont

then:                                             ; preds = %while_body
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 1, ptr %val_ptr, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val

ifcont:                                           ; preds = %while_body
  %4 = load i64, ptr %i, align 4
  %addtmp = add i64 %4, 1
  store i64 %addtmp, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %auto_wrap_result4 = alloca %result_int8, align 8
  %err_ptr5 = getelementptr inbounds %result_int8, ptr %auto_wrap_result4, i32 0, i32 0
  store i8 0, ptr %err_ptr5, align 1
  %val_ptr6 = getelementptr inbounds %result_int8, ptr %auto_wrap_result4, i32 0, i32 1
  store i8 0, ptr %val_ptr6, align 1
  %result_val7 = load %result_int8, ptr %auto_wrap_result4, align 1
  ret %result_int8 %result_val7
}

define internal %result_int8 @anyEqual_int32(ptr %arr, i64 %length, i32 %value) {
entry:
  %i = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %value3 = alloca i32, align 4
  store i32 %value, ptr %value3, align 4
  store i64 0, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont, %entry
  %0 = load i64, ptr %i, align 4
  %1 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %0, %1
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr = load ptr, ptr %arr1, align 8
  %2 = load i64, ptr %i, align 4
  %elem_ptr = getelementptr i32, ptr %arr_ptr, i64 %2
  %elem = load i32, ptr %elem_ptr, align 4
  %3 = load i32, ptr %value3, align 4
  %eqtmp = icmp eq i32 %elem, %3
  br i1 %eqtmp, label %then, label %ifcont

then:                                             ; preds = %while_body
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 1, ptr %val_ptr, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val

ifcont:                                           ; preds = %while_body
  %4 = load i64, ptr %i, align 4
  %addtmp = add i64 %4, 1
  store i64 %addtmp, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %auto_wrap_result4 = alloca %result_int8, align 8
  %err_ptr5 = getelementptr inbounds %result_int8, ptr %auto_wrap_result4, i32 0, i32 0
  store i8 0, ptr %err_ptr5, align 1
  %val_ptr6 = getelementptr inbounds %result_int8, ptr %auto_wrap_result4, i32 0, i32 1
  store i8 0, ptr %val_ptr6, align 1
  %result_val7 = load %result_int8, ptr %auto_wrap_result4, align 1
  ret %result_int8 %result_val7
}

define internal %result_int8 @anyEqual_int64(ptr %arr, i64 %length, i64 %value) {
entry:
  %i = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %value3 = alloca i64, align 8
  store i64 %value, ptr %value3, align 4
  store i64 0, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont, %entry
  %0 = load i64, ptr %i, align 4
  %1 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %0, %1
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr = load ptr, ptr %arr1, align 8
  %2 = load i64, ptr %i, align 4
  %elem_ptr = getelementptr i64, ptr %arr_ptr, i64 %2
  %elem = load i64, ptr %elem_ptr, align 4
  %3 = load i64, ptr %value3, align 4
  %eqtmp = icmp eq i64 %elem, %3
  br i1 %eqtmp, label %then, label %ifcont

then:                                             ; preds = %while_body
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 1, ptr %val_ptr, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val

ifcont:                                           ; preds = %while_body
  %4 = load i64, ptr %i, align 4
  %addtmp = add i64 %4, 1
  store i64 %addtmp, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %auto_wrap_result4 = alloca %result_int8, align 8
  %err_ptr5 = getelementptr inbounds %result_int8, ptr %auto_wrap_result4, i32 0, i32 0
  store i8 0, ptr %err_ptr5, align 1
  %val_ptr6 = getelementptr inbounds %result_int8, ptr %auto_wrap_result4, i32 0, i32 1
  store i8 0, ptr %val_ptr6, align 1
  %result_val7 = load %result_int8, ptr %auto_wrap_result4, align 1
  ret %result_int8 %result_val7
}

define internal %result_int8 @anyEqual_uint8(ptr %arr, i64 %length, i8 %value) {
entry:
  %i = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %value3 = alloca i8, align 1
  store i8 %value, ptr %value3, align 1
  store i64 0, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont, %entry
  %0 = load i64, ptr %i, align 4
  %1 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %0, %1
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr = load ptr, ptr %arr1, align 8
  %2 = load i64, ptr %i, align 4
  %elem_ptr = getelementptr i8, ptr %arr_ptr, i64 %2
  %elem = load i8, ptr %elem_ptr, align 1
  %3 = load i8, ptr %value3, align 1
  %eqtmp = icmp eq i8 %elem, %3
  br i1 %eqtmp, label %then, label %ifcont

then:                                             ; preds = %while_body
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 1, ptr %val_ptr, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val

ifcont:                                           ; preds = %while_body
  %4 = load i64, ptr %i, align 4
  %addtmp = add i64 %4, 1
  store i64 %addtmp, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %auto_wrap_result4 = alloca %result_int8, align 8
  %err_ptr5 = getelementptr inbounds %result_int8, ptr %auto_wrap_result4, i32 0, i32 0
  store i8 0, ptr %err_ptr5, align 1
  %val_ptr6 = getelementptr inbounds %result_int8, ptr %auto_wrap_result4, i32 0, i32 1
  store i8 0, ptr %val_ptr6, align 1
  %result_val7 = load %result_int8, ptr %auto_wrap_result4, align 1
  ret %result_int8 %result_val7
}

define internal %result_int8 @anyEqual_uint16(ptr %arr, i64 %length, i16 %value) {
entry:
  %i = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %value3 = alloca i16, align 2
  store i16 %value, ptr %value3, align 2
  store i64 0, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont, %entry
  %0 = load i64, ptr %i, align 4
  %1 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %0, %1
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr = load ptr, ptr %arr1, align 8
  %2 = load i64, ptr %i, align 4
  %elem_ptr = getelementptr i16, ptr %arr_ptr, i64 %2
  %elem = load i16, ptr %elem_ptr, align 2
  %3 = load i16, ptr %value3, align 2
  %eqtmp = icmp eq i16 %elem, %3
  br i1 %eqtmp, label %then, label %ifcont

then:                                             ; preds = %while_body
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 1, ptr %val_ptr, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val

ifcont:                                           ; preds = %while_body
  %4 = load i64, ptr %i, align 4
  %addtmp = add i64 %4, 1
  store i64 %addtmp, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %auto_wrap_result4 = alloca %result_int8, align 8
  %err_ptr5 = getelementptr inbounds %result_int8, ptr %auto_wrap_result4, i32 0, i32 0
  store i8 0, ptr %err_ptr5, align 1
  %val_ptr6 = getelementptr inbounds %result_int8, ptr %auto_wrap_result4, i32 0, i32 1
  store i8 0, ptr %val_ptr6, align 1
  %result_val7 = load %result_int8, ptr %auto_wrap_result4, align 1
  ret %result_int8 %result_val7
}

define internal %result_int8 @anyEqual_uint32(ptr %arr, i64 %length, i32 %value) {
entry:
  %i = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %value3 = alloca i32, align 4
  store i32 %value, ptr %value3, align 4
  store i64 0, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont, %entry
  %0 = load i64, ptr %i, align 4
  %1 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %0, %1
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr = load ptr, ptr %arr1, align 8
  %2 = load i64, ptr %i, align 4
  %elem_ptr = getelementptr i32, ptr %arr_ptr, i64 %2
  %elem = load i32, ptr %elem_ptr, align 4
  %3 = load i32, ptr %value3, align 4
  %eqtmp = icmp eq i32 %elem, %3
  br i1 %eqtmp, label %then, label %ifcont

then:                                             ; preds = %while_body
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 1, ptr %val_ptr, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val

ifcont:                                           ; preds = %while_body
  %4 = load i64, ptr %i, align 4
  %addtmp = add i64 %4, 1
  store i64 %addtmp, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %auto_wrap_result4 = alloca %result_int8, align 8
  %err_ptr5 = getelementptr inbounds %result_int8, ptr %auto_wrap_result4, i32 0, i32 0
  store i8 0, ptr %err_ptr5, align 1
  %val_ptr6 = getelementptr inbounds %result_int8, ptr %auto_wrap_result4, i32 0, i32 1
  store i8 0, ptr %val_ptr6, align 1
  %result_val7 = load %result_int8, ptr %auto_wrap_result4, align 1
  ret %result_int8 %result_val7
}

define internal %result_int8 @anyEqual_uint64(ptr %arr, i64 %length, i64 %value) {
entry:
  %i = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %value3 = alloca i64, align 8
  store i64 %value, ptr %value3, align 4
  store i64 0, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont, %entry
  %0 = load i64, ptr %i, align 4
  %1 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %0, %1
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr = load ptr, ptr %arr1, align 8
  %2 = load i64, ptr %i, align 4
  %elem_ptr = getelementptr i64, ptr %arr_ptr, i64 %2
  %elem = load i64, ptr %elem_ptr, align 4
  %3 = load i64, ptr %value3, align 4
  %eqtmp = icmp eq i64 %elem, %3
  br i1 %eqtmp, label %then, label %ifcont

then:                                             ; preds = %while_body
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 1, ptr %val_ptr, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val

ifcont:                                           ; preds = %while_body
  %4 = load i64, ptr %i, align 4
  %addtmp = add i64 %4, 1
  store i64 %addtmp, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %auto_wrap_result4 = alloca %result_int8, align 8
  %err_ptr5 = getelementptr inbounds %result_int8, ptr %auto_wrap_result4, i32 0, i32 0
  store i8 0, ptr %err_ptr5, align 1
  %val_ptr6 = getelementptr inbounds %result_int8, ptr %auto_wrap_result4, i32 0, i32 1
  store i8 0, ptr %val_ptr6, align 1
  %result_val7 = load %result_int8, ptr %auto_wrap_result4, align 1
  ret %result_int8 %result_val7
}

define internal %result_int8 @anyEqual_flt32(ptr %arr, i64 %length, float %value) {
entry:
  %i = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %value3 = alloca float, align 4
  store float %value, ptr %value3, align 4
  store i64 0, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont, %entry
  %0 = load i64, ptr %i, align 4
  %1 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %0, %1
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr = load ptr, ptr %arr1, align 8
  %2 = load i64, ptr %i, align 4
  %elem_ptr = getelementptr float, ptr %arr_ptr, i64 %2
  %elem = load float, ptr %elem_ptr, align 4
  %3 = load float, ptr %value3, align 4
  %eqtmp = fcmp oeq float %elem, %3
  br i1 %eqtmp, label %then, label %ifcont

then:                                             ; preds = %while_body
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 1, ptr %val_ptr, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val

ifcont:                                           ; preds = %while_body
  %4 = load i64, ptr %i, align 4
  %addtmp = add i64 %4, 1
  store i64 %addtmp, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %auto_wrap_result4 = alloca %result_int8, align 8
  %err_ptr5 = getelementptr inbounds %result_int8, ptr %auto_wrap_result4, i32 0, i32 0
  store i8 0, ptr %err_ptr5, align 1
  %val_ptr6 = getelementptr inbounds %result_int8, ptr %auto_wrap_result4, i32 0, i32 1
  store i8 0, ptr %val_ptr6, align 1
  %result_val7 = load %result_int8, ptr %auto_wrap_result4, align 1
  ret %result_int8 %result_val7
}

define internal %result_int8 @anyEqual_flt64(ptr %arr, i64 %length, double %value) {
entry:
  %i = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %value3 = alloca double, align 8
  store double %value, ptr %value3, align 8
  store i64 0, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont, %entry
  %0 = load i64, ptr %i, align 4
  %1 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %0, %1
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr = load ptr, ptr %arr1, align 8
  %2 = load i64, ptr %i, align 4
  %elem_ptr = getelementptr double, ptr %arr_ptr, i64 %2
  %elem = load double, ptr %elem_ptr, align 8
  %3 = load double, ptr %value3, align 8
  %eqtmp = fcmp oeq double %elem, %3
  br i1 %eqtmp, label %then, label %ifcont

then:                                             ; preds = %while_body
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 1, ptr %val_ptr, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val

ifcont:                                           ; preds = %while_body
  %4 = load i64, ptr %i, align 4
  %addtmp = add i64 %4, 1
  store i64 %addtmp, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %auto_wrap_result4 = alloca %result_int8, align 8
  %err_ptr5 = getelementptr inbounds %result_int8, ptr %auto_wrap_result4, i32 0, i32 0
  store i8 0, ptr %err_ptr5, align 1
  %val_ptr6 = getelementptr inbounds %result_int8, ptr %auto_wrap_result4, i32 0, i32 1
  store i8 0, ptr %val_ptr6, align 1
  %result_val7 = load %result_int8, ptr %auto_wrap_result4, align 1
  ret %result_int8 %result_val7
}

define internal %result_int64 @countIfGt_int8(ptr %arr, i64 %length, i8 %threshold) {
entry:
  %i = alloca i64, align 8
  %count = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %threshold3 = alloca i8, align 1
  store i8 %threshold, ptr %threshold3, align 1
  store i64 0, ptr %count, align 4
  store i64 0, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont, %entry
  %0 = load i64, ptr %i, align 4
  %1 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %0, %1
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr = load ptr, ptr %arr1, align 8
  %2 = load i64, ptr %i, align 4
  %elem_ptr = getelementptr i8, ptr %arr_ptr, i64 %2
  %elem = load i8, ptr %elem_ptr, align 1
  %3 = load i8, ptr %threshold3, align 1
  %gttmp = icmp sgt i8 %elem, %3
  br i1 %gttmp, label %then, label %ifcont

then:                                             ; preds = %while_body
  %4 = load i64, ptr %count, align 4
  %addtmp = add i64 %4, 1
  store i64 %addtmp, ptr %count, align 4
  br label %ifcont

ifcont:                                           ; preds = %then, %while_body
  %5 = load i64, ptr %i, align 4
  %addtmp4 = add i64 %5, 1
  store i64 %addtmp4, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %6 = load i64, ptr %count, align 4
  %auto_wrap_result = alloca %result_int64, align 8
  %err_ptr = getelementptr inbounds %result_int64, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int64, ptr %auto_wrap_result, i32 0, i32 1
  store i64 %6, ptr %val_ptr, align 4
  %result_val = load %result_int64, ptr %auto_wrap_result, align 4
  ret %result_int64 %result_val
}

define internal %result_int64 @countIfGt_int16(ptr %arr, i64 %length, i16 %threshold) {
entry:
  %i = alloca i64, align 8
  %count = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %threshold3 = alloca i16, align 2
  store i16 %threshold, ptr %threshold3, align 2
  store i64 0, ptr %count, align 4
  store i64 0, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont, %entry
  %0 = load i64, ptr %i, align 4
  %1 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %0, %1
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr = load ptr, ptr %arr1, align 8
  %2 = load i64, ptr %i, align 4
  %elem_ptr = getelementptr i16, ptr %arr_ptr, i64 %2
  %elem = load i16, ptr %elem_ptr, align 2
  %3 = load i16, ptr %threshold3, align 2
  %gttmp = icmp sgt i16 %elem, %3
  br i1 %gttmp, label %then, label %ifcont

then:                                             ; preds = %while_body
  %4 = load i64, ptr %count, align 4
  %addtmp = add i64 %4, 1
  store i64 %addtmp, ptr %count, align 4
  br label %ifcont

ifcont:                                           ; preds = %then, %while_body
  %5 = load i64, ptr %i, align 4
  %addtmp4 = add i64 %5, 1
  store i64 %addtmp4, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %6 = load i64, ptr %count, align 4
  %auto_wrap_result = alloca %result_int64, align 8
  %err_ptr = getelementptr inbounds %result_int64, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int64, ptr %auto_wrap_result, i32 0, i32 1
  store i64 %6, ptr %val_ptr, align 4
  %result_val = load %result_int64, ptr %auto_wrap_result, align 4
  ret %result_int64 %result_val
}

define internal %result_int64 @countIfGt_int32(ptr %arr, i64 %length, i32 %threshold) {
entry:
  %i = alloca i64, align 8
  %count = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %threshold3 = alloca i32, align 4
  store i32 %threshold, ptr %threshold3, align 4
  store i64 0, ptr %count, align 4
  store i64 0, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont, %entry
  %0 = load i64, ptr %i, align 4
  %1 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %0, %1
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr = load ptr, ptr %arr1, align 8
  %2 = load i64, ptr %i, align 4
  %elem_ptr = getelementptr i32, ptr %arr_ptr, i64 %2
  %elem = load i32, ptr %elem_ptr, align 4
  %3 = load i32, ptr %threshold3, align 4
  %gttmp = icmp sgt i32 %elem, %3
  br i1 %gttmp, label %then, label %ifcont

then:                                             ; preds = %while_body
  %4 = load i64, ptr %count, align 4
  %addtmp = add i64 %4, 1
  store i64 %addtmp, ptr %count, align 4
  br label %ifcont

ifcont:                                           ; preds = %then, %while_body
  %5 = load i64, ptr %i, align 4
  %addtmp4 = add i64 %5, 1
  store i64 %addtmp4, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %6 = load i64, ptr %count, align 4
  %auto_wrap_result = alloca %result_int64, align 8
  %err_ptr = getelementptr inbounds %result_int64, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int64, ptr %auto_wrap_result, i32 0, i32 1
  store i64 %6, ptr %val_ptr, align 4
  %result_val = load %result_int64, ptr %auto_wrap_result, align 4
  ret %result_int64 %result_val
}

define internal %result_int64 @countIfGt_int64(ptr %arr, i64 %length, i64 %threshold) {
entry:
  %i = alloca i64, align 8
  %count = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %threshold3 = alloca i64, align 8
  store i64 %threshold, ptr %threshold3, align 4
  store i64 0, ptr %count, align 4
  store i64 0, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont, %entry
  %0 = load i64, ptr %i, align 4
  %1 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %0, %1
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr = load ptr, ptr %arr1, align 8
  %2 = load i64, ptr %i, align 4
  %elem_ptr = getelementptr i64, ptr %arr_ptr, i64 %2
  %elem = load i64, ptr %elem_ptr, align 4
  %3 = load i64, ptr %threshold3, align 4
  %gttmp = icmp sgt i64 %elem, %3
  br i1 %gttmp, label %then, label %ifcont

then:                                             ; preds = %while_body
  %4 = load i64, ptr %count, align 4
  %addtmp = add i64 %4, 1
  store i64 %addtmp, ptr %count, align 4
  br label %ifcont

ifcont:                                           ; preds = %then, %while_body
  %5 = load i64, ptr %i, align 4
  %addtmp4 = add i64 %5, 1
  store i64 %addtmp4, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %6 = load i64, ptr %count, align 4
  %auto_wrap_result = alloca %result_int64, align 8
  %err_ptr = getelementptr inbounds %result_int64, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int64, ptr %auto_wrap_result, i32 0, i32 1
  store i64 %6, ptr %val_ptr, align 4
  %result_val = load %result_int64, ptr %auto_wrap_result, align 4
  ret %result_int64 %result_val
}

define internal %result_int64 @countIfGt_uint8(ptr %arr, i64 %length, i8 %threshold) {
entry:
  %i = alloca i64, align 8
  %count = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %threshold3 = alloca i8, align 1
  store i8 %threshold, ptr %threshold3, align 1
  store i64 0, ptr %count, align 4
  store i64 0, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont, %entry
  %0 = load i64, ptr %i, align 4
  %1 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %0, %1
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr = load ptr, ptr %arr1, align 8
  %2 = load i64, ptr %i, align 4
  %elem_ptr = getelementptr i8, ptr %arr_ptr, i64 %2
  %elem = load i8, ptr %elem_ptr, align 1
  %3 = load i8, ptr %threshold3, align 1
  %gttmp = icmp sgt i8 %elem, %3
  br i1 %gttmp, label %then, label %ifcont

then:                                             ; preds = %while_body
  %4 = load i64, ptr %count, align 4
  %addtmp = add i64 %4, 1
  store i64 %addtmp, ptr %count, align 4
  br label %ifcont

ifcont:                                           ; preds = %then, %while_body
  %5 = load i64, ptr %i, align 4
  %addtmp4 = add i64 %5, 1
  store i64 %addtmp4, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %6 = load i64, ptr %count, align 4
  %auto_wrap_result = alloca %result_int64, align 8
  %err_ptr = getelementptr inbounds %result_int64, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int64, ptr %auto_wrap_result, i32 0, i32 1
  store i64 %6, ptr %val_ptr, align 4
  %result_val = load %result_int64, ptr %auto_wrap_result, align 4
  ret %result_int64 %result_val
}

define internal %result_int64 @countIfGt_uint16(ptr %arr, i64 %length, i16 %threshold) {
entry:
  %i = alloca i64, align 8
  %count = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %threshold3 = alloca i16, align 2
  store i16 %threshold, ptr %threshold3, align 2
  store i64 0, ptr %count, align 4
  store i64 0, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont, %entry
  %0 = load i64, ptr %i, align 4
  %1 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %0, %1
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr = load ptr, ptr %arr1, align 8
  %2 = load i64, ptr %i, align 4
  %elem_ptr = getelementptr i16, ptr %arr_ptr, i64 %2
  %elem = load i16, ptr %elem_ptr, align 2
  %3 = load i16, ptr %threshold3, align 2
  %gttmp = icmp sgt i16 %elem, %3
  br i1 %gttmp, label %then, label %ifcont

then:                                             ; preds = %while_body
  %4 = load i64, ptr %count, align 4
  %addtmp = add i64 %4, 1
  store i64 %addtmp, ptr %count, align 4
  br label %ifcont

ifcont:                                           ; preds = %then, %while_body
  %5 = load i64, ptr %i, align 4
  %addtmp4 = add i64 %5, 1
  store i64 %addtmp4, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %6 = load i64, ptr %count, align 4
  %auto_wrap_result = alloca %result_int64, align 8
  %err_ptr = getelementptr inbounds %result_int64, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int64, ptr %auto_wrap_result, i32 0, i32 1
  store i64 %6, ptr %val_ptr, align 4
  %result_val = load %result_int64, ptr %auto_wrap_result, align 4
  ret %result_int64 %result_val
}

define internal %result_int64 @countIfGt_uint32(ptr %arr, i64 %length, i32 %threshold) {
entry:
  %i = alloca i64, align 8
  %count = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %threshold3 = alloca i32, align 4
  store i32 %threshold, ptr %threshold3, align 4
  store i64 0, ptr %count, align 4
  store i64 0, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont, %entry
  %0 = load i64, ptr %i, align 4
  %1 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %0, %1
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr = load ptr, ptr %arr1, align 8
  %2 = load i64, ptr %i, align 4
  %elem_ptr = getelementptr i32, ptr %arr_ptr, i64 %2
  %elem = load i32, ptr %elem_ptr, align 4
  %3 = load i32, ptr %threshold3, align 4
  %gttmp = icmp sgt i32 %elem, %3
  br i1 %gttmp, label %then, label %ifcont

then:                                             ; preds = %while_body
  %4 = load i64, ptr %count, align 4
  %addtmp = add i64 %4, 1
  store i64 %addtmp, ptr %count, align 4
  br label %ifcont

ifcont:                                           ; preds = %then, %while_body
  %5 = load i64, ptr %i, align 4
  %addtmp4 = add i64 %5, 1
  store i64 %addtmp4, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %6 = load i64, ptr %count, align 4
  %auto_wrap_result = alloca %result_int64, align 8
  %err_ptr = getelementptr inbounds %result_int64, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int64, ptr %auto_wrap_result, i32 0, i32 1
  store i64 %6, ptr %val_ptr, align 4
  %result_val = load %result_int64, ptr %auto_wrap_result, align 4
  ret %result_int64 %result_val
}

define internal %result_int64 @countIfGt_uint64(ptr %arr, i64 %length, i64 %threshold) {
entry:
  %i = alloca i64, align 8
  %count = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %threshold3 = alloca i64, align 8
  store i64 %threshold, ptr %threshold3, align 4
  store i64 0, ptr %count, align 4
  store i64 0, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont, %entry
  %0 = load i64, ptr %i, align 4
  %1 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %0, %1
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr = load ptr, ptr %arr1, align 8
  %2 = load i64, ptr %i, align 4
  %elem_ptr = getelementptr i64, ptr %arr_ptr, i64 %2
  %elem = load i64, ptr %elem_ptr, align 4
  %3 = load i64, ptr %threshold3, align 4
  %gttmp = icmp sgt i64 %elem, %3
  br i1 %gttmp, label %then, label %ifcont

then:                                             ; preds = %while_body
  %4 = load i64, ptr %count, align 4
  %addtmp = add i64 %4, 1
  store i64 %addtmp, ptr %count, align 4
  br label %ifcont

ifcont:                                           ; preds = %then, %while_body
  %5 = load i64, ptr %i, align 4
  %addtmp4 = add i64 %5, 1
  store i64 %addtmp4, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %6 = load i64, ptr %count, align 4
  %auto_wrap_result = alloca %result_int64, align 8
  %err_ptr = getelementptr inbounds %result_int64, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int64, ptr %auto_wrap_result, i32 0, i32 1
  store i64 %6, ptr %val_ptr, align 4
  %result_val = load %result_int64, ptr %auto_wrap_result, align 4
  ret %result_int64 %result_val
}

define internal %result_int64 @countIfGt_flt32(ptr %arr, i64 %length, float %threshold) {
entry:
  %i = alloca i64, align 8
  %count = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %threshold3 = alloca float, align 4
  store float %threshold, ptr %threshold3, align 4
  store i64 0, ptr %count, align 4
  store i64 0, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont, %entry
  %0 = load i64, ptr %i, align 4
  %1 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %0, %1
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr = load ptr, ptr %arr1, align 8
  %2 = load i64, ptr %i, align 4
  %elem_ptr = getelementptr float, ptr %arr_ptr, i64 %2
  %elem = load float, ptr %elem_ptr, align 4
  %3 = load float, ptr %threshold3, align 4
  %gttmp = fcmp ogt float %elem, %3
  br i1 %gttmp, label %then, label %ifcont

then:                                             ; preds = %while_body
  %4 = load i64, ptr %count, align 4
  %addtmp = add i64 %4, 1
  store i64 %addtmp, ptr %count, align 4
  br label %ifcont

ifcont:                                           ; preds = %then, %while_body
  %5 = load i64, ptr %i, align 4
  %addtmp4 = add i64 %5, 1
  store i64 %addtmp4, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %6 = load i64, ptr %count, align 4
  %auto_wrap_result = alloca %result_int64, align 8
  %err_ptr = getelementptr inbounds %result_int64, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int64, ptr %auto_wrap_result, i32 0, i32 1
  store i64 %6, ptr %val_ptr, align 4
  %result_val = load %result_int64, ptr %auto_wrap_result, align 4
  ret %result_int64 %result_val
}

define internal %result_int64 @countIfGt_flt64(ptr %arr, i64 %length, double %threshold) {
entry:
  %i = alloca i64, align 8
  %count = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %threshold3 = alloca double, align 8
  store double %threshold, ptr %threshold3, align 8
  store i64 0, ptr %count, align 4
  store i64 0, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont, %entry
  %0 = load i64, ptr %i, align 4
  %1 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %0, %1
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr = load ptr, ptr %arr1, align 8
  %2 = load i64, ptr %i, align 4
  %elem_ptr = getelementptr double, ptr %arr_ptr, i64 %2
  %elem = load double, ptr %elem_ptr, align 8
  %3 = load double, ptr %threshold3, align 8
  %gttmp = fcmp ogt double %elem, %3
  br i1 %gttmp, label %then, label %ifcont

then:                                             ; preds = %while_body
  %4 = load i64, ptr %count, align 4
  %addtmp = add i64 %4, 1
  store i64 %addtmp, ptr %count, align 4
  br label %ifcont

ifcont:                                           ; preds = %then, %while_body
  %5 = load i64, ptr %i, align 4
  %addtmp4 = add i64 %5, 1
  store i64 %addtmp4, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %6 = load i64, ptr %count, align 4
  %auto_wrap_result = alloca %result_int64, align 8
  %err_ptr = getelementptr inbounds %result_int64, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int64, ptr %auto_wrap_result, i32 0, i32 1
  store i64 %6, ptr %val_ptr, align 4
  %result_val = load %result_int64, ptr %auto_wrap_result, align 4
  ret %result_int64 %result_val
}

define internal %result_int8 @swap_int8(ptr %arr, i64 %i, i64 %j) {
entry:
  %tmp = alloca i8, align 1
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %i2 = alloca i64, align 8
  store i64 %i, ptr %i2, align 4
  %j3 = alloca i64, align 8
  store i64 %j, ptr %j3, align 4
  %arr_ptr = load ptr, ptr %arr1, align 8
  %0 = load i64, ptr %i2, align 4
  %elem_ptr = getelementptr i8, ptr %arr_ptr, i64 %0
  %elem = load i8, ptr %elem_ptr, align 1
  store i8 %elem, ptr %tmp, align 1
  %arr_ptr4 = load ptr, ptr %arr1, align 8
  %1 = load i64, ptr %i2, align 4
  %arr_ptr5 = load ptr, ptr %arr1, align 8
  %2 = load i64, ptr %j3, align 4
  %elem_ptr6 = getelementptr i8, ptr %arr_ptr5, i64 %2
  %elem7 = load i8, ptr %elem_ptr6, align 1
  %elem_ptr8 = getelementptr i8, ptr %arr_ptr4, i64 %1
  store i8 %elem7, ptr %elem_ptr8, align 1
  %arr_ptr9 = load ptr, ptr %arr1, align 8
  %3 = load i64, ptr %j3, align 4
  %4 = load i8, ptr %tmp, align 1
  %elem_ptr10 = getelementptr i8, ptr %arr_ptr9, i64 %3
  store i8 %4, ptr %elem_ptr10, align 1
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 0, ptr %val_ptr, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val
}

define internal %result_int8 @swap_int16(ptr %arr, i64 %i, i64 %j) {
entry:
  %tmp = alloca i16, align 2
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %i2 = alloca i64, align 8
  store i64 %i, ptr %i2, align 4
  %j3 = alloca i64, align 8
  store i64 %j, ptr %j3, align 4
  %arr_ptr = load ptr, ptr %arr1, align 8
  %0 = load i64, ptr %i2, align 4
  %elem_ptr = getelementptr i16, ptr %arr_ptr, i64 %0
  %elem = load i16, ptr %elem_ptr, align 2
  store i16 %elem, ptr %tmp, align 2
  %arr_ptr4 = load ptr, ptr %arr1, align 8
  %1 = load i64, ptr %i2, align 4
  %arr_ptr5 = load ptr, ptr %arr1, align 8
  %2 = load i64, ptr %j3, align 4
  %elem_ptr6 = getelementptr i16, ptr %arr_ptr5, i64 %2
  %elem7 = load i16, ptr %elem_ptr6, align 2
  %elem_ptr8 = getelementptr i16, ptr %arr_ptr4, i64 %1
  store i16 %elem7, ptr %elem_ptr8, align 2
  %arr_ptr9 = load ptr, ptr %arr1, align 8
  %3 = load i64, ptr %j3, align 4
  %4 = load i16, ptr %tmp, align 2
  %elem_ptr10 = getelementptr i16, ptr %arr_ptr9, i64 %3
  store i16 %4, ptr %elem_ptr10, align 2
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 0, ptr %val_ptr, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val
}

define internal %result_int8 @swap_int32(ptr %arr, i64 %i, i64 %j) {
entry:
  %tmp = alloca i32, align 4
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %i2 = alloca i64, align 8
  store i64 %i, ptr %i2, align 4
  %j3 = alloca i64, align 8
  store i64 %j, ptr %j3, align 4
  %arr_ptr = load ptr, ptr %arr1, align 8
  %0 = load i64, ptr %i2, align 4
  %elem_ptr = getelementptr i32, ptr %arr_ptr, i64 %0
  %elem = load i32, ptr %elem_ptr, align 4
  store i32 %elem, ptr %tmp, align 4
  %arr_ptr4 = load ptr, ptr %arr1, align 8
  %1 = load i64, ptr %i2, align 4
  %arr_ptr5 = load ptr, ptr %arr1, align 8
  %2 = load i64, ptr %j3, align 4
  %elem_ptr6 = getelementptr i32, ptr %arr_ptr5, i64 %2
  %elem7 = load i32, ptr %elem_ptr6, align 4
  %elem_ptr8 = getelementptr i32, ptr %arr_ptr4, i64 %1
  store i32 %elem7, ptr %elem_ptr8, align 4
  %arr_ptr9 = load ptr, ptr %arr1, align 8
  %3 = load i64, ptr %j3, align 4
  %4 = load i32, ptr %tmp, align 4
  %elem_ptr10 = getelementptr i32, ptr %arr_ptr9, i64 %3
  store i32 %4, ptr %elem_ptr10, align 4
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 0, ptr %val_ptr, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val
}

define internal %result_int8 @swap_int64(ptr %arr, i64 %i, i64 %j) {
entry:
  %tmp = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %i2 = alloca i64, align 8
  store i64 %i, ptr %i2, align 4
  %j3 = alloca i64, align 8
  store i64 %j, ptr %j3, align 4
  %arr_ptr = load ptr, ptr %arr1, align 8
  %0 = load i64, ptr %i2, align 4
  %elem_ptr = getelementptr i64, ptr %arr_ptr, i64 %0
  %elem = load i64, ptr %elem_ptr, align 4
  store i64 %elem, ptr %tmp, align 4
  %arr_ptr4 = load ptr, ptr %arr1, align 8
  %1 = load i64, ptr %i2, align 4
  %arr_ptr5 = load ptr, ptr %arr1, align 8
  %2 = load i64, ptr %j3, align 4
  %elem_ptr6 = getelementptr i64, ptr %arr_ptr5, i64 %2
  %elem7 = load i64, ptr %elem_ptr6, align 4
  %elem_ptr8 = getelementptr i64, ptr %arr_ptr4, i64 %1
  store i64 %elem7, ptr %elem_ptr8, align 4
  %arr_ptr9 = load ptr, ptr %arr1, align 8
  %3 = load i64, ptr %j3, align 4
  %4 = load i64, ptr %tmp, align 4
  %elem_ptr10 = getelementptr i64, ptr %arr_ptr9, i64 %3
  store i64 %4, ptr %elem_ptr10, align 4
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 0, ptr %val_ptr, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val
}

define internal %result_int8 @swap_uint8(ptr %arr, i64 %i, i64 %j) {
entry:
  %tmp = alloca i8, align 1
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %i2 = alloca i64, align 8
  store i64 %i, ptr %i2, align 4
  %j3 = alloca i64, align 8
  store i64 %j, ptr %j3, align 4
  %arr_ptr = load ptr, ptr %arr1, align 8
  %0 = load i64, ptr %i2, align 4
  %elem_ptr = getelementptr i8, ptr %arr_ptr, i64 %0
  %elem = load i8, ptr %elem_ptr, align 1
  store i8 %elem, ptr %tmp, align 1
  %arr_ptr4 = load ptr, ptr %arr1, align 8
  %1 = load i64, ptr %i2, align 4
  %arr_ptr5 = load ptr, ptr %arr1, align 8
  %2 = load i64, ptr %j3, align 4
  %elem_ptr6 = getelementptr i8, ptr %arr_ptr5, i64 %2
  %elem7 = load i8, ptr %elem_ptr6, align 1
  %elem_ptr8 = getelementptr i8, ptr %arr_ptr4, i64 %1
  store i8 %elem7, ptr %elem_ptr8, align 1
  %arr_ptr9 = load ptr, ptr %arr1, align 8
  %3 = load i64, ptr %j3, align 4
  %4 = load i8, ptr %tmp, align 1
  %elem_ptr10 = getelementptr i8, ptr %arr_ptr9, i64 %3
  store i8 %4, ptr %elem_ptr10, align 1
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 0, ptr %val_ptr, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val
}

define internal %result_int8 @swap_uint16(ptr %arr, i64 %i, i64 %j) {
entry:
  %tmp = alloca i16, align 2
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %i2 = alloca i64, align 8
  store i64 %i, ptr %i2, align 4
  %j3 = alloca i64, align 8
  store i64 %j, ptr %j3, align 4
  %arr_ptr = load ptr, ptr %arr1, align 8
  %0 = load i64, ptr %i2, align 4
  %elem_ptr = getelementptr i16, ptr %arr_ptr, i64 %0
  %elem = load i16, ptr %elem_ptr, align 2
  store i16 %elem, ptr %tmp, align 2
  %arr_ptr4 = load ptr, ptr %arr1, align 8
  %1 = load i64, ptr %i2, align 4
  %arr_ptr5 = load ptr, ptr %arr1, align 8
  %2 = load i64, ptr %j3, align 4
  %elem_ptr6 = getelementptr i16, ptr %arr_ptr5, i64 %2
  %elem7 = load i16, ptr %elem_ptr6, align 2
  %elem_ptr8 = getelementptr i16, ptr %arr_ptr4, i64 %1
  store i16 %elem7, ptr %elem_ptr8, align 2
  %arr_ptr9 = load ptr, ptr %arr1, align 8
  %3 = load i64, ptr %j3, align 4
  %4 = load i16, ptr %tmp, align 2
  %elem_ptr10 = getelementptr i16, ptr %arr_ptr9, i64 %3
  store i16 %4, ptr %elem_ptr10, align 2
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 0, ptr %val_ptr, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val
}

define internal %result_int8 @swap_uint32(ptr %arr, i64 %i, i64 %j) {
entry:
  %tmp = alloca i32, align 4
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %i2 = alloca i64, align 8
  store i64 %i, ptr %i2, align 4
  %j3 = alloca i64, align 8
  store i64 %j, ptr %j3, align 4
  %arr_ptr = load ptr, ptr %arr1, align 8
  %0 = load i64, ptr %i2, align 4
  %elem_ptr = getelementptr i32, ptr %arr_ptr, i64 %0
  %elem = load i32, ptr %elem_ptr, align 4
  store i32 %elem, ptr %tmp, align 4
  %arr_ptr4 = load ptr, ptr %arr1, align 8
  %1 = load i64, ptr %i2, align 4
  %arr_ptr5 = load ptr, ptr %arr1, align 8
  %2 = load i64, ptr %j3, align 4
  %elem_ptr6 = getelementptr i32, ptr %arr_ptr5, i64 %2
  %elem7 = load i32, ptr %elem_ptr6, align 4
  %elem_ptr8 = getelementptr i32, ptr %arr_ptr4, i64 %1
  store i32 %elem7, ptr %elem_ptr8, align 4
  %arr_ptr9 = load ptr, ptr %arr1, align 8
  %3 = load i64, ptr %j3, align 4
  %4 = load i32, ptr %tmp, align 4
  %elem_ptr10 = getelementptr i32, ptr %arr_ptr9, i64 %3
  store i32 %4, ptr %elem_ptr10, align 4
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 0, ptr %val_ptr, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val
}

define internal %result_int8 @swap_uint64(ptr %arr, i64 %i, i64 %j) {
entry:
  %tmp = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %i2 = alloca i64, align 8
  store i64 %i, ptr %i2, align 4
  %j3 = alloca i64, align 8
  store i64 %j, ptr %j3, align 4
  %arr_ptr = load ptr, ptr %arr1, align 8
  %0 = load i64, ptr %i2, align 4
  %elem_ptr = getelementptr i64, ptr %arr_ptr, i64 %0
  %elem = load i64, ptr %elem_ptr, align 4
  store i64 %elem, ptr %tmp, align 4
  %arr_ptr4 = load ptr, ptr %arr1, align 8
  %1 = load i64, ptr %i2, align 4
  %arr_ptr5 = load ptr, ptr %arr1, align 8
  %2 = load i64, ptr %j3, align 4
  %elem_ptr6 = getelementptr i64, ptr %arr_ptr5, i64 %2
  %elem7 = load i64, ptr %elem_ptr6, align 4
  %elem_ptr8 = getelementptr i64, ptr %arr_ptr4, i64 %1
  store i64 %elem7, ptr %elem_ptr8, align 4
  %arr_ptr9 = load ptr, ptr %arr1, align 8
  %3 = load i64, ptr %j3, align 4
  %4 = load i64, ptr %tmp, align 4
  %elem_ptr10 = getelementptr i64, ptr %arr_ptr9, i64 %3
  store i64 %4, ptr %elem_ptr10, align 4
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 0, ptr %val_ptr, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val
}

define internal %result_int8 @swap_flt32(ptr %arr, i64 %i, i64 %j) {
entry:
  %tmp = alloca float, align 4
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %i2 = alloca i64, align 8
  store i64 %i, ptr %i2, align 4
  %j3 = alloca i64, align 8
  store i64 %j, ptr %j3, align 4
  %arr_ptr = load ptr, ptr %arr1, align 8
  %0 = load i64, ptr %i2, align 4
  %elem_ptr = getelementptr float, ptr %arr_ptr, i64 %0
  %elem = load float, ptr %elem_ptr, align 4
  store float %elem, ptr %tmp, align 4
  %arr_ptr4 = load ptr, ptr %arr1, align 8
  %1 = load i64, ptr %i2, align 4
  %arr_ptr5 = load ptr, ptr %arr1, align 8
  %2 = load i64, ptr %j3, align 4
  %elem_ptr6 = getelementptr float, ptr %arr_ptr5, i64 %2
  %elem7 = load float, ptr %elem_ptr6, align 4
  %elem_ptr8 = getelementptr float, ptr %arr_ptr4, i64 %1
  store float %elem7, ptr %elem_ptr8, align 4
  %arr_ptr9 = load ptr, ptr %arr1, align 8
  %3 = load i64, ptr %j3, align 4
  %4 = load float, ptr %tmp, align 4
  %elem_ptr10 = getelementptr float, ptr %arr_ptr9, i64 %3
  store float %4, ptr %elem_ptr10, align 4
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 0, ptr %val_ptr, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val
}

define internal %result_int8 @swap_flt64(ptr %arr, i64 %i, i64 %j) {
entry:
  %tmp = alloca double, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %i2 = alloca i64, align 8
  store i64 %i, ptr %i2, align 4
  %j3 = alloca i64, align 8
  store i64 %j, ptr %j3, align 4
  %arr_ptr = load ptr, ptr %arr1, align 8
  %0 = load i64, ptr %i2, align 4
  %elem_ptr = getelementptr double, ptr %arr_ptr, i64 %0
  %elem = load double, ptr %elem_ptr, align 8
  store double %elem, ptr %tmp, align 8
  %arr_ptr4 = load ptr, ptr %arr1, align 8
  %1 = load i64, ptr %i2, align 4
  %arr_ptr5 = load ptr, ptr %arr1, align 8
  %2 = load i64, ptr %j3, align 4
  %elem_ptr6 = getelementptr double, ptr %arr_ptr5, i64 %2
  %elem7 = load double, ptr %elem_ptr6, align 8
  %elem_ptr8 = getelementptr double, ptr %arr_ptr4, i64 %1
  store double %elem7, ptr %elem_ptr8, align 8
  %arr_ptr9 = load ptr, ptr %arr1, align 8
  %3 = load i64, ptr %j3, align 4
  %4 = load double, ptr %tmp, align 8
  %elem_ptr10 = getelementptr double, ptr %arr_ptr9, i64 %3
  store double %4, ptr %elem_ptr10, align 8
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
