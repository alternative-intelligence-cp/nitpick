; ModuleID = 'tests/balanced_ternary_basic.aria'
source_filename = "tests/balanced_ternary_basic.aria"

%Numeric_vtable_t = type {}

@Numeric_vtable_int8 = internal constant %Numeric_vtable_t zeroinitializer
@Numeric_vtable_int16 = internal constant %Numeric_vtable_t zeroinitializer
@Numeric_vtable_int32 = internal constant %Numeric_vtable_t zeroinitializer
@Numeric_vtable_int64 = internal constant %Numeric_vtable_t zeroinitializer
@Numeric_vtable_uint8 = internal constant %Numeric_vtable_t zeroinitializer
@Numeric_vtable_uint16 = internal constant %Numeric_vtable_t zeroinitializer
@Numeric_vtable_uint32 = internal constant %Numeric_vtable_t zeroinitializer
@Numeric_vtable_uint64 = internal constant %Numeric_vtable_t zeroinitializer
@Numeric_vtable_tbb8 = internal constant %Numeric_vtable_t zeroinitializer
@Numeric_vtable_tbb16 = internal constant %Numeric_vtable_t zeroinitializer
@Numeric_vtable_tbb32 = internal constant %Numeric_vtable_t zeroinitializer
@Numeric_vtable_tbb64 = internal constant %Numeric_vtable_t zeroinitializer
@Numeric_vtable_frac8 = internal constant %Numeric_vtable_t zeroinitializer
@Numeric_vtable_frac16 = internal constant %Numeric_vtable_t zeroinitializer
@Numeric_vtable_frac32 = internal constant %Numeric_vtable_t zeroinitializer
@Numeric_vtable_frac64 = internal constant %Numeric_vtable_t zeroinitializer
@Numeric_vtable_tfp32 = internal constant %Numeric_vtable_t zeroinitializer
@Numeric_vtable_tfp64 = internal constant %Numeric_vtable_t zeroinitializer
@Numeric_vtable_fix256 = internal constant %Numeric_vtable_t zeroinitializer
@Numeric_vtable_flt32 = internal constant %Numeric_vtable_t zeroinitializer
@Numeric_vtable_flt64 = internal constant %Numeric_vtable_t zeroinitializer

define i32 @main(i32 %0, ptr %1) {
entry:
  call void @aria_gc_init(i64 0, i64 0)
  call void @aria_args_init(i32 %0, ptr %1)
  call void @aria_streams_init()
  %neg = alloca i8, align 1
  store i8 -1, ptr %neg, align 1
  %zero = alloca i8, align 1
  store i8 0, ptr %zero, align 1
  %pos = alloca i8, align 1
  store i8 1, ptr %pos, align 1
  %small = alloca i16, align 2
  store i16 5, ptr %small, align 2
  %medium = alloca i16, align 2
  store i16 100, ptr %medium, align 2
  %sum = alloca i8, align 1
  %pos1 = load i8, ptr %pos, align 1
  %pos2 = load i8, ptr %pos, align 1
  %trit_add = call i8 @aria_trit_add(i8 %pos1, i8 %pos2)
  store i8 %trit_add, ptr %sum, align 1
  %eq = alloca i1, align 1
  %zero3 = load i8, ptr %zero, align 1
  %eqtmp = icmp eq i8 %zero3, 0
  store i1 %eqtmp, ptr %eq, align 1
  %lt = alloca i1, align 1
  %neg4 = load i8, ptr %neg, align 1
  %pos5 = load i8, ptr %pos, align 1
  %lttmp = icmp slt i8 %neg4, %pos5
  store i1 %lttmp, ptr %lt, align 1
  %gt = alloca i1, align 1
  %pos6 = load i8, ptr %pos, align 1
  %neg7 = load i8, ptr %neg, align 1
  %gttmp = icmp sgt i8 %pos6, %neg7
  store i1 %gttmp, ptr %gt, align 1
  %eq8 = load i1, ptr %eq, align 1
  %nottmp = xor i1 %eq8, true
  %ifcond = icmp ne i1 %nottmp, false
  br i1 %ifcond, label %then, label %ifcont

then:                                             ; preds = %entry
  %one = alloca i32, align 4
  store i32 1, ptr %one, align 4
  %one9 = load i32, ptr %one, align 4
  call void @exit(i32 %one9) #0
  unreachable

ifcont:                                           ; preds = %entry
  %lt10 = load i1, ptr %lt, align 1
  %nottmp11 = xor i1 %lt10, true
  %ifcond12 = icmp ne i1 %nottmp11, false
  br i1 %ifcond12, label %then13, label %ifcont16

then13:                                           ; preds = %ifcont
  %one14 = alloca i32, align 4
  store i32 1, ptr %one14, align 4
  %one15 = load i32, ptr %one14, align 4
  call void @exit(i32 %one15) #0
  unreachable

ifcont16:                                         ; preds = %ifcont
  %ret = alloca i32, align 4
  store i32 0, ptr %ret, align 4
  %ret17 = load i32, ptr %ret, align 4
  call void @exit(i32 %ret17) #0
  unreachable
}

define { i32, ptr, i8 } @failsafe(i32 %err) {
entry:
  call void @exit(i32 1) #0
  unreachable
}

declare void @aria_gc_init(i64, i64)

declare void @aria_args_init(i32, ptr)

declare void @aria_streams_init()

declare i8 @aria_trit_add(i8, i8)

; Function Attrs: noreturn
declare void @exit(i32) #0

define i32 @__balanced_ternary_basic_init() {
entry:
  ret i32 0
}

attributes #0 = { noreturn }
