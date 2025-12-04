; ModuleID = 'aria_module'
source_filename = "aria_module"

%result_int8 = type { i8, i8 }

@closureTest = internal global i8 2

declare void @puts(ptr)

declare void @print(ptr)

define internal void @__aria_module_init() {
entry:
  ret void
}

define internal %result_int8 @test(i8 %a, i8 %b) {
entry:
  %a1 = alloca i8, align 1
  store i8 %a, ptr %a1, align 1
  %b2 = alloca i8, align 1
  store i8 %b, ptr %b2, align 1
  %0 = load i64, ptr %a1, align 4
  %1 = load i64, ptr %b2, align 4
  %multmp = mul i64 %0, %1
  %2 = load i64, ptr @closureTest, align 4
  %multmp3 = mul i64 %multmp, %2
  ret i64 %multmp3
}

define internal %result_int8 @__user_main() {
entry:
  %calltmp = call %result_int8 @test(i8 3, i8 4)
  ret %result_int8 %calltmp
}

define i64 @main() {
entry:
  call void @__aria_module_init()
  %0 = call %result_int8 @__user_main()
  ret i64 0
}
