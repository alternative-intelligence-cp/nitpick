; ModuleID = 'aria_module'
source_filename = "aria_module"

%result_int8 = type { i8, i8 }

@0 = private unnamed_addr constant [22 x i8] c"Allocating resource 1\00", align 1
@1 = private unnamed_addr constant [22 x i8] c"Allocating resource 2\00", align 1
@2 = private unnamed_addr constant [16 x i8] c"Using resources\00", align 1
@3 = private unnamed_addr constant [16 x i8] c"Function ending\00", align 1
@4 = private unnamed_addr constant [19 x i8] c"Freeing resource 2\00", align 1
@5 = private unnamed_addr constant [19 x i8] c"Freeing resource 1\00", align 1

declare void @puts(ptr)

declare void @print(ptr)

define internal void @__aria_module_init() {
entry:
  ret void
}

define internal %result_int8 @__user_main() {
entry:
  call void @puts(ptr @0)
  call void @puts(ptr @1)
  call void @puts(ptr @2)
  call void @puts(ptr @3)
  call void @puts(ptr @4)
  call void @puts(ptr @5)
  ret %result_int8 zeroinitializer
}

define i64 @main() {
entry:
  call void @__aria_module_init()
  %0 = call %result_int8 @__user_main()
  ret i64 0
}
