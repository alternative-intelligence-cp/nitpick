; ModuleID = 'aria_module'
source_filename = "aria_module"

%result_int8 = type { i8, i8 }

@0 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@1 = private unnamed_addr constant [6 x i8] c"Age: \00", align 1
@2 = private unnamed_addr constant [4 x i8] c"%ld\00", align 1
@3 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@4 = private unnamed_addr constant [7 x i8] c"Year: \00", align 1
@5 = private unnamed_addr constant [4 x i8] c"%ld\00", align 1
@6 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@7 = private unnamed_addr constant [7 x i8] c"Math: \00", align 1
@8 = private unnamed_addr constant [4 x i8] c"%ld\00", align 1
@9 = private unnamed_addr constant [3 x i8] c"%s\00", align 1
@10 = private unnamed_addr constant [13 x i8] c"Expression: \00", align 1
@11 = private unnamed_addr constant [4 x i8] c"%ld\00", align 1

declare void @puts(ptr)

declare void @print(ptr)

define internal void @__aria_module_init() {
entry:
  ret void
}

define internal %result_int8 @__user_main() {
entry:
  %year = alloca i8, align 1
  %age = alloca i8, align 1
  store i64 30, ptr %age, align 4
  store i64 2024, ptr %year, align 4
  %str_buffer = alloca i8, i64 1024, align 1
  %0 = call i32 (ptr, ptr, ...) @sprintf(ptr %str_buffer, ptr @0, ptr @1)
  %next_pos = getelementptr i8, ptr %str_buffer, i32 %0
  %1 = load i8, ptr %age, align 1
  %to_i64 = sext i8 %1 to i64
  %2 = call i32 (ptr, ptr, ...) @sprintf(ptr %next_pos, ptr @2, i64 %to_i64)
  %next_pos1 = getelementptr i8, ptr %next_pos, i32 %2
  call void @puts(ptr %str_buffer)
  %str_buffer2 = alloca i8, i64 1024, align 1
  %3 = call i32 (ptr, ptr, ...) @sprintf(ptr %str_buffer2, ptr @3, ptr @4)
  %next_pos3 = getelementptr i8, ptr %str_buffer2, i32 %3
  %4 = load i8, ptr %year, align 1
  %to_i644 = sext i8 %4 to i64
  %5 = call i32 (ptr, ptr, ...) @sprintf(ptr %next_pos3, ptr @5, i64 %to_i644)
  %next_pos5 = getelementptr i8, ptr %next_pos3, i32 %5
  call void @puts(ptr %str_buffer2)
  %str_buffer6 = alloca i8, i64 1024, align 1
  %6 = call i32 (ptr, ptr, ...) @sprintf(ptr %str_buffer6, ptr @6, ptr @7)
  %next_pos7 = getelementptr i8, ptr %str_buffer6, i32 %6
  %7 = call i32 (ptr, ptr, ...) @sprintf(ptr %next_pos7, ptr @8, i64 15)
  %next_pos8 = getelementptr i8, ptr %next_pos7, i32 %7
  call void @puts(ptr %str_buffer6)
  %str_buffer9 = alloca i8, i64 1024, align 1
  %8 = call i32 (ptr, ptr, ...) @sprintf(ptr %str_buffer9, ptr @9, ptr @10)
  %next_pos10 = getelementptr i8, ptr %str_buffer9, i32 %8
  %9 = load i8, ptr %age, align 1
  %10 = sext i8 %9 to i64
  %addtmp = add i64 %10, 5
  %11 = call i32 (ptr, ptr, ...) @sprintf(ptr %next_pos10, ptr @11, i64 %addtmp)
  %next_pos11 = getelementptr i8, ptr %next_pos10, i32 %11
  call void @puts(ptr %str_buffer9)
  %auto_wrap_result = alloca %result_int8, align 8
  %err_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int8, ptr %auto_wrap_result, i32 0, i32 1
  store i8 0, ptr %val_ptr, align 1
  %result_val = load %result_int8, ptr %auto_wrap_result, align 1
  ret %result_int8 %result_val
}

declare i32 @sprintf(ptr, ptr, ...)

define i64 @main() {
entry:
  call void @__aria_module_init()
  %0 = call %result_int8 @__user_main()
  ret i64 0
}
