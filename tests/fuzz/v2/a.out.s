	.file	"generated_1079643.aria"
	.text
	.globl	main
	.p2align	4
	.type	main,@function
main:
	.cfi_startproc
	pushq	%rax
	.cfi_def_cfa_offset 16
	xorl	%edi, %edi
	xorl	%esi, %esi
	callq	aria_gc_init@PLT
	xorl	%eax, %eax
	popq	%rcx
	.cfi_def_cfa_offset 8
	retq
.Lfunc_end0:
	.size	main, .Lfunc_end0-main
	.cfi_endproc

	.globl	__generated_1079643_init
	.p2align	4
	.type	__generated_1079643_init,@function
__generated_1079643_init:
	xorl	%eax, %eax
	retq
.Lfunc_end1:
	.size	__generated_1079643_init, .Lfunc_end1-__generated_1079643_init

	.section	".note.GNU-stack","",@progbits
