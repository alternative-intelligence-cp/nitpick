; ModuleID = 'tests/ternary_test_var.aria'
source_filename = "tests/ternary_test_var.aria"

define i64 @main() {
entry:
  %x = alloca i2, align 1
  store i32 0, ptr %x, align 4
  ret i32 42
}
