; ModuleID = 'tests/int8_simple.aria'
source_filename = "tests/int8_simple.aria"

define internal i32 @test_int8() {
entry:
  %a = alloca i8, align 1
  store i8 10, ptr %a, align 1
  %b = alloca i8, align 1
  store i8 20, ptr %b, align 1
  %sum = alloca i8, align 1
  %a1 = load i8, ptr %a, align 1
  %b2 = load i8, ptr %b, align 1
  %addtmp = add i8 %a1, %b2
  store i8 %addtmp, ptr %sum, align 1
  %diff = alloca i8, align 1
  %b3 = load i8, ptr %b, align 1
  %a4 = load i8, ptr %a, align 1
  %subtmp = sub i8 %b3, %a4
  store i8 %subtmp, ptr %diff, align 1
  %a5 = load i8, ptr %a, align 1
  %b6 = load i8, ptr %b, align 1
  %gttmp = icmp sgt i8 %a5, %b6
  %ifcond = icmp ne i1 %gttmp, false
  br i1 %ifcond, label %then, label %ifcont

then:                                             ; preds = %entry
  %err = alloca i32, align 4
  store i32 1, ptr %err, align 4
  %err7 = load i32, ptr %err, align 4
  ret i32 %err7

ifcont:                                           ; preds = %entry
  %b8 = load i8, ptr %b, align 1
  %a9 = load i8, ptr %a, align 1
  %lttmp = icmp slt i8 %b8, %a9
  %ifcond10 = icmp ne i1 %lttmp, false
  br i1 %ifcond10, label %then11, label %ifcont14

then11:                                           ; preds = %ifcont
  %err12 = alloca i32, align 4
  store i32 2, ptr %err12, align 4
  %err13 = load i32, ptr %err12, align 4
  ret i32 %err13

ifcont14:                                         ; preds = %ifcont
  %ok = alloca i32, align 4
  store i32 0, ptr %ok, align 4
  %ok15 = load i32, ptr %ok, align 4
  ret i32 %ok15
}

define internal i32 @test_uint8() {
entry:
  %x = alloca i8, align 1
  store i8 100, ptr %x, align 1
  %y = alloca i8, align 1
  store i8 50, ptr %y, align 1
  %sum = alloca i8, align 1
  %x1 = load i8, ptr %x, align 1
  %y2 = load i8, ptr %y, align 1
  %addtmp = add i8 %x1, %y2
  store i8 %addtmp, ptr %sum, align 1
  %diff = alloca i8, align 1
  %x3 = load i8, ptr %x, align 1
  %y4 = load i8, ptr %y, align 1
  %subtmp = sub i8 %x3, %y4
  store i8 %subtmp, ptr %diff, align 1
  %x5 = load i8, ptr %x, align 1
  %y6 = load i8, ptr %y, align 1
  %lttmp = icmp slt i8 %x5, %y6
  %ifcond = icmp ne i1 %lttmp, false
  br i1 %ifcond, label %then, label %ifcont

then:                                             ; preds = %entry
  %err = alloca i32, align 4
  store i32 1, ptr %err, align 4
  %err7 = load i32, ptr %err, align 4
  ret i32 %err7

ifcont:                                           ; preds = %entry
  %y8 = load i8, ptr %y, align 1
  %x9 = load i8, ptr %x, align 1
  %gttmp = icmp sgt i8 %y8, %x9
  %ifcond10 = icmp ne i1 %gttmp, false
  br i1 %ifcond10, label %then11, label %ifcont14

then11:                                           ; preds = %ifcont
  %err12 = alloca i32, align 4
  store i32 2, ptr %err12, align 4
  %err13 = load i32, ptr %err12, align 4
  ret i32 %err13

ifcont14:                                         ; preds = %ifcont
  %ok = alloca i32, align 4
  store i32 0, ptr %ok, align 4
  %ok15 = load i32, ptr %ok, align 4
  ret i32 %ok15
}

define i32 @main() {
entry:
  %result1 = alloca i32, align 4
  %0 = call i32 @test_int8()
  store i32 %0, ptr %result1, align 4
  %result11 = load i32, ptr %result1, align 4
  %cmp_cast = sext i32 %result11 to i64
  %netmp = icmp ne i64 %cmp_cast, 0
  %ifcond = icmp ne i1 %netmp, false
  br i1 %ifcond, label %then, label %ifcont

then:                                             ; preds = %entry
  %result12 = load i32, ptr %result1, align 4
  ret i32 %result12

ifcont:                                           ; preds = %entry
  %result2 = alloca i32, align 4
  %1 = call i32 @test_uint8()
  store i32 %1, ptr %result2, align 4
  %result23 = load i32, ptr %result2, align 4
  %cmp_cast4 = sext i32 %result23 to i64
  %netmp5 = icmp ne i64 %cmp_cast4, 0
  %ifcond6 = icmp ne i1 %netmp5, false
  br i1 %ifcond6, label %then7, label %ifcont9

then7:                                            ; preds = %ifcont
  %result28 = load i32, ptr %result2, align 4
  ret i32 %result28

ifcont9:                                          ; preds = %ifcont
  %success = alloca i32, align 4
  store i32 0, ptr %success, align 4
  %success10 = load i32, ptr %success, align 4
  ret i32 %success10
}

define i32 @__int8_simple_init() {
entry:
  ret i32 0
}
