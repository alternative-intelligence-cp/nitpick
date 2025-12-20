; ModuleID = 'tests/feature_validation/modules_complete.aria'
source_filename = "tests/feature_validation/modules_complete.aria"

define i64 @math.add(i64 %a, i64 %b) {
entry:
  %addtmp = add i64 %a, %b
  ret i64 %addtmp
}

define i64 @main() {
entry:
  ret i32 42
}

define i32 @__modules_complete_init() {
entry:
  ret i32 0
}
