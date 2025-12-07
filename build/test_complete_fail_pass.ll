; ModuleID = 'aria_module'
source_filename = "aria_module"

%result_int64 = type { i8, i64 }

declare void @puts(ptr)

declare void @print(ptr)

define internal void @__aria_module_init() {
entry:
  ret void
}

define internal %result_int64 @divide(i64 %a, i64 %b) {
entry:
  %a1 = alloca i64, align 8
  store i64 %a, ptr %a1, align 4
  %b2 = alloca i64, align 8
  store i64 %b, ptr %b2, align 4
  %0 = load i64, ptr %b2, align 4
  %eqtmp = icmp eq i64 %0, 0
  br i1 %eqtmp, label %then, label %ifcont

then:                                             ; preds = %entry
  %result = alloca %result_int64, align 8
  %err_ptr = getelementptr inbounds %result_int64, ptr %result, i32 0, i32 0
  store i8 1, ptr %err_ptr, align 1
  %val_ptr = getelementptr inbounds %result_int64, ptr %result, i32 0, i32 1
  store i64 0, ptr %val_ptr, align 4
  %result_val = load %result_int64, ptr %result, align 4
  ret %result_int64 %result_val

ifcont:                                           ; preds = %entry
  %1 = load i64, ptr %a1, align 4
  %2 = load i64, ptr %b2, align 4
  %divtmp = sdiv i64 %1, %2
  %result3 = alloca %result_int64, align 8
  %err_ptr4 = getelementptr inbounds %result_int64, ptr %result3, i32 0, i32 0
  store i8 0, ptr %err_ptr4, align 1
  %val_ptr5 = getelementptr inbounds %result_int64, ptr %result3, i32 0, i32 1
  store i64 %divtmp, ptr %val_ptr5, align 4
  %result_val6 = load %result_int64, ptr %result3, align 4
  ret %result_int64 %result_val6
}

define internal %result_int64 @__user_main() {
entry:
  %r2 = alloca i64, align 8
  %r1 = alloca i64, align 8
  %calltmp = call %result_int64 @divide(i64 10, i64 2)
  store %result_int64 %calltmp, ptr %r1, align 4
  %calltmp1 = call %result_int64 @divide(i64 10, i64 0)
  store %result_int64 %calltmp1, ptr %r2, align 4
  ret i64 0
}

define i64 @main() {
entry:
  call void @__aria_module_init()
  %0 = call %result_int64 @__user_main()
  ret i64 0
}
