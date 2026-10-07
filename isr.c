typedef unsigned char  uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int   uint32_t;
typedef unsigned long  uint64_t;

extern void kprintf(const char *fmt, ...);

static const char *exception_names[] = {
    "Divide by Zero",
    "Debug",
    "Non-maskable Interrupt",
    "Breakpoint",
    "Overflow",
    "Bound Range Exceeded",
    "Invalid Opcode",
    "Device Not Available",
    "Double Fault",
    "Coprocessor Segment Overrun",
    "Invalid TSS",
    "Segment Not Present",
    "Stack-Segment Fault",
    "General Protection Fault",
    "Page Fault",
    "Reserved",
    "x87 Floating-Point Exception",
    "Alignment Check",
    "Machine Check",
    "SIMD Floating-Point Exception",
    "Reserved", "Reserved", "Reserved", "Reserved",
    "Reserved", "Reserved", "Reserved", "Reserved",
    "Reserved", "Reserved", "Reserved", "Reserved"
};

void isr_handler(uint64_t int_no, uint64_t err_code, uint64_t *rsp) {
    kprintf("\n*** EXCEPTION #%d: %s ***\n",
            (int)int_no,
            int_no < 32 ? exception_names[int_no] : "Unknown");

    kprintf("  Error code: %p\n", (void*)err_code);
    kprintf("  RIP:        %p\n", (void*)rsp[0]);
    kprintf("  CS:         %p\n", (void*)rsp[1]);
    kprintf("  RFLAGS:     %p\n", (void*)rsp[2]);

    kprintf("System halted.\n");
    while (1) {
        __asm__ volatile ("cli; hlt");
    }
}