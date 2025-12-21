; ModuleID = 'tests/template_integers.aria'
source_filename = "tests/template_integers.aria"

@.str = private constant [8 x i8] c"Value: \00"
@.fmt_lld = private constant [5 x i8] c"%lld\00"
@.str.1 = private constant [1 x i8] zeroinitializer
@.str.2 = private constant [6 x i8] c"Sum: \00"
@.fmt_lld.3 = private constant [5 x i8] c"%lld\00"
@.str.4 = private constant [4 x i8] c" + \00"
@.fmt_lld.5 = private constant [5 x i8] c"%lld\00"
@.str.6 = private constant [4 x i8] c" = \00"
@.fmt_lld.7 = private constant [5 x i8] c"%lld\00"
@.str.8 = private constant [1 x i8] zeroinitializer
@.str.9 = private constant [10 x i8] c"Product: \00"
@.fmt_lld.10 = private constant [5 x i8] c"%lld\00"
@.str.11 = private constant [4 x i8] c" * \00"
@.fmt_lld.12 = private constant [5 x i8] c"%lld\00"
@.str.13 = private constant [4 x i8] c" = \00"
@.fmt_lld.14 = private constant [5 x i8] c"%lld\00"
@.str.15 = private constant [1 x i8] zeroinitializer

define i64 @main() {
entry:
  %x = alloca i64, align 8
  store i64 42, ptr %x, align 4
  %y = alloca i64, align 8
  store i64 17, ptr %y, align 4
  %msg1 = alloca ptr, align 8
  %aria_str = alloca { ptr, i64 }, align 8
  %data_ptr = getelementptr inbounds nuw { ptr, i64 }, ptr %aria_str, i32 0, i32 0
  store ptr @.str, ptr %data_ptr, align 8
  %length_ptr = getelementptr inbounds nuw { ptr, i64 }, ptr %aria_str, i32 0, i32 1
  store i64 7, ptr %length_ptr, align 4
  %aria_str_val = load { ptr, i64 }, ptr %aria_str, align 8
  %x1 = load i64, ptr %x, align 4
  %int_str_buffer = alloca [32 x i8], align 1
  %0 = call i32 (ptr, ptr, ...) @sprintf(ptr %int_str_buffer, ptr @.fmt_lld, i64 %x1)
  %1 = call i64 @strlen(ptr %int_str_buffer)
  %int_aria_str = alloca { ptr, i64 }, align 8
  %2 = getelementptr inbounds nuw { ptr, i64 }, ptr %int_aria_str, i32 0, i32 0
  store ptr %int_str_buffer, ptr %2, align 8
  %3 = getelementptr inbounds nuw { ptr, i64 }, ptr %int_aria_str, i32 0, i32 1
  store i64 %1, ptr %3, align 4
  %int_str_val = load { ptr, i64 }, ptr %int_aria_str, align 8
  %4 = call ptr @aria_string_concat({ ptr, i64 } %aria_str_val, { ptr, i64 } %int_str_val)
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
  store i64 5, ptr %length_ptr11, align 4
  %aria_str_val12 = load { ptr, i64 }, ptr %aria_str9, align 8
  %x13 = load i64, ptr %x, align 4
  %int_str_buffer14 = alloca [32 x i8], align 1
  %7 = call i32 (ptr, ptr, ...) @sprintf(ptr %int_str_buffer14, ptr @.fmt_lld.3, i64 %x13)
  %8 = call i64 @strlen(ptr %int_str_buffer14)
  %int_aria_str15 = alloca { ptr, i64 }, align 8
  %9 = getelementptr inbounds nuw { ptr, i64 }, ptr %int_aria_str15, i32 0, i32 0
  store ptr %int_str_buffer14, ptr %9, align 8
  %10 = getelementptr inbounds nuw { ptr, i64 }, ptr %int_aria_str15, i32 0, i32 1
  store i64 %8, ptr %10, align 4
  %int_str_val16 = load { ptr, i64 }, ptr %int_aria_str15, align 8
  %11 = call ptr @aria_string_concat({ ptr, i64 } %aria_str_val12, { ptr, i64 } %int_str_val16)
  %result_val_ptr17 = getelementptr inbounds nuw { ptr, ptr }, ptr %11, i32 0, i32 0
  %aria_str_ptr18 = load ptr, ptr %result_val_ptr17, align 8
  %concat_str19 = load { ptr, i64 }, ptr %aria_str_ptr18, align 8
  %aria_str20 = alloca { ptr, i64 }, align 8
  %data_ptr21 = getelementptr inbounds nuw { ptr, i64 }, ptr %aria_str20, i32 0, i32 0
  store ptr @.str.4, ptr %data_ptr21, align 8
  %length_ptr22 = getelementptr inbounds nuw { ptr, i64 }, ptr %aria_str20, i32 0, i32 1
  store i64 3, ptr %length_ptr22, align 4
  %aria_str_val23 = load { ptr, i64 }, ptr %aria_str20, align 8
  %12 = call ptr @aria_string_concat({ ptr, i64 } %concat_str19, { ptr, i64 } %aria_str_val23)
  %result_val_ptr24 = getelementptr inbounds nuw { ptr, ptr }, ptr %12, i32 0, i32 0
  %aria_str_ptr25 = load ptr, ptr %result_val_ptr24, align 8
  %concat_str26 = load { ptr, i64 }, ptr %aria_str_ptr25, align 8
  %y27 = load i64, ptr %y, align 4
  %int_str_buffer28 = alloca [32 x i8], align 1
  %13 = call i32 (ptr, ptr, ...) @sprintf(ptr %int_str_buffer28, ptr @.fmt_lld.5, i64 %y27)
  %14 = call i64 @strlen(ptr %int_str_buffer28)
  %int_aria_str29 = alloca { ptr, i64 }, align 8
  %15 = getelementptr inbounds nuw { ptr, i64 }, ptr %int_aria_str29, i32 0, i32 0
  store ptr %int_str_buffer28, ptr %15, align 8
  %16 = getelementptr inbounds nuw { ptr, i64 }, ptr %int_aria_str29, i32 0, i32 1
  store i64 %14, ptr %16, align 4
  %int_str_val30 = load { ptr, i64 }, ptr %int_aria_str29, align 8
  %17 = call ptr @aria_string_concat({ ptr, i64 } %concat_str26, { ptr, i64 } %int_str_val30)
  %result_val_ptr31 = getelementptr inbounds nuw { ptr, ptr }, ptr %17, i32 0, i32 0
  %aria_str_ptr32 = load ptr, ptr %result_val_ptr31, align 8
  %concat_str33 = load { ptr, i64 }, ptr %aria_str_ptr32, align 8
  %aria_str34 = alloca { ptr, i64 }, align 8
  %data_ptr35 = getelementptr inbounds nuw { ptr, i64 }, ptr %aria_str34, i32 0, i32 0
  store ptr @.str.6, ptr %data_ptr35, align 8
  %length_ptr36 = getelementptr inbounds nuw { ptr, i64 }, ptr %aria_str34, i32 0, i32 1
  store i64 3, ptr %length_ptr36, align 4
  %aria_str_val37 = load { ptr, i64 }, ptr %aria_str34, align 8
  %18 = call ptr @aria_string_concat({ ptr, i64 } %concat_str33, { ptr, i64 } %aria_str_val37)
  %result_val_ptr38 = getelementptr inbounds nuw { ptr, ptr }, ptr %18, i32 0, i32 0
  %aria_str_ptr39 = load ptr, ptr %result_val_ptr38, align 8
  %concat_str40 = load { ptr, i64 }, ptr %aria_str_ptr39, align 8
  %x41 = load i64, ptr %x, align 4
  %y42 = load i64, ptr %y, align 4
  %addtmp = add i64 %x41, %y42
  %int_str_buffer43 = alloca [32 x i8], align 1
  %19 = call i32 (ptr, ptr, ...) @sprintf(ptr %int_str_buffer43, ptr @.fmt_lld.7, i64 %addtmp)
  %20 = call i64 @strlen(ptr %int_str_buffer43)
  %int_aria_str44 = alloca { ptr, i64 }, align 8
  %21 = getelementptr inbounds nuw { ptr, i64 }, ptr %int_aria_str44, i32 0, i32 0
  store ptr %int_str_buffer43, ptr %21, align 8
  %22 = getelementptr inbounds nuw { ptr, i64 }, ptr %int_aria_str44, i32 0, i32 1
  store i64 %20, ptr %22, align 4
  %int_str_val45 = load { ptr, i64 }, ptr %int_aria_str44, align 8
  %23 = call ptr @aria_string_concat({ ptr, i64 } %concat_str40, { ptr, i64 } %int_str_val45)
  %result_val_ptr46 = getelementptr inbounds nuw { ptr, ptr }, ptr %23, i32 0, i32 0
  %aria_str_ptr47 = load ptr, ptr %result_val_ptr46, align 8
  %concat_str48 = load { ptr, i64 }, ptr %aria_str_ptr47, align 8
  %aria_str49 = alloca { ptr, i64 }, align 8
  %data_ptr50 = getelementptr inbounds nuw { ptr, i64 }, ptr %aria_str49, i32 0, i32 0
  store ptr @.str.8, ptr %data_ptr50, align 8
  %length_ptr51 = getelementptr inbounds nuw { ptr, i64 }, ptr %aria_str49, i32 0, i32 1
  store i64 0, ptr %length_ptr51, align 4
  %aria_str_val52 = load { ptr, i64 }, ptr %aria_str49, align 8
  %24 = call ptr @aria_string_concat({ ptr, i64 } %concat_str48, { ptr, i64 } %aria_str_val52)
  %result_val_ptr53 = getelementptr inbounds nuw { ptr, ptr }, ptr %24, i32 0, i32 0
  %aria_str_ptr54 = load ptr, ptr %result_val_ptr53, align 8
  %concat_str55 = load { ptr, i64 }, ptr %aria_str_ptr54, align 8
  %result_tmp56 = alloca { ptr, i64 }, align 8
  store { ptr, i64 } %concat_str55, ptr %result_tmp56, align 8
  %25 = getelementptr inbounds nuw { ptr, i64 }, ptr %result_tmp56, i32 0, i32 0
  %result_str_ptr57 = load ptr, ptr %25, align 8
  store ptr %result_str_ptr57, ptr %msg2, align 8
  %msg3 = alloca ptr, align 8
  %aria_str58 = alloca { ptr, i64 }, align 8
  %data_ptr59 = getelementptr inbounds nuw { ptr, i64 }, ptr %aria_str58, i32 0, i32 0
  store ptr @.str.9, ptr %data_ptr59, align 8
  %length_ptr60 = getelementptr inbounds nuw { ptr, i64 }, ptr %aria_str58, i32 0, i32 1
  store i64 9, ptr %length_ptr60, align 4
  %aria_str_val61 = load { ptr, i64 }, ptr %aria_str58, align 8
  %x62 = load i64, ptr %x, align 4
  %int_str_buffer63 = alloca [32 x i8], align 1
  %26 = call i32 (ptr, ptr, ...) @sprintf(ptr %int_str_buffer63, ptr @.fmt_lld.10, i64 %x62)
  %27 = call i64 @strlen(ptr %int_str_buffer63)
  %int_aria_str64 = alloca { ptr, i64 }, align 8
  %28 = getelementptr inbounds nuw { ptr, i64 }, ptr %int_aria_str64, i32 0, i32 0
  store ptr %int_str_buffer63, ptr %28, align 8
  %29 = getelementptr inbounds nuw { ptr, i64 }, ptr %int_aria_str64, i32 0, i32 1
  store i64 %27, ptr %29, align 4
  %int_str_val65 = load { ptr, i64 }, ptr %int_aria_str64, align 8
  %30 = call ptr @aria_string_concat({ ptr, i64 } %aria_str_val61, { ptr, i64 } %int_str_val65)
  %result_val_ptr66 = getelementptr inbounds nuw { ptr, ptr }, ptr %30, i32 0, i32 0
  %aria_str_ptr67 = load ptr, ptr %result_val_ptr66, align 8
  %concat_str68 = load { ptr, i64 }, ptr %aria_str_ptr67, align 8
  %aria_str69 = alloca { ptr, i64 }, align 8
  %data_ptr70 = getelementptr inbounds nuw { ptr, i64 }, ptr %aria_str69, i32 0, i32 0
  store ptr @.str.11, ptr %data_ptr70, align 8
  %length_ptr71 = getelementptr inbounds nuw { ptr, i64 }, ptr %aria_str69, i32 0, i32 1
  store i64 3, ptr %length_ptr71, align 4
  %aria_str_val72 = load { ptr, i64 }, ptr %aria_str69, align 8
  %31 = call ptr @aria_string_concat({ ptr, i64 } %concat_str68, { ptr, i64 } %aria_str_val72)
  %result_val_ptr73 = getelementptr inbounds nuw { ptr, ptr }, ptr %31, i32 0, i32 0
  %aria_str_ptr74 = load ptr, ptr %result_val_ptr73, align 8
  %concat_str75 = load { ptr, i64 }, ptr %aria_str_ptr74, align 8
  %y76 = load i64, ptr %y, align 4
  %int_str_buffer77 = alloca [32 x i8], align 1
  %32 = call i32 (ptr, ptr, ...) @sprintf(ptr %int_str_buffer77, ptr @.fmt_lld.12, i64 %y76)
  %33 = call i64 @strlen(ptr %int_str_buffer77)
  %int_aria_str78 = alloca { ptr, i64 }, align 8
  %34 = getelementptr inbounds nuw { ptr, i64 }, ptr %int_aria_str78, i32 0, i32 0
  store ptr %int_str_buffer77, ptr %34, align 8
  %35 = getelementptr inbounds nuw { ptr, i64 }, ptr %int_aria_str78, i32 0, i32 1
  store i64 %33, ptr %35, align 4
  %int_str_val79 = load { ptr, i64 }, ptr %int_aria_str78, align 8
  %36 = call ptr @aria_string_concat({ ptr, i64 } %concat_str75, { ptr, i64 } %int_str_val79)
  %result_val_ptr80 = getelementptr inbounds nuw { ptr, ptr }, ptr %36, i32 0, i32 0
  %aria_str_ptr81 = load ptr, ptr %result_val_ptr80, align 8
  %concat_str82 = load { ptr, i64 }, ptr %aria_str_ptr81, align 8
  %aria_str83 = alloca { ptr, i64 }, align 8
  %data_ptr84 = getelementptr inbounds nuw { ptr, i64 }, ptr %aria_str83, i32 0, i32 0
  store ptr @.str.13, ptr %data_ptr84, align 8
  %length_ptr85 = getelementptr inbounds nuw { ptr, i64 }, ptr %aria_str83, i32 0, i32 1
  store i64 3, ptr %length_ptr85, align 4
  %aria_str_val86 = load { ptr, i64 }, ptr %aria_str83, align 8
  %37 = call ptr @aria_string_concat({ ptr, i64 } %concat_str82, { ptr, i64 } %aria_str_val86)
  %result_val_ptr87 = getelementptr inbounds nuw { ptr, ptr }, ptr %37, i32 0, i32 0
  %aria_str_ptr88 = load ptr, ptr %result_val_ptr87, align 8
  %concat_str89 = load { ptr, i64 }, ptr %aria_str_ptr88, align 8
  %x90 = load i64, ptr %x, align 4
  %y91 = load i64, ptr %y, align 4
  %multmp = mul i64 %x90, %y91
  %int_str_buffer92 = alloca [32 x i8], align 1
  %38 = call i32 (ptr, ptr, ...) @sprintf(ptr %int_str_buffer92, ptr @.fmt_lld.14, i64 %multmp)
  %39 = call i64 @strlen(ptr %int_str_buffer92)
  %int_aria_str93 = alloca { ptr, i64 }, align 8
  %40 = getelementptr inbounds nuw { ptr, i64 }, ptr %int_aria_str93, i32 0, i32 0
  store ptr %int_str_buffer92, ptr %40, align 8
  %41 = getelementptr inbounds nuw { ptr, i64 }, ptr %int_aria_str93, i32 0, i32 1
  store i64 %39, ptr %41, align 4
  %int_str_val94 = load { ptr, i64 }, ptr %int_aria_str93, align 8
  %42 = call ptr @aria_string_concat({ ptr, i64 } %concat_str89, { ptr, i64 } %int_str_val94)
  %result_val_ptr95 = getelementptr inbounds nuw { ptr, ptr }, ptr %42, i32 0, i32 0
  %aria_str_ptr96 = load ptr, ptr %result_val_ptr95, align 8
  %concat_str97 = load { ptr, i64 }, ptr %aria_str_ptr96, align 8
  %aria_str98 = alloca { ptr, i64 }, align 8
  %data_ptr99 = getelementptr inbounds nuw { ptr, i64 }, ptr %aria_str98, i32 0, i32 0
  store ptr @.str.15, ptr %data_ptr99, align 8
  %length_ptr100 = getelementptr inbounds nuw { ptr, i64 }, ptr %aria_str98, i32 0, i32 1
  store i64 0, ptr %length_ptr100, align 4
  %aria_str_val101 = load { ptr, i64 }, ptr %aria_str98, align 8
  %43 = call ptr @aria_string_concat({ ptr, i64 } %concat_str97, { ptr, i64 } %aria_str_val101)
  %result_val_ptr102 = getelementptr inbounds nuw { ptr, ptr }, ptr %43, i32 0, i32 0
  %aria_str_ptr103 = load ptr, ptr %result_val_ptr102, align 8
  %concat_str104 = load { ptr, i64 }, ptr %aria_str_ptr103, align 8
  %result_tmp105 = alloca { ptr, i64 }, align 8
  store { ptr, i64 } %concat_str104, ptr %result_tmp105, align 8
  %44 = getelementptr inbounds nuw { ptr, i64 }, ptr %result_tmp105, i32 0, i32 0
  %result_str_ptr106 = load ptr, ptr %44, align 8
  store ptr %result_str_ptr106, ptr %msg3, align 8
  ret i64 0
}

declare ptr @aria_string_concat({ ptr, i64 }, { ptr, i64 })

declare i32 @sprintf(ptr, ptr, ...)

declare i64 @strlen(ptr)

define i32 @__template_integers_init() {
entry:
  ret i32 0
}
