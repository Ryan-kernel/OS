typedef unsigned char  uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int   uint32_t;
typedef unsigned long  uint64_t;

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

extern uint32_t div_u32(uint32_t a, uint32_t b);
extern uint32_t mod_u32(uint32_t a, uint32_t b);

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

void pit_get_time(int *hours, int *minutes, int *seconds) {
    uint32_t total = (uint32_t)pit_seconds();
    *hours   = (int)mod_u32(div_u32(total, 3600), 24);
    *minutes = (int)mod_u32(div_u32(total, 60), 60);
    *seconds = (int)mod_u32(total, 60);
}

void pit_sleep(uint32_t seconds) {
    volatile uint64_t target = system_ticks + (uint64_t)seconds * 100;
    while (system_ticks < target) {
        __asm__ volatile ("hlt");
    }
}