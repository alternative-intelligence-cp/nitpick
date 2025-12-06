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
	pushq	%rax
	.cfi_def_cfa_offset 16
	leaq	.L__unnamed_1(%rip), %rdi
	callq	puts@PLT
	movl	$1, %edi
	callq	aria_alloc_exec@PLT
	movq	%rax, (%rsp)
	movq	$0, (%rax)
	movq	(%rsp), %rax
	movb	$72, (%rax)
	movq	(%rsp), %rax
	movb	$-57, 1(%rax)
	movq	(%rsp), %rax
	movb	$-64, 2(%rax)
	movq	(%rsp), %rax
	movb	$42, 3(%rax)
	movq	(%rsp), %rax
	movb	$0, 4(%rax)
	movq	(%rsp), %rax
	movb	$0, 5(%rax)
	movq	(%rsp), %rax
	movb	$0, 6(%rax)
	movq	(%rsp), %rax
	movb	$-61, 7(%rax)
	leaq	.L__unnamed_2(%rip), %rdi
	callq	puts@PLT
	leaq	.L__unnamed_3(%rip), %rdi
	callq	puts@PLT
	xorl	%eax, %eax
	xorl	%edx, %edx
	popq	%rcx
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
	.asciz	"=== Wildx Array Indexing Test (Decimal) ==="
	.size	.L__unnamed_1, 44

	.type	.L__unnamed_2,@object           # @1
.L__unnamed_2:
	.asciz	"\342\234\223 Wrote machine code to wildx buffer"
	.size	.L__unnamed_2, 39

	.type	.L__unnamed_3,@object           # @2
.L__unnamed_3:
	.asciz	"  Using decimal values for verification"
	.size	.L__unnamed_3, 40

	.section	".note.GNU-stack","",@progbits
