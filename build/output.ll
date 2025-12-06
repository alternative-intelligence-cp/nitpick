; ModuleID = 'aria_module'
source_filename = "aria_module"

%result_int8 = type { i8, i8 }

@0 = private unnamed_addr constant [44 x i8] c"=== Wildx Array Indexing Test (Decimal) ===\00", align 1
@1 = private unnamed_addr constant [39 x i8] c"\E2\9C\93 Wrote machine code to wildx buffer\00", align 1
@2 = private unnamed_addr constant [40 x i8] c"  Using decimal values for verification\00", align 1

declare void @puts(ptr)

declare void @print(ptr)

define internal void @__aria_module_init() {
entry:
  ret void
}

define internal %result_int8 @__user_main() {
entry:
  call void @puts(ptr @0)
  %0 = call ptr @aria_alloc_exec(i64 1)
  %code = alloca ptr, align 8
  store ptr %0, ptr %code, align 8
  %1 = load ptr, ptr %code, align 8
  store i64 0, ptr %1, align 4
  %code_ptr = load ptr, ptr %code, align 8
  %elem_ptr = getelementptr i8, ptr %code_ptr, i64 0
  store i8 72, ptr %elem_ptr, align 1
  %code_ptr1 = load ptr, ptr %code, align 8
  %elem_ptr2 = getelementptr i8, ptr %code_ptr1, i64 1
  store i8 -57, ptr %elem_ptr2, align 1
  %code_ptr3 = load ptr, ptr %code, align 8
  %elem_ptr4 = getelementptr i8, ptr %code_ptr3, i64 2
  store i8 -64, ptr %elem_ptr4, align 1
  %code_ptr5 = load ptr, ptr %code, align 8
  %elem_ptr6 = getelementptr i8, ptr %code_ptr5, i64 3
  store i8 42, ptr %elem_ptr6, align 1
  %code_ptr7 = load ptr, ptr %code, align 8
  %elem_ptr8 = getelementptr i8, ptr %code_ptr7, i64 4
  store i8 0, ptr %elem_ptr8, align 1
  %code_ptr9 = load ptr, ptr %code, align 8
  %elem_ptr10 = getelementptr i8, ptr %code_ptr9, i64 5
  store i8 0, ptr %elem_ptr10, align 1
  %code_ptr11 = load ptr, ptr %code, align 8
  %elem_ptr12 = getelementptr i8, ptr %code_ptr11, i64 6
  store i8 0, ptr %elem_ptr12, align 1
  %code_ptr13 = load ptr, ptr %code, align 8
  %elem_ptr14 = getelementptr i8, ptr %code_ptr13, i64 7
  store i8 -61, ptr %elem_ptr14, align 1
  call void @puts(ptr @1)
  call void @puts(ptr @2)
  ret %result_int8 zeroinitializer
}

declare ptr @aria_alloc_exec(i64)

define i64 @main() {
entry:
  call void @__aria_module_init()
  %0 = call %result_int8 @__user_main()
  ret i64 0
}
