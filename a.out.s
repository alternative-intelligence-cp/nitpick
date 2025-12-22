	.file	"test_var_init.aria"
	.text
	.p2align	4
	.type	get_value,@function
get_value:
	.cfi_startproc
	movq	$100, -8(%rsp)
	movl	$100, %eax
	retq
.Lfunc_end0:
	.size	get_value, .Lfunc_end0-get_value
	.cfi_endproc

	.globl	main
	.p2align	4
	.type	main,@function
main:
	.cfi_startproc
	subq	$40, %rsp
	.cfi_def_cfa_offset 48
	movq	$42, 24(%rsp)
	leaq	.Lstr_fmt(%rip), %rdi
	leaq	.Lstr(%rip), %rsi
	xorl	%eax, %eax
	callq	printf@PLT
	movq	24(%rsp), %rsi
	leaq	.Lint64_fmt(%rip), %rdi
	xorl	%eax, %eax
	callq	printf@PLT
	callq	get_value
	movq	%rax, 16(%rsp)
	leaq	.Lstr_fmt.2(%rip), %rdi
	leaq	.Lstr.1(%rip), %rsi
	xorl	%eax, %eax
	callq	printf@PLT
	movq	16(%rsp), %rsi
	leaq	.Lint64_fmt.3(%rip), %rdi
	xorl	%eax, %eax
	callq	printf@PLT
	movl	$4096, %edi
	callq	aria_arena_new_handle@PLT
	movq	%rax, 8(%rsp)
	leaq	.Lstr_fmt.5(%rip), %rdi
	leaq	.Lstr.4(%rip), %rsi
	xorl	%eax, %eax
	callq	printf@PLT
	movq	8(%rsp), %rsi
	leaq	.Lint64_fmt.6(%rip), %rdi
	xorl	%eax, %eax
	callq	printf@PLT
	movl	$0, 36(%rsp)
	xorl	%eax, %eax
	addq	$40, %rsp
	.cfi_def_cfa_offset 8
	retq
.Lfunc_end1:
	.size	main, .Lfunc_end1-main
	.cfi_endproc

	.globl	__test_var_init_init
	.p2align	4
	.type	__test_var_init_init,@function
__test_var_init_init:
	.cfi_startproc
	xorl	%eax, %eax
	retq
.Lfunc_end2:
	.size	__test_var_init_init, .Lfunc_end2-__test_var_init_init
	.cfi_endproc

	.type	.Lstr,@object
	.section	.rodata.str1.1,"aMS",@progbits,1
.Lstr:
	.asciz	"a ="
	.size	.Lstr, 4

	.type	.Lstr_fmt,@object
.Lstr_fmt:
	.asciz	"%s\n"
	.size	.Lstr_fmt, 4

	.type	.Lint64_fmt,@object
.Lint64_fmt:
	.asciz	"%lld\n"
	.size	.Lint64_fmt, 6

	.type	.Lstr.1,@object
.Lstr.1:
	.asciz	"b ="
	.size	.Lstr.1, 4

	.type	.Lstr_fmt.2,@object
.Lstr_fmt.2:
	.asciz	"%s\n"
	.size	.Lstr_fmt.2, 4

	.type	.Lint64_fmt.3,@object
.Lint64_fmt.3:
	.asciz	"%lld\n"
	.size	.Lint64_fmt.3, 6

	.type	.Lstr.4,@object
.Lstr.4:
	.asciz	"c ="
	.size	.Lstr.4, 4

	.type	.Lstr_fmt.5,@object
.Lstr_fmt.5:
	.asciz	"%s\n"
	.size	.Lstr_fmt.5, 4

	.type	.Lint64_fmt.6,@object
.Lint64_fmt.6:
	.asciz	"%lld\n"
	.size	.Lint64_fmt.6, 6

	.section	".note.GNU-stack","",@progbits
