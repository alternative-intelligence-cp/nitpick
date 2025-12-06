; ModuleID = 'aria_module'
source_filename = "aria_module"

%result_int8 = type { i8, i8 }

@0 = private unnamed_addr constant [37 x i8] c"=== Wildx Memory Protection Test ===\00", align 1
@1 = private unnamed_addr constant [51 x i8] c"Step 1: Allocated 4KB executable memory (RW state)\00", align 1
@2 = private unnamed_addr constant [43 x i8] c"Step 2: Wrote opcodes to buffer (still RW)\00", align 1
@3 = private unnamed_addr constant [38 x i8] c"Step 3: Would seal memory to RX state\00", align 1
@4 = private unnamed_addr constant [37 x i8] c"       (protect_exec intrinsic call)\00", align 1
@5 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@6 = private unnamed_addr constant [31 x i8] c"wildx infrastructure complete!\00", align 1
@7 = private unnamed_addr constant [49 x i8] c"Next: Add function pointer support for execution\00", align 1

declare void @puts(ptr)

declare void @print(ptr)

define internal void @__aria_module_init() {
entry:
  ret void
}

define internal %result_int8 @__user_main() {
entry:
  call void @puts(ptr @0)
  %0 = call ptr @aria_alloc_exec(i64 8)
  %code_page = alloca ptr, align 8
  store ptr %0, ptr %code_page, align 8
  %1 = load ptr, ptr %code_page, align 8
  store i64 0, ptr %1, align 4
  call void @puts(ptr @1)
  store i64 0, ptr %code_page, align 4
  call void @puts(ptr @2)
  call void @puts(ptr @3)
  call void @puts(ptr @4)
  call void @puts(ptr @5)
  call void @puts(ptr @6)
  call void @puts(ptr @7)
  ret %result_int8 zeroinitializer
}

declare ptr @aria_alloc_exec(i64)

define i64 @main() {
entry:
  call void @__aria_module_init()
  %0 = call %result_int8 @__user_main()
  ret i64 0
}
