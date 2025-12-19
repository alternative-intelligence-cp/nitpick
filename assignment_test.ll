; ModuleID = 'tests/feature_validation/assignment_test.aria'
source_filename = "tests/feature_validation/assignment_test.aria"

define i32 @test_assignment() {
entry:
  %x = alloca i32, align 4
  store i32 10, ptr %x, align 4
  store i32 20, ptr %x, align 4
  store i32 30, ptr %x, align 4
  %x1 = load i32, ptr %x, align 4
  ret i32 %x1
}

define i32 @main() {
entry:
  %0 = call i32 @test_assignment()
  ret i32 %0
}
