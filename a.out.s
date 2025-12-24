	.file	"simple_shadow.aria"
	.text
	.p2align	4
	.type	test,@function
test:
	.cfi_startproc
	movl	$1, -4(%rsp)
	movl	$2, -8(%rsp)
	movl	$0, -12(%rsp)
	xorl	%eax, %eax
	retq
.Lfunc_end0:
	.size	test, .Lfunc_end0-test
	.cfi_endproc

	.globl	__simple_shadow_init
	.p2align	4
	.type	__simple_shadow_init,@function
__simple_shadow_init:
	.cfi_startproc
	xorl	%eax, %eax
	retq
.Lfunc_end1:
	.size	__simple_shadow_init, .Lfunc_end1-__simple_shadow_init
	.cfi_endproc

	.section	".note.GNU-stack","",@progbits
