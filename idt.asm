[bits 64]

global idt_load
global keyboard_handler_stub
global mouse_handler_stub

extern idt_table
extern keyboard_handler
extern mouse_handler

idt_load:
    lidt [idt_descriptor]
    ret

keyboard_handler_stub:
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

    ; 16字节对齐
    mov rbx, rsp
    and rsp, 0xFFFFFFFFFFFFFFF0
    sub rsp, 8

    call keyboard_handler

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

    mov al, 0x20
    out 0x20, al
    iretq

mouse_handler_stub:
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

    ; 16字节对齐
    mov rbx, rsp
    and rsp, 0xFFFFFFFFFFFFFFF0
    sub rsp, 8

    call mouse_handler

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

    ; 鼠标在从 PIC，必须发两次 EOI
    mov al, 0x20
    out 0xA0, al
    out 0x20, al
    iretq

section .data
align 16
idt_descriptor:
    dw 256 * 16 - 1
    dq idt_table