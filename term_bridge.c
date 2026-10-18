typedef unsigned char uint8_t;
typedef unsigned int uint32_t;

extern void gfx_term_putchar(char c);
extern void gfx_term_print(const char *s);
extern void gfx_fill_rect(int x, int y, int w, int h, uint32_t color);

extern int g_term_x;
extern int g_term_y;
extern int g_term_w;
extern int g_term_h;
extern int g_cur_x;
extern int g_cur_y;

void putchar(char c) { gfx_term_putchar(c); }
void print(const char *s) { gfx_term_print(s); }
void clear_screen(void) {
    gfx_fill_rect(g_term_x, g_term_y, g_term_w, g_term_h, 0x00000000);
    g_cur_x = 0;
    g_cur_y = 0;
}