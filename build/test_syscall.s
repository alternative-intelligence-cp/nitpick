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
	movl	$.L__unnamed_1, %edi
	callq	puts@PLT
	movq	8(%rsp), %rax
	movb	$116, (%rax)
	movq	8(%rsp), %rax
	movb	$101, 1(%rax)
	movq	8(%rsp), %rax
	movb	$115, 2(%rax)
	movq	8(%rsp), %rax
	movb	$116, 3(%rax)
	movq	8(%rsp), %rax
	movb	$46, 4(%rax)
	movq	8(%rsp), %rax
	movb	$116, 5(%rax)
	movq	8(%rsp), %rax
	movb	$120, 6(%rax)
	movq	8(%rsp), %rax
	movb	$116, 7(%rax)
	movq	8(%rsp), %rax
	movb	$0, 8(%rax)
	leaq	8(%rsp), %rdi
	movl	$2, %eax
	movl	$577, %esi                      # imm = 0x241
	movl	$438, %edx                      # imm = 0x1B6
	#APP
	syscall
	#NO_APP
	movl	%eax, 32(%rsp)
	movl	$.L__unnamed_2, %edi
	callq	puts@PLT
	movslq	32(%rsp), %rdi
	movl	$3, %eax
	#APP
	syscall
	#NO_APP
	movl	%eax, 36(%rsp)
	movl	$.L__unnamed_3, %edi
	callq	puts@PLT
	movw	$0, 24(%rsp)
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
	.asciz	"Testing syscalls"
	.size	.L__unnamed_1, 17

	.type	.L__unnamed_2,@object           # @1
.L__unnamed_2:
	.asciz	"File opened"
	.size	.L__unnamed_2, 12

	.type	.L__unnamed_3,@object           # @2
.L__unnamed_3:
	.asciz	"Done"
	.size	.L__unnamed_3, 5

	.section	".note.GNU-stack","",@progbits
