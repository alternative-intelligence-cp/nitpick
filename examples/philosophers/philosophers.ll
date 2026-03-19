; ModuleID = 'examples/philosophers/philosophers.aria'
source_filename = "examples/philosophers/philosophers.aria"

%struct.AriaString = type { ptr, i64 }
%Wave9 = type { i8, i8, i8, i8, i8, i8, i8, i8, i8 }
%struct.NIL = type {}

@.str.data = private constant [5 x i8] c"true\00"
@.str = private constant %struct.AriaString { ptr @.str.data, i64 4 }
@.str.data.1 = private constant [6 x i8] c"false\00"
@.str.2 = private constant %struct.AriaString { ptr @.str.data.1, i64 5 }
@.str.data.3 = private constant [3 x i8] c"0x\00"
@.str.4 = private constant %struct.AriaString { ptr @.str.data.3, i64 2 }
@.str.data.5 = private constant [3 x i8] c"0x\00"
@.str.6 = private constant %struct.AriaString { ptr @.str.data.5, i64 2 }
@.str.data.7 = private constant [2 x i8] c"0\00"
@.str.8 = private constant %struct.AriaString { ptr @.str.data.7, i64 1 }
@.str.data.9 = private constant [2 x i8] c"0\00"
@.str.10 = private constant %struct.AriaString { ptr @.str.data.9, i64 1 }
@.str.data.11 = private constant [5 x i8] c"true\00"
@.str.12 = private constant %struct.AriaString { ptr @.str.data.11, i64 4 }
@.str.data.13 = private constant [6 x i8] c"false\00"
@.str.14 = private constant %struct.AriaString { ptr @.str.data.13, i64 5 }
@.str.data.15 = private constant [3 x i8] c"0x\00"
@.str.16 = private constant %struct.AriaString { ptr @.str.data.15, i64 2 }
@.str.data.17 = private constant [3 x i8] c"0x\00"
@.str.18 = private constant %struct.AriaString { ptr @.str.data.17, i64 2 }
@.str.data.19 = private constant [2 x i8] c"0\00"
@.str.20 = private constant %struct.AriaString { ptr @.str.data.19, i64 1 }
@.str.data.21 = private constant [2 x i8] c"0\00"
@.str.22 = private constant %struct.AriaString { ptr @.str.data.21, i64 1 }
@.str.data.23 = private constant [4 x i8] c"N5(\00"
@.str.24 = private constant %struct.AriaString { ptr @.str.data.23, i64 3 }
@.str.data.25 = private constant [2 x i8] c",\00"
@.str.26 = private constant %struct.AriaString { ptr @.str.data.25, i64 1 }
@.str.data.27 = private constant [2 x i8] c",\00"
@.str.28 = private constant %struct.AriaString { ptr @.str.data.27, i64 1 }
@.str.data.29 = private constant [2 x i8] c",\00"
@.str.30 = private constant %struct.AriaString { ptr @.str.data.29, i64 1 }
@.str.data.31 = private constant [2 x i8] c",\00"
@.str.32 = private constant %struct.AriaString { ptr @.str.data.31, i64 1 }
@.str.data.33 = private constant [2 x i8] c")\00"
@.str.34 = private constant %struct.AriaString { ptr @.str.data.33, i64 1 }
@.str.data.35 = private constant [4 x i8] c"W9(\00"
@.str.36 = private constant %struct.AriaString { ptr @.str.data.35, i64 3 }
@.str.data.37 = private constant [2 x i8] c",\00"
@.str.38 = private constant %struct.AriaString { ptr @.str.data.37, i64 1 }
@.str.data.39 = private constant [2 x i8] c",\00"
@.str.40 = private constant %struct.AriaString { ptr @.str.data.39, i64 1 }
@.str.data.41 = private constant [2 x i8] c",\00"
@.str.42 = private constant %struct.AriaString { ptr @.str.data.41, i64 1 }
@.str.data.43 = private constant [2 x i8] c",\00"
@.str.44 = private constant %struct.AriaString { ptr @.str.data.43, i64 1 }
@.str.data.45 = private constant [2 x i8] c",\00"
@.str.46 = private constant %struct.AriaString { ptr @.str.data.45, i64 1 }
@.str.data.47 = private constant [2 x i8] c",\00"
@.str.48 = private constant %struct.AriaString { ptr @.str.data.47, i64 1 }
@.str.data.49 = private constant [2 x i8] c",\00"
@.str.50 = private constant %struct.AriaString { ptr @.str.data.49, i64 1 }
@.str.data.51 = private constant [2 x i8] c",\00"
@.str.52 = private constant %struct.AriaString { ptr @.str.data.51, i64 1 }
@.str.data.53 = private constant [2 x i8] c")\00"
@.str.54 = private constant %struct.AriaString { ptr @.str.data.53, i64 1 }
@.str.data.55 = private constant [3 x i8] c": \00"
@.str.56 = private constant %struct.AriaString { ptr @.str.data.55, i64 2 }
@.str.data.57 = private constant [3 x i8] c": \00"
@.str.58 = private constant %struct.AriaString { ptr @.str.data.57, i64 2 }
@.str.data.59 = private constant [3 x i8] c": \00"
@.str.60 = private constant %struct.AriaString { ptr @.str.data.59, i64 2 }
@.str.data.61 = private constant [3 x i8] c": \00"
@.str.62 = private constant %struct.AriaString { ptr @.str.data.61, i64 2 }
@.str.data.63 = private constant [3 x i8] c": \00"
@.str.64 = private constant %struct.AriaString { ptr @.str.data.63, i64 2 }
@.str.data.65 = private constant [3 x i8] c": \00"
@.str.66 = private constant %struct.AriaString { ptr @.str.data.65, i64 2 }
@.str.data.67 = private constant [3 x i8] c": \00"
@.str.68 = private constant %struct.AriaString { ptr @.str.data.67, i64 2 }
@.str.data.69 = private constant [3 x i8] c": \00"
@.str.70 = private constant %struct.AriaString { ptr @.str.data.69, i64 2 }
@.str.data.71 = private constant [3 x i8] c": \00"
@.str.72 = private constant %struct.AriaString { ptr @.str.data.71, i64 2 }
@.str.data.73 = private constant [1 x i8] zeroinitializer
@.str.74 = private constant %struct.AriaString { ptr @.str.data.73, i64 0 }
@.str.data.75 = private constant [2 x i8] c"-\00"
@.str.76 = private constant %struct.AriaString { ptr @.str.data.75, i64 1 }
@.str.data.77 = private constant [3 x i8] c": \00"
@.str.78 = private constant %struct.AriaString { ptr @.str.data.77, i64 2 }
@.str.data.79 = private constant [3 x i8] c": \00"
@.str.80 = private constant %struct.AriaString { ptr @.str.data.79, i64 2 }
@.str.data.81 = private constant [3 x i8] c": \00"
@.str.82 = private constant %struct.AriaString { ptr @.str.data.81, i64 2 }
@g_fork_0 = global i64 0
@g_fork_1 = global i64 0
@g_fork_2 = global i64 0
@g_fork_3 = global i64 0
@g_fork_4 = global i64 0
@g_eat_0 = global i64 0
@g_eat_1 = global i64 0
@g_eat_2 = global i64 0
@g_eat_3 = global i64 0
@g_eat_4 = global i64 0
@g_running = global i64 0
@g_tid_0 = global i64 0
@g_tid_1 = global i64 0
@g_tid_2 = global i64 0
@g_tid_3 = global i64 0
@g_tid_4 = global i64 0
@.str.data.83 = private constant [20 x i8] c"Dining Philosophers\00"
@.str.84 = private constant %struct.AriaString { ptr @.str.data.83, i64 19 }
@.str.data.85 = private constant [7 x i8] c"  N = \00"
@.str.86 = private constant %struct.AriaString { ptr @.str.data.85, i64 6 }
@.str.data.87 = private constant [18 x i8] c"  Run = 3 seconds\00"
@.str.88 = private constant %struct.AriaString { ptr @.str.data.87, i64 17 }
@.str.data.89 = private constant [4 x i8] c"---\00"
@.str.90 = private constant %struct.AriaString { ptr @.str.data.89, i64 3 }
@.str.data.91 = private constant [15 x i8] c"  Philosopher \00"
@.str.92 = private constant %struct.AriaString { ptr @.str.data.91, i64 14 }
@.str.data.93 = private constant [6 x i8] c" ate \00"
@.str.94 = private constant %struct.AriaString { ptr @.str.data.93, i64 5 }
@.str.data.95 = private constant [7 x i8] c" times\00"
@.str.96 = private constant %struct.AriaString { ptr @.str.data.95, i64 6 }
@.str.data.97 = private constant [4 x i8] c"---\00"
@.str.98 = private constant %struct.AriaString { ptr @.str.data.97, i64 3 }
@.str.data.99 = private constant [52 x i8] c"PASS: all philosophers ate (no starvation/deadlock)\00"
@.str.100 = private constant %struct.AriaString { ptr @.str.data.99, i64 51 }
@.str.data.101 = private constant [39 x i8] c"FAIL: at least one philosopher starved\00"
@.str.102 = private constant %struct.AriaString { ptr @.str.data.101, i64 38 }

define { ptr, ptr, i8 } @bool_toString(i1 %value) {
entry:
  %ifcond = icmp ne i1 %value, false
  br i1 %ifcond, label %then, label %ifcont

then:                                             ; preds = %entry
  ret { ptr, ptr, i8 } { ptr @.str, ptr null, i8 0 }

ifcont:                                           ; preds = %entry
  ret { ptr, ptr, i8 } { ptr @.str.2, ptr null, i8 0 }

entry1:                                           ; No predecessors!
  %ifcond2 = icmp ne i1 %value, false
  br i1 %ifcond2, label %then3, label %ifcont4

then3:                                            ; preds = %entry1
  ret { ptr, ptr, i8 } { ptr @.str.12, ptr null, i8 0 }

ifcont4:                                          ; preds = %entry1
  ret { ptr, ptr, i8 } { ptr @.str.14, ptr null, i8 0 }
}

define { ptr, ptr, i8 } @nit_toString(i8 %value) {
entry:
  %v = alloca i32, align 4
  %cast.sext = sext i8 %value to i32
  store i32 %cast.sext, ptr %v, align 4
  %v1 = load i32, ptr %v, align 4
  %val_i64 = sext i32 %v1 to i64
  %from_int_result = call ptr @aria_string_from_int_simple(i64 %val_i64)
  %result.val = insertvalue { ptr, ptr, i8 } undef, ptr %from_int_result, 0
  %result.err = insertvalue { ptr, ptr, i8 } %result.val, ptr null, 1
  %result.is_error = insertvalue { ptr, ptr, i8 } %result.err, i8 0, 2
  ret { ptr, ptr, i8 } %result.is_error

entry2:                                           ; No predecessors!
  %v3 = alloca i32, align 4
  %cast.sext4 = sext i8 %value to i32
  store i32 %cast.sext4, ptr %v3, align 4
  %v5 = load i32, ptr %v3, align 4
  %val_i646 = sext i32 %v5 to i64
  %from_int_result7 = call ptr @aria_string_from_int_simple(i64 %val_i646)
  %result.val8 = insertvalue { ptr, ptr, i8 } undef, ptr %from_int_result7, 0
  %result.err9 = insertvalue { ptr, ptr, i8 } %result.val8, ptr null, 1
  %result.is_error10 = insertvalue { ptr, ptr, i8 } %result.err9, i8 0, 2
  ret { ptr, ptr, i8 } %result.is_error10
}

define { ptr, ptr, i8 } @int32_toString(i32 %value) {
entry:
  %val_i64 = sext i32 %value to i64
  %from_int_result = call ptr @aria_string_from_int_simple(i64 %val_i64)
  %result.val = insertvalue { ptr, ptr, i8 } undef, ptr %from_int_result, 0
  %result.err = insertvalue { ptr, ptr, i8 } %result.val, ptr null, 1
  %result.is_error = insertvalue { ptr, ptr, i8 } %result.err, i8 0, 2
  ret { ptr, ptr, i8 } %result.is_error

entry1:                                           ; No predecessors!
  %val_i642 = sext i32 %value to i64
  %from_int_result3 = call ptr @aria_string_from_int_simple(i64 %val_i642)
  %result.val4 = insertvalue { ptr, ptr, i8 } undef, ptr %from_int_result3, 0
  %result.err5 = insertvalue { ptr, ptr, i8 } %result.val4, ptr null, 1
  %result.is_error6 = insertvalue { ptr, ptr, i8 } %result.err5, i8 0, 2
  ret { ptr, ptr, i8 } %result.is_error6
}

define { ptr, ptr, i8 } @int64_toString(i64 %value) {
entry:
  %from_int_result = call ptr @aria_string_from_int_simple(i64 %value)
  %result.val = insertvalue { ptr, ptr, i8 } undef, ptr %from_int_result, 0
  %result.err = insertvalue { ptr, ptr, i8 } %result.val, ptr null, 1
  %result.is_error = insertvalue { ptr, ptr, i8 } %result.err, i8 0, 2
  ret { ptr, ptr, i8 } %result.is_error

entry1:                                           ; No predecessors!
  %from_int_result2 = call ptr @aria_string_from_int_simple(i64 %value)
  %result.val3 = insertvalue { ptr, ptr, i8 } undef, ptr %from_int_result2, 0
  %result.err4 = insertvalue { ptr, ptr, i8 } %result.val3, ptr null, 1
  %result.is_error5 = insertvalue { ptr, ptr, i8 } %result.err4, i8 0, 2
  ret { ptr, ptr, i8 } %result.is_error5
}

define { ptr, ptr, i8 } @float_toString(i32 %value, i32 %precision) {
entry:
  %format_float_result = call ptr @aria_string_format_float_simple(i32 %value, i32 %precision)
  %result.val = insertvalue { ptr, ptr, i8 } undef, ptr %format_float_result, 0
  %result.err = insertvalue { ptr, ptr, i8 } %result.val, ptr null, 1
  %result.is_error = insertvalue { ptr, ptr, i8 } %result.err, i8 0, 2
  ret { ptr, ptr, i8 } %result.is_error

entry1:                                           ; No predecessors!
  %format_float_result2 = call ptr @aria_string_format_float_simple(i32 %value, i32 %precision)
  %result.val3 = insertvalue { ptr, ptr, i8 } undef, ptr %format_float_result2, 0
  %result.err4 = insertvalue { ptr, ptr, i8 } %result.val3, ptr null, 1
  %result.is_error5 = insertvalue { ptr, ptr, i8 } %result.err4, i8 0, 2
  ret { ptr, ptr, i8 } %result.is_error5
}

define { ptr, ptr, i8 } @float_toStringFixed(i32 %value, i32 %precision) {
entry:
  %format_float_result = call ptr @aria_string_format_float_simple(i32 %value, i32 %precision)
  %result.val = insertvalue { ptr, ptr, i8 } undef, ptr %format_float_result, 0
  %result.err = insertvalue { ptr, ptr, i8 } %result.val, ptr null, 1
  %result.is_error = insertvalue { ptr, ptr, i8 } %result.err, i8 0, 2
  ret { ptr, ptr, i8 } %result.is_error

entry1:                                           ; No predecessors!
  %format_float_result2 = call ptr @aria_string_format_float_simple(i32 %value, i32 %precision)
  %result.val3 = insertvalue { ptr, ptr, i8 } undef, ptr %format_float_result2, 0
  %result.err4 = insertvalue { ptr, ptr, i8 } %result.val3, ptr null, 1
  %result.is_error5 = insertvalue { ptr, ptr, i8 } %result.err4, i8 0, 2
  ret { ptr, ptr, i8 } %result.is_error5
}

define { ptr, ptr, i8 } @float_toStringDefault(i32 %value) {
entry:
  %format_float_result = call ptr @aria_string_format_float_simple(i32 %value, i32 6)
  %result.val = insertvalue { ptr, ptr, i8 } undef, ptr %format_float_result, 0
  %result.err = insertvalue { ptr, ptr, i8 } %result.val, ptr null, 1
  %result.is_error = insertvalue { ptr, ptr, i8 } %result.err, i8 0, 2
  ret { ptr, ptr, i8 } %result.is_error

entry1:                                           ; No predecessors!
  %format_float_result2 = call ptr @aria_string_format_float_simple(i32 %value, i32 6)
  %result.val3 = insertvalue { ptr, ptr, i8 } undef, ptr %format_float_result2, 0
  %result.err4 = insertvalue { ptr, ptr, i8 } %result.val3, ptr null, 1
  %result.is_error5 = insertvalue { ptr, ptr, i8 } %result.err4, i8 0, 2
  ret { ptr, ptr, i8 } %result.is_error5
}

define { i1, ptr, i8 } @string_isEmpty(ptr %s) {
entry:
  %str = load %struct.AriaString, ptr %s, align 8
  %length = extractvalue %struct.AriaString %str, 1
  %is_empty = icmp eq i64 %length, 0
  %result.val = insertvalue { i1, ptr, i8 } undef, i1 %is_empty, 0
  %result.err = insertvalue { i1, ptr, i8 } %result.val, ptr null, 1
  %result.is_error = insertvalue { i1, ptr, i8 } %result.err, i8 0, 2
  ret { i1, ptr, i8 } %result.is_error

entry1:                                           ; No predecessors!
  %str2 = load %struct.AriaString, ptr %s, align 8
  %length3 = extractvalue %struct.AriaString %str2, 1
  %is_empty4 = icmp eq i64 %length3, 0
  %result.val5 = insertvalue { i1, ptr, i8 } undef, i1 %is_empty4, 0
  %result.err6 = insertvalue { i1, ptr, i8 } %result.val5, ptr null, 1
  %result.is_error7 = insertvalue { i1, ptr, i8 } %result.err6, i8 0, 2
  ret { i1, ptr, i8 } %result.is_error7
}

define { ptr, ptr, i8 } @char_toString(i32 %code) {
entry:
  %byte = alloca i8, align 1
  %cast.trunc = trunc i32 %code to i8
  store i8 %cast.trunc, ptr %byte, align 1
  %byte1 = load i8, ptr %byte, align 1
  %char_str = call ptr @aria_string_from_char_simple(i8 %byte1)
  %result.val = insertvalue { ptr, ptr, i8 } undef, ptr %char_str, 0
  %result.err = insertvalue { ptr, ptr, i8 } %result.val, ptr null, 1
  %result.is_error = insertvalue { ptr, ptr, i8 } %result.err, i8 0, 2
  ret { ptr, ptr, i8 } %result.is_error

entry2:                                           ; No predecessors!
  %byte3 = alloca i8, align 1
  %cast.trunc4 = trunc i32 %code to i8
  store i8 %cast.trunc4, ptr %byte3, align 1
  %byte5 = load i8, ptr %byte3, align 1
  %char_str6 = call ptr @aria_string_from_char_simple(i8 %byte5)
  %result.val7 = insertvalue { ptr, ptr, i8 } undef, ptr %char_str6, 0
  %result.err8 = insertvalue { ptr, ptr, i8 } %result.val7, ptr null, 1
  %result.is_error9 = insertvalue { ptr, ptr, i8 } %result.err8, i8 0, 2
  ret { ptr, ptr, i8 } %result.is_error9
}

define { ptr, ptr, i8 } @int32_toHex(i32 %value) {
entry:
  %v = alloca i64, align 8
  %cast.sext = sext i32 %value to i64
  store i64 %cast.sext, ptr %v, align 4
  %v1 = load i64, ptr %v, align 4
  %from_int_hex_result = call ptr @aria_string_from_int_hex_simple(i64 %v1)
  %concat_str = call ptr @aria_string_concat_simple(ptr @.str.4, ptr %from_int_hex_result)
  %result.val = insertvalue { ptr, ptr, i8 } undef, ptr %concat_str, 0
  %result.err = insertvalue { ptr, ptr, i8 } %result.val, ptr null, 1
  %result.is_error = insertvalue { ptr, ptr, i8 } %result.err, i8 0, 2
  ret { ptr, ptr, i8 } %result.is_error

entry2:                                           ; No predecessors!
  %v3 = alloca i64, align 8
  %cast.sext4 = sext i32 %value to i64
  store i64 %cast.sext4, ptr %v3, align 4
  %v5 = load i64, ptr %v3, align 4
  %from_int_hex_result6 = call ptr @aria_string_from_int_hex_simple(i64 %v5)
  %concat_str7 = call ptr @aria_string_concat_simple(ptr @.str.16, ptr %from_int_hex_result6)
  %result.val8 = insertvalue { ptr, ptr, i8 } undef, ptr %concat_str7, 0
  %result.err9 = insertvalue { ptr, ptr, i8 } %result.val8, ptr null, 1
  %result.is_error10 = insertvalue { ptr, ptr, i8 } %result.err9, i8 0, 2
  ret { ptr, ptr, i8 } %result.is_error10
}

define { ptr, ptr, i8 } @int64_toHex(i64 %value) {
entry:
  %from_int_hex_result = call ptr @aria_string_from_int_hex_simple(i64 %value)
  %concat_str = call ptr @aria_string_concat_simple(ptr @.str.6, ptr %from_int_hex_result)
  %result.val = insertvalue { ptr, ptr, i8 } undef, ptr %concat_str, 0
  %result.err = insertvalue { ptr, ptr, i8 } %result.val, ptr null, 1
  %result.is_error = insertvalue { ptr, ptr, i8 } %result.err, i8 0, 2
  ret { ptr, ptr, i8 } %result.is_error

entry1:                                           ; No predecessors!
  %from_int_hex_result2 = call ptr @aria_string_from_int_hex_simple(i64 %value)
  %concat_str3 = call ptr @aria_string_concat_simple(ptr @.str.18, ptr %from_int_hex_result2)
  %result.val4 = insertvalue { ptr, ptr, i8 } undef, ptr %concat_str3, 0
  %result.err5 = insertvalue { ptr, ptr, i8 } %result.val4, ptr null, 1
  %result.is_error6 = insertvalue { ptr, ptr, i8 } %result.err5, i8 0, 2
  ret { ptr, ptr, i8 } %result.is_error6
}

define { ptr, ptr, i8 } @string_padLeft(ptr %s, i64 %total_length, ptr %pad_str) {
entry:
  %str1 = load %struct.AriaString, ptr %pad_str, align 8
  %str2 = load %struct.AriaString, ptr @.str.8, align 8
  %equals = call i1 @aria_string_equals(%struct.AriaString %str1, %struct.AriaString %str2)
  %ifcond = icmp ne i1 %equals, false
  br i1 %ifcond, label %then, label %ifcont

then:                                             ; preds = %entry
  %pad_left_result = call ptr @aria_string_pad_left_simple(ptr %s, i64 %total_length, i8 48)
  %result.val = insertvalue { ptr, ptr, i8 } undef, ptr %pad_left_result, 0
  %result.err = insertvalue { ptr, ptr, i8 } %result.val, ptr null, 1
  %result.is_error = insertvalue { ptr, ptr, i8 } %result.err, i8 0, 2
  ret { ptr, ptr, i8 } %result.is_error

ifcont:                                           ; preds = %entry
  %pad_left_result1 = call ptr @aria_string_pad_left_simple(ptr %s, i64 %total_length, i8 32)
  %result.val2 = insertvalue { ptr, ptr, i8 } undef, ptr %pad_left_result1, 0
  %result.err3 = insertvalue { ptr, ptr, i8 } %result.val2, ptr null, 1
  %result.is_error4 = insertvalue { ptr, ptr, i8 } %result.err3, i8 0, 2
  ret { ptr, ptr, i8 } %result.is_error4

entry5:                                           ; No predecessors!
  %str16 = load %struct.AriaString, ptr %pad_str, align 8
  %str27 = load %struct.AriaString, ptr @.str.20, align 8
  %equals8 = call i1 @aria_string_equals(%struct.AriaString %str16, %struct.AriaString %str27)
  %ifcond9 = icmp ne i1 %equals8, false
  br i1 %ifcond9, label %then10, label %ifcont15

then10:                                           ; preds = %entry5
  %pad_left_result11 = call ptr @aria_string_pad_left_simple(ptr %s, i64 %total_length, i8 48)
  %result.val12 = insertvalue { ptr, ptr, i8 } undef, ptr %pad_left_result11, 0
  %result.err13 = insertvalue { ptr, ptr, i8 } %result.val12, ptr null, 1
  %result.is_error14 = insertvalue { ptr, ptr, i8 } %result.err13, i8 0, 2
  ret { ptr, ptr, i8 } %result.is_error14

ifcont15:                                         ; preds = %entry5
  %pad_left_result16 = call ptr @aria_string_pad_left_simple(ptr %s, i64 %total_length, i8 32)
  %result.val17 = insertvalue { ptr, ptr, i8 } undef, ptr %pad_left_result16, 0
  %result.err18 = insertvalue { ptr, ptr, i8 } %result.val17, ptr null, 1
  %result.is_error19 = insertvalue { ptr, ptr, i8 } %result.err18, i8 0, 2
  ret { ptr, ptr, i8 } %result.is_error19
}

define { ptr, ptr, i8 } @string_padRight(ptr %s, i64 %total_length, ptr %pad_str) {
entry:
  %str1 = load %struct.AriaString, ptr %pad_str, align 8
  %str2 = load %struct.AriaString, ptr @.str.10, align 8
  %equals = call i1 @aria_string_equals(%struct.AriaString %str1, %struct.AriaString %str2)
  %ifcond = icmp ne i1 %equals, false
  br i1 %ifcond, label %then, label %ifcont

then:                                             ; preds = %entry
  %pad_right_result = call ptr @aria_string_pad_right_simple(ptr %s, i64 %total_length, i8 48)
  %result.val = insertvalue { ptr, ptr, i8 } undef, ptr %pad_right_result, 0
  %result.err = insertvalue { ptr, ptr, i8 } %result.val, ptr null, 1
  %result.is_error = insertvalue { ptr, ptr, i8 } %result.err, i8 0, 2
  ret { ptr, ptr, i8 } %result.is_error

ifcont:                                           ; preds = %entry
  %pad_right_result1 = call ptr @aria_string_pad_right_simple(ptr %s, i64 %total_length, i8 32)
  %result.val2 = insertvalue { ptr, ptr, i8 } undef, ptr %pad_right_result1, 0
  %result.err3 = insertvalue { ptr, ptr, i8 } %result.val2, ptr null, 1
  %result.is_error4 = insertvalue { ptr, ptr, i8 } %result.err3, i8 0, 2
  ret { ptr, ptr, i8 } %result.is_error4

entry5:                                           ; No predecessors!
  %str16 = load %struct.AriaString, ptr %pad_str, align 8
  %str27 = load %struct.AriaString, ptr @.str.22, align 8
  %equals8 = call i1 @aria_string_equals(%struct.AriaString %str16, %struct.AriaString %str27)
  %ifcond9 = icmp ne i1 %equals8, false
  br i1 %ifcond9, label %then10, label %ifcont15

then10:                                           ; preds = %entry5
  %pad_right_result11 = call ptr @aria_string_pad_right_simple(ptr %s, i64 %total_length, i8 48)
  %result.val12 = insertvalue { ptr, ptr, i8 } undef, ptr %pad_right_result11, 0
  %result.err13 = insertvalue { ptr, ptr, i8 } %result.val12, ptr null, 1
  %result.is_error14 = insertvalue { ptr, ptr, i8 } %result.err13, i8 0, 2
  ret { ptr, ptr, i8 } %result.is_error14

ifcont15:                                         ; preds = %entry5
  %pad_right_result16 = call ptr @aria_string_pad_right_simple(ptr %s, i64 %total_length, i8 32)
  %result.val17 = insertvalue { ptr, ptr, i8 } undef, ptr %pad_right_result16, 0
  %result.err18 = insertvalue { ptr, ptr, i8 } %result.val17, ptr null, 1
  %result.is_error19 = insertvalue { ptr, ptr, i8 } %result.err18, i8 0, 2
  ret { ptr, ptr, i8 } %result.is_error19
}

define { ptr, ptr, i8 } @string_zeroPad(ptr %s, i64 %width) {
entry:
  %pad_left_result = call ptr @aria_string_pad_left_simple(ptr %s, i64 %width, i8 48)
  %result.val = insertvalue { ptr, ptr, i8 } undef, ptr %pad_left_result, 0
  %result.err = insertvalue { ptr, ptr, i8 } %result.val, ptr null, 1
  %result.is_error = insertvalue { ptr, ptr, i8 } %result.err, i8 0, 2
  ret { ptr, ptr, i8 } %result.is_error

entry1:                                           ; No predecessors!
  %pad_left_result2 = call ptr @aria_string_pad_left_simple(ptr %s, i64 %width, i8 48)
  %result.val3 = insertvalue { ptr, ptr, i8 } undef, ptr %pad_left_result2, 0
  %result.err4 = insertvalue { ptr, ptr, i8 } %result.val3, ptr null, 1
  %result.is_error5 = insertvalue { ptr, ptr, i8 } %result.err4, i8 0, 2
  ret { ptr, ptr, i8 } %result.is_error5
}

define { ptr, ptr, i8 } @string_leftAlign(ptr %s, i64 %width) {
entry:
  %pad_right_result = call ptr @aria_string_pad_right_simple(ptr %s, i64 %width, i8 32)
  %result.val = insertvalue { ptr, ptr, i8 } undef, ptr %pad_right_result, 0
  %result.err = insertvalue { ptr, ptr, i8 } %result.val, ptr null, 1
  %result.is_error = insertvalue { ptr, ptr, i8 } %result.err, i8 0, 2
  ret { ptr, ptr, i8 } %result.is_error

entry1:                                           ; No predecessors!
  %pad_right_result2 = call ptr @aria_string_pad_right_simple(ptr %s, i64 %width, i8 32)
  %result.val3 = insertvalue { ptr, ptr, i8 } undef, ptr %pad_right_result2, 0
  %result.err4 = insertvalue { ptr, ptr, i8 } %result.val3, ptr null, 1
  %result.is_error5 = insertvalue { ptr, ptr, i8 } %result.err4, i8 0, 2
  ret { ptr, ptr, i8 } %result.is_error5
}

define { ptr, ptr, i8 } @string_rightAlign(ptr %s, i64 %width) {
entry:
  %pad_left_result = call ptr @aria_string_pad_left_simple(ptr %s, i64 %width, i8 32)
  %result.val = insertvalue { ptr, ptr, i8 } undef, ptr %pad_left_result, 0
  %result.err = insertvalue { ptr, ptr, i8 } %result.val, ptr null, 1
  %result.is_error = insertvalue { ptr, ptr, i8 } %result.err, i8 0, 2
  ret { ptr, ptr, i8 } %result.is_error

entry1:                                           ; No predecessors!
  %pad_left_result2 = call ptr @aria_string_pad_left_simple(ptr %s, i64 %width, i8 32)
  %result.val3 = insertvalue { ptr, ptr, i8 } undef, ptr %pad_left_result2, 0
  %result.err4 = insertvalue { ptr, ptr, i8 } %result.val3, ptr null, 1
  %result.is_error5 = insertvalue { ptr, ptr, i8 } %result.err4, i8 0, 2
  ret { ptr, ptr, i8 } %result.is_error5
}

define { ptr, ptr, i8 } @string_repeat_n(ptr %s, i32 %count) {
entry:
  %n = alloca i64, align 8
  %cast.sext = sext i32 %count to i64
  store i64 %cast.sext, ptr %n, align 4
  %n1 = load i64, ptr %n, align 4
  %repeat_result = call ptr @aria_string_repeat_simple(ptr %s, i64 %n1)
  %result.val = insertvalue { ptr, ptr, i8 } undef, ptr %repeat_result, 0
  %result.err = insertvalue { ptr, ptr, i8 } %result.val, ptr null, 1
  %result.is_error = insertvalue { ptr, ptr, i8 } %result.err, i8 0, 2
  ret { ptr, ptr, i8 } %result.is_error

entry2:                                           ; No predecessors!
  %n3 = alloca i64, align 8
  %cast.sext4 = sext i32 %count to i64
  store i64 %cast.sext4, ptr %n3, align 4
  %n5 = load i64, ptr %n3, align 4
  %repeat_result6 = call ptr @aria_string_repeat_simple(ptr %s, i64 %n5)
  %result.val7 = insertvalue { ptr, ptr, i8 } undef, ptr %repeat_result6, 0
  %result.err8 = insertvalue { ptr, ptr, i8 } %result.val7, ptr null, 1
  %result.is_error9 = insertvalue { ptr, ptr, i8 } %result.err8, i8 0, 2
  ret { ptr, ptr, i8 } %result.is_error9
}

define { ptr, ptr, i8 } @string_join(ptr %sep, ptr %a, ptr %b) {
entry:
  %result = alloca ptr, align 8
  %concat_str = call ptr @aria_string_concat_simple(ptr %a, ptr %sep)
  store ptr %concat_str, ptr %result, align 8
  %result1 = load ptr, ptr %result, align 8
  %concat_str2 = call ptr @aria_string_concat_simple(ptr %result1, ptr %b)
  store ptr %concat_str2, ptr %result, align 8
  %result3 = load ptr, ptr %result, align 8
  %result.val = insertvalue { ptr, ptr, i8 } undef, ptr %result3, 0
  %result.err = insertvalue { ptr, ptr, i8 } %result.val, ptr null, 1
  %result.is_error = insertvalue { ptr, ptr, i8 } %result.err, i8 0, 2
  ret { ptr, ptr, i8 } %result.is_error

entry4:                                           ; No predecessors!
  %result5 = alloca ptr, align 8
  %concat_str6 = call ptr @aria_string_concat_simple(ptr %a, ptr %sep)
  store ptr %concat_str6, ptr %result5, align 8
  %result7 = load ptr, ptr %result5, align 8
  %concat_str8 = call ptr @aria_string_concat_simple(ptr %result7, ptr %b)
  store ptr %concat_str8, ptr %result5, align 8
  %result9 = load ptr, ptr %result5, align 8
  %result.val10 = insertvalue { ptr, ptr, i8 } undef, ptr %result9, 0
  %result.err11 = insertvalue { ptr, ptr, i8 } %result.val10, ptr null, 1
  %result.is_error12 = insertvalue { ptr, ptr, i8 } %result.err11, i8 0, 2
  ret { ptr, ptr, i8 } %result.is_error12
}

define { ptr, ptr, i8 } @string_join3(ptr %sep, ptr %a, ptr %b, ptr %c) {
entry:
  %result = alloca ptr, align 8
  %concat_str = call ptr @aria_string_concat_simple(ptr %a, ptr %sep)
  store ptr %concat_str, ptr %result, align 8
  %result1 = load ptr, ptr %result, align 8
  %concat_str2 = call ptr @aria_string_concat_simple(ptr %result1, ptr %b)
  store ptr %concat_str2, ptr %result, align 8
  %result3 = load ptr, ptr %result, align 8
  %concat_str4 = call ptr @aria_string_concat_simple(ptr %result3, ptr %sep)
  store ptr %concat_str4, ptr %result, align 8
  %result5 = load ptr, ptr %result, align 8
  %concat_str6 = call ptr @aria_string_concat_simple(ptr %result5, ptr %c)
  store ptr %concat_str6, ptr %result, align 8
  %result7 = load ptr, ptr %result, align 8
  %result.val = insertvalue { ptr, ptr, i8 } undef, ptr %result7, 0
  %result.err = insertvalue { ptr, ptr, i8 } %result.val, ptr null, 1
  %result.is_error = insertvalue { ptr, ptr, i8 } %result.err, i8 0, 2
  ret { ptr, ptr, i8 } %result.is_error

entry8:                                           ; No predecessors!
  %result9 = alloca ptr, align 8
  %concat_str10 = call ptr @aria_string_concat_simple(ptr %a, ptr %sep)
  store ptr %concat_str10, ptr %result9, align 8
  %result11 = load ptr, ptr %result9, align 8
  %concat_str12 = call ptr @aria_string_concat_simple(ptr %result11, ptr %b)
  store ptr %concat_str12, ptr %result9, align 8
  %result13 = load ptr, ptr %result9, align 8
  %concat_str14 = call ptr @aria_string_concat_simple(ptr %result13, ptr %sep)
  store ptr %concat_str14, ptr %result9, align 8
  %result15 = load ptr, ptr %result9, align 8
  %concat_str16 = call ptr @aria_string_concat_simple(ptr %result15, ptr %c)
  store ptr %concat_str16, ptr %result9, align 8
  %result17 = load ptr, ptr %result9, align 8
  %result.val18 = insertvalue { ptr, ptr, i8 } undef, ptr %result17, 0
  %result.err19 = insertvalue { ptr, ptr, i8 } %result.val18, ptr null, 1
  %result.is_error20 = insertvalue { ptr, ptr, i8 } %result.err19, i8 0, 2
  ret { ptr, ptr, i8 } %result.is_error20
}

declare ptr @aria_string_from_int_simple(i64)

declare ptr @aria_string_format_float_simple(double, i32)

declare ptr @aria_string_from_char_simple(i8)

declare ptr @aria_string_concat_simple(ptr, ptr)

declare ptr @aria_string_from_int_hex_simple(i64)

declare i1 @aria_string_equals(%struct.AriaString, %struct.AriaString)

declare ptr @aria_string_pad_left_simple(ptr, i64, i8)

declare ptr @aria_string_pad_right_simple(ptr, i64, i8)

declare ptr @aria_string_repeat_simple(ptr, i64)

define i32 @__string_convert_init() {
entry:
  ret i32 0
}

declare i16 @aria_pack_nyte(i8, i8, i8, i8, i8)

declare i8 @aria_nyte_get_nit(i16, i32)

declare i8 @aria_lerp_component(i32, i32, i32)

declare %Wave9 @aria_wave_lerp_dims(i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32, i32)

define { %Wave9, ptr, i8 } @wave_zero() {
entry:
  %struct.tmp = alloca %Wave9, align 8
  %r.ptr = getelementptr inbounds nuw %Wave9, ptr %struct.tmp, i32 0, i32 0
  store i64 0, ptr %r.ptr, align 4
  %s.ptr = getelementptr inbounds nuw %Wave9, ptr %struct.tmp, i32 0, i32 1
  store i64 0, ptr %s.ptr, align 4
  %t.ptr = getelementptr inbounds nuw %Wave9, ptr %struct.tmp, i32 0, i32 2
  store i64 0, ptr %t.ptr, align 4
  %u.ptr = getelementptr inbounds nuw %Wave9, ptr %struct.tmp, i32 0, i32 3
  store i64 0, ptr %u.ptr, align 4
  %v.ptr = getelementptr inbounds nuw %Wave9, ptr %struct.tmp, i32 0, i32 4
  store i64 0, ptr %v.ptr, align 4
  %w.ptr = getelementptr inbounds nuw %Wave9, ptr %struct.tmp, i32 0, i32 5
  store i64 0, ptr %w.ptr, align 4
  %x.ptr = getelementptr inbounds nuw %Wave9, ptr %struct.tmp, i32 0, i32 6
  store i64 0, ptr %x.ptr, align 4
  %y.ptr = getelementptr inbounds nuw %Wave9, ptr %struct.tmp, i32 0, i32 7
  store i64 0, ptr %y.ptr, align 4
  %z.ptr = getelementptr inbounds nuw %Wave9, ptr %struct.tmp, i32 0, i32 8
  store i64 0, ptr %z.ptr, align 4
  %struct.val = load %Wave9, ptr %struct.tmp, align 1
  %result.val = insertvalue { %Wave9, ptr, i8 } undef, %Wave9 %struct.val, 0
  %result.err = insertvalue { %Wave9, ptr, i8 } %result.val, ptr null, 1
  %result.is_error = insertvalue { %Wave9, ptr, i8 } %result.err, i8 0, 2
  ret { %Wave9, ptr, i8 } %result.is_error
}

define { %Wave9, ptr, i8 } @wave_interfere(%Wave9 %a, %Wave9 %b) {
entry:
  %a_alloca = alloca %Wave9, align 8
  store %Wave9 %a, ptr %a_alloca, align 1
  %b_alloca = alloca %Wave9, align 8
  store %Wave9 %b, ptr %b_alloca, align 1
  %struct.tmp = alloca %Wave9, align 8
  %r.ptr = getelementptr inbounds nuw %Wave9, ptr %a_alloca, i32 0, i32 0
  %r = load i8, ptr %r.ptr, align 1
  %r.ptr1 = getelementptr inbounds nuw %Wave9, ptr %b_alloca, i32 0, i32 0
  %r2 = load i8, ptr %r.ptr1, align 1
  %sadd = call { i8, i1 } @llvm.sadd.with.overflow.i8(i8 %r, i8 %r2)
  %addtmp = extractvalue { i8, i1 } %sadd, 0
  %add.overflow = extractvalue { i8, i1 } %sadd, 1
  %safe.addtmp = select i1 %add.overflow, i8 127, i8 %addtmp
  %r.ptr3 = getelementptr inbounds nuw %Wave9, ptr %struct.tmp, i32 0, i32 0
  store i8 %safe.addtmp, ptr %r.ptr3, align 1
  %s.ptr = getelementptr inbounds nuw %Wave9, ptr %a_alloca, i32 0, i32 1
  %s = load i8, ptr %s.ptr, align 1
  %s.ptr4 = getelementptr inbounds nuw %Wave9, ptr %b_alloca, i32 0, i32 1
  %s5 = load i8, ptr %s.ptr4, align 1
  %sadd6 = call { i8, i1 } @llvm.sadd.with.overflow.i8(i8 %s, i8 %s5)
  %addtmp7 = extractvalue { i8, i1 } %sadd6, 0
  %add.overflow8 = extractvalue { i8, i1 } %sadd6, 1
  %safe.addtmp9 = select i1 %add.overflow8, i8 127, i8 %addtmp7
  %s.ptr10 = getelementptr inbounds nuw %Wave9, ptr %struct.tmp, i32 0, i32 1
  store i8 %safe.addtmp9, ptr %s.ptr10, align 1
  %t.ptr = getelementptr inbounds nuw %Wave9, ptr %a_alloca, i32 0, i32 2
  %t = load i8, ptr %t.ptr, align 1
  %t.ptr11 = getelementptr inbounds nuw %Wave9, ptr %b_alloca, i32 0, i32 2
  %t12 = load i8, ptr %t.ptr11, align 1
  %sadd13 = call { i8, i1 } @llvm.sadd.with.overflow.i8(i8 %t, i8 %t12)
  %addtmp14 = extractvalue { i8, i1 } %sadd13, 0
  %add.overflow15 = extractvalue { i8, i1 } %sadd13, 1
  %safe.addtmp16 = select i1 %add.overflow15, i8 127, i8 %addtmp14
  %t.ptr17 = getelementptr inbounds nuw %Wave9, ptr %struct.tmp, i32 0, i32 2
  store i8 %safe.addtmp16, ptr %t.ptr17, align 1
  %u.ptr = getelementptr inbounds nuw %Wave9, ptr %a_alloca, i32 0, i32 3
  %u = load i8, ptr %u.ptr, align 1
  %u.ptr18 = getelementptr inbounds nuw %Wave9, ptr %b_alloca, i32 0, i32 3
  %u19 = load i8, ptr %u.ptr18, align 1
  %sadd20 = call { i8, i1 } @llvm.sadd.with.overflow.i8(i8 %u, i8 %u19)
  %addtmp21 = extractvalue { i8, i1 } %sadd20, 0
  %add.overflow22 = extractvalue { i8, i1 } %sadd20, 1
  %safe.addtmp23 = select i1 %add.overflow22, i8 127, i8 %addtmp21
  %u.ptr24 = getelementptr inbounds nuw %Wave9, ptr %struct.tmp, i32 0, i32 3
  store i8 %safe.addtmp23, ptr %u.ptr24, align 1
  %v.ptr = getelementptr inbounds nuw %Wave9, ptr %a_alloca, i32 0, i32 4
  %v = load i8, ptr %v.ptr, align 1
  %v.ptr25 = getelementptr inbounds nuw %Wave9, ptr %b_alloca, i32 0, i32 4
  %v26 = load i8, ptr %v.ptr25, align 1
  %sadd27 = call { i8, i1 } @llvm.sadd.with.overflow.i8(i8 %v, i8 %v26)
  %addtmp28 = extractvalue { i8, i1 } %sadd27, 0
  %add.overflow29 = extractvalue { i8, i1 } %sadd27, 1
  %safe.addtmp30 = select i1 %add.overflow29, i8 127, i8 %addtmp28
  %v.ptr31 = getelementptr inbounds nuw %Wave9, ptr %struct.tmp, i32 0, i32 4
  store i8 %safe.addtmp30, ptr %v.ptr31, align 1
  %w.ptr = getelementptr inbounds nuw %Wave9, ptr %a_alloca, i32 0, i32 5
  %w = load i8, ptr %w.ptr, align 1
  %w.ptr32 = getelementptr inbounds nuw %Wave9, ptr %b_alloca, i32 0, i32 5
  %w33 = load i8, ptr %w.ptr32, align 1
  %sadd34 = call { i8, i1 } @llvm.sadd.with.overflow.i8(i8 %w, i8 %w33)
  %addtmp35 = extractvalue { i8, i1 } %sadd34, 0
  %add.overflow36 = extractvalue { i8, i1 } %sadd34, 1
  %safe.addtmp37 = select i1 %add.overflow36, i8 127, i8 %addtmp35
  %w.ptr38 = getelementptr inbounds nuw %Wave9, ptr %struct.tmp, i32 0, i32 5
  store i8 %safe.addtmp37, ptr %w.ptr38, align 1
  %x.ptr = getelementptr inbounds nuw %Wave9, ptr %a_alloca, i32 0, i32 6
  %x = load i8, ptr %x.ptr, align 1
  %x.ptr39 = getelementptr inbounds nuw %Wave9, ptr %b_alloca, i32 0, i32 6
  %x40 = load i8, ptr %x.ptr39, align 1
  %sadd41 = call { i8, i1 } @llvm.sadd.with.overflow.i8(i8 %x, i8 %x40)
  %addtmp42 = extractvalue { i8, i1 } %sadd41, 0
  %add.overflow43 = extractvalue { i8, i1 } %sadd41, 1
  %safe.addtmp44 = select i1 %add.overflow43, i8 127, i8 %addtmp42
  %x.ptr45 = getelementptr inbounds nuw %Wave9, ptr %struct.tmp, i32 0, i32 6
  store i8 %safe.addtmp44, ptr %x.ptr45, align 1
  %y.ptr = getelementptr inbounds nuw %Wave9, ptr %a_alloca, i32 0, i32 7
  %y = load i8, ptr %y.ptr, align 1
  %y.ptr46 = getelementptr inbounds nuw %Wave9, ptr %b_alloca, i32 0, i32 7
  %y47 = load i8, ptr %y.ptr46, align 1
  %sadd48 = call { i8, i1 } @llvm.sadd.with.overflow.i8(i8 %y, i8 %y47)
  %addtmp49 = extractvalue { i8, i1 } %sadd48, 0
  %add.overflow50 = extractvalue { i8, i1 } %sadd48, 1
  %safe.addtmp51 = select i1 %add.overflow50, i8 127, i8 %addtmp49
  %y.ptr52 = getelementptr inbounds nuw %Wave9, ptr %struct.tmp, i32 0, i32 7
  store i8 %safe.addtmp51, ptr %y.ptr52, align 1
  %z.ptr = getelementptr inbounds nuw %Wave9, ptr %a_alloca, i32 0, i32 8
  %z = load i8, ptr %z.ptr, align 1
  %z.ptr53 = getelementptr inbounds nuw %Wave9, ptr %b_alloca, i32 0, i32 8
  %z54 = load i8, ptr %z.ptr53, align 1
  %sadd55 = call { i8, i1 } @llvm.sadd.with.overflow.i8(i8 %z, i8 %z54)
  %addtmp56 = extractvalue { i8, i1 } %sadd55, 0
  %add.overflow57 = extractvalue { i8, i1 } %sadd55, 1
  %safe.addtmp58 = select i1 %add.overflow57, i8 127, i8 %addtmp56
  %z.ptr59 = getelementptr inbounds nuw %Wave9, ptr %struct.tmp, i32 0, i32 8
  store i8 %safe.addtmp58, ptr %z.ptr59, align 1
  %struct.val = load %Wave9, ptr %struct.tmp, align 1
  %result.val = insertvalue { %Wave9, ptr, i8 } undef, %Wave9 %struct.val, 0
  %result.err = insertvalue { %Wave9, ptr, i8 } %result.val, ptr null, 1
  %result.is_error = insertvalue { %Wave9, ptr, i8 } %result.err, i8 0, 2
  ret { %Wave9, ptr, i8 } %result.is_error
}

define { %Wave9, ptr, i8 } @wave_negate(%Wave9 %w) {
entry:
  %w_alloca = alloca %Wave9, align 8
  store %Wave9 %w, ptr %w_alloca, align 1
  %struct.tmp = alloca %Wave9, align 8
  %r.ptr = getelementptr inbounds nuw %Wave9, ptr %w_alloca, i32 0, i32 0
  %r = load i8, ptr %r.ptr, align 1
  %negtmp = sub i8 0, %r
  %r.ptr1 = getelementptr inbounds nuw %Wave9, ptr %struct.tmp, i32 0, i32 0
  store i8 %negtmp, ptr %r.ptr1, align 1
  %s.ptr = getelementptr inbounds nuw %Wave9, ptr %w_alloca, i32 0, i32 1
  %s = load i8, ptr %s.ptr, align 1
  %negtmp2 = sub i8 0, %s
  %s.ptr3 = getelementptr inbounds nuw %Wave9, ptr %struct.tmp, i32 0, i32 1
  store i8 %negtmp2, ptr %s.ptr3, align 1
  %t.ptr = getelementptr inbounds nuw %Wave9, ptr %w_alloca, i32 0, i32 2
  %t = load i8, ptr %t.ptr, align 1
  %negtmp4 = sub i8 0, %t
  %t.ptr5 = getelementptr inbounds nuw %Wave9, ptr %struct.tmp, i32 0, i32 2
  store i8 %negtmp4, ptr %t.ptr5, align 1
  %u.ptr = getelementptr inbounds nuw %Wave9, ptr %w_alloca, i32 0, i32 3
  %u = load i8, ptr %u.ptr, align 1
  %negtmp6 = sub i8 0, %u
  %u.ptr7 = getelementptr inbounds nuw %Wave9, ptr %struct.tmp, i32 0, i32 3
  store i8 %negtmp6, ptr %u.ptr7, align 1
  %v.ptr = getelementptr inbounds nuw %Wave9, ptr %w_alloca, i32 0, i32 4
  %v = load i8, ptr %v.ptr, align 1
  %negtmp8 = sub i8 0, %v
  %v.ptr9 = getelementptr inbounds nuw %Wave9, ptr %struct.tmp, i32 0, i32 4
  store i8 %negtmp8, ptr %v.ptr9, align 1
  %w.ptr = getelementptr inbounds nuw %Wave9, ptr %w_alloca, i32 0, i32 5
  %w10 = load i8, ptr %w.ptr, align 1
  %negtmp11 = sub i8 0, %w10
  %w.ptr12 = getelementptr inbounds nuw %Wave9, ptr %struct.tmp, i32 0, i32 5
  store i8 %negtmp11, ptr %w.ptr12, align 1
  %x.ptr = getelementptr inbounds nuw %Wave9, ptr %w_alloca, i32 0, i32 6
  %x = load i8, ptr %x.ptr, align 1
  %negtmp13 = sub i8 0, %x
  %x.ptr14 = getelementptr inbounds nuw %Wave9, ptr %struct.tmp, i32 0, i32 6
  store i8 %negtmp13, ptr %x.ptr14, align 1
  %y.ptr = getelementptr inbounds nuw %Wave9, ptr %w_alloca, i32 0, i32 7
  %y = load i8, ptr %y.ptr, align 1
  %negtmp15 = sub i8 0, %y
  %y.ptr16 = getelementptr inbounds nuw %Wave9, ptr %struct.tmp, i32 0, i32 7
  store i8 %negtmp15, ptr %y.ptr16, align 1
  %z.ptr = getelementptr inbounds nuw %Wave9, ptr %w_alloca, i32 0, i32 8
  %z = load i8, ptr %z.ptr, align 1
  %negtmp17 = sub i8 0, %z
  %z.ptr18 = getelementptr inbounds nuw %Wave9, ptr %struct.tmp, i32 0, i32 8
  store i8 %negtmp17, ptr %z.ptr18, align 1
  %struct.val = load %Wave9, ptr %struct.tmp, align 1
  %result.val = insertvalue { %Wave9, ptr, i8 } undef, %Wave9 %struct.val, 0
  %result.err = insertvalue { %Wave9, ptr, i8 } %result.val, ptr null, 1
  %result.is_error = insertvalue { %Wave9, ptr, i8 } %result.err, i8 0, 2
  ret { %Wave9, ptr, i8 } %result.is_error
}

define { %Wave9, ptr, i8 } @wave_scale(%Wave9 %w, i8 %factor) {
entry:
  %w_alloca = alloca %Wave9, align 8
  store %Wave9 %w, ptr %w_alloca, align 1
  %struct.tmp = alloca %Wave9, align 8
  %r.ptr = getelementptr inbounds nuw %Wave9, ptr %w_alloca, i32 0, i32 0
  %r = load i8, ptr %r.ptr, align 1
  %nit_mul = call i16 @aria_nit_mul(i8 %r, i8 %factor)
  %r.ptr1 = getelementptr inbounds nuw %Wave9, ptr %struct.tmp, i32 0, i32 0
  store i16 %nit_mul, ptr %r.ptr1, align 2
  %s.ptr = getelementptr inbounds nuw %Wave9, ptr %w_alloca, i32 0, i32 1
  %s = load i8, ptr %s.ptr, align 1
  %nit_mul2 = call i16 @aria_nit_mul(i8 %s, i8 %factor)
  %s.ptr3 = getelementptr inbounds nuw %Wave9, ptr %struct.tmp, i32 0, i32 1
  store i16 %nit_mul2, ptr %s.ptr3, align 2
  %t.ptr = getelementptr inbounds nuw %Wave9, ptr %w_alloca, i32 0, i32 2
  %t = load i8, ptr %t.ptr, align 1
  %nit_mul4 = call i16 @aria_nit_mul(i8 %t, i8 %factor)
  %t.ptr5 = getelementptr inbounds nuw %Wave9, ptr %struct.tmp, i32 0, i32 2
  store i16 %nit_mul4, ptr %t.ptr5, align 2
  %u.ptr = getelementptr inbounds nuw %Wave9, ptr %w_alloca, i32 0, i32 3
  %u = load i8, ptr %u.ptr, align 1
  %nit_mul6 = call i16 @aria_nit_mul(i8 %u, i8 %factor)
  %u.ptr7 = getelementptr inbounds nuw %Wave9, ptr %struct.tmp, i32 0, i32 3
  store i16 %nit_mul6, ptr %u.ptr7, align 2
  %v.ptr = getelementptr inbounds nuw %Wave9, ptr %w_alloca, i32 0, i32 4
  %v = load i8, ptr %v.ptr, align 1
  %nit_mul8 = call i16 @aria_nit_mul(i8 %v, i8 %factor)
  %v.ptr9 = getelementptr inbounds nuw %Wave9, ptr %struct.tmp, i32 0, i32 4
  store i16 %nit_mul8, ptr %v.ptr9, align 2
  %w.ptr = getelementptr inbounds nuw %Wave9, ptr %w_alloca, i32 0, i32 5
  %w10 = load i8, ptr %w.ptr, align 1
  %nit_mul11 = call i16 @aria_nit_mul(i8 %w10, i8 %factor)
  %w.ptr12 = getelementptr inbounds nuw %Wave9, ptr %struct.tmp, i32 0, i32 5
  store i16 %nit_mul11, ptr %w.ptr12, align 2
  %x.ptr = getelementptr inbounds nuw %Wave9, ptr %w_alloca, i32 0, i32 6
  %x = load i8, ptr %x.ptr, align 1
  %nit_mul13 = call i16 @aria_nit_mul(i8 %x, i8 %factor)
  %x.ptr14 = getelementptr inbounds nuw %Wave9, ptr %struct.tmp, i32 0, i32 6
  store i16 %nit_mul13, ptr %x.ptr14, align 2
  %y.ptr = getelementptr inbounds nuw %Wave9, ptr %w_alloca, i32 0, i32 7
  %y = load i8, ptr %y.ptr, align 1
  %nit_mul15 = call i16 @aria_nit_mul(i8 %y, i8 %factor)
  %y.ptr16 = getelementptr inbounds nuw %Wave9, ptr %struct.tmp, i32 0, i32 7
  store i16 %nit_mul15, ptr %y.ptr16, align 2
  %z.ptr = getelementptr inbounds nuw %Wave9, ptr %w_alloca, i32 0, i32 8
  %z = load i8, ptr %z.ptr, align 1
  %nit_mul17 = call i16 @aria_nit_mul(i8 %z, i8 %factor)
  %z.ptr18 = getelementptr inbounds nuw %Wave9, ptr %struct.tmp, i32 0, i32 8
  store i16 %nit_mul17, ptr %z.ptr18, align 2
  %struct.val = load %Wave9, ptr %struct.tmp, align 1
  %result.val = insertvalue { %Wave9, ptr, i8 } undef, %Wave9 %struct.val, 0
  %result.err = insertvalue { %Wave9, ptr, i8 } %result.val, ptr null, 1
  %result.is_error = insertvalue { %Wave9, ptr, i8 } %result.err, i8 0, 2
  ret { %Wave9, ptr, i8 } %result.is_error
}

define { i32, ptr, i8 } @wave_magnitude_squared(%Wave9 %w) {
entry:
  %w_alloca = alloca %Wave9, align 8
  store %Wave9 %w, ptr %w_alloca, align 1
  %r_val = alloca i32, align 4
  %r.ptr = getelementptr inbounds nuw %Wave9, ptr %w_alloca, i32 0, i32 0
  %r = load i8, ptr %r.ptr, align 1
  %cast.sext = sext i8 %r to i32
  store i32 %cast.sext, ptr %r_val, align 4
  %s_val = alloca i32, align 4
  %s.ptr = getelementptr inbounds nuw %Wave9, ptr %w_alloca, i32 0, i32 1
  %s = load i8, ptr %s.ptr, align 1
  %cast.sext1 = sext i8 %s to i32
  store i32 %cast.sext1, ptr %s_val, align 4
  %t_val = alloca i32, align 4
  %t.ptr = getelementptr inbounds nuw %Wave9, ptr %w_alloca, i32 0, i32 2
  %t = load i8, ptr %t.ptr, align 1
  %cast.sext2 = sext i8 %t to i32
  store i32 %cast.sext2, ptr %t_val, align 4
  %u_val = alloca i32, align 4
  %u.ptr = getelementptr inbounds nuw %Wave9, ptr %w_alloca, i32 0, i32 3
  %u = load i8, ptr %u.ptr, align 1
  %cast.sext3 = sext i8 %u to i32
  store i32 %cast.sext3, ptr %u_val, align 4
  %v_val = alloca i32, align 4
  %v.ptr = getelementptr inbounds nuw %Wave9, ptr %w_alloca, i32 0, i32 4
  %v = load i8, ptr %v.ptr, align 1
  %cast.sext4 = sext i8 %v to i32
  store i32 %cast.sext4, ptr %v_val, align 4
  %w_val = alloca i32, align 4
  %w.ptr = getelementptr inbounds nuw %Wave9, ptr %w_alloca, i32 0, i32 5
  %w5 = load i8, ptr %w.ptr, align 1
  %cast.sext6 = sext i8 %w5 to i32
  store i32 %cast.sext6, ptr %w_val, align 4
  %x_val = alloca i32, align 4
  %x.ptr = getelementptr inbounds nuw %Wave9, ptr %w_alloca, i32 0, i32 6
  %x = load i8, ptr %x.ptr, align 1
  %cast.sext7 = sext i8 %x to i32
  store i32 %cast.sext7, ptr %x_val, align 4
  %y_val = alloca i32, align 4
  %y.ptr = getelementptr inbounds nuw %Wave9, ptr %w_alloca, i32 0, i32 7
  %y = load i8, ptr %y.ptr, align 1
  %cast.sext8 = sext i8 %y to i32
  store i32 %cast.sext8, ptr %y_val, align 4
  %z_val = alloca i32, align 4
  %z.ptr = getelementptr inbounds nuw %Wave9, ptr %w_alloca, i32 0, i32 8
  %z = load i8, ptr %z.ptr, align 1
  %cast.sext9 = sext i8 %z to i32
  store i32 %cast.sext9, ptr %z_val, align 4
  %r2 = alloca i32, align 4
  %r_val10 = load i32, ptr %r_val, align 4
  %r_val11 = load i32, ptr %r_val, align 4
  %smul = call { i32, i1 } @llvm.smul.with.overflow.i32(i32 %r_val10, i32 %r_val11)
  %multmp = extractvalue { i32, i1 } %smul, 0
  %mul.overflow = extractvalue { i32, i1 } %smul, 1
  %safe.multmp = select i1 %mul.overflow, i32 2147483647, i32 %multmp
  store i32 %safe.multmp, ptr %r2, align 4
  %s2 = alloca i32, align 4
  %s_val12 = load i32, ptr %s_val, align 4
  %s_val13 = load i32, ptr %s_val, align 4
  %smul14 = call { i32, i1 } @llvm.smul.with.overflow.i32(i32 %s_val12, i32 %s_val13)
  %multmp15 = extractvalue { i32, i1 } %smul14, 0
  %mul.overflow16 = extractvalue { i32, i1 } %smul14, 1
  %safe.multmp17 = select i1 %mul.overflow16, i32 2147483647, i32 %multmp15
  store i32 %safe.multmp17, ptr %s2, align 4
  %t2 = alloca i32, align 4
  %t_val18 = load i32, ptr %t_val, align 4
  %t_val19 = load i32, ptr %t_val, align 4
  %smul20 = call { i32, i1 } @llvm.smul.with.overflow.i32(i32 %t_val18, i32 %t_val19)
  %multmp21 = extractvalue { i32, i1 } %smul20, 0
  %mul.overflow22 = extractvalue { i32, i1 } %smul20, 1
  %safe.multmp23 = select i1 %mul.overflow22, i32 2147483647, i32 %multmp21
  store i32 %safe.multmp23, ptr %t2, align 4
  %u2 = alloca i32, align 4
  %u_val24 = load i32, ptr %u_val, align 4
  %u_val25 = load i32, ptr %u_val, align 4
  %smul26 = call { i32, i1 } @llvm.smul.with.overflow.i32(i32 %u_val24, i32 %u_val25)
  %multmp27 = extractvalue { i32, i1 } %smul26, 0
  %mul.overflow28 = extractvalue { i32, i1 } %smul26, 1
  %safe.multmp29 = select i1 %mul.overflow28, i32 2147483647, i32 %multmp27
  store i32 %safe.multmp29, ptr %u2, align 4
  %v2 = alloca i32, align 4
  %v_val30 = load i32, ptr %v_val, align 4
  %v_val31 = load i32, ptr %v_val, align 4
  %smul32 = call { i32, i1 } @llvm.smul.with.overflow.i32(i32 %v_val30, i32 %v_val31)
  %multmp33 = extractvalue { i32, i1 } %smul32, 0
  %mul.overflow34 = extractvalue { i32, i1 } %smul32, 1
  %safe.multmp35 = select i1 %mul.overflow34, i32 2147483647, i32 %multmp33
  store i32 %safe.multmp35, ptr %v2, align 4
  %w2 = alloca i32, align 4
  %w_val36 = load i32, ptr %w_val, align 4
  %w_val37 = load i32, ptr %w_val, align 4
  %smul38 = call { i32, i1 } @llvm.smul.with.overflow.i32(i32 %w_val36, i32 %w_val37)
  %multmp39 = extractvalue { i32, i1 } %smul38, 0
  %mul.overflow40 = extractvalue { i32, i1 } %smul38, 1
  %safe.multmp41 = select i1 %mul.overflow40, i32 2147483647, i32 %multmp39
  store i32 %safe.multmp41, ptr %w2, align 4
  %x2 = alloca i32, align 4
  %x_val42 = load i32, ptr %x_val, align 4
  %x_val43 = load i32, ptr %x_val, align 4
  %smul44 = call { i32, i1 } @llvm.smul.with.overflow.i32(i32 %x_val42, i32 %x_val43)
  %multmp45 = extractvalue { i32, i1 } %smul44, 0
  %mul.overflow46 = extractvalue { i32, i1 } %smul44, 1
  %safe.multmp47 = select i1 %mul.overflow46, i32 2147483647, i32 %multmp45
  store i32 %safe.multmp47, ptr %x2, align 4
  %y2 = alloca i32, align 4
  %y_val48 = load i32, ptr %y_val, align 4
  %y_val49 = load i32, ptr %y_val, align 4
  %smul50 = call { i32, i1 } @llvm.smul.with.overflow.i32(i32 %y_val48, i32 %y_val49)
  %multmp51 = extractvalue { i32, i1 } %smul50, 0
  %mul.overflow52 = extractvalue { i32, i1 } %smul50, 1
  %safe.multmp53 = select i1 %mul.overflow52, i32 2147483647, i32 %multmp51
  store i32 %safe.multmp53, ptr %y2, align 4
  %z2 = alloca i32, align 4
  %z_val54 = load i32, ptr %z_val, align 4
  %z_val55 = load i32, ptr %z_val, align 4
  %smul56 = call { i32, i1 } @llvm.smul.with.overflow.i32(i32 %z_val54, i32 %z_val55)
  %multmp57 = extractvalue { i32, i1 } %smul56, 0
  %mul.overflow58 = extractvalue { i32, i1 } %smul56, 1
  %safe.multmp59 = select i1 %mul.overflow58, i32 2147483647, i32 %multmp57
  store i32 %safe.multmp59, ptr %z2, align 4
  %r260 = load i32, ptr %r2, align 4
  %s261 = load i32, ptr %s2, align 4
  %sadd = call { i32, i1 } @llvm.sadd.with.overflow.i32(i32 %r260, i32 %s261)
  %addtmp = extractvalue { i32, i1 } %sadd, 0
  %add.overflow = extractvalue { i32, i1 } %sadd, 1
  %safe.addtmp = select i1 %add.overflow, i32 2147483647, i32 %addtmp
  %t262 = load i32, ptr %t2, align 4
  %sadd63 = call { i32, i1 } @llvm.sadd.with.overflow.i32(i32 %safe.addtmp, i32 %t262)
  %addtmp64 = extractvalue { i32, i1 } %sadd63, 0
  %add.overflow65 = extractvalue { i32, i1 } %sadd63, 1
  %safe.addtmp66 = select i1 %add.overflow65, i32 2147483647, i32 %addtmp64
  %u267 = load i32, ptr %u2, align 4
  %sadd68 = call { i32, i1 } @llvm.sadd.with.overflow.i32(i32 %safe.addtmp66, i32 %u267)
  %addtmp69 = extractvalue { i32, i1 } %sadd68, 0
  %add.overflow70 = extractvalue { i32, i1 } %sadd68, 1
  %safe.addtmp71 = select i1 %add.overflow70, i32 2147483647, i32 %addtmp69
  %v272 = load i32, ptr %v2, align 4
  %sadd73 = call { i32, i1 } @llvm.sadd.with.overflow.i32(i32 %safe.addtmp71, i32 %v272)
  %addtmp74 = extractvalue { i32, i1 } %sadd73, 0
  %add.overflow75 = extractvalue { i32, i1 } %sadd73, 1
  %safe.addtmp76 = select i1 %add.overflow75, i32 2147483647, i32 %addtmp74
  %w277 = load i32, ptr %w2, align 4
  %sadd78 = call { i32, i1 } @llvm.sadd.with.overflow.i32(i32 %safe.addtmp76, i32 %w277)
  %addtmp79 = extractvalue { i32, i1 } %sadd78, 0
  %add.overflow80 = extractvalue { i32, i1 } %sadd78, 1
  %safe.addtmp81 = select i1 %add.overflow80, i32 2147483647, i32 %addtmp79
  %x282 = load i32, ptr %x2, align 4
  %sadd83 = call { i32, i1 } @llvm.sadd.with.overflow.i32(i32 %safe.addtmp81, i32 %x282)
  %addtmp84 = extractvalue { i32, i1 } %sadd83, 0
  %add.overflow85 = extractvalue { i32, i1 } %sadd83, 1
  %safe.addtmp86 = select i1 %add.overflow85, i32 2147483647, i32 %addtmp84
  %y287 = load i32, ptr %y2, align 4
  %sadd88 = call { i32, i1 } @llvm.sadd.with.overflow.i32(i32 %safe.addtmp86, i32 %y287)
  %addtmp89 = extractvalue { i32, i1 } %sadd88, 0
  %add.overflow90 = extractvalue { i32, i1 } %sadd88, 1
  %safe.addtmp91 = select i1 %add.overflow90, i32 2147483647, i32 %addtmp89
  %z292 = load i32, ptr %z2, align 4
  %sadd93 = call { i32, i1 } @llvm.sadd.with.overflow.i32(i32 %safe.addtmp91, i32 %z292)
  %addtmp94 = extractvalue { i32, i1 } %sadd93, 0
  %add.overflow95 = extractvalue { i32, i1 } %sadd93, 1
  %safe.addtmp96 = select i1 %add.overflow95, i32 2147483647, i32 %addtmp94
  %result.val = insertvalue { i32, ptr, i8 } undef, i32 %safe.addtmp96, 0
  %result.err = insertvalue { i32, ptr, i8 } %result.val, ptr null, 1
  %result.is_error = insertvalue { i32, ptr, i8 } %result.err, i8 0, 2
  ret { i32, ptr, i8 } %result.is_error
}

define { i1, ptr, i8 } @wave_equals(%Wave9 %a, %Wave9 %b) {
entry:
  %a_alloca = alloca %Wave9, align 8
  store %Wave9 %a, ptr %a_alloca, align 1
  %b_alloca = alloca %Wave9, align 8
  store %Wave9 %b, ptr %b_alloca, align 1
  %r.ptr = getelementptr inbounds nuw %Wave9, ptr %a_alloca, i32 0, i32 0
  %r = load i8, ptr %r.ptr, align 1
  %r.ptr1 = getelementptr inbounds nuw %Wave9, ptr %b_alloca, i32 0, i32 0
  %r2 = load i8, ptr %r.ptr1, align 1
  %eqtmp = icmp eq i8 %r, %r2
  %and.lhs = icmp ne i1 %eqtmp, false
  br i1 %and.lhs, label %and.rhs, label %and.merge

and.rhs:                                          ; preds = %entry
  %s.ptr = getelementptr inbounds nuw %Wave9, ptr %a_alloca, i32 0, i32 1
  %s = load i8, ptr %s.ptr, align 1
  %s.ptr3 = getelementptr inbounds nuw %Wave9, ptr %b_alloca, i32 0, i32 1
  %s4 = load i8, ptr %s.ptr3, align 1
  %eqtmp5 = icmp eq i8 %s, %s4
  %and.rhs6 = icmp ne i1 %eqtmp5, false
  br label %and.merge

and.merge:                                        ; preds = %and.rhs, %entry
  %and.result = phi i1 [ false, %entry ], [ %and.rhs6, %and.rhs ]
  %and.lhs7 = icmp ne i1 %and.result, false
  br i1 %and.lhs7, label %and.rhs8, label %and.merge13

and.rhs8:                                         ; preds = %and.merge
  %t.ptr = getelementptr inbounds nuw %Wave9, ptr %a_alloca, i32 0, i32 2
  %t = load i8, ptr %t.ptr, align 1
  %t.ptr9 = getelementptr inbounds nuw %Wave9, ptr %b_alloca, i32 0, i32 2
  %t10 = load i8, ptr %t.ptr9, align 1
  %eqtmp11 = icmp eq i8 %t, %t10
  %and.rhs12 = icmp ne i1 %eqtmp11, false
  br label %and.merge13

and.merge13:                                      ; preds = %and.rhs8, %and.merge
  %and.result14 = phi i1 [ false, %and.merge ], [ %and.rhs12, %and.rhs8 ]
  %and.lhs15 = icmp ne i1 %and.result14, false
  br i1 %and.lhs15, label %and.rhs16, label %and.merge21

and.rhs16:                                        ; preds = %and.merge13
  %u.ptr = getelementptr inbounds nuw %Wave9, ptr %a_alloca, i32 0, i32 3
  %u = load i8, ptr %u.ptr, align 1
  %u.ptr17 = getelementptr inbounds nuw %Wave9, ptr %b_alloca, i32 0, i32 3
  %u18 = load i8, ptr %u.ptr17, align 1
  %eqtmp19 = icmp eq i8 %u, %u18
  %and.rhs20 = icmp ne i1 %eqtmp19, false
  br label %and.merge21

and.merge21:                                      ; preds = %and.rhs16, %and.merge13
  %and.result22 = phi i1 [ false, %and.merge13 ], [ %and.rhs20, %and.rhs16 ]
  %and.lhs23 = icmp ne i1 %and.result22, false
  br i1 %and.lhs23, label %and.rhs24, label %and.merge29

and.rhs24:                                        ; preds = %and.merge21
  %v.ptr = getelementptr inbounds nuw %Wave9, ptr %a_alloca, i32 0, i32 4
  %v = load i8, ptr %v.ptr, align 1
  %v.ptr25 = getelementptr inbounds nuw %Wave9, ptr %b_alloca, i32 0, i32 4
  %v26 = load i8, ptr %v.ptr25, align 1
  %eqtmp27 = icmp eq i8 %v, %v26
  %and.rhs28 = icmp ne i1 %eqtmp27, false
  br label %and.merge29

and.merge29:                                      ; preds = %and.rhs24, %and.merge21
  %and.result30 = phi i1 [ false, %and.merge21 ], [ %and.rhs28, %and.rhs24 ]
  %and.lhs31 = icmp ne i1 %and.result30, false
  br i1 %and.lhs31, label %and.rhs32, label %and.merge37

and.rhs32:                                        ; preds = %and.merge29
  %w.ptr = getelementptr inbounds nuw %Wave9, ptr %a_alloca, i32 0, i32 5
  %w = load i8, ptr %w.ptr, align 1
  %w.ptr33 = getelementptr inbounds nuw %Wave9, ptr %b_alloca, i32 0, i32 5
  %w34 = load i8, ptr %w.ptr33, align 1
  %eqtmp35 = icmp eq i8 %w, %w34
  %and.rhs36 = icmp ne i1 %eqtmp35, false
  br label %and.merge37

and.merge37:                                      ; preds = %and.rhs32, %and.merge29
  %and.result38 = phi i1 [ false, %and.merge29 ], [ %and.rhs36, %and.rhs32 ]
  %and.lhs39 = icmp ne i1 %and.result38, false
  br i1 %and.lhs39, label %and.rhs40, label %and.merge45

and.rhs40:                                        ; preds = %and.merge37
  %x.ptr = getelementptr inbounds nuw %Wave9, ptr %a_alloca, i32 0, i32 6
  %x = load i8, ptr %x.ptr, align 1
  %x.ptr41 = getelementptr inbounds nuw %Wave9, ptr %b_alloca, i32 0, i32 6
  %x42 = load i8, ptr %x.ptr41, align 1
  %eqtmp43 = icmp eq i8 %x, %x42
  %and.rhs44 = icmp ne i1 %eqtmp43, false
  br label %and.merge45

and.merge45:                                      ; preds = %and.rhs40, %and.merge37
  %and.result46 = phi i1 [ false, %and.merge37 ], [ %and.rhs44, %and.rhs40 ]
  %and.lhs47 = icmp ne i1 %and.result46, false
  br i1 %and.lhs47, label %and.rhs48, label %and.merge53

and.rhs48:                                        ; preds = %and.merge45
  %y.ptr = getelementptr inbounds nuw %Wave9, ptr %a_alloca, i32 0, i32 7
  %y = load i8, ptr %y.ptr, align 1
  %y.ptr49 = getelementptr inbounds nuw %Wave9, ptr %b_alloca, i32 0, i32 7
  %y50 = load i8, ptr %y.ptr49, align 1
  %eqtmp51 = icmp eq i8 %y, %y50
  %and.rhs52 = icmp ne i1 %eqtmp51, false
  br label %and.merge53

and.merge53:                                      ; preds = %and.rhs48, %and.merge45
  %and.result54 = phi i1 [ false, %and.merge45 ], [ %and.rhs52, %and.rhs48 ]
  %and.lhs55 = icmp ne i1 %and.result54, false
  br i1 %and.lhs55, label %and.rhs56, label %and.merge61

and.rhs56:                                        ; preds = %and.merge53
  %z.ptr = getelementptr inbounds nuw %Wave9, ptr %a_alloca, i32 0, i32 8
  %z = load i8, ptr %z.ptr, align 1
  %z.ptr57 = getelementptr inbounds nuw %Wave9, ptr %b_alloca, i32 0, i32 8
  %z58 = load i8, ptr %z.ptr57, align 1
  %eqtmp59 = icmp eq i8 %z, %z58
  %and.rhs60 = icmp ne i1 %eqtmp59, false
  br label %and.merge61

and.merge61:                                      ; preds = %and.rhs56, %and.merge53
  %and.result62 = phi i1 [ false, %and.merge53 ], [ %and.rhs60, %and.rhs56 ]
  %result.val = insertvalue { i1, ptr, i8 } undef, i1 %and.result62, 0
  %result.err = insertvalue { i1, ptr, i8 } %result.val, ptr null, 1
  %result.is_error = insertvalue { i1, ptr, i8 } %result.err, i8 0, 2
  ret { i1, ptr, i8 } %result.is_error
}

define { i16, ptr, i8 } @nyte_from_wave(%Wave9 %w) {
entry:
  %w_alloca = alloca %Wave9, align 8
  store %Wave9 %w, ptr %w_alloca, align 1
  %w1 = load %Wave9, ptr %w_alloca, align 1
  %x = extractvalue %Wave9 %w1, 6
  %w2 = load %Wave9, ptr %w_alloca, align 1
  %y = extractvalue %Wave9 %w2, 7
  %w3 = load %Wave9, ptr %w_alloca, align 1
  %z = extractvalue %Wave9 %w3, 8
  %w4 = load %Wave9, ptr %w_alloca, align 1
  %u = extractvalue %Wave9 %w4, 3
  %w5 = load %Wave9, ptr %w_alloca, align 1
  %r = extractvalue %Wave9 %w5, 0
  %calltmp = call i16 @aria_pack_nyte(i8 %x, i8 %y, i8 %z, i8 %u, i8 %r)
  %result.val = insertvalue { i16, ptr, i8 } undef, i16 %calltmp, 0
  %result.err = insertvalue { i16, ptr, i8 } %result.val, ptr null, 1
  %result.is_error = insertvalue { i16, ptr, i8 } %result.err, i8 0, 2
  ret { i16, ptr, i8 } %result.is_error
}

define { %Wave9, ptr, i8 } @wave_from_nyte(i16 %encoded) {
entry:
  %n0 = alloca i8, align 1
  %calltmp = call i8 @aria_nyte_get_nit(i16 %encoded, i32 0)
  store i8 %calltmp, ptr %n0, align 1
  %n1 = alloca i8, align 1
  %calltmp1 = call i8 @aria_nyte_get_nit(i16 %encoded, i32 1)
  store i8 %calltmp1, ptr %n1, align 1
  %n2 = alloca i8, align 1
  %calltmp2 = call i8 @aria_nyte_get_nit(i16 %encoded, i32 2)
  store i8 %calltmp2, ptr %n2, align 1
  %n3 = alloca i8, align 1
  %calltmp3 = call i8 @aria_nyte_get_nit(i16 %encoded, i32 3)
  store i8 %calltmp3, ptr %n3, align 1
  %n4 = alloca i8, align 1
  %calltmp4 = call i8 @aria_nyte_get_nit(i16 %encoded, i32 4)
  store i8 %calltmp4, ptr %n4, align 1
  %zero = alloca i8, align 1
  store i8 0, ptr %zero, align 1
  %struct.tmp = alloca %Wave9, align 8
  %n45 = load i8, ptr %n4, align 1
  %r.ptr = getelementptr inbounds nuw %Wave9, ptr %struct.tmp, i32 0, i32 0
  store i8 %n45, ptr %r.ptr, align 1
  %zero6 = load i8, ptr %zero, align 1
  %s.ptr = getelementptr inbounds nuw %Wave9, ptr %struct.tmp, i32 0, i32 1
  store i8 %zero6, ptr %s.ptr, align 1
  %zero7 = load i8, ptr %zero, align 1
  %t.ptr = getelementptr inbounds nuw %Wave9, ptr %struct.tmp, i32 0, i32 2
  store i8 %zero7, ptr %t.ptr, align 1
  %n38 = load i8, ptr %n3, align 1
  %u.ptr = getelementptr inbounds nuw %Wave9, ptr %struct.tmp, i32 0, i32 3
  store i8 %n38, ptr %u.ptr, align 1
  %zero9 = load i8, ptr %zero, align 1
  %v.ptr = getelementptr inbounds nuw %Wave9, ptr %struct.tmp, i32 0, i32 4
  store i8 %zero9, ptr %v.ptr, align 1
  %zero10 = load i8, ptr %zero, align 1
  %w.ptr = getelementptr inbounds nuw %Wave9, ptr %struct.tmp, i32 0, i32 5
  store i8 %zero10, ptr %w.ptr, align 1
  %n011 = load i8, ptr %n0, align 1
  %x.ptr = getelementptr inbounds nuw %Wave9, ptr %struct.tmp, i32 0, i32 6
  store i8 %n011, ptr %x.ptr, align 1
  %n112 = load i8, ptr %n1, align 1
  %y.ptr = getelementptr inbounds nuw %Wave9, ptr %struct.tmp, i32 0, i32 7
  store i8 %n112, ptr %y.ptr, align 1
  %n213 = load i8, ptr %n2, align 1
  %z.ptr = getelementptr inbounds nuw %Wave9, ptr %struct.tmp, i32 0, i32 8
  store i8 %n213, ptr %z.ptr, align 1
  %struct.val = load %Wave9, ptr %struct.tmp, align 1
  %result.val = insertvalue { %Wave9, ptr, i8 } undef, %Wave9 %struct.val, 0
  %result.err = insertvalue { %Wave9, ptr, i8 } %result.val, ptr null, 1
  %result.is_error = insertvalue { %Wave9, ptr, i8 } %result.err, i8 0, 2
  ret { %Wave9, ptr, i8 } %result.is_error
}

define { %Wave9, ptr, i8 } @wave_from_spatial(i8 %x, i8 %y, i8 %z) {
entry:
  %struct.tmp = alloca %Wave9, align 8
  %r.ptr = getelementptr inbounds nuw %Wave9, ptr %struct.tmp, i32 0, i32 0
  store i64 0, ptr %r.ptr, align 4
  %s.ptr = getelementptr inbounds nuw %Wave9, ptr %struct.tmp, i32 0, i32 1
  store i64 0, ptr %s.ptr, align 4
  %t.ptr = getelementptr inbounds nuw %Wave9, ptr %struct.tmp, i32 0, i32 2
  store i64 0, ptr %t.ptr, align 4
  %u.ptr = getelementptr inbounds nuw %Wave9, ptr %struct.tmp, i32 0, i32 3
  store i64 0, ptr %u.ptr, align 4
  %v.ptr = getelementptr inbounds nuw %Wave9, ptr %struct.tmp, i32 0, i32 4
  store i64 0, ptr %v.ptr, align 4
  %w.ptr = getelementptr inbounds nuw %Wave9, ptr %struct.tmp, i32 0, i32 5
  store i64 0, ptr %w.ptr, align 4
  %x.ptr = getelementptr inbounds nuw %Wave9, ptr %struct.tmp, i32 0, i32 6
  store i8 %x, ptr %x.ptr, align 1
  %y.ptr = getelementptr inbounds nuw %Wave9, ptr %struct.tmp, i32 0, i32 7
  store i8 %y, ptr %y.ptr, align 1
  %z.ptr = getelementptr inbounds nuw %Wave9, ptr %struct.tmp, i32 0, i32 8
  store i8 %z, ptr %z.ptr, align 1
  %struct.val = load %Wave9, ptr %struct.tmp, align 1
  %result.val = insertvalue { %Wave9, ptr, i8 } undef, %Wave9 %struct.val, 0
  %result.err = insertvalue { %Wave9, ptr, i8 } %result.val, ptr null, 1
  %result.is_error = insertvalue { %Wave9, ptr, i8 } %result.err, i8 0, 2
  ret { %Wave9, ptr, i8 } %result.is_error
}

define { i8, ptr, i8 } @wave_spatial_x(%Wave9 %w) {
entry:
  %w_alloca = alloca %Wave9, align 8
  store %Wave9 %w, ptr %w_alloca, align 1
  %x.ptr = getelementptr inbounds nuw %Wave9, ptr %w_alloca, i32 0, i32 6
  %x = load i8, ptr %x.ptr, align 1
  %result.val = insertvalue { i8, ptr, i8 } undef, i8 %x, 0
  %result.err = insertvalue { i8, ptr, i8 } %result.val, ptr null, 1
  %result.is_error = insertvalue { i8, ptr, i8 } %result.err, i8 0, 2
  ret { i8, ptr, i8 } %result.is_error
}

define { i8, ptr, i8 } @wave_spatial_y(%Wave9 %w) {
entry:
  %w_alloca = alloca %Wave9, align 8
  store %Wave9 %w, ptr %w_alloca, align 1
  %y.ptr = getelementptr inbounds nuw %Wave9, ptr %w_alloca, i32 0, i32 7
  %y = load i8, ptr %y.ptr, align 1
  %result.val = insertvalue { i8, ptr, i8 } undef, i8 %y, 0
  %result.err = insertvalue { i8, ptr, i8 } %result.val, ptr null, 1
  %result.is_error = insertvalue { i8, ptr, i8 } %result.err, i8 0, 2
  ret { i8, ptr, i8 } %result.is_error
}

define { i8, ptr, i8 } @wave_spatial_z(%Wave9 %w) {
entry:
  %w_alloca = alloca %Wave9, align 8
  store %Wave9 %w, ptr %w_alloca, align 1
  %z.ptr = getelementptr inbounds nuw %Wave9, ptr %w_alloca, i32 0, i32 8
  %z = load i8, ptr %z.ptr, align 1
  %result.val = insertvalue { i8, ptr, i8 } undef, i8 %z, 0
  %result.err = insertvalue { i8, ptr, i8 } %result.val, ptr null, 1
  %result.is_error = insertvalue { i8, ptr, i8 } %result.err, i8 0, 2
  ret { i8, ptr, i8 } %result.is_error
}

define { %Wave9, ptr, i8 } @wave_from_quantum(i8 %u, i8 %v, i8 %w) {
entry:
  %zero = alloca i8, align 1
  store i8 0, ptr %zero, align 1
  %struct.tmp = alloca %Wave9, align 8
  %zero1 = load i8, ptr %zero, align 1
  %r.ptr = getelementptr inbounds nuw %Wave9, ptr %struct.tmp, i32 0, i32 0
  store i8 %zero1, ptr %r.ptr, align 1
  %zero2 = load i8, ptr %zero, align 1
  %s.ptr = getelementptr inbounds nuw %Wave9, ptr %struct.tmp, i32 0, i32 1
  store i8 %zero2, ptr %s.ptr, align 1
  %zero3 = load i8, ptr %zero, align 1
  %t.ptr = getelementptr inbounds nuw %Wave9, ptr %struct.tmp, i32 0, i32 2
  store i8 %zero3, ptr %t.ptr, align 1
  %u.ptr = getelementptr inbounds nuw %Wave9, ptr %struct.tmp, i32 0, i32 3
  store i8 %u, ptr %u.ptr, align 1
  %v.ptr = getelementptr inbounds nuw %Wave9, ptr %struct.tmp, i32 0, i32 4
  store i8 %v, ptr %v.ptr, align 1
  %w.ptr = getelementptr inbounds nuw %Wave9, ptr %struct.tmp, i32 0, i32 5
  store i8 %w, ptr %w.ptr, align 1
  %zero4 = load i8, ptr %zero, align 1
  %x.ptr = getelementptr inbounds nuw %Wave9, ptr %struct.tmp, i32 0, i32 6
  store i8 %zero4, ptr %x.ptr, align 1
  %zero5 = load i8, ptr %zero, align 1
  %y.ptr = getelementptr inbounds nuw %Wave9, ptr %struct.tmp, i32 0, i32 7
  store i8 %zero5, ptr %y.ptr, align 1
  %zero6 = load i8, ptr %zero, align 1
  %z.ptr = getelementptr inbounds nuw %Wave9, ptr %struct.tmp, i32 0, i32 8
  store i8 %zero6, ptr %z.ptr, align 1
  %struct.val = load %Wave9, ptr %struct.tmp, align 1
  %result.val = insertvalue { %Wave9, ptr, i8 } undef, %Wave9 %struct.val, 0
  %result.err = insertvalue { %Wave9, ptr, i8 } %result.val, ptr null, 1
  %result.is_error = insertvalue { %Wave9, ptr, i8 } %result.err, i8 0, 2
  ret { %Wave9, ptr, i8 } %result.is_error
}

define { %Wave9, ptr, i8 } @wave_from_full(i8 %r, i8 %s, i8 %t, i8 %u, i8 %v, i8 %w, i8 %x, i8 %y, i8 %z) {
entry:
  %struct.tmp = alloca %Wave9, align 8
  %r.ptr = getelementptr inbounds nuw %Wave9, ptr %struct.tmp, i32 0, i32 0
  store i8 %r, ptr %r.ptr, align 1
  %s.ptr = getelementptr inbounds nuw %Wave9, ptr %struct.tmp, i32 0, i32 1
  store i8 %s, ptr %s.ptr, align 1
  %t.ptr = getelementptr inbounds nuw %Wave9, ptr %struct.tmp, i32 0, i32 2
  store i8 %t, ptr %t.ptr, align 1
  %u.ptr = getelementptr inbounds nuw %Wave9, ptr %struct.tmp, i32 0, i32 3
  store i8 %u, ptr %u.ptr, align 1
  %v.ptr = getelementptr inbounds nuw %Wave9, ptr %struct.tmp, i32 0, i32 4
  store i8 %v, ptr %v.ptr, align 1
  %w.ptr = getelementptr inbounds nuw %Wave9, ptr %struct.tmp, i32 0, i32 5
  store i8 %w, ptr %w.ptr, align 1
  %x.ptr = getelementptr inbounds nuw %Wave9, ptr %struct.tmp, i32 0, i32 6
  store i8 %x, ptr %x.ptr, align 1
  %y.ptr = getelementptr inbounds nuw %Wave9, ptr %struct.tmp, i32 0, i32 7
  store i8 %y, ptr %y.ptr, align 1
  %z.ptr = getelementptr inbounds nuw %Wave9, ptr %struct.tmp, i32 0, i32 8
  store i8 %z, ptr %z.ptr, align 1
  %struct.val = load %Wave9, ptr %struct.tmp, align 1
  %result.val = insertvalue { %Wave9, ptr, i8 } undef, %Wave9 %struct.val, 0
  %result.err = insertvalue { %Wave9, ptr, i8 } %result.val, ptr null, 1
  %result.is_error = insertvalue { %Wave9, ptr, i8 } %result.err, i8 0, 2
  ret { %Wave9, ptr, i8 } %result.is_error
}

define { %Wave9, ptr, i8 } @wave_from_state(i8 %r, i8 %s) {
entry:
  %zero = alloca i8, align 1
  store i8 0, ptr %zero, align 1
  %struct.tmp = alloca %Wave9, align 8
  %r.ptr = getelementptr inbounds nuw %Wave9, ptr %struct.tmp, i32 0, i32 0
  store i8 %r, ptr %r.ptr, align 1
  %s.ptr = getelementptr inbounds nuw %Wave9, ptr %struct.tmp, i32 0, i32 1
  store i8 %s, ptr %s.ptr, align 1
  %zero1 = load i8, ptr %zero, align 1
  %t.ptr = getelementptr inbounds nuw %Wave9, ptr %struct.tmp, i32 0, i32 2
  store i8 %zero1, ptr %t.ptr, align 1
  %zero2 = load i8, ptr %zero, align 1
  %u.ptr = getelementptr inbounds nuw %Wave9, ptr %struct.tmp, i32 0, i32 3
  store i8 %zero2, ptr %u.ptr, align 1
  %zero3 = load i8, ptr %zero, align 1
  %v.ptr = getelementptr inbounds nuw %Wave9, ptr %struct.tmp, i32 0, i32 4
  store i8 %zero3, ptr %v.ptr, align 1
  %zero4 = load i8, ptr %zero, align 1
  %w.ptr = getelementptr inbounds nuw %Wave9, ptr %struct.tmp, i32 0, i32 5
  store i8 %zero4, ptr %w.ptr, align 1
  %zero5 = load i8, ptr %zero, align 1
  %x.ptr = getelementptr inbounds nuw %Wave9, ptr %struct.tmp, i32 0, i32 6
  store i8 %zero5, ptr %x.ptr, align 1
  %zero6 = load i8, ptr %zero, align 1
  %y.ptr = getelementptr inbounds nuw %Wave9, ptr %struct.tmp, i32 0, i32 7
  store i8 %zero6, ptr %y.ptr, align 1
  %zero7 = load i8, ptr %zero, align 1
  %z.ptr = getelementptr inbounds nuw %Wave9, ptr %struct.tmp, i32 0, i32 8
  store i8 %zero7, ptr %z.ptr, align 1
  %struct.val = load %Wave9, ptr %struct.tmp, align 1
  %result.val = insertvalue { %Wave9, ptr, i8 } undef, %Wave9 %struct.val, 0
  %result.err = insertvalue { %Wave9, ptr, i8 } %result.val, ptr null, 1
  %result.is_error = insertvalue { %Wave9, ptr, i8 } %result.err, i8 0, 2
  ret { %Wave9, ptr, i8 } %result.is_error
}

define { i32, ptr, i8 } @wave_dot(%Wave9 %a, %Wave9 %b) {
entry:
  %a_alloca = alloca %Wave9, align 8
  store %Wave9 %a, ptr %a_alloca, align 1
  %b_alloca = alloca %Wave9, align 8
  store %Wave9 %b, ptr %b_alloca, align 1
  %r1 = alloca i32, align 4
  %r.ptr = getelementptr inbounds nuw %Wave9, ptr %a_alloca, i32 0, i32 0
  %r = load i8, ptr %r.ptr, align 1
  %cast.sext = sext i8 %r to i32
  store i32 %cast.sext, ptr %r1, align 4
  %r2 = alloca i32, align 4
  %r.ptr1 = getelementptr inbounds nuw %Wave9, ptr %b_alloca, i32 0, i32 0
  %r3 = load i8, ptr %r.ptr1, align 1
  %cast.sext4 = sext i8 %r3 to i32
  store i32 %cast.sext4, ptr %r2, align 4
  %s1 = alloca i32, align 4
  %s.ptr = getelementptr inbounds nuw %Wave9, ptr %a_alloca, i32 0, i32 1
  %s = load i8, ptr %s.ptr, align 1
  %cast.sext5 = sext i8 %s to i32
  store i32 %cast.sext5, ptr %s1, align 4
  %s2 = alloca i32, align 4
  %s.ptr6 = getelementptr inbounds nuw %Wave9, ptr %b_alloca, i32 0, i32 1
  %s7 = load i8, ptr %s.ptr6, align 1
  %cast.sext8 = sext i8 %s7 to i32
  store i32 %cast.sext8, ptr %s2, align 4
  %t1 = alloca i32, align 4
  %t.ptr = getelementptr inbounds nuw %Wave9, ptr %a_alloca, i32 0, i32 2
  %t = load i8, ptr %t.ptr, align 1
  %cast.sext9 = sext i8 %t to i32
  store i32 %cast.sext9, ptr %t1, align 4
  %t2 = alloca i32, align 4
  %t.ptr10 = getelementptr inbounds nuw %Wave9, ptr %b_alloca, i32 0, i32 2
  %t11 = load i8, ptr %t.ptr10, align 1
  %cast.sext12 = sext i8 %t11 to i32
  store i32 %cast.sext12, ptr %t2, align 4
  %u1 = alloca i32, align 4
  %u.ptr = getelementptr inbounds nuw %Wave9, ptr %a_alloca, i32 0, i32 3
  %u = load i8, ptr %u.ptr, align 1
  %cast.sext13 = sext i8 %u to i32
  store i32 %cast.sext13, ptr %u1, align 4
  %u2 = alloca i32, align 4
  %u.ptr14 = getelementptr inbounds nuw %Wave9, ptr %b_alloca, i32 0, i32 3
  %u15 = load i8, ptr %u.ptr14, align 1
  %cast.sext16 = sext i8 %u15 to i32
  store i32 %cast.sext16, ptr %u2, align 4
  %v1 = alloca i32, align 4
  %v.ptr = getelementptr inbounds nuw %Wave9, ptr %a_alloca, i32 0, i32 4
  %v = load i8, ptr %v.ptr, align 1
  %cast.sext17 = sext i8 %v to i32
  store i32 %cast.sext17, ptr %v1, align 4
  %v2 = alloca i32, align 4
  %v.ptr18 = getelementptr inbounds nuw %Wave9, ptr %b_alloca, i32 0, i32 4
  %v19 = load i8, ptr %v.ptr18, align 1
  %cast.sext20 = sext i8 %v19 to i32
  store i32 %cast.sext20, ptr %v2, align 4
  %w1 = alloca i32, align 4
  %w.ptr = getelementptr inbounds nuw %Wave9, ptr %a_alloca, i32 0, i32 5
  %w = load i8, ptr %w.ptr, align 1
  %cast.sext21 = sext i8 %w to i32
  store i32 %cast.sext21, ptr %w1, align 4
  %w2 = alloca i32, align 4
  %w.ptr22 = getelementptr inbounds nuw %Wave9, ptr %b_alloca, i32 0, i32 5
  %w23 = load i8, ptr %w.ptr22, align 1
  %cast.sext24 = sext i8 %w23 to i32
  store i32 %cast.sext24, ptr %w2, align 4
  %x1 = alloca i32, align 4
  %x.ptr = getelementptr inbounds nuw %Wave9, ptr %a_alloca, i32 0, i32 6
  %x = load i8, ptr %x.ptr, align 1
  %cast.sext25 = sext i8 %x to i32
  store i32 %cast.sext25, ptr %x1, align 4
  %x2 = alloca i32, align 4
  %x.ptr26 = getelementptr inbounds nuw %Wave9, ptr %b_alloca, i32 0, i32 6
  %x27 = load i8, ptr %x.ptr26, align 1
  %cast.sext28 = sext i8 %x27 to i32
  store i32 %cast.sext28, ptr %x2, align 4
  %y1 = alloca i32, align 4
  %y.ptr = getelementptr inbounds nuw %Wave9, ptr %a_alloca, i32 0, i32 7
  %y = load i8, ptr %y.ptr, align 1
  %cast.sext29 = sext i8 %y to i32
  store i32 %cast.sext29, ptr %y1, align 4
  %y2 = alloca i32, align 4
  %y.ptr30 = getelementptr inbounds nuw %Wave9, ptr %b_alloca, i32 0, i32 7
  %y31 = load i8, ptr %y.ptr30, align 1
  %cast.sext32 = sext i8 %y31 to i32
  store i32 %cast.sext32, ptr %y2, align 4
  %z1 = alloca i32, align 4
  %z.ptr = getelementptr inbounds nuw %Wave9, ptr %a_alloca, i32 0, i32 8
  %z = load i8, ptr %z.ptr, align 1
  %cast.sext33 = sext i8 %z to i32
  store i32 %cast.sext33, ptr %z1, align 4
  %z2 = alloca i32, align 4
  %z.ptr34 = getelementptr inbounds nuw %Wave9, ptr %b_alloca, i32 0, i32 8
  %z35 = load i8, ptr %z.ptr34, align 1
  %cast.sext36 = sext i8 %z35 to i32
  store i32 %cast.sext36, ptr %z2, align 4
  %r137 = load i32, ptr %r1, align 4
  %r238 = load i32, ptr %r2, align 4
  %smul = call { i32, i1 } @llvm.smul.with.overflow.i32(i32 %r137, i32 %r238)
  %multmp = extractvalue { i32, i1 } %smul, 0
  %mul.overflow = extractvalue { i32, i1 } %smul, 1
  %safe.multmp = select i1 %mul.overflow, i32 2147483647, i32 %multmp
  %s139 = load i32, ptr %s1, align 4
  %s240 = load i32, ptr %s2, align 4
  %smul41 = call { i32, i1 } @llvm.smul.with.overflow.i32(i32 %s139, i32 %s240)
  %multmp42 = extractvalue { i32, i1 } %smul41, 0
  %mul.overflow43 = extractvalue { i32, i1 } %smul41, 1
  %safe.multmp44 = select i1 %mul.overflow43, i32 2147483647, i32 %multmp42
  %sadd = call { i32, i1 } @llvm.sadd.with.overflow.i32(i32 %safe.multmp, i32 %safe.multmp44)
  %addtmp = extractvalue { i32, i1 } %sadd, 0
  %add.overflow = extractvalue { i32, i1 } %sadd, 1
  %safe.addtmp = select i1 %add.overflow, i32 2147483647, i32 %addtmp
  %t145 = load i32, ptr %t1, align 4
  %t246 = load i32, ptr %t2, align 4
  %smul47 = call { i32, i1 } @llvm.smul.with.overflow.i32(i32 %t145, i32 %t246)
  %multmp48 = extractvalue { i32, i1 } %smul47, 0
  %mul.overflow49 = extractvalue { i32, i1 } %smul47, 1
  %safe.multmp50 = select i1 %mul.overflow49, i32 2147483647, i32 %multmp48
  %sadd51 = call { i32, i1 } @llvm.sadd.with.overflow.i32(i32 %safe.addtmp, i32 %safe.multmp50)
  %addtmp52 = extractvalue { i32, i1 } %sadd51, 0
  %add.overflow53 = extractvalue { i32, i1 } %sadd51, 1
  %safe.addtmp54 = select i1 %add.overflow53, i32 2147483647, i32 %addtmp52
  %u155 = load i32, ptr %u1, align 4
  %u256 = load i32, ptr %u2, align 4
  %smul57 = call { i32, i1 } @llvm.smul.with.overflow.i32(i32 %u155, i32 %u256)
  %multmp58 = extractvalue { i32, i1 } %smul57, 0
  %mul.overflow59 = extractvalue { i32, i1 } %smul57, 1
  %safe.multmp60 = select i1 %mul.overflow59, i32 2147483647, i32 %multmp58
  %sadd61 = call { i32, i1 } @llvm.sadd.with.overflow.i32(i32 %safe.addtmp54, i32 %safe.multmp60)
  %addtmp62 = extractvalue { i32, i1 } %sadd61, 0
  %add.overflow63 = extractvalue { i32, i1 } %sadd61, 1
  %safe.addtmp64 = select i1 %add.overflow63, i32 2147483647, i32 %addtmp62
  %v165 = load i32, ptr %v1, align 4
  %v266 = load i32, ptr %v2, align 4
  %smul67 = call { i32, i1 } @llvm.smul.with.overflow.i32(i32 %v165, i32 %v266)
  %multmp68 = extractvalue { i32, i1 } %smul67, 0
  %mul.overflow69 = extractvalue { i32, i1 } %smul67, 1
  %safe.multmp70 = select i1 %mul.overflow69, i32 2147483647, i32 %multmp68
  %sadd71 = call { i32, i1 } @llvm.sadd.with.overflow.i32(i32 %safe.addtmp64, i32 %safe.multmp70)
  %addtmp72 = extractvalue { i32, i1 } %sadd71, 0
  %add.overflow73 = extractvalue { i32, i1 } %sadd71, 1
  %safe.addtmp74 = select i1 %add.overflow73, i32 2147483647, i32 %addtmp72
  %w175 = load i32, ptr %w1, align 4
  %w276 = load i32, ptr %w2, align 4
  %smul77 = call { i32, i1 } @llvm.smul.with.overflow.i32(i32 %w175, i32 %w276)
  %multmp78 = extractvalue { i32, i1 } %smul77, 0
  %mul.overflow79 = extractvalue { i32, i1 } %smul77, 1
  %safe.multmp80 = select i1 %mul.overflow79, i32 2147483647, i32 %multmp78
  %sadd81 = call { i32, i1 } @llvm.sadd.with.overflow.i32(i32 %safe.addtmp74, i32 %safe.multmp80)
  %addtmp82 = extractvalue { i32, i1 } %sadd81, 0
  %add.overflow83 = extractvalue { i32, i1 } %sadd81, 1
  %safe.addtmp84 = select i1 %add.overflow83, i32 2147483647, i32 %addtmp82
  %x185 = load i32, ptr %x1, align 4
  %x286 = load i32, ptr %x2, align 4
  %smul87 = call { i32, i1 } @llvm.smul.with.overflow.i32(i32 %x185, i32 %x286)
  %multmp88 = extractvalue { i32, i1 } %smul87, 0
  %mul.overflow89 = extractvalue { i32, i1 } %smul87, 1
  %safe.multmp90 = select i1 %mul.overflow89, i32 2147483647, i32 %multmp88
  %sadd91 = call { i32, i1 } @llvm.sadd.with.overflow.i32(i32 %safe.addtmp84, i32 %safe.multmp90)
  %addtmp92 = extractvalue { i32, i1 } %sadd91, 0
  %add.overflow93 = extractvalue { i32, i1 } %sadd91, 1
  %safe.addtmp94 = select i1 %add.overflow93, i32 2147483647, i32 %addtmp92
  %y195 = load i32, ptr %y1, align 4
  %y296 = load i32, ptr %y2, align 4
  %smul97 = call { i32, i1 } @llvm.smul.with.overflow.i32(i32 %y195, i32 %y296)
  %multmp98 = extractvalue { i32, i1 } %smul97, 0
  %mul.overflow99 = extractvalue { i32, i1 } %smul97, 1
  %safe.multmp100 = select i1 %mul.overflow99, i32 2147483647, i32 %multmp98
  %sadd101 = call { i32, i1 } @llvm.sadd.with.overflow.i32(i32 %safe.addtmp94, i32 %safe.multmp100)
  %addtmp102 = extractvalue { i32, i1 } %sadd101, 0
  %add.overflow103 = extractvalue { i32, i1 } %sadd101, 1
  %safe.addtmp104 = select i1 %add.overflow103, i32 2147483647, i32 %addtmp102
  %z1105 = load i32, ptr %z1, align 4
  %z2106 = load i32, ptr %z2, align 4
  %smul107 = call { i32, i1 } @llvm.smul.with.overflow.i32(i32 %z1105, i32 %z2106)
  %multmp108 = extractvalue { i32, i1 } %smul107, 0
  %mul.overflow109 = extractvalue { i32, i1 } %smul107, 1
  %safe.multmp110 = select i1 %mul.overflow109, i32 2147483647, i32 %multmp108
  %sadd111 = call { i32, i1 } @llvm.sadd.with.overflow.i32(i32 %safe.addtmp104, i32 %safe.multmp110)
  %addtmp112 = extractvalue { i32, i1 } %sadd111, 0
  %add.overflow113 = extractvalue { i32, i1 } %sadd111, 1
  %safe.addtmp114 = select i1 %add.overflow113, i32 2147483647, i32 %addtmp112
  %result.val = insertvalue { i32, ptr, i8 } undef, i32 %safe.addtmp114, 0
  %result.err = insertvalue { i32, ptr, i8 } %result.val, ptr null, 1
  %result.is_error = insertvalue { i32, ptr, i8 } %result.err, i8 0, 2
  ret { i32, ptr, i8 } %result.is_error
}

define { i32, ptr, i8 } @wave_distance_squared(%Wave9 %a, %Wave9 %b) {
entry:
  %a_alloca = alloca %Wave9, align 8
  store %Wave9 %a, ptr %a_alloca, align 1
  %b_alloca = alloca %Wave9, align 8
  store %Wave9 %b, ptr %b_alloca, align 1
  %dr = alloca i32, align 4
  %r.ptr = getelementptr inbounds nuw %Wave9, ptr %a_alloca, i32 0, i32 0
  %r = load i8, ptr %r.ptr, align 1
  %r.ptr1 = getelementptr inbounds nuw %Wave9, ptr %b_alloca, i32 0, i32 0
  %r2 = load i8, ptr %r.ptr1, align 1
  %ssub = call { i8, i1 } @llvm.ssub.with.overflow.i8(i8 %r, i8 %r2)
  %subtmp = extractvalue { i8, i1 } %ssub, 0
  %sub.overflow = extractvalue { i8, i1 } %ssub, 1
  %safe.subtmp = select i1 %sub.overflow, i8 127, i8 %subtmp
  %cast.sext = sext i8 %safe.subtmp to i32
  store i32 %cast.sext, ptr %dr, align 4
  %ds = alloca i32, align 4
  %s.ptr = getelementptr inbounds nuw %Wave9, ptr %a_alloca, i32 0, i32 1
  %s = load i8, ptr %s.ptr, align 1
  %s.ptr3 = getelementptr inbounds nuw %Wave9, ptr %b_alloca, i32 0, i32 1
  %s4 = load i8, ptr %s.ptr3, align 1
  %ssub5 = call { i8, i1 } @llvm.ssub.with.overflow.i8(i8 %s, i8 %s4)
  %subtmp6 = extractvalue { i8, i1 } %ssub5, 0
  %sub.overflow7 = extractvalue { i8, i1 } %ssub5, 1
  %safe.subtmp8 = select i1 %sub.overflow7, i8 127, i8 %subtmp6
  %cast.sext9 = sext i8 %safe.subtmp8 to i32
  store i32 %cast.sext9, ptr %ds, align 4
  %dt = alloca i32, align 4
  %t.ptr = getelementptr inbounds nuw %Wave9, ptr %a_alloca, i32 0, i32 2
  %t = load i8, ptr %t.ptr, align 1
  %t.ptr10 = getelementptr inbounds nuw %Wave9, ptr %b_alloca, i32 0, i32 2
  %t11 = load i8, ptr %t.ptr10, align 1
  %ssub12 = call { i8, i1 } @llvm.ssub.with.overflow.i8(i8 %t, i8 %t11)
  %subtmp13 = extractvalue { i8, i1 } %ssub12, 0
  %sub.overflow14 = extractvalue { i8, i1 } %ssub12, 1
  %safe.subtmp15 = select i1 %sub.overflow14, i8 127, i8 %subtmp13
  %cast.sext16 = sext i8 %safe.subtmp15 to i32
  store i32 %cast.sext16, ptr %dt, align 4
  %du = alloca i32, align 4
  %u.ptr = getelementptr inbounds nuw %Wave9, ptr %a_alloca, i32 0, i32 3
  %u = load i8, ptr %u.ptr, align 1
  %u.ptr17 = getelementptr inbounds nuw %Wave9, ptr %b_alloca, i32 0, i32 3
  %u18 = load i8, ptr %u.ptr17, align 1
  %ssub19 = call { i8, i1 } @llvm.ssub.with.overflow.i8(i8 %u, i8 %u18)
  %subtmp20 = extractvalue { i8, i1 } %ssub19, 0
  %sub.overflow21 = extractvalue { i8, i1 } %ssub19, 1
  %safe.subtmp22 = select i1 %sub.overflow21, i8 127, i8 %subtmp20
  %cast.sext23 = sext i8 %safe.subtmp22 to i32
  store i32 %cast.sext23, ptr %du, align 4
  %dv = alloca i32, align 4
  %v.ptr = getelementptr inbounds nuw %Wave9, ptr %a_alloca, i32 0, i32 4
  %v = load i8, ptr %v.ptr, align 1
  %v.ptr24 = getelementptr inbounds nuw %Wave9, ptr %b_alloca, i32 0, i32 4
  %v25 = load i8, ptr %v.ptr24, align 1
  %ssub26 = call { i8, i1 } @llvm.ssub.with.overflow.i8(i8 %v, i8 %v25)
  %subtmp27 = extractvalue { i8, i1 } %ssub26, 0
  %sub.overflow28 = extractvalue { i8, i1 } %ssub26, 1
  %safe.subtmp29 = select i1 %sub.overflow28, i8 127, i8 %subtmp27
  %cast.sext30 = sext i8 %safe.subtmp29 to i32
  store i32 %cast.sext30, ptr %dv, align 4
  %dw = alloca i32, align 4
  %w.ptr = getelementptr inbounds nuw %Wave9, ptr %a_alloca, i32 0, i32 5
  %w = load i8, ptr %w.ptr, align 1
  %w.ptr31 = getelementptr inbounds nuw %Wave9, ptr %b_alloca, i32 0, i32 5
  %w32 = load i8, ptr %w.ptr31, align 1
  %ssub33 = call { i8, i1 } @llvm.ssub.with.overflow.i8(i8 %w, i8 %w32)
  %subtmp34 = extractvalue { i8, i1 } %ssub33, 0
  %sub.overflow35 = extractvalue { i8, i1 } %ssub33, 1
  %safe.subtmp36 = select i1 %sub.overflow35, i8 127, i8 %subtmp34
  %cast.sext37 = sext i8 %safe.subtmp36 to i32
  store i32 %cast.sext37, ptr %dw, align 4
  %dx = alloca i32, align 4
  %x.ptr = getelementptr inbounds nuw %Wave9, ptr %a_alloca, i32 0, i32 6
  %x = load i8, ptr %x.ptr, align 1
  %x.ptr38 = getelementptr inbounds nuw %Wave9, ptr %b_alloca, i32 0, i32 6
  %x39 = load i8, ptr %x.ptr38, align 1
  %ssub40 = call { i8, i1 } @llvm.ssub.with.overflow.i8(i8 %x, i8 %x39)
  %subtmp41 = extractvalue { i8, i1 } %ssub40, 0
  %sub.overflow42 = extractvalue { i8, i1 } %ssub40, 1
  %safe.subtmp43 = select i1 %sub.overflow42, i8 127, i8 %subtmp41
  %cast.sext44 = sext i8 %safe.subtmp43 to i32
  store i32 %cast.sext44, ptr %dx, align 4
  %dy = alloca i32, align 4
  %y.ptr = getelementptr inbounds nuw %Wave9, ptr %a_alloca, i32 0, i32 7
  %y = load i8, ptr %y.ptr, align 1
  %y.ptr45 = getelementptr inbounds nuw %Wave9, ptr %b_alloca, i32 0, i32 7
  %y46 = load i8, ptr %y.ptr45, align 1
  %ssub47 = call { i8, i1 } @llvm.ssub.with.overflow.i8(i8 %y, i8 %y46)
  %subtmp48 = extractvalue { i8, i1 } %ssub47, 0
  %sub.overflow49 = extractvalue { i8, i1 } %ssub47, 1
  %safe.subtmp50 = select i1 %sub.overflow49, i8 127, i8 %subtmp48
  %cast.sext51 = sext i8 %safe.subtmp50 to i32
  store i32 %cast.sext51, ptr %dy, align 4
  %dz = alloca i32, align 4
  %z.ptr = getelementptr inbounds nuw %Wave9, ptr %a_alloca, i32 0, i32 8
  %z = load i8, ptr %z.ptr, align 1
  %z.ptr52 = getelementptr inbounds nuw %Wave9, ptr %b_alloca, i32 0, i32 8
  %z53 = load i8, ptr %z.ptr52, align 1
  %ssub54 = call { i8, i1 } @llvm.ssub.with.overflow.i8(i8 %z, i8 %z53)
  %subtmp55 = extractvalue { i8, i1 } %ssub54, 0
  %sub.overflow56 = extractvalue { i8, i1 } %ssub54, 1
  %safe.subtmp57 = select i1 %sub.overflow56, i8 127, i8 %subtmp55
  %cast.sext58 = sext i8 %safe.subtmp57 to i32
  store i32 %cast.sext58, ptr %dz, align 4
  %dr59 = load i32, ptr %dr, align 4
  %dr60 = load i32, ptr %dr, align 4
  %smul = call { i32, i1 } @llvm.smul.with.overflow.i32(i32 %dr59, i32 %dr60)
  %multmp = extractvalue { i32, i1 } %smul, 0
  %mul.overflow = extractvalue { i32, i1 } %smul, 1
  %safe.multmp = select i1 %mul.overflow, i32 2147483647, i32 %multmp
  %ds61 = load i32, ptr %ds, align 4
  %ds62 = load i32, ptr %ds, align 4
  %smul63 = call { i32, i1 } @llvm.smul.with.overflow.i32(i32 %ds61, i32 %ds62)
  %multmp64 = extractvalue { i32, i1 } %smul63, 0
  %mul.overflow65 = extractvalue { i32, i1 } %smul63, 1
  %safe.multmp66 = select i1 %mul.overflow65, i32 2147483647, i32 %multmp64
  %sadd = call { i32, i1 } @llvm.sadd.with.overflow.i32(i32 %safe.multmp, i32 %safe.multmp66)
  %addtmp = extractvalue { i32, i1 } %sadd, 0
  %add.overflow = extractvalue { i32, i1 } %sadd, 1
  %safe.addtmp = select i1 %add.overflow, i32 2147483647, i32 %addtmp
  %dt67 = load i32, ptr %dt, align 4
  %dt68 = load i32, ptr %dt, align 4
  %smul69 = call { i32, i1 } @llvm.smul.with.overflow.i32(i32 %dt67, i32 %dt68)
  %multmp70 = extractvalue { i32, i1 } %smul69, 0
  %mul.overflow71 = extractvalue { i32, i1 } %smul69, 1
  %safe.multmp72 = select i1 %mul.overflow71, i32 2147483647, i32 %multmp70
  %sadd73 = call { i32, i1 } @llvm.sadd.with.overflow.i32(i32 %safe.addtmp, i32 %safe.multmp72)
  %addtmp74 = extractvalue { i32, i1 } %sadd73, 0
  %add.overflow75 = extractvalue { i32, i1 } %sadd73, 1
  %safe.addtmp76 = select i1 %add.overflow75, i32 2147483647, i32 %addtmp74
  %du77 = load i32, ptr %du, align 4
  %du78 = load i32, ptr %du, align 4
  %smul79 = call { i32, i1 } @llvm.smul.with.overflow.i32(i32 %du77, i32 %du78)
  %multmp80 = extractvalue { i32, i1 } %smul79, 0
  %mul.overflow81 = extractvalue { i32, i1 } %smul79, 1
  %safe.multmp82 = select i1 %mul.overflow81, i32 2147483647, i32 %multmp80
  %sadd83 = call { i32, i1 } @llvm.sadd.with.overflow.i32(i32 %safe.addtmp76, i32 %safe.multmp82)
  %addtmp84 = extractvalue { i32, i1 } %sadd83, 0
  %add.overflow85 = extractvalue { i32, i1 } %sadd83, 1
  %safe.addtmp86 = select i1 %add.overflow85, i32 2147483647, i32 %addtmp84
  %dv87 = load i32, ptr %dv, align 4
  %dv88 = load i32, ptr %dv, align 4
  %smul89 = call { i32, i1 } @llvm.smul.with.overflow.i32(i32 %dv87, i32 %dv88)
  %multmp90 = extractvalue { i32, i1 } %smul89, 0
  %mul.overflow91 = extractvalue { i32, i1 } %smul89, 1
  %safe.multmp92 = select i1 %mul.overflow91, i32 2147483647, i32 %multmp90
  %sadd93 = call { i32, i1 } @llvm.sadd.with.overflow.i32(i32 %safe.addtmp86, i32 %safe.multmp92)
  %addtmp94 = extractvalue { i32, i1 } %sadd93, 0
  %add.overflow95 = extractvalue { i32, i1 } %sadd93, 1
  %safe.addtmp96 = select i1 %add.overflow95, i32 2147483647, i32 %addtmp94
  %dw97 = load i32, ptr %dw, align 4
  %dw98 = load i32, ptr %dw, align 4
  %smul99 = call { i32, i1 } @llvm.smul.with.overflow.i32(i32 %dw97, i32 %dw98)
  %multmp100 = extractvalue { i32, i1 } %smul99, 0
  %mul.overflow101 = extractvalue { i32, i1 } %smul99, 1
  %safe.multmp102 = select i1 %mul.overflow101, i32 2147483647, i32 %multmp100
  %sadd103 = call { i32, i1 } @llvm.sadd.with.overflow.i32(i32 %safe.addtmp96, i32 %safe.multmp102)
  %addtmp104 = extractvalue { i32, i1 } %sadd103, 0
  %add.overflow105 = extractvalue { i32, i1 } %sadd103, 1
  %safe.addtmp106 = select i1 %add.overflow105, i32 2147483647, i32 %addtmp104
  %dx107 = load i32, ptr %dx, align 4
  %dx108 = load i32, ptr %dx, align 4
  %smul109 = call { i32, i1 } @llvm.smul.with.overflow.i32(i32 %dx107, i32 %dx108)
  %multmp110 = extractvalue { i32, i1 } %smul109, 0
  %mul.overflow111 = extractvalue { i32, i1 } %smul109, 1
  %safe.multmp112 = select i1 %mul.overflow111, i32 2147483647, i32 %multmp110
  %sadd113 = call { i32, i1 } @llvm.sadd.with.overflow.i32(i32 %safe.addtmp106, i32 %safe.multmp112)
  %addtmp114 = extractvalue { i32, i1 } %sadd113, 0
  %add.overflow115 = extractvalue { i32, i1 } %sadd113, 1
  %safe.addtmp116 = select i1 %add.overflow115, i32 2147483647, i32 %addtmp114
  %dy117 = load i32, ptr %dy, align 4
  %dy118 = load i32, ptr %dy, align 4
  %smul119 = call { i32, i1 } @llvm.smul.with.overflow.i32(i32 %dy117, i32 %dy118)
  %multmp120 = extractvalue { i32, i1 } %smul119, 0
  %mul.overflow121 = extractvalue { i32, i1 } %smul119, 1
  %safe.multmp122 = select i1 %mul.overflow121, i32 2147483647, i32 %multmp120
  %sadd123 = call { i32, i1 } @llvm.sadd.with.overflow.i32(i32 %safe.addtmp116, i32 %safe.multmp122)
  %addtmp124 = extractvalue { i32, i1 } %sadd123, 0
  %add.overflow125 = extractvalue { i32, i1 } %sadd123, 1
  %safe.addtmp126 = select i1 %add.overflow125, i32 2147483647, i32 %addtmp124
  %dz127 = load i32, ptr %dz, align 4
  %dz128 = load i32, ptr %dz, align 4
  %smul129 = call { i32, i1 } @llvm.smul.with.overflow.i32(i32 %dz127, i32 %dz128)
  %multmp130 = extractvalue { i32, i1 } %smul129, 0
  %mul.overflow131 = extractvalue { i32, i1 } %smul129, 1
  %safe.multmp132 = select i1 %mul.overflow131, i32 2147483647, i32 %multmp130
  %sadd133 = call { i32, i1 } @llvm.sadd.with.overflow.i32(i32 %safe.addtmp126, i32 %safe.multmp132)
  %addtmp134 = extractvalue { i32, i1 } %sadd133, 0
  %add.overflow135 = extractvalue { i32, i1 } %sadd133, 1
  %safe.addtmp136 = select i1 %add.overflow135, i32 2147483647, i32 %addtmp134
  %result.val = insertvalue { i32, ptr, i8 } undef, i32 %safe.addtmp136, 0
  %result.err = insertvalue { i32, ptr, i8 } %result.val, ptr null, 1
  %result.is_error = insertvalue { i32, ptr, i8 } %result.err, i8 0, 2
  ret { i32, ptr, i8 } %result.is_error
}

define { %Wave9, ptr, i8 } @wave_lerp(%Wave9 %a, %Wave9 %b, i32 %t) {
entry:
  %a_alloca = alloca %Wave9, align 8
  store %Wave9 %a, ptr %a_alloca, align 1
  %b_alloca = alloca %Wave9, align 8
  store %Wave9 %b, ptr %b_alloca, align 1
  %lr = alloca i8, align 1
  %a1 = load %Wave9, ptr %a_alloca, align 1
  %r = extractvalue %Wave9 %a1, 0
  %cast.sext = sext i8 %r to i32
  %b2 = load %Wave9, ptr %b_alloca, align 1
  %r3 = extractvalue %Wave9 %b2, 0
  %cast.sext4 = sext i8 %r3 to i32
  %calltmp = call i8 @aria_lerp_component(i32 %cast.sext, i32 %cast.sext4, i32 %t)
  store i8 %calltmp, ptr %lr, align 1
  %ls = alloca i8, align 1
  %a5 = load %Wave9, ptr %a_alloca, align 1
  %s = extractvalue %Wave9 %a5, 1
  %cast.sext6 = sext i8 %s to i32
  %b7 = load %Wave9, ptr %b_alloca, align 1
  %s8 = extractvalue %Wave9 %b7, 1
  %cast.sext9 = sext i8 %s8 to i32
  %calltmp10 = call i8 @aria_lerp_component(i32 %cast.sext6, i32 %cast.sext9, i32 %t)
  store i8 %calltmp10, ptr %ls, align 1
  %lt = alloca i8, align 1
  %a11 = load %Wave9, ptr %a_alloca, align 1
  %t12 = extractvalue %Wave9 %a11, 2
  %cast.sext13 = sext i8 %t12 to i32
  %b14 = load %Wave9, ptr %b_alloca, align 1
  %t15 = extractvalue %Wave9 %b14, 2
  %cast.sext16 = sext i8 %t15 to i32
  %calltmp17 = call i8 @aria_lerp_component(i32 %cast.sext13, i32 %cast.sext16, i32 %t)
  store i8 %calltmp17, ptr %lt, align 1
  %lu = alloca i8, align 1
  %a18 = load %Wave9, ptr %a_alloca, align 1
  %u = extractvalue %Wave9 %a18, 3
  %cast.sext19 = sext i8 %u to i32
  %b20 = load %Wave9, ptr %b_alloca, align 1
  %u21 = extractvalue %Wave9 %b20, 3
  %cast.sext22 = sext i8 %u21 to i32
  %calltmp23 = call i8 @aria_lerp_component(i32 %cast.sext19, i32 %cast.sext22, i32 %t)
  store i8 %calltmp23, ptr %lu, align 1
  %lv = alloca i8, align 1
  %a24 = load %Wave9, ptr %a_alloca, align 1
  %v = extractvalue %Wave9 %a24, 4
  %cast.sext25 = sext i8 %v to i32
  %b26 = load %Wave9, ptr %b_alloca, align 1
  %v27 = extractvalue %Wave9 %b26, 4
  %cast.sext28 = sext i8 %v27 to i32
  %calltmp29 = call i8 @aria_lerp_component(i32 %cast.sext25, i32 %cast.sext28, i32 %t)
  store i8 %calltmp29, ptr %lv, align 1
  %lw = alloca i8, align 1
  %a30 = load %Wave9, ptr %a_alloca, align 1
  %w = extractvalue %Wave9 %a30, 5
  %cast.sext31 = sext i8 %w to i32
  %b32 = load %Wave9, ptr %b_alloca, align 1
  %w33 = extractvalue %Wave9 %b32, 5
  %cast.sext34 = sext i8 %w33 to i32
  %calltmp35 = call i8 @aria_lerp_component(i32 %cast.sext31, i32 %cast.sext34, i32 %t)
  store i8 %calltmp35, ptr %lw, align 1
  %lx = alloca i8, align 1
  %a36 = load %Wave9, ptr %a_alloca, align 1
  %x = extractvalue %Wave9 %a36, 6
  %cast.sext37 = sext i8 %x to i32
  %b38 = load %Wave9, ptr %b_alloca, align 1
  %x39 = extractvalue %Wave9 %b38, 6
  %cast.sext40 = sext i8 %x39 to i32
  %calltmp41 = call i8 @aria_lerp_component(i32 %cast.sext37, i32 %cast.sext40, i32 %t)
  store i8 %calltmp41, ptr %lx, align 1
  %ly = alloca i8, align 1
  %a42 = load %Wave9, ptr %a_alloca, align 1
  %y = extractvalue %Wave9 %a42, 7
  %cast.sext43 = sext i8 %y to i32
  %b44 = load %Wave9, ptr %b_alloca, align 1
  %y45 = extractvalue %Wave9 %b44, 7
  %cast.sext46 = sext i8 %y45 to i32
  %calltmp47 = call i8 @aria_lerp_component(i32 %cast.sext43, i32 %cast.sext46, i32 %t)
  store i8 %calltmp47, ptr %ly, align 1
  %lz = alloca i8, align 1
  %a48 = load %Wave9, ptr %a_alloca, align 1
  %z = extractvalue %Wave9 %a48, 8
  %cast.sext49 = sext i8 %z to i32
  %b50 = load %Wave9, ptr %b_alloca, align 1
  %z51 = extractvalue %Wave9 %b50, 8
  %cast.sext52 = sext i8 %z51 to i32
  %calltmp53 = call i8 @aria_lerp_component(i32 %cast.sext49, i32 %cast.sext52, i32 %t)
  store i8 %calltmp53, ptr %lz, align 1
  %struct.tmp = alloca %Wave9, align 8
  %lr54 = load i8, ptr %lr, align 1
  %r.ptr = getelementptr inbounds nuw %Wave9, ptr %struct.tmp, i32 0, i32 0
  store i8 %lr54, ptr %r.ptr, align 1
  %ls55 = load i8, ptr %ls, align 1
  %s.ptr = getelementptr inbounds nuw %Wave9, ptr %struct.tmp, i32 0, i32 1
  store i8 %ls55, ptr %s.ptr, align 1
  %lt56 = load i8, ptr %lt, align 1
  %t.ptr = getelementptr inbounds nuw %Wave9, ptr %struct.tmp, i32 0, i32 2
  store i8 %lt56, ptr %t.ptr, align 1
  %lu57 = load i8, ptr %lu, align 1
  %u.ptr = getelementptr inbounds nuw %Wave9, ptr %struct.tmp, i32 0, i32 3
  store i8 %lu57, ptr %u.ptr, align 1
  %lv58 = load i8, ptr %lv, align 1
  %v.ptr = getelementptr inbounds nuw %Wave9, ptr %struct.tmp, i32 0, i32 4
  store i8 %lv58, ptr %v.ptr, align 1
  %lw59 = load i8, ptr %lw, align 1
  %w.ptr = getelementptr inbounds nuw %Wave9, ptr %struct.tmp, i32 0, i32 5
  store i8 %lw59, ptr %w.ptr, align 1
  %lx60 = load i8, ptr %lx, align 1
  %x.ptr = getelementptr inbounds nuw %Wave9, ptr %struct.tmp, i32 0, i32 6
  store i8 %lx60, ptr %x.ptr, align 1
  %ly61 = load i8, ptr %ly, align 1
  %y.ptr = getelementptr inbounds nuw %Wave9, ptr %struct.tmp, i32 0, i32 7
  store i8 %ly61, ptr %y.ptr, align 1
  %lz62 = load i8, ptr %lz, align 1
  %z.ptr = getelementptr inbounds nuw %Wave9, ptr %struct.tmp, i32 0, i32 8
  store i8 %lz62, ptr %z.ptr, align 1
  %struct.val = load %Wave9, ptr %struct.tmp, align 1
  %result.val = insertvalue { %Wave9, ptr, i8 } undef, %Wave9 %struct.val, 0
  %result.err = insertvalue { %Wave9, ptr, i8 } %result.val, ptr null, 1
  %result.is_error = insertvalue { %Wave9, ptr, i8 } %result.err, i8 0, 2
  ret { %Wave9, ptr, i8 } %result.is_error
}

define { ptr, ptr, i8 } @nyte_toString(i16 %encoded) {
entry:
  %n0 = alloca i8, align 1
  %calltmp = call i8 @aria_nyte_get_nit(i16 %encoded, i32 0)
  store i8 %calltmp, ptr %n0, align 1
  %n1 = alloca i8, align 1
  %calltmp1 = call i8 @aria_nyte_get_nit(i16 %encoded, i32 1)
  store i8 %calltmp1, ptr %n1, align 1
  %n2 = alloca i8, align 1
  %calltmp2 = call i8 @aria_nyte_get_nit(i16 %encoded, i32 2)
  store i8 %calltmp2, ptr %n2, align 1
  %n3 = alloca i8, align 1
  %calltmp3 = call i8 @aria_nyte_get_nit(i16 %encoded, i32 3)
  store i8 %calltmp3, ptr %n3, align 1
  %n4 = alloca i8, align 1
  %calltmp4 = call i8 @aria_nyte_get_nit(i16 %encoded, i32 4)
  store i8 %calltmp4, ptr %n4, align 1
  %result = alloca ptr, align 8
  store ptr @.str.24, ptr %result, align 8
  %result5 = load ptr, ptr %result, align 8
  %n06 = load i8, ptr %n0, align 1
  %cast.sext = sext i8 %n06 to i32
  %val_i64 = sext i32 %cast.sext to i64
  %from_int_result = call ptr @aria_string_from_int_simple(i64 %val_i64)
  %concat_str = call ptr @aria_string_concat_simple(ptr %result5, ptr %from_int_result)
  store ptr %concat_str, ptr %result, align 8
  %result7 = load ptr, ptr %result, align 8
  %concat_str8 = call ptr @aria_string_concat_simple(ptr %result7, ptr @.str.26)
  store ptr %concat_str8, ptr %result, align 8
  %result9 = load ptr, ptr %result, align 8
  %n110 = load i8, ptr %n1, align 1
  %cast.sext11 = sext i8 %n110 to i32
  %val_i6412 = sext i32 %cast.sext11 to i64
  %from_int_result13 = call ptr @aria_string_from_int_simple(i64 %val_i6412)
  %concat_str14 = call ptr @aria_string_concat_simple(ptr %result9, ptr %from_int_result13)
  store ptr %concat_str14, ptr %result, align 8
  %result15 = load ptr, ptr %result, align 8
  %concat_str16 = call ptr @aria_string_concat_simple(ptr %result15, ptr @.str.28)
  store ptr %concat_str16, ptr %result, align 8
  %result17 = load ptr, ptr %result, align 8
  %n218 = load i8, ptr %n2, align 1
  %cast.sext19 = sext i8 %n218 to i32
  %val_i6420 = sext i32 %cast.sext19 to i64
  %from_int_result21 = call ptr @aria_string_from_int_simple(i64 %val_i6420)
  %concat_str22 = call ptr @aria_string_concat_simple(ptr %result17, ptr %from_int_result21)
  store ptr %concat_str22, ptr %result, align 8
  %result23 = load ptr, ptr %result, align 8
  %concat_str24 = call ptr @aria_string_concat_simple(ptr %result23, ptr @.str.30)
  store ptr %concat_str24, ptr %result, align 8
  %result25 = load ptr, ptr %result, align 8
  %n326 = load i8, ptr %n3, align 1
  %cast.sext27 = sext i8 %n326 to i32
  %val_i6428 = sext i32 %cast.sext27 to i64
  %from_int_result29 = call ptr @aria_string_from_int_simple(i64 %val_i6428)
  %concat_str30 = call ptr @aria_string_concat_simple(ptr %result25, ptr %from_int_result29)
  store ptr %concat_str30, ptr %result, align 8
  %result31 = load ptr, ptr %result, align 8
  %concat_str32 = call ptr @aria_string_concat_simple(ptr %result31, ptr @.str.32)
  store ptr %concat_str32, ptr %result, align 8
  %result33 = load ptr, ptr %result, align 8
  %n434 = load i8, ptr %n4, align 1
  %cast.sext35 = sext i8 %n434 to i32
  %val_i6436 = sext i32 %cast.sext35 to i64
  %from_int_result37 = call ptr @aria_string_from_int_simple(i64 %val_i6436)
  %concat_str38 = call ptr @aria_string_concat_simple(ptr %result33, ptr %from_int_result37)
  store ptr %concat_str38, ptr %result, align 8
  %result39 = load ptr, ptr %result, align 8
  %concat_str40 = call ptr @aria_string_concat_simple(ptr %result39, ptr @.str.34)
  store ptr %concat_str40, ptr %result, align 8
  %result41 = load ptr, ptr %result, align 8
  %result.val = insertvalue { ptr, ptr, i8 } undef, ptr %result41, 0
  %result.err = insertvalue { ptr, ptr, i8 } %result.val, ptr null, 1
  %result.is_error = insertvalue { ptr, ptr, i8 } %result.err, i8 0, 2
  ret { ptr, ptr, i8 } %result.is_error
}

define { ptr, ptr, i8 } @wave9_toString(%Wave9 %w) {
entry:
  %w_alloca = alloca %Wave9, align 8
  store %Wave9 %w, ptr %w_alloca, align 1
  %result = alloca ptr, align 8
  store ptr @.str.36, ptr %result, align 8
  %result1 = load ptr, ptr %result, align 8
  %w2 = load %Wave9, ptr %w_alloca, align 1
  %r = extractvalue %Wave9 %w2, 0
  %cast.sext = sext i8 %r to i32
  %val_i64 = sext i32 %cast.sext to i64
  %from_int_result = call ptr @aria_string_from_int_simple(i64 %val_i64)
  %concat_str = call ptr @aria_string_concat_simple(ptr %result1, ptr %from_int_result)
  store ptr %concat_str, ptr %result, align 8
  %result3 = load ptr, ptr %result, align 8
  %concat_str4 = call ptr @aria_string_concat_simple(ptr %result3, ptr @.str.38)
  store ptr %concat_str4, ptr %result, align 8
  %result5 = load ptr, ptr %result, align 8
  %w6 = load %Wave9, ptr %w_alloca, align 1
  %s = extractvalue %Wave9 %w6, 1
  %cast.sext7 = sext i8 %s to i32
  %val_i648 = sext i32 %cast.sext7 to i64
  %from_int_result9 = call ptr @aria_string_from_int_simple(i64 %val_i648)
  %concat_str10 = call ptr @aria_string_concat_simple(ptr %result5, ptr %from_int_result9)
  store ptr %concat_str10, ptr %result, align 8
  %result11 = load ptr, ptr %result, align 8
  %concat_str12 = call ptr @aria_string_concat_simple(ptr %result11, ptr @.str.40)
  store ptr %concat_str12, ptr %result, align 8
  %result13 = load ptr, ptr %result, align 8
  %w14 = load %Wave9, ptr %w_alloca, align 1
  %t = extractvalue %Wave9 %w14, 2
  %cast.sext15 = sext i8 %t to i32
  %val_i6416 = sext i32 %cast.sext15 to i64
  %from_int_result17 = call ptr @aria_string_from_int_simple(i64 %val_i6416)
  %concat_str18 = call ptr @aria_string_concat_simple(ptr %result13, ptr %from_int_result17)
  store ptr %concat_str18, ptr %result, align 8
  %result19 = load ptr, ptr %result, align 8
  %concat_str20 = call ptr @aria_string_concat_simple(ptr %result19, ptr @.str.42)
  store ptr %concat_str20, ptr %result, align 8
  %result21 = load ptr, ptr %result, align 8
  %w22 = load %Wave9, ptr %w_alloca, align 1
  %u = extractvalue %Wave9 %w22, 3
  %cast.sext23 = sext i8 %u to i32
  %val_i6424 = sext i32 %cast.sext23 to i64
  %from_int_result25 = call ptr @aria_string_from_int_simple(i64 %val_i6424)
  %concat_str26 = call ptr @aria_string_concat_simple(ptr %result21, ptr %from_int_result25)
  store ptr %concat_str26, ptr %result, align 8
  %result27 = load ptr, ptr %result, align 8
  %concat_str28 = call ptr @aria_string_concat_simple(ptr %result27, ptr @.str.44)
  store ptr %concat_str28, ptr %result, align 8
  %result29 = load ptr, ptr %result, align 8
  %w30 = load %Wave9, ptr %w_alloca, align 1
  %v = extractvalue %Wave9 %w30, 4
  %cast.sext31 = sext i8 %v to i32
  %val_i6432 = sext i32 %cast.sext31 to i64
  %from_int_result33 = call ptr @aria_string_from_int_simple(i64 %val_i6432)
  %concat_str34 = call ptr @aria_string_concat_simple(ptr %result29, ptr %from_int_result33)
  store ptr %concat_str34, ptr %result, align 8
  %result35 = load ptr, ptr %result, align 8
  %concat_str36 = call ptr @aria_string_concat_simple(ptr %result35, ptr @.str.46)
  store ptr %concat_str36, ptr %result, align 8
  %result37 = load ptr, ptr %result, align 8
  %w38 = load %Wave9, ptr %w_alloca, align 1
  %w39 = extractvalue %Wave9 %w38, 5
  %cast.sext40 = sext i8 %w39 to i32
  %val_i6441 = sext i32 %cast.sext40 to i64
  %from_int_result42 = call ptr @aria_string_from_int_simple(i64 %val_i6441)
  %concat_str43 = call ptr @aria_string_concat_simple(ptr %result37, ptr %from_int_result42)
  store ptr %concat_str43, ptr %result, align 8
  %result44 = load ptr, ptr %result, align 8
  %concat_str45 = call ptr @aria_string_concat_simple(ptr %result44, ptr @.str.48)
  store ptr %concat_str45, ptr %result, align 8
  %result46 = load ptr, ptr %result, align 8
  %w47 = load %Wave9, ptr %w_alloca, align 1
  %x = extractvalue %Wave9 %w47, 6
  %cast.sext48 = sext i8 %x to i32
  %val_i6449 = sext i32 %cast.sext48 to i64
  %from_int_result50 = call ptr @aria_string_from_int_simple(i64 %val_i6449)
  %concat_str51 = call ptr @aria_string_concat_simple(ptr %result46, ptr %from_int_result50)
  store ptr %concat_str51, ptr %result, align 8
  %result52 = load ptr, ptr %result, align 8
  %concat_str53 = call ptr @aria_string_concat_simple(ptr %result52, ptr @.str.50)
  store ptr %concat_str53, ptr %result, align 8
  %result54 = load ptr, ptr %result, align 8
  %w55 = load %Wave9, ptr %w_alloca, align 1
  %y = extractvalue %Wave9 %w55, 7
  %cast.sext56 = sext i8 %y to i32
  %val_i6457 = sext i32 %cast.sext56 to i64
  %from_int_result58 = call ptr @aria_string_from_int_simple(i64 %val_i6457)
  %concat_str59 = call ptr @aria_string_concat_simple(ptr %result54, ptr %from_int_result58)
  store ptr %concat_str59, ptr %result, align 8
  %result60 = load ptr, ptr %result, align 8
  %concat_str61 = call ptr @aria_string_concat_simple(ptr %result60, ptr @.str.52)
  store ptr %concat_str61, ptr %result, align 8
  %result62 = load ptr, ptr %result, align 8
  %w63 = load %Wave9, ptr %w_alloca, align 1
  %z = extractvalue %Wave9 %w63, 8
  %cast.sext64 = sext i8 %z to i32
  %val_i6465 = sext i32 %cast.sext64 to i64
  %from_int_result66 = call ptr @aria_string_from_int_simple(i64 %val_i6465)
  %concat_str67 = call ptr @aria_string_concat_simple(ptr %result62, ptr %from_int_result66)
  store ptr %concat_str67, ptr %result, align 8
  %result68 = load ptr, ptr %result, align 8
  %concat_str69 = call ptr @aria_string_concat_simple(ptr %result68, ptr @.str.54)
  store ptr %concat_str69, ptr %result, align 8
  %result70 = load ptr, ptr %result, align 8
  %result.val = insertvalue { ptr, ptr, i8 } undef, ptr %result70, 0
  %result.err = insertvalue { ptr, ptr, i8 } %result.val, ptr null, 1
  %result.is_error = insertvalue { ptr, ptr, i8 } %result.err, i8 0, 2
  ret { ptr, ptr, i8 } %result.is_error
}

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare { i8, i1 } @llvm.sadd.with.overflow.i8(i8, i8) #0

declare i16 @aria_nit_mul(i16, i16)

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare { i32, i1 } @llvm.smul.with.overflow.i32(i32, i32) #0

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare { i32, i1 } @llvm.sadd.with.overflow.i32(i32, i32) #0

; Function Attrs: nocallback nofree nosync nounwind speculatable willreturn memory(none)
declare { i8, i1 } @llvm.ssub.with.overflow.i8(i8, i8) #0

define i32 @__wave_init() {
entry:
  ret i32 0
}

define { %struct.NIL, ptr, i8 } @printBool(i1 %value) {
entry:
  %calltmp = call { ptr, ptr, i8 } @bool_toString(i1 %value)
  %result_val = extractvalue { ptr, ptr, i8 } %calltmp, 0
  %str_struct = load %struct.AriaString, ptr %result_val, align 8
  %str_data = extractvalue %struct.AriaString %str_struct, 0
  %print_call = call i64 @aria_print_cstr(ptr %str_data)
  ret { %struct.NIL, ptr, i8 } zeroinitializer
}

define { %struct.NIL, ptr, i8 } @printlnBool(i1 %value) {
entry:
  %calltmp = call { ptr, ptr, i8 } @bool_toString(i1 %value)
  %result_val = extractvalue { ptr, ptr, i8 } %calltmp, 0
  %str_struct = load %struct.AriaString, ptr %result_val, align 8
  %str_data = extractvalue %struct.AriaString %str_struct, 0
  %print_call = call i64 @aria_println_cstr(ptr %str_data)
  ret { %struct.NIL, ptr, i8 } zeroinitializer
}

define { %struct.NIL, ptr, i8 } @printBoolLabeled(ptr %label, i1 %value) {
entry:
  %str_struct = load %struct.AriaString, ptr %label, align 8
  %str_data = extractvalue %struct.AriaString %str_struct, 0
  %print_call = call i64 @aria_print_cstr(ptr %str_data)
  %str_data1 = load ptr, ptr @.str.56, align 8
  %print_call2 = call i64 @aria_print_cstr(ptr %str_data1)
  %calltmp = call { ptr, ptr, i8 } @bool_toString(i1 %value)
  %result_val = extractvalue { ptr, ptr, i8 } %calltmp, 0
  %str_struct3 = load %struct.AriaString, ptr %result_val, align 8
  %str_data4 = extractvalue %struct.AriaString %str_struct3, 0
  %print_call5 = call i64 @aria_println_cstr(ptr %str_data4)
  ret { %struct.NIL, ptr, i8 } zeroinitializer
}

define { %struct.NIL, ptr, i8 } @printNit(i8 %value) {
entry:
  %calltmp = call { ptr, ptr, i8 } @nit_toString(i8 %value)
  %result_val = extractvalue { ptr, ptr, i8 } %calltmp, 0
  %str_struct = load %struct.AriaString, ptr %result_val, align 8
  %str_data = extractvalue %struct.AriaString %str_struct, 0
  %print_call = call i64 @aria_print_cstr(ptr %str_data)
  ret { %struct.NIL, ptr, i8 } zeroinitializer
}

define { %struct.NIL, ptr, i8 } @printlnNit(i8 %value) {
entry:
  %calltmp = call { ptr, ptr, i8 } @nit_toString(i8 %value)
  %result_val = extractvalue { ptr, ptr, i8 } %calltmp, 0
  %str_struct = load %struct.AriaString, ptr %result_val, align 8
  %str_data = extractvalue %struct.AriaString %str_struct, 0
  %print_call = call i64 @aria_println_cstr(ptr %str_data)
  ret { %struct.NIL, ptr, i8 } zeroinitializer
}

define { %struct.NIL, ptr, i8 } @printNitLabeled(ptr %label, i8 %value) {
entry:
  %str_struct = load %struct.AriaString, ptr %label, align 8
  %str_data = extractvalue %struct.AriaString %str_struct, 0
  %print_call = call i64 @aria_print_cstr(ptr %str_data)
  %str_data1 = load ptr, ptr @.str.58, align 8
  %print_call2 = call i64 @aria_print_cstr(ptr %str_data1)
  %calltmp = call { ptr, ptr, i8 } @nit_toString(i8 %value)
  %result_val = extractvalue { ptr, ptr, i8 } %calltmp, 0
  %str_struct3 = load %struct.AriaString, ptr %result_val, align 8
  %str_data4 = extractvalue %struct.AriaString %str_struct3, 0
  %print_call5 = call i64 @aria_println_cstr(ptr %str_data4)
  ret { %struct.NIL, ptr, i8 } zeroinitializer
}

define { %struct.NIL, ptr, i8 } @printInt32(i32 %value) {
entry:
  %calltmp = call { ptr, ptr, i8 } @int32_toString(i32 %value)
  %result_val = extractvalue { ptr, ptr, i8 } %calltmp, 0
  %str_struct = load %struct.AriaString, ptr %result_val, align 8
  %str_data = extractvalue %struct.AriaString %str_struct, 0
  %print_call = call i64 @aria_print_cstr(ptr %str_data)
  ret { %struct.NIL, ptr, i8 } zeroinitializer
}

define { %struct.NIL, ptr, i8 } @printlnInt32(i32 %value) {
entry:
  %calltmp = call { ptr, ptr, i8 } @int32_toString(i32 %value)
  %result_val = extractvalue { ptr, ptr, i8 } %calltmp, 0
  %str_struct = load %struct.AriaString, ptr %result_val, align 8
  %str_data = extractvalue %struct.AriaString %str_struct, 0
  %print_call = call i64 @aria_println_cstr(ptr %str_data)
  ret { %struct.NIL, ptr, i8 } zeroinitializer
}

define { %struct.NIL, ptr, i8 } @printInt32Labeled(ptr %label, i32 %value) {
entry:
  %str_struct = load %struct.AriaString, ptr %label, align 8
  %str_data = extractvalue %struct.AriaString %str_struct, 0
  %print_call = call i64 @aria_print_cstr(ptr %str_data)
  %str_data1 = load ptr, ptr @.str.60, align 8
  %print_call2 = call i64 @aria_print_cstr(ptr %str_data1)
  %calltmp = call { ptr, ptr, i8 } @int32_toString(i32 %value)
  %result_val = extractvalue { ptr, ptr, i8 } %calltmp, 0
  %str_struct3 = load %struct.AriaString, ptr %result_val, align 8
  %str_data4 = extractvalue %struct.AriaString %str_struct3, 0
  %print_call5 = call i64 @aria_println_cstr(ptr %str_data4)
  ret { %struct.NIL, ptr, i8 } zeroinitializer
}

define { %struct.NIL, ptr, i8 } @printInt64(i64 %value) {
entry:
  %calltmp = call { ptr, ptr, i8 } @int64_toString(i64 %value)
  %result_val = extractvalue { ptr, ptr, i8 } %calltmp, 0
  %str_struct = load %struct.AriaString, ptr %result_val, align 8
  %str_data = extractvalue %struct.AriaString %str_struct, 0
  %print_call = call i64 @aria_print_cstr(ptr %str_data)
  ret { %struct.NIL, ptr, i8 } zeroinitializer
}

define { %struct.NIL, ptr, i8 } @printlnInt64(i64 %value) {
entry:
  %calltmp = call { ptr, ptr, i8 } @int64_toString(i64 %value)
  %result_val = extractvalue { ptr, ptr, i8 } %calltmp, 0
  %str_struct = load %struct.AriaString, ptr %result_val, align 8
  %str_data = extractvalue %struct.AriaString %str_struct, 0
  %print_call = call i64 @aria_println_cstr(ptr %str_data)
  ret { %struct.NIL, ptr, i8 } zeroinitializer
}

define { %struct.NIL, ptr, i8 } @printInt64Labeled(ptr %label, i64 %value) {
entry:
  %str_struct = load %struct.AriaString, ptr %label, align 8
  %str_data = extractvalue %struct.AriaString %str_struct, 0
  %print_call = call i64 @aria_print_cstr(ptr %str_data)
  %str_data1 = load ptr, ptr @.str.62, align 8
  %print_call2 = call i64 @aria_print_cstr(ptr %str_data1)
  %calltmp = call { ptr, ptr, i8 } @int64_toString(i64 %value)
  %result_val = extractvalue { ptr, ptr, i8 } %calltmp, 0
  %str_struct3 = load %struct.AriaString, ptr %result_val, align 8
  %str_data4 = extractvalue %struct.AriaString %str_struct3, 0
  %print_call5 = call i64 @aria_println_cstr(ptr %str_data4)
  ret { %struct.NIL, ptr, i8 } zeroinitializer
}

define { %struct.NIL, ptr, i8 } @printFloat(i32 %value) {
entry:
  %calltmp = call { ptr, ptr, i8 } @float_toStringDefault(i32 %value)
  %result_val = extractvalue { ptr, ptr, i8 } %calltmp, 0
  %str_struct = load %struct.AriaString, ptr %result_val, align 8
  %str_data = extractvalue %struct.AriaString %str_struct, 0
  %print_call = call i64 @aria_print_cstr(ptr %str_data)
  ret { %struct.NIL, ptr, i8 } zeroinitializer
}

define { %struct.NIL, ptr, i8 } @printlnFloat(i32 %value) {
entry:
  %calltmp = call { ptr, ptr, i8 } @float_toStringDefault(i32 %value)
  %result_val = extractvalue { ptr, ptr, i8 } %calltmp, 0
  %str_struct = load %struct.AriaString, ptr %result_val, align 8
  %str_data = extractvalue %struct.AriaString %str_struct, 0
  %print_call = call i64 @aria_println_cstr(ptr %str_data)
  ret { %struct.NIL, ptr, i8 } zeroinitializer
}

define { %struct.NIL, ptr, i8 } @printFloatLabeled(ptr %label, i32 %value) {
entry:
  %str_struct = load %struct.AriaString, ptr %label, align 8
  %str_data = extractvalue %struct.AriaString %str_struct, 0
  %print_call = call i64 @aria_print_cstr(ptr %str_data)
  %str_data1 = load ptr, ptr @.str.64, align 8
  %print_call2 = call i64 @aria_print_cstr(ptr %str_data1)
  %calltmp = call { ptr, ptr, i8 } @float_toStringDefault(i32 %value)
  %result_val = extractvalue { ptr, ptr, i8 } %calltmp, 0
  %str_struct3 = load %struct.AriaString, ptr %result_val, align 8
  %str_data4 = extractvalue %struct.AriaString %str_struct3, 0
  %print_call5 = call i64 @aria_println_cstr(ptr %str_data4)
  ret { %struct.NIL, ptr, i8 } zeroinitializer
}

define { %struct.NIL, ptr, i8 } @printNyte(i16 %encoded) {
entry:
  %calltmp = call { ptr, ptr, i8 } @nyte_toString(i16 %encoded)
  %result_val = extractvalue { ptr, ptr, i8 } %calltmp, 0
  %str_struct = load %struct.AriaString, ptr %result_val, align 8
  %str_data = extractvalue %struct.AriaString %str_struct, 0
  %print_call = call i64 @aria_print_cstr(ptr %str_data)
  ret { %struct.NIL, ptr, i8 } zeroinitializer
}

define { %struct.NIL, ptr, i8 } @printlnNyte(i16 %encoded) {
entry:
  %calltmp = call { ptr, ptr, i8 } @nyte_toString(i16 %encoded)
  %result_val = extractvalue { ptr, ptr, i8 } %calltmp, 0
  %str_struct = load %struct.AriaString, ptr %result_val, align 8
  %str_data = extractvalue %struct.AriaString %str_struct, 0
  %print_call = call i64 @aria_println_cstr(ptr %str_data)
  ret { %struct.NIL, ptr, i8 } zeroinitializer
}

define { %struct.NIL, ptr, i8 } @printNyteLabeled(ptr %label, i16 %encoded) {
entry:
  %str_struct = load %struct.AriaString, ptr %label, align 8
  %str_data = extractvalue %struct.AriaString %str_struct, 0
  %print_call = call i64 @aria_print_cstr(ptr %str_data)
  %str_data1 = load ptr, ptr @.str.66, align 8
  %print_call2 = call i64 @aria_print_cstr(ptr %str_data1)
  %calltmp = call { ptr, ptr, i8 } @nyte_toString(i16 %encoded)
  %result_val = extractvalue { ptr, ptr, i8 } %calltmp, 0
  %str_struct3 = load %struct.AriaString, ptr %result_val, align 8
  %str_data4 = extractvalue %struct.AriaString %str_struct3, 0
  %print_call5 = call i64 @aria_println_cstr(ptr %str_data4)
  ret { %struct.NIL, ptr, i8 } zeroinitializer
}

define { %struct.NIL, ptr, i8 } @printWave9(%Wave9 %w) {
entry:
  %w_alloca = alloca %Wave9, align 8
  store %Wave9 %w, ptr %w_alloca, align 1
  %w1 = load %Wave9, ptr %w_alloca, align 1
  %calltmp = call { ptr, ptr, i8 } @wave9_toString(%Wave9 %w1)
  %result_val = extractvalue { ptr, ptr, i8 } %calltmp, 0
  %str_struct = load %struct.AriaString, ptr %result_val, align 8
  %str_data = extractvalue %struct.AriaString %str_struct, 0
  %print_call = call i64 @aria_print_cstr(ptr %str_data)
  ret { %struct.NIL, ptr, i8 } zeroinitializer
}

define { %struct.NIL, ptr, i8 } @printlnWave9(%Wave9 %w) {
entry:
  %w_alloca = alloca %Wave9, align 8
  store %Wave9 %w, ptr %w_alloca, align 1
  %w1 = load %Wave9, ptr %w_alloca, align 1
  %calltmp = call { ptr, ptr, i8 } @wave9_toString(%Wave9 %w1)
  %result_val = extractvalue { ptr, ptr, i8 } %calltmp, 0
  %str_struct = load %struct.AriaString, ptr %result_val, align 8
  %str_data = extractvalue %struct.AriaString %str_struct, 0
  %print_call = call i64 @aria_println_cstr(ptr %str_data)
  ret { %struct.NIL, ptr, i8 } zeroinitializer
}

define { %struct.NIL, ptr, i8 } @printWave9Labeled(ptr %label, %Wave9 %w) {
entry:
  %w_alloca = alloca %Wave9, align 8
  store %Wave9 %w, ptr %w_alloca, align 1
  %str_struct = load %struct.AriaString, ptr %label, align 8
  %str_data = extractvalue %struct.AriaString %str_struct, 0
  %print_call = call i64 @aria_print_cstr(ptr %str_data)
  %str_data1 = load ptr, ptr @.str.68, align 8
  %print_call2 = call i64 @aria_print_cstr(ptr %str_data1)
  %w3 = load %Wave9, ptr %w_alloca, align 1
  %calltmp = call { ptr, ptr, i8 } @wave9_toString(%Wave9 %w3)
  %result_val = extractvalue { ptr, ptr, i8 } %calltmp, 0
  %str_struct4 = load %struct.AriaString, ptr %result_val, align 8
  %str_data5 = extractvalue %struct.AriaString %str_struct4, 0
  %print_call6 = call i64 @aria_println_cstr(ptr %str_data5)
  ret { %struct.NIL, ptr, i8 } zeroinitializer
}

define { %struct.NIL, ptr, i8 } @printLabeled(ptr %label, ptr %value) {
entry:
  %str_struct = load %struct.AriaString, ptr %label, align 8
  %str_data = extractvalue %struct.AriaString %str_struct, 0
  %print_call = call i64 @aria_print_cstr(ptr %str_data)
  %str_data1 = load ptr, ptr @.str.70, align 8
  %print_call2 = call i64 @aria_print_cstr(ptr %str_data1)
  %str_struct3 = load %struct.AriaString, ptr %value, align 8
  %str_data4 = extractvalue %struct.AriaString %str_struct3, 0
  %print_call5 = call i64 @aria_println_cstr(ptr %str_data4)
  ret { %struct.NIL, ptr, i8 } zeroinitializer
}

define { %struct.NIL, ptr, i8 } @printString(ptr %value) {
entry:
  %str_struct = load %struct.AriaString, ptr %value, align 8
  %str_data = extractvalue %struct.AriaString %str_struct, 0
  %print_call = call i64 @aria_print_cstr(ptr %str_data)
  ret { %struct.NIL, ptr, i8 } zeroinitializer
}

define { %struct.NIL, ptr, i8 } @printlnString(ptr %value) {
entry:
  %str_struct = load %struct.AriaString, ptr %value, align 8
  %str_data = extractvalue %struct.AriaString %str_struct, 0
  %print_call = call i64 @aria_println_cstr(ptr %str_data)
  ret { %struct.NIL, ptr, i8 } zeroinitializer
}

define { %struct.NIL, ptr, i8 } @printStringLabeled(ptr %label, ptr %value) {
entry:
  %str_struct = load %struct.AriaString, ptr %label, align 8
  %str_data = extractvalue %struct.AriaString %str_struct, 0
  %print_call = call i64 @aria_print_cstr(ptr %str_data)
  %str_data1 = load ptr, ptr @.str.72, align 8
  %print_call2 = call i64 @aria_print_cstr(ptr %str_data1)
  %str_struct3 = load %struct.AriaString, ptr %value, align 8
  %str_data4 = extractvalue %struct.AriaString %str_struct3, 0
  %print_call5 = call i64 @aria_println_cstr(ptr %str_data4)
  ret { %struct.NIL, ptr, i8 } zeroinitializer
}

define { %struct.NIL, ptr, i8 } @printNewline() {
entry:
  %str_data = load ptr, ptr @.str.74, align 8
  %print_call = call i64 @aria_println_cstr(ptr %str_data)
  ret { %struct.NIL, ptr, i8 } zeroinitializer
}

define { %struct.NIL, ptr, i8 } @printSeparator(i32 %width) {
entry:
  %w = alloca i64, align 8
  %cast.sext = sext i32 %width to i64
  store i64 %cast.sext, ptr %w, align 4
  %w1 = load i64, ptr %w, align 4
  %repeat_result = call ptr @aria_string_repeat_simple(ptr @.str.76, i64 %w1)
  %str_struct = load %struct.AriaString, ptr %repeat_result, align 8
  %str_data = extractvalue %struct.AriaString %str_struct, 0
  %print_call = call i64 @aria_println_cstr(ptr %str_data)
  ret { %struct.NIL, ptr, i8 } zeroinitializer
}

define { %struct.NIL, ptr, i8 } @printSeparatorChar(i32 %width, ptr %ch) {
entry:
  %w = alloca i64, align 8
  %cast.sext = sext i32 %width to i64
  store i64 %cast.sext, ptr %w, align 4
  %w1 = load i64, ptr %w, align 4
  %repeat_result = call ptr @aria_string_repeat_simple(ptr %ch, i64 %w1)
  %str_struct = load %struct.AriaString, ptr %repeat_result, align 8
  %str_data = extractvalue %struct.AriaString %str_struct, 0
  %print_call = call i64 @aria_println_cstr(ptr %str_data)
  ret { %struct.NIL, ptr, i8 } zeroinitializer
}

define { %struct.NIL, ptr, i8 } @printlnFloatPrecision(i32 %value, i32 %precision) {
entry:
  %calltmp = call { ptr, ptr, i8 } @float_toString(i32 %value, i32 %precision)
  %result_val = extractvalue { ptr, ptr, i8 } %calltmp, 0
  %str_struct = load %struct.AriaString, ptr %result_val, align 8
  %str_data = extractvalue %struct.AriaString %str_struct, 0
  %print_call = call i64 @aria_println_cstr(ptr %str_data)
  ret { %struct.NIL, ptr, i8 } zeroinitializer
}

define { %struct.NIL, ptr, i8 } @printFloatPrecisionLabeled(ptr %label, i32 %value, i32 %precision) {
entry:
  %str_struct = load %struct.AriaString, ptr %label, align 8
  %str_data = extractvalue %struct.AriaString %str_struct, 0
  %print_call = call i64 @aria_print_cstr(ptr %str_data)
  %str_data1 = load ptr, ptr @.str.78, align 8
  %print_call2 = call i64 @aria_print_cstr(ptr %str_data1)
  %calltmp = call { ptr, ptr, i8 } @float_toString(i32 %value, i32 %precision)
  %result_val = extractvalue { ptr, ptr, i8 } %calltmp, 0
  %str_struct3 = load %struct.AriaString, ptr %result_val, align 8
  %str_data4 = extractvalue %struct.AriaString %str_struct3, 0
  %print_call5 = call i64 @aria_println_cstr(ptr %str_data4)
  ret { %struct.NIL, ptr, i8 } zeroinitializer
}

define { %struct.NIL, ptr, i8 } @printInt32Hex(i32 %value) {
entry:
  %calltmp = call { ptr, ptr, i8 } @int32_toHex(i32 %value)
  %result_val = extractvalue { ptr, ptr, i8 } %calltmp, 0
  %str_struct = load %struct.AriaString, ptr %result_val, align 8
  %str_data = extractvalue %struct.AriaString %str_struct, 0
  %print_call = call i64 @aria_print_cstr(ptr %str_data)
  ret { %struct.NIL, ptr, i8 } zeroinitializer
}

define { %struct.NIL, ptr, i8 } @printlnInt32Hex(i32 %value) {
entry:
  %calltmp = call { ptr, ptr, i8 } @int32_toHex(i32 %value)
  %result_val = extractvalue { ptr, ptr, i8 } %calltmp, 0
  %str_struct = load %struct.AriaString, ptr %result_val, align 8
  %str_data = extractvalue %struct.AriaString %str_struct, 0
  %print_call = call i64 @aria_println_cstr(ptr %str_data)
  ret { %struct.NIL, ptr, i8 } zeroinitializer
}

define { %struct.NIL, ptr, i8 } @printInt32HexLabeled(ptr %label, i32 %value) {
entry:
  %str_struct = load %struct.AriaString, ptr %label, align 8
  %str_data = extractvalue %struct.AriaString %str_struct, 0
  %print_call = call i64 @aria_print_cstr(ptr %str_data)
  %str_data1 = load ptr, ptr @.str.80, align 8
  %print_call2 = call i64 @aria_print_cstr(ptr %str_data1)
  %calltmp = call { ptr, ptr, i8 } @int32_toHex(i32 %value)
  %result_val = extractvalue { ptr, ptr, i8 } %calltmp, 0
  %str_struct3 = load %struct.AriaString, ptr %result_val, align 8
  %str_data4 = extractvalue %struct.AriaString %str_struct3, 0
  %print_call5 = call i64 @aria_println_cstr(ptr %str_data4)
  ret { %struct.NIL, ptr, i8 } zeroinitializer
}

define { %struct.NIL, ptr, i8 } @printInt64Hex(i64 %value) {
entry:
  %calltmp = call { ptr, ptr, i8 } @int64_toHex(i64 %value)
  %result_val = extractvalue { ptr, ptr, i8 } %calltmp, 0
  %str_struct = load %struct.AriaString, ptr %result_val, align 8
  %str_data = extractvalue %struct.AriaString %str_struct, 0
  %print_call = call i64 @aria_print_cstr(ptr %str_data)
  ret { %struct.NIL, ptr, i8 } zeroinitializer
}

define { %struct.NIL, ptr, i8 } @printlnInt64Hex(i64 %value) {
entry:
  %calltmp = call { ptr, ptr, i8 } @int64_toHex(i64 %value)
  %result_val = extractvalue { ptr, ptr, i8 } %calltmp, 0
  %str_struct = load %struct.AriaString, ptr %result_val, align 8
  %str_data = extractvalue %struct.AriaString %str_struct, 0
  %print_call = call i64 @aria_println_cstr(ptr %str_data)
  ret { %struct.NIL, ptr, i8 } zeroinitializer
}

define { %struct.NIL, ptr, i8 } @printInt64HexLabeled(ptr %label, i64 %value) {
entry:
  %str_struct = load %struct.AriaString, ptr %label, align 8
  %str_data = extractvalue %struct.AriaString %str_struct, 0
  %print_call = call i64 @aria_print_cstr(ptr %str_data)
  %str_data1 = load ptr, ptr @.str.82, align 8
  %print_call2 = call i64 @aria_print_cstr(ptr %str_data1)
  %calltmp = call { ptr, ptr, i8 } @int64_toHex(i64 %value)
  %result_val = extractvalue { ptr, ptr, i8 } %calltmp, 0
  %str_struct3 = load %struct.AriaString, ptr %result_val, align 8
  %str_data4 = extractvalue %struct.AriaString %str_struct3, 0
  %print_call5 = call i64 @aria_println_cstr(ptr %str_data4)
  ret { %struct.NIL, ptr, i8 } zeroinitializer
}

declare i64 @aria_print_cstr(ptr)

declare i64 @aria_println_cstr(ptr)

define i32 @__print_utils_init() {
entry:
  ret i32 0
}

define { i32, ptr, i8 } @RELAXED() {
entry:
  ret { i32, ptr, i8 } zeroinitializer
}

define { i32, ptr, i8 } @ACQUIRE() {
entry:
  ret { i32, ptr, i8 } { i32 1, ptr null, i8 0 }
}

define { i32, ptr, i8 } @RELEASE() {
entry:
  ret { i32, ptr, i8 } { i32 2, ptr null, i8 0 }
}

define { i32, ptr, i8 } @ACQ_REL() {
entry:
  ret { i32, ptr, i8 } { i32 3, ptr null, i8 0 }
}

define { i32, ptr, i8 } @SEQ_CST() {
entry:
  ret { i32, ptr, i8 } { i32 4, ptr null, i8 0 }
}

declare ptr @aria_atomic_int32_create(i32)

declare void @aria_atomic_int32_destroy(ptr)

declare i32 @aria_atomic_int32_load(ptr, i32)

declare void @aria_atomic_int32_store(ptr, i32, i32)

declare i1 @aria_atomic_int32_compare_exchange_strong(ptr, ptr, i32, i32, i32)

declare i32 @aria_atomic_int32_fetch_add(ptr, i32, i32)

declare i64 @aria_wild_to_int64(ptr)

declare ptr @aria_int64_to_wild(i64)

declare void @aria_wild_ptr_free(ptr)

declare void @aria_thread_sleep_ns(i64)

declare i32 @pthread_create(ptr, ptr, ptr, ptr)

declare i32 @pthread_join(i64, ptr)

define { i32, ptr, i8 } @N_PHIL() {
entry:
  ret { i32, ptr, i8 } { i32 5, ptr null, i8 0 }
}

define { i64, ptr, i8 } @RUN_NS() {
entry:
  ret { i64, ptr, i8 } { i64 3000000000, ptr null, i8 0 }
}

define { i64, ptr, i8 } @THINK_NS() {
entry:
  ret { i64, ptr, i8 } { i64 5000000, ptr null, i8 0 }
}

define { i64, ptr, i8 } @EAT_NS() {
entry:
  ret { i64, ptr, i8 } { i64 10000000, ptr null, i8 0 }
}

define { i64, ptr, i8 } @RETRY_NS() {
entry:
  ret { i64, ptr, i8 } { i64 500000, ptr null, i8 0 }
}

define { i32, ptr, i8 } @load_running() {
entry:
  %r = alloca ptr, align 8
  %g_running = load i64, ptr @g_running, align 4
  %calltmp = call ptr @aria_int64_to_wild(i64 %g_running)
  %ffi_is_null = icmp eq ptr %calltmp, null
  %ffi_has_value = xor i1 %ffi_is_null, true
  %optional_hasval = insertvalue { i1, ptr } undef, i1 %ffi_has_value, 0
  %optional_ptr = insertvalue { i1, ptr } %optional_hasval, ptr %calltmp, 1
  %unwrap_optional_ptr = extractvalue { i1, ptr } %optional_ptr, 1
  store ptr %unwrap_optional_ptr, ptr %r, align 8
  %val = alloca i32, align 4
  %r1 = load ptr, ptr %r, align 8
  %calltmp2 = call { i32, ptr, i8 } @RELAXED()
  %arg_unwrap = extractvalue { i32, ptr, i8 } %calltmp2, 0
  %calltmp3 = call i32 @aria_atomic_int32_load(ptr %r1, i32 %arg_unwrap)
  store i32 %calltmp3, ptr %val, align 4
  %r4 = load ptr, ptr %r, align 8
  %calltmp5 = call void @aria_wild_ptr_free(ptr %r4)
  %val6 = load i32, ptr %val, align 4
  %result.val = insertvalue { i32, ptr, i8 } undef, i32 %val6, 0
  %result.err = insertvalue { i32, ptr, i8 } %result.val, ptr null, 1
  %result.is_error = insertvalue { i32, ptr, i8 } %result.err, i8 0, 2
  ret { i32, ptr, i8 } %result.is_error
}

define { i1, ptr, i8 } @try_acquire_fork(i32 %fork_id) {
entry:
  %addr = alloca i64, align 8
  %g_fork_0 = load i64, ptr @g_fork_0, align 4
  store i64 %g_fork_0, ptr %addr, align 4
  %cmp_cast = sext i32 %fork_id to i64
  %eqtmp = icmp eq i64 %cmp_cast, 1
  %ifcond = icmp ne i1 %eqtmp, false
  br i1 %ifcond, label %then, label %ifcont

then:                                             ; preds = %entry
  %g_fork_1 = load i64, ptr @g_fork_1, align 4
  store i64 %g_fork_1, ptr %addr, align 4
  br label %ifcont

ifcont:                                           ; preds = %then, %entry
  %cmp_cast1 = sext i32 %fork_id to i64
  %eqtmp2 = icmp eq i64 %cmp_cast1, 2
  %ifcond3 = icmp ne i1 %eqtmp2, false
  br i1 %ifcond3, label %then4, label %ifcont5

then4:                                            ; preds = %ifcont
  %g_fork_2 = load i64, ptr @g_fork_2, align 4
  store i64 %g_fork_2, ptr %addr, align 4
  br label %ifcont5

ifcont5:                                          ; preds = %then4, %ifcont
  %cmp_cast6 = sext i32 %fork_id to i64
  %eqtmp7 = icmp eq i64 %cmp_cast6, 3
  %ifcond8 = icmp ne i1 %eqtmp7, false
  br i1 %ifcond8, label %then9, label %ifcont10

then9:                                            ; preds = %ifcont5
  %g_fork_3 = load i64, ptr @g_fork_3, align 4
  store i64 %g_fork_3, ptr %addr, align 4
  br label %ifcont10

ifcont10:                                         ; preds = %then9, %ifcont5
  %cmp_cast11 = sext i32 %fork_id to i64
  %eqtmp12 = icmp eq i64 %cmp_cast11, 4
  %ifcond13 = icmp ne i1 %eqtmp12, false
  br i1 %ifcond13, label %then14, label %ifcont15

then14:                                           ; preds = %ifcont10
  %g_fork_4 = load i64, ptr @g_fork_4, align 4
  store i64 %g_fork_4, ptr %addr, align 4
  br label %ifcont15

ifcont15:                                         ; preds = %then14, %ifcont10
  %f = alloca ptr, align 8
  %addr16 = load i64, ptr %addr, align 4
  %calltmp = call ptr @aria_int64_to_wild(i64 %addr16)
  %ffi_is_null = icmp eq ptr %calltmp, null
  %ffi_has_value = xor i1 %ffi_is_null, true
  %optional_hasval = insertvalue { i1, ptr } undef, i1 %ffi_has_value, 0
  %optional_ptr = insertvalue { i1, ptr } %optional_hasval, ptr %calltmp, 1
  %unwrap_optional_ptr = extractvalue { i1, ptr } %optional_ptr, 1
  store ptr %unwrap_optional_ptr, ptr %f, align 8
  %exp = alloca i32, align 4
  store i32 0, ptr %exp, align 4
  %acquired = alloca i1, align 1
  %f17 = load ptr, ptr %f, align 8
  %exp18 = load i32, ptr %exp, align 4
  %calltmp19 = call { i32, ptr, i8 } @ACQ_REL()
  %arg_unwrap = extractvalue { i32, ptr, i8 } %calltmp19, 0
  %calltmp20 = call { i32, ptr, i8 } @ACQUIRE()
  %arg_unwrap21 = extractvalue { i32, ptr, i8 } %calltmp20, 0
  %calltmp22 = call i1 @aria_atomic_int32_compare_exchange_strong(ptr %f17, ptr %exp, i32 1, i32 %arg_unwrap, i32 %arg_unwrap21)
  store i1 %calltmp22, ptr %acquired, align 1
  %f23 = load ptr, ptr %f, align 8
  %calltmp24 = call void @aria_wild_ptr_free(ptr %f23)
  %acquired25 = load i1, ptr %acquired, align 1
  %result.val = insertvalue { i1, ptr, i8 } undef, i1 %acquired25, 0
  %result.err = insertvalue { i1, ptr, i8 } %result.val, ptr null, 1
  %result.is_error = insertvalue { i1, ptr, i8 } %result.err, i8 0, 2
  ret { i1, ptr, i8 } %result.is_error
}

define { %struct.NIL, ptr, i8 } @release_fork(i32 %fork_id) {
entry:
  %addr = alloca i64, align 8
  %g_fork_0 = load i64, ptr @g_fork_0, align 4
  store i64 %g_fork_0, ptr %addr, align 4
  %cmp_cast = sext i32 %fork_id to i64
  %eqtmp = icmp eq i64 %cmp_cast, 1
  %ifcond = icmp ne i1 %eqtmp, false
  br i1 %ifcond, label %then, label %ifcont

then:                                             ; preds = %entry
  %g_fork_1 = load i64, ptr @g_fork_1, align 4
  store i64 %g_fork_1, ptr %addr, align 4
  br label %ifcont

ifcont:                                           ; preds = %then, %entry
  %cmp_cast1 = sext i32 %fork_id to i64
  %eqtmp2 = icmp eq i64 %cmp_cast1, 2
  %ifcond3 = icmp ne i1 %eqtmp2, false
  br i1 %ifcond3, label %then4, label %ifcont5

then4:                                            ; preds = %ifcont
  %g_fork_2 = load i64, ptr @g_fork_2, align 4
  store i64 %g_fork_2, ptr %addr, align 4
  br label %ifcont5

ifcont5:                                          ; preds = %then4, %ifcont
  %cmp_cast6 = sext i32 %fork_id to i64
  %eqtmp7 = icmp eq i64 %cmp_cast6, 3
  %ifcond8 = icmp ne i1 %eqtmp7, false
  br i1 %ifcond8, label %then9, label %ifcont10

then9:                                            ; preds = %ifcont5
  %g_fork_3 = load i64, ptr @g_fork_3, align 4
  store i64 %g_fork_3, ptr %addr, align 4
  br label %ifcont10

ifcont10:                                         ; preds = %then9, %ifcont5
  %cmp_cast11 = sext i32 %fork_id to i64
  %eqtmp12 = icmp eq i64 %cmp_cast11, 4
  %ifcond13 = icmp ne i1 %eqtmp12, false
  br i1 %ifcond13, label %then14, label %ifcont15

then14:                                           ; preds = %ifcont10
  %g_fork_4 = load i64, ptr @g_fork_4, align 4
  store i64 %g_fork_4, ptr %addr, align 4
  br label %ifcont15

ifcont15:                                         ; preds = %then14, %ifcont10
  %f = alloca ptr, align 8
  %addr16 = load i64, ptr %addr, align 4
  %calltmp = call ptr @aria_int64_to_wild(i64 %addr16)
  %ffi_is_null = icmp eq ptr %calltmp, null
  %ffi_has_value = xor i1 %ffi_is_null, true
  %optional_hasval = insertvalue { i1, ptr } undef, i1 %ffi_has_value, 0
  %optional_ptr = insertvalue { i1, ptr } %optional_hasval, ptr %calltmp, 1
  %unwrap_optional_ptr = extractvalue { i1, ptr } %optional_ptr, 1
  store ptr %unwrap_optional_ptr, ptr %f, align 8
  %f17 = load ptr, ptr %f, align 8
  %calltmp18 = call { i32, ptr, i8 } @RELEASE()
  %arg_unwrap = extractvalue { i32, ptr, i8 } %calltmp18, 0
  %calltmp19 = call void @aria_atomic_int32_store(ptr %f17, i32 0, i32 %arg_unwrap)
  %f20 = load ptr, ptr %f, align 8
  %calltmp21 = call void @aria_wild_ptr_free(ptr %f20)
  ret { %struct.NIL, ptr, i8 } zeroinitializer
}

define { %struct.NIL, ptr, i8 } @increment_eat(i32 %phil_id) {
entry:
  %addr = alloca i64, align 8
  %g_eat_0 = load i64, ptr @g_eat_0, align 4
  store i64 %g_eat_0, ptr %addr, align 4
  %cmp_cast = sext i32 %phil_id to i64
  %eqtmp = icmp eq i64 %cmp_cast, 1
  %ifcond = icmp ne i1 %eqtmp, false
  br i1 %ifcond, label %then, label %ifcont

then:                                             ; preds = %entry
  %g_eat_1 = load i64, ptr @g_eat_1, align 4
  store i64 %g_eat_1, ptr %addr, align 4
  br label %ifcont

ifcont:                                           ; preds = %then, %entry
  %cmp_cast1 = sext i32 %phil_id to i64
  %eqtmp2 = icmp eq i64 %cmp_cast1, 2
  %ifcond3 = icmp ne i1 %eqtmp2, false
  br i1 %ifcond3, label %then4, label %ifcont5

then4:                                            ; preds = %ifcont
  %g_eat_2 = load i64, ptr @g_eat_2, align 4
  store i64 %g_eat_2, ptr %addr, align 4
  br label %ifcont5

ifcont5:                                          ; preds = %then4, %ifcont
  %cmp_cast6 = sext i32 %phil_id to i64
  %eqtmp7 = icmp eq i64 %cmp_cast6, 3
  %ifcond8 = icmp ne i1 %eqtmp7, false
  br i1 %ifcond8, label %then9, label %ifcont10

then9:                                            ; preds = %ifcont5
  %g_eat_3 = load i64, ptr @g_eat_3, align 4
  store i64 %g_eat_3, ptr %addr, align 4
  br label %ifcont10

ifcont10:                                         ; preds = %then9, %ifcont5
  %cmp_cast11 = sext i32 %phil_id to i64
  %eqtmp12 = icmp eq i64 %cmp_cast11, 4
  %ifcond13 = icmp ne i1 %eqtmp12, false
  br i1 %ifcond13, label %then14, label %ifcont15

then14:                                           ; preds = %ifcont10
  %g_eat_4 = load i64, ptr @g_eat_4, align 4
  store i64 %g_eat_4, ptr %addr, align 4
  br label %ifcont15

ifcont15:                                         ; preds = %then14, %ifcont10
  %e = alloca ptr, align 8
  %addr16 = load i64, ptr %addr, align 4
  %calltmp = call ptr @aria_int64_to_wild(i64 %addr16)
  %ffi_is_null = icmp eq ptr %calltmp, null
  %ffi_has_value = xor i1 %ffi_is_null, true
  %optional_hasval = insertvalue { i1, ptr } undef, i1 %ffi_has_value, 0
  %optional_ptr = insertvalue { i1, ptr } %optional_hasval, ptr %calltmp, 1
  %unwrap_optional_ptr = extractvalue { i1, ptr } %optional_ptr, 1
  store ptr %unwrap_optional_ptr, ptr %e, align 8
  %e17 = load ptr, ptr %e, align 8
  %calltmp18 = call void @aria_wild_ptr_free(ptr %e17)
  ret { %struct.NIL, ptr, i8 } zeroinitializer
}

define { i32, ptr, i8 } @load_eat(i32 %phil_id) {
entry:
  %addr = alloca i64, align 8
  %g_eat_0 = load i64, ptr @g_eat_0, align 4
  store i64 %g_eat_0, ptr %addr, align 4
  %cmp_cast = sext i32 %phil_id to i64
  %eqtmp = icmp eq i64 %cmp_cast, 1
  %ifcond = icmp ne i1 %eqtmp, false
  br i1 %ifcond, label %then, label %ifcont

then:                                             ; preds = %entry
  %g_eat_1 = load i64, ptr @g_eat_1, align 4
  store i64 %g_eat_1, ptr %addr, align 4
  br label %ifcont

ifcont:                                           ; preds = %then, %entry
  %cmp_cast1 = sext i32 %phil_id to i64
  %eqtmp2 = icmp eq i64 %cmp_cast1, 2
  %ifcond3 = icmp ne i1 %eqtmp2, false
  br i1 %ifcond3, label %then4, label %ifcont5

then4:                                            ; preds = %ifcont
  %g_eat_2 = load i64, ptr @g_eat_2, align 4
  store i64 %g_eat_2, ptr %addr, align 4
  br label %ifcont5

ifcont5:                                          ; preds = %then4, %ifcont
  %cmp_cast6 = sext i32 %phil_id to i64
  %eqtmp7 = icmp eq i64 %cmp_cast6, 3
  %ifcond8 = icmp ne i1 %eqtmp7, false
  br i1 %ifcond8, label %then9, label %ifcont10

then9:                                            ; preds = %ifcont5
  %g_eat_3 = load i64, ptr @g_eat_3, align 4
  store i64 %g_eat_3, ptr %addr, align 4
  br label %ifcont10

ifcont10:                                         ; preds = %then9, %ifcont5
  %cmp_cast11 = sext i32 %phil_id to i64
  %eqtmp12 = icmp eq i64 %cmp_cast11, 4
  %ifcond13 = icmp ne i1 %eqtmp12, false
  br i1 %ifcond13, label %then14, label %ifcont15

then14:                                           ; preds = %ifcont10
  %g_eat_4 = load i64, ptr @g_eat_4, align 4
  store i64 %g_eat_4, ptr %addr, align 4
  br label %ifcont15

ifcont15:                                         ; preds = %then14, %ifcont10
  %e = alloca ptr, align 8
  %addr16 = load i64, ptr %addr, align 4
  %calltmp = call ptr @aria_int64_to_wild(i64 %addr16)
  %ffi_is_null = icmp eq ptr %calltmp, null
  %ffi_has_value = xor i1 %ffi_is_null, true
  %optional_hasval = insertvalue { i1, ptr } undef, i1 %ffi_has_value, 0
  %optional_ptr = insertvalue { i1, ptr } %optional_hasval, ptr %calltmp, 1
  %unwrap_optional_ptr = extractvalue { i1, ptr } %optional_ptr, 1
  store ptr %unwrap_optional_ptr, ptr %e, align 8
  %val = alloca i32, align 4
  %e17 = load ptr, ptr %e, align 8
  %calltmp18 = call { i32, ptr, i8 } @RELAXED()
  %arg_unwrap = extractvalue { i32, ptr, i8 } %calltmp18, 0
  %calltmp19 = call i32 @aria_atomic_int32_load(ptr %e17, i32 %arg_unwrap)
  store i32 %calltmp19, ptr %val, align 4
  %e20 = load ptr, ptr %e, align 8
  %calltmp21 = call void @aria_wild_ptr_free(ptr %e20)
  %val22 = load i32, ptr %val, align 4
  %result.val = insertvalue { i32, ptr, i8 } undef, i32 %val22, 0
  %result.err = insertvalue { i32, ptr, i8 } %result.val, ptr null, 1
  %result.is_error = insertvalue { i32, ptr, i8 } %result.err, i8 0, 2
  ret { i32, ptr, i8 } %result.is_error
}

define { %struct.NIL, ptr, i8 } @run_philosopher(i32 %id) {
entry:
  %N = alloca i32, align 4
  %calltmp = call { i32, ptr, i8 } @N_PHIL()
  %raw.value = extractvalue { i32, ptr, i8 } %calltmp, 0
  store i32 %raw.value, ptr %N, align 4
  %left = alloca i32, align 4
  store i32 %id, ptr %left, align 4
  %right = alloca i32, align 4
  %sadd = call { i32, i1 } @llvm.sadd.with.overflow.i32(i32 %id, i32 1)
  %addtmp = extractvalue { i32, i1 } %sadd, 0
  %add.overflow = extractvalue { i32, i1 } %sadd, 1
  %safe.addtmp = select i1 %add.overflow, i32 2147483647, i32 %addtmp
  %N1 = load i32, ptr %N, align 4
  %mod.iszero = icmp eq i32 %N1, 0
  %modtmp = srem i32 %safe.addtmp, %N1
  %safe.modtmp = select i1 %mod.iszero, i32 2147483647, i32 %modtmp
  store i32 %safe.modtmp, ptr %right, align 4
  %first = alloca i32, align 4
  %left2 = load i32, ptr %left, align 4
  store i32 %left2, ptr %first, align 4
  %second = alloca i32, align 4
  %right3 = load i32, ptr %right, align 4
  store i32 %right3, ptr %second, align 4
  %right4 = load i32, ptr %right, align 4
  %left5 = load i32, ptr %left, align 4
  %lttmp = icmp slt i32 %right4, %left5
  %ifcond = icmp ne i1 %lttmp, false
  br i1 %ifcond, label %then, label %ifcont

then:                                             ; preds = %entry
  %right6 = load i32, ptr %right, align 4
  store i32 %right6, ptr %first, align 4
  %left7 = load i32, ptr %left, align 4
  store i32 %left7, ptr %second, align 4
  br label %ifcont

ifcont:                                           ; preds = %then, %entry
  br label %whilecond

whilecond:                                        ; preds = %afterwhile44, %ifcont
  %calltmp8 = call { i32, ptr, i8 } @load_running()
  %raw.value9 = extractvalue { i32, ptr, i8 } %calltmp8, 0
  %cmp_cast = sext i32 %raw.value9 to i64
  %eqtmp = icmp eq i64 %cmp_cast, 1
  %whilecond10 = icmp ne i1 %eqtmp, false
  br i1 %whilecond10, label %whilebody, label %afterwhile53

whilebody:                                        ; preds = %whilecond
  %calltmp11 = call { i64, ptr, i8 } @THINK_NS()
  %arg_unwrap = extractvalue { i64, ptr, i8 } %calltmp11, 0
  %calltmp12 = call void @aria_thread_sleep_ns(i64 %arg_unwrap)
  %got1 = alloca i1, align 1
  store i1 false, ptr %got1, align 1
  br label %whilecond13

whilecond13:                                      ; preds = %ifcont27, %whilebody
  %got114 = load i1, ptr %got1, align 1
  %nottmp = xor i1 %got114, true
  %whilecond15 = icmp ne i1 %nottmp, false
  br i1 %whilecond15, label %whilebody16, label %afterwhile

whilebody16:                                      ; preds = %whilecond13
  %first17 = load i32, ptr %first, align 4
  %calltmp18 = call { i1, ptr, i8 } @try_acquire_fork(i32 %first17)
  %raw.value19 = extractvalue { i1, ptr, i8 } %calltmp18, 0
  store i1 %raw.value19, ptr %got1, align 1
  %got120 = load i1, ptr %got1, align 1
  %nottmp21 = xor i1 %got120, true
  %ifcond22 = icmp ne i1 %nottmp21, false
  br i1 %ifcond22, label %then23, label %ifcont27

then23:                                           ; preds = %whilebody16
  %calltmp24 = call { i64, ptr, i8 } @RETRY_NS()
  %arg_unwrap25 = extractvalue { i64, ptr, i8 } %calltmp24, 0
  %calltmp26 = call void @aria_thread_sleep_ns(i64 %arg_unwrap25)
  br label %ifcont27

ifcont27:                                         ; preds = %then23, %whilebody16
  br label %whilecond13

afterwhile:                                       ; preds = %whilecond13
  %got2 = alloca i1, align 1
  store i1 false, ptr %got2, align 1
  br label %whilecond28

whilecond28:                                      ; preds = %ifcont43, %afterwhile
  %got229 = load i1, ptr %got2, align 1
  %nottmp30 = xor i1 %got229, true
  %whilecond31 = icmp ne i1 %nottmp30, false
  br i1 %whilecond31, label %whilebody32, label %afterwhile44

whilebody32:                                      ; preds = %whilecond28
  %second33 = load i32, ptr %second, align 4
  %calltmp34 = call { i1, ptr, i8 } @try_acquire_fork(i32 %second33)
  %raw.value35 = extractvalue { i1, ptr, i8 } %calltmp34, 0
  store i1 %raw.value35, ptr %got2, align 1
  %got236 = load i1, ptr %got2, align 1
  %nottmp37 = xor i1 %got236, true
  %ifcond38 = icmp ne i1 %nottmp37, false
  br i1 %ifcond38, label %then39, label %ifcont43

then39:                                           ; preds = %whilebody32
  %calltmp40 = call { i64, ptr, i8 } @RETRY_NS()
  %arg_unwrap41 = extractvalue { i64, ptr, i8 } %calltmp40, 0
  %calltmp42 = call void @aria_thread_sleep_ns(i64 %arg_unwrap41)
  br label %ifcont43

ifcont43:                                         ; preds = %then39, %whilebody32
  br label %whilecond28

afterwhile44:                                     ; preds = %whilecond28
  %calltmp45 = call { i64, ptr, i8 } @EAT_NS()
  %arg_unwrap46 = extractvalue { i64, ptr, i8 } %calltmp45, 0
  %calltmp47 = call void @aria_thread_sleep_ns(i64 %arg_unwrap46)
  %calltmp48 = call { %struct.NIL, ptr, i8 } @increment_eat(i32 %id)
  %first49 = load i32, ptr %first, align 4
  %calltmp50 = call { %struct.NIL, ptr, i8 } @release_fork(i32 %first49)
  %second51 = load i32, ptr %second, align 4
  %calltmp52 = call { %struct.NIL, ptr, i8 } @release_fork(i32 %second51)
  br label %whilecond

afterwhile53:                                     ; preds = %whilecond
  ret { %struct.NIL, ptr, i8 } zeroinitializer
}

define { i64, ptr, i8 } @philosopher_0(i64 %_a) {
entry:
  %calltmp = call { %struct.NIL, ptr, i8 } @run_philosopher(i32 0)
  ret { i64, ptr, i8 } zeroinitializer
}

define { i64, ptr, i8 } @philosopher_1(i64 %_a) {
entry:
  %calltmp = call { %struct.NIL, ptr, i8 } @run_philosopher(i32 1)
  ret { i64, ptr, i8 } zeroinitializer
}

define { i64, ptr, i8 } @philosopher_2(i64 %_a) {
entry:
  %calltmp = call { %struct.NIL, ptr, i8 } @run_philosopher(i32 2)
  ret { i64, ptr, i8 } zeroinitializer
}

define { i64, ptr, i8 } @philosopher_3(i64 %_a) {
entry:
  %calltmp = call { %struct.NIL, ptr, i8 } @run_philosopher(i32 3)
  ret { i64, ptr, i8 } zeroinitializer
}

define { i64, ptr, i8 } @philosopher_4(i64 %_a) {
entry:
  %calltmp = call { %struct.NIL, ptr, i8 } @run_philosopher(i32 4)
  ret { i64, ptr, i8 } zeroinitializer
}

define { %struct.NIL, ptr, i8 } @failsafe(i32 %code) {
entry:
  ret { %struct.NIL, ptr, i8 } zeroinitializer
}

define i32 @main() {
entry:
  call void @aria_gc_init(i64 0, i64 0)
  %N = alloca i32, align 4
  %calltmp = call { i32, ptr, i8 } @N_PHIL()
  %raw.value = extractvalue { i32, ptr, i8 } %calltmp, 0
  store i32 %raw.value, ptr %N, align 4
  %calltmp1 = call { %struct.NIL, ptr, i8 } @printlnString(ptr @.str.84)
  %calltmp2 = call { %struct.NIL, ptr, i8 } @printString(ptr @.str.86)
  %N3 = load i32, ptr %N, align 4
  %calltmp4 = call { %struct.NIL, ptr, i8 } @printlnInt32(i32 %N3)
  %calltmp5 = call { %struct.NIL, ptr, i8 } @printlnString(ptr @.str.88)
  %calltmp6 = call { %struct.NIL, ptr, i8 } @printlnString(ptr @.str.90)
  %f0 = alloca ptr, align 8
  %calltmp7 = call ptr @aria_atomic_int32_create(i32 0)
  %ffi_is_null = icmp eq ptr %calltmp7, null
  %ffi_has_value = xor i1 %ffi_is_null, true
  %optional_hasval = insertvalue { i1, ptr } undef, i1 %ffi_has_value, 0
  %optional_ptr = insertvalue { i1, ptr } %optional_hasval, ptr %calltmp7, 1
  %unwrap_optional_ptr = extractvalue { i1, ptr } %optional_ptr, 1
  store ptr %unwrap_optional_ptr, ptr %f0, align 8
  %f1 = alloca ptr, align 8
  %calltmp8 = call ptr @aria_atomic_int32_create(i32 0)
  %ffi_is_null9 = icmp eq ptr %calltmp8, null
  %ffi_has_value10 = xor i1 %ffi_is_null9, true
  %optional_hasval11 = insertvalue { i1, ptr } undef, i1 %ffi_has_value10, 0
  %optional_ptr12 = insertvalue { i1, ptr } %optional_hasval11, ptr %calltmp8, 1
  %unwrap_optional_ptr13 = extractvalue { i1, ptr } %optional_ptr12, 1
  store ptr %unwrap_optional_ptr13, ptr %f1, align 8
  %f2 = alloca ptr, align 8
  %calltmp14 = call ptr @aria_atomic_int32_create(i32 0)
  %ffi_is_null15 = icmp eq ptr %calltmp14, null
  %ffi_has_value16 = xor i1 %ffi_is_null15, true
  %optional_hasval17 = insertvalue { i1, ptr } undef, i1 %ffi_has_value16, 0
  %optional_ptr18 = insertvalue { i1, ptr } %optional_hasval17, ptr %calltmp14, 1
  %unwrap_optional_ptr19 = extractvalue { i1, ptr } %optional_ptr18, 1
  store ptr %unwrap_optional_ptr19, ptr %f2, align 8
  %f3 = alloca ptr, align 8
  %calltmp20 = call ptr @aria_atomic_int32_create(i32 0)
  %ffi_is_null21 = icmp eq ptr %calltmp20, null
  %ffi_has_value22 = xor i1 %ffi_is_null21, true
  %optional_hasval23 = insertvalue { i1, ptr } undef, i1 %ffi_has_value22, 0
  %optional_ptr24 = insertvalue { i1, ptr } %optional_hasval23, ptr %calltmp20, 1
  %unwrap_optional_ptr25 = extractvalue { i1, ptr } %optional_ptr24, 1
  store ptr %unwrap_optional_ptr25, ptr %f3, align 8
  %f4 = alloca ptr, align 8
  %calltmp26 = call ptr @aria_atomic_int32_create(i32 0)
  %ffi_is_null27 = icmp eq ptr %calltmp26, null
  %ffi_has_value28 = xor i1 %ffi_is_null27, true
  %optional_hasval29 = insertvalue { i1, ptr } undef, i1 %ffi_has_value28, 0
  %optional_ptr30 = insertvalue { i1, ptr } %optional_hasval29, ptr %calltmp26, 1
  %unwrap_optional_ptr31 = extractvalue { i1, ptr } %optional_ptr30, 1
  store ptr %unwrap_optional_ptr31, ptr %f4, align 8
  %f032 = load ptr, ptr %f0, align 8
  %calltmp33 = call i64 @aria_wild_to_int64(ptr %f032)
  store i64 %calltmp33, ptr @g_fork_0, align 4
  %f134 = load ptr, ptr %f1, align 8
  %calltmp35 = call i64 @aria_wild_to_int64(ptr %f134)
  store i64 %calltmp35, ptr @g_fork_1, align 4
  %f236 = load ptr, ptr %f2, align 8
  %calltmp37 = call i64 @aria_wild_to_int64(ptr %f236)
  store i64 %calltmp37, ptr @g_fork_2, align 4
  %f338 = load ptr, ptr %f3, align 8
  %calltmp39 = call i64 @aria_wild_to_int64(ptr %f338)
  store i64 %calltmp39, ptr @g_fork_3, align 4
  %f440 = load ptr, ptr %f4, align 8
  %calltmp41 = call i64 @aria_wild_to_int64(ptr %f440)
  store i64 %calltmp41, ptr @g_fork_4, align 4
  %f042 = load ptr, ptr %f0, align 8
  %calltmp43 = call void @aria_wild_ptr_free(ptr %f042)
  %f144 = load ptr, ptr %f1, align 8
  %calltmp45 = call void @aria_wild_ptr_free(ptr %f144)
  %f246 = load ptr, ptr %f2, align 8
  %calltmp47 = call void @aria_wild_ptr_free(ptr %f246)
  %f348 = load ptr, ptr %f3, align 8
  %calltmp49 = call void @aria_wild_ptr_free(ptr %f348)
  %f450 = load ptr, ptr %f4, align 8
  %calltmp51 = call void @aria_wild_ptr_free(ptr %f450)
  %e0 = alloca ptr, align 8
  %calltmp52 = call ptr @aria_atomic_int32_create(i32 0)
  %ffi_is_null53 = icmp eq ptr %calltmp52, null
  %ffi_has_value54 = xor i1 %ffi_is_null53, true
  %optional_hasval55 = insertvalue { i1, ptr } undef, i1 %ffi_has_value54, 0
  %optional_ptr56 = insertvalue { i1, ptr } %optional_hasval55, ptr %calltmp52, 1
  %unwrap_optional_ptr57 = extractvalue { i1, ptr } %optional_ptr56, 1
  store ptr %unwrap_optional_ptr57, ptr %e0, align 8
  %e1 = alloca ptr, align 8
  %calltmp58 = call ptr @aria_atomic_int32_create(i32 0)
  %ffi_is_null59 = icmp eq ptr %calltmp58, null
  %ffi_has_value60 = xor i1 %ffi_is_null59, true
  %optional_hasval61 = insertvalue { i1, ptr } undef, i1 %ffi_has_value60, 0
  %optional_ptr62 = insertvalue { i1, ptr } %optional_hasval61, ptr %calltmp58, 1
  %unwrap_optional_ptr63 = extractvalue { i1, ptr } %optional_ptr62, 1
  store ptr %unwrap_optional_ptr63, ptr %e1, align 8
  %e2 = alloca ptr, align 8
  %calltmp64 = call ptr @aria_atomic_int32_create(i32 0)
  %ffi_is_null65 = icmp eq ptr %calltmp64, null
  %ffi_has_value66 = xor i1 %ffi_is_null65, true
  %optional_hasval67 = insertvalue { i1, ptr } undef, i1 %ffi_has_value66, 0
  %optional_ptr68 = insertvalue { i1, ptr } %optional_hasval67, ptr %calltmp64, 1
  %unwrap_optional_ptr69 = extractvalue { i1, ptr } %optional_ptr68, 1
  store ptr %unwrap_optional_ptr69, ptr %e2, align 8
  %e3 = alloca ptr, align 8
  %calltmp70 = call ptr @aria_atomic_int32_create(i32 0)
  %ffi_is_null71 = icmp eq ptr %calltmp70, null
  %ffi_has_value72 = xor i1 %ffi_is_null71, true
  %optional_hasval73 = insertvalue { i1, ptr } undef, i1 %ffi_has_value72, 0
  %optional_ptr74 = insertvalue { i1, ptr } %optional_hasval73, ptr %calltmp70, 1
  %unwrap_optional_ptr75 = extractvalue { i1, ptr } %optional_ptr74, 1
  store ptr %unwrap_optional_ptr75, ptr %e3, align 8
  %e4 = alloca ptr, align 8
  %calltmp76 = call ptr @aria_atomic_int32_create(i32 0)
  %ffi_is_null77 = icmp eq ptr %calltmp76, null
  %ffi_has_value78 = xor i1 %ffi_is_null77, true
  %optional_hasval79 = insertvalue { i1, ptr } undef, i1 %ffi_has_value78, 0
  %optional_ptr80 = insertvalue { i1, ptr } %optional_hasval79, ptr %calltmp76, 1
  %unwrap_optional_ptr81 = extractvalue { i1, ptr } %optional_ptr80, 1
  store ptr %unwrap_optional_ptr81, ptr %e4, align 8
  %e082 = load ptr, ptr %e0, align 8
  %calltmp83 = call i64 @aria_wild_to_int64(ptr %e082)
  store i64 %calltmp83, ptr @g_eat_0, align 4
  %e184 = load ptr, ptr %e1, align 8
  %calltmp85 = call i64 @aria_wild_to_int64(ptr %e184)
  store i64 %calltmp85, ptr @g_eat_1, align 4
  %e286 = load ptr, ptr %e2, align 8
  %calltmp87 = call i64 @aria_wild_to_int64(ptr %e286)
  store i64 %calltmp87, ptr @g_eat_2, align 4
  %e388 = load ptr, ptr %e3, align 8
  %calltmp89 = call i64 @aria_wild_to_int64(ptr %e388)
  store i64 %calltmp89, ptr @g_eat_3, align 4
  %e490 = load ptr, ptr %e4, align 8
  %calltmp91 = call i64 @aria_wild_to_int64(ptr %e490)
  store i64 %calltmp91, ptr @g_eat_4, align 4
  %e092 = load ptr, ptr %e0, align 8
  %calltmp93 = call void @aria_wild_ptr_free(ptr %e092)
  %e194 = load ptr, ptr %e1, align 8
  %calltmp95 = call void @aria_wild_ptr_free(ptr %e194)
  %e296 = load ptr, ptr %e2, align 8
  %calltmp97 = call void @aria_wild_ptr_free(ptr %e296)
  %e398 = load ptr, ptr %e3, align 8
  %calltmp99 = call void @aria_wild_ptr_free(ptr %e398)
  %e4100 = load ptr, ptr %e4, align 8
  %calltmp101 = call void @aria_wild_ptr_free(ptr %e4100)
  %ra = alloca ptr, align 8
  %calltmp102 = call ptr @aria_atomic_int32_create(i32 1)
  %ffi_is_null103 = icmp eq ptr %calltmp102, null
  %ffi_has_value104 = xor i1 %ffi_is_null103, true
  %optional_hasval105 = insertvalue { i1, ptr } undef, i1 %ffi_has_value104, 0
  %optional_ptr106 = insertvalue { i1, ptr } %optional_hasval105, ptr %calltmp102, 1
  %unwrap_optional_ptr107 = extractvalue { i1, ptr } %optional_ptr106, 1
  store ptr %unwrap_optional_ptr107, ptr %ra, align 8
  %ra108 = load ptr, ptr %ra, align 8
  %calltmp109 = call i64 @aria_wild_to_int64(ptr %ra108)
  store i64 %calltmp109, ptr @g_running, align 4
  %ra110 = load ptr, ptr %ra, align 8
  %calltmp111 = call void @aria_wild_ptr_free(ptr %ra110)
  %fn0 = alloca { ptr, ptr }, align 8
  %fn1 = alloca { ptr, ptr }, align 8
  %fn2 = alloca { ptr, ptr }, align 8
  %fn3 = alloca { ptr, ptr }, align 8
  %fn4 = alloca { ptr, ptr }, align 8
  %calltmp112 = call { i64, ptr, i8 } @RUN_NS()
  %arg_unwrap = extractvalue { i64, ptr, i8 } %calltmp112, 0
  %calltmp113 = call void @aria_thread_sleep_ns(i64 %arg_unwrap)
  %r_stop = alloca ptr, align 8
  %g_running = load i64, ptr @g_running, align 4
  %calltmp114 = call ptr @aria_int64_to_wild(i64 %g_running)
  %ffi_is_null115 = icmp eq ptr %calltmp114, null
  %ffi_has_value116 = xor i1 %ffi_is_null115, true
  %optional_hasval117 = insertvalue { i1, ptr } undef, i1 %ffi_has_value116, 0
  %optional_ptr118 = insertvalue { i1, ptr } %optional_hasval117, ptr %calltmp114, 1
  %unwrap_optional_ptr119 = extractvalue { i1, ptr } %optional_ptr118, 1
  store ptr %unwrap_optional_ptr119, ptr %r_stop, align 8
  %r_stop120 = load ptr, ptr %r_stop, align 8
  %calltmp121 = call { i32, ptr, i8 } @SEQ_CST()
  %arg_unwrap122 = extractvalue { i32, ptr, i8 } %calltmp121, 0
  %calltmp123 = call void @aria_atomic_int32_store(ptr %r_stop120, i32 0, i32 %arg_unwrap122)
  %r_stop124 = load ptr, ptr %r_stop, align 8
  %calltmp125 = call void @aria_wild_ptr_free(ptr %r_stop124)
  %all_ate = alloca i1, align 1
  store i1 true, ptr %all_ate, align 1
  %i = alloca i32, align 4
  store i32 0, ptr %i, align 4
  br label %whilecond

whilecond:                                        ; preds = %ifcont, %entry
  %i126 = load i32, ptr %i, align 4
  %N127 = load i32, ptr %N, align 4
  %lttmp = icmp slt i32 %i126, %N127
  %whilecond128 = icmp ne i1 %lttmp, false
  br i1 %whilecond128, label %whilebody, label %afterwhile

whilebody:                                        ; preds = %whilecond
  %count = alloca i32, align 4
  %i129 = load i32, ptr %i, align 4
  %calltmp130 = call { i32, ptr, i8 } @load_eat(i32 %i129)
  %raw.value131 = extractvalue { i32, ptr, i8 } %calltmp130, 0
  store i32 %raw.value131, ptr %count, align 4
  %calltmp132 = call { %struct.NIL, ptr, i8 } @printString(ptr @.str.92)
  %i133 = load i32, ptr %i, align 4
  %calltmp134 = call { ptr, ptr, i8 } @int32_toString(i32 %i133)
  %arg_unwrap135 = extractvalue { ptr, ptr, i8 } %calltmp134, 0
  %calltmp136 = call { %struct.NIL, ptr, i8 } @printString(ptr %arg_unwrap135)
  %calltmp137 = call { %struct.NIL, ptr, i8 } @printString(ptr @.str.94)
  %count138 = load i32, ptr %count, align 4
  %calltmp139 = call { ptr, ptr, i8 } @int32_toString(i32 %count138)
  %arg_unwrap140 = extractvalue { ptr, ptr, i8 } %calltmp139, 0
  %calltmp141 = call { %struct.NIL, ptr, i8 } @printString(ptr %arg_unwrap140)
  %calltmp142 = call { %struct.NIL, ptr, i8 } @printlnString(ptr @.str.96)
  %count143 = load i32, ptr %count, align 4
  %cmp_cast = sext i32 %count143 to i64
  %eqtmp = icmp eq i64 %cmp_cast, 0
  %ifcond = icmp ne i1 %eqtmp, false
  br i1 %ifcond, label %then, label %ifcont

then:                                             ; preds = %whilebody
  store i1 false, ptr %all_ate, align 1
  br label %ifcont

ifcont:                                           ; preds = %then, %whilebody
  %i144 = load i32, ptr %i, align 4
  %sadd = call { i32, i1 } @llvm.sadd.with.overflow.i32(i32 %i144, i32 1)
  %addtmp = extractvalue { i32, i1 } %sadd, 0
  %add.overflow = extractvalue { i32, i1 } %sadd, 1
  %safe.addtmp = select i1 %add.overflow, i32 2147483647, i32 %addtmp
  store i32 %safe.addtmp, ptr %i, align 4
  br label %whilecond

afterwhile:                                       ; preds = %whilecond
  %calltmp145 = call { %struct.NIL, ptr, i8 } @printlnString(ptr @.str.98)
  %all_ate146 = load i1, ptr %all_ate, align 1
  %ifcond147 = icmp ne i1 %all_ate146, false
  br i1 %ifcond147, label %then148, label %else

then148:                                          ; preds = %afterwhile
  %calltmp149 = call { %struct.NIL, ptr, i8 } @printlnString(ptr @.str.100)
  br label %ifcont151

else:                                             ; preds = %afterwhile
  %calltmp150 = call { %struct.NIL, ptr, i8 } @printlnString(ptr @.str.102)
  br label %ifcont151

ifcont151:                                        ; preds = %else, %then148
  %df0 = alloca ptr, align 8
  %g_fork_0 = load i64, ptr @g_fork_0, align 4
  %calltmp152 = call ptr @aria_int64_to_wild(i64 %g_fork_0)
  %ffi_is_null153 = icmp eq ptr %calltmp152, null
  %ffi_has_value154 = xor i1 %ffi_is_null153, true
  %optional_hasval155 = insertvalue { i1, ptr } undef, i1 %ffi_has_value154, 0
  %optional_ptr156 = insertvalue { i1, ptr } %optional_hasval155, ptr %calltmp152, 1
  %unwrap_optional_ptr157 = extractvalue { i1, ptr } %optional_ptr156, 1
  store ptr %unwrap_optional_ptr157, ptr %df0, align 8
  %df1 = alloca ptr, align 8
  %g_fork_1 = load i64, ptr @g_fork_1, align 4
  %calltmp158 = call ptr @aria_int64_to_wild(i64 %g_fork_1)
  %ffi_is_null159 = icmp eq ptr %calltmp158, null
  %ffi_has_value160 = xor i1 %ffi_is_null159, true
  %optional_hasval161 = insertvalue { i1, ptr } undef, i1 %ffi_has_value160, 0
  %optional_ptr162 = insertvalue { i1, ptr } %optional_hasval161, ptr %calltmp158, 1
  %unwrap_optional_ptr163 = extractvalue { i1, ptr } %optional_ptr162, 1
  store ptr %unwrap_optional_ptr163, ptr %df1, align 8
  %df2 = alloca ptr, align 8
  %g_fork_2 = load i64, ptr @g_fork_2, align 4
  %calltmp164 = call ptr @aria_int64_to_wild(i64 %g_fork_2)
  %ffi_is_null165 = icmp eq ptr %calltmp164, null
  %ffi_has_value166 = xor i1 %ffi_is_null165, true
  %optional_hasval167 = insertvalue { i1, ptr } undef, i1 %ffi_has_value166, 0
  %optional_ptr168 = insertvalue { i1, ptr } %optional_hasval167, ptr %calltmp164, 1
  %unwrap_optional_ptr169 = extractvalue { i1, ptr } %optional_ptr168, 1
  store ptr %unwrap_optional_ptr169, ptr %df2, align 8
  %df3 = alloca ptr, align 8
  %g_fork_3 = load i64, ptr @g_fork_3, align 4
  %calltmp170 = call ptr @aria_int64_to_wild(i64 %g_fork_3)
  %ffi_is_null171 = icmp eq ptr %calltmp170, null
  %ffi_has_value172 = xor i1 %ffi_is_null171, true
  %optional_hasval173 = insertvalue { i1, ptr } undef, i1 %ffi_has_value172, 0
  %optional_ptr174 = insertvalue { i1, ptr } %optional_hasval173, ptr %calltmp170, 1
  %unwrap_optional_ptr175 = extractvalue { i1, ptr } %optional_ptr174, 1
  store ptr %unwrap_optional_ptr175, ptr %df3, align 8
  %df4 = alloca ptr, align 8
  %g_fork_4 = load i64, ptr @g_fork_4, align 4
  %calltmp176 = call ptr @aria_int64_to_wild(i64 %g_fork_4)
  %ffi_is_null177 = icmp eq ptr %calltmp176, null
  %ffi_has_value178 = xor i1 %ffi_is_null177, true
  %optional_hasval179 = insertvalue { i1, ptr } undef, i1 %ffi_has_value178, 0
  %optional_ptr180 = insertvalue { i1, ptr } %optional_hasval179, ptr %calltmp176, 1
  %unwrap_optional_ptr181 = extractvalue { i1, ptr } %optional_ptr180, 1
  store ptr %unwrap_optional_ptr181, ptr %df4, align 8
  %df0182 = load ptr, ptr %df0, align 8
  %calltmp183 = call void @aria_atomic_int32_destroy(ptr %df0182)
  %df1184 = load ptr, ptr %df1, align 8
  %calltmp185 = call void @aria_atomic_int32_destroy(ptr %df1184)
  %df2186 = load ptr, ptr %df2, align 8
  %calltmp187 = call void @aria_atomic_int32_destroy(ptr %df2186)
  %df3188 = load ptr, ptr %df3, align 8
  %calltmp189 = call void @aria_atomic_int32_destroy(ptr %df3188)
  %df4190 = load ptr, ptr %df4, align 8
  %calltmp191 = call void @aria_atomic_int32_destroy(ptr %df4190)
  %de0 = alloca ptr, align 8
  %g_eat_0 = load i64, ptr @g_eat_0, align 4
  %calltmp192 = call ptr @aria_int64_to_wild(i64 %g_eat_0)
  %ffi_is_null193 = icmp eq ptr %calltmp192, null
  %ffi_has_value194 = xor i1 %ffi_is_null193, true
  %optional_hasval195 = insertvalue { i1, ptr } undef, i1 %ffi_has_value194, 0
  %optional_ptr196 = insertvalue { i1, ptr } %optional_hasval195, ptr %calltmp192, 1
  %unwrap_optional_ptr197 = extractvalue { i1, ptr } %optional_ptr196, 1
  store ptr %unwrap_optional_ptr197, ptr %de0, align 8
  %de1 = alloca ptr, align 8
  %g_eat_1 = load i64, ptr @g_eat_1, align 4
  %calltmp198 = call ptr @aria_int64_to_wild(i64 %g_eat_1)
  %ffi_is_null199 = icmp eq ptr %calltmp198, null
  %ffi_has_value200 = xor i1 %ffi_is_null199, true
  %optional_hasval201 = insertvalue { i1, ptr } undef, i1 %ffi_has_value200, 0
  %optional_ptr202 = insertvalue { i1, ptr } %optional_hasval201, ptr %calltmp198, 1
  %unwrap_optional_ptr203 = extractvalue { i1, ptr } %optional_ptr202, 1
  store ptr %unwrap_optional_ptr203, ptr %de1, align 8
  %de2 = alloca ptr, align 8
  %g_eat_2 = load i64, ptr @g_eat_2, align 4
  %calltmp204 = call ptr @aria_int64_to_wild(i64 %g_eat_2)
  %ffi_is_null205 = icmp eq ptr %calltmp204, null
  %ffi_has_value206 = xor i1 %ffi_is_null205, true
  %optional_hasval207 = insertvalue { i1, ptr } undef, i1 %ffi_has_value206, 0
  %optional_ptr208 = insertvalue { i1, ptr } %optional_hasval207, ptr %calltmp204, 1
  %unwrap_optional_ptr209 = extractvalue { i1, ptr } %optional_ptr208, 1
  store ptr %unwrap_optional_ptr209, ptr %de2, align 8
  %de3 = alloca ptr, align 8
  %g_eat_3 = load i64, ptr @g_eat_3, align 4
  %calltmp210 = call ptr @aria_int64_to_wild(i64 %g_eat_3)
  %ffi_is_null211 = icmp eq ptr %calltmp210, null
  %ffi_has_value212 = xor i1 %ffi_is_null211, true
  %optional_hasval213 = insertvalue { i1, ptr } undef, i1 %ffi_has_value212, 0
  %optional_ptr214 = insertvalue { i1, ptr } %optional_hasval213, ptr %calltmp210, 1
  %unwrap_optional_ptr215 = extractvalue { i1, ptr } %optional_ptr214, 1
  store ptr %unwrap_optional_ptr215, ptr %de3, align 8
  %de4 = alloca ptr, align 8
  %g_eat_4 = load i64, ptr @g_eat_4, align 4
  %calltmp216 = call ptr @aria_int64_to_wild(i64 %g_eat_4)
  %ffi_is_null217 = icmp eq ptr %calltmp216, null
  %ffi_has_value218 = xor i1 %ffi_is_null217, true
  %optional_hasval219 = insertvalue { i1, ptr } undef, i1 %ffi_has_value218, 0
  %optional_ptr220 = insertvalue { i1, ptr } %optional_hasval219, ptr %calltmp216, 1
  %unwrap_optional_ptr221 = extractvalue { i1, ptr } %optional_ptr220, 1
  store ptr %unwrap_optional_ptr221, ptr %de4, align 8
  %de0222 = load ptr, ptr %de0, align 8
  %calltmp223 = call void @aria_atomic_int32_destroy(ptr %de0222)
  %de1224 = load ptr, ptr %de1, align 8
  %calltmp225 = call void @aria_atomic_int32_destroy(ptr %de1224)
  %de2226 = load ptr, ptr %de2, align 8
  %calltmp227 = call void @aria_atomic_int32_destroy(ptr %de2226)
  %de3228 = load ptr, ptr %de3, align 8
  %calltmp229 = call void @aria_atomic_int32_destroy(ptr %de3228)
  %de4230 = load ptr, ptr %de4, align 8
  %calltmp231 = call void @aria_atomic_int32_destroy(ptr %de4230)
  %dr = alloca ptr, align 8
  %g_running232 = load i64, ptr @g_running, align 4
  %calltmp233 = call ptr @aria_int64_to_wild(i64 %g_running232)
  %ffi_is_null234 = icmp eq ptr %calltmp233, null
  %ffi_has_value235 = xor i1 %ffi_is_null234, true
  %optional_hasval236 = insertvalue { i1, ptr } undef, i1 %ffi_has_value235, 0
  %optional_ptr237 = insertvalue { i1, ptr } %optional_hasval236, ptr %calltmp233, 1
  %unwrap_optional_ptr238 = extractvalue { i1, ptr } %optional_ptr237, 1
  store ptr %unwrap_optional_ptr238, ptr %dr, align 8
  %dr239 = load ptr, ptr %dr, align 8
  %calltmp240 = call void @aria_atomic_int32_destroy(ptr %dr239)
  ret i32 0
}

declare void @aria_gc_init(i64, i64)

define i32 @__philosophers_init() {
entry:
  ret i32 0
}

attributes #0 = { nocallback nofree nosync nounwind speculatable willreturn memory(none) }
