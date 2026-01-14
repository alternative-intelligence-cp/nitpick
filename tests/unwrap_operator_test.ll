; ModuleID = 'tests/unwrap_operator_test.aria'
source_filename = "tests/unwrap_operator_test.aria"

define internal { i32, ptr, i1 } @getSuccess() {
entry:
  ret { i32, ptr, i1 } { i32 42, ptr null, i1 false }
}

define internal { i32, ptr, i1 } @getError() {
entry:
  ret { i32, ptr, i1 } { i32 0, ptr inttoptr (i32 1 to ptr), i1 true }
}

define i32 @main() {
entry:
  call void @aria_gc_init(i64 0, i64 0)
  %calltmp = call { i32, ptr, i1 } @getSuccess()
  %is_error = extractvalue { i32, ptr, i1 } %calltmp, 2
  br i1 %is_error, label %error_block, label %success_block

error_block:                                      ; preds = %entry
  br label %merge_block

success_block:                                    ; preds = %entry
  %value = extractvalue { i32, ptr, i1 } %calltmp, 0
  br label %merge_block

merge_block:                                      ; preds = %success_block, %error_block
  %unwrap_result = phi i32 [ 0, %error_block ], [ %value, %success_block ]
  %calltmp1 = call { i32, ptr, i1 } @getError()
  %is_error2 = extractvalue { i32, ptr, i1 } %calltmp1, 2
  br i1 %is_error2, label %error_block3, label %success_block4

error_block3:                                     ; preds = %merge_block
  br label %merge_block5

success_block4:                                   ; preds = %merge_block
  %value6 = extractvalue { i32, ptr, i1 } %calltmp1, 0
  br label %merge_block5

merge_block5:                                     ; preds = %success_block4, %error_block3
  %unwrap_result7 = phi i32 [ 999, %error_block3 ], [ %value6, %success_block4 ]
  %addtmp = add i32 %unwrap_result, %unwrap_result7
  %calltmp8 = call { i32, ptr, i1 } @getSuccess()
  %is_error9 = extractvalue { i32, ptr, i1 } %calltmp8, 2
  br i1 %is_error9, label %error_block10, label %success_block11

error_block10:                                    ; preds = %merge_block5
  br label %merge_block12

success_block11:                                  ; preds = %merge_block5
  %value13 = extractvalue { i32, ptr, i1 } %calltmp8, 0
  br label %merge_block12

merge_block12:                                    ; preds = %success_block11, %error_block10
  %unwrap_result14 = phi i32 [ 1, %error_block10 ], [ %value13, %success_block11 ]
  %addtmp15 = add i32 %addtmp, %unwrap_result14
  ret i32 %addtmp15
}

declare void @aria_gc_init(i64, i64)
