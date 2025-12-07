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
	.p2align	4, 0x90                         # -- Begin function get_length
	.type	get_length,@function
get_length:                             # @get_length
	.cfi_startproc
# %bb.0:                                # %entry
	movq	%rsi, %rdx
	movq	%rdi, -8(%rsp)
	movq	%rsi, -16(%rsp)
	movb	$0, -32(%rsp)
	movq	%rsi, -24(%rsp)
	xorl	%eax, %eax
	retq
.Lfunc_end1:
	.size	get_length, .Lfunc_end1-get_length
	.cfi_endproc
                                        # -- End function
	.p2align	4, 0x90                         # -- Begin function __user_main
	.type	__user_main,@function
__user_main:                            # @__user_main
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%rbp
	.cfi_def_cfa_offset 16
	.cfi_offset %rbp, -16
	movq	%rsp, %rbp
	.cfi_def_cfa_register %rbp
	subq	$32, %rsp
	movl	$.L__unnamed_1, %edi
	callq	puts@PLT
	movl	$673059850, -5(%rbp)            # imm = 0x281E140A
	movb	$50, -1(%rbp)
	leaq	-5(%rbp), %rdi
	movl	$5, %esi
	callq	get_length
	movb	%al, -24(%rbp)
	movq	%rdx, -16(%rbp)
	xorl	%ecx, %ecx
	testb	%al, %al
	cmoveq	%rdx, %rcx
	movq	%rcx, -32(%rbp)
	cmpq	$5, %rcx
	jne	.LBB2_3
# %bb.1:                                # %then
	movl	$.L__unnamed_2, %edi
	callq	puts@PLT
	movq	%rsp, %rax
	leaq	-16(%rax), %rsp
	movw	$0, -16(%rax)
	xorl	%eax, %eax
	xorl	%edx, %edx
	jmp	.LBB2_2
.LBB2_3:                                # %ifcont
	movl	$.L__unnamed_3, %edi
	callq	puts@PLT
	movq	%rsp, %rax
	leaq	-16(%rax), %rsp
	movw	$256, -16(%rax)                 # imm = 0x100
	xorl	%eax, %eax
	movb	$1, %dl
.LBB2_2:                                # %then
	movq	%rbp, %rsp
	popq	%rbp
	.cfi_def_cfa %rsp, 8
	retq
.Lfunc_end2:
	.size	__user_main, .Lfunc_end2-__user_main
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
.Lfunc_end3:
	.size	main, .Lfunc_end3-main
	.cfi_endproc
                                        # -- End function
	.type	.L__unnamed_1,@object           # @0
	.section	.rodata.str1.1,"aMS",@progbits,1
.L__unnamed_1:
	.asciz	"Testing array parameter passing..."
	.size	.L__unnamed_1, 35

	.type	.L__unnamed_2,@object           # @1
.L__unnamed_2:
	.asciz	"SUCCESS: parameter passing works"
	.size	.L__unnamed_2, 33

	.type	.L__unnamed_3,@object           # @2
.L__unnamed_3:
	.asciz	"FAIL"
	.size	.L__unnamed_3, 5

	.section	".note.GNU-stack","",@progbits
