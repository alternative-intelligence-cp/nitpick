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
	movabsq	$8022916924116329800, %rax      # imm = 0x6F57206F6C6C6548
	movq	%rax, -21(%rsp)
	movl	$560229490, -13(%rsp)           # imm = 0x21646C72
	movb	$10, -9(%rsp)
	leaq	-21(%rsp), %rsi
	movl	$1, %eax
	movl	$1, %edi
	movl	$13, %edx
	#APP
	syscall
	#NO_APP
	movq	%rax, -8(%rsp)
	movw	$0, -24(%rsp)
	xorl	%eax, %eax
	xorl	%edx, %edx
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
	.section	".note.GNU-stack","",@progbits
