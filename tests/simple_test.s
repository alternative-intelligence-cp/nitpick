	.file	"simple_test.aria"
	.text
	.globl	__simple_test_init
	.p2align	4
	.type	__simple_test_init,@function
__simple_test_init:
	.cfi_startproc
	xorl	%eax, %eax
	retq
.Lfunc_end0:
	.size	__simple_test_init, .Lfunc_end0-__simple_test_init
	.cfi_endproc

	.section	".note.GNU-stack","",@progbits
