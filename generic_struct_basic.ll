; ModuleID = 'tests/feature_validation/generic_struct_basic.aria'
source_filename = "tests/feature_validation/generic_struct_basic.aria"

define i64 @main() {
entry:
  %intBox = alloca i32, align 4
  %strBox = alloca i32, align 4
  %okResult = alloca i32, align 4
  %errResult = alloca i32, align 4
  ret i32 0
}

define i32 @__generic_struct_basic_init() {
entry:
  ret i32 0
}
