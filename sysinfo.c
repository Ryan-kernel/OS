typedef unsigned char  uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int   uint32_t;
typedef unsigned long  uint64_t;

extern void kprintf(const char *fmt, ...);

/* ---- 端口 I/O ---- */
static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/* ---- 重启 ---- */
void reboot_now(void) {
    while (1) {
        uint8_t status = inb(0x64);
        if (!(status & 0x02)) break;
    }
    outb(0x64, 0xFE);
    while (1) {
        __asm__ volatile ("cli; hlt");
    }
}

/* ---- 关机（isa-debug-exit） ---- */
void shutdown_now(void) {
    outb(0xF4, 0x00);

    kprintf("isa-debug-exit not present, rebooting instead...\n");
    reboot_now();
}

/* ---- CPU 信息 ---- */
void print_cpu_info(void) {
    uint32_t eax, ebx, ecx, edx;

    __asm__ volatile (
        "cpuid"
        : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
        : "a"(0)
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

    kprintf("Vendor: %s\n", vendor);

    __asm__ volatile (
        "cpuid"
        : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
        : "a"(1)
    );

    kprintf("Family: %d  Model: %d  Stepping: %d\n",
            (int)((eax >> 8) & 0xF),
            (int)((eax >> 4) & 0xF),
            (int)(eax & 0xF));
}