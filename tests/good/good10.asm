section .data
section .data


section .text
putchar:
    push rbp
    mov rbp, rsp
    sub rsp, 1
    mov byte [rsp], dil
    mov rax, 1
    mov rdi, 1
    lea rsi, [rsp]
    mov rdx, 1
    syscall
    mov rsp, rbp
    pop rbp
    ret
putint:
    push rbp
    mov rbp, rsp
    sub rsp, 32
    mov rax, rdi
    mov rbx, 0
    cmp rax, 0
    jge .Lputint_abs
    mov rbx, 1
    neg rax
.Lputint_abs:
    mov rcx, 10
    mov rsi, rsp
    add rsi, 31
    mov byte [rsi], 0
    dec rsi
    test rax, rax
    jnz .Lputint_loop
    mov byte [rsi], '0'
    dec rsi
    jmp .Lputint_sign
.Lputint_loop:
    xor rdx, rdx
    div rcx
    add dl, '0'
    mov [rsi], dl
    dec rsi
    test rax, rax
    jnz .Lputint_loop
.Lputint_sign:
    cmp rbx, 1
    jne .Lputint_print
    mov byte [rsi], '-'
    dec rsi
.Lputint_print:
    inc rsi
    mov rdx, rsp
    add rdx, 32
    sub rdx, rsi
    mov rax, 1
    mov rdi, 1
    mov rsi, rsi
    syscall
    mov rsp, rbp
    pop rbp
    ret
    global _start

_start:
    push rbp
    mov rbp, rsp
    call main
    mov rdi, rax
    mov rax, 60
    syscall
    global main
main:
    push rbp
    mov rbp, rsp
    sub rsp, 8
    mov rax, 0
    mov [rbp-8], rax
.Lstart0:
    mov rax, [rbp-8]
    mov ebx, eax
    mov rax, 10
    cmp ebx, eax
    je .Lend0
    mov rax, [rbp-8]
    push rax
    mov rdi, rax
    call putint
    pop rax
    mov rax, [rbp-8]
    push rax
    mov rax, 1
    mov ebx, eax
    pop rax
    mov eax, eax
    add eax, ebx
    movsxd rax, eax
    mov [rbp-8], rax
    jmp .Lstart0
.Lend0:
    mov rax, [rbp-8]
    mov rsp, rbp
    pop rbp
    ret
    mov rax, 0
    mov rsp, rbp
    pop rbp
    ret
section .note.GNU-stack noalloc noexec nowrite progbits
