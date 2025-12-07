; ModuleID = 'aria_module'
source_filename = "aria_module"

%result_int8 = type { i8, i8 }
%result_int32 = type { i8, i32 }
%result_int64 = type { i8, i64 }
%result_uint8 = type { i8, i8 }
%result_uint32 = type { i8, i32 }
%result_uint64 = type { i8, i64 }

@0 = private unnamed_addr constant [66 x i8] c"\E2\9C\85 Math library: 24 functions (abs, min, max, clamp for 6 types)\00", align 1

declare void @puts(ptr)

declare void @print(ptr)

define internal void @__aria_module_init() {
entry:
  ret void
}

define internal %result_int8 @abs_int8(i8 %value) {
entry:
  %value1 = alloca i8, align 1
  store i8 %value, ptr %value1, align 1
  %0 = load i8, ptr %value1, align 1
  %1 = sext i8 %0 to i64
  %lttmp = icmp slt i64 %1, 0
  br i1 %lttmp, label %then, label %ifcont

then:                                             ; preds = %entry
  %2 = load i8, ptr %value1, align 1
  %3 = sext i8 %2 to i64
  %subtmp = sub i64 0, %3
  %auto_wrap_cast = trunc i64 %subtmp to i8
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 %auto_wrap_cast, ptr %val_ptr, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val

ifcont:                                           ; preds = %entry
  %4 = load i8, ptr %value1, align 1
  %auto_wrap_result2 = alloca %result_int8, align 8
  %err_ptr3 = getelementptr inbounds %result_int8, ptr %auto_wrap_result2, i32 0, i32 0
  store i8 0, ptr %err_ptr3, align 1
  %val_ptr4 = getelementptr inbounds %result_int8, ptr %auto_wrap_result2, i32 0, i32 1
  store i8 %4, ptr %val_ptr4, align 1
  %result_val5 = load %result_int8, ptr %auto_wrap_result2, align 1
  ret %result_int8 %result_val5
}

define internal %result_int8 @min_int8(i8 %a, i8 %b) {
entry:
  %a1 = alloca i8, align 1
  store i8 %a, ptr %a1, align 1
  %b2 = alloca i8, align 1
  store i8 %b, ptr %b2, align 1
  %0 = load i8, ptr %a1, align 1
  %1 = load i8, ptr %b2, align 1
  %lttmp = icmp slt i8 %0, %1
  br i1 %lttmp, label %then, label %ifcont

then:                                             ; preds = %entry
  %2 = load i8, ptr %a1, align 1
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 %2, ptr %val_ptr, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val

ifcont:                                           ; preds = %entry
  %3 = load i8, ptr %b2, align 1
  %auto_wrap_result3 = alloca %result_int8, align 8
  %err_ptr4 = getelementptr inbounds %result_int8, ptr %auto_wrap_result3, i32 0, i32 0
  store i8 0, ptr %err_ptr4, align 1
  %val_ptr5 = getelementptr inbounds %result_int8, ptr %auto_wrap_result3, i32 0, i32 1
  store i8 %3, ptr %val_ptr5, align 1
  %result_val6 = load %result_int8, ptr %auto_wrap_result3, align 1
  ret %result_int8 %result_val6
}

define internal %result_int8 @max_int8(i8 %a, i8 %b) {
entry:
  %a1 = alloca i8, align 1
  store i8 %a, ptr %a1, align 1
  %b2 = alloca i8, align 1
  store i8 %b, ptr %b2, align 1
  %0 = load i8, ptr %a1, align 1
  %1 = load i8, ptr %b2, align 1
  %gttmp = icmp sgt i8 %0, %1
  br i1 %gttmp, label %then, label %ifcont

then:                                             ; preds = %entry
  %2 = load i8, ptr %a1, align 1
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 %2, ptr %val_ptr, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val

ifcont:                                           ; preds = %entry
  %3 = load i8, ptr %b2, align 1
  %auto_wrap_result3 = alloca %result_int8, align 8
  %err_ptr4 = getelementptr inbounds %result_int8, ptr %auto_wrap_result3, i32 0, i32 0
  store i8 0, ptr %err_ptr4, align 1
  %val_ptr5 = getelementptr inbounds %result_int8, ptr %auto_wrap_result3, i32 0, i32 1
  store i8 %3, ptr %val_ptr5, align 1
  %result_val6 = load %result_int8, ptr %auto_wrap_result3, align 1
  ret %result_int8 %result_val6
}

define internal %result_int8 @clamp_int8(i8 %value, i8 %lo, i8 %hi) {
entry:
  %value1 = alloca i8, align 1
  store i8 %value, ptr %value1, align 1
  %lo2 = alloca i8, align 1
  store i8 %lo, ptr %lo2, align 1
  %hi3 = alloca i8, align 1
  store i8 %hi, ptr %hi3, align 1
  %0 = load i8, ptr %value1, align 1
  %1 = load i8, ptr %lo2, align 1
  %lttmp = icmp slt i8 %0, %1
  br i1 %lttmp, label %then, label %ifcont

then:                                             ; preds = %entry
  %2 = load i8, ptr %lo2, align 1
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 %2, ptr %val_ptr, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val

ifcont:                                           ; preds = %entry
  %3 = load i8, ptr %value1, align 1
  %4 = load i8, ptr %hi3, align 1
  %gttmp = icmp sgt i8 %3, %4
  br i1 %gttmp, label %then4, label %ifcont9

then4:                                            ; preds = %ifcont
  %5 = load i8, ptr %hi3, align 1
  %auto_wrap_result5 = alloca %result_int8, align 8
  %err_ptr6 = getelementptr inbounds %result_int8, ptr %auto_wrap_result5, i32 0, i32 0
  store i8 0, ptr %err_ptr6, align 1
  %val_ptr7 = getelementptr inbounds %result_int8, ptr %auto_wrap_result5, i32 0, i32 1
  store i8 %5, ptr %val_ptr7, align 1
  %result_val8 = load %result_int8, ptr %auto_wrap_result5, align 1
  ret %result_int8 %result_val8

ifcont9:                                          ; preds = %ifcont
  %6 = load i8, ptr %value1, align 1
  %auto_wrap_result10 = alloca %result_int8, align 8
  %err_ptr11 = getelementptr inbounds %result_int8, ptr %auto_wrap_result10, i32 0, i32 0
  store i8 0, ptr %err_ptr11, align 1
  %val_ptr12 = getelementptr inbounds %result_int8, ptr %auto_wrap_result10, i32 0, i32 1
  store i8 %6, ptr %val_ptr12, align 1
  %result_val13 = load %result_int8, ptr %auto_wrap_result10, align 1
  ret %result_int8 %result_val13
}

define internal %result_int32 @abs_int32(i32 %value) {
entry:
  %value1 = alloca i32, align 4
  store i32 %value, ptr %value1, align 4
  %0 = load i32, ptr %value1, align 4
  %1 = sext i32 %0 to i64
  %lttmp = icmp slt i64 %1, 0
  br i1 %lttmp, label %then, label %ifcont

then:                                             ; preds = %entry
  %2 = load i32, ptr %value1, align 4
  %3 = sext i32 %2 to i64
  %subtmp = sub i64 0, %3
  %auto_wrap_cast = trunc i64 %subtmp to i32
  %auto_wrap_result = alloca %result_int32, align 8
  %err_ptr = getelementptr inbounds %result_int32, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int32, ptr %auto_wrap_result, i32 0, i32 1
  store i32 %auto_wrap_cast, ptr %val_ptr, align 4
  %result_val = load %result_int32, ptr %auto_wrap_result, align 4
  ret %result_int32 %result_val

ifcont:                                           ; preds = %entry
  %4 = load i32, ptr %value1, align 4
  %auto_wrap_result2 = alloca %result_int32, align 8
  %err_ptr3 = getelementptr inbounds %result_int32, ptr %auto_wrap_result2, i32 0, i32 0
  store i8 0, ptr %err_ptr3, align 1
  %val_ptr4 = getelementptr inbounds %result_int32, ptr %auto_wrap_result2, i32 0, i32 1
  store i32 %4, ptr %val_ptr4, align 4
  %result_val5 = load %result_int32, ptr %auto_wrap_result2, align 4
  ret %result_int32 %result_val5
}

define internal %result_int32 @min_int32(i32 %a, i32 %b) {
entry:
  %a1 = alloca i32, align 4
  store i32 %a, ptr %a1, align 4
  %b2 = alloca i32, align 4
  store i32 %b, ptr %b2, align 4
  %0 = load i32, ptr %a1, align 4
  %1 = load i32, ptr %b2, align 4
  %lttmp = icmp slt i32 %0, %1
  br i1 %lttmp, label %then, label %ifcont

then:                                             ; preds = %entry
  %2 = load i32, ptr %a1, align 4
  %auto_wrap_result = alloca %result_int32, align 8
  %err_ptr = getelementptr inbounds %result_int32, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int32, ptr %auto_wrap_result, i32 0, i32 1
  store i32 %2, ptr %val_ptr, align 4
  %result_val = load %result_int32, ptr %auto_wrap_result, align 4
  ret %result_int32 %result_val

ifcont:                                           ; preds = %entry
  %3 = load i32, ptr %b2, align 4
  %auto_wrap_result3 = alloca %result_int32, align 8
  %err_ptr4 = getelementptr inbounds %result_int32, ptr %auto_wrap_result3, i32 0, i32 0
  store i8 0, ptr %err_ptr4, align 1
  %val_ptr5 = getelementptr inbounds %result_int32, ptr %auto_wrap_result3, i32 0, i32 1
  store i32 %3, ptr %val_ptr5, align 4
  %result_val6 = load %result_int32, ptr %auto_wrap_result3, align 4
  ret %result_int32 %result_val6
}

define internal %result_int32 @max_int32(i32 %a, i32 %b) {
entry:
  %a1 = alloca i32, align 4
  store i32 %a, ptr %a1, align 4
  %b2 = alloca i32, align 4
  store i32 %b, ptr %b2, align 4
  %0 = load i32, ptr %a1, align 4
  %1 = load i32, ptr %b2, align 4
  %gttmp = icmp sgt i32 %0, %1
  br i1 %gttmp, label %then, label %ifcont

then:                                             ; preds = %entry
  %2 = load i32, ptr %a1, align 4
  %auto_wrap_result = alloca %result_int32, align 8
  %err_ptr = getelementptr inbounds %result_int32, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int32, ptr %auto_wrap_result, i32 0, i32 1
  store i32 %2, ptr %val_ptr, align 4
  %result_val = load %result_int32, ptr %auto_wrap_result, align 4
  ret %result_int32 %result_val

ifcont:                                           ; preds = %entry
  %3 = load i32, ptr %b2, align 4
  %auto_wrap_result3 = alloca %result_int32, align 8
  %err_ptr4 = getelementptr inbounds %result_int32, ptr %auto_wrap_result3, i32 0, i32 0
  store i8 0, ptr %err_ptr4, align 1
  %val_ptr5 = getelementptr inbounds %result_int32, ptr %auto_wrap_result3, i32 0, i32 1
  store i32 %3, ptr %val_ptr5, align 4
  %result_val6 = load %result_int32, ptr %auto_wrap_result3, align 4
  ret %result_int32 %result_val6
}

define internal %result_int32 @clamp_int32(i32 %value, i32 %lo, i32 %hi) {
entry:
  %value1 = alloca i32, align 4
  store i32 %value, ptr %value1, align 4
  %lo2 = alloca i32, align 4
  store i32 %lo, ptr %lo2, align 4
  %hi3 = alloca i32, align 4
  store i32 %hi, ptr %hi3, align 4
  %0 = load i32, ptr %value1, align 4
  %1 = load i32, ptr %lo2, align 4
  %lttmp = icmp slt i32 %0, %1
  br i1 %lttmp, label %then, label %ifcont

then:                                             ; preds = %entry
  %2 = load i32, ptr %lo2, align 4
  %auto_wrap_result = alloca %result_int32, align 8
  %err_ptr = getelementptr inbounds %result_int32, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int32, ptr %auto_wrap_result, i32 0, i32 1
  store i32 %2, ptr %val_ptr, align 4
  %result_val = load %result_int32, ptr %auto_wrap_result, align 4
  ret %result_int32 %result_val

ifcont:                                           ; preds = %entry
  %3 = load i32, ptr %value1, align 4
  %4 = load i32, ptr %hi3, align 4
  %gttmp = icmp sgt i32 %3, %4
  br i1 %gttmp, label %then4, label %ifcont9

then4:                                            ; preds = %ifcont
  %5 = load i32, ptr %hi3, align 4
  %auto_wrap_result5 = alloca %result_int32, align 8
  %err_ptr6 = getelementptr inbounds %result_int32, ptr %auto_wrap_result5, i32 0, i32 0
  store i8 0, ptr %err_ptr6, align 1
  %val_ptr7 = getelementptr inbounds %result_int32, ptr %auto_wrap_result5, i32 0, i32 1
  store i32 %5, ptr %val_ptr7, align 4
  %result_val8 = load %result_int32, ptr %auto_wrap_result5, align 4
  ret %result_int32 %result_val8

ifcont9:                                          ; preds = %ifcont
  %6 = load i32, ptr %value1, align 4
  %auto_wrap_result10 = alloca %result_int32, align 8
  %err_ptr11 = getelementptr inbounds %result_int32, ptr %auto_wrap_result10, i32 0, i32 0
  store i8 0, ptr %err_ptr11, align 1
  %val_ptr12 = getelementptr inbounds %result_int32, ptr %auto_wrap_result10, i32 0, i32 1
  store i32 %6, ptr %val_ptr12, align 4
  %result_val13 = load %result_int32, ptr %auto_wrap_result10, align 4
  ret %result_int32 %result_val13
}

define internal %result_int64 @abs_int64(i64 %value) {
entry:
  %value1 = alloca i64, align 8
  store i64 %value, ptr %value1, align 4
  %0 = load i64, ptr %value1, align 4
  %lttmp = icmp slt i64 %0, 0
  br i1 %lttmp, label %then, label %ifcont

then:                                             ; preds = %entry
  %1 = load i64, ptr %value1, align 4
  %subtmp = sub i64 0, %1
  %auto_wrap_result = alloca %result_int64, align 8
  %err_ptr = getelementptr inbounds %result_int64, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int64, ptr %auto_wrap_result, i32 0, i32 1
  store i64 %subtmp, ptr %val_ptr, align 4
  %result_val = load %result_int64, ptr %auto_wrap_result, align 4
  ret %result_int64 %result_val

ifcont:                                           ; preds = %entry
  %2 = load i64, ptr %value1, align 4
  %auto_wrap_result2 = alloca %result_int64, align 8
  %err_ptr3 = getelementptr inbounds %result_int64, ptr %auto_wrap_result2, i32 0, i32 0
  store i8 0, ptr %err_ptr3, align 1
  %val_ptr4 = getelementptr inbounds %result_int64, ptr %auto_wrap_result2, i32 0, i32 1
  store i64 %2, ptr %val_ptr4, align 4
  %result_val5 = load %result_int64, ptr %auto_wrap_result2, align 4
  ret %result_int64 %result_val5
}

define internal %result_int64 @min_int64(i64 %a, i64 %b) {
entry:
  %a1 = alloca i64, align 8
  store i64 %a, ptr %a1, align 4
  %b2 = alloca i64, align 8
  store i64 %b, ptr %b2, align 4
  %0 = load i64, ptr %a1, align 4
  %1 = load i64, ptr %b2, align 4
  %lttmp = icmp slt i64 %0, %1
  br i1 %lttmp, label %then, label %ifcont

then:                                             ; preds = %entry
  %2 = load i64, ptr %a1, align 4
  %auto_wrap_result = alloca %result_int64, align 8
  %err_ptr = getelementptr inbounds %result_int64, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int64, ptr %auto_wrap_result, i32 0, i32 1
  store i64 %2, ptr %val_ptr, align 4
  %result_val = load %result_int64, ptr %auto_wrap_result, align 4
  ret %result_int64 %result_val

ifcont:                                           ; preds = %entry
  %3 = load i64, ptr %b2, align 4
  %auto_wrap_result3 = alloca %result_int64, align 8
  %err_ptr4 = getelementptr inbounds %result_int64, ptr %auto_wrap_result3, i32 0, i32 0
  store i8 0, ptr %err_ptr4, align 1
  %val_ptr5 = getelementptr inbounds %result_int64, ptr %auto_wrap_result3, i32 0, i32 1
  store i64 %3, ptr %val_ptr5, align 4
  %result_val6 = load %result_int64, ptr %auto_wrap_result3, align 4
  ret %result_int64 %result_val6
}

define internal %result_int64 @max_int64(i64 %a, i64 %b) {
entry:
  %a1 = alloca i64, align 8
  store i64 %a, ptr %a1, align 4
  %b2 = alloca i64, align 8
  store i64 %b, ptr %b2, align 4
  %0 = load i64, ptr %a1, align 4
  %1 = load i64, ptr %b2, align 4
  %gttmp = icmp sgt i64 %0, %1
  br i1 %gttmp, label %then, label %ifcont

then:                                             ; preds = %entry
  %2 = load i64, ptr %a1, align 4
  %auto_wrap_result = alloca %result_int64, align 8
  %err_ptr = getelementptr inbounds %result_int64, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int64, ptr %auto_wrap_result, i32 0, i32 1
  store i64 %2, ptr %val_ptr, align 4
  %result_val = load %result_int64, ptr %auto_wrap_result, align 4
  ret %result_int64 %result_val

ifcont:                                           ; preds = %entry
  %3 = load i64, ptr %b2, align 4
  %auto_wrap_result3 = alloca %result_int64, align 8
  %err_ptr4 = getelementptr inbounds %result_int64, ptr %auto_wrap_result3, i32 0, i32 0
  store i8 0, ptr %err_ptr4, align 1
  %val_ptr5 = getelementptr inbounds %result_int64, ptr %auto_wrap_result3, i32 0, i32 1
  store i64 %3, ptr %val_ptr5, align 4
  %result_val6 = load %result_int64, ptr %auto_wrap_result3, align 4
  ret %result_int64 %result_val6
}

define internal %result_int64 @clamp_int64(i64 %value, i64 %lo, i64 %hi) {
entry:
  %value1 = alloca i64, align 8
  store i64 %value, ptr %value1, align 4
  %lo2 = alloca i64, align 8
  store i64 %lo, ptr %lo2, align 4
  %hi3 = alloca i64, align 8
  store i64 %hi, ptr %hi3, align 4
  %0 = load i64, ptr %value1, align 4
  %1 = load i64, ptr %lo2, align 4
  %lttmp = icmp slt i64 %0, %1
  br i1 %lttmp, label %then, label %ifcont

then:                                             ; preds = %entry
  %2 = load i64, ptr %lo2, align 4
  %auto_wrap_result = alloca %result_int64, align 8
  %err_ptr = getelementptr inbounds %result_int64, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int64, ptr %auto_wrap_result, i32 0, i32 1
  store i64 %2, ptr %val_ptr, align 4
  %result_val = load %result_int64, ptr %auto_wrap_result, align 4
  ret %result_int64 %result_val

ifcont:                                           ; preds = %entry
  %3 = load i64, ptr %value1, align 4
  %4 = load i64, ptr %hi3, align 4
  %gttmp = icmp sgt i64 %3, %4
  br i1 %gttmp, label %then4, label %ifcont9

then4:                                            ; preds = %ifcont
  %5 = load i64, ptr %hi3, align 4
  %auto_wrap_result5 = alloca %result_int64, align 8
  %err_ptr6 = getelementptr inbounds %result_int64, ptr %auto_wrap_result5, i32 0, i32 0
  store i8 0, ptr %err_ptr6, align 1
  %val_ptr7 = getelementptr inbounds %result_int64, ptr %auto_wrap_result5, i32 0, i32 1
  store i64 %5, ptr %val_ptr7, align 4
  %result_val8 = load %result_int64, ptr %auto_wrap_result5, align 4
  ret %result_int64 %result_val8

ifcont9:                                          ; preds = %ifcont
  %6 = load i64, ptr %value1, align 4
  %auto_wrap_result10 = alloca %result_int64, align 8
  %err_ptr11 = getelementptr inbounds %result_int64, ptr %auto_wrap_result10, i32 0, i32 0
  store i8 0, ptr %err_ptr11, align 1
  %val_ptr12 = getelementptr inbounds %result_int64, ptr %auto_wrap_result10, i32 0, i32 1
  store i64 %6, ptr %val_ptr12, align 4
  %result_val13 = load %result_int64, ptr %auto_wrap_result10, align 4
  ret %result_int64 %result_val13
}

define internal %result_uint8 @abs_uint8(i8 %value) {
entry:
  %value1 = alloca i8, align 1
  store i8 %value, ptr %value1, align 1
  %0 = load i8, ptr %value1, align 1
  %1 = sext i8 %0 to i64
  %lttmp = icmp slt i64 %1, 0
  br i1 %lttmp, label %then, label %ifcont

then:                                             ; preds = %entry
  %2 = load i8, ptr %value1, align 1
  %3 = sext i8 %2 to i64
  %subtmp = sub i64 0, %3
  %auto_wrap_cast = trunc i64 %subtmp to i8
  %auto_wrap_result = alloca %result_uint8, align 8
  %err_ptr = getelementptr inbounds %result_uint8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_uint8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 %auto_wrap_cast, ptr %val_ptr, align 1
  %result_val = load %result_uint8, ptr %auto_wrap_result, align 1
  ret %result_uint8 %result_val

ifcont:                                           ; preds = %entry
  %4 = load i8, ptr %value1, align 1
  %auto_wrap_result2 = alloca %result_uint8, align 8
  %err_ptr3 = getelementptr inbounds %result_uint8, ptr %auto_wrap_result2, i32 0, i32 0
  store i8 0, ptr %err_ptr3, align 1
  %val_ptr4 = getelementptr inbounds %result_uint8, ptr %auto_wrap_result2, i32 0, i32 1
  store i8 %4, ptr %val_ptr4, align 1
  %result_val5 = load %result_uint8, ptr %auto_wrap_result2, align 1
  ret %result_uint8 %result_val5
}

define internal %result_uint8 @min_uint8(i8 %a, i8 %b) {
entry:
  %a1 = alloca i8, align 1
  store i8 %a, ptr %a1, align 1
  %b2 = alloca i8, align 1
  store i8 %b, ptr %b2, align 1
  %0 = load i8, ptr %a1, align 1
  %1 = load i8, ptr %b2, align 1
  %lttmp = icmp slt i8 %0, %1
  br i1 %lttmp, label %then, label %ifcont

then:                                             ; preds = %entry
  %2 = load i8, ptr %a1, align 1
  %auto_wrap_result = alloca %result_uint8, align 8
  %err_ptr = getelementptr inbounds %result_uint8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_uint8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 %2, ptr %val_ptr, align 1
  %result_val = load %result_uint8, ptr %auto_wrap_result, align 1
  ret %result_uint8 %result_val

ifcont:                                           ; preds = %entry
  %3 = load i8, ptr %b2, align 1
  %auto_wrap_result3 = alloca %result_uint8, align 8
  %err_ptr4 = getelementptr inbounds %result_uint8, ptr %auto_wrap_result3, i32 0, i32 0
  store i8 0, ptr %err_ptr4, align 1
  %val_ptr5 = getelementptr inbounds %result_uint8, ptr %auto_wrap_result3, i32 0, i32 1
  store i8 %3, ptr %val_ptr5, align 1
  %result_val6 = load %result_uint8, ptr %auto_wrap_result3, align 1
  ret %result_uint8 %result_val6
}

define internal %result_uint8 @max_uint8(i8 %a, i8 %b) {
entry:
  %a1 = alloca i8, align 1
  store i8 %a, ptr %a1, align 1
  %b2 = alloca i8, align 1
  store i8 %b, ptr %b2, align 1
  %0 = load i8, ptr %a1, align 1
  %1 = load i8, ptr %b2, align 1
  %gttmp = icmp sgt i8 %0, %1
  br i1 %gttmp, label %then, label %ifcont

then:                                             ; preds = %entry
  %2 = load i8, ptr %a1, align 1
  %auto_wrap_result = alloca %result_uint8, align 8
  %err_ptr = getelementptr inbounds %result_uint8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_uint8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 %2, ptr %val_ptr, align 1
  %result_val = load %result_uint8, ptr %auto_wrap_result, align 1
  ret %result_uint8 %result_val

ifcont:                                           ; preds = %entry
  %3 = load i8, ptr %b2, align 1
  %auto_wrap_result3 = alloca %result_uint8, align 8
  %err_ptr4 = getelementptr inbounds %result_uint8, ptr %auto_wrap_result3, i32 0, i32 0
  store i8 0, ptr %err_ptr4, align 1
  %val_ptr5 = getelementptr inbounds %result_uint8, ptr %auto_wrap_result3, i32 0, i32 1
  store i8 %3, ptr %val_ptr5, align 1
  %result_val6 = load %result_uint8, ptr %auto_wrap_result3, align 1
  ret %result_uint8 %result_val6
}

define internal %result_uint8 @clamp_uint8(i8 %value, i8 %lo, i8 %hi) {
entry:
  %value1 = alloca i8, align 1
  store i8 %value, ptr %value1, align 1
  %lo2 = alloca i8, align 1
  store i8 %lo, ptr %lo2, align 1
  %hi3 = alloca i8, align 1
  store i8 %hi, ptr %hi3, align 1
  %0 = load i8, ptr %value1, align 1
  %1 = load i8, ptr %lo2, align 1
  %lttmp = icmp slt i8 %0, %1
  br i1 %lttmp, label %then, label %ifcont

then:                                             ; preds = %entry
  %2 = load i8, ptr %lo2, align 1
  %auto_wrap_result = alloca %result_uint8, align 8
  %err_ptr = getelementptr inbounds %result_uint8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_uint8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 %2, ptr %val_ptr, align 1
  %result_val = load %result_uint8, ptr %auto_wrap_result, align 1
  ret %result_uint8 %result_val

ifcont:                                           ; preds = %entry
  %3 = load i8, ptr %value1, align 1
  %4 = load i8, ptr %hi3, align 1
  %gttmp = icmp sgt i8 %3, %4
  br i1 %gttmp, label %then4, label %ifcont9

then4:                                            ; preds = %ifcont
  %5 = load i8, ptr %hi3, align 1
  %auto_wrap_result5 = alloca %result_uint8, align 8
  %err_ptr6 = getelementptr inbounds %result_uint8, ptr %auto_wrap_result5, i32 0, i32 0
  store i8 0, ptr %err_ptr6, align 1
  %val_ptr7 = getelementptr inbounds %result_uint8, ptr %auto_wrap_result5, i32 0, i32 1
  store i8 %5, ptr %val_ptr7, align 1
  %result_val8 = load %result_uint8, ptr %auto_wrap_result5, align 1
  ret %result_uint8 %result_val8

ifcont9:                                          ; preds = %ifcont
  %6 = load i8, ptr %value1, align 1
  %auto_wrap_result10 = alloca %result_uint8, align 8
  %err_ptr11 = getelementptr inbounds %result_uint8, ptr %auto_wrap_result10, i32 0, i32 0
  store i8 0, ptr %err_ptr11, align 1
  %val_ptr12 = getelementptr inbounds %result_uint8, ptr %auto_wrap_result10, i32 0, i32 1
  store i8 %6, ptr %val_ptr12, align 1
  %result_val13 = load %result_uint8, ptr %auto_wrap_result10, align 1
  ret %result_uint8 %result_val13
}

define internal %result_uint32 @abs_uint32(i32 %value) {
entry:
  %value1 = alloca i32, align 4
  store i32 %value, ptr %value1, align 4
  %0 = load i32, ptr %value1, align 4
  %1 = sext i32 %0 to i64
  %lttmp = icmp slt i64 %1, 0
  br i1 %lttmp, label %then, label %ifcont

then:                                             ; preds = %entry
  %2 = load i32, ptr %value1, align 4
  %3 = sext i32 %2 to i64
  %subtmp = sub i64 0, %3
  %auto_wrap_cast = trunc i64 %subtmp to i32
  %auto_wrap_result = alloca %result_uint32, align 8
  %err_ptr = getelementptr inbounds %result_uint32, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_uint32, ptr %auto_wrap_result, i32 0, i32 1
  store i32 %auto_wrap_cast, ptr %val_ptr, align 4
  %result_val = load %result_uint32, ptr %auto_wrap_result, align 4
  ret %result_uint32 %result_val

ifcont:                                           ; preds = %entry
  %4 = load i32, ptr %value1, align 4
  %auto_wrap_result2 = alloca %result_uint32, align 8
  %err_ptr3 = getelementptr inbounds %result_uint32, ptr %auto_wrap_result2, i32 0, i32 0
  store i8 0, ptr %err_ptr3, align 1
  %val_ptr4 = getelementptr inbounds %result_uint32, ptr %auto_wrap_result2, i32 0, i32 1
  store i32 %4, ptr %val_ptr4, align 4
  %result_val5 = load %result_uint32, ptr %auto_wrap_result2, align 4
  ret %result_uint32 %result_val5
}

define internal %result_uint32 @min_uint32(i32 %a, i32 %b) {
entry:
  %a1 = alloca i32, align 4
  store i32 %a, ptr %a1, align 4
  %b2 = alloca i32, align 4
  store i32 %b, ptr %b2, align 4
  %0 = load i32, ptr %a1, align 4
  %1 = load i32, ptr %b2, align 4
  %lttmp = icmp slt i32 %0, %1
  br i1 %lttmp, label %then, label %ifcont

then:                                             ; preds = %entry
  %2 = load i32, ptr %a1, align 4
  %auto_wrap_result = alloca %result_uint32, align 8
  %err_ptr = getelementptr inbounds %result_uint32, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_uint32, ptr %auto_wrap_result, i32 0, i32 1
  store i32 %2, ptr %val_ptr, align 4
  %result_val = load %result_uint32, ptr %auto_wrap_result, align 4
  ret %result_uint32 %result_val

ifcont:                                           ; preds = %entry
  %3 = load i32, ptr %b2, align 4
  %auto_wrap_result3 = alloca %result_uint32, align 8
  %err_ptr4 = getelementptr inbounds %result_uint32, ptr %auto_wrap_result3, i32 0, i32 0
  store i8 0, ptr %err_ptr4, align 1
  %val_ptr5 = getelementptr inbounds %result_uint32, ptr %auto_wrap_result3, i32 0, i32 1
  store i32 %3, ptr %val_ptr5, align 4
  %result_val6 = load %result_uint32, ptr %auto_wrap_result3, align 4
  ret %result_uint32 %result_val6
}

define internal %result_uint32 @max_uint32(i32 %a, i32 %b) {
entry:
  %a1 = alloca i32, align 4
  store i32 %a, ptr %a1, align 4
  %b2 = alloca i32, align 4
  store i32 %b, ptr %b2, align 4
  %0 = load i32, ptr %a1, align 4
  %1 = load i32, ptr %b2, align 4
  %gttmp = icmp sgt i32 %0, %1
  br i1 %gttmp, label %then, label %ifcont

then:                                             ; preds = %entry
  %2 = load i32, ptr %a1, align 4
  %auto_wrap_result = alloca %result_uint32, align 8
  %err_ptr = getelementptr inbounds %result_uint32, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_uint32, ptr %auto_wrap_result, i32 0, i32 1
  store i32 %2, ptr %val_ptr, align 4
  %result_val = load %result_uint32, ptr %auto_wrap_result, align 4
  ret %result_uint32 %result_val

ifcont:                                           ; preds = %entry
  %3 = load i32, ptr %b2, align 4
  %auto_wrap_result3 = alloca %result_uint32, align 8
  %err_ptr4 = getelementptr inbounds %result_uint32, ptr %auto_wrap_result3, i32 0, i32 0
  store i8 0, ptr %err_ptr4, align 1
  %val_ptr5 = getelementptr inbounds %result_uint32, ptr %auto_wrap_result3, i32 0, i32 1
  store i32 %3, ptr %val_ptr5, align 4
  %result_val6 = load %result_uint32, ptr %auto_wrap_result3, align 4
  ret %result_uint32 %result_val6
}

define internal %result_uint32 @clamp_uint32(i32 %value, i32 %lo, i32 %hi) {
entry:
  %value1 = alloca i32, align 4
  store i32 %value, ptr %value1, align 4
  %lo2 = alloca i32, align 4
  store i32 %lo, ptr %lo2, align 4
  %hi3 = alloca i32, align 4
  store i32 %hi, ptr %hi3, align 4
  %0 = load i32, ptr %value1, align 4
  %1 = load i32, ptr %lo2, align 4
  %lttmp = icmp slt i32 %0, %1
  br i1 %lttmp, label %then, label %ifcont

then:                                             ; preds = %entry
  %2 = load i32, ptr %lo2, align 4
  %auto_wrap_result = alloca %result_uint32, align 8
  %err_ptr = getelementptr inbounds %result_uint32, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_uint32, ptr %auto_wrap_result, i32 0, i32 1
  store i32 %2, ptr %val_ptr, align 4
  %result_val = load %result_uint32, ptr %auto_wrap_result, align 4
  ret %result_uint32 %result_val

ifcont:                                           ; preds = %entry
  %3 = load i32, ptr %value1, align 4
  %4 = load i32, ptr %hi3, align 4
  %gttmp = icmp sgt i32 %3, %4
  br i1 %gttmp, label %then4, label %ifcont9

then4:                                            ; preds = %ifcont
  %5 = load i32, ptr %hi3, align 4
  %auto_wrap_result5 = alloca %result_uint32, align 8
  %err_ptr6 = getelementptr inbounds %result_uint32, ptr %auto_wrap_result5, i32 0, i32 0
  store i8 0, ptr %err_ptr6, align 1
  %val_ptr7 = getelementptr inbounds %result_uint32, ptr %auto_wrap_result5, i32 0, i32 1
  store i32 %5, ptr %val_ptr7, align 4
  %result_val8 = load %result_uint32, ptr %auto_wrap_result5, align 4
  ret %result_uint32 %result_val8

ifcont9:                                          ; preds = %ifcont
  %6 = load i32, ptr %value1, align 4
  %auto_wrap_result10 = alloca %result_uint32, align 8
  %err_ptr11 = getelementptr inbounds %result_uint32, ptr %auto_wrap_result10, i32 0, i32 0
  store i8 0, ptr %err_ptr11, align 1
  %val_ptr12 = getelementptr inbounds %result_uint32, ptr %auto_wrap_result10, i32 0, i32 1
  store i32 %6, ptr %val_ptr12, align 4
  %result_val13 = load %result_uint32, ptr %auto_wrap_result10, align 4
  ret %result_uint32 %result_val13
}

define internal %result_uint64 @abs_uint64(i64 %value) {
entry:
  %value1 = alloca i64, align 8
  store i64 %value, ptr %value1, align 4
  %0 = load i64, ptr %value1, align 4
  %lttmp = icmp slt i64 %0, 0
  br i1 %lttmp, label %then, label %ifcont

then:                                             ; preds = %entry
  %1 = load i64, ptr %value1, align 4
  %subtmp = sub i64 0, %1
  %auto_wrap_result = alloca %result_uint64, align 8
  %err_ptr = getelementptr inbounds %result_uint64, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_uint64, ptr %auto_wrap_result, i32 0, i32 1
  store i64 %subtmp, ptr %val_ptr, align 4
  %result_val = load %result_uint64, ptr %auto_wrap_result, align 4
  ret %result_uint64 %result_val

ifcont:                                           ; preds = %entry
  %2 = load i64, ptr %value1, align 4
  %auto_wrap_result2 = alloca %result_uint64, align 8
  %err_ptr3 = getelementptr inbounds %result_uint64, ptr %auto_wrap_result2, i32 0, i32 0
  store i8 0, ptr %err_ptr3, align 1
  %val_ptr4 = getelementptr inbounds %result_uint64, ptr %auto_wrap_result2, i32 0, i32 1
  store i64 %2, ptr %val_ptr4, align 4
  %result_val5 = load %result_uint64, ptr %auto_wrap_result2, align 4
  ret %result_uint64 %result_val5
}

define internal %result_uint64 @min_uint64(i64 %a, i64 %b) {
entry:
  %a1 = alloca i64, align 8
  store i64 %a, ptr %a1, align 4
  %b2 = alloca i64, align 8
  store i64 %b, ptr %b2, align 4
  %0 = load i64, ptr %a1, align 4
  %1 = load i64, ptr %b2, align 4
  %lttmp = icmp slt i64 %0, %1
  br i1 %lttmp, label %then, label %ifcont

then:                                             ; preds = %entry
  %2 = load i64, ptr %a1, align 4
  %auto_wrap_result = alloca %result_uint64, align 8
  %err_ptr = getelementptr inbounds %result_uint64, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_uint64, ptr %auto_wrap_result, i32 0, i32 1
  store i64 %2, ptr %val_ptr, align 4
  %result_val = load %result_uint64, ptr %auto_wrap_result, align 4
  ret %result_uint64 %result_val

ifcont:                                           ; preds = %entry
  %3 = load i64, ptr %b2, align 4
  %auto_wrap_result3 = alloca %result_uint64, align 8
  %err_ptr4 = getelementptr inbounds %result_uint64, ptr %auto_wrap_result3, i32 0, i32 0
  store i8 0, ptr %err_ptr4, align 1
  %val_ptr5 = getelementptr inbounds %result_uint64, ptr %auto_wrap_result3, i32 0, i32 1
  store i64 %3, ptr %val_ptr5, align 4
  %result_val6 = load %result_uint64, ptr %auto_wrap_result3, align 4
  ret %result_uint64 %result_val6
}

define internal %result_uint64 @max_uint64(i64 %a, i64 %b) {
entry:
  %a1 = alloca i64, align 8
  store i64 %a, ptr %a1, align 4
  %b2 = alloca i64, align 8
  store i64 %b, ptr %b2, align 4
  %0 = load i64, ptr %a1, align 4
  %1 = load i64, ptr %b2, align 4
  %gttmp = icmp sgt i64 %0, %1
  br i1 %gttmp, label %then, label %ifcont

then:                                             ; preds = %entry
  %2 = load i64, ptr %a1, align 4
  %auto_wrap_result = alloca %result_uint64, align 8
  %err_ptr = getelementptr inbounds %result_uint64, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_uint64, ptr %auto_wrap_result, i32 0, i32 1
  store i64 %2, ptr %val_ptr, align 4
  %result_val = load %result_uint64, ptr %auto_wrap_result, align 4
  ret %result_uint64 %result_val

ifcont:                                           ; preds = %entry
  %3 = load i64, ptr %b2, align 4
  %auto_wrap_result3 = alloca %result_uint64, align 8
  %err_ptr4 = getelementptr inbounds %result_uint64, ptr %auto_wrap_result3, i32 0, i32 0
  store i8 0, ptr %err_ptr4, align 1
  %val_ptr5 = getelementptr inbounds %result_uint64, ptr %auto_wrap_result3, i32 0, i32 1
  store i64 %3, ptr %val_ptr5, align 4
  %result_val6 = load %result_uint64, ptr %auto_wrap_result3, align 4
  ret %result_uint64 %result_val6
}

define internal %result_uint64 @clamp_uint64(i64 %value, i64 %lo, i64 %hi) {
entry:
  %value1 = alloca i64, align 8
  store i64 %value, ptr %value1, align 4
  %lo2 = alloca i64, align 8
  store i64 %lo, ptr %lo2, align 4
  %hi3 = alloca i64, align 8
  store i64 %hi, ptr %hi3, align 4
  %0 = load i64, ptr %value1, align 4
  %1 = load i64, ptr %lo2, align 4
  %lttmp = icmp slt i64 %0, %1
  br i1 %lttmp, label %then, label %ifcont

then:                                             ; preds = %entry
  %2 = load i64, ptr %lo2, align 4
  %auto_wrap_result = alloca %result_uint64, align 8
  %err_ptr = getelementptr inbounds %result_uint64, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_uint64, ptr %auto_wrap_result, i32 0, i32 1
  store i64 %2, ptr %val_ptr, align 4
  %result_val = load %result_uint64, ptr %auto_wrap_result, align 4
  ret %result_uint64 %result_val

ifcont:                                           ; preds = %entry
  %3 = load i64, ptr %value1, align 4
  %4 = load i64, ptr %hi3, align 4
  %gttmp = icmp sgt i64 %3, %4
  br i1 %gttmp, label %then4, label %ifcont9

then4:                                            ; preds = %ifcont
  %5 = load i64, ptr %hi3, align 4
  %auto_wrap_result5 = alloca %result_uint64, align 8
  %err_ptr6 = getelementptr inbounds %result_uint64, ptr %auto_wrap_result5, i32 0, i32 0
  store i8 0, ptr %err_ptr6, align 1
  %val_ptr7 = getelementptr inbounds %result_uint64, ptr %auto_wrap_result5, i32 0, i32 1
  store i64 %5, ptr %val_ptr7, align 4
  %result_val8 = load %result_uint64, ptr %auto_wrap_result5, align 4
  ret %result_uint64 %result_val8

ifcont9:                                          ; preds = %ifcont
  %6 = load i64, ptr %value1, align 4
  %auto_wrap_result10 = alloca %result_uint64, align 8
  %err_ptr11 = getelementptr inbounds %result_uint64, ptr %auto_wrap_result10, i32 0, i32 0
  store i8 0, ptr %err_ptr11, align 1
  %val_ptr12 = getelementptr inbounds %result_uint64, ptr %auto_wrap_result10, i32 0, i32 1
  store i64 %6, ptr %val_ptr12, align 4
  %result_val13 = load %result_uint64, ptr %auto_wrap_result10, align 4
  ret %result_uint64 %result_val13
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
