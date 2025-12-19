; ModuleID = 'tests/feature_validation/comprehensive_strings.aria'
source_filename = "tests/feature_validation/comprehensive_strings.aria"

@str = private unnamed_addr constant [6 x i8] c"Hello\00", align 1
@str.1 = private unnamed_addr constant [5 x i8] c"Aria\00", align 1
@str.2 = private unnamed_addr constant [7 x i8] c"v0.1.0\00", align 1
@str.3 = private unnamed_addr constant [1 x i8] zeroinitializer, align 1
@str.4 = private unnamed_addr constant [26 x i8] c"The quick brown fox jumps\00", align 1
@str.5 = private unnamed_addr constant [25 x i8] c"  leading and trailing  \00", align 1
@str.6 = private unnamed_addr constant [11 x i8] c"!@#$%^&*()\00", align 1
@str.7 = private unnamed_addr constant [10 x i8] c"123456789\00", align 1
@str.8 = private unnamed_addr constant [11 x i8] c"Test123!@#\00", align 1
@str.9 = private unnamed_addr constant [6 x i8] c"First\00", align 1
@str.10 = private unnamed_addr constant [7 x i8] c"Second\00", align 1
@str.11 = private unnamed_addr constant [6 x i8] c"Third\00", align 1
@str.12 = private unnamed_addr constant [7 x i8] c"Fourth\00", align 1
@str.13 = private unnamed_addr constant [6 x i8] c"Fifth\00", align 1
@str.14 = private unnamed_addr constant [145 x i8] c"This is a much longer string that contains multiple words and demonstrates that the string literal system can handle longer text without issues.\00", align 1
@str.15 = private unnamed_addr constant [8 x i8] c"Initial\00", align 1
@str.16 = private unnamed_addr constant [8 x i8] c"Changed\00", align 1
@str.17 = private unnamed_addr constant [6 x i8] c"Final\00", align 1

define i32 @test_string_declaration() {
entry:
  %greeting = alloca ptr, align 8
  store ptr @str, ptr %greeting, align 8
  %name = alloca ptr, align 8
  store ptr @str.1, ptr %name, align 8
  %version = alloca ptr, align 8
  store ptr @str.2, ptr %version, align 8
  ret i32 1
}

define i32 @test_empty_string() {
entry:
  %empty = alloca ptr, align 8
  store ptr @str.3, ptr %empty, align 8
  ret i32 2
}

define i32 @test_string_with_spaces() {
entry:
  %sentence = alloca ptr, align 8
  store ptr @str.4, ptr %sentence, align 8
  %spaced = alloca ptr, align 8
  store ptr @str.5, ptr %spaced, align 8
  ret i32 3
}

define i32 @test_string_with_special_chars() {
entry:
  %symbols = alloca ptr, align 8
  store ptr @str.6, ptr %symbols, align 8
  %numbers = alloca ptr, align 8
  store ptr @str.7, ptr %numbers, align 8
  %mixed = alloca ptr, align 8
  store ptr @str.8, ptr %mixed, align 8
  ret i32 4
}

define i32 @test_multiple_strings() {
entry:
  %s1 = alloca ptr, align 8
  store ptr @str.9, ptr %s1, align 8
  %s2 = alloca ptr, align 8
  store ptr @str.10, ptr %s2, align 8
  %s3 = alloca ptr, align 8
  store ptr @str.11, ptr %s3, align 8
  %s4 = alloca ptr, align 8
  store ptr @str.12, ptr %s4, align 8
  %s5 = alloca ptr, align 8
  store ptr @str.13, ptr %s5, align 8
  ret i32 5
}

define i32 @test_long_string() {
entry:
  %long = alloca ptr, align 8
  store ptr @str.14, ptr %long, align 8
  ret i32 6
}

define i32 @test_string_reassignment() {
entry:
  %msg = alloca ptr, align 8
  store ptr @str.15, ptr %msg, align 8
  store ptr @str.16, ptr %msg, align 8
  store ptr @str.17, ptr %msg, align 8
  ret i32 7
}

define i32 @main() {
entry:
  %r1 = alloca i32, align 4
  %0 = call i32 @test_string_declaration()
  store i32 %0, ptr %r1, align 4
  %r2 = alloca i32, align 4
  %1 = call i32 @test_empty_string()
  store i32 %1, ptr %r2, align 4
  %r3 = alloca i32, align 4
  %2 = call i32 @test_string_with_spaces()
  store i32 %2, ptr %r3, align 4
  %r4 = alloca i32, align 4
  %3 = call i32 @test_string_with_special_chars()
  store i32 %3, ptr %r4, align 4
  %r5 = alloca i32, align 4
  %4 = call i32 @test_multiple_strings()
  store i32 %4, ptr %r5, align 4
  %r6 = alloca i32, align 4
  %5 = call i32 @test_long_string()
  store i32 %5, ptr %r6, align 4
  %r7 = alloca i32, align 4
  %6 = call i32 @test_string_reassignment()
  store i32 %6, ptr %r7, align 4
  %total = alloca i32, align 4
  %r11 = load i32, ptr %r1, align 4
  %r22 = load i32, ptr %r2, align 4
  %addtmp = add i32 %r11, %r22
  %r33 = load i32, ptr %r3, align 4
  %addtmp4 = add i32 %addtmp, %r33
  store i32 %addtmp4, ptr %total, align 4
  %total5 = load i32, ptr %total, align 4
  %r46 = load i32, ptr %r4, align 4
  %addtmp7 = add i32 %total5, %r46
  store i32 %addtmp7, ptr %total, align 4
  %total8 = load i32, ptr %total, align 4
  %r59 = load i32, ptr %r5, align 4
  %addtmp10 = add i32 %total8, %r59
  store i32 %addtmp10, ptr %total, align 4
  %total11 = load i32, ptr %total, align 4
  %r612 = load i32, ptr %r6, align 4
  %addtmp13 = add i32 %total11, %r612
  store i32 %addtmp13, ptr %total, align 4
  %total14 = load i32, ptr %total, align 4
  %r715 = load i32, ptr %r7, align 4
  %addtmp16 = add i32 %total14, %r715
  store i32 %addtmp16, ptr %total, align 4
  %total17 = load i32, ptr %total, align 4
  ret i32 %total17
}

define i32 @__comprehensive_strings_init() {
entry:
  ret i32 0
}
