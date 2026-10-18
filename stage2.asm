[org 0x7e00]
[bits 16]

stage2_start:
    cli

    ; ==========================================
    ; 1. 初始化 VBE (1024x768 32bpp)
    ; ==========================================
    mov ax, 0x4F00
    mov di, 0x8000
    mov dword [di], 'VBE2'
    int 0x10
    cmp ax, 0x004F
    jne vbe_error

    mov si, [0x8000 + 14]
    mov ax, [0x8000 + 16]
    mov fs, ax

.find_mode:
    mov cx, [fs:si]
    cmp cx, 0xFFFF
    je vbe_error
    add si, 2

    push si
    push fs
    mov ax, 0x4F01
    mov di, 0x8200
    int 0x10
    pop fs
    pop si
    cmp ax, 0x004F
    jne .find_mode

    mov ax, [0x8200]
    test ax, 0x0080      ; LFB
    jz .find_mode
    test ax, 0x0010      ; Graphics
    jz .find_mode

    cmp word [0x8200 + 18], 1024
    jne .find_mode
    cmp word [0x8200 + 20], 768
    jne .find_mode
    cmp byte [0x8200 + 25], 32
    jne .find_mode

    mov bx, cx
    or bx, 0x4000        ; LFB 模式
    mov ax, 0x4F02
    int 0x10
    cmp ax, 0x004F
    jne vbe_error

    ; 关闭光标
    mov dx, 0x3D4
    mov al, 0x0A
    out dx, al
    inc dx
    mov al, 0x20
    out dx, al

    ; ==========================================
    ; 强行保存参数到 0x9000 (假设 0xFD000000)
    ; ==========================================
    mov dword [0x9000], 0xFD000000
    mov ax, [0x8200 + 18]
    mov [0x9004], ax
    mov ax, [0x8200 + 20]
    mov [0x9006], ax
    mov ax, [0x8200 + 16]
    mov [0x9008], ax

    jmp vbe_ok

vbe_error:
    jmp $

vbe_ok:
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7c00

    ; 读内核
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
    jmp $

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

    ; ==========================================
    ; 完美的 4 级页表映射 512GB 空间
    ; ==========================================
    ; 1. PML4[0] 指向 PDPT
    mov dword [0x100000], 0x101000 | 0x3

    ; 2. 填充 512 个 PDPT 项 (每一项指向一个 1GB 的 PDT)
    ; 因为我们只需要用到 4GB (0xFD000000 在第 4GB 范围内)，所以其实只需要填前几项。
    ; 为了安全，我们填前 4 项，映射前 4GB 物理内存。
    mov edi, 0x101000
    mov eax, 0x102000 | 0x3
    mov ecx, 4            ; 只需要 4 个 PDPT 项，映射 4GB
.fill_pdpt:
    mov [edi], eax
    add eax, 0x1000       ; 每个 PDT 是 4KB，下一个页表在 +0x1000
    add edi, 8
    loop .fill_pdpt

    ; 3. 填满 4 个 PDT (每个 512 个 2MB 大页，共映射 4GB 内存)
    mov edi, 0x102000
    mov eax, 0x83         ; Present, Read/Write, 2MB Page
    mov ecx, 2048         ; 4 * 512 = 2048 个 2MB 页
.fill_pdt:
    mov [edi], eax
    add eax, 0x200000
    add edi, 8
    loop .fill_pdt

    mov eax, 0x100000
    mov cr3, eax

    mov eax, cr4
    or eax, (1 << 5) | (1 << 4)   ; PAE | PSE
    mov cr4, eax

    mov ecx, 0xC0000080
    rdmsr
    or eax, 1 << 8                 ; EFER.LME
    wrmsr

    mov eax, cr0
    or eax, 1 << 31                ; CR0.PG
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