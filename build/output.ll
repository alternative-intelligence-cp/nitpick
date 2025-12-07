; ModuleID = 'aria_module'
source_filename = "aria_module"

%result_int64 = type { i8, i64 }
%result_int8 = type { i8, i8 }

@0 = private unnamed_addr constant [32 x i8] c"Function signature test passed!\00", align 1

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

define internal %result_int8 @__user_main() {
entry:
  %r = alloca %result_int64, align 8
  %my_add = alloca ptr, align 8
  %calltmp = call %result_int64 @add(i64 10, i64 20)
  store %result_int64 %calltmp, ptr %r, align 4
  %0 = load %result_int64, ptr %r, align 4
  %temp = alloca %result_int64, align 8
  store %result_int64 %0, ptr %temp, align 4
  %err_ptr = getelementptr inbounds %result_int64, ptr %temp, i32 0, i32 0
  %err = load i8, ptr %err_ptr, align 1
  %1 = sext i8 %err to i64
  %eqtmp = icmp eq i64 %1, 0
  br i1 %eqtmp, label %then, label %ifcont

then:                                             ; preds = %entry
  call void @puts(ptr @0)
  br label %ifcont

ifcont:                                           ; preds = %then, %entry
  %result = alloca %result_int8, align 8
  %err_ptr1 = getelementptr inbounds %result_int8, ptr %result, i32 0, i32 0
  store i8 1, ptr %err_ptr1, align 1
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
