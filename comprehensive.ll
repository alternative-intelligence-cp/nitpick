; ModuleID = 'tests/feature_validation/comprehensive_break_continue.aria'
source_filename = "tests/feature_validation/comprehensive_break_continue.aria"

define i32 @test_simple_break() {
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

define i32 @test_simple_continue() {
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

define i32 @test_conditional_break() {
entry:
  %total = alloca i32, align 4
  store i32 0, ptr %total, align 4
  %i = alloca i32, align 4
  store i32 0, ptr %i, align 4
  br label %whilecond

whilecond:                                        ; preds = %ifcont, %entry
  %i1 = load i32, ptr %i, align 4
  %lttmp = icmp slt i32 %i1, 20
  %whilecond2 = icmp ne i1 %lttmp, false
  br i1 %whilecond2, label %whilebody, label %afterwhile

whilebody:                                        ; preds = %whilecond
  %i3 = load i32, ptr %i, align 4
  %addtmp = add i32 %i3, 1
  store i32 %addtmp, ptr %i, align 4
  %total4 = load i32, ptr %total, align 4
  %i5 = load i32, ptr %i, align 4
  %addtmp6 = add i32 %total4, %i5
  store i32 %addtmp6, ptr %total, align 4
  %total7 = load i32, ptr %total, align 4
  %gttmp = icmp sgt i32 %total7, 50
  %ifcond = icmp ne i1 %gttmp, false
  br i1 %ifcond, label %then, label %ifcont

then:                                             ; preds = %whilebody
  br label %afterwhile

afterbreak:                                       ; No predecessors!
  br label %ifcont

ifcont:                                           ; preds = %afterbreak, %whilebody
  br label %whilecond

afterwhile:                                       ; preds = %then, %whilecond
  %total8 = load i32, ptr %total, align 4
  ret i32 %total8
}

define i32 @test_multiple_continues() {
entry:
  %count = alloca i32, align 4
  store i32 0, ptr %count, align 4
  %i = alloca i32, align 4
  store i32 0, ptr %i, align 4
  br label %whilecond

whilecond:                                        ; preds = %ifcont10, %then8, %then, %entry
  %i1 = load i32, ptr %i, align 4
  %lttmp = icmp slt i32 %i1, 20
  %whilecond2 = icmp ne i1 %lttmp, false
  br i1 %whilecond2, label %whilebody, label %afterwhile

whilebody:                                        ; preds = %whilecond
  %i3 = load i32, ptr %i, align 4
  %addtmp = add i32 %i3, 1
  store i32 %addtmp, ptr %i, align 4
  %i4 = load i32, ptr %i, align 4
  %lttmp5 = icmp slt i32 %i4, 5
  %ifcond = icmp ne i1 %lttmp5, false
  br i1 %ifcond, label %then, label %ifcont

then:                                             ; preds = %whilebody
  br label %whilecond

aftercontinue:                                    ; No predecessors!
  br label %ifcont

ifcont:                                           ; preds = %aftercontinue, %whilebody
  %i6 = load i32, ptr %i, align 4
  %gttmp = icmp sgt i32 %i6, 15
  %ifcond7 = icmp ne i1 %gttmp, false
  br i1 %ifcond7, label %then8, label %ifcont10

then8:                                            ; preds = %ifcont
  br label %whilecond

aftercontinue9:                                   ; No predecessors!
  br label %ifcont10

ifcont10:                                         ; preds = %aftercontinue9, %ifcont
  %count11 = load i32, ptr %count, align 4
  %addtmp12 = add i32 %count11, 1
  store i32 %addtmp12, ptr %count, align 4
  br label %whilecond

afterwhile:                                       ; preds = %whilecond
  %count13 = load i32, ptr %count, align 4
  ret i32 %count13
}

define i32 @test_immediate_break() {
entry:
  %executed = alloca i32, align 4
  store i32 0, ptr %executed, align 4
  br label %whilecond

whilecond:                                        ; preds = %afterbreak, %entry
  br i1 true, label %whilebody, label %afterwhile

whilebody:                                        ; preds = %whilecond
  br label %afterwhile

afterbreak:                                       ; No predecessors!
  store i32 1, ptr %executed, align 4
  br label %whilecond

afterwhile:                                       ; preds = %whilebody, %whilecond
  %executed1 = load i32, ptr %executed, align 4
  ret i32 %executed1
}

define i32 @test_continue_at_end() {
entry:
  %count = alloca i32, align 4
  store i32 0, ptr %count, align 4
  %i = alloca i32, align 4
  store i32 0, ptr %i, align 4
  br label %whilecond

whilecond:                                        ; preds = %aftercontinue, %whilebody, %entry
  %i1 = load i32, ptr %i, align 4
  %lttmp = icmp slt i32 %i1, 5
  %whilecond2 = icmp ne i1 %lttmp, false
  br i1 %whilecond2, label %whilebody, label %afterwhile

whilebody:                                        ; preds = %whilecond
  %i3 = load i32, ptr %i, align 4
  %addtmp = add i32 %i3, 1
  store i32 %addtmp, ptr %i, align 4
  %count4 = load i32, ptr %count, align 4
  %addtmp5 = add i32 %count4, 1
  store i32 %addtmp5, ptr %count, align 4
  br label %whilecond

aftercontinue:                                    ; No predecessors!
  br label %whilecond

afterwhile:                                       ; preds = %whilecond
  %count6 = load i32, ptr %count, align 4
  ret i32 %count6
}

define i32 @main() {
entry:
  %r1 = alloca i32, align 4
  %0 = call i32 @test_simple_break()
  store i32 %0, ptr %r1, align 4
  %r2 = alloca i32, align 4
  %1 = call i32 @test_simple_continue()
  store i32 %1, ptr %r2, align 4
  %r3 = alloca i32, align 4
  %2 = call i32 @test_conditional_break()
  store i32 %2, ptr %r3, align 4
  %r4 = alloca i32, align 4
  %3 = call i32 @test_multiple_continues()
  store i32 %3, ptr %r4, align 4
  %r5 = alloca i32, align 4
  %4 = call i32 @test_immediate_break()
  store i32 %4, ptr %r5, align 4
  %r6 = alloca i32, align 4
  %5 = call i32 @test_continue_at_end()
  store i32 %5, ptr %r6, align 4
  %total = alloca i32, align 4
  %r11 = load i32, ptr %r1, align 4
  %r22 = load i32, ptr %r2, align 4
  %addtmp = add i32 %r11, %r22
  store i32 %addtmp, ptr %total, align 4
  %total3 = load i32, ptr %total, align 4
  %r34 = load i32, ptr %r3, align 4
  %addtmp5 = add i32 %total3, %r34
  store i32 %addtmp5, ptr %total, align 4
  %total6 = load i32, ptr %total, align 4
  %r47 = load i32, ptr %r4, align 4
  %addtmp8 = add i32 %total6, %r47
  store i32 %addtmp8, ptr %total, align 4
  %total9 = load i32, ptr %total, align 4
  %r510 = load i32, ptr %r5, align 4
  %addtmp11 = add i32 %total9, %r510
  store i32 %addtmp11, ptr %total, align 4
  %total12 = load i32, ptr %total, align 4
  %r613 = load i32, ptr %r6, align 4
  %addtmp14 = add i32 %total12, %r613
  store i32 %addtmp14, ptr %total, align 4
  %total15 = load i32, ptr %total, align 4
  ret i32 %total15
}

define i32 @__comprehensive_break_continue_init() {
entry:
  ret i32 0
}
