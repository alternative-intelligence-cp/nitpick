; ModuleID = 'tests/feature_validation/array_indexing.aria'
source_filename = "tests/feature_validation/array_indexing.aria"

define i32 @main() {
entry:
  %arr = alloca i32, align 4
  %array.tmp = alloca i32, i64 3, align 4
  %0 = getelementptr i32, ptr %array.tmp, i64 0
  store i32 10, ptr %0, align 4
  %1 = getelementptr i32, ptr %array.tmp, i64 1
  store i32 20, ptr %1, align 4
  %2 = getelementptr i32, ptr %array.tmp, i64 2
  store i32 30, ptr %2, align 4
  store ptr %array.tmp, ptr %arr, align 8
  %first = alloca i32, align 4
  %arrayidx = getelementptr i64, ptr %arr, i32 0
  %elem = load i64, ptr %arrayidx, align 4
  store i64 %elem, ptr %first, align 4
  %second = alloca i32, align 4
  %arrayidx1 = getelementptr i64, ptr %arr, i32 1
  %elem2 = load i64, ptr %arrayidx1, align 4
  store i64 %elem2, ptr %second, align 4
  %third = alloca i32, align 4
  %arrayidx3 = getelementptr i64, ptr %arr, i32 2
  %elem4 = load i64, ptr %arrayidx3, align 4
  store i64 %elem4, ptr %third, align 4
  %ten = alloca i32, align 4
  store i32 10, ptr %ten, align 4
  %first5 = load i32, ptr %first, align 4
  %ten6 = load i32, ptr %ten, align 4
  %eqtmp = icmp eq i32 %first5, %ten6
  %ifcond = icmp ne i1 %eqtmp, false
  br i1 %ifcond, label %then, label %ifcont

then:                                             ; preds = %entry
  %success = alloca i32, align 4
  store i32 42, ptr %success, align 4
  %success7 = load i32, ptr %success, align 4
  ret i32 %success7

ifcont:                                           ; preds = %entry
  %fail_code = alloca i32, align 4
  store i32 1, ptr %fail_code, align 4
  %fail_code8 = load i32, ptr %fail_code, align 4
  ret i32 %fail_code8
}

define i32 @__array_indexing_init() {
entry:
  ret i32 0
}
