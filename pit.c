typedef unsigned char  uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int   uint32_t;
typedef unsigned long  uint64_t;

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

volatile uint64_t system_ticks = 0;

void pit_init(void) {
    uint32_t divisor = 1193182 / 100;   /* 100 Hz */
    outb(0x43, 0x36);
    outb(0x40, (uint8_t)(divisor & 0xFF));
    outb(0x40, (uint8_t)((divisor >> 8) & 0xFF));
}

void pit_handler(void) {
    system_ticks++;
    outb(0x20, 0x20);
}

uint64_t pit_seconds(void) {
    return system_ticks / 100;
}