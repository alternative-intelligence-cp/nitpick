; ModuleID = 'aria_module'
source_filename = "aria_module"

%result_int8 = type { i8, i8 }

declare void @puts(ptr)

declare void @print(ptr)

define internal void @__aria_module_init() {
entry:
  ret void
}

define internal %result_int8 @test_defer() {
entry:
  %x = alloca i8, align 1
  store i64 1, ptr %x, align 4
  %0 = load i8, ptr %x, align 1
  %1 = sext i8 %0 to i64
  %addtmp = add i64 %1, 2
  store i64 %addtmp, ptr %x, align 4
  %2 = load i8, ptr %x, align 1
  %3 = sext i8 %2 to i64
  %addtmp1 = add i64 %3, 10
  store i64 %addtmp1, ptr %x, align 4
  %4 = load i8, ptr %x, align 1
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 %4, ptr %val_ptr, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val
}

define internal %result_int8 @test_multiple_defers() {
entry:
  %sum = alloca i8, align 1
  store i64 0, ptr %sum, align 4
  %0 = load i8, ptr %sum, align 1
  %1 = sext i8 %0 to i64
  %addtmp = add i64 %1, 100
  store i64 %addtmp, ptr %sum, align 4
  %2 = load i8, ptr %sum, align 1
  %3 = sext i8 %2 to i64
  %addtmp1 = add i64 %3, 10
  store i64 %addtmp1, ptr %sum, align 4
  %4 = load i8, ptr %sum, align 1
  %5 = sext i8 %4 to i64
  %addtmp2 = add i64 %5, 1
  store i64 %addtmp2, ptr %sum, align 4
  %6 = load i8, ptr %x, align 1
  %7 = sext i8 %6 to i64
  %addtmp3 = add i64 %7, 10
  store i64 %addtmp3, ptr %x, align 4
  %8 = load i8, ptr %sum, align 1
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 %8, ptr %val_ptr, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val
}

define i64 @main() {
entry:
  call void @__aria_module_init()
  ret i64 0
}
