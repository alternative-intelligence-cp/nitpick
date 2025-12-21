; ModuleID = 'tests/template_floats.aria'
source_filename = "tests/template_floats.aria"

@.str = private constant [21 x i8] c"Pi is approximately \00"
@.fmt_g = private constant [3 x i8] c"%g\00"
@.str.1 = private constant [1 x i8] zeroinitializer
@.str.2 = private constant [20 x i8] c"e is approximately \00"
@.fmt_g.3 = private constant [3 x i8] c"%g\00"
@.str.4 = private constant [1 x i8] zeroinitializer
@.str.5 = private constant [10 x i8] c"Pi + e = \00"
@.fmt_g.6 = private constant [3 x i8] c"%g\00"
@.str.7 = private constant [1 x i8] zeroinitializer

define i64 @main() {
entry:
  %pi = alloca double, align 8
  store double 3.141590e+00, ptr %pi, align 8
  %e = alloca double, align 8
  store double 2.718280e+00, ptr %e, align 8
  %msg1 = alloca ptr, align 8
  %aria_str = alloca { ptr, i64 }, align 8
  %data_ptr = getelementptr inbounds nuw { ptr, i64 }, ptr %aria_str, i32 0, i32 0
  store ptr @.str, ptr %data_ptr, align 8
  %length_ptr = getelementptr inbounds nuw { ptr, i64 }, ptr %aria_str, i32 0, i32 1
  store i64 20, ptr %length_ptr, align 4
  %aria_str_val = load { ptr, i64 }, ptr %aria_str, align 8
  %pi1 = load double, ptr %pi, align 8
  %float_str_buffer = alloca [64 x i8], align 1
  %0 = call i32 (ptr, ptr, ...) @sprintf(ptr %float_str_buffer, ptr @.fmt_g, double %pi1)
  %1 = call i64 @strlen(ptr %float_str_buffer)
  %float_aria_str = alloca { ptr, i64 }, align 8
  %2 = getelementptr inbounds nuw { ptr, i64 }, ptr %float_aria_str, i32 0, i32 0
  store ptr %float_str_buffer, ptr %2, align 8
  %3 = getelementptr inbounds nuw { ptr, i64 }, ptr %float_aria_str, i32 0, i32 1
  store i64 %1, ptr %3, align 4
  %float_str_val = load { ptr, i64 }, ptr %float_aria_str, align 8
  %4 = call ptr @aria_string_concat({ ptr, i64 } %aria_str_val, { ptr, i64 } %float_str_val)
  %result_val_ptr = getelementptr inbounds nuw { ptr, ptr }, ptr %4, i32 0, i32 0
  %aria_str_ptr = load ptr, ptr %result_val_ptr, align 8
  %concat_str = load { ptr, i64 }, ptr %aria_str_ptr, align 8
  %aria_str2 = alloca { ptr, i64 }, align 8
  %data_ptr3 = getelementptr inbounds nuw { ptr, i64 }, ptr %aria_str2, i32 0, i32 0
  store ptr @.str.1, ptr %data_ptr3, align 8
  %length_ptr4 = getelementptr inbounds nuw { ptr, i64 }, ptr %aria_str2, i32 0, i32 1
  store i64 0, ptr %length_ptr4, align 4
  %aria_str_val5 = load { ptr, i64 }, ptr %aria_str2, align 8
  %5 = call ptr @aria_string_concat({ ptr, i64 } %concat_str, { ptr, i64 } %aria_str_val5)
  %result_val_ptr6 = getelementptr inbounds nuw { ptr, ptr }, ptr %5, i32 0, i32 0
  %aria_str_ptr7 = load ptr, ptr %result_val_ptr6, align 8
  %concat_str8 = load { ptr, i64 }, ptr %aria_str_ptr7, align 8
  %result_tmp = alloca { ptr, i64 }, align 8
  store { ptr, i64 } %concat_str8, ptr %result_tmp, align 8
  %6 = getelementptr inbounds nuw { ptr, i64 }, ptr %result_tmp, i32 0, i32 0
  %result_str_ptr = load ptr, ptr %6, align 8
  store ptr %result_str_ptr, ptr %msg1, align 8
  %msg2 = alloca ptr, align 8
  %aria_str9 = alloca { ptr, i64 }, align 8
  %data_ptr10 = getelementptr inbounds nuw { ptr, i64 }, ptr %aria_str9, i32 0, i32 0
  store ptr @.str.2, ptr %data_ptr10, align 8
  %length_ptr11 = getelementptr inbounds nuw { ptr, i64 }, ptr %aria_str9, i32 0, i32 1
  store i64 19, ptr %length_ptr11, align 4
  %aria_str_val12 = load { ptr, i64 }, ptr %aria_str9, align 8
  %e13 = load double, ptr %e, align 8
  %float_str_buffer14 = alloca [64 x i8], align 1
  %7 = call i32 (ptr, ptr, ...) @sprintf(ptr %float_str_buffer14, ptr @.fmt_g.3, double %e13)
  %8 = call i64 @strlen(ptr %float_str_buffer14)
  %float_aria_str15 = alloca { ptr, i64 }, align 8
  %9 = getelementptr inbounds nuw { ptr, i64 }, ptr %float_aria_str15, i32 0, i32 0
  store ptr %float_str_buffer14, ptr %9, align 8
  %10 = getelementptr inbounds nuw { ptr, i64 }, ptr %float_aria_str15, i32 0, i32 1
  store i64 %8, ptr %10, align 4
  %float_str_val16 = load { ptr, i64 }, ptr %float_aria_str15, align 8
  %11 = call ptr @aria_string_concat({ ptr, i64 } %aria_str_val12, { ptr, i64 } %float_str_val16)
  %result_val_ptr17 = getelementptr inbounds nuw { ptr, ptr }, ptr %11, i32 0, i32 0
  %aria_str_ptr18 = load ptr, ptr %result_val_ptr17, align 8
  %concat_str19 = load { ptr, i64 }, ptr %aria_str_ptr18, align 8
  %aria_str20 = alloca { ptr, i64 }, align 8
  %data_ptr21 = getelementptr inbounds nuw { ptr, i64 }, ptr %aria_str20, i32 0, i32 0
  store ptr @.str.4, ptr %data_ptr21, align 8
  %length_ptr22 = getelementptr inbounds nuw { ptr, i64 }, ptr %aria_str20, i32 0, i32 1
  store i64 0, ptr %length_ptr22, align 4
  %aria_str_val23 = load { ptr, i64 }, ptr %aria_str20, align 8
  %12 = call ptr @aria_string_concat({ ptr, i64 } %concat_str19, { ptr, i64 } %aria_str_val23)
  %result_val_ptr24 = getelementptr inbounds nuw { ptr, ptr }, ptr %12, i32 0, i32 0
  %aria_str_ptr25 = load ptr, ptr %result_val_ptr24, align 8
  %concat_str26 = load { ptr, i64 }, ptr %aria_str_ptr25, align 8
  %result_tmp27 = alloca { ptr, i64 }, align 8
  store { ptr, i64 } %concat_str26, ptr %result_tmp27, align 8
  %13 = getelementptr inbounds nuw { ptr, i64 }, ptr %result_tmp27, i32 0, i32 0
  %result_str_ptr28 = load ptr, ptr %13, align 8
  store ptr %result_str_ptr28, ptr %msg2, align 8
  %msg3 = alloca ptr, align 8
  %aria_str29 = alloca { ptr, i64 }, align 8
  %data_ptr30 = getelementptr inbounds nuw { ptr, i64 }, ptr %aria_str29, i32 0, i32 0
  store ptr @.str.5, ptr %data_ptr30, align 8
  %length_ptr31 = getelementptr inbounds nuw { ptr, i64 }, ptr %aria_str29, i32 0, i32 1
  store i64 9, ptr %length_ptr31, align 4
  %aria_str_val32 = load { ptr, i64 }, ptr %aria_str29, align 8
  %pi33 = load double, ptr %pi, align 8
  %e34 = load double, ptr %e, align 8
  %addtmp = fadd double %pi33, %e34
  %float_str_buffer35 = alloca [64 x i8], align 1
  %14 = call i32 (ptr, ptr, ...) @sprintf(ptr %float_str_buffer35, ptr @.fmt_g.6, double %addtmp)
  %15 = call i64 @strlen(ptr %float_str_buffer35)
  %float_aria_str36 = alloca { ptr, i64 }, align 8
  %16 = getelementptr inbounds nuw { ptr, i64 }, ptr %float_aria_str36, i32 0, i32 0
  store ptr %float_str_buffer35, ptr %16, align 8
  %17 = getelementptr inbounds nuw { ptr, i64 }, ptr %float_aria_str36, i32 0, i32 1
  store i64 %15, ptr %17, align 4
  %float_str_val37 = load { ptr, i64 }, ptr %float_aria_str36, align 8
  %18 = call ptr @aria_string_concat({ ptr, i64 } %aria_str_val32, { ptr, i64 } %float_str_val37)
  %result_val_ptr38 = getelementptr inbounds nuw { ptr, ptr }, ptr %18, i32 0, i32 0
  %aria_str_ptr39 = load ptr, ptr %result_val_ptr38, align 8
  %concat_str40 = load { ptr, i64 }, ptr %aria_str_ptr39, align 8
  %aria_str41 = alloca { ptr, i64 }, align 8
  %data_ptr42 = getelementptr inbounds nuw { ptr, i64 }, ptr %aria_str41, i32 0, i32 0
  store ptr @.str.7, ptr %data_ptr42, align 8
  %length_ptr43 = getelementptr inbounds nuw { ptr, i64 }, ptr %aria_str41, i32 0, i32 1
  store i64 0, ptr %length_ptr43, align 4
  %aria_str_val44 = load { ptr, i64 }, ptr %aria_str41, align 8
  %19 = call ptr @aria_string_concat({ ptr, i64 } %concat_str40, { ptr, i64 } %aria_str_val44)
  %result_val_ptr45 = getelementptr inbounds nuw { ptr, ptr }, ptr %19, i32 0, i32 0
  %aria_str_ptr46 = load ptr, ptr %result_val_ptr45, align 8
  %concat_str47 = load { ptr, i64 }, ptr %aria_str_ptr46, align 8
  %result_tmp48 = alloca { ptr, i64 }, align 8
  store { ptr, i64 } %concat_str47, ptr %result_tmp48, align 8
  %20 = getelementptr inbounds nuw { ptr, i64 }, ptr %result_tmp48, i32 0, i32 0
  %result_str_ptr49 = load ptr, ptr %20, align 8
  store ptr %result_str_ptr49, ptr %msg3, align 8
  ret i64 0
}

declare ptr @aria_string_concat({ ptr, i64 }, { ptr, i64 })

declare i32 @sprintf(ptr, ptr, ...)

declare i64 @strlen(ptr)

define i32 @__template_floats_init() {
entry:
  ret i32 0
}
