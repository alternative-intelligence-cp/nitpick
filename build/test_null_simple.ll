; ModuleID = 'aria_module'
source_filename = "aria_module"

%result_int64 = type { i8, i64 }

declare void @puts(ptr)

declare void @print(ptr)

define internal void @__aria_module_init() {
entry:
  ret void
}

define internal %result_int64 @test_null() {
entry:
  %not_null = alloca ptr, align 8
  %is_null = alloca ptr, align 8
  %0 = call ptr @aria_alloc(i64 8)
  %ptr = alloca ptr, align 8
  store ptr %0, ptr %ptr, align 8
  %1 = load ptr, ptr %ptr, align 8
  store ptr null, ptr %1, align 8
  %2 = load ptr, ptr %ptr, align 8
  %3 = load ptr, ptr %2, align 8
  %eqtmp = icmp eq ptr %3, null
  store i1 %eqtmp, ptr %is_null, align 1
  %4 = load ptr, ptr %ptr, align 8
  %5 = load ptr, ptr %4, align 8
  %netmp = icmp ne ptr %5, null
  store i1 %netmp, ptr %not_null, align 1
  ret i64 0
}

declare ptr @aria_alloc(i64)

define i64 @main() {
entry:
  call void @__aria_module_init()
  ret i64 0
}
