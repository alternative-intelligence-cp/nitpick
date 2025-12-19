; ModuleID = 'tests/feature_validation/generics_instantiate.aria'
source_filename = "tests/feature_validation/generics_instantiate.aria"

define i32 @_Aria_M_identity_f9e1c08f4291a8df_int32(i32 %value) {
entry:
  ret i32 %value
}

define i32 @main() {
entry:
  %x = alloca i32, align 4
  store i32 42, ptr %x, align 4
  %y = alloca i32, align 4
  %x1 = load i32, ptr %x, align 4
  %0 = call i32 @_Aria_M_identity_f9e1c08f4291a8df_int32(i32 %x1)
  store i32 %0, ptr %y, align 4
  %y2 = load i32, ptr %y, align 4
  %eqtmp = icmp eq i32 %y2, 42
  %ifcond = icmp ne i1 %eqtmp, false
  br i1 %ifcond, label %then, label %ifcont

then:                                             ; preds = %entry
  ret i32 0

ifcont:                                           ; preds = %entry
  ret i32 1
}
