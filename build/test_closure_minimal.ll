; ModuleID = 'aria_module'
source_filename = "aria_module"

%result_int8 = type { i8, i8 }

@globalValue = internal global i8 5

declare void @puts(ptr)

declare void @print(ptr)

define internal void @__aria_module_init() {
entry:
  ret void
}

define internal %result_int8 @useGlobal(i8 %x) {
entry:
  %x1 = alloca i8, align 1
  store i8 %x, ptr %x1, align 1
  %0 = load i64, ptr %x1, align 4
  %1 = load i64, ptr @globalValue, align 4
  %addtmp = add i64 %0, %1
  ret i64 %addtmp
}

define internal %result_int8 @__user_main() {
entry:
  %calltmp = call %result_int8 @useGlobal(i8 10)
  ret %result_int8 %calltmp
}

define i64 @main() {
entry:
  call void @__aria_module_init()
  %0 = call %result_int8 @__user_main()
  ret i64 0
}
