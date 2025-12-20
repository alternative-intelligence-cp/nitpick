; ModuleID = 'tests/feature_validation/modules_ir_test.aria'
source_filename = "tests/feature_validation/modules_ir_test.aria"

define i64 @math.add(i64 %a, i64 %b) {
entry:
  %addtmp = add i64 %a, %b
  ret i64 %addtmp
}

define i64 @math.multiply(i64 %a, i64 %b) {
entry:
  %multmp = mul i64 %a, %b
  ret i64 %multmp
}

define internal i64 @math.helper(i64 %x) {
entry:
  %addtmp = add i64 %x, i32 10
  ret i64 %addtmp
}

define i64 @utils.double(i64 %x) {
entry:
  %multmp = mul i64 %x, i32 2
  ret i64 %multmp
}

define i64 @network.http.get(i64 %port) {
entry:
  ret i64 %port
}

define i64 @network.http.post(i64 %port) {
entry:
  %addtmp = add i64 %port, i32 1
  ret i64 %addtmp
}

define i64 @network.tcp.connect(i64 %port) {
entry:
  %addtmp = add i64 %port, i32 100
  ret i64 %addtmp
}

define internal i64 @main() {
entry:
  ret i32 42
}
