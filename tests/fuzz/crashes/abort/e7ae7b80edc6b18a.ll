; ModuleID = 'tests/fuzz/crashes/abort/e7ae7b80edc6b18a.aria'
source_filename = "tests/fuzz/crashes/abort/e7ae7b80edc6b18a.aria"

define i32 @main() {
entry:
  call void @aria_gc_init(i64 0, i64 0)
  %await.save = call token @llvm.coro.save(i64 0)
  %await.suspend = call i8 @llvm.coro.suspend(token %await.save, i1 false)
  switch i8 %await.suspend, label %await.resume [
    i8 1, label %await.cleanup
  ]

await.resume:                                     ; preds = %entry
  ret i32 0

await.cleanup:                                    ; preds = %entry
  unreachable
}

declare void @aria_gc_init(i64, i64)

declare void @llvm.coro.resume(ptr)

; Function Attrs: nomerge nounwind
declare token @llvm.coro.save(ptr) #0

; Function Attrs: nounwind
declare i8 @llvm.coro.suspend(token, i1) #1

define i32 @__e7ae7b80edc6b18a_init() {
entry:
  ret i32 0
}

attributes #0 = { nomerge nounwind }
attributes #1 = { nounwind }
