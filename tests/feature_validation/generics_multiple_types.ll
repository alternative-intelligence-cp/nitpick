; ModuleID = 'tests/feature_validation/generics_multiple_types.aria'
source_filename = "tests/feature_validation/generics_multiple_types.aria"

define i32 @_Aria_M_identity_f9e1c08f4291a8df_int32(i32 %value) {
entry:
  ret i32 %value
}

define i8 @_Aria_M_identity_f5a67dc57a8fe232_int8(i8 %value) {
entry:
  ret i8 %value
}

define double @_Aria_M_identity_8a31f1a9c766def3_flt64(double %value) {
entry:
  ret double %value
}

define i32 @main() {
entry:
  %x = alloca i32, align 4
  store i32 42, ptr %x, align 4
  %y = alloca i32, align 4
  %x1 = load i32, ptr %x, align 4
  %0 = call i32 @_Aria_M_identity_f9e1c08f4291a8df_int32(i32 %x1)
  store i32 %0, ptr %y, align 4
  %a = alloca i8, align 1
  store i32 10, ptr %a, align 4
  %b = alloca i8, align 1
  %a2 = load i32, ptr %a, align 4
  %1 = call i8 @_Aria_M_identity_f5a67dc57a8fe232_int8(i32 %a2)
  store i8 %1, ptr %b, align 1
  %f = alloca double, align 8
  store double 3.140000e+00, ptr %f, align 8
  %g = alloca double, align 8
  %f3 = load i32, ptr %f, align 4
  %2 = call double @_Aria_M_identity_8a31f1a9c766def3_flt64(i32 %f3)
  store double %2, ptr %g, align 8
  ret i32 0
}
