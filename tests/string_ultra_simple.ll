; ModuleID = 'tests/string_ultra_simple.aria'
source_filename = "tests/string_ultra_simple.aria"

%struct.AriaString = type { ptr, i64 }

@.str.data = private constant [6 x i8] c"Hello\00"
@.str = private constant %struct.AriaString { ptr @.str.data, i64 5 }

define i64 @main() {
entry:
  %len = alloca i64, align 8
  %str = load %struct.AriaString, ptr @.str, align 8
  %length = extractvalue %struct.AriaString %str, 1
  store i64 %length, ptr %len, align 4
  %len1 = load i64, ptr %len, align 4
  ret i64 %len1
}

define i32 @__string_ultra_simple_init() {
entry:
  ret i32 0
}
