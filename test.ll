; ModuleID = 'tests/ternary_minimal.aria'
source_filename = "tests/ternary_minimal.aria"

define i64 @main() {
entry:
  %zero = alloca i2, align 1
  store i32 0, ptr %zero, align 4
  %positive = alloca i2, align 1
  store i32 1, ptr %positive, align 4
  %negative = alloca i2, align 1
  store i32 -1, ptr %negative, align 4
  %zero1 = load i2, ptr %zero, align 1
  %cmp_cast = sext i2 %zero1 to i32
  %eqtmp = icmp eq i32 %cmp_cast, 0
  %ifcond = icmp ne i1 %eqtmp, false
  br i1 %ifcond, label %then, label %ifcont

then:                                             ; preds = %entry
  ret i32 42

ifcont:                                           ; preds = %entry
  ret i32 1
}

define i32 @__ternary_minimal_init() {
entry:
  ret i32 0
}
