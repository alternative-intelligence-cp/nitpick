; ModuleID = 'aria_module'
source_filename = "aria_module"

declare void @puts(ptr)

declare void @print(ptr)

define internal void @__aria_module_init() {
entry:
  ret void
}

define internal i8 @test_basic_assign() {
entry:
  %0 = call ptr @get_current_thread_nursery()
  %1 = call ptr @aria_gc_alloc(ptr %0, i64 1)
  %x = alloca ptr, align 8
  store ptr %1, ptr %x, align 8
  %2 = load ptr, ptr %x, align 8
  store i64 5, ptr %2, align 4
  store i64 10, ptr %x, align 4
  %3 = load i8, ptr %x, align 1
  ret i8 %3
}

declare ptr @get_current_thread_nursery()

declare ptr @aria_gc_alloc(ptr, i64)

define internal i8 @test_plus_assign() {
entry:
  %0 = call ptr @get_current_thread_nursery.1()
  %1 = call ptr @aria_gc_alloc.2(ptr %0, i64 1)
  %x = alloca ptr, align 8
  store ptr %1, ptr %x, align 8
  %2 = load ptr, ptr %x, align 8
  store i64 5, ptr %2, align 4
  %3 = load i8, ptr %x, align 1
  %addtmp = add i8 %3, i64 3
  store i8 %addtmp, ptr %x, align 1
  %4 = load i8, ptr %x, align 1
  ret i8 %4
}

declare ptr @get_current_thread_nursery.1()

declare ptr @aria_gc_alloc.2(ptr, i64)

define internal i8 @test_minus_assign() {
entry:
  %0 = call ptr @get_current_thread_nursery.3()
  %1 = call ptr @aria_gc_alloc.4(ptr %0, i64 1)
  %x = alloca ptr, align 8
  store ptr %1, ptr %x, align 8
  %2 = load ptr, ptr %x, align 8
  store i64 10, ptr %2, align 4
  %3 = load i8, ptr %x, align 1
  %subtmp = sub i8 %3, i64 4
  store i8 %subtmp, ptr %x, align 1
  %4 = load i8, ptr %x, align 1
  ret i8 %4
}

declare ptr @get_current_thread_nursery.3()

declare ptr @aria_gc_alloc.4(ptr, i64)

define internal i8 @test_mul_assign() {
entry:
  %0 = call ptr @get_current_thread_nursery.5()
  %1 = call ptr @aria_gc_alloc.6(ptr %0, i64 1)
  %x = alloca ptr, align 8
  store ptr %1, ptr %x, align 8
  %2 = load ptr, ptr %x, align 8
  store i64 5, ptr %2, align 4
  %3 = load i8, ptr %x, align 1
  %multmp = mul i8 %3, i64 2
  store i8 %multmp, ptr %x, align 1
  %4 = load i8, ptr %x, align 1
  ret i8 %4
}

declare ptr @get_current_thread_nursery.5()

declare ptr @aria_gc_alloc.6(ptr, i64)

define internal i8 @test_div_assign() {
entry:
  %0 = call ptr @get_current_thread_nursery.7()
  %1 = call ptr @aria_gc_alloc.8(ptr %0, i64 1)
  %x = alloca ptr, align 8
  store ptr %1, ptr %x, align 8
  %2 = load ptr, ptr %x, align 8
  store i64 20, ptr %2, align 4
  %3 = load i8, ptr %x, align 1
  %divtmp = sdiv i8 %3, i64 4
  store i8 %divtmp, ptr %x, align 1
  %4 = load i8, ptr %x, align 1
  ret i8 %4
}

declare ptr @get_current_thread_nursery.7()

declare ptr @aria_gc_alloc.8(ptr, i64)

define internal i8 @test_mod_assign() {
entry:
  %0 = call ptr @get_current_thread_nursery.9()
  %1 = call ptr @aria_gc_alloc.10(ptr %0, i64 1)
  %x = alloca ptr, align 8
  store ptr %1, ptr %x, align 8
  %2 = load ptr, ptr %x, align 8
  store i64 17, ptr %2, align 4
  %3 = load i8, ptr %x, align 1
  %modtmp = srem i8 %3, i64 5
  store i8 %modtmp, ptr %x, align 1
  %4 = load i8, ptr %x, align 1
  ret i8 %4
}

declare ptr @get_current_thread_nursery.9()

declare ptr @aria_gc_alloc.10(ptr, i64)

define internal i8 @__user_main() {
entry:
  %0 = call ptr @get_current_thread_nursery.11()
  %1 = call ptr @aria_gc_alloc.12(ptr %0, i64 1)
  %r1 = alloca ptr, align 8
  store ptr %1, ptr %r1, align 8
  %calltmp = call i8 @test_basic_assign()
  %2 = load ptr, ptr %r1, align 8
  store i8 %calltmp, ptr %2, align 1
  %3 = call ptr @get_current_thread_nursery.13()
  %4 = call ptr @aria_gc_alloc.14(ptr %3, i64 1)
  %r2 = alloca ptr, align 8
  store ptr %4, ptr %r2, align 8
  %calltmp1 = call i8 @test_plus_assign()
  %5 = load ptr, ptr %r2, align 8
  store i8 %calltmp1, ptr %5, align 1
  %6 = call ptr @get_current_thread_nursery.15()
  %7 = call ptr @aria_gc_alloc.16(ptr %6, i64 1)
  %r3 = alloca ptr, align 8
  store ptr %7, ptr %r3, align 8
  %calltmp2 = call i8 @test_minus_assign()
  %8 = load ptr, ptr %r3, align 8
  store i8 %calltmp2, ptr %8, align 1
  %9 = call ptr @get_current_thread_nursery.17()
  %10 = call ptr @aria_gc_alloc.18(ptr %9, i64 1)
  %r4 = alloca ptr, align 8
  store ptr %10, ptr %r4, align 8
  %calltmp3 = call i8 @test_mul_assign()
  %11 = load ptr, ptr %r4, align 8
  store i8 %calltmp3, ptr %11, align 1
  %12 = call ptr @get_current_thread_nursery.19()
  %13 = call ptr @aria_gc_alloc.20(ptr %12, i64 1)
  %r5 = alloca ptr, align 8
  store ptr %13, ptr %r5, align 8
  %calltmp4 = call i8 @test_div_assign()
  %14 = load ptr, ptr %r5, align 8
  store i8 %calltmp4, ptr %14, align 1
  %15 = call ptr @get_current_thread_nursery.21()
  %16 = call ptr @aria_gc_alloc.22(ptr %15, i64 1)
  %r6 = alloca ptr, align 8
  store ptr %16, ptr %r6, align 8
  %calltmp5 = call i8 @test_mod_assign()
  %17 = load ptr, ptr %r6, align 8
  store i8 %calltmp5, ptr %17, align 1
  ret i64 0
}

declare ptr @get_current_thread_nursery.11()

declare ptr @aria_gc_alloc.12(ptr, i64)

declare ptr @get_current_thread_nursery.13()

declare ptr @aria_gc_alloc.14(ptr, i64)

declare ptr @get_current_thread_nursery.15()

declare ptr @aria_gc_alloc.16(ptr, i64)

declare ptr @get_current_thread_nursery.17()

declare ptr @aria_gc_alloc.18(ptr, i64)

declare ptr @get_current_thread_nursery.19()

declare ptr @aria_gc_alloc.20(ptr, i64)

declare ptr @get_current_thread_nursery.21()

declare ptr @aria_gc_alloc.22(ptr, i64)

define i64 @main() {
entry:
  call void @__aria_module_init()
  %0 = call i8 @__user_main()
  %1 = sext i8 %0 to i64
  ret i64 %1
}
