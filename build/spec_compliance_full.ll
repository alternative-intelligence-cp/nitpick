; ModuleID = 'aria_module'
source_filename = "aria_module"

%result_int8 = type { i8, i8 }

@i = internal global i8 9
@0 = private unnamed_addr constant [9 x i8] c"whats up\00", align 1
@str = internal global ptr @0
@1 = private unnamed_addr constant [9 x i8] c"whats up\00", align 1
@c = internal global i8 0
@closureTest = internal global i8 2

declare void @puts(ptr)

declare void @print(ptr)

define internal void @__aria_module_init() {
entry:
  ret void
}

define internal %result_int8 @test_pick(i8 %c) {
entry:
  %c1 = alloca i8, align 1
  store i8 %c, ptr %c1, align 1
  %0 = load i64, ptr %c1, align 4
  %pick_lt = icmp slt i64 %0, 9
  br i1 %pick_lt, label %case_body_0, label %case_next_0

pick_label_fail:                                  ; preds = %case_next_3
  br label %pick_done

pick_label_success:                               ; preds = %case_next_4
  br label %pick_done

pick_label_err:                                   ; preds = %case_next_5
  br label %pick_done

pick_label_done:                                  ; preds = %case_next_6
  ret i64 1

case_body_0:                                      ; preds = %entry
  br label %pick_done

case_next_0:                                      ; preds = %entry
  %pick_gt = icmp sgt i64 %0, 9
  br i1 %pick_gt, label %case_body_1, label %case_next_1

case_body_1:                                      ; preds = %case_next_0
  br label %pick_done

case_next_1:                                      ; preds = %case_next_0
  %pick_eq = icmp eq i64 %0, 9
  br i1 %pick_eq, label %case_body_2, label %case_next_2

case_body_2:                                      ; preds = %case_next_1
  br label %pick_done

case_next_2:                                      ; preds = %case_next_1
  br i1 true, label %case_body_3, label %case_next_3

case_body_3:                                      ; preds = %case_next_2
  br label %pick_done

case_next_3:                                      ; preds = %case_next_2
  br label %pick_label_fail

case_next_4:                                      ; No predecessors!
  br label %pick_label_success

case_next_5:                                      ; No predecessors!
  br label %pick_label_err

case_next_6:                                      ; No predecessors!
  br label %pick_label_done

case_next_7:                                      ; No predecessors!
  br label %pick_done

pick_done:                                        ; preds = %case_next_7, %pick_label_err, %pick_label_success, %pick_label_fail, %case_body_3, %case_body_2, %case_body_1, %case_body_0
  ret %result_int8 zeroinitializer
}

define internal %result_int8 @test(i8 %a, i8 %b) {
entry:
  %a1 = alloca i8, align 1
  store i8 %a, ptr %a1, align 1
  %b2 = alloca i8, align 1
  store i8 %b, ptr %b2, align 1
  %0 = load i64, ptr %a1, align 4
  %1 = load i64, ptr %b2, align 4
  %multmp = mul i64 %0, %1
  %2 = load i64, ptr @closureTest, align 4
  %multmp3 = mul i64 %multmp, %2
  ret i64 %multmp3
}

define i64 @main() {
entry:
  call void @__aria_module_init()
  ret i64 0
}
