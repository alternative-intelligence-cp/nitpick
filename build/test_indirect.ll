; ModuleID = 'aria_module'
source_filename = "aria_module"

%result_int64 = type { i8, i64 }
%result_int8 = type { i8, i8 }

@0 = private unnamed_addr constant [25 x i8] c"First result (10 + 20): \00", align 1
@1 = private unnamed_addr constant [26 x i8] c"Second result (10 * 20): \00", align 1
@2 = private unnamed_addr constant [27 x i8] c"Indirect call test passed!\00", align 1

declare void @puts(ptr)

declare void @print(ptr)

define internal void @__aria_module_init() {
entry:
  ret void
}

define internal %result_int64 @add(i64 %a, i64 %b) {
entry:
  %a1 = alloca i64, align 8
  store i64 %a, ptr %a1, align 4
  %b2 = alloca i64, align 8
  store i64 %b, ptr %b2, align 4
  %0 = load i64, ptr %a1, align 4
  %1 = load i64, ptr %b2, align 4
  %addtmp = add i64 %0, %1
  %result = alloca %result_int64, align 8
  %err_ptr = getelementptr inbounds %result_int64, ptr %result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int64, ptr %result, i32 0, i32 1
  store i64 %addtmp, ptr %val_ptr, align 4
  %result_val = load %result_int64, ptr %result, align 4
  ret %result_int64 %result_val
}

define internal %result_int64 @multiply(i64 %a, i64 %b) {
entry:
  %a1 = alloca i64, align 8
  store i64 %a, ptr %a1, align 4
  %b2 = alloca i64, align 8
  store i64 %b, ptr %b2, align 4
  %0 = load i64, ptr %a1, align 4
  %1 = load i64, ptr %b2, align 4
  %multmp = mul i64 %0, %1
  %result = alloca %result_int64, align 8
  %err_ptr = getelementptr inbounds %result_int64, ptr %result, i32 0, i32 0
  store i8 0, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int64, ptr %result, i32 0, i32 1
  store i64 %multmp, ptr %val_ptr, align 4
  %result_val = load %result_int64, ptr %result, align 4
  ret %result_int64 %result_val
}

define internal %result_int8 @__user_main() {
entry:
  %r2 = alloca %result_int64, align 8
  %r1 = alloca %result_int64, align 8
  %calltmp = call %result_int64 @add(i64 10, i64 20)
  store %result_int64 %calltmp, ptr %r1, align 4
  call void @puts(ptr @0)
  %calltmp1 = call %result_int64 @add(i64 10, i64 20)
  store %result_int64 %calltmp1, ptr %r2, align 4
  call void @puts(ptr @1)
  call void @puts(ptr @2)
  ret %result_int8 zeroinitializer
}

define i64 @main() {
entry:
  call void @__aria_module_init()
  %0 = call %result_int8 @__user_main()
  ret i64 0
}
