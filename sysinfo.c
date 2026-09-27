typedef unsigned char  uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int   uint32_t;
typedef unsigned long  uint64_t;

extern void putchar(char c);
extern void print(const char *s);
extern void print_dec(int n);

/* ---- 重启 ---- */
void reboot_now(void) {
    while (1) {
        uint8_t status;
        __asm__ volatile ("inb $0x64, %0" : "=a"(status));
        if (!(status & 0x02)) break;
    }
    __asm__ volatile ("outb %0, $0x64" : : "a"((uint8_t)0xFE));
    while (1) {
        __asm__ volatile ("hlt");
    }
}

/* ---- CUP信息 ---- */
void print_cpu_info(void) {
    uint32_t eax, ebx, ecx, edx;

    /* ---- leaf 0：厂商字符串 ---- */
    __asm__ volatile (
        "pushq %%rbx\n\t"
        "movl $0, %%eax\n\t"
        "cpuid\n\t"
        "movl %%ebx, %%esi\n\t"
        "popq %%rbx"
        : "=a"(eax), "=S"(ebx), "=c"(ecx), "=d"(edx)
        :
        : "memory"
    );

    char vendor[13];
    vendor[0]  = (char)(ebx & 0xFF);
    vendor[1]  = (char)((ebx >> 8) & 0xFF);
    vendor[2]  = (char)((ebx >> 16) & 0xFF);
    vendor[3]  = (char)((ebx >> 24) & 0xFF);
    vendor[4]  = (char)(edx & 0xFF);
    vendor[5]  = (char)((edx >> 8) & 0xFF);
    vendor[6]  = (char)((edx >> 16) & 0xFF);
    vendor[7]  = (char)((edx >> 24) & 0xFF);
    vendor[8]  = (char)(ecx & 0xFF);
    vendor[9]  = (char)((ecx >> 8) & 0xFF);
    vendor[10] = (char)((ecx >> 16) & 0xFF);
    vendor[11] = (char)((ecx >> 24) & 0xFF);
    vendor[12] = 0;

    print("Vendor: ");
    print(vendor);
    putchar('\n');

    __asm__ volatile (
        "pushq %%rbx\n\t"
        "movl $1, %%eax\n\t"
        "cpuid\n\t"
        "movl %%ebx, %%esi\n\t"
        "popq %%rbx"
        : "=a"(eax), "=S"(ebx), "=c"(ecx), "=d"(edx)
        :
        : "memory"
    );

    print("Family: ");
    print_dec((int)((eax >> 8) & 0xF));
    print("  Model: ");
    print_dec((int)((eax >> 4) & 0xF));
    print("  Stepping: ");
    print_dec((int)(eax & 0xF));
    putchar('\n');
}