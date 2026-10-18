typedef unsigned char  uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int   uint32_t;
typedef unsigned long  uint64_t;

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/* 外部依赖 */
extern void gfx_init(void);
extern void gfx_clear(uint32_t color);
extern void gfx_print(const char *s);
extern void gfx_fill_rect(int x, int y, int w, int h, uint32_t color);
extern void gfx_fill_round_rect(int x, int y, int w, int h, int r, uint32_t color);
extern void gfx_draw_mouse_cursor(int x, int y, uint32_t color);
extern void gfx_draw_string_at(const char *s, int x, int y, uint32_t color);
extern void gfx_set_terminal(int x, int y, int w, int h);
extern void gfx_flip(void);

extern void idt_init(void);
extern void shell_init(void);
extern void shell_prompt(void);
extern void fs_init(void);
extern void shell_input(char c);
extern void shell_enter(void);
extern void shell_backspace(void);

extern volatile uint64_t system_ticks;

/* 补齐汇编桩需要的空函数 */
void keyboard_handler(void) {}
void mouse_handler(void) {}
void isr_handler(uint64_t int_no, uint64_t err_code, uint64_t *rsp) {
    while (1) __asm__ volatile ("cli; hlt");
}

/* 数学工具 */
uint32_t div_u32(uint32_t a, uint32_t b) {
    uint32_t q = 0, r = 0;
    for (int i = 31; i >= 0; i--) {
        r = (r << 1) | ((a >> i) & 1);
        if (r >= b) { r -= b; q |= (1U << i); }
    }
    return q;
}
uint32_t mod_u32(uint32_t a, uint32_t b) {
    uint32_t r = 0;
    for (int i = 31; i >= 0; i--) {
        r = (r << 1) | ((a >> i) & 1);
        if (r >= b) r -= b;
    }
    return r;
}

/* UI 布局参数 */
#define TASKBAR_Y     720
#define TASKBAR_H     48
#define START_BTN_X   10
#define START_BTN_Y   724
#define START_BTN_W   100
#define START_BTN_H   40

#define MENU_X        10
#define MENU_Y        500
#define MENU_W        200
#define MENU_H        216

int is_menu_open = 0;
int shell_active = 0; 
int settings_active = 0; 
uint32_t last_drawn_sec = 0;
int last_hover = 0;

void format_2digits(uint32_t val, char *out) {
    out[0] = (val / 10) + '0';
    out[1] = (val % 10) + '0';
}

void get_time_string(uint32_t seconds, char *out) {
    uint32_t h = seconds / 3600;
    uint32_t m = (seconds % 3600) / 60;
    uint32_t s = seconds % 60;
    format_2digits(h, out);
    out[2] = ':';
    format_2digits(m, out + 3);
    out[5] = ':';
    format_2digits(s, out + 6);
    out[8] = 0;
}

void draw_settings_window(int cur_x, int cur_y) {
    // 1. 画黑色大背景（覆盖桌面）
    gfx_fill_round_rect(100, 100, 824, 500, 10, 0x00000000);
    // 2. 画蓝色标题栏
    gfx_fill_round_rect(100, 100, 824, 30, 10, 0x00333399);
    gfx_draw_string_at("Settings (Press ESC to exit)", 110, 107, 0x00FFFFFF);

    // 3. 画菜单项
    gfx_draw_string_at("System Settings", 150, 160, 0x00FFFFFF);
    
    // 判断悬停
    int hover_display = (cur_x >= 150 && cur_x <= 350 && cur_y >= 200 && cur_y <= 240);
    int hover_sound   = (cur_x >= 150 && cur_x <= 350 && cur_y >= 260 && cur_y <= 300);
    int hover_net     = (cur_x >= 150 && cur_x <= 350 && cur_y >= 320 && cur_y <= 360);

    // 按钮
    uint32_t c1 = hover_display ? 0x002196F3 : 0x00555555;
    gfx_fill_round_rect(150, 200, 200, 40, 5, c1);
    gfx_draw_string_at("Display", 180, 212, 0x00FFFFFF);
    
    uint32_t c2 = hover_sound ? 0x002196F3 : 0x00555555;
    gfx_fill_round_rect(150, 260, 200, 40, 5, c2);
    gfx_draw_string_at("Sound", 180, 272, 0x00FFFFFF);
    
    uint32_t c3 = hover_net ? 0x002196F3 : 0x00555555;
    gfx_fill_round_rect(150, 320, 200, 40, 5, c3);
    gfx_draw_string_at("Network", 180, 332, 0x00FFFFFF);

    // 右侧信息框
    gfx_fill_rect(400, 180, 480, 300, 0x00151515);
    gfx_draw_string_at("OS Version: 1.0.0", 420, 200, 0x00CCCCCC);
    gfx_draw_string_at("Architecture: x86_64", 420, 220, 0x00CCCCCC);
    gfx_draw_string_at("Display: 1024x768 32bpp", 420, 240, 0x00CCCCCC);
    gfx_draw_string_at("File System: ExtentFS", 420, 260, 0x00CCCCCC);

    // 光标
    gfx_draw_mouse_cursor(cur_x, cur_y, 0x00FFFFFF);
}

void draw_ui(int cur_x, int cur_y, int hover_start) {
    if (shell_active) return;

    gfx_clear(0x001E1E2E);
    
    if (settings_active) {
        draw_settings_window(cur_x, cur_y);
    } else {
        gfx_print("Graphic OS Desktop\n");
        gfx_print("Move cursor over Start -> ENTER -> Terminal.\n\n");

        // 任务栏
        gfx_fill_rect(0, TASKBAR_Y, 1024, TASKBAR_H, 0x00151515);

        // Start 按钮
        uint32_t btn_color = hover_start ? 0x00E53935 : 0x00B71C1C;
        gfx_fill_round_rect(START_BTN_X, START_BTN_Y, START_BTN_W, START_BTN_H, 8, btn_color);
        gfx_draw_string_at("Start", START_BTN_X + 28, START_BTN_Y + 12, 0x00FFFFFF);

        // 开始菜单
        if (is_menu_open) {
            gfx_fill_round_rect(MENU_X, MENU_Y, MENU_W, MENU_H, 10, 0x001976D2);
            gfx_fill_round_rect(MENU_X, MENU_Y, MENU_W, 30, 10, 0x00B71C1C);
            gfx_draw_string_at("Menu", MENU_X + 10, MENU_Y + 7, 0x00FFFFFF);
            
            gfx_fill_round_rect(MENU_X + 10, MENU_Y + 50, 180, 30, 5, 0x002196F3);
            gfx_draw_string_at("Terminal", MENU_X + 20, MENU_Y + 57, 0x00FFFFFF);
            gfx_fill_round_rect(MENU_X + 10, MENU_Y + 90, 180, 30, 5, 0x002196F3);
            gfx_draw_string_at("Settings", MENU_X + 20, MENU_Y + 97, 0x00FFFFFF);
        }

        // 时钟
        char time_str[16];
        get_time_string(last_drawn_sec, time_str);
        gfx_draw_string_at(time_str, 900, TASKBAR_Y + 16, 0x00FFFFFF);

        // 光标
        gfx_draw_mouse_cursor(cur_x, cur_y, 0x00FFFFFF);
    }
}

void kernel_main(void) {
    gfx_init();
    idt_init();
    fs_init();

    int cur_x = 512;
    int cur_y = 384;
    int old_x = cur_x;
    int old_y = cur_y;
    int hover_start = 0;

    draw_ui(cur_x, cur_y, hover_start);
    gfx_flip();

    #define QUEUE_SIZE 256
    uint8_t sc_queue[QUEUE_SIZE];
    int q_head = 0;
    int q_tail = 0;
    
    int shift_pressed = 0; 
    int caps_lock = 0;     

    __asm__ volatile ("sti");

    while (1) {
        if (inb(0x64) & 1) {
            uint8_t sc = inb(0x60);
            int next = (q_head + 1) % QUEUE_SIZE;
            if (next != q_tail) {
                sc_queue[q_head] = sc;
                q_head = next;
            }
        }

        while (q_tail != q_head) {
            uint8_t sc = sc_queue[q_tail];
            q_tail = (q_tail + 1) % QUEUE_SIZE;

            if (sc == 0x2A || sc == 0x36) { shift_pressed = 1; continue; }
            if (sc == 0xAA || sc == 0xB6) { shift_pressed = 0; continue; }
            if (sc == 0x3A) { caps_lock = !caps_lock; continue; }
            if (sc & 0x80) continue; 

            if (shell_active) {
                // --- 终端模式 ---
                if (sc == 0x01) { 
                    shell_active = 0;
                    draw_ui(cur_x, cur_y, hover_start);
                    gfx_flip();
                    continue;
                }
                if (sc == 0x1C) { shell_enter(); gfx_flip(); continue; }
                if (sc == 0x0E) { shell_backspace(); gfx_flip(); continue; }

                char c = 0;
                switch(sc) {
                    case 0x1E: c = (shift_pressed ^ caps_lock) ? 'A' : 'a'; break;
                    case 0x30: c = (shift_pressed ^ caps_lock) ? 'B' : 'b'; break;
                    case 0x2E: c = (shift_pressed ^ caps_lock) ? 'C' : 'c'; break;
                    case 0x20: c = (shift_pressed ^ caps_lock) ? 'D' : 'd'; break;
                    case 0x12: c = (shift_pressed ^ caps_lock) ? 'E' : 'e'; break;
                    case 0x21: c = (shift_pressed ^ caps_lock) ? 'F' : 'f'; break;
                    case 0x22: c = (shift_pressed ^ caps_lock) ? 'G' : 'g'; break;
                    case 0x23: c = (shift_pressed ^ caps_lock) ? 'H' : 'h'; break;
                    case 0x17: c = (shift_pressed ^ caps_lock) ? 'I' : 'i'; break;
                    case 0x24: c = (shift_pressed ^ caps_lock) ? 'J' : 'j'; break;
                    case 0x25: c = (shift_pressed ^ caps_lock) ? 'K' : 'k'; break;
                    case 0x26: c = (shift_pressed ^ caps_lock) ? 'L' : 'l'; break;
                    case 0x32: c = (shift_pressed ^ caps_lock) ? 'M' : 'm'; break;
                    case 0x31: c = (shift_pressed ^ caps_lock) ? 'N' : 'n'; break;
                    case 0x18: c = (shift_pressed ^ caps_lock) ? 'O' : 'o'; break;
                    case 0x19: c = (shift_pressed ^ caps_lock) ? 'P' : 'p'; break;
                    case 0x10: c = (shift_pressed ^ caps_lock) ? 'Q' : 'q'; break;
                    case 0x13: c = (shift_pressed ^ caps_lock) ? 'R' : 'r'; break;
                    case 0x1F: c = (shift_pressed ^ caps_lock) ? 'S' : 's'; break;
                    case 0x14: c = (shift_pressed ^ caps_lock) ? 'T' : 't'; break;
                    case 0x16: c = (shift_pressed ^ caps_lock) ? 'U' : 'u'; break;
                    case 0x2F: c = (shift_pressed ^ caps_lock) ? 'V' : 'v'; break;
                    case 0x11: c = (shift_pressed ^ caps_lock) ? 'W' : 'w'; break;
                    case 0x2D: c = (shift_pressed ^ caps_lock) ? 'X' : 'x'; break;
                    case 0x15: c = (shift_pressed ^ caps_lock) ? 'Y' : 'y'; break;
                    case 0x2C: c = (shift_pressed ^ caps_lock) ? 'Z' : 'z'; break;

                    case 0x02: c = shift_pressed ? '!' : '1'; break;
                    case 0x03: c = shift_pressed ? '@' : '2'; break;
                    case 0x04: c = shift_pressed ? '#' : '3'; break;
                    case 0x05: c = shift_pressed ? '$' : '4'; break;
                    case 0x06: c = shift_pressed ? '%' : '5'; break;
                    case 0x07: c = shift_pressed ? '^' : '6'; break;
                    case 0x08: c = shift_pressed ? '&' : '7'; break;
                    case 0x09: c = shift_pressed ? '*' : '8'; break;
                    case 0x0A: c = shift_pressed ? '(' : '9'; break;
                    case 0x0B: c = shift_pressed ? ')' : '0'; break;
                    case 0x0C: c = shift_pressed ? '_' : '-'; break;
                    case 0x0D: c = shift_pressed ? '+' : '='; break;
                    case 0x39: c = ' '; break;
                }
                if (c != 0) {
                    shell_input(c);
                    gfx_flip();
                }

            } else if (settings_active) {
                // --- 设置模式 ---
                if (sc == 0x01) {
                    settings_active = 0;
                    draw_ui(cur_x, cur_y, hover_start);
                    gfx_flip();
                    continue;
                }

                if (sc == 0xE0) continue; 
                
                int moved = 0;
                if (sc == 0x48) { cur_y -= 15; moved = 1; }
                if (sc == 0x50) { cur_y += 15; moved = 1; }
                if (sc == 0x4B) { cur_x -= 15; moved = 1; }
                if (sc == 0x4D) { cur_x += 15; moved = 1; }

                if (cur_x < 0) cur_x = 0;
                if (cur_x > 1000) cur_x = 1000;
                if (cur_y < 0) cur_y = 0;
                if (cur_y > 750) cur_y = 750;

                // 处理点击
                if (sc == 0x1C) {
                    if (cur_x >= 150 && cur_x <= 350 && cur_y >= 200 && cur_y <= 240) {
                        gfx_fill_rect(400, 180, 480, 300, 0x00151515);
                        gfx_draw_string_at("Display Settings", 420, 200, 0x00FFFFFF);
                        gfx_draw_string_at("Resolution: 1024 x 768", 420, 230, 0x00CCCCCC);
                        gfx_draw_string_at("Color Depth: 32bpp", 420, 250, 0x00CCCCCC);
                        gfx_flip();
                        continue;
                    }
                    if (cur_x >= 150 && cur_x <= 350 && cur_y >= 260 && cur_y <= 300) {
                        gfx_fill_rect(400, 180, 480, 300, 0x00151515);
                        gfx_draw_string_at("Sound Settings", 420, 200, 0x00FFFFFF);
                        gfx_draw_string_at("Volume: 100%", 420, 230, 0x00CCCCCC);
                        gfx_draw_string_at("Device: PC Speaker", 420, 250, 0x00CCCCCC);
                        gfx_flip();
                        continue;
                    }
                    if (cur_x >= 150 && cur_x <= 350 && cur_y >= 320 && cur_y <= 360) {
                        gfx_fill_rect(400, 180, 480, 300, 0x00151515);
                        gfx_draw_string_at("Network Settings", 420, 200, 0x00FFFFFF);
                        gfx_draw_string_at("Status: Disconnected", 420, 230, 0x00CCCCCC);
                        gfx_draw_string_at("Interface: e1000", 420, 250, 0x00CCCCCC);
                        gfx_flip();
                        continue;
                    }
                }

                if (moved) {
                    draw_ui(cur_x, cur_y, hover_start);
                    gfx_flip();
                }

            } else {
                // --- 桌面模式 ---
                if (sc == 0xE0) continue; 
                
                int moved = 0;
                if (sc == 0x48) { cur_y -= 15; moved = 1; }
                if (sc == 0x50) { cur_y += 15; moved = 1; }
                if (sc == 0x4B) { cur_x -= 15; moved = 1; }
                if (sc == 0x4D) { cur_x += 15; moved = 1; }

                if (cur_x < 0) cur_x = 0;
                if (cur_x > 1000) cur_x = 1000;
                if (cur_y < 0) cur_y = 0;
                if (cur_y > 750) cur_y = 750;

                int now_hover = (cur_x >= START_BTN_X && cur_x <= START_BTN_X + START_BTN_W &&
                                 cur_y >= START_BTN_Y && cur_y <= START_BTN_Y + START_BTN_H);

                if (sc == 0x1C) { 
                    if (now_hover) {
                        is_menu_open = !is_menu_open;
                    } else if (is_menu_open && cur_x >= MENU_X && cur_x <= MENU_X + MENU_W &&
                               cur_y >= MENU_Y + 50 && cur_y <= MENU_Y + 80) {
                        // 点击 Terminal
                        gfx_fill_round_rect(100, 100, 824, 500, 10, 0x00000000);
                        gfx_fill_round_rect(100, 100, 824, 30, 10, 0x00333399);
                        gfx_draw_string_at("Terminal (Press ESC to exit)", 110, 107, 0x00FFFFFF);
                        gfx_set_terminal(110, 140, 800, 440);
                        shell_init();
                        shell_active = 1;
                        shell_prompt();
                        gfx_flip();
                        continue;
                    } else if (is_menu_open && cur_x >= MENU_X && cur_x <= MENU_X + MENU_W &&
                               cur_y >= MENU_Y + 90 && cur_y <= MENU_Y + 120) {
                        // 点击 Settings
                        is_menu_open = 0; 
                        settings_active = 1;
                        draw_ui(cur_x, cur_y, hover_start);
                        gfx_flip();
                        continue;
                    }
                }

                // 纯双缓冲刷新
                if (moved || sc == 0x1C || now_hover != hover_start) {
                    hover_start = now_hover;
                    old_x = cur_x;
                    old_y = cur_y;
                    draw_ui(cur_x, cur_y, hover_start);
                    gfx_flip();
                }
            }
        }

        uint32_t current_sec = (uint32_t)(system_ticks / 100); 
        if (current_sec != last_drawn_sec) {
            last_drawn_sec = current_sec;
            
            if (!shell_active && !settings_active) {
                gfx_fill_rect(890, TASKBAR_Y + 10, 100, 28, 0x00151515);
                char time_str[16];
                get_time_string(last_drawn_sec, time_str);
                gfx_draw_string_at(time_str, 900, TASKBAR_Y + 16, 0x00FFFFFF);
                gfx_flip();
            }
        }
    }
}