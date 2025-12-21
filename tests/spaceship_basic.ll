; ModuleID = 'tests/spaceship_basic.aria'
source_filename = "tests/spaceship_basic.aria"

define internal i64 @test_spaceship() {
entry:
  %x = alloca i64, align 8
  store i64 5, ptr %x, align 4
  %y = alloca i64, align 8
  store i64 3, ptr %y, align 4
  %z = alloca i64, align 8
  store i64 5, ptr %z, align 4
  %result1 = alloca i64, align 8
  %x1 = load i64, ptr %x, align 4
  %y2 = load i64, ptr %y, align 4
  %result2 = alloca i64, align 8
  %y3 = load i64, ptr %y, align 4
  %x4 = load i64, ptr %x, align 4
  %result3 = alloca i64, align 8
  %x5 = load i64, ptr %x, align 4
  %z6 = load i64, ptr %z, align 4
  %result17 = load i64, ptr %result1, align 4
  %result28 = load i64, ptr %result2, align 4
  %addtmp = add i64 %result17, %result28
  %result39 = load i64, ptr %result3, align 4
  %addtmp10 = add i64 %addtmp, %result39
  ret i64 %addtmp10
}

define i32 @__spaceship_basic_init() {
entry:
  ret i32 0
}
