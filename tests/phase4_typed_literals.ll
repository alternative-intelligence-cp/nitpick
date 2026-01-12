; ModuleID = 'tests/phase4_typed_literals.aria'
source_filename = "tests/phase4_typed_literals.aria"

define i32 @main() {
entry:
  call void @aria_gc_init(i64 0, i64 0)
  %a = alloca i8, align 1
  store i8 -1, ptr %a, align 1
  %b = alloca i16, align 2
  store i16 -1, ptr %b, align 2
  %c = alloca i32, align 4
  store i32 42, ptr %c, align 4
  %d = alloca i64, align 8
  store i64 1000, ptr %d, align 4
  %e = alloca i8, align 1
  store i8 127, ptr %e, align 1
  %f = alloca i16, align 2
  store i16 32767, ptr %f, align 2
  %g = alloca i32, align 4
  store i32 42, ptr %g, align 4
  %h = alloca i64, align 8
  store i64 1000, ptr %h, align 4
  %x = alloca float, align 4
  store float 0x40091EB860000000, ptr %x, align 4
  %y = alloca double, align 8
  store double 2.718280e+00, ptr %y, align 8
  ret i32 0
}

declare void @aria_gc_init(i64, i64)

define i32 @__phase4_typed_literals_init() {
entry:
  ret i32 0
}
