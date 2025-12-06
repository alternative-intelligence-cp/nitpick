; ModuleID = 'aria_module'
source_filename = "aria_module"

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
