; ModuleID = 'aria_module'
source_filename = "aria_module"

@0 = private unnamed_addr constant [19 x i8] c"For loop iteration\00", align 1
@1 = private unnamed_addr constant [19 x i8] c"For loop complete!\00", align 1

declare void @puts(ptr)

declare void @print(ptr)

define internal void @__aria_module_init() {
entry:
  br label %for_cond

for_cond:                                         ; preds = %for_body, %entry
  %i = phi i64 [ 0, %entry ], [ %next_iter, %for_body ]
  %for_cond1 = icmp slt i64 %i, 5
  br i1 %for_cond1, label %for_body, label %for_exit

for_body:                                         ; preds = %for_cond
  call void @puts(ptr @0)
  %next_iter = add i64 %i, 1
  br label %for_cond

for_exit:                                         ; preds = %for_cond
  call void @puts(ptr @1)
  ret void
}

define i64 @main() {
entry:
  call void @__aria_module_init()
  ret i64 0
}
