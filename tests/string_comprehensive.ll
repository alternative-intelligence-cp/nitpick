; ModuleID = 'tests/string_comprehensive.aria'
source_filename = "tests/string_comprehensive.aria"

%struct.AriaString = type { ptr, i64 }

@.str.data = private constant [6 x i8] c"Hello\00"
@.str = private constant %struct.AriaString { ptr @.str.data, i64 5 }
@.str.data.1 = private constant [7 x i8] c"World!\00"
@.str.2 = private constant %struct.AriaString { ptr @.str.data.1, i64 6 }
@.str.data.3 = private constant [1 x i8] zeroinitializer
@.str.4 = private constant %struct.AriaString { ptr @.str.data.3, i64 0 }
@.str.data.5 = private constant [1 x i8] zeroinitializer
@.str.6 = private constant %struct.AriaString { ptr @.str.data.5, i64 0 }
@.str.data.7 = private constant [9 x i8] c"NotEmpty\00"
@.str.8 = private constant %struct.AriaString { ptr @.str.data.7, i64 8 }
@.str.data.9 = private constant [6 x i8] c"Hello\00"
@.str.10 = private constant %struct.AriaString { ptr @.str.data.9, i64 5 }
@.str.data.11 = private constant [6 x i8] c"Hello\00"
@.str.12 = private constant %struct.AriaString { ptr @.str.data.11, i64 5 }
@.str.data.13 = private constant [6 x i8] c"Hello\00"
@.str.14 = private constant %struct.AriaString { ptr @.str.data.13, i64 5 }
@.str.data.15 = private constant [6 x i8] c"World\00"
@.str.16 = private constant %struct.AriaString { ptr @.str.data.15, i64 5 }
@.str.data.17 = private constant [6 x i8] c"Hello\00"
@.str.18 = private constant %struct.AriaString { ptr @.str.data.17, i64 5 }
@.str.data.19 = private constant [2 x i8] c" \00"
@.str.20 = private constant %struct.AriaString { ptr @.str.data.19, i64 1 }
@.str.data.21 = private constant [6 x i8] c"World\00"
@.str.22 = private constant %struct.AriaString { ptr @.str.data.21, i64 5 }
@.str.data.23 = private constant [2 x i8] c"!\00"
@.str.24 = private constant %struct.AriaString { ptr @.str.data.23, i64 1 }
@.str.data.25 = private constant [12 x i8] c"Hello World\00"
@.str.26 = private constant %struct.AriaString { ptr @.str.data.25, i64 11 }
@.str.data.27 = private constant [12 x i8] c"Hello World\00"
@.str.28 = private constant %struct.AriaString { ptr @.str.data.27, i64 11 }
@.str.data.29 = private constant [12 x i8] c"Hello World\00"
@.str.30 = private constant %struct.AriaString { ptr @.str.data.29, i64 11 }
@.str.data.31 = private constant [6 x i8] c"World\00"
@.str.32 = private constant %struct.AriaString { ptr @.str.data.31, i64 5 }
@.str.data.33 = private constant [12 x i8] c"Hello World\00"
@.str.34 = private constant %struct.AriaString { ptr @.str.data.33, i64 11 }
@.str.data.35 = private constant [4 x i8] c"xyz\00"
@.str.36 = private constant %struct.AriaString { ptr @.str.data.35, i64 3 }
@.str.data.37 = private constant [12 x i8] c"Hello World\00"
@.str.38 = private constant %struct.AriaString { ptr @.str.data.37, i64 11 }
@.str.data.39 = private constant [6 x i8] c"Hello\00"
@.str.40 = private constant %struct.AriaString { ptr @.str.data.39, i64 5 }
@.str.data.41 = private constant [12 x i8] c"Hello World\00"
@.str.42 = private constant %struct.AriaString { ptr @.str.data.41, i64 11 }
@.str.data.43 = private constant [6 x i8] c"World\00"
@.str.44 = private constant %struct.AriaString { ptr @.str.data.43, i64 5 }
@.str.data.45 = private constant [12 x i8] c"Hello World\00"
@.str.46 = private constant %struct.AriaString { ptr @.str.data.45, i64 11 }
@.str.data.47 = private constant [6 x i8] c"World\00"
@.str.48 = private constant %struct.AriaString { ptr @.str.data.47, i64 5 }
@.str.data.49 = private constant [12 x i8] c"Hello World\00"
@.str.50 = private constant %struct.AriaString { ptr @.str.data.49, i64 11 }
@.str.data.51 = private constant [6 x i8] c"Hello\00"
@.str.52 = private constant %struct.AriaString { ptr @.str.data.51, i64 5 }
@.str.data.53 = private constant [10 x i8] c"  Hello  \00"
@.str.54 = private constant %struct.AriaString { ptr @.str.data.53, i64 9 }
@.str.data.55 = private constant [9 x i8] c"NoSpaces\00"
@.str.56 = private constant %struct.AriaString { ptr @.str.data.55, i64 8 }
@.str.data.57 = private constant [6 x i8] c"hello\00"
@.str.58 = private constant %struct.AriaString { ptr @.str.data.57, i64 5 }
@.str.data.59 = private constant [15 x i8] c"Mixed Case 123\00"
@.str.60 = private constant %struct.AriaString { ptr @.str.data.59, i64 14 }
@.str.data.61 = private constant [6 x i8] c"HELLO\00"
@.str.62 = private constant %struct.AriaString { ptr @.str.data.61, i64 5 }
@.str.data.63 = private constant [15 x i8] c"Mixed Case 123\00"
@.str.64 = private constant %struct.AriaString { ptr @.str.data.63, i64 14 }
@.str.data.65 = private constant [12 x i8] c"Hello World\00"
@.str.66 = private constant %struct.AriaString { ptr @.str.data.65, i64 11 }

define i64 @main() {
entry:
  %len1 = alloca i64, align 8
  %str = load %struct.AriaString, ptr @.str, align 8
  %length = extractvalue %struct.AriaString %str, 1
  store i64 %length, ptr %len1, align 4
  %len2 = alloca i64, align 8
  %str1 = load %struct.AriaString, ptr @.str.2, align 8
  %length2 = extractvalue %struct.AriaString %str1, 1
  store i64 %length2, ptr %len2, align 4
  %len3 = alloca i64, align 8
  %str3 = load %struct.AriaString, ptr @.str.4, align 8
  %length4 = extractvalue %struct.AriaString %str3, 1
  store i64 %length4, ptr %len3, align 4
  %str5 = load %struct.AriaString, ptr @.str.6, align 8
  %length6 = extractvalue %struct.AriaString %str5, 1
  %is_empty = icmp eq i64 %length6, 0
  %str7 = load %struct.AriaString, ptr @.str.8, align 8
  %length8 = extractvalue %struct.AriaString %str7, 1
  %is_empty9 = icmp eq i64 %length8, 0
  %str110 = load %struct.AriaString, ptr @.str.10, align 8
  %str2 = load %struct.AriaString, ptr @.str.12, align 8
  %equals = call i1 @aria_string_equals(%struct.AriaString %str110, %struct.AriaString %str2)
  %str111 = load %struct.AriaString, ptr @.str.14, align 8
  %str212 = load %struct.AriaString, ptr @.str.16, align 8
  %equals13 = call i1 @aria_string_equals(%struct.AriaString %str111, %struct.AriaString %str212)
  %str114 = load %struct.AriaString, ptr @.str.18, align 8
  %str215 = load %struct.AriaString, ptr @.str.20, align 8
  %concat_result = call ptr @aria_string_concat(%struct.AriaString %str114, %struct.AriaString %str215)
  %str116 = load %struct.AriaString, ptr @.str.22, align 8
  %str217 = load %struct.AriaString, ptr @.str.24, align 8
  %concat_result18 = call ptr @aria_string_concat(%struct.AriaString %str116, %struct.AriaString %str217)
  %str19 = load %struct.AriaString, ptr @.str.26, align 8
  %substr_result = call ptr @aria_string_substring(%struct.AriaString %str19, i32 0, i32 5)
  %str20 = load %struct.AriaString, ptr @.str.28, align 8
  %substr_result21 = call ptr @aria_string_substring(%struct.AriaString %str20, i32 6, i32 5)
  %haystack = load %struct.AriaString, ptr @.str.30, align 8
  %needle = load %struct.AriaString, ptr @.str.32, align 8
  %contains = call i1 @aria_string_contains(%struct.AriaString %haystack, %struct.AriaString %needle)
  %haystack22 = load %struct.AriaString, ptr @.str.34, align 8
  %needle23 = load %struct.AriaString, ptr @.str.36, align 8
  %contains24 = call i1 @aria_string_contains(%struct.AriaString %haystack22, %struct.AriaString %needle23)
  %str25 = load %struct.AriaString, ptr @.str.38, align 8
  %prefix = load %struct.AriaString, ptr @.str.40, align 8
  %starts_with = call i1 @aria_string_starts_with(%struct.AriaString %str25, %struct.AriaString %prefix)
  %str26 = load %struct.AriaString, ptr @.str.42, align 8
  %prefix27 = load %struct.AriaString, ptr @.str.44, align 8
  %starts_with28 = call i1 @aria_string_starts_with(%struct.AriaString %str26, %struct.AriaString %prefix27)
  %str29 = load %struct.AriaString, ptr @.str.46, align 8
  %suffix = load %struct.AriaString, ptr @.str.48, align 8
  %ends_with = call i1 @aria_string_ends_with(%struct.AriaString %str29, %struct.AriaString %suffix)
  %str30 = load %struct.AriaString, ptr @.str.50, align 8
  %suffix31 = load %struct.AriaString, ptr @.str.52, align 8
  %ends_with32 = call i1 @aria_string_ends_with(%struct.AriaString %str30, %struct.AriaString %suffix31)
  %str33 = load %struct.AriaString, ptr @.str.54, align 8
  %trim_result = call ptr @aria_string_trim(%struct.AriaString %str33)
  %str34 = load %struct.AriaString, ptr @.str.56, align 8
  %trim_result35 = call ptr @aria_string_trim(%struct.AriaString %str34)
  %str36 = load %struct.AriaString, ptr @.str.58, align 8
  %upper_result = call ptr @aria_string_to_upper(%struct.AriaString %str36)
  %str37 = load %struct.AriaString, ptr @.str.60, align 8
  %upper_result38 = call ptr @aria_string_to_upper(%struct.AriaString %str37)
  %str39 = load %struct.AriaString, ptr @.str.62, align 8
  %lower_result = call ptr @aria_string_to_lower(%struct.AriaString %str39)
  %str40 = load %struct.AriaString, ptr @.str.64, align 8
  %lower_result41 = call ptr @aria_string_to_lower(%struct.AriaString %str40)
  %result_len = alloca i64, align 8
  %str42 = load %struct.AriaString, ptr @.str.66, align 8
  %length43 = extractvalue %struct.AriaString %str42, 1
  store i64 %length43, ptr %result_len, align 4
  ret i64 0
}

declare i1 @aria_string_equals(%struct.AriaString, %struct.AriaString)

declare ptr @aria_string_concat(%struct.AriaString, %struct.AriaString)

declare ptr @aria_string_substring(%struct.AriaString, i64, i64)

declare i1 @aria_string_contains(%struct.AriaString, %struct.AriaString)

declare i1 @aria_string_starts_with(%struct.AriaString, %struct.AriaString)

declare i1 @aria_string_ends_with(%struct.AriaString, %struct.AriaString)

declare ptr @aria_string_trim(%struct.AriaString)

declare ptr @aria_string_to_upper(%struct.AriaString)

declare ptr @aria_string_to_lower(%struct.AriaString)

define i32 @__string_comprehensive_init() {
entry:
  ret i32 0
}
