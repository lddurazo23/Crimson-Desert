/* Gateway propio para un punto intermedio. Preserva todos los registros,
   banderas, x87/SSE/AVX y los componentes habilitados de XCR0. El área XSAVE
   se alinea a 64 bytes y la pila se sondea por página antes de reservarla.
   RBP mantiene el marco fijo para el unwind de Windows durante la reserva.
   Solo ContextoMonturas5.rdx puede cambiar como resultado del callback. */
.intel_syntax noprefix
.text
#ifdef _WIN32
.extern DsMonturasCallback
.extern DsMonturasTrampolin
.extern DsMonturasXsaveBytes
.extern DsMonturasXsaveLo
.extern DsMonturasXsaveHi
.extern DsMonturasMxcsr
#endif
.globl DsMonturasGateway
#ifdef _WIN32
.def DsMonturasGateway; .scl 2; .type 32; .endef
.seh_proc DsMonturasGateway
#else
.type DsMonturasGateway,@function
#endif
DsMonturasGateway:
    pushfq
#ifdef _WIN32
.seh_stackalloc 8
#endif
    push rax
#ifdef _WIN32
.seh_pushreg rax
#endif
    push rcx
#ifdef _WIN32
.seh_pushreg rcx
#endif
    push rdx
#ifdef _WIN32
.seh_pushreg rdx
#endif
    push rbx
#ifdef _WIN32
.seh_pushreg rbx
#endif
    push rbp
#ifdef _WIN32
.seh_pushreg rbp
#endif
    push rsi
#ifdef _WIN32
.seh_pushreg rsi
#endif
    push rdi
#ifdef _WIN32
.seh_pushreg rdi
#endif
    push r8
#ifdef _WIN32
.seh_pushreg r8
#endif
    push r9
#ifdef _WIN32
.seh_pushreg r9
#endif
    push r10
#ifdef _WIN32
.seh_pushreg r10
#endif
    push r11
#ifdef _WIN32
.seh_pushreg r11
#endif
    push r12
#ifdef _WIN32
.seh_pushreg r12
#endif
    push r13
#ifdef _WIN32
.seh_pushreg r13
#endif
    push r14
#ifdef _WIN32
.seh_pushreg r14
#endif
    push r15
#ifdef _WIN32
.seh_pushreg r15
#endif
    mov rbp, rsp
#ifdef _WIN32
.seh_setframe rbp, 0
.seh_endprologue
#endif
    xor ecx, ecx
    xgetbv
    cmp eax, DWORD PTR [rip+DsMonturasXsaveLo]
    jne DsMonturasRestaurar
    cmp edx, DWORD PTR [rip+DsMonturasXsaveHi]
    jne DsMonturasRestaurar
    mov r11, rsp
    mov eax, DWORD PTR [rip+DsMonturasXsaveBytes]
    add rax, 191
    mov r10, rsp
    sub r10, rax
    and r10, -64
    mov rax, rsp
    DsMonturasSondear:
    sub rax, 4096
    cmp rax, r10
    jae DsMonturasTocar
    mov rax, r10
    DsMonturasTocar:
    test BYTE PTR [rax], 0
    cmp rax, r10
    jne DsMonturasSondear
    mov rsp, r10
    mov QWORD PTR [rsp+32], r11
    lea rdi, [rsp+64]
    mov QWORD PTR [rsp+40], rdi
    cld
    mov QWORD PTR [rdi+512], 0
    mov QWORD PTR [rdi+520], 0
    mov QWORD PTR [rdi+528], 0
    mov QWORD PTR [rdi+536], 0
    mov QWORD PTR [rdi+544], 0
    mov QWORD PTR [rdi+552], 0
    mov QWORD PTR [rdi+560], 0
    mov QWORD PTR [rdi+568], 0
    mov eax, DWORD PTR [rip+DsMonturasXsaveLo]
    mov edx, DWORD PTR [rip+DsMonturasXsaveHi]
    xsave64 [rdi]
    fninit
    ldmxcsr DWORD PTR [rip+DsMonturasMxcsr]
    mov rcx, r11
    call DsMonturasCallback
    mov rdi, QWORD PTR [rsp+40]
    mov eax, DWORD PTR [rip+DsMonturasXsaveLo]
    mov edx, DWORD PTR [rip+DsMonturasXsaveHi]
    xrstor64 [rdi]
    mov rsp, QWORD PTR [rsp+32]
    DsMonturasRestaurar:
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rbp
    pop rbx
    pop rdx
    pop rcx
    pop rax
    popfq
    jmp QWORD PTR [rip+DsMonturasTrampolin]
#ifdef _WIN32
.seh_endproc
#else
.size DsMonturasGateway,.-DsMonturasGateway
#endif
.globl DsMonturasLeerXcr0
#ifdef _WIN32
.def DsMonturasLeerXcr0; .scl 2; .type 32; .endef
#else
.type DsMonturasLeerXcr0,@function
#endif
DsMonturasLeerXcr0:
    xor ecx,ecx
    xgetbv
    shl rdx,32
    or rax,rdx
    ret
#ifndef _WIN32
.size DsMonturasLeerXcr0,.-DsMonturasLeerXcr0
.section .note.GNU-stack,"",@progbits
#endif
