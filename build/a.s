	.text
	.file	"aria_module"
	.globl	main                            # -- Begin function main
	.p2align	4, 0x90
	.type	main,@function
main:                                   # @main
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%rax
	.cfi_def_cfa_offset 16
	movl	$.L__unnamed_1, %edi
	callq	puts@PLT
	movl	$10, %edi
	movl	$5, %esi
	callq	add@PLT
	movl	$10, %edi
	movl	$5, %esi
	callq	sub@PLT
	movl	$10, %edi
	movl	$5, %esi
	callq	mul@PLT
	movl	$.L__unnamed_2, %edi
	callq	puts@PLT
	popq	%rax
	.cfi_def_cfa_offset 8
	retq
.Lfunc_end0:
	.size	main, .Lfunc_end0-main
	.cfi_endproc
                                        # -- End function
	.globl	add                             # -- Begin function add
	.p2align	4, 0x90
	.type	add,@function
add:                                    # @add
	.cfi_startproc
# %bb.0:                                # %entry
                                        # kill: def $esi killed $esi def $rsi
                                        # kill: def $edi killed $edi def $rdi
	movb	%dil, -1(%rsp)
	movb	%sil, -2(%rsp)
	leal	(%rdi,%rsi), %eax
                                        # kill: def $al killed $al killed $eax
	retq
.Lfunc_end1:
	.size	add, .Lfunc_end1-add
	.cfi_endproc
                                        # -- End function
	.globl	sub                             # -- Begin function sub
	.p2align	4, 0x90
	.type	sub,@function
sub:                                    # @sub
	.cfi_startproc
# %bb.0:                                # %entry
	movl	%edi, %eax
	movb	%al, -1(%rsp)
	movb	%sil, -2(%rsp)
	subb	%sil, %al
                                        # kill: def $al killed $al killed $eax
	retq
.Lfunc_end2:
	.size	sub, .Lfunc_end2-sub
	.cfi_endproc
                                        # -- End function
	.globl	mul                             # -- Begin function mul
	.p2align	4, 0x90
	.type	mul,@function
mul:                                    # @mul
	.cfi_startproc
# %bb.0:                                # %entry
	movl	%edi, %eax
	movb	%al, -1(%rsp)
	movb	%sil, -2(%rsp)
                                        # kill: def $al killed $al killed $eax
	mulb	%sil
	retq
.Lfunc_end3:
	.size	mul, .Lfunc_end3-mul
	.cfi_endproc
                                        # -- End function
	.type	.L__unnamed_1,@object           # @0
	.section	.rodata.str1.1,"aMS",@progbits,1
.L__unnamed_1:
	.asciz	"Testing math operations..."
	.size	.L__unnamed_1, 27

	.type	.L__unnamed_2,@object           # @1
.L__unnamed_2:
	.asciz	"Math operations complete!"
	.size	.L__unnamed_2, 26

	.section	".note.GNU-stack","",@progbits
