	.file	"kernel.c"
	.text
	.p2align 4
	.globl	saxpy_f32
	.type	saxpy_f32, @function
saxpy_f32:
.LFB0:
	.cfi_startproc
	movq	%rdi, %r8
	movq	%rdx, %rdi
	cmpq	$1, %rcx
	je	.L2
	salq	$2, %rcx
	xorl	%eax, %eax
	xorl	%edx, %edx
	testq	%rdi, %rdi
	je	.L32
	.p2align 4
	.p2align 3
.L12:
	incq	%rdx
	vmovss	(%r8,%rax), %xmm1
	vfmadd213ss	(%rsi,%rax), %xmm0, %xmm1
	vmovss	%xmm1, (%rsi,%rax)
	addq	%rcx, %rax
	cmpq	%rdx, %rdi
	jne	.L12
	ret
	.p2align 4
	.p2align 3
.L2:
	testq	%rdx, %rdx
	je	.L30
	leaq	-1(%rdx), %rax
	leaq	4(%r8), %rcx
	movq	%rsi, %rdx
	subq	%rcx, %rdx
	cmpq	$24, %rdx
	jbe	.L13
	cmpq	$2, %rax
	jbe	.L13
	cmpq	$6, %rax
	jbe	.L14
	movq	%rdi, %rdx
	vbroadcastss	%xmm0, %ymm2
	xorl	%eax, %eax
	shrq	$3, %rdx
	salq	$5, %rdx
	.p2align 4
	.p2align 3
.L7:
	vmovups	(%r8,%rax), %ymm1
	vfmadd213ps	(%rsi,%rax), %ymm2, %ymm1
	vmovups	%ymm1, (%rsi,%rax)
	addq	$32, %rax
	cmpq	%rax, %rdx
	jne	.L7
	movq	%rdi, %rax
	andq	$-8, %rax
	testb	$7, %dil
	je	.L28
	movq	%rdi, %rdx
	subq	%rax, %rdx
	leaq	-1(%rdx), %rcx
	cmpq	$2, %rcx
	jbe	.L33
	vzeroupper
.L6:
	leaq	0(,%rax,4), %r9
	vbroadcastss	%xmm0, %xmm1
	leaq	(%rsi,%r9), %rcx
	vmovups	(%rcx), %xmm3
	vfmadd132ps	(%r8,%r9), %xmm3, %xmm1
	vmovups	%xmm1, (%rcx)
	movq	%rdx, %rcx
	andq	$-4, %rcx
	addq	%rcx, %rax
	cmpq	%rcx, %rdx
	je	.L30
.L9:
	leaq	0(,%rax,4), %rdx
	leaq	(%rsi,%rdx), %rcx
	vmovss	(%r8,%rdx), %xmm1
	vfmadd213ss	(%rcx), %xmm0, %xmm1
	vmovss	%xmm1, (%rcx)
	leaq	1(%rax), %rcx
	cmpq	%rdi, %rcx
	jnb	.L30
	leaq	4(%rsi,%rdx), %rcx
	addq	$2, %rax
	vmovss	4(%r8,%rdx), %xmm1
	vfmadd213ss	(%rcx), %xmm0, %xmm1
	vmovss	%xmm1, (%rcx)
	cmpq	%rdi, %rax
	jnb	.L30
	leaq	8(%rsi,%rdx), %rax
	vmovss	(%rax), %xmm4
	vfmadd132ss	8(%r8,%rdx), %xmm4, %xmm0
	vmovss	%xmm0, (%rax)
	ret
.L28:
	vzeroupper
.L30:
	ret
	.p2align 4
	.p2align 3
.L32:
	ret
	.p2align 4
	.p2align 3
.L13:
	xorl	%eax, %eax
	.p2align 4
	.p2align 3
.L5:
	vmovss	(%r8,%rax,4), %xmm1
	vfmadd213ss	(%rsi,%rax,4), %xmm0, %xmm1
	vmovss	%xmm1, (%rsi,%rax,4)
	incq	%rax
	cmpq	%rax, %rdi
	jne	.L5
	ret
.L14:
	movq	%rdi, %rdx
	xorl	%eax, %eax
	jmp	.L6
.L33:
	vzeroupper
	jmp	.L9
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
	movq	%rdx, %rdi
	cmpq	$1, %rcx
	je	.L35
	salq	$3, %rcx
	xorl	%eax, %eax
	xorl	%edx, %edx
	testq	%rdi, %rdi
	je	.L67
	.p2align 4
	.p2align 3
.L45:
	incq	%rdx
	vmovsd	(%r8,%rax), %xmm1
	vfmadd213sd	(%rsi,%rax), %xmm0, %xmm1
	vmovsd	%xmm1, (%rsi,%rax)
	addq	%rcx, %rax
	cmpq	%rdx, %rdi
	jne	.L45
	ret
	.p2align 4
	.p2align 3
.L35:
	testq	%rdx, %rdx
	je	.L66
	leaq	8(%r8), %rdx
	movq	%rsi, %rax
	subq	%rdx, %rax
	cmpq	$16, %rax
	jbe	.L46
	cmpq	$1, %rdi
	je	.L46
	leaq	-1(%rdi), %rax
	cmpq	$2, %rax
	jbe	.L47
	movq	%rdi, %rdx
	vbroadcastsd	%xmm0, %ymm2
	xorl	%eax, %eax
	shrq	$2, %rdx
	salq	$5, %rdx
	.p2align 4
	.p2align 3
.L40:
	vmovupd	(%r8,%rax), %ymm1
	vfmadd213pd	(%rsi,%rax), %ymm2, %ymm1
	vmovupd	%ymm1, (%rsi,%rax)
	addq	$32, %rax
	cmpq	%rdx, %rax
	jne	.L40
	movq	%rdi, %rax
	andq	$-4, %rax
	testb	$3, %dil
	je	.L64
	subq	%rax, %rdi
	cmpq	$1, %rdi
	je	.L68
	vzeroupper
.L39:
	leaq	0(,%rax,8), %rcx
	vmovddup	%xmm0, %xmm1
	leaq	(%rsi,%rcx), %rdx
	vmovupd	(%rdx), %xmm4
	vfmadd132pd	(%r8,%rcx), %xmm4, %xmm1
	vmovupd	%xmm1, (%rdx)
	movq	%rdi, %rdx
	andq	$-2, %rdx
	addq	%rdx, %rax
	cmpq	%rdx, %rdi
	je	.L66
.L42:
	salq	$3, %rax
	addq	%rax, %rsi
	vmovsd	(%rsi), %xmm3
	vfmadd132sd	(%r8,%rax), %xmm3, %xmm0
	vmovsd	%xmm0, (%rsi)
	ret
.L64:
	vzeroupper
.L66:
	ret
	.p2align 4
	.p2align 3
.L67:
	ret
	.p2align 4
	.p2align 3
.L46:
	xorl	%eax, %eax
	.p2align 4
	.p2align 3
.L38:
	vmovsd	(%r8,%rax,8), %xmm1
	vfmadd213sd	(%rsi,%rax,8), %xmm0, %xmm1
	vmovsd	%xmm1, (%rsi,%rax,8)
	incq	%rax
	cmpq	%rax, %rdi
	jne	.L38
	ret
.L47:
	xorl	%eax, %eax
	jmp	.L39
.L68:
	vzeroupper
	jmp	.L42
	.cfi_endproc
.LFE1:
	.size	saxpy_f64, .-saxpy_f64
	.p2align 4
	.globl	dot_f32
	.type	dot_f32, @function
dot_f32:
.LFB2:
	.cfi_startproc
	vxorps	%xmm3, %xmm3, %xmm3
	cmpq	$1, %rcx
	je	.L70
	testq	%rdx, %rdx
	je	.L75
	salq	$2, %rcx
	vxorpd	%xmm2, %xmm2, %xmm2
	xorl	%eax, %eax
	xorl	%r8d, %r8d
	.p2align 4
	.p2align 3
.L74:
	incq	%r8
	vcvtss2sd	(%rdi,%rax), %xmm3, %xmm0
	vcvtss2sd	(%rsi,%rax), %xmm3, %xmm1
	addq	%rcx, %rax
	vmulsd	%xmm1, %xmm0, %xmm0
	vaddsd	%xmm0, %xmm2, %xmm2
	cmpq	%r8, %rdx
	jne	.L74
	vmovsd	%xmm2, %xmm2, %xmm0
	ret
	.p2align 4
	.p2align 3
.L70:
	testq	%rdx, %rdx
	je	.L75
	xorl	%eax, %eax
	vxorpd	%xmm2, %xmm2, %xmm2
	.p2align 4
	.p2align 3
.L73:
	vcvtss2sd	(%rdi,%rax,4), %xmm3, %xmm0
	vcvtss2sd	(%rsi,%rax,4), %xmm3, %xmm1
	incq	%rax
	vmulsd	%xmm1, %xmm0, %xmm0
	vaddsd	%xmm0, %xmm2, %xmm2
	cmpq	%rax, %rdx
	jne	.L73
	vmovsd	%xmm2, %xmm2, %xmm0
	ret
	.p2align 4
	.p2align 3
.L75:
	vxorpd	%xmm2, %xmm2, %xmm2
	vmovsd	%xmm2, %xmm2, %xmm0
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
	movq	%rsi, %r8
	movq	%rdx, %rsi
	cmpq	$1, %rcx
	je	.L79
	testq	%rdx, %rdx
	je	.L87
	salq	$3, %rcx
	vxorpd	%xmm0, %xmm0, %xmm0
	xorl	%eax, %eax
	xorl	%edx, %edx
	.p2align 4
	.p2align 3
.L86:
	incq	%rdx
	vmovsd	(%rdi,%rax), %xmm1
	vmulsd	(%r8,%rax), %xmm1, %xmm1
	addq	%rcx, %rax
	vaddsd	%xmm1, %xmm0, %xmm0
	cmpq	%rdx, %rsi
	jne	.L86
	ret
	.p2align 4
	.p2align 3
.L79:
	testq	%rdx, %rdx
	je	.L87
	leaq	-1(%rdx), %rax
	cmpq	$2, %rax
	jbe	.L88
	shrq	$2, %rdx
	vxorpd	%xmm0, %xmm0, %xmm0
	salq	$5, %rdx
	xorl	%eax, %eax
	.p2align 4
	.p2align 3
.L83:
	vmovupd	(%r8,%rax), %ymm4
	vmulpd	(%rdi,%rax), %ymm4, %ymm1
	addq	$32, %rax
	vaddsd	%xmm1, %xmm0, %xmm0
	vunpckhpd	%xmm1, %xmm1, %xmm2
	vextractf64x2	$0x1, %ymm1, %xmm1
	vaddsd	%xmm0, %xmm2, %xmm2
	vaddsd	%xmm2, %xmm1, %xmm0
	vunpckhpd	%xmm1, %xmm1, %xmm1
	vaddsd	%xmm1, %xmm0, %xmm0
	cmpq	%rax, %rdx
	jne	.L83
	movq	%rsi, %rax
	andq	$-4, %rax
	testb	$3, %sil
	je	.L92
	vzeroupper
.L82:
	leaq	1(%rax), %rcx
	vmovsd	(%rdi,%rax,8), %xmm5
	leaq	0(,%rax,8), %rdx
	vfmadd231sd	(%r8,%rax,8), %xmm5, %xmm0
	cmpq	%rcx, %rsi
	jbe	.L78
	addq	$2, %rax
	vmovsd	8(%rdi,%rdx), %xmm6
	vfmadd231sd	8(%r8,%rdx), %xmm6, %xmm0
	cmpq	%rax, %rsi
	jbe	.L78
	vmovsd	16(%r8,%rdx), %xmm7
	vfmadd231sd	16(%rdi,%rdx), %xmm7, %xmm0
	ret
	.p2align 4
	.p2align 3
.L87:
	vxorpd	%xmm0, %xmm0, %xmm0
.L78:
	ret
	.p2align 4
	.p2align 3
.L92:
	vzeroupper
	ret
.L88:
	xorl	%eax, %eax
	vxorpd	%xmm0, %xmm0, %xmm0
	jmp	.L82
	.cfi_endproc
.LFE3:
	.size	dot_f64, .-dot_f64
	.p2align 4
	.globl	elemul_f32
	.type	elemul_f32, @function
elemul_f32:
.LFB4:
	.cfi_startproc
	movq	%rdi, %r9
	movq	%rsi, %rdi
	movq	%rdx, %rsi
	cmpq	$1, %r8
	je	.L94
	salq	$2, %r8
	xorl	%eax, %eax
	xorl	%edx, %edx
	testq	%rcx, %rcx
	je	.L123
	.p2align 4
	.p2align 3
.L104:
	vmovss	(%r9,%rax), %xmm0
	vmulss	(%rdi,%rax), %xmm0, %xmm0
	incq	%rdx
	vmovss	%xmm0, (%rsi,%rax)
	addq	%r8, %rax
	cmpq	%rdx, %rcx
	jne	.L104
	ret
	.p2align 4
	.p2align 3
.L94:
	testq	%rcx, %rcx
	je	.L122
	leaq	4(%rdi), %r8
	leaq	-1(%rcx), %rax
	subq	%r8, %rdx
	cmpq	$24, %rdx
	seta	%r8b
	cmpq	$2, %rax
	seta	%dl
	testb	%dl, %r8b
	je	.L105
	leaq	4(%r9), %r8
	movq	%rsi, %rdx
	subq	%r8, %rdx
	cmpq	$24, %rdx
	jbe	.L105
	cmpq	$6, %rax
	jbe	.L106
	movq	%rcx, %rdx
	xorl	%eax, %eax
	shrq	$3, %rdx
	salq	$5, %rdx
	.p2align 4
	.p2align 3
.L99:
	vmovups	(%r9,%rax), %ymm1
	vmulps	(%rdi,%rax), %ymm1, %ymm0
	vmovups	%ymm0, (%rsi,%rax)
	addq	$32, %rax
	cmpq	%rax, %rdx
	jne	.L99
	movq	%rcx, %rax
	andq	$-8, %rax
	testb	$7, %cl
	je	.L120
	movq	%rcx, %r8
	subq	%rax, %r8
	leaq	-1(%r8), %rdx
	cmpq	$2, %rdx
	jbe	.L124
	vzeroupper
.L98:
	leaq	0(,%rax,4), %rdx
	vmovups	(%r9,%rdx), %xmm2
	vmulps	(%rdi,%rdx), %xmm2, %xmm0
	vmovups	%xmm0, (%rsi,%rdx)
	movq	%r8, %rdx
	andq	$-4, %rdx
	addq	%rdx, %rax
	cmpq	%rdx, %r8
	je	.L122
.L101:
	leaq	0(,%rax,4), %rdx
	leaq	1(%rax), %r8
	vmovss	(%r9,%rdx), %xmm0
	vmulss	(%rdi,%rdx), %xmm0, %xmm0
	vmovss	%xmm0, (%rsi,%rdx)
	cmpq	%r8, %rcx
	jbe	.L122
	vmovss	4(%r9,%rdx), %xmm0
	vmulss	4(%rdi,%rdx), %xmm0, %xmm0
	addq	$2, %rax
	vmovss	%xmm0, 4(%rsi,%rdx)
	cmpq	%rax, %rcx
	jbe	.L122
	vmovss	8(%r9,%rdx), %xmm0
	vmulss	8(%rdi,%rdx), %xmm0, %xmm0
	vmovss	%xmm0, 8(%rsi,%rdx)
	ret
.L120:
	vzeroupper
.L122:
	ret
	.p2align 4
	.p2align 3
.L123:
	ret
	.p2align 4
	.p2align 3
.L105:
	xorl	%eax, %eax
	.p2align 4
	.p2align 3
.L97:
	vmovss	(%rdi,%rax,4), %xmm0
	vmulss	(%r9,%rax,4), %xmm0, %xmm0
	vmovss	%xmm0, (%rsi,%rax,4)
	incq	%rax
	cmpq	%rax, %rcx
	jne	.L97
	ret
.L106:
	movq	%rcx, %r8
	xorl	%eax, %eax
	jmp	.L98
.L124:
	vzeroupper
	jmp	.L101
	.cfi_endproc
.LFE4:
	.size	elemul_f32, .-elemul_f32
	.p2align 4
	.globl	elemul_f64
	.type	elemul_f64, @function
elemul_f64:
.LFB5:
	.cfi_startproc
	movq	%rdi, %r9
	movq	%rsi, %rdi
	movq	%rdx, %rsi
	cmpq	$1, %r8
	je	.L126
	salq	$3, %r8
	xorl	%eax, %eax
	xorl	%edx, %edx
	testq	%rcx, %rcx
	je	.L158
	.p2align 4
	.p2align 3
.L136:
	incq	%rdx
	vmovsd	(%r9,%rax), %xmm0
	vmulsd	(%rdi,%rax), %xmm0, %xmm0
	vmovsd	%xmm0, (%rsi,%rax)
	addq	%r8, %rax
	cmpq	%rdx, %rcx
	jne	.L136
	ret
	.p2align 4
	.p2align 3
.L126:
	testq	%rcx, %rcx
	je	.L157
	leaq	8(%rdi), %rdx
	movq	%rsi, %rax
	subq	%rdx, %rax
	cmpq	$16, %rax
	seta	%dl
	cmpq	$1, %rcx
	setne	%al
	testb	%al, %dl
	je	.L137
	leaq	8(%r9), %rdx
	movq	%rsi, %rax
	subq	%rdx, %rax
	cmpq	$16, %rax
	jbe	.L137
	leaq	-1(%rcx), %rax
	cmpq	$2, %rax
	jbe	.L138
	movq	%rcx, %rdx
	xorl	%eax, %eax
	shrq	$2, %rdx
	salq	$5, %rdx
	.p2align 4
	.p2align 3
.L131:
	vmovupd	(%r9,%rax), %ymm1
	vmulpd	(%rdi,%rax), %ymm1, %ymm0
	vmovupd	%ymm0, (%rsi,%rax)
	addq	$32, %rax
	cmpq	%rdx, %rax
	jne	.L131
	movq	%rcx, %rax
	andq	$-4, %rax
	testb	$3, %cl
	je	.L155
	subq	%rax, %rcx
	cmpq	$1, %rcx
	je	.L159
	vzeroupper
.L130:
	leaq	0(,%rax,8), %rdx
	vmovupd	(%r9,%rdx), %xmm2
	vmulpd	(%rdi,%rdx), %xmm2, %xmm0
	vmovupd	%xmm0, (%rsi,%rdx)
	movq	%rcx, %rdx
	andq	$-2, %rdx
	addq	%rdx, %rax
	cmpq	%rdx, %rcx
	je	.L157
.L133:
	salq	$3, %rax
	vmovsd	(%r9,%rax), %xmm0
	vmulsd	(%rdi,%rax), %xmm0, %xmm0
	vmovsd	%xmm0, (%rsi,%rax)
	ret
.L155:
	vzeroupper
.L157:
	ret
	.p2align 4
	.p2align 3
.L158:
	ret
	.p2align 4
	.p2align 3
.L137:
	xorl	%eax, %eax
	.p2align 4
	.p2align 3
.L129:
	vmovsd	(%rdi,%rax,8), %xmm0
	vmulsd	(%r9,%rax,8), %xmm0, %xmm0
	vmovsd	%xmm0, (%rsi,%rax,8)
	incq	%rax
	cmpq	%rax, %rcx
	jne	.L129
	ret
.L138:
	xorl	%eax, %eax
	jmp	.L130
.L159:
	vzeroupper
	jmp	.L133
	.cfi_endproc
.LFE5:
	.size	elemul_f64, .-elemul_f64
	.ident	"GCC: (GNU) 11.5.0 20240719 (Red Hat 11.5.0-5)"
	.section	.note.GNU-stack,"",@progbits
