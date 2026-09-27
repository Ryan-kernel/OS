[org 0x7c00]
[bits 16]

start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7c00
    sti

    mov ah, 0x00
    mov al, 0x03
    int 0x10
    
    mov ah, 0x02
    mov al, 32
    mov ch, 0
    mov cl, 2
    mov dh, 0
    mov dl, 0x00
    mov bx, 0x7e00
    int 0x13
    jc disk_error

    jmp 0x0000:0x7e00

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

err_msg db "Disk error!", 0

times 510-($-$$) db 0
dw 0xaa55
