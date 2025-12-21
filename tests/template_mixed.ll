; ModuleID = 'tests/template_mixed.aria'
source_filename = "tests/template_mixed.aria"

@str = private unnamed_addr constant [6 x i8] c"Alice\00", align 1
@.str = private constant [7 x i8] c"User: \00"
@.str.1 = private constant [8 x i8] c", Age: \00"
@.fmt_lld = private constant [5 x i8] c"%lld\00"
@.str.2 = private constant [11 x i8] c", Height: \00"
@.fmt_g = private constant [3 x i8] c"%g\00"
@.str.3 = private constant [13 x i8] c"ft, Active: \00"
@.str.4 = private constant [5 x i8] c"true\00"
@.str.5 = private constant [6 x i8] c"false\00"
@.str.6 = private constant [1 x i8] zeroinitializer

define i64 @main() {
entry:
  %name = alloca ptr, align 8
  store ptr @str, ptr %name, align 8
  %age = alloca i64, align 8
  store i64 30, ptr %age, align 4
  %height = alloca double, align 8
  store double 5.700000e+00, ptr %height, align 8
  %active = alloca i1, align 1
  store i1 true, ptr %active, align 1
  %profile = alloca ptr, align 8
  %aria_str = alloca { ptr, i64 }, align 8
  %data_ptr = getelementptr inbounds nuw { ptr, i64 }, ptr %aria_str, i32 0, i32 0
  store ptr @.str, ptr %data_ptr, align 8
  %length_ptr = getelementptr inbounds nuw { ptr, i64 }, ptr %aria_str, i32 0, i32 1
  store i64 6, ptr %length_ptr, align 4
  %aria_str_val = load { ptr, i64 }, ptr %aria_str, align 8
  %name1 = load ptr, ptr %name, align 8
  %0 = call i64 @strlen(ptr %name1)
  %ptr_aria_str = alloca { ptr, i64 }, align 8
  %1 = getelementptr inbounds nuw { ptr, i64 }, ptr %ptr_aria_str, i32 0, i32 0
  store ptr %name1, ptr %1, align 8
  %2 = getelementptr inbounds nuw { ptr, i64 }, ptr %ptr_aria_str, i32 0, i32 1
  store i64 %0, ptr %2, align 4
  %ptr_str_val = load { ptr, i64 }, ptr %ptr_aria_str, align 8
  %3 = call ptr @aria_string_concat({ ptr, i64 } %aria_str_val, { ptr, i64 } %ptr_str_val)
  %result_val_ptr = getelementptr inbounds nuw { ptr, ptr }, ptr %3, i32 0, i32 0
  %aria_str_ptr = load ptr, ptr %result_val_ptr, align 8
  %concat_str = load { ptr, i64 }, ptr %aria_str_ptr, align 8
  %aria_str2 = alloca { ptr, i64 }, align 8
  %data_ptr3 = getelementptr inbounds nuw { ptr, i64 }, ptr %aria_str2, i32 0, i32 0
  store ptr @.str.1, ptr %data_ptr3, align 8
  %length_ptr4 = getelementptr inbounds nuw { ptr, i64 }, ptr %aria_str2, i32 0, i32 1
  store i64 7, ptr %length_ptr4, align 4
  %aria_str_val5 = load { ptr, i64 }, ptr %aria_str2, align 8
  %4 = call ptr @aria_string_concat({ ptr, i64 } %concat_str, { ptr, i64 } %aria_str_val5)
  %result_val_ptr6 = getelementptr inbounds nuw { ptr, ptr }, ptr %4, i32 0, i32 0
  %aria_str_ptr7 = load ptr, ptr %result_val_ptr6, align 8
  %concat_str8 = load { ptr, i64 }, ptr %aria_str_ptr7, align 8
  %age9 = load i64, ptr %age, align 4
  %int_str_buffer = alloca [32 x i8], align 1
  %5 = call i32 (ptr, ptr, ...) @sprintf(ptr %int_str_buffer, ptr @.fmt_lld, i64 %age9)
  %6 = call i64 @strlen(ptr %int_str_buffer)
  %int_aria_str = alloca { ptr, i64 }, align 8
  %7 = getelementptr inbounds nuw { ptr, i64 }, ptr %int_aria_str, i32 0, i32 0
  store ptr %int_str_buffer, ptr %7, align 8
  %8 = getelementptr inbounds nuw { ptr, i64 }, ptr %int_aria_str, i32 0, i32 1
  store i64 %6, ptr %8, align 4
  %int_str_val = load { ptr, i64 }, ptr %int_aria_str, align 8
  %9 = call ptr @aria_string_concat({ ptr, i64 } %concat_str8, { ptr, i64 } %int_str_val)
  %result_val_ptr10 = getelementptr inbounds nuw { ptr, ptr }, ptr %9, i32 0, i32 0
  %aria_str_ptr11 = load ptr, ptr %result_val_ptr10, align 8
  %concat_str12 = load { ptr, i64 }, ptr %aria_str_ptr11, align 8
  %aria_str13 = alloca { ptr, i64 }, align 8
  %data_ptr14 = getelementptr inbounds nuw { ptr, i64 }, ptr %aria_str13, i32 0, i32 0
  store ptr @.str.2, ptr %data_ptr14, align 8
  %length_ptr15 = getelementptr inbounds nuw { ptr, i64 }, ptr %aria_str13, i32 0, i32 1
  store i64 10, ptr %length_ptr15, align 4
  %aria_str_val16 = load { ptr, i64 }, ptr %aria_str13, align 8
  %10 = call ptr @aria_string_concat({ ptr, i64 } %concat_str12, { ptr, i64 } %aria_str_val16)
  %result_val_ptr17 = getelementptr inbounds nuw { ptr, ptr }, ptr %10, i32 0, i32 0
  %aria_str_ptr18 = load ptr, ptr %result_val_ptr17, align 8
  %concat_str19 = load { ptr, i64 }, ptr %aria_str_ptr18, align 8
  %height20 = load double, ptr %height, align 8
  %float_str_buffer = alloca [64 x i8], align 1
  %11 = call i32 (ptr, ptr, ...) @sprintf(ptr %float_str_buffer, ptr @.fmt_g, double %height20)
  %12 = call i64 @strlen(ptr %float_str_buffer)
  %float_aria_str = alloca { ptr, i64 }, align 8
  %13 = getelementptr inbounds nuw { ptr, i64 }, ptr %float_aria_str, i32 0, i32 0
  store ptr %float_str_buffer, ptr %13, align 8
  %14 = getelementptr inbounds nuw { ptr, i64 }, ptr %float_aria_str, i32 0, i32 1
  store i64 %12, ptr %14, align 4
  %float_str_val = load { ptr, i64 }, ptr %float_aria_str, align 8
  %15 = call ptr @aria_string_concat({ ptr, i64 } %concat_str19, { ptr, i64 } %float_str_val)
  %result_val_ptr21 = getelementptr inbounds nuw { ptr, ptr }, ptr %15, i32 0, i32 0
  %aria_str_ptr22 = load ptr, ptr %result_val_ptr21, align 8
  %concat_str23 = load { ptr, i64 }, ptr %aria_str_ptr22, align 8
  %aria_str24 = alloca { ptr, i64 }, align 8
  %data_ptr25 = getelementptr inbounds nuw { ptr, i64 }, ptr %aria_str24, i32 0, i32 0
  store ptr @.str.3, ptr %data_ptr25, align 8
  %length_ptr26 = getelementptr inbounds nuw { ptr, i64 }, ptr %aria_str24, i32 0, i32 1
  store i64 12, ptr %length_ptr26, align 4
  %aria_str_val27 = load { ptr, i64 }, ptr %aria_str24, align 8
  %16 = call ptr @aria_string_concat({ ptr, i64 } %concat_str23, { ptr, i64 } %aria_str_val27)
  %result_val_ptr28 = getelementptr inbounds nuw { ptr, ptr }, ptr %16, i32 0, i32 0
  %aria_str_ptr29 = load ptr, ptr %result_val_ptr28, align 8
  %concat_str30 = load { ptr, i64 }, ptr %aria_str_ptr29, align 8
  %active31 = load i1, ptr %active, align 1
  br i1 %active31, label %bool_true, label %bool_false

bool_true:                                        ; preds = %entry
  %aria_str32 = alloca { ptr, i64 }, align 8
  %data_ptr33 = getelementptr inbounds nuw { ptr, i64 }, ptr %aria_str32, i32 0, i32 0
  store ptr @.str.4, ptr %data_ptr33, align 8
  %length_ptr34 = getelementptr inbounds nuw { ptr, i64 }, ptr %aria_str32, i32 0, i32 1
  store i64 4, ptr %length_ptr34, align 4
  %aria_str_val35 = load { ptr, i64 }, ptr %aria_str32, align 8
  br label %bool_merge

bool_false:                                       ; preds = %entry
  %aria_str36 = alloca { ptr, i64 }, align 8
  %data_ptr37 = getelementptr inbounds nuw { ptr, i64 }, ptr %aria_str36, i32 0, i32 0
  store ptr @.str.5, ptr %data_ptr37, align 8
  %length_ptr38 = getelementptr inbounds nuw { ptr, i64 }, ptr %aria_str36, i32 0, i32 1
  store i64 5, ptr %length_ptr38, align 4
  %aria_str_val39 = load { ptr, i64 }, ptr %aria_str36, align 8
  br label %bool_merge

bool_merge:                                       ; preds = %bool_false, %bool_true
  %bool_str = phi { ptr, i64 } [ %aria_str_val35, %bool_true ], [ %aria_str_val39, %bool_false ]
  %17 = call ptr @aria_string_concat({ ptr, i64 } %concat_str30, { ptr, i64 } %bool_str)
  %result_val_ptr40 = getelementptr inbounds nuw { ptr, ptr }, ptr %17, i32 0, i32 0
  %aria_str_ptr41 = load ptr, ptr %result_val_ptr40, align 8
  %concat_str42 = load { ptr, i64 }, ptr %aria_str_ptr41, align 8
  %aria_str43 = alloca { ptr, i64 }, align 8
  %data_ptr44 = getelementptr inbounds nuw { ptr, i64 }, ptr %aria_str43, i32 0, i32 0
  store ptr @.str.6, ptr %data_ptr44, align 8
  %length_ptr45 = getelementptr inbounds nuw { ptr, i64 }, ptr %aria_str43, i32 0, i32 1
  store i64 0, ptr %length_ptr45, align 4
  %aria_str_val46 = load { ptr, i64 }, ptr %aria_str43, align 8
  %18 = call ptr @aria_string_concat({ ptr, i64 } %concat_str42, { ptr, i64 } %aria_str_val46)
  %result_val_ptr47 = getelementptr inbounds nuw { ptr, ptr }, ptr %18, i32 0, i32 0
  %aria_str_ptr48 = load ptr, ptr %result_val_ptr47, align 8
  %concat_str49 = load { ptr, i64 }, ptr %aria_str_ptr48, align 8
  %result_tmp = alloca { ptr, i64 }, align 8
  store { ptr, i64 } %concat_str49, ptr %result_tmp, align 8
  %19 = getelementptr inbounds nuw { ptr, i64 }, ptr %result_tmp, i32 0, i32 0
  %result_str_ptr = load ptr, ptr %19, align 8
  store ptr %result_str_ptr, ptr %profile, align 8
  ret i64 0
}

declare ptr @aria_string_concat({ ptr, i64 }, { ptr, i64 })

declare i64 @strlen(ptr)

declare i32 @sprintf(ptr, ptr, ...)

define i32 @__template_mixed_init() {
entry:
  ret i32 0
}
