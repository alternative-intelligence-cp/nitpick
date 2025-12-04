; ModuleID = 'aria_module'
source_filename = "aria_module"

%result_int8 = type { i8, i8 }

declare void @puts(ptr)

declare void @print(ptr)

define internal void @__aria_module_init() {
entry:
  ret void
}

define internal %result_int8 @process_number(i8 %x) {
entry:
  %x1 = alloca i8, align 1
  store i8 %x, ptr %x1, align 1
  %0 = load i64, ptr %x1, align 4
  br label %pick_label_positive

pick_label_positive:                              ; preds = %entry
  br label %pick_done

pick_label_zero:                                  ; preds = %case_next_0
  br label %pick_done

pick_label_negative:                              ; preds = %case_next_1
  br label %pick_done

pick_label_done:                                  ; preds = %case_next_2
  %1 = load i64, ptr %x1, align 4
  ret i64 %1

case_next_0:                                      ; No predecessors!
  br label %pick_label_zero

case_next_1:                                      ; No predecessors!
  br label %pick_label_negative

case_next_2:                                      ; No predecessors!
  br label %pick_label_done

case_next_3:                                      ; No predecessors!
  br label %pick_done

pick_done:                                        ; preds = %case_next_3, %pick_label_negative, %pick_label_zero, %pick_label_positive
  ret %result_int8 zeroinitializer
}

define internal %result_int8 @__user_main() {
entry:
  %calltmp = call %result_int8 @process_number(i8 5)
  ret %result_int8 %calltmp
}

define i64 @main() {
entry:
  call void @__aria_module_init()
  %0 = call %result_int8 @__user_main()
  ret i64 0
}
