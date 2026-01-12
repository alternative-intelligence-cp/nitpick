	.file	"5d7699276605421a.aria"
	.text
	.globl	main
	.p2align	4
	.type	main,@function
main:
	.cfi_startproc
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	pushq	%rbx
	pushq	%rax
	.cfi_offset %rbx, -24
	xorl	%ebx, %ebx
	xorl	%edi, %edi
	xorl	%esi, %esi
	callq	aria_gc_init@PLT
	xorl	%esi, %esi
	.p2align	4
.LBB0_1:
	addq	%rsi, %rbx
	adcq	$0, %rax
	adcq	$0, %rcx
	adcq	$0, %rdx
	incq	%rsi
	cmpq	$10, %rsi
	jne	.LBB0_1
	movq	%rsp, %rsi
	leaq	-32(%rsi), %rdi
	movq	%rdi, %rsp
	movq	$45, -32(%rsi)
	movq	%rax, -24(%rsi)
	movq	%rcx, -16(%rsi)
	movq	%rdx, -8(%rsi)
	callq	aria_format_int256@PLT
	movq	(%rax), %rsi
	leaq	.Lstr_fmt(%rip), %rdi
	xorl	%eax, %eax
	callq	printf@PLT
	xorl	%eax, %eax
	leaq	-8(%rbp), %rsp
	popq	%rbx
	popq	%rbp
	.cfi_def_cfa %rsp, 8
	retq
.Lfunc_end0:
	.size	main, .Lfunc_end0-main
	.cfi_endproc

	.globl	__5d7699276605421a_init
	.p2align	4
	.type	__5d7699276605421a_init,@function
__5d7699276605421a_init:
	xorl	%eax, %eax
	retq
.Lfunc_end1:
	.size	__5d7699276605421a_init, .Lfunc_end1-__5d7699276605421a_init

	.type	.Lstr_fmt,@object
	.section	.rodata.str1.1,"aMS",@progbits,1
.Lstr_fmt:
	.asciz	"%s"
	.size	.Lstr_fmt, 3

	.section	".note.GNU-stack","",@progbits
