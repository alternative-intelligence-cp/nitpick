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
	.p2align	4, 0x90                         # -- Begin function __user_main
	.type	__user_main,@function
__user_main:                            # @__user_main
	.cfi_startproc
# %bb.0:                                # %entry
	subq	$40, %rsp
	.cfi_def_cfa_offset 48
	movb	$100, 11(%rsp)
	movb	$-100, 12(%rsp)
	movb	$0, 13(%rsp)
	movb	$127, 14(%rsp)
	movb	$-127, 15(%rsp)
	movw	$30000, 24(%rsp)                # imm = 0x7530
	movw	$-30000, 26(%rsp)               # imm = 0x8AD0
	movl	$1000000, 28(%rsp)              # imm = 0xF4240
	movq	$1000000000, 32(%rsp)           # imm = 0x3B9ACA00
	movl	$.L__unnamed_1, %edi
	callq	puts@PLT
	movw	$0, 16(%rsp)
	xorl	%eax, %eax
	xorl	%edx, %edx
	addq	$40, %rsp
	.cfi_def_cfa_offset 8
	retq
.Lfunc_end1:
	.size	__user_main, .Lfunc_end1-__user_main
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
.Lfunc_end2:
	.size	main, .Lfunc_end2-main
	.cfi_endproc
                                        # -- End function
	.type	.L__unnamed_1,@object           # @0
	.section	.rodata.str1.1,"aMS",@progbits,1
.L__unnamed_1:
	.asciz	"TBB types compiled successfully!"
	.size	.L__unnamed_1, 33

	.section	".note.GNU-stack","",@progbits
