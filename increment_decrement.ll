; ModuleID = 'tests/feature_validation/increment_decrement.aria'
source_filename = "tests/feature_validation/increment_decrement.aria"

define i32 @test_increment() {
entry:
  %x = alloca i32, align 4
  store i32 5, ptr %x, align 4
  %x1 = load i32, ptr %x, align 4
  %addtmp = add i32 %x1, 1
  store i32 %addtmp, ptr %x, align 4
  %x2 = load i32, ptr %x, align 4
  %addtmp3 = add i32 %x2, 1
  store i32 %addtmp3, ptr %x, align 4
  %x4 = load i32, ptr %x, align 4
  %addtmp5 = add i32 %x4, 1
  store i32 %addtmp5, ptr %x, align 4
  %x6 = load i32, ptr %x, align 4
  ret i32 %x6
}

define i32 @test_decrement() {
entry:
  %y = alloca i32, align 4
  store i32 10, ptr %y, align 4
  %y1 = load i32, ptr %y, align 4
  %subtmp = sub i32 %y1, 1
  store i32 %subtmp, ptr %y, align 4
  %y2 = load i32, ptr %y, align 4
  %subtmp3 = sub i32 %y2, 1
  store i32 %subtmp3, ptr %y, align 4
  %y4 = load i32, ptr %y, align 4
  %subtmp5 = sub i32 %y4, 1
  store i32 %subtmp5, ptr %y, align 4
  %y6 = load i32, ptr %y, align 4
  ret i32 %y6
}

define i32 @test_mixed() {
entry:
  %z = alloca i32, align 4
  store i32 0, ptr %z, align 4
  %z1 = load i32, ptr %z, align 4
  %addtmp = add i32 %z1, 1
  store i32 %addtmp, ptr %z, align 4
  %z2 = load i32, ptr %z, align 4
  %addtmp3 = add i32 %z2, 1
  store i32 %addtmp3, ptr %z, align 4
  %z4 = load i32, ptr %z, align 4
  %addtmp5 = add i32 %z4, 1
  store i32 %addtmp5, ptr %z, align 4
  %z6 = load i32, ptr %z, align 4
  %subtmp = sub i32 %z6, 1
  store i32 %subtmp, ptr %z, align 4
  %z7 = load i32, ptr %z, align 4
  %addtmp8 = add i32 %z7, 1
  store i32 %addtmp8, ptr %z, align 4
  %z9 = load i32, ptr %z, align 4
  %addtmp10 = add i32 %z9, 1
  store i32 %addtmp10, ptr %z, align 4
  %z11 = load i32, ptr %z, align 4
  ret i32 %z11
}

define i32 @main() {
entry:
  %a = alloca i32, align 4
  %0 = call i32 @test_increment()
  store i32 %0, ptr %a, align 4
  %b = alloca i32, align 4
  %1 = call i32 @test_decrement()
  store i32 %1, ptr %b, align 4
  %c = alloca i32, align 4
  %2 = call i32 @test_mixed()
  store i32 %2, ptr %c, align 4
  %a1 = load i32, ptr %a, align 4
  %b2 = load i32, ptr %b, align 4
  %addtmp = add i32 %a1, %b2
  %c3 = load i32, ptr %c, align 4
  %addtmp4 = add i32 %addtmp, %c3
  ret i32 %addtmp4
}

define i32 @__increment_decrement_init() {
entry:
  ret i32 0
}
