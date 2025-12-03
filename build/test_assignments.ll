; ModuleID = 'aria_module'
source_filename = "aria_module"

declare void @puts(ptr)

declare void @print(ptr)

define internal void @__aria_module_init() {
entry:
  ret void
}

define internal i8 @test_basic_assign() {
entry:
  %x = alloca i8, align 1
  store i64 5, ptr %x, align 4
  store i64 10, ptr %x, align 4
  %0 = load i8, ptr %x, align 1
  ret i8 %0
}

define internal i8 @test_plus_assign() {
entry:
  %x = alloca i8, align 1
  store i64 5, ptr %x, align 4
  %0 = load i8, ptr %x, align 1
  %1 = sext i8 %0 to i64
  %addtmp = add i64 %1, 3
  store i64 %addtmp, ptr %x, align 4
  %2 = load i8, ptr %x, align 1
  ret i8 %2
}

define internal i8 @test_minus_assign() {
entry:
  %x = alloca i8, align 1
  store i64 10, ptr %x, align 4
  %0 = load i8, ptr %x, align 1
  %1 = sext i8 %0 to i64
  %subtmp = sub i64 %1, 4
  store i64 %subtmp, ptr %x, align 4
  %2 = load i8, ptr %x, align 1
  ret i8 %2
}

define internal i8 @test_mul_assign() {
entry:
  %x = alloca i8, align 1
  store i64 5, ptr %x, align 4
  %0 = load i8, ptr %x, align 1
  %1 = sext i8 %0 to i64
  %multmp = mul i64 %1, 2
  store i64 %multmp, ptr %x, align 4
  %2 = load i8, ptr %x, align 1
  ret i8 %2
}

define internal i8 @test_div_assign() {
entry:
  %x = alloca i8, align 1
  store i64 20, ptr %x, align 4
  %0 = load i8, ptr %x, align 1
  %1 = sext i8 %0 to i64
  %divtmp = sdiv i64 %1, 4
  store i64 %divtmp, ptr %x, align 4
  %2 = load i8, ptr %x, align 1
  ret i8 %2
}

define internal i8 @test_mod_assign() {
entry:
  %x = alloca i8, align 1
  store i64 17, ptr %x, align 4
  %0 = load i8, ptr %x, align 1
  %1 = sext i8 %0 to i64
  %modtmp = srem i64 %1, 5
  store i64 %modtmp, ptr %x, align 4
  %2 = load i8, ptr %x, align 1
  ret i8 %2
}

define internal i8 @__user_main() {
entry:
  %r6 = alloca i8, align 1
  %r5 = alloca i8, align 1
  %r4 = alloca i8, align 1
  %r3 = alloca i8, align 1
  %r2 = alloca i8, align 1
  %r1 = alloca i8, align 1
  %calltmp = call i8 @test_basic_assign()
  store i8 %calltmp, ptr %r1, align 1
  %calltmp1 = call i8 @test_plus_assign()
  store i8 %calltmp1, ptr %r2, align 1
  %calltmp2 = call i8 @test_minus_assign()
  store i8 %calltmp2, ptr %r3, align 1
  %calltmp3 = call i8 @test_mul_assign()
  store i8 %calltmp3, ptr %r4, align 1
  %calltmp4 = call i8 @test_div_assign()
  store i8 %calltmp4, ptr %r5, align 1
  %calltmp5 = call i8 @test_mod_assign()
  store i8 %calltmp5, ptr %r6, align 1
  ret i8 0
}

define i64 @main() {
entry:
  call void @__aria_module_init()
  %0 = call i8 @__user_main()
  %1 = sext i8 %0 to i64
  ret i64 %1
}
