; ModuleID = 'tests/feature_validation/minimal_defer.aria'
source_filename = "tests/feature_validation/minimal_defer.aria"

define i32 @main() {
entry:
  %x = alloca i32, align 4
  store i32 1, ptr %x, align 4
  ret i32 42
}

define i32 @__minimal_defer_init() {
entry:
  ret i32 0
}
