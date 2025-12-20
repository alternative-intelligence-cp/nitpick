; ModuleID = 'tests/feature_validation/int64_test.aria'
source_filename = "tests/feature_validation/int64_test.aria"

define internal i64 @test() {
entry:
  %x = alloca i64, align 8
  store i32 5, ptr %x, align 4
  %x1 = load i32, ptr %x, align 4
  ret i32 %x1
}

define i64 @main() {
entry:
  %result = alloca i64, align 8
  %0 = call i64 @test()
  store i64 %0, ptr %result, align 4
  %result1 = load i32, ptr %result, align 4
  ret i32 %result1
}
