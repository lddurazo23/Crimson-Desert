.intel_syntax noprefix
.text
.globl ProbarGateway
.type ProbarGateway,@function
ProbarGateway:
    push rbx
    push rbp
    push r12
    push r13
    push r14
    push r15
    mov r15, 16
    mov r14, 15
    mov r13, 14
    mov r12, 13
    mov r11, 12
    mov r10, 11
    mov r9, 10
    mov r8, 9
    mov rdi, 8
    mov rsi, 7
    mov rbp, 6
    mov rbx, 5
    mov rdx, 4
    mov rcx, 3
    mov rax, 2
    vmovdqu ymm0, YMMWORD PTR [rip+VectorSemilla]
    vmovdqu ymm1, YMMWORD PTR [rip+VectorSemilla]
    vmovdqu ymm2, YMMWORD PTR [rip+VectorSemilla]
    vmovdqu ymm3, YMMWORD PTR [rip+VectorSemilla]
    vmovdqu ymm4, YMMWORD PTR [rip+VectorSemilla]
    vmovdqu ymm5, YMMWORD PTR [rip+VectorSemilla]
    vmovdqu ymm6, YMMWORD PTR [rip+VectorSemilla]
    vmovdqu ymm7, YMMWORD PTR [rip+VectorSemilla]
    vmovdqu ymm8, YMMWORD PTR [rip+VectorSemilla]
    vmovdqu ymm9, YMMWORD PTR [rip+VectorSemilla]
    vmovdqu ymm10, YMMWORD PTR [rip+VectorSemilla]
    vmovdqu ymm11, YMMWORD PTR [rip+VectorSemilla]
    vmovdqu ymm12, YMMWORD PTR [rip+VectorSemilla]
    vmovdqu ymm13, YMMWORD PTR [rip+VectorSemilla]
    vmovdqu ymm14, YMMWORD PTR [rip+VectorSemilla]
    vmovdqu ymm15, YMMWORD PTR [rip+VectorSemilla]
    cld
    stc
    jmp DsMonturasGateway
.size ProbarGateway,.-ProbarGateway
.globl CapturarGateway
.type CapturarGateway,@function
CapturarGateway:
    mov QWORD PTR [rip+CapturaRegistros+0], r15
    mov QWORD PTR [rip+CapturaRegistros+8], r14
    mov QWORD PTR [rip+CapturaRegistros+16], r13
    mov QWORD PTR [rip+CapturaRegistros+24], r12
    mov QWORD PTR [rip+CapturaRegistros+32], r11
    mov QWORD PTR [rip+CapturaRegistros+40], r10
    mov QWORD PTR [rip+CapturaRegistros+48], r9
    mov QWORD PTR [rip+CapturaRegistros+56], r8
    mov QWORD PTR [rip+CapturaRegistros+64], rdi
    mov QWORD PTR [rip+CapturaRegistros+72], rsi
    mov QWORD PTR [rip+CapturaRegistros+80], rbp
    mov QWORD PTR [rip+CapturaRegistros+88], rbx
    mov QWORD PTR [rip+CapturaRegistros+96], rdx
    mov QWORD PTR [rip+CapturaRegistros+104], rcx
    mov QWORD PTR [rip+CapturaRegistros+112], rax
    pushfq
    pop QWORD PTR [rip+CapturaRegistros+120]
    vmovdqu YMMWORD PTR [rip+CapturaVectores+0], ymm0
    vmovdqu YMMWORD PTR [rip+CapturaVectores+32], ymm1
    vmovdqu YMMWORD PTR [rip+CapturaVectores+64], ymm2
    vmovdqu YMMWORD PTR [rip+CapturaVectores+96], ymm3
    vmovdqu YMMWORD PTR [rip+CapturaVectores+128], ymm4
    vmovdqu YMMWORD PTR [rip+CapturaVectores+160], ymm5
    vmovdqu YMMWORD PTR [rip+CapturaVectores+192], ymm6
    vmovdqu YMMWORD PTR [rip+CapturaVectores+224], ymm7
    vmovdqu YMMWORD PTR [rip+CapturaVectores+256], ymm8
    vmovdqu YMMWORD PTR [rip+CapturaVectores+288], ymm9
    vmovdqu YMMWORD PTR [rip+CapturaVectores+320], ymm10
    vmovdqu YMMWORD PTR [rip+CapturaVectores+352], ymm11
    vmovdqu YMMWORD PTR [rip+CapturaVectores+384], ymm12
    vmovdqu YMMWORD PTR [rip+CapturaVectores+416], ymm13
    vmovdqu YMMWORD PTR [rip+CapturaVectores+448], ymm14
    vmovdqu YMMWORD PTR [rip+CapturaVectores+480], ymm15
    vzeroupper
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbp
    pop rbx
    ret
.size CapturarGateway,.-CapturarGateway
.globl DsMonturasCallback
.type DsMonturasCallback,@function
DsMonturasCallback:
    mov QWORD PTR [rcx+96], 5000
    xor r15, r15
    xor r14, r14
    xor r13, r13
    xor r12, r12
    xor r11, r11
    xor r10, r10
    xor r9, r9
    xor r8, r8
    xor rdi, rdi
    xor rsi, rsi
    xor rbp, rbp
    xor rbx, rbx
    xor rdx, rdx
    xor rcx, rcx
    xor rax, rax
    vpxor ymm0, ymm0, ymm0
    vpxor ymm1, ymm1, ymm1
    vpxor ymm2, ymm2, ymm2
    vpxor ymm3, ymm3, ymm3
    vpxor ymm4, ymm4, ymm4
    vpxor ymm5, ymm5, ymm5
    vpxor ymm6, ymm6, ymm6
    vpxor ymm7, ymm7, ymm7
    vpxor ymm8, ymm8, ymm8
    vpxor ymm9, ymm9, ymm9
    vpxor ymm10, ymm10, ymm10
    vpxor ymm11, ymm11, ymm11
    vpxor ymm12, ymm12, ymm12
    vpxor ymm13, ymm13, ymm13
    vpxor ymm14, ymm14, ymm14
    vpxor ymm15, ymm15, ymm15
    std
    clc
    ret
.size DsMonturasCallback,.-DsMonturasCallback
.section .note.GNU-stack,"",@progbits
