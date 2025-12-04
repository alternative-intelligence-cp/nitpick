; ModuleID = 'aria_module'
source_filename = "aria_module"

@x = internal global i64 42
@0 = private unnamed_addr constant [20 x i8] c"The value is <expr>\00", align 1
@message = internal global ptr @0
@1 = private unnamed_addr constant [20 x i8] c"The value is <expr>\00", align 1

declare void @puts(ptr)

declare void @print(ptr)

define internal void @__aria_module_init() {
entry:
  ret void
}

define i64 @main() {
entry:
  call void @__aria_module_init()
  ret i64 0
}
