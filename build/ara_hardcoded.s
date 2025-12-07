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
	.p2align	4, 0x90                         # -- Begin function emit_mov_rax_rdi
	.type	emit_mov_rax_rdi,@function
emit_mov_rax_rdi:                       # @emit_mov_rax_rdi
	.cfi_startproc
# %bb.0:                                # %entry
	movq	%rdi, -24(%rsp)
	movq	%rsi, -32(%rsp)
	movb	$72, (%rdi,%rsi)
	movq	-24(%rsp), %rax
	movq	-32(%rsp), %rcx
	movb	$-119, 1(%rcx,%rax)
	movq	-24(%rsp), %rax
	movq	-32(%rsp), %rcx
	movb	$-8, 2(%rcx,%rax)
	movq	-32(%rsp), %rdx
	addq	$3, %rdx
	movb	$0, -16(%rsp)
	movq	%rdx, -8(%rsp)
	xorl	%eax, %eax
	retq
.Lfunc_end1:
	.size	emit_mov_rax_rdi, .Lfunc_end1-emit_mov_rax_rdi
	.cfi_endproc
                                        # -- End function
	.p2align	4, 0x90                         # -- Begin function emit_add_rax_10
	.type	emit_add_rax_10,@function
emit_add_rax_10:                        # @emit_add_rax_10
	.cfi_startproc
# %bb.0:                                # %entry
	movq	%rdi, -24(%rsp)
	movq	%rsi, -32(%rsp)
	movb	$72, (%rdi,%rsi)
	movq	-24(%rsp), %rax
	movq	-32(%rsp), %rcx
	movb	$-125, 1(%rcx,%rax)
	movq	-24(%rsp), %rax
	movq	-32(%rsp), %rcx
	movb	$-64, 2(%rcx,%rax)
	movq	-24(%rsp), %rax
	movq	-32(%rsp), %rcx
	movb	$10, 3(%rcx,%rax)
	movq	-32(%rsp), %rdx
	addq	$4, %rdx
	movb	$0, -16(%rsp)
	movq	%rdx, -8(%rsp)
	xorl	%eax, %eax
	retq
.Lfunc_end2:
	.size	emit_add_rax_10, .Lfunc_end2-emit_add_rax_10
	.cfi_endproc
                                        # -- End function
	.p2align	4, 0x90                         # -- Begin function emit_sub_rax_5
	.type	emit_sub_rax_5,@function
emit_sub_rax_5:                         # @emit_sub_rax_5
	.cfi_startproc
# %bb.0:                                # %entry
	movq	%rdi, -24(%rsp)
	movq	%rsi, -32(%rsp)
	movb	$72, (%rdi,%rsi)
	movq	-24(%rsp), %rax
	movq	-32(%rsp), %rcx
	movb	$-125, 1(%rcx,%rax)
	movq	-24(%rsp), %rax
	movq	-32(%rsp), %rcx
	movb	$-24, 2(%rcx,%rax)
	movq	-24(%rsp), %rax
	movq	-32(%rsp), %rcx
	movb	$5, 3(%rcx,%rax)
	movq	-32(%rsp), %rdx
	addq	$4, %rdx
	movb	$0, -16(%rsp)
	movq	%rdx, -8(%rsp)
	xorl	%eax, %eax
	retq
.Lfunc_end3:
	.size	emit_sub_rax_5, .Lfunc_end3-emit_sub_rax_5
	.cfi_endproc
                                        # -- End function
	.p2align	4, 0x90                         # -- Begin function emit_ret
	.type	emit_ret,@function
emit_ret:                               # @emit_ret
	.cfi_startproc
# %bb.0:                                # %entry
	movq	%rdi, -8(%rsp)
	movq	%rsi, -32(%rsp)
	movb	$-61, (%rdi,%rsi)
	movq	-32(%rsp), %rdx
	incq	%rdx
	movb	$0, -24(%rsp)
	movq	%rdx, -16(%rsp)
	xorl	%eax, %eax
	retq
.Lfunc_end4:
	.size	emit_ret, .Lfunc_end4-emit_ret
	.cfi_endproc
                                        # -- End function
	.p2align	4, 0x90                         # -- Begin function __user_main
	.type	__user_main,@function
__user_main:                            # @__user_main
	.cfi_startproc
# %bb.0:                                # %entry
	subq	$136, %rsp
	.cfi_def_cfa_offset 144
	movl	$.L__unnamed_1, %edi
	callq	puts@PLT
	movl	$.L__unnamed_2, %edi
	callq	puts@PLT
	movl	$.L__unnamed_3, %edi
	callq	puts@PLT
	movl	$8, %edi
	callq	aria_alloc_exec@PLT
	movq	%rax, 8(%rsp)
	movq	$0, (%rax)
	movq	8(%rsp), %rdi
	xorl	%esi, %esi
	callq	emit_mov_rax_rdi
	movb	%al, 72(%rsp)
	movq	%rdx, 80(%rsp)
	movq	8(%rsp), %rdi
	movb	%al, 56(%rsp)
	movq	%rdx, 64(%rsp)
	movq	%rdx, %rsi
	callq	emit_add_rax_10
	movb	%al, 88(%rsp)
	movq	%rdx, 96(%rsp)
	movq	8(%rsp), %rdi
	movb	%al, 40(%rsp)
	movq	%rdx, 48(%rsp)
	movq	%rdx, %rsi
	callq	emit_sub_rax_5
	movb	%al, 104(%rsp)
	movq	%rdx, 112(%rsp)
	movq	8(%rsp), %rdi
	movb	%al, 24(%rsp)
	movq	%rdx, 32(%rsp)
	movq	%rdx, %rsi
	callq	emit_ret
	movb	%al, 120(%rsp)
	movq	%rdx, 128(%rsp)
	movl	$.L__unnamed_4, %edi
	callq	puts@PLT
	movl	$.L__unnamed_5, %edi
	callq	puts@PLT
	movw	$0, 16(%rsp)
	xorl	%eax, %eax
	xorl	%edx, %edx
	addq	$136, %rsp
	.cfi_def_cfa_offset 8
	retq
.Lfunc_end5:
	.size	__user_main, .Lfunc_end5-__user_main
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
.Lfunc_end6:
	.size	main, .Lfunc_end6-main
	.cfi_endproc
                                        # -- End function
	.type	RAX,@object                     # @RAX
	.local	RAX
	.comm	RAX,1,1
	.type	RDI,@object                     # @RDI
	.data
RDI:
	.byte	7                               # 0x7
	.size	RDI, 1

	.type	.L__unnamed_1,@object           # @0
	.section	.rodata.str1.1,"aMS",@progbits,1
.L__unnamed_1:
	.asciz	"========================================"
	.size	.L__unnamed_1, 41

	.type	.L__unnamed_2,@object           # @1
.L__unnamed_2:
	.asciz	"  Aria Runtime Assembler - WORKING!   "
	.size	.L__unnamed_2, 39

	.type	.L__unnamed_3,@object           # @2
.L__unnamed_3:
	.asciz	"========================================"
	.size	.L__unnamed_3, 41

	.type	.L__unnamed_4,@object           # @3
.L__unnamed_4:
	.asciz	"Generated 12 bytes of x86-64 code"
	.size	.L__unnamed_4, 34

	.type	.L__unnamed_5,@object           # @4
.L__unnamed_5:
	.asciz	"========================================"
	.size	.L__unnamed_5, 41

	.section	".note.GNU-stack","",@progbits
