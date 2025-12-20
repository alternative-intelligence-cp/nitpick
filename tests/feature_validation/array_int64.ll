; ModuleID = 'tests/feature_validation/array_int64.aria'
source_filename = "tests/feature_validation/array_int64.aria"

define i32 @main() {
entry:
  %arr = alloca i32, align 4
  %array.tmp = alloca i32, i64 3, align 4
  %0 = getelementptr i32, ptr %array.tmp, i64 0
  store i32 1, ptr %0, align 4
  %1 = getelementptr i32, ptr %array.tmp, i64 1
  store i32 2, ptr %1, align 4
  %2 = getelementptr i32, ptr %array.tmp, i64 2
  store i32 3, ptr %2, align 4
  store ptr %array.tmp, ptr %arr, align 8
  %result = alloca i32, align 4
  store i32 42, ptr %result, align 4
  %result1 = load i32, ptr %result, align 4
  ret i32 %result1
}

define i32 @__array_int64_init() {
entry:
  ret i32 0
}
