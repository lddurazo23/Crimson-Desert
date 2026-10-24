; Gateway propio equivalente a monturas5_gateway.S. Los datos del juego no
; aparecen aquí: solo se preserva contexto y se llama al callback de C++.
OPTION CASEMAP:NONE
EXTERN DsMonturasCallback:PROC
EXTERN DsMonturasTrampolin:QWORD
EXTERN DsMonturasXsaveBytes:DWORD
EXTERN DsMonturasXsaveLo:DWORD
EXTERN DsMonturasXsaveHi:DWORD
EXTERN DsMonturasMxcsr:DWORD
PUBLIC DsMonturasGateway
PUBLIC DsMonturasLeerXcr0
.code
DsMonturasGateway PROC FRAME
    pushfq
    .allocstack 8
    push rax
    .pushreg rax
    push rcx
    .pushreg rcx
    push rdx
    .pushreg rdx
    push rbx
    .pushreg rbx
    push rbp
    .pushreg rbp
    push rsi
    .pushreg rsi
    push rdi
    .pushreg rdi
    push r8
    .pushreg r8
    push r9
    .pushreg r9
    push r10
    .pushreg r10
    push r11
    .pushreg r11
    push r12
    .pushreg r12
    push r13
    .pushreg r13
    push r14
    .pushreg r14
    push r15
    .pushreg r15
    mov rbp, rsp
    .setframe rbp, 0
    .endprolog
    xor ecx, ecx
    xgetbv
    cmp eax, DWORD PTR [DsMonturasXsaveLo]
    jne DsMonturasRestaurar
    cmp edx, DWORD PTR [DsMonturasXsaveHi]
    jne DsMonturasRestaurar
    mov r11, rsp
    mov eax, DWORD PTR [DsMonturasXsaveBytes]
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
    mov eax, DWORD PTR [DsMonturasXsaveLo]
    mov edx, DWORD PTR [DsMonturasXsaveHi]
    xsave64 [rdi]
    fninit
    ldmxcsr DWORD PTR [DsMonturasMxcsr]
    mov rcx, r11
    call DsMonturasCallback
    mov rdi, QWORD PTR [rsp+40]
    mov eax, DWORD PTR [DsMonturasXsaveLo]
    mov edx, DWORD PTR [DsMonturasXsaveHi]
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
    jmp QWORD PTR [DsMonturasTrampolin]
DsMonturasGateway ENDP
DsMonturasLeerXcr0 PROC
    xor ecx,ecx
    xgetbv
    shl rdx,32
    or rax,rdx
    ret
DsMonturasLeerXcr0 ENDP
END
