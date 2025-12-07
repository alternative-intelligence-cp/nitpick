; ModuleID = 'aria_module'
source_filename = "aria_module"

%result_int8 = type { i8, i8 }

@0 = private unnamed_addr constant [33 x i8] c"TBB types compiled successfully!\00", align 1

declare void @puts(ptr)

declare void @print(ptr)

define internal void @__aria_module_init() {
entry:
  ret void
}

define internal %result_int8 @__user_main() {
entry:
  %huge = alloca i64, align 8
  %big = alloca i32, align 4
  %y = alloca i16, align 2
  %x = alloca i16, align 2
  %e = alloca i8, align 1
  %d = alloca i8, align 1
  %c = alloca i8, align 1
  %b = alloca i8, align 1
  %a = alloca i8, align 1
  store i8 100, ptr %a, align 1
  store i8 -100, ptr %b, align 1
  store i8 0, ptr %c, align 1
  store i8 127, ptr %d, align 1
  store i8 -127, ptr %e, align 1
  store i16 30000, ptr %x, align 2
  store i16 -30000, ptr %y, align 2
  store i32 1000000, ptr %big, align 4
  store i64 1000000000, ptr %huge, align 4
  call void @puts(ptr @0)
  %result = alloca %result_int8, align 8
  %err_ptr = getelementptr inbounds %result_int8, ptr %result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int8, ptr %result, i32 0, i32 1
  store i8 0, ptr %val_ptr, align 1
  %result_val = load %result_int8, ptr %result, align 1
  ret %result_int8 %result_val
}

define i64 @main() {
entry:
  call void @__aria_module_init()
  %0 = call %result_int8 @__user_main()
  ret i64 0
}
