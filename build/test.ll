; ModuleID = 'aria_module'
source_filename = "aria_module"

@0 = private unnamed_addr constant [25 x i8] c"All pick tests complete!\00", align 1

declare void @puts(ptr)

declare void @print(ptr)

define internal void @__aria_module_init() {
entry:
  ret void
}

define internal i8 @test_pick_exact(i8 %value) {
entry:
  %output = alloca i8, align 1
  %value1 = alloca i8, align 1
  store i8 %value, ptr %value1, align 1
  store i64 0, ptr %output, align 4
  %0 = load i64, ptr %value1, align 4
  %pick_eq = icmp eq i64 %0, 0
  br i1 %pick_eq, label %case_body_0, label %case_next_0

case_body_0:                                      ; preds = %entry
  store i64 10, ptr %output, align 4
  br label %pick_done

case_next_0:                                      ; preds = %entry
  %pick_eq2 = icmp eq i64 %0, 1
  br i1 %pick_eq2, label %case_body_1, label %case_next_1

case_body_1:                                      ; preds = %case_next_0
  store i64 20, ptr %output, align 4
  br label %pick_done

case_next_1:                                      ; preds = %case_next_0
  %pick_eq3 = icmp eq i64 %0, 2
  br i1 %pick_eq3, label %case_body_2, label %case_next_2

case_body_2:                                      ; preds = %case_next_1
  store i64 30, ptr %output, align 4
  br label %pick_done

case_next_2:                                      ; preds = %case_next_1
  br i1 true, label %case_body_3, label %case_next_3

case_body_3:                                      ; preds = %case_next_2
  store i64 -1, ptr %output, align 4
  br label %pick_done

case_next_3:                                      ; preds = %case_next_2
  br label %pick_done

pick_done:                                        ; preds = %case_next_3, %case_body_3, %case_body_2, %case_body_1, %case_body_0
  %1 = load i8, ptr %output, align 1
  ret i8 %1
}

define internal i8 @test_pick_less_than(i8 %value) {
entry:
  %output = alloca i8, align 1
  %value1 = alloca i8, align 1
  store i8 %value, ptr %value1, align 1
  store i64 0, ptr %output, align 4
  %0 = load i64, ptr %value1, align 4
  %pick_lt = icmp slt i64 %0, 5
  br i1 %pick_lt, label %case_body_0, label %case_next_0

case_body_0:                                      ; preds = %entry
  store i64 1, ptr %output, align 4
  br label %pick_done

case_next_0:                                      ; preds = %entry
  %pick_lt2 = icmp slt i64 %0, 10
  br i1 %pick_lt2, label %case_body_1, label %case_next_1

case_body_1:                                      ; preds = %case_next_0
  store i64 2, ptr %output, align 4
  br label %pick_done

case_next_1:                                      ; preds = %case_next_0
  br i1 true, label %case_body_2, label %case_next_2

case_body_2:                                      ; preds = %case_next_1
  store i64 3, ptr %output, align 4
  br label %pick_done

case_next_2:                                      ; preds = %case_next_1
  br label %pick_done

pick_done:                                        ; preds = %case_next_2, %case_body_2, %case_body_1, %case_body_0
  %1 = load i8, ptr %output, align 1
  ret i8 %1
}

define internal i8 @test_pick_greater_than(i8 %value) {
entry:
  %output = alloca i8, align 1
  %value1 = alloca i8, align 1
  store i8 %value, ptr %value1, align 1
  store i64 0, ptr %output, align 4
  %0 = load i64, ptr %value1, align 4
  %pick_gt = icmp sgt i64 %0, 100
  br i1 %pick_gt, label %case_body_0, label %case_next_0

case_body_0:                                      ; preds = %entry
  store i64 1, ptr %output, align 4
  br label %pick_done

case_next_0:                                      ; preds = %entry
  %pick_gt2 = icmp sgt i64 %0, 50
  br i1 %pick_gt2, label %case_body_1, label %case_next_1

case_body_1:                                      ; preds = %case_next_0
  store i64 2, ptr %output, align 4
  br label %pick_done

case_next_1:                                      ; preds = %case_next_0
  %pick_gt3 = icmp sgt i64 %0, 10
  br i1 %pick_gt3, label %case_body_2, label %case_next_2

case_body_2:                                      ; preds = %case_next_1
  store i64 3, ptr %output, align 4
  br label %pick_done

case_next_2:                                      ; preds = %case_next_1
  br i1 true, label %case_body_3, label %case_next_3

case_body_3:                                      ; preds = %case_next_2
  store i64 4, ptr %output, align 4
  br label %pick_done

case_next_3:                                      ; preds = %case_next_2
  br label %pick_done

pick_done:                                        ; preds = %case_next_3, %case_body_3, %case_body_2, %case_body_1, %case_body_0
  %1 = load i8, ptr %output, align 1
  ret i8 %1
}

define internal i8 @test_pick_less_equal(i8 %value) {
entry:
  %output = alloca i8, align 1
  %value1 = alloca i8, align 1
  store i8 %value, ptr %value1, align 1
  store i64 0, ptr %output, align 4
  %0 = load i64, ptr %value1, align 4
  %pick_le = icmp sle i64 %0, 5
  br i1 %pick_le, label %case_body_0, label %case_next_0

case_body_0:                                      ; preds = %entry
  store i64 10, ptr %output, align 4
  br label %pick_done

case_next_0:                                      ; preds = %entry
  %pick_le2 = icmp sle i64 %0, 10
  br i1 %pick_le2, label %case_body_1, label %case_next_1

case_body_1:                                      ; preds = %case_next_0
  store i64 20, ptr %output, align 4
  br label %pick_done

case_next_1:                                      ; preds = %case_next_0
  br i1 true, label %case_body_2, label %case_next_2

case_body_2:                                      ; preds = %case_next_1
  store i64 30, ptr %output, align 4
  br label %pick_done

case_next_2:                                      ; preds = %case_next_1
  br label %pick_done

pick_done:                                        ; preds = %case_next_2, %case_body_2, %case_body_1, %case_body_0
  %1 = load i8, ptr %output, align 1
  ret i8 %1
}

define internal i8 @test_pick_greater_equal(i8 %value) {
entry:
  %output = alloca i8, align 1
  %value1 = alloca i8, align 1
  store i8 %value, ptr %value1, align 1
  store i64 0, ptr %output, align 4
  %0 = load i64, ptr %value1, align 4
  %pick_ge = icmp sge i64 %0, 100
  br i1 %pick_ge, label %case_body_0, label %case_next_0

case_body_0:                                      ; preds = %entry
  store i64 100, ptr %output, align 4
  br label %pick_done

case_next_0:                                      ; preds = %entry
  %pick_ge2 = icmp sge i64 %0, 50
  br i1 %pick_ge2, label %case_body_1, label %case_next_1

case_body_1:                                      ; preds = %case_next_0
  store i64 50, ptr %output, align 4
  br label %pick_done

case_next_1:                                      ; preds = %case_next_0
  %pick_ge3 = icmp sge i64 %0, 0
  br i1 %pick_ge3, label %case_body_2, label %case_next_2

case_body_2:                                      ; preds = %case_next_1
  store i64 0, ptr %output, align 4
  br label %pick_done

case_next_2:                                      ; preds = %case_next_1
  br i1 true, label %case_body_3, label %case_next_3

case_body_3:                                      ; preds = %case_next_2
  store i64 -1, ptr %output, align 4
  br label %pick_done

case_next_3:                                      ; preds = %case_next_2
  br label %pick_done

pick_done:                                        ; preds = %case_next_3, %case_body_3, %case_body_2, %case_body_1, %case_body_0
  %1 = load i8, ptr %output, align 1
  ret i8 %1
}

define internal i8 @test_pick_range_inclusive(i8 %value) {
entry:
  %output = alloca i8, align 1
  %value1 = alloca i8, align 1
  store i8 %value, ptr %value1, align 1
  store i64 0, ptr %output, align 4
  %0 = load i64, ptr %value1, align 4
  %range_ge = icmp sge i64 %0, 0
  %range_le = icmp sle i64 %0, 5
  %range_match = and i1 %range_ge, %range_le
  br i1 %range_match, label %case_body_0, label %case_next_0

case_body_0:                                      ; preds = %entry
  store i64 1, ptr %output, align 4
  br label %pick_done

case_next_0:                                      ; preds = %entry
  %range_ge2 = icmp sge i64 %0, 6
  %range_le3 = icmp sle i64 %0, 10
  %range_match4 = and i1 %range_ge2, %range_le3
  br i1 %range_match4, label %case_body_1, label %case_next_1

case_body_1:                                      ; preds = %case_next_0
  store i64 2, ptr %output, align 4
  br label %pick_done

case_next_1:                                      ; preds = %case_next_0
  %range_ge5 = icmp sge i64 %0, 11
  %range_le6 = icmp sle i64 %0, 20
  %range_match7 = and i1 %range_ge5, %range_le6
  br i1 %range_match7, label %case_body_2, label %case_next_2

case_body_2:                                      ; preds = %case_next_1
  store i64 3, ptr %output, align 4
  br label %pick_done

case_next_2:                                      ; preds = %case_next_1
  br i1 true, label %case_body_3, label %case_next_3

case_body_3:                                      ; preds = %case_next_2
  store i64 4, ptr %output, align 4
  br label %pick_done

case_next_3:                                      ; preds = %case_next_2
  br label %pick_done

pick_done:                                        ; preds = %case_next_3, %case_body_3, %case_body_2, %case_body_1, %case_body_0
  %1 = load i8, ptr %output, align 1
  ret i8 %1
}

define internal i8 @test_pick_range_exclusive(i8 %value) {
entry:
  %output = alloca i8, align 1
  %value1 = alloca i8, align 1
  store i8 %value, ptr %value1, align 1
  store i64 0, ptr %output, align 4
  %0 = load i64, ptr %value1, align 4
  %range_ge = icmp sge i64 %0, 0
  %range_lt = icmp slt i64 %0, 5
  %range_match = and i1 %range_ge, %range_lt
  br i1 %range_match, label %case_body_0, label %case_next_0

case_body_0:                                      ; preds = %entry
  store i64 1, ptr %output, align 4
  br label %pick_done

case_next_0:                                      ; preds = %entry
  %range_ge2 = icmp sge i64 %0, 5
  %range_lt3 = icmp slt i64 %0, 10
  %range_match4 = and i1 %range_ge2, %range_lt3
  br i1 %range_match4, label %case_body_1, label %case_next_1

case_body_1:                                      ; preds = %case_next_0
  store i64 2, ptr %output, align 4
  br label %pick_done

case_next_1:                                      ; preds = %case_next_0
  br i1 true, label %case_body_2, label %case_next_2

case_body_2:                                      ; preds = %case_next_1
  store i64 3, ptr %output, align 4
  br label %pick_done

case_next_2:                                      ; preds = %case_next_1
  br label %pick_done

pick_done:                                        ; preds = %case_next_2, %case_body_2, %case_body_1, %case_body_0
  %1 = load i8, ptr %output, align 1
  ret i8 %1
}

define internal i8 @test_pick_wildcard(i8 %value) {
entry:
  %output = alloca i8, align 1
  %value1 = alloca i8, align 1
  store i8 %value, ptr %value1, align 1
  store i64 0, ptr %output, align 4
  %0 = load i64, ptr %value1, align 4
  %pick_eq = icmp eq i64 %0, 0
  br i1 %pick_eq, label %case_body_0, label %case_next_0

case_body_0:                                      ; preds = %entry
  store i64 10, ptr %output, align 4
  br label %pick_done

case_next_0:                                      ; preds = %entry
  %pick_eq2 = icmp eq i64 %0, 1
  br i1 %pick_eq2, label %case_body_1, label %case_next_1

case_body_1:                                      ; preds = %case_next_0
  store i64 20, ptr %output, align 4
  br label %pick_done

case_next_1:                                      ; preds = %case_next_0
  br i1 true, label %case_body_2, label %case_next_2

case_body_2:                                      ; preds = %case_next_1
  store i64 999, ptr %output, align 4
  br label %pick_done

case_next_2:                                      ; preds = %case_next_1
  br label %pick_done

pick_done:                                        ; preds = %case_next_2, %case_body_2, %case_body_1, %case_body_0
  %1 = load i8, ptr %output, align 1
  ret i8 %1
}

define internal i8 @test_pick_multi(i8 %value) {
entry:
  %output = alloca i8, align 1
  %value1 = alloca i8, align 1
  store i8 %value, ptr %value1, align 1
  store i64 0, ptr %output, align 4
  %0 = load i64, ptr %value1, align 4
  %pick_eq = icmp eq i64 %0, 0
  br i1 %pick_eq, label %case_body_0, label %case_next_0

case_body_0:                                      ; preds = %entry
  store i64 1, ptr %output, align 4
  br label %pick_done

case_next_0:                                      ; preds = %entry
  %pick_eq2 = icmp eq i64 %0, 1
  br i1 %pick_eq2, label %case_body_1, label %case_next_1

case_body_1:                                      ; preds = %case_next_0
  store i64 1, ptr %output, align 4
  br label %pick_done

case_next_1:                                      ; preds = %case_next_0
  %pick_eq3 = icmp eq i64 %0, 2
  br i1 %pick_eq3, label %case_body_2, label %case_next_2

case_body_2:                                      ; preds = %case_next_1
  store i64 1, ptr %output, align 4
  br label %pick_done

case_next_2:                                      ; preds = %case_next_1
  %pick_eq4 = icmp eq i64 %0, 3
  br i1 %pick_eq4, label %case_body_3, label %case_next_3

case_body_3:                                      ; preds = %case_next_2
  store i64 2, ptr %output, align 4
  br label %pick_done

case_next_3:                                      ; preds = %case_next_2
  %pick_eq5 = icmp eq i64 %0, 4
  br i1 %pick_eq5, label %case_body_4, label %case_next_4

case_body_4:                                      ; preds = %case_next_3
  store i64 2, ptr %output, align 4
  br label %pick_done

case_next_4:                                      ; preds = %case_next_3
  %pick_eq6 = icmp eq i64 %0, 5
  br i1 %pick_eq6, label %case_body_5, label %case_next_5

case_body_5:                                      ; preds = %case_next_4
  store i64 2, ptr %output, align 4
  br label %pick_done

case_next_5:                                      ; preds = %case_next_4
  br i1 true, label %case_body_6, label %case_next_6

case_body_6:                                      ; preds = %case_next_5
  store i64 3, ptr %output, align 4
  br label %pick_done

case_next_6:                                      ; preds = %case_next_5
  br label %pick_done

pick_done:                                        ; preds = %case_next_6, %case_body_6, %case_body_5, %case_body_4, %case_body_3, %case_body_2, %case_body_1, %case_body_0
  %1 = load i8, ptr %output, align 1
  ret i8 %1
}

define internal i8 @test_pick_no_wildcard(i8 %value) {
entry:
  %output = alloca i8, align 1
  %value1 = alloca i8, align 1
  store i8 %value, ptr %value1, align 1
  store i64 -1, ptr %output, align 4
  %0 = load i64, ptr %value1, align 4
  %pick_eq = icmp eq i64 %0, 0
  br i1 %pick_eq, label %case_body_0, label %case_next_0

case_body_0:                                      ; preds = %entry
  store i64 0, ptr %output, align 4
  br label %pick_done

case_next_0:                                      ; preds = %entry
  %pick_eq2 = icmp eq i64 %0, 1
  br i1 %pick_eq2, label %case_body_1, label %case_next_1

case_body_1:                                      ; preds = %case_next_0
  store i64 1, ptr %output, align 4
  br label %pick_done

case_next_1:                                      ; preds = %case_next_0
  %pick_eq3 = icmp eq i64 %0, 2
  br i1 %pick_eq3, label %case_body_2, label %case_next_2

case_body_2:                                      ; preds = %case_next_1
  store i64 2, ptr %output, align 4
  br label %pick_done

case_next_2:                                      ; preds = %case_next_1
  br label %pick_done

pick_done:                                        ; preds = %case_next_2, %case_body_2, %case_body_1, %case_body_0
  %1 = load i8, ptr %output, align 1
  ret i8 %1
}

define internal i8 @test_pick_nested(i8 %outer) {
entry:
  %inner7 = alloca i8, align 1
  %inner = alloca i8, align 1
  %output = alloca i8, align 1
  %outer1 = alloca i8, align 1
  store i8 %outer, ptr %outer1, align 1
  store i64 0, ptr %output, align 4
  %0 = load i64, ptr %outer1, align 4
  %pick_eq = icmp eq i64 %0, 0
  br i1 %pick_eq, label %case_body_0, label %case_next_04

case_body_0:                                      ; preds = %entry
  store i64 5, ptr %inner, align 4
  %1 = load i8, ptr %inner, align 1
  %pick_eq3 = icmp eq i8 %1, 5
  br i1 %pick_eq3, label %case_body_02, label %case_next_0

case_body_02:                                     ; preds = %case_body_0
  store i64 100, ptr %output, align 4
  br label %pick_done

case_next_0:                                      ; preds = %case_body_0
  br i1 true, label %case_body_1, label %case_next_1

case_body_1:                                      ; preds = %case_next_0
  store i64 -1, ptr %output, align 4
  br label %pick_done

case_next_1:                                      ; preds = %case_next_0
  br label %pick_done

pick_done:                                        ; preds = %case_next_1, %case_body_1, %case_body_02
  br label %pick_done15

case_next_04:                                     ; preds = %entry
  %pick_eq6 = icmp eq i64 %0, 1
  br i1 %pick_eq6, label %case_body_15, label %case_next_114

case_body_15:                                     ; preds = %case_next_04
  store i64 10, ptr %inner7, align 4
  %2 = load i8, ptr %inner7, align 1
  %pick_eq9 = icmp eq i8 %2, 10
  br i1 %pick_eq9, label %case_body_08, label %case_next_010

case_body_08:                                     ; preds = %case_body_15
  store i64 200, ptr %output, align 4
  br label %pick_done13

case_next_010:                                    ; preds = %case_body_15
  br i1 true, label %case_body_111, label %case_next_112

case_body_111:                                    ; preds = %case_next_010
  store i64 -1, ptr %output, align 4
  br label %pick_done13

case_next_112:                                    ; preds = %case_next_010
  br label %pick_done13

pick_done13:                                      ; preds = %case_next_112, %case_body_111, %case_body_08
  br label %pick_done15

case_next_114:                                    ; preds = %case_next_04
  br i1 true, label %case_body_2, label %case_next_2

case_body_2:                                      ; preds = %case_next_114
  store i64 0, ptr %output, align 4
  br label %pick_done15

case_next_2:                                      ; preds = %case_next_114
  br label %pick_done15

pick_done15:                                      ; preds = %case_next_2, %case_body_2, %pick_done13, %pick_done
  %3 = load i8, ptr %output, align 1
  ret i8 %3
}

define internal i8 @test_pick_complex(i8 %value) {
entry:
  %output = alloca i8, align 1
  %value1 = alloca i8, align 1
  store i8 %value, ptr %value1, align 1
  store i64 0, ptr %output, align 4
  %0 = load i64, ptr %value1, align 4
  %range_ge = icmp sge i64 %0, 0
  %range_le = icmp sle i64 %0, 9
  %range_match = and i1 %range_ge, %range_le
  br i1 %range_match, label %case_body_0, label %case_next_0

case_body_0:                                      ; preds = %entry
  store i64 1, ptr %output, align 4
  br label %pick_done

case_next_0:                                      ; preds = %entry
  %range_ge2 = icmp sge i64 %0, 10
  %range_le3 = icmp sle i64 %0, 99
  %range_match4 = and i1 %range_ge2, %range_le3
  br i1 %range_match4, label %case_body_1, label %case_next_1

case_body_1:                                      ; preds = %case_next_0
  store i64 2, ptr %output, align 4
  br label %pick_done

case_next_1:                                      ; preds = %case_next_0
  %range_ge5 = icmp sge i64 %0, 100
  %range_le6 = icmp sle i64 %0, 127
  %range_match7 = and i1 %range_ge5, %range_le6
  br i1 %range_match7, label %case_body_2, label %case_next_2

case_body_2:                                      ; preds = %case_next_1
  store i64 3, ptr %output, align 4
  br label %pick_done

case_next_2:                                      ; preds = %case_next_1
  br i1 true, label %case_body_3, label %case_next_3

case_body_3:                                      ; preds = %case_next_2
  store i64 4, ptr %output, align 4
  br label %pick_done

case_next_3:                                      ; preds = %case_next_2
  br label %pick_done

pick_done:                                        ; preds = %case_next_3, %case_body_3, %case_body_2, %case_body_1, %case_body_0
  %1 = load i8, ptr %output, align 1
  ret i8 %1
}

define internal i8 @test_pick_sign(i8 %value) {
entry:
  %output = alloca i8, align 1
  %value1 = alloca i8, align 1
  store i8 %value, ptr %value1, align 1
  store i64 0, ptr %output, align 4
  %0 = load i64, ptr %value1, align 4
  %pick_lt = icmp slt i64 %0, 0
  br i1 %pick_lt, label %case_body_0, label %case_next_0

case_body_0:                                      ; preds = %entry
  store i64 -1, ptr %output, align 4
  br label %pick_done

case_next_0:                                      ; preds = %entry
  %pick_eq = icmp eq i64 %0, 0
  br i1 %pick_eq, label %case_body_1, label %case_next_1

case_body_1:                                      ; preds = %case_next_0
  store i64 0, ptr %output, align 4
  br label %pick_done

case_next_1:                                      ; preds = %case_next_0
  %pick_gt = icmp sgt i64 %0, 0
  br i1 %pick_gt, label %case_body_2, label %case_next_2

case_body_2:                                      ; preds = %case_next_1
  store i64 1, ptr %output, align 4
  br label %pick_done

case_next_2:                                      ; preds = %case_next_1
  br label %pick_done

pick_done:                                        ; preds = %case_next_2, %case_body_2, %case_body_1, %case_body_0
  %1 = load i8, ptr %output, align 1
  ret i8 %1
}

define internal i8 @__user_main() {
entry:
  %r10 = alloca i8, align 1
  %r9 = alloca i8, align 1
  %r8 = alloca i8, align 1
  %r7 = alloca i8, align 1
  %r6 = alloca i8, align 1
  %r5 = alloca i8, align 1
  %r4 = alloca i8, align 1
  %r3 = alloca i8, align 1
  %r2 = alloca i8, align 1
  %r1 = alloca i8, align 1
  %calltmp = call i8 @test_pick_exact(i8 0)
  store i8 %calltmp, ptr %r1, align 1
  %calltmp1 = call i8 @test_pick_exact(i8 1)
  store i8 %calltmp1, ptr %r2, align 1
  %calltmp2 = call i8 @test_pick_exact(i8 99)
  store i8 %calltmp2, ptr %r3, align 1
  %calltmp3 = call i8 @test_pick_less_than(i8 3)
  store i8 %calltmp3, ptr %r4, align 1
  %calltmp4 = call i8 @test_pick_greater_than(i8 60)
  store i8 %calltmp4, ptr %r5, align 1
  %calltmp5 = call i8 @test_pick_range_inclusive(i8 3)
  store i8 %calltmp5, ptr %r6, align 1
  %calltmp6 = call i8 @test_pick_range_exclusive(i8 5)
  store i8 %calltmp6, ptr %r7, align 1
  %calltmp7 = call i8 @test_pick_sign(i8 -5)
  store i8 %calltmp7, ptr %r8, align 1
  %calltmp8 = call i8 @test_pick_sign(i8 0)
  store i8 %calltmp8, ptr %r9, align 1
  %calltmp9 = call i8 @test_pick_sign(i8 10)
  store i8 %calltmp9, ptr %r10, align 1
  call void @puts(ptr @0)
  ret i8 0
}

define i64 @main() {
entry:
  call void @__aria_module_init()
  %0 = call i8 @__user_main()
  %1 = sext i8 %0 to i64
  ret i64 %1
}
