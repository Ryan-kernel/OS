typedef unsigned char  uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int   uint32_t;
typedef unsigned long  uint64_t;

extern void putchar(char c);
extern void print(const char *s);
extern void print_hex(uint64_t n);
extern void print_dec(int n);

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
    putchar('\n');
    print("Exception: ");
    if (int_no < 32) {
        print(exception_names[int_no]);
    } else {
        print("Unknown");
    }
    putchar('\n');

    print("  Error code: ");
    print_hex(err_code);
    putchar('\n');
    
    print("  RIP: ");
    print_hex(rsp[0]);
    putchar('\n');

    print("  CS:  ");
    print_hex(rsp[1]);
    putchar('\n');

    print("  RFLAGS: ");
    print_hex(rsp[2]);
    putchar('\n');

    print("System halted.\n");
    while (1) {
        __asm__ volatile ("hlt");
    }
}