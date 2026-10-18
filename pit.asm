[bits 64]

global pit_handler_stub
extern pit_handler

pit_handler_stub:
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15

    ; 对齐栈
    mov rbx, rsp
    and rsp, 0xFFFFFFFFFFFFFFF0
    sub rsp, 8

    call pit_handler

    mov rsp, rbx

    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax

    ; 发送 EOI
    mov al, 0x20
    out 0x20, al
    iretq