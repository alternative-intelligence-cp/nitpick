; ModuleID = 'tools/alive2/ir_tests/ternary_O0.ll'
source_filename = "tests/balanced_ternary_basic.aria"

; Function Attrs: noreturn
define noundef i32 @main(i32 %0, ptr %1) local_unnamed_addr #0 {
entry:
  tail call void @aria_gc_init(i64 0, i64 0)
  tail call void @aria_args_init(i32 %0, ptr %1)
  tail call void @aria_streams_init()
  %trit_add = tail call i8 @aria_trit_add(i8 1, i8 1)
  tail call void @exit(i32 0) #0
  unreachable
}

; Function Attrs: cold nofree noreturn
define noundef { i32, ptr, i8 } @failsafe(i32 %err) local_unnamed_addr #1 {
entry:
  tail call void @exit(i32 1) #4
  unreachable
}

declare void @aria_gc_init(i64, i64) local_unnamed_addr

declare void @aria_args_init(i32, ptr) local_unnamed_addr

declare void @aria_streams_init() local_unnamed_addr

declare i8 @aria_trit_add(i8, i8) local_unnamed_addr

; Function Attrs: nofree noreturn
declare void @exit(i32) local_unnamed_addr #2

; Function Attrs: mustprogress nofree norecurse nosync nounwind willreturn memory(none)
define noundef i32 @__balanced_ternary_basic_init() local_unnamed_addr #3 {
entry:
  ret i32 0
}

attributes #0 = { noreturn }
attributes #1 = { cold nofree noreturn }
attributes #2 = { nofree noreturn }
attributes #3 = { mustprogress nofree norecurse nosync nounwind willreturn memory(none) }
attributes #4 = { cold noreturn }
