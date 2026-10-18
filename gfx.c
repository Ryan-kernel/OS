typedef unsigned char  uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int   uint32_t;
typedef unsigned long  uint64_t;

extern void *kmalloc(uint64_t size);

uint32_t* g_fb = 0;
uint32_t* back_buffer = 0;

int g_width = 0;
int g_height = 0;
int g_pitch = 0;
int g_pixels_per_line = 0;

int g_cur_x = 0;
int g_cur_y = 0;
int g_term_x = 0;
int g_term_y = 0;
int g_term_w = 0;
int g_term_h = 0;

extern const uint8_t font8x16[95][16];

void gfx_init(void) {
    uint32_t fb_addr = *(uint32_t*)0x9000;
    g_width  = *(uint16_t*)0x9004;
    g_height = *(uint16_t*)0x9006;
    g_pitch  = *(uint16_t*)0x9008;

    g_fb = (uint32_t*)(uint64_t)fb_addr;
    g_pixels_per_line = g_pitch / 4;

    back_buffer = (uint32_t*)kmalloc(g_width * g_height * 4);
    
    g_cur_x = 0;
    g_cur_y = 0;
}

// ★ 全屏刷新，使用 rep movsq 极速拷贝
void gfx_flip(void) {
    if (!back_buffer) return;
    uint64_t count = (uint64_t)(g_width * g_height) / 2; // 每次拷 8 字节，所以除以 2
    uint64_t *src = (uint64_t*)back_buffer;
    uint64_t *dst = (uint64_t*)g_fb;

    // 先把指针放进寄存器，再执行 rep movsq
    __asm__ volatile (
        "movq %0, %%rsi\n\t"
        "movq %1, %%rdi\n\t"
        "movq %2, %%rcx\n\t"
        "cld\n\t"
        "rep movsq"
        :
        : "r"(src), "r"(dst), "r"(count)
        : "memory", "rcx", "rsi", "rdi"
    );
}

void gfx_put_pixel(int x, int y, uint32_t color) {
    if (x < 0 || x >= g_width || y < 0 || y >= g_height) return;
    if (back_buffer) {
        back_buffer[y * g_pixels_per_line + x] = color;
    } else {
        g_fb[y * g_pixels_per_line + x] = color;
    }
}

void gfx_clear(uint32_t color) {
    for (int y = 0; y < g_height; y++) {
        for (int x = 0; x < g_width; x++) {
            gfx_put_pixel(x, y, color);
        }
    }
    g_cur_x = 0;
    g_cur_y = 0;
}

void gfx_fill_rect(int x, int y, int w, int h, uint32_t color) {
    for (int j = 0; j < h; j++) {
        for (int i = 0; i < w; i++) {
            gfx_put_pixel(x + i, y + j, color);
        }
    }
}

void gfx_fill_round_rect(int x, int y, int w, int h, int r, uint32_t color) {
    gfx_fill_rect(x + r, y, w - 2 * r, h, color);
    gfx_fill_rect(x, y + r, r, h - 2 * r, color);
    gfx_fill_rect(x + w - r, y + r, r, h - 2 * r, color);
    
    for (int i = 0; i <= r; i++) {
        for (int j = 0; j <= r; j++) {
            if (i * i + j * j <= r * r) {
                gfx_put_pixel(x + r - i, y + r - j, color);
                gfx_put_pixel(x + w - r - 1 + i, y + r - j, color);
                gfx_put_pixel(x + r - i, y + h - r - 1 + j, color);
                gfx_put_pixel(x + w - r - 1 + i, y + h - r - 1 + j, color);
            }
        }
    }
}

static void draw_char(char c, int x, int y, uint32_t color) {
    if (c < 32 || c > 126) c = '?';
    const uint8_t *glyph = font8x16[c - 32];
    for (int row = 0; row < 16; row++) {
        uint8_t bits = glyph[row];
        for (int col = 0; col < 8; col++) {
            if (bits & (0x80 >> col)) {
                gfx_put_pixel(x + col, y + row, color);
            }
        }
    }
}

void gfx_draw_string_at(const char *s, int x, int y, uint32_t color) {
    while (*s) {
        if (*s == '\n') {
            y += 16;
        } else {
            draw_char(*s, x, y, color);
            x += 8;
        }
        s++;
    }
}

void gfx_putchar(char c) {
    uint32_t fg = 0x00FFFFFF;
    uint32_t bg = 0x001E1E2E;

    if (c == '\n') { g_cur_x = 0; g_cur_y += 16; return; }
    if (c == '\r') { g_cur_x = 0; return; }
    if (c == '\b') {
        if (g_cur_x >= 8) {
            g_cur_x -= 8;
            gfx_fill_rect(g_cur_x, g_cur_y, 8, 16, bg);
        }
        return;
    }
    draw_char(c, g_cur_x, g_cur_y, fg);
    g_cur_x += 8;
    if (g_cur_x + 8 > g_width) { g_cur_x = 0; g_cur_y += 16; }
    if (g_cur_y + 16 > g_height) { gfx_clear(bg); }
}

void gfx_print(const char *s) { while (*s) gfx_putchar(*s++); }

void gfx_set_terminal(int x, int y, int w, int h) {
    g_term_x = x; g_term_y = y;
    g_term_w = w; g_term_h = h;
    g_cur_x = 0; g_cur_y = 0;
}

void gfx_term_putchar(char c) {
    uint32_t fg = 0x00FFFFFF;
    uint32_t bg = 0x00000000;

    if (c == '\n') { g_cur_x = 0; g_cur_y += 16; return; }
    if (c == '\r') { g_cur_x = 0; return; }
    if (c == '\b') {
        if (g_cur_x >= 8) {
            g_cur_x -= 8;
            gfx_fill_rect(g_term_x + g_cur_x, g_term_y + g_cur_y, 8, 16, bg);
        }
        return;
    }

    draw_char(c, g_term_x + g_cur_x, g_term_y + g_cur_y, fg);
    g_cur_x += 8;

    if (g_cur_x + 8 > g_term_w) {
        g_cur_x = 0;
        g_cur_y += 16;
    }
    if (g_cur_y + 16 > g_term_h) {
        gfx_fill_rect(g_term_x, g_term_y, g_term_w, g_term_h, bg);
        g_cur_x = 0;
        g_cur_y = 0;
    }
}

void gfx_term_print(const char *s) {
    while (*s) gfx_term_putchar(*s++);
}

void gfx_draw_mouse_cursor(int x, int y, uint32_t color) {
    gfx_fill_rect(x, y, 8, 8, color);
}