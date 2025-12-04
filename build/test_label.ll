; ModuleID = 'aria_module'
source_filename = "aria_module"

%result_int8 = type { i8, i8 }

declare void @puts(ptr)

declare void @print(ptr)

define internal void @__aria_module_init() {
entry:
  ret void
}

define internal %result_int8 @test_labels(i8 %x) {
entry:
  %val3 = alloca i8, align 1
  %val2 = alloca i8, align 1
  %val = alloca i8, align 1
  %x1 = alloca i8, align 1
  store i8 %x, ptr %x1, align 1
  %0 = load i64, ptr %x1, align 4
  br label %pick_label_positive

pick_label_positive:                              ; preds = %entry
  store i64 1, ptr %val, align 4
  %1 = load i8, ptr %val, align 1
  ret i8 %1

pick_label_zero:                                  ; preds = %case_next_0
  store i64 0, ptr %val2, align 4
  %2 = load i8, ptr %val2, align 1
  ret i8 %2

pick_label_negative:                              ; preds = %case_next_1
  store i64 -1, ptr %val3, align 4
  %3 = load i8, ptr %val3, align 1
  ret i8 %3

case_next_0:                                      ; No predecessors!
  br label %pick_label_zero

case_next_1:                                      ; No predecessors!
  br label %pick_label_negative

case_next_2:                                      ; No predecessors!
  br label %pick_done

pick_done:                                        ; preds = %case_next_2
  ret %result_int8 zeroinitializer
}

define internal %result_int8 @__user_main() {
entry:
  ret i64 0
}

define i64 @main() {
entry:
  call void @__aria_module_init()
  %0 = call %result_int8 @__user_main()
  ret i64 0
}
