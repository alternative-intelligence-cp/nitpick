define i64 @main() {
entry:
  %0 = call %result_int8 @__user_main()
  %1 = extractvalue %result_int8 %0, 0
  %2 = sext i8 %1 to i64
  ret i64 %2
}

declare %result_int8 @__user_main()
