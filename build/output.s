	.text
	.file	"aria_module"
	.globl	main                            # -- Begin function main
	.p2align	4, 0x90
	.type	main,@function
main:                                   # @main
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%r15
	.cfi_def_cfa_offset 16
	pushq	%r14
	.cfi_def_cfa_offset 24
	pushq	%r12
	.cfi_def_cfa_offset 32
	pushq	%rbx
	.cfi_def_cfa_offset 40
	pushq	%rax
	.cfi_def_cfa_offset 48
	.cfi_offset %rbx, -40
	.cfi_offset %r12, -32
	.cfi_offset %r14, -24
	.cfi_offset %r15, -16
	leaq	.L__unnamed_1(%rip), %rdi
	callq	puts@PLT
	leaq	.L__unnamed_2(%rip), %rdi
	callq	puts@PLT
	leaq	.L__unnamed_3(%rip), %rdi
	callq	puts@PLT
	movl	$3, %edi
	callq	greet@PLT
	leaq	.L__unnamed_4(%rip), %rdi
	callq	puts@PLT
	leaq	.L__unnamed_5(%rip), %rdi
	callq	puts@PLT
	xorl	%r15d, %r15d
	leaq	.L__unnamed_6(%rip), %rbx
	leaq	.L__unnamed_7(%rip), %r14
	jmp	.LBB0_1
	.p2align	4, 0x90
.LBB0_5:                                # %for_exit
                                        #   in Loop: Header=BB0_1 Depth=1
	incq	%r15
.LBB0_1:                                # %for_cond
                                        # =>This Loop Header: Depth=1
                                        #     Child Loop BB0_4 Depth 2
	cmpq	$1, %r15
	jg	.LBB0_6
# %bb.2:                                # %for_body
                                        #   in Loop: Header=BB0_1 Depth=1
	movq	%rbx, %rdi
	callq	puts@PLT
	xorl	%r12d, %r12d
	cmpq	$2, %r12
	jg	.LBB0_5
	.p2align	4, 0x90
.LBB0_4:                                # %for_body3
                                        #   Parent Loop BB0_1 Depth=1
                                        # =>  This Inner Loop Header: Depth=2
	movq	%r14, %rdi
	callq	puts@PLT
	incq	%r12
	cmpq	$2, %r12
	jle	.LBB0_4
	jmp	.LBB0_5
.LBB0_6:                                # %for_exit6
	leaq	.L__unnamed_8(%rip), %rdi
	callq	puts@PLT
	leaq	.L__unnamed_9(%rip), %rdi
	callq	puts@PLT
	movl	$2, %edi
	callq	greet@PLT
	leaq	.L__unnamed_10(%rip), %rdi
	callq	puts@PLT
	leaq	.L__unnamed_11(%rip), %rdi
	callq	puts@PLT
	addq	$8, %rsp
	.cfi_def_cfa_offset 40
	popq	%rbx
	.cfi_def_cfa_offset 32
	popq	%r12
	.cfi_def_cfa_offset 24
	popq	%r14
	.cfi_def_cfa_offset 16
	popq	%r15
	.cfi_def_cfa_offset 8
	retq
.Lfunc_end0:
	.size	main, .Lfunc_end0-main
	.cfi_endproc
                                        # -- End function
	.globl	greet                           # -- Begin function greet
	.p2align	4, 0x90
	.type	greet,@function
greet:                                  # @greet
	.cfi_startproc
# %bb.0:                                # %entry
	pushq	%r15
	.cfi_def_cfa_offset 16
	pushq	%r14
	.cfi_def_cfa_offset 24
	pushq	%rbx
	.cfi_def_cfa_offset 32
	subq	$16, %rsp
	.cfi_def_cfa_offset 48
	.cfi_offset %rbx, -32
	.cfi_offset %r14, -24
	.cfi_offset %r15, -16
	movb	%dil, 15(%rsp)
	leaq	.L__unnamed_12(%rip), %rdi
	callq	puts@PLT
	movsbq	15(%rsp), %r14
	xorl	%r15d, %r15d
	leaq	.L__unnamed_13(%rip), %rbx
	cmpq	%r14, %r15
	jge	.LBB1_3
	.p2align	4, 0x90
.LBB1_2:                                # %for_body
                                        # =>This Inner Loop Header: Depth=1
	movq	%rbx, %rdi
	callq	puts@PLT
	incq	%r15
	cmpq	%r14, %r15
	jl	.LBB1_2
.LBB1_3:                                # %for_exit
	addq	$16, %rsp
	.cfi_def_cfa_offset 32
	popq	%rbx
	.cfi_def_cfa_offset 24
	popq	%r14
	.cfi_def_cfa_offset 16
	popq	%r15
	.cfi_def_cfa_offset 8
	retq
.Lfunc_end1:
	.size	greet, .Lfunc_end1-greet
	.cfi_endproc
                                        # -- End function
	.type	.L__unnamed_12,@object          # @0
	.section	.rodata.str1.1,"aMS",@progbits,1
.L__unnamed_12:
	.asciz	"Hello from Aria!"
	.size	.L__unnamed_12, 17

	.type	.L__unnamed_13,@object          # @1
.L__unnamed_13:
	.asciz	"Greeting number..."
	.size	.L__unnamed_13, 19

	.type	.L__unnamed_1,@object           # @2
.L__unnamed_1:
	.asciz	"=== Aria Compiler Feature Test ==="
	.size	.L__unnamed_1, 35

	.type	.L__unnamed_2,@object           # @3
.L__unnamed_2:
	.zero	1
	.size	.L__unnamed_2, 1

	.type	.L__unnamed_3,@object           # @4
.L__unnamed_3:
	.asciz	"Test 1: Simple function call"
	.size	.L__unnamed_3, 29

	.type	.L__unnamed_4,@object           # @5
.L__unnamed_4:
	.zero	1
	.size	.L__unnamed_4, 1

	.type	.L__unnamed_5,@object           # @6
.L__unnamed_5:
	.asciz	"Test 2: Nested loops"
	.size	.L__unnamed_5, 21

	.type	.L__unnamed_6,@object           # @7
.L__unnamed_6:
	.asciz	"Outer loop"
	.size	.L__unnamed_6, 11

	.type	.L__unnamed_7,@object           # @8
.L__unnamed_7:
	.asciz	"  Inner loop"
	.size	.L__unnamed_7, 13

	.type	.L__unnamed_8,@object           # @9
.L__unnamed_8:
	.zero	1
	.size	.L__unnamed_8, 1

	.type	.L__unnamed_9,@object           # @10
.L__unnamed_9:
	.asciz	"Test 3: Multiple functions"
	.size	.L__unnamed_9, 27

	.type	.L__unnamed_10,@object          # @11
.L__unnamed_10:
	.zero	1
	.size	.L__unnamed_10, 1

	.type	.L__unnamed_11,@object          # @12
.L__unnamed_11:
	.asciz	"=== All Tests Complete! ==="
	.size	.L__unnamed_11, 28

	.section	".note.GNU-stack","",@progbits
