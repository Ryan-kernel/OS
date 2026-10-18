typedef unsigned char  uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int   uint32_t;
typedef unsigned long  uint64_t;

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

volatile uint64_t system_ticks = 0;

void pit_init(void) {
    // 100 Hz 的时钟中断
    uint32_t divisor = 1193182 / 100;
    outb(0x43, 0x36);
    outb(0x40, (uint8_t)(divisor & 0xFF));
    outb(0x40, (uint8_t)((divisor >> 8) & 0xFF));
}

// 每次触发时钟，仅仅增加计数，绝不调用任何复杂函数
void pit_handler(void) {
    system_ticks++;
}

/* 补回给 shell.c 和 task.c 用的时间函数 */
uint64_t pit_seconds(void) {
    return system_ticks / 100;
}

void pit_get_time(int *hours, int *minutes, int *seconds) {
    uint64_t total = pit_seconds();
    *hours   = (int)((total / 3600) % 24);
    *minutes = (int)((total / 60) % 60);
    *seconds = (int)(total % 60);
}

void pit_sleep(uint32_t seconds) {
    volatile uint64_t target = system_ticks + (uint64_t)seconds * 100;
    while (system_ticks < target) {
        __asm__ volatile ("hlt");
    }
}