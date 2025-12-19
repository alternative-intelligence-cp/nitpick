; ModuleID = 'tests/feature_validation/check_string_support.aria'
source_filename = "tests/feature_validation/check_string_support.aria"

@str = private unnamed_addr constant [14 x i8] c"Hello, World!\00", align 1
@str.1 = private unnamed_addr constant [5 x i8] c"Aria\00", align 1

define i32 @main() {
entry:
  %msg = alloca ptr, align 8
  store ptr @str, ptr %msg, align 8
  %name = alloca ptr, align 8
  store ptr @str.1, ptr %name, align 8
  ret i32 42
}

define i32 @__check_string_support_init() {
entry:
  ret i32 0
}
