; ModuleID = 'aria_module'
source_filename = "aria_module"

%result_int64 = type { i8, i64 }

declare void @puts(ptr)

declare void @print(ptr)

define internal void @__aria_module_init() {
entry:
  ret void
}

define internal %result_int64 @test() {
entry:
  %y = alloca ptr, align 8
  %x = alloca ptr, align 8
  %result = alloca %result_int64, align 8
  %err_ptr = getelementptr inbounds %result_int64, ptr %result, i32 0, i32 0
  store i8 42, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int64, ptr %result, i32 0, i32 1
  store i64 0, ptr %val_ptr, align 4
  %result_val = load %result_int64, ptr %result, align 4
  store %result_int64 %result_val, ptr %x, align 4
  %result1 = alloca %result_int64, align 8
  %err_ptr2 = getelementptr inbounds %result_int64, ptr %result1, i32 0, i32 0
  store i8 0, ptr %err_ptr2, align 1
  %val_ptr3 = getelementptr inbounds %result_int64, ptr %result1, i32 0, i32 1
  store i64 100, ptr %val_ptr3, align 4
  %result_val4 = load %result_int64, ptr %result1, align 4
  store %result_int64 %result_val4, ptr %y, align 4
  ret i64 0
}

define i64 @main() {
entry:
  call void @__aria_module_init()
  ret i64 0
}
