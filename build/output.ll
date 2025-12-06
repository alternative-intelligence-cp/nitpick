; ModuleID = 'aria_module'
source_filename = "aria_module"

%result_int8 = type { i8, i8 }

@0 = private unnamed_addr constant [56 x i8] c"=== JIT Compilation - Function Pointer Casting Test ===\00", align 1
@1 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@2 = private unnamed_addr constant [40 x i8] c"Step 1: Allocating executable memory...\00", align 1
@3 = private unnamed_addr constant [46 x i8] c"        \E2\9C\93 Allocated wildx buffer (RW state)\00", align 1
@4 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@5 = private unnamed_addr constant [32 x i8] c"Step 2: Machine code generation\00", align 1
@6 = private unnamed_addr constant [40 x i8] c"        (Would write: MOV RAX, 42; RET)\00", align 1
@7 = private unnamed_addr constant [48 x i8] c"        (Requires array indexing - coming soon)\00", align 1
@8 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@9 = private unnamed_addr constant [52 x i8] c"Step 3: Sealing memory for execution (RW \E2\86\92 RX)...\00", align 1
@10 = private unnamed_addr constant [38 x i8] c"        \E2\9C\93 Memory sealed to RX state\00", align 1
@11 = private unnamed_addr constant [32 x i8] c"        (W^X security enforced)\00", align 1
@12 = private unnamed_addr constant [35 x i8] c"        \E2\9C\97 Failed to seal memory!\00", align 1
@13 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@14 = private unnamed_addr constant [50 x i8] c"Step 4: Wildx \E2\86\92 Function Pointer Infrastructure\00", align 1
@15 = private unnamed_addr constant [45 x i8] c"        (Casting infrastructure implemented)\00", align 1
@16 = private unnamed_addr constant [42 x i8] c"        (Full demo requires type aliases)\00", align 1
@17 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@18 = private unnamed_addr constant [45 x i8] c"Step 5: Function pointer ready for execution\00", align 1
@19 = private unnamed_addr constant [48 x i8] c"        (Execution requires valid machine code)\00", align 1
@20 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@21 = private unnamed_addr constant [38 x i8] c"=== Wildx Infrastructure Complete ===\00", align 1
@22 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@23 = private unnamed_addr constant [26 x i8] c"\E2\9C\93 Implemented Features:\00", align 1
@24 = private unnamed_addr constant [42 x i8] c"  [X] wildx keyword for executable memory\00", align 1
@25 = private unnamed_addr constant [52 x i8] c"  [X] Cross-platform allocation (mmap/VirtualAlloc)\00", align 1
@26 = private unnamed_addr constant [50 x i8] c"  [X] Memory protection intrinsics (protect_exec)\00", align 1
@27 = private unnamed_addr constant [31 x i8] c"  [X] W^X security enforcement\00", align 1
@28 = private unnamed_addr constant [41 x i8] c"  [X] Type-safe function pointer casting\00", align 1
@29 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@30 = private unnamed_addr constant [16 x i8] c"\E2\8F\B8 Next Steps:\00", align 1
@31 = private unnamed_addr constant [37 x i8] c"  [ ] Array indexing for code buffer\00", align 1
@32 = private unnamed_addr constant [38 x i8] c"  [ ] Complete x86-64 code generation\00", align 1
@33 = private unnamed_addr constant [36 x i8] c"  [ ] Actual JIT function execution\00", align 1
@34 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@35 = private unnamed_addr constant [31 x i8] c"Ready for Tsoding stream! \F0\9F\9A\80\00", align 1

declare void @puts(ptr)

declare void @print(ptr)

define internal void @__aria_module_init() {
entry:
  ret void
}

define internal %result_int8 @__user_main() {
entry:
  %seal_result = alloca i32, align 4
  call void @puts(ptr @0)
  call void @puts(ptr @1)
  call void @puts(ptr @2)
  %0 = call ptr @aria_alloc_exec(i64 8)
  %code_buffer = alloca ptr, align 8
  store ptr %0, ptr %code_buffer, align 8
  %1 = load ptr, ptr %code_buffer, align 8
  store i64 0, ptr %1, align 4
  call void @puts(ptr @3)
  call void @puts(ptr @4)
  call void @puts(ptr @5)
  call void @puts(ptr @6)
  call void @puts(ptr @7)
  call void @puts(ptr @8)
  call void @puts(ptr @9)
  %2 = load ptr, ptr %code_buffer, align 8
  %3 = load ptr, ptr %2, align 8
  %calltmp = call i32 @aria_mem_protect_exec(ptr %3, i64 4096)
  store i32 %calltmp, ptr %seal_result, align 4
  %4 = load i32, ptr %seal_result, align 4
  %5 = sext i32 %4 to i64
  %eqtmp = icmp eq i64 %5, 0
  br i1 %eqtmp, label %then, label %else

then:                                             ; preds = %entry
  call void @puts(ptr @10)
  call void @puts(ptr @11)
  br label %ifcont

else:                                             ; preds = %entry
  call void @puts(ptr @12)
  br label %ifcont

ifcont:                                           ; preds = %else, %then
  call void @puts(ptr @13)
  call void @puts(ptr @14)
  call void @puts(ptr @15)
  call void @puts(ptr @16)
  call void @puts(ptr @17)
  call void @puts(ptr @18)
  call void @puts(ptr @19)
  call void @puts(ptr @20)
  call void @puts(ptr @21)
  call void @puts(ptr @22)
  call void @puts(ptr @23)
  call void @puts(ptr @24)
  call void @puts(ptr @25)
  call void @puts(ptr @26)
  call void @puts(ptr @27)
  call void @puts(ptr @28)
  call void @puts(ptr @29)
  call void @puts(ptr @30)
  call void @puts(ptr @31)
  call void @puts(ptr @32)
  call void @puts(ptr @33)
  call void @puts(ptr @34)
  call void @puts(ptr @35)
  ret %result_int8 zeroinitializer
}

declare ptr @aria_alloc_exec(i64)

declare i32 @aria_mem_protect_exec(ptr, i64)

define i64 @main() {
entry:
  call void @__aria_module_init()
  %0 = call %result_int8 @__user_main()
  ret i64 0
}
