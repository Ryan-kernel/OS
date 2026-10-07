[org 0x7e00]
[bits 16]

stage2_start:
    cli

    mov ax, 0x0003
    int 0x10

    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7c00

    mov dl, [0x7DFC]

    mov ax, 0x1000
    mov es, ax
    mov ah, 0x02
    mov al, 128
    mov ch, 0
    mov cl, 10
    mov dh, 0
    mov bx, 0x0000
    int 0x13
    jc disk_error

    xor ax, ax
    mov es, ax

    lgdt [gdt_descriptor]
    mov eax, cr0
    or eax, 0x1
    mov cr0, eax
    jmp CODE_SEG:init_pm

disk_error:
    mov si, err_msg
    call print_string
    jmp $

print_string:
    mov ah, 0x0e
.next:
    lodsb
    cmp al, 0
    je .done
    int 0x10
    jmp .next
.done:
    ret

err_msg db "Kernel load error!", 0

gdt_start:
gdt_null:
    dq 0

gdt_code32:
    dw 0xffff
    dw 0
    db 0
    db 10011010b
    db 11001111b
    db 0

gdt_data:
    dw 0xffff
    dw 0
    db 0
    db 10010010b
    db 11001111b
    db 0

gdt_code64:
    dw 0
    dw 0
    db 0
    db 10011010b
    db 00100000b
    db 0

gdt_end:

gdt_descriptor:
    dw gdt_end - gdt_start - 1
    dd gdt_start

CODE_SEG   equ gdt_code32 - gdt_start
DATA_SEG   equ gdt_data - gdt_start
CODE64_SEG equ gdt_code64 - gdt_start

[bits 32]
init_pm:
    mov ax, DATA_SEG
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x500000

    mov edi, 0x100000
    mov ecx, 0xC00
    xor eax, eax
    rep stosd

    mov dword [0x100000], 0x101000 | 0x3
    mov dword [0x101000], 0x102000 | 0x3

    mov edi, 0x102000
    mov eax, 0x83
    mov ecx, 512
.fill_pdt:
    mov [edi], eax
    add eax, 0x200000
    add edi, 8
    loop .fill_pdt

    mov eax, 0x100000
    mov cr3, eax

    mov eax, cr4
    or eax, (1 << 5) | (1 << 4)
    mov cr4, eax

    mov ecx, 0xC0000080
    rdmsr
    or eax, 1 << 8
    wrmsr

    mov eax, cr0
    or eax, 1 << 31
    mov cr0, eax

    jmp CODE64_SEG:init_lm

[bits 64]
init_lm:
    mov ax, DATA_SEG
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov rsp, 0x500000

    mov rax, 0x10000
    jmp rax

times 4096-($-$$) db 0