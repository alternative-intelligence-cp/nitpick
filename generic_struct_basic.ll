; ModuleID = 'tests/generic_struct_basic.aria'
source_filename = "tests/generic_struct_basic.aria"

%_Aria_M_Box_f9d0c88f42834344_int64 = type { i64 }
%_Aria_M_Box_704be0d8faaffc58_string = type { i0 }
%_Aria_M_Result_64903478ef1bd0eb_string_int64 = type { i64, i0, i1 }

define i64 @main() {
entry:
  %intBox = alloca %_Aria_M_Box_f9d0c88f42834344_int64, align 8
  %strBox = alloca %_Aria_M_Box_704be0d8faaffc58_string, align 8
  %result = alloca %_Aria_M_Result_64903478ef1bd0eb_string_int64, align 8
  ret i32 0
}

define i32 @__generic_struct_basic_init() {
entry:
  ret i32 0
}
