[org 0x7e00]
[bits 16]

stage2_start:
    cli
    lgdt [gdt_descriptor]
    mov eax, cr0
    or eax, 0x1
    mov cr0, eax
    jmp CODE_SEG:init_pm

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
    mov esp, 0x200000
    
    mov byte [0xb8000], '1'
    mov byte [0xb8001], 0x0f
    
    mov edi, 0x100000
    mov ecx, 0xC00
    xor eax, eax
    rep stosd

    mov byte [0xb8002], '2'
    mov byte [0xb8003], 0x0f

    ; PML4[0] = 0x101000 | 0x3
    mov dword [0x100000], 0x101000 | 0x3
    ; PDPT[0] = 0x102000 | 0x3
    mov dword [0x101000], 0x102000 | 0x3

    mov byte [0xb8004], '3'
    mov byte [0xb8005], 0x0f
    
    mov edi, 0x102000
    mov eax, 0x83
    mov ecx, 512
.fill_pdt:
    mov [edi], eax
    add eax, 0x200000
    add edi, 8
    loop .fill_pdt

    mov byte [0xb8006], '4'
    mov byte [0xb8007], 0x0f
    
    mov eax, 0x100000
    mov cr3, eax

    mov byte [0xb8008], '5'
    mov byte [0xb8009], 0x0f

    mov eax, cr4
    or eax, 1 << 5              ; CR4.PAE = 1
    mov cr4, eax

    mov byte [0xb800a], '6'
    mov byte [0xb800b], 0x0f

    mov ecx, 0xC0000080
    rdmsr
    or eax, 1 << 8
    wrmsr

    mov byte [0xb800c], '7'
    mov byte [0xb800d], 0x0f

    mov eax, cr0
    or eax, 1 << 31
    mov cr0, eax

    mov byte [0xb800e], '8'
    mov byte [0xb800f], 0x0f
    
    jmp CODE64_SEG:init_lm

[bits 64]
init_lm:
    mov ax, DATA_SEG
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov rsp, 0x200000
    
    mov byte [0xb8010], '9'
    mov byte [0xb8011], 0x0f
    
    mov rax, 0x8000
    jmp rax

times 512-($-$$) db 0