; ModuleID = 'tests/feature_validation/test_break_continue.aria'
source_filename = "tests/feature_validation/test_break_continue.aria"

define i32 @test_break() {
entry:
  %count = alloca i32, align 4
  store i32 0, ptr %count, align 4
  br label %whilecond

whilecond:                                        ; preds = %ifcont, %entry
  %count1 = load i32, ptr %count, align 4
  %lttmp = icmp slt i32 %count1, 100
  %whilecond2 = icmp ne i1 %lttmp, false
  br i1 %whilecond2, label %whilebody, label %afterwhile

whilebody:                                        ; preds = %whilecond
  %count3 = load i32, ptr %count, align 4
  %addtmp = add i32 %count3, 1
  store i32 %addtmp, ptr %count, align 4
  %count4 = load i32, ptr %count, align 4
  %eqtmp = icmp eq i32 %count4, 10
  %ifcond = icmp ne i1 %eqtmp, false
  br i1 %ifcond, label %then, label %ifcont

then:                                             ; preds = %whilebody
  br label %afterwhile

afterbreak:                                       ; No predecessors!
  br label %ifcont

ifcont:                                           ; preds = %afterbreak, %whilebody
  br label %whilecond

afterwhile:                                       ; preds = %then, %whilecond
  %count5 = load i32, ptr %count, align 4
  ret i32 %count5
}

define i32 @test_continue() {
entry:
  %sum = alloca i32, align 4
  store i32 0, ptr %sum, align 4
  %i = alloca i32, align 4
  store i32 0, ptr %i, align 4
  br label %whilecond

whilecond:                                        ; preds = %ifcont, %then, %entry
  %i1 = load i32, ptr %i, align 4
  %lttmp = icmp slt i32 %i1, 10
  %whilecond2 = icmp ne i1 %lttmp, false
  br i1 %whilecond2, label %whilebody, label %afterwhile

whilebody:                                        ; preds = %whilecond
  %i3 = load i32, ptr %i, align 4
  %addtmp = add i32 %i3, 1
  store i32 %addtmp, ptr %i, align 4
  %i4 = load i32, ptr %i, align 4
  %modtmp = srem i32 %i4, 2
  %eqtmp = icmp eq i32 %modtmp, 0
  %ifcond = icmp ne i1 %eqtmp, false
  br i1 %ifcond, label %then, label %ifcont

then:                                             ; preds = %whilebody
  br label %whilecond

aftercontinue:                                    ; No predecessors!
  br label %ifcont

ifcont:                                           ; preds = %aftercontinue, %whilebody
  %sum5 = load i32, ptr %sum, align 4
  %i6 = load i32, ptr %i, align 4
  %addtmp7 = add i32 %sum5, %i6
  store i32 %addtmp7, ptr %sum, align 4
  br label %whilecond

afterwhile:                                       ; preds = %whilecond
  %sum8 = load i32, ptr %sum, align 4
  ret i32 %sum8
}

define i32 @__test_break_continue_init() {
entry:
  ret i32 0
}
