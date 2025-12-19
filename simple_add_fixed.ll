; ModuleID = 'tests/feature_validation/simple_add.aria'
source_filename = "tests/feature_validation/simple_add.aria"

define i32 @add(i32 %a, i32 %b) {
entry:
  %addtmp = add i32 %a, %b
  ret i32 %addtmp
}

define i32 @main() {
entry:
  %0 = call i32 @add(i32 10, i32 5)
  ret i32 %0
}

define i32 @__simple_add_init() {
entry:
  ret i32 0
}
