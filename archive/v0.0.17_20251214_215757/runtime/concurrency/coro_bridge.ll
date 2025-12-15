; LLVM coroutine bridge for scheduler integration
; This function is callable from C++ and invokes llvm.coro.resume

declare void @llvm.coro.resume(ptr)

define void @aria_coro_resume_bridge(ptr %coro_handle) {
entry:
    ; Directly call the LLVM coroutine resume intrinsic
    call void @llvm.coro.resume(ptr %coro_handle)
    ret void
}
