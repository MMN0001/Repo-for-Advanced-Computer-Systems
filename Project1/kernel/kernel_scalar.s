	.file	"kernel.c"
	.text
	.p2align 4
	.globl	saxpy_f32
	.type	saxpy_f32, @function
saxpy_f32:
.LFB0:
	.cfi_startproc
	movq	%rdi, %r8
	cmpq	$1, %rcx
	je	.L2
	salq	$2, %rcx
	xorl	%eax, %eax
	xorl	%edi, %edi
	testq	%rdx, %rdx
	je	.L14
	.p2align 4,,10
	.p2align 3
.L6:
	movss	(%r8,%rax), %xmm1
	addq	$1, %rdi
	mulss	%xmm0, %xmm1
	addss	(%rsi,%rax), %xmm1
	movss	%xmm1, (%rsi,%rax)
	addq	%rcx, %rax
	cmpq	%rdi, %rdx
	jne	.L6
.L1:
	ret
	.p2align 4,,10
	.p2align 3
.L2:
	testq	%rdx, %rdx
	je	.L1
	xorl	%eax, %eax
	.p2align 4,,10
	.p2align 3
.L5:
	movss	(%r8,%rax,4), %xmm1
	mulss	%xmm0, %xmm1
	addss	(%rsi,%rax,4), %xmm1
	movss	%xmm1, (%rsi,%rax,4)
	addq	$1, %rax
	cmpq	%rax, %rdx
	jne	.L5
	ret
	.p2align 4,,10
	.p2align 3
.L14:
	ret
	.cfi_endproc
.LFE0:
	.size	saxpy_f32, .-saxpy_f32
	.p2align 4
	.globl	saxpy_f64
	.type	saxpy_f64, @function
saxpy_f64:
.LFB1:
	.cfi_startproc
	movq	%rdi, %r8
	cmpq	$1, %rcx
	je	.L16
	salq	$3, %rcx
	xorl	%eax, %eax
	xorl	%edi, %edi
	testq	%rdx, %rdx
	je	.L27
	.p2align 4,,10
	.p2align 3
.L20:
	movsd	(%r8,%rax), %xmm1
	addq	$1, %rdi
	mulsd	%xmm0, %xmm1
	addsd	(%rsi,%rax), %xmm1
	movsd	%xmm1, (%rsi,%rax)
	addq	%rcx, %rax
	cmpq	%rdi, %rdx
	jne	.L20
.L15:
	ret
	.p2align 4,,10
	.p2align 3
.L16:
	testq	%rdx, %rdx
	je	.L15
	xorl	%eax, %eax
	.p2align 4,,10
	.p2align 3
.L19:
	movsd	(%r8,%rax,8), %xmm1
	mulsd	%xmm0, %xmm1
	addsd	(%rsi,%rax,8), %xmm1
	movsd	%xmm1, (%rsi,%rax,8)
	addq	$1, %rax
	cmpq	%rax, %rdx
	jne	.L19
	ret
	.p2align 4,,10
	.p2align 3
.L27:
	ret
	.cfi_endproc
.LFE1:
	.size	saxpy_f64, .-saxpy_f64
	.p2align 4
	.globl	dot_f32
	.type	dot_f32, @function
dot_f32:
.LFB2:
	.cfi_startproc
	cmpq	$1, %rcx
	je	.L29
	testq	%rdx, %rdx
	je	.L34
	salq	$2, %rcx
	xorl	%eax, %eax
	pxor	%xmm1, %xmm1
	xorl	%r8d, %r8d
	.p2align 4,,10
	.p2align 3
.L33:
	pxor	%xmm0, %xmm0
	pxor	%xmm2, %xmm2
	addq	$1, %r8
	cvtss2sd	(%rdi,%rax), %xmm0
	cvtss2sd	(%rsi,%rax), %xmm2
	mulsd	%xmm2, %xmm0
	addq	%rcx, %rax
	addsd	%xmm0, %xmm1
	cmpq	%r8, %rdx
	jne	.L33
	movapd	%xmm1, %xmm0
	ret
	.p2align 4,,10
	.p2align 3
.L29:
	testq	%rdx, %rdx
	je	.L34
	xorl	%eax, %eax
	pxor	%xmm1, %xmm1
	.p2align 4,,10
	.p2align 3
.L32:
	pxor	%xmm0, %xmm0
	pxor	%xmm2, %xmm2
	cvtss2sd	(%rdi,%rax,4), %xmm0
	cvtss2sd	(%rsi,%rax,4), %xmm2
	mulsd	%xmm2, %xmm0
	addq	$1, %rax
	addsd	%xmm0, %xmm1
	cmpq	%rax, %rdx
	jne	.L32
	movapd	%xmm1, %xmm0
	ret
	.p2align 4,,10
	.p2align 3
.L34:
	pxor	%xmm1, %xmm1
	movapd	%xmm1, %xmm0
	ret
	.cfi_endproc
.LFE2:
	.size	dot_f32, .-dot_f32
	.p2align 4
	.globl	dot_f64
	.type	dot_f64, @function
dot_f64:
.LFB3:
	.cfi_startproc
	cmpq	$1, %rcx
	je	.L38
	testq	%rdx, %rdx
	je	.L43
	salq	$3, %rcx
	xorl	%eax, %eax
	pxor	%xmm1, %xmm1
	xorl	%r8d, %r8d
	.p2align 4,,10
	.p2align 3
.L42:
	movsd	(%rdi,%rax), %xmm0
	mulsd	(%rsi,%rax), %xmm0
	addq	$1, %r8
	addq	%rcx, %rax
	addsd	%xmm0, %xmm1
	cmpq	%r8, %rdx
	jne	.L42
	movapd	%xmm1, %xmm0
	ret
	.p2align 4,,10
	.p2align 3
.L38:
	testq	%rdx, %rdx
	je	.L43
	xorl	%eax, %eax
	pxor	%xmm1, %xmm1
	.p2align 4,,10
	.p2align 3
.L41:
	movsd	(%rdi,%rax,8), %xmm0
	mulsd	(%rsi,%rax,8), %xmm0
	addq	$1, %rax
	addsd	%xmm0, %xmm1
	cmpq	%rax, %rdx
	jne	.L41
	movapd	%xmm1, %xmm0
	ret
	.p2align 4,,10
	.p2align 3
.L43:
	pxor	%xmm1, %xmm1
	movapd	%xmm1, %xmm0
	ret
	.cfi_endproc
.LFE3:
	.size	dot_f64, .-dot_f64
	.p2align 4
	.globl	elemul_f32
	.type	elemul_f32, @function
elemul_f32:
.LFB4:
	.cfi_startproc
	movq	%rsi, %r9
	cmpq	$1, %r8
	je	.L47
	salq	$2, %r8
	xorl	%eax, %eax
	xorl	%esi, %esi
	testq	%rcx, %rcx
	je	.L58
	.p2align 4,,10
	.p2align 3
.L51:
	movss	(%rdi,%rax), %xmm0
	mulss	(%r9,%rax), %xmm0
	addq	$1, %rsi
	movss	%xmm0, (%rdx,%rax)
	addq	%r8, %rax
	cmpq	%rsi, %rcx
	jne	.L51
.L46:
	ret
	.p2align 4,,10
	.p2align 3
.L47:
	testq	%rcx, %rcx
	je	.L46
	xorl	%eax, %eax
	.p2align 4,,10
	.p2align 3
.L50:
	movss	(%rdi,%rax,4), %xmm0
	mulss	(%r9,%rax,4), %xmm0
	movss	%xmm0, (%rdx,%rax,4)
	addq	$1, %rax
	cmpq	%rax, %rcx
	jne	.L50
	ret
	.p2align 4,,10
	.p2align 3
.L58:
	ret
	.cfi_endproc
.LFE4:
	.size	elemul_f32, .-elemul_f32
	.p2align 4
	.globl	elemul_f64
	.type	elemul_f64, @function
elemul_f64:
.LFB5:
	.cfi_startproc
	movq	%rsi, %r9
	cmpq	$1, %r8
	je	.L60
	salq	$3, %r8
	xorl	%eax, %eax
	xorl	%esi, %esi
	testq	%rcx, %rcx
	je	.L71
	.p2align 4,,10
	.p2align 3
.L64:
	movsd	(%rdi,%rax), %xmm0
	mulsd	(%r9,%rax), %xmm0
	addq	$1, %rsi
	movsd	%xmm0, (%rdx,%rax)
	addq	%r8, %rax
	cmpq	%rsi, %rcx
	jne	.L64
.L59:
	ret
	.p2align 4,,10
	.p2align 3
.L60:
	testq	%rcx, %rcx
	je	.L59
	xorl	%eax, %eax
	.p2align 4,,10
	.p2align 3
.L63:
	movsd	(%rdi,%rax,8), %xmm0
	mulsd	(%r9,%rax,8), %xmm0
	movsd	%xmm0, (%rdx,%rax,8)
	addq	$1, %rax
	cmpq	%rax, %rcx
	jne	.L63
	ret
	.p2align 4,,10
	.p2align 3
.L71:
	ret
	.cfi_endproc
.LFE5:
	.size	elemul_f64, .-elemul_f64
	.ident	"GCC: (GNU) 11.5.0 20240719 (Red Hat 11.5.0-5)"
	.section	.note.GNU-stack,"",@progbits
