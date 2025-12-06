; ModuleID = 'aria_module'
source_filename = "aria_module"

%result_int8 = type { i8, i8 }

@0 = private unnamed_addr constant [33 x i8] c"=== Type System Verification ===\00", align 1
@1 = private unnamed_addr constant [41 x i8] c"\E2\9C\93 All standard types compile correctly\00", align 1

declare void @puts(ptr)

declare void @print(ptr)

define internal void @__aria_module_init() {
entry:
  ret void
}

define internal %result_int8 @__user_main() {
entry:
  %o = alloca i8, align 1
  %n = alloca i4, align 1
  %k = alloca i64, align 8
  %j = alloca i32, align 4
  %i = alloca i16, align 2
  %h = alloca i8, align 1
  %g = alloca i64, align 8
  %f = alloca i32, align 4
  %e = alloca i16, align 2
  %d = alloca i8, align 1
  %c = alloca i4, align 1
  %b = alloca i2, align 1
  %a = alloca i1, align 1
  call void @puts(ptr @0)
  store i64 1, ptr %a, align 4
  store i64 3, ptr %b, align 4
  store i64 15, ptr %c, align 4
  store i64 127, ptr %d, align 4
  store i64 32000, ptr %e, align 4
  store i64 2000000, ptr %f, align 4
  store i64 9000000, ptr %g, align 4
  store i64 255, ptr %h, align 4
  store i64 60000, ptr %i, align 4
  store i64 4000000, ptr %j, align 4
  store i64 9000000, ptr %k, align 4
  store i64 4, ptr %n, align 4
  store i64 1, ptr %o, align 4
  call void @puts(ptr @1)
  ret %result_int8 zeroinitializer
}

define i64 @main() {
entry:
  call void @__aria_module_init()
  %0 = call %result_int8 @__user_main()
  ret i64 0
}
