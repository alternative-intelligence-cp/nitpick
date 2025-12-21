; ModuleID = 'tests/template_basic.aria'
source_filename = "tests/template_basic.aria"

@str = private unnamed_addr constant [6 x i8] c"World\00", align 1
@.str = private constant [7 x i8] c"Hello \00"
@.str.1 = private constant [2 x i8] c"!\00"

define i64 @main() {
entry:
  %name = alloca ptr, align 8
  store ptr @str, ptr %name, align 8
  %message = alloca ptr, align 8
  %aria_str = alloca { ptr, i64 }, align 8
  %data_ptr = getelementptr inbounds nuw { ptr, i64 }, ptr %aria_str, i32 0, i32 0
  store ptr @.str, ptr %data_ptr, align 8
  %length_ptr = getelementptr inbounds nuw { ptr, i64 }, ptr %aria_str, i32 0, i32 1
  store i64 6, ptr %length_ptr, align 4
  %aria_str_val = load { ptr, i64 }, ptr %aria_str, align 8
  %name1 = load ptr, ptr %name, align 8
  %0 = call ptr @aria_string_concat({ ptr, i64 } %aria_str_val, ptr %name1)
  %concat_str = load { ptr, i64 }, ptr %0, align 8
  %aria_str2 = alloca { ptr, i64 }, align 8
  %data_ptr3 = getelementptr inbounds nuw { ptr, i64 }, ptr %aria_str2, i32 0, i32 0
  store ptr @.str.1, ptr %data_ptr3, align 8
  %length_ptr4 = getelementptr inbounds nuw { ptr, i64 }, ptr %aria_str2, i32 0, i32 1
  store i64 1, ptr %length_ptr4, align 4
  %aria_str_val5 = load { ptr, i64 }, ptr %aria_str2, align 8
  %1 = call ptr @aria_string_concat({ ptr, i64 } %concat_str, { ptr, i64 } %aria_str_val5)
  %concat_str6 = load { ptr, i64 }, ptr %1, align 8
  %result_tmp = alloca { ptr, i64 }, align 8
  store { ptr, i64 } %concat_str6, ptr %result_tmp, align 8
  %2 = getelementptr inbounds nuw { ptr, i64 }, ptr %result_tmp, i32 0, i32 0
  %result_str_ptr = load ptr, ptr %2, align 8
  store ptr %result_str_ptr, ptr %message, align 8
  ret i64 42
}

declare ptr @aria_string_concat({ ptr, i64 }, { ptr, i64 })

define i32 @__template_basic_init() {
entry:
  ret i32 0
}
