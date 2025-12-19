; ModuleID = 'tests/feature_validation/defer_scope_test.aria'
source_filename = "tests/feature_validation/defer_scope_test.aria"

define i32 @test() {
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
  %addtmp5 = add i32 %counter4, 10
  store i32 %addtmp5, ptr %counter, align 4
  %counter6 = load i32, ptr %counter, align 4
  ret i32 %counter6
}

define i32 @main() {
entry:
  %value = alloca i32, align 4
  %0 = call i32 @test()
  store i32 %0, ptr %value, align 4
  %value1 = load i32, ptr %value, align 4
  ret i32 %value1
}
