; ModuleID = 'tests/int512_basic.aria'
source_filename = "tests/int512_basic.aria"

define i32 @main() {
entry:
  %a = alloca i512, align 8
  store i512 1000000000000000, ptr %a, align 4
  %b = alloca i512, align 8
  store i512 2000000000000000, ptr %b, align 4
  %c = alloca i512, align 8
  store i512 -5000000000000000, ptr %c, align 4
  %sum = alloca i512, align 8
  %a1 = load i512, ptr %a, align 4
  %b2 = load i512, ptr %b, align 4
  %addtmp = add i512 %a1, %b2
  store i512 %addtmp, ptr %sum, align 4
  %diff = alloca i512, align 8
  %b3 = load i512, ptr %b, align 4
  %a4 = load i512, ptr %a, align 4
  %subtmp = sub i512 %b3, %a4
  store i512 %subtmp, ptr %diff, align 4
  %prod = alloca i512, align 8
  %a5 = load i512, ptr %a, align 4
  %multmp = mul i512 %a5, i64 3
  store i512 %multmp, ptr %prod, align 4
  %lt = alloca i1, align 1
  %a6 = load i512, ptr %a, align 4
  %b7 = load i512, ptr %b, align 4
  %lttmp = icmp slt i512 %a6, %b7
  store i1 %lttmp, ptr %lt, align 1
  %eq = alloca i1, align 1
  %a8 = load i512, ptr %a, align 4
  %a9 = load i512, ptr %a, align 4
  %eqtmp = icmp eq i512 %a8, %a9
  store i1 %eqtmp, ptr %eq, align 1
  %neg_lt = alloca i1, align 1
  %c10 = load i512, ptr %c, align 4
  %a11 = load i512, ptr %a, align 4
  %lttmp12 = icmp slt i512 %c10, %a11
  store i1 %lttmp12, ptr %neg_lt, align 1
  %expected_sum = alloca i512, align 8
  store i512 3000000000000000, ptr %expected_sum, align 4
  %expected_prod = alloca i512, align 8
  store i512 3000000000000000, ptr %expected_prod, align 4
  %sum13 = load i512, ptr %sum, align 4
  %expected_sum14 = load i512, ptr %expected_sum, align 4
  %netmp = icmp ne i512 %sum13, %expected_sum14
  %ifcond = icmp ne i1 %netmp, false
  br i1 %ifcond, label %then, label %ifcont

then:                                             ; preds = %entry
  %one = alloca i32, align 4
  store i32 1, ptr %one, align 4
  %one15 = load i32, ptr %one, align 4
  ret i32 %one15

ifcont:                                           ; preds = %entry
  %prod16 = load i512, ptr %prod, align 4
  %expected_prod17 = load i512, ptr %expected_prod, align 4
  %netmp18 = icmp ne i512 %prod16, %expected_prod17
  %ifcond19 = icmp ne i1 %netmp18, false
  br i1 %ifcond19, label %then20, label %ifcont23

then20:                                           ; preds = %ifcont
  %one21 = alloca i32, align 4
  store i32 1, ptr %one21, align 4
  %one22 = load i32, ptr %one21, align 4
  ret i32 %one22

ifcont23:                                         ; preds = %ifcont
  %lt24 = load i1, ptr %lt, align 1
  %tobool = icmp ne i1 %lt24, false
  %nottmp = xor i1 %tobool, true
  %ifcond25 = icmp ne i1 %nottmp, false
  br i1 %ifcond25, label %then26, label %ifcont29

then26:                                           ; preds = %ifcont23
  %one27 = alloca i32, align 4
  store i32 1, ptr %one27, align 4
  %one28 = load i32, ptr %one27, align 4
  ret i32 %one28

ifcont29:                                         ; preds = %ifcont23
  %zero = alloca i32, align 4
  store i32 0, ptr %zero, align 4
  %zero30 = load i32, ptr %zero, align 4
  ret i32 %zero30
}

define i32 @__int512_basic_init() {
entry:
  ret i32 0
}
