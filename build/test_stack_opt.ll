; ModuleID = 'aria_module'
source_filename = "aria_module"

declare void @puts(ptr)

declare void @print(ptr)

define internal void @__aria_module_init() {
entry:
  ret void
}

define internal i64 @stack_primitives() {
entry:
  %sum = alloca i64, align 8
  %flag = alloca ptr, align 8
  %d = alloca i64, align 8
  %c = alloca i32, align 4
  %b = alloca i16, align 2
  %a = alloca i8, align 1
  store i64 1, ptr %a, align 4
  store i64 2, ptr %b, align 4
  store i64 3, ptr %c, align 4
  store i64 4, ptr %d, align 4
  store i1 true, ptr %flag, align 1
  %0 = load i8, ptr %a, align 1
  %1 = load i16, ptr %b, align 2
  %2 = sext i8 %0 to i16
  %addtmp = add i16 %2, %1
  %3 = load i32, ptr %c, align 4
  %4 = sext i16 %addtmp to i32
  %addtmp1 = add i32 %4, %3
  %5 = load i64, ptr %d, align 4
  %6 = sext i32 %addtmp1 to i64
  %addtmp2 = add i64 %6, %5
  store i64 %addtmp2, ptr %sum, align 4
  %7 = load i64, ptr %sum, align 4
  ret i64 %7
}

define internal i64 @explicit_heap() {
entry:
  %y = alloca i64, align 8
  %x = alloca i64, align 8
  store i64 100, ptr %x, align 4
  store i64 200, ptr %y, align 4
  %0 = load i64, ptr %x, align 4
  %1 = load i64, ptr %y, align 4
  %addtmp = add i64 %0, %1
  ret i64 %addtmp
}

define internal i64 @__user_main() {
entry:
  %result2 = alloca i64, align 8
  %result1 = alloca i64, align 8
  %calltmp = call i64 @stack_primitives()
  store i64 %calltmp, ptr %result1, align 4
  %calltmp1 = call i64 @explicit_heap()
  store i64 %calltmp1, ptr %result2, align 4
  %0 = load i64, ptr %result1, align 4
  %1 = load i64, ptr %result2, align 4
  %addtmp = add i64 %0, %1
  ret i64 %addtmp
}

define i64 @main() {
entry:
  call void @__aria_module_init()
  %0 = call i64 @__user_main()
  ret i64 %0
}
