; ModuleID = 'tests/template_basic.aria'
source_filename = "tests/template_basic.aria"

%struct.AriaString = type { ptr, i64 }

@.str.data = private constant [6 x i8] c"World\00"
@.str = private constant %struct.AriaString { ptr @.str.data, i64 5 }
@.str.data.1 = private constant [15 x i8] c"Hello ${name}!\00"
@.str.2 = private constant %struct.AriaString { ptr @.str.data.1, i64 14 }

define i64 @main() {
entry:
  call void @aria_gc_init(i64 0, i64 0)
  %name = alloca ptr, align 8
  store ptr @.str, ptr %name, align 8
  %message = alloca ptr, align 8
  store ptr @.str.2, ptr %message, align 8
  ret i64 42
}

declare void @aria_gc_init(i64, i64)

define i32 @__template_basic_init() {
entry:
  ret i32 0
}
