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
	movq	$1, 2(%rsp)
	movq	$3, 3(%rsp)
	movq	$15, 4(%rsp)
	movq	$1, 5(%rsp)
	movq	$3, 6(%rsp)
	movq	$15, 7(%rsp)
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
	.asciz	"=== Testing uint1/2/4 Aliases ==="
	.size	.L__unnamed_1, 34

	.type	.L__unnamed_2,@object           # @1
.L__unnamed_2:
	.asciz	"\342\234\223 uint1, uint2, uint4 aliases work correctly"
	.size	.L__unnamed_2, 47

	.type	.L__unnamed_3,@object           # @2
.L__unnamed_3:
	.asciz	"\342\234\223 No confusion for users coming from other languages"
	.size	.L__unnamed_3, 55

	.section	".note.GNU-stack","",@progbits
