; ModuleID = 'aria_module'
source_filename = "aria_module"

%result_int8 = type { i8, i8 }

@0 = private unnamed_addr constant [34 x i8] c"=== Testing uint1/2/4 Aliases ===\00", align 1
@1 = private unnamed_addr constant [47 x i8] c"\E2\9C\93 uint1, uint2, uint4 aliases work correctly\00", align 1
@2 = private unnamed_addr constant [55 x i8] c"\E2\9C\93 No confusion for users coming from other languages\00", align 1

declare void @puts(ptr)

declare void @print(ptr)

define internal void @__aria_module_init() {
entry:
  ret void
}

define internal %result_int8 @__user_main() {
entry:
  %f = alloca i4, align 1
  %e = alloca i2, align 1
  %d = alloca i1, align 1
  %c = alloca i4, align 1
  %b = alloca i2, align 1
  %a = alloca i1, align 1
  call void @puts(ptr @0)
  store i64 1, ptr %a, align 4
  store i64 3, ptr %b, align 4
  store i64 15, ptr %c, align 4
  store i64 1, ptr %d, align 4
  store i64 3, ptr %e, align 4
  store i64 15, ptr %f, align 4
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
