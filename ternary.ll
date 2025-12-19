; ModuleID = 'tests/feature_validation/manual_ternary_pattern.aria'
source_filename = "tests/feature_validation/manual_ternary_pattern.aria"

define i32 @test_manual_ternary() {
entry:
  %x = alloca i32, align 4
  store i32 10, ptr %x, align 4
  %y = alloca i32, align 4
  store i32 20, ptr %y, align 4
  %max = alloca i32, align 4
  store i32 0, ptr %max, align 4
  %x1 = load i32, ptr %x, align 4
  %y2 = load i32, ptr %y, align 4
  %gttmp = icmp sgt i32 %x1, %y2
  %ifcond = icmp ne i1 %gttmp, false
  br i1 %ifcond, label %then, label %else

then:                                             ; preds = %entry
  %x3 = load i32, ptr %x, align 4
  store i32 %x3, ptr %max, align 4
  br label %ifcont

else:                                             ; preds = %entry
  %y4 = load i32, ptr %y, align 4
  store i32 %y4, ptr %max, align 4
  br label %ifcont

ifcont:                                           ; preds = %else, %then
  %max5 = load i32, ptr %max, align 4
  ret i32 %max5
}

define i32 @test_ternary_with_computation() {
entry:
  %a = alloca i32, align 4
  store i32 5, ptr %a, align 4
  %b = alloca i32, align 4
  store i32 10, ptr %b, align 4
  %value = alloca i32, align 4
  store i32 0, ptr %value, align 4
  %a1 = load i32, ptr %a, align 4
  %b2 = load i32, ptr %b, align 4
  %lttmp = icmp slt i32 %a1, %b2
  %ifcond = icmp ne i1 %lttmp, false
  br i1 %ifcond, label %then, label %else

then:                                             ; preds = %entry
  %a3 = load i32, ptr %a, align 4
  %b4 = load i32, ptr %b, align 4
  %addtmp = add i32 %a3, %b4
  store i32 %addtmp, ptr %value, align 4
  br label %ifcont

else:                                             ; preds = %entry
  %a5 = load i32, ptr %a, align 4
  %b6 = load i32, ptr %b, align 4
  %subtmp = sub i32 %a5, %b6
  store i32 %subtmp, ptr %value, align 4
  br label %ifcont

ifcont:                                           ; preds = %else, %then
  %value7 = load i32, ptr %value, align 4
  ret i32 %value7
}

define i32 @test_nested_conditions() {
entry:
  %score = alloca i32, align 4
  store i32 85, ptr %score, align 4
  %grade = alloca i32, align 4
  store i32 0, ptr %grade, align 4
  %score1 = load i32, ptr %score, align 4
  %gttmp = icmp sgt i32 %score1, 90
  %ifcond = icmp ne i1 %gttmp, false
  br i1 %ifcond, label %then, label %else

then:                                             ; preds = %entry
  store i32 1, ptr %grade, align 4
  br label %ifcont7

else:                                             ; preds = %entry
  %score2 = load i32, ptr %score, align 4
  %gttmp3 = icmp sgt i32 %score2, 80
  %ifcond4 = icmp ne i1 %gttmp3, false
  br i1 %ifcond4, label %then5, label %else6

then5:                                            ; preds = %else
  store i32 2, ptr %grade, align 4
  br label %ifcont

else6:                                            ; preds = %else
  store i32 3, ptr %grade, align 4
  br label %ifcont

ifcont:                                           ; preds = %else6, %then5
  br label %ifcont7

ifcont7:                                          ; preds = %ifcont, %then
  %grade8 = load i32, ptr %grade, align 4
  ret i32 %grade8
}

define i32 @main() {
entry:
  %r1 = alloca i32, align 4
  %0 = call i32 @test_manual_ternary()
  store i32 %0, ptr %r1, align 4
  %r2 = alloca i32, align 4
  %1 = call i32 @test_ternary_with_computation()
  store i32 %1, ptr %r2, align 4
  %r3 = alloca i32, align 4
  %2 = call i32 @test_nested_conditions()
  store i32 %2, ptr %r3, align 4
  %total = alloca i32, align 4
  %r11 = load i32, ptr %r1, align 4
  %r22 = load i32, ptr %r2, align 4
  %addtmp = add i32 %r11, %r22
  %r33 = load i32, ptr %r3, align 4
  %addtmp4 = add i32 %addtmp, %r33
  store i32 %addtmp4, ptr %total, align 4
  %total5 = load i32, ptr %total, align 4
  ret i32 %total5
}

define i32 @__manual_ternary_pattern_init() {
entry:
  ret i32 0
}
