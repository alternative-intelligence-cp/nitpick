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

define internal %result_int64 @lastIndexOf_int8(ptr %arr, i64 %length, i8 %value) {
entry:
  %i = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %value3 = alloca i8, align 1
  store i8 %value, ptr %value3, align 1
  %0 = load i64, ptr %length2, align 4
  %subtmp = sub i64 %0, 1
  store i64 %subtmp, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont, %entry
  %1 = load i64, ptr %i, align 4
  %getmp = icmp sge i64 %1, 0
  br i1 %getmp, label %while_body, label %while_exit

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
  %subtmp4 = sub i64 %5, 1
  store i64 %subtmp4, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %auto_wrap_result5 = alloca %result_int64, align 8
  %err_ptr6 = getelementptr inbounds %result_int64, ptr %auto_wrap_result5, i32 0, i32 0
  store i8 0, ptr %err_ptr6, align 1
  %val_ptr7 = getelementptr inbounds %result_int64, ptr %auto_wrap_result5, i32 0, i32 1
  store i64 -1, ptr %val_ptr7, align 4
  %result_val8 = load %result_int64, ptr %auto_wrap_result5, align 4
  ret %result_int64 %result_val8
}

define internal %result_int64 @lastIndexOf_int16(ptr %arr, i64 %length, i16 %value) {
entry:
  %i = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %value3 = alloca i16, align 2
  store i16 %value, ptr %value3, align 2
  %0 = load i64, ptr %length2, align 4
  %subtmp = sub i64 %0, 1
  store i64 %subtmp, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont, %entry
  %1 = load i64, ptr %i, align 4
  %getmp = icmp sge i64 %1, 0
  br i1 %getmp, label %while_body, label %while_exit

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
  %subtmp4 = sub i64 %5, 1
  store i64 %subtmp4, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %auto_wrap_result5 = alloca %result_int64, align 8
  %err_ptr6 = getelementptr inbounds %result_int64, ptr %auto_wrap_result5, i32 0, i32 0
  store i8 0, ptr %err_ptr6, align 1
  %val_ptr7 = getelementptr inbounds %result_int64, ptr %auto_wrap_result5, i32 0, i32 1
  store i64 -1, ptr %val_ptr7, align 4
  %result_val8 = load %result_int64, ptr %auto_wrap_result5, align 4
  ret %result_int64 %result_val8
}

define internal %result_int64 @lastIndexOf_int32(ptr %arr, i64 %length, i32 %value) {
entry:
  %i = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %value3 = alloca i32, align 4
  store i32 %value, ptr %value3, align 4
  %0 = load i64, ptr %length2, align 4
  %subtmp = sub i64 %0, 1
  store i64 %subtmp, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont, %entry
  %1 = load i64, ptr %i, align 4
  %getmp = icmp sge i64 %1, 0
  br i1 %getmp, label %while_body, label %while_exit

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
  %subtmp4 = sub i64 %5, 1
  store i64 %subtmp4, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %auto_wrap_result5 = alloca %result_int64, align 8
  %err_ptr6 = getelementptr inbounds %result_int64, ptr %auto_wrap_result5, i32 0, i32 0
  store i8 0, ptr %err_ptr6, align 1
  %val_ptr7 = getelementptr inbounds %result_int64, ptr %auto_wrap_result5, i32 0, i32 1
  store i64 -1, ptr %val_ptr7, align 4
  %result_val8 = load %result_int64, ptr %auto_wrap_result5, align 4
  ret %result_int64 %result_val8
}

define internal %result_int64 @lastIndexOf_int64(ptr %arr, i64 %length, i64 %value) {
entry:
  %i = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %value3 = alloca i64, align 8
  store i64 %value, ptr %value3, align 4
  %0 = load i64, ptr %length2, align 4
  %subtmp = sub i64 %0, 1
  store i64 %subtmp, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont, %entry
  %1 = load i64, ptr %i, align 4
  %getmp = icmp sge i64 %1, 0
  br i1 %getmp, label %while_body, label %while_exit

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
  %subtmp4 = sub i64 %5, 1
  store i64 %subtmp4, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %auto_wrap_result5 = alloca %result_int64, align 8
  %err_ptr6 = getelementptr inbounds %result_int64, ptr %auto_wrap_result5, i32 0, i32 0
  store i8 0, ptr %err_ptr6, align 1
  %val_ptr7 = getelementptr inbounds %result_int64, ptr %auto_wrap_result5, i32 0, i32 1
  store i64 -1, ptr %val_ptr7, align 4
  %result_val8 = load %result_int64, ptr %auto_wrap_result5, align 4
  ret %result_int64 %result_val8
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

define internal %result_int8 @reverse_int8(ptr %arr, i64 %length) {
entry:
  %temp = alloca i8, align 1
  %right = alloca i64, align 8
  %left = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  store i64 0, ptr %left, align 4
  %0 = load i64, ptr %length2, align 4
  %subtmp = sub i64 %0, 1
  store i64 %subtmp, ptr %right, align 4
  br label %while_cond

while_cond:                                       ; preds = %while_body, %entry
  %1 = load i64, ptr %left, align 4
  %2 = load i64, ptr %right, align 4
  %lttmp = icmp slt i64 %1, %2
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr = load ptr, ptr %arr1, align 8
  %3 = load i64, ptr %left, align 4
  %elem_ptr = getelementptr i8, ptr %arr_ptr, i64 %3
  %elem = load i8, ptr %elem_ptr, align 1
  store i8 %elem, ptr %temp, align 1
  %arr_ptr3 = load ptr, ptr %arr1, align 8
  %4 = load i64, ptr %left, align 4
  %arr_ptr4 = load ptr, ptr %arr1, align 8
  %5 = load i64, ptr %right, align 4
  %elem_ptr5 = getelementptr i8, ptr %arr_ptr4, i64 %5
  %elem6 = load i8, ptr %elem_ptr5, align 1
  %elem_ptr7 = getelementptr i8, ptr %arr_ptr3, i64 %4
  store i8 %elem6, ptr %elem_ptr7, align 1
  %arr_ptr8 = load ptr, ptr %arr1, align 8
  %6 = load i64, ptr %right, align 4
  %7 = load i8, ptr %temp, align 1
  %elem_ptr9 = getelementptr i8, ptr %arr_ptr8, i64 %6
  store i8 %7, ptr %elem_ptr9, align 1
  %8 = load i64, ptr %left, align 4
  %addtmp = add i64 %8, 1
  store i64 %addtmp, ptr %left, align 4
  %9 = load i64, ptr %right, align 4
  %subtmp10 = sub i64 %9, 1
  store i64 %subtmp10, ptr %right, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 1, ptr %val_ptr, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val
}

define internal %result_int8 @reverse_int16(ptr %arr, i64 %length) {
entry:
  %temp = alloca i16, align 2
  %right = alloca i64, align 8
  %left = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  store i64 0, ptr %left, align 4
  %0 = load i64, ptr %length2, align 4
  %subtmp = sub i64 %0, 1
  store i64 %subtmp, ptr %right, align 4
  br label %while_cond

while_cond:                                       ; preds = %while_body, %entry
  %1 = load i64, ptr %left, align 4
  %2 = load i64, ptr %right, align 4
  %lttmp = icmp slt i64 %1, %2
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr = load ptr, ptr %arr1, align 8
  %3 = load i64, ptr %left, align 4
  %elem_ptr = getelementptr i16, ptr %arr_ptr, i64 %3
  %elem = load i16, ptr %elem_ptr, align 2
  store i16 %elem, ptr %temp, align 2
  %arr_ptr3 = load ptr, ptr %arr1, align 8
  %4 = load i64, ptr %left, align 4
  %arr_ptr4 = load ptr, ptr %arr1, align 8
  %5 = load i64, ptr %right, align 4
  %elem_ptr5 = getelementptr i16, ptr %arr_ptr4, i64 %5
  %elem6 = load i16, ptr %elem_ptr5, align 2
  %elem_ptr7 = getelementptr i16, ptr %arr_ptr3, i64 %4
  store i16 %elem6, ptr %elem_ptr7, align 2
  %arr_ptr8 = load ptr, ptr %arr1, align 8
  %6 = load i64, ptr %right, align 4
  %7 = load i16, ptr %temp, align 2
  %elem_ptr9 = getelementptr i16, ptr %arr_ptr8, i64 %6
  store i16 %7, ptr %elem_ptr9, align 2
  %8 = load i64, ptr %left, align 4
  %addtmp = add i64 %8, 1
  store i64 %addtmp, ptr %left, align 4
  %9 = load i64, ptr %right, align 4
  %subtmp10 = sub i64 %9, 1
  store i64 %subtmp10, ptr %right, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 1, ptr %val_ptr, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val
}

define internal %result_int8 @reverse_int32(ptr %arr, i64 %length) {
entry:
  %temp = alloca i32, align 4
  %right = alloca i64, align 8
  %left = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  store i64 0, ptr %left, align 4
  %0 = load i64, ptr %length2, align 4
  %subtmp = sub i64 %0, 1
  store i64 %subtmp, ptr %right, align 4
  br label %while_cond

while_cond:                                       ; preds = %while_body, %entry
  %1 = load i64, ptr %left, align 4
  %2 = load i64, ptr %right, align 4
  %lttmp = icmp slt i64 %1, %2
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr = load ptr, ptr %arr1, align 8
  %3 = load i64, ptr %left, align 4
  %elem_ptr = getelementptr i32, ptr %arr_ptr, i64 %3
  %elem = load i32, ptr %elem_ptr, align 4
  store i32 %elem, ptr %temp, align 4
  %arr_ptr3 = load ptr, ptr %arr1, align 8
  %4 = load i64, ptr %left, align 4
  %arr_ptr4 = load ptr, ptr %arr1, align 8
  %5 = load i64, ptr %right, align 4
  %elem_ptr5 = getelementptr i32, ptr %arr_ptr4, i64 %5
  %elem6 = load i32, ptr %elem_ptr5, align 4
  %elem_ptr7 = getelementptr i32, ptr %arr_ptr3, i64 %4
  store i32 %elem6, ptr %elem_ptr7, align 4
  %arr_ptr8 = load ptr, ptr %arr1, align 8
  %6 = load i64, ptr %right, align 4
  %7 = load i32, ptr %temp, align 4
  %elem_ptr9 = getelementptr i32, ptr %arr_ptr8, i64 %6
  store i32 %7, ptr %elem_ptr9, align 4
  %8 = load i64, ptr %left, align 4
  %addtmp = add i64 %8, 1
  store i64 %addtmp, ptr %left, align 4
  %9 = load i64, ptr %right, align 4
  %subtmp10 = sub i64 %9, 1
  store i64 %subtmp10, ptr %right, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 1, ptr %val_ptr, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val
}

define internal %result_int8 @reverse_int64(ptr %arr, i64 %length) {
entry:
  %temp = alloca i64, align 8
  %right = alloca i64, align 8
  %left = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  store i64 0, ptr %left, align 4
  %0 = load i64, ptr %length2, align 4
  %subtmp = sub i64 %0, 1
  store i64 %subtmp, ptr %right, align 4
  br label %while_cond

while_cond:                                       ; preds = %while_body, %entry
  %1 = load i64, ptr %left, align 4
  %2 = load i64, ptr %right, align 4
  %lttmp = icmp slt i64 %1, %2
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr = load ptr, ptr %arr1, align 8
  %3 = load i64, ptr %left, align 4
  %elem_ptr = getelementptr i64, ptr %arr_ptr, i64 %3
  %elem = load i64, ptr %elem_ptr, align 4
  store i64 %elem, ptr %temp, align 4
  %arr_ptr3 = load ptr, ptr %arr1, align 8
  %4 = load i64, ptr %left, align 4
  %arr_ptr4 = load ptr, ptr %arr1, align 8
  %5 = load i64, ptr %right, align 4
  %elem_ptr5 = getelementptr i64, ptr %arr_ptr4, i64 %5
  %elem6 = load i64, ptr %elem_ptr5, align 4
  %elem_ptr7 = getelementptr i64, ptr %arr_ptr3, i64 %4
  store i64 %elem6, ptr %elem_ptr7, align 4
  %arr_ptr8 = load ptr, ptr %arr1, align 8
  %6 = load i64, ptr %right, align 4
  %7 = load i64, ptr %temp, align 4
  %elem_ptr9 = getelementptr i64, ptr %arr_ptr8, i64 %6
  store i64 %7, ptr %elem_ptr9, align 4
  %8 = load i64, ptr %left, align 4
  %addtmp = add i64 %8, 1
  store i64 %addtmp, ptr %left, align 4
  %9 = load i64, ptr %right, align 4
  %subtmp10 = sub i64 %9, 1
  store i64 %subtmp10, ptr %right, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 1, ptr %val_ptr, align 1
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
  store i8 1, ptr %val_ptr, align 1
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
  store i8 1, ptr %val_ptr, align 1
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
  store i8 1, ptr %val_ptr, align 1
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
  store i8 1, ptr %val_ptr, align 1
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
  store i8 1, ptr %val_ptr, align 1
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
  store i8 1, ptr %val_ptr, align 1
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
  store i8 1, ptr %val_ptr, align 1
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
  store i8 1, ptr %val_ptr, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val
}

define internal %result_int8 @equals_int8(ptr %arr1, ptr %arr2, i64 %length) {
entry:
  %i = alloca i64, align 8
  %arr11 = alloca ptr, align 8
  store ptr %arr1, ptr %arr11, align 8
  %arr22 = alloca ptr, align 8
  store ptr %arr2, ptr %arr22, align 8
  %length3 = alloca i64, align 8
  store i64 %length, ptr %length3, align 4
  store i64 0, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont, %entry
  %0 = load i64, ptr %i, align 4
  %1 = load i64, ptr %length3, align 4
  %lttmp = icmp slt i64 %0, %1
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr1_ptr = load ptr, ptr %arr11, align 8
  %2 = load i64, ptr %i, align 4
  %elem_ptr = getelementptr i8, ptr %arr1_ptr, i64 %2
  %elem = load i8, ptr %elem_ptr, align 1
  %arr2_ptr = load ptr, ptr %arr22, align 8
  %3 = load i64, ptr %i, align 4
  %elem_ptr4 = getelementptr i8, ptr %arr2_ptr, i64 %3
  %elem5 = load i8, ptr %elem_ptr4, align 1
  %netmp = icmp ne i8 %elem, %elem5
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
  %auto_wrap_result6 = alloca %result_int8, align 8
  %err_ptr7 = getelementptr inbounds %result_int8, ptr %auto_wrap_result6, i32 0, i32 0
  store i8 0, ptr %err_ptr7, align 1
  %val_ptr8 = getelementptr inbounds %result_int8, ptr %auto_wrap_result6, i32 0, i32 1
  store i8 1, ptr %val_ptr8, align 1
  %result_val9 = load %result_int8, ptr %auto_wrap_result6, align 1
  ret %result_int8 %result_val9
}

define internal %result_int8 @equals_int16(ptr %arr1, ptr %arr2, i64 %length) {
entry:
  %i = alloca i64, align 8
  %arr11 = alloca ptr, align 8
  store ptr %arr1, ptr %arr11, align 8
  %arr22 = alloca ptr, align 8
  store ptr %arr2, ptr %arr22, align 8
  %length3 = alloca i64, align 8
  store i64 %length, ptr %length3, align 4
  store i64 0, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont, %entry
  %0 = load i64, ptr %i, align 4
  %1 = load i64, ptr %length3, align 4
  %lttmp = icmp slt i64 %0, %1
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr1_ptr = load ptr, ptr %arr11, align 8
  %2 = load i64, ptr %i, align 4
  %elem_ptr = getelementptr i16, ptr %arr1_ptr, i64 %2
  %elem = load i16, ptr %elem_ptr, align 2
  %arr2_ptr = load ptr, ptr %arr22, align 8
  %3 = load i64, ptr %i, align 4
  %elem_ptr4 = getelementptr i16, ptr %arr2_ptr, i64 %3
  %elem5 = load i16, ptr %elem_ptr4, align 2
  %netmp = icmp ne i16 %elem, %elem5
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
  %auto_wrap_result6 = alloca %result_int8, align 8
  %err_ptr7 = getelementptr inbounds %result_int8, ptr %auto_wrap_result6, i32 0, i32 0
  store i8 0, ptr %err_ptr7, align 1
  %val_ptr8 = getelementptr inbounds %result_int8, ptr %auto_wrap_result6, i32 0, i32 1
  store i8 1, ptr %val_ptr8, align 1
  %result_val9 = load %result_int8, ptr %auto_wrap_result6, align 1
  ret %result_int8 %result_val9
}

define internal %result_int8 @equals_int32(ptr %arr1, ptr %arr2, i64 %length) {
entry:
  %i = alloca i64, align 8
  %arr11 = alloca ptr, align 8
  store ptr %arr1, ptr %arr11, align 8
  %arr22 = alloca ptr, align 8
  store ptr %arr2, ptr %arr22, align 8
  %length3 = alloca i64, align 8
  store i64 %length, ptr %length3, align 4
  store i64 0, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont, %entry
  %0 = load i64, ptr %i, align 4
  %1 = load i64, ptr %length3, align 4
  %lttmp = icmp slt i64 %0, %1
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr1_ptr = load ptr, ptr %arr11, align 8
  %2 = load i64, ptr %i, align 4
  %elem_ptr = getelementptr i32, ptr %arr1_ptr, i64 %2
  %elem = load i32, ptr %elem_ptr, align 4
  %arr2_ptr = load ptr, ptr %arr22, align 8
  %3 = load i64, ptr %i, align 4
  %elem_ptr4 = getelementptr i32, ptr %arr2_ptr, i64 %3
  %elem5 = load i32, ptr %elem_ptr4, align 4
  %netmp = icmp ne i32 %elem, %elem5
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
  %auto_wrap_result6 = alloca %result_int8, align 8
  %err_ptr7 = getelementptr inbounds %result_int8, ptr %auto_wrap_result6, i32 0, i32 0
  store i8 0, ptr %err_ptr7, align 1
  %val_ptr8 = getelementptr inbounds %result_int8, ptr %auto_wrap_result6, i32 0, i32 1
  store i8 1, ptr %val_ptr8, align 1
  %result_val9 = load %result_int8, ptr %auto_wrap_result6, align 1
  ret %result_int8 %result_val9
}

define internal %result_int8 @equals_int64(ptr %arr1, ptr %arr2, i64 %length) {
entry:
  %i = alloca i64, align 8
  %arr11 = alloca ptr, align 8
  store ptr %arr1, ptr %arr11, align 8
  %arr22 = alloca ptr, align 8
  store ptr %arr2, ptr %arr22, align 8
  %length3 = alloca i64, align 8
  store i64 %length, ptr %length3, align 4
  store i64 0, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont, %entry
  %0 = load i64, ptr %i, align 4
  %1 = load i64, ptr %length3, align 4
  %lttmp = icmp slt i64 %0, %1
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr1_ptr = load ptr, ptr %arr11, align 8
  %2 = load i64, ptr %i, align 4
  %elem_ptr = getelementptr i64, ptr %arr1_ptr, i64 %2
  %elem = load i64, ptr %elem_ptr, align 4
  %arr2_ptr = load ptr, ptr %arr22, align 8
  %3 = load i64, ptr %i, align 4
  %elem_ptr4 = getelementptr i64, ptr %arr2_ptr, i64 %3
  %elem5 = load i64, ptr %elem_ptr4, align 4
  %netmp = icmp ne i64 %elem, %elem5
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
  %auto_wrap_result6 = alloca %result_int8, align 8
  %err_ptr7 = getelementptr inbounds %result_int8, ptr %auto_wrap_result6, i32 0, i32 0
  store i8 0, ptr %err_ptr7, align 1
  %val_ptr8 = getelementptr inbounds %result_int8, ptr %auto_wrap_result6, i32 0, i32 1
  store i8 1, ptr %val_ptr8, align 1
  %result_val9 = load %result_int8, ptr %auto_wrap_result6, align 1
  ret %result_int8 %result_val9
}

define internal %result_int8 @min_in_array_int8(ptr %arr, i64 %length) {
entry:
  %i = alloca i64, align 8
  %min_val = alloca i8, align 1
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %0 = load i64, ptr %length2, align 4
  %letmp = icmp sle i64 %0, 0
  br i1 %letmp, label %then, label %ifcont

then:                                             ; preds = %entry
  %arr_ptr = load ptr, ptr %arr1, align 8
  %elem_ptr = getelementptr i8, ptr %arr_ptr, i64 0
  %elem = load i8, ptr %elem_ptr, align 1
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 %elem, ptr %val_ptr, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val

ifcont:                                           ; preds = %entry
  %arr_ptr3 = load ptr, ptr %arr1, align 8
  %elem_ptr4 = getelementptr i8, ptr %arr_ptr3, i64 0
  %elem5 = load i8, ptr %elem_ptr4, align 1
  store i8 %elem5, ptr %min_val, align 1
  store i64 1, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont14, %ifcont
  %1 = load i64, ptr %i, align 4
  %2 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %1, %2
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr6 = load ptr, ptr %arr1, align 8
  %3 = load i64, ptr %i, align 4
  %elem_ptr7 = getelementptr i8, ptr %arr_ptr6, i64 %3
  %elem8 = load i8, ptr %elem_ptr7, align 1
  %4 = load i8, ptr %min_val, align 1
  %lttmp9 = icmp slt i8 %elem8, %4
  br i1 %lttmp9, label %then10, label %ifcont14

then10:                                           ; preds = %while_body
  %arr_ptr11 = load ptr, ptr %arr1, align 8
  %5 = load i64, ptr %i, align 4
  %elem_ptr12 = getelementptr i8, ptr %arr_ptr11, i64 %5
  %elem13 = load i8, ptr %elem_ptr12, align 1
  store i8 %elem13, ptr %min_val, align 1
  br label %ifcont14

ifcont14:                                         ; preds = %then10, %while_body
  %6 = load i64, ptr %i, align 4
  %addtmp = add i64 %6, 1
  store i64 %addtmp, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %7 = load i8, ptr %min_val, align 1
  %auto_wrap_result15 = alloca %result_int8, align 8
  %err_ptr16 = getelementptr inbounds %result_int8, ptr %auto_wrap_result15, i32 0, i32 0
  store i8 0, ptr %err_ptr16, align 1
  %val_ptr17 = getelementptr inbounds %result_int8, ptr %auto_wrap_result15, i32 0, i32 1
  store i8 %7, ptr %val_ptr17, align 1
  %result_val18 = load %result_int8, ptr %auto_wrap_result15, align 1
  ret %result_int8 %result_val18
}

define internal %result_int16 @min_in_array_int16(ptr %arr, i64 %length) {
entry:
  %i = alloca i64, align 8
  %min_val = alloca i16, align 2
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %0 = load i64, ptr %length2, align 4
  %letmp = icmp sle i64 %0, 0
  br i1 %letmp, label %then, label %ifcont

then:                                             ; preds = %entry
  %arr_ptr = load ptr, ptr %arr1, align 8
  %elem_ptr = getelementptr i16, ptr %arr_ptr, i64 0
  %elem = load i16, ptr %elem_ptr, align 2
  %auto_wrap_result = alloca %result_int16, align 8
  %err_ptr = getelementptr inbounds %result_int16, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int16, ptr %auto_wrap_result, i32 0, i32 1
  store i16 %elem, ptr %val_ptr, align 2
  %result_val = load %result_int16, ptr %auto_wrap_result, align 2
  ret %result_int16 %result_val

ifcont:                                           ; preds = %entry
  %arr_ptr3 = load ptr, ptr %arr1, align 8
  %elem_ptr4 = getelementptr i16, ptr %arr_ptr3, i64 0
  %elem5 = load i16, ptr %elem_ptr4, align 2
  store i16 %elem5, ptr %min_val, align 2
  store i64 1, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont14, %ifcont
  %1 = load i64, ptr %i, align 4
  %2 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %1, %2
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr6 = load ptr, ptr %arr1, align 8
  %3 = load i64, ptr %i, align 4
  %elem_ptr7 = getelementptr i16, ptr %arr_ptr6, i64 %3
  %elem8 = load i16, ptr %elem_ptr7, align 2
  %4 = load i16, ptr %min_val, align 2
  %lttmp9 = icmp slt i16 %elem8, %4
  br i1 %lttmp9, label %then10, label %ifcont14

then10:                                           ; preds = %while_body
  %arr_ptr11 = load ptr, ptr %arr1, align 8
  %5 = load i64, ptr %i, align 4
  %elem_ptr12 = getelementptr i16, ptr %arr_ptr11, i64 %5
  %elem13 = load i16, ptr %elem_ptr12, align 2
  store i16 %elem13, ptr %min_val, align 2
  br label %ifcont14

ifcont14:                                         ; preds = %then10, %while_body
  %6 = load i64, ptr %i, align 4
  %addtmp = add i64 %6, 1
  store i64 %addtmp, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %7 = load i16, ptr %min_val, align 2
  %auto_wrap_result15 = alloca %result_int16, align 8
  %err_ptr16 = getelementptr inbounds %result_int16, ptr %auto_wrap_result15, i32 0, i32 0
  store i8 0, ptr %err_ptr16, align 1
  %val_ptr17 = getelementptr inbounds %result_int16, ptr %auto_wrap_result15, i32 0, i32 1
  store i16 %7, ptr %val_ptr17, align 2
  %result_val18 = load %result_int16, ptr %auto_wrap_result15, align 2
  ret %result_int16 %result_val18
}

define internal %result_int32 @min_in_array_int32(ptr %arr, i64 %length) {
entry:
  %i = alloca i64, align 8
  %min_val = alloca i32, align 4
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %0 = load i64, ptr %length2, align 4
  %letmp = icmp sle i64 %0, 0
  br i1 %letmp, label %then, label %ifcont

then:                                             ; preds = %entry
  %arr_ptr = load ptr, ptr %arr1, align 8
  %elem_ptr = getelementptr i32, ptr %arr_ptr, i64 0
  %elem = load i32, ptr %elem_ptr, align 4
  %auto_wrap_result = alloca %result_int32, align 8
  %err_ptr = getelementptr inbounds %result_int32, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int32, ptr %auto_wrap_result, i32 0, i32 1
  store i32 %elem, ptr %val_ptr, align 4
  %result_val = load %result_int32, ptr %auto_wrap_result, align 4
  ret %result_int32 %result_val

ifcont:                                           ; preds = %entry
  %arr_ptr3 = load ptr, ptr %arr1, align 8
  %elem_ptr4 = getelementptr i32, ptr %arr_ptr3, i64 0
  %elem5 = load i32, ptr %elem_ptr4, align 4
  store i32 %elem5, ptr %min_val, align 4
  store i64 1, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont14, %ifcont
  %1 = load i64, ptr %i, align 4
  %2 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %1, %2
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr6 = load ptr, ptr %arr1, align 8
  %3 = load i64, ptr %i, align 4
  %elem_ptr7 = getelementptr i32, ptr %arr_ptr6, i64 %3
  %elem8 = load i32, ptr %elem_ptr7, align 4
  %4 = load i32, ptr %min_val, align 4
  %lttmp9 = icmp slt i32 %elem8, %4
  br i1 %lttmp9, label %then10, label %ifcont14

then10:                                           ; preds = %while_body
  %arr_ptr11 = load ptr, ptr %arr1, align 8
  %5 = load i64, ptr %i, align 4
  %elem_ptr12 = getelementptr i32, ptr %arr_ptr11, i64 %5
  %elem13 = load i32, ptr %elem_ptr12, align 4
  store i32 %elem13, ptr %min_val, align 4
  br label %ifcont14

ifcont14:                                         ; preds = %then10, %while_body
  %6 = load i64, ptr %i, align 4
  %addtmp = add i64 %6, 1
  store i64 %addtmp, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %7 = load i32, ptr %min_val, align 4
  %auto_wrap_result15 = alloca %result_int32, align 8
  %err_ptr16 = getelementptr inbounds %result_int32, ptr %auto_wrap_result15, i32 0, i32 0
  store i8 0, ptr %err_ptr16, align 1
  %val_ptr17 = getelementptr inbounds %result_int32, ptr %auto_wrap_result15, i32 0, i32 1
  store i32 %7, ptr %val_ptr17, align 4
  %result_val18 = load %result_int32, ptr %auto_wrap_result15, align 4
  ret %result_int32 %result_val18
}

define internal %result_int64 @min_in_array_int64(ptr %arr, i64 %length) {
entry:
  %i = alloca i64, align 8
  %min_val = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %0 = load i64, ptr %length2, align 4
  %letmp = icmp sle i64 %0, 0
  br i1 %letmp, label %then, label %ifcont

then:                                             ; preds = %entry
  %arr_ptr = load ptr, ptr %arr1, align 8
  %elem_ptr = getelementptr i64, ptr %arr_ptr, i64 0
  %elem = load i64, ptr %elem_ptr, align 4
  %auto_wrap_result = alloca %result_int64, align 8
  %err_ptr = getelementptr inbounds %result_int64, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int64, ptr %auto_wrap_result, i32 0, i32 1
  store i64 %elem, ptr %val_ptr, align 4
  %result_val = load %result_int64, ptr %auto_wrap_result, align 4
  ret %result_int64 %result_val

ifcont:                                           ; preds = %entry
  %arr_ptr3 = load ptr, ptr %arr1, align 8
  %elem_ptr4 = getelementptr i64, ptr %arr_ptr3, i64 0
  %elem5 = load i64, ptr %elem_ptr4, align 4
  store i64 %elem5, ptr %min_val, align 4
  store i64 1, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont14, %ifcont
  %1 = load i64, ptr %i, align 4
  %2 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %1, %2
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr6 = load ptr, ptr %arr1, align 8
  %3 = load i64, ptr %i, align 4
  %elem_ptr7 = getelementptr i64, ptr %arr_ptr6, i64 %3
  %elem8 = load i64, ptr %elem_ptr7, align 4
  %4 = load i64, ptr %min_val, align 4
  %lttmp9 = icmp slt i64 %elem8, %4
  br i1 %lttmp9, label %then10, label %ifcont14

then10:                                           ; preds = %while_body
  %arr_ptr11 = load ptr, ptr %arr1, align 8
  %5 = load i64, ptr %i, align 4
  %elem_ptr12 = getelementptr i64, ptr %arr_ptr11, i64 %5
  %elem13 = load i64, ptr %elem_ptr12, align 4
  store i64 %elem13, ptr %min_val, align 4
  br label %ifcont14

ifcont14:                                         ; preds = %then10, %while_body
  %6 = load i64, ptr %i, align 4
  %addtmp = add i64 %6, 1
  store i64 %addtmp, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %7 = load i64, ptr %min_val, align 4
  %auto_wrap_result15 = alloca %result_int64, align 8
  %err_ptr16 = getelementptr inbounds %result_int64, ptr %auto_wrap_result15, i32 0, i32 0
  store i8 0, ptr %err_ptr16, align 1
  %val_ptr17 = getelementptr inbounds %result_int64, ptr %auto_wrap_result15, i32 0, i32 1
  store i64 %7, ptr %val_ptr17, align 4
  %result_val18 = load %result_int64, ptr %auto_wrap_result15, align 4
  ret %result_int64 %result_val18
}

define internal %result_int8 @max_in_array_int8(ptr %arr, i64 %length) {
entry:
  %i = alloca i64, align 8
  %max_val = alloca i8, align 1
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %0 = load i64, ptr %length2, align 4
  %letmp = icmp sle i64 %0, 0
  br i1 %letmp, label %then, label %ifcont

then:                                             ; preds = %entry
  %arr_ptr = load ptr, ptr %arr1, align 8
  %elem_ptr = getelementptr i8, ptr %arr_ptr, i64 0
  %elem = load i8, ptr %elem_ptr, align 1
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 %elem, ptr %val_ptr, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val

ifcont:                                           ; preds = %entry
  %arr_ptr3 = load ptr, ptr %arr1, align 8
  %elem_ptr4 = getelementptr i8, ptr %arr_ptr3, i64 0
  %elem5 = load i8, ptr %elem_ptr4, align 1
  store i8 %elem5, ptr %max_val, align 1
  store i64 1, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont13, %ifcont
  %1 = load i64, ptr %i, align 4
  %2 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %1, %2
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr6 = load ptr, ptr %arr1, align 8
  %3 = load i64, ptr %i, align 4
  %elem_ptr7 = getelementptr i8, ptr %arr_ptr6, i64 %3
  %elem8 = load i8, ptr %elem_ptr7, align 1
  %4 = load i8, ptr %max_val, align 1
  %gttmp = icmp sgt i8 %elem8, %4
  br i1 %gttmp, label %then9, label %ifcont13

then9:                                            ; preds = %while_body
  %arr_ptr10 = load ptr, ptr %arr1, align 8
  %5 = load i64, ptr %i, align 4
  %elem_ptr11 = getelementptr i8, ptr %arr_ptr10, i64 %5
  %elem12 = load i8, ptr %elem_ptr11, align 1
  store i8 %elem12, ptr %max_val, align 1
  br label %ifcont13

ifcont13:                                         ; preds = %then9, %while_body
  %6 = load i64, ptr %i, align 4
  %addtmp = add i64 %6, 1
  store i64 %addtmp, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %7 = load i8, ptr %max_val, align 1
  %auto_wrap_result14 = alloca %result_int8, align 8
  %err_ptr15 = getelementptr inbounds %result_int8, ptr %auto_wrap_result14, i32 0, i32 0
  store i8 0, ptr %err_ptr15, align 1
  %val_ptr16 = getelementptr inbounds %result_int8, ptr %auto_wrap_result14, i32 0, i32 1
  store i8 %7, ptr %val_ptr16, align 1
  %result_val17 = load %result_int8, ptr %auto_wrap_result14, align 1
  ret %result_int8 %result_val17
}

define internal %result_int16 @max_in_array_int16(ptr %arr, i64 %length) {
entry:
  %i = alloca i64, align 8
  %max_val = alloca i16, align 2
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %0 = load i64, ptr %length2, align 4
  %letmp = icmp sle i64 %0, 0
  br i1 %letmp, label %then, label %ifcont

then:                                             ; preds = %entry
  %arr_ptr = load ptr, ptr %arr1, align 8
  %elem_ptr = getelementptr i16, ptr %arr_ptr, i64 0
  %elem = load i16, ptr %elem_ptr, align 2
  %auto_wrap_result = alloca %result_int16, align 8
  %err_ptr = getelementptr inbounds %result_int16, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int16, ptr %auto_wrap_result, i32 0, i32 1
  store i16 %elem, ptr %val_ptr, align 2
  %result_val = load %result_int16, ptr %auto_wrap_result, align 2
  ret %result_int16 %result_val

ifcont:                                           ; preds = %entry
  %arr_ptr3 = load ptr, ptr %arr1, align 8
  %elem_ptr4 = getelementptr i16, ptr %arr_ptr3, i64 0
  %elem5 = load i16, ptr %elem_ptr4, align 2
  store i16 %elem5, ptr %max_val, align 2
  store i64 1, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont13, %ifcont
  %1 = load i64, ptr %i, align 4
  %2 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %1, %2
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr6 = load ptr, ptr %arr1, align 8
  %3 = load i64, ptr %i, align 4
  %elem_ptr7 = getelementptr i16, ptr %arr_ptr6, i64 %3
  %elem8 = load i16, ptr %elem_ptr7, align 2
  %4 = load i16, ptr %max_val, align 2
  %gttmp = icmp sgt i16 %elem8, %4
  br i1 %gttmp, label %then9, label %ifcont13

then9:                                            ; preds = %while_body
  %arr_ptr10 = load ptr, ptr %arr1, align 8
  %5 = load i64, ptr %i, align 4
  %elem_ptr11 = getelementptr i16, ptr %arr_ptr10, i64 %5
  %elem12 = load i16, ptr %elem_ptr11, align 2
  store i16 %elem12, ptr %max_val, align 2
  br label %ifcont13

ifcont13:                                         ; preds = %then9, %while_body
  %6 = load i64, ptr %i, align 4
  %addtmp = add i64 %6, 1
  store i64 %addtmp, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %7 = load i16, ptr %max_val, align 2
  %auto_wrap_result14 = alloca %result_int16, align 8
  %err_ptr15 = getelementptr inbounds %result_int16, ptr %auto_wrap_result14, i32 0, i32 0
  store i8 0, ptr %err_ptr15, align 1
  %val_ptr16 = getelementptr inbounds %result_int16, ptr %auto_wrap_result14, i32 0, i32 1
  store i16 %7, ptr %val_ptr16, align 2
  %result_val17 = load %result_int16, ptr %auto_wrap_result14, align 2
  ret %result_int16 %result_val17
}

define internal %result_int32 @max_in_array_int32(ptr %arr, i64 %length) {
entry:
  %i = alloca i64, align 8
  %max_val = alloca i32, align 4
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %0 = load i64, ptr %length2, align 4
  %letmp = icmp sle i64 %0, 0
  br i1 %letmp, label %then, label %ifcont

then:                                             ; preds = %entry
  %arr_ptr = load ptr, ptr %arr1, align 8
  %elem_ptr = getelementptr i32, ptr %arr_ptr, i64 0
  %elem = load i32, ptr %elem_ptr, align 4
  %auto_wrap_result = alloca %result_int32, align 8
  %err_ptr = getelementptr inbounds %result_int32, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int32, ptr %auto_wrap_result, i32 0, i32 1
  store i32 %elem, ptr %val_ptr, align 4
  %result_val = load %result_int32, ptr %auto_wrap_result, align 4
  ret %result_int32 %result_val

ifcont:                                           ; preds = %entry
  %arr_ptr3 = load ptr, ptr %arr1, align 8
  %elem_ptr4 = getelementptr i32, ptr %arr_ptr3, i64 0
  %elem5 = load i32, ptr %elem_ptr4, align 4
  store i32 %elem5, ptr %max_val, align 4
  store i64 1, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont13, %ifcont
  %1 = load i64, ptr %i, align 4
  %2 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %1, %2
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr6 = load ptr, ptr %arr1, align 8
  %3 = load i64, ptr %i, align 4
  %elem_ptr7 = getelementptr i32, ptr %arr_ptr6, i64 %3
  %elem8 = load i32, ptr %elem_ptr7, align 4
  %4 = load i32, ptr %max_val, align 4
  %gttmp = icmp sgt i32 %elem8, %4
  br i1 %gttmp, label %then9, label %ifcont13

then9:                                            ; preds = %while_body
  %arr_ptr10 = load ptr, ptr %arr1, align 8
  %5 = load i64, ptr %i, align 4
  %elem_ptr11 = getelementptr i32, ptr %arr_ptr10, i64 %5
  %elem12 = load i32, ptr %elem_ptr11, align 4
  store i32 %elem12, ptr %max_val, align 4
  br label %ifcont13

ifcont13:                                         ; preds = %then9, %while_body
  %6 = load i64, ptr %i, align 4
  %addtmp = add i64 %6, 1
  store i64 %addtmp, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %7 = load i32, ptr %max_val, align 4
  %auto_wrap_result14 = alloca %result_int32, align 8
  %err_ptr15 = getelementptr inbounds %result_int32, ptr %auto_wrap_result14, i32 0, i32 0
  store i8 0, ptr %err_ptr15, align 1
  %val_ptr16 = getelementptr inbounds %result_int32, ptr %auto_wrap_result14, i32 0, i32 1
  store i32 %7, ptr %val_ptr16, align 4
  %result_val17 = load %result_int32, ptr %auto_wrap_result14, align 4
  ret %result_int32 %result_val17
}

define internal %result_int64 @max_in_array_int64(ptr %arr, i64 %length) {
entry:
  %i = alloca i64, align 8
  %max_val = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %0 = load i64, ptr %length2, align 4
  %letmp = icmp sle i64 %0, 0
  br i1 %letmp, label %then, label %ifcont

then:                                             ; preds = %entry
  %arr_ptr = load ptr, ptr %arr1, align 8
  %elem_ptr = getelementptr i64, ptr %arr_ptr, i64 0
  %elem = load i64, ptr %elem_ptr, align 4
  %auto_wrap_result = alloca %result_int64, align 8
  %err_ptr = getelementptr inbounds %result_int64, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int64, ptr %auto_wrap_result, i32 0, i32 1
  store i64 %elem, ptr %val_ptr, align 4
  %result_val = load %result_int64, ptr %auto_wrap_result, align 4
  ret %result_int64 %result_val

ifcont:                                           ; preds = %entry
  %arr_ptr3 = load ptr, ptr %arr1, align 8
  %elem_ptr4 = getelementptr i64, ptr %arr_ptr3, i64 0
  %elem5 = load i64, ptr %elem_ptr4, align 4
  store i64 %elem5, ptr %max_val, align 4
  store i64 1, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont13, %ifcont
  %1 = load i64, ptr %i, align 4
  %2 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %1, %2
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr6 = load ptr, ptr %arr1, align 8
  %3 = load i64, ptr %i, align 4
  %elem_ptr7 = getelementptr i64, ptr %arr_ptr6, i64 %3
  %elem8 = load i64, ptr %elem_ptr7, align 4
  %4 = load i64, ptr %max_val, align 4
  %gttmp = icmp sgt i64 %elem8, %4
  br i1 %gttmp, label %then9, label %ifcont13

then9:                                            ; preds = %while_body
  %arr_ptr10 = load ptr, ptr %arr1, align 8
  %5 = load i64, ptr %i, align 4
  %elem_ptr11 = getelementptr i64, ptr %arr_ptr10, i64 %5
  %elem12 = load i64, ptr %elem_ptr11, align 4
  store i64 %elem12, ptr %max_val, align 4
  br label %ifcont13

ifcont13:                                         ; preds = %then9, %while_body
  %6 = load i64, ptr %i, align 4
  %addtmp = add i64 %6, 1
  store i64 %addtmp, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %7 = load i64, ptr %max_val, align 4
  %auto_wrap_result14 = alloca %result_int64, align 8
  %err_ptr15 = getelementptr inbounds %result_int64, ptr %auto_wrap_result14, i32 0, i32 0
  store i8 0, ptr %err_ptr15, align 1
  %val_ptr16 = getelementptr inbounds %result_int64, ptr %auto_wrap_result14, i32 0, i32 1
  store i64 %7, ptr %val_ptr16, align 4
  %result_val17 = load %result_int64, ptr %auto_wrap_result14, align 4
  ret %result_int64 %result_val17
}

define internal %result_int8 @sum_int8(ptr %arr, i64 %length) {
entry:
  %i = alloca i64, align 8
  %total = alloca i8, align 1
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  store i8 0, ptr %total, align 1
  store i64 0, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %while_body, %entry
  %0 = load i64, ptr %i, align 4
  %1 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %0, %1
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %2 = load i8, ptr %total, align 1
  %arr_ptr = load ptr, ptr %arr1, align 8
  %3 = load i64, ptr %i, align 4
  %elem_ptr = getelementptr i8, ptr %arr_ptr, i64 %3
  %elem = load i8, ptr %elem_ptr, align 1
  %addtmp = add i8 %2, %elem
  store i8 %addtmp, ptr %total, align 1
  %4 = load i64, ptr %i, align 4
  %addtmp3 = add i64 %4, 1
  store i64 %addtmp3, ptr %i, align 4
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
  store i16 0, ptr %total, align 2
  store i64 0, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %while_body, %entry
  %0 = load i64, ptr %i, align 4
  %1 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %0, %1
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %2 = load i16, ptr %total, align 2
  %arr_ptr = load ptr, ptr %arr1, align 8
  %3 = load i64, ptr %i, align 4
  %elem_ptr = getelementptr i16, ptr %arr_ptr, i64 %3
  %elem = load i16, ptr %elem_ptr, align 2
  %addtmp = add i16 %2, %elem
  store i16 %addtmp, ptr %total, align 2
  %4 = load i64, ptr %i, align 4
  %addtmp3 = add i64 %4, 1
  store i64 %addtmp3, ptr %i, align 4
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
  store i32 0, ptr %total, align 4
  store i64 0, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %while_body, %entry
  %0 = load i64, ptr %i, align 4
  %1 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %0, %1
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %2 = load i32, ptr %total, align 4
  %arr_ptr = load ptr, ptr %arr1, align 8
  %3 = load i64, ptr %i, align 4
  %elem_ptr = getelementptr i32, ptr %arr_ptr, i64 %3
  %elem = load i32, ptr %elem_ptr, align 4
  %addtmp = add i32 %2, %elem
  store i32 %addtmp, ptr %total, align 4
  %4 = load i64, ptr %i, align 4
  %addtmp3 = add i64 %4, 1
  store i64 %addtmp3, ptr %i, align 4
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
  store i64 0, ptr %total, align 4
  store i64 0, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %while_body, %entry
  %0 = load i64, ptr %i, align 4
  %1 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %0, %1
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %2 = load i64, ptr %total, align 4
  %arr_ptr = load ptr, ptr %arr1, align 8
  %3 = load i64, ptr %i, align 4
  %elem_ptr = getelementptr i64, ptr %arr_ptr, i64 %3
  %elem = load i64, ptr %elem_ptr, align 4
  %addtmp = add i64 %2, %elem
  store i64 %addtmp, ptr %total, align 4
  %4 = load i64, ptr %i, align 4
  %addtmp3 = add i64 %4, 1
  store i64 %addtmp3, ptr %i, align 4
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

define internal %result_int8 @is_sorted_int8(ptr %arr, i64 %length) {
entry:
  %i = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %0 = load i64, ptr %length2, align 4
  %letmp = icmp sle i64 %0, 1
  br i1 %letmp, label %then, label %ifcont

then:                                             ; preds = %entry
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 1, ptr %val_ptr, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val

ifcont:                                           ; preds = %entry
  store i64 0, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont11, %ifcont
  %1 = load i64, ptr %i, align 4
  %2 = load i64, ptr %length2, align 4
  %subtmp = sub i64 %2, 1
  %lttmp = icmp slt i64 %1, %subtmp
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr = load ptr, ptr %arr1, align 8
  %3 = load i64, ptr %i, align 4
  %elem_ptr = getelementptr i8, ptr %arr_ptr, i64 %3
  %elem = load i8, ptr %elem_ptr, align 1
  %arr_ptr3 = load ptr, ptr %arr1, align 8
  %4 = load i64, ptr %i, align 4
  %addtmp = add i64 %4, 1
  %elem_ptr4 = getelementptr i8, ptr %arr_ptr3, i64 %addtmp
  %elem5 = load i8, ptr %elem_ptr4, align 1
  %gttmp = icmp sgt i8 %elem, %elem5
  br i1 %gttmp, label %then6, label %ifcont11

then6:                                            ; preds = %while_body
  %auto_wrap_result7 = alloca %result_int8, align 8
  %err_ptr8 = getelementptr inbounds %result_int8, ptr %auto_wrap_result7, i32 0, i32 0
  store i8 0, ptr %err_ptr8, align 1
  %val_ptr9 = getelementptr inbounds %result_int8, ptr %auto_wrap_result7, i32 0, i32 1
  store i8 0, ptr %val_ptr9, align 1
  %result_val10 = load %result_int8, ptr %auto_wrap_result7, align 1
  ret %result_int8 %result_val10

ifcont11:                                         ; preds = %while_body
  %5 = load i64, ptr %i, align 4
  %addtmp12 = add i64 %5, 1
  store i64 %addtmp12, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %auto_wrap_result13 = alloca %result_int8, align 8
  %err_ptr14 = getelementptr inbounds %result_int8, ptr %auto_wrap_result13, i32 0, i32 0
  store i8 0, ptr %err_ptr14, align 1
  %val_ptr15 = getelementptr inbounds %result_int8, ptr %auto_wrap_result13, i32 0, i32 1
  store i8 1, ptr %val_ptr15, align 1
  %result_val16 = load %result_int8, ptr %auto_wrap_result13, align 1
  ret %result_int8 %result_val16
}

define internal %result_int8 @is_sorted_int16(ptr %arr, i64 %length) {
entry:
  %i = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %0 = load i64, ptr %length2, align 4
  %letmp = icmp sle i64 %0, 1
  br i1 %letmp, label %then, label %ifcont

then:                                             ; preds = %entry
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 1, ptr %val_ptr, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val

ifcont:                                           ; preds = %entry
  store i64 0, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont11, %ifcont
  %1 = load i64, ptr %i, align 4
  %2 = load i64, ptr %length2, align 4
  %subtmp = sub i64 %2, 1
  %lttmp = icmp slt i64 %1, %subtmp
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr = load ptr, ptr %arr1, align 8
  %3 = load i64, ptr %i, align 4
  %elem_ptr = getelementptr i16, ptr %arr_ptr, i64 %3
  %elem = load i16, ptr %elem_ptr, align 2
  %arr_ptr3 = load ptr, ptr %arr1, align 8
  %4 = load i64, ptr %i, align 4
  %addtmp = add i64 %4, 1
  %elem_ptr4 = getelementptr i16, ptr %arr_ptr3, i64 %addtmp
  %elem5 = load i16, ptr %elem_ptr4, align 2
  %gttmp = icmp sgt i16 %elem, %elem5
  br i1 %gttmp, label %then6, label %ifcont11

then6:                                            ; preds = %while_body
  %auto_wrap_result7 = alloca %result_int8, align 8
  %err_ptr8 = getelementptr inbounds %result_int8, ptr %auto_wrap_result7, i32 0, i32 0
  store i8 0, ptr %err_ptr8, align 1
  %val_ptr9 = getelementptr inbounds %result_int8, ptr %auto_wrap_result7, i32 0, i32 1
  store i8 0, ptr %val_ptr9, align 1
  %result_val10 = load %result_int8, ptr %auto_wrap_result7, align 1
  ret %result_int8 %result_val10

ifcont11:                                         ; preds = %while_body
  %5 = load i64, ptr %i, align 4
  %addtmp12 = add i64 %5, 1
  store i64 %addtmp12, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %auto_wrap_result13 = alloca %result_int8, align 8
  %err_ptr14 = getelementptr inbounds %result_int8, ptr %auto_wrap_result13, i32 0, i32 0
  store i8 0, ptr %err_ptr14, align 1
  %val_ptr15 = getelementptr inbounds %result_int8, ptr %auto_wrap_result13, i32 0, i32 1
  store i8 1, ptr %val_ptr15, align 1
  %result_val16 = load %result_int8, ptr %auto_wrap_result13, align 1
  ret %result_int8 %result_val16
}

define internal %result_int8 @is_sorted_int32(ptr %arr, i64 %length) {
entry:
  %i = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %0 = load i64, ptr %length2, align 4
  %letmp = icmp sle i64 %0, 1
  br i1 %letmp, label %then, label %ifcont

then:                                             ; preds = %entry
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 1, ptr %val_ptr, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val

ifcont:                                           ; preds = %entry
  store i64 0, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont11, %ifcont
  %1 = load i64, ptr %i, align 4
  %2 = load i64, ptr %length2, align 4
  %subtmp = sub i64 %2, 1
  %lttmp = icmp slt i64 %1, %subtmp
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr = load ptr, ptr %arr1, align 8
  %3 = load i64, ptr %i, align 4
  %elem_ptr = getelementptr i32, ptr %arr_ptr, i64 %3
  %elem = load i32, ptr %elem_ptr, align 4
  %arr_ptr3 = load ptr, ptr %arr1, align 8
  %4 = load i64, ptr %i, align 4
  %addtmp = add i64 %4, 1
  %elem_ptr4 = getelementptr i32, ptr %arr_ptr3, i64 %addtmp
  %elem5 = load i32, ptr %elem_ptr4, align 4
  %gttmp = icmp sgt i32 %elem, %elem5
  br i1 %gttmp, label %then6, label %ifcont11

then6:                                            ; preds = %while_body
  %auto_wrap_result7 = alloca %result_int8, align 8
  %err_ptr8 = getelementptr inbounds %result_int8, ptr %auto_wrap_result7, i32 0, i32 0
  store i8 0, ptr %err_ptr8, align 1
  %val_ptr9 = getelementptr inbounds %result_int8, ptr %auto_wrap_result7, i32 0, i32 1
  store i8 0, ptr %val_ptr9, align 1
  %result_val10 = load %result_int8, ptr %auto_wrap_result7, align 1
  ret %result_int8 %result_val10

ifcont11:                                         ; preds = %while_body
  %5 = load i64, ptr %i, align 4
  %addtmp12 = add i64 %5, 1
  store i64 %addtmp12, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %auto_wrap_result13 = alloca %result_int8, align 8
  %err_ptr14 = getelementptr inbounds %result_int8, ptr %auto_wrap_result13, i32 0, i32 0
  store i8 0, ptr %err_ptr14, align 1
  %val_ptr15 = getelementptr inbounds %result_int8, ptr %auto_wrap_result13, i32 0, i32 1
  store i8 1, ptr %val_ptr15, align 1
  %result_val16 = load %result_int8, ptr %auto_wrap_result13, align 1
  ret %result_int8 %result_val16
}

define internal %result_int8 @is_sorted_int64(ptr %arr, i64 %length) {
entry:
  %i = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %0 = load i64, ptr %length2, align 4
  %letmp = icmp sle i64 %0, 1
  br i1 %letmp, label %then, label %ifcont

then:                                             ; preds = %entry
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 1, ptr %val_ptr, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val

ifcont:                                           ; preds = %entry
  store i64 0, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont11, %ifcont
  %1 = load i64, ptr %i, align 4
  %2 = load i64, ptr %length2, align 4
  %subtmp = sub i64 %2, 1
  %lttmp = icmp slt i64 %1, %subtmp
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr = load ptr, ptr %arr1, align 8
  %3 = load i64, ptr %i, align 4
  %elem_ptr = getelementptr i64, ptr %arr_ptr, i64 %3
  %elem = load i64, ptr %elem_ptr, align 4
  %arr_ptr3 = load ptr, ptr %arr1, align 8
  %4 = load i64, ptr %i, align 4
  %addtmp = add i64 %4, 1
  %elem_ptr4 = getelementptr i64, ptr %arr_ptr3, i64 %addtmp
  %elem5 = load i64, ptr %elem_ptr4, align 4
  %gttmp = icmp sgt i64 %elem, %elem5
  br i1 %gttmp, label %then6, label %ifcont11

then6:                                            ; preds = %while_body
  %auto_wrap_result7 = alloca %result_int8, align 8
  %err_ptr8 = getelementptr inbounds %result_int8, ptr %auto_wrap_result7, i32 0, i32 0
  store i8 0, ptr %err_ptr8, align 1
  %val_ptr9 = getelementptr inbounds %result_int8, ptr %auto_wrap_result7, i32 0, i32 1
  store i8 0, ptr %val_ptr9, align 1
  %result_val10 = load %result_int8, ptr %auto_wrap_result7, align 1
  ret %result_int8 %result_val10

ifcont11:                                         ; preds = %while_body
  %5 = load i64, ptr %i, align 4
  %addtmp12 = add i64 %5, 1
  store i64 %addtmp12, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %auto_wrap_result13 = alloca %result_int8, align 8
  %err_ptr14 = getelementptr inbounds %result_int8, ptr %auto_wrap_result13, i32 0, i32 0
  store i8 0, ptr %err_ptr14, align 1
  %val_ptr15 = getelementptr inbounds %result_int8, ptr %auto_wrap_result13, i32 0, i32 1
  store i8 1, ptr %val_ptr15, align 1
  %result_val16 = load %result_int8, ptr %auto_wrap_result13, align 1
  ret %result_int8 %result_val16
}

define internal %result_int8 @is_sorted_desc_int8(ptr %arr, i64 %length) {
entry:
  %i = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %0 = load i64, ptr %length2, align 4
  %letmp = icmp sle i64 %0, 1
  br i1 %letmp, label %then, label %ifcont

then:                                             ; preds = %entry
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 1, ptr %val_ptr, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val

ifcont:                                           ; preds = %entry
  store i64 0, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont12, %ifcont
  %1 = load i64, ptr %i, align 4
  %2 = load i64, ptr %length2, align 4
  %subtmp = sub i64 %2, 1
  %lttmp = icmp slt i64 %1, %subtmp
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr = load ptr, ptr %arr1, align 8
  %3 = load i64, ptr %i, align 4
  %elem_ptr = getelementptr i8, ptr %arr_ptr, i64 %3
  %elem = load i8, ptr %elem_ptr, align 1
  %arr_ptr3 = load ptr, ptr %arr1, align 8
  %4 = load i64, ptr %i, align 4
  %addtmp = add i64 %4, 1
  %elem_ptr4 = getelementptr i8, ptr %arr_ptr3, i64 %addtmp
  %elem5 = load i8, ptr %elem_ptr4, align 1
  %lttmp6 = icmp slt i8 %elem, %elem5
  br i1 %lttmp6, label %then7, label %ifcont12

then7:                                            ; preds = %while_body
  %auto_wrap_result8 = alloca %result_int8, align 8
  %err_ptr9 = getelementptr inbounds %result_int8, ptr %auto_wrap_result8, i32 0, i32 0
  store i8 0, ptr %err_ptr9, align 1
  %val_ptr10 = getelementptr inbounds %result_int8, ptr %auto_wrap_result8, i32 0, i32 1
  store i8 0, ptr %val_ptr10, align 1
  %result_val11 = load %result_int8, ptr %auto_wrap_result8, align 1
  ret %result_int8 %result_val11

ifcont12:                                         ; preds = %while_body
  %5 = load i64, ptr %i, align 4
  %addtmp13 = add i64 %5, 1
  store i64 %addtmp13, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %auto_wrap_result14 = alloca %result_int8, align 8
  %err_ptr15 = getelementptr inbounds %result_int8, ptr %auto_wrap_result14, i32 0, i32 0
  store i8 0, ptr %err_ptr15, align 1
  %val_ptr16 = getelementptr inbounds %result_int8, ptr %auto_wrap_result14, i32 0, i32 1
  store i8 1, ptr %val_ptr16, align 1
  %result_val17 = load %result_int8, ptr %auto_wrap_result14, align 1
  ret %result_int8 %result_val17
}

define internal %result_int8 @is_sorted_desc_int16(ptr %arr, i64 %length) {
entry:
  %i = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %0 = load i64, ptr %length2, align 4
  %letmp = icmp sle i64 %0, 1
  br i1 %letmp, label %then, label %ifcont

then:                                             ; preds = %entry
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 1, ptr %val_ptr, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val

ifcont:                                           ; preds = %entry
  store i64 0, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont12, %ifcont
  %1 = load i64, ptr %i, align 4
  %2 = load i64, ptr %length2, align 4
  %subtmp = sub i64 %2, 1
  %lttmp = icmp slt i64 %1, %subtmp
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr = load ptr, ptr %arr1, align 8
  %3 = load i64, ptr %i, align 4
  %elem_ptr = getelementptr i16, ptr %arr_ptr, i64 %3
  %elem = load i16, ptr %elem_ptr, align 2
  %arr_ptr3 = load ptr, ptr %arr1, align 8
  %4 = load i64, ptr %i, align 4
  %addtmp = add i64 %4, 1
  %elem_ptr4 = getelementptr i16, ptr %arr_ptr3, i64 %addtmp
  %elem5 = load i16, ptr %elem_ptr4, align 2
  %lttmp6 = icmp slt i16 %elem, %elem5
  br i1 %lttmp6, label %then7, label %ifcont12

then7:                                            ; preds = %while_body
  %auto_wrap_result8 = alloca %result_int8, align 8
  %err_ptr9 = getelementptr inbounds %result_int8, ptr %auto_wrap_result8, i32 0, i32 0
  store i8 0, ptr %err_ptr9, align 1
  %val_ptr10 = getelementptr inbounds %result_int8, ptr %auto_wrap_result8, i32 0, i32 1
  store i8 0, ptr %val_ptr10, align 1
  %result_val11 = load %result_int8, ptr %auto_wrap_result8, align 1
  ret %result_int8 %result_val11

ifcont12:                                         ; preds = %while_body
  %5 = load i64, ptr %i, align 4
  %addtmp13 = add i64 %5, 1
  store i64 %addtmp13, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %auto_wrap_result14 = alloca %result_int8, align 8
  %err_ptr15 = getelementptr inbounds %result_int8, ptr %auto_wrap_result14, i32 0, i32 0
  store i8 0, ptr %err_ptr15, align 1
  %val_ptr16 = getelementptr inbounds %result_int8, ptr %auto_wrap_result14, i32 0, i32 1
  store i8 1, ptr %val_ptr16, align 1
  %result_val17 = load %result_int8, ptr %auto_wrap_result14, align 1
  ret %result_int8 %result_val17
}

define internal %result_int8 @is_sorted_desc_int32(ptr %arr, i64 %length) {
entry:
  %i = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %0 = load i64, ptr %length2, align 4
  %letmp = icmp sle i64 %0, 1
  br i1 %letmp, label %then, label %ifcont

then:                                             ; preds = %entry
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 1, ptr %val_ptr, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val

ifcont:                                           ; preds = %entry
  store i64 0, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont12, %ifcont
  %1 = load i64, ptr %i, align 4
  %2 = load i64, ptr %length2, align 4
  %subtmp = sub i64 %2, 1
  %lttmp = icmp slt i64 %1, %subtmp
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr = load ptr, ptr %arr1, align 8
  %3 = load i64, ptr %i, align 4
  %elem_ptr = getelementptr i32, ptr %arr_ptr, i64 %3
  %elem = load i32, ptr %elem_ptr, align 4
  %arr_ptr3 = load ptr, ptr %arr1, align 8
  %4 = load i64, ptr %i, align 4
  %addtmp = add i64 %4, 1
  %elem_ptr4 = getelementptr i32, ptr %arr_ptr3, i64 %addtmp
  %elem5 = load i32, ptr %elem_ptr4, align 4
  %lttmp6 = icmp slt i32 %elem, %elem5
  br i1 %lttmp6, label %then7, label %ifcont12

then7:                                            ; preds = %while_body
  %auto_wrap_result8 = alloca %result_int8, align 8
  %err_ptr9 = getelementptr inbounds %result_int8, ptr %auto_wrap_result8, i32 0, i32 0
  store i8 0, ptr %err_ptr9, align 1
  %val_ptr10 = getelementptr inbounds %result_int8, ptr %auto_wrap_result8, i32 0, i32 1
  store i8 0, ptr %val_ptr10, align 1
  %result_val11 = load %result_int8, ptr %auto_wrap_result8, align 1
  ret %result_int8 %result_val11

ifcont12:                                         ; preds = %while_body
  %5 = load i64, ptr %i, align 4
  %addtmp13 = add i64 %5, 1
  store i64 %addtmp13, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %auto_wrap_result14 = alloca %result_int8, align 8
  %err_ptr15 = getelementptr inbounds %result_int8, ptr %auto_wrap_result14, i32 0, i32 0
  store i8 0, ptr %err_ptr15, align 1
  %val_ptr16 = getelementptr inbounds %result_int8, ptr %auto_wrap_result14, i32 0, i32 1
  store i8 1, ptr %val_ptr16, align 1
  %result_val17 = load %result_int8, ptr %auto_wrap_result14, align 1
  ret %result_int8 %result_val17
}

define internal %result_int8 @is_sorted_desc_int64(ptr %arr, i64 %length) {
entry:
  %i = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %0 = load i64, ptr %length2, align 4
  %letmp = icmp sle i64 %0, 1
  br i1 %letmp, label %then, label %ifcont

then:                                             ; preds = %entry
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 1, ptr %val_ptr, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val

ifcont:                                           ; preds = %entry
  store i64 0, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont12, %ifcont
  %1 = load i64, ptr %i, align 4
  %2 = load i64, ptr %length2, align 4
  %subtmp = sub i64 %2, 1
  %lttmp = icmp slt i64 %1, %subtmp
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr = load ptr, ptr %arr1, align 8
  %3 = load i64, ptr %i, align 4
  %elem_ptr = getelementptr i64, ptr %arr_ptr, i64 %3
  %elem = load i64, ptr %elem_ptr, align 4
  %arr_ptr3 = load ptr, ptr %arr1, align 8
  %4 = load i64, ptr %i, align 4
  %addtmp = add i64 %4, 1
  %elem_ptr4 = getelementptr i64, ptr %arr_ptr3, i64 %addtmp
  %elem5 = load i64, ptr %elem_ptr4, align 4
  %lttmp6 = icmp slt i64 %elem, %elem5
  br i1 %lttmp6, label %then7, label %ifcont12

then7:                                            ; preds = %while_body
  %auto_wrap_result8 = alloca %result_int8, align 8
  %err_ptr9 = getelementptr inbounds %result_int8, ptr %auto_wrap_result8, i32 0, i32 0
  store i8 0, ptr %err_ptr9, align 1
  %val_ptr10 = getelementptr inbounds %result_int8, ptr %auto_wrap_result8, i32 0, i32 1
  store i8 0, ptr %val_ptr10, align 1
  %result_val11 = load %result_int8, ptr %auto_wrap_result8, align 1
  ret %result_int8 %result_val11

ifcont12:                                         ; preds = %while_body
  %5 = load i64, ptr %i, align 4
  %addtmp13 = add i64 %5, 1
  store i64 %addtmp13, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %auto_wrap_result14 = alloca %result_int8, align 8
  %err_ptr15 = getelementptr inbounds %result_int8, ptr %auto_wrap_result14, i32 0, i32 0
  store i8 0, ptr %err_ptr15, align 1
  %val_ptr16 = getelementptr inbounds %result_int8, ptr %auto_wrap_result14, i32 0, i32 1
  store i8 1, ptr %val_ptr16, align 1
  %result_val17 = load %result_int8, ptr %auto_wrap_result14, align 1
  ret %result_int8 %result_val17
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

define internal %result_int64 @lastIndexOf_uint8(ptr %arr, i64 %length, i8 %value) {
entry:
  %i = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %value3 = alloca i8, align 1
  store i8 %value, ptr %value3, align 1
  %0 = load i64, ptr %length2, align 4
  %subtmp = sub i64 %0, 1
  store i64 %subtmp, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont, %entry
  %1 = load i64, ptr %i, align 4
  %getmp = icmp sge i64 %1, 0
  br i1 %getmp, label %while_body, label %while_exit

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
  %subtmp4 = sub i64 %5, 1
  store i64 %subtmp4, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %auto_wrap_result5 = alloca %result_int64, align 8
  %err_ptr6 = getelementptr inbounds %result_int64, ptr %auto_wrap_result5, i32 0, i32 0
  store i8 0, ptr %err_ptr6, align 1
  %val_ptr7 = getelementptr inbounds %result_int64, ptr %auto_wrap_result5, i32 0, i32 1
  store i64 -1, ptr %val_ptr7, align 4
  %result_val8 = load %result_int64, ptr %auto_wrap_result5, align 4
  ret %result_int64 %result_val8
}

define internal %result_int64 @lastIndexOf_uint16(ptr %arr, i64 %length, i16 %value) {
entry:
  %i = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %value3 = alloca i16, align 2
  store i16 %value, ptr %value3, align 2
  %0 = load i64, ptr %length2, align 4
  %subtmp = sub i64 %0, 1
  store i64 %subtmp, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont, %entry
  %1 = load i64, ptr %i, align 4
  %getmp = icmp sge i64 %1, 0
  br i1 %getmp, label %while_body, label %while_exit

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
  %subtmp4 = sub i64 %5, 1
  store i64 %subtmp4, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %auto_wrap_result5 = alloca %result_int64, align 8
  %err_ptr6 = getelementptr inbounds %result_int64, ptr %auto_wrap_result5, i32 0, i32 0
  store i8 0, ptr %err_ptr6, align 1
  %val_ptr7 = getelementptr inbounds %result_int64, ptr %auto_wrap_result5, i32 0, i32 1
  store i64 -1, ptr %val_ptr7, align 4
  %result_val8 = load %result_int64, ptr %auto_wrap_result5, align 4
  ret %result_int64 %result_val8
}

define internal %result_int64 @lastIndexOf_uint32(ptr %arr, i64 %length, i32 %value) {
entry:
  %i = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %value3 = alloca i32, align 4
  store i32 %value, ptr %value3, align 4
  %0 = load i64, ptr %length2, align 4
  %subtmp = sub i64 %0, 1
  store i64 %subtmp, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont, %entry
  %1 = load i64, ptr %i, align 4
  %getmp = icmp sge i64 %1, 0
  br i1 %getmp, label %while_body, label %while_exit

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
  %subtmp4 = sub i64 %5, 1
  store i64 %subtmp4, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %auto_wrap_result5 = alloca %result_int64, align 8
  %err_ptr6 = getelementptr inbounds %result_int64, ptr %auto_wrap_result5, i32 0, i32 0
  store i8 0, ptr %err_ptr6, align 1
  %val_ptr7 = getelementptr inbounds %result_int64, ptr %auto_wrap_result5, i32 0, i32 1
  store i64 -1, ptr %val_ptr7, align 4
  %result_val8 = load %result_int64, ptr %auto_wrap_result5, align 4
  ret %result_int64 %result_val8
}

define internal %result_int64 @lastIndexOf_uint64(ptr %arr, i64 %length, i64 %value) {
entry:
  %i = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %value3 = alloca i64, align 8
  store i64 %value, ptr %value3, align 4
  %0 = load i64, ptr %length2, align 4
  %subtmp = sub i64 %0, 1
  store i64 %subtmp, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont, %entry
  %1 = load i64, ptr %i, align 4
  %getmp = icmp sge i64 %1, 0
  br i1 %getmp, label %while_body, label %while_exit

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
  %subtmp4 = sub i64 %5, 1
  store i64 %subtmp4, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %auto_wrap_result5 = alloca %result_int64, align 8
  %err_ptr6 = getelementptr inbounds %result_int64, ptr %auto_wrap_result5, i32 0, i32 0
  store i8 0, ptr %err_ptr6, align 1
  %val_ptr7 = getelementptr inbounds %result_int64, ptr %auto_wrap_result5, i32 0, i32 1
  store i64 -1, ptr %val_ptr7, align 4
  %result_val8 = load %result_int64, ptr %auto_wrap_result5, align 4
  ret %result_int64 %result_val8
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

define internal %result_int8 @reverse_uint8(ptr %arr, i64 %length) {
entry:
  %temp = alloca i8, align 1
  %right = alloca i64, align 8
  %left = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  store i64 0, ptr %left, align 4
  %0 = load i64, ptr %length2, align 4
  %subtmp = sub i64 %0, 1
  store i64 %subtmp, ptr %right, align 4
  br label %while_cond

while_cond:                                       ; preds = %while_body, %entry
  %1 = load i64, ptr %left, align 4
  %2 = load i64, ptr %right, align 4
  %lttmp = icmp slt i64 %1, %2
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr = load ptr, ptr %arr1, align 8
  %3 = load i64, ptr %left, align 4
  %elem_ptr = getelementptr i8, ptr %arr_ptr, i64 %3
  %elem = load i8, ptr %elem_ptr, align 1
  store i8 %elem, ptr %temp, align 1
  %arr_ptr3 = load ptr, ptr %arr1, align 8
  %4 = load i64, ptr %left, align 4
  %arr_ptr4 = load ptr, ptr %arr1, align 8
  %5 = load i64, ptr %right, align 4
  %elem_ptr5 = getelementptr i8, ptr %arr_ptr4, i64 %5
  %elem6 = load i8, ptr %elem_ptr5, align 1
  %elem_ptr7 = getelementptr i8, ptr %arr_ptr3, i64 %4
  store i8 %elem6, ptr %elem_ptr7, align 1
  %arr_ptr8 = load ptr, ptr %arr1, align 8
  %6 = load i64, ptr %right, align 4
  %7 = load i8, ptr %temp, align 1
  %elem_ptr9 = getelementptr i8, ptr %arr_ptr8, i64 %6
  store i8 %7, ptr %elem_ptr9, align 1
  %8 = load i64, ptr %left, align 4
  %addtmp = add i64 %8, 1
  store i64 %addtmp, ptr %left, align 4
  %9 = load i64, ptr %right, align 4
  %subtmp10 = sub i64 %9, 1
  store i64 %subtmp10, ptr %right, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 1, ptr %val_ptr, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val
}

define internal %result_int8 @reverse_uint16(ptr %arr, i64 %length) {
entry:
  %temp = alloca i16, align 2
  %right = alloca i64, align 8
  %left = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  store i64 0, ptr %left, align 4
  %0 = load i64, ptr %length2, align 4
  %subtmp = sub i64 %0, 1
  store i64 %subtmp, ptr %right, align 4
  br label %while_cond

while_cond:                                       ; preds = %while_body, %entry
  %1 = load i64, ptr %left, align 4
  %2 = load i64, ptr %right, align 4
  %lttmp = icmp slt i64 %1, %2
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr = load ptr, ptr %arr1, align 8
  %3 = load i64, ptr %left, align 4
  %elem_ptr = getelementptr i16, ptr %arr_ptr, i64 %3
  %elem = load i16, ptr %elem_ptr, align 2
  store i16 %elem, ptr %temp, align 2
  %arr_ptr3 = load ptr, ptr %arr1, align 8
  %4 = load i64, ptr %left, align 4
  %arr_ptr4 = load ptr, ptr %arr1, align 8
  %5 = load i64, ptr %right, align 4
  %elem_ptr5 = getelementptr i16, ptr %arr_ptr4, i64 %5
  %elem6 = load i16, ptr %elem_ptr5, align 2
  %elem_ptr7 = getelementptr i16, ptr %arr_ptr3, i64 %4
  store i16 %elem6, ptr %elem_ptr7, align 2
  %arr_ptr8 = load ptr, ptr %arr1, align 8
  %6 = load i64, ptr %right, align 4
  %7 = load i16, ptr %temp, align 2
  %elem_ptr9 = getelementptr i16, ptr %arr_ptr8, i64 %6
  store i16 %7, ptr %elem_ptr9, align 2
  %8 = load i64, ptr %left, align 4
  %addtmp = add i64 %8, 1
  store i64 %addtmp, ptr %left, align 4
  %9 = load i64, ptr %right, align 4
  %subtmp10 = sub i64 %9, 1
  store i64 %subtmp10, ptr %right, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 1, ptr %val_ptr, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val
}

define internal %result_int8 @reverse_uint32(ptr %arr, i64 %length) {
entry:
  %temp = alloca i32, align 4
  %right = alloca i64, align 8
  %left = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  store i64 0, ptr %left, align 4
  %0 = load i64, ptr %length2, align 4
  %subtmp = sub i64 %0, 1
  store i64 %subtmp, ptr %right, align 4
  br label %while_cond

while_cond:                                       ; preds = %while_body, %entry
  %1 = load i64, ptr %left, align 4
  %2 = load i64, ptr %right, align 4
  %lttmp = icmp slt i64 %1, %2
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr = load ptr, ptr %arr1, align 8
  %3 = load i64, ptr %left, align 4
  %elem_ptr = getelementptr i32, ptr %arr_ptr, i64 %3
  %elem = load i32, ptr %elem_ptr, align 4
  store i32 %elem, ptr %temp, align 4
  %arr_ptr3 = load ptr, ptr %arr1, align 8
  %4 = load i64, ptr %left, align 4
  %arr_ptr4 = load ptr, ptr %arr1, align 8
  %5 = load i64, ptr %right, align 4
  %elem_ptr5 = getelementptr i32, ptr %arr_ptr4, i64 %5
  %elem6 = load i32, ptr %elem_ptr5, align 4
  %elem_ptr7 = getelementptr i32, ptr %arr_ptr3, i64 %4
  store i32 %elem6, ptr %elem_ptr7, align 4
  %arr_ptr8 = load ptr, ptr %arr1, align 8
  %6 = load i64, ptr %right, align 4
  %7 = load i32, ptr %temp, align 4
  %elem_ptr9 = getelementptr i32, ptr %arr_ptr8, i64 %6
  store i32 %7, ptr %elem_ptr9, align 4
  %8 = load i64, ptr %left, align 4
  %addtmp = add i64 %8, 1
  store i64 %addtmp, ptr %left, align 4
  %9 = load i64, ptr %right, align 4
  %subtmp10 = sub i64 %9, 1
  store i64 %subtmp10, ptr %right, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 1, ptr %val_ptr, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val
}

define internal %result_int8 @reverse_uint64(ptr %arr, i64 %length) {
entry:
  %temp = alloca i64, align 8
  %right = alloca i64, align 8
  %left = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  store i64 0, ptr %left, align 4
  %0 = load i64, ptr %length2, align 4
  %subtmp = sub i64 %0, 1
  store i64 %subtmp, ptr %right, align 4
  br label %while_cond

while_cond:                                       ; preds = %while_body, %entry
  %1 = load i64, ptr %left, align 4
  %2 = load i64, ptr %right, align 4
  %lttmp = icmp slt i64 %1, %2
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr = load ptr, ptr %arr1, align 8
  %3 = load i64, ptr %left, align 4
  %elem_ptr = getelementptr i64, ptr %arr_ptr, i64 %3
  %elem = load i64, ptr %elem_ptr, align 4
  store i64 %elem, ptr %temp, align 4
  %arr_ptr3 = load ptr, ptr %arr1, align 8
  %4 = load i64, ptr %left, align 4
  %arr_ptr4 = load ptr, ptr %arr1, align 8
  %5 = load i64, ptr %right, align 4
  %elem_ptr5 = getelementptr i64, ptr %arr_ptr4, i64 %5
  %elem6 = load i64, ptr %elem_ptr5, align 4
  %elem_ptr7 = getelementptr i64, ptr %arr_ptr3, i64 %4
  store i64 %elem6, ptr %elem_ptr7, align 4
  %arr_ptr8 = load ptr, ptr %arr1, align 8
  %6 = load i64, ptr %right, align 4
  %7 = load i64, ptr %temp, align 4
  %elem_ptr9 = getelementptr i64, ptr %arr_ptr8, i64 %6
  store i64 %7, ptr %elem_ptr9, align 4
  %8 = load i64, ptr %left, align 4
  %addtmp = add i64 %8, 1
  store i64 %addtmp, ptr %left, align 4
  %9 = load i64, ptr %right, align 4
  %subtmp10 = sub i64 %9, 1
  store i64 %subtmp10, ptr %right, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 1, ptr %val_ptr, align 1
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
  store i8 1, ptr %val_ptr, align 1
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
  store i8 1, ptr %val_ptr, align 1
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
  store i8 1, ptr %val_ptr, align 1
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
  store i8 1, ptr %val_ptr, align 1
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
  store i8 1, ptr %val_ptr, align 1
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
  store i8 1, ptr %val_ptr, align 1
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
  store i8 1, ptr %val_ptr, align 1
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
  store i8 1, ptr %val_ptr, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val
}

define internal %result_int8 @equals_uint8(ptr %arr1, ptr %arr2, i64 %length) {
entry:
  %i = alloca i64, align 8
  %arr11 = alloca ptr, align 8
  store ptr %arr1, ptr %arr11, align 8
  %arr22 = alloca ptr, align 8
  store ptr %arr2, ptr %arr22, align 8
  %length3 = alloca i64, align 8
  store i64 %length, ptr %length3, align 4
  store i64 0, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont, %entry
  %0 = load i64, ptr %i, align 4
  %1 = load i64, ptr %length3, align 4
  %lttmp = icmp slt i64 %0, %1
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr1_ptr = load ptr, ptr %arr11, align 8
  %2 = load i64, ptr %i, align 4
  %elem_ptr = getelementptr i8, ptr %arr1_ptr, i64 %2
  %elem = load i8, ptr %elem_ptr, align 1
  %arr2_ptr = load ptr, ptr %arr22, align 8
  %3 = load i64, ptr %i, align 4
  %elem_ptr4 = getelementptr i8, ptr %arr2_ptr, i64 %3
  %elem5 = load i8, ptr %elem_ptr4, align 1
  %netmp = icmp ne i8 %elem, %elem5
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
  %auto_wrap_result6 = alloca %result_int8, align 8
  %err_ptr7 = getelementptr inbounds %result_int8, ptr %auto_wrap_result6, i32 0, i32 0
  store i8 0, ptr %err_ptr7, align 1
  %val_ptr8 = getelementptr inbounds %result_int8, ptr %auto_wrap_result6, i32 0, i32 1
  store i8 1, ptr %val_ptr8, align 1
  %result_val9 = load %result_int8, ptr %auto_wrap_result6, align 1
  ret %result_int8 %result_val9
}

define internal %result_int8 @equals_uint16(ptr %arr1, ptr %arr2, i64 %length) {
entry:
  %i = alloca i64, align 8
  %arr11 = alloca ptr, align 8
  store ptr %arr1, ptr %arr11, align 8
  %arr22 = alloca ptr, align 8
  store ptr %arr2, ptr %arr22, align 8
  %length3 = alloca i64, align 8
  store i64 %length, ptr %length3, align 4
  store i64 0, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont, %entry
  %0 = load i64, ptr %i, align 4
  %1 = load i64, ptr %length3, align 4
  %lttmp = icmp slt i64 %0, %1
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr1_ptr = load ptr, ptr %arr11, align 8
  %2 = load i64, ptr %i, align 4
  %elem_ptr = getelementptr i16, ptr %arr1_ptr, i64 %2
  %elem = load i16, ptr %elem_ptr, align 2
  %arr2_ptr = load ptr, ptr %arr22, align 8
  %3 = load i64, ptr %i, align 4
  %elem_ptr4 = getelementptr i16, ptr %arr2_ptr, i64 %3
  %elem5 = load i16, ptr %elem_ptr4, align 2
  %netmp = icmp ne i16 %elem, %elem5
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
  %auto_wrap_result6 = alloca %result_int8, align 8
  %err_ptr7 = getelementptr inbounds %result_int8, ptr %auto_wrap_result6, i32 0, i32 0
  store i8 0, ptr %err_ptr7, align 1
  %val_ptr8 = getelementptr inbounds %result_int8, ptr %auto_wrap_result6, i32 0, i32 1
  store i8 1, ptr %val_ptr8, align 1
  %result_val9 = load %result_int8, ptr %auto_wrap_result6, align 1
  ret %result_int8 %result_val9
}

define internal %result_int8 @equals_uint32(ptr %arr1, ptr %arr2, i64 %length) {
entry:
  %i = alloca i64, align 8
  %arr11 = alloca ptr, align 8
  store ptr %arr1, ptr %arr11, align 8
  %arr22 = alloca ptr, align 8
  store ptr %arr2, ptr %arr22, align 8
  %length3 = alloca i64, align 8
  store i64 %length, ptr %length3, align 4
  store i64 0, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont, %entry
  %0 = load i64, ptr %i, align 4
  %1 = load i64, ptr %length3, align 4
  %lttmp = icmp slt i64 %0, %1
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr1_ptr = load ptr, ptr %arr11, align 8
  %2 = load i64, ptr %i, align 4
  %elem_ptr = getelementptr i32, ptr %arr1_ptr, i64 %2
  %elem = load i32, ptr %elem_ptr, align 4
  %arr2_ptr = load ptr, ptr %arr22, align 8
  %3 = load i64, ptr %i, align 4
  %elem_ptr4 = getelementptr i32, ptr %arr2_ptr, i64 %3
  %elem5 = load i32, ptr %elem_ptr4, align 4
  %netmp = icmp ne i32 %elem, %elem5
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
  %auto_wrap_result6 = alloca %result_int8, align 8
  %err_ptr7 = getelementptr inbounds %result_int8, ptr %auto_wrap_result6, i32 0, i32 0
  store i8 0, ptr %err_ptr7, align 1
  %val_ptr8 = getelementptr inbounds %result_int8, ptr %auto_wrap_result6, i32 0, i32 1
  store i8 1, ptr %val_ptr8, align 1
  %result_val9 = load %result_int8, ptr %auto_wrap_result6, align 1
  ret %result_int8 %result_val9
}

define internal %result_int8 @equals_uint64(ptr %arr1, ptr %arr2, i64 %length) {
entry:
  %i = alloca i64, align 8
  %arr11 = alloca ptr, align 8
  store ptr %arr1, ptr %arr11, align 8
  %arr22 = alloca ptr, align 8
  store ptr %arr2, ptr %arr22, align 8
  %length3 = alloca i64, align 8
  store i64 %length, ptr %length3, align 4
  store i64 0, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont, %entry
  %0 = load i64, ptr %i, align 4
  %1 = load i64, ptr %length3, align 4
  %lttmp = icmp slt i64 %0, %1
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr1_ptr = load ptr, ptr %arr11, align 8
  %2 = load i64, ptr %i, align 4
  %elem_ptr = getelementptr i64, ptr %arr1_ptr, i64 %2
  %elem = load i64, ptr %elem_ptr, align 4
  %arr2_ptr = load ptr, ptr %arr22, align 8
  %3 = load i64, ptr %i, align 4
  %elem_ptr4 = getelementptr i64, ptr %arr2_ptr, i64 %3
  %elem5 = load i64, ptr %elem_ptr4, align 4
  %netmp = icmp ne i64 %elem, %elem5
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
  %auto_wrap_result6 = alloca %result_int8, align 8
  %err_ptr7 = getelementptr inbounds %result_int8, ptr %auto_wrap_result6, i32 0, i32 0
  store i8 0, ptr %err_ptr7, align 1
  %val_ptr8 = getelementptr inbounds %result_int8, ptr %auto_wrap_result6, i32 0, i32 1
  store i8 1, ptr %val_ptr8, align 1
  %result_val9 = load %result_int8, ptr %auto_wrap_result6, align 1
  ret %result_int8 %result_val9
}

define internal %result_uint8 @min_in_array_uint8(ptr %arr, i64 %length) {
entry:
  %i = alloca i64, align 8
  %min_val = alloca i8, align 1
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %0 = load i64, ptr %length2, align 4
  %letmp = icmp sle i64 %0, 0
  br i1 %letmp, label %then, label %ifcont

then:                                             ; preds = %entry
  %arr_ptr = load ptr, ptr %arr1, align 8
  %elem_ptr = getelementptr i8, ptr %arr_ptr, i64 0
  %elem = load i8, ptr %elem_ptr, align 1
  %auto_wrap_result = alloca %result_uint8, align 8
  %err_ptr = getelementptr inbounds %result_uint8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_uint8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 %elem, ptr %val_ptr, align 1
  %result_val = load %result_uint8, ptr %auto_wrap_result, align 1
  ret %result_uint8 %result_val

ifcont:                                           ; preds = %entry
  %arr_ptr3 = load ptr, ptr %arr1, align 8
  %elem_ptr4 = getelementptr i8, ptr %arr_ptr3, i64 0
  %elem5 = load i8, ptr %elem_ptr4, align 1
  store i8 %elem5, ptr %min_val, align 1
  store i64 1, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont14, %ifcont
  %1 = load i64, ptr %i, align 4
  %2 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %1, %2
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr6 = load ptr, ptr %arr1, align 8
  %3 = load i64, ptr %i, align 4
  %elem_ptr7 = getelementptr i8, ptr %arr_ptr6, i64 %3
  %elem8 = load i8, ptr %elem_ptr7, align 1
  %4 = load i8, ptr %min_val, align 1
  %lttmp9 = icmp slt i8 %elem8, %4
  br i1 %lttmp9, label %then10, label %ifcont14

then10:                                           ; preds = %while_body
  %arr_ptr11 = load ptr, ptr %arr1, align 8
  %5 = load i64, ptr %i, align 4
  %elem_ptr12 = getelementptr i8, ptr %arr_ptr11, i64 %5
  %elem13 = load i8, ptr %elem_ptr12, align 1
  store i8 %elem13, ptr %min_val, align 1
  br label %ifcont14

ifcont14:                                         ; preds = %then10, %while_body
  %6 = load i64, ptr %i, align 4
  %addtmp = add i64 %6, 1
  store i64 %addtmp, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %7 = load i8, ptr %min_val, align 1
  %auto_wrap_result15 = alloca %result_uint8, align 8
  %err_ptr16 = getelementptr inbounds %result_uint8, ptr %auto_wrap_result15, i32 0, i32 0
  store i8 0, ptr %err_ptr16, align 1
  %val_ptr17 = getelementptr inbounds %result_uint8, ptr %auto_wrap_result15, i32 0, i32 1
  store i8 %7, ptr %val_ptr17, align 1
  %result_val18 = load %result_uint8, ptr %auto_wrap_result15, align 1
  ret %result_uint8 %result_val18
}

define internal %result_uint16 @min_in_array_uint16(ptr %arr, i64 %length) {
entry:
  %i = alloca i64, align 8
  %min_val = alloca i16, align 2
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %0 = load i64, ptr %length2, align 4
  %letmp = icmp sle i64 %0, 0
  br i1 %letmp, label %then, label %ifcont

then:                                             ; preds = %entry
  %arr_ptr = load ptr, ptr %arr1, align 8
  %elem_ptr = getelementptr i16, ptr %arr_ptr, i64 0
  %elem = load i16, ptr %elem_ptr, align 2
  %auto_wrap_result = alloca %result_uint16, align 8
  %err_ptr = getelementptr inbounds %result_uint16, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_uint16, ptr %auto_wrap_result, i32 0, i32 1
  store i16 %elem, ptr %val_ptr, align 2
  %result_val = load %result_uint16, ptr %auto_wrap_result, align 2
  ret %result_uint16 %result_val

ifcont:                                           ; preds = %entry
  %arr_ptr3 = load ptr, ptr %arr1, align 8
  %elem_ptr4 = getelementptr i16, ptr %arr_ptr3, i64 0
  %elem5 = load i16, ptr %elem_ptr4, align 2
  store i16 %elem5, ptr %min_val, align 2
  store i64 1, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont14, %ifcont
  %1 = load i64, ptr %i, align 4
  %2 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %1, %2
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr6 = load ptr, ptr %arr1, align 8
  %3 = load i64, ptr %i, align 4
  %elem_ptr7 = getelementptr i16, ptr %arr_ptr6, i64 %3
  %elem8 = load i16, ptr %elem_ptr7, align 2
  %4 = load i16, ptr %min_val, align 2
  %lttmp9 = icmp slt i16 %elem8, %4
  br i1 %lttmp9, label %then10, label %ifcont14

then10:                                           ; preds = %while_body
  %arr_ptr11 = load ptr, ptr %arr1, align 8
  %5 = load i64, ptr %i, align 4
  %elem_ptr12 = getelementptr i16, ptr %arr_ptr11, i64 %5
  %elem13 = load i16, ptr %elem_ptr12, align 2
  store i16 %elem13, ptr %min_val, align 2
  br label %ifcont14

ifcont14:                                         ; preds = %then10, %while_body
  %6 = load i64, ptr %i, align 4
  %addtmp = add i64 %6, 1
  store i64 %addtmp, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %7 = load i16, ptr %min_val, align 2
  %auto_wrap_result15 = alloca %result_uint16, align 8
  %err_ptr16 = getelementptr inbounds %result_uint16, ptr %auto_wrap_result15, i32 0, i32 0
  store i8 0, ptr %err_ptr16, align 1
  %val_ptr17 = getelementptr inbounds %result_uint16, ptr %auto_wrap_result15, i32 0, i32 1
  store i16 %7, ptr %val_ptr17, align 2
  %result_val18 = load %result_uint16, ptr %auto_wrap_result15, align 2
  ret %result_uint16 %result_val18
}

define internal %result_uint32 @min_in_array_uint32(ptr %arr, i64 %length) {
entry:
  %i = alloca i64, align 8
  %min_val = alloca i32, align 4
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %0 = load i64, ptr %length2, align 4
  %letmp = icmp sle i64 %0, 0
  br i1 %letmp, label %then, label %ifcont

then:                                             ; preds = %entry
  %arr_ptr = load ptr, ptr %arr1, align 8
  %elem_ptr = getelementptr i32, ptr %arr_ptr, i64 0
  %elem = load i32, ptr %elem_ptr, align 4
  %auto_wrap_result = alloca %result_uint32, align 8
  %err_ptr = getelementptr inbounds %result_uint32, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_uint32, ptr %auto_wrap_result, i32 0, i32 1
  store i32 %elem, ptr %val_ptr, align 4
  %result_val = load %result_uint32, ptr %auto_wrap_result, align 4
  ret %result_uint32 %result_val

ifcont:                                           ; preds = %entry
  %arr_ptr3 = load ptr, ptr %arr1, align 8
  %elem_ptr4 = getelementptr i32, ptr %arr_ptr3, i64 0
  %elem5 = load i32, ptr %elem_ptr4, align 4
  store i32 %elem5, ptr %min_val, align 4
  store i64 1, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont14, %ifcont
  %1 = load i64, ptr %i, align 4
  %2 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %1, %2
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr6 = load ptr, ptr %arr1, align 8
  %3 = load i64, ptr %i, align 4
  %elem_ptr7 = getelementptr i32, ptr %arr_ptr6, i64 %3
  %elem8 = load i32, ptr %elem_ptr7, align 4
  %4 = load i32, ptr %min_val, align 4
  %lttmp9 = icmp slt i32 %elem8, %4
  br i1 %lttmp9, label %then10, label %ifcont14

then10:                                           ; preds = %while_body
  %arr_ptr11 = load ptr, ptr %arr1, align 8
  %5 = load i64, ptr %i, align 4
  %elem_ptr12 = getelementptr i32, ptr %arr_ptr11, i64 %5
  %elem13 = load i32, ptr %elem_ptr12, align 4
  store i32 %elem13, ptr %min_val, align 4
  br label %ifcont14

ifcont14:                                         ; preds = %then10, %while_body
  %6 = load i64, ptr %i, align 4
  %addtmp = add i64 %6, 1
  store i64 %addtmp, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %7 = load i32, ptr %min_val, align 4
  %auto_wrap_result15 = alloca %result_uint32, align 8
  %err_ptr16 = getelementptr inbounds %result_uint32, ptr %auto_wrap_result15, i32 0, i32 0
  store i8 0, ptr %err_ptr16, align 1
  %val_ptr17 = getelementptr inbounds %result_uint32, ptr %auto_wrap_result15, i32 0, i32 1
  store i32 %7, ptr %val_ptr17, align 4
  %result_val18 = load %result_uint32, ptr %auto_wrap_result15, align 4
  ret %result_uint32 %result_val18
}

define internal %result_uint64 @min_in_array_uint64(ptr %arr, i64 %length) {
entry:
  %i = alloca i64, align 8
  %min_val = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %0 = load i64, ptr %length2, align 4
  %letmp = icmp sle i64 %0, 0
  br i1 %letmp, label %then, label %ifcont

then:                                             ; preds = %entry
  %arr_ptr = load ptr, ptr %arr1, align 8
  %elem_ptr = getelementptr i64, ptr %arr_ptr, i64 0
  %elem = load i64, ptr %elem_ptr, align 4
  %auto_wrap_result = alloca %result_uint64, align 8
  %err_ptr = getelementptr inbounds %result_uint64, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_uint64, ptr %auto_wrap_result, i32 0, i32 1
  store i64 %elem, ptr %val_ptr, align 4
  %result_val = load %result_uint64, ptr %auto_wrap_result, align 4
  ret %result_uint64 %result_val

ifcont:                                           ; preds = %entry
  %arr_ptr3 = load ptr, ptr %arr1, align 8
  %elem_ptr4 = getelementptr i64, ptr %arr_ptr3, i64 0
  %elem5 = load i64, ptr %elem_ptr4, align 4
  store i64 %elem5, ptr %min_val, align 4
  store i64 1, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont14, %ifcont
  %1 = load i64, ptr %i, align 4
  %2 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %1, %2
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr6 = load ptr, ptr %arr1, align 8
  %3 = load i64, ptr %i, align 4
  %elem_ptr7 = getelementptr i64, ptr %arr_ptr6, i64 %3
  %elem8 = load i64, ptr %elem_ptr7, align 4
  %4 = load i64, ptr %min_val, align 4
  %lttmp9 = icmp slt i64 %elem8, %4
  br i1 %lttmp9, label %then10, label %ifcont14

then10:                                           ; preds = %while_body
  %arr_ptr11 = load ptr, ptr %arr1, align 8
  %5 = load i64, ptr %i, align 4
  %elem_ptr12 = getelementptr i64, ptr %arr_ptr11, i64 %5
  %elem13 = load i64, ptr %elem_ptr12, align 4
  store i64 %elem13, ptr %min_val, align 4
  br label %ifcont14

ifcont14:                                         ; preds = %then10, %while_body
  %6 = load i64, ptr %i, align 4
  %addtmp = add i64 %6, 1
  store i64 %addtmp, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %7 = load i64, ptr %min_val, align 4
  %auto_wrap_result15 = alloca %result_uint64, align 8
  %err_ptr16 = getelementptr inbounds %result_uint64, ptr %auto_wrap_result15, i32 0, i32 0
  store i8 0, ptr %err_ptr16, align 1
  %val_ptr17 = getelementptr inbounds %result_uint64, ptr %auto_wrap_result15, i32 0, i32 1
  store i64 %7, ptr %val_ptr17, align 4
  %result_val18 = load %result_uint64, ptr %auto_wrap_result15, align 4
  ret %result_uint64 %result_val18
}

define internal %result_uint8 @max_in_array_uint8(ptr %arr, i64 %length) {
entry:
  %i = alloca i64, align 8
  %max_val = alloca i8, align 1
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %0 = load i64, ptr %length2, align 4
  %letmp = icmp sle i64 %0, 0
  br i1 %letmp, label %then, label %ifcont

then:                                             ; preds = %entry
  %arr_ptr = load ptr, ptr %arr1, align 8
  %elem_ptr = getelementptr i8, ptr %arr_ptr, i64 0
  %elem = load i8, ptr %elem_ptr, align 1
  %auto_wrap_result = alloca %result_uint8, align 8
  %err_ptr = getelementptr inbounds %result_uint8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_uint8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 %elem, ptr %val_ptr, align 1
  %result_val = load %result_uint8, ptr %auto_wrap_result, align 1
  ret %result_uint8 %result_val

ifcont:                                           ; preds = %entry
  %arr_ptr3 = load ptr, ptr %arr1, align 8
  %elem_ptr4 = getelementptr i8, ptr %arr_ptr3, i64 0
  %elem5 = load i8, ptr %elem_ptr4, align 1
  store i8 %elem5, ptr %max_val, align 1
  store i64 1, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont13, %ifcont
  %1 = load i64, ptr %i, align 4
  %2 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %1, %2
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr6 = load ptr, ptr %arr1, align 8
  %3 = load i64, ptr %i, align 4
  %elem_ptr7 = getelementptr i8, ptr %arr_ptr6, i64 %3
  %elem8 = load i8, ptr %elem_ptr7, align 1
  %4 = load i8, ptr %max_val, align 1
  %gttmp = icmp sgt i8 %elem8, %4
  br i1 %gttmp, label %then9, label %ifcont13

then9:                                            ; preds = %while_body
  %arr_ptr10 = load ptr, ptr %arr1, align 8
  %5 = load i64, ptr %i, align 4
  %elem_ptr11 = getelementptr i8, ptr %arr_ptr10, i64 %5
  %elem12 = load i8, ptr %elem_ptr11, align 1
  store i8 %elem12, ptr %max_val, align 1
  br label %ifcont13

ifcont13:                                         ; preds = %then9, %while_body
  %6 = load i64, ptr %i, align 4
  %addtmp = add i64 %6, 1
  store i64 %addtmp, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %7 = load i8, ptr %max_val, align 1
  %auto_wrap_result14 = alloca %result_uint8, align 8
  %err_ptr15 = getelementptr inbounds %result_uint8, ptr %auto_wrap_result14, i32 0, i32 0
  store i8 0, ptr %err_ptr15, align 1
  %val_ptr16 = getelementptr inbounds %result_uint8, ptr %auto_wrap_result14, i32 0, i32 1
  store i8 %7, ptr %val_ptr16, align 1
  %result_val17 = load %result_uint8, ptr %auto_wrap_result14, align 1
  ret %result_uint8 %result_val17
}

define internal %result_uint16 @max_in_array_uint16(ptr %arr, i64 %length) {
entry:
  %i = alloca i64, align 8
  %max_val = alloca i16, align 2
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %0 = load i64, ptr %length2, align 4
  %letmp = icmp sle i64 %0, 0
  br i1 %letmp, label %then, label %ifcont

then:                                             ; preds = %entry
  %arr_ptr = load ptr, ptr %arr1, align 8
  %elem_ptr = getelementptr i16, ptr %arr_ptr, i64 0
  %elem = load i16, ptr %elem_ptr, align 2
  %auto_wrap_result = alloca %result_uint16, align 8
  %err_ptr = getelementptr inbounds %result_uint16, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_uint16, ptr %auto_wrap_result, i32 0, i32 1
  store i16 %elem, ptr %val_ptr, align 2
  %result_val = load %result_uint16, ptr %auto_wrap_result, align 2
  ret %result_uint16 %result_val

ifcont:                                           ; preds = %entry
  %arr_ptr3 = load ptr, ptr %arr1, align 8
  %elem_ptr4 = getelementptr i16, ptr %arr_ptr3, i64 0
  %elem5 = load i16, ptr %elem_ptr4, align 2
  store i16 %elem5, ptr %max_val, align 2
  store i64 1, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont13, %ifcont
  %1 = load i64, ptr %i, align 4
  %2 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %1, %2
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr6 = load ptr, ptr %arr1, align 8
  %3 = load i64, ptr %i, align 4
  %elem_ptr7 = getelementptr i16, ptr %arr_ptr6, i64 %3
  %elem8 = load i16, ptr %elem_ptr7, align 2
  %4 = load i16, ptr %max_val, align 2
  %gttmp = icmp sgt i16 %elem8, %4
  br i1 %gttmp, label %then9, label %ifcont13

then9:                                            ; preds = %while_body
  %arr_ptr10 = load ptr, ptr %arr1, align 8
  %5 = load i64, ptr %i, align 4
  %elem_ptr11 = getelementptr i16, ptr %arr_ptr10, i64 %5
  %elem12 = load i16, ptr %elem_ptr11, align 2
  store i16 %elem12, ptr %max_val, align 2
  br label %ifcont13

ifcont13:                                         ; preds = %then9, %while_body
  %6 = load i64, ptr %i, align 4
  %addtmp = add i64 %6, 1
  store i64 %addtmp, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %7 = load i16, ptr %max_val, align 2
  %auto_wrap_result14 = alloca %result_uint16, align 8
  %err_ptr15 = getelementptr inbounds %result_uint16, ptr %auto_wrap_result14, i32 0, i32 0
  store i8 0, ptr %err_ptr15, align 1
  %val_ptr16 = getelementptr inbounds %result_uint16, ptr %auto_wrap_result14, i32 0, i32 1
  store i16 %7, ptr %val_ptr16, align 2
  %result_val17 = load %result_uint16, ptr %auto_wrap_result14, align 2
  ret %result_uint16 %result_val17
}

define internal %result_uint32 @max_in_array_uint32(ptr %arr, i64 %length) {
entry:
  %i = alloca i64, align 8
  %max_val = alloca i32, align 4
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %0 = load i64, ptr %length2, align 4
  %letmp = icmp sle i64 %0, 0
  br i1 %letmp, label %then, label %ifcont

then:                                             ; preds = %entry
  %arr_ptr = load ptr, ptr %arr1, align 8
  %elem_ptr = getelementptr i32, ptr %arr_ptr, i64 0
  %elem = load i32, ptr %elem_ptr, align 4
  %auto_wrap_result = alloca %result_uint32, align 8
  %err_ptr = getelementptr inbounds %result_uint32, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_uint32, ptr %auto_wrap_result, i32 0, i32 1
  store i32 %elem, ptr %val_ptr, align 4
  %result_val = load %result_uint32, ptr %auto_wrap_result, align 4
  ret %result_uint32 %result_val

ifcont:                                           ; preds = %entry
  %arr_ptr3 = load ptr, ptr %arr1, align 8
  %elem_ptr4 = getelementptr i32, ptr %arr_ptr3, i64 0
  %elem5 = load i32, ptr %elem_ptr4, align 4
  store i32 %elem5, ptr %max_val, align 4
  store i64 1, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont13, %ifcont
  %1 = load i64, ptr %i, align 4
  %2 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %1, %2
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr6 = load ptr, ptr %arr1, align 8
  %3 = load i64, ptr %i, align 4
  %elem_ptr7 = getelementptr i32, ptr %arr_ptr6, i64 %3
  %elem8 = load i32, ptr %elem_ptr7, align 4
  %4 = load i32, ptr %max_val, align 4
  %gttmp = icmp sgt i32 %elem8, %4
  br i1 %gttmp, label %then9, label %ifcont13

then9:                                            ; preds = %while_body
  %arr_ptr10 = load ptr, ptr %arr1, align 8
  %5 = load i64, ptr %i, align 4
  %elem_ptr11 = getelementptr i32, ptr %arr_ptr10, i64 %5
  %elem12 = load i32, ptr %elem_ptr11, align 4
  store i32 %elem12, ptr %max_val, align 4
  br label %ifcont13

ifcont13:                                         ; preds = %then9, %while_body
  %6 = load i64, ptr %i, align 4
  %addtmp = add i64 %6, 1
  store i64 %addtmp, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %7 = load i32, ptr %max_val, align 4
  %auto_wrap_result14 = alloca %result_uint32, align 8
  %err_ptr15 = getelementptr inbounds %result_uint32, ptr %auto_wrap_result14, i32 0, i32 0
  store i8 0, ptr %err_ptr15, align 1
  %val_ptr16 = getelementptr inbounds %result_uint32, ptr %auto_wrap_result14, i32 0, i32 1
  store i32 %7, ptr %val_ptr16, align 4
  %result_val17 = load %result_uint32, ptr %auto_wrap_result14, align 4
  ret %result_uint32 %result_val17
}

define internal %result_uint64 @max_in_array_uint64(ptr %arr, i64 %length) {
entry:
  %i = alloca i64, align 8
  %max_val = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %0 = load i64, ptr %length2, align 4
  %letmp = icmp sle i64 %0, 0
  br i1 %letmp, label %then, label %ifcont

then:                                             ; preds = %entry
  %arr_ptr = load ptr, ptr %arr1, align 8
  %elem_ptr = getelementptr i64, ptr %arr_ptr, i64 0
  %elem = load i64, ptr %elem_ptr, align 4
  %auto_wrap_result = alloca %result_uint64, align 8
  %err_ptr = getelementptr inbounds %result_uint64, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_uint64, ptr %auto_wrap_result, i32 0, i32 1
  store i64 %elem, ptr %val_ptr, align 4
  %result_val = load %result_uint64, ptr %auto_wrap_result, align 4
  ret %result_uint64 %result_val

ifcont:                                           ; preds = %entry
  %arr_ptr3 = load ptr, ptr %arr1, align 8
  %elem_ptr4 = getelementptr i64, ptr %arr_ptr3, i64 0
  %elem5 = load i64, ptr %elem_ptr4, align 4
  store i64 %elem5, ptr %max_val, align 4
  store i64 1, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont13, %ifcont
  %1 = load i64, ptr %i, align 4
  %2 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %1, %2
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr6 = load ptr, ptr %arr1, align 8
  %3 = load i64, ptr %i, align 4
  %elem_ptr7 = getelementptr i64, ptr %arr_ptr6, i64 %3
  %elem8 = load i64, ptr %elem_ptr7, align 4
  %4 = load i64, ptr %max_val, align 4
  %gttmp = icmp sgt i64 %elem8, %4
  br i1 %gttmp, label %then9, label %ifcont13

then9:                                            ; preds = %while_body
  %arr_ptr10 = load ptr, ptr %arr1, align 8
  %5 = load i64, ptr %i, align 4
  %elem_ptr11 = getelementptr i64, ptr %arr_ptr10, i64 %5
  %elem12 = load i64, ptr %elem_ptr11, align 4
  store i64 %elem12, ptr %max_val, align 4
  br label %ifcont13

ifcont13:                                         ; preds = %then9, %while_body
  %6 = load i64, ptr %i, align 4
  %addtmp = add i64 %6, 1
  store i64 %addtmp, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %7 = load i64, ptr %max_val, align 4
  %auto_wrap_result14 = alloca %result_uint64, align 8
  %err_ptr15 = getelementptr inbounds %result_uint64, ptr %auto_wrap_result14, i32 0, i32 0
  store i8 0, ptr %err_ptr15, align 1
  %val_ptr16 = getelementptr inbounds %result_uint64, ptr %auto_wrap_result14, i32 0, i32 1
  store i64 %7, ptr %val_ptr16, align 4
  %result_val17 = load %result_uint64, ptr %auto_wrap_result14, align 4
  ret %result_uint64 %result_val17
}

define internal %result_uint8 @sum_uint8(ptr %arr, i64 %length) {
entry:
  %i = alloca i64, align 8
  %total = alloca i8, align 1
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  store i8 0, ptr %total, align 1
  store i64 0, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %while_body, %entry
  %0 = load i64, ptr %i, align 4
  %1 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %0, %1
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %2 = load i8, ptr %total, align 1
  %arr_ptr = load ptr, ptr %arr1, align 8
  %3 = load i64, ptr %i, align 4
  %elem_ptr = getelementptr i8, ptr %arr_ptr, i64 %3
  %elem = load i8, ptr %elem_ptr, align 1
  %addtmp = add i8 %2, %elem
  store i8 %addtmp, ptr %total, align 1
  %4 = load i64, ptr %i, align 4
  %addtmp3 = add i64 %4, 1
  store i64 %addtmp3, ptr %i, align 4
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
  store i16 0, ptr %total, align 2
  store i64 0, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %while_body, %entry
  %0 = load i64, ptr %i, align 4
  %1 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %0, %1
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %2 = load i16, ptr %total, align 2
  %arr_ptr = load ptr, ptr %arr1, align 8
  %3 = load i64, ptr %i, align 4
  %elem_ptr = getelementptr i16, ptr %arr_ptr, i64 %3
  %elem = load i16, ptr %elem_ptr, align 2
  %addtmp = add i16 %2, %elem
  store i16 %addtmp, ptr %total, align 2
  %4 = load i64, ptr %i, align 4
  %addtmp3 = add i64 %4, 1
  store i64 %addtmp3, ptr %i, align 4
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
  store i32 0, ptr %total, align 4
  store i64 0, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %while_body, %entry
  %0 = load i64, ptr %i, align 4
  %1 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %0, %1
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %2 = load i32, ptr %total, align 4
  %arr_ptr = load ptr, ptr %arr1, align 8
  %3 = load i64, ptr %i, align 4
  %elem_ptr = getelementptr i32, ptr %arr_ptr, i64 %3
  %elem = load i32, ptr %elem_ptr, align 4
  %addtmp = add i32 %2, %elem
  store i32 %addtmp, ptr %total, align 4
  %4 = load i64, ptr %i, align 4
  %addtmp3 = add i64 %4, 1
  store i64 %addtmp3, ptr %i, align 4
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
  store i64 0, ptr %total, align 4
  store i64 0, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %while_body, %entry
  %0 = load i64, ptr %i, align 4
  %1 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %0, %1
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %2 = load i64, ptr %total, align 4
  %arr_ptr = load ptr, ptr %arr1, align 8
  %3 = load i64, ptr %i, align 4
  %elem_ptr = getelementptr i64, ptr %arr_ptr, i64 %3
  %elem = load i64, ptr %elem_ptr, align 4
  %addtmp = add i64 %2, %elem
  store i64 %addtmp, ptr %total, align 4
  %4 = load i64, ptr %i, align 4
  %addtmp3 = add i64 %4, 1
  store i64 %addtmp3, ptr %i, align 4
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

define internal %result_int8 @is_sorted_uint8(ptr %arr, i64 %length) {
entry:
  %i = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %0 = load i64, ptr %length2, align 4
  %letmp = icmp sle i64 %0, 1
  br i1 %letmp, label %then, label %ifcont

then:                                             ; preds = %entry
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 1, ptr %val_ptr, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val

ifcont:                                           ; preds = %entry
  store i64 0, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont11, %ifcont
  %1 = load i64, ptr %i, align 4
  %2 = load i64, ptr %length2, align 4
  %subtmp = sub i64 %2, 1
  %lttmp = icmp slt i64 %1, %subtmp
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr = load ptr, ptr %arr1, align 8
  %3 = load i64, ptr %i, align 4
  %elem_ptr = getelementptr i8, ptr %arr_ptr, i64 %3
  %elem = load i8, ptr %elem_ptr, align 1
  %arr_ptr3 = load ptr, ptr %arr1, align 8
  %4 = load i64, ptr %i, align 4
  %addtmp = add i64 %4, 1
  %elem_ptr4 = getelementptr i8, ptr %arr_ptr3, i64 %addtmp
  %elem5 = load i8, ptr %elem_ptr4, align 1
  %gttmp = icmp sgt i8 %elem, %elem5
  br i1 %gttmp, label %then6, label %ifcont11

then6:                                            ; preds = %while_body
  %auto_wrap_result7 = alloca %result_int8, align 8
  %err_ptr8 = getelementptr inbounds %result_int8, ptr %auto_wrap_result7, i32 0, i32 0
  store i8 0, ptr %err_ptr8, align 1
  %val_ptr9 = getelementptr inbounds %result_int8, ptr %auto_wrap_result7, i32 0, i32 1
  store i8 0, ptr %val_ptr9, align 1
  %result_val10 = load %result_int8, ptr %auto_wrap_result7, align 1
  ret %result_int8 %result_val10

ifcont11:                                         ; preds = %while_body
  %5 = load i64, ptr %i, align 4
  %addtmp12 = add i64 %5, 1
  store i64 %addtmp12, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %auto_wrap_result13 = alloca %result_int8, align 8
  %err_ptr14 = getelementptr inbounds %result_int8, ptr %auto_wrap_result13, i32 0, i32 0
  store i8 0, ptr %err_ptr14, align 1
  %val_ptr15 = getelementptr inbounds %result_int8, ptr %auto_wrap_result13, i32 0, i32 1
  store i8 1, ptr %val_ptr15, align 1
  %result_val16 = load %result_int8, ptr %auto_wrap_result13, align 1
  ret %result_int8 %result_val16
}

define internal %result_int8 @is_sorted_uint16(ptr %arr, i64 %length) {
entry:
  %i = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %0 = load i64, ptr %length2, align 4
  %letmp = icmp sle i64 %0, 1
  br i1 %letmp, label %then, label %ifcont

then:                                             ; preds = %entry
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 1, ptr %val_ptr, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val

ifcont:                                           ; preds = %entry
  store i64 0, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont11, %ifcont
  %1 = load i64, ptr %i, align 4
  %2 = load i64, ptr %length2, align 4
  %subtmp = sub i64 %2, 1
  %lttmp = icmp slt i64 %1, %subtmp
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr = load ptr, ptr %arr1, align 8
  %3 = load i64, ptr %i, align 4
  %elem_ptr = getelementptr i16, ptr %arr_ptr, i64 %3
  %elem = load i16, ptr %elem_ptr, align 2
  %arr_ptr3 = load ptr, ptr %arr1, align 8
  %4 = load i64, ptr %i, align 4
  %addtmp = add i64 %4, 1
  %elem_ptr4 = getelementptr i16, ptr %arr_ptr3, i64 %addtmp
  %elem5 = load i16, ptr %elem_ptr4, align 2
  %gttmp = icmp sgt i16 %elem, %elem5
  br i1 %gttmp, label %then6, label %ifcont11

then6:                                            ; preds = %while_body
  %auto_wrap_result7 = alloca %result_int8, align 8
  %err_ptr8 = getelementptr inbounds %result_int8, ptr %auto_wrap_result7, i32 0, i32 0
  store i8 0, ptr %err_ptr8, align 1
  %val_ptr9 = getelementptr inbounds %result_int8, ptr %auto_wrap_result7, i32 0, i32 1
  store i8 0, ptr %val_ptr9, align 1
  %result_val10 = load %result_int8, ptr %auto_wrap_result7, align 1
  ret %result_int8 %result_val10

ifcont11:                                         ; preds = %while_body
  %5 = load i64, ptr %i, align 4
  %addtmp12 = add i64 %5, 1
  store i64 %addtmp12, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %auto_wrap_result13 = alloca %result_int8, align 8
  %err_ptr14 = getelementptr inbounds %result_int8, ptr %auto_wrap_result13, i32 0, i32 0
  store i8 0, ptr %err_ptr14, align 1
  %val_ptr15 = getelementptr inbounds %result_int8, ptr %auto_wrap_result13, i32 0, i32 1
  store i8 1, ptr %val_ptr15, align 1
  %result_val16 = load %result_int8, ptr %auto_wrap_result13, align 1
  ret %result_int8 %result_val16
}

define internal %result_int8 @is_sorted_uint32(ptr %arr, i64 %length) {
entry:
  %i = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %0 = load i64, ptr %length2, align 4
  %letmp = icmp sle i64 %0, 1
  br i1 %letmp, label %then, label %ifcont

then:                                             ; preds = %entry
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 1, ptr %val_ptr, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val

ifcont:                                           ; preds = %entry
  store i64 0, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont11, %ifcont
  %1 = load i64, ptr %i, align 4
  %2 = load i64, ptr %length2, align 4
  %subtmp = sub i64 %2, 1
  %lttmp = icmp slt i64 %1, %subtmp
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr = load ptr, ptr %arr1, align 8
  %3 = load i64, ptr %i, align 4
  %elem_ptr = getelementptr i32, ptr %arr_ptr, i64 %3
  %elem = load i32, ptr %elem_ptr, align 4
  %arr_ptr3 = load ptr, ptr %arr1, align 8
  %4 = load i64, ptr %i, align 4
  %addtmp = add i64 %4, 1
  %elem_ptr4 = getelementptr i32, ptr %arr_ptr3, i64 %addtmp
  %elem5 = load i32, ptr %elem_ptr4, align 4
  %gttmp = icmp sgt i32 %elem, %elem5
  br i1 %gttmp, label %then6, label %ifcont11

then6:                                            ; preds = %while_body
  %auto_wrap_result7 = alloca %result_int8, align 8
  %err_ptr8 = getelementptr inbounds %result_int8, ptr %auto_wrap_result7, i32 0, i32 0
  store i8 0, ptr %err_ptr8, align 1
  %val_ptr9 = getelementptr inbounds %result_int8, ptr %auto_wrap_result7, i32 0, i32 1
  store i8 0, ptr %val_ptr9, align 1
  %result_val10 = load %result_int8, ptr %auto_wrap_result7, align 1
  ret %result_int8 %result_val10

ifcont11:                                         ; preds = %while_body
  %5 = load i64, ptr %i, align 4
  %addtmp12 = add i64 %5, 1
  store i64 %addtmp12, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %auto_wrap_result13 = alloca %result_int8, align 8
  %err_ptr14 = getelementptr inbounds %result_int8, ptr %auto_wrap_result13, i32 0, i32 0
  store i8 0, ptr %err_ptr14, align 1
  %val_ptr15 = getelementptr inbounds %result_int8, ptr %auto_wrap_result13, i32 0, i32 1
  store i8 1, ptr %val_ptr15, align 1
  %result_val16 = load %result_int8, ptr %auto_wrap_result13, align 1
  ret %result_int8 %result_val16
}

define internal %result_int8 @is_sorted_uint64(ptr %arr, i64 %length) {
entry:
  %i = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %0 = load i64, ptr %length2, align 4
  %letmp = icmp sle i64 %0, 1
  br i1 %letmp, label %then, label %ifcont

then:                                             ; preds = %entry
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 1, ptr %val_ptr, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val

ifcont:                                           ; preds = %entry
  store i64 0, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont11, %ifcont
  %1 = load i64, ptr %i, align 4
  %2 = load i64, ptr %length2, align 4
  %subtmp = sub i64 %2, 1
  %lttmp = icmp slt i64 %1, %subtmp
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr = load ptr, ptr %arr1, align 8
  %3 = load i64, ptr %i, align 4
  %elem_ptr = getelementptr i64, ptr %arr_ptr, i64 %3
  %elem = load i64, ptr %elem_ptr, align 4
  %arr_ptr3 = load ptr, ptr %arr1, align 8
  %4 = load i64, ptr %i, align 4
  %addtmp = add i64 %4, 1
  %elem_ptr4 = getelementptr i64, ptr %arr_ptr3, i64 %addtmp
  %elem5 = load i64, ptr %elem_ptr4, align 4
  %gttmp = icmp sgt i64 %elem, %elem5
  br i1 %gttmp, label %then6, label %ifcont11

then6:                                            ; preds = %while_body
  %auto_wrap_result7 = alloca %result_int8, align 8
  %err_ptr8 = getelementptr inbounds %result_int8, ptr %auto_wrap_result7, i32 0, i32 0
  store i8 0, ptr %err_ptr8, align 1
  %val_ptr9 = getelementptr inbounds %result_int8, ptr %auto_wrap_result7, i32 0, i32 1
  store i8 0, ptr %val_ptr9, align 1
  %result_val10 = load %result_int8, ptr %auto_wrap_result7, align 1
  ret %result_int8 %result_val10

ifcont11:                                         ; preds = %while_body
  %5 = load i64, ptr %i, align 4
  %addtmp12 = add i64 %5, 1
  store i64 %addtmp12, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %auto_wrap_result13 = alloca %result_int8, align 8
  %err_ptr14 = getelementptr inbounds %result_int8, ptr %auto_wrap_result13, i32 0, i32 0
  store i8 0, ptr %err_ptr14, align 1
  %val_ptr15 = getelementptr inbounds %result_int8, ptr %auto_wrap_result13, i32 0, i32 1
  store i8 1, ptr %val_ptr15, align 1
  %result_val16 = load %result_int8, ptr %auto_wrap_result13, align 1
  ret %result_int8 %result_val16
}

define internal %result_int8 @is_sorted_desc_uint8(ptr %arr, i64 %length) {
entry:
  %i = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %0 = load i64, ptr %length2, align 4
  %letmp = icmp sle i64 %0, 1
  br i1 %letmp, label %then, label %ifcont

then:                                             ; preds = %entry
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 1, ptr %val_ptr, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val

ifcont:                                           ; preds = %entry
  store i64 0, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont12, %ifcont
  %1 = load i64, ptr %i, align 4
  %2 = load i64, ptr %length2, align 4
  %subtmp = sub i64 %2, 1
  %lttmp = icmp slt i64 %1, %subtmp
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr = load ptr, ptr %arr1, align 8
  %3 = load i64, ptr %i, align 4
  %elem_ptr = getelementptr i8, ptr %arr_ptr, i64 %3
  %elem = load i8, ptr %elem_ptr, align 1
  %arr_ptr3 = load ptr, ptr %arr1, align 8
  %4 = load i64, ptr %i, align 4
  %addtmp = add i64 %4, 1
  %elem_ptr4 = getelementptr i8, ptr %arr_ptr3, i64 %addtmp
  %elem5 = load i8, ptr %elem_ptr4, align 1
  %lttmp6 = icmp slt i8 %elem, %elem5
  br i1 %lttmp6, label %then7, label %ifcont12

then7:                                            ; preds = %while_body
  %auto_wrap_result8 = alloca %result_int8, align 8
  %err_ptr9 = getelementptr inbounds %result_int8, ptr %auto_wrap_result8, i32 0, i32 0
  store i8 0, ptr %err_ptr9, align 1
  %val_ptr10 = getelementptr inbounds %result_int8, ptr %auto_wrap_result8, i32 0, i32 1
  store i8 0, ptr %val_ptr10, align 1
  %result_val11 = load %result_int8, ptr %auto_wrap_result8, align 1
  ret %result_int8 %result_val11

ifcont12:                                         ; preds = %while_body
  %5 = load i64, ptr %i, align 4
  %addtmp13 = add i64 %5, 1
  store i64 %addtmp13, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %auto_wrap_result14 = alloca %result_int8, align 8
  %err_ptr15 = getelementptr inbounds %result_int8, ptr %auto_wrap_result14, i32 0, i32 0
  store i8 0, ptr %err_ptr15, align 1
  %val_ptr16 = getelementptr inbounds %result_int8, ptr %auto_wrap_result14, i32 0, i32 1
  store i8 1, ptr %val_ptr16, align 1
  %result_val17 = load %result_int8, ptr %auto_wrap_result14, align 1
  ret %result_int8 %result_val17
}

define internal %result_int8 @is_sorted_desc_uint16(ptr %arr, i64 %length) {
entry:
  %i = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %0 = load i64, ptr %length2, align 4
  %letmp = icmp sle i64 %0, 1
  br i1 %letmp, label %then, label %ifcont

then:                                             ; preds = %entry
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 1, ptr %val_ptr, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val

ifcont:                                           ; preds = %entry
  store i64 0, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont12, %ifcont
  %1 = load i64, ptr %i, align 4
  %2 = load i64, ptr %length2, align 4
  %subtmp = sub i64 %2, 1
  %lttmp = icmp slt i64 %1, %subtmp
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr = load ptr, ptr %arr1, align 8
  %3 = load i64, ptr %i, align 4
  %elem_ptr = getelementptr i16, ptr %arr_ptr, i64 %3
  %elem = load i16, ptr %elem_ptr, align 2
  %arr_ptr3 = load ptr, ptr %arr1, align 8
  %4 = load i64, ptr %i, align 4
  %addtmp = add i64 %4, 1
  %elem_ptr4 = getelementptr i16, ptr %arr_ptr3, i64 %addtmp
  %elem5 = load i16, ptr %elem_ptr4, align 2
  %lttmp6 = icmp slt i16 %elem, %elem5
  br i1 %lttmp6, label %then7, label %ifcont12

then7:                                            ; preds = %while_body
  %auto_wrap_result8 = alloca %result_int8, align 8
  %err_ptr9 = getelementptr inbounds %result_int8, ptr %auto_wrap_result8, i32 0, i32 0
  store i8 0, ptr %err_ptr9, align 1
  %val_ptr10 = getelementptr inbounds %result_int8, ptr %auto_wrap_result8, i32 0, i32 1
  store i8 0, ptr %val_ptr10, align 1
  %result_val11 = load %result_int8, ptr %auto_wrap_result8, align 1
  ret %result_int8 %result_val11

ifcont12:                                         ; preds = %while_body
  %5 = load i64, ptr %i, align 4
  %addtmp13 = add i64 %5, 1
  store i64 %addtmp13, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %auto_wrap_result14 = alloca %result_int8, align 8
  %err_ptr15 = getelementptr inbounds %result_int8, ptr %auto_wrap_result14, i32 0, i32 0
  store i8 0, ptr %err_ptr15, align 1
  %val_ptr16 = getelementptr inbounds %result_int8, ptr %auto_wrap_result14, i32 0, i32 1
  store i8 1, ptr %val_ptr16, align 1
  %result_val17 = load %result_int8, ptr %auto_wrap_result14, align 1
  ret %result_int8 %result_val17
}

define internal %result_int8 @is_sorted_desc_uint32(ptr %arr, i64 %length) {
entry:
  %i = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %0 = load i64, ptr %length2, align 4
  %letmp = icmp sle i64 %0, 1
  br i1 %letmp, label %then, label %ifcont

then:                                             ; preds = %entry
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 1, ptr %val_ptr, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val

ifcont:                                           ; preds = %entry
  store i64 0, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont12, %ifcont
  %1 = load i64, ptr %i, align 4
  %2 = load i64, ptr %length2, align 4
  %subtmp = sub i64 %2, 1
  %lttmp = icmp slt i64 %1, %subtmp
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr = load ptr, ptr %arr1, align 8
  %3 = load i64, ptr %i, align 4
  %elem_ptr = getelementptr i32, ptr %arr_ptr, i64 %3
  %elem = load i32, ptr %elem_ptr, align 4
  %arr_ptr3 = load ptr, ptr %arr1, align 8
  %4 = load i64, ptr %i, align 4
  %addtmp = add i64 %4, 1
  %elem_ptr4 = getelementptr i32, ptr %arr_ptr3, i64 %addtmp
  %elem5 = load i32, ptr %elem_ptr4, align 4
  %lttmp6 = icmp slt i32 %elem, %elem5
  br i1 %lttmp6, label %then7, label %ifcont12

then7:                                            ; preds = %while_body
  %auto_wrap_result8 = alloca %result_int8, align 8
  %err_ptr9 = getelementptr inbounds %result_int8, ptr %auto_wrap_result8, i32 0, i32 0
  store i8 0, ptr %err_ptr9, align 1
  %val_ptr10 = getelementptr inbounds %result_int8, ptr %auto_wrap_result8, i32 0, i32 1
  store i8 0, ptr %val_ptr10, align 1
  %result_val11 = load %result_int8, ptr %auto_wrap_result8, align 1
  ret %result_int8 %result_val11

ifcont12:                                         ; preds = %while_body
  %5 = load i64, ptr %i, align 4
  %addtmp13 = add i64 %5, 1
  store i64 %addtmp13, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %auto_wrap_result14 = alloca %result_int8, align 8
  %err_ptr15 = getelementptr inbounds %result_int8, ptr %auto_wrap_result14, i32 0, i32 0
  store i8 0, ptr %err_ptr15, align 1
  %val_ptr16 = getelementptr inbounds %result_int8, ptr %auto_wrap_result14, i32 0, i32 1
  store i8 1, ptr %val_ptr16, align 1
  %result_val17 = load %result_int8, ptr %auto_wrap_result14, align 1
  ret %result_int8 %result_val17
}

define internal %result_int8 @is_sorted_desc_uint64(ptr %arr, i64 %length) {
entry:
  %i = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %0 = load i64, ptr %length2, align 4
  %letmp = icmp sle i64 %0, 1
  br i1 %letmp, label %then, label %ifcont

then:                                             ; preds = %entry
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 1, ptr %val_ptr, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val

ifcont:                                           ; preds = %entry
  store i64 0, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont12, %ifcont
  %1 = load i64, ptr %i, align 4
  %2 = load i64, ptr %length2, align 4
  %subtmp = sub i64 %2, 1
  %lttmp = icmp slt i64 %1, %subtmp
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr = load ptr, ptr %arr1, align 8
  %3 = load i64, ptr %i, align 4
  %elem_ptr = getelementptr i64, ptr %arr_ptr, i64 %3
  %elem = load i64, ptr %elem_ptr, align 4
  %arr_ptr3 = load ptr, ptr %arr1, align 8
  %4 = load i64, ptr %i, align 4
  %addtmp = add i64 %4, 1
  %elem_ptr4 = getelementptr i64, ptr %arr_ptr3, i64 %addtmp
  %elem5 = load i64, ptr %elem_ptr4, align 4
  %lttmp6 = icmp slt i64 %elem, %elem5
  br i1 %lttmp6, label %then7, label %ifcont12

then7:                                            ; preds = %while_body
  %auto_wrap_result8 = alloca %result_int8, align 8
  %err_ptr9 = getelementptr inbounds %result_int8, ptr %auto_wrap_result8, i32 0, i32 0
  store i8 0, ptr %err_ptr9, align 1
  %val_ptr10 = getelementptr inbounds %result_int8, ptr %auto_wrap_result8, i32 0, i32 1
  store i8 0, ptr %val_ptr10, align 1
  %result_val11 = load %result_int8, ptr %auto_wrap_result8, align 1
  ret %result_int8 %result_val11

ifcont12:                                         ; preds = %while_body
  %5 = load i64, ptr %i, align 4
  %addtmp13 = add i64 %5, 1
  store i64 %addtmp13, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %auto_wrap_result14 = alloca %result_int8, align 8
  %err_ptr15 = getelementptr inbounds %result_int8, ptr %auto_wrap_result14, i32 0, i32 0
  store i8 0, ptr %err_ptr15, align 1
  %val_ptr16 = getelementptr inbounds %result_int8, ptr %auto_wrap_result14, i32 0, i32 1
  store i8 1, ptr %val_ptr16, align 1
  %result_val17 = load %result_int8, ptr %auto_wrap_result14, align 1
  ret %result_int8 %result_val17
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

define internal %result_int64 @lastIndexOf_flt32(ptr %arr, i64 %length, float %value) {
entry:
  %i = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %value3 = alloca float, align 4
  store float %value, ptr %value3, align 4
  %0 = load i64, ptr %length2, align 4
  %subtmp = sub i64 %0, 1
  store i64 %subtmp, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont, %entry
  %1 = load i64, ptr %i, align 4
  %getmp = icmp sge i64 %1, 0
  br i1 %getmp, label %while_body, label %while_exit

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
  %subtmp4 = sub i64 %5, 1
  store i64 %subtmp4, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %auto_wrap_result5 = alloca %result_int64, align 8
  %err_ptr6 = getelementptr inbounds %result_int64, ptr %auto_wrap_result5, i32 0, i32 0
  store i8 0, ptr %err_ptr6, align 1
  %val_ptr7 = getelementptr inbounds %result_int64, ptr %auto_wrap_result5, i32 0, i32 1
  store i64 -1, ptr %val_ptr7, align 4
  %result_val8 = load %result_int64, ptr %auto_wrap_result5, align 4
  ret %result_int64 %result_val8
}

define internal %result_int64 @lastIndexOf_flt64(ptr %arr, i64 %length, double %value) {
entry:
  %i = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %value3 = alloca double, align 8
  store double %value, ptr %value3, align 8
  %0 = load i64, ptr %length2, align 4
  %subtmp = sub i64 %0, 1
  store i64 %subtmp, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont, %entry
  %1 = load i64, ptr %i, align 4
  %getmp = icmp sge i64 %1, 0
  br i1 %getmp, label %while_body, label %while_exit

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
  %subtmp4 = sub i64 %5, 1
  store i64 %subtmp4, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %auto_wrap_result5 = alloca %result_int64, align 8
  %err_ptr6 = getelementptr inbounds %result_int64, ptr %auto_wrap_result5, i32 0, i32 0
  store i8 0, ptr %err_ptr6, align 1
  %val_ptr7 = getelementptr inbounds %result_int64, ptr %auto_wrap_result5, i32 0, i32 1
  store i64 -1, ptr %val_ptr7, align 4
  %result_val8 = load %result_int64, ptr %auto_wrap_result5, align 4
  ret %result_int64 %result_val8
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

define internal %result_int8 @reverse_flt32(ptr %arr, i64 %length) {
entry:
  %temp = alloca float, align 4
  %right = alloca i64, align 8
  %left = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  store i64 0, ptr %left, align 4
  %0 = load i64, ptr %length2, align 4
  %subtmp = sub i64 %0, 1
  store i64 %subtmp, ptr %right, align 4
  br label %while_cond

while_cond:                                       ; preds = %while_body, %entry
  %1 = load i64, ptr %left, align 4
  %2 = load i64, ptr %right, align 4
  %lttmp = icmp slt i64 %1, %2
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr = load ptr, ptr %arr1, align 8
  %3 = load i64, ptr %left, align 4
  %elem_ptr = getelementptr float, ptr %arr_ptr, i64 %3
  %elem = load float, ptr %elem_ptr, align 4
  store float %elem, ptr %temp, align 4
  %arr_ptr3 = load ptr, ptr %arr1, align 8
  %4 = load i64, ptr %left, align 4
  %arr_ptr4 = load ptr, ptr %arr1, align 8
  %5 = load i64, ptr %right, align 4
  %elem_ptr5 = getelementptr float, ptr %arr_ptr4, i64 %5
  %elem6 = load float, ptr %elem_ptr5, align 4
  %elem_ptr7 = getelementptr float, ptr %arr_ptr3, i64 %4
  store float %elem6, ptr %elem_ptr7, align 4
  %arr_ptr8 = load ptr, ptr %arr1, align 8
  %6 = load i64, ptr %right, align 4
  %7 = load float, ptr %temp, align 4
  %elem_ptr9 = getelementptr float, ptr %arr_ptr8, i64 %6
  store float %7, ptr %elem_ptr9, align 4
  %8 = load i64, ptr %left, align 4
  %addtmp = add i64 %8, 1
  store i64 %addtmp, ptr %left, align 4
  %9 = load i64, ptr %right, align 4
  %subtmp10 = sub i64 %9, 1
  store i64 %subtmp10, ptr %right, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 1, ptr %val_ptr, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val
}

define internal %result_int8 @reverse_flt64(ptr %arr, i64 %length) {
entry:
  %temp = alloca double, align 8
  %right = alloca i64, align 8
  %left = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  store i64 0, ptr %left, align 4
  %0 = load i64, ptr %length2, align 4
  %subtmp = sub i64 %0, 1
  store i64 %subtmp, ptr %right, align 4
  br label %while_cond

while_cond:                                       ; preds = %while_body, %entry
  %1 = load i64, ptr %left, align 4
  %2 = load i64, ptr %right, align 4
  %lttmp = icmp slt i64 %1, %2
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr = load ptr, ptr %arr1, align 8
  %3 = load i64, ptr %left, align 4
  %elem_ptr = getelementptr double, ptr %arr_ptr, i64 %3
  %elem = load double, ptr %elem_ptr, align 8
  store double %elem, ptr %temp, align 8
  %arr_ptr3 = load ptr, ptr %arr1, align 8
  %4 = load i64, ptr %left, align 4
  %arr_ptr4 = load ptr, ptr %arr1, align 8
  %5 = load i64, ptr %right, align 4
  %elem_ptr5 = getelementptr double, ptr %arr_ptr4, i64 %5
  %elem6 = load double, ptr %elem_ptr5, align 8
  %elem_ptr7 = getelementptr double, ptr %arr_ptr3, i64 %4
  store double %elem6, ptr %elem_ptr7, align 8
  %arr_ptr8 = load ptr, ptr %arr1, align 8
  %6 = load i64, ptr %right, align 4
  %7 = load double, ptr %temp, align 8
  %elem_ptr9 = getelementptr double, ptr %arr_ptr8, i64 %6
  store double %7, ptr %elem_ptr9, align 8
  %8 = load i64, ptr %left, align 4
  %addtmp = add i64 %8, 1
  store i64 %addtmp, ptr %left, align 4
  %9 = load i64, ptr %right, align 4
  %subtmp10 = sub i64 %9, 1
  store i64 %subtmp10, ptr %right, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 1, ptr %val_ptr, align 1
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
  store i8 1, ptr %val_ptr, align 1
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
  store i8 1, ptr %val_ptr, align 1
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
  store i8 1, ptr %val_ptr, align 1
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
  store i8 1, ptr %val_ptr, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val
}

define internal %result_int8 @equals_flt32(ptr %arr1, ptr %arr2, i64 %length) {
entry:
  %i = alloca i64, align 8
  %arr11 = alloca ptr, align 8
  store ptr %arr1, ptr %arr11, align 8
  %arr22 = alloca ptr, align 8
  store ptr %arr2, ptr %arr22, align 8
  %length3 = alloca i64, align 8
  store i64 %length, ptr %length3, align 4
  store i64 0, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont, %entry
  %0 = load i64, ptr %i, align 4
  %1 = load i64, ptr %length3, align 4
  %lttmp = icmp slt i64 %0, %1
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr1_ptr = load ptr, ptr %arr11, align 8
  %2 = load i64, ptr %i, align 4
  %elem_ptr = getelementptr float, ptr %arr1_ptr, i64 %2
  %elem = load float, ptr %elem_ptr, align 4
  %arr2_ptr = load ptr, ptr %arr22, align 8
  %3 = load i64, ptr %i, align 4
  %elem_ptr4 = getelementptr float, ptr %arr2_ptr, i64 %3
  %elem5 = load float, ptr %elem_ptr4, align 4
  %netmp = fcmp one float %elem, %elem5
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
  %auto_wrap_result6 = alloca %result_int8, align 8
  %err_ptr7 = getelementptr inbounds %result_int8, ptr %auto_wrap_result6, i32 0, i32 0
  store i8 0, ptr %err_ptr7, align 1
  %val_ptr8 = getelementptr inbounds %result_int8, ptr %auto_wrap_result6, i32 0, i32 1
  store i8 1, ptr %val_ptr8, align 1
  %result_val9 = load %result_int8, ptr %auto_wrap_result6, align 1
  ret %result_int8 %result_val9
}

define internal %result_int8 @equals_flt64(ptr %arr1, ptr %arr2, i64 %length) {
entry:
  %i = alloca i64, align 8
  %arr11 = alloca ptr, align 8
  store ptr %arr1, ptr %arr11, align 8
  %arr22 = alloca ptr, align 8
  store ptr %arr2, ptr %arr22, align 8
  %length3 = alloca i64, align 8
  store i64 %length, ptr %length3, align 4
  store i64 0, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont, %entry
  %0 = load i64, ptr %i, align 4
  %1 = load i64, ptr %length3, align 4
  %lttmp = icmp slt i64 %0, %1
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr1_ptr = load ptr, ptr %arr11, align 8
  %2 = load i64, ptr %i, align 4
  %elem_ptr = getelementptr double, ptr %arr1_ptr, i64 %2
  %elem = load double, ptr %elem_ptr, align 8
  %arr2_ptr = load ptr, ptr %arr22, align 8
  %3 = load i64, ptr %i, align 4
  %elem_ptr4 = getelementptr double, ptr %arr2_ptr, i64 %3
  %elem5 = load double, ptr %elem_ptr4, align 8
  %netmp = fcmp one double %elem, %elem5
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
  %auto_wrap_result6 = alloca %result_int8, align 8
  %err_ptr7 = getelementptr inbounds %result_int8, ptr %auto_wrap_result6, i32 0, i32 0
  store i8 0, ptr %err_ptr7, align 1
  %val_ptr8 = getelementptr inbounds %result_int8, ptr %auto_wrap_result6, i32 0, i32 1
  store i8 1, ptr %val_ptr8, align 1
  %result_val9 = load %result_int8, ptr %auto_wrap_result6, align 1
  ret %result_int8 %result_val9
}

define internal %result_flt32 @min_in_array_flt32(ptr %arr, i64 %length) {
entry:
  %i = alloca i64, align 8
  %min_val = alloca float, align 4
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %0 = load i64, ptr %length2, align 4
  %letmp = icmp sle i64 %0, 0
  br i1 %letmp, label %then, label %ifcont

then:                                             ; preds = %entry
  %arr_ptr = load ptr, ptr %arr1, align 8
  %elem_ptr = getelementptr float, ptr %arr_ptr, i64 0
  %elem = load float, ptr %elem_ptr, align 4
  %auto_wrap_result = alloca %result_flt32, align 8
  %err_ptr = getelementptr inbounds %result_flt32, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_flt32, ptr %auto_wrap_result, i32 0, i32 1
  store float %elem, ptr %val_ptr, align 4
  %result_val = load %result_flt32, ptr %auto_wrap_result, align 4
  ret %result_flt32 %result_val

ifcont:                                           ; preds = %entry
  %arr_ptr3 = load ptr, ptr %arr1, align 8
  %elem_ptr4 = getelementptr float, ptr %arr_ptr3, i64 0
  %elem5 = load float, ptr %elem_ptr4, align 4
  store float %elem5, ptr %min_val, align 4
  store i64 1, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont14, %ifcont
  %1 = load i64, ptr %i, align 4
  %2 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %1, %2
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr6 = load ptr, ptr %arr1, align 8
  %3 = load i64, ptr %i, align 4
  %elem_ptr7 = getelementptr float, ptr %arr_ptr6, i64 %3
  %elem8 = load float, ptr %elem_ptr7, align 4
  %4 = load float, ptr %min_val, align 4
  %lttmp9 = fcmp olt float %elem8, %4
  br i1 %lttmp9, label %then10, label %ifcont14

then10:                                           ; preds = %while_body
  %arr_ptr11 = load ptr, ptr %arr1, align 8
  %5 = load i64, ptr %i, align 4
  %elem_ptr12 = getelementptr float, ptr %arr_ptr11, i64 %5
  %elem13 = load float, ptr %elem_ptr12, align 4
  store float %elem13, ptr %min_val, align 4
  br label %ifcont14

ifcont14:                                         ; preds = %then10, %while_body
  %6 = load i64, ptr %i, align 4
  %addtmp = add i64 %6, 1
  store i64 %addtmp, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %7 = load float, ptr %min_val, align 4
  %auto_wrap_result15 = alloca %result_flt32, align 8
  %err_ptr16 = getelementptr inbounds %result_flt32, ptr %auto_wrap_result15, i32 0, i32 0
  store i8 0, ptr %err_ptr16, align 1
  %val_ptr17 = getelementptr inbounds %result_flt32, ptr %auto_wrap_result15, i32 0, i32 1
  store float %7, ptr %val_ptr17, align 4
  %result_val18 = load %result_flt32, ptr %auto_wrap_result15, align 4
  ret %result_flt32 %result_val18
}

define internal %result_flt64 @min_in_array_flt64(ptr %arr, i64 %length) {
entry:
  %i = alloca i64, align 8
  %min_val = alloca double, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %0 = load i64, ptr %length2, align 4
  %letmp = icmp sle i64 %0, 0
  br i1 %letmp, label %then, label %ifcont

then:                                             ; preds = %entry
  %arr_ptr = load ptr, ptr %arr1, align 8
  %elem_ptr = getelementptr double, ptr %arr_ptr, i64 0
  %elem = load double, ptr %elem_ptr, align 8
  %auto_wrap_result = alloca %result_flt64, align 8
  %err_ptr = getelementptr inbounds %result_flt64, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_flt64, ptr %auto_wrap_result, i32 0, i32 1
  store double %elem, ptr %val_ptr, align 8
  %result_val = load %result_flt64, ptr %auto_wrap_result, align 8
  ret %result_flt64 %result_val

ifcont:                                           ; preds = %entry
  %arr_ptr3 = load ptr, ptr %arr1, align 8
  %elem_ptr4 = getelementptr double, ptr %arr_ptr3, i64 0
  %elem5 = load double, ptr %elem_ptr4, align 8
  store double %elem5, ptr %min_val, align 8
  store i64 1, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont14, %ifcont
  %1 = load i64, ptr %i, align 4
  %2 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %1, %2
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr6 = load ptr, ptr %arr1, align 8
  %3 = load i64, ptr %i, align 4
  %elem_ptr7 = getelementptr double, ptr %arr_ptr6, i64 %3
  %elem8 = load double, ptr %elem_ptr7, align 8
  %4 = load double, ptr %min_val, align 8
  %lttmp9 = fcmp olt double %elem8, %4
  br i1 %lttmp9, label %then10, label %ifcont14

then10:                                           ; preds = %while_body
  %arr_ptr11 = load ptr, ptr %arr1, align 8
  %5 = load i64, ptr %i, align 4
  %elem_ptr12 = getelementptr double, ptr %arr_ptr11, i64 %5
  %elem13 = load double, ptr %elem_ptr12, align 8
  store double %elem13, ptr %min_val, align 8
  br label %ifcont14

ifcont14:                                         ; preds = %then10, %while_body
  %6 = load i64, ptr %i, align 4
  %addtmp = add i64 %6, 1
  store i64 %addtmp, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %7 = load double, ptr %min_val, align 8
  %auto_wrap_result15 = alloca %result_flt64, align 8
  %err_ptr16 = getelementptr inbounds %result_flt64, ptr %auto_wrap_result15, i32 0, i32 0
  store i8 0, ptr %err_ptr16, align 1
  %val_ptr17 = getelementptr inbounds %result_flt64, ptr %auto_wrap_result15, i32 0, i32 1
  store double %7, ptr %val_ptr17, align 8
  %result_val18 = load %result_flt64, ptr %auto_wrap_result15, align 8
  ret %result_flt64 %result_val18
}

define internal %result_flt32 @max_in_array_flt32(ptr %arr, i64 %length) {
entry:
  %i = alloca i64, align 8
  %max_val = alloca float, align 4
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %0 = load i64, ptr %length2, align 4
  %letmp = icmp sle i64 %0, 0
  br i1 %letmp, label %then, label %ifcont

then:                                             ; preds = %entry
  %arr_ptr = load ptr, ptr %arr1, align 8
  %elem_ptr = getelementptr float, ptr %arr_ptr, i64 0
  %elem = load float, ptr %elem_ptr, align 4
  %auto_wrap_result = alloca %result_flt32, align 8
  %err_ptr = getelementptr inbounds %result_flt32, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_flt32, ptr %auto_wrap_result, i32 0, i32 1
  store float %elem, ptr %val_ptr, align 4
  %result_val = load %result_flt32, ptr %auto_wrap_result, align 4
  ret %result_flt32 %result_val

ifcont:                                           ; preds = %entry
  %arr_ptr3 = load ptr, ptr %arr1, align 8
  %elem_ptr4 = getelementptr float, ptr %arr_ptr3, i64 0
  %elem5 = load float, ptr %elem_ptr4, align 4
  store float %elem5, ptr %max_val, align 4
  store i64 1, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont13, %ifcont
  %1 = load i64, ptr %i, align 4
  %2 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %1, %2
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr6 = load ptr, ptr %arr1, align 8
  %3 = load i64, ptr %i, align 4
  %elem_ptr7 = getelementptr float, ptr %arr_ptr6, i64 %3
  %elem8 = load float, ptr %elem_ptr7, align 4
  %4 = load float, ptr %max_val, align 4
  %gttmp = fcmp ogt float %elem8, %4
  br i1 %gttmp, label %then9, label %ifcont13

then9:                                            ; preds = %while_body
  %arr_ptr10 = load ptr, ptr %arr1, align 8
  %5 = load i64, ptr %i, align 4
  %elem_ptr11 = getelementptr float, ptr %arr_ptr10, i64 %5
  %elem12 = load float, ptr %elem_ptr11, align 4
  store float %elem12, ptr %max_val, align 4
  br label %ifcont13

ifcont13:                                         ; preds = %then9, %while_body
  %6 = load i64, ptr %i, align 4
  %addtmp = add i64 %6, 1
  store i64 %addtmp, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %7 = load float, ptr %max_val, align 4
  %auto_wrap_result14 = alloca %result_flt32, align 8
  %err_ptr15 = getelementptr inbounds %result_flt32, ptr %auto_wrap_result14, i32 0, i32 0
  store i8 0, ptr %err_ptr15, align 1
  %val_ptr16 = getelementptr inbounds %result_flt32, ptr %auto_wrap_result14, i32 0, i32 1
  store float %7, ptr %val_ptr16, align 4
  %result_val17 = load %result_flt32, ptr %auto_wrap_result14, align 4
  ret %result_flt32 %result_val17
}

define internal %result_flt64 @max_in_array_flt64(ptr %arr, i64 %length) {
entry:
  %i = alloca i64, align 8
  %max_val = alloca double, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %0 = load i64, ptr %length2, align 4
  %letmp = icmp sle i64 %0, 0
  br i1 %letmp, label %then, label %ifcont

then:                                             ; preds = %entry
  %arr_ptr = load ptr, ptr %arr1, align 8
  %elem_ptr = getelementptr double, ptr %arr_ptr, i64 0
  %elem = load double, ptr %elem_ptr, align 8
  %auto_wrap_result = alloca %result_flt64, align 8
  %err_ptr = getelementptr inbounds %result_flt64, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_flt64, ptr %auto_wrap_result, i32 0, i32 1
  store double %elem, ptr %val_ptr, align 8
  %result_val = load %result_flt64, ptr %auto_wrap_result, align 8
  ret %result_flt64 %result_val

ifcont:                                           ; preds = %entry
  %arr_ptr3 = load ptr, ptr %arr1, align 8
  %elem_ptr4 = getelementptr double, ptr %arr_ptr3, i64 0
  %elem5 = load double, ptr %elem_ptr4, align 8
  store double %elem5, ptr %max_val, align 8
  store i64 1, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont13, %ifcont
  %1 = load i64, ptr %i, align 4
  %2 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %1, %2
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr6 = load ptr, ptr %arr1, align 8
  %3 = load i64, ptr %i, align 4
  %elem_ptr7 = getelementptr double, ptr %arr_ptr6, i64 %3
  %elem8 = load double, ptr %elem_ptr7, align 8
  %4 = load double, ptr %max_val, align 8
  %gttmp = fcmp ogt double %elem8, %4
  br i1 %gttmp, label %then9, label %ifcont13

then9:                                            ; preds = %while_body
  %arr_ptr10 = load ptr, ptr %arr1, align 8
  %5 = load i64, ptr %i, align 4
  %elem_ptr11 = getelementptr double, ptr %arr_ptr10, i64 %5
  %elem12 = load double, ptr %elem_ptr11, align 8
  store double %elem12, ptr %max_val, align 8
  br label %ifcont13

ifcont13:                                         ; preds = %then9, %while_body
  %6 = load i64, ptr %i, align 4
  %addtmp = add i64 %6, 1
  store i64 %addtmp, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %7 = load double, ptr %max_val, align 8
  %auto_wrap_result14 = alloca %result_flt64, align 8
  %err_ptr15 = getelementptr inbounds %result_flt64, ptr %auto_wrap_result14, i32 0, i32 0
  store i8 0, ptr %err_ptr15, align 1
  %val_ptr16 = getelementptr inbounds %result_flt64, ptr %auto_wrap_result14, i32 0, i32 1
  store double %7, ptr %val_ptr16, align 8
  %result_val17 = load %result_flt64, ptr %auto_wrap_result14, align 8
  ret %result_flt64 %result_val17
}

define internal %result_flt32 @sum_flt32(ptr %arr, i64 %length) {
entry:
  %i = alloca i64, align 8
  %total = alloca float, align 4
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  store i64 0, ptr %total, align 4
  store i64 0, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %while_body, %entry
  %0 = load i64, ptr %i, align 4
  %1 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %0, %1
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %2 = load float, ptr %total, align 4
  %arr_ptr = load ptr, ptr %arr1, align 8
  %3 = load i64, ptr %i, align 4
  %elem_ptr = getelementptr float, ptr %arr_ptr, i64 %3
  %elem = load float, ptr %elem_ptr, align 4
  %addtmp = fadd float %2, %elem
  store float %addtmp, ptr %total, align 4
  %4 = load i64, ptr %i, align 4
  %addtmp3 = add i64 %4, 1
  store i64 %addtmp3, ptr %i, align 4
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
  store i64 0, ptr %total, align 4
  store i64 0, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %while_body, %entry
  %0 = load i64, ptr %i, align 4
  %1 = load i64, ptr %length2, align 4
  %lttmp = icmp slt i64 %0, %1
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %2 = load double, ptr %total, align 8
  %arr_ptr = load ptr, ptr %arr1, align 8
  %3 = load i64, ptr %i, align 4
  %elem_ptr = getelementptr double, ptr %arr_ptr, i64 %3
  %elem = load double, ptr %elem_ptr, align 8
  %addtmp = fadd double %2, %elem
  store double %addtmp, ptr %total, align 8
  %4 = load i64, ptr %i, align 4
  %addtmp3 = add i64 %4, 1
  store i64 %addtmp3, ptr %i, align 4
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

define internal %result_int8 @is_sorted_flt32(ptr %arr, i64 %length) {
entry:
  %i = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %0 = load i64, ptr %length2, align 4
  %letmp = icmp sle i64 %0, 1
  br i1 %letmp, label %then, label %ifcont

then:                                             ; preds = %entry
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 1, ptr %val_ptr, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val

ifcont:                                           ; preds = %entry
  store i64 0, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont11, %ifcont
  %1 = load i64, ptr %i, align 4
  %2 = load i64, ptr %length2, align 4
  %subtmp = sub i64 %2, 1
  %lttmp = icmp slt i64 %1, %subtmp
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr = load ptr, ptr %arr1, align 8
  %3 = load i64, ptr %i, align 4
  %elem_ptr = getelementptr float, ptr %arr_ptr, i64 %3
  %elem = load float, ptr %elem_ptr, align 4
  %arr_ptr3 = load ptr, ptr %arr1, align 8
  %4 = load i64, ptr %i, align 4
  %addtmp = add i64 %4, 1
  %elem_ptr4 = getelementptr float, ptr %arr_ptr3, i64 %addtmp
  %elem5 = load float, ptr %elem_ptr4, align 4
  %gttmp = fcmp ogt float %elem, %elem5
  br i1 %gttmp, label %then6, label %ifcont11

then6:                                            ; preds = %while_body
  %auto_wrap_result7 = alloca %result_int8, align 8
  %err_ptr8 = getelementptr inbounds %result_int8, ptr %auto_wrap_result7, i32 0, i32 0
  store i8 0, ptr %err_ptr8, align 1
  %val_ptr9 = getelementptr inbounds %result_int8, ptr %auto_wrap_result7, i32 0, i32 1
  store i8 0, ptr %val_ptr9, align 1
  %result_val10 = load %result_int8, ptr %auto_wrap_result7, align 1
  ret %result_int8 %result_val10

ifcont11:                                         ; preds = %while_body
  %5 = load i64, ptr %i, align 4
  %addtmp12 = add i64 %5, 1
  store i64 %addtmp12, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %auto_wrap_result13 = alloca %result_int8, align 8
  %err_ptr14 = getelementptr inbounds %result_int8, ptr %auto_wrap_result13, i32 0, i32 0
  store i8 0, ptr %err_ptr14, align 1
  %val_ptr15 = getelementptr inbounds %result_int8, ptr %auto_wrap_result13, i32 0, i32 1
  store i8 1, ptr %val_ptr15, align 1
  %result_val16 = load %result_int8, ptr %auto_wrap_result13, align 1
  ret %result_int8 %result_val16
}

define internal %result_int8 @is_sorted_flt64(ptr %arr, i64 %length) {
entry:
  %i = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %0 = load i64, ptr %length2, align 4
  %letmp = icmp sle i64 %0, 1
  br i1 %letmp, label %then, label %ifcont

then:                                             ; preds = %entry
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 1, ptr %val_ptr, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val

ifcont:                                           ; preds = %entry
  store i64 0, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont11, %ifcont
  %1 = load i64, ptr %i, align 4
  %2 = load i64, ptr %length2, align 4
  %subtmp = sub i64 %2, 1
  %lttmp = icmp slt i64 %1, %subtmp
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr = load ptr, ptr %arr1, align 8
  %3 = load i64, ptr %i, align 4
  %elem_ptr = getelementptr double, ptr %arr_ptr, i64 %3
  %elem = load double, ptr %elem_ptr, align 8
  %arr_ptr3 = load ptr, ptr %arr1, align 8
  %4 = load i64, ptr %i, align 4
  %addtmp = add i64 %4, 1
  %elem_ptr4 = getelementptr double, ptr %arr_ptr3, i64 %addtmp
  %elem5 = load double, ptr %elem_ptr4, align 8
  %gttmp = fcmp ogt double %elem, %elem5
  br i1 %gttmp, label %then6, label %ifcont11

then6:                                            ; preds = %while_body
  %auto_wrap_result7 = alloca %result_int8, align 8
  %err_ptr8 = getelementptr inbounds %result_int8, ptr %auto_wrap_result7, i32 0, i32 0
  store i8 0, ptr %err_ptr8, align 1
  %val_ptr9 = getelementptr inbounds %result_int8, ptr %auto_wrap_result7, i32 0, i32 1
  store i8 0, ptr %val_ptr9, align 1
  %result_val10 = load %result_int8, ptr %auto_wrap_result7, align 1
  ret %result_int8 %result_val10

ifcont11:                                         ; preds = %while_body
  %5 = load i64, ptr %i, align 4
  %addtmp12 = add i64 %5, 1
  store i64 %addtmp12, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %auto_wrap_result13 = alloca %result_int8, align 8
  %err_ptr14 = getelementptr inbounds %result_int8, ptr %auto_wrap_result13, i32 0, i32 0
  store i8 0, ptr %err_ptr14, align 1
  %val_ptr15 = getelementptr inbounds %result_int8, ptr %auto_wrap_result13, i32 0, i32 1
  store i8 1, ptr %val_ptr15, align 1
  %result_val16 = load %result_int8, ptr %auto_wrap_result13, align 1
  ret %result_int8 %result_val16
}

define internal %result_int8 @is_sorted_desc_flt32(ptr %arr, i64 %length) {
entry:
  %i = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %0 = load i64, ptr %length2, align 4
  %letmp = icmp sle i64 %0, 1
  br i1 %letmp, label %then, label %ifcont

then:                                             ; preds = %entry
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 1, ptr %val_ptr, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val

ifcont:                                           ; preds = %entry
  store i64 0, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont12, %ifcont
  %1 = load i64, ptr %i, align 4
  %2 = load i64, ptr %length2, align 4
  %subtmp = sub i64 %2, 1
  %lttmp = icmp slt i64 %1, %subtmp
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr = load ptr, ptr %arr1, align 8
  %3 = load i64, ptr %i, align 4
  %elem_ptr = getelementptr float, ptr %arr_ptr, i64 %3
  %elem = load float, ptr %elem_ptr, align 4
  %arr_ptr3 = load ptr, ptr %arr1, align 8
  %4 = load i64, ptr %i, align 4
  %addtmp = add i64 %4, 1
  %elem_ptr4 = getelementptr float, ptr %arr_ptr3, i64 %addtmp
  %elem5 = load float, ptr %elem_ptr4, align 4
  %lttmp6 = fcmp olt float %elem, %elem5
  br i1 %lttmp6, label %then7, label %ifcont12

then7:                                            ; preds = %while_body
  %auto_wrap_result8 = alloca %result_int8, align 8
  %err_ptr9 = getelementptr inbounds %result_int8, ptr %auto_wrap_result8, i32 0, i32 0
  store i8 0, ptr %err_ptr9, align 1
  %val_ptr10 = getelementptr inbounds %result_int8, ptr %auto_wrap_result8, i32 0, i32 1
  store i8 0, ptr %val_ptr10, align 1
  %result_val11 = load %result_int8, ptr %auto_wrap_result8, align 1
  ret %result_int8 %result_val11

ifcont12:                                         ; preds = %while_body
  %5 = load i64, ptr %i, align 4
  %addtmp13 = add i64 %5, 1
  store i64 %addtmp13, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %auto_wrap_result14 = alloca %result_int8, align 8
  %err_ptr15 = getelementptr inbounds %result_int8, ptr %auto_wrap_result14, i32 0, i32 0
  store i8 0, ptr %err_ptr15, align 1
  %val_ptr16 = getelementptr inbounds %result_int8, ptr %auto_wrap_result14, i32 0, i32 1
  store i8 1, ptr %val_ptr16, align 1
  %result_val17 = load %result_int8, ptr %auto_wrap_result14, align 1
  ret %result_int8 %result_val17
}

define internal %result_int8 @is_sorted_desc_flt64(ptr %arr, i64 %length) {
entry:
  %i = alloca i64, align 8
  %arr1 = alloca ptr, align 8
  store ptr %arr, ptr %arr1, align 8
  %length2 = alloca i64, align 8
  store i64 %length, ptr %length2, align 4
  %0 = load i64, ptr %length2, align 4
  %letmp = icmp sle i64 %0, 1
  br i1 %letmp, label %then, label %ifcont

then:                                             ; preds = %entry
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 1, ptr %val_ptr, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val

ifcont:                                           ; preds = %entry
  store i64 0, ptr %i, align 4
  br label %while_cond

while_cond:                                       ; preds = %ifcont12, %ifcont
  %1 = load i64, ptr %i, align 4
  %2 = load i64, ptr %length2, align 4
  %subtmp = sub i64 %2, 1
  %lttmp = icmp slt i64 %1, %subtmp
  br i1 %lttmp, label %while_body, label %while_exit

while_body:                                       ; preds = %while_cond
  %arr_ptr = load ptr, ptr %arr1, align 8
  %3 = load i64, ptr %i, align 4
  %elem_ptr = getelementptr double, ptr %arr_ptr, i64 %3
  %elem = load double, ptr %elem_ptr, align 8
  %arr_ptr3 = load ptr, ptr %arr1, align 8
  %4 = load i64, ptr %i, align 4
  %addtmp = add i64 %4, 1
  %elem_ptr4 = getelementptr double, ptr %arr_ptr3, i64 %addtmp
  %elem5 = load double, ptr %elem_ptr4, align 8
  %lttmp6 = fcmp olt double %elem, %elem5
  br i1 %lttmp6, label %then7, label %ifcont12

then7:                                            ; preds = %while_body
  %auto_wrap_result8 = alloca %result_int8, align 8
  %err_ptr9 = getelementptr inbounds %result_int8, ptr %auto_wrap_result8, i32 0, i32 0
  store i8 0, ptr %err_ptr9, align 1
  %val_ptr10 = getelementptr inbounds %result_int8, ptr %auto_wrap_result8, i32 0, i32 1
  store i8 0, ptr %val_ptr10, align 1
  %result_val11 = load %result_int8, ptr %auto_wrap_result8, align 1
  ret %result_int8 %result_val11

ifcont12:                                         ; preds = %while_body
  %5 = load i64, ptr %i, align 4
  %addtmp13 = add i64 %5, 1
  store i64 %addtmp13, ptr %i, align 4
  br label %while_cond

while_exit:                                       ; preds = %while_cond
  %auto_wrap_result14 = alloca %result_int8, align 8
  %err_ptr15 = getelementptr inbounds %result_int8, ptr %auto_wrap_result14, i32 0, i32 0
  store i8 0, ptr %err_ptr15, align 1
  %val_ptr16 = getelementptr inbounds %result_int8, ptr %auto_wrap_result14, i32 0, i32 1
  store i8 1, ptr %val_ptr16, align 1
  %result_val17 = load %result_int8, ptr %auto_wrap_result14, align 1
  ret %result_int8 %result_val17
}

define i64 @main() {
entry:
  call void @__aria_module_init()
  ret i64 0
}
