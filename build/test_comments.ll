; ModuleID = 'aria_module'
source_filename = "aria_module"

%result_int8 = type { i8, i8 }
%result_int16 = type { i8, i16 }
%result_int32 = type { i8, i32 }
%result_int64 = type { i8, i64 }

@0 = private unnamed_addr constant [17 x i8] c"Testing int size\00", align 1
@1 = private unnamed_addr constant [17 x i8] c"Testing int size\00", align 1
@2 = private unnamed_addr constant [17 x i8] c"Testing int size\00", align 1
@3 = private unnamed_addr constant [17 x i8] c"Testing int size\00", align 1

declare void @puts(ptr)

declare void @print(ptr)

define internal void @__aria_module_init() {
entry:
  ret void
}

define internal %result_int8 @test_int8() {
entry:
  call void @puts(ptr @0)
  ret %result_int8 zeroinitializer
}

define internal %result_int16 @test_int16() {
entry:
  call void @puts(ptr @1)
  ret %result_int16 zeroinitializer
}

define internal %result_int32 @test_int32() {
entry:
  call void @puts(ptr @2)
  ret %result_int32 zeroinitializer
}

define internal %result_int64 @test_int64() {
entry:
  call void @puts(ptr @3)
  ret %result_int64 zeroinitializer
}

define internal %result_int8 @__user_main() {
entry:
  %calltmp = call %result_int8 @test_int8()
  ret %result_int8 zeroinitializer
}

define i64 @main() {
entry:
  call void @__aria_module_init()
  %0 = call %result_int8 @__user_main()
  ret i64 0
}
