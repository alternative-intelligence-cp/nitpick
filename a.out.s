	.file	"test_race_rwlock.aria"
	.text
	.globl	rw_writer
	.p2align	4
	.type	rw_writer,@function
rw_writer:
	.cfi_startproc
	pushq	%rbx
	.cfi_def_cfa_offset 16
	.cfi_offset %rbx, -16
	movq	rwl@GOTPCREL(%rip), %rbx
	movq	(%rbx), %rdi
	callq	aria_libc_rwlock_wrlock@PLT
	movq	shared_value@GOTPCREL(%rip), %rax
	movl	(%rax), %ecx
	incl	%ecx
	movl	$2147483647, %edx
	cmovnol	%ecx, %edx
	movl	%edx, (%rax)
	movq	(%rbx), %rdi
	callq	aria_libc_rwlock_unlock@PLT
	xorl	%eax, %eax
	xorl	%edx, %edx
	xorl	%ecx, %ecx
	popq	%rbx
	.cfi_def_cfa_offset 8
	retq
.Lfunc_end0:
	.size	rw_writer, .Lfunc_end0-rw_writer
	.cfi_endproc

	.globl	test_rwlock_protected
	.p2align	4
	.type	test_rwlock_protected,@function
test_rwlock_protected:
	.cfi_startproc
	pushq	%r14
	.cfi_def_cfa_offset 16
	pushq	%rbx
	.cfi_def_cfa_offset 24
	pushq	%rax
	.cfi_def_cfa_offset 32
	.cfi_offset %rbx, -24
	.cfi_offset %r14, -16
	callq	aria_libc_rwlock_create@PLT
	movq	rwl@GOTPCREL(%rip), %r14
	movq	%rax, (%r14)
	leaq	__thunk_rw_writer(%rip), %rdi
	xorl	%esi, %esi
	callq	aria_libc_thread_spawn@PLT
	movq	%rax, %rbx
	movq	(%r14), %rdi
	callq	aria_libc_rwlock_rdlock@PLT
	movq	(%r14), %rdi
	callq	aria_libc_rwlock_unlock@PLT
	movq	%rbx, %rdi
	callq	aria_libc_thread_join@PLT
	movq	(%r14), %rdi
	callq	aria_libc_rwlock_destroy@PLT
	xorl	%eax, %eax
	xorl	%edx, %edx
	addq	$8, %rsp
	.cfi_def_cfa_offset 24
	popq	%rbx
	.cfi_def_cfa_offset 16
	popq	%r14
	.cfi_def_cfa_offset 8
	retq
.Lfunc_end1:
	.size	test_rwlock_protected, .Lfunc_end1-test_rwlock_protected
	.cfi_endproc

	.section	.text.unlikely.,"ax",@progbits
	.globl	failsafe
	.p2align	4
	.type	failsafe,@function
failsafe:
	.cfi_startproc
	pushq	%rax
	.cfi_def_cfa_offset 16
	leaq	.L.str.data(%rip), %rdi
	callq	puts@PLT
	movl	$1, %edi
	callq	exit@PLT
.Lfunc_end2:
	.size	failsafe, .Lfunc_end2-failsafe
	.cfi_endproc

	.text
	.globl	main
	.p2align	4
	.type	main,@function
main:
	.cfi_startproc
	pushq	%rbp
	.cfi_def_cfa_offset 16
	pushq	%rbx
	.cfi_def_cfa_offset 24
	pushq	%rax
	.cfi_def_cfa_offset 32
	.cfi_offset %rbx, -24
	.cfi_offset %rbp, -16
	movq	%rsi, %rbx
	movl	%edi, %ebp
	xorl	%edi, %edi
	xorl	%esi, %esi
	callq	aria_gc_init@PLT
	movl	%ebp, %edi
	movq	%rbx, %rsi
	callq	aria_args_init@PLT
	callq	aria_streams_init@PLT
	leaq	.L.str.data.1(%rip), %rdi
	callq	puts@PLT
	callq	test_rwlock_protected@PLT
	leaq	.L.str.data.3(%rip), %rdi
	callq	puts@PLT
	xorl	%edi, %edi
	callq	exit@PLT
.Lfunc_end3:
	.size	main, .Lfunc_end3-main
	.cfi_endproc

	.p2align	4
	.type	__thunk_rw_writer,@function
__thunk_rw_writer:
	.cfi_startproc
	pushq	%rbx
	.cfi_def_cfa_offset 16
	.cfi_offset %rbx, -16
	movq	rwl@GOTPCREL(%rip), %rbx
	movq	(%rbx), %rdi
	callq	aria_libc_rwlock_wrlock@PLT
	movq	shared_value@GOTPCREL(%rip), %rax
	movl	(%rax), %ecx
	incl	%ecx
	movl	$2147483647, %edx
	cmovnol	%ecx, %edx
	movl	%edx, (%rax)
	movq	(%rbx), %rdi
	callq	aria_libc_rwlock_unlock@PLT
	xorl	%eax, %eax
	xorl	%edx, %edx
	xorl	%ecx, %ecx
	popq	%rbx
	.cfi_def_cfa_offset 8
	retq
.Lfunc_end4:
	.size	__thunk_rw_writer, .Lfunc_end4-__thunk_rw_writer
	.cfi_endproc

	.globl	__test_race_rwlock_init
	.p2align	4
	.type	__test_race_rwlock_init,@function
__test_race_rwlock_init:
	xorl	%eax, %eax
	retq
.Lfunc_end5:
	.size	__test_race_rwlock_init, .Lfunc_end5-__test_race_rwlock_init

	.type	shared_value,@object
	.bss
	.globl	shared_value
	.p2align	2, 0x0
shared_value:
	.long	0
	.size	shared_value, 4

	.type	rwl,@object
	.globl	rwl
	.p2align	3, 0x0
rwl:
	.quad	0
	.size	rwl, 8

	.type	.L.str.data,@object
	.section	.rodata,"a",@progbits
.L.str.data:
	.asciz	"FAILSAFE"
	.size	.L.str.data, 9

	.type	.L.str.data.1,@object
	.p2align	4, 0x0
.L.str.data.1:
	.asciz	"=== RWLock Test ==="
	.size	.L.str.data.1, 20

	.type	.L.str.data.3,@object
.L.str.data.3:
	.asciz	"=== Done ==="
	.size	.L.str.data.3, 13

	.section	".note.GNU-stack","",@progbits
