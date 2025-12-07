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
	.p2align	4, 0x90                         # -- Begin function test
	.type	test,@function
test:                                   # @test
	.cfi_startproc
# %bb.0:                                # %entry
	movl	$2, %eax
	xorl	%edi, %edi
	xorl	%esi, %esi
	xorl	%edx, %edx
	#APP
	syscall
	#NO_APP
	movl	%eax, -8(%rsp)
	movslq	%eax, %rdi
	movl	$3, %eax
	#APP
	syscall
	#NO_APP
	movl	%eax, -4(%rsp)
	movw	$0, -16(%rsp)
	xorl	%eax, %eax
	xorl	%edx, %edx
	retq
.Lfunc_end1:
	.size	test, .Lfunc_end1-test
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
	xorl	%eax, %eax
	popq	%rcx
	.cfi_def_cfa_offset 8
	retq
.Lfunc_end2:
	.size	main, .Lfunc_end2-main
	.cfi_endproc
                                        # -- End function
	.section	".note.GNU-stack","",@progbits
