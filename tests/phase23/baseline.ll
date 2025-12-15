; ModuleID = 'aria_module'
source_filename = "aria_module"

@x = internal global i8 42
@y = internal global i8 0
@z = internal global i8 0

declare void @puts(ptr)

declare void @print(ptr)

declare void @llvm.coro.resume(ptr)

define void @aria_coro_resume_bridge(ptr %0) {
entry:
  call void @llvm.coro.resume(ptr %0)
  ret void
}

define internal void @__aria_module_init() {
entry:
  %0 = load i8, ptr @x, align 1
  %1 = sext i8 %0 to i64
  %add = add i64 %1, 10
  %2 = load i8, ptr @x, align 1
  %3 = sext i8 %2 to i64
  %add1 = add i64 %3, 10
  %4 = trunc i64 %add1 to i8
  store i8 %4, ptr @y, align 1
  %5 = load i8, ptr @y, align 1
  %6 = sext i8 %5 to i64
  %mul = mul i64 %6, 2
  %7 = load i8, ptr @y, align 1
  %8 = sext i8 %7 to i64
  %mul2 = mul i64 %8, 2
  %9 = trunc i64 %mul2 to i8
  store i8 %9, ptr @z, align 1
  ret void
}

define i64 @main() {
entry:
  call void @aria_scheduler_init(i32 0)
  call void @_ZN4aria7backend10TernaryOps10initializeEv()
  call void @__aria_module_init()
  ret i64 0
}

declare void @aria_scheduler_init(i32)

declare void @_ZN4aria7backend10TernaryOps10initializeEv()
