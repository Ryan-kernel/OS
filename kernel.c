typedef unsigned char  uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int   uint32_t;
typedef unsigned long  uint64_t;

#define VGA_WIDTH  80
#define VGA_HEIGHT 25
#define VGA_BUFFER 0xB8000

/* ---------- 端口 IO ---------- */
static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/* ---------- VGA 光标 ---------- */
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

/* ---------- 清屏 ---------- */
void clear_screen(void) {
    volatile char *vga = (volatile char *)VGA_BUFFER;
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        vga[i * 2]     = ' ';
        vga[i * 2 + 1] = 0x0F;
    }
    set_cursor(0);
}

/* ---------- 输出单字符 ---------- */
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

    /* 滚屏 */
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

/* ---------- 输出字符串 ---------- */
void print(const char *s) {
    while (*s) putchar(*s++);
}

/* ---------- 输出 16 进制 ---------- */
void print_hex(uint64_t n) {
    char hex[17];
    hex[0]  = '0'; hex[1]  = '1'; hex[2]  = '2'; hex[3]  = '3';
    hex[4]  = '4'; hex[5]  = '5'; hex[6]  = '6'; hex[7]  = '7';
    hex[8]  = '8'; hex[9]  = '9'; hex[10] = 'A'; hex[11] = 'B';
    hex[12] = 'C'; hex[13] = 'D'; hex[14] = 'E'; hex[15] = 'F';
    hex[16] = 0;

    putchar('0'); putchar('x');
    for (int i = 60; i >= 0; i -= 4) {
        putchar(hex[(n >> i) & 0xF]);
    }
}

/* ---------- 输出十进制 ---------- */
void print_dec(int n) {
    if (n < 0) { putchar('-'); n = -n; }
    if (n >= 10) print_dec(n / 10);
    putchar('0' + (n % 10));
}

/* ---------- 外部函数 ---------- */
extern void idt_init(void);

/* ---------- 主函数 ---------- */
void kernel_main(void) {
    clear_screen();
    
    idt_init();

    while (1) {
        __asm__ volatile ("hlt");
    }
}