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
	subq	$56, %rsp
	.cfi_def_cfa_offset 64
	movl	$.L__unnamed_1, %edi
	callq	puts@PLT
	movl	$1819043144, 24(%rsp)           # imm = 0x6C6C6548
	movw	$2671, 28(%rsp)                 # imm = 0xA6F
	leaq	24(%rsp), %rsi
	movl	$1, %eax
	movl	$1, %edi
	movl	$6, %edx
	#APP
	syscall
	#NO_APP
	movq	%rax, 16(%rsp)
	movl	$.L__unnamed_2, %edi
	callq	puts@PLT
	movw	$0, 8(%rsp)
	xorl	%eax, %eax
	xorl	%edx, %edx
	addq	$56, %rsp
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
	.asciz	"Syscall test starting"
	.size	.L__unnamed_1, 22

	.type	.L__unnamed_2,@object           # @1
.L__unnamed_2:
	.asciz	"Done!"
	.size	.L__unnamed_2, 6

	.section	".note.GNU-stack","",@progbits
