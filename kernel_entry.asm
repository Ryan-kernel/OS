[bits 64]
[extern kernel_main]
[extern __bss_start]
[extern __bss_end]

global _start
_start:
    mov rsp, 0x500000

    mov rdi, __bss_start
    mov rcx, __bss_end
    sub rcx, rdi
    xor eax, eax
    cld
    rep stosb

    call kernel_main
    jmp $