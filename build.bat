@echo off
set NASM=nasm
set CC="D:\elf-tools\bin\x86_64-elf-gcc"
set LD="D:\elf-tools\bin\x86_64-elf-ld"
set OBJCOPY="D:\elf-tools\bin\x86_64-elf-objcopy"

set CFLAGS=-m64 -c -nostdlib -ffreestanding -fno-pic -fno-pie -fno-stack-protector -mno-red-zone -mno-sse -mno-mmx -mno-80387 -mno-fp-ret-in-387

%NASM% -f bin boot.asm -o boot.bin
%NASM% -f bin stage2.asm -o stage2.bin
%NASM% -f elf64 kernel_entry.asm -o kernel_entry.o
%NASM% -f elf64 idt.asm -o idt_asm.o
%NASM% -f elf64 pit.asm -o pit_asm.o
%NASM% -f elf64 isr.asm -o isr_asm.o

%CC% %CFLAGS% -o kernel.o kernel.c
%CC% %CFLAGS% -o gfx.o gfx.c
%CC% %CFLAGS% -o font.o font.c
%CC% %CFLAGS% -o idt.o idt.c
%CC% %CFLAGS% -o pit.o pit.c
%CC% %CFLAGS% -o term_bridge.o term_bridge.c
%CC% %CFLAGS% -o shell.o shell.c
%CC% %CFLAGS% -o fs.o fs.c
%CC% %CFLAGS% -o kprintf.o kprintf.c
%CC% %CFLAGS% -o kmalloc.o kmalloc.c
%CC% %CFLAGS% -o task.o task.c
%CC% %CFLAGS% -o sysinfo.o sysinfo.c

%LD% -T linker.ld -o kernel.elf ^
    kernel_entry.o idt_asm.o pit_asm.o isr_asm.o ^
    kernel.o gfx.o font.o idt.o pit.o term_bridge.o ^
    shell.o fs.o kprintf.o kmalloc.o task.o sysinfo.o

%OBJCOPY% -O binary kernel.elf kernel.bin

dd if=/dev/zero of=os.img bs=512 count=2880
dd if=boot.bin   of=os.img bs=512 seek=0 conv=notrunc
dd if=stage2.bin of=os.img bs=512 seek=1 conv=notrunc
dd if=kernel.bin of=os.img bs=512 seek=9 conv=notrunc

if not exist disk.img (
    dd if=/dev/zero of=disk.img bs=512 count=2048
)

echo Build done.
"D:\qemu\qemu-system-x86_64" -accel whpx -drive file=os.img,format=raw,if=floppy -drive file=disk.img,format=raw,if=ide -device isa-debug-exit,iobase=0xf4,iosize=0x04