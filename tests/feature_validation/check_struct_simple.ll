; ModuleID = 'tests/feature_validation/check_struct_simple.aria'
source_filename = "tests/feature_validation/check_struct_simple.aria"

%Point = type { i32, i32 }

define i32 @main() {
entry:
  %p = alloca %Point, align 8
  %struct.tmp = alloca %Point, align 8
  %x.ptr = getelementptr inbounds nuw %Point, ptr %struct.tmp, i32 0, i32 0
  store i32 42, ptr %x.ptr, align 4
  %y.ptr = getelementptr inbounds nuw %Point, ptr %struct.tmp, i32 0, i32 1
  store i32 100, ptr %y.ptr, align 4
  %struct.val = load %Point, ptr %struct.tmp, align 4
  store %Point %struct.val, ptr %p, align 4
  %x_val = alloca i32, align 4
  %x.ptr1 = getelementptr inbounds nuw %Point, ptr %p, i32 0, i32 0
  %x = load i32, ptr %x.ptr1, align 4
  store i32 %x, ptr %x_val, align 4
  %y_val = alloca i32, align 4
  %y.ptr2 = getelementptr inbounds nuw %Point, ptr %p, i32 0, i32 1
  %y = load i32, ptr %y.ptr2, align 4
  store i32 %y, ptr %y_val, align 4
  %x_val3 = load i32, ptr %x_val, align 4
  %netmp = icmp ne i32 %x_val3, 42
  %ifcond = icmp ne i1 %netmp, false
  br i1 %ifcond, label %then, label %ifcont

then:                                             ; preds = %entry
  ret i32 1

ifcont:                                           ; preds = %entry
  %y_val4 = load i32, ptr %y_val, align 4
  %netmp5 = icmp ne i32 %y_val4, 100
  %ifcond6 = icmp ne i1 %netmp5, false
  br i1 %ifcond6, label %then7, label %ifcont8

then7:                                            ; preds = %ifcont
  ret i32 2

ifcont8:                                          ; preds = %ifcont
  ret i32 0
}

define i32 @__check_struct_simple_init() {
entry:
  ret i32 0
}
