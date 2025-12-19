; ModuleID = 'tests/feature_validation/check_ternary_support.aria'
source_filename = "tests/feature_validation/check_ternary_support.aria"

define i32 @test_ternary() {
entry:
  %x = alloca i32, align 4
  store i32 10, ptr %x, align 4
  %y = alloca i32, align 4
  store i32 20, ptr %y, align 4
  %max = alloca i32, align 4
  %x1 = load i32, ptr %x, align 4
  %y2 = load i32, ptr %y, align 4
  %gttmp = icmp sgt i32 %x1, %y2
  %terncond = icmp ne i1 %gttmp, false
  br i1 %terncond, label %tern.true, label %tern.false

tern.true:                                        ; preds = %entry
  %x3 = load i32, ptr %x, align 4
  br label %tern.cont

tern.false:                                       ; preds = %entry
  %y4 = load i32, ptr %y, align 4
  br label %tern.cont

tern.cont:                                        ; preds = %tern.false, %tern.true
  %ternphi = phi i32 [ %x3, %tern.true ], [ %y4, %tern.false ]
  store i32 %ternphi, ptr %max, align 4
  %max5 = load i32, ptr %max, align 4
  ret i32 %max5
}

define i32 @test_nested_ternary() {
entry:
  %a = alloca i32, align 4
  store i32 5, ptr %a, align 4
  %b = alloca i32, align 4
  store i32 10, ptr %b, align 4
  %c = alloca i32, align 4
  store i32 15, ptr %c, align 4
  %max = alloca i32, align 4
  %a1 = load i32, ptr %a, align 4
  %b2 = load i32, ptr %b, align 4
  %gttmp = icmp sgt i32 %a1, %b2
  %terncond = icmp ne i1 %gttmp, false
  br i1 %terncond, label %tern.true, label %tern.false

tern.true:                                        ; preds = %entry
  %a3 = load i32, ptr %a, align 4
  br label %tern.cont12

tern.false:                                       ; preds = %entry
  %b4 = load i32, ptr %b, align 4
  %c5 = load i32, ptr %c, align 4
  %gttmp6 = icmp sgt i32 %b4, %c5
  %terncond7 = icmp ne i1 %gttmp6, false
  br i1 %terncond7, label %tern.true8, label %tern.false10

tern.true8:                                       ; preds = %tern.false
  %b9 = load i32, ptr %b, align 4
  br label %tern.cont

tern.false10:                                     ; preds = %tern.false
  %c11 = load i32, ptr %c, align 4
  br label %tern.cont

tern.cont:                                        ; preds = %tern.false10, %tern.true8
  %ternphi = phi i32 [ %b9, %tern.true8 ], [ %c11, %tern.false10 ]
  br label %tern.cont12

tern.cont12:                                      ; preds = %tern.cont, %tern.true
  %ternphi13 = phi i32 [ %a3, %tern.true ], [ %ternphi, %tern.cont ]
  store i32 %ternphi13, ptr %max, align 4
  %max14 = load i32, ptr %max, align 4
  ret i32 %max14
}

define i32 @main() {
entry:
  %r1 = alloca i32, align 4
  %0 = call i32 @test_ternary()
  store i32 %0, ptr %r1, align 4
  %r2 = alloca i32, align 4
  %1 = call i32 @test_nested_ternary()
  store i32 %1, ptr %r2, align 4
  %r11 = load i32, ptr %r1, align 4
  %r22 = load i32, ptr %r2, align 4
  %addtmp = add i32 %r11, %r22
  ret i32 %addtmp
}

define i32 @__check_ternary_support_init() {
entry:
  ret i32 0
}
