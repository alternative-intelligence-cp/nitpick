; ModuleID = 'tests/template_bools.aria'
source_filename = "tests/template_bools.aria"

@.str = private constant [8 x i8] c"Ready: \00"
@.str.1 = private constant [5 x i8] c"true\00"
@.str.2 = private constant [6 x i8] c"false\00"
@.str.3 = private constant [1 x i8] zeroinitializer
@.str.4 = private constant [7 x i8] c"Done: \00"
@.str.5 = private constant [5 x i8] c"true\00"
@.str.6 = private constant [6 x i8] c"false\00"
@.str.7 = private constant [1 x i8] zeroinitializer

define i64 @main() {
entry:
  %isReady = alloca i1, align 1
  store i1 true, ptr %isReady, align 1
  %isDone = alloca i1, align 1
  store i1 false, ptr %isDone, align 1
  %msg1 = alloca ptr, align 8
  %aria_str = alloca { ptr, i64 }, align 8
  %data_ptr = getelementptr inbounds nuw { ptr, i64 }, ptr %aria_str, i32 0, i32 0
  store ptr @.str, ptr %data_ptr, align 8
  %length_ptr = getelementptr inbounds nuw { ptr, i64 }, ptr %aria_str, i32 0, i32 1
  store i64 7, ptr %length_ptr, align 4
  %aria_str_val = load { ptr, i64 }, ptr %aria_str, align 8
  %isReady1 = load i1, ptr %isReady, align 1
  br i1 %isReady1, label %bool_true, label %bool_false

bool_true:                                        ; preds = %entry
  %aria_str2 = alloca { ptr, i64 }, align 8
  %data_ptr3 = getelementptr inbounds nuw { ptr, i64 }, ptr %aria_str2, i32 0, i32 0
  store ptr @.str.1, ptr %data_ptr3, align 8
  %length_ptr4 = getelementptr inbounds nuw { ptr, i64 }, ptr %aria_str2, i32 0, i32 1
  store i64 4, ptr %length_ptr4, align 4
  %aria_str_val5 = load { ptr, i64 }, ptr %aria_str2, align 8
  br label %bool_merge

bool_false:                                       ; preds = %entry
  %aria_str6 = alloca { ptr, i64 }, align 8
  %data_ptr7 = getelementptr inbounds nuw { ptr, i64 }, ptr %aria_str6, i32 0, i32 0
  store ptr @.str.2, ptr %data_ptr7, align 8
  %length_ptr8 = getelementptr inbounds nuw { ptr, i64 }, ptr %aria_str6, i32 0, i32 1
  store i64 5, ptr %length_ptr8, align 4
  %aria_str_val9 = load { ptr, i64 }, ptr %aria_str6, align 8
  br label %bool_merge

bool_merge:                                       ; preds = %bool_false, %bool_true
  %bool_str = phi { ptr, i64 } [ %aria_str_val5, %bool_true ], [ %aria_str_val9, %bool_false ]
  %0 = call ptr @aria_string_concat({ ptr, i64 } %aria_str_val, { ptr, i64 } %bool_str)
  %result_val_ptr = getelementptr inbounds nuw { ptr, ptr }, ptr %0, i32 0, i32 0
  %aria_str_ptr = load ptr, ptr %result_val_ptr, align 8
  %concat_str = load { ptr, i64 }, ptr %aria_str_ptr, align 8
  %aria_str10 = alloca { ptr, i64 }, align 8
  %data_ptr11 = getelementptr inbounds nuw { ptr, i64 }, ptr %aria_str10, i32 0, i32 0
  store ptr @.str.3, ptr %data_ptr11, align 8
  %length_ptr12 = getelementptr inbounds nuw { ptr, i64 }, ptr %aria_str10, i32 0, i32 1
  store i64 0, ptr %length_ptr12, align 4
  %aria_str_val13 = load { ptr, i64 }, ptr %aria_str10, align 8
  %1 = call ptr @aria_string_concat({ ptr, i64 } %concat_str, { ptr, i64 } %aria_str_val13)
  %result_val_ptr14 = getelementptr inbounds nuw { ptr, ptr }, ptr %1, i32 0, i32 0
  %aria_str_ptr15 = load ptr, ptr %result_val_ptr14, align 8
  %concat_str16 = load { ptr, i64 }, ptr %aria_str_ptr15, align 8
  %result_tmp = alloca { ptr, i64 }, align 8
  store { ptr, i64 } %concat_str16, ptr %result_tmp, align 8
  %2 = getelementptr inbounds nuw { ptr, i64 }, ptr %result_tmp, i32 0, i32 0
  %result_str_ptr = load ptr, ptr %2, align 8
  store ptr %result_str_ptr, ptr %msg1, align 8
  %msg2 = alloca ptr, align 8
  %aria_str17 = alloca { ptr, i64 }, align 8
  %data_ptr18 = getelementptr inbounds nuw { ptr, i64 }, ptr %aria_str17, i32 0, i32 0
  store ptr @.str.4, ptr %data_ptr18, align 8
  %length_ptr19 = getelementptr inbounds nuw { ptr, i64 }, ptr %aria_str17, i32 0, i32 1
  store i64 6, ptr %length_ptr19, align 4
  %aria_str_val20 = load { ptr, i64 }, ptr %aria_str17, align 8
  %isDone21 = load i1, ptr %isDone, align 1
  br i1 %isDone21, label %bool_true22, label %bool_false23

bool_true22:                                      ; preds = %bool_merge
  %aria_str25 = alloca { ptr, i64 }, align 8
  %data_ptr26 = getelementptr inbounds nuw { ptr, i64 }, ptr %aria_str25, i32 0, i32 0
  store ptr @.str.5, ptr %data_ptr26, align 8
  %length_ptr27 = getelementptr inbounds nuw { ptr, i64 }, ptr %aria_str25, i32 0, i32 1
  store i64 4, ptr %length_ptr27, align 4
  %aria_str_val28 = load { ptr, i64 }, ptr %aria_str25, align 8
  br label %bool_merge24

bool_false23:                                     ; preds = %bool_merge
  %aria_str29 = alloca { ptr, i64 }, align 8
  %data_ptr30 = getelementptr inbounds nuw { ptr, i64 }, ptr %aria_str29, i32 0, i32 0
  store ptr @.str.6, ptr %data_ptr30, align 8
  %length_ptr31 = getelementptr inbounds nuw { ptr, i64 }, ptr %aria_str29, i32 0, i32 1
  store i64 5, ptr %length_ptr31, align 4
  %aria_str_val32 = load { ptr, i64 }, ptr %aria_str29, align 8
  br label %bool_merge24

bool_merge24:                                     ; preds = %bool_false23, %bool_true22
  %bool_str33 = phi { ptr, i64 } [ %aria_str_val28, %bool_true22 ], [ %aria_str_val32, %bool_false23 ]
  %3 = call ptr @aria_string_concat({ ptr, i64 } %aria_str_val20, { ptr, i64 } %bool_str33)
  %result_val_ptr34 = getelementptr inbounds nuw { ptr, ptr }, ptr %3, i32 0, i32 0
  %aria_str_ptr35 = load ptr, ptr %result_val_ptr34, align 8
  %concat_str36 = load { ptr, i64 }, ptr %aria_str_ptr35, align 8
  %aria_str37 = alloca { ptr, i64 }, align 8
  %data_ptr38 = getelementptr inbounds nuw { ptr, i64 }, ptr %aria_str37, i32 0, i32 0
  store ptr @.str.7, ptr %data_ptr38, align 8
  %length_ptr39 = getelementptr inbounds nuw { ptr, i64 }, ptr %aria_str37, i32 0, i32 1
  store i64 0, ptr %length_ptr39, align 4
  %aria_str_val40 = load { ptr, i64 }, ptr %aria_str37, align 8
  %4 = call ptr @aria_string_concat({ ptr, i64 } %concat_str36, { ptr, i64 } %aria_str_val40)
  %result_val_ptr41 = getelementptr inbounds nuw { ptr, ptr }, ptr %4, i32 0, i32 0
  %aria_str_ptr42 = load ptr, ptr %result_val_ptr41, align 8
  %concat_str43 = load { ptr, i64 }, ptr %aria_str_ptr42, align 8
  %result_tmp44 = alloca { ptr, i64 }, align 8
  store { ptr, i64 } %concat_str43, ptr %result_tmp44, align 8
  %5 = getelementptr inbounds nuw { ptr, i64 }, ptr %result_tmp44, i32 0, i32 0
  %result_str_ptr45 = load ptr, ptr %5, align 8
  store ptr %result_str_ptr45, ptr %msg2, align 8
  ret i64 0
}

declare ptr @aria_string_concat({ ptr, i64 }, { ptr, i64 })

define i32 @__template_bools_init() {
entry:
  ret i32 0
}
