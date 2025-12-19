; ModuleID = 'tests/feature_validation/test_current_features.aria'
source_filename = "tests/feature_validation/test_current_features.aria"

define i32 @test_simple() {
entry:
  %x = alloca i32, align 4
  store i32 10, ptr %x, align 4
  %y = alloca i32, align 4
  store i32 20, ptr %y, align 4
  %sum = alloca i32, align 4
  %x1 = load i32, ptr %x, align 4
  %y2 = load i32, ptr %y, align 4
  %addtmp = add i32 %x1, %y2
  store i32 %addtmp, ptr %sum, align 4
  %y3 = load i32, ptr %y, align 4
  %addtmp4 = add i32 %y3, 5
  store i32 %addtmp4, ptr %y, align 4
  %sum5 = load i32, ptr %sum, align 4
  ret i32 %sum5
}

define i32 @test_loops() {
entry:
  %count = alloca i32, align 4
  store i32 0, ptr %count, align 4
  br label %whilecond

whilecond:                                        ; preds = %whilebody, %entry
  %count1 = load i32, ptr %count, align 4
  %lttmp = icmp slt i32 %count1, 10
  %whilecond2 = icmp ne i1 %lttmp, false
  br i1 %whilecond2, label %whilebody, label %afterwhile

whilebody:                                        ; preds = %whilecond
  %count3 = load i32, ptr %count, align 4
  %addtmp = add i32 %count3, 1
  store i32 %addtmp, ptr %count, align 4
  br label %whilecond

afterwhile:                                       ; preds = %whilecond
  %count4 = load i32, ptr %count, align 4
  ret i32 %count4
}

define i32 @__test_current_features_init() {
entry:
  ret i32 0
}
