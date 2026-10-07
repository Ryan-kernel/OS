typedef unsigned char  uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int   uint32_t;
typedef unsigned long  uint64_t;

#define VGA_WIDTH  80
#define VGA_HEIGHT 25
#define VGA_BUFFER 0xB8000

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static int get_cursor(void) {
    outb(0x3D4, 0x0E);
    int pos = (int)inb(0x3D5) << 8;
    outb(0x3D4, 0x0F);
    pos |= inb(0x3D5);
    return pos;
}

static void set_cursor(int pos) {
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
}

void clear_screen(void) {
    volatile char *vga = (volatile char *)VGA_BUFFER;
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        vga[i * 2]     = ' ';
        vga[i * 2 + 1] = 0x0F;
    }
    set_cursor(0);
}

void putchar(char c) {
    volatile char *vga = (volatile char *)VGA_BUFFER;
    int cursor = get_cursor();

    if (c == '\n') {
        cursor = (cursor / VGA_WIDTH + 1) * VGA_WIDTH;
    } else if (c == '\r') {
        cursor = (cursor / VGA_WIDTH) * VGA_WIDTH;
    } else if (c == '\b') {
        if (cursor > 0) {
            cursor--;
            vga[cursor * 2]     = ' ';
            vga[cursor * 2 + 1] = 0x0F;
        }
    } else {
        vga[cursor * 2]     = c;
        vga[cursor * 2 + 1] = 0x0F;
        cursor++;
    }

    if (cursor >= VGA_WIDTH * VGA_HEIGHT) {
        for (int i = 0; i < (VGA_HEIGHT - 1) * VGA_WIDTH; i++) {
            vga[i * 2]     = vga[(i + VGA_WIDTH) * 2];
            vga[i * 2 + 1] = vga[(i + VGA_WIDTH) * 2 + 1];
        }
        for (int i = (VGA_HEIGHT - 1) * VGA_WIDTH; i < VGA_HEIGHT * VGA_WIDTH; i++) {
            vga[i * 2]     = ' ';
            vga[i * 2 + 1] = 0x0F;
        }
        cursor -= VGA_WIDTH;
    }

    set_cursor(cursor);
}

void print(const char *s) {
    while (*s) putchar(*s++);
}

uint32_t div_u32(uint32_t a, uint32_t b) {
    uint32_t q = 0;
    uint32_t r = 0;
    for (int i = 31; i >= 0; i--) {
        r = (r << 1) | ((a >> i) & 1);
        if (r >= b) {
            r -= b;
            q |= (1U << i);
        }
    }
    return q;
}

uint32_t mod_u32(uint32_t a, uint32_t b) {
    uint32_t r = 0;
    for (int i = 31; i >= 0; i--) {
        r = (r << 1) | ((a >> i) & 1);
        if (r >= b) {
            r -= b;
        }
    }
    return r;
}

extern void idt_init(void);
extern void shell_init(void);
extern void heap_init(void);
extern void fs_init(void);
extern void kprintf(const char *fmt, ...);

void kernel_main(void) {
    clear_screen();
    idt_init();
    heap_init();
    fs_init();
    shell_init();

    while (1) {
        __asm__ volatile ("hlt");
    }
}