	.file	"alife.aria"
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
	pushq	%r15
	pushq	%r14
	pushq	%r13
	pushq	%r12
	pushq	%rbx
	subq	$8200, %rsp
	.cfi_offset %rbx, -56
	.cfi_offset %r12, -48
	.cfi_offset %r13, -40
	.cfi_offset %r14, -32
	.cfi_offset %r15, -24
	movabsq	$9223372036854775807, %r14
	xorl	%ebx, %ebx
	xorl	%edi, %edi
	xorl	%esi, %esi
	callq	aria_gc_init@PLT
	leaq	-4136(%rbp), %rdi
	movl	$4096, %edx
	xorl	%esi, %esi
	callq	memset@PLT
	leaq	-8232(%rbp), %rdi
	movl	$4096, %edx
	xorl	%esi, %esi
	callq	memset@PLT
	movq	$1, -3864(%rbp)
	movq	$1, -3600(%rbp)
	movq	$1, -3360(%rbp)
	movq	$1, -3352(%rbp)
	movq	$1, -3344(%rbp)
	movq	$1, -3680(%rbp)
	movq	$1, -3672(%rbp)
	movq	$1, -3664(%rbp)
	movq	$1, -2496(%rbp)
	movq	$1, -2488(%rbp)
	movq	$1, -2248(%rbp)
	movq	$1, -2240(%rbp)
	movq	$1, -1984(%rbp)
	movq	$1, -1416(%rbp)
	movq	$1, -1408(%rbp)
	movq	$1, -1160(%rbp)
	movq	$1, -1152(%rbp)
	movq	$1, -888(%rbp)
	movq	$1, -880(%rbp)
	movq	$1, -632(%rbp)
	movq	$1, -624(%rbp)
	callq	alife_init@PLT
	jmp	.LBB0_1
	.p2align	4
.LBB0_33:
	movl	$4096, %edx
	leaq	-4136(%rbp), %rdi
	leaq	-8232(%rbp), %rsi
	callq	memcpy@PLT
	incq	%rbx
	cmovoq	%r14, %rbx
	cmpq	$400, %rbx
	jge	.LBB0_34
.LBB0_1:
	xorl	%r15d, %r15d
	.p2align	4
.LBB0_2:
	movq	%r15, %r12
	shlq	$5, %r12
	xorl	%r13d, %r13d
	.p2align	4
.LBB0_3:
	movq	%r12, %rax
	addq	%r13, %rax
	cmovoq	%r14, %rax
	movq	-4136(%rbp,%rax,8), %rdx
	movq	%r15, %rdi
	movq	%r13, %rsi
	callq	alife_put@PLT
	incq	%r13
	cmpq	$32, %r13
	jne	.LBB0_3
	incq	%r15
	cmpq	$16, %r15
	jne	.LBB0_2
	movq	%rbx, %rdi
	callq	alife_show@PLT
	movl	$80, %edi
	callq	alife_sleep@PLT
	xorl	%eax, %eax
	leaq	-3872(%rbp), %rcx
	xorl	%edx, %edx
	jmp	.LBB0_6
	.p2align	4
.LBB0_32:
	incq	%rdx
	addq	$256, %rcx
	addq	$32, %rax
	cmpq	$16, %rdx
	je	.LBB0_33
.LBB0_6:
	movq	%rdx, %rsi
	shlq	$5, %rsi
	leaq	-32(%rsi), %rdi
	leaq	32(%rsi), %r8
	xorl	%r9d, %r9d
	jmp	.LBB0_7
	.p2align	4
.LBB0_31:
	movq	(%r12), %r10
	movq	%r10, -8232(%rbp,%r11,8)
	incq	%r9
	cmpq	$32, %r9
	je	.LBB0_32
.LBB0_7:
	movq	%rsp, %r11
	leaq	-16(%r11), %r10
	movq	%r10, %rsp
	movq	$0, -16(%r11)
	testq	%rdx, %rdx
	je	.LBB0_14
	testq	%r9, %r9
	je	.LBB0_10
	movq	%rdi, %r11
	addq	%r9, %r11
	cmovoq	%r14, %r11
	decq	%r11
	cmovoq	%r14, %r11
	movq	-4136(%rbp,%r11,8), %r11
	movq	%r11, (%r10)
.LBB0_10:
	movq	(%r10), %r11
	addq	-520(%rcx,%r9,8), %r11
	cmovoq	%r14, %r11
	movq	%r11, (%r10)
	cmpq	$30, %r9
	ja	.LBB0_14
	leaq	(%rax,%r9), %r15
	addq	$-32, %r15
	incq	%r15
	cmovoq	%r14, %r15
	addq	-4136(%rbp,%r15,8), %r11
	movq	%r14, %r15
	jo	.LBB0_13
	movq	%r11, %r15
.LBB0_13:
	movq	%r15, (%r10)
.LBB0_14:
	testq	%r9, %r9
	je	.LBB0_16
	movq	%rsi, %r11
	addq	%r9, %r11
	cmovoq	%r14, %r11
	decq	%r11
	cmovoq	%r14, %r11
	movq	(%r10), %r15
	addq	-4136(%rbp,%r11,8), %r15
	cmovoq	%r14, %r15
	movq	%r15, (%r10)
.LBB0_16:
	cmpq	$30, %r9
	ja	.LBB0_18
	leaq	(%rax,%r9), %r11
	incq	%r11
	cmovoq	%r14, %r11
	movq	(%r10), %r15
	addq	-4136(%rbp,%r11,8), %r15
	cmovoq	%r14, %r15
	movq	%r15, (%r10)
.LBB0_18:
	cmpq	$14, %rdx
	ja	.LBB0_25
	testq	%r9, %r9
	je	.LBB0_21
	movq	%r8, %r11
	addq	%r9, %r11
	cmovoq	%r14, %r11
	decq	%r11
	cmovoq	%r14, %r11
	movq	(%r10), %r15
	addq	-4136(%rbp,%r11,8), %r15
	cmovoq	%r14, %r15
	movq	%r15, (%r10)
.LBB0_21:
	movq	(%r10), %r11
	addq	-8(%rcx,%r9,8), %r11
	cmovoq	%r14, %r11
	movq	%r11, (%r10)
	cmpq	$30, %r9
	ja	.LBB0_25
	addq	(%rcx,%r9,8), %r11
	movq	%r14, %r15
	jo	.LBB0_24
	movq	%r11, %r15
.LBB0_24:
	movq	%r15, (%r10)
.LBB0_25:
	movq	%rsi, %r11
	addq	%r9, %r11
	cmovoq	%r14, %r11
	movq	-4136(%rbp,%r11,8), %r15
	movq	%rsp, %r13
	leaq	-16(%r13), %r12
	movq	%r12, %rsp
	movq	$0, -16(%r13)
	cmpq	$1, %r15
	jne	.LBB0_28
	movq	(%r10), %r13
	andq	$-2, %r13
	cmpq	$2, %r13
	jne	.LBB0_28
	movq	$1, (%r12)
.LBB0_28:
	testq	%r15, %r15
	jne	.LBB0_31
	cmpq	$3, (%r10)
	jne	.LBB0_31
	movq	$1, (%r12)
	jmp	.LBB0_31
.LBB0_34:
	callq	alife_done@PLT
	xorl	%eax, %eax
	leaq	-40(%rbp), %rsp
	popq	%rbx
	popq	%r12
	popq	%r13
	popq	%r14
	popq	%r15
	popq	%rbp
	.cfi_def_cfa %rsp, 8
	retq
.Lfunc_end0:
	.size	main, .Lfunc_end0-main
	.cfi_endproc

	.globl	failsafe
	.p2align	4
	.type	failsafe,@function
failsafe:
	.cfi_startproc
	pushq	%rax
	.cfi_def_cfa_offset 16
	callq	alife_done@PLT
	xorl	%eax, %eax
	xorl	%edx, %edx
	popq	%rcx
	.cfi_def_cfa_offset 8
	retq
.Lfunc_end1:
	.size	failsafe, .Lfunc_end1-failsafe
	.cfi_endproc

	.globl	__alife_init
	.p2align	4
	.type	__alife_init,@function
__alife_init:
	xorl	%eax, %eax
	retq
.Lfunc_end2:
	.size	__alife_init, .Lfunc_end2-__alife_init

	.section	".note.GNU-stack","",@progbits
