; ModuleID = 'tests/feature_validation/compound_assignment.aria'
source_filename = "tests/feature_validation/compound_assignment.aria"

define i32 @test_compound() {
entry:
  %x = alloca i32, align 4
  store i32 10, ptr %x, align 4
  %x1 = load i32, ptr %x, align 4
  %addtmp = add i32 %x1, 5
  store i32 %addtmp, ptr %x, align 4
  %x2 = load i32, ptr %x, align 4
  %multmp = mul i32 %x2, 2
  store i32 %multmp, ptr %x, align 4
  %x3 = load i32, ptr %x, align 4
  %subtmp = sub i32 %x3, 10
  store i32 %subtmp, ptr %x, align 4
  %x4 = load i32, ptr %x, align 4
  ret i32 %x4
}

define i32 @test_compound_ops() {
entry:
  %y = alloca i32, align 4
  store i32 100, ptr %y, align 4
  %y1 = load i32, ptr %y, align 4
  %addtmp = add i32 %y1, 50
  store i32 %addtmp, ptr %y, align 4
  %y2 = load i32, ptr %y, align 4
  %subtmp = sub i32 %y2, 30
  store i32 %subtmp, ptr %y, align 4
  %y3 = load i32, ptr %y, align 4
  %multmp = mul i32 %y3, 2
  store i32 %multmp, ptr %y, align 4
  %y4 = load i32, ptr %y, align 4
  %divtmp = sdiv i32 %y4, 4
  store i32 %divtmp, ptr %y, align 4
  %y5 = load i32, ptr %y, align 4
  %modtmp = srem i32 %y5, 7
  store i32 %modtmp, ptr %y, align 4
  %y6 = load i32, ptr %y, align 4
  ret i32 %y6
}

define i32 @main() {
entry:
  %a = alloca i32, align 4
  %0 = call i32 @test_compound()
  store i32 %0, ptr %a, align 4
  %b = alloca i32, align 4
  %1 = call i32 @test_compound_ops()
  store i32 %1, ptr %b, align 4
  %a1 = load i32, ptr %a, align 4
  %b2 = load i32, ptr %b, align 4
  %addtmp = add i32 %a1, %b2
  ret i32 %addtmp
}

define i32 @__compound_assignment_init() {
entry:
  ret i32 0
}
