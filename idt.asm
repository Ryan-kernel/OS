[bits 64]
[extern idt_table]

global idt_load
global keyboard_handler_stub

extern keyboard_handler

idt_load:
    lidt [idt_descriptor]
    ret

keyboard_handler_stub:
    push rax
    push rcx
    push rdx
    push rsi
    push rdi
    push r8
    push r9
    push r10
    push r11
    
    call keyboard_handler
    
    pop r11
    pop r10
    pop r9
    pop r8
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rax
    iretq

section .data
align 16
idt_descriptor:
    dw 256 * 16 - 1
    dq idt_table