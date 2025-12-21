; ModuleID = 'tests/template_basic.aria'
source_filename = "tests/template_basic.aria"

@str = private unnamed_addr constant [6 x i8] c"World\00", align 1

define i64 @main() {
entry:
  %name = alloca ptr, align 8
  store ptr @str, ptr %name, align 8
  %message = alloca ptr, align 8
  ret i64 42
}

define i32 @__template_basic_init() {
entry:
  ret i32 0
}
