; ModuleID = 'tests/feature_validation/check_defer.aria'
source_filename = "tests/feature_validation/check_defer.aria"

define i32 @main() {
entry:
  %counter = alloca i32, align 4
  store i32 0, ptr %counter, align 4
  %counter1 = load i32, ptr %counter, align 4
  %addtmp = add i32 %counter1, 1
  store i32 %addtmp, ptr %counter, align 4
  %counter2 = load i32, ptr %counter, align 4
  %addtmp3 = add i32 %counter2, 1
  store i32 %addtmp3, ptr %counter, align 4
  %counter4 = load i32, ptr %counter, align 4
  %netmp = icmp ne i32 %counter4, 12
  %ifcond = icmp ne i1 %netmp, false
  br i1 %ifcond, label %then, label %ifcont

then:                                             ; preds = %entry
  %counter5 = load i32, ptr %counter, align 4
  ret i32 %counter5

ifcont:                                           ; preds = %entry
  store i32 0, ptr %counter, align 4
  %counter6 = load i32, ptr %counter, align 4
  %netmp7 = icmp ne i32 %counter6, 111
  %ifcond8 = icmp ne i1 %netmp7, false
  br i1 %ifcond8, label %then9, label %ifcont10

then9:                                            ; preds = %ifcont
  ret i32 2

ifcont10:                                         ; preds = %ifcont
  store i32 0, ptr %counter, align 4
  %counter11 = load i32, ptr %counter, align 4
  %addtmp12 = add i32 %counter11, 1
  store i32 %addtmp12, ptr %counter, align 4
  %counter13 = load i32, ptr %counter, align 4
  %addtmp14 = add i32 %counter13, 1
  store i32 %addtmp14, ptr %counter, align 4
  %counter15 = load i32, ptr %counter, align 4
  %addtmp16 = add i32 %counter15, 1
  store i32 %addtmp16, ptr %counter, align 4
  %counter17 = load i32, ptr %counter, align 4
  %addtmp18 = add i32 %counter17, 1
  store i32 %addtmp18, ptr %counter, align 4
  %counter19 = load i32, ptr %counter, align 4
  %netmp20 = icmp ne i32 %counter19, 114
  %ifcond21 = icmp ne i1 %netmp20, false
  br i1 %ifcond21, label %then22, label %ifcont23

then22:                                           ; preds = %ifcont10
  ret i32 3

ifcont23:                                         ; preds = %ifcont10
  store i32 0, ptr %counter, align 4
  br i1 true, label %then24, label %ifcont27

then24:                                           ; preds = %ifcont23
  br i1 true, label %then25, label %ifcont26

then25:                                           ; preds = %then24
  ret i32 42

ifcont26:                                         ; preds = %then24
  br label %ifcont27

ifcont27:                                         ; preds = %ifcont26, %ifcont23
  ret i32 99
}

define i32 @__check_defer_init() {
entry:
  ret i32 0
}
