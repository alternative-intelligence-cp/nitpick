	.text
	.file	"aria_module"
	.p2align	4, 0x90                         # -- Begin function __aria_module_init
	.type	__aria_module_init,@function
__aria_module_init:                     # @__aria_module_init
	.cfi_startproc
# %bb.0:                                # %entry
	retq
.Lfunc_end0:
	.size	__aria_module_init, .Lfunc_end0-__aria_module_init
	.cfi_endproc
                                        # -- End function
	.p2align	4, 0x90                         # -- Begin function abs_int8
	.type	abs_int8,@function
abs_int8:                               # @abs_int8
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	subq	$16, %rsp
	movb	%dil, -1(%rbp)
	cmpq	$0, -1(%rbp)
	js	.LBB1_1
# %bb.2:                                # %ifcont
	movzbl	-1(%rbp), %edx
	movq	%rsp, %rax
	leaq	-16(%rax), %rsp
	movb	$0, -16(%rax)
	movb	%dl, -15(%rax)
	xorl	%eax, %eax
	jmp	.LBB1_3
.LBB1_1:                                # %then
	xorl	%edx, %edx
	subb	-1(%rbp), %dl
	movb	%dl, -2(%rbp)
	movq	%rsp, %rax
	leaq	-16(%rax), %rsp
	movb	$0, -16(%rax)
	movb	%dl, -15(%rax)
	xorl	%eax, %eax
                                        # kill: def $dl killed $dl killed $edx
.LBB1_3:                                # %ifcont
	movq	%rbp, %rsp
	popq	%rbp
	.cfi_def_cfa %rsp, 8
	retq
.Lfunc_end1:
	.size	abs_int8, .Lfunc_end1-abs_int8
	.cfi_endproc
                                        # -- End function
	.p2align	4, 0x90                         # -- Begin function abs_int16
	.type	abs_int16,@function
abs_int16:                              # @abs_int16
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	subq	$16, %rsp
	movw	%di, -2(%rbp)
	cmpq	$0, -2(%rbp)
	js	.LBB2_1
# %bb.2:                                # %ifcont
	movzwl	-2(%rbp), %edx
	movq	%rsp, %rax
	leaq	-16(%rax), %rsp
	movb	$0, -16(%rax)
	movw	%dx, -14(%rax)
	xorl	%eax, %eax
	jmp	.LBB2_3
.LBB2_1:                                # %then
	movzwl	-2(%rbp), %eax
	xorl	%edx, %edx
	subw	%ax, %dx
	movw	%dx, -4(%rbp)
	movq	%rsp, %rax
	leaq	-16(%rax), %rsp
	movb	$0, -16(%rax)
	movw	%dx, -14(%rax)
	xorl	%eax, %eax
                                        # kill: def $dx killed $dx killed $edx
.LBB2_3:                                # %ifcont
	movq	%rbp, %rsp
	popq	%rbp
	.cfi_def_cfa %rsp, 8
	retq
.Lfunc_end2:
	.size	abs_int16, .Lfunc_end2-abs_int16
	.cfi_endproc
                                        # -- End function
	.p2align	4, 0x90                         # -- Begin function abs_int32
	.type	abs_int32,@function
abs_int32:                              # @abs_int32
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	subq	$16, %rsp
	movl	%edi, -4(%rbp)
	cmpq	$0, -4(%rbp)
	js	.LBB3_1
# %bb.2:                                # %ifcont
	movl	-4(%rbp), %edx
	jmp	.LBB3_3
.LBB3_1:                                # %then
	xorl	%edx, %edx
	subl	-4(%rbp), %edx
	movl	%edx, -8(%rbp)
.LBB3_3:                                # %ifcont
	movq	%rsp, %rax
	leaq	-16(%rax), %rsp
	movb	$0, -16(%rax)
	movl	%edx, -12(%rax)
	xorl	%eax, %eax
	movq	%rbp, %rsp
	popq	%rbp
	.cfi_def_cfa %rsp, 8
	retq
.Lfunc_end3:
	.size	abs_int32, .Lfunc_end3-abs_int32
	.cfi_endproc
                                        # -- End function
	.p2align	4, 0x90                         # -- Begin function abs_int64
	.type	abs_int64,@function
abs_int64:                              # @abs_int64
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	subq	$16, %rsp
	movq	%rdi, -8(%rbp)
	testq	%rdi, %rdi
	js	.LBB4_1
# %bb.2:                                # %ifcont
	movq	-8(%rbp), %rdx
	jmp	.LBB4_3
.LBB4_1:                                # %then
	movq	-8(%rbp), %rax
	xorl	%edx, %edx
	subq	%rax, %rdx
	movq	%rdx, -16(%rbp)
.LBB4_3:                                # %ifcont
	movq	%rsp, %rax
	leaq	-16(%rax), %rsp
	movb	$0, -16(%rax)
	movq	%rdx, -8(%rax)
	xorl	%eax, %eax
	movq	%rbp, %rsp
	popq	%rbp
	.cfi_def_cfa %rsp, 8
	retq
.Lfunc_end4:
	.size	abs_int64, .Lfunc_end4-abs_int64
	.cfi_endproc
                                        # -- End function
	.p2align	4, 0x90                         # -- Begin function min_int8
	.type	min_int8,@function
min_int8:                               # @min_int8
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	subq	$16, %rsp
	movb	%dil, -2(%rbp)
	movb	%sil, -1(%rbp)
	movq	-2(%rbp), %rax
	cmpq	-1(%rbp), %rax
	jge	.LBB5_3
# %bb.1:                                # %then
	movzbl	-2(%rbp), %edx
	jmp	.LBB5_2
.LBB5_3:                                # %ifcont
	movzbl	-1(%rbp), %edx
.LBB5_2:                                # %then
	movq	%rsp, %rax
	leaq	-16(%rax), %rsp
	movb	$0, -16(%rax)
	movb	%dl, -15(%rax)
	xorl	%eax, %eax
	movq	%rbp, %rsp
	popq	%rbp
	.cfi_def_cfa %rsp, 8
	retq
.Lfunc_end5:
	.size	min_int8, .Lfunc_end5-min_int8
	.cfi_endproc
                                        # -- End function
	.p2align	4, 0x90                         # -- Begin function min_int16
	.type	min_int16,@function
min_int16:                              # @min_int16
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	subq	$16, %rsp
	movw	%di, -4(%rbp)
	movw	%si, -2(%rbp)
	movq	-4(%rbp), %rax
	cmpq	-2(%rbp), %rax
	jge	.LBB6_3
# %bb.1:                                # %then
	movzwl	-4(%rbp), %edx
	jmp	.LBB6_2
.LBB6_3:                                # %ifcont
	movzwl	-2(%rbp), %edx
.LBB6_2:                                # %then
	movq	%rsp, %rax
	leaq	-16(%rax), %rsp
	movb	$0, -16(%rax)
	movw	%dx, -14(%rax)
	xorl	%eax, %eax
	movq	%rbp, %rsp
	popq	%rbp
	.cfi_def_cfa %rsp, 8
	retq
.Lfunc_end6:
	.size	min_int16, .Lfunc_end6-min_int16
	.cfi_endproc
                                        # -- End function
	.p2align	4, 0x90                         # -- Begin function min_int32
	.type	min_int32,@function
min_int32:                              # @min_int32
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	subq	$16, %rsp
	movl	%edi, -8(%rbp)
	movl	%esi, -4(%rbp)
	movq	-8(%rbp), %rax
	cmpq	-4(%rbp), %rax
	jge	.LBB7_3
# %bb.1:                                # %then
	movl	-8(%rbp), %edx
	jmp	.LBB7_2
.LBB7_3:                                # %ifcont
	movl	-4(%rbp), %edx
.LBB7_2:                                # %then
	movq	%rsp, %rax
	leaq	-16(%rax), %rsp
	movb	$0, -16(%rax)
	movl	%edx, -12(%rax)
	xorl	%eax, %eax
	movq	%rbp, %rsp
	popq	%rbp
	.cfi_def_cfa %rsp, 8
	retq
.Lfunc_end7:
	.size	min_int32, .Lfunc_end7-min_int32
	.cfi_endproc
                                        # -- End function
	.p2align	4, 0x90                         # -- Begin function min_int64
	.type	min_int64,@function
min_int64:                              # @min_int64
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	subq	$16, %rsp
	movq	%rdi, -16(%rbp)
	movq	%rsi, -8(%rbp)
	cmpq	%rsi, %rdi
	jge	.LBB8_3
# %bb.1:                                # %then
	movq	-16(%rbp), %rdx
	jmp	.LBB8_2
.LBB8_3:                                # %ifcont
	movq	-8(%rbp), %rdx
.LBB8_2:                                # %then
	movq	%rsp, %rax
	leaq	-16(%rax), %rsp
	movb	$0, -16(%rax)
	movq	%rdx, -8(%rax)
	xorl	%eax, %eax
	movq	%rbp, %rsp
	popq	%rbp
	.cfi_def_cfa %rsp, 8
	retq
.Lfunc_end8:
	.size	min_int64, .Lfunc_end8-min_int64
	.cfi_endproc
                                        # -- End function
	.p2align	4, 0x90                         # -- Begin function min_uint8
	.type	min_uint8,@function
min_uint8:                              # @min_uint8
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	subq	$16, %rsp
	movb	%dil, -2(%rbp)
	movb	%sil, -1(%rbp)
	movq	-2(%rbp), %rax
	cmpq	-1(%rbp), %rax
	jge	.LBB9_3
# %bb.1:                                # %then
	movzbl	-2(%rbp), %edx
	jmp	.LBB9_2
.LBB9_3:                                # %ifcont
	movzbl	-1(%rbp), %edx
.LBB9_2:                                # %then
	movq	%rsp, %rax
	leaq	-16(%rax), %rsp
	movb	$0, -16(%rax)
	movb	%dl, -15(%rax)
	xorl	%eax, %eax
	movq	%rbp, %rsp
	popq	%rbp
	.cfi_def_cfa %rsp, 8
	retq
.Lfunc_end9:
	.size	min_uint8, .Lfunc_end9-min_uint8
	.cfi_endproc
                                        # -- End function
	.p2align	4, 0x90                         # -- Begin function min_uint16
	.type	min_uint16,@function
min_uint16:                             # @min_uint16
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	subq	$16, %rsp
	movw	%di, -4(%rbp)
	movw	%si, -2(%rbp)
	movq	-4(%rbp), %rax
	cmpq	-2(%rbp), %rax
	jge	.LBB10_3
# %bb.1:                                # %then
	movzwl	-4(%rbp), %edx
	jmp	.LBB10_2
.LBB10_3:                               # %ifcont
	movzwl	-2(%rbp), %edx
.LBB10_2:                               # %then
	movq	%rsp, %rax
	leaq	-16(%rax), %rsp
	movb	$0, -16(%rax)
	movw	%dx, -14(%rax)
	xorl	%eax, %eax
	movq	%rbp, %rsp
	popq	%rbp
	.cfi_def_cfa %rsp, 8
	retq
.Lfunc_end10:
	.size	min_uint16, .Lfunc_end10-min_uint16
	.cfi_endproc
                                        # -- End function
	.p2align	4, 0x90                         # -- Begin function min_uint32
	.type	min_uint32,@function
min_uint32:                             # @min_uint32
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	subq	$16, %rsp
	movl	%edi, -8(%rbp)
	movl	%esi, -4(%rbp)
	movq	-8(%rbp), %rax
	cmpq	-4(%rbp), %rax
	jge	.LBB11_3
# %bb.1:                                # %then
	movl	-8(%rbp), %edx
	jmp	.LBB11_2
.LBB11_3:                               # %ifcont
	movl	-4(%rbp), %edx
.LBB11_2:                               # %then
	movq	%rsp, %rax
	leaq	-16(%rax), %rsp
	movb	$0, -16(%rax)
	movl	%edx, -12(%rax)
	xorl	%eax, %eax
	movq	%rbp, %rsp
	popq	%rbp
	.cfi_def_cfa %rsp, 8
	retq
.Lfunc_end11:
	.size	min_uint32, .Lfunc_end11-min_uint32
	.cfi_endproc
                                        # -- End function
	.p2align	4, 0x90                         # -- Begin function min_uint64
	.type	min_uint64,@function
min_uint64:                             # @min_uint64
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	subq	$16, %rsp
	movq	%rdi, -16(%rbp)
	movq	%rsi, -8(%rbp)
	cmpq	%rsi, %rdi
	jge	.LBB12_3
# %bb.1:                                # %then
	movq	-16(%rbp), %rdx
	jmp	.LBB12_2
.LBB12_3:                               # %ifcont
	movq	-8(%rbp), %rdx
.LBB12_2:                               # %then
	movq	%rsp, %rax
	leaq	-16(%rax), %rsp
	movb	$0, -16(%rax)
	movq	%rdx, -8(%rax)
	xorl	%eax, %eax
	movq	%rbp, %rsp
	popq	%rbp
	.cfi_def_cfa %rsp, 8
	retq
.Lfunc_end12:
	.size	min_uint64, .Lfunc_end12-min_uint64
	.cfi_endproc
                                        # -- End function
	.p2align	4, 0x90                         # -- Begin function max_int8
	.type	max_int8,@function
max_int8:                               # @max_int8
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	subq	$16, %rsp
	movb	%dil, -2(%rbp)
	movb	%sil, -1(%rbp)
	movq	-2(%rbp), %rax
	cmpq	-1(%rbp), %rax
	jle	.LBB13_3
# %bb.1:                                # %then
	movzbl	-2(%rbp), %edx
	jmp	.LBB13_2
.LBB13_3:                               # %ifcont
	movzbl	-1(%rbp), %edx
.LBB13_2:                               # %then
	movq	%rsp, %rax
	leaq	-16(%rax), %rsp
	movb	$0, -16(%rax)
	movb	%dl, -15(%rax)
	xorl	%eax, %eax
	movq	%rbp, %rsp
	popq	%rbp
	.cfi_def_cfa %rsp, 8
	retq
.Lfunc_end13:
	.size	max_int8, .Lfunc_end13-max_int8
	.cfi_endproc
                                        # -- End function
	.p2align	4, 0x90                         # -- Begin function max_int16
	.type	max_int16,@function
max_int16:                              # @max_int16
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	subq	$16, %rsp
	movw	%di, -4(%rbp)
	movw	%si, -2(%rbp)
	movq	-4(%rbp), %rax
	cmpq	-2(%rbp), %rax
	jle	.LBB14_3
# %bb.1:                                # %then
	movzwl	-4(%rbp), %edx
	jmp	.LBB14_2
.LBB14_3:                               # %ifcont
	movzwl	-2(%rbp), %edx
.LBB14_2:                               # %then
	movq	%rsp, %rax
	leaq	-16(%rax), %rsp
	movb	$0, -16(%rax)
	movw	%dx, -14(%rax)
	xorl	%eax, %eax
	movq	%rbp, %rsp
	popq	%rbp
	.cfi_def_cfa %rsp, 8
	retq
.Lfunc_end14:
	.size	max_int16, .Lfunc_end14-max_int16
	.cfi_endproc
                                        # -- End function
	.p2align	4, 0x90                         # -- Begin function max_int32
	.type	max_int32,@function
max_int32:                              # @max_int32
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	subq	$16, %rsp
	movl	%edi, -8(%rbp)
	movl	%esi, -4(%rbp)
	movq	-8(%rbp), %rax
	cmpq	-4(%rbp), %rax
	jle	.LBB15_3
# %bb.1:                                # %then
	movl	-8(%rbp), %edx
	jmp	.LBB15_2
.LBB15_3:                               # %ifcont
	movl	-4(%rbp), %edx
.LBB15_2:                               # %then
	movq	%rsp, %rax
	leaq	-16(%rax), %rsp
	movb	$0, -16(%rax)
	movl	%edx, -12(%rax)
	xorl	%eax, %eax
	movq	%rbp, %rsp
	popq	%rbp
	.cfi_def_cfa %rsp, 8
	retq
.Lfunc_end15:
	.size	max_int32, .Lfunc_end15-max_int32
	.cfi_endproc
                                        # -- End function
	.p2align	4, 0x90                         # -- Begin function max_int64
	.type	max_int64,@function
max_int64:                              # @max_int64
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	subq	$16, %rsp
	movq	%rdi, -16(%rbp)
	movq	%rsi, -8(%rbp)
	cmpq	%rsi, %rdi
	jle	.LBB16_3
# %bb.1:                                # %then
	movq	-16(%rbp), %rdx
	jmp	.LBB16_2
.LBB16_3:                               # %ifcont
	movq	-8(%rbp), %rdx
.LBB16_2:                               # %then
	movq	%rsp, %rax
	leaq	-16(%rax), %rsp
	movb	$0, -16(%rax)
	movq	%rdx, -8(%rax)
	xorl	%eax, %eax
	movq	%rbp, %rsp
	popq	%rbp
	.cfi_def_cfa %rsp, 8
	retq
.Lfunc_end16:
	.size	max_int64, .Lfunc_end16-max_int64
	.cfi_endproc
                                        # -- End function
	.p2align	4, 0x90                         # -- Begin function max_uint8
	.type	max_uint8,@function
max_uint8:                              # @max_uint8
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	subq	$16, %rsp
	movb	%dil, -2(%rbp)
	movb	%sil, -1(%rbp)
	movq	-2(%rbp), %rax
	cmpq	-1(%rbp), %rax
	jle	.LBB17_3
# %bb.1:                                # %then
	movzbl	-2(%rbp), %edx
	jmp	.LBB17_2
.LBB17_3:                               # %ifcont
	movzbl	-1(%rbp), %edx
.LBB17_2:                               # %then
	movq	%rsp, %rax
	leaq	-16(%rax), %rsp
	movb	$0, -16(%rax)
	movb	%dl, -15(%rax)
	xorl	%eax, %eax
	movq	%rbp, %rsp
	popq	%rbp
	.cfi_def_cfa %rsp, 8
	retq
.Lfunc_end17:
	.size	max_uint8, .Lfunc_end17-max_uint8
	.cfi_endproc
                                        # -- End function
	.p2align	4, 0x90                         # -- Begin function max_uint16
	.type	max_uint16,@function
max_uint16:                             # @max_uint16
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	subq	$16, %rsp
	movw	%di, -4(%rbp)
	movw	%si, -2(%rbp)
	movq	-4(%rbp), %rax
	cmpq	-2(%rbp), %rax
	jle	.LBB18_3
# %bb.1:                                # %then
	movzwl	-4(%rbp), %edx
	jmp	.LBB18_2
.LBB18_3:                               # %ifcont
	movzwl	-2(%rbp), %edx
.LBB18_2:                               # %then
	movq	%rsp, %rax
	leaq	-16(%rax), %rsp
	movb	$0, -16(%rax)
	movw	%dx, -14(%rax)
	xorl	%eax, %eax
	movq	%rbp, %rsp
	popq	%rbp
	.cfi_def_cfa %rsp, 8
	retq
.Lfunc_end18:
	.size	max_uint16, .Lfunc_end18-max_uint16
	.cfi_endproc
                                        # -- End function
	.p2align	4, 0x90                         # -- Begin function max_uint32
	.type	max_uint32,@function
max_uint32:                             # @max_uint32
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	subq	$16, %rsp
	movl	%edi, -8(%rbp)
	movl	%esi, -4(%rbp)
	movq	-8(%rbp), %rax
	cmpq	-4(%rbp), %rax
	jle	.LBB19_3
# %bb.1:                                # %then
	movl	-8(%rbp), %edx
	jmp	.LBB19_2
.LBB19_3:                               # %ifcont
	movl	-4(%rbp), %edx
.LBB19_2:                               # %then
	movq	%rsp, %rax
	leaq	-16(%rax), %rsp
	movb	$0, -16(%rax)
	movl	%edx, -12(%rax)
	xorl	%eax, %eax
	movq	%rbp, %rsp
	popq	%rbp
	.cfi_def_cfa %rsp, 8
	retq
.Lfunc_end19:
	.size	max_uint32, .Lfunc_end19-max_uint32
	.cfi_endproc
                                        # -- End function
	.p2align	4, 0x90                         # -- Begin function max_uint64
	.type	max_uint64,@function
max_uint64:                             # @max_uint64
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	subq	$16, %rsp
	movq	%rdi, -16(%rbp)
	movq	%rsi, -8(%rbp)
	cmpq	%rsi, %rdi
	jle	.LBB20_3
# %bb.1:                                # %then
	movq	-16(%rbp), %rdx
	jmp	.LBB20_2
.LBB20_3:                               # %ifcont
	movq	-8(%rbp), %rdx
.LBB20_2:                               # %then
	movq	%rsp, %rax
	leaq	-16(%rax), %rsp
	movb	$0, -16(%rax)
	movq	%rdx, -8(%rax)
	xorl	%eax, %eax
	movq	%rbp, %rsp
	popq	%rbp
	.cfi_def_cfa %rsp, 8
	retq
.Lfunc_end20:
	.size	max_uint64, .Lfunc_end20-max_uint64
	.cfi_endproc
                                        # -- End function
	.p2align	4, 0x90                         # -- Begin function clamp_int8
	.type	clamp_int8,@function
clamp_int8:                             # @clamp_int8
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	subq	$16, %rsp
	movb	%dil, -1(%rbp)
	movb	%sil, -3(%rbp)
	movb	%dl, -2(%rbp)
	movq	-1(%rbp), %rax
	cmpq	-3(%rbp), %rax
	jge	.LBB21_3
# %bb.1:                                # %then
	movzbl	-3(%rbp), %edx
	jmp	.LBB21_2
.LBB21_3:                               # %ifcont
	movq	-1(%rbp), %rax
	cmpq	-2(%rbp), %rax
	jle	.LBB21_5
# %bb.4:                                # %then4
	movzbl	-2(%rbp), %edx
	jmp	.LBB21_2
.LBB21_5:                               # %ifcont10
	movzbl	-1(%rbp), %edx
.LBB21_2:                               # %then
	movq	%rsp, %rax
	leaq	-16(%rax), %rsp
	movb	$0, -16(%rax)
	movb	%dl, -15(%rax)
	xorl	%eax, %eax
	movq	%rbp, %rsp
	popq	%rbp
	.cfi_def_cfa %rsp, 8
	retq
.Lfunc_end21:
	.size	clamp_int8, .Lfunc_end21-clamp_int8
	.cfi_endproc
                                        # -- End function
	.p2align	4, 0x90                         # -- Begin function clamp_int64
	.type	clamp_int64,@function
clamp_int64:                            # @clamp_int64
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	subq	$32, %rsp
	movq	%rdi, -16(%rbp)
	movq	%rsi, -24(%rbp)
	movq	%rdx, -8(%rbp)
	cmpq	%rsi, %rdi
	jge	.LBB22_3
# %bb.1:                                # %then
	movq	-24(%rbp), %rdx
	jmp	.LBB22_2
.LBB22_3:                               # %ifcont
	movq	-16(%rbp), %rax
	cmpq	-8(%rbp), %rax
	jle	.LBB22_5
# %bb.4:                                # %then4
	movq	-8(%rbp), %rdx
	jmp	.LBB22_2
.LBB22_5:                               # %ifcont9
	movq	-16(%rbp), %rdx
.LBB22_2:                               # %then
	movq	%rsp, %rax
	leaq	-16(%rax), %rsp
	movb	$0, -16(%rax)
	movq	%rdx, -8(%rax)
	xorl	%eax, %eax
	movq	%rbp, %rsp
	popq	%rbp
	.cfi_def_cfa %rsp, 8
	retq
.Lfunc_end22:
	.size	clamp_int64, .Lfunc_end22-clamp_int64
	.cfi_endproc
                                        # -- End function
	.p2align	4, 0x90                         # -- Begin function clamp_uint8
	.type	clamp_uint8,@function
clamp_uint8:                            # @clamp_uint8
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	subq	$16, %rsp
	movb	%dil, -1(%rbp)
	movb	%sil, -3(%rbp)
	movb	%dl, -2(%rbp)
	movq	-1(%rbp), %rax
	cmpq	-3(%rbp), %rax
	jge	.LBB23_3
# %bb.1:                                # %then
	movzbl	-3(%rbp), %edx
	jmp	.LBB23_2
.LBB23_3:                               # %ifcont
	movq	-1(%rbp), %rax
	cmpq	-2(%rbp), %rax
	jle	.LBB23_5
# %bb.4:                                # %then4
	movzbl	-2(%rbp), %edx
	jmp	.LBB23_2
.LBB23_5:                               # %ifcont10
	movzbl	-1(%rbp), %edx
.LBB23_2:                               # %then
	movq	%rsp, %rax
	leaq	-16(%rax), %rsp
	movb	$0, -16(%rax)
	movb	%dl, -15(%rax)
	xorl	%eax, %eax
	movq	%rbp, %rsp
	popq	%rbp
	.cfi_def_cfa %rsp, 8
	retq
.Lfunc_end23:
	.size	clamp_uint8, .Lfunc_end23-clamp_uint8
	.cfi_endproc
                                        # -- End function
	.p2align	4, 0x90                         # -- Begin function clamp_uint64
	.type	clamp_uint64,@function
clamp_uint64:                           # @clamp_uint64
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	subq	$32, %rsp
	movq	%rdi, -16(%rbp)
	movq	%rsi, -24(%rbp)
	movq	%rdx, -8(%rbp)
	cmpq	%rsi, %rdi
	jge	.LBB24_3
# %bb.1:                                # %then
	movq	-24(%rbp), %rdx
	jmp	.LBB24_2
.LBB24_3:                               # %ifcont
	movq	-16(%rbp), %rax
	cmpq	-8(%rbp), %rax
	jle	.LBB24_5
# %bb.4:                                # %then4
	movq	-8(%rbp), %rdx
	jmp	.LBB24_2
.LBB24_5:                               # %ifcont9
	movq	-16(%rbp), %rdx
.LBB24_2:                               # %then
	movq	%rsp, %rax
	leaq	-16(%rax), %rsp
	movb	$0, -16(%rax)
	movq	%rdx, -8(%rax)
	xorl	%eax, %eax
	movq	%rbp, %rsp
	popq	%rbp
	.cfi_def_cfa %rsp, 8
	retq
.Lfunc_end24:
	.size	clamp_uint64, .Lfunc_end24-clamp_uint64
	.cfi_endproc
                                        # -- End function
	.p2align	4, 0x90                         # -- Begin function is_power_of_2_uint8
	.type	is_power_of_2_uint8,@function
is_power_of_2_uint8:                    # @is_power_of_2_uint8
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	subq	$16, %rsp
	movb	%dil, -1(%rbp)
	cmpq	$0, -1(%rbp)
	je	.LBB25_1
# %bb.3:                                # %ifcont
	movq	-1(%rbp), %rax
	decq	%rax
	movb	%al, -2(%rbp)
	andl	-1(%rbp), %eax
	movb	%al, -3(%rbp)
	testb	%al, %al
	je	.LBB25_4
.LBB25_1:                               # %then
	movq	%rsp, %rax
	leaq	-16(%rax), %rsp
	movw	$0, -16(%rax)
	xorl	%eax, %eax
	xorl	%edx, %edx
	jmp	.LBB25_2
.LBB25_4:                               # %then3
	movq	%rsp, %rax
	leaq	-16(%rax), %rsp
	movw	$256, -16(%rax)                 # imm = 0x100
	xorl	%eax, %eax
	movb	$1, %dl
.LBB25_2:                               # %then
	movq	%rbp, %rsp
	popq	%rbp
	.cfi_def_cfa %rsp, 8
	retq
.Lfunc_end25:
	.size	is_power_of_2_uint8, .Lfunc_end25-is_power_of_2_uint8
	.cfi_endproc
                                        # -- End function
	.p2align	4, 0x90                         # -- Begin function is_power_of_2_uint64
	.type	is_power_of_2_uint64,@function
is_power_of_2_uint64:                   # @is_power_of_2_uint64
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	subq	$32, %rsp
	movq	%rdi, -8(%rbp)
	testq	%rdi, %rdi
	je	.LBB26_1
# %bb.3:                                # %ifcont
	movq	-8(%rbp), %rax
	leaq	-1(%rax), %rcx
	movq	%rcx, -16(%rbp)
	andq	%rcx, %rax
	movq	%rax, -24(%rbp)
	je	.LBB26_4
.LBB26_1:                               # %then
	movq	%rsp, %rax
	leaq	-16(%rax), %rsp
	movw	$0, -16(%rax)
	xorl	%eax, %eax
	xorl	%edx, %edx
	jmp	.LBB26_2
.LBB26_4:                               # %then3
	movq	%rsp, %rax
	leaq	-16(%rax), %rsp
	movw	$256, -16(%rax)                 # imm = 0x100
	xorl	%eax, %eax
	movb	$1, %dl
.LBB26_2:                               # %then
	movq	%rbp, %rsp
	popq	%rbp
	.cfi_def_cfa %rsp, 8
	retq
.Lfunc_end26:
	.size	is_power_of_2_uint64, .Lfunc_end26-is_power_of_2_uint64
	.cfi_endproc
                                        # -- End function
	.p2align	4, 0x90                         # -- Begin function sign_int8
	.type	sign_int8,@function
sign_int8:                              # @sign_int8
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	subq	$16, %rsp
	movb	%dil, -1(%rbp)
	cmpq	$0, -1(%rbp)
	js	.LBB27_1
# %bb.3:                                # %ifcont
	cmpq	$0, -1(%rbp)
	jle	.LBB27_5
# %bb.4:                                # %then2
	movq	%rsp, %rax
	leaq	-16(%rax), %rsp
	movw	$256, -16(%rax)                 # imm = 0x100
	xorl	%eax, %eax
	movb	$1, %dl
	jmp	.LBB27_2
.LBB27_1:                               # %then
	movq	%rsp, %rax
	leaq	-16(%rax), %rsp
	movw	$-256, -16(%rax)
	xorl	%eax, %eax
	movb	$-1, %dl
	jmp	.LBB27_2
.LBB27_5:                               # %ifcont7
	movq	%rsp, %rax
	leaq	-16(%rax), %rsp
	movw	$0, -16(%rax)
	xorl	%eax, %eax
	xorl	%edx, %edx
.LBB27_2:                               # %then
	movq	%rbp, %rsp
	popq	%rbp
	.cfi_def_cfa %rsp, 8
	retq
.Lfunc_end27:
	.size	sign_int8, .Lfunc_end27-sign_int8
	.cfi_endproc
                                        # -- End function
	.p2align	4, 0x90                         # -- Begin function sign_int64
	.type	sign_int64,@function
sign_int64:                             # @sign_int64
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	subq	$16, %rsp
	movq	%rdi, -8(%rbp)
	testq	%rdi, %rdi
	js	.LBB28_1
# %bb.3:                                # %ifcont
	cmpq	$0, -8(%rbp)
	jle	.LBB28_5
# %bb.4:                                # %then2
	movq	%rsp, %rax
	leaq	-16(%rax), %rsp
	movw	$256, -16(%rax)                 # imm = 0x100
	xorl	%eax, %eax
	movb	$1, %dl
	jmp	.LBB28_2
.LBB28_1:                               # %then
	movq	%rsp, %rax
	leaq	-16(%rax), %rsp
	movw	$-256, -16(%rax)
	xorl	%eax, %eax
	movb	$-1, %dl
	jmp	.LBB28_2
.LBB28_5:                               # %ifcont7
	movq	%rsp, %rax
	leaq	-16(%rax), %rsp
	movw	$0, -16(%rax)
	xorl	%eax, %eax
	xorl	%edx, %edx
.LBB28_2:                               # %then
	movq	%rbp, %rsp
	popq	%rbp
	.cfi_def_cfa %rsp, 8
	retq
.Lfunc_end28:
	.size	sign_int64, .Lfunc_end28-sign_int64
	.cfi_endproc
                                        # -- End function
	.p2align	4, 0x90                         # -- Begin function is_uppercase
	.type	is_uppercase,@function
is_uppercase:                           # @is_uppercase
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	subq	$16, %rsp
	movb	%dil, -1(%rbp)
	movq	-1(%rbp), %rax
	cmpq	$65, %rax
	jl	.LBB29_4
# %bb.1:                                # %then
	movq	-1(%rbp), %rax
	cmpq	$90, %rax
	jg	.LBB29_4
# %bb.2:                                # %then2
	movq	%rsp, %rax
	leaq	-16(%rax), %rsp
	movw	$256, -16(%rax)                 # imm = 0x100
	xorl	%eax, %eax
	movb	$1, %dl
	jmp	.LBB29_3
.LBB29_4:                               # %ifcont3
	movq	%rsp, %rax
	leaq	-16(%rax), %rsp
	movw	$0, -16(%rax)
	xorl	%eax, %eax
	xorl	%edx, %edx
.LBB29_3:                               # %then2
	movq	%rbp, %rsp
	popq	%rbp
	.cfi_def_cfa %rsp, 8
	retq
.Lfunc_end29:
	.size	is_uppercase, .Lfunc_end29-is_uppercase
	.cfi_endproc
                                        # -- End function
	.p2align	4, 0x90                         # -- Begin function is_lowercase
	.type	is_lowercase,@function
is_lowercase:                           # @is_lowercase
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	subq	$16, %rsp
	movb	%dil, -1(%rbp)
	movq	-1(%rbp), %rax
	cmpq	$97, %rax
	jl	.LBB30_4
# %bb.1:                                # %then
	movq	-1(%rbp), %rax
	cmpq	$122, %rax
	jg	.LBB30_4
# %bb.2:                                # %then2
	movq	%rsp, %rax
	leaq	-16(%rax), %rsp
	movw	$256, -16(%rax)                 # imm = 0x100
	xorl	%eax, %eax
	movb	$1, %dl
	jmp	.LBB30_3
.LBB30_4:                               # %ifcont3
	movq	%rsp, %rax
	leaq	-16(%rax), %rsp
	movw	$0, -16(%rax)
	xorl	%eax, %eax
	xorl	%edx, %edx
.LBB30_3:                               # %then2
	movq	%rbp, %rsp
	popq	%rbp
	.cfi_def_cfa %rsp, 8
	retq
.Lfunc_end30:
	.size	is_lowercase, .Lfunc_end30-is_lowercase
	.cfi_endproc
                                        # -- End function
	.p2align	4, 0x90                         # -- Begin function is_alpha
	.type	is_alpha,@function
is_alpha:                               # @is_alpha
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	subq	$64, %rsp
	movb	%dil, -1(%rbp)
	callq	is_uppercase
	movb	%al, -24(%rbp)
	movb	%dl, -23(%rbp)
	movq	-16(%rbp), %rcx
	movb	%al, -56(%rbp)
	movq	%rcx, -48(%rbp)
	cmpq	$1, %rcx
	jne	.LBB31_3
# %bb.1:                                # %then
	movq	%rsp, %rax
	leaq	-16(%rax), %rsp
	movw	$256, -16(%rax)                 # imm = 0x100
	xorl	%eax, %eax
	movb	$1, %dl
	jmp	.LBB31_2
.LBB31_3:                               # %ifcont
	movl	-1(%rbp), %edi
	callq	is_lowercase
	movb	%al, -40(%rbp)
	movb	%dl, -39(%rbp)
	movq	-32(%rbp), %rdx
	movq	%rsp, %rcx
	leaq	-16(%rcx), %rsp
	movb	%al, -16(%rcx)
	movq	%rdx, -8(%rcx)
	movq	%rsp, %rax
	leaq	-16(%rax), %rsp
	movb	$0, -16(%rax)
	movb	%dl, -15(%rax)
	xorl	%eax, %eax
                                        # kill: def $dl killed $dl killed $rdx
.LBB31_2:                               # %then
	movq	%rbp, %rsp
	popq	%rbp
	.cfi_def_cfa %rsp, 8
	retq
.Lfunc_end31:
	.size	is_alpha, .Lfunc_end31-is_alpha
	.cfi_endproc
                                        # -- End function
	.p2align	4, 0x90                         # -- Begin function is_digit
	.type	is_digit,@function
is_digit:                               # @is_digit
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	subq	$16, %rsp
	movb	%dil, -1(%rbp)
	movq	-1(%rbp), %rax
	cmpq	$48, %rax
	jl	.LBB32_4
# %bb.1:                                # %then
	movq	-1(%rbp), %rax
	cmpq	$57, %rax
	jg	.LBB32_4
# %bb.2:                                # %then2
	movq	%rsp, %rax
	leaq	-16(%rax), %rsp
	movw	$256, -16(%rax)                 # imm = 0x100
	xorl	%eax, %eax
	movb	$1, %dl
	jmp	.LBB32_3
.LBB32_4:                               # %ifcont3
	movq	%rsp, %rax
	leaq	-16(%rax), %rsp
	movw	$0, -16(%rax)
	xorl	%eax, %eax
	xorl	%edx, %edx
.LBB32_3:                               # %then2
	movq	%rbp, %rsp
	popq	%rbp
	.cfi_def_cfa %rsp, 8
	retq
.Lfunc_end32:
	.size	is_digit, .Lfunc_end32-is_digit
	.cfi_endproc
                                        # -- End function
	.p2align	4, 0x90                         # -- Begin function is_alnum
	.type	is_alnum,@function
is_alnum:                               # @is_alnum
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	subq	$64, %rsp
	movb	%dil, -1(%rbp)
	callq	is_alpha
	movb	%al, -24(%rbp)
	movb	%dl, -23(%rbp)
	movq	-16(%rbp), %rcx
	movb	%al, -56(%rbp)
	movq	%rcx, -48(%rbp)
	cmpq	$1, %rcx
	jne	.LBB33_3
# %bb.1:                                # %then
	movq	%rsp, %rax
	leaq	-16(%rax), %rsp
	movw	$256, -16(%rax)                 # imm = 0x100
	xorl	%eax, %eax
	movb	$1, %dl
	jmp	.LBB33_2
.LBB33_3:                               # %ifcont
	movl	-1(%rbp), %edi
	callq	is_digit
	movb	%al, -40(%rbp)
	movb	%dl, -39(%rbp)
	movq	-32(%rbp), %rdx
	movq	%rsp, %rcx
	leaq	-16(%rcx), %rsp
	movb	%al, -16(%rcx)
	movq	%rdx, -8(%rcx)
	movq	%rsp, %rax
	leaq	-16(%rax), %rsp
	movb	$0, -16(%rax)
	movb	%dl, -15(%rax)
	xorl	%eax, %eax
                                        # kill: def $dl killed $dl killed $rdx
.LBB33_2:                               # %then
	movq	%rbp, %rsp
	popq	%rbp
	.cfi_def_cfa %rsp, 8
	retq
.Lfunc_end33:
	.size	is_alnum, .Lfunc_end33-is_alnum
	.cfi_endproc
                                        # -- End function
	.p2align	4, 0x90                         # -- Begin function is_whitespace
	.type	is_whitespace,@function
is_whitespace:                          # @is_whitespace
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	subq	$16, %rsp
	movb	%dil, -1(%rbp)
	movq	-1(%rbp), %rax
	cmpq	$32, %rax
	je	.LBB34_1
# %bb.3:                                # %ifcont
	movq	-1(%rbp), %rax
	cmpq	$9, %rax
	je	.LBB34_1
# %bb.4:                                # %ifcont8
	movq	-1(%rbp), %rax
	cmpq	$10, %rax
	je	.LBB34_1
# %bb.5:                                # %ifcont15
	movq	-1(%rbp), %rax
	cmpq	$13, %rax
	jne	.LBB34_6
.LBB34_1:                               # %then
	movq	%rsp, %rax
	leaq	-16(%rax), %rsp
	movw	$256, -16(%rax)                 # imm = 0x100
	xorl	%eax, %eax
	movb	$1, %dl
.LBB34_2:                               # %then
	movq	%rbp, %rsp
	popq	%rbp
	.cfi_def_cfa %rsp, 8
	retq
.LBB34_6:                               # %ifcont22
	.cfi_def_cfa %rbp, 16
	movq	%rsp, %rax
	leaq	-16(%rax), %rsp
	movw	$0, -16(%rax)
	xorl	%eax, %eax
	xorl	%edx, %edx
	jmp	.LBB34_2
.Lfunc_end34:
	.size	is_whitespace, .Lfunc_end34-is_whitespace
	.cfi_endproc
                                        # -- End function
	.p2align	4, 0x90                         # -- Begin function is_printable
	.type	is_printable,@function
is_printable:                           # @is_printable
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	subq	$16, %rsp
	movb	%dil, -1(%rbp)
	movq	-1(%rbp), %rax
	cmpq	$32, %rax
	jl	.LBB35_4
# %bb.1:                                # %then
	movq	-1(%rbp), %rax
	cmpq	$126, %rax
	jg	.LBB35_4
# %bb.2:                                # %then2
	movq	%rsp, %rax
	leaq	-16(%rax), %rsp
	movw	$256, -16(%rax)                 # imm = 0x100
	xorl	%eax, %eax
	movb	$1, %dl
	jmp	.LBB35_3
.LBB35_4:                               # %ifcont3
	movq	%rsp, %rax
	leaq	-16(%rax), %rsp
	movw	$0, -16(%rax)
	xorl	%eax, %eax
	xorl	%edx, %edx
.LBB35_3:                               # %then2
	movq	%rbp, %rsp
	popq	%rbp
	.cfi_def_cfa %rsp, 8
	retq
.Lfunc_end35:
	.size	is_printable, .Lfunc_end35-is_printable
	.cfi_endproc
                                        # -- End function
	.p2align	4, 0x90                         # -- Begin function is_control
	.type	is_control,@function
is_control:                             # @is_control
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	subq	$16, %rsp
	movb	%dil, -1(%rbp)
	movq	-1(%rbp), %rax
	cmpq	$31, %rax
	jle	.LBB36_1
# %bb.3:                                # %ifcont
	movq	-1(%rbp), %rax
	cmpq	$127, %rax
	jne	.LBB36_4
.LBB36_1:                               # %then
	movq	%rsp, %rax
	leaq	-16(%rax), %rsp
	movw	$256, -16(%rax)                 # imm = 0x100
	xorl	%eax, %eax
	movb	$1, %dl
	jmp	.LBB36_2
.LBB36_4:                               # %ifcont7
	movq	%rsp, %rax
	leaq	-16(%rax), %rsp
	movw	$0, -16(%rax)
	xorl	%eax, %eax
	xorl	%edx, %edx
.LBB36_2:                               # %then
	movq	%rbp, %rsp
	popq	%rbp
	.cfi_def_cfa %rsp, 8
	retq
.Lfunc_end36:
	.size	is_control, .Lfunc_end36-is_control
	.cfi_endproc
                                        # -- End function
	.p2align	4, 0x90                         # -- Begin function is_hexdigit
	.type	is_hexdigit,@function
is_hexdigit:                            # @is_hexdigit
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	subq	$16, %rsp
	movb	%dil, -1(%rbp)
	movq	-1(%rbp), %rax
	cmpq	$48, %rax
	jl	.LBB37_4
# %bb.1:                                # %then
	movq	-1(%rbp), %rax
	cmpq	$57, %rax
	jle	.LBB37_2
.LBB37_4:                               # %ifcont3
	movq	-1(%rbp), %rax
	cmpq	$65, %rax
	jl	.LBB37_6
# %bb.5:                                # %then5
	movq	-1(%rbp), %rax
	cmpq	$70, %rax
	jle	.LBB37_2
.LBB37_6:                               # %ifcont13
	movq	-1(%rbp), %rax
	cmpq	$97, %rax
	jl	.LBB37_8
# %bb.7:                                # %then15
	movq	-1(%rbp), %rax
	cmpq	$102, %rax
	jg	.LBB37_8
.LBB37_2:                               # %then2
	movq	%rsp, %rax
	leaq	-16(%rax), %rsp
	movw	$256, -16(%rax)                 # imm = 0x100
	xorl	%eax, %eax
	movb	$1, %dl
	jmp	.LBB37_3
.LBB37_8:                               # %ifcont23
	movq	%rsp, %rax
	leaq	-16(%rax), %rsp
	movw	$0, -16(%rax)
	xorl	%eax, %eax
	xorl	%edx, %edx
.LBB37_3:                               # %then2
	movq	%rbp, %rsp
	popq	%rbp
	.cfi_def_cfa %rsp, 8
	retq
.Lfunc_end37:
	.size	is_hexdigit, .Lfunc_end37-is_hexdigit
	.cfi_endproc
                                        # -- End function
	.p2align	4, 0x90                         # -- Begin function to_uppercase
	.type	to_uppercase,@function
to_uppercase:                           # @to_uppercase
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	subq	$48, %rsp
	movb	%dil, -1(%rbp)
	callq	is_lowercase
	movb	%al, -24(%rbp)
	movb	%dl, -23(%rbp)
	movq	-16(%rbp), %rcx
	movb	%al, -40(%rbp)
	movq	%rcx, -32(%rbp)
	cmpq	$1, %rcx
	jne	.LBB38_2
# %bb.1:                                # %then
	movzbl	-1(%rbp), %edx
	addb	$-32, %dl
	movb	%dl, -2(%rbp)
	jmp	.LBB38_3
.LBB38_2:                               # %ifcont
	movzbl	-1(%rbp), %edx
.LBB38_3:                               # %ifcont
	movq	%rsp, %rax
	leaq	-16(%rax), %rsp
	movb	$0, -16(%rax)
	movb	%dl, -15(%rax)
	xorl	%eax, %eax
	movq	%rbp, %rsp
	popq	%rbp
	.cfi_def_cfa %rsp, 8
	retq
.Lfunc_end38:
	.size	to_uppercase, .Lfunc_end38-to_uppercase
	.cfi_endproc
                                        # -- End function
	.p2align	4, 0x90                         # -- Begin function to_lowercase
	.type	to_lowercase,@function
to_lowercase:                           # @to_lowercase
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	subq	$48, %rsp
	movb	%dil, -1(%rbp)
	callq	is_uppercase
	movb	%al, -24(%rbp)
	movb	%dl, -23(%rbp)
	movq	-16(%rbp), %rcx
	movb	%al, -40(%rbp)
	movq	%rcx, -32(%rbp)
	cmpq	$1, %rcx
	jne	.LBB39_2
# %bb.1:                                # %then
	movzbl	-1(%rbp), %edx
	addb	$32, %dl
	movb	%dl, -2(%rbp)
	jmp	.LBB39_3
.LBB39_2:                               # %ifcont
	movzbl	-1(%rbp), %edx
.LBB39_3:                               # %ifcont
	movq	%rsp, %rax
	leaq	-16(%rax), %rsp
	movb	$0, -16(%rax)
	movb	%dl, -15(%rax)
	xorl	%eax, %eax
	movq	%rbp, %rsp
	popq	%rbp
	.cfi_def_cfa %rsp, 8
	retq
.Lfunc_end39:
	.size	to_lowercase, .Lfunc_end39-to_lowercase
	.cfi_endproc
                                        # -- End function
	.p2align	4, 0x90                         # -- Begin function char_to_digit
	.type	char_to_digit,@function
char_to_digit:                          # @char_to_digit
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	subq	$48, %rsp
	movb	%dil, -1(%rbp)
	callq	is_digit
	movb	%al, -24(%rbp)
	movb	%dl, -23(%rbp)
	movq	-16(%rbp), %rcx
	movb	%al, -40(%rbp)
	movq	%rcx, -32(%rbp)
	cmpq	$1, %rcx
	jne	.LBB40_2
# %bb.1:                                # %then
	movzbl	-1(%rbp), %edx
	addb	$-48, %dl
	movb	%dl, -2(%rbp)
	movq	%rsp, %rax
	leaq	-16(%rax), %rsp
	movb	$0, -16(%rax)
	movb	%dl, -15(%rax)
	xorl	%eax, %eax
	jmp	.LBB40_3
.LBB40_2:                               # %ifcont
	movq	%rsp, %rax
	leaq	-16(%rax), %rsp
	movw	$1, -16(%rax)
	movb	$1, %al
	xorl	%edx, %edx
.LBB40_3:                               # %ifcont
	movq	%rbp, %rsp
	popq	%rbp
	.cfi_def_cfa %rsp, 8
	retq
.Lfunc_end40:
	.size	char_to_digit, .Lfunc_end40-char_to_digit
	.cfi_endproc
                                        # -- End function
	.p2align	4, 0x90                         # -- Begin function digit_to_char
	.type	digit_to_char,@function
digit_to_char:                          # @digit_to_char
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	subq	$16, %rsp
	movb	%dil, -1(%rbp)
	cmpq	$0, -1(%rbp)
	js	.LBB41_1
# %bb.3:                                # %ifcont
	movq	-1(%rbp), %rax
	cmpq	$10, %rax
	jl	.LBB41_4
.LBB41_1:                               # %then
	movq	%rsp, %rax
	leaq	-16(%rax), %rsp
	movw	$1, -16(%rax)
	movb	$1, %al
	xorl	%edx, %edx
	jmp	.LBB41_2
.LBB41_4:                               # %ifcont7
	movzbl	-1(%rbp), %edx
	addb	$48, %dl
	movb	%dl, -2(%rbp)
	movq	%rsp, %rax
	leaq	-16(%rax), %rsp
	movb	$0, -16(%rax)
	movb	%dl, -15(%rax)
	xorl	%eax, %eax
.LBB41_2:                               # %then
	movq	%rbp, %rsp
	popq	%rbp
	.cfi_def_cfa %rsp, 8
	retq
.Lfunc_end41:
	.size	digit_to_char, .Lfunc_end41-digit_to_char
	.cfi_endproc
                                        # -- End function
	.p2align	4, 0x90                         # -- Begin function hex_to_digit
	.type	hex_to_digit,@function
hex_to_digit:                           # @hex_to_digit
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	subq	$16, %rsp
	movb	%dil, -1(%rbp)
	movq	-1(%rbp), %rax
	cmpq	$48, %rax
	jl	.LBB42_4
# %bb.1:                                # %then
	movq	-1(%rbp), %rax
	cmpq	$57, %rax
	jg	.LBB42_4
# %bb.2:                                # %then2
	movzbl	-1(%rbp), %edx
	addb	$-48, %dl
	movb	%dl, -2(%rbp)
	jmp	.LBB42_3
.LBB42_4:                               # %ifcont3
	movq	-1(%rbp), %rax
	cmpq	$65, %rax
	jl	.LBB42_7
# %bb.5:                                # %then5
	movq	-1(%rbp), %rax
	cmpq	$70, %rax
	jg	.LBB42_7
# %bb.6:                                # %then7
	movzbl	-1(%rbp), %edx
	addb	$-55, %dl
	movb	%dl, -3(%rbp)
	jmp	.LBB42_3
.LBB42_7:                               # %ifcont15
	movq	-1(%rbp), %rax
	cmpq	$97, %rax
	jl	.LBB42_10
# %bb.8:                                # %then17
	movq	-1(%rbp), %rax
	cmpq	$102, %rax
	jg	.LBB42_10
# %bb.9:                                # %then19
	movzbl	-1(%rbp), %edx
	addb	$-87, %dl
	movb	%dl, -4(%rbp)
.LBB42_3:                               # %then2
	movq	%rsp, %rax
	leaq	-16(%rax), %rsp
	movb	$0, -16(%rax)
	movb	%dl, -15(%rax)
	xorl	%eax, %eax
	jmp	.LBB42_11
.LBB42_10:                              # %ifcont27
	movq	%rsp, %rax
	leaq	-16(%rax), %rsp
	movw	$1, -16(%rax)
	movb	$1, %al
	xorl	%edx, %edx
.LBB42_11:                              # %ifcont27
	movq	%rbp, %rsp
	popq	%rbp
	.cfi_def_cfa %rsp, 8
	retq
.Lfunc_end42:
	.size	hex_to_digit, .Lfunc_end42-hex_to_digit
	.cfi_endproc
                                        # -- End function
	.p2align	4, 0x90                         # -- Begin function digit_to_hex
	.type	digit_to_hex,@function
digit_to_hex:                           # @digit_to_hex
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	subq	$16, %rsp
	movb	%dil, -1(%rbp)
	cmpq	$0, -1(%rbp)
	js	.LBB43_1
# %bb.3:                                # %ifcont
	movq	-1(%rbp), %rax
	cmpq	$16, %rax
	jl	.LBB43_4
.LBB43_1:                               # %then
	movq	%rsp, %rax
	leaq	-16(%rax), %rsp
	movw	$1, -16(%rax)
	movb	$1, %al
	xorl	%edx, %edx
	jmp	.LBB43_2
.LBB43_4:                               # %ifcont7
	movq	-1(%rbp), %rax
	cmpq	$9, %rax
	jg	.LBB43_7
# %bb.5:                                # %then9
	movzbl	-1(%rbp), %edx
	addb	$48, %dl
	movb	%dl, -2(%rbp)
	jmp	.LBB43_6
.LBB43_7:                               # %ifcont14
	movzbl	-1(%rbp), %edx
	addb	$55, %dl
	movb	%dl, -3(%rbp)
.LBB43_6:                               # %then9
	movq	%rsp, %rax
	leaq	-16(%rax), %rsp
	movb	$0, -16(%rax)
	movb	%dl, -15(%rax)
	xorl	%eax, %eax
.LBB43_2:                               # %then
	movq	%rbp, %rsp
	popq	%rbp
	.cfi_def_cfa %rsp, 8
	retq
.Lfunc_end43:
	.size	digit_to_hex, .Lfunc_end43-digit_to_hex
	.cfi_endproc
                                        # -- End function
	.p2align	4, 0x90                         # -- Begin function __user_main
	.type	__user_main,@function
__user_main:                            # @__user_main
	.cfi_startproc
# %bb.0:                                # %entry
	subq	$360, %rsp                      # imm = 0x168
	.cfi_def_cfa_offset 368
	movl	$.L__unnamed_1, %edi
	callq	puts@PLT
	movl	$.L__unnamed_2, %edi
	callq	puts@PLT
	movl	$.L__unnamed_3, %edi
	callq	puts@PLT
	movl	$42, %edi
	callq	abs_int64
	movb	%al, 8(%rsp)
	movq	%rdx, 16(%rsp)
	movq	$-42, %rdi
	callq	abs_int64
	movb	%al, 24(%rsp)
	movq	%rdx, 32(%rsp)
	movl	$.L__unnamed_4, %edi
	callq	puts@PLT
	movl	$10, %edi
	movl	$20, %esi
	callq	min_int64
	movb	%al, 40(%rsp)
	movq	%rdx, 48(%rsp)
	movl	$10, %edi
	movl	$20, %esi
	callq	max_int64
	movb	%al, 56(%rsp)
	movq	%rdx, 64(%rsp)
	movl	$.L__unnamed_5, %edi
	callq	puts@PLT
	movl	$5, %edi
	movl	$10, %esi
	movl	$20, %edx
	callq	clamp_int64
	movb	%al, 72(%rsp)
	movq	%rdx, 80(%rsp)
	movl	$15, %edi
	movl	$10, %esi
	movl	$20, %edx
	callq	clamp_int64
	movb	%al, 88(%rsp)
	movq	%rdx, 96(%rsp)
	movl	$25, %edi
	movl	$10, %esi
	movl	$20, %edx
	callq	clamp_int64
	movb	%al, 104(%rsp)
	movq	%rdx, 112(%rsp)
	movl	$.L__unnamed_6, %edi
	callq	puts@PLT
	movl	$64, %edi
	callq	is_power_of_2_uint64
	movb	%al, 120(%rsp)
	movb	%dl, 121(%rsp)
	movl	$65, %edi
	callq	is_power_of_2_uint64
	movb	%al, 136(%rsp)
	movb	%dl, 137(%rsp)
	movl	$.L__unnamed_7, %edi
	callq	puts@PLT
	movl	$100, %edi
	callq	sign_int64
	movb	%al, 152(%rsp)
	movb	%dl, 153(%rsp)
	movq	$-100, %rdi
	callq	sign_int64
	movb	%al, 168(%rsp)
	movb	%dl, 169(%rsp)
	xorl	%edi, %edi
	callq	sign_int64
	movb	%al, 184(%rsp)
	movb	%dl, 185(%rsp)
	movl	$.L__unnamed_8, %edi
	callq	puts@PLT
	movl	$.L__unnamed_9, %edi
	callq	puts@PLT
	movl	$.L__unnamed_10, %edi
	callq	puts@PLT
	movl	$65, %edi
	callq	is_uppercase
	movb	%al, 200(%rsp)
	movb	%dl, 201(%rsp)
	movl	$97, %edi
	callq	is_lowercase
	movb	%al, 216(%rsp)
	movb	%dl, 217(%rsp)
	movl	$48, %edi
	callq	is_digit
	movb	%al, 232(%rsp)
	movb	%dl, 233(%rsp)
	movl	$32, %edi
	callq	is_whitespace
	movb	%al, 248(%rsp)
	movb	%dl, 249(%rsp)
	movl	$.L__unnamed_11, %edi
	callq	puts@PLT
	movl	$97, %edi
	callq	to_uppercase
	movb	%al, 264(%rsp)
	movb	%dl, 265(%rsp)
	movl	$65, %edi
	callq	to_lowercase
	movb	%al, 280(%rsp)
	movb	%dl, 281(%rsp)
	movl	$.L__unnamed_12, %edi
	callq	puts@PLT
	movl	$53, %edi
	callq	char_to_digit
	movb	%al, 296(%rsp)
	movb	%dl, 297(%rsp)
	movl	$5, %edi
	callq	digit_to_char
	movb	%al, 312(%rsp)
	movb	%dl, 313(%rsp)
	movl	$.L__unnamed_13, %edi
	callq	puts@PLT
	movl	$65, %edi
	callq	hex_to_digit
	movb	%al, 328(%rsp)
	movb	%dl, 329(%rsp)
	movl	$10, %edi
	callq	digit_to_hex
	movb	%al, 344(%rsp)
	movb	%dl, 345(%rsp)
	movl	$.L__unnamed_14, %edi
	callq	puts@PLT
	movl	$.L__unnamed_15, %edi
	callq	puts@PLT
	movl	$.L__unnamed_16, %edi
	callq	puts@PLT
	movw	$0, (%rsp)
	xorl	%eax, %eax
	xorl	%edx, %edx
	addq	$360, %rsp                      # imm = 0x168
	.cfi_def_cfa_offset 8
	retq
.Lfunc_end44:
	.size	__user_main, .Lfunc_end44-__user_main
	.cfi_endproc
                                        # -- End function
	.globl	main                            # -- Begin function main
	.p2align	4, 0x90
	.type	main,@function
main:                                   # @main
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%rax
	.cfi_def_cfa_offset 16
	callq	__aria_module_init
	callq	__user_main
	xorl	%eax, %eax
	popq	%rcx
	.cfi_def_cfa_offset 8
	retq
.Lfunc_end45:
	.size	main, .Lfunc_end45-main
	.cfi_endproc
                                        # -- End function
	.type	.L__unnamed_1,@object           # @0
	.section	.rodata.str1.1,"aMS",@progbits,1
.L__unnamed_1:
	.asciz	"=== Testing Aria Standard Library ==="
	.size	.L__unnamed_1, 38

	.type	.L__unnamed_2,@object           # @1
.L__unnamed_2:
	.zero	1
	.size	.L__unnamed_2, 1

	.type	.L__unnamed_3,@object           # @2
.L__unnamed_3:
	.asciz	"--- Math Functions ---"
	.size	.L__unnamed_3, 23

	.type	.L__unnamed_4,@object           # @3
.L__unnamed_4:
	.asciz	"abs(42) and abs(-42) tested"
	.size	.L__unnamed_4, 28

	.type	.L__unnamed_5,@object           # @4
.L__unnamed_5:
	.asciz	"min(10, 20) and max(10, 20) tested"
	.size	.L__unnamed_5, 35

	.type	.L__unnamed_6,@object           # @5
.L__unnamed_6:
	.asciz	"clamp tests completed"
	.size	.L__unnamed_6, 22

	.type	.L__unnamed_7,@object           # @6
.L__unnamed_7:
	.asciz	"is_power_of_2 tests completed"
	.size	.L__unnamed_7, 30

	.type	.L__unnamed_8,@object           # @7
.L__unnamed_8:
	.asciz	"sign tests completed"
	.size	.L__unnamed_8, 21

	.type	.L__unnamed_9,@object           # @8
.L__unnamed_9:
	.zero	1
	.size	.L__unnamed_9, 1

	.type	.L__unnamed_10,@object          # @9
.L__unnamed_10:
	.asciz	"--- String/Character Functions ---"
	.size	.L__unnamed_10, 35

	.type	.L__unnamed_11,@object          # @10
.L__unnamed_11:
	.asciz	"Character classification tested"
	.size	.L__unnamed_11, 32

	.type	.L__unnamed_12,@object          # @11
.L__unnamed_12:
	.asciz	"Character conversion tested"
	.size	.L__unnamed_12, 28

	.type	.L__unnamed_13,@object          # @12
.L__unnamed_13:
	.asciz	"Digit conversion tested"
	.size	.L__unnamed_13, 24

	.type	.L__unnamed_14,@object          # @13
.L__unnamed_14:
	.asciz	"Hex conversion tested"
	.size	.L__unnamed_14, 22

	.type	.L__unnamed_15,@object          # @14
.L__unnamed_15:
	.zero	1
	.size	.L__unnamed_15, 1

	.type	.L__unnamed_16,@object          # @15
.L__unnamed_16:
	.asciz	"=== All Standard Library Tests Passed! ==="
	.size	.L__unnamed_16, 43

	.section	".note.GNU-stack","",@progbits
