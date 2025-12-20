; ModuleID = 'tests/ternary_debug_value.aria'
source_filename = "tests/ternary_debug_value.aria"

define i64 @main() {
entry:
  %t_add = alloca i2, align 1
  store i2 1, ptr %t_add, align 1
  %t_one = alloca i2, align 1
  store i2 1, ptr %t_one, align 1
  %t_sum = alloca i2, align 1
  %t_add1 = load i2, ptr %t_add, align 1
  %t_one2 = load i2, ptr %t_one, align 1
  %0 = add i2 %t_add1, %t_one2
  %1 = icmp sgt i2 %0, 1
  %2 = select i1 %1, i2 1, i2 %0
  %3 = icmp slt i2 %2, -1
  %4 = select i1 %3, i2 -1, i2 %2
  store i2 %4, ptr %t_sum, align 1
  %t_sum3 = load i2, ptr %t_sum, align 1
  %ret_sext = sext i2 %t_sum3 to i64
  ret i64 %ret_sext
}

define i32 @__ternary_debug_value_init() {
entry:
  ret i32 0
}
