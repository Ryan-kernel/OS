nasm -f bin boot.asm -o boot.bin
nasm -f bin stage2.asm -o stage2.bin
nasm -f elf64 kernel_entry.asm -o kernel_entry.o
nasm -f elf64 idt.asm -o idt_asm.o

"D:\elf-tools\bin\x86_64-elf-gcc" -m64 -c -nostdlib -ffreestanding -fno-pic -fno-stack-protector -mno-red-zone -mcmodel=large -o kernel.o kernel.c
"D:\elf-tools\bin\x86_64-elf-gcc" -m64 -c -nostdlib -ffreestanding -fno-pic -fno-stack-protector -mno-red-zone -mcmodel=large -o idt.o idt.c

"D:\elf-tools\bin\x86_64-elf-ld" -T linker.ld -o kernel.elf kernel_entry.o idt_asm.o kernel.o idt.o

objdump -h kernel.elf

"D:\elf-tools\bin\x86_64-elf-objcopy" -O binary --only-section=.text --only-section=.rodata --only-section=.data kernel.elf kernel.bin

ls -la stage2.bin kernel.bin

dd if=/dev/zero of=os.img bs=512 count=2880
dd if=boot.bin   of=os.img bs=512 seek=0 conv=notrunc
dd if=stage2.bin of=os.img bs=512 seek=1 conv=notrunc
dd if=kernel.bin of=os.img bs=512 seek=2 conv=notrunc