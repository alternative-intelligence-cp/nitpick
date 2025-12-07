; ModuleID = 'aria_module'
source_filename = "aria_module"

%result_int8 = type { i8, i8 }
%result_int16 = type { i8, i16 }

declare void @puts(ptr)

declare void @print(ptr)

define internal void @__aria_module_init() {
entry:
  ret void
}

define internal %result_int8 @test_int8(i8 %val) {
entry:
  %val1 = alloca i8, align 1
  store i8 %val, ptr %val1, align 1
  %0 = load i8, ptr %val1, align 1
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 %0, ptr %val_ptr, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val
}

define internal %result_int16 @test_int16(i16 %val) {
entry:
  %val1 = alloca i16, align 2
  store i16 %val, ptr %val1, align 2
  %0 = load i16, ptr %val1, align 2
  %auto_wrap_result = alloca %result_int16, align 8
  %err_ptr = getelementptr inbounds %result_int16, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int16, ptr %auto_wrap_result, i32 0, i32 1
  store i16 %0, ptr %val_ptr, align 2
  %result_val = load %result_int16, ptr %auto_wrap_result, align 2
  ret %result_int16 %result_val
}

define internal %result_int8 @__user_main() {
entry:
  %x = alloca i8, align 1
  %calltmp = call %result_int8 @test_int8(i8 5)
  store %result_int8 %calltmp, ptr %x, align 1
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 0, ptr %val_ptr, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val
}

define i64 @main() {
entry:
  call void @__aria_module_init()
  %0 = call %result_int8 @__user_main()
  ret i64 0
}
