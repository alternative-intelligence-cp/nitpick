; ModuleID = 'aria_module'
source_filename = "aria_module"

%result_int8 = type { i8, i8 }
%result_int64 = type { i8, i64 }

@0 = private unnamed_addr constant [31 x i8] c"ERROR: Port cannot be negative\00", align 1
@1 = private unnamed_addr constant [44 x i8] c"WARNING: Port is in reserved range (0-1023)\00", align 1
@2 = private unnamed_addr constant [36 x i8] c"ERROR: Port exceeds maximum (65535)\00", align 1
@3 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@4 = private unnamed_addr constant [6 x i8] c"Port \00", align 1
@5 = private unnamed_addr constant [4 x i8] c"%ld\00", align 1
@6 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@7 = private unnamed_addr constant [9 x i8] c"is valid\00", align 1
@8 = private unnamed_addr constant [34 x i8] c"ERROR: Timeout cannot be negative\00", align 1
@9 = private unnamed_addr constant [40 x i8] c"WARNING: Timeout is very short (<100ms)\00", align 1
@10 = private unnamed_addr constant [37 x i8] c"WARNING: Timeout is very long (>60s)\00", align 1
@11 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@12 = private unnamed_addr constant [9 x i8] c"Timeout \00", align 1
@13 = private unnamed_addr constant [4 x i8] c"%ld\00", align 1
@14 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@15 = private unnamed_addr constant [12 x i8] c"ms is valid\00", align 1
@16 = private unnamed_addr constant [38 x i8] c"ERROR: Retry count cannot be negative\00", align 1
@17 = private unnamed_addr constant [43 x i8] c"WARNING: No retries configured (retries=0)\00", align 1
@18 = private unnamed_addr constant [48 x i8] c"WARNING: High retry count may cause long delays\00", align 1
@19 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@20 = private unnamed_addr constant [13 x i8] c"Retry count \00", align 1
@21 = private unnamed_addr constant [4 x i8] c"%ld\00", align 1
@22 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@23 = private unnamed_addr constant [9 x i8] c"is valid\00", align 1
@24 = private unnamed_addr constant [8 x i8] c"UNKNOWN\00", align 1
@25 = private unnamed_addr constant [6 x i8] c"DEBUG\00", align 1
@26 = private unnamed_addr constant [5 x i8] c"INFO\00", align 1
@27 = private unnamed_addr constant [5 x i8] c"WARN\00", align 1
@28 = private unnamed_addr constant [6 x i8] c"ERROR\00", align 1
@29 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@30 = private unnamed_addr constant [26 x i8] c"ERROR: Invalid log level \00", align 1
@31 = private unnamed_addr constant [4 x i8] c"%ld\00", align 1
@32 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@33 = private unnamed_addr constant [11 x i8] c"Log level \00", align 1
@34 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@35 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@36 = private unnamed_addr constant [9 x i8] c"is valid\00", align 1
@37 = private unnamed_addr constant [33 x i8] c"=== Configuration Validation ===\00", align 1
@38 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@39 = private unnamed_addr constant [22 x i8] c"Checking server port:\00", align 1
@40 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@41 = private unnamed_addr constant [34 x i8] c"Checking invalid port (negative):\00", align 1
@42 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@43 = private unnamed_addr constant [26 x i8] c"Checking privileged port:\00", align 1
@44 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@45 = private unnamed_addr constant [35 x i8] c"Checking invalid port (too large):\00", align 1
@46 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@47 = private unnamed_addr constant [29 x i8] c"Checking connection timeout:\00", align 1
@48 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@49 = private unnamed_addr constant [29 x i8] c"Checking very short timeout:\00", align 1
@50 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@51 = private unnamed_addr constant [22 x i8] c"Checking retry count:\00", align 1
@52 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@53 = private unnamed_addr constant [23 x i8] c"Checking zero retries:\00", align 1
@54 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@55 = private unnamed_addr constant [27 x i8] c"Checking log level (INFO):\00", align 1
@56 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@57 = private unnamed_addr constant [28 x i8] c"Checking invalid log level:\00", align 1
@58 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@59 = private unnamed_addr constant [27 x i8] c"=== Validation Summary ===\00", align 1
@60 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@61 = private unnamed_addr constant [9 x i8] c"Errors: \00", align 1
@62 = private unnamed_addr constant [4 x i8] c"%ld\00", align 1
@63 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@64 = private unnamed_addr constant [11 x i8] c"Warnings: \00", align 1
@65 = private unnamed_addr constant [4 x i8] c"%ld\00", align 1
@66 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@67 = private unnamed_addr constant [37 x i8] c"\E2\9C\93 Configuration validation passed!\00", align 1
@68 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@69 = private unnamed_addr constant [42 x i8] c"\E2\9C\97 Configuration validation failed with \00", align 1
@70 = private unnamed_addr constant [4 x i8] c"%ld\00", align 1
@71 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@72 = private unnamed_addr constant [9 x i8] c"error(s)\00", align 1
@73 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@74 = private unnamed_addr constant [40 x i8] c"All validations complete. System ready.\00", align 1
@75 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@76 = private unnamed_addr constant [61 x i8] c"Configuration errors detected. Please fix before proceeding.\00", align 1

declare void @puts(ptr)

declare void @print(ptr)

define internal void @__aria_module_init() {
entry:
  ret void
}

define internal %result_int8 @validatePort(i64 %port) {
entry:
  %port1 = alloca i64, align 8
  store i64 %port, ptr %port1, align 4
  %0 = load i64, ptr %port1, align 4
  %pick_lt = icmp slt i64 %0, 0
  br i1 %pick_lt, label %case_body_0, label %case_next_0

case_body_0:                                      ; preds = %entry
  call void @puts(ptr @0)
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 1, ptr %val_ptr, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val

case_next_0:                                      ; preds = %entry
  %pick_lt2 = icmp slt i64 %0, 1024
  br i1 %pick_lt2, label %case_body_1, label %case_next_1

case_body_1:                                      ; preds = %case_next_0
  call void @puts(ptr @1)
  %auto_wrap_result3 = alloca %result_int8, align 8
  %err_ptr4 = getelementptr inbounds %result_int8, ptr %auto_wrap_result3, i32 0, i32 0
  store i8 0, ptr %err_ptr4, align 1
  %val_ptr5 = getelementptr inbounds %result_int8, ptr %auto_wrap_result3, i32 0, i32 1
  store i8 2, ptr %val_ptr5, align 1
  %result_val6 = load %result_int8, ptr %auto_wrap_result3, align 1
  ret %result_int8 %result_val6

case_next_1:                                      ; preds = %case_next_0
  %pick_gt = icmp sgt i64 %0, 65535
  br i1 %pick_gt, label %case_body_2, label %case_next_2

case_body_2:                                      ; preds = %case_next_1
  call void @puts(ptr @2)
  %auto_wrap_result7 = alloca %result_int8, align 8
  %err_ptr8 = getelementptr inbounds %result_int8, ptr %auto_wrap_result7, i32 0, i32 0
  store i8 0, ptr %err_ptr8, align 1
  %val_ptr9 = getelementptr inbounds %result_int8, ptr %auto_wrap_result7, i32 0, i32 1
  store i8 1, ptr %val_ptr9, align 1
  %result_val10 = load %result_int8, ptr %auto_wrap_result7, align 1
  ret %result_int8 %result_val10

case_next_2:                                      ; preds = %case_next_1
  br i1 true, label %case_body_3, label %case_next_3

case_body_3:                                      ; preds = %case_next_2
  %str_buffer = alloca i8, i64 1024, align 1
  %1 = call i32 (ptr, ptr, ...) @sprintf(ptr %str_buffer, ptr @3, ptr @4)
  %next_pos = getelementptr i8, ptr %str_buffer, i32 %1
  %2 = load i64, ptr %port1, align 4
  %3 = call i32 (ptr, ptr, ...) @sprintf(ptr %next_pos, ptr @5, i64 %2)
  %next_pos11 = getelementptr i8, ptr %next_pos, i32 %3
  %4 = call i32 (ptr, ptr, ...) @sprintf(ptr %next_pos11, ptr @6, ptr @7)
  %next_pos12 = getelementptr i8, ptr %next_pos11, i32 %4
  call void @puts(ptr %str_buffer)
  %auto_wrap_result13 = alloca %result_int8, align 8
  %err_ptr14 = getelementptr inbounds %result_int8, ptr %auto_wrap_result13, i32 0, i32 0
  store i8 0, ptr %err_ptr14, align 1
  %val_ptr15 = getelementptr inbounds %result_int8, ptr %auto_wrap_result13, i32 0, i32 1
  store i8 0, ptr %val_ptr15, align 1
  %result_val16 = load %result_int8, ptr %auto_wrap_result13, align 1
  ret %result_int8 %result_val16

case_next_3:                                      ; preds = %case_next_2
  br label %pick_done

pick_done:                                        ; preds = %case_next_3
  ret %result_int8 zeroinitializer
}

declare i32 @sprintf(ptr, ptr, ...)

define internal %result_int8 @validateTimeout(i64 %timeout_ms) {
entry:
  %timeout_ms1 = alloca i64, align 8
  store i64 %timeout_ms, ptr %timeout_ms1, align 4
  %0 = load i64, ptr %timeout_ms1, align 4
  %pick_lt = icmp slt i64 %0, 0
  br i1 %pick_lt, label %case_body_0, label %case_next_0

case_body_0:                                      ; preds = %entry
  call void @puts(ptr @8)
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 1, ptr %val_ptr, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val

case_next_0:                                      ; preds = %entry
  %pick_lt2 = icmp slt i64 %0, 100
  br i1 %pick_lt2, label %case_body_1, label %case_next_1

case_body_1:                                      ; preds = %case_next_0
  call void @puts(ptr @9)
  %auto_wrap_result3 = alloca %result_int8, align 8
  %err_ptr4 = getelementptr inbounds %result_int8, ptr %auto_wrap_result3, i32 0, i32 0
  store i8 0, ptr %err_ptr4, align 1
  %val_ptr5 = getelementptr inbounds %result_int8, ptr %auto_wrap_result3, i32 0, i32 1
  store i8 2, ptr %val_ptr5, align 1
  %result_val6 = load %result_int8, ptr %auto_wrap_result3, align 1
  ret %result_int8 %result_val6

case_next_1:                                      ; preds = %case_next_0
  %pick_gt = icmp sgt i64 %0, 60000
  br i1 %pick_gt, label %case_body_2, label %case_next_2

case_body_2:                                      ; preds = %case_next_1
  call void @puts(ptr @10)
  %auto_wrap_result7 = alloca %result_int8, align 8
  %err_ptr8 = getelementptr inbounds %result_int8, ptr %auto_wrap_result7, i32 0, i32 0
  store i8 0, ptr %err_ptr8, align 1
  %val_ptr9 = getelementptr inbounds %result_int8, ptr %auto_wrap_result7, i32 0, i32 1
  store i8 2, ptr %val_ptr9, align 1
  %result_val10 = load %result_int8, ptr %auto_wrap_result7, align 1
  ret %result_int8 %result_val10

case_next_2:                                      ; preds = %case_next_1
  br i1 true, label %case_body_3, label %case_next_3

case_body_3:                                      ; preds = %case_next_2
  %str_buffer = alloca i8, i64 1024, align 1
  %1 = call i32 (ptr, ptr, ...) @sprintf(ptr %str_buffer, ptr @11, ptr @12)
  %next_pos = getelementptr i8, ptr %str_buffer, i32 %1
  %2 = load i64, ptr %timeout_ms1, align 4
  %3 = call i32 (ptr, ptr, ...) @sprintf(ptr %next_pos, ptr @13, i64 %2)
  %next_pos11 = getelementptr i8, ptr %next_pos, i32 %3
  %4 = call i32 (ptr, ptr, ...) @sprintf(ptr %next_pos11, ptr @14, ptr @15)
  %next_pos12 = getelementptr i8, ptr %next_pos11, i32 %4
  call void @puts(ptr %str_buffer)
  %auto_wrap_result13 = alloca %result_int8, align 8
  %err_ptr14 = getelementptr inbounds %result_int8, ptr %auto_wrap_result13, i32 0, i32 0
  store i8 0, ptr %err_ptr14, align 1
  %val_ptr15 = getelementptr inbounds %result_int8, ptr %auto_wrap_result13, i32 0, i32 1
  store i8 0, ptr %val_ptr15, align 1
  %result_val16 = load %result_int8, ptr %auto_wrap_result13, align 1
  ret %result_int8 %result_val16

case_next_3:                                      ; preds = %case_next_2
  br label %pick_done

pick_done:                                        ; preds = %case_next_3
  ret %result_int8 zeroinitializer
}

define internal %result_int8 @validateRetries(i8 %retries) {
entry:
  %retries1 = alloca i8, align 1
  store i8 %retries, ptr %retries1, align 1
  %0 = load i64, ptr %retries1, align 4
  %pick_lt = icmp slt i64 %0, 0
  br i1 %pick_lt, label %case_body_0, label %case_next_0

case_body_0:                                      ; preds = %entry
  call void @puts(ptr @16)
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 1, ptr %val_ptr, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val

case_next_0:                                      ; preds = %entry
  %pick_eq = icmp eq i64 %0, 0
  br i1 %pick_eq, label %case_body_1, label %case_next_1

case_body_1:                                      ; preds = %case_next_0
  call void @puts(ptr @17)
  %auto_wrap_result2 = alloca %result_int8, align 8
  %err_ptr3 = getelementptr inbounds %result_int8, ptr %auto_wrap_result2, i32 0, i32 0
  store i8 0, ptr %err_ptr3, align 1
  %val_ptr4 = getelementptr inbounds %result_int8, ptr %auto_wrap_result2, i32 0, i32 1
  store i8 2, ptr %val_ptr4, align 1
  %result_val5 = load %result_int8, ptr %auto_wrap_result2, align 1
  ret %result_int8 %result_val5

case_next_1:                                      ; preds = %case_next_0
  %pick_gt = icmp sgt i64 %0, 10
  br i1 %pick_gt, label %case_body_2, label %case_next_2

case_body_2:                                      ; preds = %case_next_1
  call void @puts(ptr @18)
  %auto_wrap_result6 = alloca %result_int8, align 8
  %err_ptr7 = getelementptr inbounds %result_int8, ptr %auto_wrap_result6, i32 0, i32 0
  store i8 0, ptr %err_ptr7, align 1
  %val_ptr8 = getelementptr inbounds %result_int8, ptr %auto_wrap_result6, i32 0, i32 1
  store i8 2, ptr %val_ptr8, align 1
  %result_val9 = load %result_int8, ptr %auto_wrap_result6, align 1
  ret %result_int8 %result_val9

case_next_2:                                      ; preds = %case_next_1
  br i1 true, label %case_body_3, label %case_next_3

case_body_3:                                      ; preds = %case_next_2
  %str_buffer = alloca i8, i64 1024, align 1
  %1 = call i32 (ptr, ptr, ...) @sprintf(ptr %str_buffer, ptr @19, ptr @20)
  %next_pos = getelementptr i8, ptr %str_buffer, i32 %1
  %2 = load i64, ptr %retries1, align 4
  %3 = call i32 (ptr, ptr, ...) @sprintf(ptr %next_pos, ptr @21, i64 %2)
  %next_pos10 = getelementptr i8, ptr %next_pos, i32 %3
  %4 = call i32 (ptr, ptr, ...) @sprintf(ptr %next_pos10, ptr @22, ptr @23)
  %next_pos11 = getelementptr i8, ptr %next_pos10, i32 %4
  call void @puts(ptr %str_buffer)
  %auto_wrap_result12 = alloca %result_int8, align 8
  %err_ptr13 = getelementptr inbounds %result_int8, ptr %auto_wrap_result12, i32 0, i32 0
  store i8 0, ptr %err_ptr13, align 1
  %val_ptr14 = getelementptr inbounds %result_int8, ptr %auto_wrap_result12, i32 0, i32 1
  store i8 0, ptr %val_ptr14, align 1
  %result_val15 = load %result_int8, ptr %auto_wrap_result12, align 1
  ret %result_int8 %result_val15

case_next_3:                                      ; preds = %case_next_2
  br label %pick_done

pick_done:                                        ; preds = %case_next_3
  ret %result_int8 zeroinitializer
}

define internal %result_int8 @validateLogLevel(i8 %level) {
entry:
  %is_valid = alloca i8, align 1
  %level_name = alloca ptr, align 8
  %level1 = alloca i8, align 1
  store i8 %level, ptr %level1, align 1
  store ptr @24, ptr %level_name, align 8
  store i64 0, ptr %is_valid, align 4
  %0 = load i64, ptr %level1, align 4
  %pick_eq = icmp eq i64 %0, 0
  br i1 %pick_eq, label %case_body_0, label %case_next_0

case_body_0:                                      ; preds = %entry
  store ptr @25, ptr %level_name, align 8
  store i64 1, ptr %is_valid, align 4
  br label %pick_done

case_next_0:                                      ; preds = %entry
  %pick_eq2 = icmp eq i64 %0, 1
  br i1 %pick_eq2, label %case_body_1, label %case_next_1

case_body_1:                                      ; preds = %case_next_0
  store ptr @26, ptr %level_name, align 8
  store i64 1, ptr %is_valid, align 4
  br label %pick_done

case_next_1:                                      ; preds = %case_next_0
  %pick_eq3 = icmp eq i64 %0, 2
  br i1 %pick_eq3, label %case_body_2, label %case_next_2

case_body_2:                                      ; preds = %case_next_1
  store ptr @27, ptr %level_name, align 8
  store i64 1, ptr %is_valid, align 4
  br label %pick_done

case_next_2:                                      ; preds = %case_next_1
  %pick_eq4 = icmp eq i64 %0, 3
  br i1 %pick_eq4, label %case_body_3, label %case_next_3

case_body_3:                                      ; preds = %case_next_2
  store ptr @28, ptr %level_name, align 8
  store i64 1, ptr %is_valid, align 4
  br label %pick_done

case_next_3:                                      ; preds = %case_next_2
  br i1 true, label %case_body_4, label %case_next_4

case_body_4:                                      ; preds = %case_next_3
  %str_buffer = alloca i8, i64 1024, align 1
  %1 = call i32 (ptr, ptr, ...) @sprintf(ptr %str_buffer, ptr @29, ptr @30)
  %next_pos = getelementptr i8, ptr %str_buffer, i32 %1
  %2 = load i64, ptr %level1, align 4
  %3 = call i32 (ptr, ptr, ...) @sprintf(ptr %next_pos, ptr @31, i64 %2)
  %next_pos5 = getelementptr i8, ptr %next_pos, i32 %3
  call void @puts(ptr %str_buffer)
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 1, ptr %val_ptr, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val

case_next_4:                                      ; preds = %case_next_3
  br label %pick_done

pick_done:                                        ; preds = %case_next_4, %case_body_3, %case_body_2, %case_body_1, %case_body_0
  %4 = load i8, ptr %is_valid, align 1
  %pick_eq7 = icmp eq i8 %4, 1
  br i1 %pick_eq7, label %case_body_06, label %case_next_016

case_body_06:                                     ; preds = %pick_done
  %str_buffer8 = alloca i8, i64 1024, align 1
  %5 = call i32 (ptr, ptr, ...) @sprintf(ptr %str_buffer8, ptr @32, ptr @33)
  %next_pos9 = getelementptr i8, ptr %str_buffer8, i32 %5
  %6 = load ptr, ptr %level_name, align 8
  %7 = call i32 (ptr, ptr, ...) @sprintf(ptr %next_pos9, ptr @34, ptr %6)
  %next_pos10 = getelementptr i8, ptr %next_pos9, i32 %7
  %8 = call i32 (ptr, ptr, ...) @sprintf(ptr %next_pos10, ptr @35, ptr @36)
  %next_pos11 = getelementptr i8, ptr %next_pos10, i32 %8
  call void @puts(ptr %str_buffer8)
  %auto_wrap_result12 = alloca %result_int8, align 8
  %err_ptr13 = getelementptr inbounds %result_int8, ptr %auto_wrap_result12, i32 0, i32 0
  store i8 0, ptr %err_ptr13, align 1
  %val_ptr14 = getelementptr inbounds %result_int8, ptr %auto_wrap_result12, i32 0, i32 1
  store i8 0, ptr %val_ptr14, align 1
  %result_val15 = load %result_int8, ptr %auto_wrap_result12, align 1
  ret %result_int8 %result_val15

case_next_016:                                    ; preds = %pick_done
  br i1 true, label %case_body_117, label %case_next_122

case_body_117:                                    ; preds = %case_next_016
  %auto_wrap_result18 = alloca %result_int8, align 8
  %err_ptr19 = getelementptr inbounds %result_int8, ptr %auto_wrap_result18, i32 0, i32 0
  store i8 0, ptr %err_ptr19, align 1
  %val_ptr20 = getelementptr inbounds %result_int8, ptr %auto_wrap_result18, i32 0, i32 1
  store i8 1, ptr %val_ptr20, align 1
  %result_val21 = load %result_int8, ptr %auto_wrap_result18, align 1
  ret %result_int8 %result_val21

case_next_122:                                    ; preds = %case_next_016
  br label %pick_done23

pick_done23:                                      ; preds = %case_next_122
  ret %result_int8 zeroinitializer
}

define internal %result_int8 @validateConfig() {
entry:
  %r10 = alloca %result_int64, align 8
  %r9 = alloca %result_int64, align 8
  %r8 = alloca %result_int64, align 8
  %r7 = alloca %result_int64, align 8
  %r6 = alloca %result_int64, align 8
  %r5 = alloca %result_int64, align 8
  %r4 = alloca %result_int64, align 8
  %r3 = alloca %result_int64, align 8
  %r2 = alloca %result_int64, align 8
  %r1 = alloca %result_int64, align 8
  %warning_count = alloca i8, align 1
  %error_count = alloca i8, align 1
  store i64 0, ptr %error_count, align 4
  store i64 0, ptr %warning_count, align 4
  call void @puts(ptr @37)
  call void @puts(ptr @38)
  call void @puts(ptr @39)
  %calltmp = call %result_int8 @validatePort(i64 8080)
  store %result_int8 %calltmp, ptr %r1, align 1
  %0 = load %result_int64, ptr %r1, align 4
  %temp = alloca %result_int64, align 8
  store %result_int64 %0, ptr %temp, align 4
  %err_ptr = getelementptr inbounds %result_int64, ptr %temp, i32 0, i32 0
  %err = load i8, ptr %err_ptr, align 1
  %pick_eq = icmp eq i8 %err, 1
  br i1 %pick_eq, label %case_body_0, label %case_next_0

case_body_0:                                      ; preds = %entry
  %1 = load i8, ptr %error_count, align 1
  %2 = load i64, ptr %error_count, align 4
  %3 = add i64 %2, 1
  store i64 %3, ptr %error_count, align 4
  br label %pick_done

case_next_0:                                      ; preds = %entry
  %pick_eq1 = icmp eq i8 %err, 2
  br i1 %pick_eq1, label %case_body_1, label %case_next_1

case_body_1:                                      ; preds = %case_next_0
  %4 = load i8, ptr %warning_count, align 1
  %5 = load i64, ptr %warning_count, align 4
  %6 = add i64 %5, 1
  store i64 %6, ptr %warning_count, align 4
  br label %pick_done

case_next_1:                                      ; preds = %case_next_0
  br i1 true, label %case_body_2, label %case_next_2

case_body_2:                                      ; preds = %case_next_1
  br label %pick_done

case_next_2:                                      ; preds = %case_next_1
  br label %pick_done

pick_done:                                        ; preds = %case_next_2, %case_body_2, %case_body_1, %case_body_0
  call void @puts(ptr @40)
  call void @puts(ptr @41)
  %calltmp2 = call %result_int8 @validatePort(i64 -1)
  store %result_int8 %calltmp2, ptr %r2, align 1
  %7 = load %result_int64, ptr %r2, align 4
  %temp3 = alloca %result_int64, align 8
  store %result_int64 %7, ptr %temp3, align 4
  %err_ptr4 = getelementptr inbounds %result_int64, ptr %temp3, i32 0, i32 0
  %err5 = load i8, ptr %err_ptr4, align 1
  %pick_eq7 = icmp eq i8 %err5, 1
  br i1 %pick_eq7, label %case_body_06, label %case_next_08

case_body_06:                                     ; preds = %pick_done
  %8 = load i8, ptr %error_count, align 1
  %9 = load i64, ptr %error_count, align 4
  %10 = add i64 %9, 1
  store i64 %10, ptr %error_count, align 4
  br label %pick_done14

case_next_08:                                     ; preds = %pick_done
  %pick_eq10 = icmp eq i8 %err5, 2
  br i1 %pick_eq10, label %case_body_19, label %case_next_111

case_body_19:                                     ; preds = %case_next_08
  %11 = load i8, ptr %warning_count, align 1
  %12 = load i64, ptr %warning_count, align 4
  %13 = add i64 %12, 1
  store i64 %13, ptr %warning_count, align 4
  br label %pick_done14

case_next_111:                                    ; preds = %case_next_08
  br i1 true, label %case_body_212, label %case_next_213

case_body_212:                                    ; preds = %case_next_111
  br label %pick_done14

case_next_213:                                    ; preds = %case_next_111
  br label %pick_done14

pick_done14:                                      ; preds = %case_next_213, %case_body_212, %case_body_19, %case_body_06
  call void @puts(ptr @42)
  call void @puts(ptr @43)
  %calltmp15 = call %result_int8 @validatePort(i64 80)
  store %result_int8 %calltmp15, ptr %r3, align 1
  %14 = load %result_int64, ptr %r3, align 4
  %temp16 = alloca %result_int64, align 8
  store %result_int64 %14, ptr %temp16, align 4
  %err_ptr17 = getelementptr inbounds %result_int64, ptr %temp16, i32 0, i32 0
  %err18 = load i8, ptr %err_ptr17, align 1
  %pick_eq20 = icmp eq i8 %err18, 1
  br i1 %pick_eq20, label %case_body_019, label %case_next_021

case_body_019:                                    ; preds = %pick_done14
  %15 = load i8, ptr %error_count, align 1
  %16 = load i64, ptr %error_count, align 4
  %17 = add i64 %16, 1
  store i64 %17, ptr %error_count, align 4
  br label %pick_done27

case_next_021:                                    ; preds = %pick_done14
  %pick_eq23 = icmp eq i8 %err18, 2
  br i1 %pick_eq23, label %case_body_122, label %case_next_124

case_body_122:                                    ; preds = %case_next_021
  %18 = load i8, ptr %warning_count, align 1
  %19 = load i64, ptr %warning_count, align 4
  %20 = add i64 %19, 1
  store i64 %20, ptr %warning_count, align 4
  br label %pick_done27

case_next_124:                                    ; preds = %case_next_021
  br i1 true, label %case_body_225, label %case_next_226

case_body_225:                                    ; preds = %case_next_124
  br label %pick_done27

case_next_226:                                    ; preds = %case_next_124
  br label %pick_done27

pick_done27:                                      ; preds = %case_next_226, %case_body_225, %case_body_122, %case_body_019
  call void @puts(ptr @44)
  call void @puts(ptr @45)
  %calltmp28 = call %result_int8 @validatePort(i64 99999)
  store %result_int8 %calltmp28, ptr %r4, align 1
  %21 = load %result_int64, ptr %r4, align 4
  %temp29 = alloca %result_int64, align 8
  store %result_int64 %21, ptr %temp29, align 4
  %err_ptr30 = getelementptr inbounds %result_int64, ptr %temp29, i32 0, i32 0
  %err31 = load i8, ptr %err_ptr30, align 1
  %pick_eq33 = icmp eq i8 %err31, 1
  br i1 %pick_eq33, label %case_body_032, label %case_next_034

case_body_032:                                    ; preds = %pick_done27
  %22 = load i8, ptr %error_count, align 1
  %23 = load i64, ptr %error_count, align 4
  %24 = add i64 %23, 1
  store i64 %24, ptr %error_count, align 4
  br label %pick_done40

case_next_034:                                    ; preds = %pick_done27
  %pick_eq36 = icmp eq i8 %err31, 2
  br i1 %pick_eq36, label %case_body_135, label %case_next_137

case_body_135:                                    ; preds = %case_next_034
  %25 = load i8, ptr %warning_count, align 1
  %26 = load i64, ptr %warning_count, align 4
  %27 = add i64 %26, 1
  store i64 %27, ptr %warning_count, align 4
  br label %pick_done40

case_next_137:                                    ; preds = %case_next_034
  br i1 true, label %case_body_238, label %case_next_239

case_body_238:                                    ; preds = %case_next_137
  br label %pick_done40

case_next_239:                                    ; preds = %case_next_137
  br label %pick_done40

pick_done40:                                      ; preds = %case_next_239, %case_body_238, %case_body_135, %case_body_032
  call void @puts(ptr @46)
  call void @puts(ptr @47)
  %calltmp41 = call %result_int8 @validateTimeout(i64 5000)
  store %result_int8 %calltmp41, ptr %r5, align 1
  %28 = load %result_int64, ptr %r5, align 4
  %temp42 = alloca %result_int64, align 8
  store %result_int64 %28, ptr %temp42, align 4
  %err_ptr43 = getelementptr inbounds %result_int64, ptr %temp42, i32 0, i32 0
  %err44 = load i8, ptr %err_ptr43, align 1
  %pick_eq46 = icmp eq i8 %err44, 1
  br i1 %pick_eq46, label %case_body_045, label %case_next_047

case_body_045:                                    ; preds = %pick_done40
  %29 = load i8, ptr %error_count, align 1
  %30 = load i64, ptr %error_count, align 4
  %31 = add i64 %30, 1
  store i64 %31, ptr %error_count, align 4
  br label %pick_done53

case_next_047:                                    ; preds = %pick_done40
  %pick_eq49 = icmp eq i8 %err44, 2
  br i1 %pick_eq49, label %case_body_148, label %case_next_150

case_body_148:                                    ; preds = %case_next_047
  %32 = load i8, ptr %warning_count, align 1
  %33 = load i64, ptr %warning_count, align 4
  %34 = add i64 %33, 1
  store i64 %34, ptr %warning_count, align 4
  br label %pick_done53

case_next_150:                                    ; preds = %case_next_047
  br i1 true, label %case_body_251, label %case_next_252

case_body_251:                                    ; preds = %case_next_150
  br label %pick_done53

case_next_252:                                    ; preds = %case_next_150
  br label %pick_done53

pick_done53:                                      ; preds = %case_next_252, %case_body_251, %case_body_148, %case_body_045
  call void @puts(ptr @48)
  call void @puts(ptr @49)
  %calltmp54 = call %result_int8 @validateTimeout(i64 50)
  store %result_int8 %calltmp54, ptr %r6, align 1
  %35 = load %result_int64, ptr %r6, align 4
  %temp55 = alloca %result_int64, align 8
  store %result_int64 %35, ptr %temp55, align 4
  %err_ptr56 = getelementptr inbounds %result_int64, ptr %temp55, i32 0, i32 0
  %err57 = load i8, ptr %err_ptr56, align 1
  %pick_eq59 = icmp eq i8 %err57, 1
  br i1 %pick_eq59, label %case_body_058, label %case_next_060

case_body_058:                                    ; preds = %pick_done53
  %36 = load i8, ptr %error_count, align 1
  %37 = load i64, ptr %error_count, align 4
  %38 = add i64 %37, 1
  store i64 %38, ptr %error_count, align 4
  br label %pick_done66

case_next_060:                                    ; preds = %pick_done53
  %pick_eq62 = icmp eq i8 %err57, 2
  br i1 %pick_eq62, label %case_body_161, label %case_next_163

case_body_161:                                    ; preds = %case_next_060
  %39 = load i8, ptr %warning_count, align 1
  %40 = load i64, ptr %warning_count, align 4
  %41 = add i64 %40, 1
  store i64 %41, ptr %warning_count, align 4
  br label %pick_done66

case_next_163:                                    ; preds = %case_next_060
  br i1 true, label %case_body_264, label %case_next_265

case_body_264:                                    ; preds = %case_next_163
  br label %pick_done66

case_next_265:                                    ; preds = %case_next_163
  br label %pick_done66

pick_done66:                                      ; preds = %case_next_265, %case_body_264, %case_body_161, %case_body_058
  call void @puts(ptr @50)
  call void @puts(ptr @51)
  %calltmp67 = call %result_int8 @validateRetries(i8 3)
  store %result_int8 %calltmp67, ptr %r7, align 1
  %42 = load %result_int64, ptr %r7, align 4
  %temp68 = alloca %result_int64, align 8
  store %result_int64 %42, ptr %temp68, align 4
  %err_ptr69 = getelementptr inbounds %result_int64, ptr %temp68, i32 0, i32 0
  %err70 = load i8, ptr %err_ptr69, align 1
  %pick_eq72 = icmp eq i8 %err70, 1
  br i1 %pick_eq72, label %case_body_071, label %case_next_073

case_body_071:                                    ; preds = %pick_done66
  %43 = load i8, ptr %error_count, align 1
  %44 = load i64, ptr %error_count, align 4
  %45 = add i64 %44, 1
  store i64 %45, ptr %error_count, align 4
  br label %pick_done79

case_next_073:                                    ; preds = %pick_done66
  %pick_eq75 = icmp eq i8 %err70, 2
  br i1 %pick_eq75, label %case_body_174, label %case_next_176

case_body_174:                                    ; preds = %case_next_073
  %46 = load i8, ptr %warning_count, align 1
  %47 = load i64, ptr %warning_count, align 4
  %48 = add i64 %47, 1
  store i64 %48, ptr %warning_count, align 4
  br label %pick_done79

case_next_176:                                    ; preds = %case_next_073
  br i1 true, label %case_body_277, label %case_next_278

case_body_277:                                    ; preds = %case_next_176
  br label %pick_done79

case_next_278:                                    ; preds = %case_next_176
  br label %pick_done79

pick_done79:                                      ; preds = %case_next_278, %case_body_277, %case_body_174, %case_body_071
  call void @puts(ptr @52)
  call void @puts(ptr @53)
  %calltmp80 = call %result_int8 @validateRetries(i8 0)
  store %result_int8 %calltmp80, ptr %r8, align 1
  %49 = load %result_int64, ptr %r8, align 4
  %temp81 = alloca %result_int64, align 8
  store %result_int64 %49, ptr %temp81, align 4
  %err_ptr82 = getelementptr inbounds %result_int64, ptr %temp81, i32 0, i32 0
  %err83 = load i8, ptr %err_ptr82, align 1
  %pick_eq85 = icmp eq i8 %err83, 1
  br i1 %pick_eq85, label %case_body_084, label %case_next_086

case_body_084:                                    ; preds = %pick_done79
  %50 = load i8, ptr %error_count, align 1
  %51 = load i64, ptr %error_count, align 4
  %52 = add i64 %51, 1
  store i64 %52, ptr %error_count, align 4
  br label %pick_done92

case_next_086:                                    ; preds = %pick_done79
  %pick_eq88 = icmp eq i8 %err83, 2
  br i1 %pick_eq88, label %case_body_187, label %case_next_189

case_body_187:                                    ; preds = %case_next_086
  %53 = load i8, ptr %warning_count, align 1
  %54 = load i64, ptr %warning_count, align 4
  %55 = add i64 %54, 1
  store i64 %55, ptr %warning_count, align 4
  br label %pick_done92

case_next_189:                                    ; preds = %case_next_086
  br i1 true, label %case_body_290, label %case_next_291

case_body_290:                                    ; preds = %case_next_189
  br label %pick_done92

case_next_291:                                    ; preds = %case_next_189
  br label %pick_done92

pick_done92:                                      ; preds = %case_next_291, %case_body_290, %case_body_187, %case_body_084
  call void @puts(ptr @54)
  call void @puts(ptr @55)
  %calltmp93 = call %result_int8 @validateLogLevel(i8 1)
  store %result_int8 %calltmp93, ptr %r9, align 1
  %56 = load %result_int64, ptr %r9, align 4
  %temp94 = alloca %result_int64, align 8
  store %result_int64 %56, ptr %temp94, align 4
  %err_ptr95 = getelementptr inbounds %result_int64, ptr %temp94, i32 0, i32 0
  %err96 = load i8, ptr %err_ptr95, align 1
  %pick_eq98 = icmp eq i8 %err96, 1
  br i1 %pick_eq98, label %case_body_097, label %case_next_099

case_body_097:                                    ; preds = %pick_done92
  %57 = load i8, ptr %error_count, align 1
  %58 = load i64, ptr %error_count, align 4
  %59 = add i64 %58, 1
  store i64 %59, ptr %error_count, align 4
  br label %pick_done105

case_next_099:                                    ; preds = %pick_done92
  %pick_eq101 = icmp eq i8 %err96, 2
  br i1 %pick_eq101, label %case_body_1100, label %case_next_1102

case_body_1100:                                   ; preds = %case_next_099
  %60 = load i8, ptr %warning_count, align 1
  %61 = load i64, ptr %warning_count, align 4
  %62 = add i64 %61, 1
  store i64 %62, ptr %warning_count, align 4
  br label %pick_done105

case_next_1102:                                   ; preds = %case_next_099
  br i1 true, label %case_body_2103, label %case_next_2104

case_body_2103:                                   ; preds = %case_next_1102
  br label %pick_done105

case_next_2104:                                   ; preds = %case_next_1102
  br label %pick_done105

pick_done105:                                     ; preds = %case_next_2104, %case_body_2103, %case_body_1100, %case_body_097
  call void @puts(ptr @56)
  call void @puts(ptr @57)
  %calltmp106 = call %result_int8 @validateLogLevel(i8 99)
  store %result_int8 %calltmp106, ptr %r10, align 1
  %63 = load %result_int64, ptr %r10, align 4
  %temp107 = alloca %result_int64, align 8
  store %result_int64 %63, ptr %temp107, align 4
  %err_ptr108 = getelementptr inbounds %result_int64, ptr %temp107, i32 0, i32 0
  %err109 = load i8, ptr %err_ptr108, align 1
  %pick_eq111 = icmp eq i8 %err109, 1
  br i1 %pick_eq111, label %case_body_0110, label %case_next_0112

case_body_0110:                                   ; preds = %pick_done105
  %64 = load i8, ptr %error_count, align 1
  %65 = load i64, ptr %error_count, align 4
  %66 = add i64 %65, 1
  store i64 %66, ptr %error_count, align 4
  br label %pick_done118

case_next_0112:                                   ; preds = %pick_done105
  %pick_eq114 = icmp eq i8 %err109, 2
  br i1 %pick_eq114, label %case_body_1113, label %case_next_1115

case_body_1113:                                   ; preds = %case_next_0112
  %67 = load i8, ptr %warning_count, align 1
  %68 = load i64, ptr %warning_count, align 4
  %69 = add i64 %68, 1
  store i64 %69, ptr %warning_count, align 4
  br label %pick_done118

case_next_1115:                                   ; preds = %case_next_0112
  br i1 true, label %case_body_2116, label %case_next_2117

case_body_2116:                                   ; preds = %case_next_1115
  br label %pick_done118

case_next_2117:                                   ; preds = %case_next_1115
  br label %pick_done118

pick_done118:                                     ; preds = %case_next_2117, %case_body_2116, %case_body_1113, %case_body_0110
  call void @puts(ptr @58)
  call void @puts(ptr @59)
  %str_buffer = alloca i8, i64 1024, align 1
  %70 = call i32 (ptr, ptr, ...) @sprintf(ptr %str_buffer, ptr @60, ptr @61)
  %next_pos = getelementptr i8, ptr %str_buffer, i32 %70
  %71 = load i8, ptr %error_count, align 1
  %to_i64 = sext i8 %71 to i64
  %72 = call i32 (ptr, ptr, ...) @sprintf(ptr %next_pos, ptr @62, i64 %to_i64)
  %next_pos119 = getelementptr i8, ptr %next_pos, i32 %72
  call void @puts(ptr %str_buffer)
  %str_buffer120 = alloca i8, i64 1024, align 1
  %73 = call i32 (ptr, ptr, ...) @sprintf(ptr %str_buffer120, ptr @63, ptr @64)
  %next_pos121 = getelementptr i8, ptr %str_buffer120, i32 %73
  %74 = load i8, ptr %warning_count, align 1
  %to_i64122 = sext i8 %74 to i64
  %75 = call i32 (ptr, ptr, ...) @sprintf(ptr %next_pos121, ptr @65, i64 %to_i64122)
  %next_pos123 = getelementptr i8, ptr %next_pos121, i32 %75
  call void @puts(ptr %str_buffer120)
  call void @puts(ptr @66)
  %76 = load i8, ptr %error_count, align 1
  %pick_eq125 = icmp eq i8 %76, 0
  br i1 %pick_eq125, label %case_body_0124, label %case_next_0127

case_body_0124:                                   ; preds = %pick_done118
  call void @puts(ptr @67)
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr126 = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr126, align 1
  %val_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 0, ptr %val_ptr, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val

case_next_0127:                                   ; preds = %pick_done118
  br i1 true, label %case_body_1128, label %case_next_1138

case_body_1128:                                   ; preds = %case_next_0127
  %str_buffer129 = alloca i8, i64 1024, align 1
  %77 = call i32 (ptr, ptr, ...) @sprintf(ptr %str_buffer129, ptr @68, ptr @69)
  %next_pos130 = getelementptr i8, ptr %str_buffer129, i32 %77
  %78 = load i8, ptr %error_count, align 1
  %to_i64131 = sext i8 %78 to i64
  %79 = call i32 (ptr, ptr, ...) @sprintf(ptr %next_pos130, ptr @70, i64 %to_i64131)
  %next_pos132 = getelementptr i8, ptr %next_pos130, i32 %79
  %80 = call i32 (ptr, ptr, ...) @sprintf(ptr %next_pos132, ptr @71, ptr @72)
  %next_pos133 = getelementptr i8, ptr %next_pos132, i32 %80
  call void @puts(ptr %str_buffer129)
  %auto_wrap_result134 = alloca %result_int8, align 8
  %err_ptr135 = getelementptr inbounds %result_int8, ptr %auto_wrap_result134, i32 0, i32 0
  store i8 0, ptr %err_ptr135, align 1
  %val_ptr136 = getelementptr inbounds %result_int8, ptr %auto_wrap_result134, i32 0, i32 1
  store i8 1, ptr %val_ptr136, align 1
  %result_val137 = load %result_int8, ptr %auto_wrap_result134, align 1
  ret %result_int8 %result_val137

case_next_1138:                                   ; preds = %case_next_0127
  br label %pick_done139

pick_done139:                                     ; preds = %case_next_1138
  ret %result_int8 zeroinitializer
}

define internal %result_int8 @__user_main() {
entry:
  %validation = alloca %result_int64, align 8
  %calltmp = call %result_int8 @validateConfig()
  store %result_int8 %calltmp, ptr %validation, align 1
  %0 = load %result_int64, ptr %validation, align 4
  %temp = alloca %result_int64, align 8
  store %result_int64 %0, ptr %temp, align 4
  %err_ptr = getelementptr inbounds %result_int64, ptr %temp, i32 0, i32 0
  %err = load i8, ptr %err_ptr, align 1
  %pick_eq = icmp eq i8 %err, 0
  br i1 %pick_eq, label %case_body_0, label %case_next_0

case_body_0:                                      ; preds = %entry
  call void @puts(ptr @73)
  call void @puts(ptr @74)
  br label %pick_done

case_next_0:                                      ; preds = %entry
  br i1 true, label %case_body_1, label %case_next_1

case_body_1:                                      ; preds = %case_next_0
  call void @puts(ptr @75)
  call void @puts(ptr @76)
  br label %pick_done

case_next_1:                                      ; preds = %case_next_0
  br label %pick_done

pick_done:                                        ; preds = %case_next_1, %case_body_1, %case_body_0
  %1 = load %result_int64, ptr %validation, align 4
  %temp1 = alloca %result_int64, align 8
  store %result_int64 %1, ptr %temp1, align 4
  %err_ptr2 = getelementptr inbounds %result_int64, ptr %temp1, i32 0, i32 0
  %err3 = load i8, ptr %err_ptr2, align 1
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr4 = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr4, align 1
  %val_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 %err3, ptr %val_ptr, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val
}

define i64 @main() {
entry:
  call void @__aria_module_init()
  %0 = call %result_int8 @__user_main()
  ret i64 0
}
