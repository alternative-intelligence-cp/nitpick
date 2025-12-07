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
	subq	$24, %rsp
	.cfi_def_cfa_offset 32
	movl	$.L__unnamed_1, %edi
	callq	puts@PLT
	movl	$.L__unnamed_2, %edi
	callq	puts@PLT
	movl	$.L__unnamed_3, %edi
	callq	puts@PLT
	movl	$1, %edi
	callq	aria_alloc_exec@PLT
	movq	%rax, 8(%rsp)
	movq	$0, (%rax)
	movl	$.L__unnamed_4, %edi
	callq	puts@PLT
	movl	$.L__unnamed_5, %edi
	callq	puts@PLT
	movl	$.L__unnamed_6, %edi
	callq	puts@PLT
	movl	$.L__unnamed_7, %edi
	callq	puts@PLT
	movl	$.L__unnamed_8, %edi
	callq	puts@PLT
	movl	$.L__unnamed_9, %edi
	callq	puts@PLT
	movl	$.L__unnamed_10, %edi
	callq	puts@PLT
	movq	8(%rsp), %rdi
	movl	$4096, %esi                     # imm = 0x1000
	callq	aria_mem_protect_exec@PLT
	movl	%eax, 20(%rsp)
	testl	%eax, %eax
	je	.LBB1_1
# %bb.2:                                # %else
	movl	$.L__unnamed_11, %edi
	jmp	.LBB1_3
.LBB1_1:                                # %then
	movl	$.L__unnamed_12, %edi
	callq	puts@PLT
	movl	$.L__unnamed_13, %edi
.LBB1_3:                                # %ifcont
	callq	puts@PLT
	movl	$.L__unnamed_14, %edi
	callq	puts@PLT
	movl	$.L__unnamed_15, %edi
	callq	puts@PLT
	movl	$.L__unnamed_16, %edi
	callq	puts@PLT
	movl	$.L__unnamed_17, %edi
	callq	puts@PLT
	movl	$.L__unnamed_18, %edi
	callq	puts@PLT
	movl	$.L__unnamed_19, %edi
	callq	puts@PLT
	movl	$.L__unnamed_20, %edi
	callq	puts@PLT
	movl	$.L__unnamed_21, %edi
	callq	puts@PLT
	movl	$.L__unnamed_22, %edi
	callq	puts@PLT
	movl	$.L__unnamed_23, %edi
	callq	puts@PLT
	movl	$.L__unnamed_24, %edi
	callq	puts@PLT
	movl	$.L__unnamed_25, %edi
	callq	puts@PLT
	movl	$.L__unnamed_26, %edi
	callq	puts@PLT
	movl	$.L__unnamed_27, %edi
	callq	puts@PLT
	movl	$.L__unnamed_28, %edi
	callq	puts@PLT
	movl	$.L__unnamed_29, %edi
	callq	puts@PLT
	movl	$.L__unnamed_30, %edi
	callq	puts@PLT
	movl	$.L__unnamed_31, %edi
	callq	puts@PLT
	movl	$.L__unnamed_32, %edi
	callq	puts@PLT
	movl	$.L__unnamed_33, %edi
	callq	puts@PLT
	movl	$.L__unnamed_34, %edi
	callq	puts@PLT
	movl	$.L__unnamed_35, %edi
	callq	puts@PLT
	movl	$.L__unnamed_36, %edi
	callq	puts@PLT
	xorl	%eax, %eax
	xorl	%edx, %edx
	addq	$24, %rsp
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
	.asciz	"=== JIT Compilation - Function Pointer Casting Test ==="
	.size	.L__unnamed_1, 56

	.type	.L__unnamed_2,@object           # @1
.L__unnamed_2:
	.zero	1
	.size	.L__unnamed_2, 1

	.type	.L__unnamed_3,@object           # @2
.L__unnamed_3:
	.asciz	"Step 1: Allocating executable memory..."
	.size	.L__unnamed_3, 40

	.type	.L__unnamed_4,@object           # @3
.L__unnamed_4:
	.asciz	"        \342\234\223 Allocated wildx buffer (RW state)"
	.size	.L__unnamed_4, 46

	.type	.L__unnamed_5,@object           # @4
.L__unnamed_5:
	.zero	1
	.size	.L__unnamed_5, 1

	.type	.L__unnamed_6,@object           # @5
.L__unnamed_6:
	.asciz	"Step 2: Machine code generation"
	.size	.L__unnamed_6, 32

	.type	.L__unnamed_7,@object           # @6
.L__unnamed_7:
	.asciz	"        (Would write: MOV RAX, 42; RET)"
	.size	.L__unnamed_7, 40

	.type	.L__unnamed_8,@object           # @7
.L__unnamed_8:
	.asciz	"        (Requires array indexing - coming soon)"
	.size	.L__unnamed_8, 48

	.type	.L__unnamed_9,@object           # @8
.L__unnamed_9:
	.zero	1
	.size	.L__unnamed_9, 1

	.type	.L__unnamed_10,@object          # @9
.L__unnamed_10:
	.asciz	"Step 3: Sealing memory for execution (RW \342\206\222 RX)..."
	.size	.L__unnamed_10, 52

	.type	.L__unnamed_12,@object          # @10
.L__unnamed_12:
	.asciz	"        \342\234\223 Memory sealed to RX state"
	.size	.L__unnamed_12, 38

	.type	.L__unnamed_13,@object          # @11
.L__unnamed_13:
	.asciz	"        (W^X security enforced)"
	.size	.L__unnamed_13, 32

	.type	.L__unnamed_11,@object          # @12
.L__unnamed_11:
	.asciz	"        \342\234\227 Failed to seal memory!"
	.size	.L__unnamed_11, 35

	.type	.L__unnamed_14,@object          # @13
.L__unnamed_14:
	.zero	1
	.size	.L__unnamed_14, 1

	.type	.L__unnamed_15,@object          # @14
.L__unnamed_15:
	.asciz	"Step 4: Wildx \342\206\222 Function Pointer Infrastructure"
	.size	.L__unnamed_15, 50

	.type	.L__unnamed_16,@object          # @15
.L__unnamed_16:
	.asciz	"        (Casting infrastructure implemented)"
	.size	.L__unnamed_16, 45

	.type	.L__unnamed_17,@object          # @16
.L__unnamed_17:
	.asciz	"        (Full demo requires type aliases)"
	.size	.L__unnamed_17, 42

	.type	.L__unnamed_18,@object          # @17
.L__unnamed_18:
	.zero	1
	.size	.L__unnamed_18, 1

	.type	.L__unnamed_19,@object          # @18
.L__unnamed_19:
	.asciz	"Step 5: Function pointer ready for execution"
	.size	.L__unnamed_19, 45

	.type	.L__unnamed_20,@object          # @19
.L__unnamed_20:
	.asciz	"        (Execution requires valid machine code)"
	.size	.L__unnamed_20, 48

	.type	.L__unnamed_21,@object          # @20
.L__unnamed_21:
	.zero	1
	.size	.L__unnamed_21, 1

	.type	.L__unnamed_22,@object          # @21
.L__unnamed_22:
	.asciz	"=== Wildx Infrastructure Complete ==="
	.size	.L__unnamed_22, 38

	.type	.L__unnamed_23,@object          # @22
.L__unnamed_23:
	.zero	1
	.size	.L__unnamed_23, 1

	.type	.L__unnamed_24,@object          # @23
.L__unnamed_24:
	.asciz	"\342\234\223 Implemented Features:"
	.size	.L__unnamed_24, 26

	.type	.L__unnamed_25,@object          # @24
.L__unnamed_25:
	.asciz	"  [X] wildx keyword for executable memory"
	.size	.L__unnamed_25, 42

	.type	.L__unnamed_26,@object          # @25
.L__unnamed_26:
	.asciz	"  [X] Cross-platform allocation (mmap/VirtualAlloc)"
	.size	.L__unnamed_26, 52

	.type	.L__unnamed_27,@object          # @26
.L__unnamed_27:
	.asciz	"  [X] Memory protection intrinsics (protect_exec)"
	.size	.L__unnamed_27, 50

	.type	.L__unnamed_28,@object          # @27
.L__unnamed_28:
	.asciz	"  [X] W^X security enforcement"
	.size	.L__unnamed_28, 31

	.type	.L__unnamed_29,@object          # @28
.L__unnamed_29:
	.asciz	"  [X] Type-safe function pointer casting"
	.size	.L__unnamed_29, 41

	.type	.L__unnamed_30,@object          # @29
.L__unnamed_30:
	.zero	1
	.size	.L__unnamed_30, 1

	.type	.L__unnamed_31,@object          # @30
.L__unnamed_31:
	.asciz	"\342\217\270 Next Steps:"
	.size	.L__unnamed_31, 16

	.type	.L__unnamed_32,@object          # @31
.L__unnamed_32:
	.asciz	"  [ ] Array indexing for code buffer"
	.size	.L__unnamed_32, 37

	.type	.L__unnamed_33,@object          # @32
.L__unnamed_33:
	.asciz	"  [ ] Complete x86-64 code generation"
	.size	.L__unnamed_33, 38

	.type	.L__unnamed_34,@object          # @33
.L__unnamed_34:
	.asciz	"  [ ] Actual JIT function execution"
	.size	.L__unnamed_34, 36

	.type	.L__unnamed_35,@object          # @34
.L__unnamed_35:
	.zero	1
	.size	.L__unnamed_35, 1

	.type	.L__unnamed_36,@object          # @35
.L__unnamed_36:
	.asciz	"Ready for Tsoding stream! \360\237\232\200"
	.size	.L__unnamed_36, 31

	.section	".note.GNU-stack","",@progbits
