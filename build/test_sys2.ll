; ModuleID = 'aria_module'
source_filename = "aria_module"

%result_int8 = type { i8, i8 }

@0 = private unnamed_addr constant [22 x i8] c"Syscall test starting\00", align 1
@1 = private unnamed_addr constant [6 x i8] c"Done!\00", align 1

declare void @puts(ptr)

declare void @print(ptr)

define internal void @__aria_module_init() {
entry:
  ret void
}

define internal %result_int8 @__user_main() {
entry:
  %written = alloca i64, align 8
  %msg = alloca [32 x i8], align 1
  call void @puts(ptr @0)
  %msg_ptr = getelementptr [32 x i8], ptr %msg, i64 0, i64 0
  %elem_ptr = getelementptr i8, ptr %msg_ptr, i64 0
  store i8 72, ptr %elem_ptr, align 1
  %msg_ptr1 = getelementptr [32 x i8], ptr %msg, i64 0, i64 0
  %elem_ptr2 = getelementptr i8, ptr %msg_ptr1, i64 1
  store i8 101, ptr %elem_ptr2, align 1
  %msg_ptr3 = getelementptr [32 x i8], ptr %msg, i64 0, i64 0
  %elem_ptr4 = getelementptr i8, ptr %msg_ptr3, i64 2
  store i8 108, ptr %elem_ptr4, align 1
  %msg_ptr5 = getelementptr [32 x i8], ptr %msg, i64 0, i64 0
  %elem_ptr6 = getelementptr i8, ptr %msg_ptr5, i64 3
  store i8 108, ptr %elem_ptr6, align 1
  %msg_ptr7 = getelementptr [32 x i8], ptr %msg, i64 0, i64 0
  %elem_ptr8 = getelementptr i8, ptr %msg_ptr7, i64 4
  store i8 111, ptr %elem_ptr8, align 1
  %msg_ptr9 = getelementptr [32 x i8], ptr %msg, i64 0, i64 0
  %elem_ptr10 = getelementptr i8, ptr %msg_ptr9, i64 5
  store i8 10, ptr %elem_ptr10, align 1
  %msg_ptr11 = getelementptr [32 x i8], ptr %msg, i64 0, i64 0
  %0 = ptrtoint ptr %msg_ptr11 to i64
  %syscall_result = call i64 asm sideeffect "syscall", "={rax},{rax},{rdi},{rsi},{rdx}"(i64 1, i64 1, i64 %0, i64 6)
  store i64 %syscall_result, ptr %written, align 4
  call void @puts(ptr @1)
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
