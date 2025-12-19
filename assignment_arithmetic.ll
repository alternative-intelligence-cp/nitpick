; ModuleID = 'tests/feature_validation/assignment_arithmetic.aria'
source_filename = "tests/feature_validation/assignment_arithmetic.aria"

define i32 @counter() {
entry:
  %count = alloca i32, align 4
  store i32 0, ptr %count, align 4
  %count1 = load i32, ptr %count, align 4
  %addtmp = add i32 %count1, 1
  store i32 %addtmp, ptr %count, align 4
  %count2 = load i32, ptr %count, align 4
  %addtmp3 = add i32 %count2, 2
  store i32 %addtmp3, ptr %count, align 4
  %count4 = load i32, ptr %count, align 4
  %addtmp5 = add i32 %count4, 3
  store i32 %addtmp5, ptr %count, align 4
  %count6 = load i32, ptr %count, align 4
  ret i32 %count6
}

define i32 @main() {
entry:
  %0 = call i32 @counter()
  ret i32 %0
}

define i32 @__assignment_arithmetic_init() {
entry:
  ret i32 0
}
