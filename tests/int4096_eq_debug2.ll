; ModuleID = 'tests/int4096_eq_debug2.aria'
source_filename = "tests/int4096_eq_debug2.aria"

%struct.int4096 = type { [64 x i64] }

define i32 @main() {
entry:
  call void @aria_gc_init(i64 0, i64 0)
  %a = alloca %struct.int4096, align 8
  %lbim_promoted = alloca %struct.int4096, align 8
  %limb_ptr = getelementptr %struct.int4096, ptr %lbim_promoted, i32 0, i32 0, i32 0
  store i64 0, ptr %limb_ptr, align 4
  %limb_ptr1 = getelementptr %struct.int4096, ptr %lbim_promoted, i32 0, i32 0, i32 1
  store i64 0, ptr %limb_ptr1, align 4
  %limb_ptr2 = getelementptr %struct.int4096, ptr %lbim_promoted, i32 0, i32 0, i32 2
  store i64 0, ptr %limb_ptr2, align 4
  %limb_ptr3 = getelementptr %struct.int4096, ptr %lbim_promoted, i32 0, i32 0, i32 3
  store i64 0, ptr %limb_ptr3, align 4
  %limb_ptr4 = getelementptr %struct.int4096, ptr %lbim_promoted, i32 0, i32 0, i32 4
  store i64 0, ptr %limb_ptr4, align 4
  %limb_ptr5 = getelementptr %struct.int4096, ptr %lbim_promoted, i32 0, i32 0, i32 5
  store i64 0, ptr %limb_ptr5, align 4
  %limb_ptr6 = getelementptr %struct.int4096, ptr %lbim_promoted, i32 0, i32 0, i32 6
  store i64 0, ptr %limb_ptr6, align 4
  %limb_ptr7 = getelementptr %struct.int4096, ptr %lbim_promoted, i32 0, i32 0, i32 7
  store i64 0, ptr %limb_ptr7, align 4
  %limb_ptr8 = getelementptr %struct.int4096, ptr %lbim_promoted, i32 0, i32 0, i32 8
  store i64 0, ptr %limb_ptr8, align 4
  %limb_ptr9 = getelementptr %struct.int4096, ptr %lbim_promoted, i32 0, i32 0, i32 9
  store i64 0, ptr %limb_ptr9, align 4
  %limb_ptr10 = getelementptr %struct.int4096, ptr %lbim_promoted, i32 0, i32 0, i32 10
  store i64 0, ptr %limb_ptr10, align 4
  %limb_ptr11 = getelementptr %struct.int4096, ptr %lbim_promoted, i32 0, i32 0, i32 11
  store i64 0, ptr %limb_ptr11, align 4
  %limb_ptr12 = getelementptr %struct.int4096, ptr %lbim_promoted, i32 0, i32 0, i32 12
  store i64 0, ptr %limb_ptr12, align 4
  %limb_ptr13 = getelementptr %struct.int4096, ptr %lbim_promoted, i32 0, i32 0, i32 13
  store i64 0, ptr %limb_ptr13, align 4
  %limb_ptr14 = getelementptr %struct.int4096, ptr %lbim_promoted, i32 0, i32 0, i32 14
  store i64 0, ptr %limb_ptr14, align 4
  %limb_ptr15 = getelementptr %struct.int4096, ptr %lbim_promoted, i32 0, i32 0, i32 15
  store i64 0, ptr %limb_ptr15, align 4
  %limb_ptr16 = getelementptr %struct.int4096, ptr %lbim_promoted, i32 0, i32 0, i32 16
  store i64 0, ptr %limb_ptr16, align 4
  %limb_ptr17 = getelementptr %struct.int4096, ptr %lbim_promoted, i32 0, i32 0, i32 17
  store i64 0, ptr %limb_ptr17, align 4
  %limb_ptr18 = getelementptr %struct.int4096, ptr %lbim_promoted, i32 0, i32 0, i32 18
  store i64 0, ptr %limb_ptr18, align 4
  %limb_ptr19 = getelementptr %struct.int4096, ptr %lbim_promoted, i32 0, i32 0, i32 19
  store i64 0, ptr %limb_ptr19, align 4
  %limb_ptr20 = getelementptr %struct.int4096, ptr %lbim_promoted, i32 0, i32 0, i32 20
  store i64 0, ptr %limb_ptr20, align 4
  %limb_ptr21 = getelementptr %struct.int4096, ptr %lbim_promoted, i32 0, i32 0, i32 21
  store i64 0, ptr %limb_ptr21, align 4
  %limb_ptr22 = getelementptr %struct.int4096, ptr %lbim_promoted, i32 0, i32 0, i32 22
  store i64 0, ptr %limb_ptr22, align 4
  %limb_ptr23 = getelementptr %struct.int4096, ptr %lbim_promoted, i32 0, i32 0, i32 23
  store i64 0, ptr %limb_ptr23, align 4
  %limb_ptr24 = getelementptr %struct.int4096, ptr %lbim_promoted, i32 0, i32 0, i32 24
  store i64 0, ptr %limb_ptr24, align 4
  %limb_ptr25 = getelementptr %struct.int4096, ptr %lbim_promoted, i32 0, i32 0, i32 25
  store i64 0, ptr %limb_ptr25, align 4
  %limb_ptr26 = getelementptr %struct.int4096, ptr %lbim_promoted, i32 0, i32 0, i32 26
  store i64 0, ptr %limb_ptr26, align 4
  %limb_ptr27 = getelementptr %struct.int4096, ptr %lbim_promoted, i32 0, i32 0, i32 27
  store i64 0, ptr %limb_ptr27, align 4
  %limb_ptr28 = getelementptr %struct.int4096, ptr %lbim_promoted, i32 0, i32 0, i32 28
  store i64 0, ptr %limb_ptr28, align 4
  %limb_ptr29 = getelementptr %struct.int4096, ptr %lbim_promoted, i32 0, i32 0, i32 29
  store i64 0, ptr %limb_ptr29, align 4
  %limb_ptr30 = getelementptr %struct.int4096, ptr %lbim_promoted, i32 0, i32 0, i32 30
  store i64 0, ptr %limb_ptr30, align 4
  %limb_ptr31 = getelementptr %struct.int4096, ptr %lbim_promoted, i32 0, i32 0, i32 31
  store i64 0, ptr %limb_ptr31, align 4
  %limb_ptr32 = getelementptr %struct.int4096, ptr %lbim_promoted, i32 0, i32 0, i32 32
  store i64 0, ptr %limb_ptr32, align 4
  %limb_ptr33 = getelementptr %struct.int4096, ptr %lbim_promoted, i32 0, i32 0, i32 33
  store i64 0, ptr %limb_ptr33, align 4
  %limb_ptr34 = getelementptr %struct.int4096, ptr %lbim_promoted, i32 0, i32 0, i32 34
  store i64 0, ptr %limb_ptr34, align 4
  %limb_ptr35 = getelementptr %struct.int4096, ptr %lbim_promoted, i32 0, i32 0, i32 35
  store i64 0, ptr %limb_ptr35, align 4
  %limb_ptr36 = getelementptr %struct.int4096, ptr %lbim_promoted, i32 0, i32 0, i32 36
  store i64 0, ptr %limb_ptr36, align 4
  %limb_ptr37 = getelementptr %struct.int4096, ptr %lbim_promoted, i32 0, i32 0, i32 37
  store i64 0, ptr %limb_ptr37, align 4
  %limb_ptr38 = getelementptr %struct.int4096, ptr %lbim_promoted, i32 0, i32 0, i32 38
  store i64 0, ptr %limb_ptr38, align 4
  %limb_ptr39 = getelementptr %struct.int4096, ptr %lbim_promoted, i32 0, i32 0, i32 39
  store i64 0, ptr %limb_ptr39, align 4
  %limb_ptr40 = getelementptr %struct.int4096, ptr %lbim_promoted, i32 0, i32 0, i32 40
  store i64 0, ptr %limb_ptr40, align 4
  %limb_ptr41 = getelementptr %struct.int4096, ptr %lbim_promoted, i32 0, i32 0, i32 41
  store i64 0, ptr %limb_ptr41, align 4
  %limb_ptr42 = getelementptr %struct.int4096, ptr %lbim_promoted, i32 0, i32 0, i32 42
  store i64 0, ptr %limb_ptr42, align 4
  %limb_ptr43 = getelementptr %struct.int4096, ptr %lbim_promoted, i32 0, i32 0, i32 43
  store i64 0, ptr %limb_ptr43, align 4
  %limb_ptr44 = getelementptr %struct.int4096, ptr %lbim_promoted, i32 0, i32 0, i32 44
  store i64 0, ptr %limb_ptr44, align 4
  %limb_ptr45 = getelementptr %struct.int4096, ptr %lbim_promoted, i32 0, i32 0, i32 45
  store i64 0, ptr %limb_ptr45, align 4
  %limb_ptr46 = getelementptr %struct.int4096, ptr %lbim_promoted, i32 0, i32 0, i32 46
  store i64 0, ptr %limb_ptr46, align 4
  %limb_ptr47 = getelementptr %struct.int4096, ptr %lbim_promoted, i32 0, i32 0, i32 47
  store i64 0, ptr %limb_ptr47, align 4
  %limb_ptr48 = getelementptr %struct.int4096, ptr %lbim_promoted, i32 0, i32 0, i32 48
  store i64 0, ptr %limb_ptr48, align 4
  %limb_ptr49 = getelementptr %struct.int4096, ptr %lbim_promoted, i32 0, i32 0, i32 49
  store i64 0, ptr %limb_ptr49, align 4
  %limb_ptr50 = getelementptr %struct.int4096, ptr %lbim_promoted, i32 0, i32 0, i32 50
  store i64 0, ptr %limb_ptr50, align 4
  %limb_ptr51 = getelementptr %struct.int4096, ptr %lbim_promoted, i32 0, i32 0, i32 51
  store i64 0, ptr %limb_ptr51, align 4
  %limb_ptr52 = getelementptr %struct.int4096, ptr %lbim_promoted, i32 0, i32 0, i32 52
  store i64 0, ptr %limb_ptr52, align 4
  %limb_ptr53 = getelementptr %struct.int4096, ptr %lbim_promoted, i32 0, i32 0, i32 53
  store i64 0, ptr %limb_ptr53, align 4
  %limb_ptr54 = getelementptr %struct.int4096, ptr %lbim_promoted, i32 0, i32 0, i32 54
  store i64 0, ptr %limb_ptr54, align 4
  %limb_ptr55 = getelementptr %struct.int4096, ptr %lbim_promoted, i32 0, i32 0, i32 55
  store i64 0, ptr %limb_ptr55, align 4
  %limb_ptr56 = getelementptr %struct.int4096, ptr %lbim_promoted, i32 0, i32 0, i32 56
  store i64 0, ptr %limb_ptr56, align 4
  %limb_ptr57 = getelementptr %struct.int4096, ptr %lbim_promoted, i32 0, i32 0, i32 57
  store i64 0, ptr %limb_ptr57, align 4
  %limb_ptr58 = getelementptr %struct.int4096, ptr %lbim_promoted, i32 0, i32 0, i32 58
  store i64 0, ptr %limb_ptr58, align 4
  %limb_ptr59 = getelementptr %struct.int4096, ptr %lbim_promoted, i32 0, i32 0, i32 59
  store i64 0, ptr %limb_ptr59, align 4
  %limb_ptr60 = getelementptr %struct.int4096, ptr %lbim_promoted, i32 0, i32 0, i32 60
  store i64 0, ptr %limb_ptr60, align 4
  %limb_ptr61 = getelementptr %struct.int4096, ptr %lbim_promoted, i32 0, i32 0, i32 61
  store i64 0, ptr %limb_ptr61, align 4
  %limb_ptr62 = getelementptr %struct.int4096, ptr %lbim_promoted, i32 0, i32 0, i32 62
  store i64 0, ptr %limb_ptr62, align 4
  %limb_ptr63 = getelementptr %struct.int4096, ptr %lbim_promoted, i32 0, i32 0, i32 63
  store i64 0, ptr %limb_ptr63, align 4
  %limb0_ptr = getelementptr %struct.int4096, ptr %lbim_promoted, i32 0, i32 0, i32 0
  store i64 1000000000, ptr %limb0_ptr, align 4
  br i1 false, label %sign_extend, label %continue

sign_extend:                                      ; preds = %entry
  %limb_ptr64 = getelementptr %struct.int4096, ptr %lbim_promoted, i32 0, i32 0, i32 1
  store i64 -1, ptr %limb_ptr64, align 4
  %limb_ptr65 = getelementptr %struct.int4096, ptr %lbim_promoted, i32 0, i32 0, i32 2
  store i64 -1, ptr %limb_ptr65, align 4
  %limb_ptr66 = getelementptr %struct.int4096, ptr %lbim_promoted, i32 0, i32 0, i32 3
  store i64 -1, ptr %limb_ptr66, align 4
  %limb_ptr67 = getelementptr %struct.int4096, ptr %lbim_promoted, i32 0, i32 0, i32 4
  store i64 -1, ptr %limb_ptr67, align 4
  %limb_ptr68 = getelementptr %struct.int4096, ptr %lbim_promoted, i32 0, i32 0, i32 5
  store i64 -1, ptr %limb_ptr68, align 4
  %limb_ptr69 = getelementptr %struct.int4096, ptr %lbim_promoted, i32 0, i32 0, i32 6
  store i64 -1, ptr %limb_ptr69, align 4
  %limb_ptr70 = getelementptr %struct.int4096, ptr %lbim_promoted, i32 0, i32 0, i32 7
  store i64 -1, ptr %limb_ptr70, align 4
  %limb_ptr71 = getelementptr %struct.int4096, ptr %lbim_promoted, i32 0, i32 0, i32 8
  store i64 -1, ptr %limb_ptr71, align 4
  %limb_ptr72 = getelementptr %struct.int4096, ptr %lbim_promoted, i32 0, i32 0, i32 9
  store i64 -1, ptr %limb_ptr72, align 4
  %limb_ptr73 = getelementptr %struct.int4096, ptr %lbim_promoted, i32 0, i32 0, i32 10
  store i64 -1, ptr %limb_ptr73, align 4
  %limb_ptr74 = getelementptr %struct.int4096, ptr %lbim_promoted, i32 0, i32 0, i32 11
  store i64 -1, ptr %limb_ptr74, align 4
  %limb_ptr75 = getelementptr %struct.int4096, ptr %lbim_promoted, i32 0, i32 0, i32 12
  store i64 -1, ptr %limb_ptr75, align 4
  %limb_ptr76 = getelementptr %struct.int4096, ptr %lbim_promoted, i32 0, i32 0, i32 13
  store i64 -1, ptr %limb_ptr76, align 4
  %limb_ptr77 = getelementptr %struct.int4096, ptr %lbim_promoted, i32 0, i32 0, i32 14
  store i64 -1, ptr %limb_ptr77, align 4
  %limb_ptr78 = getelementptr %struct.int4096, ptr %lbim_promoted, i32 0, i32 0, i32 15
  store i64 -1, ptr %limb_ptr78, align 4
  %limb_ptr79 = getelementptr %struct.int4096, ptr %lbim_promoted, i32 0, i32 0, i32 16
  store i64 -1, ptr %limb_ptr79, align 4
  %limb_ptr80 = getelementptr %struct.int4096, ptr %lbim_promoted, i32 0, i32 0, i32 17
  store i64 -1, ptr %limb_ptr80, align 4
  %limb_ptr81 = getelementptr %struct.int4096, ptr %lbim_promoted, i32 0, i32 0, i32 18
  store i64 -1, ptr %limb_ptr81, align 4
  %limb_ptr82 = getelementptr %struct.int4096, ptr %lbim_promoted, i32 0, i32 0, i32 19
  store i64 -1, ptr %limb_ptr82, align 4
  %limb_ptr83 = getelementptr %struct.int4096, ptr %lbim_promoted, i32 0, i32 0, i32 20
  store i64 -1, ptr %limb_ptr83, align 4
  %limb_ptr84 = getelementptr %struct.int4096, ptr %lbim_promoted, i32 0, i32 0, i32 21
  store i64 -1, ptr %limb_ptr84, align 4
  %limb_ptr85 = getelementptr %struct.int4096, ptr %lbim_promoted, i32 0, i32 0, i32 22
  store i64 -1, ptr %limb_ptr85, align 4
  %limb_ptr86 = getelementptr %struct.int4096, ptr %lbim_promoted, i32 0, i32 0, i32 23
  store i64 -1, ptr %limb_ptr86, align 4
  %limb_ptr87 = getelementptr %struct.int4096, ptr %lbim_promoted, i32 0, i32 0, i32 24
  store i64 -1, ptr %limb_ptr87, align 4
  %limb_ptr88 = getelementptr %struct.int4096, ptr %lbim_promoted, i32 0, i32 0, i32 25
  store i64 -1, ptr %limb_ptr88, align 4
  %limb_ptr89 = getelementptr %struct.int4096, ptr %lbim_promoted, i32 0, i32 0, i32 26
  store i64 -1, ptr %limb_ptr89, align 4
  %limb_ptr90 = getelementptr %struct.int4096, ptr %lbim_promoted, i32 0, i32 0, i32 27
  store i64 -1, ptr %limb_ptr90, align 4
  %limb_ptr91 = getelementptr %struct.int4096, ptr %lbim_promoted, i32 0, i32 0, i32 28
  store i64 -1, ptr %limb_ptr91, align 4
  %limb_ptr92 = getelementptr %struct.int4096, ptr %lbim_promoted, i32 0, i32 0, i32 29
  store i64 -1, ptr %limb_ptr92, align 4
  %limb_ptr93 = getelementptr %struct.int4096, ptr %lbim_promoted, i32 0, i32 0, i32 30
  store i64 -1, ptr %limb_ptr93, align 4
  %limb_ptr94 = getelementptr %struct.int4096, ptr %lbim_promoted, i32 0, i32 0, i32 31
  store i64 -1, ptr %limb_ptr94, align 4
  %limb_ptr95 = getelementptr %struct.int4096, ptr %lbim_promoted, i32 0, i32 0, i32 32
  store i64 -1, ptr %limb_ptr95, align 4
  %limb_ptr96 = getelementptr %struct.int4096, ptr %lbim_promoted, i32 0, i32 0, i32 33
  store i64 -1, ptr %limb_ptr96, align 4
  %limb_ptr97 = getelementptr %struct.int4096, ptr %lbim_promoted, i32 0, i32 0, i32 34
  store i64 -1, ptr %limb_ptr97, align 4
  %limb_ptr98 = getelementptr %struct.int4096, ptr %lbim_promoted, i32 0, i32 0, i32 35
  store i64 -1, ptr %limb_ptr98, align 4
  %limb_ptr99 = getelementptr %struct.int4096, ptr %lbim_promoted, i32 0, i32 0, i32 36
  store i64 -1, ptr %limb_ptr99, align 4
  %limb_ptr100 = getelementptr %struct.int4096, ptr %lbim_promoted, i32 0, i32 0, i32 37
  store i64 -1, ptr %limb_ptr100, align 4
  %limb_ptr101 = getelementptr %struct.int4096, ptr %lbim_promoted, i32 0, i32 0, i32 38
  store i64 -1, ptr %limb_ptr101, align 4
  %limb_ptr102 = getelementptr %struct.int4096, ptr %lbim_promoted, i32 0, i32 0, i32 39
  store i64 -1, ptr %limb_ptr102, align 4
  %limb_ptr103 = getelementptr %struct.int4096, ptr %lbim_promoted, i32 0, i32 0, i32 40
  store i64 -1, ptr %limb_ptr103, align 4
  %limb_ptr104 = getelementptr %struct.int4096, ptr %lbim_promoted, i32 0, i32 0, i32 41
  store i64 -1, ptr %limb_ptr104, align 4
  %limb_ptr105 = getelementptr %struct.int4096, ptr %lbim_promoted, i32 0, i32 0, i32 42
  store i64 -1, ptr %limb_ptr105, align 4
  %limb_ptr106 = getelementptr %struct.int4096, ptr %lbim_promoted, i32 0, i32 0, i32 43
  store i64 -1, ptr %limb_ptr106, align 4
  %limb_ptr107 = getelementptr %struct.int4096, ptr %lbim_promoted, i32 0, i32 0, i32 44
  store i64 -1, ptr %limb_ptr107, align 4
  %limb_ptr108 = getelementptr %struct.int4096, ptr %lbim_promoted, i32 0, i32 0, i32 45
  store i64 -1, ptr %limb_ptr108, align 4
  %limb_ptr109 = getelementptr %struct.int4096, ptr %lbim_promoted, i32 0, i32 0, i32 46
  store i64 -1, ptr %limb_ptr109, align 4
  %limb_ptr110 = getelementptr %struct.int4096, ptr %lbim_promoted, i32 0, i32 0, i32 47
  store i64 -1, ptr %limb_ptr110, align 4
  %limb_ptr111 = getelementptr %struct.int4096, ptr %lbim_promoted, i32 0, i32 0, i32 48
  store i64 -1, ptr %limb_ptr111, align 4
  %limb_ptr112 = getelementptr %struct.int4096, ptr %lbim_promoted, i32 0, i32 0, i32 49
  store i64 -1, ptr %limb_ptr112, align 4
  %limb_ptr113 = getelementptr %struct.int4096, ptr %lbim_promoted, i32 0, i32 0, i32 50
  store i64 -1, ptr %limb_ptr113, align 4
  %limb_ptr114 = getelementptr %struct.int4096, ptr %lbim_promoted, i32 0, i32 0, i32 51
  store i64 -1, ptr %limb_ptr114, align 4
  %limb_ptr115 = getelementptr %struct.int4096, ptr %lbim_promoted, i32 0, i32 0, i32 52
  store i64 -1, ptr %limb_ptr115, align 4
  %limb_ptr116 = getelementptr %struct.int4096, ptr %lbim_promoted, i32 0, i32 0, i32 53
  store i64 -1, ptr %limb_ptr116, align 4
  %limb_ptr117 = getelementptr %struct.int4096, ptr %lbim_promoted, i32 0, i32 0, i32 54
  store i64 -1, ptr %limb_ptr117, align 4
  %limb_ptr118 = getelementptr %struct.int4096, ptr %lbim_promoted, i32 0, i32 0, i32 55
  store i64 -1, ptr %limb_ptr118, align 4
  %limb_ptr119 = getelementptr %struct.int4096, ptr %lbim_promoted, i32 0, i32 0, i32 56
  store i64 -1, ptr %limb_ptr119, align 4
  %limb_ptr120 = getelementptr %struct.int4096, ptr %lbim_promoted, i32 0, i32 0, i32 57
  store i64 -1, ptr %limb_ptr120, align 4
  %limb_ptr121 = getelementptr %struct.int4096, ptr %lbim_promoted, i32 0, i32 0, i32 58
  store i64 -1, ptr %limb_ptr121, align 4
  %limb_ptr122 = getelementptr %struct.int4096, ptr %lbim_promoted, i32 0, i32 0, i32 59
  store i64 -1, ptr %limb_ptr122, align 4
  %limb_ptr123 = getelementptr %struct.int4096, ptr %lbim_promoted, i32 0, i32 0, i32 60
  store i64 -1, ptr %limb_ptr123, align 4
  %limb_ptr124 = getelementptr %struct.int4096, ptr %lbim_promoted, i32 0, i32 0, i32 61
  store i64 -1, ptr %limb_ptr124, align 4
  %limb_ptr125 = getelementptr %struct.int4096, ptr %lbim_promoted, i32 0, i32 0, i32 62
  store i64 -1, ptr %limb_ptr125, align 4
  %limb_ptr126 = getelementptr %struct.int4096, ptr %lbim_promoted, i32 0, i32 0, i32 63
  store i64 -1, ptr %limb_ptr126, align 4
  br label %continue

continue:                                         ; preds = %sign_extend, %entry
  %lbim_promoted_val = load %struct.int4096, ptr %lbim_promoted, align 4
  store %struct.int4096 %lbim_promoted_val, ptr %a, align 4
  %b = alloca %struct.int4096, align 8
  %lbim_promoted127 = alloca %struct.int4096, align 8
  %limb_ptr128 = getelementptr %struct.int4096, ptr %lbim_promoted127, i32 0, i32 0, i32 0
  store i64 0, ptr %limb_ptr128, align 4
  %limb_ptr129 = getelementptr %struct.int4096, ptr %lbim_promoted127, i32 0, i32 0, i32 1
  store i64 0, ptr %limb_ptr129, align 4
  %limb_ptr130 = getelementptr %struct.int4096, ptr %lbim_promoted127, i32 0, i32 0, i32 2
  store i64 0, ptr %limb_ptr130, align 4
  %limb_ptr131 = getelementptr %struct.int4096, ptr %lbim_promoted127, i32 0, i32 0, i32 3
  store i64 0, ptr %limb_ptr131, align 4
  %limb_ptr132 = getelementptr %struct.int4096, ptr %lbim_promoted127, i32 0, i32 0, i32 4
  store i64 0, ptr %limb_ptr132, align 4
  %limb_ptr133 = getelementptr %struct.int4096, ptr %lbim_promoted127, i32 0, i32 0, i32 5
  store i64 0, ptr %limb_ptr133, align 4
  %limb_ptr134 = getelementptr %struct.int4096, ptr %lbim_promoted127, i32 0, i32 0, i32 6
  store i64 0, ptr %limb_ptr134, align 4
  %limb_ptr135 = getelementptr %struct.int4096, ptr %lbim_promoted127, i32 0, i32 0, i32 7
  store i64 0, ptr %limb_ptr135, align 4
  %limb_ptr136 = getelementptr %struct.int4096, ptr %lbim_promoted127, i32 0, i32 0, i32 8
  store i64 0, ptr %limb_ptr136, align 4
  %limb_ptr137 = getelementptr %struct.int4096, ptr %lbim_promoted127, i32 0, i32 0, i32 9
  store i64 0, ptr %limb_ptr137, align 4
  %limb_ptr138 = getelementptr %struct.int4096, ptr %lbim_promoted127, i32 0, i32 0, i32 10
  store i64 0, ptr %limb_ptr138, align 4
  %limb_ptr139 = getelementptr %struct.int4096, ptr %lbim_promoted127, i32 0, i32 0, i32 11
  store i64 0, ptr %limb_ptr139, align 4
  %limb_ptr140 = getelementptr %struct.int4096, ptr %lbim_promoted127, i32 0, i32 0, i32 12
  store i64 0, ptr %limb_ptr140, align 4
  %limb_ptr141 = getelementptr %struct.int4096, ptr %lbim_promoted127, i32 0, i32 0, i32 13
  store i64 0, ptr %limb_ptr141, align 4
  %limb_ptr142 = getelementptr %struct.int4096, ptr %lbim_promoted127, i32 0, i32 0, i32 14
  store i64 0, ptr %limb_ptr142, align 4
  %limb_ptr143 = getelementptr %struct.int4096, ptr %lbim_promoted127, i32 0, i32 0, i32 15
  store i64 0, ptr %limb_ptr143, align 4
  %limb_ptr144 = getelementptr %struct.int4096, ptr %lbim_promoted127, i32 0, i32 0, i32 16
  store i64 0, ptr %limb_ptr144, align 4
  %limb_ptr145 = getelementptr %struct.int4096, ptr %lbim_promoted127, i32 0, i32 0, i32 17
  store i64 0, ptr %limb_ptr145, align 4
  %limb_ptr146 = getelementptr %struct.int4096, ptr %lbim_promoted127, i32 0, i32 0, i32 18
  store i64 0, ptr %limb_ptr146, align 4
  %limb_ptr147 = getelementptr %struct.int4096, ptr %lbim_promoted127, i32 0, i32 0, i32 19
  store i64 0, ptr %limb_ptr147, align 4
  %limb_ptr148 = getelementptr %struct.int4096, ptr %lbim_promoted127, i32 0, i32 0, i32 20
  store i64 0, ptr %limb_ptr148, align 4
  %limb_ptr149 = getelementptr %struct.int4096, ptr %lbim_promoted127, i32 0, i32 0, i32 21
  store i64 0, ptr %limb_ptr149, align 4
  %limb_ptr150 = getelementptr %struct.int4096, ptr %lbim_promoted127, i32 0, i32 0, i32 22
  store i64 0, ptr %limb_ptr150, align 4
  %limb_ptr151 = getelementptr %struct.int4096, ptr %lbim_promoted127, i32 0, i32 0, i32 23
  store i64 0, ptr %limb_ptr151, align 4
  %limb_ptr152 = getelementptr %struct.int4096, ptr %lbim_promoted127, i32 0, i32 0, i32 24
  store i64 0, ptr %limb_ptr152, align 4
  %limb_ptr153 = getelementptr %struct.int4096, ptr %lbim_promoted127, i32 0, i32 0, i32 25
  store i64 0, ptr %limb_ptr153, align 4
  %limb_ptr154 = getelementptr %struct.int4096, ptr %lbim_promoted127, i32 0, i32 0, i32 26
  store i64 0, ptr %limb_ptr154, align 4
  %limb_ptr155 = getelementptr %struct.int4096, ptr %lbim_promoted127, i32 0, i32 0, i32 27
  store i64 0, ptr %limb_ptr155, align 4
  %limb_ptr156 = getelementptr %struct.int4096, ptr %lbim_promoted127, i32 0, i32 0, i32 28
  store i64 0, ptr %limb_ptr156, align 4
  %limb_ptr157 = getelementptr %struct.int4096, ptr %lbim_promoted127, i32 0, i32 0, i32 29
  store i64 0, ptr %limb_ptr157, align 4
  %limb_ptr158 = getelementptr %struct.int4096, ptr %lbim_promoted127, i32 0, i32 0, i32 30
  store i64 0, ptr %limb_ptr158, align 4
  %limb_ptr159 = getelementptr %struct.int4096, ptr %lbim_promoted127, i32 0, i32 0, i32 31
  store i64 0, ptr %limb_ptr159, align 4
  %limb_ptr160 = getelementptr %struct.int4096, ptr %lbim_promoted127, i32 0, i32 0, i32 32
  store i64 0, ptr %limb_ptr160, align 4
  %limb_ptr161 = getelementptr %struct.int4096, ptr %lbim_promoted127, i32 0, i32 0, i32 33
  store i64 0, ptr %limb_ptr161, align 4
  %limb_ptr162 = getelementptr %struct.int4096, ptr %lbim_promoted127, i32 0, i32 0, i32 34
  store i64 0, ptr %limb_ptr162, align 4
  %limb_ptr163 = getelementptr %struct.int4096, ptr %lbim_promoted127, i32 0, i32 0, i32 35
  store i64 0, ptr %limb_ptr163, align 4
  %limb_ptr164 = getelementptr %struct.int4096, ptr %lbim_promoted127, i32 0, i32 0, i32 36
  store i64 0, ptr %limb_ptr164, align 4
  %limb_ptr165 = getelementptr %struct.int4096, ptr %lbim_promoted127, i32 0, i32 0, i32 37
  store i64 0, ptr %limb_ptr165, align 4
  %limb_ptr166 = getelementptr %struct.int4096, ptr %lbim_promoted127, i32 0, i32 0, i32 38
  store i64 0, ptr %limb_ptr166, align 4
  %limb_ptr167 = getelementptr %struct.int4096, ptr %lbim_promoted127, i32 0, i32 0, i32 39
  store i64 0, ptr %limb_ptr167, align 4
  %limb_ptr168 = getelementptr %struct.int4096, ptr %lbim_promoted127, i32 0, i32 0, i32 40
  store i64 0, ptr %limb_ptr168, align 4
  %limb_ptr169 = getelementptr %struct.int4096, ptr %lbim_promoted127, i32 0, i32 0, i32 41
  store i64 0, ptr %limb_ptr169, align 4
  %limb_ptr170 = getelementptr %struct.int4096, ptr %lbim_promoted127, i32 0, i32 0, i32 42
  store i64 0, ptr %limb_ptr170, align 4
  %limb_ptr171 = getelementptr %struct.int4096, ptr %lbim_promoted127, i32 0, i32 0, i32 43
  store i64 0, ptr %limb_ptr171, align 4
  %limb_ptr172 = getelementptr %struct.int4096, ptr %lbim_promoted127, i32 0, i32 0, i32 44
  store i64 0, ptr %limb_ptr172, align 4
  %limb_ptr173 = getelementptr %struct.int4096, ptr %lbim_promoted127, i32 0, i32 0, i32 45
  store i64 0, ptr %limb_ptr173, align 4
  %limb_ptr174 = getelementptr %struct.int4096, ptr %lbim_promoted127, i32 0, i32 0, i32 46
  store i64 0, ptr %limb_ptr174, align 4
  %limb_ptr175 = getelementptr %struct.int4096, ptr %lbim_promoted127, i32 0, i32 0, i32 47
  store i64 0, ptr %limb_ptr175, align 4
  %limb_ptr176 = getelementptr %struct.int4096, ptr %lbim_promoted127, i32 0, i32 0, i32 48
  store i64 0, ptr %limb_ptr176, align 4
  %limb_ptr177 = getelementptr %struct.int4096, ptr %lbim_promoted127, i32 0, i32 0, i32 49
  store i64 0, ptr %limb_ptr177, align 4
  %limb_ptr178 = getelementptr %struct.int4096, ptr %lbim_promoted127, i32 0, i32 0, i32 50
  store i64 0, ptr %limb_ptr178, align 4
  %limb_ptr179 = getelementptr %struct.int4096, ptr %lbim_promoted127, i32 0, i32 0, i32 51
  store i64 0, ptr %limb_ptr179, align 4
  %limb_ptr180 = getelementptr %struct.int4096, ptr %lbim_promoted127, i32 0, i32 0, i32 52
  store i64 0, ptr %limb_ptr180, align 4
  %limb_ptr181 = getelementptr %struct.int4096, ptr %lbim_promoted127, i32 0, i32 0, i32 53
  store i64 0, ptr %limb_ptr181, align 4
  %limb_ptr182 = getelementptr %struct.int4096, ptr %lbim_promoted127, i32 0, i32 0, i32 54
  store i64 0, ptr %limb_ptr182, align 4
  %limb_ptr183 = getelementptr %struct.int4096, ptr %lbim_promoted127, i32 0, i32 0, i32 55
  store i64 0, ptr %limb_ptr183, align 4
  %limb_ptr184 = getelementptr %struct.int4096, ptr %lbim_promoted127, i32 0, i32 0, i32 56
  store i64 0, ptr %limb_ptr184, align 4
  %limb_ptr185 = getelementptr %struct.int4096, ptr %lbim_promoted127, i32 0, i32 0, i32 57
  store i64 0, ptr %limb_ptr185, align 4
  %limb_ptr186 = getelementptr %struct.int4096, ptr %lbim_promoted127, i32 0, i32 0, i32 58
  store i64 0, ptr %limb_ptr186, align 4
  %limb_ptr187 = getelementptr %struct.int4096, ptr %lbim_promoted127, i32 0, i32 0, i32 59
  store i64 0, ptr %limb_ptr187, align 4
  %limb_ptr188 = getelementptr %struct.int4096, ptr %lbim_promoted127, i32 0, i32 0, i32 60
  store i64 0, ptr %limb_ptr188, align 4
  %limb_ptr189 = getelementptr %struct.int4096, ptr %lbim_promoted127, i32 0, i32 0, i32 61
  store i64 0, ptr %limb_ptr189, align 4
  %limb_ptr190 = getelementptr %struct.int4096, ptr %lbim_promoted127, i32 0, i32 0, i32 62
  store i64 0, ptr %limb_ptr190, align 4
  %limb_ptr191 = getelementptr %struct.int4096, ptr %lbim_promoted127, i32 0, i32 0, i32 63
  store i64 0, ptr %limb_ptr191, align 4
  %limb0_ptr192 = getelementptr %struct.int4096, ptr %lbim_promoted127, i32 0, i32 0, i32 0
  store i64 1000000000, ptr %limb0_ptr192, align 4
  br i1 false, label %sign_extend193, label %continue194

sign_extend193:                                   ; preds = %continue
  %limb_ptr195 = getelementptr %struct.int4096, ptr %lbim_promoted127, i32 0, i32 0, i32 1
  store i64 -1, ptr %limb_ptr195, align 4
  %limb_ptr196 = getelementptr %struct.int4096, ptr %lbim_promoted127, i32 0, i32 0, i32 2
  store i64 -1, ptr %limb_ptr196, align 4
  %limb_ptr197 = getelementptr %struct.int4096, ptr %lbim_promoted127, i32 0, i32 0, i32 3
  store i64 -1, ptr %limb_ptr197, align 4
  %limb_ptr198 = getelementptr %struct.int4096, ptr %lbim_promoted127, i32 0, i32 0, i32 4
  store i64 -1, ptr %limb_ptr198, align 4
  %limb_ptr199 = getelementptr %struct.int4096, ptr %lbim_promoted127, i32 0, i32 0, i32 5
  store i64 -1, ptr %limb_ptr199, align 4
  %limb_ptr200 = getelementptr %struct.int4096, ptr %lbim_promoted127, i32 0, i32 0, i32 6
  store i64 -1, ptr %limb_ptr200, align 4
  %limb_ptr201 = getelementptr %struct.int4096, ptr %lbim_promoted127, i32 0, i32 0, i32 7
  store i64 -1, ptr %limb_ptr201, align 4
  %limb_ptr202 = getelementptr %struct.int4096, ptr %lbim_promoted127, i32 0, i32 0, i32 8
  store i64 -1, ptr %limb_ptr202, align 4
  %limb_ptr203 = getelementptr %struct.int4096, ptr %lbim_promoted127, i32 0, i32 0, i32 9
  store i64 -1, ptr %limb_ptr203, align 4
  %limb_ptr204 = getelementptr %struct.int4096, ptr %lbim_promoted127, i32 0, i32 0, i32 10
  store i64 -1, ptr %limb_ptr204, align 4
  %limb_ptr205 = getelementptr %struct.int4096, ptr %lbim_promoted127, i32 0, i32 0, i32 11
  store i64 -1, ptr %limb_ptr205, align 4
  %limb_ptr206 = getelementptr %struct.int4096, ptr %lbim_promoted127, i32 0, i32 0, i32 12
  store i64 -1, ptr %limb_ptr206, align 4
  %limb_ptr207 = getelementptr %struct.int4096, ptr %lbim_promoted127, i32 0, i32 0, i32 13
  store i64 -1, ptr %limb_ptr207, align 4
  %limb_ptr208 = getelementptr %struct.int4096, ptr %lbim_promoted127, i32 0, i32 0, i32 14
  store i64 -1, ptr %limb_ptr208, align 4
  %limb_ptr209 = getelementptr %struct.int4096, ptr %lbim_promoted127, i32 0, i32 0, i32 15
  store i64 -1, ptr %limb_ptr209, align 4
  %limb_ptr210 = getelementptr %struct.int4096, ptr %lbim_promoted127, i32 0, i32 0, i32 16
  store i64 -1, ptr %limb_ptr210, align 4
  %limb_ptr211 = getelementptr %struct.int4096, ptr %lbim_promoted127, i32 0, i32 0, i32 17
  store i64 -1, ptr %limb_ptr211, align 4
  %limb_ptr212 = getelementptr %struct.int4096, ptr %lbim_promoted127, i32 0, i32 0, i32 18
  store i64 -1, ptr %limb_ptr212, align 4
  %limb_ptr213 = getelementptr %struct.int4096, ptr %lbim_promoted127, i32 0, i32 0, i32 19
  store i64 -1, ptr %limb_ptr213, align 4
  %limb_ptr214 = getelementptr %struct.int4096, ptr %lbim_promoted127, i32 0, i32 0, i32 20
  store i64 -1, ptr %limb_ptr214, align 4
  %limb_ptr215 = getelementptr %struct.int4096, ptr %lbim_promoted127, i32 0, i32 0, i32 21
  store i64 -1, ptr %limb_ptr215, align 4
  %limb_ptr216 = getelementptr %struct.int4096, ptr %lbim_promoted127, i32 0, i32 0, i32 22
  store i64 -1, ptr %limb_ptr216, align 4
  %limb_ptr217 = getelementptr %struct.int4096, ptr %lbim_promoted127, i32 0, i32 0, i32 23
  store i64 -1, ptr %limb_ptr217, align 4
  %limb_ptr218 = getelementptr %struct.int4096, ptr %lbim_promoted127, i32 0, i32 0, i32 24
  store i64 -1, ptr %limb_ptr218, align 4
  %limb_ptr219 = getelementptr %struct.int4096, ptr %lbim_promoted127, i32 0, i32 0, i32 25
  store i64 -1, ptr %limb_ptr219, align 4
  %limb_ptr220 = getelementptr %struct.int4096, ptr %lbim_promoted127, i32 0, i32 0, i32 26
  store i64 -1, ptr %limb_ptr220, align 4
  %limb_ptr221 = getelementptr %struct.int4096, ptr %lbim_promoted127, i32 0, i32 0, i32 27
  store i64 -1, ptr %limb_ptr221, align 4
  %limb_ptr222 = getelementptr %struct.int4096, ptr %lbim_promoted127, i32 0, i32 0, i32 28
  store i64 -1, ptr %limb_ptr222, align 4
  %limb_ptr223 = getelementptr %struct.int4096, ptr %lbim_promoted127, i32 0, i32 0, i32 29
  store i64 -1, ptr %limb_ptr223, align 4
  %limb_ptr224 = getelementptr %struct.int4096, ptr %lbim_promoted127, i32 0, i32 0, i32 30
  store i64 -1, ptr %limb_ptr224, align 4
  %limb_ptr225 = getelementptr %struct.int4096, ptr %lbim_promoted127, i32 0, i32 0, i32 31
  store i64 -1, ptr %limb_ptr225, align 4
  %limb_ptr226 = getelementptr %struct.int4096, ptr %lbim_promoted127, i32 0, i32 0, i32 32
  store i64 -1, ptr %limb_ptr226, align 4
  %limb_ptr227 = getelementptr %struct.int4096, ptr %lbim_promoted127, i32 0, i32 0, i32 33
  store i64 -1, ptr %limb_ptr227, align 4
  %limb_ptr228 = getelementptr %struct.int4096, ptr %lbim_promoted127, i32 0, i32 0, i32 34
  store i64 -1, ptr %limb_ptr228, align 4
  %limb_ptr229 = getelementptr %struct.int4096, ptr %lbim_promoted127, i32 0, i32 0, i32 35
  store i64 -1, ptr %limb_ptr229, align 4
  %limb_ptr230 = getelementptr %struct.int4096, ptr %lbim_promoted127, i32 0, i32 0, i32 36
  store i64 -1, ptr %limb_ptr230, align 4
  %limb_ptr231 = getelementptr %struct.int4096, ptr %lbim_promoted127, i32 0, i32 0, i32 37
  store i64 -1, ptr %limb_ptr231, align 4
  %limb_ptr232 = getelementptr %struct.int4096, ptr %lbim_promoted127, i32 0, i32 0, i32 38
  store i64 -1, ptr %limb_ptr232, align 4
  %limb_ptr233 = getelementptr %struct.int4096, ptr %lbim_promoted127, i32 0, i32 0, i32 39
  store i64 -1, ptr %limb_ptr233, align 4
  %limb_ptr234 = getelementptr %struct.int4096, ptr %lbim_promoted127, i32 0, i32 0, i32 40
  store i64 -1, ptr %limb_ptr234, align 4
  %limb_ptr235 = getelementptr %struct.int4096, ptr %lbim_promoted127, i32 0, i32 0, i32 41
  store i64 -1, ptr %limb_ptr235, align 4
  %limb_ptr236 = getelementptr %struct.int4096, ptr %lbim_promoted127, i32 0, i32 0, i32 42
  store i64 -1, ptr %limb_ptr236, align 4
  %limb_ptr237 = getelementptr %struct.int4096, ptr %lbim_promoted127, i32 0, i32 0, i32 43
  store i64 -1, ptr %limb_ptr237, align 4
  %limb_ptr238 = getelementptr %struct.int4096, ptr %lbim_promoted127, i32 0, i32 0, i32 44
  store i64 -1, ptr %limb_ptr238, align 4
  %limb_ptr239 = getelementptr %struct.int4096, ptr %lbim_promoted127, i32 0, i32 0, i32 45
  store i64 -1, ptr %limb_ptr239, align 4
  %limb_ptr240 = getelementptr %struct.int4096, ptr %lbim_promoted127, i32 0, i32 0, i32 46
  store i64 -1, ptr %limb_ptr240, align 4
  %limb_ptr241 = getelementptr %struct.int4096, ptr %lbim_promoted127, i32 0, i32 0, i32 47
  store i64 -1, ptr %limb_ptr241, align 4
  %limb_ptr242 = getelementptr %struct.int4096, ptr %lbim_promoted127, i32 0, i32 0, i32 48
  store i64 -1, ptr %limb_ptr242, align 4
  %limb_ptr243 = getelementptr %struct.int4096, ptr %lbim_promoted127, i32 0, i32 0, i32 49
  store i64 -1, ptr %limb_ptr243, align 4
  %limb_ptr244 = getelementptr %struct.int4096, ptr %lbim_promoted127, i32 0, i32 0, i32 50
  store i64 -1, ptr %limb_ptr244, align 4
  %limb_ptr245 = getelementptr %struct.int4096, ptr %lbim_promoted127, i32 0, i32 0, i32 51
  store i64 -1, ptr %limb_ptr245, align 4
  %limb_ptr246 = getelementptr %struct.int4096, ptr %lbim_promoted127, i32 0, i32 0, i32 52
  store i64 -1, ptr %limb_ptr246, align 4
  %limb_ptr247 = getelementptr %struct.int4096, ptr %lbim_promoted127, i32 0, i32 0, i32 53
  store i64 -1, ptr %limb_ptr247, align 4
  %limb_ptr248 = getelementptr %struct.int4096, ptr %lbim_promoted127, i32 0, i32 0, i32 54
  store i64 -1, ptr %limb_ptr248, align 4
  %limb_ptr249 = getelementptr %struct.int4096, ptr %lbim_promoted127, i32 0, i32 0, i32 55
  store i64 -1, ptr %limb_ptr249, align 4
  %limb_ptr250 = getelementptr %struct.int4096, ptr %lbim_promoted127, i32 0, i32 0, i32 56
  store i64 -1, ptr %limb_ptr250, align 4
  %limb_ptr251 = getelementptr %struct.int4096, ptr %lbim_promoted127, i32 0, i32 0, i32 57
  store i64 -1, ptr %limb_ptr251, align 4
  %limb_ptr252 = getelementptr %struct.int4096, ptr %lbim_promoted127, i32 0, i32 0, i32 58
  store i64 -1, ptr %limb_ptr252, align 4
  %limb_ptr253 = getelementptr %struct.int4096, ptr %lbim_promoted127, i32 0, i32 0, i32 59
  store i64 -1, ptr %limb_ptr253, align 4
  %limb_ptr254 = getelementptr %struct.int4096, ptr %lbim_promoted127, i32 0, i32 0, i32 60
  store i64 -1, ptr %limb_ptr254, align 4
  %limb_ptr255 = getelementptr %struct.int4096, ptr %lbim_promoted127, i32 0, i32 0, i32 61
  store i64 -1, ptr %limb_ptr255, align 4
  %limb_ptr256 = getelementptr %struct.int4096, ptr %lbim_promoted127, i32 0, i32 0, i32 62
  store i64 -1, ptr %limb_ptr256, align 4
  %limb_ptr257 = getelementptr %struct.int4096, ptr %lbim_promoted127, i32 0, i32 0, i32 63
  store i64 -1, ptr %limb_ptr257, align 4
  br label %continue194

continue194:                                      ; preds = %sign_extend193, %continue
  %lbim_promoted_val258 = load %struct.int4096, ptr %lbim_promoted127, align 4
  store %struct.int4096 %lbim_promoted_val258, ptr %b, align 4
  %a259 = load %struct.int4096, ptr %a, align 4
  %b260 = load %struct.int4096, ptr %b, align 4
  %lbim.eq = call i1 @aria_lbim_eq4096(%struct.int4096 %a259, %struct.int4096 %b260)
  %ifcond = icmp ne i1 %lbim.eq, false
  br i1 %ifcond, label %then, label %ifcont

then:                                             ; preds = %continue194
  ret i32 0

ifcont:                                           ; preds = %continue194
  ret i32 10
}

declare void @aria_gc_init(i64, i64)

declare i1 @aria_lbim_eq4096(%struct.int4096, %struct.int4096)

define i32 @__int4096_eq_debug2_init() {
entry:
  ret i32 0
}
